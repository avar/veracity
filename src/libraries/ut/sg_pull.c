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

#define GENERATIONS_PER_ROUNDTRIP 100

#define TRACE_PULL 0

/* This exists primarily as a convenience so policy/credential data can be added to it later. 
 * It's only used in this file, there's no opaque wrapper. */
typedef struct
{
	char* pszPullId;
	SG_staging* pStaging;
	SG_repo* pPullIntoRepo;
} sg_pull_instance_data;


/* This is the structure for which SG_pull is an opaque wrapper. */
typedef struct
{
	SG_client* pClient;
	SG_vhash* pvhFragballRequest;
	sg_pull_instance_data* pPullInstance;
} _sg_pull;

static void _free_instance_data(SG_context* pCtx,
								sg_pull_instance_data* pMe)
{
	if (pMe)
	{
		SG_NULLFREE(pCtx, pMe->pszPullId);
		SG_STAGING_NULLFREE(pCtx, pMe->pStaging);
		SG_REPO_NULLFREE(pCtx, pMe->pPullIntoRepo);
		SG_NULLFREE(pCtx, pMe);
	}
}
#define _NULLFREE_INSTANCE_DATA(pCtx,p) SG_STATEMENT(  _free_instance_data(pCtx, p); p=NULL;  )

static void _sg_pull__nullfree(SG_context* pCtx, _sg_pull** ppMyPull)
{
	if (ppMyPull && *ppMyPull)
	{
		_sg_pull* pMyPull = *ppMyPull;
		SG_VHASH_NULLFREE(pCtx, pMyPull->pvhFragballRequest);
		SG_ERR_IGNORE(  _free_instance_data(pCtx, pMyPull->pPullInstance)  );
		SG_NULLFREE(pCtx, pMyPull);
		*ppMyPull = NULL;
	}
}

/**
 * Creates a staging area and returns an initialized instance data structure.
 */
static void _pull_init(SG_context* pCtx, 
					   SG_client* pClient,
					   const char* pszPullIntoRepoDescriptorName,
					   sg_pull_instance_data** ppMe)
{
	char* pszThisRepoId = NULL;
	char* pszThisHashMethod = NULL;
	char* pszOtherRepoId = NULL;
	char* pszOtherHashMethod = NULL;

	sg_pull_instance_data* pMe = NULL;
	
	SG_repo* pPullIntoRepo = NULL;

	SG_NULLARGCHECK_RETURN(pszPullIntoRepoDescriptorName);
	SG_NULLARGCHECK_RETURN(ppMe);

	SG_ERR_CHECK(  SG_client__get_repo_info(pCtx, pClient, &pszOtherRepoId, NULL, &pszOtherHashMethod)  );

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, pszPullIntoRepoDescriptorName, &pPullIntoRepo)  );

	/* TODO This will care about dagnums once we're using the user dag. */
	SG_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pPullIntoRepo, &pszThisRepoId)  );
	if (strcmp(pszThisRepoId, pszOtherRepoId) != 0)
		SG_ERR_THROW(SG_ERR_REPO_ID_MISMATCH);

	/* TODO check admin id when appropriate */

	SG_ERR_CHECK(  SG_repo__get_hash_method(pCtx, pPullIntoRepo, &pszThisHashMethod)  );
	if (strcmp(pszThisHashMethod, pszOtherHashMethod) != 0)
		SG_ERR_THROW(SG_ERR_REPO_HASH_METHOD_MISMATCH);

	// alloc instance data, store pull id in it (which identifies the staging area)
	SG_ERR_CHECK(  SG_alloc1(pCtx, pMe)  );
	SG_ERR_CHECK(  SG_staging__create(pCtx, pszPullIntoRepoDescriptorName, &pMe->pszPullId, &pMe->pStaging)  );
	pMe->pPullIntoRepo = pPullIntoRepo;
	pPullIntoRepo = NULL;

	SG_RETURN_AND_NULL(pMe, ppMe);

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszThisRepoId);
	SG_NULLFREE(pCtx, pszThisHashMethod);
	SG_NULLFREE(pCtx, pszOtherRepoId);
	SG_NULLFREE(pCtx, pszOtherHashMethod);
	SG_NULLFREE(pCtx, pPullIntoRepo);
	_NULLFREE_INSTANCE_DATA(pCtx, pMe);
}

static void _add_dagnodes_until_connected(SG_context* pCtx, 
										  SG_vhash** ppvhStagingStatus, 
										  sg_pull_instance_data* pMe, 
										  SG_client* pClient)
{
	SG_bool disconnected = SG_FALSE;
	SG_vhash* pvhFragballRequest = NULL;
	char* pszFragballName = NULL;
	SG_vhash* pvhRequestStatus = NULL;
	const SG_pathname* pStagingPathname;

	SG_ERR_CHECK(  SG_staging__get_pathname(pCtx, pMe->pStaging, &pStagingPathname)  );

	SG_ERR_CHECK(  SG_vhash__has(pCtx, *ppvhStagingStatus, SG_SYNC_STATUS_KEY__DAGS, &disconnected)  );
	while (disconnected)
	{

#if TRACE_PULL
		SG_ERR_CHECK(  SG_vhash_debug__dump_to_console__named(pCtx, *ppvhStagingStatus, "pull staging status")  );
#endif
		// There's at least one dag with connection problems.  
		
		// Convert the staging status vhash into a fragball request vhash.
		pvhFragballRequest = *ppvhStagingStatus;
		*ppvhStagingStatus = NULL;
		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvhFragballRequest, SG_SYNC_STATUS_KEY__GENERATIONS, GENERATIONS_PER_ROUNDTRIP)  );

		SG_ERR_CHECK(  SG_client__pull_request_fragball(pCtx, pClient, pvhFragballRequest, pStagingPathname, &pszFragballName, &pvhRequestStatus)  );

		/* Ian TODO: inspect pvhRequestStatus */

		SG_VHASH_NULLFREE(pCtx, pvhRequestStatus);
		SG_VHASH_NULLFREE(pCtx, pvhFragballRequest);

		SG_ERR_CHECK(  SG_staging__slurp_fragball(pCtx, pMe->pStaging, (const char*)pszFragballName)  );
		SG_NULLFREE(pCtx, pszFragballName);

		SG_ERR_CHECK(  SG_staging__check_status(pCtx, pMe->pStaging, SG_TRUE, SG_FALSE, SG_FALSE, SG_FALSE, SG_FALSE, ppvhStagingStatus)  );

		SG_ERR_CHECK(  SG_vhash__has(pCtx, *ppvhStagingStatus, SG_SYNC_STATUS_KEY__DAGS, &disconnected)  );

#if TRACE_PULL
		SG_ERR_CHECK(  SG_vhash_debug__dump_to_console__named(pCtx, *ppvhStagingStatus, "pull staging status")  );
#endif
	}

	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "done")  );

	/* fall through */
fail:
	SG_VHASH_NULLFREE(pCtx, *ppvhStagingStatus);
	SG_VHASH_NULLFREE(pCtx, pvhFragballRequest);
	SG_NULLFREE(pCtx, pszFragballName);
	SG_VHASH_NULLFREE(pCtx, pvhRequestStatus);
	SG_ERR_IGNORE(  SG_context__msg__emit(pCtx, "\n")  );
}

static void _add_blobs_until_done(SG_context* pCtx, 
								  SG_staging* pStaging, 
								  SG_client* pClient) 
{
	SG_bool need_blobs = SG_FALSE;
	SG_vhash* pvhFragballRequest = NULL;
	char* pszFragballName = NULL;
	SG_vhash* pvhRequestStatus = NULL;
	const SG_pathname* pStagingPathname = NULL;
	SG_vhash* pvhStagingStatus = NULL;

	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "Retrieving blobs...")  );

	SG_ERR_CHECK(  SG_staging__check_status(pCtx, pStaging, SG_FALSE, SG_FALSE, SG_FALSE, SG_TRUE, SG_TRUE, &pvhStagingStatus)  );

	SG_ERR_CHECK(  SG_vhash__has(pCtx, pvhStagingStatus, SG_SYNC_STATUS_KEY__BLOBS, &need_blobs)  );
	
	if (need_blobs)
		SG_ERR_CHECK(  SG_staging__get_pathname(pCtx, pStaging, &pStagingPathname)  );

	while (need_blobs)
	{
		pvhFragballRequest = pvhStagingStatus;
		pvhStagingStatus = NULL;

		SG_ERR_CHECK(  SG_client__pull_request_fragball(pCtx, pClient, pvhFragballRequest, pStagingPathname, &pszFragballName, &pvhRequestStatus)  );

		/* Ian TODO: inspect pvhRequestStatus */

		SG_VHASH_NULLFREE(pCtx, pvhRequestStatus);
		SG_VHASH_NULLFREE(pCtx, pvhFragballRequest);

		SG_ERR_CHECK(  SG_staging__slurp_fragball(pCtx, pStaging, (const char*)pszFragballName)  );
		SG_NULLFREE(pCtx, pszFragballName);

		SG_ERR_CHECK(  SG_staging__check_status(pCtx, pStaging, SG_FALSE, SG_FALSE, SG_FALSE, SG_TRUE, SG_TRUE, &pvhStagingStatus)  );

#if TRACE_PULL
		SG_ERR_CHECK(  SG_vhash_debug__dump_to_console__named(pCtx, pvhStagingStatus, "pull staging status")  );
#endif

		SG_ERR_CHECK(  SG_vhash__has(pCtx, pvhStagingStatus, SG_SYNC_STATUS_KEY__BLOBS, &need_blobs)  );
	}

	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "done")  );

	/* fall through */
fail:
	SG_VHASH_NULLFREE(pCtx, pvhStagingStatus);
	SG_VHASH_NULLFREE(pCtx, pvhFragballRequest);
	SG_NULLFREE(pCtx, pszFragballName);
	SG_VHASH_NULLFREE(pCtx, pvhRequestStatus);
	SG_ERR_IGNORE(  SG_context__msg__emit(pCtx, "\n")  );
}

/**
 * To be called after an initial roundtrip in which the initial leaves for the pull were added.
 * Caller should provide the (address of the) status vhash from that initial server roundtrip, we'll free it.
 * This routine will complete the pull.
 */
static void _pull_common(SG_context* pCtx,
						 SG_client* pClient,
						 SG_vhash** ppvhStagingStatus,
						 sg_pull_instance_data* pMe,
						 SG_varray** ppvaErr,
						 SG_varray** ppvaLog)
{
	/* Check the status and use it to request more dagnodes until the dags connect */
	SG_ERR_CHECK_RETURN(  _add_dagnodes_until_connected(pCtx, ppvhStagingStatus, pMe, pClient)  );

	/* All necessary dagnodes are in the frag at this point.  Now get remaining missing blobs. */
	SG_ERR_CHECK_RETURN(  _add_blobs_until_done(pCtx, pMe->pStaging, pClient)  );

	/* commit and cleanup */
	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "Committing changes...")  );
	SG_ERR_CHECK_RETURN(  SG_staging__commit(pCtx, pMe->pStaging)  );
	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "done\n")  );
	SG_ERR_CHECK_RETURN(  SG_staging__cleanup(pCtx, &pMe->pStaging)  );

	/* auto-merge zing dags */
	SG_ERR_CHECK_RETURN(  SG_zing__auto_merge_all_dags(pCtx, pMe->pPullIntoRepo, ppvaErr, ppvaLog)  );
}

void SG_pull__clone(
	SG_context* pCtx, 
	const char* pszPullIntoRepoDescriptorName, 
	SG_client* pClient)
{
	sg_pull_instance_data* pMe = NULL;
	char* pszFragballName = NULL;
	const SG_pathname* pStagingPathname;

	SG_NULLARGCHECK_RETURN(pszPullIntoRepoDescriptorName);
	SG_NULLARGCHECK_RETURN(pClient);

	SG_ERR_CHECK(  _pull_init(pCtx, pClient, pszPullIntoRepoDescriptorName, &pMe)  );
	SG_ERR_CHECK(  SG_staging__get_pathname(pCtx, pMe->pStaging, &pStagingPathname)  );

	/* Request a fragball containing the entire repo */
	SG_ERR_CHECK(  SG_client__pull_clone(pCtx, pClient, pStagingPathname, &pszFragballName)  );

	/* commit and cleanup */
	SG_ERR_CHECK_RETURN(  SG_staging__commit_fragball(pCtx, pMe->pStaging, pszFragballName)  );

	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "Cleaning up...")  );
	SG_ERR_CHECK_RETURN(  SG_staging__cleanup(pCtx, &pMe->pStaging)  );
	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "done")  );

	/* fall through */
fail:
	_NULLFREE_INSTANCE_DATA(pCtx, pMe);
	SG_NULLFREE(pCtx, pszFragballName);
	SG_ERR_IGNORE(  SG_context__msg__emit(pCtx, "\n")  );
}


void SG_pull__all(SG_context* pCtx, 
				  const char* pszPullIntoRepoDescriptorName, 
				  SG_client* pClient,
				  SG_varray** ppvaErr,
				  SG_varray** ppvaLog)
{
	sg_pull_instance_data* pMe = NULL;
	char* pszFragballName = NULL;
	SG_vhash* pvhStatus = NULL; // Used for both fragball request status returned by SG_client
								// and staging status returned by SG_staging.
	const SG_pathname* pStagingPathname;

	SG_NULLARGCHECK_RETURN(pszPullIntoRepoDescriptorName);
	SG_NULLARGCHECK_RETURN(pClient);

	SG_ERR_CHECK(  _pull_init(pCtx, pClient, pszPullIntoRepoDescriptorName, &pMe)  );
	SG_ERR_CHECK(  SG_staging__get_pathname(pCtx, pMe->pStaging, &pStagingPathname)  );

	/* Request a fragball containing leaves of every dag */
	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "Retrieving dagnodes...")  );
	SG_ERR_CHECK(  SG_client__pull_request_fragball(pCtx, pClient, NULL, pStagingPathname, &pszFragballName, &pvhStatus)  );

	/* Ian TODO: inspect pvhStatus */

	SG_ERR_CHECK(  SG_VHASH_NULLFREE(pCtx, pvhStatus)  );

	SG_ERR_CHECK(  SG_staging__slurp_fragball(pCtx, pMe->pStaging, pszFragballName)  );
	SG_ERR_CHECK(  SG_staging__check_status(pCtx, pMe->pStaging, SG_TRUE, SG_FALSE, SG_FALSE, SG_FALSE, SG_FALSE, &pvhStatus)  );

	/* Finish the pull using the status data we got back. */
	SG_ERR_CHECK(  _pull_common(pCtx, pClient, &pvhStatus, pMe, ppvaErr, ppvaLog)  );

	/* fall through */
fail:
	_NULLFREE_INSTANCE_DATA(pCtx, pMe);
	SG_VHASH_NULLFREE(pCtx, pvhStatus);
	SG_NULLFREE(pCtx, pszFragballName);
}

void SG_pull__begin(
	SG_context* pCtx,
	const char* pszPullIntoRepoDescriptorName, 
	SG_client* pClient,
	SG_pull** ppPull)
{
	_sg_pull* pMyPull = NULL;

	SG_NULLARGCHECK_RETURN(pszPullIntoRepoDescriptorName);
	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(ppPull);

	SG_alloc1(pCtx, pMyPull);
	SG_ERR_CHECK(  _pull_init(pCtx, pClient, pszPullIntoRepoDescriptorName, &pMyPull->pPullInstance)  );
	pMyPull->pClient = pClient;

	*ppPull = (SG_pull*)pMyPull;
	pMyPull = NULL;

	/* fall through */
fail:
	SG_ERR_IGNORE(  _sg_pull__nullfree(pCtx, &pMyPull)  );;
}

/**
 * Add to the fragball request vhash (see SG_server_prototypes.h for format).
 */
void SG_pull__add(
	SG_context* pCtx,
	SG_pull* pPull,
	SG_uint32 iDagnum,
	SG_rbtree* prbDagnodes,
	SG_rbtree* prbTags,
	SG_rbtree* prbDagnodePrefixes)
{
	_sg_pull* pMyPull = NULL;
	char bufDagnum[SG_DAGNUM__BUF_MAX__DEC];
	SG_bool found = SG_FALSE;
	SG_vhash* pvhDags = NULL; // Needs to be freed
	SG_vhash* pvhDagsRef = NULL; // Does not need to be freed, owned by parent vhash
	SG_vhash* pvhDagnum = NULL; // Needs to be freed
	SG_vhash* pvhDagnumRef = NULL; // Does not need to be freed, owned by parent vhash
	SG_rbtree_iterator* pit = NULL;

	SG_NULLARGCHECK_RETURN(pPull);
	SG_ARGCHECK_RETURN(iDagnum, iDagnum);
	pMyPull = (_sg_pull*)pPull;

	if (!pMyPull->pvhFragballRequest)
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pMyPull->pvhFragballRequest)  );

	SG_ERR_CHECK(  SG_dagnum__to_sz__decimal(pCtx, iDagnum, bufDagnum, sizeof(bufDagnum))  );

	/* Get dagnum vhash, adding it if necessary. */
	SG_ERR_CHECK(  SG_vhash__has(pCtx, pMyPull->pvhFragballRequest, SG_SYNC_STATUS_KEY__DAGS, &found)  );
	if (found)
		SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pMyPull->pvhFragballRequest, SG_SYNC_STATUS_KEY__DAGS, &pvhDagsRef)  );
	else
	{
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhDags)  );
		pvhDagsRef = pvhDags;
		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pMyPull->pvhFragballRequest, SG_SYNC_STATUS_KEY__DAGS, &pvhDags)  );
	}
	
	SG_ERR_CHECK(  SG_vhash__has(pCtx, pvhDagsRef, bufDagnum, &found)  );
	if (found)
		SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhDagsRef, bufDagnum, &pvhDagnumRef)  );
	
	if (!pvhDagnumRef)
	{
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhDagnum)  );
		pvhDagnumRef = pvhDagnum;
		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhDagsRef, bufDagnum, &pvhDagnum)  );
	}

	/* If dagnodes were provided, add them to the dagnum vhash */
	if (prbDagnodes)
	{
		const char* pszHid;
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prbDagnodes, &found, &pszHid, NULL)  );
		while (found)
		{
			SG_ERR_CHECK(  SG_vhash__update__null(pCtx, pvhDagnumRef, pszHid)  );
			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &found, &pszHid, NULL)  );
		}
		SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
	}	
	/* If tags were provided, add them to the dagnum vhash */
	if (prbTags)
	{
		const char* pszTag;
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prbTags, &found, &pszTag, NULL)  );
		while (found)
		{
			SG_ERR_CHECK(  SG_vhash__update__string__sz(pCtx, pvhDagnumRef, pszTag, SG_SYNC_REQUEST_VALUE_TAG)  );
			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &found, &pszTag, NULL)  );
		}
		SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
	}	
	/* If dagnode hid prefixes were provided, add them to the dagnum vhash */
	if (prbDagnodePrefixes)
	{
		const char* pszHidPrefix;
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prbDagnodePrefixes, &found, &pszHidPrefix, NULL)  );
		while (found)
		{
			SG_ERR_CHECK(  SG_vhash__update__string__sz(pCtx, pvhDagnumRef, pszHidPrefix, SG_SYNC_REQUEST_VALUE_HID_PREFIX)  );
			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &found, &pszHidPrefix, NULL)  );
		}
		SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
	}	

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhDagnum);
	SG_VHASH_NULLFREE(pCtx, pvhDags);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
}

void SG_pull__commit(
	SG_context* pCtx,
	SG_pull** ppPull,
	SG_varray** ppvaZingErr,
	SG_varray** ppvaZingLog)
{
	_sg_pull* pMyPull = NULL;
	sg_pull_instance_data* pMe = NULL;
	SG_vhash* pvhStatus = NULL;
	char* pszFragballName = NULL;
	const SG_pathname* pStagingPathname;
	
	SG_NULL_PP_CHECK_RETURN(ppPull);
	pMyPull = (_sg_pull*)*ppPull;
	pMe = pMyPull->pPullInstance;

	SG_ERR_CHECK(  SG_staging__get_pathname(pCtx, pMe->pStaging, &pStagingPathname)  );

	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "Retrieving dagnodes...")  );
	SG_ERR_CHECK(  SG_client__pull_request_fragball(pCtx, pMyPull->pClient, pMyPull->pvhFragballRequest, pStagingPathname, &pszFragballName, &pvhStatus)  );
#if TRACE_PULL
	SG_ERR_CHECK(  SG_vhash_debug__dump_to_console__named(pCtx, pvhStatus, "pull staging status")  );
#endif

	/* Ian TODO: check status of pull request */

	SG_ERR_CHECK(  SG_VHASH_NULLFREE(pCtx, pvhStatus)  );

	SG_ERR_CHECK(  SG_staging__slurp_fragball(pCtx, pMe->pStaging, pszFragballName)  );
	SG_ERR_CHECK(  SG_staging__check_status(pCtx, pMe->pStaging, SG_TRUE, SG_FALSE, SG_FALSE, SG_FALSE, SG_FALSE, &pvhStatus)  );

	/* Finish the pull using the status data we got back. */
	SG_ERR_CHECK(  _pull_common(pCtx, pMyPull->pClient, &pvhStatus, pMe, ppvaZingErr, ppvaZingLog)  );

	/* fall through */
fail:
	SG_ERR_IGNORE(  _sg_pull__nullfree(pCtx, (_sg_pull**)ppPull)  );
	SG_VHASH_NULLFREE(pCtx, pvhStatus);
	SG_NULLFREE(pCtx, pszFragballName);
}

void SG_pull__abort(
	SG_context* pCtx,
	SG_pull** ppPull)
{
	_sg_pull** ppMyPull = (_sg_pull**)ppPull;

	SG_NULL_PP_CHECK_RETURN(ppPull);

	SG_ERR_CHECK_RETURN(  _sg_pull__nullfree(pCtx, ppMyPull)  );
}

void SG_pull__all__list_incoming(
	SG_context* pCtx,
	const char* pszPullIntoRepoDescriptorName,
	SG_client* pClient,
	SG_varray** ppvaInfo)
{
	sg_pull_instance_data* pMe = NULL;
	char* pszFragballName = NULL;
	SG_vhash* pvhStatus = NULL; // Used for both fragball request status returned by SG_client
	// and staging status returned by SG_staging.
	const SG_pathname* pStagingPathname;

	SG_NULLARGCHECK_RETURN(pszPullIntoRepoDescriptorName);
	SG_NULLARGCHECK_RETURN(pClient);

	SG_ERR_CHECK(  _pull_init(pCtx, pClient, pszPullIntoRepoDescriptorName, &pMe)  );
	SG_ERR_CHECK(  SG_staging__get_pathname(pCtx, pMe->pStaging, &pStagingPathname)  );

	/* Request a fragball containing leaves of every dag */
	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "Retrieving dagnodes...")  );
	SG_ERR_CHECK(  SG_client__pull_request_fragball(pCtx, pClient, NULL, pStagingPathname, &pszFragballName, &pvhStatus)  );

	/* Ian TODO: inspect pvhStatus */

	SG_ERR_CHECK(  SG_VHASH_NULLFREE(pCtx, pvhStatus)  );

	SG_ERR_CHECK(  SG_staging__slurp_fragball(pCtx, pMe->pStaging, pszFragballName)  );
	SG_ERR_CHECK(  SG_staging__check_status(pCtx, pMe->pStaging, SG_TRUE, SG_FALSE, SG_FALSE, SG_FALSE, SG_FALSE, &pvhStatus)  );

	/* Check the status and use it to request more dagnodes until the dags connect */
	SG_ERR_CHECK(  _add_dagnodes_until_connected(pCtx, &pvhStatus, pMe, pClient)  );
	SG_ERR_CHECK(  SG_VHASH_NULLFREE(pCtx, pvhStatus)  );

	SG_ERR_CHECK(  SG_staging__check_status(pCtx, pMe->pStaging, SG_FALSE, SG_FALSE, SG_TRUE, SG_FALSE, SG_FALSE, &pvhStatus)  );
	{
		SG_bool b = SG_FALSE;
		SG_vhash* pvhRequest = NULL;
		SG_ERR_CHECK(  SG_vhash__has(pCtx, pvhStatus, SG_SYNC_STATUS_KEY__NEW_NODES, &b)  );
		if (b)
		{
			/* There are incoming nodes.  Get their info. */
			SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhStatus, SG_SYNC_STATUS_KEY__NEW_NODES, &pvhRequest)  );
			SG_ERR_CHECK(  SG_client__get_dagnode_info(pCtx, pClient, pvhRequest, ppvaInfo)  );
		}
	}

	/* fall through */
fail:
	_NULLFREE_INSTANCE_DATA(pCtx, pMe);
	SG_VHASH_NULLFREE(pCtx, pvhStatus);
	SG_NULLFREE(pCtx, pszFragballName);
}