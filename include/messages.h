/***************************************************************************
 * Portland Doors                                                          *
 * messages.h:		IPC messages.                                      *
 *                                                                         *
 * Released under the LGPL version 3 (see COPYING).  Copyright (C) 2008    *
 * Loren B. Davis.  Based on work by Jason Lango.                          *
 ***************************************************************************/

#ifndef H_MESSAGES
#define H_MESSAGES

#include "standards.h"
#include "door.h"

#include <stdbool.h>
#include <stdint.h>

#define DOOR_CALL_RESERVED	16
#define DOOR_RETURN_RESERVED	16

struct msg_error {
	char data[8];
};

static inline struct msg_error* msg_error_init( struct msg_error* p,
                                                int e
                                              )
{
	*(uint32_t*)(p->data) = 0;
	*(int32_t*)(p->data + 4) = (int32_t)e;

	return p;
}

static inline int msg_error_decode( const struct msg_error* p )
{
	return (int)*(const uint32_t*)(p->data + 4);
}

static inline bool is_msg_get_error( const struct msg_error* p )
{
	return 0U == *(const uint16_t*)(p->data + 2);
}

struct msg_request {
	char data[8];
};

static inline struct msg_request*
msg_request_init( struct msg_request* p, unsigned int request )
{
	*(uint16_t*)(p->data) = 0U;
	*(uint16_t*)(p->data + 2) = 1U;
	*(uint32_t*)(p->data + 4) = (uint32_t)request;

	return p;
}

static inline unsigned int
msg_request_decode(const struct msg_request* p)
{
	return (unsigned int)*(const uint32_t*)(p->data + 4);
}

static inline bool is_msg_request( const struct msg_request* p )
{
	return 1U == *(const uint16_t*)(p->data + 2);
}

struct msg_door_info {
	char data[40];
};

static inline struct msg_door_info*
msg_door_info_init( struct msg_door_info* p,
                    pid_t target,
                    void (*proc)(),
                    void* cookie,
                    door_attr_t attr,
                    door_id_t id
                  )
{
	*(uint16_t*)(p->data) = 0;
	*(uint16_t*)(p->data + 2) = 2U;
	*(uint32_t*)(p->data + 4) = (uint32_t)attr;
	*(uint64_t*)(p->data + 8) = (uint64_t)target;
	*(uint64_t*)(p->data + 16) = 0;
	memcpy( p->data + 16, &proc, sizeof(proc) );
	*(uint64_t*)(p->data + 24) = (uint64_t)cookie;
	*(uint64_t*)(p->data + 32) = (uint64_t)id;

	return p;
}

static inline struct door_info*
msg_door_info_decode( const struct msg_door_info* p,
                      struct door_info* r
                    )
{
	r->di_target = (pid_t)*(const uint64_t*)(p->data + 8);
	r->di_proc = (door_ptr_t)*(const uint64_t*)(p->data + 16);
	r->di_data = (door_ptr_t)*(const uint64_t*)(p->data + 24);
	r->di_attributes = (door_attr_t)*(const uint32_t*)(p->data + 4);
	r->di_uniquifier = (door_id_t)*(const uint64_t*)(p->data + 32);

	return r;
}

static inline bool is_msg_door_info( const struct msg_door_info* p )
{
	return 2U == *(const uint16_t*)(p->data + 2);
}

struct msg_door_getparam {
	char data[16];
};

static inline struct msg_door_getparam*
msg_door_getparam_init( struct msg_door_getparam* p,
                        unsigned int param,
                        size_t val
                      )
{
	*(uint16_t*)(p->data) = 0U;
	*(uint16_t*)(p->data + 2) = 3U;
	*(uint32_t*)(p->data + 4) = (uint32_t)param;
	*(uint64_t*)(p->data + 8) = (uint64_t)val;

	return p;
}

static inline size_t
msg_door_getparam_decode( const struct msg_door_getparam* p )
{
	return (size_t)*(const uint64_t*)(p->data + 8);
}

static inline bool is_msg_door_getparam( const struct msg_door_getparam* p )
{
	return 3U == *(const uint16_t*)(p->data + 2);
}

struct msg_door_call {
	char header[16];
};

static inline bool is_msg_door_call( const struct msg_door_call* p )
{
	return 4U == *(const uint16_t*)(p->header + 2);
}

struct msg_door_return {
	char header[16];
};

static inline bool is_msg_door_return( const struct msg_door_return* p )
{
	return 5U == *(const uint16_t*)(p->header + 2);
}

#endif /* !defined(H_MESSAGES) */
