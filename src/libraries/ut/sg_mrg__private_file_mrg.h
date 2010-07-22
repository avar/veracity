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
 * @file sg_mrg__private_file_mrg.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_FILE_MRG_H
#define H_SG_MRG__PRIVATE_FILE_MRG_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

struct _amf_data
{
	SG_mrg *				pMrg;
	SG_mrg_cset *			pMrgCSet;
	SG_mrg_cset_stats *		pMrgCSetStats;
};

typedef struct _amf_data _amf_data;

//////////////////////////////////////////////////////////////////

/**
 * Get the file suffix (in the case of a rename or a rename conflict, we may have
 * to do multiple guesses (match-first, match-best, ... like we do in SGDM)) and
 * see if this is a file type that we have auto-merge handler for.  (binary vs text
 * vs xml vs ... like we do in SGDM)
 */
static void _select_external_automerge_handler(SG_UNUSED_PARAM(SG_context * pCtx),
											   SG_UNUSED_PARAM(_amf_data * pAmfData),
											   SG_UNUSED_PARAM(SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict),
											   SG_mrg_external_automerge_handler ** ppfn)
{
	// TODO allow localsettings to have multiple handlers defined
	// TODO {suffix-list, exe, arg-list-with-%field-replacements and etc., exit-status-meanings}
	// TODO
	// TODO have builtin list of known configurations for diff3 and sgdm
	// TODO where we already know the arg-list details....
	// TODO and where we already know text from binary and etc.
	// TODO
	// TODO have an internal handler (link in the sgd-core-light or something
	// TODO where we can do the auto-merge algorithm without fork/exec...
	// TODO
	// TODO for now, just assume gnu-diff3

	SG_UNUSED(pCtx);
	SG_UNUSED(pAmfData);
	SG_UNUSED(pMrgCSetEntryConflict);

	// for now, assume that we handle all suffixes....

	*ppfn = SG_mrg_external_automerge_handler__builtin_diff3;
}

//////////////////////////////////////////////////////////////////

static void _handle_external_automerge(SG_context * pCtx,
									   SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
									   SG_mrg_external_automerge_handler * pfn,
									   SG_bool * pbAutoMergeSuccessful)
{
	SG_mrg_automerge_plan_item * pItem = NULL;
	SG_uint32 k, nrItems;

	SG_ERR_CHECK_RETURN(  SG_vector__length(pCtx,pMrgCSetEntryConflict->pVec_AutoMergePlan,&nrItems)  );
	for (k=0; k<nrItems; k++)
	{
		// let the external merge tool process item[k].

		SG_ERR_CHECK_RETURN(  SG_vector__get(pCtx,pMrgCSetEntryConflict->pVec_AutoMergePlan,k,(void **)&pItem)  );
		SG_ERR_CHECK_RETURN(  (*pfn)(pCtx,pItem)  );

		if (pItem->result != SG_MRG_AUTOMERGE_RESULT__SUCCESSFUL)
		{
			*pbAutoMergeSuccessful = SG_FALSE;

			return;
		}
	}

	// the auto-merge worked on the full cascade.

	// we have already recorded the pathname of the final F(M) in the conflict
	// structure so that later we can move the file to the WD when we get ready
	// to update it.
	//
	// delete and free all the intermediate files since auto-merge did all the work.
	//
	// but first we null out the result in the final step in the plan so that the
	// actual result file doesn't get deleted by the foreach.

	if (pItem)
		SG_PATHNAME_NULLFREE(pCtx,pItem->pPath_Result);
	SG_VECTOR_NULLFREE_WITH_ASSOC(pCtx,pMrgCSetEntryConflict->pVec_AutoMergePlan,(SG_free_callback *)SG_mrg_automerge_plan_item__remove_and_free);

	*pbAutoMergeSuccessful = SG_TRUE;
}

//////////////////////////////////////////////////////////////////

/**
 * We get called once for each conflict in SG_mrg_cset.prbConflicts.
 * We may be a structural conflict or a TBD content conflict.  Ignore
 * the former and try to automerge the contents of the latter.
 */
static SG_rbtree_foreach_callback _try_automerge_files;

static void _try_automerge_files(SG_context * pCtx,
								 SG_UNUSED_PARAM(const char * pszKey_Gid_Entry),
								 void * pVoidAssocData_MrgCSetEntryConflict,
								 void * pVoid_Data)
{
	SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict = (SG_mrg_cset_entry_conflict *)pVoidAssocData_MrgCSetEntryConflict;
	_amf_data * pAmfData = (_amf_data *)pVoid_Data;
	SG_mrg_external_automerge_handler * pfn = NULL;
	SG_bool bAutoMergeSuccessful = SG_FALSE;
	SG_uint32 k, nrItems;

	SG_UNUSED(pszKey_Gid_Entry);

	if ((pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__TBD) == 0)
		return;

	SG_ASSERT( (pMrgCSetEntryConflict->pMrgCSetEntry_Composite->tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE) );
	SG_ASSERT( (pMrgCSetEntryConflict->pVec_AutoMergePlan) );
	SG_ASSERT( (pMrgCSetEntryConflict->pPathAutoMergePlanResult) );

	// We are given a "merge-plan".  A sequence of steps that we need to
	// perform (and in the given order) to completely merge the various
	// versions of the file's content.
	//
	// We fetch the blobs for F(A) and F(Ux) into a private temp directory and
	// let the external merge tool compute the merges.  The merge-results are
	// also placed in the temp directory.
	//
	// The final F(M) may be eventually moved to the user's WD (if all goes well).
	// We may or may not move the other F(A), F(Ux), and F(mx) files to the user's
	// WD (if we wanted to do the .YOURS/.MINE stuff).
	//
	//////////////////////////////////////////////////////////////////
	//
	// Note that we are only attempting to create F(M) for this entry, we DO NOT do
	// any commits -- and we bail the first time that auto-merge fails.  That is,
	// if we are in an Octopus-style merge, it is up to us how hard we try to
	// auto-merge a file that was modified in more than 2 unique ways.  If it works,
	// fine; if not, we stop the auto-merge on this entry.  The rest of the plan
	// remains un-evaluated.
	//
	//////////////////////////////////////////////////////////////////
	//
	// TODO decide how we want to fetch the actual versions/blobs from
	// TODO the repo.
	// TODO [1] if we don't have a handler for that suffix, we need to
	// TODO     just get them all and give the user a script or something
	// TODO     to show them the "best" order for combining them and let
	// TODO     the do all of the work and later manually tell us where
	// TODO     the final version is.
	// TODO
	// TODO [2] if we do have a handler, should we just look at each
	// TODO     item in the vector, get the pair (if necessary) and do
	// TODO     the merge and keep looping until we get something that
	// TODO     can't be auto-merged, then just get everything remaining
	// TODO     in the plan and give up.
	// TODO
	// TODO [3] just blindly get all of the various versions from the
	// TODO     repo.  and then try as much of the auto-merge as we can.
	// TODO
	// TODO The last one is probably easiest, but might be problematic
	// TODO if the files are huge.
	// TODO
	// TODO For now, just do [3].

	SG_ERR_CHECK(  SG_vector__length(pCtx,pMrgCSetEntryConflict->pVec_AutoMergePlan,&nrItems)  );
	for (k=0; k<nrItems; k++)
	{
		SG_mrg_automerge_plan_item * pItem;

		SG_ERR_CHECK(  SG_vector__get(pCtx,pMrgCSetEntryConflict->pVec_AutoMergePlan,k,(void **)&pItem)  );
		SG_ERR_CHECK(  SG_mrg_automerge_plan_item__fetch_files(pCtx,pItem)  );
	}

	// choose handler based upon suffix or the phase of the moon....

	SG_ERR_CHECK(  _select_external_automerge_handler(pCtx,pAmfData,pMrgCSetEntryConflict,&pfn)  );
	if (pfn == NULL)
	{
		pMrgCSetEntryConflict->flags |=  SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__NO_RULE;
		pAmfData->pMrgCSetStats->nrAutoMerge_NoRule++;
	}
	else
	{
		SG_ERR_CHECK(  _handle_external_automerge(pCtx,pMrgCSetEntryConflict,pfn,&bAutoMergeSuccessful)  );

		if (bAutoMergeSuccessful)
		{
			pMrgCSetEntryConflict->flags |=  SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__AUTO_OK;
			pAmfData->pMrgCSetStats->nrAutoMerge_OK++;
		}
		else
		{
			pMrgCSetEntryConflict->flags |=  SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__AUTO_FAILED;
			pAmfData->pMrgCSetStats->nrAutoMerge_Fail++;
		}
	}

	pMrgCSetEntryConflict->flags &= ~SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__TBD;
	pAmfData->pMrgCSetStats->nrAutoMerge_TBD--;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

void SG_mrg__automerge_files(SG_context * pCtx, SG_mrg * pMrg, SG_mrg_cset * pMrgCSet, SG_mrg_cset_stats * pMrgCSetStats)
{
	_amf_data amfData;

	SG_NULLARGCHECK_RETURN(pMrg);
	SG_NULLARGCHECK_RETURN(pMrgCSet);

	// if there were no conflicts *anywhere* in the result-cset, we don't have to do anything.

	if (!pMrgCSet->prbConflicts)
		return;

	// visit each entry in the result-cset and do any auto-merges that we can.

	amfData.pMrg = pMrg;
	amfData.pMrgCSet = pMrgCSet;
	amfData.pMrgCSetStats = pMrgCSetStats;

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,pMrgCSet->prbConflicts,_try_automerge_files,&amfData)  );
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_FILE_MRG_H
