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
#include <string.h>
#include <sys/types.h>

#define DOOR_CALL_RESERVED	(sizeof(struct msg_door_call))
#define DOOR_RETURN_RESERVED	(sizeof(struct msg_door_return))

#define REQ_DOOR_INFO		0

/* Message types are 32-bit unsigned integers.  Therefore, the only
 * standard type guaranteed to hold any message type, or the value -1,
 * is a signed long long int.
 */
static inline long long int message_type( int d )
{
	static const long long ERROR = -1;
	uint32_t type;

	if ( 0 > recv( d, &type, sizeof(type), MSG_PEEK ) )
		return ERROR;

	return (long long int)type;
}

struct msg_error {
	uint32_t	code;
        int32_t		value;
};

static inline struct msg_error* msg_error_init( struct msg_error* p,
                                                int e
                                              )
{
	p->code = 0;
	p->value = (int32_t)e;

	return p;
}

static inline int msg_error_decode( const struct msg_error* p )
{
	return (int)(p->value);
}

struct msg_request {
	uint32_t	code;
	uint32_t	request;
};

static inline struct msg_request*
msg_request_init( struct msg_request* p, unsigned int request )
{
	p->code = 1U;
	p->request = request;

	return p;
}

static inline unsigned int
msg_request_decode(const struct msg_request* p)
{
	return (unsigned int)(p->request);
}

struct msg_door_info {
	uint32_t	code;
	uint32_t	attr;
	uint64_t	target;
	uint64_t	proc;
	uint64_t	cookie;
	uint64_t	id;
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
	p->code = 2U;
	p->attr = (uint32_t)attr;
	p->target = (uint64_t)target;
	p->proc = 0;
	memcpy( &p->proc, &proc, sizeof(proc) );
	p->cookie = (uint64_t)cookie;
	p->id = (uint64_t)id;

	return p;
}

static inline struct door_info*
msg_door_info_decode( const struct msg_door_info* p,
                      struct door_info* r
                    )
{
	r->di_target = (pid_t)(p->target);
	r->di_proc = (door_ptr_t)(p->proc);
	r->di_data = (door_ptr_t)(p->cookie);
	r->di_attributes = (door_attr_t)(p->attr);
	r->di_uniquifier = (door_id_t)(p->id);

	return r;
}

struct msg_door_getparam {
	uint32_t	code;
	uint32_t	param;
	uint64_t	value;
};

static inline struct msg_door_getparam*
msg_door_getparam_init( struct msg_door_getparam* p,
                        unsigned int param,
                        size_t val
                      )
{
	p->code = 3U;
	p->param = (uint32_t)param;
	p->value = (uint64_t)val;

	return p;
}

static inline size_t
msg_door_getparam_decode( const struct msg_door_getparam* p )
{
	return (size_t)(p->value);
}

struct msg_door_call {
	uint32_t	code;
	uint32_t	ndesc;
	uint64_t	arg_size;
};

static inline struct msg_door_call*
msg_door_call_init( struct msg_door_call* p,
                    size_t data_size
                  )
{
	p -> code = 4U;
	p -> ndesc = 0U;
	p -> arg_size = (uint64_t)data_size;

	return p;
}

struct msg_door_return {
	uint32_t        code;
	uint32_t        ndesc;
	uint64_t        arg_size;
};

static ssize_t
msg_door_return_get_data_size( const struct msg_door_return* p )
{
	if ( SIZE_MAX < p->arg_size ) {
/* The server returned more data than our memory model can support! */
		return -1;
	}
	else
		return (ssize_t)(p->arg_size);
}

#endif /* !defined(H_MESSAGES) */
