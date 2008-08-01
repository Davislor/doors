/***************************************************************************
 * Portland Doors                                                          *
 * door.h: Include file for the Doors library.                             *
 *         This file exposes the Doors interfaces to client code.  It does *
 *         not currently include any feature-test macros, as it shouldn't  * 
 *         conflict with client declarations.                              *
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

/* This implementation of the library should be mostly source-compatible
 * with Doors on Solaris.  The most important difference is that
 * door_attach() replaces fattach() and requires that you NOT create
 * another file with the same pathname first.  Likewise, door_detach()
 * replaces fdetach() and removes the door from the filesystem.  There is a
 * similar pair of replacements for open() and close(), door_open() and
 * door_close().
 *
 * This header file reserves all identifiers beginning with door_, 
 * DOOR_, _door_, and _DOOR_ for future use.
 */

#ifndef _DOOR_H
#define _DOOR_H

/* GCC 4 supports the restrict keyword, but does not define __STDC_VERSION. */
#if ( !defined(__STDC_VERSION) || (__STDC_VERSION < 199901L) ) && !defined(__GNUC__)
#define restrict /**/
#endif

/* Size_t and pid_t are in <sys/types.h>. */
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The uint_t type is not defined by POSIX, or in the Linux standard 
 * headers.  Solaris and at least one part of the Linux kernel define it 
 * as unsigned int.
 *
 * Pace Greg Kroah-Hartman, these typedefs are all necessary to 
 * support legacy code.
 */
typedef unsigned int		uint_t;
typedef unsigned long long	door_id_t;
typedef unsigned int		door_attr_t;

/* A door_ptr_t must be large enough to hold either an object pointer 
 * or a function pointer for either 32-bit or 64-bit code.  This 
 * implementation represents them according to their native type on the 
 * server side, converts them to 64-bit unsigned integers in an 
 * unspecified manner, and then exposes them to clients as a standard 
 * type long enough to hold them, currently unsigned long long int.
 *
 * Again, the typedef is necessary to support legacy code.
 */
typedef unsigned long long int	door_ptr_t;

/* Placeholders */
typedef struct door_desc_t	door_desc_t;
typedef struct door_cred_t	door_cred_t;
typedef struct ucred_t		ucred_t;
typedef struct door_tcred_t	door_tcred_t;

/* door_info takes a struct door_info*, but door_server_create takes a 
 * door_info_t*.
 */
typedef struct door_info {
	pid_t	 	di_target;
	door_ptr_t 	di_proc;
	door_ptr_t	di_data;
	door_attr_t	di_attributes;
	door_id_t	di_uniquifier;
} door_info_t;

/* For source-compatibility, the typedef is necessary.  I introduce a
 * slight change to the API that shouldn't break anything, in that the
 * data_ptr and rbuf members are pointers to void here.
 */
typedef struct door_arg_t {
	const void*	data_ptr; /* Points to data */
	door_desc_t*	desc_ptr; /* Currently unsupported.  Use NULL. */
	size_t		data_size; /* Size of data. */
	uint_t		desc_num; /* Currently unsupported.  Use 0. */
	void*		rbuf; /* Results buffer. */
	size_t		rsize; /* Size of the results buffer. */
} door_arg_t;

/* Status flags: */
#define DOOR_UNREF		0x001U
#define DOOR_UNREF_MULTI	0x002U
#define DOOR_PRIVATE		0x004U
#define DOOR_REFUSE_DESC	0x008U
#define DOOR_NO_CANCEL		0x010U
#define DOOR_LOCAL		0x020U
#define DOOR_REVOKED		0x040U
#define DOOR_IS_UNREF		0x080U

/* Parameters for door_setparam() and door_getparam(): */
/* 0 is the code for door_info in a request. */
#define DOOR_PARAM_DATA_MAX	1
#define DOOR_PARAM_DATA_MIN	2
#define DOOR_PARAM_DESC_MAX	3

/* This argument to a door server indicates that it's been unreferenced. */
extern char* const restrict	DOOR_UNREF_DATA;

/* Currently unimplemented. */
extern int door_bind(int did);

/* Partially implemented. */
extern int door_call( int d, door_arg_t* params );

/* This type is subtly different from the original implementation: the const
 * and restrict qualifiers are new, and the argument buffer is now a void*
 * rather than char*.  Legacy code should still run, but if you want to
 * suppress compiler warnings, just cast to (door_server_proc_t).
 */
typedef void (*door_server_proc_t)( void* restrict		cookie,
                                    void* restrict		argp,
                                    size_t			arg_size,
                                    const door_desc_t* restrict	dp,
                                    uint_t			n_desc
                                  );
/* For consistency with _door_thread_proc: */
typedef door_server_proc_t	_door_server_proc;

/* Changed the declaration of the attributes parameter to match that of the
 * door_info structure; also left it as an unsigned int for backwards-
 * compatibility.
 */
extern int door_create( door_server_proc_t server_procedure,
                        void* cookie,
                        door_attr_t attributes
                      );

/* Currently unimplemented. */
extern int door_cred( door_cred_t* info );

extern int door_getparam( int d, int param, size_t* out );

extern int door_info( int d, struct door_info* info );

/* Probably never will be implemented.  Trusted Solaris only. */
extern int door_tcred( door_tcred_t* info );

/* Currently unimplemented. */
extern int door_return( void* restrict data_ptr,
                        size_t data_size,
                        door_desc_t* restrict desc_ptr,
                        uint_t num_desc
                      );

extern int door_revoke( int d );

/* Currently unimplemented. */
typedef void (* door_thread_proc_t)( door_info_t* );
/* For backward-compatibility: */
typedef door_thread_proc_t	_door_thread_proc;

extern door_thread_proc_t
door_server_create( door_thread_proc_t create_proc );

extern int door_setparam( int d, int param, size_t val );

/* Currently unimplemented. */
extern int door_ucred( ucred_t **info );

/* Currently unimplemented. */
extern int door_unbind(void);

/* Currently, I use door_attach() and door_detach() in place of
 * fattach() and fdetach().  The reason for this departure from the
 * existing API is substantially different semantics.  Since programs
 * calling fattach() and fdetach() need updating anyway, changing the
 * interface guarantees that it will happen.  If you created an empty
 * temporary file as the argument to fattach(), you should remove or
 * comment out the code creating it and checking that nothing else is
 * attached to it.  If you were masking an existing file, and counting
 * on fdetach() to restore it intact, this will no longer work with
 * door_detach().  Even more importantly: you must not call
 * door_detach() on a mount point as a precautionary measure, the way
 * you could with fdetach().  Doors do not have mount points in this
 * implementation (although it makes its best effort to check for this
 * error and not destroy data).
 *
 * Therefore, door_attach() has identical syntax to fattach(), but
 * requires that no file exist at that path.  The door_detach() function
 * has identical syntax to fdetach(), but removes the attached door.
 * Calling either on anything but a door causes undefined behavior,
 * although the implementation makes its best effort to detect this and
 * fail gracefully.
 *
 * The door_attach() function creates a door that no one can use; follow
 * it with a call such as chmod().  It temporarily modifies the umask,
 * and so is not thread-safe in that regard.
 */
extern int door_attach( int d, const char* path );

extern int door_detach( const char* path );

/* In this implementation, the open() system call doesn't do the right
 * thing with doors (which are sockets).  I therefore provide
 * door_open() as a replacement for open.  It does not have any flags 
 * at present, because it makes no sense for a door to be open for 
 * reading but not writing, or vice versa.  (It might conceivably make
 * logical sense to have a door that does asynchronous IPC, but setting
 * O_NONBLOCK on the door descriptor would not have the desired effect.)
 */
extern int door_open( const char* path );

/* This wrapper for close() exists to give the implementation
 * flexibility to allocate data structures for each open connection,
 * without leaking memory.
 */
extern int door_close( int d );


#ifdef __cplusplus
} /* extern "C" */
#endif

#undef restrict

#endif /* defined(_DOOR_H) */
