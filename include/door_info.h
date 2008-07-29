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

#include "standards.h"

#include <stdint.h>

typedef void (*server_proc_t) (void*, char*, size_t, door_desc_t*, uint_t);

union door_ptr_conv_t {
	uint64_t	u64;
	void*		optr;
	server_proc_t	fptr;
};

static inline uint64_t optr2u64(void* p)
{
	union door_ptr_conv_t scratch = {
		.u64 = 0
	};

	scratch.optr = p;
	return scratch.u64;
}

static inline uint64_t fptr2u64(server_proc_t p)
{
	union door_ptr_conv_t scratch = {
		.u64 = 0
	};

	scratch.fptr = p;
	return scratch.u64;
}

#endif /* Include guard. */
