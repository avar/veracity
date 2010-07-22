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
 * @file sg_mrg__private_master_plan__entry.h
 *
 * @details We are used after the MERGE result has
 * been computed.  We are responsible for creating
 * the steps in the WD-PLAN to handle items present
 * in the final-result; this includes things only
 * present in the final-result and things that are
 * present in both.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_MASTER_PLAN__ENTRY_H
#define H_SG_MRG__PRIVATE_MASTER_PLAN__ENTRY_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * We get called for each entry in the final-result.
 * We get called in RANDOM GID order.
 *
 * Compare the item with the corresponding item in the
 * baseline and schedule whatever changes we need to make
 * to the WD.
 */
static SG_rbtree_foreach_callback _sg_mrg__plan__entry__cb;

static void _sg_mrg__plan__entry__cb(SG_context * pCtx,
									const char * pszKey_GidEntry,
									void * pVoidValue_MrgCSetEntry,
									void * pVoidData_Mrg)
{
	// pMrgCSetEntry_FinalResult represents the final/composite state of the entry.
	// and in most cases, we don't know where the pieces came from.  (we only completely
	// track that for conflicts.)  so we compute a diff between the original baseline
	// version and the final result.
	//
	// this may sound stupid (if we only consider a simple 2-leaf merge), but when we
	// get more fancy, we don't know where the individual parts of the result came from.

	SG_mrg_cset_entry * pMrgCSetEntry_FinalResult = (SG_mrg_cset_entry *)pVoidValue_MrgCSetEntry;
	SG_mrg * pMrg = (SG_mrg *)pVoidData_Mrg;
	SG_mrg_cset_entry * pMrgCSetEntry_Baseline;
	SG_bool bFound;

	// see if this entry in the final-result was already processed.  this can
	// happen when we go recursive to handle the parent directory of an entry.
	if (pMrgCSetEntry_FinalResult->markerValue & _SG_MRG__PLAN__MARKER__FLAGS__APPLIED)
		return;

	// insist that the parent directory of this entry in the final-result has
	// already been processed.  this helps when the entry was created or moved
	// into a created directory.  (this causes a little redundancy in our loop,
	// but that's ok.)
	if (pMrgCSetEntry_FinalResult->bufGid_Parent[0])
	{
		SG_mrg_cset_entry * pMrgCSetEntry_FinalResult_Parent;

		SG_ERR_CHECK(  SG_rbtree__find(pCtx, pMrg->pMrgCSet_FinalResult->prbEntries,
									   pMrgCSetEntry_FinalResult->bufGid_Parent,
									   &bFound,
									   (void **)&pMrgCSetEntry_FinalResult_Parent)  );
		SG_ASSERT(  (bFound)  );
		SG_ERR_CHECK(  _sg_mrg__plan__entry__cb(pCtx,
											   pMrgCSetEntry_FinalResult->bufGid_Parent,
											   pMrgCSetEntry_FinalResult_Parent,
											   pMrg)  );
	}

	// see if this entry has a peer in the baseline and/or the LCA.
	SG_ERR_CHECK(  SG_rbtree__find(pCtx, pMrg->pMrgCSet_Baseline->prbEntries, pszKey_GidEntry,
								   &bFound,
								   (void **)&pMrgCSetEntry_Baseline)  );
	if (bFound)
	{
		// the entry was also present in the baseline (and therefore on disk in the WD).

		SG_ASSERT(  ((pMrgCSetEntry_Baseline->markerValue & _SG_MRG__PLAN__MARKER__FLAGS__APPLIED) == 0)  );
		SG_ERR_CHECK(  _sg_mrg__plan__peer(pCtx, pMrg,
										   pszKey_GidEntry,
										   pMrgCSetEntry_FinalResult,
										   pMrgCSetEntry_Baseline)  );

		pMrgCSetEntry_FinalResult->markerValue |= _SG_MRG__PLAN__MARKER__FLAGS__APPLIED;
		pMrgCSetEntry_Baseline->markerValue |= _SG_MRG__PLAN__MARKER__FLAGS__APPLIED;
	}
	else
	{
		// the entry was not present in the baseline.  this could mean that it was
		// ADDED in the final-result --OR-- it was DELETED from the baseline.

		SG_mrg_cset_entry * pMrgCSetEntry_LCA;

		SG_ERR_CHECK(  SG_rbtree__find(pCtx, pMrg->pMrgCSet_LCA->prbEntries, pszKey_GidEntry,
									   &bFound,
									   (void **)&pMrgCSetEntry_LCA)  );
		if (bFound)
		{
			// the entry was present in the LCA.  therefore, it was DELETED from
			// the baseline.  this is like an ADD, but has some different settings.

			SG_ERR_CHECK(  _sg_mrg__plan__overrule_delete(pCtx, pMrg,
														  pszKey_GidEntry,
														  pMrgCSetEntry_FinalResult)  );
		}
		else
		{
			// the entry was not present in the baseline nor LCA.  this must be a real ADD.

			SG_ERR_CHECK(  _sg_mrg__plan__add(pCtx, pMrg,
											  pszKey_GidEntry,
											  pMrgCSetEntry_FinalResult)  );
		}

		pMrgCSetEntry_FinalResult->markerValue |= _SG_MRG__PLAN__MARKER__FLAGS__APPLIED;
	}

fail:
	return;
}

//////////////////////////////////////////////////////////////////

static void _sg_mrg__plan__entry(SG_context * pCtx, SG_mrg * pMrg)
{
	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx, pMrg->pMrgCSet_FinalResult->prbEntries, _sg_mrg__plan__entry__cb, pMrg)  );
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_MASTER_PLAN__ENTRY_H
