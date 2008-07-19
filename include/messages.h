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

#define DOOR_CALL_RESERVED	(sizeof(struct msg_door_call))
#define DOOR_RETURN_RESERVED	(sizeof(struct msg_door_return))

#define REQ_DOOR_INFO		0

static inline long long message_type( int d )
{
	static const long long ERROR = -1;
	uint32_t type;

	if ( 0 > recv( d, &type, sizeof(type), MSG_PEEK ) )
		return ERROR;

	return (long long)type;
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

static inline bool is_msg_error( const struct msg_error* p )
{
	return 0U == p->code;
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

static inline bool is_msg_request( const struct msg_request* p )
{
	return 1U == p->code;
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

static inline bool is_msg_door_info( const struct msg_door_info* p )
{
	return 2U == p->code;
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

static inline bool is_msg_door_getparam( const struct msg_door_getparam* p,
                                         unsigned int param
                                       )
{
	return ( 3U == p->code && param == (unsigned int)(p->param) );
}

struct msg_door_call {
	uint32_t	code;
	uint32_t	ndesc;
	uint64_t	arg_size;
};

static inline bool is_msg_door_call( const struct msg_door_call* p )
{
	return 4U == p->code;
}

struct msg_door_return {
	uint32_t        code;
	uint32_t        ndesc;
	uint64_t        arg_size;
};

static inline bool is_msg_door_return( const struct msg_door_return* p )
{
	return 5U == p->code;
}

#endif /* !defined(H_MESSAGES) */
