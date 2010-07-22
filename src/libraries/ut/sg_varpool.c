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

typedef struct _sg_variantsubpool
{
	SG_uint32 count;
	SG_uint32 space;
	SG_variant* pVariants;
	struct _sg_variantsubpool* pNext;
} sg_variantsubpool;

struct _SG_varpool
{
	SG_uint32 subpool_space;
	SG_uint32 count_subpools;
	SG_uint32 count_variants;
	sg_variantsubpool *pHead;
};

void sg_variantsubpool__free(SG_context * pCtx, sg_variantsubpool *psp);

void sg_variantsubpool__alloc(SG_context* pCtx, SG_uint32 space, sg_variantsubpool* pNext,
								  sg_variantsubpool ** ppNew)
{
	sg_variantsubpool* pThis = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pThis)  );

	pThis->space = space;
	SG_ERR_CHECK(  SG_alloc(pCtx, pThis->space,sizeof(SG_variant),&pThis->pVariants)  );

	pThis->pNext = pNext;
	pThis->count = 0;

	*ppNew = pThis;

	return;

fail:
	SG_ERR_IGNORE(  sg_variantsubpool__free(pCtx, pThis)  );
}

void sg_variantsubpool__free(SG_context * pCtx, sg_variantsubpool *psp)
{
    sg_variantsubpool* p = psp;

    while (p)
    {
        sg_variantsubpool* pnext = p->pNext;

        SG_NULLFREE(pCtx, p->pVariants);

        SG_NULLFREE(pCtx, p);

        p = pnext;
    }
}

void SG_varpool__alloc(SG_context* pCtx, SG_varpool** ppResult, SG_uint32 subpool_space)
{
	SG_varpool* pThis = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pThis)  );

	pThis->subpool_space = subpool_space;

	SG_ERR_CHECK(  sg_variantsubpool__alloc(pCtx, pThis->subpool_space, NULL, &pThis->pHead)  );

	pThis->count_subpools = 1;

	*ppResult = pThis;

	return;

fail:
	SG_ERR_IGNORE(  SG_varpool__free(pCtx, pThis)  );
}

void SG_varpool__free(SG_context * pCtx, SG_varpool* pPool)
{
	if (!pPool)
	{
		return;
	}

	sg_variantsubpool__free(pCtx, pPool->pHead);
	SG_NULLFREE(pCtx, pPool);
}

void SG_varpool__add(SG_context* pCtx, SG_varpool* pPool, SG_variant** ppResult)
{
	if ((pPool->pHead->count + 1) > pPool->pHead->space)
	{
		sg_variantsubpool* psp;

		SG_ERR_CHECK_RETURN(  sg_variantsubpool__alloc(pCtx, pPool->subpool_space, pPool->pHead, & psp)  );

		pPool->pHead = psp;
		pPool->count_subpools++;
	}

	*ppResult = &(pPool->pHead->pVariants[pPool->pHead->count++]);
	pPool->count_variants++;
}

