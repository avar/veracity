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

/*
 *
 * @file sg_client_vtable__c.c
 *
 */

#include <sg.h>
#include "sg_client__api.h"

#include "sg_client_vtable__c.h"

struct _sg_client_c_push_handle
{
	char* pszPushId;
};
typedef struct _sg_client_c_push_handle sg_client_c_push_handle;

struct _sg_client_c_instance_data
{
	SG_server* pServer;  // In the c vtable, we can call the server directly, so we keep a pointer to it here.
};
typedef struct _sg_client_c_instance_data sg_client_c_instance_data;

static void _handle_free(SG_context * pCtx, sg_client_c_push_handle* pPush)
{
	if (pPush)
	{
		if ( (pPush)->pszPushId )
			SG_NULLFREE(pCtx, pPush->pszPushId);

		SG_NULLFREE(pCtx, pPush);
	}
}
#define _NULLFREE_PUSH_HANDLE(pCtx,p) SG_STATEMENT(  _handle_free(pCtx, p); p=NULL;  )

void sg_client__c__open(SG_context* pCtx,
						SG_client * pClient,
						const SG_vhash* pvh_credentials)
{
	sg_client_c_instance_data* pMe = NULL;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_ARGCHECK_RETURN(pvh_credentials || pvh_credentials == NULL_CREDENTIAL, pvh_credentials);

	SG_ERR_CHECK(  SG_alloc1(pCtx, pMe)  );
	SG_ERR_CHECK(  SG_server__alloc(pCtx, &pMe->pServer)  );

	pClient->p_vtable_instance_data = (sg_client__vtable__instance_data *)pMe;

	return;

fail:
	if (pMe)
	{
		SG_ERR_IGNORE(  SG_server__free(pCtx, pMe->pServer)  );
		SG_NULLFREE(pCtx, pMe);
	}
}

void sg_client__c__close(SG_context * pCtx, SG_client * pClient)
{
	sg_client_c_instance_data* pMe = NULL;

	if (!pClient)
		return;

	pMe = (sg_client_c_instance_data*)pClient->p_vtable_instance_data;
	SG_ERR_CHECK_RETURN(  SG_server__free(pCtx, pMe->pServer)  );
	SG_ERR_CHECK_RETURN(  SG_NULLFREE(pCtx, pMe)  );
}

void sg_client__c__list_repo_instances(SG_context* pCtx,
									   SG_client * pClient,
									   SG_vhash** ppResult)
{
	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(ppResult);

    /* TODO call SG_server__list_repo_instances */

    SG_ERR_THROW_RETURN(SG_ERR_NOTIMPLEMENTED);
}

void sg_client__c__push_begin(SG_context* pCtx,
							  SG_client * pClient,
							  SG_pathname** ppFragballDirPathname,
							  SG_client_push_handle** ppPush)
{
	sg_client_c_push_handle* pPush = NULL;
	sg_client_c_instance_data* pMe = NULL;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(ppFragballDirPathname);
	SG_NULLARGCHECK_RETURN(ppPush);

	pMe = (sg_client_c_instance_data*)pClient->p_vtable_instance_data;

	// Alloc a push handle.
	SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(sg_client_c_push_handle), &pPush)  );

	// Tell the server we're about to push.  We get back a push ID, which we save in the push handle.
	SG_ERR_CHECK(  SG_server__push_begin(pCtx, pMe->pServer, pClient->psz_remote_repo_spec, (const char **)&pPush->pszPushId)  );

	/* This is a local push, so we tell our caller to put fragballs directly in the server's staging area. */
	SG_ERR_CHECK(  SG_server__push_get_staging_path(pCtx, pMe->pServer, pPush->pszPushId, ppFragballDirPathname)  );

	// Return the new push handle.
	*ppPush = (SG_client_push_handle*)pPush;
	pPush = NULL;

	/* fall through */

fail:
	_NULLFREE_PUSH_HANDLE(pCtx, pPush);
}

void sg_client__c__push_add(SG_context* pCtx,
							SG_client * pClient,
							SG_client_push_handle* pPush,
							SG_pathname** ppPath_fragball,
							SG_vhash** ppResult)
{
	sg_client_c_instance_data* pMe = NULL;
	sg_client_c_push_handle* pMyPush = (sg_client_c_push_handle*)pPush;
	SG_string* pFragballName = NULL;
	SG_staging* pStaging = NULL;
	SG_vhash* pvhRepoDescriptor = NULL;
	SG_repo* pRepo = NULL;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(pPush);
	SG_NULLARGCHECK_RETURN(ppResult);

	pMe = (sg_client_c_instance_data*)pClient->p_vtable_instance_data;

	if (!ppPath_fragball || !*ppPath_fragball)
    {
		/* get the push's current status */
		SG_ERR_CHECK(  SG_staging__open(pCtx, pMyPush->pszPushId, &pStaging)  );

		SG_ERR_CHECK(  SG_staging__check_status(pCtx, pStaging, SG_TRUE, SG_TRUE, SG_TRUE, SG_TRUE, SG_TRUE, ppResult)  );
    }
    else
    {
		/* add the fragball to the push */

		// TODO: This is doing a lot more than we need here.
		SG_ERR_CHECK(  SG_pathname__get_last(pCtx, *ppPath_fragball, &pFragballName)  );

        /* Tell the server to add the fragball. */
		SG_ERR_CHECK(  SG_server__push_add(pCtx, pMe->pServer, pMyPush->pszPushId, SG_string__sz(pFragballName), ppResult) );

		SG_PATHNAME_NULLFREE(pCtx, *ppPath_fragball);
    }

	/* fall through */
fail:
	SG_STRING_NULLFREE(pCtx, pFragballName);
	SG_STAGING_NULLFREE(pCtx, pStaging);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_VHASH_NULLFREE(pCtx, pvhRepoDescriptor);
}

void sg_client__c__push_remove(SG_context* pCtx,
							   SG_client * pClient,
							   SG_client_push_handle* pPush,
							   SG_vhash* pvh_stuff_to_be_removed_from_staging_area,
							   SG_vhash** ppResult)
{
	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(pPush);
	SG_NULLARGCHECK_RETURN(pvh_stuff_to_be_removed_from_staging_area);
	SG_NULLARGCHECK_RETURN(ppResult);

	SG_ERR_THROW_RETURN(SG_ERR_NOTIMPLEMENTED);
}

void sg_client__c__push_commit(SG_context* pCtx,
							   SG_client * pClient,
							   SG_client_push_handle* pPush,
							   SG_vhash** ppResult)
{
	sg_client_c_push_handle* pMyPush = (sg_client_c_push_handle*)pPush;
	sg_client_c_instance_data* pMe = (sg_client_c_instance_data*)pClient->p_vtable_instance_data;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(pPush);
	SG_NULLARGCHECK_RETURN(ppResult);

	SG_ERR_CHECK_RETURN(  SG_server__push_commit(pCtx, pMe->pServer, pMyPush->pszPushId, ppResult)  );
}

void sg_client__c__push_end(SG_context * pCtx,
							SG_client * pClient,
							SG_client_push_handle** ppPush)
{
	sg_client_c_instance_data* pMe = NULL;
	sg_client_c_push_handle* pMyPush = NULL;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(ppPush);

	pMe = (sg_client_c_instance_data*)pClient->p_vtable_instance_data;
	pMyPush = (sg_client_c_push_handle*)*ppPush;

	SG_ERR_CHECK(  SG_server__push_end(pCtx, pMe->pServer, pMyPush->pszPushId)  );

	// fall through

fail:
	// We free the push handle even on failure, because SG_push_handle is opaque outside this file:
	// this is the only way to free it.
	_NULLFREE_PUSH_HANDLE(pCtx, pMyPush);
	*ppPush = NULL;
}


void sg_client__c__pull_request_fragball(SG_context* pCtx,
										 SG_client* pClient,
										 SG_vhash* pvhRequest,
										 const SG_pathname* pStagingPathname,
										 char** ppszFragballName,
										 SG_vhash** ppvhStatus)
{
	sg_client_c_instance_data* pMe = NULL;
	SG_repo* pRepo = NULL;
	char* pszFragballName = NULL;
	SG_vhash* pvhStatus = NULL;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(ppszFragballName);
	SG_NULLARGCHECK_RETURN(ppvhStatus);

	pMe = (sg_client_c_instance_data*)pClient->p_vtable_instance_data;

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, pClient->psz_remote_repo_spec, &pRepo)  );

	/* Tell the server to build its fragball in our staging directory.
	   We can do this just calling pServer directly because we know it's a local repo. */
	SG_ERR_CHECK(  SG_server__pull_request_fragball(pCtx, pRepo, pvhRequest, pStagingPathname, &pszFragballName, &pvhStatus) );

	*ppszFragballName = pszFragballName;
	pszFragballName = NULL;
	*ppvhStatus = pvhStatus;
	pvhStatus = NULL;

	/* fall through */
fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_VHASH_NULLFREE(pCtx, pvhStatus);
	SG_NULLFREE(pCtx, pszFragballName);
	SG_VHASH_NULLFREE(pCtx, pvhStatus);
}

void sg_client__c__pull_clone(
	SG_context* pCtx,
	SG_client* pClient,
	const SG_pathname* pStagingPathname,
	char** ppszFragballName)
{
	sg_client_c_instance_data* pMe = NULL;
	SG_repo* pRepo = NULL;
	SG_vhash* pvhStatus = NULL;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(ppszFragballName);

	pMe = (sg_client_c_instance_data*)pClient->p_vtable_instance_data;

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, pClient->psz_remote_repo_spec, &pRepo)  );

	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "Copying repository...")  );
	SG_ERR_CHECK(  SG_repo__fetch_repo__fragball(pCtx, pRepo, pStagingPathname, ppszFragballName) );
	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "done")  );

	/* fall through */
fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_VHASH_NULLFREE(pCtx, pvhStatus);
	SG_ERR_IGNORE(  SG_context__msg__emit(pCtx, "\n")  );
}

void sg_client__c__get_repo_info(SG_context* pCtx,
								 SG_client* pClient,
								 char** ppszRepoId,
								 char** ppszAdminId,
								 char** ppszHashMethod)
{
	sg_client_c_instance_data* pMe = NULL;
	SG_repo* pRepo = NULL;

	SG_NULLARGCHECK_RETURN(pClient);

	pMe = (sg_client_c_instance_data*)pClient->p_vtable_instance_data;

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, pClient->psz_remote_repo_spec, &pRepo)  );

	SG_ERR_CHECK(  SG_server__get_repo_info(pCtx, pRepo, ppszRepoId, ppszAdminId, ppszHashMethod)  );

	/* fall through */
fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
}

void sg_client__c__get_dagnode_info(
	SG_context* pCtx,
	SG_client* pClient,
	SG_vhash* pvhRequest,
	SG_varray** ppvaInfo)
{
	sg_client_c_instance_data* pMe = NULL;
	SG_repo* pRepo = NULL;

	SG_NULLARGCHECK_RETURN(pClient);

	pMe = (sg_client_c_instance_data*)pClient->p_vtable_instance_data;

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, pClient->psz_remote_repo_spec, &pRepo)  );
	SG_ERR_CHECK(  SG_server__get_dagnode_info(pCtx, pRepo, pvhRequest, ppvaInfo)  );

	/* fall through */
fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
}