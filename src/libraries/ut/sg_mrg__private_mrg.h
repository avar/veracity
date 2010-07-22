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
 * @file sg_mrg__private_mrg.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_MRG_H
#define H_SG_MRG__PRIVATE_MRG_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_mrg__free(SG_context * pCtx, SG_mrg * pMrg)
{
	if (!pMrg)
		return;

	// we do not own pMrg->pPendingTree

	SG_TREEDIFF2_NULLFREE(pCtx, pMrg->pTreeDiff_Baseline_WD_Dirt);

	// we only delete the TEMP dir pathname (if set) and leave the directory on the disk.
	// if you want to delete the directory, call SG_mrg__rmdir_temp_dir() before calling us.
	SG_PATHNAME_NULLFREE(pCtx, pMrg->pPathOurTempDir);

	SG_STRING_NULLFREE(pCtx, pMrg->pStrRepoDescriptorName);
	SG_NULLFREE(pCtx, pMrg->pszGid_anchor_directory);
	SG_VHASH_NULLFREE(pCtx, pMrg->pvhRepoDescriptor);

	SG_RBTREE_NULLFREE(pCtx,pMrg->prbCSets_OtherLeaves);
	SG_DAGLCA_NULLFREE(pCtx,pMrg->pDagLca);
	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx,pMrg->prbCSets,(SG_free_callback *)SG_mrg_cset__free);
	// we do not own pMrgCSet_LCA
	// we do not own pMrgCSet_Baseline

	SG_MRG_CSET_NULLFREE(pCtx,pMrg->pMrgCSet_FinalResult);
	SG_MRG_CSET_STATS_NULLFREE(pCtx,pMrg->pMrgCSetStats_FinalResult);

	SG_INV_DIRS_NULLFREE(pCtx, pMrg->pInvDirs);
	SG_WD_PLAN_NULLFREE(pCtx, pMrg->p_wd_plan);
	SG_VARRAY_NULLFREE(pCtx, pMrg->pvaIssues);

	SG_NULLFREE(pCtx, pMrg);
}

/**
 * Allocate a SG_mrg object and prepare to start computing a MERGE.
 * We require a PendingTree (which must reflect a "mostly" clean WD)
 * so that we can:
 * [] find everything on disk,
 * [] hold the required locks as we munge things in the WD,
 * [] and update the pendingtree with the candidate merge results.
 */
void SG_mrg__alloc(SG_context * pCtx,
				   SG_pendingtree * pPendingTree,
				   SG_bool bComplainIfBaselineNotLeaf,
				   SG_mrg ** ppMrg_New)
{
	SG_mrg * pMrg = NULL;
	SG_rbtree * prbAllLeaves = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(ppMrg_New);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx,pMrg)  );
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx,&pMrg->prbCSets)  );

	// We need to keep the pendingtree alive (and the lock on disk held) for the duration.
	// This also means we can borrow the pPathWorkingDirectoryTop and pRepo values inside
	// the pendingtree and not have to have/open our own copy.

	pMrg->pPendingTree = pPendingTree;

	// Get the VArray of HIDs of the parent CSet(s) of the WD from the pendingtree.
	// This was formerly called just the "baseline".  We require that the WD currently
	// only have one parent; that is, to not contain an uncommitted merge.
	//
	// since we can only have 1 parent, it is by definition our baseline.
	// during the actual merge (when we alter the WD and make changes to the
	// pendingtree) those changes will be relative to the baseline, so parent[0]
	// becomes special.

	{
		const SG_varray * pva_wd_parents;
		const char * psz_hid_wd_parent_0;
		SG_uint32 nrParents;

		SG_ERR_CHECK(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pva_wd_parents)  );
		SG_ERR_CHECK(  SG_varray__count(pCtx, pva_wd_parents, &nrParents)  );
		if (nrParents > 1)
		{
			SG_ERR_THROW2(  SG_ERR_CANNOT_DO_WHILE_UNCOMMITTED_MERGE,
							(pCtx, "Working directory already contains an uncommitted merge."));
		}
		SG_ASSERT(  (nrParents == 1)  );
		SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_wd_parents, 0, &psz_hid_wd_parent_0)  );
		SG_ERR_CHECK(  SG_strcpy(pCtx,pMrg->bufHid_CSet_Baseline,SG_NrElements(pMrg->bufHid_CSet_Baseline),psz_hid_wd_parent_0)  );
	}

	if (bComplainIfBaselineNotLeaf)
	{
		SG_repo * pRepo;
		SG_bool bBaselineIsLeaf;

		SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

		// DO NOT filter this using named branches.  This list should be a complete list
		// of all leaves/heads.

		SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, &prbAllLeaves)  );
		SG_ERR_CHECK(  SG_rbtree__find(pCtx, prbAllLeaves, pMrg->bufHid_CSet_Baseline, &bBaselineIsLeaf, NULL)  );
		if (!bBaselineIsLeaf)
			SG_ERR_THROW(  SG_ERR_BASELINE_NOT_HEAD  );
		SG_RBTREE_NULLFREE(pCtx, prbAllLeaves);
	}

	// We now allow each REPO to specify the Hash Algorithm when it is created.
	// This means that the length of a HID is not a compile-time constant anymore;
	// However, all of the HIDs in a REPO will have the same length.  (And we only
	// need to know this length for a "%*s" format.)

	pMrg->nrHexDigitsInHIDs = strlen(pMrg->bufHid_CSet_Baseline);

	//////////////////////////////////////////////////////////////////
	// Ask the pending-tree to DIFF/SCAN the WD with the relative to the BASELINE and
	// see if the WD is dirty.  NOTE: This causes side-effects on the PTNODES (which
	// we fix by re-reading the wd.json (all while holding the file lock)), so some
	// pendingtree and ptnode pointers (and pva_wd_parents) may have been reallocated,
	// so don't cache any pointers around this.
	// 
	// If there is dirt that we can handle, we keep this around until the apply phase.
	// This throws if there is dirt we can't handle.

	SG_ERR_CHECK(  _mrg__look_for_dirt(pCtx, pMrg)  );

	*ppMrg_New = pMrg;
	return;

fail:
	SG_MRG_NULLFREE(pCtx, pMrg);
	SG_RBTREE_NULLFREE(pCtx, prbAllLeaves);
}

//////////////////////////////////////////////////////////////////

struct _load_cset_data
{
	SG_mrg *		pMrg;

	SG_uint32		kSPCA;
	SG_uint32		kLeaves;
};

typedef struct _load_cset_data _load_cset_data;

//////////////////////////////////////////////////////////////////

static SG_daglca__foreach_callback _sg_mrg__daglca_callback__load_cset;

static void _sg_mrg__daglca_callback__load_cset(SG_context * pCtx,
												const char * pszHid_CSet,
												SG_daglca_node_type nodeType,
												SG_UNUSED_PARAM(SG_int32 generation),
												SG_rbtree * prbImmediateDescendants,
												void * pVoidCallerData)
{
	// Each CSET in the DAGLCA graph needs to be loaded into memory.
	// This allows us to do arbitrary compares between the different
	// versions of the tree (guided by the DAGLCA graph).

	_load_cset_data * pLoadCSetData = (_load_cset_data *)pVoidCallerData;
	SG_mrg_cset * pMrgCSet_Allocated = NULL;
	SG_mrg_cset * pMrgCSet;
	SG_repo * pRepo;
	char bufLabel[100];
	SG_bool bIsBaseline;

	SG_UNUSED(generation);

	bIsBaseline = (strcmp(pszHid_CSet,pLoadCSetData->pMrg->bufHid_CSet_Baseline) == 0);

	// create a nice "label" for identifying the cset.  this is useful for debugging and when
	// we need to write various versions of a file to the temp directory and use an external
	// merge tool.  for example, we can say:
	//      "diff3 foo.L0  foo.A  foo.L1  >  foo.M"
	// or:  "diff3 L0/foo  A/foo  L1/foo  >  M/foo"
	// and not have to use GID/HIDs in the filenames.

	switch (nodeType)
	{
	case SG_DAGLCA_NODE_TYPE__LEAF:
		if (bIsBaseline)
			SG_ERR_CHECK(  SG_sprintf(pCtx,bufLabel,SG_NrElements(bufLabel),"L0")  );
		else
			SG_ERR_CHECK(  SG_sprintf(pCtx,bufLabel,SG_NrElements(bufLabel),"L%d",pLoadCSetData->kLeaves++)  );
		break;

	case SG_DAGLCA_NODE_TYPE__SPCA:
		SG_ERR_CHECK(  SG_sprintf(pCtx,bufLabel,SG_NrElements(bufLabel),"a%d",pLoadCSetData->kSPCA++)  );
		break;

	case SG_DAGLCA_NODE_TYPE__LCA:
		SG_ERR_CHECK(  SG_sprintf(pCtx,bufLabel,SG_NrElements(bufLabel),"A")  );	// there can only be one LCA
		break;

	default:
		SG_ASSERT( (0) );
		break;
	}

	SG_ERR_CHECK_RETURN(  SG_mrg_cset__alloc(pCtx,
											 bufLabel,
											 SG_MRG_CSET_ORIGIN__LOADED,
											 nodeType,
											 &pMrgCSet_Allocated)  );
	SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx,
											  pLoadCSetData->pMrg->prbCSets,
											  pszHid_CSet,
											  (void *)pMrgCSet_Allocated)  );
	pMrgCSet = pMrgCSet_Allocated;			// rbtree owns it now
	pMrgCSet_Allocated = NULL;

	// cache the pointers to the CSET for the LCA and the Baseline.

	if (nodeType == SG_DAGLCA_NODE_TYPE__LCA)
		pLoadCSetData->pMrg->pMrgCSet_LCA = pMrgCSet;
	else if (bIsBaseline)
		pLoadCSetData->pMrg->pMrgCSet_Baseline = pMrgCSet;

	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pLoadCSetData->pMrg->pPendingTree, &pRepo)  );
	SG_ERR_CHECK(  SG_mrg_cset__load_entire_cset(pCtx,
												 pMrgCSet,
												 pRepo,
												 pszHid_CSet)  );

	// TODO use the prbImmediateDescendants to compute a plan for performing merge.

fail:
	SG_RBTREE_NULLFREE(pCtx, prbImmediateDescendants);
	SG_MRG_CSET_NULLFREE(pCtx, pMrgCSet_Allocated);
}

//////////////////////////////////////////////////////////////////

/**
 * Compute the MERGE using the given CSET and the BASELINE.
 * The LCA is computed and used as the Ancestor.  This builds
 * the merge-result in-memory and *DOES NOT* affect the WD.
 */
void SG_mrg__compute_merge_on_cset_with_baseline(SG_context * pCtx,
												 SG_mrg * pMrg,
												 const char * pszHid_CSet_Other)
{
	SG_rbtree * prb_temp = NULL;
	SG_mrg_cset * pMrgCSet_Other;
	SG_repo * pRepo;
	SG_uint32 nrLCA, nrSPCA, nrLeaves;
	SG_bool bEqualCSets;
	SG_bool bFound;
	_load_cset_data load_cset_data;

	SG_NULLARGCHECK_RETURN(pMrg);
	SG_NONEMPTYCHECK_RETURN(pszHid_CSet_Other);

	// If they gave us the HID of the baseline, we stop -- it doesn't make any
	// sense to merge something with itself.

	bEqualCSets = (0 == strcmp(pMrg->bufHid_CSet_Baseline,pszHid_CSet_Other));
	if (bEqualCSets)
		SG_ERR_THROW(  SG_ERR_MERGE_REQUIRES_UNIQUE_CSETS  );

	// we were asked to merge(hid_cset_other,hid_cset_baseline)
	// compute the LCA from the DAG.

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_temp)  );
	SG_ERR_CHECK(  SG_rbtree__add(pCtx,prb_temp,pszHid_CSet_Other)  );
	SG_ERR_CHECK(  SG_rbtree__add(pCtx,prb_temp,pMrg->bufHid_CSet_Baseline)  );
	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pMrg->pPendingTree, &pRepo)  );
	SG_ERR_CHECK(  SG_repo__get_dag_lca(pCtx,pRepo,SG_DAGNUM__VERSION_CONTROL,prb_temp,&pMrg->pDagLca)  );

	SG_ERR_CHECK(  SG_daglca__get_stats(pCtx,pMrg->pDagLca,&nrLCA,&nrSPCA,&nrLeaves)  );
	SG_ASSERT(  nrLCA == 1  );		// for a simple cset-vs-baseline, we must have 1 LCA and 2 Leaves.
	SG_ASSERT(  nrLeaves == 2  );	// the number of SPCAs is a different matter and handled below.

	memset(&load_cset_data,0,sizeof(load_cset_data));
	load_cset_data.pMrg = pMrg;
	load_cset_data.kLeaves = 1;	// force required baseline to be L0 and label others starting with L1.

	// load the complete version control tree of all of the CSETs (leaves, SPCAs, and the LCA)
	// into the CSET-RBTREE.  This converts the treenodes and treenode-entries into sg_mrg_cset_entry's
	// and builds the flat entry-list in each CSET.

	SG_ERR_CHECK(  SG_daglca__foreach(pCtx,pMrg->pDagLca,SG_TRUE,_sg_mrg__daglca_callback__load_cset,&load_cset_data)  );

	SG_ASSERT(  (pMrg->pMrgCSet_LCA)  );
	SG_ASSERT(  (pMrg->pMrgCSet_Baseline)  );

	SG_ERR_CHECK(  SG_rbtree__find(pCtx,pMrg->prbCSets,pszHid_CSet_Other,&bFound,(void **)&pMrgCSet_Other)  );
	SG_ASSERT(  bFound  );

	// TODO 2010/05/14 For now, pretend that we don't have any SPCAs.  In a
	// TODO            2-way merge, the only way to have SPCAs is to have
	// TODO            criss-crosses.  We are going to ignore these for now.
	// TODO            And since it only affects the tightness and/or ancestor
	// TODO            that we choose when doing an auto-merge on an individual
	// TODO            file, we should be fine using the absolute LCA (although
	// TODO            the auto-merge may be more likely to find conflicts and/or
	// TODO            require the user to re-merge things that we could better
	// TODO            handle if we tried a little harder here.  But to do that
	// TODO            I need to add per-file LCA computations....

	//
	//                  A
	//                 / \.
	//               L0   L1
	//                 \ /
	//                  M

	SG_ERR_CHECK(  SG_mrg__compute_simple2(pCtx,pMrg,"M",
										   pMrg->pMrgCSet_LCA,
										   pMrg->pMrgCSet_Baseline,
										   pMrgCSet_Other,
										   &pMrg->pMrgCSet_FinalResult,
										   &pMrg->pMrgCSetStats_FinalResult)  );

fail:
	SG_RBTREE_NULLFREE(pCtx, prb_temp);
}

//////////////////////////////////////////////////////////////////

#if 0	// not ready yet
/**
 * Compute the MERGE using the set of CSETs and the Baseline.
 * This is similar to OCTOPUS merge in GIT.
 *
 * We *DO NOT* alter the WD; we only compute the in-memory merge-result.
 */
void SG_mrg__compute_n_way_merge_with_baseline(SG_context * pCtx, SG_mrg * pMrg, const SG_rbtree * prbSet)
{
	SG_rbtree * prb_temp = NULL;
	SG_repo * pRepo;

	SG_NULLARGCHECK_RETURN(pMrg);
	SG_NULLARGCHECK_RETURN(prbSet);

	// create a private copy of the rbtree of CSETs.
	// add the HID of the Baseline to our copy.
	// the caller's set SHOULD NOT have the baseline in it.

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_temp)  );
	SG_ERR_CHECK(  SG_rbtree__add__from_other_rbtree(pCtx,prb_temp,prbSet)  );
	SG_ERR_CHECK(  SG_rbtree__find(pCtx,prb_temp,pMrg->bufHid_CSet_Baseline,&bBaselineAlreadyInSet,NULL)  );
	SG_ASSERT(  (!bBaselineAlreadyInSet)  );
	SG_ERR_CHECK(  SG_rbtree__add(pCtx,prb_temp,pMrg->bufHid_CSet_Baseline)  );

	// compute the LCA from the DAG using the augmented set of leaves.
	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pMrg->pPendingTree, &pRepo)  );
	SG_ERR_CHECK(  SG_repo__get_dag_lca(pCtx,pRepo,SG_DAGNUM__VERSION_CONTROL,prb_temp,&pMrg->pDagLca)  );
	SG_RBTREE_NULLFREE(pCtx, prb_temp);

	SG_ERR_CHECK(  SG_daglca__get_stats(pCtx,pMrg->pDagLca,&nrLCA,&nrSPCA,&nrLeaves)  );
	SG_ASSERT(  nrLCA == 1  );
	SG_ASSERT(  nrLeaves >= 2  );

	memset(&load_cset_data,0,sizeof(load_cset_data));
	load_cset_data.pMrg = pMrg;

	// load the complete version control tree of all of the CSETs (leaves, SPCAs, and the LCA)
	// into the CSET-RBTREE.  This converts the treenodes and treenode-entries into sg_mrg_cset_entry's
	// and builds the flat entry-list in each CSET.

	SG_ERR_CHECK(  SG_daglca__foreach(pCtx,pMrg->pDagLca,SG_TRUE,_sg_mrg__daglca_callback__load_cset,&load_cset_data)  );

	SG_ASSERT(  (pMrg->pMrgCSet_LCA)  );
	SG_ASSERT(  (pMrg->pMrgCSet_Baseline)  );

	// TODO to be continued.....
}
#endif

//////////////////////////////////////////////////////////////////

void SG_mrg__compute_merge(SG_context * pCtx,
						   SG_mrg * pMrg,
						   const SG_rbtree * prbCSetsToMerge,
						   SG_uint32 * pNrUnresolvedIssues)
{
	SG_rbtree_iterator * pIter = NULL;
	const char * pszHid;
	SG_uint32 nrOtherLeaves;
	SG_bool bFound;
	SG_bool bOK;

	SG_NULLARGCHECK_RETURN(pMrg);
	// prbCSetsToMerge is optional
	// pNrUnresolvedIssues is optional

	if (!prbCSetsToMerge)
	{
		// silently supply the list of heads/leaves (sans the baseline).
		//
		// TODO 2010/07/06 we need to add "named branches" so that we can distinguish
		// TODO            between the short-lived (this morning's work) branches and
		// TODO            the long-lived (dev vs maint) branches.  We should use that
		// TODO            to only get the short-lived branches associated with the
		// TODO            baseline.

		SG_repo * pRepo;

		SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pMrg->pPendingTree, &pRepo)  );
		SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, &pMrg->prbCSets_OtherLeaves)  );
		SG_ERR_CHECK(  SG_rbtree__find(pCtx, pMrg->prbCSets_OtherLeaves, pMrg->bufHid_CSet_Baseline, &bFound, NULL)  );
		if (bFound)
			SG_ERR_CHECK(  SG_rbtree__remove(pCtx, pMrg->prbCSets_OtherLeaves, pMrg->bufHid_CSet_Baseline)  );
	}
	else
	{
		// Copy all of the HIDs in prbCSetsToMerge (except for the baseline)
		// into the pMrg->prbCSets_OtherLeaves.

		SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pMrg->prbCSets_OtherLeaves)  );
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, prbCSetsToMerge, &bOK, &pszHid, NULL)  );
		while (bOK)
		{
			if (strcmp(pszHid, pMrg->bufHid_CSet_Baseline) != 0)
				SG_ERR_CHECK(  SG_rbtree__add(pCtx, pMrg->prbCSets_OtherLeaves, pszHid)  );
			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter, &bOK, &pszHid, NULL)  );
		}
	}

	// prbCSetsToMerge has all of the HIDs that we want to merge *EXCEPT* for
	// the baseline, which is implied.

	SG_ERR_CHECK(  SG_rbtree__count(pCtx, pMrg->prbCSets_OtherLeaves, &nrOtherLeaves)  );
	switch (nrOtherLeaves)
	{
	case 0:
		SG_ERR_THROW(  SG_ERR_MERGE_NEEDS_ANOTHER_CSET  );

	case 1:
		{
			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,NULL,pMrg->prbCSets_OtherLeaves,&bFound,&pszHid,NULL)  );
			SG_ASSERT(  (bFound)  );
			SG_ERR_CHECK(  SG_mrg__compute_merge_on_cset_with_baseline(pCtx,pMrg,pszHid)  );
		}
		break;

	default:
		// TODO finish __compute_n_way_merge_with_baseline()
		SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
	}

	SG_ERR_CHECK(  SG_mrg_cset__compute_cset_dir_list(pCtx,pMrg->pMrgCSet_FinalResult)  );
	SG_ERR_CHECK(  SG_mrg_cset__check_dirs_for_collisions(pCtx, pMrg->pMrgCSet_FinalResult, pMrg)  );
	SG_ERR_CHECK(  SG_mrg_cset__make_unique_entrynames(pCtx,pMrg->pMrgCSet_FinalResult)  );

#if TRACE_MRG
	SG_ERR_IGNORE(  _trace_msg__dir_list_as_hierarchy(pCtx,"merge",pMrg->pMrgCSet_FinalResult)  );
#endif

	SG_ERR_CHECK(  SG_mrg__automerge_files(pCtx,pMrg,pMrg->pMrgCSet_FinalResult,pMrg->pMrgCSetStats_FinalResult)  );

	SG_ERR_CHECK(  SG_mrg__prepare_master_plan(pCtx, pMrg)  );
	SG_ERR_CHECK(  _sg_mrg__prepare_issues(pCtx, pMrg)  );

#if TRACE_MRG
	// Re-print stats with updated auto-merge results.
	SG_ERR_IGNORE(  _trace_msg__v_result(pCtx,"SG_mrg__compute_merge", pMrg->pMrgCSet_FinalResult, pMrg->pMrgCSetStats_FinalResult)  );
#endif

	if (pNrUnresolvedIssues)
	{
		SG_uint32 nrUnresolved = 0;

		if (pMrg->pvaIssues)
		{
			SG_uint32 k, nrTotal;

			SG_ERR_CHECK(  SG_varray__count(pCtx, pMrg->pvaIssues, &nrTotal)  );
			for (k=0; k<nrTotal; k++)
			{
				SG_int64 s;
				SG_pendingtree_wd_issue_status status;
				SG_vhash * pvhIssue;

				SG_ERR_CHECK_RETURN(  SG_varray__get__vhash(pCtx, pMrg->pvaIssues, k, &pvhIssue)  );
				SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, pvhIssue, "status", &s)  );
				status = (SG_pendingtree_wd_issue_status)s;

				if ((status & SG_ISSUE_STATUS__MARKED_RESOLVED) == 0)
					nrUnresolved++;
			}
		}

		*pNrUnresolvedIssues = nrUnresolved;
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}

void SG_mrg__apply_merge(SG_context * pCtx,
						 SG_mrg * pMrg)
{
	SG_varray * pvaParents = NULL;
	SG_rbtree_iterator * pIter = NULL;
	const char * pszHid;
	SG_bool bFound;

	// execute the wd-plan and make changes to the WD and the in-memory verion of the pendingtree.

	SG_ERR_CHECK(  SG_wd_plan__execute(pCtx, pMrg->p_wd_plan, pMrg->pPendingTree)  );

	// build new parent array.  parent[0] is special and needs to be the current baseline.
	// the others can be in any order.  update the parents in the in-memory version of the
	// pendingtree.

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pvaParents)  );
	SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pvaParents, pMrg->bufHid_CSet_Baseline)  );
	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, pMrg->prbCSets_OtherLeaves, &bFound, &pszHid, NULL)  );
	while (bFound)
	{
		SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pvaParents, pszHid)  );
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter, &bFound, &pszHid, NULL)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
	SG_ERR_CHECK(  SG_pendingtree__set_wd_parents(pCtx, pMrg->pPendingTree, &pvaParents)  );	// we no longer own pvaParents

	SG_ERR_CHECK(  SG_pendingtree__set_wd_issues(pCtx, pMrg->pPendingTree, &pMrg->pvaIssues)  );	// we no longer have or own the issues
	pMrg->bIssuesSavedInPendingTree = SG_TRUE;

fail:
	SG_VARRAY_NULLFREE(pCtx, pvaParents);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}

//////////////////////////////////////////////////////////////////

void SG_mrg__get_wd_plan__ref(SG_context * pCtx,
							  SG_mrg * pMrg,
							  const SG_wd_plan ** pp_wd_plan)
{
	SG_NULLARGCHECK_RETURN(pMrg);
	SG_NULLARGCHECK_RETURN(pp_wd_plan);

	*pp_wd_plan = pMrg->p_wd_plan;
}

void SG_mrg__get_wd_issues__ref(SG_context * pCtx,
								SG_mrg * pMrg,
								SG_bool * pbHaveIssues,
								const SG_varray ** ppvaIssues)
{
	SG_NULLARGCHECK_RETURN(pMrg);
	SG_NULLARGCHECK_RETURN(pbHaveIssues);
	SG_NULLARGCHECK_RETURN(ppvaIssues);

	if (pMrg->bIssuesSavedInPendingTree)
	{
		SG_ERR_CHECK_RETURN(  SG_pendingtree__get_wd_issues__ref(pCtx, pMrg->pPendingTree, pbHaveIssues, ppvaIssues)  );
	}
	else
	{
		SG_uint32 count = 0;

		if (pMrg->pvaIssues)
			SG_ERR_CHECK_RETURN(  SG_varray__count(pCtx, pMrg->pvaIssues, &count)  );

		*pbHaveIssues = (count > 0);
		*ppvaIssues = pMrg->pvaIssues;
	}
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_MRG_H
