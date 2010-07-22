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

#define TRACE_PUSH 0

typedef struct
{
	SG_repo* pRepo;
	SG_client* pClient;
	SG_pathname* pTempPathname;
	SG_pathname* pFragballPathname;
	SG_client_push_handle* pClientPushHandle;
} _sg_push;

static void _sg_push__nullfree(SG_context* pCtx, _sg_push** ppMyPush)
{
	if (ppMyPush && *ppMyPush)
	{
		_sg_push* pMyPush = *ppMyPush;
		SG_PATHNAME_NULLFREE(pCtx, pMyPush->pTempPathname);
		SG_PATHNAME_NULLFREE(pCtx, pMyPush->pFragballPathname);
		if (pMyPush->pClient && pMyPush->pClientPushHandle)
			SG_ERR_IGNORE(  SG_client__push_end(pCtx, pMyPush->pClient, &pMyPush->pClientPushHandle)  );
		SG_NULLFREE(pCtx, pMyPush);
		*ppMyPush = NULL;
	}
}

static void _push_init(SG_context* pCtx, 
					   SG_repo* pThisRepo,
					   SG_client* pClient, 
					   SG_pathname** ppTempPathname, 
					   SG_pathname** ppFragballPathname, 
					   SG_client_push_handle** ppClientPushHandle)
{
	char* pszThisRepoId = NULL;
	char* pszThisHashMethod = NULL;
	char* pszOtherRepoId = NULL;
	char* pszOtherHashMethod = NULL;

	SG_pathname* pTempPathname = *ppTempPathname;
	SG_pathname* pFragballPathname = NULL;
	SG_client_push_handle* pClientPushHandle = NULL;

	SG_ERR_CHECK(  SG_client__get_repo_info(pCtx, pClient, &pszOtherRepoId, NULL, &pszOtherHashMethod)  );

	/* TODO This will care about dagnums once we're using the user dag. */
	SG_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pThisRepo, &pszThisRepoId)  );
	if (strcmp(pszThisRepoId, pszOtherRepoId) != 0)
		SG_ERR_THROW(SG_ERR_REPO_ID_MISMATCH);

	/* TODO check admin id when appropriate */

	SG_ERR_CHECK(  SG_repo__get_hash_method(pCtx, pThisRepo, &pszThisHashMethod)  );
	if (strcmp(pszThisHashMethod, pszOtherHashMethod) != 0)
		SG_ERR_THROW(SG_ERR_REPO_HASH_METHOD_MISMATCH);

	/* Start the push operation */
	SG_ERR_CHECK(  SG_client__push_begin(pCtx, pClient, &pTempPathname, &pClientPushHandle)  );

	/* Create a fragball */
	SG_ERR_CHECK(  SG_fragball__create(pCtx, pTempPathname, &pFragballPathname)  );

	SG_RETURN_AND_NULL(pTempPathname, ppTempPathname);
	SG_RETURN_AND_NULL(pFragballPathname, ppFragballPathname);
	SG_RETURN_AND_NULL(pClientPushHandle, ppClientPushHandle);

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszThisRepoId);
	SG_NULLFREE(pCtx, pszThisHashMethod);
	SG_NULLFREE(pCtx, pszOtherRepoId);
	SG_NULLFREE(pCtx, pszOtherHashMethod);
	SG_PATHNAME_NULLFREE(pCtx, pTempPathname);
	SG_FRAGBALL_NULLFREE(pCtx, pFragballPathname);
	if (SG_context__has_err(pCtx) && pClientPushHandle)
		SG_ERR_IGNORE(  SG_client__push_end(pCtx, pClient, &pClientPushHandle)  );
}


static void _add_dagnodes_until_connected(SG_context* pCtx, 
										  SG_vhash** ppvh_status, 
										  SG_pathname* pPath_tempdir, 
										  SG_repo* pRepo, 
										  SG_client* pClient, 
										  SG_client_push_handle* pClientPushHandle)
{
	SG_uint32 i;
	SG_bool disconnected = SG_FALSE;
	SG_pathname* pPath_fragball = NULL;
	SG_vhash* pvh_unknown_dagnodes = NULL;
	const char* pszDagNum = NULL; 
	const SG_variant* pvMissingNodes = NULL;
	SG_vhash* pvh_missing_nodes = NULL;
	SG_rbtree* prb_missing_nodes = NULL;
	const char* pszHidMissingDagnode = NULL;
	SG_uint32 count_dagnums;

	SG_ERR_CHECK(  SG_vhash__has(pCtx, *ppvh_status, SG_SYNC_STATUS_KEY__DAGS, &disconnected)  );
	while (disconnected)
	{
		// There's at least one dag with connection problems.
		SG_ERR_CHECK(  SG_fragball__create(pCtx, pPath_tempdir, &pPath_fragball)  );
		SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, *ppvh_status, SG_SYNC_STATUS_KEY__DAGS, &pvh_unknown_dagnodes)  );

		// For each dag, get the unknown nodes.
		SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_unknown_dagnodes, &count_dagnums)  );
		for (i=0; i<count_dagnums; i++)
		{
			SG_uint32 iMissingNodeCount;

			// Get the dag's missing node vhash.
			SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_unknown_dagnodes, i, &pszDagNum, &pvMissingNodes)  );

			SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pvMissingNodes, &pvh_missing_nodes)  );
			SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_missing_nodes, &iMissingNodeCount)  );
			if (iMissingNodeCount > 0)
			{
				SG_uint32 iDagnum;
				SG_uint32 j;

				SG_ERR_CHECK(  SG_rbtree__alloc__params(pCtx, &prb_missing_nodes, iMissingNodeCount, NULL)  );
				for (j=0; j<iMissingNodeCount; j++)
				{
					SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_missing_nodes, j, &pszHidMissingDagnode, NULL)  );
					SG_ERR_CHECK(  SG_rbtree__update(pCtx, prb_missing_nodes, pszHidMissingDagnode)  );

					SG_ERR_CHECK(  SG_sync__add_n_generations(pCtx, pRepo, pszHidMissingDagnode, prb_missing_nodes, GENERATIONS_PER_ROUNDTRIP)  );
				}

				SG_ERR_CHECK(  SG_dagnum__from_sz__decimal(pCtx, pszDagNum, &iDagnum)  );
				SG_ERR_CHECK(  SG_fragball__append__dagnodes(pCtx, pPath_fragball, pRepo, iDagnum, prb_missing_nodes)  );
				SG_RBTREE_NULLFREE(pCtx, prb_missing_nodes);
			}
		}

		SG_VHASH_NULLFREE(pCtx, *ppvh_status);

		SG_ERR_CHECK(  SG_client__push_add(pCtx, pClient, pClientPushHandle, &pPath_fragball, ppvh_status)  );

		SG_ERR_CHECK(  SG_vhash__has(pCtx, *ppvh_status, SG_SYNC_STATUS_KEY__DAGS, &disconnected)  );

#if TRACE_PUSH
		SG_ERR_CHECK(  SG_vhash_debug__dump_to_console__named(pCtx, *ppvh_status, "push staging status")  );
#endif
	}

	SG_FRAGBALL_NULLFREE(pCtx, pPath_fragball);
	SG_RBTREE_NULLFREE(pCtx, prb_missing_nodes);

	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "done\n")  );

	return;

fail:
	SG_FRAGBALL_NULLFREE(pCtx, pPath_fragball);
	SG_RBTREE_NULLFREE(pCtx, prb_missing_nodes);
	SG_VHASH_NULLFREE(pCtx, *ppvh_status);
	SG_ERR_IGNORE(  SG_context__msg__emit(pCtx, "\n")  );
}

static void _add_blobs_until_done(SG_context* pCtx, 
								  SG_vhash** ppvh_status, 
								  SG_pathname* pPath_tempdir, 
								  SG_repo* pRepo, 
								  SG_client* pClient, 
								  SG_client_push_handle* pph) 
{
	SG_bool need_blobs = SG_FALSE;
	SG_pathname* pPath_fragball = NULL;

	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "Sending blobs...")  );

	SG_ERR_CHECK(  SG_vhash__has(pCtx, *ppvh_status, SG_SYNC_STATUS_KEY__BLOBS, &need_blobs)  );

	while (need_blobs)
	{
		SG_vhash* pvh;
		SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, *ppvh_status, SG_SYNC_STATUS_KEY__BLOBS, &pvh)  );
		SG_ERR_CHECK(  SG_fragball__create(pCtx, pPath_tempdir, &pPath_fragball)  );
		SG_ERR_CHECK(  SG_sync__add_blobs_to_fragball(pCtx, pRepo, pPath_fragball, pvh)  );
		SG_VHASH_NULLFREE(pCtx, *ppvh_status);
		SG_ERR_CHECK(  SG_client__push_add(pCtx, pClient, pph, &pPath_fragball, ppvh_status)  );

#if TRACE_PUSH
		SG_ERR_CHECK(  SG_vhash_debug__dump_to_console__named(pCtx, *ppvh_status, "push staging status")  );
#endif

		SG_ERR_CHECK(  SG_vhash__has(pCtx, *ppvh_status, SG_SYNC_STATUS_KEY__BLOBS, &need_blobs)  );
	}

	SG_FRAGBALL_NULLFREE(pCtx, pPath_fragball);

	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "done\n")  );

	return;

fail:
	SG_FRAGBALL_NULLFREE(pCtx, pPath_fragball);
	SG_VHASH_NULLFREE(pCtx, *ppvh_status);
	SG_ERR_IGNORE(  SG_context__msg__emit(pCtx, "\n")  );
}

static void _check_for_head_addition(SG_context* pCtx, SG_vhash* pvhStatus)
{
	SG_vhash* pvhLeafDeltas = NULL;
	SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhStatus, SG_SYNC_STATUS_KEY__LEAFD, &pvhLeafDeltas)  );
	if (pvhLeafDeltas)
	{
		SG_uint32 i, count;
		const char* pszDagnum;
		const SG_variant* pvLeafDelta;
		SG_ERR_CHECK(  SG_vhash__count(pCtx, pvhLeafDeltas, &count)  );
		for (i = 0; i < count; i++)
		{
			SG_int64 leafDelta;
			SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvhLeafDeltas, i, &pszDagnum, &pvLeafDelta)  );
			SG_ERR_CHECK(  SG_variant__get__int64(pCtx, pvLeafDelta, &leafDelta)  );
			if (leafDelta > 0)
			{
				SG_uint32 iDagnum = 0;
				SG_ERR_CHECK(  SG_dagnum__from_sz__decimal(pCtx, pszDagnum, &iDagnum)  );
				if (SG_DAGNUM__VERSION_CONTROL == iDagnum)
					SG_ERR_THROW(SG_ERR_SYNC_ADDS_HEADS);
				else
				{
					char bufDagName[SG_DAGNUM__BUF_MAX__NAME];
					SG_ERR_CHECK(  SG_dagnum__to_name(pCtx, iDagnum, bufDagName, sizeof(bufDagName))  );
					SG_ERR_THROW2(SG_ERR_SYNC_ADDS_HEADS, (pCtx, "%s DAG", bufDagName));
				}
			}
		}
	}

	/* fall through */
fail:
	;
}

/**
 * To be called after an initial roundtrip in which the initial leaves for the push were added.
 * Caller should provide the (address of the) status vhash from that initial server roundtrip, we'll free it.
 * This routine will complete the push.
 */
static void _push_common(SG_context* pCtx,
						 SG_repo* pRepo,
						 SG_client* pClient,
						 SG_client_push_handle** ppClientPushHandle,
						 SG_vhash** ppvh_status,
						 SG_pathname* pPath_tempdir,
						 SG_bool bAllowHeadCountIncrease)
{
	/* Check the status and use it to send more dagnodes until the dags connect */
	SG_ERR_CHECK(  _add_dagnodes_until_connected(pCtx, ppvh_status, pPath_tempdir, pRepo, pClient, *ppClientPushHandle)  );

	if (!bAllowHeadCountIncrease)
		SG_ERR_CHECK(  _check_for_head_addition(pCtx, *ppvh_status)  );

	/* All necessary dagnodes are in the frag at this point.  Add the blobs the server said it was missing. */
	SG_ERR_CHECK(  _add_blobs_until_done(pCtx, ppvh_status, pPath_tempdir, pRepo, pClient, *ppClientPushHandle)  );

	SG_VHASH_NULLFREE(pCtx, *ppvh_status);

	/* commit and end */
	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "Committing changes...")  );
	SG_ERR_CHECK(  SG_client__push_commit(pCtx, pClient, *ppClientPushHandle, ppvh_status)  );
	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "done\n")  );

#if TRACE_PUSH
	SG_ERR_CHECK(  SG_vhash_debug__dump_to_console__named(pCtx, *ppvh_status, "push staging status")  );
#endif

	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "Cleaning up...")  );
	SG_ERR_CHECK(  SG_client__push_end(pCtx, pClient, ppClientPushHandle)  );
	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "done")  );

	/* fall through */
fail:
	SG_VHASH_NULLFREE(pCtx, *ppvh_status);
	SG_ERR_IGNORE(  SG_context__msg__emit(pCtx, "\n")  );
}

void SG_push__all(SG_context* pCtx, SG_repo* pSrcRepo, SG_client* pClient, SG_bool bAllowHeadCountIncrease)
{
    SG_pathname* pPath_tempdir = NULL;
    SG_pathname* pPath_fragball = NULL;
    SG_uint32 count_dagnums = 0;
    SG_uint32* paDagNums = NULL;
    SG_uint32 i;
    SG_rbtree* prb_leaves = NULL;
    SG_client_push_handle* pClientPushHandle = NULL;
	SG_vhash* pvh_status = NULL;

	SG_NULLARGCHECK(pSrcRepo);
	SG_NULLARGCHECK(pClient);

	SG_ERR_CHECK(  _push_init(pCtx, pSrcRepo, pClient, &pPath_tempdir, &pPath_fragball, &pClientPushHandle)  );

	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "Sending dagnodes...")  );

	/* For push__all, the initial fragball contains leaves from every dag.  We build it here. */
    SG_ERR_CHECK(  SG_repo__list_dags(pCtx, pSrcRepo, &count_dagnums, &paDagNums)  );

    for (i=0; i<count_dagnums; i++)
    {
         SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pSrcRepo, paDagNums[i], &prb_leaves)  );
         SG_ERR_CHECK(  SG_fragball__append__dagnodes(pCtx, pPath_fragball, pSrcRepo, paDagNums[i], prb_leaves)  );
		 SG_RBTREE_NULLFREE(pCtx, prb_leaves);
    }
    SG_NULLFREE(pCtx, paDagNums);
	SG_RBTREE_NULLFREE(pCtx, prb_leaves);

    SG_ERR_CHECK(  SG_client__push_add(pCtx, pClient, pClientPushHandle, &pPath_fragball, &pvh_status)  );

#if TRACE_PUSH
	SG_ERR_CHECK(  SG_vhash_debug__dump_to_console__named(pCtx, pvh_status, "push staging status")  );
#endif

	/* Finish the push using the status data we got back. */
	SG_ERR_CHECK(  _push_common(pCtx, pSrcRepo, pClient, &pClientPushHandle, &pvh_status, pPath_tempdir, bAllowHeadCountIncrease)  );
	
	/* fall through */
fail:
	if (SG_context__has_err(pCtx) && pClientPushHandle)
		SG_ERR_IGNORE(  SG_client__push_end(pCtx, pClient, &pClientPushHandle)  );

 	if (pPath_tempdir)
 		SG_ERR_IGNORE(  SG_fsobj__rmdir_recursive__pathname(pCtx, pPath_tempdir)  );

	SG_FRAGBALL_NULLFREE(pCtx, pPath_fragball);
	SG_PATHNAME_NULLFREE(pCtx, pPath_tempdir);
	SG_NULLFREE(pCtx, paDagNums);
	SG_RBTREE_NULLFREE(pCtx, prb_leaves);
	SG_VHASH_NULLFREE(pCtx, pvh_status);
}

void SG_push__begin(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_client* pClient,
	SG_push** ppPush)
{
	_sg_push* pMyPush = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(ppPush);

	SG_alloc1(pCtx, pMyPush);
	SG_ERR_CHECK(  _push_init(pCtx, pRepo, pClient, &pMyPush->pTempPathname, &pMyPush->pFragballPathname, &pMyPush->pClientPushHandle)  );
	pMyPush->pClient = pClient;
	pMyPush->pRepo = pRepo;

	*ppPush = (SG_push*)pMyPush;

	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "Sending dagnodes...")  );

	return;
	
fail:
	SG_ERR_IGNORE(  _sg_push__nullfree(pCtx, &pMyPush)  );
}

void SG_push__add(
	SG_context* pCtx, 
	SG_push* pPush,
	SG_uint32 iDagnum,
	SG_rbtree* prbDagnodes)
{
	_sg_push* pMyPush = NULL;
	SG_uint32 countDagnums;
	SG_uint32 i;
	SG_uint32* dagnums;
	SG_bool isDagnumValid = SG_FALSE;
	SG_rbtree* prbLeaves = NULL;

	SG_NULLARGCHECK_RETURN(pPush);
	pMyPush = (_sg_push*)pPush;

	// Verify that the requested dagnum exists
	SG_ERR_CHECK(  SG_repo__list_dags(pCtx, pMyPush->pRepo, &countDagnums, &dagnums)  );
	for (i = 0; i < countDagnums; i++)
	{
		if (dagnums[i] == iDagnum)
		{
			isDagnumValid = SG_TRUE;
			break;
		}
	}
	if (!isDagnumValid)
	{
		char buf[SG_DAGNUM__BUF_MAX__NAME];
		SG_ERR_CHECK(  SG_dagnum__to_name(pCtx, iDagnum, buf, sizeof(buf))  );
		SG_ERR_THROW2(SG_ERR_NO_SUCH_DAG, (pCtx, "%s", buf));
	}


	/* TODO: do we handle duplicate dagnodes in the fragball gracefully? */

	if (prbDagnodes)
	{
		/* Add specified nodes to the initial fragball */
		SG_ERR_CHECK(  SG_fragball__append__dagnodes(pCtx, pMyPush->pFragballPathname, pMyPush->pRepo, iDagnum, prbDagnodes)  );
	}
	else
	{
		/* No specific nodes were provided, so add the leaves */
		SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pMyPush->pRepo, iDagnum, &prbLeaves)  );
		SG_ERR_CHECK(  SG_fragball__append__dagnodes(pCtx, pMyPush->pFragballPathname, pMyPush->pRepo, iDagnum, prbLeaves)  );
	}

	/* fall through */
fail:
	SG_NULLFREE(pCtx, dagnums);
	SG_RBTREE_NULLFREE(pCtx, prbLeaves);

}

void SG_push__commit(
	SG_context* pCtx,
	SG_push** ppPush,
	SG_bool bAllowHeadCountIncrease)
{
	_sg_push* pMyPush = NULL;
	SG_vhash* pvh_status = NULL;

	SG_NULL_PP_CHECK_RETURN(ppPush);
	pMyPush = (_sg_push*)*ppPush;

	/* Push the initial fragball */
	SG_ERR_CHECK(  SG_client__push_add(pCtx, pMyPush->pClient, pMyPush->pClientPushHandle, &pMyPush->pFragballPathname, &pvh_status)  );

#if TRACE_PUSH
	SG_ERR_CHECK(  SG_vhash_debug__dump_to_console__named(pCtx, pvh_status, "push staging status")  );
#endif

	/* Finish the push using the status data we got back. */
	SG_ERR_CHECK(  _push_common(pCtx, pMyPush->pRepo, pMyPush->pClient, &pMyPush->pClientPushHandle, 
		&pvh_status, pMyPush->pTempPathname, bAllowHeadCountIncrease)  );
	
	SG_ERR_CHECK(  _sg_push__nullfree(pCtx, &pMyPush)  );
	*ppPush = NULL;
	SG_VHASH_NULLFREE(pCtx, pvh_status);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh_status);
}

void SG_push__abort(
	SG_context* pCtx,
	SG_push** ppPush)
{
	_sg_push* pMyPush = NULL;

	SG_NULL_PP_CHECK_RETURN(ppPush);
	pMyPush = (_sg_push*)*ppPush;

	SG_ERR_CHECK_RETURN(  SG_client__push_end(pCtx, pMyPush->pClient, &pMyPush->pClientPushHandle)  );
	SG_ERR_CHECK_RETURN(  _sg_push__nullfree(pCtx, &pMyPush)  );
	*ppPush = NULL;
}

void SG_push__all__list_outgoing(SG_context* pCtx, SG_repo* pSrcRepo, SG_client* pClient, SG_varray** ppvaInfo)
{
	SG_pathname* pPath_tempdir = NULL;
	SG_pathname* pPath_fragball = NULL;
	SG_uint32 count_dagnums = 0;
	SG_uint32* paDagNums = NULL;
	SG_uint32 i;
	SG_rbtree* prb_leaves = NULL;
	SG_client_push_handle* pClientPushHandle = NULL;
	SG_vhash* pvh_status = NULL;

	SG_NULLARGCHECK(pSrcRepo);
	SG_NULLARGCHECK(pClient);

	SG_ERR_CHECK(  _push_init(pCtx, pSrcRepo, pClient, &pPath_tempdir, &pPath_fragball, &pClientPushHandle)  );

	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "Sending dagnodes...")  );

	/* For push__all, the initial fragball contains leaves from every dag.  We build it here. */
	SG_ERR_CHECK(  SG_repo__list_dags(pCtx, pSrcRepo, &count_dagnums, &paDagNums)  );

	for (i=0; i<count_dagnums; i++)
	{
		SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pSrcRepo, paDagNums[i], &prb_leaves)  );
		SG_ERR_CHECK(  SG_fragball__append__dagnodes(pCtx, pPath_fragball, pSrcRepo, paDagNums[i], prb_leaves)  );
		SG_RBTREE_NULLFREE(pCtx, prb_leaves);
	}
	SG_NULLFREE(pCtx, paDagNums);
	SG_RBTREE_NULLFREE(pCtx, prb_leaves);

	SG_ERR_CHECK(  SG_client__push_add(pCtx, pClient, pClientPushHandle, &pPath_fragball, &pvh_status)  );

#if TRACE_PUSH
	SG_ERR_CHECK(  SG_vhash_debug__dump_to_console__named(pCtx, pvh_status, "push staging status")  );
#endif

	/* Check the status and use it to send more dagnodes until the dags connect */
	SG_ERR_CHECK(  _add_dagnodes_until_connected(pCtx, &pvh_status, pPath_tempdir, pSrcRepo, pClient, pClientPushHandle)  );
	SG_VHASH_NULLFREE(pCtx, pvh_status);

	/* An add with no fragball does a complete status check */
	SG_ERR_CHECK(  SG_client__push_add(pCtx, pClient, pClientPushHandle, NULL, &pvh_status)  );
	{
		SG_bool b = SG_FALSE;
		SG_vhash* pvhRequest = NULL;
		SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_status, SG_SYNC_STATUS_KEY__NEW_NODES, &b)  );
		if (b)
		{
			/* There are outgoing nodes.  Get their info. */
			SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_status, SG_SYNC_STATUS_KEY__NEW_NODES, &pvhRequest)  );
			SG_ERR_CHECK(  SG_server__get_dagnode_info(pCtx, pSrcRepo, pvhRequest, ppvaInfo)  );
		}
	}

	SG_ERR_CHECK(  SG_client__push_end(pCtx, pClient, &pClientPushHandle)  );

	/* fall through */
fail:
	if (SG_context__has_err(pCtx) && pClientPushHandle)
		SG_ERR_IGNORE(  SG_client__push_end(pCtx, pClient, &pClientPushHandle)  );

	if (pPath_tempdir)
		SG_ERR_IGNORE(  SG_fsobj__rmdir_recursive__pathname(pCtx, pPath_tempdir)  );

	SG_FRAGBALL_NULLFREE(pCtx, pPath_fragball);
	SG_PATHNAME_NULLFREE(pCtx, pPath_tempdir);
	SG_NULLFREE(pCtx, paDagNums);
	SG_RBTREE_NULLFREE(pCtx, prb_leaves);
	SG_VHASH_NULLFREE(pCtx, pvh_status);
}
