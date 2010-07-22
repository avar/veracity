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
 * @file sg_vector_i64.c
 *
 * @details A growable vector to SG_int64.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

static const SG_uint32 MIN_CHUNK_SIZE = 10;

struct _SG_vector_i64
{
	SG_uint32		m_uiSizeInUse;		// currently used
	SG_uint32		m_uiAllocated;		// amount allocated
	SG_uint32		m_uiChunkSize;		// realloc delta
	SG_int64 *		m_array;			// an array of I64
};

//////////////////////////////////////////////////////////////////

static void _sg_vector_i64__grow(SG_context* pCtx, SG_vector_i64 * pVector, SG_uint32 additionalSpaceNeeded)
{
	SG_uint32 totalSpaceNeeded;
	SG_uint32 totalSpaceRounded;
	SG_int64 * pNewArray = NULL;

	SG_NULLARGCHECK_RETURN(pVector);

	totalSpaceNeeded = pVector->m_uiSizeInUse + additionalSpaceNeeded;
	if (totalSpaceNeeded <= pVector->m_uiAllocated)
		return;

	totalSpaceRounded = ((totalSpaceNeeded + pVector->m_uiChunkSize - 1) / pVector->m_uiChunkSize) * pVector->m_uiChunkSize;

	SG_ERR_CHECK_RETURN(  SG_alloc(pCtx, totalSpaceRounded,sizeof(SG_int64),&pNewArray)  );

	if (pVector->m_array)
	{
		if (pVector->m_uiSizeInUse > 0)
			memmove(pNewArray,pVector->m_array,pVector->m_uiSizeInUse*sizeof(SG_int64));
		SG_NULLFREE(pCtx, pVector->m_array);
	}

	pVector->m_array = pNewArray;
	pVector->m_uiAllocated = totalSpaceRounded;
}

void SG_vector_i64__reserve(SG_context* pCtx, SG_vector_i64 * pVector, SG_uint32 sizeNeeded)
{
	SG_NULLARGCHECK_RETURN(pVector);

	if (sizeNeeded > pVector->m_uiAllocated)
		SG_ERR_CHECK_RETURN(  _sg_vector_i64__grow(pCtx, pVector, (sizeNeeded - pVector->m_uiAllocated))  );

	if (sizeNeeded > pVector->m_uiSizeInUse)
	{
		SG_uint32 k;
		for (k=pVector->m_uiSizeInUse; k<sizeNeeded; k++)
			pVector->m_array[k] = 0;
		pVector->m_uiSizeInUse = sizeNeeded;
	}
}

//////////////////////////////////////////////////////////////////

void SG_vector_i64__alloc(SG_context* pCtx, SG_vector_i64 ** ppVector, SG_uint32 suggestedInitialSize)
{
	SG_vector_i64 * pVector = NULL;

	SG_NULLARGCHECK_RETURN(ppVector);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pVector)  );

	pVector->m_uiSizeInUse = 0;
	pVector->m_uiAllocated = 0;
	pVector->m_uiChunkSize = MIN_CHUNK_SIZE;
	pVector->m_array = NULL;

	SG_ERR_CHECK(  _sg_vector_i64__grow(pCtx, pVector,suggestedInitialSize)  );

	*ppVector = pVector;

	return;

fail:
	SG_VECTOR_I64_NULLFREE(pCtx, pVector);
}

void SG_vector_i64__alloc__from_varray(SG_context* pCtx, SG_varray* pva, SG_vector_i64 ** ppVector)
{
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_vector_i64* pvec = NULL;

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    SG_ERR_CHECK(  SG_vector_i64__alloc(pCtx, &pvec, count)  );
    for (i=0; i<count; i++)
    {
        SG_int64 v = 0;

        SG_ERR_CHECK(  SG_varray__get__int64(pCtx, pva, i, &v)  );
        SG_ERR_CHECK(  SG_vector_i64__append(pCtx, pvec, v, NULL)  );
    }

    *ppVector = pvec;
    pvec = NULL;

    return;

fail:
    SG_VECTOR_I64_NULLFREE(pCtx, pvec);

    return;
}

void SG_vector_i64__free(SG_context * pCtx, SG_vector_i64 * pVector)
{
	if (!pVector)
		return;

	SG_NULLFREE(pCtx, pVector->m_array);

	pVector->m_uiSizeInUse = 0;
	pVector->m_uiAllocated = 0;
	SG_NULLFREE(pCtx, pVector);
}

//////////////////////////////////////////////////////////////////

void SG_vector_i64__append(SG_context* pCtx, SG_vector_i64 * pVector, SG_int64 value, SG_uint32 * pIndexReturned)
{
	SG_uint32 k;

	SG_NULLARGCHECK_RETURN(pVector);

	// insert value at array[size_in_use]

	if (pVector->m_uiSizeInUse >= pVector->m_uiAllocated)
	{
		SG_ERR_CHECK_RETURN(  _sg_vector_i64__grow(pCtx, pVector,1)  );
		SG_ASSERT( (pVector->m_uiSizeInUse < pVector->m_uiAllocated)  &&  "vector_i64 grow failed"  );
	}

	k = pVector->m_uiSizeInUse++;

	pVector->m_array[k] = value;
	if (pIndexReturned)
		*pIndexReturned = k;
}

//////////////////////////////////////////////////////////////////

void SG_vector_i64__get(SG_context* pCtx, const SG_vector_i64 * pVector, SG_uint32 k, SG_int64 * pValue)
{
	SG_NULLARGCHECK_RETURN(pVector);
	SG_NULLARGCHECK_RETURN(pValue);

	SG_ARGCHECK_RETURN(k < pVector->m_uiSizeInUse, pVector->m_uiSizeInUse);

	*pValue = pVector->m_array[k];
}

void SG_vector_i64__set(SG_context* pCtx, SG_vector_i64 * pVector, SG_uint32 k, SG_int64 value)
{
	SG_NULLARGCHECK_RETURN(pVector);

	SG_ARGCHECK_RETURN(k < pVector->m_uiSizeInUse, pVector->m_uiSizeInUse);

	pVector->m_array[k] = value;
}

//////////////////////////////////////////////////////////////////

void SG_vector_i64__clear(SG_context* pCtx, SG_vector_i64 * pVector)
{
	SG_NULLARGCHECK_RETURN(pVector);

	pVector->m_uiSizeInUse = 0;

	if (pVector->m_array && (pVector->m_uiAllocated > 0))
		memset(pVector->m_array,0,(pVector->m_uiAllocated * sizeof(SG_int64)));
}

//////////////////////////////////////////////////////////////////

void SG_vector_i64__length(SG_context* pCtx, const SG_vector_i64 * pVector, SG_uint32 * pLength)
{
	SG_NULLARGCHECK_RETURN(pVector);
	SG_NULLARGCHECK_RETURN(pLength);

	*pLength = pVector->m_uiSizeInUse;
}

//////////////////////////////////////////////////////////////////

void SG_vector_i64__setChunkSize(SG_context* pCtx, SG_vector_i64 * pVector, SG_uint32 size)
{
	SG_NULLARGCHECK_RETURN(pVector);

	if (size < MIN_CHUNK_SIZE)
		size = MIN_CHUNK_SIZE;

	pVector->m_uiChunkSize = size;
}

//////////////////////////////////////////////////////////////////
