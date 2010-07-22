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

/**
 * Recursively compare dagnodes depth-first.
 */
static void _compare_dagnodes(SG_context* pCtx,
							  SG_repo* pRepo1,
							  SG_dagnode* pDagnode1,
							  SG_repo* pRepo2,
							  SG_dagnode* pDagnode2,
							  SG_bool* pbIdentical)
{
	SG_bool bDagnodesEqual = SG_FALSE;
	SG_uint32 iParentCount1, iParentCount2;
	const char** paParentIds1 = NULL;
	const char** paParentIds2 = NULL;
	SG_dagnode* pParentDagnode1 = NULL;
	SG_dagnode* pParentDagnode2 = NULL;

	SG_NULLARGCHECK_RETURN(pDagnode1);
	SG_NULLARGCHECK_RETURN(pDagnode2);
	SG_NULLARGCHECK_RETURN(pbIdentical);

	*pbIdentical = SG_TRUE;

	// Compare the dagnodes.  If they're different, return false.
	SG_ERR_CHECK(  SG_dagnode__equal(pCtx, pDagnode1, pDagnode2, &bDagnodesEqual)  );
	if (!bDagnodesEqual)
	{
#if TRACE_SYNC
		SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR, "dagnodes not equal\n")  );
#endif
		*pbIdentical = SG_FALSE;
		return;
	}

	// The dagnodes are identical.  Look at their parents.
	SG_ERR_CHECK(  SG_dagnode__get_parents(pCtx, pDagnode1, &iParentCount1, &paParentIds1)  );
	SG_ERR_CHECK(  SG_dagnode__get_parents(pCtx, pDagnode2, &iParentCount2, &paParentIds2)  );
	if (iParentCount1 == iParentCount2)
	{
		// The dagnodes have the same number of parents.  Compare the parents recursively.
		SG_uint32 i;
		for (i = 0; i < iParentCount1; i++)
		{
			SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo1, paParentIds1[i], &pParentDagnode1)  );
			SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo2, paParentIds2[i], &pParentDagnode2)  );

			SG_ERR_CHECK(  _compare_dagnodes(pCtx, pRepo1, pParentDagnode1, pRepo2, pParentDagnode2, pbIdentical)  );
			SG_DAGNODE_NULLFREE(pCtx, pParentDagnode1);
			SG_DAGNODE_NULLFREE(pCtx, pParentDagnode2);
			if (!(*pbIdentical))
				break;
		}
	}
	else
	{
		// The dagnodes have a different number of parents.
		*pbIdentical = SG_FALSE;
	}

	// fall through
fail:
	SG_NULLFREE(pCtx, paParentIds1);
	SG_NULLFREE(pCtx, paParentIds2);
	SG_DAGNODE_NULLFREE(pCtx, pParentDagnode1);
	SG_DAGNODE_NULLFREE(pCtx, pParentDagnode2);

}

/**
 * Compare all the nodes of a single DAG in two repos.
 */
static void _compare_one_dag(SG_context* pCtx,
							 SG_repo* pRepo1,
							 SG_repo* pRepo2,
							 SG_uint32 iDagNum,
							 SG_bool* pbIdentical)
{
	SG_bool bFinalResult = SG_FALSE;
	SG_rbtree* prbRepo1Leaves = NULL;
	SG_rbtree* prbRepo2Leaves = NULL;
	SG_uint32 iRepo1LeafCount, iRepo2LeafCount;
	SG_rbtree_iterator* pIterator = NULL;
	const char* pszId = NULL;
	SG_dagnode* pRepo1Dagnode = NULL;
	SG_dagnode* pRepo2Dagnode = NULL;
	SG_bool bFoundRepo1Leaf = SG_FALSE;
	SG_bool bFoundRepo2Leaf = SG_FALSE;
	SG_bool bDagnodesEqual = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pRepo1);
	SG_NULLARGCHECK_RETURN(pRepo2);
	SG_NULLARGCHECK_RETURN(pbIdentical);

	SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo1, iDagNum, &prbRepo1Leaves)  );
	SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo2, iDagNum, &prbRepo2Leaves)  );

	SG_ERR_CHECK(  SG_rbtree__count(pCtx, prbRepo1Leaves, &iRepo1LeafCount)  );
	SG_ERR_CHECK(  SG_rbtree__count(pCtx, prbRepo2Leaves, &iRepo2LeafCount)  );

	if (iRepo1LeafCount != iRepo2LeafCount)
	{
#if TRACE_SYNC
		SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR, "leaf count differs\n")  );
#endif
		goto Different;
	}

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, prbRepo1Leaves, &bFoundRepo1Leaf, &pszId, NULL)  );
	while (bFoundRepo1Leaf)
	{
		SG_ERR_CHECK(  SG_rbtree__find(pCtx, prbRepo2Leaves, pszId, &bFoundRepo2Leaf, NULL)  );
		if (!bFoundRepo2Leaf)
		{
#if TRACE_SYNC && 0
			SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR, "couldn't locate leaf\r\n")  );
			SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR, "Repo 1 leaves:\r\n")  );
			SG_ERR_CHECK(  SG_rbtree_debug__dump_keys_to_console(pCtx, prbRepo1Leaves) );
			SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR, "Repo 2 leaves:\r\n")  );
			SG_ERR_CHECK(  SG_rbtree_debug__dump_keys_to_console(pCtx, prbRepo2Leaves) );
			SG_ERR_CHECK(  SG_console__flush(pCtx, SG_CS_STDERR)  );
#endif
			goto Different;
		}

		SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo1, pszId, &pRepo1Dagnode)  );
		SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo2, pszId, &pRepo2Dagnode)  );

		SG_ERR_CHECK(  _compare_dagnodes(pCtx, pRepo1, pRepo1Dagnode, pRepo2, pRepo2Dagnode, &bDagnodesEqual)  );

		SG_DAGNODE_NULLFREE(pCtx, pRepo1Dagnode);
		SG_DAGNODE_NULLFREE(pCtx, pRepo2Dagnode);

		if (!bDagnodesEqual)
			goto Different;

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &bFoundRepo1Leaf, &pszId, NULL)  );
	}

	bFinalResult = SG_TRUE;

Different:
	*pbIdentical = bFinalResult;

	// fall through
fail:
	SG_RBTREE_NULLFREE(pCtx, prbRepo1Leaves);
	SG_RBTREE_NULLFREE(pCtx, prbRepo2Leaves);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}

void SG_sync__compare_repo_dags(SG_context* pCtx,
								SG_repo* pRepo1,
								SG_repo* pRepo2,
								SG_bool* pbIdentical)
{
	SG_uint32* paRepo1DagNums = NULL;
	SG_uint32* paRepo2DagNums = NULL;
	SG_uint32 iRepo1DagCount, iRepo2DagCount;
	SG_uint32 i;

	SG_NULLARGCHECK_RETURN(pRepo1);
	SG_NULLARGCHECK_RETURN(pRepo2);
	SG_NULLARGCHECK_RETURN(pbIdentical);

	SG_ERR_CHECK(  SG_repo__list_dags(pCtx, pRepo1, &iRepo1DagCount, &paRepo1DagNums)  );
	SG_ERR_CHECK(  SG_repo__list_dags(pCtx, pRepo2, &iRepo2DagCount, &paRepo2DagNums)  );

	if (iRepo1DagCount != iRepo2DagCount)
	{
		SG_NULLFREE(pCtx, paRepo1DagNums);
		SG_NULLFREE(pCtx, paRepo2DagNums);
#if TRACE_SYNC
		SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR, "dag count differs\n")  );
#endif
		*pbIdentical = SG_FALSE;
	}
	else
	{
		for (i = 0; i < iRepo1DagCount; i++)
		{
			SG_ERR_CHECK(  _compare_one_dag(pCtx, pRepo1, pRepo2, paRepo1DagNums[i], pbIdentical)  );
			if (!(*pbIdentical))
				break;
		}
	}

	// fall through
fail:
	SG_NULLFREE(pCtx, paRepo1DagNums);
	SG_NULLFREE(pCtx, paRepo2DagNums);
}

void SG_sync__compare_repo_blobs(SG_context* pCtx,
								 SG_repo* pRepo1,
								 SG_repo* pRepo2,
								 SG_bool* pbIdentical)
{
	const SG_uint32 chunk_size = 1000;
	SG_vhash* pvh = NULL;
	const char* pszBlob1 = NULL;
	SG_uint32 i, j;
	SG_uint32 count_observed = 0;
	SG_uint32 count_returned;

	SG_NULLARGCHECK_RETURN(pRepo1);
	SG_NULLARGCHECK_RETURN(pRepo2);
	SG_NULLARGCHECK_RETURN(pbIdentical);

	for(i = 0; SG_TRUE; i++)
	{
		// Ian TODO: other encodings
		SG_ERR_CHECK(  SG_repo__list_blobs(pCtx, pRepo1, SG_BLOBENCODING__ZLIB, SG_FALSE, SG_FALSE, chunk_size, chunk_size * i, &pvh)  );

		for (j = 0; j < chunk_size; j++)
		{
			SG_bool b = SG_TRUE;

			SG_vhash__get_nth_pair(pCtx, pvh, j, &pszBlob1, NULL);
			if (SG_context__err_equals(pCtx, SG_ERR_ARGUMENT_OUT_OF_RANGE))
			{
				SG_context__err_reset(pCtx);
				break;
			}
			SG_ERR_CHECK_CURRENT;

			SG_ERR_CHECK(  SG_repo__does_blob_exist(pCtx, pRepo2, pszBlob1, &b)  );
			if (!b)
			{
				*pbIdentical = SG_FALSE;
				break;
			}
			count_observed++;
		}
		if (!j)
			break;

		SG_VHASH_NULLFREE(pCtx, pvh);
	}

	SG_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo2, NULL, NULL, &count_returned, NULL, NULL, NULL, NULL, NULL, NULL, NULL)  );
	if (count_returned != count_observed)
		*pbIdentical = SG_FALSE;

	// fall through
fail:
	SG_VHASH_NULLFREE(pCtx, pvh);
}

typedef struct
{
	const char* pszStartNodeHid;
	SG_int32 genLimit;
	SG_rbtree* prbVisitedNodes;
} _dagwalk_data;

static void _dagwalk_callback(SG_context* pCtx,
							  SG_UNUSED_PARAM(SG_repo* pRepo),
							  void* pData,
							  SG_dagnode* pCurrentNode,
							  SG_UNUSED_PARAM(SG_rbtree* pDagnodeCache),
							  SG_bool* pbContinue)
{
	_dagwalk_data* pDagWalkData = (_dagwalk_data*)pData;
	const char* pszCurrentNodeHid = NULL;
	SG_int32 genCurrentNode;

	SG_UNUSED(pRepo);
	SG_UNUSED(pDagnodeCache);

	SG_ERR_CHECK_RETURN(  SG_dagnode__get_generation(pCtx, pCurrentNode, &genCurrentNode)  );
	if (genCurrentNode < pDagWalkData->genLimit)
	{
		*pbContinue = SG_FALSE;
		return;
	}

	SG_ERR_CHECK_RETURN(  SG_dagnode__get_id_ref(pCtx, (const SG_dagnode*)pCurrentNode, &pszCurrentNodeHid)  );
	if (!strcmp(pDagWalkData->pszStartNodeHid, (const char*)pszCurrentNodeHid))
		return;

	SG_ERR_CHECK_RETURN(  SG_rbtree__update(pCtx, pDagWalkData->prbVisitedNodes, (const char*)pszCurrentNodeHid)  );

	// TODO: Stop walking when this node and all it siblings are already in prbVisitedNodes?
}

void SG_sync__add_n_generations(SG_context* pCtx,
										   SG_repo* pRepo,
										   const char* pszDagnodeHid,
										   SG_rbtree* prbDagnodeHids,
										   SG_uint32 generations)
{
	_dagwalk_data dagWalkData;
	SG_dagnode* pStartNode = NULL;
	SG_int32 startGen;

	dagWalkData.pszStartNodeHid = pszDagnodeHid;
	SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, pszDagnodeHid, &pStartNode)  );
	SG_ERR_CHECK(  SG_dagnode__get_generation(pCtx, pStartNode, &startGen)  );
	dagWalkData.genLimit = startGen - generations;
	dagWalkData.prbVisitedNodes = prbDagnodeHids;

	SG_ERR_CHECK(  SG_dagwalker__walk_dag_single(pCtx, pRepo, pszDagnodeHid, _dagwalk_callback, &dagWalkData)  );

	/* fall through */
fail:
	SG_DAGNODE_NULLFREE(pCtx, pStartNode);
}

void SG_sync__add_blobs_to_fragball(SG_context* pCtx, SG_repo* pRepo, SG_pathname* pPath_fragball, SG_vhash* pvh_missing_blobs)
{
	SG_uint32 iMissingBlobCount;
	const char** paszHids = NULL;

	SG_NULLARGCHECK_RETURN(pPath_fragball);
	SG_NULLARGCHECK_RETURN(pvh_missing_blobs);

	SG_ERR_CHECK_RETURN(  SG_vhash__count(pCtx, pvh_missing_blobs, &iMissingBlobCount)  );
	if (iMissingBlobCount > 0)
	{
		SG_uint32 i;
		SG_ERR_CHECK(  SG_allocN(pCtx, iMissingBlobCount, paszHids)  );
		for (i = 0; i < iMissingBlobCount; i++)
			SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_missing_blobs, i, &paszHids[i], NULL)  );
	}

	SG_ERR_CHECK(  SG_fragball__append__blobs(pCtx, pPath_fragball, pRepo, paszHids, iMissingBlobCount)  );

	/* fall through */
fail:
	SG_NULLFREE(pCtx, paszHids);
}
