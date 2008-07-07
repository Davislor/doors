#include "standards.h"

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <stropts.h>
#include <unistd.h>

int main(void)
{
  static const char* const filename = "/tmp/stream";
  int fd[2];

  if ( 0 > pipe(fd) )
  {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  if ( 0 > creat( filename, S_IRUSR | S_IWUSR ) )
  {
    perror("creat");
    exit(EXIT_FAILURE);
  }

  if ( 0 > fattach( fd[0], filename ) )
  {
    perror("fattach");
    exit(EXIT_FAILURE);
  }

  if ( 0 > fdetach(filename) )
  {
    perror("fdetach");
    exit(EXIT_FAILURE);
  }

  if ( 0 > unlink(filename) )
  {
    perror("unlink");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
