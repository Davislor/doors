
#include "standards.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "door.h"

const char* const door_path = "/tmp/door";
static const int nclients = 4;

void dummy_server( void* restrict cookie,
                   char* restrict argp,
                   size_t arg_size,
                   door_desc_t* restrict dp,
                   uint_t ndesc
                 )
{
	return;
}

int server_process(void)
{
	int door;
	struct door_info info;
	size_t data_min, data_max, desc_max;

	door_detach(door_path);

	door = door_create( dummy_server, (void*)&door_path, DOOR_REFUSE_DESC );

	if ( 0 > door ) {
		perror("door_create");
		return EXIT_FAILURE;
	}

	if ( 0 != door_attach( door, door_path ) ) {
		perror("door_attach");
		return EXIT_FAILURE;
	}

	if ( 0 != chmod( door_path, S_IRWXU ) ) {
		perror("chmod");
		return EXIT_FAILURE;
	}

	if ( 0 != door_info( door, &info ) )
		perror("door_info (server)");

	if ( 0 != door_getparam( door, DOOR_PARAM_DATA_MIN, &data_min ) )
		perror("door_getparam (server)");

	if ( 0 != door_getparam( door, DOOR_PARAM_DATA_MAX, &data_max ) )
		perror("door_getparam (server)");

	if ( 0 != door_getparam( door, DOOR_PARAM_DESC_MAX, &desc_max ) )
		perror("door_getparam (server)");

	printf("Server:\nPID:\t\t%ld\nProcedure:\t%llX\nCookie:\t\t%llx\n"
	       "Attributes:\t%X\nID:\t\t%llX\nMin Data:\t%zu\n"
	       "Max Data:\t%zu\nMax Descs:\t%zu\n",
	       (long)info.di_target,
	       (unsigned long long)info.di_proc,
               (unsigned long long)info.di_data,
               (unsigned int)info.di_attributes,
	       (unsigned long long)info.di_uniquifier,
	       data_min,
	       data_max,
	       desc_max
	      );
	fflush(stdout);

	return EXIT_SUCCESS;
}

int client_process(void)
{
	int door;
	struct door_info info;
	size_t data_min, data_max, desc_max;

	door = door_open(door_path);
	if ( 0 > door )
		perror("door_open");

	if ( 0 != door_info( door, &info ) )
		perror("door_info");

	if ( 0 != door_getparam( door, DOOR_PARAM_DATA_MIN, &data_min ) )
		perror("door_getparam");

	if ( 0 != door_getparam( door, DOOR_PARAM_DATA_MAX, &data_max ) )
		perror("door_getparam");

	if ( 0 != door_getparam( door, DOOR_PARAM_DESC_MAX, &desc_max ) )
		perror("door_getparam");

	printf("Client %ld:\nPID:\t\t%ld\nProcedure:\t%llX\nCookie:\t\t%llx\n"
	       "Attributes:\t%X\nID:\t\t%llX\nMin Data:\t%zu\n"
	       "Max Data:\t%zu\nMax Descs:\t%zu\n",
	       (long)getpid(),
	       (long)info.di_target,
	       (unsigned long long)info.di_proc,
               (unsigned long long)info.di_data,
               (unsigned int)info.di_attributes,
	       (unsigned long long)info.di_uniquifier,
	       data_min,
	       data_max,
	       desc_max
	      );
	fflush(stdout);

	return EXIT_SUCCESS;
}

int main(void)
{
	int i, retval;

	errno = 0;

/* Spawn one server process. */
	retval = server_process();

	for ( i = 0; i < nclients; ++i ) {
		int child_pid;
		child_pid = fork();

		if ( 0 > child_pid )
			perror("fork");
		else if ( 0 == child_pid )
			return client_process();
	}

/* We spawn four children. */
	for ( i = 0; i < nclients; ++i ) {
		int status;

		wait(&status);
	}

	door_detach(door_path);

	return retval;
}
