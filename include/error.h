/***************************************************************************
 * Portland Doors                                                          *
 * error.h: Error-handling functions.                                      *
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

#ifndef H_ERROR_INCLUDED
#define H_ERROR_INCLUDED

extern void _syserr( const char* desc );

#define _str(x) #x

#define fatal_system_error(file,line,desc) \
_syserr( "(" file ", line " _str(line) ") " desc )

#endif /* defined(H_ERROR_INCLUDED) */
