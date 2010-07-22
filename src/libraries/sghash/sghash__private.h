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
 * @file sghash__private.h
 *
 * @details Details that are private to the SGHASH library.
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SGHASH__PRIVATE_H
#define H_SGHASH__PRIVATE_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

#if defined(WINDOWS)

#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#define u_int8_t  SG_byte
#define u_int32_t SG_uint32
#define u_int64_t SG_uint64
#define caddr_t   void *

#define bzero(p,len)			memset((p),0,(len))
#define bcopy(psrc,pdest,len)	memcpy((pdest),(psrc),(len))

#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN    4321
#define BYTE_ORDER    LITTLE_ENDIAN

#pragma warning( disable : 4244 )	// MSVC complains about possible loss of data when mixing size_t and u_int8_t

#else

#include <stdlib.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <strings.h>
#if defined(MAC)
#include <machine/endian.h>
#elif defined(LINUX)
#include <endian.h>
#endif
#include <assert.h>

#endif

//////////////////////////////////////////////////////////////////

typedef void FN__hash_init  (void * pAlgCtx);
typedef void FN__hash_update(void * pAlgCtx, const SG_byte * pBuf, SG_uint32 lenBuf);
typedef void FN__hash_final (void * pAlgCtx, char * ppszResult);

struct _sghash_algorithm
{
	const char *				pszHashMethod;			// hash-method name (something like "SHA1/160")
	SG_uint32					sizeof_AlgCtx;			// working space required by algorithm
	SG_uint32					strlen_Hash;			// strlen() of a hash result (does not include the null)

	FN__hash_init * const		fn_hash_init;
	FN__hash_update * const		fn_hash_update;
	FN__hash_final * const		fn_hash_final;
};

struct _sghash_handle
{
	const SGHASH_algorithm *	pVTable;
	SG_uint64					variable[1];			// start of AlgCtx -- a variable length array of bytes
};

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SGHASH__PRIVATE_H
