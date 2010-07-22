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
 * @file sg_mrg__private_master_plan__delete.h
 *
 * @details We are used after the MERGE result has been
 * computed.  We are one of the last steps in the WD-PLAN
 * construction.  We are responsible for making the plan
 * steps for things that were deleted between the baseline
 * and the final-result.  Stuff that is in the WD on disk
 * (because it was in the baseline) and that needs to be
 * deleted (because it is not in the final-result).
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_MASTER_PLAN__DELETE_H
#define H_SG_MRG__PRIVATE_MASTER_PLAN__DELETE_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

struct _sg_mrg__plan__delete__data
{
	SG_mrg *		pMrg;
	SG_rbtree *		prbDirectoriesToDelete;		// map[pathname --> pInvEntry] -- a reverse-sorted list of directories that need to be deleted (these are the transient pathnames)
};

typedef struct _sg_mrg__plan__delete__data _sg_mrg__plan__delete__data;

//////////////////////////////////////////////////////////////////

static SG_rbtree_foreach_callback _sg_mrg__plan__deleted__cb;

static void _sg_mrg__plan__deleted__cb(SG_context * pCtx,
									   const char * pszKey_GidEntry,
									   void * pVoidValue_MrgCSetEntry,
									   void * pVoidData)
{
	SG_mrg_cset_entry * pMrgCSetEntry_Baseline = (SG_mrg_cset_entry *)pVoidValue_MrgCSetEntry;
	_sg_mrg__plan__delete__data * pData = (_sg_mrg__plan__delete__data *)pVoidData;
	SG_pathname * pPath_Allocated = NULL;
	SG_inv_entry * pInvEntry;
	SG_mrg_cset_entry * pMrgCSetEntry_Check;
	const SG_string * pStringSourceRepoPath;
	SG_bool bFound;

	if (pMrgCSetEntry_Baseline->markerValue & _SG_MRG__PLAN__MARKER__FLAGS__APPLIED)
	{
		// this entry in the baseline was already processed when its peer
		// in the final-result was processed.
		return;
	}

	// this entry was present in BASELINE but should not be in the Final-Result.

	SG_ERR_CHECK(  SG_inv_dirs__fetch_source_binding(pCtx, pData->pMrg->pInvDirs, pszKey_GidEntry,
													 &bFound, &pInvEntry, (void **)&pMrgCSetEntry_Check)  );
	if (!bFound)
	{
#if TRACE_MRG
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Merge_Plan_Deleted: no source binding for [%s] [%s]\n",
								   pszKey_GidEntry, SG_string__sz(pMrgCSetEntry_Baseline->pStringEntryname))  );
#endif
		return;
	}

	SG_ASSERT(  (pMrgCSetEntry_Check == pMrgCSetEntry_Baseline)  );

	// compute the pathname to the it.  this is where we *THINK* it is in our PLAN.

	SG_ERR_CHECK(  _sg_mrg__plan__compute_transient_path__source(pCtx, pData->pMrg, pszKey_GidEntry, &pPath_Allocated)  );


	if (pMrgCSetEntry_Baseline->tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		// defer directory deletes until the next pass (after we
		// (have taken care of everything within the directory).
		//
		// build reverse-sorted queue based upon absolute pathname;
		// this will let us rmdir children before parents.

		if (!pData->prbDirectoriesToDelete)
			SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS2(pCtx, &pData->prbDirectoriesToDelete,
													 128, NULL,
													 SG_rbtree__compare_function__reverse_strcmp)  );

		SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pData->prbDirectoriesToDelete,
												  SG_pathname__sz(pPath_Allocated),
												  pInvEntry)  );
	}
	else
	{
		// get the repo-path as it was in the baseline/source.  this should match the value
		// computed by SG_mrg_cset__compute_repo_path_for_entry(..., pMrgCSetEntry_Baseline,...)

		SG_ERR_CHECK(  SG_inv_entry__get_repo_path__source__ref(pCtx, pInvEntry, &pStringSourceRepoPath)  );

		if (pMrgCSetEntry_Baseline->tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
		{
			SG_ERR_CHECK(  SG_wd_plan__remove_file(pCtx, pData->pMrg->p_wd_plan,
												   pMrgCSetEntry_Baseline->bufGid_Entry,
												   SG_string__sz(pStringSourceRepoPath),
												   SG_pathname__sz(pPath_Allocated),
												   "FILE unchanged in baseline, but removed in other branch.")  );
		}
		else if (pMrgCSetEntry_Baseline->tneType == SG_TREENODEENTRY_TYPE_SYMLINK)
		{
			SG_ERR_CHECK(  SG_wd_plan__remove_symlink(pCtx, pData->pMrg->p_wd_plan,
													  pMrgCSetEntry_Baseline->bufGid_Entry,
													  SG_string__sz(pStringSourceRepoPath),
													  SG_pathname__sz(pPath_Allocated),
													  "SYMLINK unchanged in baseline, but removed in other branch.")  );
		}
		else
		{
			SG_ASSERT(  (0)  );
		}

		pMrgCSetEntry_Baseline->markerValue |= _SG_MRG__PLAN__MARKER__FLAGS__APPLIED;
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Allocated);
}

//////////////////////////////////////////////////////////////////

static SG_rbtree_foreach_callback _sg_mrg__plan__deleted_dirs__cb;

static void _sg_mrg__plan__deleted_dirs__cb(SG_context * pCtx,
											const char * pszKey_Path,
											void * pVoidValue_InvEntry,
											void * pVoidData)
{
	SG_inv_entry * pInvEntry = (SG_inv_entry *)pVoidValue_InvEntry;
	_sg_mrg__plan__delete__data * pData = (_sg_mrg__plan__delete__data *)pVoidData;
	const char * pszGid_Entry;
	const SG_string * pStringRepoPath;

	SG_ERR_CHECK(  SG_inv_entry__get_gid__source__ref(pCtx, pInvEntry, &pszGid_Entry)  );
	SG_ERR_CHECK(  SG_inv_entry__get_repo_path__source__ref(pCtx, pInvEntry, &pStringRepoPath)  );

	SG_ERR_CHECK(  SG_wd_plan__remove_directory(pCtx, pData->pMrg->p_wd_plan,
												pszGid_Entry,
												SG_string__sz(pStringRepoPath),
												pszKey_Path,
												"DIRECTORY unchanged in baseline, but removed in other branch.")  );

fail:
	return;
}

//////////////////////////////////////////////////////////////////

static void _sg_mrg__plan__delete(SG_context * pCtx, SG_mrg * pMrg)
{
	_sg_mrg__plan__delete__data data;

	memset(&data,0,sizeof(data));
	data.pMrg = pMrg;
	// defer allocation of prbDirectoriesToDelete until we have one

	// inspect each entry in the baseline and look for ones that
	// haven't been applied yet.  since we've already handled all
	// of the peer entries, these must not be present in the final-result
	// and need to be removed from the WD.  add these deletes to the plan.

	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx, pMrg->pMrgCSet_Baseline->prbEntries, _sg_mrg__plan__deleted__cb, &data)  );
	if (data.prbDirectoriesToDelete)
		SG_ERR_CHECK(  SG_rbtree__foreach(pCtx, data.prbDirectoriesToDelete, _sg_mrg__plan__deleted_dirs__cb, &data)  );

fail:
	SG_RBTREE_NULLFREE(pCtx, data.prbDirectoriesToDelete);
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_MASTER_PLAN__DELETE_H
