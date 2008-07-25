/***************************************************************************
 * Portland Doors                                                          *
 * error1.c: Test driver for the fatal_system_error() utility function.    *
 *                                                                         *
 *           Correct output is as follows:                                 *
 *                                                                         *
 *           "(test/error1.c, line 24) Just Checking: Success" or some-    *
 *           thing very similar, depending on your system.                 *
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/


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
