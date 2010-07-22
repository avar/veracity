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
 * @file sg_mrg__private_master_plan__peer.h
 *
 * @details Handle adding items to the WD-PLAN for entries that
 * are present in both the baseline and final-result.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_MASTER_PLAN__PEER_H
#define H_SG_MRG__PRIVATE_MASTER_PLAN__PEER_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

static void _sg_mrg__plan__peer(SG_context * pCtx, SG_mrg * pMrg,
								const char * pszKey_GidEntry,
								SG_mrg_cset_entry * pMrgCSetEntry_FinalResult,
								SG_mrg_cset_entry * pMrgCSetEntry_Baseline)
{
	SG_pathname * pPath_Baseline = NULL;
	SG_pathname * pPath_FinalResult = NULL;
	SG_string * pStringRepoPath_Baseline = NULL;
	SG_string * pStringRepoPath_FinalResult = NULL;
	SG_mrg_cset_entry_neq neq;

	// compare the baseline version and the final result version.  this version of 'neq'
	// reflects only the differences in those 2 versions (and none of the history).

	SG_ERR_CHECK(  SG_mrg_cset_entry__equal(pCtx,pMrgCSetEntry_FinalResult,pMrgCSetEntry_Baseline,&neq)  );
	if (neq == SG_MRG_CSET_ENTRY_NEQ__ALL_EQUAL)
	{
#if TRACE_MRG
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   ("DEBUG Baseline version unchanged in final-result: [gid %s]\n"),
								   pszKey_GidEntry)  );
#endif
	}
	else
	{
		SG_ERR_CHECK(  _sg_mrg__plan__compute_transient_path__source(pCtx, pMrg, pszKey_GidEntry, &pPath_Baseline)  );
		SG_ERR_CHECK(  _sg_mrg__plan__compute_transient_path__target(pCtx, pMrg, pszKey_GidEntry, &pPath_FinalResult)  );

		SG_ERR_CHECK(  SG_mrg_cset__compute_repo_path_for_entry(pCtx, pMrg->pMrgCSet_Baseline,    pszKey_GidEntry, SG_FALSE, &pStringRepoPath_Baseline)  );
		SG_ERR_CHECK(  SG_mrg_cset__compute_repo_path_for_entry(pCtx, pMrg->pMrgCSet_FinalResult, pszKey_GidEntry, SG_FALSE, &pStringRepoPath_FinalResult)  );

		if (pMrgCSetEntry_FinalResult->tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
		{
			SG_ERR_CHECK(  SG_wd_plan__alter_directory(pCtx, pMrg->p_wd_plan,
													   pszKey_GidEntry,
													   SG_string__sz(pStringRepoPath_FinalResult), SG_string__sz(pStringRepoPath_Baseline),
													   SG_pathname__sz(pPath_FinalResult),         SG_pathname__sz(pPath_Baseline),
													   pMrgCSetEntry_FinalResult->attrBits,        pMrgCSetEntry_Baseline->attrBits,
													   pMrgCSetEntry_FinalResult->bufHid_XAttr,    pMrgCSetEntry_Baseline->bufHid_XAttr,
													   "DIRECTORY altered between baseline and final-result.")  );
		}
		else if (pMrgCSetEntry_FinalResult->tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
		{
			if (pMrgCSetEntry_FinalResult->bufHid_Blob[0] != 0)
			{
				// something changed between the baseline and the final-result version
				// of the file, but it wasn't the content -- that came straight from
				// another branch.  we may still have other non-content changes/conflicts
				// that need to be dealt with.
				//
				// TODO the "reason" string is a bit vague.  it could really say what
				// TODO changed.
				SG_ERR_CHECK(  SG_wd_plan__alter_file_from_repo(pCtx, pMrg->p_wd_plan,
																pszKey_GidEntry,
																SG_string__sz(pStringRepoPath_FinalResult), SG_string__sz(pStringRepoPath_Baseline),
																SG_pathname__sz(pPath_FinalResult),         SG_pathname__sz(pPath_Baseline),
																pMrgCSetEntry_FinalResult->bufHid_Blob,     pMrgCSetEntry_Baseline->bufHid_Blob,
																pMrgCSetEntry_FinalResult->attrBits,        pMrgCSetEntry_Baseline->attrBits,
																pMrgCSetEntry_FinalResult->bufHid_XAttr,    pMrgCSetEntry_Baseline->bufHid_XAttr,
																"FILE altered between baseline and final-result.")  );
			}
			else if (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__AUTO_OK)
			{
				// auto-merge was successful, so we silently supply the auto-merge-result
				// file from tmp and then let the __alter_file code handle any other
				// non-content changes/conflicts.
				//
				// TODO the "reason" string is a bit vague (WRT the non-content changes).
				//
				// TODO Also, when in an n-way merge, the baseline version could be unchanged
				// TODO and the auto-merge could be for edits made in the various other
				// TODO branches.
				// TODO We may want to use that info to alter the "reason" message to
				// TODO indicate whether the baseline was involved in the auto-merge.

				SG_ERR_CHECK(  SG_wd_plan__alter_file_from_auto_merge_result(pCtx, pMrg->p_wd_plan,
																			 pszKey_GidEntry,
																			 SG_string__sz(pStringRepoPath_FinalResult), SG_string__sz(pStringRepoPath_Baseline),
																			 SG_pathname__sz(pPath_FinalResult),         SG_pathname__sz(pPath_Baseline),
																			 SG_pathname__sz(pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pPathAutoMergePlanResult),
																			 pMrgCSetEntry_Baseline->bufHid_Blob,
																			 pMrgCSetEntry_FinalResult->attrBits,        pMrgCSetEntry_Baseline->attrBits,
																			 pMrgCSetEntry_FinalResult->bufHid_XAttr,    pMrgCSetEntry_Baseline->bufHid_XAttr,
																			 "FILE content auto-merged.")  );
			}
			else
			{
				// auto-merge was not successful (or not attempted).  splat a copy of
				// each unique version of the file into the directory and let them
				// deal with it afterwards.

				SG_ERR_CHECK(  _sg_mrg__plan__alter_file_from_divergent_edit(pCtx, pMrg,
																			 pMrgCSetEntry_FinalResult,   pMrgCSetEntry_Baseline,
																			 pStringRepoPath_FinalResult, pStringRepoPath_Baseline,
																			 pPath_FinalResult,           pPath_Baseline)  );
			}
		}
		else if (pMrgCSetEntry_FinalResult->tneType == SG_TREENODEENTRY_TYPE_SYMLINK)
		{
			SG_ERR_CHECK(  SG_wd_plan__alter_symlink(pCtx, pMrg->p_wd_plan,
													 pszKey_GidEntry,
													 SG_string__sz(pStringRepoPath_FinalResult), SG_string__sz(pStringRepoPath_Baseline),
													 SG_pathname__sz(pPath_FinalResult),         SG_pathname__sz(pPath_Baseline),
													 pMrgCSetEntry_FinalResult->bufHid_Blob,     pMrgCSetEntry_Baseline->bufHid_Blob,
													 pMrgCSetEntry_FinalResult->attrBits,        pMrgCSetEntry_Baseline->attrBits,
													 pMrgCSetEntry_FinalResult->bufHid_XAttr,    pMrgCSetEntry_Baseline->bufHid_XAttr,
													 "SYMLINK altered between baseline and final-result.")  );
		}
		else
		{
			SG_ASSERT(  (0)  );
		}
	}

	// TODO deal with conflict/collision portion of wd.json

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Baseline);
	SG_PATHNAME_NULLFREE(pCtx, pPath_FinalResult);
	SG_STRING_NULLFREE(pCtx, pStringRepoPath_Baseline);
	SG_STRING_NULLFREE(pCtx, pStringRepoPath_FinalResult);
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_MASTER_PLAN__PEER_H
