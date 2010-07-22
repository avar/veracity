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
 * @file sg_mrg__private_master_plan.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_MASTER_PLAN_H
#define H_SG_MRG__PRIVATE_MASTER_PLAN_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////

/**
 * To be called after we have computed the in-memory merge and
 * have the MERGE-GOAL in pMrgCSet_FinalResult.
 *
 * Our job is to figure out the sequence of disk operations
 * required to convert the current WD (which has a CLEAN view
 * of the BASELINE) into the FinalResult such that the user
 * could (if they want to) RUN the master plan and then COMMIT.
 *
 * This WD juggling is, in spirit, very similar to what we need
 * to do in REVERT and UPDATE to transform the WD.
 */
void SG_mrg__prepare_master_plan(SG_context * pCtx, SG_mrg * pMrg)
{
	SG_ERR_CHECK(  SG_WD_PLAN__ALLOC(pCtx, &pMrg->p_wd_plan)  );

	// set the markers to 0 on all of the entries in the baseline
	// and final-result csets so that we know which ones we have
	// visited and which ones are peerless and the transient status
	// as we build the plan.

	SG_ERR_CHECK(  SG_mrg_cset__set_all_markers(pCtx,pMrg->pMrgCSet_FinalResult, _SG_MRG__PLAN__MARKER__FLAGS__ZERO)  );
	SG_ERR_CHECK(  SG_mrg_cset__set_all_markers(pCtx,pMrg->pMrgCSet_Baseline,    _SG_MRG__PLAN__MARKER__FLAGS__ZERO)  );

	// if we have allowable dirt in the WD, figure out if
	// it interferes with the merge result.  make plans to
	// push it out of the way.

	if (pMrg->pTreeDiff_Baseline_WD_Dirt)
		SG_ERR_CHECK(  _mrg__handle_dirt(pCtx, pMrg)  );

	// figure out what transient collisions we have and make
	// plans to move stuff to the parking lot.

	SG_ERR_CHECK(  _sg_mrg__plan__park(pCtx, pMrg)  );

	// for each entry in the final-result, look at it and the
	// corresponding entry in the baseline and decide what needs
	// to be done to the WD and the pendingtree.  add this to the
	// plan.

	SG_ERR_CHECK(  _sg_mrg__plan__entry(pCtx, pMrg)  );

	// delete anything that was in the baseline and
	// not in the final-result.

	SG_ERR_CHECK(  _sg_mrg__plan__delete(pCtx, pMrg)  );

	// if __plan__park() added "@/.sgtemp" and "@/.sgtemp/parkinglot" to
	// pendingtree, remove them.

	SG_ERR_CHECK(  _sg_mrg__plan__remove_parkinglot(pCtx, pMrg)  );

#if 0 && defined(DEBUG)
	SG_ERR_IGNORE(  SG_wd_plan_debug__dump_script(pCtx, pMrg->p_wd_plan, "Merge")  );
#endif

fail:
	return;
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_MASTER_PLAN_H
