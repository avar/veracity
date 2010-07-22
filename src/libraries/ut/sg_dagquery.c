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
 * @file sg_dagquery.c
 *
 * @details An odd collection of routines to ask some hard DAG relationship questions.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

void SG_dagquery__how_are_dagnodes_related(SG_context * pCtx,
										   SG_repo * pRepo,
										   const char * pszHid1,
										   const char * pszHid2,
										   SG_dagquery_relationship * pdqRel)
{
	SG_changeset * pcs1 = NULL;
	SG_changeset * pcs2 = NULL;
	SG_dagfrag * pFrag = NULL;
	SG_dagquery_relationship dqRel = SG_DAGQUERY_RELATIONSHIP__UNKNOWN;
	SG_uint32 iDagNum1, iDagNum2;
	SG_int32 gen1, gen2;
	SG_bool bFound;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NONEMPTYCHECK_RETURN(pszHid1);
	SG_NONEMPTYCHECK_RETURN(pszHid2);
	SG_NULLARGCHECK_RETURN(pdqRel);

	if (strcmp(pszHid1, pszHid2) == 0)
	{
		dqRel = SG_DAGQUERY_RELATIONSHIP__SAME;
		goto cleanup;
	}

	// fetch the changeset for both HIDs.  this throws when the HID is not found.
	// we get the changeset rather than the dagnode because we need to check the
	// dagnum and that field is not present in the dagnode.

	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, pszHid1, &pcs1)  );
	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, pszHid2, &pcs2)  );

	//////////////////////////////////////////////////////////////////
	// TODO when we have named-branches, we should put something here
	// TODO to see if they are on different branches and quit.
	//////////////////////////////////////////////////////////////////

	// we say that 2 nodes are either:
	// [1] ancestor/descendant of each other;
	// [2] or that they are peers (cousins) of each other (no matter
	//     how distant in the DAG).  (that have an LCA, but we don't
	//     care about it.)
	// [3] unrelated because they are in different DAGNUMs; this should
	//     not happen.

	SG_ERR_CHECK(  SG_changeset__get_dagnum(pCtx, pcs1, &iDagNum1)  );
	SG_ERR_CHECK(  SG_changeset__get_dagnum(pCtx, pcs2, &iDagNum2)  );
	if (iDagNum1 != iDagNum2)
	{
		dqRel = SG_DAGQUERY_RELATIONSHIP__UNRELATED;
		goto cleanup;
	}

	// get the generation of both dagnodes.  if they are the same, then they
	// cannot have an ancestor/descendant relationship and therefore must be
	// peers/cousins (we don't care how close/distant they are).

	SG_ERR_CHECK(  SG_changeset__get_generation(pCtx, pcs1, &gen1)  );
	SG_ERR_CHECK(  SG_changeset__get_generation(pCtx, pcs2, &gen2)  );
	if (gen1 == gen2)
	{
		dqRel = SG_DAGQUERY_RELATIONSHIP__PEER;
		goto cleanup;
	}

	// see if one is an ancestor of the other.  since we only have PARENT
	// edges in our DAG, we start with the deeper one and walk backwards
	// until we've visited all ancestors at the depth of the shallower one.
	//
	// i'm going to be lazy here and not reinvent a recursive-ish parent-edge
	// graph walker.  instead, i'm going to create a DAGFRAG using the
	// deeper one and request the generation difference as the "thickness".
	// in theory, if we have an ancestor/descendant relationship, the
	// shallower one should be in the END-FRINGE of the DAGFRAG.
	//
	// i'm going to pick an arbitrary direction "cs1 is R of cs2".

	SG_ERR_CHECK(  SG_dagfrag__alloc_transient(pCtx, &pFrag)  );
	if (gen1 > gen2)
	{
		SG_ERR_CHECK(  SG_dagfrag__load_from_repo__one(pCtx, pFrag, pRepo, pszHid1, (gen1 - gen2))  );
		SG_ERR_CHECK(  SG_dagfrag__query(pCtx, pFrag, pszHid2, NULL, NULL, &bFound, NULL)  );

		if (bFound)			// pszHid2 is an ancestor of pszHid1.  READ pszHid1 is a descendent of pszHid2.
			dqRel = SG_DAGQUERY_RELATIONSHIP__DESCENDANT;
		else				// they are *distant* peers.
			dqRel = SG_DAGQUERY_RELATIONSHIP__PEER;
		goto cleanup;
	}
	else
	{
		SG_ERR_CHECK(  SG_dagfrag__load_from_repo__one(pCtx, pFrag, pRepo, pszHid2, (gen2 - gen1))  );
		SG_ERR_CHECK(  SG_dagfrag__query(pCtx, pFrag, pszHid1, NULL, NULL, &bFound, NULL)  );

		if (bFound)			// pszHid1 is an ancestor of pszHid2.
			dqRel = SG_DAGQUERY_RELATIONSHIP__ANCESTOR;
		else				// they are *distant* peers.
			dqRel = SG_DAGQUERY_RELATIONSHIP__PEER;
		goto cleanup;
	}

	/*NOTREACHED*/

cleanup:
	*pdqRel = dqRel;

fail:
	SG_CHANGESET_NULLFREE(pCtx, pcs1);
	SG_CHANGESET_NULLFREE(pCtx, pcs2);
	SG_DAGFRAG_NULLFREE(pCtx, pFrag);
}

//////////////////////////////////////////////////////////////////

void SG_dagquery__find_descendant_heads(SG_context * pCtx,
										SG_repo * pRepo,
										const char * pszHidStart,
										SG_bool bStopIfMultiple,
										SG_dagquery_find_head_status * pdqfhs,
										SG_rbtree ** pprbHeads)
{
	SG_changeset * pcsStart = NULL;
	SG_rbtree * prbLeaves = NULL;
	SG_rbtree * prbHeadsFound = NULL;
	SG_rbtree_iterator * pIter = NULL;
	const char * pszKey_k;
	SG_uint32 iDagNum;
	SG_bool b;
	SG_dagquery_find_head_status dqfhs;
	SG_dagquery_relationship dqRel;
	SG_uint32 nrFound;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NONEMPTYCHECK_RETURN(pszHidStart);
	SG_NULLARGCHECK_RETURN(pdqfhs);
	SG_NULLARGCHECK_RETURN(pprbHeads);

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prbHeadsFound)  );

	// we fetch the CHANGESET rather than the DAGNODE for the starting point
	// because we need the DAGNUM to fetch the leaves from the repo.

	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, pszHidStart, &pcsStart)  );
	SG_ERR_CHECK(  SG_changeset__get_dagnum(pCtx, pcsStart, &iDagNum)  );

	// fetch a list of all of the LEAVES in the DAG.
	// this rbtree only contains keys; no assoc values.

	SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, iDagNum, &prbLeaves)  );

	// if the starting point is a leaf, then we are done (we don't care how many
	// other leaves are in the rbtree because none will be a child of ours because
	// we are a leaf).

	SG_ERR_CHECK(  SG_rbtree__find(pCtx, prbLeaves, pszHidStart, &b, NULL)  );
	if (b)
	{
		SG_ERR_CHECK(  SG_rbtree__add(pCtx, prbHeadsFound, pszHidStart)  );

		dqfhs = SG_DAGQUERY_FIND_HEAD_STATUS__IS_LEAF;
		goto done;
	}

	// inspect each leaf and qualify it; put the ones that pass
	// into the list of actual heads.

	nrFound = 0;
	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, prbLeaves, &b, &pszKey_k, NULL)  );
	while (b)
	{
		SG_ERR_CHECK(  SG_dagquery__how_are_dagnodes_related(pCtx, pRepo, pszKey_k, pszHidStart, &dqRel)  );

		if (dqRel == SG_DAGQUERY_RELATIONSHIP__DESCENDANT)
		{
			nrFound++;

			if (bStopIfMultiple && (nrFound > 1))
			{
				// they wanted a unique answer and we've found too many answers
				// (which they won't be able to use anyway) so just stop and
				// return the status.  (we delete prbHeadsFound because it is
				// incomplete and so that they won't be tempted to use it.)

				SG_RBTREE_NULLFREE(pCtx, prbHeadsFound);
				dqfhs = SG_DAGQUERY_FIND_HEAD_STATUS__MULTIPLE;
				goto done;
			}

			SG_ERR_CHECK(  SG_rbtree__add(pCtx, prbHeadsFound, pszKey_k)  );
		}

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter, &b, &pszKey_k, NULL)  );
	}

	switch (nrFound)
	{
	case 0:
		// this should NEVER happen.  we should always be able to find a
		// leaf/head for a node.
		//
		// TODO the only case where this might happen is if named branches
		// TODO cause the leaf to be disqualified.  so i'm going to THROW
		// TODO here rather than ASSERT.

		SG_ERR_THROW2(  SG_ERR_DAG_NOT_CONSISTENT,
						(pCtx, "Could not find head/leaf for changeset [%s]", pszHidStart)  );
		break;

	case 1:
		dqfhs = SG_DAGQUERY_FIND_HEAD_STATUS__UNIQUE;
		break;

	default:
		dqfhs = SG_DAGQUERY_FIND_HEAD_STATUS__MULTIPLE;
		break;
	}

done:
	*pprbHeads = prbHeadsFound;
	prbHeadsFound = NULL;

	*pdqfhs = dqfhs;

fail:
	SG_CHANGESET_NULLFREE(pCtx, pcsStart);
	SG_RBTREE_NULLFREE(pCtx, prbLeaves);
	SG_RBTREE_NULLFREE(pCtx, prbHeadsFound);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}

//////////////////////////////////////////////////////////////////

void SG_dagquery__find_single_descendant_head(SG_context * pCtx,
											  SG_repo * pRepo,
											  const char * pszHidStart,
											  SG_dagquery_find_head_status * pdqfhs,
											  char * bufHidHead,
											  SG_uint32 lenBufHidHead)
{
	SG_rbtree * prbHeads = NULL;
	const char * pszKey_0;
	SG_dagquery_find_head_status dqfhs;
	SG_bool b;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NONEMPTYCHECK_RETURN(pszHidStart);
	SG_NULLARGCHECK_RETURN(pdqfhs);
	SG_NULLARGCHECK_RETURN(bufHidHead);

	SG_ERR_CHECK(  SG_dagquery__find_descendant_heads(pCtx, pRepo, pszHidStart, SG_TRUE, &dqfhs, &prbHeads)  );

	switch (dqfhs)
	{
	case SG_DAGQUERY_FIND_HEAD_STATUS__IS_LEAF:
	case SG_DAGQUERY_FIND_HEAD_STATUS__UNIQUE:
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, NULL, prbHeads, &b, &pszKey_0, NULL)  );
		SG_ASSERT(  (b)  );
		SG_ERR_CHECK(  SG_strcpy(pCtx, bufHidHead, lenBufHidHead, pszKey_0)  );
		break;

	case SG_DAGQUERY_FIND_HEAD_STATUS__MULTIPLE:
	default:
		// don't throw, just return the status
		bufHidHead[0] = 0;
		break;
	}

	*pdqfhs = dqfhs;

fail:
	SG_RBTREE_NULLFREE(pCtx, prbHeads);
}
