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
 * @file sg_wd_plan_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_WD_PLAN_PROTOTYPES_H
#define H_SG_WD_PLAN_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_wd_plan__free(SG_context * pCtx, SG_wd_plan * pPlan);
void SG_wd_plan__alloc(SG_context * pCtx, SG_wd_plan ** ppPlan);

#if defined(DEBUG)
#define SG_WD_PLAN__ALLOC(pCtx,ppNew)				SG_STATEMENT(	SG_wd_plan * _p = NULL;                                         \
																	SG_wd_plan__alloc(pCtx, &_p);									\
																	_sg_mem__set_caller_data(_p,__FILE__,__LINE__,"SG_wd_plan");	\
																	*(ppNew) = _p;                                                  )
#else
#define SG_WD_PLAN__ALLOC(pCtx,ppNew)				SG_wd_plan__alloc(pCtx,ppNew)
#endif

//////////////////////////////////////////////////////////////////

/**
 * Append a "move" or "rename" command to the plan.
 * When the plan is executed, this command should
 * move/rename it in the WD on disk and in the pendingtree.
 *
 * We are given the (gid,repo-path) for an entry.
 * The path-source and path-destination are either
 * in the WD or parking lot.
 *
 * This is intended for moving stuff to/from the parking lot.
 */
void SG_wd_plan__move_rename(SG_context * pCtx,
							 SG_wd_plan * pPlan,
							 const char * pszGid,
							 const char * pszRepoPath,
							 const char * pszPathSource,
							 const char * pszPathDestination,
							 const char * pszReason);

//////////////////////////////////////////////////////////////////

/**
 * Append a "remove" command to the plan.
 * When the plan is executed, this command should
 * remove the item from the WD on disk and REMOVE
 * it from the pendingtree.
 *
 * These commands DO NOT make any backups.
 */
void SG_wd_plan__remove_file(SG_context * pCtx,
							 SG_wd_plan * pPlan,
							 const char * pszGid,
							 const char * pszRepoPath,
							 const char * pszPath,
							 const char * pszReason);

void SG_wd_plan__remove_symlink(SG_context * pCtx,
								SG_wd_plan * pPlan,
								const char * pszGid,
								const char * pszRepoPath,
								const char * pszPath,
								const char * pszReason);

void SG_wd_plan__remove_directory(SG_context * pCtx,
								  SG_wd_plan * pPlan,
								  const char * pszGid,
								  const char * pszRepoPath,
								  const char * pszPath,
								  const char * pszReason);

//////////////////////////////////////////////////////////////////

/**
 * Append an "add_new" command to the plan.
 * When the plan is executed, this command will ADD it
 * to the pendingtree.  The item must exist on disk (and
 * may or may not have FOUND status.
 *
 * This GID *MUST* be a newly generated one for the item,
 * You SHOULD NOT use this for items that were added in
 * another branch and already have a GID.
 */
void SG_wd_plan__add_new_directory_to_pendingtree(SG_context * pCtx,
												  SG_wd_plan * pPlan,
												  const char * pszGid,
												  const char * pszRepoPath,
												  const char * pszPath,
												  const char * pszReason);

/**
 * UN-ADD the named directory from the pendingtree.  This DOES NOT
 * alter the WD.  This is intended to reverse the effects of the
 * "add_new" command.
 */
void SG_wd_plan__unadd_new_directory(SG_context * pCtx,
									 SG_wd_plan * pPlan,
									 const char * pszGid,
									 const char * pszRepoPath,
									 const char * pszPath,
									 const char * pszReason);

//////////////////////////////////////////////////////////////////

/**
 * Append a "get" command to the plan.
 * When the plan is executed, this command will fetch
 * an existing item from the repo and add it to the WD
 * and the pendingtree.
 *
 * This routine should be used when an entry is marked
 * as ADDED to the final-result (and not present in the
 * baseline)
 *
 * It will have the same GID that it had in the other branch.
 */
void SG_wd_plan__get_file_from_repo(SG_context * pCtx,
									SG_wd_plan * pPlan,
									const char * pszGid,
									const char * pszRepoPath_FinalResult,
									const char * pszPath,
									const char * pszHid_Blob,
									SG_int64 attrBits,
									const char * pszHid_XAttr,
									const char * pszReason);

void SG_wd_plan__get_file_from_auto_merge_result(SG_context * pCtx,
												 SG_wd_plan * pPlan,
												 const char * pszGid,
												 const char * pszRepoPath_FinalResult,
												 const char * pszPath,
												 const char * pszPath_AutoMergeResult,
												 SG_int64 attrBits,
												 const char * pszHid_XAttr,
												 const char * pszReason);

void SG_wd_plan__get_symlink(SG_context * pCtx,
							 SG_wd_plan * pPlan,
							 const char * pszGid,
							 const char * pszRepoPath_FinalResult,
							 const char * pszPath,
							 const char * pszHid_Blob,
							 SG_int64 attrBits,
							 const char * pszHid_XAttr,
							 const char * pszReason);

void SG_wd_plan__get_directory(SG_context * pCtx,
							   SG_wd_plan * pPlan,
							   const char * pszGid,
							   const char * pszRepoPath_FinalResult,
							   const char * pszPath,
							   SG_int64 attrBits,
							   const char * pszHid_XAttr,
							   const char * pszReason);

//////////////////////////////////////////////////////////////////

/**
 * Append an "alter" command to the plan.
 * When the plan is executed, this command will modify an entry
 * that is present in the WD/baseline and make it look
 * like it does in the final-result.
 *
 * This routine should be used when an entry is present in both
 * the baseline and the final-result (matched by GID).
 *
 * Note that we take both the repo-path and WD absolute path for
 * both the before and after.  This is because the absolute paths
 * are transient paths based upon our position in the plan.
 */

void SG_wd_plan__alter_file_from_repo(SG_context * pCtx,
									  SG_wd_plan * pPlan,
									  const char * pszGid,
									  const char * pszRepoPath_FinalResult,  const char * pszRepoPath_Baseline,
									  const char * pszPath_FinalResult,      const char * pszPath_Baseline,
									  const char * pszHid_Blob_FinalResult,  const char * pszHid_Blob_Baseline,
									  SG_int64 attrBits_FinalResult,         SG_int64 attrBits_Baseline,
									  const char * pszHid_XAttr_FinalResult, const char * pszHid_XAttr_Baseline,
									  const char * pszReason);

void SG_wd_plan__alter_file_from_auto_merge_result(SG_context * pCtx,
												   SG_wd_plan * pPlan,
												   const char * pszGid,
												   const char * pszRepoPath_FinalResult,  const char * pszRepoPath_Baseline,
												   const char * pszPath_FinalResult,      const char * pszPath_Baseline,
												   const char * pszPath_AutoMergeResult,  const char * pszHid_Blob_Baseline,
												   SG_int64 attrBits_FinalResult,         SG_int64 attrBits_Baseline,
												   const char * pszHid_XAttr_FinalResult, const char * pszHid_XAttr_Baseline,
												   const char * pszReason);

void SG_wd_plan__alter_symlink(SG_context * pCtx,
							   SG_wd_plan * pPlan,
							   const char * pszGid,
							   const char * pszRepoPath_FinalResult,  const char * pszRepoPath_Baseline,
							   const char * pszPath_FinalResult,      const char * pszPath_Baseline,
							   const char * pszHid_Blob_FinalResult,  const char * pszHid_Blob_Baseline,
							   SG_int64 attrBits_FinalResult,         SG_int64 attrBits_Baseline,
							   const char * pszHid_XAttr_FinalResult, const char * pszHid_XAttr_Baseline,
							   const char * pszReason);

void SG_wd_plan__alter_directory(SG_context * pCtx,
								 SG_wd_plan * pPlan,
								 const char * pszGid,
								 const char * pszRepoPath_FinalResult,  const char * pszRepoPath_Baseline,
								 const char * pszPath_FinalResult,      const char * pszPath_Baseline,
								 SG_int64 attrBits_FinalResult,         SG_int64 attrBits_Baseline,
								 const char * pszHid_XAttr_FinalResult, const char * pszHid_XAttr_Baseline,
								 const char * pszReason);

//////////////////////////////////////////////////////////////////

/**
 * Add a move/rename command to the wd-plan to move/rename an
 * existing file/symlink/directory.  This DOES NOT alter the
 * pendingtree.  It only manipulates the WD.
 *
 * This is intended to move the various .yours/.mine files
 * from tmp into the WD.
 */
void SG_wd_plan__move_entry(SG_context * pCtx,
							SG_wd_plan * pPlan,
							const SG_pathname * pPath_Source,
							const SG_pathname * pPath_Destination,
							const char * pszReason);

//////////////////////////////////////////////////////////////////


void SG_wd_plan_debug__dump_script(SG_context * pCtx, const SG_wd_plan * pPlan, const char * pszLabel);
//void SG_wd_plan_debug__dump_status(SG_context * pCtx, SG_wd_plan * pPlan, const char * pszLabel);

//////////////////////////////////////////////////////////////////

/**
 * Apply the wd-plan to the WD and PendingTree.
 */
void SG_wd_plan__execute(SG_context * pCtx, const SG_wd_plan * pPlan, SG_pendingtree * pPendingTree);

//////////////////////////////////////////////////////////////////

/**
 * Get a summary of the number of operations in the merge.
 *
 * You DO NOT own the returned structure.
 */
void SG_wd_plan__get_stats__ref(SG_context * pCtx, const SG_wd_plan * pPlan, const SG_wd_plan_stats ** ppStats);

//////////////////////////////////////////////////////////////////

/**
 * Format the details of the plan to something suitable for printing on the console.
 *
 * NOTE: The plan is a sequence of basically shell-like commands to re-arrange the WD
 * and/or pull stuff from the repo in order to convert the existing WD into what we
 * want for the final result.  IT IS NOT A DIFF OR A STATUS.  Some steps have repo-paths
 * and some do not; some are just 1 or 2 absolute pathnames within the WD.  Furthermore,
 * the pathnames may be transient because moves/renames on parent directories may also
 * be in the plan.
 * 
 * You must free the returned string.
 */
void SG_wd_plan__format__to_string(SG_context * pCtx, const SG_wd_plan * pPlan, SG_string ** ppStrOutput)

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_WD_PLAN_PROTOTYPES_H
