/*
Copyright 2010 SourceGear, LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/**
 *
 * @file sg_context_typedefs.h
 *
 */

#ifndef H_SG_CONTEXT_TYPEDEFS_H
#define H_SG_CONTEXT_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////
// These are garden-variety public definitions.
// (You need not pretend you didn't see them here.)

typedef void SG_context__msg_callback(
	SG_context * pCtx,
	const char * pszMsg);


//////////////////////////////////////////////////////////////////
// ALL OF THESE DEFINITIONS SHOULD BE CONSIDERED *PRIVATE* TO sg_context.c
// Normally, these would be hidden within sg_context.c and only the typedef
// would be public, but we need the structure declarations to allow (for
// performance reasons) some of the SG_ERR_ macros to look at the error
// state directly without doing a function call.  So pretend you didn't
// see this in a public header file.

#define SG_CONTEXT_MAX_STACK_TRACE_LEN_BYTES		1048576
#define SG_CONTEXT_LEN_DESCRIPTION					1023
#define SG_CONTEXT_MAX_ERROR_LEVELS					100

#define SG_CONTEXT_MAX_MSG_LISTENERS 8

struct _sg_context
{
	SG_uint32		level;				// current error value is errValue[level]
	SG_uint32		lenStackTrace;		// current strlen(szStackTrace)

	SG_error		errValues[SG_CONTEXT_MAX_ERROR_LEVELS];

	SG_bool			bStackTraceAtLimit; // Rather than returning an error if we run out of space for the stack trace,
										// we set this true and ignore subsequent string appends.

	char			szDescription[SG_CONTEXT_LEN_DESCRIPTION + 1];
	char			szStackTrace[SG_CONTEXT_MAX_STACK_TRACE_LEN_BYTES + 1];

	SG_uint32		numMsgListeners;
	SG_context__msg_callback* msg_callbacks[SG_CONTEXT_MAX_MSG_LISTENERS];
};

// We already defined the following in <sg.h>
//     typedef struct _sg_context SG_context;
// so we can't do it here.

/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_CONTEXT_TYPEDEFS_H
