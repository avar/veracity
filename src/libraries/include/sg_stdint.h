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

// sg_stdint.h
// Declarations for known-width integer types (like <stdint.h>).
//////////////////////////////////////////////////////////////////

#ifndef H_SG_STDINT_H
#define H_SG_STDINT_H

BEGIN_EXTERN_C

//////////////////////////////////////////////////////////////////
// basic scalar types for all interface definitions.  these must
// safely cross languages and be platform-independent.
//
// C99 defines <stdint.h> which defines the intX_t and uintX_t
// types and a collection of macros to help out -- but VS2005 does
// not ship a version of it.
//
// we define SG_uint32 and friends ourselves.

typedef unsigned char			SG_byte;

#if defined(WINDOWS)

typedef signed char				SG_int8;
typedef unsigned char			SG_uint8;

typedef signed short			SG_int16;
typedef unsigned short			SG_uint16;

typedef int						SG_int32;
typedef unsigned int			SG_uint32;

typedef __int64					SG_int64;
typedef unsigned __int64		SG_uint64;

#else

#include <stdint.h>

typedef int8_t					SG_int8;
typedef uint8_t					SG_uint8;

typedef int16_t					SG_int16;
typedef uint16_t				SG_uint16;

typedef int32_t					SG_int32;
typedef uint32_t				SG_uint32;

typedef int64_t					SG_int64;
typedef uint64_t				SG_uint64;

#endif

SG_STATIC_ASSERT(sizeof(SG_int8)==1);
SG_STATIC_ASSERT(sizeof(SG_uint8)==1);
SG_STATIC_ASSERT(sizeof(SG_int16)==2);
SG_STATIC_ASSERT(sizeof(SG_uint16)==2);
SG_STATIC_ASSERT(sizeof(SG_int32)==4);
SG_STATIC_ASSERT(sizeof(SG_uint32)==4);
SG_STATIC_ASSERT(sizeof(SG_int64)==8);
SG_STATIC_ASSERT(sizeof(SG_uint64)==8);

#define SG_UINT8_MAX			((SG_uint8) 0xffU)
#define SG_UINT16_MAX			((SG_uint16)0xffffU)
#define SG_UINT32_MAX			((SG_uint32)0xffffffffUL)
#define SG_UINT64_MAX			((SG_uint64)0xffffffffffffffffULL)

#define SG_INT8_MAX				((SG_int8) 0x7f)
#define SG_INT16_MAX			((SG_int16)0x7fff)
#define SG_INT32_MAX			((SG_int32)0x7fffffffL)
#define SG_INT64_MIN			((SG_int64)0x8000000000000000LL)
#define SG_INT64_MAX			((SG_int64)0x7fffffffffffffffLL)

typedef SG_int32 SG_enumeration;

//////////////////////////////////////////////////////////////////

END_EXTERN_C

#endif//H_SG_STDINT_H
