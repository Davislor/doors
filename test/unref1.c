
#include "standards.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "door.h"
#include "error.h"

static const char* const door_path = "/tmp/door";
static const char* const unref_msg = "Unreferenced invocation received.\n";

static void unref_server( const char* restrict to_print,
                          const void* restrict unref_data,
                          size_t unused1,
                          const door_desc_t* restrict unused2,
                          uint_t unused3
                        )
{
	assert( DOOR_UNREF_DATA == unref_data );
	assert( 0 == unused1 );
	assert( NULL == unused2 );
	assert( 0 == unused3 );

	printf(to_print);
	fflush(stdout);

	return;
}

static void server_proc(void)
{
	int door;	/* A door descriptor. */

	door = door_create( (door_server_proc_t)unref_server,
	                    (void*)unref_msg,
	                    DOOR_REFUSE_DESC | DOOR_UNREF_MULTI
	                  );

	if ( 0 > door )
		fatal_system_error( __FILE__, __LINE__, "door_create" );

	if ( 0 != door_setparam( door, DOOR_PARAM_DATA_MAX, 0 ) )
		fatal_system_error( __FILE__, __LINE__, "door_setparam" );

	door_detach(door_path);

	if ( 0 != door_attach( door, door_path ) )
		fatal_system_error( __FILE__, __LINE__, "door_attach" );

	if ( 0 != chmod( door_path, S_IRWXU ) )
		fatal_system_error( __FILE__, __LINE__, "chmod" );
}

static void client_proc(void)
{
	int door1, door2, door3;	/* Door descriptors. */

	door1 = door_open(door_path);
	if ( 0 > door1 )
		fatal_system_error( __FILE__, __LINE__, "door_open" );

	printf("There should be exactly two 'Unreferenced invocation "
	       "received' messages, both following this line.\n"
	      );
	fflush(stdout);

	if ( 0 != door_close(door1) )
		fatal_system_error( __FILE__, __LINE__, "door_close" );

	door1 = door_open(door_path);
	if ( 0 > door1 )
		fatal_system_error( __FILE__, __LINE__, "door_open" );

	door2 = door_open(door_path);
	if ( 0 > door2 )
		fatal_system_error( __FILE__, __LINE__, "door_open" );

	door3 = door_open(door_path);
	if ( 0 > door3 )
		fatal_system_error( __FILE__, __LINE__, "door_open" );

	if ( 0 != door_close(door3) )
		fatal_system_error( __FILE__, __LINE__, "door_close" );

	if ( 0 != door_close(door2) )
		fatal_system_error( __FILE__, __LINE__, "door_close" );

	printf("There should be at least one such message following this "
	       "line.\n"
	      );
	fflush(stdout);

	if ( 0 != door_close(door1) )
		fatal_system_error( __FILE__, __LINE__, "door_close" );

	return;
}

int main(void)
{
	server_proc();
	client_proc();

	if ( 0 != door_detach(door_path) )
		fatal_system_error( __FILE__, __LINE__, "door_detach" );

/* Wait a moment, in case the server thread is still working. */
	sleep(1);

	return EXIT_SUCCESS;
}
