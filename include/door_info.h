/***************************************************************************
 * Portland Doors                                                          *
 * door_info.h:  Utility functions to convert between pointers and 64-bit  *
 *               unsigned integers.                                        *
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

#ifndef H_DOOR_INFO
#define H_DOOR_INFO

#include "door.h"
#include "standards.h"

#include <stdint.h>

union door_ptr_conv_t {
	uintptr_t		ui;
	const void*		optr;
	door_server_proc_t	fptr;
};

static inline uint64_t optr2u64( const void* p )
{
	union door_ptr_conv_t scratch = {
		.ui = 0
	};

	scratch.optr = p;
	return (uint64_t)scratch.ui;
}

static inline uint64_t fptr2u64( door_server_proc_t p )
{
	union door_ptr_conv_t scratch = {
		.ui = 0
	};

	scratch.fptr = p;
	return (uint64_t)scratch.ui;
}

#endif /* Include guard. */
