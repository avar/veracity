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
 * @file sg_vector.c
 *
 * @details A growable vector to store pointers.  This is a peer of RBTREE
 * that lets us store an array of pointers.  These are like VHASH and VARRAY.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

static const SG_uint32 MIN_CHUNK_SIZE = 10;

struct _SG_vector
{
	SG_uint32		m_uiSizeInUse;		// currently used
	SG_uint32		m_uiAllocated;		// amount allocated
	SG_uint32		m_uiChunkSize;		// realloc delta
	void **			m_array;			// an array of pointers
};

//////////////////////////////////////////////////////////////////

static void _sg_vector__grow(SG_context* pCtx, SG_vector * pVector, SG_uint32 additionalSpaceNeeded)
{
	SG_uint32 totalSpaceNeeded;
	SG_uint32 totalSpaceRounded;
	void ** pNewArray = NULL;

	SG_NULLARGCHECK_RETURN(pVector);

	totalSpaceNeeded = pVector->m_uiSizeInUse + additionalSpaceNeeded;
	if (totalSpaceNeeded <= pVector->m_uiAllocated)
		return;

	totalSpaceRounded = ((totalSpaceNeeded + pVector->m_uiChunkSize - 1) / pVector->m_uiChunkSize) * pVector->m_uiChunkSize;

	SG_ERR_CHECK_RETURN(  SG_alloc(pCtx, totalSpaceRounded,sizeof(void *),&pNewArray)  );

	if (pVector->m_array)
	{
		if (pVector->m_uiSizeInUse > 0)
			memmove(pNewArray,pVector->m_array,pVector->m_uiSizeInUse*sizeof(void *));
		SG_NULLFREE(pCtx, pVector->m_array);
	}

	pVector->m_array = pNewArray;
	pVector->m_uiAllocated = totalSpaceRounded;
}

void SG_vector__reserve(SG_context* pCtx, SG_vector * pVector, SG_uint32 sizeNeeded)
{
	SG_NULLARGCHECK_RETURN(pVector);

	if (sizeNeeded > pVector->m_uiAllocated)
		SG_ERR_CHECK_RETURN(  _sg_vector__grow(pCtx, pVector, (sizeNeeded - pVector->m_uiAllocated))  );

	if (sizeNeeded > pVector->m_uiSizeInUse)
	{
		SG_uint32 k;
		for (k=pVector->m_uiSizeInUse; k<sizeNeeded; k++)
			pVector->m_array[k] = 0;
		pVector->m_uiSizeInUse = sizeNeeded;
	}
}

//////////////////////////////////////////////////////////////////

void SG_vector__alloc(SG_context* pCtx, SG_vector ** ppVector, SG_uint32 suggestedInitialSize)
{
	SG_vector * pVector = NULL;

	SG_NULLARGCHECK_RETURN(ppVector);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pVector)  );

	pVector->m_uiSizeInUse = 0;
	pVector->m_uiAllocated = 0;
	pVector->m_uiChunkSize = MIN_CHUNK_SIZE;
	pVector->m_array = NULL;

	SG_ERR_CHECK(  _sg_vector__grow(pCtx, pVector,suggestedInitialSize)  );

	*ppVector = pVector;

	return;

fail:
	SG_VECTOR_NULLFREE(pCtx, pVector);
}

void SG_vector__free(SG_context * pCtx, SG_vector * pVector)
{
	if (!pVector)
		return;

	// since we don't know what the array of pointers are, we do not free them.
	// the caller is responsible for this.

	SG_NULLFREE(pCtx, pVector->m_array);

	pVector->m_uiSizeInUse = 0;
	pVector->m_uiAllocated = 0;
	SG_NULLFREE(pCtx, pVector);
}

void SG_vector__free__with_assoc(SG_context * pCtx, SG_vector * pVector, SG_free_callback * pcbFreeValue)
{
	SG_uint32 k, kLimit;

	if (!pVector)
		return;

	if (pVector->m_array)
	{
		kLimit = pVector->m_uiSizeInUse;
		for (k=0; k<kLimit; k++)
		{
			void * p = pVector->m_array[k];
			if (p)
			{
				(*pcbFreeValue)(pCtx, p);
				pVector->m_array[k] = NULL;
			}
		}
	}

	SG_vector__free(pCtx, pVector);
}

//////////////////////////////////////////////////////////////////

void SG_vector__append(SG_context* pCtx, SG_vector * pVector, void * pValue, SG_uint32 * pIndexReturned)
{
	SG_uint32 k;

	SG_NULLARGCHECK_RETURN(pVector);

	// insert value at array[size_in_use]

	if (pVector->m_uiSizeInUse >= pVector->m_uiAllocated)
	{
		SG_ERR_CHECK_RETURN(  _sg_vector__grow(pCtx, pVector,1)  );
		SG_ASSERT( (pVector->m_uiSizeInUse < pVector->m_uiAllocated)  &&  "vector grow failed"  );
	}

	k = pVector->m_uiSizeInUse++;

	pVector->m_array[k] = pValue;
	if (pIndexReturned)
		*pIndexReturned = k;
}

void SG_vector__pop_back(SG_context * pCtx, SG_vector * pVector, void ** ppValue)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pVector);

    if(ppValue!=NULL)
        *ppValue = pVector->m_array[pVector->m_uiSizeInUse-1];

    --pVector->m_uiSizeInUse;
}

//////////////////////////////////////////////////////////////////

void SG_vector__get(SG_context* pCtx, const SG_vector * pVector, SG_uint32 k, void ** ppValue)
{
	SG_NULLARGCHECK_RETURN(pVector);
	SG_NULLARGCHECK_RETURN(ppValue);

	SG_ARGCHECK_RETURN(k < pVector->m_uiSizeInUse, pVector->m_uiSizeInUse);

	*ppValue = pVector->m_array[k];
}

void SG_vector__set(SG_context* pCtx, SG_vector * pVector, SG_uint32 k, void * pValue)
{
	SG_NULLARGCHECK_RETURN(pVector);

	SG_ARGCHECK_RETURN(k < pVector->m_uiSizeInUse, pVector->m_uiSizeInUse);

	pVector->m_array[k] = pValue;
}

//////////////////////////////////////////////////////////////////

void SG_vector__clear(SG_context* pCtx, SG_vector * pVector)
{
	SG_NULLARGCHECK_RETURN(pVector);

	pVector->m_uiSizeInUse = 0;

	if (pVector->m_array && (pVector->m_uiAllocated > 0))
		memset(pVector->m_array,0,(pVector->m_uiAllocated * sizeof(void *)));
}

//////////////////////////////////////////////////////////////////

void SG_vector__length(SG_context* pCtx, const SG_vector * pVector, SG_uint32 * pLength)
{
	SG_NULLARGCHECK_RETURN(pVector);
	SG_NULLARGCHECK_RETURN(pLength);

	*pLength = pVector->m_uiSizeInUse;
}

//////////////////////////////////////////////////////////////////

void SG_vector__setChunkSize(SG_context* pCtx, SG_vector * pVector, SG_uint32 size)
{
	SG_NULLARGCHECK_RETURN(pVector);

	if (size < MIN_CHUNK_SIZE)
		size = MIN_CHUNK_SIZE;

	pVector->m_uiChunkSize = size;
}

//////////////////////////////////////////////////////////////////

void SG_vector__foreach(SG_context * pCtx,
						SG_vector * pVector,
						SG_vector_foreach_callback * pfn,
						void * pVoidCallerData)
{
	SG_uint32 k, kLimit;

	if (!pVector)
		return;

	if (!pVector->m_array)
		return;

	kLimit = pVector->m_uiSizeInUse;
	for (k=0; k<kLimit; k++)
	{
		void * p = pVector->m_array[k];			// this may be null if you haven't put anything in this cell

		SG_ERR_CHECK_RETURN(  (*pfn)(pCtx,k,p,pVoidCallerData)  );
	}
}
