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
#include "sg_client__api.h"

/* This file is basically just functions that call
 * through the vtable.  Nothing here does any actual
 * work. */

//////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////

void SG_client__open(SG_context* pCtx,
					 const char* psz_remote_repo_spec,
					 const SG_vhash* pvh_credentials,
					 SG_client** ppNew)
{
	SG_client* pClient = NULL;

	SG_NULLARGCHECK(psz_remote_repo_spec);
	SG_NULLARGCHECK(ppNew);

	SG_alloc1(pCtx,pClient);

	SG_ERR_CHECK(  SG_strdup(pCtx, psz_remote_repo_spec, &pClient->psz_remote_repo_spec)  );

	SG_ERR_CHECK(  sg_client__bind_vtable(pCtx, pClient)  );
	SG_ERR_CHECK(  pClient->p_vtable->open(pCtx, pClient, pvh_credentials)  );

    SG_RETURN_AND_NULL(pClient, ppNew);

	return;

fail:
	SG_CLIENT_NULLFREE(pCtx, pClient);
}

#define VERIFY_VTABLE(pClient)												\
	SG_STATEMENT(	SG_NULLARGCHECK_RETURN(pClient);						\
					if (!(pClient)->p_vtable)								\
						SG_ERR_THROW_RETURN(SG_ERR_VTABLE_NOT_BOUND);	\
				)

void SG_client__close_free(SG_context * pCtx,
					  SG_client * pClient)
{
	if (!pClient)
		return;

	VERIFY_VTABLE(pClient);

	SG_ERR_CHECK_RETURN(  pClient->p_vtable->close(pCtx, pClient)  );

	sg_client__unbind_vtable(pClient);

	SG_NULLFREE(pCtx, pClient->psz_remote_repo_spec);
	SG_NULLFREE(pCtx, pClient);
}

void SG_client__push_begin(
		SG_context* pCtx,
        SG_client * pClient,
		SG_pathname** pFragballDirPathname,
        SG_client_push_handle** ppPush
        )
{
    VERIFY_VTABLE(pClient);

    SG_ERR_CHECK_RETURN(  pClient->p_vtable->push_begin(pCtx, pClient, pFragballDirPathname, ppPush)  );
}

void SG_client__push_add(
		SG_context* pCtx,
        SG_client * pClient,
        SG_client_push_handle* pPush,
        SG_pathname** ppPath_fragball,
        SG_vhash** ppResult
        )
{
    VERIFY_VTABLE(pClient);

    SG_ERR_CHECK_RETURN(  pClient->p_vtable->push_add(pCtx, pClient, pPush, ppPath_fragball, ppResult)  );
}

void SG_client__push_remove(
		SG_context* pCtx,
		SG_client * pClient,
        SG_client_push_handle* pPush,
        SG_vhash* pvh_stuff_to_be_removed_from_staging_area,
        SG_vhash** ppResult
        )
{
    VERIFY_VTABLE(pClient);

    SG_ERR_CHECK_RETURN(  pClient->p_vtable->push_remove(pCtx, pClient, pPush, pvh_stuff_to_be_removed_from_staging_area, ppResult)  );
}

void SG_client__push_commit(
		SG_context* pCtx,
		SG_client * pClient,
        SG_client_push_handle* pPush,
        SG_vhash** ppResult
        )
{
    VERIFY_VTABLE(pClient);

    SG_ERR_CHECK_RETURN(  pClient->p_vtable->push_commit(pCtx, pClient, pPush, ppResult)  );
}

void SG_client__push_end(
		SG_context* pCtx,
		SG_client * pClient,
        SG_client_push_handle** ppPush
        )
{
    VERIFY_VTABLE(pClient);

    SG_ERR_CHECK_RETURN(  pClient->p_vtable->push_end(pCtx, pClient, ppPush)  );
}

void SG_client__pull_request_fragball(SG_context* pCtx,
									  SG_client* pClient,
									  SG_vhash* pvhRequest,
									  const SG_pathname* pStagingPathname,
									  char** ppszFragballName,
									  SG_vhash** ppvhStatus)
{
    VERIFY_VTABLE(pClient);

    SG_ERR_CHECK_RETURN(  pClient->p_vtable->pull_request_fragball(pCtx, pClient, pvhRequest, pStagingPathname, ppszFragballName, ppvhStatus)  );
}

void SG_client__pull_clone(
	SG_context* pCtx,
	SG_client* pClient,
	const SG_pathname* pStagingPathname,
	char** ppszFragballName)
{
	VERIFY_VTABLE(pClient);

	SG_ERR_CHECK_RETURN(  pClient->p_vtable->pull_clone(pCtx, pClient, pStagingPathname, ppszFragballName)  );
}

void SG_client__get_repo_info(SG_context* pCtx,
							  SG_client* pClient,
							  char** ppszRepoId,
							  char** ppszAdminId,
							  char** ppszHashMethod)
{
	VERIFY_VTABLE(pClient);

	SG_ERR_CHECK_RETURN(  pClient->p_vtable->get_repo_info(pCtx, pClient, ppszRepoId, ppszAdminId, ppszHashMethod)  );
}

void SG_client__get_dagnode_info(
	SG_context* pCtx,
	SG_client* pClient,
	SG_vhash* pvhRequest,
	SG_varray** ppvaInfo)
{
	VERIFY_VTABLE(pClient);

	SG_ERR_CHECK_RETURN(  pClient->p_vtable->get_dagnode_info(pCtx, pClient, pvhRequest, ppvaInfo)  );
}