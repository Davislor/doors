/***************************************************************************
 * Doors/Linux                                                             *
 *                                                                         *
 * door_server.c: Server data structures and functions requiring           *
 *                knowledge thereof: door_create, door_revoke, door_info.  *
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

#include "standards.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "door.h"
#include "error.h"

/* The default size of door_table, used unless {OPEN_MAX} is a lower, 
 * positive number.
 */
static const size_t open_default = 1024U;

/* Converted into a 64-bit unsigned integer, and thence into an unsigned
 * long long, by door_info().
 */
typedef void (*server_proc_t) (void*, char*, size_t, door_desc_t*, uint_t);

/* Several functions manipulate the door_table, which is an array of 
 * OPEN_MAX door_data structures.  A file descriptor fd refers to a 
 * valid, local door if and only if door_table[fd] holds valid data.
 *
 * The program allocates memory for the door_table upon the first call 
 * to door_create.  This means that only servers need to deal with the 
 * overhead.  As child processes currently do not inherit any doors, 
 * fork() causes the child to close all local door descriptors, free 
 * the table's storage, and set door_table to NULL.
 */

struct door_data {
	pid_t		target;		/* Server PID */
	server_proc_t	server_proc;	/* Points to server proc */
	void*		cookie;		/* Passed to the above */
	door_attr_t	attr;		/* Attributes */
	door_id_t	id;		/* System-wide unique ID */
	size_t		data_min;	/* Minimum length of input */
	size_t		data_max;	/* Maximum length of input */
};

/* The door table is an array of pointers to door_data structures.  A
 * non-NULL value of door_table[d] means that d is a local door with
 * heap-allocated data.
 */
static struct door_data** door_table = NULL;

/* A thread which attempts to create, resize, destroy or move door_table 
 * must hold the following lock in exclusive mode.  One which attempts 
 * to manipulate individual entries must hold it in shared mode.
 *
 * Initialized (indirectly) by pthread_once(), from door_create().
 */
pthread_rwlock_t door_table_lock;

/* Stores the current value of OPEN_MAX. */
static size_t open_max;

/* Have we already initialized the server? */
static pthread_once_t is_server_ready = PTHREAD_ONCE_INIT;

/* Internal functions with file scope: */

static void prepare_fork_handler(void)
/* Acquires all critical locks before a fork().
 */
{
	if ( 0 != pthread_rwlock_wrlock(&door_table_lock) )
		fatal_system_error(__FILE__, __LINE__, "pthread_rwlock_wrlock");

	return;
}

static void parent_fork_handler(void)
/* Releases all critical locks after a fork().
 */
{
	if ( 0 != pthread_rwlock_unlock(&door_table_lock) )
		fatal_system_error(__FILE__, __LINE__, "pthread_rwlock_unlock");

	return;
}

static void child_fork_handler(void)
/* Close all local doors.  Wipe the door_table, freeing its memory.  
 * Also release the lock on the table, which prepare_fork_handler() 
 * claimed.
 *
 * Upon a fork, pthread_atfork() calls this function in the child 
 * process.
 */
{
	size_t i;

	if (door_table) {
/* Close all local doors.  The table is potentially very large, but 
 * we skip over most of it.
 */
		for ( i = 0; i < open_max; ++i )
			if ( door_table[i] ) {
				free(door_table[i]);
				close(i);
			}

		free(door_table);
		door_table = NULL;
	}

	if ( 0 != pthread_rwlock_unlock(&door_table_lock) )
		fatal_system_error(__FILE__, __LINE__, "pthread_rwlock_unlock");

	return;
}

static void server_init(void)
/* The first time a client calls door_create(), it calls this handler.
 *
 * Currently, this registers a fork handler and initializes 
 * door_table_lock.  It does not initialize door_table itself, even
 * though we must always do that next, because we must be able to detect 
 * whether initialization failed.
 *
 * More to the point, we could have destroyed the table after a fork() 
 * while leaving the once-control triggered and the lock intact.
 */
{
	if ( 0 != pthread_rwlock_init( &door_table_lock, NULL ) )
		fatal_system_error(__FILE__, __LINE__, "pthread_rwlock_init");

	if ( 0 != pthread_atfork( prepare_fork_handler, 
	                          parent_fork_handler,
	                          child_fork_handler )
           )
		fatal_system_error(__FILE__, __LINE__, "pthread_atfork");
}

static door_id_t get_unique_id (void)
/* Returns an identifier intended to be unique among all doors on the
 * system.  The current algorithm generates a 64-bit ID from the
 * following fields: the PID of the calling process (mod 2^19-1), the
 * number of seconds since the Epoch (mod 2^31), and a 14-bit counter
 * shared by all threads of this program.  It can therefore only generate
 * duplicates if: A) the system generates two simultaneous PIDs with the
 * same hash value (out of 524,787 possible, a Mersenne prime), B) the 
 * system has an uptime in excess of 68 years, and happens to recycle 
 * the PID and sequence number during the one second in seven decades 
 * that would cause a collision, or C) the program creates more than 
 * 16,384 doors in one second, despite resource contention for the 
 * counter.
 *
 * A system-wide counter in the kernel might be simpler.
 *
 * TODO: Look into standard UUID generators.
 */
{
	static const unsigned long long seq_period = 16384;
	static const unsigned long long time_period = 2147483648;
	static const unsigned long long pid_modulus = 524787;

	static unsigned short seq_count = 0;
	static pthread_mutex_t seq_lock = PTHREAD_MUTEX_INITIALIZER;

	unsigned long long id;

	id = ((unsigned long long)getpid() % pid_modulus) << 45;
	id |= ((unsigned long long)time(NULL) % time_period) << 14;

	if ( 0 != pthread_mutex_lock(&seq_lock) )
		fatal_system_error(__FILE__, __LINE__, "pthread_mutex_lock");

	++seq_count;
	seq_count %= seq_period;
	id |= (unsigned long long)seq_count;

	if ( 0 != pthread_mutex_unlock(&seq_lock) )
		fatal_system_error(__FILE__, __LINE__, "pthread_mutex_unlock");

	return id;
}

static struct door_data** init_door_table(void)
/* Initializes door_table, setting all of its bits to zero.  If the 
 * value of {OPEN_MAX}, as reported by sysconf(), is more than 0 but 
 * less than 1,024, door_table has {OPEN_MAX} entries.  Otherwise, it 
 * has 1,024 entries, which was the maximum on Solaris 10, and 
 * therefore should suffice for legacy code.  The function always stores 
 * the number of entries in open_max.
 *
 * It locks the table in exclusive mode to prevent a race condition in 
 * which two threads both believe they're creating door_table in 
 * different places.  Therefore, avoid any situation in which a thread 
 * holding the lock waits for door_create(), or both will deadlock.
 *
 * It returns a pointer to door_table on success, or NULL if allocation 
 * failed.
 */
{
	long sys;
	size_t i;

	if ( 0 != pthread_rwlock_wrlock(&door_table_lock) )
		fatal_system_error(__FILE__, __LINE__, "pthread_rwlock_wrlock");

/* Once we hold the lock, if door_table is not NULL, then this thread 
 * was in a race to create the first door, it lost, and another thread 
 * already initialized door_table.  Release the lock (Very important!) 
 * and break here.  Otherwise, proceed.
 */
        if ( NULL == door_table) {
		sys = sysconf(_SC_OPEN_MAX);
/* The reported limit could be less than 0, if the system can't 
 * determine a limit at all, or as high as LONG_MAX, which is an 
 * unreasonable number of entries to allocate.  Allocating more than 
 * {OPEN_MAX} entries wastes space.
 */
		if ( (0 >= sys) || ((long)open_default < sys) )
			open_max = open_default;
		else
			open_max = (size_t)sys;

		door_table =
(struct door_data**)malloc( open_max * sizeof(struct door_data*) );

		if (door_table)
			for ( i = 0; i < open_max; ++i )
				door_table[i] = NULL;

	} /* end if (!door_table) */

/* We must release this lock unconditionally: */
	if ( 0 != pthread_rwlock_unlock(&door_table_lock) )
		fatal_system_error(__FILE__, __LINE__, "pthread_rwlock_unlock");

	return door_table;
}

static struct door_data** resize_door_table( int did )
/* Resize door_table to have at least did entries.
 *
 * The implementation finds a new size greater than or equal to did, 
 * updates open_max to that size, and then resizes the table 
 * accordingly, possibly moving it.  Therefore, it must lock the table 
 * in exclusive mode.
 *
 * It returns a pointer to door_table on success, or NULL if allocation 
 * failed.
 */
{
	size_t i, guess, new_max;
	long sys;

	if ( 0 != pthread_rwlock_wrlock(&door_table_lock) )
		fatal_system_error(__FILE__, __LINE__, "pthread_rwlock_wrlock");

/* Once we hold the lock, if did < open_max, we were in a race to resize 
 * the table, we lost, and we must not truncate door_table.
 */
	if ( did >= open_max ) {
/* Round up to the next kibi. */
		guess = ( (size_t)did + 1024 ) & ~(size_t)1023;

		sys = sysconf(_SC_OPEN_MAX);
		assert( sys > did );

/* Allocating more than {OPEN_MAX} entries wastes space. */
		if ( (0 >= sys) || (guess < (size_t)sys) )
			new_max = guess;
		else
			new_max = (size_t)sys;

/* Rounding up ensures that guess > did.  The assert() ensures that
 * sys > did.  We checked earlier that open_max <= did.  Therefore, 
 * new_max > did >= open_max.
 */
		door_table = (struct door_data**)
realloc( door_table, new_max * sizeof(struct door_data*) );

/* The implementation depends on empty entries being zeroed out. */
		if (door_table) {
			for ( i = open_max; i < new_max; ++i )
				door_table[i] = NULL;

			open_max = new_max;
		} /* end if (door_table) */
	} /* end if (did >= open_max) */

/* We must release this lock unconditionally: */
	if ( 0 != pthread_rwlock_unlock(&door_table_lock) )
		fatal_system_error(__FILE__, __LINE__, "pthread_rwlock_unlock");

	return door_table;
}

static inline void lock_door_table(void)
/* Claims a non-exclusive lock on door_table.  Use this before 
 * manipulating its entries, to prevent another thread from moving it in 
 * memory.
 */
{
	if ( 0 != pthread_rwlock_rdlock(&door_table_lock) ) {
		perror("pthread_rwlock_rdlock");
		exit(EXIT_FAILURE);
	}

	return;
}

static inline void unlock_door_table(void)
/* This small helper function releases a lock on door_table.  I created 
 * it to simplify the program at many places where a function encounters 
 * an error while holding the lock.
 */
{
	if ( 0 != pthread_rwlock_rdlock(&door_table_lock) ) {
		perror("pthread_rwlock_rdlock");
		exit(EXIT_FAILURE);
	}

	return;
}

/* Functions <door.h> exports: */

int door_attach ( int d, const char* path )
/* Attach the door descriptor d to the filesystem, with pathname path.
 * This function replaces fattach(), which does not exist on Linux.  It
 * has one major difference: fattach() expects a file with that pathname
 * to already exist, and does some elaborate things with the original
 * file.  In contrast, door_attach() expects NO file with the same name
 * to exist.  If one does, this function will fail rather than overwrite
 * it or try to move it.
 *
 * Additionally, POSIX fattach() works if you own the file at the 
 * target location or "have appropriate privileges," whereas 
 * door_attach() works if and only if you can create a new socket at the 
 * location.
 *
 * Previously, you would set up file permissions ahead of time.  Now, 
 * you must do so after the door_attach() call.  For reasons of 
 * security, door_attach creates a new door that no one can open.  To 
 * avoid a race condition, it temporarily changes the umask, so it's 
 * not completely thread-safe.
 *
 * The result of calling this function with anything but the descriptor 
 * of a local door as the d argument is undefined.  The function 
 * currently attempts to detect this and report EPERM.
 *
 * The fattach() function allowed you to set up file permissions first.  
 * As door_attach() does not, we initially set the file permissions of 
 * the bound socket to 0, for security reasons.  The server should 
 * immediately change the owner and group if desired, and then enable 
 * read, write and execute permission for the desired users with 
 * fcntl().
 *
 * This function returns 0 on success, or -1 on error, setting errno
 * appropriately.
 *
 * FIXME: Document errno values.
 */
{
	static const int ERROR = -1;
	mode_t old_umask;		/* Used by umask() */

	size_t path_len;
	struct sockaddr_un address;

	if ( NULL == path ) {
		errno = EINVAL;
		return ERROR;
	}

/* Implementations are allowed to do some tricky things with the 
 * sun_path array, but this code should work portably:
 */
	path_len = strlen(path);

	if ( path_len >= sizeof(address.sun_path) ) {
		errno = ENAMETOOLONG;
		return ERROR;
	}

	address.sun_family = AF_UNIX;

/* Using memcpy and an explicit terminator are redundant, individually
 * and together, unless the data change from under us.  In that case,
 * we're already in a state of mortal sin.  I do it anyway, as the cost
 * is well worth making a buffer overrun impossible.
 */
	memcpy( address.sun_path, path, path_len );
	address.sun_path[path_len] = '\0';

	old_umask = umask( S_IRWXU | S_IRWXG | S_IRWXO );

	if ( 0 != bind( d,
	                (const struct sockaddr*)&address,
	                offsetof( struct sockaddr_un, sun_path ) + path_len
	              )
	   ) {
		umask(old_umask);
		return ERROR;
	}

	umask(old_umask);

	return listen( d, SOMAXCONN );
}

int door_create(
	void (*server_procedure) ( void* cookie,
	                           char* restrict argp,
	                           size_t arg_size,
	                           door_desc_t* restrict dp,
	                           uint_t n_desc
	                         ),
	                         void* cookie,
	                         uint_t attributes
              )
/* See the SunOS 5.11 manual for a specification of how this function 
 * behaves.
 *
 * Currently, this implementation does not support any attributes other 
 * than DOOR_REFUSE_DESC, which it implements as a no-op.  It implements 
 * doors as UNIX domain sockets.
 *
 * It can return ERRNO codes of EINVAL (unrecognized attribute or NULL 
 * server procedure), ENOMEM (no memory for internal data structures), 
 * or any value set by socket.
 */
{
	static const int ERROR = -1;
	static const uint_t UNRECOGNIZED = ~(uint_t)(DOOR_REFUSE_DESC);

	int did;		/* The descriptor of the new door */
	int default_buf;	/* Used by getsockopt() */
/* Used by getsockopt(): */
	socklen_t int_length = sizeof(int);
	struct door_data* p;	/* Holds the new table entry. */

	if (
(NULL == server_procedure) || (attributes & UNRECOGNIZED)
	   ) {
		errno = EINVAL;
		return ERROR;
	}

/* If this is the first time we've created a door, initialize the
 * server module.  Among other things, this initializes door_table_lock 
 * so that init_door_table() will work.
 */
	pthread_once( &is_server_ready, server_init );

/* If door_table does not exist, create it. */
	if ( NULL == door_table )
		if ( NULL == init_door_table() ) {
			errno = ENOMEM;
			return ERROR;
		}

/* Doors are currently sockets: */
	if ( 0 > ( did = socket( AF_UNIX, SOCK_SEQPACKET, 0 ) ) )
		return ERROR;

	if ( did >= open_max )
/* Our table is too small.  Better resize. */
		if ( NULL == resize_door_table(did) ) {
			errno = ENOMEM;
			close(did);
			return ERROR;
		}

	if ( 0 != getsockopt( did,
	                      SOL_SOCKET,
	                      SO_RCVBUF,
	                      &default_buf,
	                      &int_length
	                    )
	   ) {
		close(did);
		return ERROR;
	}

/* It makes no sense to keep a door open after exec(), as even if the
 * new program is also a door server, it won't know about this door.
 */
	if ( 0 != fcntl( did, F_SETFL, FD_CLOEXEC ) ) {
		close(did);
		return ERROR;
	}

/* Writes to the table can proceed in shared mode, as two doors being 
 * created simultaneously will not have the same file descriptor, but 
 * another thread must not move the table elsewhere until we finish.
 *
 * Do not attempt to use the door until door_create() has returned, or 
 * we may have a race!
 */
	p = calloc( 1, sizeof(struct door_data) );
	if ( NULL == p ) {
		close(did);
		errno = ENOMEM;
		return ERROR;
	}

	lock_door_table();
	door_table[did] = p;
	unlock_door_table();

	p->target = getpid();
	p->server_proc = server_procedure;
	p->cookie = cookie;
	p->attr = attributes;
	p->id = get_unique_id();
	p->data_min = 0;
	p->data_max = (size_t)default_buf;

	return did;
}

int door_detach ( const char* path )
/* Detaches a door from the specified point in the filesystem.  Unlike 
 * fdetach(), this does not leave anything behind.
 *
 * Additionally, POSIX fdetach() works if you are the owner or "have 
 * appropriate privileges," whereas door_detach() checks instead whether  
 * you can unlink() the door.
 *
 * Currently, this is just a wrapper for unlink().  If client code was 
 * counting on fdetach() to restore a file hidden by fattach(), 
 * door_detach() will not do that!
 *
 * The results of calling this function on anything but an attached door 
 * are undefined.  However, the function makes its best effort to check 
 * for this error and report EPERM, so as to avoid data loss from this 
 * type of programmer error.  It therefore won't remove a file that it 
 * can't stat().
 *
 * Returns 0 on success or -1 on failure, setting errno appropriately.
 *
 * FIXME: document errno values.
 */
{
	static const int ERROR = -1;

	struct stat buf;

	if ( NULL == path ) {
		errno = EINVAL;
		return ERROR;
	}

/* Check that the target exists and is a socket.  Might produce an 
 * unusual errno value.
 */
	if ( 0 == stat( path, &buf ) ) {
		errno = EPERM;
		return ERROR;
	}

	if ( !S_ISSOCK(buf.st_mode) ) {
		errno = EPERM;
		return ERROR;
	}

/* It is not an error to detach a door which you cannot open.  We 
 * could, however, attempt to open it and check socket options as well.
 */

	return unlink(path);
}

int door_getparam (int d, int param, size_t* out)
/* See the SunOS 5.11 manual for a specification of how this function
 * should work.
 *
 * At present, this function is only implemented for local doors.
 */
{
	static const int ERROR = -1;
	static const int SUCCESS = 0;

	const struct door_data* p;

	if ( NULL == out ) {
		errno = EINVAL;
		return ERROR;
	}

	lock_door_table();
	p = door_table[d];
	unlock_door_table();

	if ( ( NULL == door_table ) ||
	     ( d >= open_max ) ||
	     ( 0 > d ) ||
	     ( NULL == p )
	   ) {
		errno = EBADF;
		return ERROR;
	}

	switch (param) {
		case DOOR_PARAM_DATA_MAX:
			*out = p->data_max;
			break;

		case DOOR_PARAM_DATA_MIN:
			*out = p->data_min;
			break;

		case DOOR_PARAM_DESC_MAX:
			*out = 0;
			break;

		default:
			errno = EINVAL;
			return ERROR;
 	}

	return SUCCESS;
}

int door_info (int d, struct door_info* info)
/* See the SunOS 5.11 manual for a specification of how this function 
 * should work.
 *
 * At present, this function is only implemented for local doors.
 */
{
	static const int ERROR = -1;
	static const int SUCCESS = 0;

/* Hope (and assert) that this holds a function pointer: */
	uintptr_t scratch;
	const struct door_data* p;

	if ( NULL == info ) {
		errno = EINVAL;
		return ERROR;
	}

	lock_door_table();
	p = door_table[d];
	unlock_door_table();

	if ( ( NULL == door_table ) ||
	     ( open_max <= d ) ||
	     ( 0 > d ) ||
	     ( NULL == p )
	   ) {
/* Not a local door. */
		errno = EBADF;
		return ERROR;
	}

	info->di_target = p->target;

/* Function pointer casts are not explicitly permitted, so use memcpy().
 */
	assert( sizeof(uintptr_t) == sizeof(server_proc_t) );
	memcpy( &scratch,
	        &(p->server_proc),
	        sizeof(server_proc_t)
	      );
	info->di_proc = (door_ptr_t)scratch;

/* On the other hand, pointers to void do convert to integral types. */
	info->di_data = (door_ptr_t)(p->cookie);

	info->di_attributes = p->attr | DOOR_LOCAL;
	info->di_uniquifier = p->id;

	return SUCCESS;
}

int door_revoke (int d)
/* See the SunOS 5.11 manual for a specification of how this function 
 * works.
 *
 * This implementation marks a door as revoked by marking its server 
 * process as 0.  It additionally sets the DOOR_REVOKED attribute.
 *
 * Currently, this function makes no effort to distinguish between 
 * non-local doors and non-doors.  If door_table[d].info reports 0 as 
 * the server process, you get back EPERM.  Also, it makes no special 
 * effort to guarantee that door operations are atomic.
 */
{
	static const int ERROR = -1;
	static const int SUCCESS = 0;

	lock_door_table();

	if ( ( NULL == door_table ) ||
	     ( open_max <= d ) ||
	     ( 0 > d ) ||
	     ( NULL == door_table[d] )
	   ) {
		errno = EBADF;
		unlock_door_table();
		return ERROR;
	}

	free(door_table[d]);
	door_table[d] = NULL;
	unlock_door_table();

	close(d);

	return SUCCESS;
}

int door_setparam ( int d, int param, size_t val )
/* See the SunOS 5.11 manual for a specification of how this function 
 * should work.
 *
 * At present, DOOR_PARAM_DESC_MAX must be 0.  Additionally, the function 
 * doesn't distinguish between files that aren't doors and doors created 
 * by other processes, so it never reports EPERM.  Changes also affect 
 * only future calls to door_call().
 */
{
	static const int ERROR = -1;
	static const int SUCCESS = 0;

	int scratch;	/* Used by setsockopt() */
	struct door_data* p;

	lock_door_table();
	p = door_table[d];
	unlock_door_table();

	if ( ( NULL == door_table ) ||
	     ( d >= open_max ) ||
	     ( 0 > d ) ||
	     ( NULL == p )
	   ) {
		errno = EBADF;
		return ERROR;
	}

	switch (param) {
/* To change both DOOR_PARAM_DATA_MIN and DOOR_PARAM_DATA_MAX, you must 
 * take care to leave the door in a callable state after each step.
 */
		case DOOR_PARAM_DATA_MAX:
			if ( val < p->data_min ) {
				errno = EINVAL;
				return ERROR;
			}

			if ( INT_MAX < val ) {
				errno = ERANGE;
				return ERROR;
			}

			scratch = (int)val;

			if (
0 != setsockopt( d, SOL_SOCKET, SO_RCVBUF, &scratch, sizeof(int) )
			   ) {
				return ERROR;
			}

			p->data_max = val;
			break;

		case DOOR_PARAM_DATA_MIN:
			if ( val > p->data_max ) {
				errno = EINVAL;
				return ERROR;
			}
			p->data_min = val;
			break;

		case DOOR_PARAM_DESC_MAX:
/* Implicitly 0 at present.  However, we distinguish between a request 
 * that fails because of DOOR_REFUSE_DESC and one that fails because we 
 * just don't support the option yet.
 */
			if ( val > 0 ) {
				if (DOOR_REFUSE_DESC | p->attr)
					errno = ENOTSUP;
				else
					errno = ERANGE;

				return ERROR;
			}
			break;

/* Either the program's buggy, or ahead of this version of the library. 
 */
		default:
			errno = EINVAL;
			return ERROR;
	}

	return SUCCESS;
}
