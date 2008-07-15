#include "standards.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "door.h"
#include "messages.h"

void dummy_server( void* cookie,
                   char* restrict argp,
                   size_t arg_size,
                   door_desc_t* restrict dp,
                   uint_t n_desc
                 )
{
	return;
}

void print_door_info( const struct door_info* info )
{
	fflush(stdout);

	printf( "PID:\t\t%llu\n",
	        (unsigned long long int)info->di_target
	      );

	printf( "Procedure:\t%llX\n",
	        (unsigned long long int)info->di_proc
	      );

	printf( "Cookie:\t\t%llX\n",
	        (unsigned long long int)info->di_data
	      );

	printf( "Attributes:\t%X\n", (unsigned int)info->di_attributes );

	printf( "ID:\t\t%llu\n", (unsigned long long)info->di_uniquifier );

	return;
}

int main(void)
{
	int status;	/* Return value of fork() */	
	static const char* door_path = "/tmp/door";

	status = fork();

	if ( 0 > status ) {
		perror("fork");
		return EXIT_FAILURE;
	}
	else if ( 0 != status ) {
		struct door_info info;
		int d;

		sleep(1);
		d = door_open(door_path);
		if ( 0 > d ) {
			perror("door_open");
			return EXIT_FAILURE;
		}

		if ( 0 != door_info( d, &info ) ) {
			perror("door_info (client)");
			return EXIT_FAILURE;
		}

		printf("Client:\n");
		print_door_info(&info);

		if ( 0 != door_close(d) ) {
			perror("door_close");
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}
	else {
		int d;		/* Descriptor from door_create() */
		int e;		/* Descriptor from accept() */
		struct sockaddr_un address;
		socklen_t addr_len = sizeof(struct sockaddr_un);
		struct door_info info;		/* Used by door_info() */
		struct msg_request incoming;
		struct msg_door_info outgoing;
		door_attr_t attr;

		door_detach(door_path);

		d = door_create( dummy_server, (void*)door_path, 0 );

		if ( 0 > d ) {
			perror("door_create");
			return EXIT_FAILURE;
		}

		if ( 0 != door_attach( d, door_path ) ) {
			perror("door_attach");
			return EXIT_FAILURE;
		}

		if ( 0 != chmod( door_path, S_IRUSR | S_IWUSR ) ) {
			perror("chmod");
			return EXIT_FAILURE;
		}

		if ( 0 != door_info( d, &info ) ) {
			perror("door_info (server)");
			return EXIT_FAILURE;
		}

		printf("Server:\n");
		print_door_info(&info);

		e = accept( d, (struct sockaddr*)&address, &addr_len );
		if ( 0 > e ) {
			perror("accept");
			return EXIT_FAILURE;
		}

		if ( 0 > recv( e, &incoming, sizeof(incoming), 0 ) ) {
			perror("recv");
			return EXIT_FAILURE;
		}

		attr = info.di_attributes & ~(door_attr_t)DOOR_LOCAL;

		msg_door_info_init( &outgoing,
		                    getpid(),
		                    dummy_server,
		                    (void*)door_path,
		                    attr,
		                    info.di_uniquifier
		                  );

		if ( sizeof(outgoing) != send( e,
		                               &outgoing, 
		                               sizeof(outgoing),
		                               MSG_EOR
		                             )
		   ) {
			perror("send");
			return EXIT_FAILURE;
		}

		if ( 0 != door_detach(door_path) ) {
			perror("door_detach");
			return EXIT_FAILURE;
		}

		return EXIT_FAILURE;
	}
}
