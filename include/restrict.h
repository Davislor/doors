/***************************************************************************
 * Doors/Linux                                                             *
 * restrict.h: Define the _RESTRICT macro for pre-C99 compatibility.       *
 *             Not used at present.                                        *
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

#ifndef _RESTRICT_INCLUDED
#define _RESTRICT_INCLUDED

#ifdef _RESTRICT
#  undef _RESTRICT
#endif

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901
# define _RESTRICT
#else
# define _RESTRICT restrict
#endif

#endif /* defined(_RESTRICT_INCLUDED) */
