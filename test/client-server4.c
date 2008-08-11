/***************************************************************************
 * Portland Doors                                                          *
 * client-server4.c:  Test driver for multi-threaded clients.              *
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
	static const unsigned int one = 1;
	int door1 = -1;

	door1 = door_create( (door_server_proc_t)delay_proc, (void*)&one, 0 );

	if ( 0 > door1 )
		perror("door_create door1");

	door_detach("/tmp/door1");

	if ( 0 != door_attach( door1, "/tmp/door1" ) )
		perror("door_attach door1");
	else
		chmod( "/tmp/door1", S_IRWXU );

	return;
}

static void* spawn_thread( void* x )
{
	struct door_arg_t args;
	unsigned retval;
	int door = *(int*)x;

	bzero( &args, sizeof(args) );
	args.rbuf = &retval;
	args.rsize = sizeof(retval);

	printf("Called a door.\n");

	if ( 0 != door_call( door, &args ) )
		perror("door_call");

	assert( NULL != args.rbuf && args.rsize >= sizeof(unsigned) );

	printf( "Returned %u in %llus.\n",
	        *(unsigned int*)args.rbuf,
	        (unsigned long long)( time(NULL) - start_time )
	      );

	return NULL;
}

static void client_proc(void)
{
	pthread_t thread1 = 0, thread2 = 0, thread3 = 0;
	int door;

	door = door_open("/tmp/door1");
	if ( 0 > door )
		perror("door_open");

	start_time = time(NULL);

	if ( 0 !=
pthread_create( &thread3, NULL, spawn_thread, &door )
	   )
		perror("pthread_create thread3");

	if ( 0 !=
pthread_create( &thread2, NULL, spawn_thread, &door )
	   )
		perror("pthread_create thread2");

	if ( 0 !=
pthread_create( &thread1, NULL, spawn_thread, &door )
	   )
		perror("pthread_create thread1");

	if ( 0 != pthread_join( thread1, NULL ) )
		perror("pthread_join thread1");

	if ( 0 != pthread_join( thread2, NULL ) )
		perror("pthread_join thread2");

	if ( 0 != pthread_join( thread3, NULL ) )
		perror("pthread_join thread3");

	if ( 0 != door_close(door) )
		perror("door_close");

	door_detach("/tmp/door1");

	return;
}

int main(void)
{
	errno = 0;

	server_proc();
	client_proc();

	return EXIT_SUCCESS;
}
