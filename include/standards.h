/***************************************************************************
 * Doors/Linux                                                             *
 * standards.h:  Feature-test macros and configuration options.            *
 *               Include this file in every source file above any other    *
 *               header inclusions, as it may alter definitions in the     *
 *               system include files.                                     *
 *                                                                         *
 *               This version of the program conforms to IEEE Std          *
 *               1003.1-2001 (Posix.1-2001), and declares its macros       *
 *               accordingly.  It should compile using c99 with POSIX      *
 *               thread support.                                           *
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

#ifndef H_STANDARDS_INCLUDED
#define H_STANDARDS_INCLUDED

#define _POSIX_C_SOURCE	200112L
/* Not required on gcc, SunOS 5 or Solaris 10, but harmless: */
#define _XOPEN_SOURCE	600

/* Sun c99 interprets these options to mean that we're writing to an 
 * obsolete standard incompatible with C99.
 *
 * #define _POSIX_SOURCE 1
 * #define _XOPEN_SOURCE_EXTENDED 1
 */

/* GCC-specific options: */
#define _ISOC99_SOURCE	1
#define _REENTRANT	1
#define _THREAD_SAFE	1

#endif /* defined(H_STANDARDS_INCLUDED) */
