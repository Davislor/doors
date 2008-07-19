#include "standards.h"
#include "door.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void proc1 (void* cookie, char* argp, size_t arg_size, door_desc_t* dp, 
uint_t n_desc )
{

}

void proc2 (void* cookie, char* argp, size_t arg_size, door_desc_t* dp,
uint_t n_desc )
{

}

void proc3 (void* cookie, char* argp, size_t arg_size, door_desc_t* dp,
uint_t n_desc )
{

}

int main(void)
{
	int doors[3];
	struct door_info info;
	pid_t self;

	errno = 0;
	self = getpid();

	doors[0] = door_create( proc1, (void*)proc1, DOOR_REFUSE_DESC );
	if ( 0 > doors[0] )
		perror("door_create (proc1)");

	doors[1] = door_create( proc2, (void*)proc2, 0 );
	if ( 0 > doors[1] )
		perror("door_create (proc1)");

	doors[2] = door_create( proc3, (void*)proc3, 0 );
	if ( 0 > doors[2] )
		perror("door_create (proc1)");

	if ( 0 != door_info( doors[0], &info ) )
		perror("door_info (proc1)");

	if ( (self == info.di_target) &&
	     ((door_ptr_t)proc1 == info.di_proc) &&
	     ((door_ptr_t)proc1 == info.di_data) &&
	     ( (DOOR_LOCAL | DOOR_REFUSE_DESC) == info.di_attributes )
	   )
		printf("Check 1 passed.\n");
	else
		perror("Bad info for proc1.");

	if ( 0 != door_revoke(doors[0]) )
		perror("door_revoke (proc1)");

	if ( 0 != door_info( doors[1], &info ) )
		perror("door_info (proc2)");

	if ( (self == info.di_target) &&
	     ((door_ptr_t)proc2 == info.di_proc) &&
	     ((door_ptr_t)proc2 == info.di_data) &&
	     ( DOOR_LOCAL == info.di_attributes )
	   )
		printf("Check 2 passed.\n");
	else
		perror("Bad info for proc2.");

	if ( 0 != door_revoke(doors[1]) )
		perror("door_revoke (proc2)");

	if ( 0 != door_info( doors[2], &info ) )
		perror("door_info (proc3)");

	if ( (self == info.di_target) &&
	     ((door_ptr_t)proc3 == info.di_proc) &&
	     ((door_ptr_t)proc3 == info.di_data) &&
	     ( DOOR_LOCAL == info.di_attributes )
	   )
		printf("Check 3 passed.\n");
	else
		perror("Bad info for proc3.");

	if ( 0 != door_revoke(doors[2]) )
		perror("door_revoke (proc3)");

	return EXIT_SUCCESS;
}
