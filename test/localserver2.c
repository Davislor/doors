/***************************************************************************
 * Portland Doors                                                          *
 * localserver2.c:  Test driver for door_setparam() and door_getparam() on *
 *                  local doors.                                           *
 *                                                                         *
 *                  This program creates a door, sets its data_min param-  *
 *                  eter to 1,024, sets its data_max parameter to 4,096,   *
 *                  and then reads these parameters and desc_max back.     *
 *                  The last must be 0, because the door has the flag      *
 *                  DOOR_REFUSE_DESC set.                                  *
 *                                                                         *
 *                  Correct output:                                        *
 *                                                                         *
 *                  The first line should say that the door accepts bet-   *
 *                  ween 0 and the default number of bytes, which will     *
 *                  vary from system to system.                            *
 *                                                                         *
 *                  The second line should say that the door accepts bet-  *
 *                  ween 1,024 and 4,096 bytes, exactly.                   *
 *                                                                         *
 *                  The third line should give the size of the internal    *
 *                  buffer which accepts this data.  This size must be at  *
 *                  least enough to hold a message containing the maximum  *
 *                  amount of data; otherwise, an assertion will fail.     *
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

#include "standards.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "door.h"
#include "messages.h"

static void nop_server( void* cookie,
                 char* argp,
                 size_t arg_size,
                 door_desc_t* dp,
                 uint_t n_desc
               )
{
	return;
}

int main(void)
{
	int d;
	size_t limit;
	int error;
	int scratch;
	socklen_t int_len = sizeof(int);

	static const unsigned int NEW_MIN = 1024U;
	static const unsigned int NEW_MAX = 4096U;

	d = door_create( (door_server_proc_t)nop_server,
	                 NULL,
	                 DOOR_REFUSE_DESC
	               );

	assert( 0 <= d );

	error = door_getparam( d, DOOR_PARAM_DATA_MIN, &limit );
	assert ( 0 == error );
	assert( 0 == limit );

	error = door_getparam( d, DOOR_PARAM_DATA_MAX, &limit );
	printf( "Accepts %u-%u bytes.\n", 0, (unsigned int)limit );

	error = door_setparam( d, DOOR_PARAM_DATA_MAX, NEW_MAX );
	assert( 0 == error );

	error = door_getparam( d, DOOR_PARAM_DATA_MAX, &limit );
	assert( 0 == error );
	assert( NEW_MAX == limit );

	error = door_setparam( d, DOOR_PARAM_DATA_MIN, NEW_MIN );
	assert( 0 == error );

	error = door_getparam( d, DOOR_PARAM_DATA_MIN, &limit );
	assert( NEW_MIN == limit );

	printf( "Accepts %u-%u bytes.\n", NEW_MIN, NEW_MAX );

	error = getsockopt(d, SOL_SOCKET, SO_RCVBUF, &scratch, &int_len);
	assert( 0 == error );
	assert( (int)(NEW_MAX + DOOR_CALL_RESERVED) <= scratch );

	printf( "Internal buffer: %d bytes.\n", scratch );

	return EXIT_SUCCESS;
}
