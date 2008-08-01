/***************************************************************************
 * Portland Doors                                                          *
 * door_client.c: Implements parts of the library that do not depend on    *
 *                details of the internal implementation.                  * 
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

#include "standards.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#include "door.h"
#include "error.h"
#include "messages.h"

static size_t page_size = 0;

/* Internal functions: */
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

/* Exported functions: */
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
