#include "standards.h"

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "door.h"

static void dummy_server( void* cookie,
                          char* restrict argp,
                          size_t arg_size,
                          door_desc_t* restrict dp,
                          uint_t n_desc
                        )
{
	return;
}

int main(void)
{
	int status;
	static const char* const door_name = "/tmp/door";
	int door;
	struct door_info info;

	status = fork();

	if ( 0 > status ) {
		perror("fork");
		return EXIT_FAILURE;
	}
	else if ( 0 == status ) {
/* Child process. */
		struct sockaddr_un door_addr;

		sleep(1);

		door = socket( AF_UNIX, SOCK_SEQPACKET, 0 );

		if ( 0 > door ) {
			perror("socket");
			return EXIT_FAILURE;
		}

		door_addr.sun_family = AF_UNIX;
		strcpy( door_addr.sun_path, door_name );

		if ( 0 != connect( door,
		                   (const struct sockaddr*)&door_addr,
offsetof( struct sockaddr_un, sun_path ) + strlen(door_name) 
		                 )
		   ) {
			perror("connect");
			return EXIT_FAILURE;
		}

		if ( 0 != door_info( door, &info ) ) {
			perror("door_info");
			return EXIT_FAILURE;
		}

		printf("Pid:\t\t%llu\n",
		       (unsigned long long)info.di_target
		      );

		printf("Procedure:\t%llX\n",
		       (unsigned long long)info.di_proc
		      );

		printf("Cookie:\t\t%llX\n",
		       (unsigned long long)info.di_data
		      );

		printf("Flags:\t\t%X\n",
		       (unsigned int)info.di_attributes 
		      );

		printf("Door ID:\t%llx\n",
		       (unsigned long long)info.di_uniquifier
		      );

		return EXIT_SUCCESS;
	}
	else {
/* Parent process */
		door =
door_create( dummy_server, (void*)door_name, DOOR_REFUSE_DESC );

		if( 0 > door ) {
			perror("door_create");
			return EXIT_FAILURE;
		}

                if ( 0 != door_info( door, &info ) ) {
                        perror("door_info");
                        return EXIT_FAILURE;
                }

                printf("Pid:\t\t%llu\n",
                       (unsigned long long)info.di_target
                      );

                printf("Procedure:\t%llX\n",
                       (unsigned long long)info.di_proc
                      );

                printf("Cookie:\t\t%llX\n",
                       (unsigned long long)info.di_data
                      );

                printf("Flags:\t\t%X\n",
                       (unsigned int)info.di_attributes
                      );

                printf("Door ID:\t%llx\n",
                       (unsigned long long)info.di_uniquifier
                      );

		door_detach(door_name);

		if ( 0 != door_attach( door, door_name ) ) {
			perror("door_attach");
			return EXIT_FAILURE;
		}

		door_detach(door_name);

		printf("Done!\n");

		return EXIT_SUCCESS;
	}
}
