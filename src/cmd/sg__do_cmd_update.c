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
 * @file sg__do_cmd_update.c
 *
 * @details Handle the UPDATE command.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_typedefs.h"
#include "sg_prototypes.h"

//////////////////////////////////////////////////////////////////

static void _get_baseline(SG_context * pCtx, SG_pendingtree * pPendingTree, char ** ppszBaseline)
{
	const SG_varray * pvaParents;
	const char * pszBaseline;
	char * pszAllocated = NULL;

	// get the HID of the BASELINE (aka PARENT[0] from the pendingtree).

	SG_ERR_CHECK(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pvaParents)  );
	SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pvaParents, 0, &pszBaseline)  );

	SG_ERR_CHECK(  SG_strdup(pCtx, pszBaseline, &pszAllocated)  );

	*ppszBaseline = pszAllocated;
	return;

fail:
	SG_NULLFREE(pCtx, pszAllocated);
}


//////////////////////////////////////////////////////////////////

static void _advise_after_update(SG_context * pCtx,
								 SG_option_state * pOptSt,
								 SG_pathname * pPathCwd,
								 const char * pszBaselineBeforeUpdate)
{
	SG_pendingtree * pPendingTree = NULL;
	SG_repo * pRepo;
	char * pszBaselineAfterUpdate = NULL;
	SG_rbtree * prbLeaves = NULL;
	SG_uint32 nrLeaves;
	SG_bool bUpdateChangedBaseline;

	// re-open pendingtree to get the now-current baseline (we have to do
	// this in a new instance because the UPDATE saves the pendingtree which
	// frees all of the interesting stuff).

	SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathCwd, pOptSt->bIgnoreWarnings, &pPendingTree)  );
	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

	SG_ERR_CHECK(  _get_baseline(pCtx, pPendingTree, &pszBaselineAfterUpdate)  );

	// see if the update actually changed the baseline.

	bUpdateChangedBaseline = (strcmp(pszBaselineBeforeUpdate, pszBaselineAfterUpdate) != 0);

	// get the list of all heads/leaves.
	//
	// TODO 2010/06/30 Revisit this when we have NAMED BRANCHES because we
	// TODO            want to filter this list for things within their BRANCH.

	SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx,pRepo,SG_DAGNUM__VERSION_CONTROL,&prbLeaves)  );
#if defined(DEBUG)
	{
		SG_bool bFound = SG_FALSE;
		SG_ERR_CHECK(  SG_rbtree__find(pCtx, prbLeaves, pszBaselineAfterUpdate, &bFound, NULL)  );
		SG_ASSERT(  (bFound)  );
	}
#endif	
	SG_ERR_CHECK(  SG_rbtree__count(pCtx, prbLeaves, &nrLeaves)  );

	if (nrLeaves > 1)
	{
		if (bUpdateChangedBaseline)
		{
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT,
									   "Baseline updated to descendant head, but there are multiple heads; consider merging.\n")  );
		}
		else
		{
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT,
									   "Baseline already at head, but there are multiple heads; consider merging.\n")  );
		}
	}
	else
	{
		if (bUpdateChangedBaseline)
		{
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT,
									   "Baseline updated to head.\n")  );
		}
		else
		{
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT,
									   "Baseline already at head.\n")  );
		}
	}

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_RBTREE_NULLFREE(pCtx, prbLeaves);
	SG_NULLFREE(pCtx, pszBaselineAfterUpdate);
}

//////////////////////////////////////////////////////////////////

static void _my_do_cmd_update(SG_context * pCtx,
							  SG_option_state * pOptSt,
							  SG_pendingtree * pPendingTree,
							  const char * pszTargetChangeset)
{
	SG_pendingtree_action_log_enum eActionLog;

	if (pOptSt->bTest)
	{
		eActionLog = SG_PT_ACTION__LOG_IT;
	}
	else
	{
		eActionLog = SG_PT_ACTION__DO_IT;
		if (pOptSt->bVerbose)
			eActionLog |= SG_PT_ACTION__LOG_IT;
	}

	SG_ERR_CHECK(  SG_pendingtree__update_baseline(pCtx, pPendingTree, pszTargetChangeset,
												   pOptSt->bForce,
												   eActionLog)  );

	if (eActionLog & SG_PT_ACTION__LOG_IT)
		SG_ERR_IGNORE(  sg_report_action_log(pCtx,pPendingTree)  );

fail:
	if (SG_context__err_equals(pCtx,SG_ERR_PORTABILITY_WARNINGS))
		SG_ERR_IGNORE(  sg_report_portability_warnings(pCtx,pPendingTree)  );
}

//////////////////////////////////////////////////////////////////

/**
 * Handle the UPDATE command.
 *
 *
 */
void do_cmd_update(SG_context * pCtx,
				   SG_option_state * pOptSt)
{
	SG_pathname * pPathCwd = NULL;
	SG_pendingtree * pPendingTree = NULL;
	SG_repo * pRepo;
	char * pszTargetChangeset = NULL;
	char * pszBaselineBeforeUpdate = NULL;

	// use the current directory to find the pending-tree, the repo, and the current baseline.

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathCwd, pOptSt->bIgnoreWarnings, &pPendingTree)  );
	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

	SG_ERR_CHECK(  _get_baseline(pCtx, pPendingTree, &pszBaselineBeforeUpdate)  );

	// determine the target changeset

	// we check that we have at most 1 rev *or* 1 tag up in sg.c

	if (pOptSt->iCountRevs == 1)
	{
		SG_rev_tag_obj* pRTobj = NULL;
		const char * psz_rev_0;

		SG_ERR_CHECK(  SG_vector__get(pCtx, pOptSt->pvec_rev_tags, 0, (void**)&pRTobj)  );

		psz_rev_0 = pRTobj->pszRevTag;

		SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, psz_rev_0, &pszTargetChangeset)  );
	}
	else if (pOptSt->iCountTags == 1)
	{
		SG_rev_tag_obj* pRTobj = NULL;
		const char * psz_tag_0;

		SG_ERR_CHECK(  SG_vector__get(pCtx, pOptSt->pvec_rev_tags, 0, (void**)&pRTobj)  );

		psz_tag_0 = pRTobj->pszRevTag;

		SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, psz_tag_0, &pszTargetChangeset)  );
	}
	else
	{
		// pass NULL for target changeset and let the UPDATE code find the proper head/tip.
	}

	SG_ERR_CHECK(  _my_do_cmd_update(pCtx, pOptSt, pPendingTree, pszTargetChangeset)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	pRepo = NULL;

	if (pszTargetChangeset == NULL)
	{
		// if they didn't ask for a specific changeset (and we successfully
		// went to the SINGLE/UNIQUE DESCENDANT HEAD from their (then current)
		// BASELINE, we should look around and see if there are other heads/leaves
		// and advise them to MERGE with them.
		//
		// Since we did successfully do the UPDATE we should exit with OK, so
		// I'm going to do all of this advisory stuff in an IGNORE.

		SG_ERR_IGNORE(  _advise_after_update(pCtx, pOptSt, pPathCwd, pszBaselineBeforeUpdate)  );
	}

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_NULLFREE(pCtx, pszTargetChangeset);
	SG_NULLFREE(pCtx, pszBaselineBeforeUpdate);
}
