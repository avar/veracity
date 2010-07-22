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

// sg_misc_utils.h
// misc utilities.
//////////////////////////////////////////////////////////////////

#ifndef H_SG_MISC_UTILS_H
#define H_SG_MISC_UTILS_H

BEGIN_EXTERN_C

//////////////////////////////////////////////////////////////////

SG_bool SG_parse_bool(const char* psz);
SG_bool SG_int64__fits_in_double(SG_int64 i64);
SG_bool SG_int64__fits_in_int32(SG_int64 i);
SG_bool SG_int64__fits_in_uint32(SG_int64 i);
SG_bool SG_uint64__fits_in_int32(SG_uint64 i);
SG_bool SG_uint64__fits_in_uint32(SG_uint64 i);

SG_bool SG_double__fits_in_int64(double);

/**
 * Sleep n ms (may be rounded up to a second on some platforms).
 */
void SG_sleep_ms(SG_uint32 ms);

//////////////////////////////////////////////////////////////////

/**
 * SG_qsort() is a platform-independent wrapper for qsort() that allows
 * us to pass both a pCtx and a caller-data pointer to the compare function.
 *
 * DO NOT CALL qsort() DIRECTLY.
 *
 * NOTE: If you pass any error information out though the context pointer sent
 * to your compare callback, the sort will continue but the context pointer
 * will remain dirty in subsequent calls to your callback function. Best
 * practice is simply to never pass error information out. (The pointer is
 * essentially an input parameter for your convenience--if you need a context
 * pointer you don't have to allocate it yourself.)
 */

typedef int SG_qsort_compare_function(SG_context * pCtx,
									  const void * pArg1,
									  const void * pArg2,
									  void * pVoidCallerData);

void SG_qsort(SG_context * pCtx,
			  void * pArray, size_t nrElements, size_t sizeElements,
			  SG_qsort_compare_function * pfnCompare,
			  void * pVoidCallerData);

//////////////////////////////////////////////////////////////////

END_EXTERN_C

#endif//H_SG_MISC_UTILS_H
