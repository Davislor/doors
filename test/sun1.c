/***************************************************************************
 * Portland Doors                                                          *
 * sun1.c:  Example code from the Solaris 10 manpages.                     *
 *                                                                         *
 *          This program exposes several differences between the portable  *
 *          version of the API and the original version.  It will run      *
 *          without an error on Solaris, but fail on Linux.  The equiva-   *
 *          lent program for the new API is test/sun2.                     *
 ***************************************************************************/

#include "standards.h"
#include <door.h>

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <stropts.h>
#include <sys/stat.h>
#include <unistd.h>

static void
server(void *cookie, char *argp, size_t arg_size, door_desc_t *dp,
    uint_t n_desc)
{
    door_return(NULL, 0, NULL, 0);
    /* NOTREACHED */
}

int
main(int argc, char *argv[])
{
    int did;
    struct stat buf;

    if ((did = door_create(server, 0, 0)) < 0) {
        perror("door_create");
        exit(1);
    }

    /* make sure file system location exists */
    if (stat("/tmp/door", &buf) < 0) {
        int newfd;
        if ((newfd = creat("/tmp/door", 0444)) < 0) {
            perror("creat");
            exit(1);
        }
        (void) close(newfd);
    }

    /* make sure nothing else is attached */
    (void) fdetach("/tmp/door");

    /* attach to file system */
    if (fattach(did, "/tmp/door") < 0) {
        perror("fattach");
        exit(2);
    }

    return EXIT_SUCCESS;
}
