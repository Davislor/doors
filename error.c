/***************************************************************************
 * Doors/Linux                                                             *
 * error.c: Error-handling functions.                                      *
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

#include "standards.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>

void _syserr( const char* desc )
/* Prints an informative error message and exits. */
{
	fflush(stdout);
	perror(desc);

	exit(EXIT_FAILURE);

	/* NOTREACHED */
}
