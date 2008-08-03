/***************************************************************************
 * Portland Doors                                                          *
 * error.h: Error-handling functions.                                      *
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

#ifndef H_ERROR_INCLUDED
#define H_ERROR_INCLUDED

#include "standards.h"

#include <stdio.h>
#include <stdlib.h>

#define _str(x) #x

static void _syserr( const char* desc )
/* Prints an informative error message and exits. */
{
	fflush(stdout);
	perror(desc);

	exit(EXIT_FAILURE);

	/* NOTREACHED */
}

#define fatal_system_error(file,line,desc) \
_syserr( "(" file ", line " _str(line) ") " desc )

#endif /* defined(H_ERROR_INCLUDED) */
