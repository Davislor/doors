#include "standards.h"
#include "door.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

void nop_server( void* cookie,
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

	d = door_create( nop_server, NULL, DOOR_REFUSE_DESC );

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

	printf( "Internal buffer: %d bytes.\n", scratch );

	return EXIT_SUCCESS;
}
