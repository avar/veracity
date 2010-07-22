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
 * @file sg_pendingtree_prototypes.h
 *
 * @details The Pending Tree is an object for managing the pending
 * changes to a version control tree.  It could also be called the
 * pending changeset.
 *
 * This header file describes the API for a pending tree.  The
 * implementation of this class is actually in sg_closet.c, since the
 * pending tree is stored in the closet.
 *
 * The pending tree is initialized with a root directory HID obtained
 * from a dag node.  After that, you can call various methods to make
 * changes to the tree.  This class demand-loads any treenodes
 * necessary to keep an accurate list of everything that is dirty.
 *
 * When the time comes, you can tell a pending tree to commit itself
 * to the repo.
 *
 * A pending tree is specific to a given working directory.  Its main
 * job is to tell us what the repo would look like if all the changes
 * in the working directory were committed.  This is the main reason
 * we call this a pending tree instead of a pending changeset.  It is
 * more than just a list of changes.  You can ask it questions about
 * any part the tree, not just the changed parts.
 *
 * The pending tree handles "bubble up" automatically.
 *
 * The pending tree handles the "two level lookup" needed to make sure
 * that operations on a working directory happen to the right objects
 * even after renames and moves may have happened.
 *
 * The pending tree API offers functions that allow individual
 * operations to be reverted.
 *
 * As noted above, the pending tree API can commit itself to the repo,
 * but it also can perform this operation at finer granularity,
 * committing only a portion of its pending changes instead of the
 * entire pending changeset.
 *
 * We have spoken in the past of managing the pending changeset as two
 * lists (a list of dirty tree nodes and a list of new file blobs).
 * The pending tree conceptually encapsulates and manages both of
 * these lists.
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_PENDINGTREE_PROTOTYPES_H
#define H_SG_PENDINGTREE_PROTOTYPES_H

BEGIN_EXTERN_C;

/**
 * This function creates the in-memory pending changeset object for
 * the given working directory.  If a pending changeset already exists
 * (in the closet) for this working directory, it will be loaded into
 * the in-memory object.  Otherwise, a new one is created, empty.
 *
 */
void SG_pendingtree__alloc(
	SG_context* pCtx,
	const SG_pathname* pPathWorkingDirectory,
    SG_bool bIgnoreWarnings,
	SG_pendingtree** ppThis
	);

/**
 * This function creates the in-memory pending changeset object for
 * the current working directory.  If a pending changeset already exists
 * (in the closet) for this working directory, it will be loaded into
 * the in-memory object.  Otherwise, a new one is created, empty.
 *
 */
void SG_pendingtree__alloc_from_cwd(
	SG_context* pCtx,
    SG_bool bIgnoreWarnings,
	SG_pendingtree** ppThis
	);

#if defined(DEBUG)
#define SG_PENDINGTREE__ALLOC(pCtx,pPathWorkingDirectory,bIgnoreWarnings,ppThis)	SG_STATEMENT(	SG_pendingtree * _pNew = NULL;												\
																									SG_pendingtree__alloc(pCtx,pPathWorkingDirectory,bIgnoreWarnings,&_pNew);	\
																									_sg_mem__set_caller_data(_pNew,__FILE__,__LINE__,"SG_pendingtree");			\
																									*(ppThis) = _pNew;															)
#else
#define SG_PENDINGTREE__ALLOC(pCtx,pPathWorkingDirectory,bIgnoreWarnings,ppThis)	SG_pendingtree__alloc(pCtx,pPathWorkingDirectory,bIgnoreWarnings,ppThis)
#endif

void SG_pendingtree__free(SG_context* pCtx, SG_pendingtree* pThis);

//////////////////////////////////////////////////////////////////

/**
 * Re-read the wd.json and rebuild the PTNODEs and etc.
 * This is ***ONLY*** necessary because I needed to do 2 operations
 * while holding the file lock when doing a MERGE.
 * [1] first i need to do a STATUS (aka scan_dir) to see if there is
 *     any dirt in the WD.  The STATUS modifies the in-memory PTNODES/
 *     pendingtree.  But normally, a STATUS does not get saved back
 *     to disk.
 * [2] Then the MERGE wants to combine the CSETS (using just Treenodes
 *     and treenode-entries) and then update the WD and the PTNODES
 *     and SAVE the pendingtree back to wd.json.
 * Because [1] calls scan_dir with "addremove=1, markimplicit=1" (and
 * causes all of the LOST/FOUND items to be added to the pendingtree
 * (and because the IMPLICIT bit is in temp_flags rather than saved_flags)
 * the SAVE does more than it thinks it does.
 *
 * This routine can be used between [1] and [2] to discard the dirt
 * created by [1] and give [2] a fresh view -- all without releasing
 * the VFILE lock.
 *
 * TODO 2010/07/02 Refactor everything that made the above comment
 * TODO            necessary.
 *
 */
void SG_pendingtree__reload_pendingtree_from_vfile(SG_context * pCtx,
												   SG_pendingtree * pPendingTree);

//////////////////////////////////////////////////////////////////

/**
 * Scan is a bad idea in general.  It's useful for testing, and we
 * can probably end up with a version which is useful for some
 * end-user scenarios.  But it's not a very scalable concept.
 *
 * One of the reasons that Git is considered to be so fast is that it
 * doesn't scan by default.  All files are explicitly added to the
 * pending changeset.
 *
 * TODO 2010/06/05 The above comment is very old.  Review and see if
 * TODO            we still think it is bad thing.
 *
 * This is primarily an external API for scan and used by the CLC.
 * It takes care of loading the IGNORES list.  It SAVES the result
 * of the scan to the wd.json.
 */
void SG_pendingtree__scan(SG_context* pCtx,
						  SG_pendingtree* pPendingTree,
						  SG_bool bRecursive,
						  const char* const* paszIncludes, SG_uint32 count_includes,
						  const char* const* paszExcludes, SG_uint32 count_excludes);

void SG_pendingtree__get_working_directory_top(
	SG_context* pCtx,
	const SG_pendingtree* pPendingTree,
	SG_pathname** ppPath);

/**
 * borrow the pathname to the "@" directory.
 */
void SG_pendingtree__get_working_directory_top__ref(
	SG_context* pCtx,
	const SG_pendingtree* pPendingTree,
	const SG_pathname** ppPath);

/**
 * This is an external API to scan the directory and automatically
 * ADD newly found items and REMOVE missing items.  It SAVES the
 * result of the scan to the wd.json.
 */
void SG_pendingtree__addremove(SG_context* pCtx,
							   SG_pendingtree* pPendingTree,
							   SG_uint32 count_items, const char** paszItems,
							   SG_bool bRecursive,
							   const char* const* pvaIncludes, SG_uint32 count_includes,
							   const char* const* pvaExcludes, SG_uint32 count_excludes,
							   SG_bool bTest);

/**
 * The list of INCLUDES/EXCLUDES apply to the things to be actually committed
 * (and not to any internal scans that we might need to make to ensure that we
 * are up to date before or afterwards).
 * Commit DOES NOT take an IGNORES list.
 */
void SG_pendingtree__commit(SG_context* pCtx,
							SG_pendingtree* pPendingTree,
							const SG_audit* pq,
							const SG_pathname* pPathRelativeTo,
							SG_uint32 count_items, const char** paszItems,
							SG_bool bRecursive,
							const char* const* paszIncludes, SG_uint32 count_includes,
							const char* const* paszExcludes, SG_uint32 count_excludes,
							const char* const* paszAssocs, SG_uint32 count_assocs,
							SG_dagnode** ppdn);

void SG_pendingtree__get_repo_path(
	SG_context* pCtx,
	SG_pendingtree* pPendingTree,
	const char* pszgid,
	SG_string** ppResult
	);

void SG_pendingtree__revert(SG_context* pCtx,
							SG_pendingtree* pPendingTree,
							SG_pendingtree_action_log_enum eActionLog,
							const SG_pathname* pPathRelativeTo,
							SG_uint32 count_items, const char** paszItems,
							SG_bool bRecursive,
							const char* const* paszIncludes, SG_uint32 count_includes,
							const char* const* paszExcludes, SG_uint32 count_excludes);

void SG_pendingtree__remove(SG_context* pCtx,
							SG_pendingtree* pPendingTree,
							const SG_pathname* pPathRelativeTo,
							SG_uint32 count_items, const char** paszItems,
							const char* const* paszIncludes, SG_uint32 count_includes,
							const char* const* paszExcludes, SG_uint32 count_excludes,
							SG_bool bTest);

void SG_pendingtree__remove_dont_save_pendingtree(SG_context* pCtx,
												  SG_pendingtree* pPendingTree,
												  const SG_pathname* pPathRelativeTo,
												  SG_uint32 count_items, const char** paszItems,
												  const char* const* paszIncludes, SG_uint32 count_includes,
												  const char* const* paszExcludes, SG_uint32 count_excludes,
												  const char* const* paszIgnores,  SG_uint32 count_ignores,
												  SG_bool bTest);

/**
 * See note above the function source to see why you probably
 * don't want to call this.
 */
void SG_pendingtree__hack__unadd_dont_save_pendingtree(SG_context * pCtx,
													   SG_pendingtree * pPendingTree,
													   const char * pszGid,
													   const SG_pathname * pPath_Input);

void SG_pendingtree__rename(
	SG_context* pCtx,
	SG_pendingtree* pPendingTree,
	const SG_pathname* pPathLocal,
	const char* pszNewName,
	SG_bool bIgnoreErrorNotFoundOnDisk
	);

void SG_pendingtree__rename_dont_save_pendingtree(
	SG_context* pCtx,
	SG_pendingtree* pPendingTree,
	const SG_pathname* pPathLocal,
	const char* pszNewName,
	SG_bool bIgnoreErrorNotFoundOnDisk
	);

void SG_pendingtree__move(
	SG_context* pCtx,
	SG_pendingtree* pPendingTree,
	const SG_pathname** pArrayPathsToMove,
	SG_uint32 nCountMoves,
	const SG_pathname* pPathMoveTo,
	SG_bool bForce
	);

void SG_pendingtree__move_dont_save_pendingtree(
	SG_context* pCtx,
	SG_pendingtree* pPendingTree,
	const SG_pathname* pPathMoveMe,
	const SG_pathname* pPathMoveTo,
	SG_bool bForce
	);

void SG_pendingtree__add(SG_context* pCtx,
						 SG_pendingtree* pPendingTree,
						 const SG_pathname* pPathRelativeTo,
						 SG_uint32 count_items, const char** paszItems,
						 SG_bool bRecursive,
						 const char* const* paszIncludes, SG_uint32 count_includes,
						 const char* const* paszExcludes, SG_uint32 count_excludes,
						 SG_bool bTest);

void SG_pendingtree__add_dont_save_pendingtree(SG_context* pCtx,
											   SG_pendingtree* pPendingTree,
											   const SG_pathname* pPathRelativeTo,
											   SG_uint32 count_items, const char** paszItems,
											   SG_bool bRecursive,
											   const char* const* paszIncludes, SG_uint32 count_includes,
											   const char* const* paszExcludes, SG_uint32 count_excludes,
											   const char* const* paszIgnores,  SG_uint32 count_ignores);

/**
 * Add an entry to the pendingtree using the given GID.
 * This is needed for MERGE.
 */
void SG_pendingtree__add_dont_save_pendingtree__with_gid(
	SG_context * pCtx,
	SG_pendingtree * pPendingTree,
	const char * pszGid,
	const SG_pathname * pPath);

/**
 * SG_pendingtree__update_baseline() is used primarily to update the contents
 * of the Working Directory to be relative to a different changeset -- almost
 * as if a fresh WD were created using the named changeset.  I say almost because
 * we do allow for dirt in the current WD and try to "move it forward" if possible.
 * This works best when your local dirt is completely independent of any changes
 * that have been made to a descendant changeset of your current baseline.
 *
 * When there is dirt in the WD, we do place restrictions on the target changeset
 * (such as being a descendant and on the same named branch) so that we have
 * some hope of the dirt being carried forward successfully.  (If you want to
 * warp to an arbitrary changeset, do a COMMIT or REVERT first and then warp
 * with a clean WD.)
 *
 * If the target changeset (HID) is NULL, we assume the head/tip of the
 * current branch (when we get around to doing named branches) or the leaf
 * descended from the current baseline.  If this is not unique/well-defined,
 * we fail (and let them try again and specify the target).
 *
 * We try to preserve the WD dirt as best as possible during the update.  This
 * means performing individual merges for files that are dirty in the WD and
 * that were modified in any of the changesets between the current baseline and
 * the target changeset.  As always, this can produce conflicts.
 *
 * If bForce is set, we ignore/discard all changes in the WD before updating.
 * This is equivalent to doing a REVERT-to-BASELINE (UNDO) before starting.
 *
 * If bTest is set, we report what *would-have-been-done* and quit.
 *
 */
void SG_pendingtree__update_baseline(
	SG_context* pCtx,
	SG_pendingtree* pPendingTree,
	const char* pszHid,
	SG_bool bForce,
	SG_pendingtree_action_log_enum eActionLog
	);

void SG_pendingtree__get_repo(
        SG_context* pCtx,
		SG_pendingtree* pPendingTree,
        SG_repo** ppRepo
        );

/**
 * Return a VArray containing the HIDs of the parent changeset(s)
 * for the Working Directory.  Normally, there is only one parent
 * and any changes (dirt) in the WD is considered to be relative
 * to it.  This has historically been called the "baseline".
 *
 * However, when we have an uncommitted merge, we will have
 * multiple parents.  Parents are generally considered to be
 * equivalent and they may be in random order in the pendingtree.
 * When the merge is committed, the parents will be sorted for HID
 * computation reasons.
 *
 * However, for user consistency, Parent[0] in the uncommitted merge
 * will be the baseline before the merge (so that any DIFF commands
 * will show the user all of the changes relative to their last baseline).
 *
 * The returned VArray is an allocated copy; you own it and must free it.
 * It remains valid after the pendingtree is unlocked/freed.
 */
void SG_pendingtree__get_wd_parents(SG_context* pCtx,
									SG_pendingtree * pPendingTree,
									SG_varray ** ppvaParents);

/**
 * Like __get_wd_parents() but without the alloc.  The VArray is
 * within the pendingtree and is only valid as long as the pendingtree
 * is valid.
 */
void SG_pendingtree__get_wd_parents__ref(SG_context* pCtx,
										 SG_pendingtree * pPendingTree,
										 const SG_varray ** ppvaParents);

/**
 * Set the changeset parent(s) (aka baseline) of the Working Directory.
 * Normally this is only updated after a COMMIT.  It may also be
 * set during an UPDATE, MERGE, or REVERT.
 *
 * We STEAL YOUR VARRAY and NULL your pointer to it.
 *
 * You must call __save at some point to get the new set of parent(s)
 * written to disk (along with the rest of the pendingtree stuff)
 */
void SG_pendingtree__set_wd_parents(SG_context * pCtx,
									SG_pendingtree * pPendingTree,
									SG_varray ** ppva_new_wd_parents);

/**
 * Convenience wrapper for __set_wd_parents() when you only have 1 parent.
 *
 * As with it, you must still call __save at some point.
 */
void SG_pendingtree__set_single_wd_parent(SG_context * pCtx,
										  SG_pendingtree * pPendingTree,
										  const char * psz_hid_parent);

/**
 * Store a varray[vhash] os unresolved issues with the pendingtree.
 * These are problems that MERGE had and could not automatically
 * resolve.  We put them in the wd.json so that the user can later
 * use the RESOLVE command and addess each issue.  We also use this
 * to prevent them from doing a COMMIT with outstanding issues.
 *
 * We STEAL YOUR VARRAY and NULL your pointer to it.
 *
 * You must call __save at some point to get the new set of issues
 * written to disk (along with the rest of the pendingtree stuff).
 */
void SG_pendingtree__set_wd_issues(SG_context * pCtx,
								   SG_pendingtree * pPendingTree,
								   SG_varray ** ppva_new_wd_issues);

/**
 * Clear the set of unresolved issues from the pendingtree.  This
 * removes the varray[vhash] from the pendingtree (and after a __save()
 * from the wd.json).
 *
 * You must call __save at some point to get the new set of issues
 * written to disk (along with the rest of the pendingtree stuff).
 */
void SG_pendingtree__clear_wd_issues(SG_context * pCtx,
									 SG_pendingtree * pPendingTree);

/**
 * Return a VARRAY containing the UNRESOLVED ISSUES caused by a MERGE.
 * You DO NOT own this.  This is the live version within the pendingtree
 * and only valid as long as the pendingtree is in memory.
 */
void SG_pendingtree__get_wd_issues__ref(SG_context * pCtx,
										SG_pendingtree * pPendingTree,
										SG_bool * pbHaveIssues,
										const SG_varray ** ppva_wd_issues);

/**
 * Set the resolved/unresolved STATUS on an individual ISSUE in the
 * array of ISSUES.
 *
 * THIS ONLY SETS THE STATUS OF THE ISSUE; IT DOES NOT ALTER THE WD
 * IN ANY WAY.  This is to allow the user to MANUALLY resolve/unresolve
 * an issue after MANUALLY making whatever changes were needed to the WD.
 *
 * This update the live version of the VARRAY within the pendingtree.
 *
 * You must call __save at some point to get the new set of issues
 * written to disk (along with the rest of the pendingtree stuff).
 */
void SG_pendingtree__set_wd_issue_status__dont_save_pendingtree(SG_context * pCtx,
																SG_pendingtree * pPendingTree,
																const SG_vhash * pvhIssue,
																SG_pendingtree_wd_issue_status status);

/**
 * Set the STATUS on an individual ISSUE and SAVE the pendingtree to disk.
 */
void SG_pendingtree__set_wd_issue_status(SG_context * pCtx,
										 SG_pendingtree * pPendingTree,
										 const SG_vhash * pvhIssue,
										 SG_pendingtree_wd_issue_status status);

/**
 * See if there are any unresolved issues.
 */
void SG_pendingtree__count_unresolved_wd_issues(SG_context * pCtx,
												SG_pendingtree * pPendingTree,
												SG_uint32 * pNrUnresolved);

/**
 * update the "result" field on a step in an auto-merge plan.
 * This lets us remember that the user has performed a manual
 * merge which overrides the auto-merge result that we computed
 * when we applied the merge.  We can use this to avoid forcing
 * them to re-do the manual merge if they get interrupted at a
 * later step in the RESOLVE.
 */
void SG_pendingtree__set_wd_issue_plan_step_status__dont_save_pendingtree(SG_context * pCtx,
																		  SG_pendingtree * pPendingTree,
																		  const SG_vhash * pvhStep,
																		  SG_mrg_automerge_result r);

/**
 * Fetch the ISSUE associated with the given GID, if present.
 */
void SG_pendingtree__find_wd_issue_by_gid(SG_context * pCtx,
										  SG_pendingtree * pPendingTree,
										  const char * pszGidToFind,
										  SG_bool * pbFound,
										  const SG_vhash ** ppvhIssue);

/**
 * Save the in-memory pendingtree to disk.  This updates both the
 * "pending" and the "parents" parts of the "wd.json" file.
 */
void SG_pendingtree__save(
	SG_context* pCtx,
	SG_pendingtree* pPendingTree
	);

//////////////////////////////////////////////////////////////////

/**
 * TODO HACK This routine is a hack/compromise because the MERGE
 * TODO      code needs to do a diff in the middle of the computation
 * TODO      and we want to keep as much of the ptnodes in-memory
 * TODO      because stuff from the merge will likely touch clean
 * TODO      stuff.  ALSO, we need to PREVENT the diff code from
 * TODO      nulling the super-root (which is part of that
 * TODO      one-operation-and-save problem).
 * TODO
 * TODO      See SPRAWL-660.
 */
void SG_pendingtree__do_diff__keep_clean_objects(SG_context* pCtx, SG_pendingtree * pPendingTree,
														 const char * szHidArbitraryCSet,		/* optional */
														 SG_treediff2 * pTreeDiff_cache_owner,	/* optional */
														 SG_treediff2 ** ppTreeDiff);

/**
 * TODO HACK HACK This routine builds upon the previous hack and
 * TODO           reloads the VFILE/VHASH after the diff has been
 * TODO           computed because the caller doesn't want the
 * TODO           side-effects of the diff dirtying up the PTNODES
 * TODO           but can't just throw-away the pendingtree and
 * TODO           create a new one because of the file locking.
 */
void SG_pendingtree__do_diff__keep_clean_objects__reload(SG_context* pCtx, SG_pendingtree * pPendingTree,
														 const char * szHidArbitraryCSet,		/* optional */
														 SG_treediff2 * pTreeDiff_cache_owner,	/* optional */
														 SG_treediff2 ** ppTreeDiff);

//////////////////////////////////////////////////////////////////

void SG_pendingtree__diff_or_status(
		SG_context * pCtx,
		SG_pendingtree * pTree,
		const char * psz_cset_1,
		const char * psz_cset_2,
		SG_treediff2** ppTreeDiff);

void SG_pendingtree__get_gid_from_local_path(
		SG_context* pCtx,
		SG_pendingtree* pTree,
		SG_pathname* pLocalPathPathname,
		char** ppszgid);

void SG_pendingtree__get_gids_for_paths(SG_context * pCtx,
										SG_pendingtree * pPendingTree,
										SG_uint32 count_args,
										const char ** paszArgs,
										SG_stringarray* pReturnGIDs,
										SG_bool * bRootIsOneOfThem);


//////////////////////////////////////////////////////////////////

/**
 * Compute a tree-diff on the values in the pendingtree.  This allocates
 * a temporary SG_pendingtree, performs the diff, and creates and returns
 * a SG_treediff2 structure.
 *
 * DO NOT CALL THIS WHILE YOU ARE HOLDING ANOTHER SG_pendingtree object
 * open because of the file locking on the PT cache file.
 *
 * szHidArbitraryCSet, if set, is the HID of an arbitrary CSET and will
 * cause us to do a composite/joined cset-vs-baseline-vs-pendingtree
 * diff.  if not set, we do a simple baseline-vs-pendingtree diff.
 */
void SG_pendingtree__diff(SG_context* pCtx, SG_pendingtree* pPendingTree,
							  const char * szHidArbitraryCSet,		/* optional */
							  SG_treediff2 ** ppTreeDiff
							  );

/**
 * This version caused the created treediff to share the treenode cache
 * with the given treediff.  if the cache_owner is null, it behaves like
 * the other version.
 */
void SG_pendingtree__diff__shared(SG_context* pCtx, SG_pendingtree* pPendingTree,
									  const char * szHidArbitraryCSet,		/* optional */
									  SG_treediff2 * pTreeDiff_cache_owner,	/* optional */
									  SG_treediff2 ** ppTreeDiff
									 );

void SG_pendingtree__history(SG_context * pCtx, SG_pendingtree * pPendingTree,
						 SG_uint32 count_args, const char ** paszArgs,
						 const char ** pasz_changesets,
						 SG_uint32 nCountChangesets,
						 const char* pStrUser,
						 SG_uint32 nResultLimit,
						 SG_int64 nFromDate,
						 SG_int64 nToDate,
						 SG_bool bAllLeaves,
						 SG_varray ** ppVArrayResults);



/**
 * Get the "Action Log" of the low-level working-directory operations
 * that need to be / were performed.  This is only valid for verbs
 * that have a --test or --verbose option.
 *
 * You DO NOT own the returned varray.
 *
 * The returned varray pointer is NULL when empty.
 */
void SG_pendingtree__get_action_log(SG_context * pCtx,
									SG_pendingtree * pPendingTree,
									SG_varray ** ppResult);

//////////////////////////////////////////////////////////////////

/**
 * Get handle to VARRAY of portability warnings issued so far.
 *
 * You CAN append new warnings to the end.  These will
 * get returned to the creator of the pendingtree as if
 * generated by pendingtree-proper.
 *
 * You DO NOT own this.
 */
void SG_pendingtree__get_warnings(SG_context* pCtx,
								  SG_pendingtree* pPendingTree,
								  SG_varray** ppResult);

/**
 * Get various portability-warning settings.
 *
 * For pvaWarnings, this is a handle to VARRAY of portability
 * warnings issued so far.  You CAN append new warnings to the
 * end.  These will get returned to the creator of the
 * pendingtree as if generated by pendingtree-proper.
 * You DO NOT own this VARRAY.
 */
void SG_pendingtree__get_port_settings(SG_context * pCtx,
									   SG_pendingtree * pPendingTree,
									   SG_utf8_converter ** ppConverter,
									   SG_varray ** ppvaWarnings,
									   SG_portability_flags * pPortMask,
									   SG_bool * pbIgnoreWarnings);

//////////////////////////////////////////////////////////////////

/**
 * Return a VHASH containing the current TIMESTAMP CACHE for version controlled
 * files in the WD.  The values in this cache are the timestamps on the files
 * when we populated or last updated them.  We use this to help speed up scan-dir.
 *
 * This is a READ-ONLY handle to the LIVE VHASH in the pendingtree; it is only
 * valid as long as the pendingtree is in memory.
 *
 * You DO NOT own this.
 *
 * If this returns a NULL VHASH, then there is no timestamp cache.
 */
void SG_pendingtree__get_wd_timestamp_cache__ref(SG_context * pCtx,
												 SG_pendingtree * pPendingTree,
												 const SG_vhash ** ppvhTimestamps);

/**
 * Set the file timestamp cache to exactly the list given;
 * any other cache values are removed.
 *
 * This is intended for use when we create a WD and do an
 * initial GET of a revision into it.
 *
 * We steal your vhash and null your pointer to it.
 *
 * You must do a SG_pendingtree__save() at some point after this.
 */
void SG_pendingtree__set_wd_timestamp_cache(SG_context * pCtx,
											SG_pendingtree * pPendingTree,
											SG_vhash ** ppvhTimestamps);

/**
 * Clear the timestamp cache.  This removes all entries from the cache.
 * This is applied to the live in-memory view.
 *
 * You must do a SG_pendingtree__save() at some point after this.
 */
void SG_pendingtree__clear_wd_timestamp_cache(SG_context * pCtx,
											  SG_pendingtree * pPendingTree);

//////////////////////////////////////////////////////////////////
/**
 * Set an individual file timestamp in the cache.  This updates
 * the live in-memory vhash.
 *
 * You must do a SG_pendingtree__save() at some point after this.
 */
void SG_pendingtree__set_wd_file_timestamp__dont_save_pendingtree(SG_context * pCtx,
																  SG_pendingtree * pPendingTree,
																  const char * pszGid,
																  SG_int64 mtime_ms);

/**
 * Fetch an individual file timestamp from the cache.
 */
void SG_pendingtree__get_wd_file_timestamp(SG_context * pCtx,
										   SG_pendingtree * pPendingTree,
										   const char * pszGid,
										   SG_bool * pbFoundEntry,
										   SG_int64 * p_mtime_ms,
										   SG_int64 * p_clock_ms);
/**
 * Does the timestamp cache have and entry for the given GID
 * and if so, does the cached mtime match the given mtime
 * such that we can assume that whatever HID we have in the
 * pendingtree is still valid and that we don't need to recompute
 * the HID.
 */
void SG_pendingtree__is_wd_file_timestamp_valid(SG_context * pCtx,
												SG_pendingtree * pPendingTree,
												const char * pszGid,
												SG_int64 mtime_ms_observed_now,
												SG_bool * pbResult);

/**
 * Remove an individual file timestamp from the cache.
 */
void SG_pendingtree__clear_wd_file_timestamp(SG_context * pCtx,
											 SG_pendingtree * pPendingTree,
											 const char * pszGid);

//////////////////////////////////////////////////////////////////

/**
 * Use GID to find entry and return repo-path.
 * You own the returned string.
 */
void SG_pendingtree__find_repo_path_by_gid(SG_context * pCtx,
										   SG_pendingtree * pPendingTree,
										   const char * pszGid,
										   SG_string ** ppStrRepoPath);

//////////////////////////////////////////////////////////////////

/**
 * Format a RESOLVED/UNRESOLVED ISSUE from a MERGE and append to
 * the given string.
 *
 * Optionally also return the current repo-path for the issue.
 * You own this and must free it.
 */
void SG_pendingtree__format_issue(SG_context * pCtx,
								  SG_pendingtree * pPendingTree,
								  const SG_vhash * pvhIssue,
								  SG_string * pStrOutput,
								  SG_string ** ppStrRepoPath_Returned);

//////////////////////////////////////////////////////////////////

/**
 * load/build a temporary pendingtree, diff it against the baseline
 * and dump it to vhash (like we write to the vfile, but also include extra info).
 */
void SG_pendingtree_debug__export(SG_context * pCtx, SG_pathname * pPathWorkingDir,
								  SG_bool bProcessIgnores,
								  SG_treediff2 ** ppTreeDiff,
								  SG_vhash ** ppvhExport);

/**
 * dump already in-memory pendingtree to vhash and write it to the console.
 */
void SG_pendingtree_debug__dump_existing_to_console(SG_context * pCtx, SG_pendingtree * pPendingTree);

/**
 * Inspect the "wd.json" file.
 */
void SG_pendingtree_debug__get_wd_dot_json_stats(SG_context * pCtx,
												 const SG_pathname * pPathWorkingDir,
												 SG_uint32 * pNrParents,
												 SG_bool * pbHavePending);


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif //H_SG_PENDINGTREE_PROTOTYPES_H
