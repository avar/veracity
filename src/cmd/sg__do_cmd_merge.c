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
 * @file sg__do_cmd_merge.c
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_typedefs.h"
#include "sg_prototypes.h"

//////////////////////////////////////////////////////////////////

/**
 * Lookup each of the "--rev=r" and "--tag=t" args and get the
 * full unique HID for each prefix value.
 *
 * Build a rbtree of the full HIDs.  The caller must free this.
 */
static void _do_cmd_merge__collect_rev_tags(SG_context * pCtx,
											SG_option_state * pOptSt,
											SG_pendingtree * pPendingTree,
											SG_rbtree ** pprbCSetsToMerge)
{
	SG_rbtree * prbCSetsToMerge = NULL;
	char * pszHid_k = NULL;
	SG_repo * pRepo;
	const SG_varray * pva_wd_parents;
	const char * pszHidBaseline;
	SG_rev_tag_obj * pRTobj_k;
	SG_uint32 nrRevTag;
	SG_uint32 k;
	SG_bool bIsBaseline;
	SG_bool bDuplicate;

	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

	SG_ERR_CHECK(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pva_wd_parents)  );
	SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_wd_parents, 0, &pszHidBaseline)  );

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prbCSetsToMerge)  );

	SG_ERR_CHECK(  SG_vector__length(pCtx, pOptSt->pvec_rev_tags, &nrRevTag)  );
	for (k=0; k<nrRevTag; k++)
	{
		// convert them from prefixes to full CSET HIDs.

		SG_ERR_CHECK(  SG_vector__get(pCtx, pOptSt->pvec_rev_tags, k, (void **)&pRTobj_k)  );
		if (pRTobj_k->bRev)
			SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, pRTobj_k->pszRevTag, &pszHid_k)  );
		else
			SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, pRTobj_k->pszRevTag, &pszHid_k)  );

		bIsBaseline = (strcmp(pszHidBaseline,pszHid_k) == 0);
		if (bIsBaseline)
			SG_ERR_THROW2(  SG_ERR_MERGE_BASELINE_IS_IMPLIED,
							(pCtx, "%s %s is an alias for the current baseline",
							 ((pRTobj_k->bRev) ? "--rev" : "--tag"),
							 pRTobj_k->pszRevTag)  );

		SG_ERR_CHECK(  SG_rbtree__find(pCtx, prbCSetsToMerge, pszHid_k, &bDuplicate, NULL)  );
		if (bDuplicate)
			SG_ERR_THROW2(  SG_ERR_MERGE_REQUIRES_UNIQUE_CSETS,
							(pCtx, "%s %s is a duplicate",
							 ((pRTobj_k->bRev) ? "--rev" : "--tag"),
							 pRTobj_k->pszRevTag)  );

		SG_ERR_CHECK(  SG_rbtree__add(pCtx, prbCSetsToMerge, pszHid_k)  );
		SG_NULLFREE(pCtx, pszHid_k);
	}

	*pprbCSetsToMerge = prbCSetsToMerge;
	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, prbCSetsToMerge);
	SG_NULLFREE(pCtx, pszHid_k);
}

//////////////////////////////////////////////////////////////////

/**
 * Handle the MERGE command.
 *
 *
 */
void do_cmd_merge(SG_context * pCtx,
				  SG_option_state * pOptSt)
{
	SG_pathname * pPathCwd = NULL;
	SG_pendingtree * pPendingTree = NULL;
	SG_rbtree * prbCSetsToMerge = NULL;
	SG_mrg * pMrg = NULL;
	SG_string * pStrOutput = NULL;
	const SG_wd_plan * pPlan = NULL;
	const SG_wd_plan_stats * pStats;
	SG_uint32 nrUnresolvedIssues = 0;

	// TODO 2010/07/06 Decide if we want to warn them if the baseline is not a leaf/head
	// TODO            or make them give us a --force or something?
	// TODO
	// TODO            We have to allow them to do the merge using the baseline, but we
	// TODO            might want to warn them (and maybe nudge them) that they probably
	// TODO            should be starting from a leaf rather than something with children.
	// TODO
	// TODO            See sprawl-964.

	SG_bool bComplainIfBaselineNotLeaf = SG_FALSE;

	// use the current directory to find the pending-tree and the repo.

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathCwd, pOptSt->bIgnoreWarnings, &pPendingTree)  );

	if (pOptSt->pvec_rev_tags)
	{
		// the user gave us specific target changeset(s) to merge with the baseline.
		// these come from one or more --rev and/or --tag options.

		SG_ERR_CHECK(  _do_cmd_merge__collect_rev_tags(pCtx, pOptSt, pPendingTree, &prbCSetsToMerge)  );
	}
	else
	{
		// when no --rev or --tag we default to all heads (except the baseline)

		prbCSetsToMerge = NULL;
	}

	SG_ERR_CHECK(  SG_MRG__ALLOC(pCtx, pPendingTree, bComplainIfBaselineNotLeaf, &pMrg)  );
	SG_ERR_CHECK(  SG_mrg__compute_merge(pCtx, pMrg, prbCSetsToMerge, &nrUnresolvedIssues)  );

	SG_ERR_CHECK(  SG_mrg__get_wd_plan__ref(pCtx, pMrg, &pPlan)  );
	SG_ERR_CHECK(  SG_wd_plan__get_stats__ref(pCtx, pPlan, &pStats)  );

	if (pOptSt->bTest || pOptSt->bVerbose)
	{
		// Do a pretty display of the wd_plan and unresolved issues.

#if 0 && defined(DEBUG)
		SG_ERR_IGNORE(  SG_wd_plan_debug__dump_script(pCtx, pPlan, "Merge Operations Required")  );
#endif

		SG_ERR_CHECK(  SG_wd_plan__format__to_string(pCtx, pPlan, &pStrOutput)  );

        SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT, "Steps required to perform the merge:\n")  );
		SG_ERR_IGNORE(  SG_console__raw(pCtx, SG_CS_STDOUT, SG_string__sz(pStrOutput))  );
	}

	if (!pOptSt->bTest)
	{
		SG_ERR_CHECK(  SG_mrg__apply_merge(pCtx, pMrg)  );
		SG_ERR_CHECK(  SG_pendingtree__save(pCtx, pPendingTree)  );

		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT,
								   ("%d updated, %d deleted, %d added, %d merged, %d unresolved\n"),
								   pStats->nrDirsChanged + pStats->nrFilesChanged,
								   pStats->nrDirsDeleted + pStats->nrFilesDeleted,
								   pStats->nrDirsAdded   + pStats->nrFilesAdded,
								   pStats->nrFilesAutoMerged,
								   nrUnresolvedIssues)  );
	}

fail:
	// TODO decide how the portability warnings from SG_mrg and SG_pendingtree mesh
	if (pPendingTree && SG_context__err_equals(pCtx,SG_ERR_PORTABILITY_WARNINGS))
		SG_ERR_IGNORE(  sg_report_portability_warnings(pCtx,pPendingTree)  );

	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_RBTREE_NULLFREE(pCtx, prbCSetsToMerge);
	SG_MRG_NULLFREE(pCtx, pMrg);
	SG_STRING_NULLFREE(pCtx, pStrOutput);
}
