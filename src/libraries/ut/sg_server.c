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

#define TRACE_SERVER 0

void SG_server__alloc(
	SG_context* pCtx,
	SG_server** ppNew
	)
{
	SG_server* pThis = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pThis)  );

	*ppNew = pThis;
	pThis = NULL;

	/* fallthru */

fail:
	SG_SERVER_NULLFREE(pCtx, pThis);
}

void SG_server__free(SG_context * pCtx, SG_server* pServer)
{
	if (!pServer)
		return;

	SG_NULLFREE(pCtx, pServer);
}

void SG_server__push_get_staging_path(SG_context* pCtx, SG_server* pServer, const char* pPushId, SG_pathname** ppStagingPathname)
{
	SG_staging* pStaging = NULL;
	const SG_pathname* pStagingPathname = NULL;

	SG_NULLARGCHECK_RETURN(pServer);
	SG_NULLARGCHECK_RETURN(ppStagingPathname);

	SG_ERR_CHECK(  SG_staging__open(pCtx, pPushId, &pStaging)  );
	SG_ERR_CHECK(  SG_staging__get_pathname(pCtx, pStaging, &pStagingPathname)  );
	SG_ERR_CHECK(  SG_pathname__alloc__copy(pCtx, ppStagingPathname, pStagingPathname)  );

	/* fall through */
fail:
	SG_STAGING_NULLFREE(pCtx, pStaging);
}

void SG_server__push_begin(
	SG_context* pCtx,
	SG_server * pServer,
	const char* psz_repo_descriptor_name,
	const char** ppszPushId
	)
{
	char* psz_staging_area_name = NULL;

	SG_NULLARGCHECK_RETURN(pServer);
	SG_NULLARGCHECK_RETURN(ppszPushId);

	SG_ERR_CHECK(  SG_staging__create(pCtx, psz_repo_descriptor_name, &psz_staging_area_name, NULL)  );

	SG_RETURN_AND_NULL(psz_staging_area_name, ppszPushId);

	/* fallthru */

fail:
	SG_NULLFREE(pCtx, psz_staging_area_name);
}

void SG_server__push_add(
	SG_context* pCtx,
	SG_server * pServer,
	const char* pPushId,
	const char* psz_fragball_name,
	SG_vhash** ppResult
	)
{
	SG_staging* pStaging = NULL;
	SG_vhash* pvh_status = NULL;
	SG_repo* pRepo = NULL;

	SG_NULLARGCHECK_RETURN(pServer);
	SG_NULLARGCHECK_RETURN(pPushId);
	SG_NULLARGCHECK_RETURN(ppResult);

	SG_ERR_CHECK(  SG_staging__open(pCtx, pPushId, &pStaging)  );

	SG_ERR_CHECK(  SG_staging__slurp_fragball(pCtx, pStaging, psz_fragball_name)  );

	SG_ERR_CHECK(  SG_staging__check_status(pCtx, pStaging, SG_TRUE, SG_TRUE, SG_FALSE, SG_TRUE, SG_TRUE, &pvh_status)  );

	*ppResult = pvh_status;
	pvh_status = NULL;

	/* fallthru */

fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_VHASH_NULLFREE(pCtx, pvh_status);
	SG_STAGING_NULLFREE(pCtx, pStaging);
}

void SG_server__push_remove(
	SG_context* pCtx,
	SG_server * pServer,
	const char* pPush,
	SG_vhash* pvh_stuff_to_be_removed_from_staging_area,
	SG_vhash** ppResult
	);

void SG_server__push_commit(
	SG_context* pCtx,
	SG_server * pServer,
	const char* pPushId,
	SG_vhash** ppResult
	)
{
	SG_staging* pStaging = NULL;
	SG_vhash* pvh_status = NULL;
	SG_repo* pRepo = NULL;

	SG_NULLARGCHECK_RETURN(pServer);
	SG_NULLARGCHECK_RETURN(pPushId);
	SG_NULLARGCHECK_RETURN(ppResult);

	SG_ERR_CHECK(  SG_staging__open(pCtx, pPushId, &pStaging)  );

	SG_ERR_CHECK(  SG_staging__commit(pCtx, pStaging)  );

	SG_ERR_CHECK(  SG_staging__check_status(pCtx, pStaging, SG_TRUE, SG_TRUE, SG_FALSE, SG_TRUE, SG_TRUE, &pvh_status)  );

	*ppResult = pvh_status;
	pvh_status = NULL;

	/* fallthru */

fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_VHASH_NULLFREE(pCtx, pvh_status);
	SG_STAGING_NULLFREE(pCtx, pStaging);
}

void SG_server__push_end(SG_context* pCtx,
						 SG_server * pServer,
						 const char* pPushId)
{
	SG_NULLARGCHECK_RETURN(pServer);
	SG_NULLARGCHECK_RETURN(pPushId);

	SG_ERR_CHECK_RETURN(  SG_staging__cleanup__by_id(pCtx, pPushId)  );
}

void SG_server__pull_request_fragball(SG_context* pCtx,
									  SG_repo* pRepo,
									  SG_vhash* pvhRequest,
									  const SG_pathname* pFragballDirPathname,
									  char** ppszFragballName,
									  SG_vhash** ppvhStatus)
{
	SG_pathname* pFragballPathname = NULL;
	SG_uint32* paDagNums = NULL;
    SG_rbtree* prbDagnodes = NULL;
	SG_string* pstrFragballName = NULL;
	char* pszRevFullHid = NULL;
	SG_rbtree_iterator* pit = NULL;
	SG_uint32* repoDagnums = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pFragballDirPathname);
	SG_NULLARGCHECK_RETURN(ppvhStatus);

#if TRACE_SERVER
	SG_ERR_CHECK(  SG_vhash_debug__dump_to_console__named(pCtx, pvhRequest, "pull fragball request")  );
#endif

	SG_ERR_CHECK(  SG_fragball__create(pCtx, pFragballDirPathname, &pFragballPathname)  );

	if (!pvhRequest)
	{
		// Add leaves from every dag to the fragball.
		SG_uint32 count_dagnums;
		SG_uint32 i;
		SG_ERR_CHECK(  SG_repo__list_dags(pCtx, pRepo, &count_dagnums, &paDagNums)  );

		for (i=0; i<count_dagnums; i++)
		{
			SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, paDagNums[i], &prbDagnodes)  );
			SG_ERR_CHECK(  SG_fragball__append__dagnodes(pCtx, pFragballPathname, pRepo, paDagNums[i], prbDagnodes)  );
			SG_RBTREE_NULLFREE(pCtx, prbDagnodes);
		}

		SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pFragballPathname, &pstrFragballName)  );
		SG_ERR_CHECK(  SG_STRDUP(pCtx, SG_string__sz(pstrFragballName), ppszFragballName)  );
	}
	else
	{
		// Build the requested fragball.
		SG_bool found;

		SG_ERR_CHECK(  SG_vhash__has(pCtx, pvhRequest, SG_SYNC_STATUS_KEY__CLONE, &found)  );
		if (found)
		{
			// Full clone requested.
			SG_ERR_CHECK(  SG_repo__fetch_repo__fragball(pCtx, pRepo, pFragballDirPathname, ppszFragballName) );
		}
		else
		{
			// Not a full clone.

			SG_ERR_CHECK(  SG_vhash__has(pCtx, pvhRequest, SG_SYNC_STATUS_KEY__DAGS, &found)  );
			if (found)
			{
				// Dagnodes were requested.

				SG_uint32 generations = 0;
				SG_vhash* pvhDags;
				SG_uint32 count_requested_dagnums;
				SG_uint32 count_repo_dagnums = 0;
				SG_uint32 i;
				const char* pszDagNum = NULL;
				const SG_variant* pvRequestedNodes = NULL;
				SG_vhash* pvhRequestedNodes = NULL;
				const char* pszHidRequestedDagnode = NULL;

				// Were additional generations requested?
				SG_ERR_CHECK(  SG_vhash__has(pCtx, pvhRequest, SG_SYNC_STATUS_KEY__GENERATIONS, &found)  );
				if (found)
					SG_ERR_CHECK(  SG_vhash__get__uint32(pCtx, pvhRequest, SG_SYNC_STATUS_KEY__GENERATIONS, &generations)  );

				SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhRequest, SG_SYNC_STATUS_KEY__DAGS, &pvhDags)  );
				SG_ERR_CHECK(  SG_vhash__count(pCtx, pvhDags, &count_requested_dagnums)  );
				if (count_requested_dagnums)
					SG_ERR_CHECK(  SG_repo__list_dags(pCtx, pRepo, &count_repo_dagnums, &repoDagnums)  );

				// For each requested dag, get the requested nodes.
				for (i=0; i<count_requested_dagnums; i++)
				{
					SG_uint32 iMissingNodeCount;
					SG_uint32 iDagnum;
					SG_uint32 j;
					SG_bool isValidDagnum = SG_FALSE;
					SG_bool bSpecificNodesRequested = SG_FALSE;

					// Get the dag's missing node vhash.
					SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvhDags, i, &pszDagNum, &pvRequestedNodes)  );
					SG_ERR_CHECK(  SG_dagnum__from_sz__decimal(pCtx, pszDagNum, &iDagnum)  );

					// Verify that requested dagnum exists
					for (j = 0; j < count_repo_dagnums; j++)
					{
						if (repoDagnums[j] == iDagnum)
						{
							isValidDagnum = SG_TRUE;
							break;
						}
					}
					if (!isValidDagnum)
					{
						char buf[SG_DAGNUM__BUF_MAX__NAME];
						SG_ERR_CHECK(  SG_dagnum__to_name(pCtx, iDagnum, buf, sizeof(buf))  );
						SG_ERR_THROW2(SG_ERR_NO_SUCH_DAG, (pCtx, "%s", buf));
					}

					if (pvRequestedNodes)
					{
						SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pvRequestedNodes, &pvhRequestedNodes)  );

						// Get each node listed for the dag
						SG_ERR_CHECK(  SG_vhash__count(pCtx, pvhRequestedNodes, &iMissingNodeCount)  );
						if (iMissingNodeCount > 0)
						{
							SG_uint32 j;
							const SG_variant* pvVal;

							bSpecificNodesRequested = SG_TRUE;

							SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS(pCtx, &prbDagnodes, iMissingNodeCount, NULL)  );
							for (j=0; j<iMissingNodeCount; j++)
							{
								SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvhRequestedNodes, j, &pszHidRequestedDagnode, &pvVal)  );

								if (pvVal)
								{
									const char* pszVal;
									SG_ERR_CHECK(  SG_variant__get__sz(pCtx, pvVal, &pszVal)  );
									if (pszVal)
									{
										if (0 == strcmp(pszVal, SG_SYNC_REQUEST_VALUE_HID_PREFIX))
										{
											SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, iDagnum, pszHidRequestedDagnode, &pszRevFullHid)  );
											pszHidRequestedDagnode = pszRevFullHid;
										}
										else if (0 == strcmp(pszVal, SG_SYNC_REQUEST_VALUE_TAG))
										{
											SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, pszHidRequestedDagnode, &pszRevFullHid)  );
											if (!pszRevFullHid)
												SG_ERR_THROW(SG_ERR_TAG_NOT_FOUND);
											pszHidRequestedDagnode = pszRevFullHid;
										}
										else
											SG_ERR_THROW(SG_ERR_PULL_INVALID_FRAGBALL_REQUEST);
									}
								}
								
								SG_ERR_CHECK(  SG_rbtree__update(pCtx, prbDagnodes, pszHidRequestedDagnode)  );
								// Get additional dagnode generations, if requested.
								SG_ERR_CHECK(  SG_sync__add_n_generations(pCtx, pRepo, pszHidRequestedDagnode, prbDagnodes, generations)  );
								SG_NULLFREE(pCtx, pszRevFullHid);
							}
						}
					}

					if (!bSpecificNodesRequested)
					{
						// When no specific nodes are in the request, add all leaves.
						SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, iDagnum, &prbDagnodes)  );

						// Get additional dagnode generations, if requested.
						if (generations)
						{
							SG_bool found;
							const char* hid;
							
							SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prbDagnodes, &found, &hid, NULL)  );
							while (found)
							{
								SG_ERR_CHECK(  SG_sync__add_n_generations(pCtx, pRepo, hid, prbDagnodes, generations)  );
								SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &found, &hid, NULL)  );
							}
						}
					}

					if (prbDagnodes) // can be null when leaves of an empty dag are requested
					{
						SG_ERR_CHECK(  SG_fragball__append__dagnodes(pCtx, pFragballPathname, pRepo, iDagnum, prbDagnodes)  );
						SG_RBTREE_NULLFREE(pCtx, prbDagnodes);
					}

				} // dagnum loop
			} // if "dags" exists

			/* Add requested blobs to the fragball */
			SG_ERR_CHECK(  SG_vhash__has(pCtx, pvhRequest, SG_SYNC_STATUS_KEY__BLOBS, &found)  );
			if (found)
			{
				// Blobs were requested.
				SG_vhash* pvhBlobs;
				SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhRequest, SG_SYNC_STATUS_KEY__BLOBS, &pvhBlobs)  );
				SG_ERR_CHECK(  SG_sync__add_blobs_to_fragball(pCtx, pRepo, pFragballPathname, pvhBlobs)  );
			}

			SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pFragballPathname, &pstrFragballName)  );
			SG_ERR_CHECK(  SG_STRDUP(pCtx, SG_string__sz(pstrFragballName), ppszFragballName)  );
		}
	}
	
	/* fallthru */
fail:
	// If we had an error, delete the half-baked fragball.
	if (pFragballPathname && SG_context__has_err(pCtx))
		SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pFragballPathname)  );

	SG_PATHNAME_NULLFREE(pCtx, pFragballPathname);
	SG_NULLFREE(pCtx, paDagNums);
	SG_RBTREE_NULLFREE(pCtx, prbDagnodes);
	SG_STRING_NULLFREE(pCtx, pstrFragballName);
	SG_NULLFREE(pCtx, pszRevFullHid);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
	SG_NULLFREE(pCtx, repoDagnums);
}

void SG_server__get_temp_fragball_path(SG_context* pCtx,
									   const char* pPushId,
									   const char** ppsz_fragball_name,
									   SG_pathname** ppPath)
{
	SG_pathname* pPath = NULL;
	char* psz_fragball_name = NULL;

	SG_NULLARGCHECK_RETURN(ppsz_fragball_name);
	SG_NULLARGCHECK_RETURN(ppPath);

	SG_ERR_CHECK(  SG_staging__get_pathname__from_id(pCtx, pPushId, &pPath)  );
	SG_ERR_CHECK(  SG_tid__alloc(pCtx, &psz_fragball_name)  );		// TODO consider SG_tid__alloc2(...)

	*ppPath = pPath;
	*ppsz_fragball_name = psz_fragball_name;

	return;
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_NULLFREE(pCtx, psz_fragball_name);
}

void SG_server__get_repo_info(SG_context* pCtx,
							  SG_repo* pRepo,
							  char** ppszRepoId,
							  char** ppszAdminId,
							  char** ppszHashMethod)
{
	char* pszRepoId = NULL;
	char* pszAdminId = NULL;
	char* pszHashMethod = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	if (ppszRepoId)
		SG_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pRepo, &pszRepoId)  );
	if (ppszAdminId)
		SG_ERR_CHECK(  SG_repo__get_admin_id(pCtx, pRepo, &pszAdminId)  );
	if (ppszHashMethod)
		SG_ERR_CHECK(  SG_repo__get_hash_method(pCtx, pRepo, &pszHashMethod)  );

	if (ppszRepoId)
	{
		*ppszRepoId = pszRepoId;
		pszRepoId = NULL;
	}
	if (ppszAdminId)
	{
		*ppszAdminId = pszAdminId;
		pszAdminId = NULL;
	}
	if (ppszHashMethod)
	{
		*ppszHashMethod = pszHashMethod;
		pszHashMethod = NULL;
	}

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszRepoId);
	SG_NULLFREE(pCtx, pszAdminId);
	SG_NULLFREE(pCtx, pszHashMethod);
}

void SG_server__get_dagnode_info(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_vhash* pvhRequest,
	SG_varray** ppvaInfo)
{
	char bufDagnum[SG_DAGNUM__BUF_MAX__DEC];
	SG_vhash* pvhRefVersionControlHids = NULL;
	SG_varray* pvaHids = NULL;
	SG_stringarray* psaHids = NULL;
	const char* const* paszHids = NULL;
	SG_uint32 countHids = 0;

	SG_ERR_CHECK(  SG_dagnum__to_sz__decimal(pCtx, SG_DAGNUM__VERSION_CONTROL, bufDagnum, sizeof(bufDagnum))  );
	SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhRequest, bufDagnum, &pvhRefVersionControlHids)  );

	// Ugh.  Is vhash->varray->stringarray->char** the best option?
	SG_ERR_CHECK(  SG_vhash__get_keys(pCtx, pvhRefVersionControlHids, &pvaHids)  );
	SG_ERR_CHECK(  SG_varray__to_stringarray(pCtx, pvaHids, &psaHids)  );
	SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, psaHids, &paszHids, &countHids)  );

	SG_ERR_CHECK(  SG_history__query(pCtx, NULL, pRepo, 0, NULL, paszHids, countHids, NULL, NULL, 0, 0, 0, SG_FALSE, SG_FALSE, ppvaInfo)  );

	/* fall through */
fail:
	SG_VARRAY_NULLFREE(pCtx, pvaHids);
	SG_STRINGARRAY_NULLFREE(pCtx, psaHids);
}
