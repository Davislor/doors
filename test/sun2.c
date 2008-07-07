/***************************************************************************
 * Doors/Linux                                                             *
 * Simple test application based on demonstration code from the Solaris 10 *
 * door_create manpage.                                                    *
 ***************************************************************************/

#include "standards.h"
#include "door.h"

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <stropts.h>
#include <sys/stat.h>
#include <unistd.h>

void
server(void *cookie,
       char *argp,
       size_t arg_size,
       door_desc_t *dp,
       uint_t n_desc
      )
{
    door_return(NULL, 0, NULL, 0);
    /* NOTREACHED */
}

int
main(int argc, char *argv[])
{
	int did;

	if ((did = door_create(server, 0, 0)) < 0) {
		perror("door_create");
		exit(1);
	}

/* attach to file system */
	if (door_attach(did, "/tmp/door") < 0) {
		perror("door_attach");
		exit(2);
	}

	if (door_detach("/tmp/door") < 0) {
		perror("door_detach");
		exit(3);
	}

	return EXIT_SUCCESS;
}
