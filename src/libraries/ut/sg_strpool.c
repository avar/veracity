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

typedef struct _sg_strsubpool
{
	SG_uint32 count;
	SG_uint32 space;
	char* pBytes;
	struct _sg_strsubpool* pNext;
} sg_strsubpool;

struct _SG_strpool
{
	SG_uint32 subpool_space;
	SG_uint32 count_bytes;
	SG_uint32 count_subpools;
	SG_uint32 count_strings;
	sg_strsubpool *pHead;
};

void sg_strsubpool__free(SG_context * pCtx, sg_strsubpool *psp);

void sg_strsubpool__alloc(
        SG_context* pCtx,
        SG_uint32 space,
        sg_strsubpool* pNext,
        sg_strsubpool** ppNew
        )
{
	sg_strsubpool* pThis = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pThis)  );

	pThis->space = space;

	SG_ERR_CHECK(  SG_allocN(pCtx, space,pThis->pBytes)  );

	pThis->pNext = pNext;
	pThis->count = 0;

	*ppNew = pThis;
	return;

fail:
	SG_ERR_IGNORE(  sg_strsubpool__free(pCtx, pThis)  );
}

void sg_strsubpool__free(SG_context * pCtx, sg_strsubpool *psp)
{
	while (psp)
	{
		sg_strsubpool * pspNext = psp->pNext;

		SG_NULLFREE(pCtx, psp->pBytes);
		SG_NULLFREE(pCtx, psp);

		psp = pspNext;
	}
}

void SG_strpool__alloc(SG_context* pCtx, SG_strpool** ppResult, SG_uint32 subpool_space)
{
	SG_strpool* pThis = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pThis)  );

	SG_ERR_CHECK(  sg_strsubpool__alloc(pCtx, subpool_space,NULL,&pThis->pHead)  );

	pThis->subpool_space = subpool_space;
	pThis->count_subpools = 1;

	*ppResult = pThis;

	return;

fail:
	SG_STRPOOL_NULLFREE(pCtx, pThis);
}

void            SG_strpool__free(SG_context * pCtx, SG_strpool* pPool)
{
	if (!pPool)
	{
		return;
	}

	sg_strsubpool__free(pCtx, pPool->pHead);
	SG_NULLFREE(pCtx, pPool);
}

static void sg_strpool__get_space(
        SG_context* pCtx,
        SG_strpool* pPool,
        SG_uint32 len,
        char** ppOut
        )
{
	char* pResult = NULL;
    SG_uint32 mod = 0;

    // TODO review this constant.  for now, I'm just aiming for 4-byte ptr
    // alignment

#define MY_PTR_ALIGN 4

    mod = len % MY_PTR_ALIGN;
    if (mod)
    {
        len += (MY_PTR_ALIGN - mod);
    }

	if ((pPool->pHead->count + len) > pPool->pHead->space)
	{
		SG_uint32 space = pPool->subpool_space;
		sg_strsubpool* psp = NULL;

		if (len > space)
		{
			space = len;
		}

		SG_ERR_CHECK_RETURN(  sg_strsubpool__alloc(pCtx, space, pPool->pHead, &psp)  );

		pPool->pHead = psp;
		pPool->count_subpools++;
	}

	pResult = pPool->pHead->pBytes + pPool->pHead->count;
	pPool->pHead->count += len;
	pPool->count_bytes += len;
	pPool->count_strings++;

	*ppOut = pResult;
}

void SG_strpool__add__buflen__binary(
        SG_context* pCtx,
        SG_strpool* pPool,
        const char* pIn,
        SG_uint32 len,
        const char** ppOut
        )
{
	char* pResult = NULL;

    SG_ERR_CHECK_RETURN(  sg_strpool__get_space(pCtx, pPool, len, &pResult)  );

	if (pIn)
	{
		memcpy(pResult, pIn, len);
	}

	*ppOut = pResult;
}

void SG_strpool__add__len(
        SG_context* pCtx,
        SG_strpool* pPool,
        SG_uint32 len,
        const char** ppOut
        )
{
	SG_ERR_CHECK_RETURN(  SG_strpool__add__buflen__binary(pCtx, pPool, NULL, len, ppOut)  );
}

void SG_strpool__add__buflen__sz(
        SG_context* pCtx,
        SG_strpool* pPool,
        const char* pszIn,
        SG_uint32 len_spec,
        const char** ppszOut
        )
{
	char* pResult = NULL;
	SG_uint32 len;

	if (len_spec == 0)
	{
		len = (SG_uint32) strlen(pszIn);
	}
	else
	{
		const char* p = pszIn;
		len = 0;
		while (*p && (len < len_spec))
		{
			p++;
			len++;
		}
	}

    SG_ERR_CHECK_RETURN(  sg_strpool__get_space(pCtx, pPool, len+1, &pResult)  );

	SG_ERR_IGNORE(  SG_strcpy(pCtx, pResult, len+1, pszIn)  );

	*ppszOut = pResult;
}

void SG_strpool__add__sz(
        SG_context* pCtx,
        SG_strpool* pPool,
        const char* pszIn,
        const char** ppszOut
        )
{
	SG_ERR_CHECK_RETURN(  SG_strpool__add__buflen__binary(pCtx, pPool, pszIn, (SG_uint32) (1 + strlen(pszIn)), ppszOut)  );
}

