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

#include <sg.h>

struct _SG_varray
{
	SG_strpool *pStrPool;
	SG_bool strpool_is_mine;
	SG_varpool *pVarPool;
	SG_bool varpool_is_mine;
	SG_variant** aSlots;
	SG_uint32 space;
	SG_uint32 count;
};

void SG_varray__count(SG_context* pCtx, const SG_varray* pva, SG_uint32* piResult)
{
	SG_NULLARGCHECK_RETURN(pva);
	SG_NULLARGCHECK_RETURN(piResult);
	*piResult = pva->count;
}

void sg_varray__grow(SG_context* pCtx, SG_varray* pa)
{
	SG_uint32 new_space = pa->space * 2;
	SG_variant** new_aSlots = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc(pCtx, new_space, sizeof(SG_variant*), &new_aSlots)  );

	memcpy(new_aSlots, pa->aSlots, pa->count * sizeof(SG_variant*));
	SG_NULLFREE(pCtx, pa->aSlots);
	pa->aSlots = new_aSlots;
	pa->space = new_space;
}

void SG_varray__alloc__params(SG_context* pCtx, SG_varray** ppResult, SG_uint32 initial_space, SG_strpool* pStrPool, SG_varpool* pVarPool)
{
	SG_varray * pThis;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pThis)  );

	if (pStrPool)
	{
		pThis->strpool_is_mine = SG_FALSE;
		pThis->pStrPool = pStrPool;
	}
	else
	{
		pThis->strpool_is_mine = SG_TRUE;
		SG_ERR_CHECK(  SG_STRPOOL__ALLOC(pCtx, &pThis->pStrPool, initial_space * 10)  );
	}

	if (pVarPool)
	{
		pThis->varpool_is_mine = SG_FALSE;
		pThis->pVarPool = pVarPool;
	}
	else
	{
		pThis->varpool_is_mine = SG_TRUE;
		SG_ERR_CHECK(  SG_VARPOOL__ALLOC(pCtx, &pThis->pVarPool, initial_space)  );
	}

	pThis->space = initial_space;

	SG_ERR_CHECK(  SG_alloc(pCtx, pThis->space, sizeof(SG_variant *), &pThis->aSlots)  );

	*ppResult = pThis;

	return;
fail:
	if (pThis)
	{
		if (pThis->strpool_is_mine && pThis->pStrPool)
		{
			SG_STRPOOL_NULLFREE(pCtx, pThis->pStrPool);
		}

		if (pThis->varpool_is_mine && pThis->pVarPool)
		{
			SG_VARPOOL_NULLFREE(pCtx, pThis->pVarPool);
		}

		if (pThis->aSlots)
		{
			SG_NULLFREE(pCtx, pThis->aSlots);
		}

		SG_NULLFREE(pCtx, pThis);
	}
}

void SG_varray__alloc__shared(
        SG_context* pCtx,
        SG_varray** ppResult,
        SG_uint32 initial_size,
        SG_varray* pva_other
        )
{
	SG_ERR_CHECK_RETURN(  SG_varray__alloc__params(pCtx, ppResult, initial_size, pva_other->pStrPool, pva_other->pVarPool)  );		// do not use SG_VARRAY__ALLOC__PARAMS
}

void SG_varray__alloc(SG_context* pCtx, SG_varray** ppResult)
{
	SG_ERR_CHECK_RETURN(  SG_varray__alloc__params(pCtx, ppResult, 16, NULL, NULL)  );		// do not use SG_VARRAY__ALLOC__PARAMS
}

void SG_varray__alloc__stringarray(SG_context* pCtx, SG_varray** ppNew, const SG_stringarray* pStringarray)
{
    SG_varray * pNew = NULL;
    SG_uint32 count = 0;
    SG_uint32 i;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(ppNew);
    SG_NULLARGCHECK_RETURN(pStringarray);

    SG_ERR_CHECK(  SG_stringarray__count(pCtx, pStringarray, &count )  );
    SG_ERR_CHECK(  SG_varray__alloc__params(pCtx, &pNew, count, NULL, NULL)  );
    for(i=0;i<count;++i)
    {
        const char * sz = NULL;
        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pStringarray, i, &sz)  );
        SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pNew, sz)  );
    }

    *ppNew = pNew;

    return;
fail:
    SG_VARRAY_NULLFREE(pCtx, pNew);
}

void SG_varray__free(SG_context * pCtx, SG_varray* pThis)
{
	SG_uint32 i;

	if (!pThis)
	{
		return;
	}

	for (i=0; i<pThis->count; i++)
	{
		switch (pThis->aSlots[i]->type)
		{
		case SG_VARIANT_TYPE_VARRAY:
			SG_VARRAY_NULLFREE(pCtx, pThis->aSlots[i]->v.val_varray);
			break;
		case SG_VARIANT_TYPE_VHASH:
			SG_VHASH_NULLFREE(pCtx, pThis->aSlots[i]->v.val_vhash);
			break;
		}
	}

	if (pThis->strpool_is_mine && pThis->pStrPool)
	{
		SG_STRPOOL_NULLFREE(pCtx, pThis->pStrPool);
	}

	if (pThis->varpool_is_mine && pThis->pVarPool)
	{
		SG_VARPOOL_NULLFREE(pCtx, pThis->pVarPool);
	}

	if (pThis->aSlots)
	{
		SG_NULLFREE(pCtx, pThis->aSlots);
	}

	SG_NULLFREE(pCtx, pThis);
}

void sg_varray__append(SG_context* pCtx, SG_varray* pva, SG_variant** ppv)
{
	if ((pva->count + 1) > pva->space)
	{
		SG_ERR_CHECK_RETURN(  sg_varray__grow(pCtx, pva)  );
	}

	SG_ERR_CHECK_RETURN(  SG_varpool__add(pCtx, pva->pVarPool, &(pva->aSlots[pva->count]))  );

	*ppv = pva->aSlots[pva->count++];
}

void SG_varray__remove(SG_context *pCtx, SG_varray *pThis, SG_uint32 idx)
{
	SG_uint32 i;

	SG_NULLARGCHECK_RETURN(pThis);
	if (idx >= pThis->count)
	{
		SG_ERR_THROW_RETURN(SG_ERR_VARRAY_INDEX_OUT_OF_RANGE);
	}

	switch (pThis->aSlots[idx]->type)
	{
		case SG_VARIANT_TYPE_VARRAY:
			SG_VARRAY_NULLFREE(pCtx, pThis->aSlots[idx]->v.val_varray);
			break;
		case SG_VARIANT_TYPE_VHASH:
			SG_VHASH_NULLFREE(pCtx, pThis->aSlots[idx]->v.val_vhash);
			break;
	}	

	for ( i = idx; i < pThis->count - 1; ++i )
	{
		pThis->aSlots[i]->type = pThis->aSlots[i + 1]->type;
		pThis->aSlots[i]->v = pThis->aSlots[i + 1]->v;
	}

	pThis->count--;
}

void SG_varray__append__string__sz(SG_context* pCtx, SG_varray* pva, const char* putf8Value)
{
	SG_ERR_CHECK_RETURN(  SG_varray__append__string__buflen(pCtx, pva, putf8Value, 0) );
}

void SG_varray__append__string__buflen(SG_context* pCtx, SG_varray* pva, const char* putf8Value, SG_uint32 len)
{
	SG_variant* pv = NULL;

	if (putf8Value)
	{
		SG_ERR_CHECK_RETURN(  sg_varray__append(pCtx, pva, &pv)  );

		pv->type = SG_VARIANT_TYPE_SZ;

		SG_ERR_CHECK_RETURN(  SG_strpool__add__buflen__sz(pCtx, pva->pStrPool, putf8Value, len, &(pv->v.val_sz))  );
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_varray__append__null(pCtx, pva)  );
	}
}

void SG_varray__append__int64(SG_context* pCtx, SG_varray* pva, SG_int64 ival)
{
	SG_variant* pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_varray__append(pCtx, pva, &pv)  );

	pv->type = SG_VARIANT_TYPE_INT64;
	pv->v.val_int64 = ival;
}

void SG_varray__append__double(SG_context* pCtx, SG_varray* pva, double fv)
{
	SG_variant* pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_varray__append(pCtx, pva, &pv)  );

	pv->type = SG_VARIANT_TYPE_DOUBLE;
	pv->v.val_double = fv;
}

void SG_varray__appendnew__vhash(SG_context* pCtx, SG_varray* pva, SG_vhash** ppvh)
{
    SG_vhash* pvh_sub = NULL;
    SG_vhash* pvh_result = NULL;

    SG_ERR_CHECK(  SG_vhash__alloc__params(pCtx, &pvh_sub, 4, pva->pStrPool, pva->pVarPool)  );
    pvh_result = pvh_sub;
    SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pva, &pvh_sub)  );

    *ppvh = pvh_result;
    pvh_result = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_sub);
    ;
}

void SG_varray__append__vhash(SG_context* pCtx, SG_varray* pva, SG_vhash** ppvh)
{
	SG_variant* pv = NULL;

	if (*ppvh)
	{
		SG_ERR_CHECK_RETURN(  sg_varray__append(pCtx, pva, &pv)  );

		pv->type = SG_VARIANT_TYPE_VHASH;
		pv->v.val_vhash = *ppvh;
        *ppvh = NULL;
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_varray__append__null(pCtx, pva)  );
	}
}

void SG_varray__appendnew__varray(SG_context* pCtx, SG_varray* pva, SG_varray** ppva)
{
    SG_varray* pva_sub = NULL;
    SG_varray* pva_result = NULL;

    SG_ERR_CHECK(  SG_varray__alloc__params(pCtx, &pva_sub, 4, pva->pStrPool, pva->pVarPool)  );
    pva_result = pva_sub;
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva, &pva_sub)  );

    *ppva = pva_result;
    pva_result = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_sub);
    ;
}

void SG_varray__append__varray(SG_context* pCtx, SG_varray* pva, SG_varray** ppva)
{
	SG_variant* pv = NULL;

	if (*ppva)
	{
		SG_ERR_CHECK_RETURN(  sg_varray__append(pCtx, pva, &pv)  );

		pv->type = SG_VARIANT_TYPE_VARRAY;
		pv->v.val_varray = *ppva;
        *ppva = NULL;
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_varray__append__null(pCtx, pva) );
	}
}

void SG_varray__append__bool(SG_context* pCtx, SG_varray* pva, SG_bool b)
{
	SG_variant* pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_varray__append(pCtx, pva, &pv)  );

	pv->type = SG_VARIANT_TYPE_BOOL;
	pv->v.val_bool = b;
}

void SG_varray__append__null(SG_context* pCtx, SG_varray* pva)
{
	SG_variant* pv = NULL;

	SG_ERR_CHECK_RETURN(  sg_varray__append(pCtx, pva, &pv)  );

	pv->type = SG_VARIANT_TYPE_NULL;
}

void sg_varray__write_json_callback(SG_context* pCtx, void* ctx, SG_UNUSED_PARAM(const SG_varray* pva), SG_UNUSED_PARAM(SG_uint32 ndx), const SG_variant* pv)
{
	SG_jsonwriter* pjson = (SG_jsonwriter*) ctx;

	SG_UNUSED(pva);
	SG_UNUSED(ndx);

	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_element__variant(pCtx, pjson, pv)  );
}

void SG_varray__foreach(SG_context* pCtx, const SG_varray* pva, SG_varray_foreach_callback cb, void* ctx)
{
	SG_uint32 i;

	for (i=0; i<pva->count; i++)
	{
		SG_ERR_CHECK_RETURN(  cb(pCtx, ctx, pva, i, pva->aSlots[i])  );
	}
}

void SG_varray__write_json(SG_context* pCtx, const SG_varray* pva, SG_jsonwriter* pjson)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_start_array(pCtx, pjson)  );

	SG_ERR_CHECK_RETURN(  SG_varray__foreach(pCtx, pva, sg_varray__write_json_callback, pjson)  );

	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_end_array(pCtx, pjson)  );
}

void SG_varray__to_json(SG_context* pCtx, const SG_varray* pva, SG_string* pStr)
{
	SG_jsonwriter* pjson = NULL;

	SG_NULLARGCHECK_RETURN(pva);
	SG_NULLARGCHECK_RETURN(pStr);

	SG_ERR_CHECK(  SG_jsonwriter__alloc(pCtx, &pjson, pStr)  );

	SG_ERR_CHECK(  SG_varray__write_json(pCtx, pva, pjson)  );

fail:
    SG_JSONWRITER_NULLFREE(pCtx, pjson);
}

void SG_varray__equal(SG_context* pCtx, const SG_varray* pv1, const SG_varray* pv2, SG_bool* pbResult)
{
	SG_uint32 i;

	if (pv1->count != pv2->count)
	{
		*pbResult = SG_FALSE;
		return;
	}

	for (i=0; i<pv1->count; i++)
	{
		SG_bool b = SG_FALSE;

		SG_ERR_CHECK_RETURN(  SG_variant__equal(pCtx, pv1->aSlots[i], pv2->aSlots[i], &b)  );

		if (!b)
		{
			*pbResult = SG_FALSE;
			return;
		}
	}

	*pbResult = SG_TRUE;
}

void SG_varray__get__variant(SG_context* pCtx, const SG_varray* pva, SG_uint32 ndx, const SG_variant** pResult)
{
	if (ndx >= pva->count)
	{
		SG_ERR_THROW_RETURN(SG_ERR_VARRAY_INDEX_OUT_OF_RANGE);
	}

	*pResult = pva->aSlots[ndx];
}

void SG_varray__get__sz(SG_context* pCtx, const SG_varray* pva, SG_uint32 ndx, const char** pResult)
{
	if (ndx >= pva->count)
	{
		SG_ERR_THROW_RETURN(SG_ERR_VARRAY_INDEX_OUT_OF_RANGE);
	}

	SG_ERR_CHECK_RETURN(  SG_variant__get__sz(pCtx, pva->aSlots[ndx], pResult)  );
}

void SG_varray__get__int64(SG_context* pCtx, const SG_varray* pva, SG_uint32 ndx, SG_int64* pResult)
{
	if (ndx >= pva->count)
	{
		SG_ERR_THROW_RETURN(SG_ERR_VARRAY_INDEX_OUT_OF_RANGE);
	}

	SG_ERR_CHECK_RETURN(  SG_variant__get__int64(pCtx, pva->aSlots[ndx], pResult)  );
}

void SG_varray__get__double(SG_context* pCtx, const SG_varray* pva, SG_uint32 ndx, double* pResult)
{
	if (ndx >= pva->count)
	{
		SG_ERR_THROW_RETURN(SG_ERR_VARRAY_INDEX_OUT_OF_RANGE);
	}

	SG_ERR_CHECK_RETURN(  SG_variant__get__double(pCtx, pva->aSlots[ndx], pResult)  );
}

void SG_varray__get__bool(SG_context* pCtx, const SG_varray* pva, SG_uint32 ndx, SG_bool* pResult)
{
	if (ndx >= pva->count)
	{
		SG_ERR_THROW_RETURN(SG_ERR_VARRAY_INDEX_OUT_OF_RANGE);
	}

	SG_ERR_CHECK_RETURN(  SG_variant__get__bool(pCtx, pva->aSlots[ndx], pResult)  );
}

void SG_varray__get__varray(SG_context* pCtx, const SG_varray* pva, SG_uint32 ndx, SG_varray** pResult)  // TODO should the result be const?
{
	if (ndx >= pva->count)
	{
		SG_ERR_THROW_RETURN(SG_ERR_VARRAY_INDEX_OUT_OF_RANGE);
	}

	SG_ERR_CHECK_RETURN(  SG_variant__get__varray(pCtx, pva->aSlots[ndx], pResult)  );
}

void SG_varray__get__vhash(SG_context* pCtx, const SG_varray* pva, SG_uint32 ndx, SG_vhash** pResult)  // TODO should the result be const?
{
	if (ndx >= pva->count)
	{
		SG_ERR_THROW_RETURN(SG_ERR_VARRAY_INDEX_OUT_OF_RANGE);
	}

	SG_ERR_CHECK_RETURN(  SG_variant__get__vhash(pCtx, pva->aSlots[ndx], pResult)  );
}

void SG_varray__typeof(SG_context* pCtx, const SG_varray* pva, SG_uint32 ndx, SG_uint16* pResult)
{
	if (ndx >= pva->count)
	{
		SG_ERR_THROW_RETURN(SG_ERR_VARRAY_INDEX_OUT_OF_RANGE);
	}

	*pResult = pva->aSlots[ndx]->type;
}

void SG_varray__sort(SG_context* pCtx, SG_varray * pva, SG_qsort_compare_function pfnCompare)
{
	SG_NULLARGCHECK_RETURN(pva);
	SG_ARGCHECK_RETURN( (pfnCompare!=NULL), pfnCompare );

	if (pva->count > 1)
	{
		/* We used to check every item for sortability, but since we can have a custom callback
		 * to sort vhashes by a specific key, it's better to let the comparer check for sortability
		 * (which they do).
		 */
		/* Now the sort should be completely safe, so just do it */

		SG_ERR_CHECK_RETURN(  SG_qsort(pCtx,
									   pva->aSlots,pva->count,sizeof(SG_variant *),
									   pfnCompare,
									   NULL)  );
	}
}

//////////////////////////////////////////////////////////////////

static void _sg_varray__to_idset_callback(
	SG_context* pCtx,
	void * ctx,
	SG_UNUSED_PARAM(const SG_varray * pva),
	SG_UNUSED_PARAM(SG_uint32 ndx),
	const SG_variant * pVariant
	)
{
	SG_rbtree * pidset = (SG_rbtree *)ctx;
	const char * szTemp;

	SG_UNUSED(pva);
	SG_UNUSED(ndx);

	SG_variant__get__sz(pCtx,pVariant,&szTemp);
	// might be SG_ERR_VARIANT_INVALIDTYPE
	SG_ERR_CHECK_RETURN_CURRENT;
	if (!szTemp)					// happens when SG_VARIANT_TYPE_NULL
		return;						// silently eat it.

	/* TODO should we silently eat the SG_VARIANT_TYPE_NULL case in
	 * this context?  In what situation are we converting a varray to
	 * an idset where we want to allow NULLs? */

	SG_ERR_CHECK_RETURN(  SG_rbtree__add(pCtx, pidset,szTemp)  );
}

void SG_varray__to_rbtree(SG_context* pCtx, const SG_varray * pva, SG_rbtree ** ppidset)
{
	SG_rbtree * pidset = NULL;

	SG_NULLARGCHECK_RETURN(pva);
	SG_NULLARGCHECK_RETURN(ppidset);

	SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC__PARAMS(pCtx, &pidset, pva->count, NULL)  );

	SG_ERR_CHECK(  SG_varray__foreach(pCtx, pva,_sg_varray__to_idset_callback,pidset)  );

	*ppidset = pidset;
    pidset = NULL;

fail:
	SG_RBTREE_NULLFREE(pCtx, pidset);
}

static void _sg_varray__to_stringarray_callback(
	SG_context* pCtx,
	void * ctx,
	SG_UNUSED_PARAM(const SG_varray * pva),
	SG_UNUSED_PARAM(SG_uint32 ndx),
	const SG_variant * pVariant
	)
{
	SG_stringarray * psa = (SG_stringarray *)ctx;
	const char * szTemp;

	SG_UNUSED(pva);
	SG_UNUSED(ndx);

	SG_variant__get__sz(pCtx,pVariant,&szTemp);
	// might be SG_ERR_VARIANT_INVALIDTYPE
	SG_ERR_CHECK_RETURN_CURRENT;
	if (!szTemp)					// happens when SG_VARIANT_TYPE_NULL
		return;						// silently eat it.

	/* TODO should we silently eat the SG_VARIANT_TYPE_NULL case in
	 * this context?  In what situation are we converting a varray to
	 * an idset where we want to allow NULLs? */

	SG_ERR_CHECK_RETURN(  SG_stringarray__add(pCtx, psa, szTemp)  );
}

void SG_varray__to_stringarray(SG_context* pCtx, const SG_varray * pva, SG_stringarray ** ppsa)
{
	SG_stringarray * psa = NULL;

	SG_NULLARGCHECK_RETURN(pva);
	SG_NULLARGCHECK_RETURN(ppsa);

	SG_ERR_CHECK_RETURN(  SG_STRINGARRAY__ALLOC(pCtx, &psa, 1 + pva->count)  );

	SG_ERR_CHECK(  SG_varray__foreach(pCtx, pva,_sg_varray__to_stringarray_callback,psa)  );

	*ppsa = psa;
    psa = NULL;

	return;
fail:
	SG_STRINGARRAY_NULLFREE(pCtx, psa);
}

void SG_varray__to_vhash_with_indexes(SG_context* pCtx, const SG_varray * pva, SG_vhash ** ppvh)
{
    SG_uint32 i = 0;
    SG_uint32 count = 0;
    SG_vhash* pvh = NULL;

	SG_NULLARGCHECK_RETURN(pva);
	SG_NULLARGCHECK_RETURN(ppvh);

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    SG_ERR_CHECK(  SG_vhash__alloc__params(pCtx, &pvh, count, NULL, NULL)  );
    for (i=0; i<count; i++)
    {
        const char* psz = NULL;

        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, i, &psz)  );
        SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, psz, (SG_int64) i)  );
    }

	*ppvh = pvh;
    pvh = NULL;

	return;
fail:
	SG_VHASH_NULLFREE(pCtx, pvh);
}

//////////////////////////////////////////////////////////////////

void SG_varray_debug__dump_varray_of_vhashes_to_console(SG_context * pCtx, const SG_varray * pva, const char * pszLabel)
{
	SG_vhash * pvhItem = NULL;
	SG_string * pString = NULL;
	SG_uint32 k = 0, count = 0;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, ("VARRAY-of-VHASH Dump: %s\n"), ((pszLabel) ? pszLabel : ""))  );

	SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
	for (k=0; k<count; k++)
	{
		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, k, &pvhItem)  );

		SG_ERR_CHECK(  SG_string__clear(pCtx, pString)  );
		SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhItem, pString)  );
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR ,"%s\n", SG_string__sz(pString))  );
	}

fail:
    SG_STRING_NULLFREE(pCtx, pString);
}

void SG_varray__copy_items(
    SG_context* pCtx,
    const SG_varray* pva_from,
    SG_varray* pva_to
	)
{
    SG_uint32 i = 0;
    SG_vhash* pvh_sub = NULL;
    SG_varray* pva_sub = NULL;

    for (i=0; i<pva_from->count; i++)
    {
        const SG_variant* pv = NULL;

        SG_ERR_CHECK(  SG_varray__get__variant(pCtx, pva_from, i, &pv)  );
        switch (pv->type)
        {
        case SG_VARIANT_TYPE_DOUBLE:
            SG_ERR_CHECK(  SG_varray__append__double(pCtx, pva_to, pv->v.val_double)  );
            break;

        case SG_VARIANT_TYPE_INT64:
            SG_ERR_CHECK(  SG_varray__append__int64(pCtx, pva_to, pv->v.val_int64)  );
            break;

        case SG_VARIANT_TYPE_BOOL:
            SG_ERR_CHECK(  SG_varray__append__bool(pCtx, pva_to, pv->v.val_bool)  );
            break;

        case SG_VARIANT_TYPE_NULL:
            SG_ERR_CHECK(  SG_varray__append__null(pCtx, pva_to)  );
            break;

        case SG_VARIANT_TYPE_SZ:
            SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_to, pv->v.val_sz)  );
            break;

        case SG_VARIANT_TYPE_VHASH:
            SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva_to, &pvh_sub)  );
            SG_ERR_CHECK(  SG_vhash__copy_items(pCtx, pv->v.val_vhash, pvh_sub)  );
            break;

        case SG_VARIANT_TYPE_VARRAY:
            SG_ERR_CHECK(  SG_varray__appendnew__varray(pCtx, pva_to, &pva_sub)  );
            SG_ERR_CHECK(  SG_varray__copy_items(pCtx, pv->v.val_varray, pva_sub)  );
            break;
        }
    }

fail:
    ;
}

