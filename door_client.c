/***************************************************************************
 * Portland Doors                                                          *
 * door_client.c: Implements parts of the library that do not depend on    *
 *                details of the internal implementation.                  * 
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

#include "standards.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "door.h"
#include "error.h"
#include "messages.h"

int door_open( const char* path )
/* Drop-in replacement for open().  Currently, this opens a door 
 * descriptor, which is a socket connected to the UNIX domain socket at 
 * the requested pathname.
 */
{
	static const int ERROR = -1;

	struct sockaddr_un address;	/* Address to connect to */
/* File descriptor of the new door: */
	int d;
	size_t path_len;		/* Length of path. */

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

	return d;
}
