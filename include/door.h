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
 * replaces fdetach() and removes the door from the filesystem.  All
 * doors servers will probably require slight modification, but doors
 * clients should not.
 *
 * This header file reserves all identifiers beginning with door_, 
 * DOOR_, _door_, and _DOOR_ for future use.
 */

#ifndef _DOOR_H
#define _DOOR_H

#undef	RESTRICT

#if (__STDC_VERSION >= 199901L)
#define RESTRICT restrict
#else
#define RESTRICT
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

typedef struct door_arg_t	door_arg_t;

typedef void (* _door_thread_proc) ();

/* Status flags: */
#define DOOR_UNREF		0x001
#define DOOR_UNREF_MULTI	0x002
#define DOOR_PRIVATE		0x004
#define DOOR_REFUSE_DESC	0x008
#define DOOR_NO_CANCEL		0x010
#define DOOR_LOCAL		0x020
#define DOOR_REVOKED		0x040

/* Parameters for door_setparam() and door_getparam(): */
/* 0 is the code for door_info in a request. */
#define DOOR_PARAM_DATA_MAX	1
#define DOOR_PARAM_DATA_MIN	2
#define DOOR_PARAM_DESC_MAX	3

/* Currently unimplemented. */
extern int door_bind(int did);

/* Currently unimplemented. */
extern int door_call(int d, door_arg_t* params);

extern int door_create( void (* server_procedure)
(void* cookie, char* argp, size_t arg_size, door_desc_t* dp, uint_t n_desc),
                        void* cookie,
                        uint_t attributes
                      );

/* Currently unimplemented. */
extern int door_cred (door_cred_t* info);

extern int door_getparam(int d, int param, size_t* out);

extern int door_info(int d, struct door_info* info);

/* Probably never will be implemented.  Trusted Solaris only. */
extern int door_tcred( door_tcred_t* info );

/* Currently unimplemented. */
extern int door_return( RESTRICT char* data_ptr,
                        size_t data_size,
                        RESTRICT door_desc_t* desc_ptr,
                        uint_t num_desc
                      );

extern int door_revoke(int d);

/* Currently unimplemented. */
extern _door_thread_proc
door_server_create( _door_thread_proc create_proc );

extern int door_setparam(int d, int param, size_t val);

/* Currently unimplemented. */
extern int door_ucred(ucred_t **info);

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

#ifdef __cplusplus
} /* extern "C" */
#endif

#undef RESTRICT

#endif /* defined(_DOOR_H) */
