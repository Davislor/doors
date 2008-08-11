/***************************************************************************
 * Portland Doors                                                          *
 * client-server3.c:  Test driver for multi-threaded clients.              *
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

#include "standards.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "door.h"

time_t start_time = 0;

static void delay_proc( const unsigned int* restrict delay,
                        const void* restrict argp,
                        size_t arg_size,
                        const door_desc_t* restrict dp,
                        uint_t n_desc
                      )
{
	sleep(*delay);

	printf( "Slept %u seconds.\n", *delay );

	door_return( delay, sizeof(unsigned int), NULL, 0 );
}

static void server_proc(void)
{
	static const unsigned int one = 1, two = 2, three = 3;
	int door1 = -1, door2 = -1, door3 = -1;

	door1 = door_create( (door_server_proc_t)delay_proc, (void*)&one, 0 );
	door2 = door_create( (door_server_proc_t)delay_proc, (void*)&two, 0 );
	door3 = door_create( (door_server_proc_t)delay_proc, (void*)&three, 0 );

	if ( 0 > door1 )
		perror("door_create door1");

	if ( 0 > door2 )
		perror("door_create door2");

	if ( 0 > door3 )
		perror("door_create door3");

	door_detach("/tmp/door1");
	door_detach("/tmp/door2");
	door_detach("/tmp/door3");

	if ( 0 != door_attach( door1, "/tmp/door1" ) )
		perror("door_attach door1");
	else
		chmod( "/tmp/door1", S_IRWXU );

	if ( 0 != door_attach( door2, "/tmp/door2" ) )
		perror("door_attach door2");
	else
		chmod( "/tmp/door2", S_IRWXU );

	if ( 0 != door_attach( door3, "/tmp/door3" ) )
		perror("door_attach door3");
	else
		chmod( "/tmp/door3", S_IRWXU );

	return;
}

static void* spawn_thread( void* x )
{
	const char * const path = x;
	struct door_arg_t args;
	unsigned retval;
	int door;

	bzero( &args, sizeof(args) );
	args.rbuf = &retval;
	args.rsize = sizeof(retval);

	door = door_open(path);
	if ( 0 > door )
		perror("door_open");

	printf( "Called %s.\n", path );

	if ( 0 != door_call( door, &args ) )
		perror("door_call");

	assert( NULL != args.rbuf && args.rsize >= sizeof(unsigned) );

	printf( "Returned %u in %llus.\n",
	        *(unsigned int*)args.rbuf,
	        (unsigned long long)( time(NULL) - start_time )
	      );

	if ( 0 != door_close(door) )
		perror("door_close");

	return NULL;
}

static void client_proc(void)
{
	pthread_t thread1 = 0, thread2 = 0, thread3 = 0;

	start_time = time(NULL);

	if ( 0 !=
pthread_create( &thread3, NULL, spawn_thread, "/tmp/door3" )
	   )
		perror("pthread_create thread3");

	if ( 0 !=
pthread_create( &thread2, NULL, spawn_thread, "/tmp/door2" )
	   )
		perror("pthread_create thread2");

	if ( 0 !=
pthread_create( &thread1, NULL, spawn_thread, "/tmp/door1" )
	   )
		perror("pthread_create thread1");

	if ( 0 != pthread_join( thread1, NULL ) )
		perror("pthread_join thread1");

	if ( 0 != pthread_join( thread2, NULL ) )
		perror("pthread_join thread2");

	if ( 0 != pthread_join( thread3, NULL ) )
		perror("pthread_join thread3");

	door_detach("/tmp/door1");
	door_detach("/tmp/door2");
	door_detach("/tmp/door3");

	return;
}

int main(void)
{
	errno = 0;

	server_proc();
	client_proc();

	return EXIT_SUCCESS;
}
