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
 * @file sg_mrg_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG_PROTOTYPES_H
#define H_SG_MRG_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_mrg__free(SG_context * pCtx, SG_mrg * pMrg);

/**
 * Allocate a pMrg structure for computing a merge on the version control tree.
 * This only allocates the structure and used the given pending-tree to find the
 * WD, REPO, BASELINE, and determine if the WD is dirty.
 *
 * Use one of the SG_mrg__compute_* routines to actually compute the merge in memory
 * and then SG_mrg__TODO to actually update the WD.
 */
void SG_mrg__alloc(SG_context * pCtx,
				   SG_pendingtree * pPendingTree,
				   SG_bool bComplainIfBaselineNotLeaf,
				   SG_mrg ** ppMrg_New);

#if defined(DEBUG)
#define SG_MRG__ALLOC(pCtx,pPendingTree,bComplain,ppMrg_New)		SG_STATEMENT(	SG_mrg * _pNew = NULL;										\
																					SG_mrg__alloc(pCtx,pPendingTree,bComplain,&_pNew);			\
																					_sg_mem__set_caller_data(_pNew,__FILE__,__LINE__,"SG_mrg");	\
																					*(ppMrg_New) = _pNew;										)
#else
#define SG_MRG__ALLOC(pCtx,pPendingTree,bComplain,ppMrg_New)		SG_mrg__alloc(pCtx,pPendingTree,bComplain,ppMrg_New)
#endif

/**
 * Compute the MERGE using the given CSETs and the (implied) BASELINE.
 * This ONLY computes what needs to be done (the wd_plan) and the
 * conflicts that we will have (the wd_issues).
 *
 * The BASELINE may or may not be present in the rbtree; it is implied.
 *
 * The caller still needs to call SG_mrg__apply_merge() to actually
 * update the WD and the pendingtree.
 */
void SG_mrg__compute_merge(SG_context * pCtx,
						   SG_mrg * pMrg,
						   const SG_rbtree * prbCSetsToMerge,
						   SG_uint32 * pNrUnresolvedIssues);

/**
 * Execute the wd_plan and make changes to the WD and the in-memory of the pendingtree.
 *
 * The caller still needs to call SG_pendingtree__save() to actually update the wd.json
 * and release the lock on it.
 */
void SG_mrg__apply_merge(SG_context * pCtx,
						 SG_mrg * pMrg);

/**
 * Fetch the WD-PLAN that we computed for the MERGE.  This plan
 * is the same for both --test and --verbose.
 */
void SG_mrg__get_wd_plan__ref(SG_context * pCtx,
							  SG_mrg * pMrg,
							  const SG_wd_plan ** pp_wd_plan);

/**
 * Fetch the computed list unresolved issues that the merge will produce.
 * This is valid after __compute_mrg.  Once __apply_mrg is called it is
 * not valid.
 */
void SG_mrg__get_wd_issues__ref(SG_context * pCtx,
								SG_mrg * pMrg,
								SG_bool * pbHaveIssues,
								const SG_varray ** ppvaIssues);

//////////////////////////////////////////////////////////////////

/**
 * Display preview of the merge.  After computing a merge, use this to
 * dump a diff/status report of the changes relative to the baseline.
 * This uses the information in the in-memory merge-result (both for the
 * baseline and the results) to SYNTHESIZE a diff/status report.  (We
 * can't use the actual treediff/diff_ext code because we haven't updated
 * the WD yet.)  In theory, this output should match what a "vv diff"
 * would report after this in-memory merge-result is actually applied.
 *
 * TODO 2010/05/07 Get rid of this preview and use the new wd_plan and conflict list.
 */
void SG_mrg__preview(SG_context * pCtx, SG_mrg * pMrg,
					 SG_diff_ext__compare_callback * pfnCompare,
					 SG_file * pFileStdout, SG_file * pFileStderr);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG_PROTOTYPES_H
