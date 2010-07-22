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
 * @file sg_exec_argvec.c
 *
 * @details Wrappers for argument vector used by sg_exec.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

void SG_exec_argvec__alloc(SG_context* pCtx, SG_exec_argvec ** ppArgVec)
{
	SG_ERR_CHECK_RETURN(  SG_vector__alloc(pCtx, (SG_vector **)ppArgVec, 10)  );
}

void SG_exec_argvec__free(SG_context * pCtx, SG_exec_argvec * pArgVec)
{
	SG_vector__free__with_assoc(pCtx, (SG_vector *)pArgVec,(SG_free_callback *)SG_string__free);
}

//////////////////////////////////////////////////////////////////

void SG_exec_argvec__append__sz(SG_context* pCtx, SG_exec_argvec * pArgVec, const char * sz)
{
	SG_string * pStr = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pStr,sz)  );
	SG_ERR_CHECK(  SG_vector__append(pCtx,(SG_vector *)pArgVec,pStr,NULL)  );

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pStr);
}
void SG_exec_argvec__remove_last(SG_context* pCtx, SG_exec_argvec * pArgVec)
{
    SG_string * pStr = NULL;
    SG_ERR_CHECK_RETURN(  SG_vector__pop_back(pCtx, (SG_vector *)pArgVec, (void**)&pStr)  );
    SG_STRING_NULLFREE(pCtx, pStr);
}

void SG_exec_argvec__length(SG_context* pCtx, const SG_exec_argvec * pArgVec, SG_uint32 * pLength)
{
	SG_ERR_CHECK_RETURN(  SG_vector__length(pCtx,(const SG_vector *)pArgVec,pLength)  );
}

void SG_exec_argvec__get(SG_context* pCtx, const SG_exec_argvec * pArgVec, SG_uint32 k, const SG_string ** ppStr)
{
	SG_ERR_CHECK_RETURN(  SG_vector__get(pCtx, (const SG_vector *)pArgVec,k,(void **)ppStr)  );
}
