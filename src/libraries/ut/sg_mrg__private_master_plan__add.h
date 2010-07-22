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
 * @file sg_mrg__private_master_plan__add.h
 *
 * @details We are used after the MERGE result has
 * been computed.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_MASTER_PLAN__ADD_H
#define H_SG_MRG__PRIVATE_MASTER_PLAN__ADD_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * We are responsible for creating
 * the steps in the WD-PLAN to handle items that
 * are only present in the final-result and not in
 * the baseline.
 *
 * For these items, we need to create an item in
 * the WD using content from the repo (or possibly
 * the auto-merge result when doing an n-way merge).
 *
 * We also need to do a "special-add" for the entry
 * in the pendingtree so that it gets the existing GID.
 *
 * For this "special-add" entries, the repo-path
 * is in terms of the final-result (since the entry
 * wasn't present in the baseline and therefore
 * can't have a baseline-repo-path).
 *
 * Note that just because the entry was not present
 * in the baseline doesn't mean that there aren't
 * conflict/collision issues (either from other
 * entries or (when doing an n-way merge) from multiple
 * versions of this entry in the other branches.
 * An earlier step made an ARBITRARY choice to
 * resolve most of these (such as when we have a
 * divergent rename), so we should be able to at
 * least create the entry, possibly with a PROVISIONAL
 * name/location/attribute/etc.
 *
 * TODO The remaining conflict/collision issues
 * TODO are written to the "conflicts" area of wd.json
 * TODO so that we can have a smart "vv resolve"
 * TODO command.
 */
static void _sg_mrg__plan__add(SG_context * pCtx, SG_mrg * pMrg,
							   const char * pszKey_GidEntry,
							   SG_mrg_cset_entry * pMrgCSetEntry_FinalResult)
{
	SG_pathname * pPath_Allocated = NULL;
	SG_string * pStringRepoPath_FinalResult = NULL;

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
		SG_ERR_CHECK(  SG_wd_plan__get_directory(pCtx, pMrg->p_wd_plan,
												 pszKey_GidEntry,
												 SG_string__sz(pStringRepoPath_FinalResult),
												 SG_pathname__sz(pPath_Allocated),
												 pMrgCSetEntry_FinalResult->attrBits,
												 pMrgCSetEntry_FinalResult->bufHid_XAttr,
												 "DIRECTORY created in other branch.")  );
	}
	else if (pMrgCSetEntry_FinalResult->tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
	{
		if (pMrgCSetEntry_FinalResult->bufHid_Blob[0] == 0)
			SG_ERR_CHECK(  _sg_mrg__plan__get_file_from_divergent_edit(pCtx, pMrg,
																	   pMrgCSetEntry_FinalResult,
																	   pStringRepoPath_FinalResult,
																	   pPath_Allocated)  );
		else
			SG_ERR_CHECK(  SG_wd_plan__get_file_from_repo(pCtx, pMrg->p_wd_plan,
														  pszKey_GidEntry,
														  SG_string__sz(pStringRepoPath_FinalResult),
														  SG_pathname__sz(pPath_Allocated),
														  pMrgCSetEntry_FinalResult->bufHid_Blob,
														  pMrgCSetEntry_FinalResult->attrBits,
														  pMrgCSetEntry_FinalResult->bufHid_XAttr,
														  "FILE created in other branch.")  );
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
											   "SYMLINK created in other branch.")  );
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

#endif//H_SG_MRG__PRIVATE_MASTER_PLAN__ADD_H
