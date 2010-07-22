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
 * @file sg_mrg__private_master_plan__overrule.h
 *
 * @details Like sg_mrg__private_master_plan__add.h but devoted to
 * entries that had an "overruled-delete".  An item that is present
 * in the LCA and the final-result, but not in the baseline.  Meaning
 * that it was DELETED in the baseline, but because of other changes
 * in the other branch(es), it must exist in the final-result and so
 * we need to override the delete.  We also need to make sure that
 * we preserve the entry's GID from the other branch.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_MASTER_PLAN__OVERRULE_H
#define H_SG_MRG__PRIVATE_MASTER_PLAN__OVERRULE_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

static void _sg_mrg__plan__overrule_delete(SG_context * pCtx, SG_mrg * pMrg,
										   const char * pszKey_GidEntry,
										   SG_mrg_cset_entry * pMrgCSetEntry_FinalResult)
{
	SG_pathname * pPath_Allocated = NULL;
	SG_string * pStringRepoPath_FinalResult = NULL;
	const char * pszReason;

	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict)  );
	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__UNDELETE__MASK)  );

	// compute the absolute WD path where we should put the item.  this
	// is a transient path because one or more of the parent directories
	// for this item may be parked in the parking lot.
	SG_ERR_CHECK(  _sg_mrg__plan__compute_transient_path__target(pCtx, pMrg, pszKey_GidEntry,
																 &pPath_Allocated)  );

	// compute the repo-path where the item will be in the final-result.
	SG_ERR_CHECK(  SG_mrg_cset__compute_repo_path_for_entry(pCtx,
															pMrg->pMrgCSet_FinalResult,
															pszKey_GidEntry,
															SG_FALSE,
															&pStringRepoPath_FinalResult)  );

	if (pMrgCSetEntry_FinalResult->tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		SG_mrg_cset_entry_conflict_flags flags = (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__UNDELETE__MASK);
		SG_bool bDeleteCausedOrphan = ((flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_CAUSED_ORPHAN) != 0);
		SG_mrg_cset_entry_conflict_flags flags_rest = (flags & ~SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_CAUSED_ORPHAN);
		SG_bool bOtherDeleteConflicts = (flags_rest != 0);

		// For directories, the phase "...because of directory modifications..." in the reason
		// refers to MOVE/RENAME/ATTRBITS/XATTRS on the directory and not the list of files in
		// the directory.

		if (bDeleteCausedOrphan)
		{
			if (bOtherDeleteConflicts)
				pszReason = "DIRECTORY delete in baseline cancelled because of directory modifications and entries created in another branch.";
			else
				pszReason = "DIRECTORY delete in baseline cancelled because of entries created in another branch.";
		}
		else
		{
			pszReason = "DIRECTORY delete in baseline cancelled because of directory modifications in another branch.";
		}

		SG_ERR_CHECK(  SG_wd_plan__get_directory(pCtx, pMrg->p_wd_plan,
												 pszKey_GidEntry,
												 SG_string__sz(pStringRepoPath_FinalResult),
												 SG_pathname__sz(pPath_Allocated),
												 pMrgCSetEntry_FinalResult->attrBits,
												 pMrgCSetEntry_FinalResult->bufHid_XAttr,
												 pszReason)  );
	}
	else if (pMrgCSetEntry_FinalResult->tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
	{
		if (pMrgCSetEntry_FinalResult->bufHid_Blob[0] == 0)
			SG_ERR_CHECK(  _sg_mrg__plan__get_file_from_divergent_edit(pCtx, pMrg,
																	   pMrgCSetEntry_FinalResult,
																	   pStringRepoPath_FinalResult,
																	   pPath_Allocated)  );
		else
		{
			// For FILES, the phrase "... cancelled because of modifications..." means any changes
			// both file edits to the content -and- moves/renames/attrbits/xattrs on the file.

			pszReason = "FILE delete in baseline cancelled because of modifications in another branch.";

			SG_ERR_CHECK(  SG_wd_plan__get_file_from_repo(pCtx, pMrg->p_wd_plan,
														  pszKey_GidEntry,
														  SG_string__sz(pStringRepoPath_FinalResult),
														  SG_pathname__sz(pPath_Allocated),
														  pMrgCSetEntry_FinalResult->bufHid_Blob,
														  pMrgCSetEntry_FinalResult->attrBits,
														  pMrgCSetEntry_FinalResult->bufHid_XAttr,
														  pszReason)  );
		}
	}
	else if (pMrgCSetEntry_FinalResult->tneType == SG_TREENODEENTRY_TYPE_SYMLINK)
	{
		SG_ERR_CHECK(  SG_wd_plan__get_symlink(pCtx, pMrg->p_wd_plan,
											   pszKey_GidEntry,
											   SG_string__sz(pStringRepoPath_FinalResult),
											   SG_pathname__sz(pPath_Allocated),
											   pMrgCSetEntry_FinalResult->bufHid_Blob,
											   pMrgCSetEntry_FinalResult->attrBits,
											   pMrgCSetEntry_FinalResult->bufHid_XAttr,
											   "SYMLINK delete in baseline cancelled becaus of modifications in another branch.")  );
	}
	else
	{
		SG_ASSERT(  (0)  );
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Allocated);
	SG_STRING_NULLFREE(pCtx, pStringRepoPath_FinalResult);
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_MASTER_PLAN__OVERRULE_H
