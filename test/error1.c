#include "standards.h"
#include "error.h"

#include <errno.h>

int main(void)
{
  errno = 0;

  fatal_system_error( __FILE__, __LINE__, "Just Checking" );

/* NOTREACHED */
  return 0;
}
