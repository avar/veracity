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

#include "sg_js_safeptr__private.h"

struct _SG_safeptr
{
    const char* psz_type;
    void* ptr;
};


//////////////////////////////////////////////////////////////////

#define MY_WRAP_RETURN(pCtx, pPtr, safe_type, ppNewSafePtr)       \
	SG_STATEMENT(                                                 \
		SG_safeptr * psp = NULL;                                  \
		SG_NULLARGCHECK_RETURN(  (pPtr)  );                       \
		SG_NULLARGCHECK_RETURN(  (ppNewSafePtr)  );               \
		SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, psp)  );            \
		psp->psz_type = (safe_type);                              \
		psp->ptr = (pPtr);                                        \
		*(ppNewSafePtr) = psp;                                    )

// Review: Jeff says: do we want to keep SG_ERR_SAFEPTR_NULL as the
//                    return value for a null argument or do we want
//                    use the standard SG_ERR_INVALIDARG ?

#define MY_UNWRAP_RETURN(pCtx, psp, safe_type, ppResult, c_type) \
	SG_STATEMENT(                                                \
		if (!(psp))                                              \
			SG_ERR_THROW_RETURN( SG_ERR_SAFEPTR_NULL );          \
		if (0 != strcmp((psp)->psz_type,(safe_type)))            \
			SG_ERR_THROW_RETURN( SG_ERR_SAFEPTR_WRONG_TYPE );    \
		SG_NULLARGCHECK_RETURN(  (ppResult)  );                  \
		*(ppResult) = (c_type)((psp)->ptr);                      )

//////////////////////////////////////////////////////////////////

void SG_safeptr__wrap__repo(SG_context* pCtx, SG_repo* p, SG_safeptr** ppsp)
{
	MY_WRAP_RETURN(pCtx, p, SG_SAFEPTR_TYPE__REPO, ppsp);
}

void SG_safeptr__unwrap__repo(SG_context* pCtx, SG_safeptr* psp, SG_repo** pp)
{
	MY_UNWRAP_RETURN(pCtx, psp, SG_SAFEPTR_TYPE__REPO, pp, SG_repo *);
}

void SG_safeptr__wrap__zingdb(SG_context* pCtx, sg_zingdb* p, SG_safeptr** ppsp)
{
	MY_WRAP_RETURN(pCtx, p, SG_SAFEPTR_TYPE__ZINGSTATE, ppsp);
}

void SG_safeptr__unwrap__zingdb(SG_context* pCtx, SG_safeptr* psp, sg_zingdb** pp)
{
	MY_UNWRAP_RETURN(pCtx, psp, SG_SAFEPTR_TYPE__ZINGSTATE, pp, sg_zingdb *);
}

void SG_safeptr__wrap__zingrecord(SG_context* pCtx, SG_zingrecord* p, SG_safeptr** ppsp)
{
	MY_WRAP_RETURN(pCtx, p, SG_SAFEPTR_TYPE__ZINGRECORD, ppsp);
}

void SG_safeptr__unwrap__zingrecord(SG_context* pCtx, SG_safeptr* psp, SG_zingrecord** pp)
{
	MY_UNWRAP_RETURN(pCtx, psp, SG_SAFEPTR_TYPE__ZINGRECORD, pp, SG_zingrecord *);
}

void SG_safeptr__wrap__zinglinkcollection(SG_context* pCtx, sg_zinglinkcollection* p, SG_safeptr** ppsp)
{
	MY_WRAP_RETURN(pCtx, p, SG_SAFEPTR_TYPE__ZINGLINKCOLLECTION, ppsp);
}

void SG_safeptr__unwrap__zinglinkcollection(SG_context* pCtx, SG_safeptr* psp, sg_zinglinkcollection** pp)
{
	MY_UNWRAP_RETURN(pCtx, psp, SG_SAFEPTR_TYPE__ZINGLINKCOLLECTION, pp, sg_zinglinkcollection *);
}

void SG_safeptr__wrap__statetxcombo(SG_context* pCtx, sg_statetxcombo* p, SG_safeptr** ppsp)
{
	MY_WRAP_RETURN(pCtx, p, SG_SAFEPTR_TYPE__STATETXCOMBO, ppsp);
}

void SG_safeptr__unwrap__statetxcombo(SG_context* pCtx, SG_safeptr* psp, sg_statetxcombo** pp)
{
	MY_UNWRAP_RETURN(pCtx, psp, SG_SAFEPTR_TYPE__STATETXCOMBO, pp, sg_statetxcombo *);
}

void SG_safeptr__free(SG_context * pCtx, SG_safeptr* p)
{
    if (!p)
        return;

    SG_NULLFREE(pCtx, p);
}

