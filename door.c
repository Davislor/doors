/***************************************************************************
 * Portland Doors                                                          *
 * door_server.c: Server data structures and functions requiring           *
 *                knowledge thereof.                                       *
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
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "door.h"
#include "door_info.h"
#include "error.h"
#include "messages.h"

/* The default size of door_table, used unless {OPEN_MAX} is a lower, 
 * positive number.
 */
static const size_t open_default = 1024U;

/* The value of {PAGE_SIZE}, to be filled in by sysconf(): */
static size_t page_size = 0;

/* Several functions manipulate the door_table, which is an array of 
 * OPEN_MAX fd_data structures.  A file descriptor fd refers to a 
 * valid, local door if and only if door_table[fd] holds valid data.
 *
 * The program allocates memory for the door_table upon the first call 
 * to door_create.  This means that only servers need to deal with the 
 * overhead.  As child processes currently do not inherit any doors, 
 * fork() causes the child to close all local door descriptors, free 
 * the table's storage, and set door_table to NULL.
 */

enum fd_type {
	fd_none = 0;		/* This is not a door descriptor at all. */
	fd_server;		/* Returned by door_create() */
	fd_client;		/* Returned by door_open() */
};

struct fd_data {
	enum fd_type type;	/* The type of data. */
	void* data;		/* A pointer to the data, or NULL. */
};

struct door_data {
	pid_t		target;			/* Server PID */
	door_server_proc_t	server_proc;	/* Points to server proc */
	void*		cookie;			/* Passed to the above */
	door_attr_t	attr;			/* Attributes */
	door_id_t	id;			/* System-wide unique ID */
	size_t		data_min;		/* Minimum length of input */
	size_t		data_max;		/* Maximum length of input */
/* Number of pointers to this structure; each listener thread holds a copy,
 * so we should decrement this reference count and free it only when it hits.
 * 0.  Signed in order to more easily detect underflow.  We don't bother to
 * count the reference in the door_table, since door_revoke() erases that
 * first.
 */
	int		pointers;
	bool		revoked;	/* Has this door been revoked? */
	bool		attachments;	/* Is this thread attached? */
/* Has this door been unreferenced at least once? */
	bool		was_unref;
	pthread_cond_t	can_listen;	/* Has this thread been bound? */
	pthread_mutex_t	lock_data;	/* Own to modify this structure. */
};

struct conn_data {
	pthread_mutex_t	desc_lock	/* A mutex lock on this descriptor. */
}

/* The door table is an array of pointers to door_data structures.  A
 * non-NULL value of door_table[d] means that d is a local door with
 * heap-allocated data.
 */
static struct door_data** door_table = NULL;

/* The thread start procedure that listens to each new connection
 * receives a pointer to one of these structures.  The listen_fd member
 * contains the endpoint to listen to, and the data member points to the
 * associated door's entry in the door_table.
 */
struct door_connect_t {
	int			listen_fd;
	struct door_data*	data_ptr;
};

/* Data the thread calling the door server procedure will need.
 */
struct door_server_args_t {
	int			fd;	
	void*			data_ptr;
	door_desc_t*		desc_ptr;
	size_t			data_size;
	uint_t			desc_num;
	door_server_proc_t	server_proc;
	void*			cookie;
};

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

/* This key tells door_return() which file descriptor should receive
 * the return data.
 *
 * It should be safe to pass by reference a variable from a calling
 * function's stack, as that is thread-specific data whose lifetime has
 * not expired.
 */
static pthread_key_t caller_fd;

/* Each thread handling a door_call() also needs to free its allocated
 * buffer.  We accomplish this by declaring it as a thread-specific key.
*/
static pthread_key_t door_arg_buf;

/* Finally, the thread calling the server procedure needs to free its server
 * argument buffer.
 */
static pthread_key_t server_arg_buf;

/* It doesn't matter what this refers to, only that it's unique: */
const char* const DOOR_UNREF_DATA = { 0 };

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

static void client_init(void)
/* Initializes the client's data structures.  Currently, the first call
 * to door_open() calls this, as any door must be opened before the
 * client can validly do anything else with it.
 */
{
	long x;

	x = sysconf(_SC_PAGE_SIZE);

/* Are our values sane? */
	assert( 0 < x && SIZE_MAX > (unsigned long)x );
/* Should also check that the result is suitable for posix_memalign().
 * I.e., a power of 2 and a multiple of sizeof(void*).
 */

	page_size = (size_t)x;

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
 *
 * This function initializes the thread-specific data door_return()
 * uses.
 */
{
	if ( 0 != pthread_rwlock_init( &door_table_lock, NULL ) )
		fatal_system_error(__FILE__, __LINE__, "pthread_rwlock_init");

	if ( 0 != pthread_atfork( prepare_fork_handler, 
	                          parent_fork_handler,
	                          child_fork_handler )
           )
		fatal_system_error(__FILE__, __LINE__, "pthread_atfork");

	if ( 0 != pthread_key_create( &caller_fd, NULL ) )
		fatal_system_error(__FILE__, __LINE__, "pthread_key_create");

	if ( 0 != pthread_key_create( &door_arg_buf, free ) )
		fatal_system_error(__FILE__, __LINE__, "pthread_key_create");

	if ( 0 != pthread_key_create( &server_arg_buf, free ) )
		fatal_system_error(__FILE__, __LINE__, "pthread_key_create");

	return;
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
 * already initialized door_table.  Release the lock (Very Important!) 
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
	if ( (size_t)did >= open_max ) {
/* Round up to the next kibi. */
		guess = ( (size_t)did + 1024 ) & ~(size_t)1023;

		sys = sysconf(_SC_OPEN_MAX);
/* Don't crash if sysconf() reports -1 (no fixed limit). */
		assert( 0 > sys || sys > did );

/* Allocating more than {OPEN_MAX} entries wastes space. */
		if ( (0 >= sys) || (guess < (size_t)sys) )
			new_max = guess;
		else
			new_max = (size_t)sys;

/* Rounding up ensures that guess > did.  The assert() ensures that
 * sys > did or sys == -1.  We checked earlier that open_max <= did.
 * Therefore, new_max > did >= open_max.
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

static void* start_server_proc( void* p )
/* Invokes the given server procedure based on the arguments in args.
 */
{
	const struct door_server_args_t* args = p;

/* Store the socket fd where door_return() can retrieve it. */
	if ( 0 != pthread_setspecific( caller_fd, &args->fd ) )
		fatal_system_error(__FILE__,__LINE__,"pthread_setspecific");

/* Free the data buffer when this thread exits (outside the critical path). */
	if ( 0 != pthread_setspecific( door_arg_buf, args->data_ptr ) )
		fatal_system_error(__FILE__,__LINE__,"pthread_setspecific");

/* Also free the argument buffer when this thread exits. */
	if ( 0 != pthread_setspecific( server_arg_buf, args ) )
		fatal_system_error(__FILE__,__LINE__,"pthread_setspecific");

	(args->server_proc)( args->cookie,
	                     args->data_ptr,
	                     args->data_size,
	                     args->desc_ptr,
	                     args->desc_num
	                   );

	return NULL;
}

static void* start_unreferenced_invocation_thread( void* p )
/* Calls the door whose information is stored in the door_data structure p
 * points to with special unreferenced invocation arguments.
 */
{
	static const int invalid_fd = -1;

	const door_server_proc_t server_proc =
((struct door_data*)p)->server_proc;
	void* const cookie = ((struct door_data*)p)->cookie;

/* We should set the file descriptor key to an invalid value, just in case the
 * server procedure calls door_return() by mistake.  This key does not attempt
 * to free its target, so we should be safe.
 */
	if ( 0 != pthread_setspecific( caller_fd, &invalid_fd ) )
		fatal_system_error(__FILE__,__LINE__,"pthread_setspecific");

/* The Sun man page says that the dp parameter is 0, not NULL. */
	server_proc( cookie, DOOR_UNREF_DATA, 0, NULL, 0 );

	return NULL;
}

static inline void invoke_unreferenced( struct door_data* p )
/* This function spawns a new thread to deliver an unreferenced invocation to
 * the provided door's sever procedure.
 */
{
	pthread_t thread_id;

	if ( 0 !=
	     pthread_create( &thread_id,
	                     NULL,
	                     start_unreferenced_invocation_thread,
	                     p
	                   )
	   )
		fatal_system_error( __FILE__, __LINE__, "pthread_create" );

	return;
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
	if ( 0 != pthread_rwlock_unlock(&door_table_lock) ) {
		perror("pthread_rwlock_unlock");
		exit(EXIT_FAILURE);
	}

	return;
}

static inline void lock_door_data( struct door_data* p )
/* Acquires a lock on a door_data structure, so that operations on it will be
 * thread-safe and atomic.
 */
{
	assert( NULL != p );

	if ( 0 != pthread_mutex_lock( & p->lock_data ) )
		fatal_system_error( __FILE__, __LINE__, "Lock door_data" );

	return;
}

static inline void unlock_door_data( struct door_data* p )
/* Releases a lock on a door_data structure, so that other operations on it
 * may proceed.
 */
{
	assert( NULL != p );

	if ( 0 != pthread_mutex_unlock( & p->lock_data ) )
		fatal_system_error( __FILE__, __LINE__, "Unock door_data" );

	return;
}

static inline void increment_door_data_pointers( struct door_data* p )
/* Increases the reference count of pointers to the structure by 1, preventing
 * another thread from freeing the structure until we release it.
 *
 * This function does not, repeat, does not lock the data first, unlike
 * release_door_data().  This is to reduce the number of lock/unlock calls and
 * facilitate combining writes into a single atomic operation.  You must call
 * lock_door_data() first and unlock_door_data() afterwards!
 */
{
	assert( NULL != p );

	++p->pointers;
	p->attr &= ~DOOR_IS_UNREF;

	return;
}

static inline void release_door_data( struct door_data* p )
/* Decrements the reference count of pointers to the structure by 1.  If the
 * new reference count is 0, this function frees all memory allocated to the
 * structure and its members.
 *
 * This function does, repeat, does lock the data first, unlike
 * increment_door_data_pointers().  This is so that the caller does not need
 * to check whether p is still valid and unlock the data conditionally.  It
 * means that the calling thread must not own the lock, or the program will
 * deadlock!
 *
 * On return, either p will no longer be valid, or the calling thread will not
 * own its lock.  In either case, the caller must not attempt to unlock the
 * data.
 */
{
	assert( NULL != p );

	lock_door_data(p);

/* If the reference count is less than 0, we released the data more times than
 * we referenced it, a serious logic error.  The pointers member is signed
 * in order to detect this.
 */
	assert( 0 <= p->pointers );

	if ( 0 == p->pointers ) {
/* We hold the last copy of the data.  We can and should safely free it.
 *
 * According to the POSIX spec, attempting to destroy a pthread_cond_t that
 * other threads are waiting on causes undefined behavior.  Because there are
 * no other copies of p left, however, there must be no such threads.
 * Likewise, we must free the lock because destroying an owned lock causes
 * undefined behavior.  Because there are no other copies of p left, there
 * must not be any other threads waiting to grab the lock.
 */
		pthread_cond_destroy( & p->can_listen );
		unlock_door_data(p);
		pthread_mutex_destroy( & p->lock_data );
		free(p);
	}
	else if ( ( ! p->revoked ) &&
	          ( 2 == p->pointers ) &&
	          ( ( DOOR_UNREF_MULTI & p->attr ) ||
	            ( ( DOOR_UNREF & p->attr ) && ( ! p->was_unref ) )
	          )
	        ) {
/* We should send an unreferenced invocation. */
		--p->pointers;
		p->was_unref = true;
		p->attr |= DOOR_IS_UNREF;

		unlock_door_data(p);
		invoke_unreferenced(p);
	}
	else {
		--p->pointers;
		unlock_door_data(p);
	}

	return;
}

static inline void handle_door_call( int fd, struct door_data* p )
/* Reads a msg_door_call message from the connected socket fd, calls
 * that thread's server procedure with the correct parameters.
 */
{
	struct msg_door_call incoming;
	void* argp = NULL;
	ssize_t arg_size;
	struct iovec read_iovs[2];
	struct msghdr read_hdr;
	struct door_server_args_t* arg_ptr;
	pthread_t thread_id;

	if ( 0 > recv( fd, &incoming, sizeof(incoming), MSG_PEEK ) ) {
		return;
	}

	if ( ! is_msg_door_call(&incoming) ) {
/* If we're here and the next message isn't a door_call, something
 * broke.  (Eliminate this check for speed?)
 */
		xmit_error( fd, EBADMSG );
		return;
	}

	arg_size = msg_door_call_get_arg_size(&incoming);

	lock_door_data(p);
	if ( 0 > arg_size ||
	     p->data_max < (size_t)arg_size ||
	     p->data_min > (size_t)arg_size
	   ) {
		unlock_door_data(p);
		xmit_error( fd, ENOBUFS );
		return;
	}
	else
		unlock_door_data(p);

	if ( 0 != arg_size ) {
		argp = malloc((size_t)arg_size);

		if ( NULL == argp ) {
			xmit_error( fd, ENOBUFS );
			return;
		}
	}
/* We have stored the socket to send the results to where door_return()
 * can retrieve it.  We know that arg_size is an appropriate amount of
 * data; furthermore, if it is nonzero, argp points to a buffer large
 * enough to hold it, and which will automatically be freed upon thread
 * cancellation.
 */
	bzero( read_iovs, 2*sizeof(struct iovec) );
	bzero( &read_hdr, sizeof(read_hdr) );

	read_hdr.msg_iov = read_iovs;
	read_hdr.msg_iovlen = 2;

	read_iovs[0].iov_base = &incoming;
	read_iovs[0].iov_len = sizeof(incoming);

	read_iovs[1].iov_base = argp;
	read_iovs[1].iov_len = (size_t)arg_size;

	if ( (ssize_t)sizeof(struct msg_door_call) + arg_size !=
	     recvmsg( fd, &read_hdr, 0 )
	   ) {
		xmit_error( fd, EBADMSG );
		return;
	}

/* Handle the door call asynchronously, so as not to block the socket.  (Also,
 * this allows door_return() to keep track of which call it's returning from
 * using thread-specific data.
 */
	arg_ptr = malloc( sizeof(struct door_server_args_t) );
	if ( NULL == arg_ptr ) {
		xmit_error( fd, ENOBUFS );
		return;
	}

	arg_ptr->fd = fd;
	arg_ptr->data_ptr = argp;
	arg_ptr->data_size = (size_t)arg_size;
	arg_ptr->desc_ptr = NULL;
	arg_ptr->desc_num = 0;
/* No other function alters these data members during the door's lifetime.
 * Therefore, we do not need to lock the data to prevent another process from
 * writing to them while we are reading.
 */
	arg_ptr->server_proc = p->server_proc;
	arg_ptr->cookie = p->cookie;

	if ( 0 != pthread_create( &thread_id, NULL, start_server_proc, arg_ptr ) )
		fatal_system_error(__FILE__,__LINE__,"pthread_create");

	return;
}

static inline void handle_msg_request( int fd, struct door_data* p )
/* Reads a request message from the connected socket fd, generates a message
 * based on the information to which p points, and transmits that message
 * back.
 *
 * Transmits back an error message (EINVAL) if it does not recognize the
 * request.
 */
{
	struct msg_request incoming;

	if ( 0 > recv( fd, &incoming, sizeof(incoming), 0 ) ) {
		return;
	}

	if ( ! is_msg_request(&incoming) ) {
/* If it's not an informational request, we shouldn't be here. */
		xmit_error( fd, EBADMSG );
		return;
	}

	switch ( msg_request_decode(&incoming) ) {
		case 0: { /* door_info */
			struct msg_door_info outgoing;

			lock_door_data(p);	/* Necessary? */
			msg_door_info_init( &outgoing,
			                    p->target,
			                    p->server_proc,
			                    p->cookie,
			                    p->attr,
			                    p->id
			                  );
			unlock_door_data(p);

			send( fd, &outgoing, sizeof(outgoing), MSG_EOR );
			break;
		}
		case 1: { /* data_max */
			struct msg_door_getparam outgoing;

			lock_door_data(p);
			msg_door_getparam_init( &outgoing, 1, p->data_max );
			unlock_door_data(p);
			send( fd, &outgoing, sizeof(outgoing), MSG_EOR );
			break;
		}
		case 2: { /* data_min */
			struct msg_door_getparam outgoing;

			lock_door_data(p);
			msg_door_getparam_init( &outgoing, 2, p->data_min );
			send( fd, &outgoing, sizeof(outgoing), MSG_EOR );
			unlock_door_data(p);
			break;
		}
		case 3: { /* desc_max */
			struct msg_door_getparam outgoing;

/* The implementation does not yet support descriptor passing. */
			msg_door_getparam_init( &outgoing, 3, 0 );
			send( fd, &outgoing, sizeof(outgoing), MSG_EOR );
			break;
		}
		default: { /* Bad or unknown request! */
			xmit_error( fd, EINVAL );
		}
	}

	return;
}

static void* connection_listen( void* connection_ptr )
/* Listen for messages on the given connection.  The argument is a
 * pointer to a door_connect_t structure.  This thread must free it when
 * finished.
 *
 * It performs little or no argument-checking and always "returns" NULL.
 */
{
	const int fd = ((struct door_connect_t*)connection_ptr)->listen_fd;
	struct door_data* const p =
((struct door_connect_t*)connection_ptr)->data_ptr;
	long long int code;	/* The incoming message code. */

/* Since we've already copied the data to local storage, we no longer need
 * the buffer.
 */
	free(connection_ptr);

/* Peek ahead at the type of the next message. */
	while ( 0 <= ( code = message_type(fd) ) ) {
		switch (code) {
			case code_request:
				handle_msg_request( fd, p );
				break;
			case code_door_call:
				handle_door_call( fd, p );
				break;
			default: {
				xmit_error( fd, ENOTSUP );
/* We could recover from this error. We could at least linger.  At present, we
 * just close the socket.
 */
				close(fd);
			}
		}
	}

/* Our attempt to read a request code failed.  Could this be because the
 * connection no longer exists?
 */

	release_door_data(p);
	return NULL;
}

static void* door_listen( void* int_ptr )
/* This function listens on the door descriptor pointed to by d_ptr
 * (a pointer to const int), and spawns a new thread to listen on each 
 * connection to this descriptor.
 *
 * When the door closes (or becomes invalid), accept() will fail, and 
 * the thread will terminate itself, calling free() on d_ptr.
 *
 * This function is intended as an argument to pthread_create().  It
 * does not handle bad arguments robustly (as they are programming
 * errors).  It always "returns" NULL.
 */
{
	const int d = *(const int*)int_ptr;	/* Our door descriptor. */
/* Keep a reference to the door's associated data. */
	struct door_data* p;
	int endpoint;

/* Since we've already copied the data to local storage, we no longer need 
 * the buffer.
 */
	free(int_ptr);

	lock_door_table();
	p = door_table[d];
	unlock_door_table();	

	if ( NULL == p ) {
/* We lost a race with door_revoke(), and the door is no longer valid. */
		return NULL;
	}

	lock_door_data(p);
	increment_door_data_pointers(p);
	unlock_door_data(p);

/* If DOOR_PRIVATE is implemented, we should ensure that only one server
 * thread listens at a time.  Currently, it isn't.
 */

	while ( ! p->revoked ) {
		while ( ! p->attachments ) {
/* Wait for another thread to call listen() on the door.  The POSIX standard
 * requires us to own the mutex lock when we call posix_cond_wait().  That
 * function will atomically release the lock if it blocks, so door_attach() 
 * won't deadlock.
 *
 * The case where we will need to loop more than once here is extremely rare,
 * whereas the case where we skip this loop entirely is common, so I bring the
 * locking and unlocking inside the loop to optimize the latter.
 */
			lock_door_data(p);

			if ( 0 != pthread_cond_wait( &p->can_listen,
			                             &p->lock_data
			                           )
			   ) {
				fatal_system_error( __FILE__,
				                    __LINE__,
				                    "pthread_cond_wait"
			                  );
			} /* end if ( 0 != pthread_cond_wait(...) ) */
			if ( p->revoked ) {
/* A call to door_revoke() woke us.  Terminate the thread. */
				unlock_door_data(p);
				release_door_data(p);
				return NULL;
			} /* end if (p->revoked) */
			else
				unlock_door_data(p);
		} /* end while ( ! p-> attachments ) */

/* The p->attachments predicate is true, meaning that the door is ready to
 * accept connections.  We've unlocked the door_table entry, so other threads
 * may now modify it without deadlock.
 */
		while ( 0 <= ( endpoint = accept( d, NULL, 0 ) )
		       ) {
/* We have a new connection.  Spawn another thread to listen on it.  The
 * door_revoke() function closes the file descriptor, which should cause
 * accept() to fail.
 */
/* Information the new thread will need, and free: */
			struct door_connect_t* arg;
/* We'll get back the thread ID, but not keep it. */
			pthread_t thread_id;

			arg =
(struct door_connect_t*)malloc(sizeof(struct door_connect_t));
			if ( NULL == arg )
				return NULL;

			arg->listen_fd = endpoint;
			arg->data_ptr = p;

			lock_door_data(p);
			increment_door_data_pointers(p);
			unlock_door_data(p);

			if (
0 != pthread_create( &thread_id, NULL, connection_listen, (void*)arg )
			) {
/* No one's listening!  Close the connection.  Also, there's no one
 * else to free arg, so do that now.
 */
				free(arg);
				close(endpoint);
				release_door_data(p);
			} /* end if */
		} /* end while ( 0 <= accept() ) */
/* If accept() reports EINVAL, that means that our descriptor is no longer
 * accepting connections.  If that isn't because it's been revoked, we should
 * wait for it to be re-attached with door_attach(). 
 */
		if ( EINVAL == errno )
			p->attachments = false;

	} /* end while ( ! p->revoked ) */

/* Our door has been revoked, or failed unrecoverably. */
	release_door_data(p);
	return NULL;
}

static int spawn_door_server( int d )
/* Spawn a thread to listen on door descriptor d for incoming 
 * connections.  The new thread and its children attempt to block all signals.
 *
 * Copies d, which has an unknown lifetime, to automatic storage.  The
 * new thread must free() this storage before it terminates.
 *
 * Returns 0 on success, -1 on failure, and sets errno.
 */
{
	static const int ERROR = -1;
	int* p;
	pthread_t thread_id;
	sigset_t all_signals;
	sigset_t old_mask;
	int retval;

	if ( d < 0 || (size_t)d >= open_max ) {
		errno = EBADF;
		return ERROR;
	}


	p = (int*)malloc(sizeof(int));

	if ( NULL == p ) {
		errno = ENOMEM;
		return ERROR;
	}

	*p = d;

	sigfillset(&all_signals);

	pthread_sigmask( SIG_BLOCK, &all_signals, &old_mask );
	retval = pthread_create( &thread_id, NULL, door_listen, (void*)p );
	pthread_sigmask( SIG_SETMASK, &old_mask, NULL );

	return retval;
}

static inline struct door_data* local_door_data( int d )
/* If d is the descriptor of a local door, return a pointer to its associated
 * door_data structure.  Otherwise, return NULL.
 *
 * Returns the pointer, rather than true, to avoid a race condition in which
 * a door might be revoked after we return true, but before we copy the
 * pointer.
 */
{
	struct door_data* retval;

	if ( NULL == door_table )
		return NULL;

	lock_door_table();

	if ( open_max <= (size_t)d )
		retval = NULL;
	else
		retval = door_table[d];

	unlock_door_table();

	return retval;
}

/* Functions <door.h> exports: */

int door_attach( int d, const char* path )
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
 * chmod() or the like.
 *
 * This function returns 0 on success, or -1 on error, setting errno
 * appropriately.
 *
 * FIXME: Document errno values.
 */
{
	static const int SUCCESS = 0, ERROR = -1;
	mode_t old_umask;		/* Used by umask() */

	size_t path_len;
	struct sockaddr_un address;
	struct door_data* p;

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

	if ( 0 != listen( d, SOMAXCONN ) )
		return ERROR;

	p = local_door_data(d);

	if ( NULL == p ) {
		errno = EBADF;
		return ERROR;
	}

/* Now, we can tell the listening thread to listen.  The POSIX standard tells
 * us to own this mutex when we call pthread_cond_signal() if we want 
 * "predictable scheduling behavior."
 */
	lock_door_data(p);

/* If door_bind() is implemented, there may be more than one listener thread.
 * The pthreads library requires them to detect if more than one has woken up,
 * but still, wake up as few unwanted threads as possible.
 */
	p->attachments = true;	/* Should change this to a reference count. */

	if ( 0 != pthread_cond_signal(&p->can_listen) )
		fatal_system_error(__FILE__,__LINE__,"pthread_cond_signal");

	unlock_door_data(p);

	return SUCCESS;
}

int door_call( int door, door_arg_t* params )
/* See the SunOS 5.11 manual for a specification of how this function
 * should work.
 *
 * Known bugs:
 * - File descriptor passing not supported yet.  All doors have a
 * DOOR_PARAM_DESC_MAX parameter of 0, and cannot increase it.
 * - If the client is multi-threaded and responses come back in a
 * different order than they were sent, this function will fail.
 * - If the client is multi-threaded and attempts to get a response from
 * the server while another door_call is in progress, this function can
 * fail.
 * - Cancellation is not supported.
 *
 * Differences between this implementation and Sun's include:
 * - The SunOS 5.11 man page says, "If the results of a door invocation
 * exceed the size of the buffer specified by rsize, the system
 * automatically allocates a new buffer in the caller's address space."
 *
 * While not a literal incompatibility, this implementation explicitly 
 * reserves the right to allocate a new buffer for any call, even if the 
 * resulting data are not larger than the buffer.  Future versions might 
 * add a flags member to params, to tweak its memory-handling.  For the 
 * moment, if the buffer is heap-allocated and not garbage-collected, 
 * you should compare rbuf to its original value upon return, and free 
 * the original buffer if necessary.
 * - The SunOS 5.11 man page says to deallocate a library-allocated
 * buffer using munmap().  In this implementation, the preferred method
 * is free().  However, the implementation currently allocates page-
 * aligned buffers using posix_memalign(), so either method should work.
 * - The values of errno might differ from those listed under some
 * circumstances.
 *
 * This function returns 0 on success or -1 on failure, setting errno
 * appropriately.  A partial list of errno values (FIXME: document more
 * fully):
 * - EBADMSG: The client received an inappropriate message in response.
 * - EFAULT: The user passed in a NULL data buffer with a non-zero data
 * size, or a NULL descriptor list with a non-zero number of 
 * descriptors.
 * - ENFILE: The user passed in too many descriptors (currently, more
 * than zero).
 * - ENOMEM: The server returned too much data for us to store.
 */
{
	static const int ERROR = -1, SUCCESS = 0;
	struct msg_door_call outgoing;
	long long int incoming_code;

	if ( NULL == params || 0 == params->data_size ) {
/* There are no parameters. */
		msg_door_call_init( &outgoing, 0 );
		send( door, &outgoing, sizeof(outgoing), MSG_EOR );
	}
	else {
/* Within this block, we know that params != NULL. */

		if ( ( NULL == params->data_ptr ) ||
		     ( NULL == params->rbuf && 0 != params->rsize )
		   ) {
/* The caller passed in an invalid buffer.  It is not an error to call
 * a door with a non-NULL params and a NULL params->data_ptr, as this
 * correctly indicates that the door takes no data, but may return
 * some.  However, the function thinks it's passing in actual data, so
 * something's gone wrong.  It is likewise not an error to pass in a
 * NULL params->rbuf, as this indicates that the system should allocate
 * a results buffer if we need one.  However, if the caller thinks that
 * NULL points to a valid buffer, something's gone wrong.  We can
 * recover, but better to point out the logic error.
 */
			errno = EFAULT;
			return ERROR;
		}

		if ( 0 != params->desc_num ) {
/* The caller tried to pass in door descriptors, which are not
 * yet supported.  This implementation considers all doors to accept a
 * maximum of zero descriptors.
 */
			errno = ENFILE;
			return ERROR;
		}
		else {
/* The caller has data to pass in, and that data is in a non-NULL
 * buffer.
 */
			struct iovec send_iovs[2];
			struct msghdr send_hdr;

			bzero( &send_iovs[0], 2*sizeof(struct iovec) );
			bzero( &send_hdr, sizeof(send_hdr) );

			send_hdr.msg_iov = send_iovs;
			send_hdr.msg_iovlen = 2;

			send_iovs[0].iov_base = &outgoing;
			send_iovs[0].iov_len = sizeof(outgoing);

			send_iovs[1].iov_base = (void*)params->data_ptr;
			send_iovs[1].iov_len = params->data_size;

			msg_door_call_init( &outgoing,
			                    params->data_size
			                  );

			sendmsg( door, &send_hdr, MSG_EOR );
		} /* end if (Passed in any descriptors?) */
	} /* end if (Passed in any params?) */
/* We've now sent the message, and await a msg_door_return in reply. */

	incoming_code = message_type(door);
	if ( 0 > incoming_code )
		return ERROR;
	else if ( code_error == incoming_code ) {
/* We received an error message back. */
		struct msg_error incoming;

		if (
0 > recv( door, &incoming, sizeof(incoming), MSG_WAITALL )
		   )
			return ERROR;

		errno = msg_error_decode(&incoming);
		return ERROR;
	} /* end if ( code_error == incoming_code ) */
	else if ( code_door_return == incoming_code ) {
/* The server responded with a door_return message, as expected. */
		struct msg_door_return incoming;
		ssize_t return_size, bytes_read;
		void* return_buf = NULL;
		bool new_buffer = false;
		struct iovec recv_iovs[2];
		struct msghdr recv_hdr;

		if (
0 > recv( door, &incoming, sizeof(incoming), MSG_PEEK ) 
		   )
			return ERROR;

		return_size = msg_door_return_get_data_size(&incoming);

		if ( NULL == params ) {
/* We cannot receive any data. */
			if ( 0 != return_size ) {
				errno = ENOMEM;
				return ERROR;
			} /* end if ( 0 != return_size ) */

			return SUCCESS;
		} /* end if ( NULL == params ) */

		if ( 0 > return_size ) {
/* The door returned too much data for us to even address! */
			errno = ENOMEM;
			params->data_size = 0;
			return ERROR;
		}

		if ( (size_t)return_size > params->rsize ) {
/* Allocate a new buffer. */
			if ( 0 != posix_memalign( &return_buf,
			                          page_size,
			                          (size_t)return_size
			                        )
			   ) {
				params->data_size = 0;
				errno = ENOMEM;
				return ERROR;
			}
			new_buffer = true;
		} /* end if ( (size_t)return_size > params->rsize ) */
		else
			return_buf = params->rbuf;

/* The return_buf variable now points to a buffer big enough to hold 
 * the requested data.
 */
		bzero( recv_iovs, 2*sizeof(struct iovec) );
		bzero( &recv_hdr, sizeof(recv_hdr) );

		recv_hdr.msg_iov = recv_iovs;
		recv_hdr.msg_iovlen = 2;

		recv_iovs[0].iov_base = &incoming;
		recv_iovs[0].iov_len = sizeof(incoming);

		recv_iovs[1].iov_base = return_buf;
		recv_iovs[1].iov_len = (size_t)return_size;

		bytes_read = recvmsg( door, &recv_hdr, MSG_WAITALL );

		if ( (ssize_t)sizeof(incoming) + return_size !=
		     bytes_read
		   ) {
/* Failed to read the data that should be there. */
			if (new_buffer)
				free(return_buf);

			params->rsize = 0;
			errno = EBADMSG;
			return ERROR;
		} /* end if( bytes_read < return_size ) */
		else {
/* We read the correct amount of data. */
			params->rbuf = return_buf;
			params->data_ptr = return_buf;
			params->rsize = (size_t)return_size;
			params->data_size = (size_t)return_size;

			return SUCCESS;
		} /* end if ( number of bytes read ). */
	} /* end if ( type of message received ) */

/* We received the wrong kind of message. */
	close(door);
	errno = EBADMSG;
	return ERROR;
}

int door_close( int d )
/* Drop-in replacement for close().  Currently, just a wrapper for
 * close(), but I anticipate needing to add per-door data structures in
 * the future.  If so, we will need this function to avoid leaking
 * memory, and it will be difficult to get programmers to switch to it
 * after using close().
 */
{
	return close(d);
}

int door_open( const char* path )
/* Drop-in replacement for open().  Currently, this opens a door 
 * descriptor, which is a socket connected to the UNIX domain socket at 
 * the requested pathname.
 */
{
	static const int ERROR = -1;
	static pthread_once_t once_control = PTHREAD_ONCE_INIT;
	struct sockaddr_un address;	/* Address to connect to */
/* File descriptor of the new door: */
	int d;
	size_t path_len;		/* Length of path. */

	pthread_once( &once_control, client_init );

	if ( NULL == path ) {
		errno = EINVAL;
		return ERROR;
	}

	d = socket( AF_UNIX, SOCK_SEQPACKET, 0 );
	if ( 0 > d )
		return ERROR;

	path_len = strlen(path);
	if ( offsetof( struct sockaddr_un, sun_path ) + path_len >=
	     sizeof(struct sockaddr_un)
	   ) {
		errno = ENAMETOOLONG;
		return ERROR;
	}

	address.sun_family = AF_UNIX;
	memcpy( &address.sun_path, path, path_len );
	address.sun_path[path_len] = '\0';

	if ( 0 != connect( d,
	                   (const struct sockaddr*)&address, 
	                   offsetof( struct sockaddr_un, sun_path ) + path_len
	                 )
	   ) {
		close(d);
		return ERROR;
	}

	fcntl( d, F_SETFL, FD_CLOEXEC );

	return d;
}

int door_create( door_server_proc_t server_procedure,
                 void* cookie,
                 door_attr_t attributes
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
	static const uint_t UNRECOGNIZED =
~( DOOR_REFUSE_DESC | DOOR_UNREF | DOOR_UNREF_MULTI );

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

	if ( (size_t)did >= open_max )
/* Our table is too small.  Better resize. */
		if ( NULL == resize_door_table(did) ) {
			close(did);
			errno = ENOMEM;
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

	assert( default_buf > (int)DOOR_CALL_RESERVED );

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
	p = (struct door_data*)calloc( 1, sizeof(struct door_data) );
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
	p->data_max = (size_t)default_buf - DOOR_CALL_RESERVED;
	p->attachments = false;
	p->revoked = false;
	p->pointers = 0;
	p->was_unref = false;
	pthread_cond_init( &p->can_listen, NULL );
	pthread_mutex_init ( &p->lock_data, NULL );

	if ( 0 != spawn_door_server(did) ) {
		close(did);
		return ERROR;
	}

/* Perhaps sync with the listener thread here, to prevent races? */

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
	if ( 0 != stat( path, &buf ) ) {
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

/* Code to decrement the attachment count goes here. */

	return unlink(path);
}

int door_getparam (int d, int param, size_t* out)
/* See the SunOS 5.11 manual for a specification of how this function
 * should work.
 */
{
	static const int ERROR = -1;
	static const int SUCCESS = 0;

	struct door_data* p;

	if ( param < DOOR_PARAM_DATA_MAX ||
	     param > DOOR_PARAM_DESC_MAX 
	   ) {
		errno = EINVAL;
		return ERROR;
	}

	if ( NULL == out ) {
		errno = EINVAL;
		return ERROR;
	}

	p = local_door_data(d);

	if ( NULL == p ) {
/* Not a local door. */
		struct msg_request outgoing;
		uint32_t type;

		msg_request_init( &outgoing, param );
		send( d, &outgoing, sizeof(outgoing), MSG_EOR );

		if ( 0 > recv( d, &type, sizeof(type), MSG_PEEK ) )
			return ERROR;

		if ( code_door_getparam == type ) {
			struct msg_door_getparam incoming;

			if (
0 > recv( d, &incoming, sizeof(incoming), 0 )
			   )
				return ERROR;

			*out =  msg_door_getparam_decode(&incoming);
		}
		else if ( code_error == type ) {
			struct msg_error incoming;

			if (
0 > recv( d, &incoming, sizeof(incoming), 0 )
			   )
				return ERROR;

			errno = msg_error_decode(&incoming);
			return ERROR;
		} /* end if (message type) */

		return SUCCESS;
	}

/* A local door. */
	switch (param) {
		case DOOR_PARAM_DATA_MAX:
			lock_door_data(p);
			*out = p->data_max;
			unlock_door_data(p);
			break;

		case DOOR_PARAM_DATA_MIN:
			lock_door_data(p);
			*out = p->data_min;
			unlock_door_data(p);
			break;

		case DOOR_PARAM_DESC_MAX:
			*out = 0;
			break;
/* We already tested that param is one of those three. */
 	}

	return SUCCESS;
}

int door_info( int d, struct door_info* info )
/* See the SunOS 5.11 manual for a specification of how this function 
 * should work.
 */
{
	static const int ERROR = -1;
	static const int SUCCESS = 0;

	struct door_data* p;

	if ( NULL == info ) {
		errno = EINVAL;
		return ERROR;
	}

	p = local_door_data(d);

	if ( NULL == p ) {
/* Not a local door. */
		struct msg_request outgoing;
		long long int code;

		msg_request_init( &outgoing, REQ_DOOR_INFO );
		if ( 0 > send( d, &outgoing, sizeof(outgoing), MSG_EOR ) )
			return ERROR;

		code = message_type(d);

		if ( code_door_info == code ) {
			struct msg_door_info incoming;

			if (
0 > recv( d, &incoming, sizeof(incoming), 0 )
			   )
				return EBADMSG;

			msg_door_info_decode( &incoming, info );

			if ( getpid() == (pid_t)info->di_target )
				info->di_attributes |= DOOR_LOCAL;

			return SUCCESS;
		}
		else if ( code_error == code ) {
			struct msg_error incoming;

			if (
0 > recv( d, &incoming, sizeof(incoming), 0 )
			   )
				return EBADMSG;

			errno = msg_error_decode(&incoming);
			return ERROR;
		}
		else {
			errno = EBADF;
			return ERROR;
		}
	}

/* A local door. */

	lock_door_data(p);	/* Necessary? */
	info->di_target = p->target;
	info->di_proc = (door_ptr_t)fptr2u64(p->server_proc);
	info->di_data = (door_ptr_t)optr2u64(p->cookie);
	info->di_attributes = p->attr | DOOR_LOCAL;
	info->di_uniquifier = p->id;
	unlock_door_data(p);

	return SUCCESS;
}

int door_return( void* restrict data_ptr,
                 size_t data_size,
                 door_desc_t* restrict desc_ptr,
                 uint_t num_desc
               )
/* See the SunOS 5.11 man page for a specification for how the function
 * should work.
 *
 * Known bugs:
 * - Passing door descriptors is not supported yet.  Any attempt to do
 * so will fail with EMFILE.
 *
 * Known incompatibilities:
 * - The pointer arguments now have the restrict qualifier.  No sane
 * code should ever have returned an array of door_desc_t structures as
 * an argument anyway.
 *
 * Other limitations:
 * - There should be a way to tell the library to free a 
 * dynamically-allocated buffer.  (Probably a separate function.)
 */
{
	static const int ERROR = -1;
	int fd;
	struct msg_door_return outgoing;
	struct iovec send_iovs[2];
	struct msghdr send_hdr;

	if ( ( NULL == data_ptr && 0 != data_size ) ||
	     ( NULL == desc_ptr && 0 != num_desc )
	   ) {
		errno = EFAULT;
		return ERROR;
	}

	if ( 0 != num_desc ) {
		errno = EMFILE;
		return ERROR;
	}

	fd = *(int*)pthread_getspecific(caller_fd);

	msg_door_return_init( &outgoing, data_size );

	bzero( send_iovs, 2*sizeof(struct iovec) );
	bzero( &send_hdr, sizeof(send_hdr) );

	send_hdr.msg_iov = send_iovs;
	send_hdr.msg_iovlen = 2;

	send_iovs[0].iov_base = &outgoing;
	send_iovs[0].iov_len = sizeof(outgoing);

	send_iovs[1].iov_base = data_ptr;
	send_iovs[1].iov_len = data_size;

	if ( 0 > sendmsg( fd, &send_hdr, MSG_EOR ) ) {
		errno = EINVAL;
		return ERROR;
	}

/* Everything worked, so kill this thread. */
	pthread_exit(NULL);

/* NOTREACHED */
}

int door_revoke( int d )
/* See the SunOS 5.11 manual for a specification of how this function 
 * works.
 *
 * This implementation marks the given door as revoked, wakes up all threads
 * listening to that door so that they can immediately detect this fact and
 * terminate, and decrements the reference count on the data, so that the last
 * thread to release its copy of the door data will free it properly.
 *
 * At present, established connections will not immediately abort when a door
 * is revoked; a call in progress may even complete.
 *
 * Currently, this function makes no effort to distinguish between 
 * non-local doors and non-doors.  If door_table[d].info reports 0 as 
 * the server process, you get back EPERM.
 */
{
	static const int ERROR = -1;
	static const int SUCCESS = 0;

	struct door_data* p;

	lock_door_table();

	p = local_door_data(d);

	if ( NULL == p ) {
		errno = EBADF;
		return ERROR;
	}

	close(d);

	p->revoked = true;
	pthread_cond_broadcast( & p->can_listen );
	unlock_door_data(p);

	release_door_data(p);

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

	p = local_door_data(d);

	if ( NULL == p ) {
/* Not a local door. */
		errno = EBADF;
		return ERROR;
	}

/* Possible race with door_revoke setting door_table[d] to NULL? */

/* A local door. */
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

			scratch = (int)val + DOOR_CALL_RESERVED;

			if (
0 != setsockopt( d, SOL_SOCKET, SO_RCVBUF, &scratch, sizeof(int) )
			   ) {
				return ERROR;
			}

			lock_door_data(p);
			p->data_max = val;
			unlock_door_data(p);
			break;

		case DOOR_PARAM_DATA_MIN:
			if ( val > p->data_max ) {
				errno = EINVAL;
				return ERROR;
			}
			lock_door_data(p);
			p->data_min = val;
			unlock_door_data(p);
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
