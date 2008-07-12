#include "standards.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "door.h"
#include "messages.h"

void dummy_server_proc( void* cookie,
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

	printf( "ID:\t\t%llu\n", (unsigned long long)info->di_uniquifier
	      );

	return;
}

int main(void)
{
	int sockets[2];	/* Used by socketpair() */
	int status;

	struct door_info info;

	if ( 0 != socketpair( AF_UNIX, SOCK_SEQPACKET, 0, sockets ) ) {
		perror("socketpair");
		return EXIT_FAILURE;
	}

	status = fork();

	if ( 0 > status ) {
		perror("fork");
		return EXIT_FAILURE;
	}
	else if ( 0 == status ) {
/* We are the child process. */

		struct msg_request outgoing;
		struct msg_door_info incoming;

		close(sockets[0]);

		msg_request_init( &outgoing, REQ_DOOR_INFO );
		if ( 0 >send( sockets[1],
		              &outgoing,
		              sizeof(outgoing), 
		              MSG_EOR
		            )
		   ) {
			perror("send (msg_request)");
			return EXIT_FAILURE;
		}

		if ( 0 > recv( sockets[1],
		               &incoming,
		               sizeof(incoming), 
		               0
		             )
		   ) {
			perror("recv (msg_door_info)");
			return EXIT_FAILURE;
		}

		msg_door_info_decode( &incoming, &info );

		printf("Client\n");
		print_door_info(&info);
		return EXIT_SUCCESS;
	}
	else {
/* We are the parent process. */
		int door;
		struct msg_request incoming;
		struct msg_door_info outgoing;
		int stat_val;		/* Used by waitpid() */

		close(sockets[1]);

		door = door_create( dummy_server_proc, 
                                    (void*)&status, 
                                     DOOR_REFUSE_DESC
		                  );

		if ( 0 > door ) {
			perror("door_create");
			return EXIT_FAILURE;
		}

		if ( 0 != door_info( door, &info ) ) {
			perror("door_info (server)");
			return EXIT_FAILURE;
		}

		printf("Server\n");
		print_door_info(&info);

		if ( 0 > recv( sockets[0],
		               &incoming,
		               sizeof(incoming),
		               0
		             )
		   ) {
			perror("recv (msg_request)");
			return EXIT_FAILURE;
		}

		msg_door_info_init( &outgoing,
                                    getpid(),
		                    dummy_server_proc,
		                    (void*)&status,
		                    DOOR_REFUSE_DESC,
		                    info.di_uniquifier
		                  );

		if ( 0 > send( sockets[0],
		               &outgoing,
		               sizeof(outgoing),
		               MSG_EOR
		             )
		   ) {
			perror("send (msg_door_info)");
			return EXIT_FAILURE;
		}

		wait(&stat_val);
		return EXIT_SUCCESS;
	}
}
