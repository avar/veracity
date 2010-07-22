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

struct _sg_stringarray
{
	SG_uint32 count;
	SG_uint32 space;
	SG_strpool* pStrPool;
	const char** aStrings;
};


void SG_stringarray__alloc(
	SG_context* pCtx,
	SG_stringarray** ppThis,
	SG_uint32 space
	)
{
	SG_stringarray* pThis = NULL;

    if(space==0)
        space = 1;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pThis)  );

	pThis->space = space;

	SG_ERR_CHECK(  SG_alloc(pCtx, pThis->space, sizeof(const char *), &pThis->aStrings)  );
	SG_ERR_CHECK(  SG_STRPOOL__ALLOC(pCtx, &pThis->pStrPool, pThis->space * 64)  );

	*ppThis = pThis;
	return;

fail:
	SG_STRINGARRAY_NULLFREE(pCtx, pThis);
}

#if 0
void SG_stringarray__alloc__copy(
	SG_context* pCtx,
	SG_stringarray** ppThis,
	const SG_stringarray* pOther
	)
{
    SG_uint32 count = 0;
    SG_stringarray * pThis = NULL;
    SG_uint32 i;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(ppThis);
    SG_NULLARGCHECK_RETURN(pOther);

    SG_ERR_CHECK(  SG_stringarray__count(pCtx, pOther, &count)  );
    SG_ERR_CHECK(  SG_stringarray__alloc(pCtx, &pThis, count)  );
    for(i=0;i<count;++i)
    {
        const char * sz = NULL;
        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pOther, i, &sz)  );
        SG_ERR_CHECK(  SG_stringarray__add(pCtx, pThis, sz)  );
    }

    *ppThis = pThis;

    return;
fail:
    SG_STRINGARRAY_NULLFREE(pCtx, pThis);
}
#endif

void SG_stringarray__free(SG_context * pCtx, SG_stringarray* pThis)
{
	if (!pThis)
	{
		return;
	}

	SG_NULLFREE(pCtx, pThis->aStrings);
	SG_STRPOOL_NULLFREE(pCtx, pThis->pStrPool);
	SG_NULLFREE(pCtx, pThis);
}

void SG_stringarray__add(
	SG_context* pCtx,
	SG_stringarray* psa,
	const char* psz
	)
{
	const char* pCopy = NULL;
	const char** new_aStrings;

	SG_NULLARGCHECK_RETURN( psa );
	SG_NULLARGCHECK_RETURN( psz );

	if ((1 + psa->count) > psa->space)
	{
        SG_uint32 new_space = psa->space * 2;
		SG_ERR_CHECK(  SG_alloc(pCtx, new_space, sizeof(const char *), &new_aStrings)  );
        memcpy((char **)new_aStrings, psa->aStrings, psa->count * sizeof(const char*));
        SG_NULLFREE(pCtx, psa->aStrings);
        psa->aStrings = new_aStrings;
        psa->space = new_space;
	}

	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, psa->pStrPool, psz, &pCopy)  );

	psa->aStrings[psa->count++] = pCopy;

fail:
	return;
}

void SG_stringarray__sort(
	SG_context* pCtx,
	SG_stringarray* pStringarray,
    SG_qsort_compare_function compare_function
	)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pStringarray);
    SG_ERR_CHECK_RETURN(  SG_qsort(pCtx, (void*)pStringarray->aStrings, pStringarray->count, sizeof(char *), compare_function, NULL)  );
}

void SG_stringarray__remove_all(
    SG_context* pCtx,
    SG_stringarray* psa,
    const char* psz,
    SG_uint32 * pNumOccurrencesRemoved
    )
{
    SG_uint32 numOccurrencesRemoved = 0;
    SG_uint32 i;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(psa);
    SG_NULLARGCHECK_RETURN(psz);

    for(i=0;i<psa->count;++i)
    {
        if(strcmp(psa->aStrings[i],psz)==0)
            numOccurrencesRemoved += 1;
        else
            psa->aStrings[i-numOccurrencesRemoved] = psa->aStrings[i];
    }

    psa->count -= numOccurrencesRemoved;

    if(pNumOccurrencesRemoved!=NULL)
        *pNumOccurrencesRemoved = numOccurrencesRemoved;
}


void SG_stringarray__get_nth(
	SG_context* pCtx,
	const SG_stringarray* psa,
	SG_uint32 n,
	const char** ppsz
	)
{

	SG_NULLARGCHECK_RETURN( psa );
	SG_NULLARGCHECK_RETURN( ppsz );

	if (n >= psa->count)
	{
		SG_ERR_THROW_RETURN( SG_ERR_ARGUMENT_OUT_OF_RANGE );
	}

	*ppsz = psa->aStrings[n];

}

void SG_stringarray__count(
	SG_context* pCtx,
	const SG_stringarray* psa,
	SG_uint32 * pCount)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(psa);
    SG_NULLARGCHECK_RETURN(pCount);
    *pCount = psa->count;
}

void SG_stringarray__sz_array(
	SG_context* pCtx,
	const SG_stringarray* psa,
	const char * const ** pppStrings)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(psa);
    SG_NULLARGCHECK_RETURN(pppStrings);
	*pppStrings = psa->aStrings;
}

void SG_stringarray__sz_array_and_count(
	SG_context* pCtx,
	const SG_stringarray* psa,
	const char * const ** pppStrings,
	SG_uint32 * pCount)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(psa);
    SG_NULLARGCHECK_RETURN(pppStrings);
    SG_NULLARGCHECK_RETURN(pCount);
	*pppStrings = psa->aStrings;
    *pCount = psa->count;
}

void SG_stringarray__find(
	SG_context * pCtx,
	const SG_stringarray * psa,
	const char * pszToFind,
	SG_uint32 ndxStart,
	SG_bool * pbFound,
	SG_uint32 * pNdxFound)
{
	SG_uint32 k;

	SG_NULLARGCHECK_RETURN(psa);
	SG_NONEMPTYCHECK_RETURN(pszToFind);
	SG_NULLARGCHECK_RETURN(pbFound);
	// pNdxFound is optional

	for (k=ndxStart; k<psa->count; k++)
	{
		if (strcmp(psa->aStrings[k], pszToFind) == 0)
		{
			*pbFound = SG_TRUE;
			if (pNdxFound)
				*pNdxFound = k;
			return;
		}
	}

	*pbFound = SG_FALSE;
}
