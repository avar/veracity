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

#ifndef H_SG_VARRAY_H
#define H_SG_VARRAY_H

BEGIN_EXTERN_C

/*
  SG_varray is an array for typed values.
*/

//////////////////////////////////////////////////////////////////

void SG_varray__alloc__params(
        SG_context* pCtx,
        SG_varray** ppNew,
        SG_uint32 initial_size,
        SG_strpool* pPool,
        SG_varpool* pVarPool
        );

void SG_varray__alloc__shared(
        SG_context* pCtx,
        SG_varray** ppNew,
        SG_uint32 initial_size,
        SG_varray* pva_other
        );

void SG_varray__alloc(SG_context* pCtx, SG_varray** ppNew);

void SG_varray__alloc__stringarray(SG_context* pCtx, SG_varray** ppNew, const SG_stringarray* pStringarray);

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#define __SG_VARRAY__ALLOC__(pp,expr)		SG_STATEMENT(	SG_varray * _p = NULL;										\
															expr;														\
															_sg_mem__set_caller_data(_p,__FILE__,__LINE__,"SG_varray");	\
															*(pp) = _p;													)

#define SG_VARRAY__ALLOC__PARAMS(pCtx,ppNew,initial_size,pStrPool,pVarPool)		__SG_VARRAY__ALLOC__(ppNew, SG_varray__alloc__params   (pCtx,&_p,initial_size,pStrPool,pVarPool) )
#define SG_VARRAY__ALLOC(pCtx,ppNew)											__SG_VARRAY__ALLOC__(ppNew, SG_varray__alloc           (pCtx,&_p) )

#else

#define SG_VARRAY__ALLOC__PARAMS(pCtx,ppNew,initial_size,pStrPool,pVarPool)		SG_varray__alloc__params   (pCtx,ppNew,initial_size,pStrPool,pVarPool)
#define SG_VARRAY__ALLOC(pCtx,ppNew)											SG_varray__alloc           (pCtx,ppNew)

#endif

//////////////////////////////////////////////////////////////////

void        SG_varray__free(SG_context * pCtx, SG_varray* pva);

void        SG_varray__count(SG_context*, const SG_varray* pva, SG_uint32* piResult);

void        SG_varray__write_json(SG_context*, const SG_varray* pva, SG_jsonwriter* pjson);
void        SG_varray__to_json(SG_context*, const SG_varray* pva, SG_string* pStr);

void        SG_varray__append__string__sz(SG_context*, SG_varray* pva, const char* putf8Value);
void        SG_varray__append__string__buflen(SG_context*, SG_varray* pva, const char* putf8Value, SG_uint32 len);
void        SG_varray__append__int64(SG_context*, SG_varray* pva, SG_int64 intValue);
void        SG_varray__append__double(SG_context*, SG_varray* pva, double fv);
void        SG_varray__append__vhash(SG_context*, SG_varray* pva, SG_vhash** ppvh);
void SG_varray__appendnew__vhash(SG_context* pCtx, SG_varray* pva, SG_vhash** ppvh);
void SG_varray__appendnew__varray(SG_context* pCtx, SG_varray* pva, SG_varray** ppva);
void        SG_varray__append__varray(SG_context*, SG_varray* pva, SG_varray** ppva_val);
void        SG_varray__append__bool(SG_context*, SG_varray* pva, SG_bool b);
void        SG_varray__append__null(SG_context*, SG_varray* pva);

// TODO insert

void        SG_varray__remove(SG_context*, SG_varray* pva, SG_uint32 ndx);

void        SG_varray__get__variant(SG_context*, const SG_varray* pva, SG_uint32 ndx, const SG_variant** ppResult);

void        SG_varray__get__sz(SG_context*, const SG_varray* pva, SG_uint32 ndx, const char** pResult);
void        SG_varray__get__int64(SG_context*, const SG_varray* pva, SG_uint32 ndx, SG_int64* pResult);
void        SG_varray__get__double(SG_context*, const SG_varray* pva, SG_uint32 ndx, double* pResult);
void        SG_varray__get__bool(SG_context*, const SG_varray* pva, SG_uint32 ndx, SG_bool* pResult);
void        SG_varray__get__vhash(SG_context*, const SG_varray* pva, SG_uint32 ndx, SG_vhash** pResult);  // TODO should the result be const?
void        SG_varray__get__varray(SG_context*, const SG_varray* pva, SG_uint32 ndx, SG_varray** pResult);  // TODO should the result be const?

typedef void (SG_varray_foreach_callback)(SG_context* pCtx, void* pVoidData, const SG_varray* pva, SG_uint32 ndx, const SG_variant* pv);

void        SG_varray__foreach(SG_context*, const SG_varray* pva, SG_varray_foreach_callback cb, void* ctx);

void        SG_varray__equal(SG_context*, const SG_varray* pv1, const SG_varray* pv2, SG_bool* pbResult);

void        SG_varray__typeof(SG_context*, const SG_varray* pva, SG_uint32 ndx, SG_uint16* pResult);

/**
 * Sort the varray.  Elements added after sorting will be appended to
 * the end (they are not inserted in order).
 *
 */
void SG_varray__sort(SG_context*, SG_varray * pva, SG_qsort_compare_function pfnCompare);

void SG_varray__to_rbtree(SG_context*, const SG_varray * pva, SG_rbtree ** pprbtree);

void SG_varray__to_stringarray(SG_context*, const SG_varray * pva, SG_stringarray ** ppsa);
void SG_varray__to_vhash_with_indexes(SG_context*, const SG_varray * pva, SG_vhash ** ppvh);

void SG_varray__copy_items(
    SG_context* pCtx,
    const SG_varray* pva_from,
    SG_varray* pva_to
	);


void SG_varray_debug__dump_varray_of_vhashes_to_console(SG_context * pCtx, const SG_varray * pva, const char * pszLabel);

END_EXTERN_C

#endif
