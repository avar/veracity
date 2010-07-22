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
 * @file sg_stringarray_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_STRINGARRAY_H
#define H_SG_STRINGARRAY_H

BEGIN_EXTERN_C;

void SG_stringarray__alloc(
	SG_context* pCtx,
	SG_stringarray** ppThis,
	SG_uint32 count
	);

#if 0
void SG_stringarray__alloc__copy(
	SG_context* pCtx,
	SG_stringarray** ppThis,
	const SG_stringarray* pOther
	);
#endif

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#define __SG_STRINGARRAY__ALLOC__(pp,expr)		SG_STATEMENT(	SG_stringarray * _p = NULL;											\
																expr;																\
																_sg_mem__set_caller_data(_p,__FILE__,__LINE__,"SG_stringarray");	\
																*(pp) = _p;															)

#define SG_STRINGARRAY__ALLOC(pCtx,ppNew,c)						__SG_STRINGARRAY__ALLOC__(ppNew, SG_stringarray__alloc      (pCtx,&_p,c) )
#define SG_STRINGARRAY__ALLOC__COPY(pCtx,ppNew,pStringArray)	__SG_STRINGARRAY__ALLOC__(ppNew, SG_stringarray__alloc__copy(pCtx,&_p,pStringArray) )

#else

#define SG_STRINGARRAY__ALLOC(pCtx,ppNew,c)						SG_stringarray__alloc      (pCtx,ppNew,c)
#define SG_STRINGARRAY__ALLOC__COPY(pCtx,ppNew,pStringArray)	SG_stringarray__alloc__copy(pCtx,ppNew,pStringArray)

#endif

//////////////////////////////////////////////////////////////////

void SG_stringarray__free(SG_context * pCtx, SG_stringarray* pThis);

void SG_stringarray__add(
	SG_context* pCtx,
	SG_stringarray* pStringarray,
	const char* psz
	);

void SG_stringarray__sort(
	SG_context* pCtx,
	SG_stringarray* pStringarray,
	SG_qsort_compare_function compare_function
	);

void SG_stringarray__remove_all(
	SG_context* pCtx,
	SG_stringarray* pStringarray,
	const char* psz, ///< String to be removed from the list--case sensitive!
	SG_uint32 * pNumOccurrencesRemoved ///< Optional output parameter
	);

void SG_stringarray__get_nth(
	SG_context* pCtx,
	const SG_stringarray* pStringarray,
	SG_uint32 n,
	const char** ppsz
	);

/**
 * Find the first occurrence of the given string in the
 * array at or beyond the starting index.
 */
void SG_stringarray__find(
	SG_context * pCtx,
	const SG_stringarray * psa,
	const char * psz,
	SG_uint32 ndxStart,
	SG_bool * pbFound,
	SG_uint32 * pndxFound);

void SG_stringarray__count(
	SG_context* pCtx,
	const SG_stringarray* psa,
	SG_uint32 * pCount);

void SG_stringarray__sz_array(
	SG_context* pCtx,
	const SG_stringarray* psa,
	const char * const ** pppStrings);

void SG_stringarray__sz_array_and_count(
	SG_context* pCtx,
	const SG_stringarray* psa,
	const char * const ** pppStrings,
	SG_uint32 * pCount);

END_EXTERN_C;

#endif //H_SG_STRINGARRAY_H
