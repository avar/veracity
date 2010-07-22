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
 * @file sg_mrg__private_master_plan__utils.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_MASTER_PLAN__UTILS_H
#define H_SG_MRG__PRIVATE_MASTER_PLAN__UTILS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

static void _sg_mrg__plan__compute_transient_path__source(SG_context * pCtx,
														  SG_mrg * pMrg,
														  const char * pszGid_Entry,
														  SG_pathname ** ppPath);

//////////////////////////////////////////////////////////////////

/**
 * Compute the transient disk path for the given entry as it
 * WILL APPEAR after we actually execute the plan.
 */
static void _sg_mrg__plan__compute_transient_path__target(SG_context * pCtx,
														  SG_mrg * pMrg,
														  const char * pszGid_Entry,
														  SG_pathname ** ppPath)
{
	SG_pathname * pPath_Allocated = NULL;
	SG_inv_entry * pInvEntry;
	SG_mrg_cset_entry * pMrgCSetEntry;
	SG_bool bFound;

	SG_NULLARGCHECK_RETURN(pMrg);
	SG_NULLARGCHECK_RETURN(pszGid_Entry);

	SG_ERR_CHECK(  SG_inv_dirs__fetch_target_binding(pCtx, pMrg->pInvDirs, pszGid_Entry,
													 &bFound, &pInvEntry, (void **)&pMrgCSetEntry)  );
	// TODO this should probably be an assert, but i want the extra info.
	if (!bFound)
		SG_ERR_THROW2_RETURN(  SG_ERR_NOT_FOUND,
							   (pCtx, "TODO Merge transient target path for [%s]", pszGid_Entry)  );

	// we only worry about the markerValue on the source (baseline) view
	// because the plan is concerned with converting the source view to
	// the target view.  so, for example, we don't park things in the target
	// view.

	// the assumption is that we want the target version of this entry.
	// this includes the target-entryname and the target-parent-directory.
	// *BUT* we don't know what the status of the parent-directory is, so
	// we use the more-general source version.

	if (pMrgCSetEntry->bufGid_Parent[0])
	{
		SG_ERR_CHECK(  _sg_mrg__plan__compute_transient_path__source(pCtx, pMrg,
																	 pMrgCSetEntry->bufGid_Parent,
																	 &pPath_Allocated)  );
		SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath_Allocated,
													SG_string__sz(pMrgCSetEntry->pStringEntryname))  );
	}
	else
	{
		SG_ASSERT(  (strcmp(SG_string__sz(pMrgCSetEntry->pStringEntryname),"@") == 0)  );
		SG_ERR_CHECK(  SG_pendingtree__get_working_directory_top(pCtx, pMrg->pPendingTree,
																 &pPath_Allocated)  );
	}

	*ppPath = pPath_Allocated;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Allocated);
}

/**
 * Compute the transient disk path for the given entry as
 * it WOULD APPEAR if we were actually executing the plan
 * as we compute it.  That is, compensate for the steps
 * we've already added to the PLAN.  For example, if we have
 * already SCHEDULED for some directories to be parked, a
 * reference to something within that directory needs to use
 * the parked path for the parent directory --- EVEN THOUGH
 * IT IS STILL IN THE WD IN ITS ORIGINAL LOCATION.
 *
 * This is intended for entries in the BASELINE (SOURCE)
 * version of the WD as we juggle it to become the result.
 */
static void _sg_mrg__plan__compute_transient_path__source(SG_context * pCtx,
														  SG_mrg * pMrg,
														  const char * pszGid_Entry,
														  SG_pathname ** ppPath)
{
	SG_pathname * pPath_Allocated = NULL;
	SG_inv_entry * pInvEntry;
	SG_mrg_cset_entry * pMrgCSetEntry;
	SG_bool bFound;

	SG_NULLARGCHECK_RETURN(pMrg);
	SG_NULLARGCHECK_RETURN(pszGid_Entry);

	SG_ERR_CHECK(  SG_inv_dirs__fetch_source_binding(pCtx, pMrg->pInvDirs, pszGid_Entry,
													 &bFound, &pInvEntry, (void **)&pMrgCSetEntry)  );
	if (bFound)
	{
		// the entry is present in the baseline.

		if (pMrgCSetEntry->markerValue & _SG_MRG__PLAN__MARKER__FLAGS__APPLIED)
		{
			// the entry has already been processed.  get the target version.

			SG_ERR_CHECK(  _sg_mrg__plan__compute_transient_path__target(pCtx, pMrg, pszGid_Entry, &pPath_Allocated)  );
		}
		else if (pMrgCSetEntry->markerValue & _SG_MRG__PLAN__MARKER__FLAGS__PARKED)
		{
			// the entry itself was parked.   it is in the parking lot.

			const SG_pathname * pPathParked;

			SG_ERR_CHECK(  SG_inv_entry__get_parked_path__ref(pCtx, pInvEntry, &pPathParked)  );
			SG_ASSERT(  (pPathParked)  );
			SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPath_Allocated, pPathParked)  );
		}
		else
		{
			// the entry is still where it was in the baseline relative to its parent directory.
			// that is, it is still in the same directory and has the same entyrname.
			// (but its parent directory (or grand*parent) may have been moved or parked.)

			if (pMrgCSetEntry->bufGid_Parent[0])
			{
				SG_ERR_CHECK(  _sg_mrg__plan__compute_transient_path__source(pCtx, pMrg,
																			 pMrgCSetEntry->bufGid_Parent,
																			 &pPath_Allocated)  );
				SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath_Allocated,
															SG_string__sz(pMrgCSetEntry->pStringEntryname))  );
			}
			else
			{
				SG_ASSERT(  (strcmp(SG_string__sz(pMrgCSetEntry->pStringEntryname),"@") == 0)  );
				SG_ERR_CHECK(  SG_pendingtree__get_working_directory_top(pCtx, pMrg->pPendingTree,
																		 &pPath_Allocated)  );
			}
		}
	}
	else
	{
		SG_ERR_CHECK(  _sg_mrg__plan__compute_transient_path__target(pCtx, pMrg, pszGid_Entry, &pPath_Allocated)  );
	}

	*ppPath = pPath_Allocated;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Allocated);
}

//////////////////////////////////////////////////////////////////

/**
 * Append one or more commands to the plan to
 * populate the WD and pendingtree with a file
 * that has an existing GID and whose content
 * reflects either the auto-merge-result or a
 * series of yours/mine/ancestor files.
 *
 * The difference between __get_file and __alter_file
 * is that we use __get_file when the file is not
 * present in the baseline so there is no juggling
 * of the current WD version to worry about.  All of
 * the other problems/issues still apply in both
 * instances.  (You will probably only see calls to
 * __get_file when doing n-way merges.)
 */
static void _sg_mrg__plan__get_file_from_divergent_edit(SG_context * pCtx,
														SG_mrg * pMrg,
														SG_mrg_cset_entry * pMrgCSetEntry_FinalResult,
														const SG_string * pStringRepoPath_FinalResult,
														const SG_pathname * pPath_Destination)
{
	SG_ASSERT(  (pMrgCSetEntry_FinalResult)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->bufHid_Blob[0] == 0)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pVec_MrgCSetEntry_Changes)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pVec_MrgCSetEntryNeq_Changes)  );

	if (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__AUTO_OK)
	{
		// not present in baseline, but auto-merge was successful (must be n-way merge)
		// so we can treat this like a normal __get but pull the file content from the
		// auto-merge result file rather than from the repo using the hid.

		const char * pszReason;

		if (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__UNDELETE__MASK)
			pszReason = "FILE delete in baseline cancelled; FILE auto-merged from edits in other branches.";
		else
			pszReason = "FILE created from auto-merge of edits in other branches.";

		SG_ERR_CHECK(  SG_wd_plan__get_file_from_auto_merge_result(pCtx, pMrg->p_wd_plan,
																   pMrgCSetEntry_FinalResult->bufGid_Entry,
																   SG_string__sz(pStringRepoPath_FinalResult),
																   SG_pathname__sz(pPath_Destination),
																   SG_pathname__sz(pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pPathAutoMergePlanResult),
																   pMrgCSetEntry_FinalResult->attrBits,
																   pMrgCSetEntry_FinalResult->bufHid_XAttr,
																   pszReason)  );
	}
	else
	{
		// TODO 2010/05/05 We have an n-way merge and this file (which is not
		// TODO            present in the baseline) was edited in multiple
		// TODO            branches *and* auto-merge failed.  So we need to
		// TODO            dump a bunch of versions in the directory and
		// TODO            leave a conflict for them to resolve later by
		// TODO            doing one or more manual merges.
		// TODO
		// TODO            One of the issues here is how to name the various
		// TODO            temp files.  The .yours/.mine trick only works when
		// TODO            we have 1 set to merge.
		// TODO
		// TODO            Save all of this until we get n-way merges working.
		SG_ERR_THROW2_RETURN(  SG_ERR_NOTIMPLEMENTED,
						   (pCtx, ("TODO plan for get-file-from-divergent-edit:\n"
								   "                  [gid %s]\n"
								   "            [repo-path %s]\n"
								   "                 [path %s]\n"),
							pMrgCSetEntry_FinalResult->bufGid_Entry,
							SG_string__sz(pStringRepoPath_FinalResult),
							SG_pathname__sz(pPath_Destination))  );
	}

fail:
	return;
}

/**
 * auto-merge was not successful (or not attempted).
 * add commands to the wd-plan to splat the various
 * temp files into the WD with funky names so that
 * they can manually do the file merge afterwards.
 */
static void _sg_mrg__plan__alter_file_from_divergent_edit(SG_context * pCtx,
														  SG_mrg * pMrg,
														  SG_mrg_cset_entry * pMrgCSetEntry_FinalResult, SG_mrg_cset_entry * pMrgCSetEntry_Baseline,
														  const SG_string * pStringRepoPath_FinalResult, const SG_string * pStringRepoPath_Baseline,
														  const SG_pathname * pPath_FinalResult,         const SG_pathname * pPath_Baseline)
{
	SG_uint32 nrLeaves;
	SG_uint32 nrItems;
	const SG_mrg_automerge_plan_item * pItem_0;
	SG_pathname * pPath_Directory = NULL;
	SG_pathname * pPath_Directory_Entryname_Label = NULL;
	SG_string * pString_Entryname = NULL;
	SG_string * pString_Entryname_Label = NULL;

	SG_ASSERT(  (pMrgCSetEntry_FinalResult)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->bufHid_Blob[0] == 0)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK__MANUAL)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pVec_MrgCSetEntry_Changes)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pVec_MrgCSetEntryNeq_Changes)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pVec_AutoMergePlan)  );

	// pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict-->prbUnique_File_HidBlob_or_AutoMergePlanResult
	// contains all of the different versions of the file content that we need to let the user see and
	// decide what to do with.  For the versions that came straight from the REPO, the KEYS are the
	// HIDs of the associated blobs.  For the versions that are the result of a sub-merge (in an n-way
	// merge), the KEYS are the pathnames of the tmp file which we *should* use during the auto-merge.
	//
	// if the auto-merge was attempted and we fetched/created a tmp file, the corresponding rbtree
	// VALUE has the path to it.  (for the sub-merge results, I think the VALUE is a copy of the KEY.)
	//
	// furthermore, we have an auto-merge-plan that gives the order that the various versions should
	// be merged (along with the associated ancestor (both LCA and interior)).
	//
	// as always, items are stored in random GID or temp file name order, so we don't immediately
	// know which was associated with the baseline (if any).  because of the way we generate the
	// auto-merge plan, we have a series of UNIQUE versions (named A_A, a0_U0, ... A_M) that must
	// be combined.  In the 2-way case, it would be easy to figure out which of A_U0 or A_U1 maps
	// to the baseline, but for the n-way case it is not so simple -- or even useful.  That is,
	// preserving the .yours/.mine metaphore for the 2-way case might be fine, but breaks down
	// on the n-way cases.
	//
	// for now we assume that in sg_mrg__private_file_mrg.h:__try_automerge_files() that we
	// ALWAYS populate ALL of the versions (option [3]) before we start attempting sub-merges
	// and before we decide if we have a handler for that suffix -- so that we always have the
	// files in tmp and don't need to fetch them here.

	SG_ERR_CHECK(  SG_daglca__get_stats(pCtx,pMrg->pDagLca,NULL,NULL,&nrLeaves)  );
	if (nrLeaves == 2)
	{
		// we have a 2-way merge.
		//
		// lets go with tradition and splat .yours/.mine files in the destination directory.
		// add commands to the wd-plan to do the following:
		//
		// [1] pretend that the auto-merge-result was OK and do a normal __alter_file
		//     (so that any move/rename/attrbits/xattr changes get applied and the
		//     original file is converted to the auto-merge-result)  This keeps the
		//     WD and the pendingtree happy.
		//
		// [2] move the various A_A, A_U0, and A_U1 files from the tmp dir to the
		//     WD (but do not add to the pendingtree).  These will be named as
		//     <filename>.{ancestor,baseline,other}
		//
		// TODO is it right to set the non-default attrbits/xattrs here -- knowing
		// TODO that whatever tool that they use to do the merge may not respect
		// TODO them.  would it be better to have this done during the resolve.
		// TODO this is more of an issue when the auto-merge is not attempted and
		// TODO we don't have a result file.

		SG_ERR_CHECK(  SG_wd_plan__alter_file_from_auto_merge_result(pCtx, pMrg->p_wd_plan,
																	 pMrgCSetEntry_FinalResult->bufGid_Entry,
																	 SG_string__sz(pStringRepoPath_FinalResult), SG_string__sz(pStringRepoPath_Baseline),
																	 SG_pathname__sz(pPath_FinalResult),         SG_pathname__sz(pPath_Baseline),
																	 SG_pathname__sz(pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pPathAutoMergePlanResult),
																	 pMrgCSetEntry_Baseline->bufHid_Blob,
																	 pMrgCSetEntry_FinalResult->attrBits,        pMrgCSetEntry_Baseline->attrBits,
																	 pMrgCSetEntry_FinalResult->bufHid_XAttr,    pMrgCSetEntry_Baseline->bufHid_XAttr,
																	 "FILE content of auto-merge conflict.")  );

		// split the final-result pathname into <directory> and <entryname>
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPath_Directory, pPath_FinalResult)  );
		SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPath_Directory, &pString_Entryname)  );
		SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPath_Directory)  );

		SG_ERR_CHECK(  SG_vector__length(pCtx, pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pVec_AutoMergePlan, &nrItems)  );
		SG_ASSERT(  (nrItems > 0)  );
		SG_ERR_CHECK(  SG_vector__get(pCtx, pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pVec_AutoMergePlan, 0, (void **)&pItem_0)  );

		// TODO should we include a unique number (likg ~sg00~) or just let __move_temp_file overwrite any existing file?
		//
		// TODO what names do we want to use for these files?  Yours/Mine/Ancestor?  Other/Baseline/Original?
		// TODO I don't like any of them, but is there one that we dislike the least?
		//
		// TODO Revisit the somewhat arbitrary way we set A_U0 and A_U1 to mine and yours when we
		// TODO constructed the auto-merge-plan-item and see if we can guarantee that A_U0 is for L0
		// TODO (the baseline).

		SG_ERR_CHECK(  SG_string__alloc__format(pCtx, &pString_Entryname_Label, "%s~ancestor", SG_string__sz(pString_Entryname))  );
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_Directory_Entryname_Label, pPath_Directory, SG_string__sz(pString_Entryname_Label))  );
		SG_ERR_CHECK(  SG_wd_plan__move_entry(pCtx, pMrg->p_wd_plan, pItem_0->pPath_Ancestor, pPath_Directory_Entryname_Label,
											  "FILE content of ancestor version in conflict.")  );
		SG_PATHNAME_NULLFREE(pCtx, pPath_Directory_Entryname_Label);
		SG_STRING_NULLFREE(pCtx, pString_Entryname_Label);

		SG_ERR_CHECK(  SG_string__alloc__format(pCtx, &pString_Entryname_Label, "%s~mine", SG_string__sz(pString_Entryname))  );
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_Directory_Entryname_Label, pPath_Directory, SG_string__sz(pString_Entryname_Label))  );
		SG_ERR_CHECK(  SG_wd_plan__move_entry(pCtx, pMrg->p_wd_plan, pItem_0->pPath_Mine, pPath_Directory_Entryname_Label,
											  "FILE content of baseline version in conflict.")  );
		SG_PATHNAME_NULLFREE(pCtx, pPath_Directory_Entryname_Label);
		SG_STRING_NULLFREE(pCtx, pString_Entryname_Label);

		SG_ERR_CHECK(  SG_string__alloc__format(pCtx, &pString_Entryname_Label, "%s~other", SG_string__sz(pString_Entryname))  );
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_Directory_Entryname_Label, pPath_Directory, SG_string__sz(pString_Entryname_Label))  );
		SG_ERR_CHECK(  SG_wd_plan__move_entry(pCtx, pMrg->p_wd_plan, pItem_0->pPath_Yours, pPath_Directory_Entryname_Label,
											  "FILE content of other version in conflict.")  );
		SG_PATHNAME_NULLFREE(pCtx, pPath_Directory_Entryname_Label);
		SG_STRING_NULLFREE(pCtx, pString_Entryname_Label);
	}
	else
	{
		// in an n-way merge.  whatever naming scheme we came up with for the
		// 2-way merge probably doesn't make sense here.
		//
		// TODO if there were only 2 versions of the file we might duplicate
		// TODO the above (maybe with the same naming scheme or maybe use the
		// TODO <filename>~A_U0 names or try <filename>~L0 names), but if 3
		// TODO or more branches modified the file and we have sub-merges, we
		// TODO may want to just move the whole per-file tmp dir into the WD,
		// TODO so we'd have <filename> with the last result or sub-result
		// TODO that we attempted and a <filename>~CONFLICT directory with
		// TODO <filename~CONFLICT/{A_A, A_U0, ...}.

		SG_ERR_THROW2_RETURN(  SG_ERR_NOTIMPLEMENTED,
							   (pCtx, ("TODO plan for n-way alter-file-from-divergent-edit:\n"
									   "                  [gid %s]\n"
									   "            [repo-path %s]\n"
									   "                      [%s]\n"
									   "                 [path %s]\n"
									   "                      [%s]\n"),
								pMrgCSetEntry_FinalResult->bufGid_Entry,
								SG_string__sz(pStringRepoPath_FinalResult), SG_string__sz(pStringRepoPath_Baseline),
								SG_pathname__sz(pPath_FinalResult),         SG_pathname__sz(pPath_Baseline))  );
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Directory);
	SG_PATHNAME_NULLFREE(pCtx, pPath_Directory_Entryname_Label);
	SG_STRING_NULLFREE(pCtx, pString_Entryname);
	SG_STRING_NULLFREE(pCtx, pString_Entryname_Label);
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_MASTER_PLAN__UTILS_H
