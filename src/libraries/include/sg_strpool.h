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
 * @file sg_strpool.h
 *
 * @details SG_strpool is a way to have lots of strings pooled into
 * fewer mallocs.  When you add a string to the pool, it copies the
 * string into its pool and returns a pointer to the copy.
 *
 */

#ifndef H_SG_STRPOOL_H
#define H_SG_STRPOOL_H

BEGIN_EXTERN_C

void SG_strpool__alloc(
	SG_context* pCtx,
	SG_strpool** ppNew,
	SG_uint32 subpool_space
	);

#if defined(DEBUG)
#define SG_STRPOOL__ALLOC(pCtx,ppNew,subpool_space)		SG_STATEMENT(	SG_strpool * _p = NULL;											\
																		SG_strpool__alloc(pCtx,&_p,subpool_space);						\
																		_sg_mem__set_caller_data(_p,__FILE__,__LINE__,"SG_strpool");	\
																		*(ppNew) = _p;													)
#else
#define SG_STRPOOL__ALLOC(pCtx,ppNew,subpool_space)		SG_strpool__alloc(pCtx,ppNew,subpool_space)
#endif

//////////////////////////////////////////////////////////////////

void SG_strpool__free(SG_context * pCtx, SG_strpool* pPool);

void SG_strpool__add__sz(
	SG_context* pCtx,
	SG_strpool* pPool,
	const char* pszIn,
	const char** ppszOut
	);

/**
 * add a string specified by a pointer and a length.
 * if you specify length == 0 then it will use strlen
 * if you specify a length > strlen, it will use strlen
 *
 * The resulting string in the pool will be null-terminated.
 * Use this when you want to add a string which is not
 * null terminated now but needs to be, such as when you
 * want to add some characters from the middle of an
 * existing string.
 */
void        SG_strpool__add__buflen__sz(
	SG_context* pCtx,
	SG_strpool* pPool,
	const char* pszIn,
	SG_uint32 len,
	const char** ppszOut
	);

/**
 * Use this to add a string to the pool which is
 * not null terminated and shouldn't be.
 */
void        SG_strpool__add__buflen__binary(
	SG_context* pCtx,
	SG_strpool* pPool,
	const char* pIn,
	SG_uint32 len,
	const char** ppOut
	);

void        SG_strpool__add__len(
	SG_context* pCtx,
	SG_strpool* pPool,
	SG_uint32 len,
	const char** ppOut
	);

END_EXTERN_C

#endif
