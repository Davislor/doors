#include "standards.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "door.h"
#include "error.h"

void echo( void* restrict,
           char* restrict,
           size_t,
           door_desc_t* restrict,
           uint_t
         );

void client_proc(void);
void server_proc(void);

static const char* const door_path = "/tmp/door";

void echo( void* restrict stream,
           char* restrict string,
           size_t len,
           door_desc_t* restrict unused1,
           uint_t unused2
         )
{
	size_t i;

	for ( i = 0; i < len; ++i )
		fputc( string[i], (FILE*)stream );

	fputc( '\n', (FILE*)stream );
	fflush((FILE*)stream);

	if ( 0 != door_return( (char*)&len, sizeof(len), NULL, 0 ) )
		fatal_system_error(__FILE__,__LINE__,"door_return");

	fprintf( stderr,
	         "Error: door_return returned \"successfully\"!"
	       );
	exit(EXIT_FAILURE);
}

void server_proc(void)
{
	int door;

	door_detach(door_path);

	door = door_create( echo, (void*)stdout, DOOR_REFUSE_DESC );

	if ( 0 > door )
		fatal_system_error( __FILE__, __LINE__, "door_create" );

	if ( 0 != door_attach( door, door_path ) )
		fatal_system_error( __FILE__, __LINE__, "door_attach" );

	if ( 0 != chmod( door_path, S_IRWXU ) )
		fatal_system_error( __FILE__, __LINE__, "chmod" );

	return;
}

void client_proc(void)
{
	int door;
	struct door_arg_t params;
	size_t printed = 0;

	memset( &params, 0, sizeof(params) );

	params.desc_ptr = NULL;
	params.desc_num = 0;
	params.rbuf = &printed;
	params.rsize = sizeof(printed);

	door = door_open(door_path);
	if ( 0 > door )
		fatal_system_error( __FILE__, __LINE__, "door_open" );

	params.data_ptr = "Hello, world!";
	params.data_size = sizeof("Hello, world!");
	if ( 0 != door_call( door, &params ) )
		fatal_system_error( __FILE__, __LINE__, "door_call" );

	assert( sizeof("Hello, world!") == *(const size_t*)params.data_ptr );

	if ( params.rbuf == &printed )
		assert( sizeof("Hello, world!") == printed );

	if ( 0 != door_close(door) )
		fatal_system_error( __FILE__, __LINE__, "door_close" );

	return;
}

int main(void)
{
	errno = 0;

	server_proc();
	client_proc();

	return EXIT_SUCCESS;
}
