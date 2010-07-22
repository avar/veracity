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
 * @file sg_pendingtree__private_update_baseline.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_PENDINGTREE__PRIVATE_UPDATE_BASELINE_H
#define H_SG_PENDINGTREE__PRIVATE_UPDATE_BASELINE_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#define TRACE_UPDATE		0
#else
#define TRACE_UPDATE		0
#endif

//////////////////////////////////////////////////////////////////

struct _pt_update_data
{
	SG_pendingtree *					pPendingTree;					// we do not own this.

	SG_inv_dirs *						pInvDirs;						// we own this.
	SG_vector *							pVecCycles;						// vec[struct sg_ptnode *] of moved dirs marked PARKED_FOR_CYCLE    we do not own these.

	SG_treediff2 *						pTreeDiff_composite;			// See Figure 3.   we own this.

	SG_uint32							nrChanges_goalcset_vs_baseline;
	SG_uint32							nrChanges_baseline_vs_wd;
	SG_uint32							nrChanges_composite;
	SG_uint32							nrChanges_total;

	SG_dagquery_relationship			dqRel;

	SG_bool								bForce;
	SG_bool								bInDoFull;

	char								bufHid_goalcset[SG_HID_MAX_BUFFER_LENGTH];
	char								bufHid_baseline[SG_HID_MAX_BUFFER_LENGTH];
};

//////////////////////////////////////////////////////////////////

static void _pt_update__move_entry_with_possible_rename(SG_context * pCtx,
														struct _pt_update_data * pud,
														const char * pszGid,
														SG_treediff2_ObjectData * pOD,
														SG_bool bTouchWD);
static void _pt_update__rename_entry(SG_context * pCtx,
									 struct _pt_update_data * pud,
									 const char * pszGid,
									 SG_treediff2_ObjectData * pOD,
									 SG_bool bTouchWD);
static void _pt_update__change_attrbits_on_entry(SG_context * pCtx,
												 struct _pt_update_data * pud,
												 const char * pszGid,
												 SG_treediff2_ObjectData * pOD);
static void _pt_update__change_xattrs_on_entry(SG_context * pCtx,
											   struct _pt_update_data * pud,
											   const char * pszGid,
											   SG_treediff2_ObjectData * pOD);
static void _pt_update__update_dir_hid(SG_context * pCtx,
									   struct _pt_update_data * pud,
									   const char * pszGid,
									   SG_treediff2_ObjectData * pOD);

//////////////////////////////////////////////////////////////////

/**
 * Determine how the goal changeset and the baseline are related
 * in the DAG.  If there is dirt in the WD that we need to try to
 * preserve, we require the changests to be ANCESTOR/DESCENDANT of
 * each other so that conceptually the baseline is between the goal
 * changeset and the WD for merge purposes.  See Figure 1.
 */
static void _pt_update__verify_changeset_relationship(SG_context * pCtx,
													  struct _pt_update_data * pud)
{
	SG_ASSERT(  (strcmp(pud->bufHid_goalcset, pud->bufHid_baseline) != 0)  );
	SG_ASSERT(  (pud->nrChanges_goalcset_vs_baseline > 0)  );
	SG_ASSERT(  (pud->nrChanges_baseline_vs_wd > 0)  );
	SG_ASSERT(  (!pud->bForce)  );

	// We already know how the 2 changesets are related if we had to
	// compute the best head/tip.  Otherwise, walk the DAG and look it up.
	//
	// For a normal "move it forward" update, the goal changeset is a descendant of
	// the current baseline and we should get SG_DAGQUERY_RELATIONSHIP_DESCENDANT.

	if (pud->dqRel == SG_DAGQUERY_RELATIONSHIP__UNKNOWN)
		SG_ERR_CHECK_RETURN(  SG_dagquery__how_are_dagnodes_related(pCtx, pud->pPendingTree->pRepo,
																	pud->bufHid_goalcset, pud->bufHid_baseline,
																	&pud->dqRel)  );

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   "_pt_update: [baseline %s] [target %s] [dagnode relationship %d]\n",
							   pud->bufHid_baseline,
							   pud->bufHid_goalcset,
							   pud->dqRel)  );
#endif

	switch (pud->dqRel)
	{
//	case SG_DAGQUERY_RELATIONSHIP__UNKNOWN:
//	case SG_DAGQUERY_RELATIONSHIP__SAME:			// we already checked HIDs for equality
//	case SG_DAGQUERY_RELATIONSHIP__UNRELATED:		// should not happen; probably different dagnums
//	case SG_DAGQUERY_RELATIONSHIP__PEER:			// siblings/cousins/... so folding/merging in of WD dirt not necessarily safe
	default:
		SG_ERR_THROW2_RETURN(  SG_ERR_DAGNODES_UNRELATED,
							   (pCtx, "Goal changeset [%s] is not descendant or ancestor of the current baseline [rel %d].",
								pud->bufHid_goalcset, pud->dqRel)  );

	case SG_DAGQUERY_RELATIONSHIP__DESCENDANT:		// target is a descendant of the current baseline
	case SG_DAGQUERY_RELATIONSHIP__ANCESTOR:		// target is an ancestor of the current baseline
		return;
	}
}

//////////////////////////////////////////////////////////////////

/**
 * UPDATE to SELF.
 *
 * This could either be a NOOP or an error, but let's be nice.
 */
static void _pt_update__do__goal_hid_equals_baseline_hid(SG_context * pCtx,
														 struct _pt_update_data * pud)
{
	SG_ASSERT(  (strcmp(pud->bufHid_goalcset, pud->bufHid_baseline) == 0)  );

	if (pud->bForce)
	{
		// since the HIDs didn't change, this is really just a regular "REVERT --all".

#if TRACE_UPDATE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "_pt_update: converting trivial 'UPDATE --force to BASELINE' to 'REVERT --all'\n")  );
#endif
		SG_ERR_CHECK_RETURN(  SG_pendingtree__revert(pCtx, pud->pPendingTree, pud->pPendingTree->eActionLog,
													 NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	}
	else
	{
#if TRACE_UPDATE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "_pt_update: ignoring trivial 'UPDATE to BASELINE'\n")  );
#endif
	}
}

//////////////////////////////////////////////////////////////////

/**
 * the contents of the version control tree for the goal and the
 * baseline are identical (even though they have different HIDs).
 * this can happen because the changesets include their parent
 * HID(s) in the hash.
 *
 * this is like a NOOP.  All we need to do is to "claim" the goal
 * changeset as our new baseline.
 */
static void _pt_update__do__goal_content_identical_to_baseline(SG_context * pCtx,
															   struct _pt_update_data * pud)
{
	SG_ASSERT(  (pud->nrChanges_goalcset_vs_baseline == 0)  );

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   "_pt_update: doing trivial 'UPDATE to changeset %s'\n",
							   pud->bufHid_goalcset)  );
#endif

	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
	{
		SG_ERR_CHECK(  SG_strcpy(pCtx,
								 pud->bufHid_baseline, sizeof(pud->bufHid_baseline),
								 pud->bufHid_goalcset)  );

		SG_ERR_CHECK(  SG_pendingtree__set_single_wd_parent(pCtx, pud->pPendingTree, pud->bufHid_baseline)  );

		SG_ERR_CHECK(  SG_pendingtree__save(pCtx, pud->pPendingTree)  );
	}

fail:
	return;
}

//////////////////////////////////////////////////////////////////

static void _pt_update__bind(SG_context * pCtx,
							 struct _pt_update_data * pud,
							 const char * pszGid,
							 SG_treediff2_ObjectData * pOD,
							 SG_bool bCurrentEntryName,
							 SG_bool bCurrentParent,
							 SG_bool bSource,
							 SG_bool bTarget)
{
	const char * pszEntryName;
	const char * pszGid_Parent;
	const char * pszRepoPath_Parent_CurrentPath = NULL;
	SG_treenode_entry_type tneType;
	SG_bool bFound;

	if (bCurrentEntryName)
		SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_name(pCtx, pOD, &pszEntryName)  );
	else
		SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_old_name(pCtx, pOD, &pszEntryName)  );

	if (bCurrentParent)
		SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_parent_gid(pCtx, pOD, &pszGid_Parent)  );
	else
		SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_moved_from_gid(pCtx, pOD, &pszGid_Parent)  );

	SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_tneType(pCtx, pOD, &tneType)  );

	if (bSource)
	{
		const SG_treediff2_ObjectData * pOD_Parent;

		SG_ERR_CHECK_RETURN(  SG_treediff2__get_ObjectData(pCtx, pud->pTreeDiff_composite, pszGid_Parent, &bFound, &pOD_Parent)  );
		SG_ASSERT(  (bFound)  );

		// get the repo-path of whichever parent directory *AS IT CURRENTLY EXISTS* in the WD.
		// this is important because the current WD on disk is the starting point for the juggling
		// that we must do to create a WD that looks like the goal changeset (with or without the
		// dirt in the WD).

		SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_repo_path(pCtx, pOD_Parent, &pszRepoPath_Parent_CurrentPath)  );
	}

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   ("_pt_update__bind_current: [current %c%c] [gid %s / %s] [claiming %c%c] [%s][%s] [tneType %d]\n"),
							   ((bCurrentEntryName) ? 'n' : 'o'), ((bCurrentParent) ? 'c' : 'f'),
							   pszGid_Parent, pszGid,
							   ((bSource) ? 's' : ' '), ((bTarget) ? 't' : ' '),
							   ((pszRepoPath_Parent_CurrentPath) ? pszRepoPath_Parent_CurrentPath : "(null)"),
							   pszEntryName,
							   tneType)  );
#endif

	// TODO make sure hard conflicts get thrown with a useful extra-info message.
	// TODO REVERT didn't need this because we were reverting from the current view
	// TODO in the WD to the baseline view (and the only issue was conflicts on
	// TODO PARTIAL REVERTS).  But here, an UPDATE with dirty WD is like a mini MERGE.
	// TODO So we can get filename collisions from independent entries being given
	// TODO the same name in different (micro) branches.

	if (bSource)
		SG_ERR_CHECK_RETURN(  SG_inv_dirs__bind_source(pCtx, pud->pInvDirs,
													   pszGid_Parent, pszRepoPath_Parent_CurrentPath,
													   pszGid, pszEntryName,
													   (tneType == SG_TREENODEENTRY_TYPE_DIRECTORY),
													   (void *)pOD,
													   SG_TRUE,
													   NULL, NULL)  );

	if (bTarget)
		SG_ERR_CHECK_RETURN(  SG_inv_dirs__bind_target(pCtx, pud->pInvDirs,
													   pszGid_Parent,
													   pszGid, pszEntryName, (void *)pOD,
													   SG_TRUE,
													   NULL, NULL)  );
}

//////////////////////////////////////////////////////////////////

/**
 * __do__full_composite_update: step 1.
 *
 * [1] scan each difference between the WD and the goal changeset and
 *     build an "inverted tree" that maps:
 *     (directory-gid-object, entryname) --> { entry-gid-object_source, entry-gid-object_target }
 *     and claim:
 *     (directory-gid-object, entryname) in either the SOURCE or TARGET versions of the tree.
 *
 *     NOTE on Terminology: for UPDATE, we call the current WD the "SOURCE"
 *                          and the goal changeset the "TARGET".
 *
 *     This tells us who "owns" each entryname in each directory in each version of the tree.
 *
 *     This lets us identify when there HARD are conflicts between the WD and goal changeset.
 *     For example, if I do "vv mv foo bar" in my WD and someone else does "vv mv xyz bar"
 *     in their WD and commits it, then when I try to update to their changeset and roll my
 *     changes forward, I'll get a collision on "bar".  I'm going to let this (and all the other
 *     strange ways stuff like that can happen) fail and advise them to COMMIT the WD dirt and
 *     then do a real MERGE.
 *
 *     This also serves as the basis for deciding who needs to use the parking lot.  For example,
 *     if the cummulative changes between the baseline and the goal chanseset contain something
 *     like: "vv mv foo bar" and "vv mv xxx foo" and those files were unchanged in the WD,
 *     then we need to replicate that shuffle -- but there are order dependencies there.  So we
 *     detect the contention on "foo" and create a temp file in the parking lot for it.
 */
static void _pt_update__do__full_1(SG_context * pCtx,
								   struct _pt_update_data * pud)
{
	SG_treediff2_iterator * pIter = NULL;
	const char * pszGid_k;
	SG_treediff2_ObjectData * pOD_k;
	SG_diffstatus_flags dsFlags_goal_vs_baseline;
	SG_diffstatus_flags dsFlags_baseline_vs_wd;
	SG_diffstatus_flags dsFlags_composite;
	SG_diffstatus_flags dsFlags_baseline_vs_goal;
	SG_bool bOK;

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: begin [1]\n")  );
#endif

	SG_ASSERT(  (pud->bInDoFull)  );

	SG_ERR_CHECK(  SG_treediff2__iterator__first(pCtx, pud->pTreeDiff_composite, &bOK, &pszGid_k, &pOD_k, &pIter)  );
	while (bOK)
	{
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_composite_dsFlags_by_part(pCtx, pOD_k,
																			   &dsFlags_goal_vs_baseline,
																			   &dsFlags_baseline_vs_wd,
																			   &dsFlags_composite)  );

		// Since we built the composite treediff using Figure 3 rather
		// than Figure 1, we need to swap the direction of the goal-vs-baseline
		// flags so that we can (sanely) work with baseline on the left of both.
		//
		// WARNING: some of the tests below are going to look somewhat inside-out
		// WARNING: because of this.

		SG_ERR_CHECK(  SG_treediff2__Invert_dsFlags(pCtx, dsFlags_goal_vs_baseline, &dsFlags_baseline_vs_goal)  );

		if (dsFlags_baseline_vs_wd == SG_DIFFSTATUS_FLAGS__ZERO)
		{
			// entry was not changed in WD/pendingtree relative to baseline.
			// use the changes made in the goal changeset (relative to the baseline) to populate the result.

			SG_ASSERT(   (dsFlags_baseline_vs_goal != SG_DIFFSTATUS_FLAGS__ZERO)         );
			SG_ASSERT(  ((dsFlags_baseline_vs_goal &  SG_DIFFSTATUS_FLAGS__FOUND) == 0)  );	// not possible for cset-vs-cset diff
			SG_ASSERT(  ((dsFlags_baseline_vs_goal &  SG_DIFFSTATUS_FLAGS__LOST)  == 0)  );	// not possible for cset-vs-cset diff

			if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__ADDED)
			{
				// added in goal changeset; not present in the baseline; not present in the WD.
				// do not claim anything the source.
				// use the (goal_parent,goal_entryname) in the target.

				SG_ERR_CHECK(  _pt_update__bind(pCtx, pud, pszGid_k, pOD_k, SG_FALSE, SG_FALSE, SG_FALSE, SG_TRUE)  );
				goto NEXT;
			}

			if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__DELETED)
			{
				// deleted in the goal changeset; present in the baseline; not changed in the WD.
				// use the (current_parent,current_entryname) in the source (because it is in both baseline and WD that way).
				// do not claim anything in the target.

				SG_ERR_CHECK(  _pt_update__bind(pCtx, pud, pszGid_k, pOD_k, SG_TRUE, SG_TRUE, SG_TRUE, SG_FALSE)  );
				goto NEXT;
			}

			// entry in goal may or may not be __MODIFIED (edited), but we don't care about that here.

			if (dsFlags_baseline_vs_goal & (SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__RENAMED))
			{
				// entry was MOVED, RENAMED, or MOVED+RENAMED in the goal changeset (relative to the baseline).
				// entry present in the baseline and not changed in the WD.
				// use the (current_parent,current_entryname) in the source.
				// use the (goal_parent,goal_entryname) in the target.

				SG_ERR_CHECK(  _pt_update__bind(pCtx, pud, pszGid_k, pOD_k, SG_TRUE, SG_TRUE, SG_TRUE, SG_FALSE)  );
				SG_ERR_CHECK(  _pt_update__bind(pCtx, pud, pszGid_k, pOD_k, SG_FALSE, SG_FALSE, SG_FALSE, SG_TRUE)  );
				goto NEXT;
			}

			// TODO 3/25/10 Should this be an ASSERT(0) ?
			goto NEXT;
		}

		// the entry was changed in the WD.  try to carry the change forward to the result.

		if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__ADDED)
		{
			// entry was added in WD/pendingtree relative to baseline.
			// it should not currently be present in baseline or in goal changeset.

			SG_ASSERT(  (dsFlags_baseline_vs_goal == SG_DIFFSTATUS_FLAGS__ZERO)  );

			// try to add it to the result -- we preserve the ADD.
			// claim the (current_parent,current_entryname) in both the source (because it was present)
			// and claim the target (to do the carry-forward/preserve).  in both cases, use the WD version
			// of the entryname and parent directory.

			SG_ERR_CHECK(  _pt_update__bind(pCtx, pud, pszGid_k, pOD_k, SG_TRUE, SG_TRUE, SG_TRUE, SG_TRUE)  );
			goto NEXT;
		}

		if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__FOUND)
		{
			// entry was found in the WD.  we know nothing about it other than that it was there.
			// it should not currently be present in the baseline or in the goal changeset.

			SG_ASSERT(  (dsFlags_baseline_vs_goal == SG_DIFFSTATUS_FLAGS__ZERO)  );

			// claim the (current_parent,current_entryname) in both the source (because it was present)
			// and claim the target (so that we detect collisions).  in both cases, use the WD version
			// of the entryname and the parent directory.
			//
			// if the goal changeset has something with the same name in that directory, we may want
			// to move this entry to a backup name.

			SG_ERR_CHECK(  _pt_update__bind(pCtx, pud, pszGid_k, pOD_k, SG_TRUE, SG_TRUE, SG_TRUE, SG_TRUE)  );
			goto NEXT;
		}

		if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__DELETED)
		{
			// entry was deleted in WD.
			// it was present in the baseline.
			// it may or may not be present in the goal changeset.
			//
			// do not claim anything the source (because it wasn't in the WD).
			//
			// if it was unchanged or deleted in the goal (relative to the baseline)
			// we carry-forward the delete, so we do not claim anything in the target.
			// (any other type of change in the goal should already have thrown an
			// error in __check_for_individual_conflicts().)

			SG_ASSERT(  (   (dsFlags_baseline_vs_goal == SG_DIFFSTATUS_FLAGS__ZERO)
						 || (dsFlags_baseline_vs_goal == SG_DIFFSTATUS_FLAGS__DELETED))  );
			goto NEXT;
		}

		if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__LOST)
		{
			// entry was lost in the WD.  we don't know what happened to it.
			// supply the verion as it was in the goal changeset.

			if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__DELETED)
				goto NEXT;

			SG_ASSERT(  ((dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__LOST) == 0)  );	// __LOST is only set on baseline-vs-WD-type diffs

			// entry is present in the goal changeset.  claim the target using the
			// goal changeset's version of the entryname and parent directory.

			SG_ERR_CHECK(  _pt_update__bind(pCtx, pud, pszGid_k, pOD_k, SG_FALSE, SG_FALSE, SG_FALSE, SG_TRUE)  );
			goto NEXT;
		}

		// we may or may not be __MODIFIED (edited), but we don't care about that here.

		if (dsFlags_baseline_vs_wd & (SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__RENAMED))
		{
			// entry was MOVED, RENAMED, or MOVED+RENAMED in the WD.
			//
			// claim the (current_parent,current_entryname) in the source (because it was present).

			SG_ERR_CHECK(  _pt_update__bind(pCtx, pud, pszGid_k, pOD_k, SG_TRUE, SG_TRUE, SG_TRUE, SG_FALSE)  );

			// claim the (goal_parent,goal_entryname) in the target (either or both values may be
			// the same as they were in the baseline).

			SG_ERR_CHECK(  _pt_update__bind(pCtx, pud, pszGid_k, pOD_k, SG_FALSE, SG_FALSE, SG_FALSE, SG_TRUE)  );
			goto NEXT;
		}

NEXT:
		SG_ERR_CHECK(  SG_treediff2__iterator__next(pCtx, pIter, &bOK, &pszGid_k, &pOD_k)  );
	}

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"_pt_update:  after [1] in full composite\n")  );
	SG_ERR_IGNORE(  SG_inv_dirs_debug__dump_to_console(pCtx,pud->pInvDirs)  );
#endif

fail:
	SG_TREEDIFF2_ITERATOR_NULLFREE(pCtx, pIter);
}

//////////////////////////////////////////////////////////////////

/**
 * __do__full_composite_update: step 2.
 *
 * [2] Identify entrynames that are claimed by different entries in the 2 trees and
 *     create a "swap" pathname in the parking lot.
 *
 *     Unlike the REVERT code, we don't have PARTIAL UPDATES.  UPDATE is an all or
 *     nothing operation.  So, all of the entries should have the __ACTIVE_ bits set.
 *
 */
static void _pt_update__do__full_2(SG_context * pCtx,
								   struct _pt_update_data * pud)
{
#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: begin [2]\n")  );
#endif

	SG_ASSERT(  (pud->bInDoFull)  );

	SG_ERR_CHECK_RETURN(  SG_inv_dirs__check_for_swaps(pCtx, pud->pInvDirs)  );
}

//////////////////////////////////////////////////////////////////

/**
 * Look at the various dsFlags in different component parts of this OD
 * and decide what the final parent will be.
 *
 * For things which were only MOVED in the baseline-vs-WD, we choose the WD.
 * For things which were only MOVED in the baseline-vs-goal, we choose the goal.
 * If it was MOVED in both, it should be to the same value (because we already
 * checked for divergent MOVES).
 * If it was NOT MOVED in both, return the existing parent.
 *
 * Return the GID of the final parent.  You DO NOT own this string.
 * Return true if TRUE if the child is in a different parent directory in the
 * final result (than it is in the dirty WD).
 */
static void _pt_update__determine_final_parent_gid(SG_context * pCtx,
												   const SG_treediff2_ObjectData * pOD,
												   const char ** ppszGid_FinalParent,
												   SG_bool * pbMovedInFinalResult)
{
	SG_diffstatus_flags dsFlags_goal_vs_baseline;
	SG_diffstatus_flags dsFlags_baseline_vs_wd;
	SG_diffstatus_flags dsFlags_composite;
	SG_diffstatus_flags dsFlags_baseline_vs_goal;
	const char * pszGid_FinalParent;
	SG_bool bMovedInFinalResult = SG_FALSE;

	SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_composite_dsFlags_by_part(pCtx, pOD,
																				  &dsFlags_goal_vs_baseline,
																				  &dsFlags_baseline_vs_wd,
																				  &dsFlags_composite)  );
	SG_ERR_CHECK_RETURN(  SG_treediff2__Invert_dsFlags(pCtx, dsFlags_goal_vs_baseline, &dsFlags_baseline_vs_goal)  );

	if ( (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__MOVED) && (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__MOVED) )
	{
		SG_ASSERT(  (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_MOVED)  );
	}

	if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__MOVED)
	{
		// for an arbitrary-cset-vs-wd composite diff Ndx_Net holds the WD value.
		SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_parent_gid(pCtx, pOD, &pszGid_FinalParent)  );
	}
	else if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__MOVED)
	{
		// for an arbitrary-cset-vs-wd composite diff Ndx_Orig holds goal cset value
		// (when you think in terms of Figure 3 and the inverting it go get to Figure 1.)

		SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_moved_from_gid(pCtx, pOD, &pszGid_FinalParent)  );
		bMovedInFinalResult = SG_TRUE;
	}
	else
	{
		// not moved in either, so it doesn't matter which value we use because
		// they both should be the same.
		SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_parent_gid(pCtx, pOD, &pszGid_FinalParent)  );
	}

#if TRACE_UPDATE
	{
		const char * pszGid_Self = NULL;

		SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_gid(pCtx, pOD, &pszGid_Self)  );
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "_pt_update:  final parent of [%s] is [%s] [moved %d]\n",
								   ((pszGid_Self) ? pszGid_Self : "(null)"),
								   ((pszGid_FinalParent) ? pszGid_FinalParent : "(null)"),
								   bMovedInFinalResult)  );
	}
#endif

	*ppszGid_FinalParent = pszGid_FinalParent;

	if (pbMovedInFinalResult)
		*pbMovedInFinalResult = bMovedInFinalResult;
}

/**
 * See if the entry with the given GID will be present
 * in the final result.  That is, make sure that is
 * hasn't been deleted in goal and/or WD --or-- when
 * it represents something new (not in the baseline)
 * that it will be created by either the goal and/or WD.
 */
static void _pt_update__is_entry_present_in_final_result(SG_context * pCtx,
														 const SG_treediff2_ObjectData * pOD,
														 SG_bool * pbFound)
{
	SG_diffstatus_flags dsFlags_goal_vs_baseline;
	SG_diffstatus_flags dsFlags_baseline_vs_wd;
	SG_diffstatus_flags dsFlags_composite;
	SG_diffstatus_flags dsFlags_baseline_vs_goal;

	SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_composite_dsFlags_by_part(pCtx, pOD,
																				  &dsFlags_goal_vs_baseline,
																				  &dsFlags_baseline_vs_wd,
																				  &dsFlags_composite)  );
	SG_ERR_CHECK_RETURN(  SG_treediff2__Invert_dsFlags(pCtx, dsFlags_goal_vs_baseline, &dsFlags_baseline_vs_goal)  );

	if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__DELETED)
	{
		*pbFound = SG_FALSE;
		return;
	}

	if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__DELETED)
	{
		*pbFound = SG_FALSE;
		return;
	}

	if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__LOST)
	{
		// TODO if it was LOST from the WD, then we silently supply the
		// TODO version from the goal changeset.  if the goal and the
		// TODO baseline version are different, we want the goal version
		// TODO anyway, right?  If they are the same, then it doesn't
		// TODO matter which we choose.  The only option is whether we
		// TODO should leave it as a LOST entry.  See also
		// TODO __check_for_individual_conflicts().

		*pbFound = SG_TRUE;			// assume goal cset version will be supplied
		return;
	}

	*pbFound = SG_TRUE;
}

/**
 * TODO
 * TODO We may not actually need this routine.  I think I fixed all of the
 * TODO cases in __check_for_individual_conflicts(), so that this won't
 * TODO find anything.
 * TODO
 * TODO Verify that.
 *
 *
 * See if the parent directory (in the (micro) merge version of the tree
 * that we are creating) will exist (wasn't deleted in the other branch).
 */
static void _pt_update__require_final_parent_to_be_present_in_final_result(SG_context * pCtx,
																		   struct _pt_update_data * pud,
																		   const SG_treediff2_ObjectData * pOD)
{
	const char * pszGid_FinalParent = NULL;
	const SG_treediff2_ObjectData * pOD_FinalParent = NULL;
	const char * pszRepoPath_Self = NULL;
	SG_bool bWeWereMovedInFinalResult = SG_FALSE, bFound = SG_FALSE, bParentPresent = SG_FALSE;

	SG_ERR_CHECK_RETURN(  _pt_update__determine_final_parent_gid(pCtx, pOD, &pszGid_FinalParent, &bWeWereMovedInFinalResult)  );
	if (pszGid_FinalParent)				// if not the "@/" root directory
	{
		SG_ERR_CHECK_RETURN(  SG_treediff2__get_ObjectData(pCtx, pud->pTreeDiff_composite,
														   pszGid_FinalParent,
														   &bFound, &pOD_FinalParent)  );
		SG_ASSERT(  (bFound)  );
		SG_ERR_CHECK_RETURN(  _pt_update__is_entry_present_in_final_result(pCtx, pOD_FinalParent, &bParentPresent)  );

		if (!bParentPresent)
		{
			// the parent directory is not present in the final result.
			//
			// for the error message, we get the "current" repo-path for the given entry.
			// this is "source" version relative -- ie, the path as it looks in the current WD.
			//
			// TODO This repo-path may cause some confusion if there are a bunch of MOVES
			// TODO and/or RENAMES in the either the WD or the goal changeset.  Revisit this
			// TODO later.
			SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_repo_path(pCtx, pOD, &pszRepoPath_Self)  );
			SG_ERR_THROW2_RETURN(  SG_ERR_UPDATE_CONFLICT,
								   (pCtx, "Parent directory of '%s' not present in final result.",
									pszRepoPath_Self)  );
		}
	}
}

/**
 * __do__full_composite_update: step 3.
 *
 * [3] Look for hard conflicts caused by structural changes to tree and
 *     identify any other issues that we may be able to handle.
 *
 *     (We assume that __check_for_individual_conflicts() has already run.)
 *
 *     For example, if I move .../dir_a into .../dir_b/... in my WD and
 *     someone else moves .../dir_b into .../dir_a/... in their WD and
 *     commits it, then when we try to update and preserve my changes
 *     we won't be able to populate the result because of a pathname cycle.
 *     For this a host of other screw cases, we just fail the UPDATE
 *     and advise them to COMMIT and MERGE.
 */
static void _pt_update__do__full_3(SG_context * pCtx,
								   struct _pt_update_data * pud)
{
	SG_treediff2_iterator * pIter = NULL;
	const char * pszGid_k = NULL;
	SG_treediff2_ObjectData * pOD_k = NULL;
	SG_bool bOK = SG_FALSE, bPresent = SG_FALSE;

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: begin [3]\n")  );
#endif

	SG_ASSERT(  (pud->bInDoFull)  );

	SG_ERR_CHECK(  SG_treediff2__iterator__first(pCtx, pud->pTreeDiff_composite, &bOK, &pszGid_k, &pOD_k, &pIter)  );
	while (bOK)
	{
		SG_diffstatus_flags dsFlags_goal_vs_baseline;
		SG_diffstatus_flags dsFlags_baseline_vs_wd;
		SG_diffstatus_flags dsFlags_composite;
		SG_diffstatus_flags dsFlags_baseline_vs_goal;

		SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_composite_dsFlags_by_part(pCtx, pOD_k,
																					  &dsFlags_goal_vs_baseline,
																					  &dsFlags_baseline_vs_wd,
																					  &dsFlags_composite)  );
		SG_ERR_CHECK_RETURN(  SG_treediff2__Invert_dsFlags(pCtx, dsFlags_goal_vs_baseline, &dsFlags_baseline_vs_goal)  );

		// see if entry should exist in the final result.
		// that is, make sure it wasn't deleted in either the goal or the WD.
		SG_ERR_CHECK(  _pt_update__is_entry_present_in_final_result(pCtx, pOD_k, &bPresent)  );
		if (bPresent)
		{
			// sanity check that the immediate parent wasn't deleted by one side or the other.
			SG_ERR_CHECK(  _pt_update__require_final_parent_to_be_present_in_final_result(pCtx, pud, pOD_k)  );

			// look for path cycles.

			// TODO
		}

		SG_ERR_CHECK(  SG_treediff2__iterator__next(pCtx, pIter, &bOK, &pszGid_k, &pOD_k)  );
	}

fail:
	SG_TREEDIFF2_ITERATOR_NULLFREE(pCtx, pIter);
}

/**
 * __do__full_composite_update: step 4.
 *
 * [4] Look for portability issues when the dirt in the WD and the changes
 *     in the goal changeset are folded together.
 *
 *     For example, if I do "vv add foo" and someone else does
 *     "vv add FOO" and commits it, then we need to stop and complain
 *     here -- before we try to populate the result.
 */
static void _pt_update__do__full_4(SG_context * pCtx,
								   struct _pt_update_data * pud)
{
	SG_portability_flags flags_logged;

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: begin [4]\n")  );
#endif

	SG_ASSERT(  (pud->bInDoFull)  );

	if (pud->nrChanges_baseline_vs_wd == 0)
	{
		// if the WD is clean, then there is no (micro) merge and
		// all of the entrynames in the result will reflect the
		// contents of the goal changeset and were already inspected
		// and approved.
		return;
	}

	if (pud->pPendingTree->bIgnoreWarnings)
	{
		// TODO (MAYBE) if we were told to ignore portability warnings
		// TODO (MAYBE) and the (micro) merge will attempt to create
		// TODO (MAYBE) both "FOO" and "foo" in the same directory
		// TODO (MAYBE) on a Mac/Windows system, we won't find out
		// TODO (MAYBE) about the problem UNTIL we are part way thru
		// TODO (MAYBE) scrambling their WD when the filesystem gives
		// TODO (MAYBE) a hard error.
		// TODO (MAYBE)
		// TODO (MAYBE) So we might crap out and leave their WD in a
		// TODO (MAYBE) mess -or- worse overwrite one with the other
		// TODO (MAYBE) without noticing it.
		// TODO (MAYBE)
		// TODO (MAYBE) And our --test results will be incomplete
		// TODO (MAYBE) because we will have failed to predict it.
		// TODO (MAYBE)
		// TODO (MAYBE) We should think about secretely running the
		// TODO (MAYBE) portability code and see if any of the
		// TODO (MAYBE) *potential* errors would be *actual* errors
		// TODO (MAYBE) on the underlying filesystem and throw the
		// TODO (MAYBE) error now.

		return;
	}

	// TODO actually do the work....

	SG_ERR_CHECK_RETURN(  SG_inv_dirs__get_portability_warnings_observed(pCtx, pud->pInvDirs, &flags_logged)  );
	if (flags_logged)
		SG_ERR_THROW_RETURN(  SG_ERR_PORTABILITY_WARNINGS  );
}

//////////////////////////////////////////////////////////////////

/**
 * __do__full_composite_update: step 5.
 *
 * [5] Move the entries that have a "parked/swap" pathname to their
 *     temporary location in the parking lot.
 *
 *     Because the WD dirt that we are preserving is already reflected
 *     in the on-disk WD (by definition), we shouldn't need to worry
 *     about parking anything in the WD/pendingtree-side of the composite
 *     treediff.  We should only have to worry about parking complex
 *     MOVES/RENAMES made between the baseline and the goal changeset.
 *
 *     We should not have to worry about entries that were ADDED in
 *     the goal changeset.  (They will be handled in a later step.)
 *
 *
 *     We want to park everything that needs it in one step and then
 *     later un-park them all at their new locations.  Since parking
 *     a directory moves it (and everything within it to the parking
 *     lot), we either need to park the (deeper) files before we park
 *     the directories --or-- we need the "repo_path --> absolute_path"
 *     conversion for the files to know whether the containing
 *     directories have already been parked.  I think it is easier
 *     to just dive.
 *
 *
 *     The REVERT code could just do a depth-first walk using the
 *     pendingtree and start moving things to the parking lot and
 *     then move the parent directories on the way out because
 *     everything that could possibly be parked was in the pendingtree.
 *
 *
 *     We can't do that for UPDATE because the stuff we need to park
 *     is in the goal changeset and not in the pendingtree (which
 *     only has the WD dirt in it).  So WE FAKE THIS by building a
 *     reverse-sorted queue of pathnames to park; by reverse-sorting
 *     it, the deepest stuff is first -- AND WE DON'T NEED TO ACCESS
 *     a recursive tree-structure, (such as the pendingtree (which we
 *     don't have for baseline-vs-goal changes)).
 */
static SG_inv_dirs_foreach_in_queue_for_parking_callback _pt_update__do__full_5__cb;

static void _pt_update__do__full_5__cb(SG_context * pCtx,
									   const char * pszKey_RepoPath,		// key
									   SG_inv_entry * pInvEntry,			// value
									   void * pVoidData)
{
	struct _pt_update_data * pud = (struct _pt_update_data *)pVoidData;

	SG_pathname * pPath_in_wd = NULL;
	const char * pszGid;
	const SG_treediff2_ObjectData * pOD;
	const char * pszRepoPath_in_treediff;
	const SG_pathname * pPathParked = NULL;		// we do not own this
	SG_inv_entry_flags inv_entry_flags;

#if !defined(DEBUG)
    SG_UNUSED(pszKey_RepoPath);
#endif

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, ("_pt_update[5]: Parking [%s]\n"), pszKey_RepoPath)  );
#endif

	SG_ERR_CHECK(  SG_inv_entry__get_assoc_data__source(pCtx, pInvEntry, (void **)&pOD)  );

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_gid(pCtx, pOD, &pszGid)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_repo_path(pCtx, pOD, &pszRepoPath_in_treediff)  );

	SG_ASSERT(  (strcmp(pszKey_RepoPath, pszRepoPath_in_treediff) == 0)  );
	SG_ERR_CHECK(  SG_inv_entry__get_flags(pCtx, pInvEntry, &inv_entry_flags)  );
	SG_ASSERT(  (inv_entry_flags & SG_INV_ENTRY_FLAGS__PARKED__MASK)  );

	SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx,
																		 pud->pPendingTree->pPathWorkingDirectoryTop,
																		 pszRepoPath_in_treediff,
																		 &pPath_in_wd)  );

	SG_ERR_CHECK(  SG_inv_entry__get_parked_path__ref(pCtx, pInvEntry, &pPathParked)  );
	SG_ASSERT(  (pPathParked)  );

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   ("_pt_update[5]:    reason %x\n"
								"                  gid    [%s]\n"
								"                  repo   [%s]\n"
								"                  wd     [%s]\n"
								"                  park   [%s]\n"),
							   inv_entry_flags,
							   pszGid,
							   pszRepoPath_in_treediff,
							   SG_pathname__sz(pPath_in_wd),
							   SG_pathname__sz(pPathParked))  );
#endif

	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
		SG_ERR_CHECK(  _pt_log__move(pCtx,
									 pud->pPendingTree,
									 pszGid,
									 SG_pathname__sz(pPath_in_wd),
									 SG_pathname__sz(pPathParked),
									 "PARK_FOR_SWAP")  );
	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx, pPath_in_wd, pPathParked)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx,pPath_in_wd);
}


static void _pt_update__do__full_5(SG_context * pCtx,
								   struct _pt_update_data * pud)
{
#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: begin [5]\n")  );
#endif

	SG_ASSERT(  (pud->bInDoFull)  );

	SG_ERR_CHECK_RETURN(  SG_inv_dirs__foreach_in_queue_for_parking(pCtx, pud->pInvDirs,
																	_pt_update__do__full_5__cb,
																	(void *)pud)  );
}

//////////////////////////////////////////////////////////////////

/**
 * Compute the current pathname for a given directory.  This reflects
 * either the current dirty WD or the parking lot.
 */
static void _pt_update__do__full_6_1_get_current_transient_path(SG_context * pCtx,
																struct _pt_update_data * pud,
																const SG_treediff2_ObjectData * pOD,
																SG_pathname ** ppPath)
{
	// we assert that pOD is a directory that exists in the baseline.

	SG_pathname * pPath = NULL;
	const char * pszGid;
	const char * pszGid_Parent_Current;
	const SG_treediff2_ObjectData * pOD_Parent_Current;
	struct _inv_entry * pInvEntry;
	const char * pszName_Current;
	const SG_pathname * pPathParked = NULL;		// we do not own this
	SG_bool bFound;

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_name(pCtx, pOD, &pszName_Current)  );
	SG_ASSERT(  (pszName_Current && *pszName_Current)  );
	if (strcmp(pszName_Current, "@") == 0)
	{
		SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__COPY(pCtx, ppPath, pud->pPendingTree->pPathWorkingDirectoryTop)  );
		return;
	}

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_gid(pCtx, pOD, &pszGid)  );
	SG_ERR_CHECK(  SG_inv_dirs__fetch_source_binding(pCtx, pud->pInvDirs, pszGid, &bFound, &pInvEntry, NULL)  );
	if (bFound)
		SG_ERR_CHECK(  SG_inv_entry__get_parked_path__ref(pCtx, pInvEntry, &pPathParked)  );
	if (pPathParked)
	{
		SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__COPY(pCtx, ppPath, pPathParked)  );
		return;
	}

	// get our parent directory as it is in the dirty WD or baseline (where it
	// currently is on disk).

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_parent_gid(pCtx, pOD, &pszGid_Parent_Current)  );
	SG_ASSERT(  (pszGid_Parent_Current && *pszGid_Parent_Current)  );
	SG_ERR_CHECK(  SG_treediff2__get_ObjectData(pCtx, pud->pTreeDiff_composite, pszGid_Parent_Current, &bFound, &pOD_Parent_Current)  );
	SG_ASSERT(  (bFound)  );
	SG_ERR_CHECK(  _pt_update__do__full_6_1_get_current_transient_path(pCtx, pud, pOD_Parent_Current, &pPath)  );

	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath, pszName_Current)  );

	*ppPath = pPath;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

/**
 * we have a directory that was ADDED in the goal changeset (and doesn't exist
 * in the baseline nor in the WD).  we want to create a new directory on disk
 * in the WD so that the entries (that should be *within* the directory in the
 * final result) can be created/moved into it during [7], but first we need to
 * compute the pathnames for the new directories.
 */
static void _pt_update__do__full_6_1_add(SG_context * pCtx,
										 struct _pt_update_data * pud,
										 SG_rbtree * prbDirsSortedByTransientPath,
										 const SG_treediff2_ObjectData * pOD_k,
										 SG_pathname ** ppPath_k)
{
	// we assert that pOD_k is a directory and was added in the goal changeset.

	SG_pathname * pPath = NULL;
	const char * pszGidParent_Goal;
	const SG_treediff2_ObjectData * pOD_Parent_Goal;
	SG_diffstatus_flags dsFlags_Parent_goal_vs_baseline;
	SG_diffstatus_flags dsFlags_Parent_baseline_vs_wd;
	SG_diffstatus_flags dsFlags_Parent_composite;
	SG_diffstatus_flags dsFlags_Parent_baseline_vs_goal;
	const char * pszName_Goal;
	SG_bool bFound;

	// get the parent directory in the goal changeset.
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_moved_from_gid(pCtx, pOD_k, &pszGidParent_Goal)  );
	SG_ERR_CHECK(  SG_treediff2__get_ObjectData(pCtx, pud->pTreeDiff_composite, pszGidParent_Goal, &bFound, &pOD_Parent_Goal)  );
	SG_ASSERT(  (bFound)  );
	SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_composite_dsFlags_by_part(pCtx, pOD_Parent_Goal,
																				  &dsFlags_Parent_goal_vs_baseline,
																				  &dsFlags_Parent_baseline_vs_wd,
																				  &dsFlags_Parent_composite)  );
	SG_ERR_CHECK_RETURN(  SG_treediff2__Invert_dsFlags(pCtx, dsFlags_Parent_goal_vs_baseline, &dsFlags_Parent_baseline_vs_goal)  );

	if (dsFlags_Parent_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__ADDED)
	{
		// the goal parent was also added in the goal changeset.
		// recurse and ask it to compute a pathname for it in the
		// current WD.

		SG_ERR_CHECK(  _pt_update__do__full_6_1_add(pCtx, pud, prbDirsSortedByTransientPath, pOD_Parent_Goal, &pPath)  );
	}
	else
	{
		// the goal parent was present in the baseline.  compute a
		// pathname for it in the current WD.  that is, where the
		// parent directory is currently (either in the tree or in
		// the parking lot).

		SG_ERR_CHECK(  _pt_update__do__full_6_1_get_current_transient_path(pCtx, pud, pOD_Parent_Goal, &pPath)  );
	}

	// append our goal/target entryname to the current WD path.
	// this should be safe and conflict free because we aready
	// claimed the entryname in the target and any collisions
	// were parked.

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, pOD_k, &pszName_Goal)  );
	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath, pszName_Goal)  );

	// add it to the queue so that we can create it in-order.
	//
	// TODO 4/14/10 this division between [6.1] and [6.2] might not be
	// TODO 4/14/10 necessary with the new recursive __full_6_1_add.

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   ("_pt_update: skeleton need to mkdir [%s]\n"),
							   SG_pathname__sz(pPath))  );
#endif

	// i do __update__ rather than __add__ here because of the way we now
	// do things recursively (a nested create dir should cause 2 items to
	// be added before we're done) -- BUT -- we are inside an iteration
	// based upon GID andthe iteration may have seen the shallower one
	// first, but this doesn't matter.
	SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx,prbDirsSortedByTransientPath,
												 SG_pathname__sz(pPath),
												 (void *)pOD_k,
												 NULL)  );

	*ppPath_k = pPath;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

/**
 * Step 6.1: Identify the directories that need to be created to
 *           fully populate the skeleton of the tree in the result.
 *
 *           Add these to the provided queue and sorted by the
 *           transient pathname so that later when we actual create
 *           the directories, we can be independent of the GIDs.
 */
static void _pt_update__do__full_6_1(SG_context * pCtx,
									 struct _pt_update_data * pud,
									 SG_rbtree * prbDirsSortedByTransientPath)
{
	SG_treediff2_iterator * pIter = NULL;
	SG_pathname * pPath = NULL;
	const char * pszGid_k;
	SG_treediff2_ObjectData * pOD_k;
	SG_bool bOK;

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: begin [6.1]\n")  );
#endif

	SG_ERR_CHECK(  SG_treediff2__iterator__first(pCtx, pud->pTreeDiff_composite, &bOK, &pszGid_k, &pOD_k, &pIter)  );
	while (bOK)
	{
		SG_treenode_entry_type tneType;

		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_tneType(pCtx,pOD_k,&tneType)  );
		if (tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
		{
			SG_diffstatus_flags dsFlags_goal_vs_baseline;
			SG_diffstatus_flags dsFlags_baseline_vs_wd;
			SG_diffstatus_flags dsFlags_composite;
			SG_diffstatus_flags dsFlags_baseline_vs_goal;

			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_composite_dsFlags_by_part(pCtx, pOD_k,
																				   &dsFlags_goal_vs_baseline,
																				   &dsFlags_baseline_vs_wd,
																				   &dsFlags_composite)  );
			SG_ERR_CHECK(  SG_treediff2__Invert_dsFlags(pCtx, dsFlags_goal_vs_baseline, &dsFlags_baseline_vs_goal)  );

			if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__ADDED)
			{
				// we have a directory that was created in the goal changeset (and doesn't
				// exist in the baseline nor in the WD).  we want to create a new directory
				// on disk in the WD so that the entries (that should be *within* the
				// directory in the final result) can be created/moved in it during [7].
				//
				// this is a little awkward because they could have created multiple nested
				// directories in the goal changeset and since we are iterating in the random
				// GID order rather than from the root.  (we can't do the root-based walk
				// because these entries are not in the pendingtree.)
				//
				// Also, because we were not present in the SOURCE version of the tree, we
				// cannot have been parked in the parking lot by [5].  Also, we did not
				// claim a SOURCE-entryname for it, so we cannot call
				// __inv_dirs__fetch_source_binding() on it.  *BUT* we *MAY* be able to call
				// it on our parent directory.
				//
				// Furthermore, because we claimed the TARGET-entryname, we own it cleanly
				// and can create the directory in the WD without having to worry about
				// backup names or collisions.
				//
				// Further complicating this is that we want to be able to support the "--test"
				// option where we compute the steps that would have been done but without
				// actually making any changes in the WD, so we can't just optionally do a
				// mkdir and/or recursively call mkdir on our parent.  Rather, we need to
				// make a list of the steps we need to do and sort them.

				SG_ERR_CHECK(  _pt_update__do__full_6_1_add(pCtx, pud, prbDirsSortedByTransientPath, pOD_k, &pPath)  );

				SG_PATHNAME_NULLFREE(pCtx, pPath);
			}
			else if ((dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__LOST)
					 && ((dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__DELETED) == 0))
			{
				// The directory was LOST in the WD and not deleted in the GOAL,
				// so we will silently restore it in [7].
				//
				// For all of the same reasons as above, go ahead and create the directory
				// skeleton so that LOST stuff within the directory can be restored too.
				//
				// When we create the directory skeleton by restoring LOST directories,
				// we d0 so using the GOAL pathnames (from the treediff) (and using the
				// TARGET bindings) ***rather*** than the current values in the PTNODE
				// (because LOST/DELETED items don't get SOURCE bindings and if we did,
				// we'd might have to turn around and move or rename them).

				SG_ERR_CHECK(  _pt_update__do__full_6_1_add(pCtx, pud, prbDirsSortedByTransientPath, pOD_k, &pPath)  );

				SG_PATHNAME_NULLFREE(pCtx, pPath);
			}
			
		}

		SG_ERR_CHECK(  SG_treediff2__iterator__next(pCtx, pIter, &bOK, &pszGid_k, &pOD_k)  );
	}

fail:
	SG_TREEDIFF2_ITERATOR_NULLFREE(pCtx, pIter);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

/**
 * Synthesize a treenode-entry for the goal changeset version of an entry.
 *
 * TODO it is possible that we may be able to access an instance of
 * TODO the treenode-entry from the cached treenodes being held by the
 * TODO treediff and/or have the treediff2 remember the pTNE in
 * TODO _td__set_od_column(), but that might not be wise.
 *
 * TODO also consider moving this to SG_treediff2 so that it can
 * TODO access some private stuff.  It should only be defined on
 * TODO Ndx_Orig, right?
 */
static void _pt_update__synthesize_goal_treenode_entry(SG_context * pCtx,
													   const SG_treediff2_ObjectData * pOD,
													   SG_treenode_entry ** ppTNE)
{
	SG_treenode_entry * pTNE = NULL;
	const char * pszHid;
	const char * pszHidXAttrs;
	const char * pszName;
	SG_int64 attrbits;
	SG_treenode_entry_type tneType;

	SG_ERR_CHECK_RETURN(  SG_TREENODE_ENTRY__ALLOC(pCtx, &pTNE)  );

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_tneType(pCtx, pOD, &tneType)  );
	SG_ERR_CHECK(  SG_treenode_entry__set_entry_type(pCtx, pTNE, tneType)  );

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_content_hid(pCtx, pOD, &pszHid)  );
	SG_ERR_CHECK(  SG_treenode_entry__set_hid_blob(pCtx, pTNE, pszHid)  );

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_attrbits(pCtx, pOD, &attrbits)  );
	SG_ERR_CHECK(  SG_treenode_entry__set_attribute_bits(pCtx, pTNE, attrbits)  );

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_xattrs(pCtx, pOD, &pszHidXAttrs)  );
	SG_ERR_CHECK(  SG_treenode_entry__set_hid_xattrs(pCtx, pTNE, pszHidXAttrs)  );

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, pOD, &pszName)  );
	SG_ERR_CHECK(  SG_treenode_entry__set_entry_name(pCtx, pTNE, pszName)  );

	*ppTNE = pTNE;
	return;

fail:
	SG_TREENODE_ENTRY_NULLFREE(pCtx, pTNE);
}

/**
 * Step 6.2: Use the sorted queue to create the skeleton directories.
 *
 *           Also add the directories to the pendingtree in their current
 *           location.  That is, add each created directory to the ptnode
 *           of the containing directory (wherever that parent directory
 *           may be in the tree); this will probably be the location in
 *           the SOURCE view.
 */
static void _pt_update__do__full_6_2(SG_context * pCtx,
									 struct _pt_update_data * pud,
									 SG_rbtree * prbDirsSortedByTransientPath)
{
	SG_rbtree_iterator * pIter = NULL;
	SG_pathname * pPath = NULL;
	SG_treenode_entry * pTNE = NULL;
	struct sg_ptnode * ptn = NULL;
	struct sg_ptnode * ptnParent = NULL;
	SG_treediff2_ObjectData * pOD;
	const char * pszKey_Path;
	const char * pszGid;
	const char * pszGid_Parent;
	SG_diffstatus_flags dsFlags_goal_vs_baseline;
	SG_diffstatus_flags dsFlags_baseline_vs_wd;
	SG_diffstatus_flags dsFlags_composite;
	SG_diffstatus_flags dsFlags_baseline_vs_goal;
	SG_uint32 nrPaths;
	SG_bool bOK;
	SG_bool bAlreadyPresent;

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: begin [6.2]\n")  );
#endif

	SG_ERR_CHECK(  SG_rbtree__count(pCtx, prbDirsSortedByTransientPath, &nrPaths)  );
	if (nrPaths == 0)
		return;

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, prbDirsSortedByTransientPath, &bOK, &pszKey_Path, (void**)&pOD)  );
	while (bOK)
	{
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_gid(pCtx, pOD, &pszGid)  );

		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_composite_dsFlags_by_part(pCtx, pOD,
																			   &dsFlags_goal_vs_baseline,
																			   &dsFlags_baseline_vs_wd,
																			   &dsFlags_composite)  );
		SG_ERR_CHECK(  SG_treediff2__Invert_dsFlags(pCtx, dsFlags_goal_vs_baseline, &dsFlags_baseline_vs_goal)  );

		if (pud->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
		{
			SG_ERR_CHECK(  _pt_log__mkdir(pCtx,pud->pPendingTree,
										  pszGid,
										  pszKey_Path,
										  "SKELETON")  );
		}
		if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		{
			// TODO this can throw SG_ERR_DIR_ALREADY_EXISTS -- this should not happen because
			// TODO we already determined that it (the GID and the ENTRYNAME) was created in the goal changeset.
			// TODO so i'm going to just let this throw.
			//
			// TODO we might get this if the portability-warnings are turned off/ignored, maybe?

#if TRACE_UPDATE
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: [6.2] mkdir [%s] [baseline-vs-goal %d][baseline-vs-wd %d]\n",
									   pszKey_Path,
									   dsFlags_baseline_vs_goal,
									   dsFlags_baseline_vs_wd)  );
#endif

			SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, pszKey_Path)  );
			SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath)  );
			SG_PATHNAME_NULLFREE(pCtx, pPath);
		}

		if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__LOST)
		{
			// We handle the restore of LOST directories a little differently.  We do
			// mkdir using the GOAL/TARGET pathname rather than the baseline/WD pathname
			// (for collision and other reasons).  But the PTNODE doesn't know about this.
			// So we need to fix up the PTNODE now so that we are in sync before [7].

#if TRACE_UPDATE
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: (lost) [6.2] mkdir [%s] [baseline-vs-goal %d]\n",
									   pszKey_Path, dsFlags_baseline_vs_goal)  );
#endif

			if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__MOVED)
				SG_ERR_CHECK(  _pt_update__move_entry_with_possible_rename(pCtx, pud, pszGid, pOD, SG_FALSE)  );
			else if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__RENAMED)
				SG_ERR_CHECK(  _pt_update__rename_entry(pCtx, pud, pszGid, pOD, SG_FALSE)  );
			SG_ERR_CHECK(  _pt_update__update_dir_hid(pCtx, pud, pszGid, pOD)  );
			SG_ERR_CHECK(  _pt_update__change_attrbits_on_entry(pCtx, pud, pszGid, pOD)  );
			SG_ERR_CHECK(  _pt_update__change_xattrs_on_entry(pCtx, pud, pszGid, pOD)  );
		}
		else
		{
			// put the created directory into the pendingtree.  This gives us a
			// place to find the repo-path / absolute-path for this entry as
			// the parent(s) get moved/renamed during step 7.  (In theory, once
			// we change the baseline the goal changeset (and maybe rescan), this
			// ptnode will be considered "clean" with respect to the new baseline
			// and stripped out.

			SG_ERR_CHECK(  _pt_update__determine_final_parent_gid(pCtx, pOD, &pszGid_Parent, NULL)  );
			SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent, &ptnParent)  );
			SG_ASSERT(  (ptnParent)  );

			SG_ERR_CHECK(  SG_rbtree__find(pCtx, ptnParent->prbItems, pszGid, &bAlreadyPresent, NULL)  );
			if (bAlreadyPresent)
			{
				SG_ERR_THROW2(  SG_ERR_ASSERT,
								(pCtx, "_pt_update__do__full_6_2: Should not happen [%s]", pszKey_Path)  );
			}

			SG_ERR_CHECK(  _pt_update__synthesize_goal_treenode_entry(pCtx, pOD, &pTNE)  );
			// TODO write a version of sg_ptnode__alloc__entry that steals the given pTNE
			// TODO (so that we don't have to wait for it to copy ours that we're about to
			// TODO throw away anyway) (or even better, have a version of __alloc__entry()
			// TODO that takes the individual fields used to create the TNE and let it do
			// TODO what it pleases.))
			SG_ERR_CHECK(  sg_ptnode__alloc__entry(pCtx, &ptn, pud->pPendingTree, pTNE, pszGid)  );
			// TODO we don't load the full treenode from the goal changeset for this directory,
			// TODO (we probably could using the HID in the TNE, but I'm not sure that matters).
			// TODO but we do need to allocate prbItems now, I think.
			SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &ptn->prbItems)  );
			SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx, ptnParent, ptn)  );
			ptn->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;
			ptn = NULL;
			SG_TREENODE_ENTRY_NULLFREE(pCtx, pTNE);
		}

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter, &bOK, &pszKey_Path, (void **)&pOD)  );
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_TREENODE_ENTRY_NULLFREE(pCtx, pTNE);
	SG_PTNODE_NULLFREE(pCtx, ptn);
}

/**
 * _do__full_composite_update: step 6.
 *
 * [6] Create any missing skeleton of the result tree in either
 *     the WD or the parking lot.  That is, identify all of the
 *     directories that need to exist in the final result and
 *     create them so that the stuff can be moved/created *into*
 *     them.
 *
 *     At this point, we only worry about creating the directories;
 *     we'll worry about xattrs and attrbits *on* the directory
 *     later.
 *
 */
static void _pt_update__do__full_6(SG_context * pCtx,
								   struct _pt_update_data * pud)
{
	SG_rbtree * prbDirsSortedByTransientPath = NULL;	// map[path --> pOD]

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: begin [6]\n")  );
#endif

	SG_ASSERT(  (pud->bInDoFull)  );

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prbDirsSortedByTransientPath)  );

	// Step 6.1: Identify the directories that need to be created to
	//           fully populate the skeleton of the tree in the result.
	//           Add these to a queue sorted by the transient pathname
	//           so that we can be independent of the GIDs.

	SG_ERR_CHECK(  _pt_update__do__full_6_1(pCtx, pud, prbDirsSortedByTransientPath)  );

	// Step 6.2: Use the sorted queue to create the parent directories
	//           before the child directories when we have multiple
	//           levels of directories to create.

	SG_ERR_CHECK(  _pt_update__do__full_6_2(pCtx, pud, prbDirsSortedByTransientPath)  );

fail:
	SG_RBTREE_NULLFREE(pCtx, prbDirsSortedByTransientPath);
}

//////////////////////////////////////////////////////////////////

/**
 * Get transient absolute path for the file/symlink/directory
 * represented by the given ptnode.  This may be in the WD
 * proper or in the parking lot.  When the it is in the WD
 * proper, it is where the pendingtree currently thinks it is.
 */
static void _pt_update__get_transient_path(SG_context * pCtx,
										   struct _pt_update_data * pud,
										   struct sg_ptnode * ptn,
										   SG_pathname ** ppPath)
{
	SG_pathname * pPath_Allocated = NULL;
	struct _inv_entry * pInvEntry = NULL;
	const char * pszEntryName = NULL;
	const SG_pathname * pPathParked = NULL;		// we do not own this
	SG_bool bFound, bExists;

	if (!ptn->pszgid || !ptn->pszgid[0])
	{
		// ptn is the super-root
		SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__COPY(pCtx,ppPath,pud->pPendingTree->pPathWorkingDirectoryTop)  );
		return;
	}

	SG_ERR_CHECK_RETURN(  SG_inv_dirs__fetch_source_binding(pCtx,pud->pInvDirs,ptn->pszgid,&bFound,&pInvEntry,NULL)  );
	if (bFound)
		SG_ERR_CHECK(  SG_inv_entry__get_parked_path__ref(pCtx, pInvEntry, &pPathParked)  );
	if (pPathParked)
	{
		// if the swap path still exists on disk, then we haven't restored it into the working-directory yet.
		//
		// TODO 3/23/10 does this "if exists" cause our answer to change when doing "--test" only vs
		// TODO 3/23/10 when we are actually doing it and have "--verbose" turned on?
		// TODO
		// TODO 3/25/10 when step 7 does (or pretends to do) the work on an
		// TODO 3/25/10 pInvEntry, should we free pParkedPath -- because in theory it is no longer
		// TODO 3/25/10 parked.  that would eliminate the discrepancy between --test and --verbose
		// TODO 3/25/10 runs, right?  It also help us split the REVERT code into separate
		// TODO 3/25/10 build-the-list-of-stuff-to-do and do-the-list steps.

		SG_ERR_CHECK_RETURN(  SG_fsobj__exists__pathname(pCtx,pPathParked,&bExists,NULL,NULL)  );
		if (bExists)
		{
			SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__COPY(pCtx,ppPath,pPathParked)  );
			return;
		}
	}

	SG_ASSERT(  (ptn->pCurrentParent)  );
	SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx,pud,ptn->pCurrentParent,&pPath_Allocated)  );

	SG_ERR_CHECK(  sg_ptnode__get_name(pCtx,ptn,&pszEntryName)  );	// get current or fallback to baseline

#if TRACE_UPDATE
	{
		const char * pszOld;
		SG_ERR_CHECK(  sg_ptnode__get_old_name(pCtx, ptn, &pszOld)  );
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "_pt_update__get_transient_path: [cur %s][old %s][saveflags %d][tempflag %d]\n",
								   pszEntryName, pszOld, ptn->saved_flags, ptn->temp_flags)  );
	}
#endif

	if (pszEntryName && (strcmp(pszEntryName,"@") != 0))
		SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pPath_Allocated,pszEntryName)  );

	*ppPath = pPath_Allocated;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx,pPath_Allocated);
}

static void _pt_update__set_attrbits(SG_context * pCtx,
									 struct _pt_update_data * pud,
									 const char * pszGid,
									 SG_treediff2_ObjectData * pOD,
									 const SG_pathname * pPath,
									 const char * pszReason)
{
	SG_int64 attrbits;

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_attrbits(pCtx, pOD, &attrbits)  );

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_update__update_attrbits: setting ATTRBITS to [0x%x]\n"
								"                                             for %s\n"),
							   ((SG_uint32)attrbits),
							   SG_pathname__sz(pPath))  );
#endif
	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
		SG_ERR_CHECK(  _pt_log__attrbits(pCtx,pud->pPendingTree,
										 pszGid,
										 SG_pathname__sz(pPath),
										 attrbits,
										 pszReason)  );
	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		SG_ERR_CHECK(  SG_attributes__bits__apply(pCtx, pPath, attrbits)  );

fail:
	return;
}

static void _pt_update__set_xattrs(SG_context * pCtx,
								   struct _pt_update_data * pud,
								   const char * pszGid,
								   SG_treediff2_ObjectData * pOD,
								   const SG_pathname * pPath,
								   const char * pszReason)
{
	SG_vhash * pvhXAttrs = NULL;
	const char * pszHidXAttrs;

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_xattrs(pCtx, pOD, &pszHidXAttrs)  );

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_update__set_xattrs: settings XATTRs to [hid %s]\n"
								"                                       for %s\n"),
							   ((pszHidXAttrs) ? pszHidXAttrs : "(null)"),
							   SG_pathname__sz(pPath))  );
#endif

    if (pszHidXAttrs)
        SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, pud->pPendingTree->pRepo, pszHidXAttrs, &pvhXAttrs)  );

	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
		SG_ERR_CHECK(  _pt_log__xattrs(pCtx,pud->pPendingTree,
									   pszGid,
									   SG_pathname__sz(pPath),
									   pszHidXAttrs,
									   pvhXAttrs,
									   pszReason)  );
#ifdef SG_BUILD_FLAG_FEATURE_XATTR
	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		SG_ERR_CHECK(  SG_attributes__xattrs__apply(pCtx, pPath, pvhXAttrs, pud->pPendingTree->pRepo)  );
#else
	// TODO 3/29/10 What to do on NON-XATTR platforms?  I'm setting it so that
	// TODO 3/29/10 we do everything but actually update the XATTRs on the
	// TODO 3/29/10 filesystem.  Granted, there is nothing that this platform
	// TODO 3/29/10 can do with them, but we kinda need to keep the HID in the
	// TODO 3/29/10 pendingtree so that it doesn't look like this Windows user
	// TODO 3/29/10 stripped off the XATTRs that a MAC user put on a file.
#endif

fail:
    SG_VHASH_NULLFREE(pCtx, pvhXAttrs);
}

/**
 * Do the actual dirty work of creating or updating the target of a SYMLINK
 */
static void _pt_update__set_symlink(SG_context * pCtx,
									struct _pt_update_data * pud,
									const char * pszGid,
									SG_treediff2_ObjectData * pOD,
									const SG_pathname * pPath,
									const char * pszReason)
{
	SG_string * pStringLink = NULL;
	SG_byte * pBytes = NULL;
	const char * pszHid;
	SG_uint64 iLenBytes = 0;

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_content_hid(pCtx, pOD, &pszHid)  );

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_update__set_symlink: symlink [gid %s]\n"
								"                                 [hid %s]\n"
								"                              to %s\n"),
							   pszGid,
							   pszHid,
							   SG_pathname__sz(pPath))  );
#endif

	SG_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pud->pPendingTree->pRepo, pszHid, &pBytes, &iLenBytes)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC__BUF_LEN(pCtx, &pStringLink, pBytes, (SG_uint32) iLenBytes)  );
	SG_NULLFREE(pCtx, pBytes);

	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
		SG_ERR_CHECK(  _pt_log__restore_symlink(pCtx,pud->pPendingTree,
												pszGid,
												SG_pathname__sz(pPath),
												SG_string__sz(pStringLink),
												pszReason)  );
	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
	{
		SG_bool bFound;

		SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath, &bFound, NULL, NULL)  );
		if (bFound)
		{
			// To replace/update the target of a symlink we need to delete the existing
			// symlink and re-create it with the new target; we can't just change the target.

			SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath)  );
		}

		SG_ERR_CHECK(  SG_fsobj__symlink(pCtx, pStringLink, pPath)  );
	}

fail:
	SG_STRING_NULLFREE(pCtx, pStringLink);
	SG_NULLFREE(pCtx, pBytes);
}

/**
 * Do the actual dirty work of fetching the contents of a file from
 * the repo and writing it to the WD.
 */
static void _pt_update__fetch_file(SG_context * pCtx,
								   struct _pt_update_data * pud,
								   const char * pszGid,
								   SG_treediff2_ObjectData * pOD,
								   const SG_pathname * pPath,
								   SG_bool bCreateNew,
								   const char * pszReason)
{
	SG_file * pFile = NULL;
	SG_file_flags flags;
	const char * pszHid;
	SG_fsobj_stat stat_now_current;

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_content_hid(pCtx, pOD, &pszHid)  );

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_update__set_file: fetching file contents [gid %s]\n"
								"                                             [hid %s]\n"
								"                                             [bCreateNew %d]\n"
								"                                             [reason %s]\n"
								"                                          to %s\n"),
							   pszGid,
							   pszHid,
							   bCreateNew,
							   pszReason,
							   SG_pathname__sz(pPath))  );
#endif
	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
		SG_ERR_CHECK(  _pt_log__restore_file(pCtx,pud->pPendingTree,
											 pszGid,
											 SG_pathname__sz(pPath),
											 pszHid,
											 pszReason)  );
	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
	{
		if (bCreateNew)
			flags = SG_FILE_WRONLY | SG_FILE_CREATE_NEW;
		else
			flags = SG_FILE_WRONLY | SG_FILE_OPEN_OR_CREATE | SG_FILE_TRUNC;

#if 0
		{
			SG_bool bExists;
			SG_fsobj_perms perms = 0;
			SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath, &bExists, NULL, &perms)  );
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
									   "Before OPEN [bExists %d][perms %04o]\n",
									   bExists, perms)  );
		}
#endif
		// we just default the mode bits to (0700 - umask) and no XATTRS
		// for the create/open.  this may or may not match our desired
		// final result, but we'll fix them in a minute.

		SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, flags, SG_FSOBJ_PERMS__MASK, &pFile)  );
		SG_ERR_CHECK(  SG_repo__fetch_blob_into_file(pCtx, pud->pPendingTree->pRepo, pszHid, pFile, NULL)  );
		SG_FILE_NULLCLOSE(pCtx, pFile);

#if 0
		{
			SG_bool bExists;
			SG_fsobj_perms perms = 0;
			SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath, &bExists, NULL, &perms)  );
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
									   "After OPEN [bExists %d][perms %04o]\n",
									   bExists, perms)  );
		}
#endif
		// update the timestamp cache for the newly created/updated file.
		// we don't have a ptnode for it yet, but if we did the pszCurrentHid would be null.

		SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath, &stat_now_current)  );
		SG_ERR_CHECK(  SG_pendingtree__set_wd_file_timestamp__dont_save_pendingtree(pCtx,
																					pud->pPendingTree,
																					pszGid,
																					stat_now_current.mtime_ms)  );
	}

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
}


/**
 * Create Entry.
 *
 * The entry with this GID was ADDED in the goal changeset.
 * It was not present in the baseline or the WD.
 *
 * Create the entry as it was in the goal changeset.  This
 * includes creating the item and setting the attrbits and xattrs.
 * If the entry is a directory, the create was already handled
 * in [6.2] during the skeleton step.
 *
 * Sine we already checked for entyrname collisions in the
 * final result, we can assume that there doesn't exist any
 * thing in the WD on disk with this entryname, so we shouldn't
 * need to make a backup.
 *
 * TODO HOWEVER, if there are portability issues and warnings
 * TODO were turned off, we may have some work to do.
 */
static void _pt_update__create_entry(SG_context * pCtx,
									 struct _pt_update_data * pud,
									 const char * pszGid,
									 SG_treediff2_ObjectData * pOD)
{
	SG_pathname * pPath_Parent = NULL;
	SG_pathname * pPath = NULL;
	SG_treenode_entry * pTNE = NULL;
	struct sg_ptnode * ptn = NULL;
	const char * pszGid_Parent = NULL;
	const char * pszEntryName = NULL;
	struct sg_ptnode * ptnParent = NULL;
	SG_treenode_entry_type tneType;

	// get the absolute path the parent directory as it currently
	// exists on disk (either in the WD or in the parking lot).
	// this is where the pendingtree CURRENTLY thinks it is.

	SG_ERR_CHECK(  _pt_update__determine_final_parent_gid(pCtx, pOD, &pszGid_Parent, NULL)  );
	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent, &ptnParent)  );
	SG_ASSERT(  (ptnParent)  );
	SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx, pud, ptnParent, &pPath_Parent)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, pOD, &pszEntryName)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pPath_Parent, pszEntryName)  );

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   ("_pt_update__create_entry: [path %s]\n"),
							   SG_pathname__sz(pPath))  );
#endif

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_tneType(pCtx, pOD, &tneType)  );
	if (tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		// if we are a directory, step 6.2 already created the directory.

#if defined(DEBUG)
		if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		{
            SG_bool bFound = SG_FALSE;

			SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath, &bFound, NULL, NULL)  );
			SG_ASSERT(  (bFound)  );
		}
#endif
	}
	else if (tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
	{
		SG_ERR_CHECK(  _pt_update__fetch_file(pCtx, pud, pszGid, pOD, pPath, SG_TRUE, "CREATE")  );

		SG_ERR_CHECK(  _pt_update__synthesize_goal_treenode_entry(pCtx, pOD, &pTNE)  );
		// TODO write a version of sg_ptnode__alloc__entry that steals the given pTNE
		// TODO (so that we don't have to wait for it to copy ours that we're about to
		// TODO throw away anyway) (or even better, have a version of __alloc__entry()
		// TODO that takes the individual fields used to create the TNE and let it do
		// TODO what it pleases.))
		SG_ERR_CHECK(  sg_ptnode__alloc__entry(pCtx, &ptn, pud->pPendingTree, pTNE, pszGid)  );
		SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx, ptnParent, ptn)  );
		ptn->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;
		ptn = NULL;
		SG_TREENODE_ENTRY_NULLFREE(pCtx, pTNE);
	}
	else if (tneType == SG_TREENODEENTRY_TYPE_SYMLINK)
	{
		SG_ERR_CHECK(  _pt_update__set_symlink(pCtx, pud, pszGid, pOD, pPath, "CREATE")  );

		SG_ERR_CHECK(  _pt_update__synthesize_goal_treenode_entry(pCtx, pOD, &pTNE)  );
		// TODO write a version of sg_ptnode__alloc__entry that steals the given pTNE.
		SG_ERR_CHECK(  sg_ptnode__alloc__entry(pCtx, &ptn, pud->pPendingTree, pTNE, pszGid)  );
		SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx, ptnParent, ptn)  );
		ptn->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;
		ptn = NULL;
		SG_TREENODE_ENTRY_NULLFREE(pCtx, pTNE);
	}
	else
	{
		SG_ERR_THROW(  SG_ERR_INVALID_PENDINGTREE_OBJECT_TYPE  );
	}

	// set the attrbits and XAttrs on disk that were set when the entry
	// was created in the goal changeset.  (The values here should
	// match the values used when we synthesized the TNE and created
	// the ptnode above.)
	//
	// IT IS IMPORTANT that we DO NOT set "current" fields in the PTNODE
	// for things which we put in the TNE because we want these PTNODEs
	// to look as if they are CLEAN W/R/T the GOAL changeset (after we
	// do the switch).

	SG_ERR_CHECK(  _pt_update__set_attrbits(pCtx, pud, pszGid, pOD, pPath, "CREATE")  );

#ifdef SG_BUILD_FLAG_FEATURE_XATTR
	SG_ERR_CHECK(  _pt_update__set_xattrs(pCtx, pud, pszGid, pOD, pPath, "CREATE")  );
#endif

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Parent);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_TREENODE_ENTRY_NULLFREE(pCtx, pTNE);
	SG_PTNODE_NULLFREE(pCtx, ptn);
}

enum _de_flags
{
	_de_flags__unmodified,		// GID was DELETED in goal changeset and UNMODIFIED in (possibly) dirty WD.
	_de_flags__lost,			// GID was DELETED in goal changeset and LOST in dirty WD.
	_de_flags__also_deleted		// GID was DELETED in goal changeset and DELETED in dirty WD.
};

/**
 * Delete entry.
 *
 * The entry with this GID was DELETED in the goal changeset.
 * It was either LOST or UNMODIFIED in the dirty WD.
 *
 * Delete it from the disk (if necessary) and update the pendingtree.
 */
static void _pt_update__delete_entry(SG_context * pCtx,
									 struct _pt_update_data * pud,
									 const char * pszGid,
									 SG_treediff2_ObjectData * pOD,
									 enum _de_flags deFlags)
{
	SG_pathname * pPath_Allocated = NULL;
	const SG_pathname * pPath = NULL;
	const SG_pathname * pPathParked = NULL;		// we do not own this
	struct sg_ptnode * ptn_owned = NULL;
	const char * pszGid_Parent = NULL;
	const char * pszEntryName = NULL;
	struct sg_ptnode * ptnParent = NULL;
	struct _inv_entry * pInvEntry = NULL;
	SG_treenode_entry_type tneType;
	SG_bool bFound;

	// get the absolute path of the parent directory as it currently
	// exists on disk (either in the WD or in the parking lot).
	// this is where the pendingtree CURRENTLY thinks it is.

	SG_ERR_CHECK(  _pt_update__determine_final_parent_gid(pCtx, pOD, &pszGid_Parent, NULL)  );
	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent, &ptnParent)  );
	SG_ASSERT(  (ptnParent)  );

	// find the current location of the entry.  it may be in the WD
	// or in the parking lot.

	SG_ERR_CHECK(  SG_inv_dirs__fetch_source_binding(pCtx, pud->pInvDirs, pszGid, &bFound, &pInvEntry, NULL)  );
	if (bFound)
		SG_ERR_CHECK(  SG_inv_entry__get_parked_path__ref(pCtx, pInvEntry, &pPathParked)  );
	if (pPathParked)
	{
		pPath = pPathParked;
	}
	else
	{
		SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx, pud, ptnParent, &pPath_Allocated)  );
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, pOD, &pszEntryName)  );
		SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath_Allocated, pszEntryName)  );

		pPath = pPath_Allocated;
	}

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   ("_pt_update__delete_entry: [path %s] [deFlags %d]\n"),
							   SG_pathname__sz(pPath),
							   deFlags)  );
#endif

	switch (deFlags)
	{
	default:		// quiets compiler
		SG_ASSERT(  (0)  );
		break;

	case _de_flags__also_deleted:
#if defined(DEBUG)
		if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		{
			SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath, &bFound, NULL, NULL)  );
			SG_ASSERT(  (bFound == SG_FALSE)  );
		}
#endif
		break;

	case _de_flags__lost:
#if defined(DEBUG)
		if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		{
			SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath, &bFound, NULL, NULL)  );
			SG_ASSERT(  (bFound == SG_FALSE)  );
		}
#endif
		break;

	case _de_flags__unmodified:
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_tneType(pCtx, pOD, &tneType)  );
		if (tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
		{
			// TODO 3/26/10 I'm going to assume that the directory is empty and that we
			// TODO 3/26/10 don't/shouldn't do a recursive rmdir.
			// TODO 3/26/10 If this throws because the directory isn't empty, we have a bug.

			if (pud->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
				SG_ERR_CHECK(  _pt_log__rmdir(pCtx,pud->pPendingTree,
											  pszGid,
											  SG_pathname__sz(pPath),
											  "RMDIR")  );
			if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
				SG_ERR_CHECK(  SG_fsobj__rmdir__pathname(pCtx, pPath)  );
		}
		else if (tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
		{
			// No backups; just delete the file.
			// The caller confirmed that this file wasn't changed in the WD.

			if (pud->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
				SG_ERR_CHECK(  _pt_log__delete_file(pCtx,pud->pPendingTree,
													pszGid,
													SG_pathname__sz(pPath),
													"DELETE")  );
			if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
			{
				SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath)  );
				SG_ERR_CHECK(  SG_pendingtree__clear_wd_file_timestamp(pCtx, pud->pPendingTree, pszGid)  );
			}
		}
		else if (tneType == SG_TREENODEENTRY_TYPE_SYMLINK)
		{
			// No backups; just delete the link.
			//
			// TODO 3/26/10 How does this work on Windows?

			if (pud->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
				SG_ERR_CHECK(  _pt_log__delete_symlink(pCtx,pud->pPendingTree,
													   pszGid,
													   SG_pathname__sz(pPath),
													   "DELETE")  );
			if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
				SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath)  );
		}
		else
		{
			SG_ERR_THROW(  SG_ERR_INVALID_PENDINGTREE_OBJECT_TYPE  );
		}
		break;
	}

	// remove the ptnode from the pendingtree.

	SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, ptnParent->prbItems, pszGid, (void **)&ptn_owned)  );

fail:
	SG_PTNODE_NULLFREE(pCtx, ptn_owned);
	SG_PATHNAME_NULLFREE(pCtx, pPath_Allocated);
}

/**
 * MOVE entry (with possible RENAME).
 *
 * The entry with this GID was MOVED or MOVED+RENAMED in
 * the goal changeset.
 *
 * Since the filesystem rename() command takes 2 full pathnames, we
 * can handle both MOVES and RENAMES in one step.
 *
 * There are several cases here:
 *     MOVED or MOVED+RENAMED in the goal;
 *     UNCHANGED, RENAMED, or ***similarly*** MOVED or MOVED+RENAMED in the dirty WD.
 * But they all work out the same (assuming that there is actually
 * some work to do (and we've already rejected divergent changes)).
 *
 * Move the entry on the disk and update the pendingtree.
 * It may be currently in the tree or in the parking lot.
 */
static void _pt_update__move_entry_with_possible_rename(SG_context * pCtx,
														struct _pt_update_data * pud,
														const char * pszGid,
														SG_treediff2_ObjectData * pOD,
														SG_bool bTouchWD)
{
	SG_pathname * pPath_Parent_goal = NULL;
	SG_pathname * pPath_wd = NULL;
	SG_pathname * pPath_goal = NULL;
	const char * pszGid_Parent_wd = NULL;
	const char * pszGid_Parent_goal = NULL;
	const char * pszGid_Parent_baseline = NULL;
	const char * pszEntryName_wd = NULL;
	const char * pszEntryName_goal = NULL;
	struct sg_ptnode * ptnParent_wd = NULL;
	struct sg_ptnode * ptnParent_goal = NULL;
	struct sg_ptnode * ptnParent_baseline = NULL;
	struct sg_ptnode * ptn_wd = NULL;
	struct sg_ptnode * ptn_OWNED = NULL;
	SG_bool bFound;

	SG_diffstatus_flags dsFlags_goal_vs_baseline;
	SG_diffstatus_flags dsFlags_baseline_vs_wd;
	SG_diffstatus_flags dsFlags_composite;
	SG_diffstatus_flags dsFlags_baseline_vs_goal;

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_composite_dsFlags_by_part(pCtx, pOD,
																		   &dsFlags_goal_vs_baseline,
																		   &dsFlags_baseline_vs_wd,
																		   &dsFlags_composite)  );
	SG_ERR_CHECK(  SG_treediff2__Invert_dsFlags(pCtx, dsFlags_goal_vs_baseline, &dsFlags_baseline_vs_goal)  );
	SG_ASSERT(   (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__MOVED)  );

	SG_ERR_CHECK(  SG_treediff2__ObjectData__composite__get_parent_gids(pCtx, pOD,
																		&pszGid_Parent_goal,
																		&pszGid_Parent_baseline,
																		&pszGid_Parent_wd)  );
	if (!pszGid_Parent_wd || !*pszGid_Parent_wd)
		pszGid_Parent_wd = pszGid_Parent_baseline;

	// get the full path of the version on disk.  this is where the
	// entry is in the dirty WD.  this may be in the tree-proper or
	// it may be in the parking lot.

	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent_wd, &ptnParent_wd)  );
	SG_ASSERT(  (ptnParent_wd)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_name(pCtx, pOD, &pszEntryName_wd)  );
	SG_ERR_CHECK(  SG_rbtree__find(pCtx, ptnParent_wd->prbItems, pszGid, &bFound, (void *)&ptn_wd)  );
	SG_ASSERT(  (bFound)  );
	SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx, pud, ptn_wd, &pPath_wd)  );

	// get the destination path -- where it was moved to and possibly renamed as
	// in the goal changeset.

	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent_goal, &ptnParent_goal)  );
	SG_ASSERT(  (ptnParent_goal)  );
	SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx, pud, ptnParent_goal, &pPath_Parent_goal)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, pOD, &pszEntryName_goal)  );

	// allow for MOVE or MOVE+RENAME in goal.  take the entryname from the goal
	// if changed there but fall back to the WD name incase it was renamed.

	if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__RENAMED)
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_goal, pPath_Parent_goal, pszEntryName_goal)  );
	else
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_goal, pPath_Parent_goal, pszEntryName_wd)  );

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   ("_pt_update__move_entry_with_possible_rename: from [%s]\n"
								"                                               to [%s]\n"),
							   SG_pathname__sz(pPath_wd),
							   SG_pathname__sz(pPath_goal))  );
#endif

//	SG_ASSERT(  (strcmp(SG_pathname__sz(pPath_goal), SG_pathname__sz(pPath_wd)) != 0)  );
//	SG_ASSERT(  (ptnParent_goal != ptnParent_wd)  );

	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
	{
		const char * pszMsg = NULL;

		if (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_MOVED)
		{
			if (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_RENAMED)
			{
				pszMsg = "SIMILAR_MOVE_WITH_SIMILAR_RENAME";
			}
			else if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__RENAMED)
			{
				pszMsg = "SIMILAR_MOVE_WITH_RENAME";
			}
			else
			{
				pszMsg = "SIMILAR_MOVE";
			}
		}
		else /* if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__MOVED) */
		{
			if (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_RENAMED)
			{
				pszMsg = "MOVED_WITH_SIMILAR_RENAME";
			}
			else if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__RENAMED)
			{
				pszMsg = "MOVED_WITH_RENAME";
			}
			else
			{
				pszMsg = "MOVED";
			}
		}

		SG_ERR_CHECK(  _pt_log__move(pCtx, pud->pPendingTree,
									 pszGid,
									 SG_pathname__sz(pPath_wd),
									 SG_pathname__sz(pPath_goal),
									 pszMsg)  );
	}
	if (bTouchWD && (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT))
	{
		SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_Parent_goal, &bFound, NULL, NULL)  );
		SG_ASSERT(  (bFound)  );

		if (strcmp(SG_pathname__sz(pPath_goal), SG_pathname__sz(pPath_wd)) != 0)
		{
			SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_goal, &bFound, NULL, NULL)  );
			if (bFound)
				SG_ERR_THROW2(  SG_ERR_NOTIMPLEMENTED,
								(pCtx,
								 "_pt_update__move_entry_with_possible_rename: deal with collisions caused by case-insensitive filesystems [goal %s]",
								 SG_pathname__sz(pPath_goal))  );

			SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx, pPath_wd, pPath_goal)  );
		}
	}

	// update the pendingtree.  note that we only have PTNODES for
	// the baseline/WD version; NOT the goal version.

	if (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_MOVED)
	{
		SG_ASSERT(  (ptnParent_goal == ptnParent_wd)  );

		// both goal changeset and the dirty WD moved the entry to
		// the same directory.  the current baseline should have a
		// phantom node.
		//
		// we need to delete the phantom node and keep the existing
		// WD node and clear any "current" fields in it so that it
		// looks clear (relative to the goal changeset).

		SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent_baseline, &ptnParent_baseline)  );
		SG_ASSERT(  (ptnParent_baseline)  );
		SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, ptnParent_baseline->prbItems, pszGid, (void **)&ptn_OWNED)  );
		SG_ASSERT(  (ptn_OWNED)  );
		SG_ASSERT(  (ptn_OWNED->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)  );
		SG_PTNODE_NULLFREE(pCtx, ptn_OWNED);

		// parent GID is not stored in the TNE, so we don't need
		// to update ptn_wd->pBaselineEntry.

		ptn_wd->pszgidMovedFrom = NULL;
	}
	else
	{
		// since we were moved in the goal and not the WD, we SHOULD NOT
		// have a phantom/moved-away ptnode; only a ptnode for the entry
		// in the WD directory.
		//
		// we need to move the current PTNODE to the destination directory
		// and directly update the TNE fields.
		//
		// we DO NOT create a PHANTOM node nor update the "current" fields
		// in the ptnode because we want it to look CLEAN after we switch
		// baselines.

		SG_ASSERT(  (ptnParent_goal != ptnParent_wd)  );

		SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, ptnParent_wd->prbItems, pszGid, (void **)&ptn_OWNED)  );
		SG_ASSERT(  (ptn_OWNED == ptn_wd)  );
		SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx, ptnParent_goal, ptn_OWNED)  );
		ptn_OWNED = NULL;

		// parent GID is not stored in the TNE, so we don't need
		// to update ptn_wd->pBaselineEntry.

		ptn_wd->pszgidMovedFrom = NULL;
	}

	SG_ASSERT(  (ptn_wd->pBaselineEntry)  );
	if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__RENAMED)
	{
		// if the entry was renamed in the goal, we update the TNE so that it
		// will appear as if it always had that name.
		//
		// we also do this even for UNDONE_RENAMED entries so that we clear the "current" fields.

		SG_ERR_CHECK(  SG_treenode_entry__set_entry_name(pCtx, ptn_wd->pBaselineEntry, pszEntryName_goal)  );
		SG_ASSERT(  (ptn_wd->pszCurrentName == NULL)  ||  (strcmp(ptn_wd->pszCurrentName, pszEntryName_goal) == 0)  );
		ptn_wd->pszCurrentName = NULL;
	}

	ptn_wd->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;

fail:
	SG_PATHNAME_NULLFREE(pCtx,pPath_wd);
	SG_PATHNAME_NULLFREE(pCtx,pPath_Parent_goal);
	SG_PATHNAME_NULLFREE(pCtx,pPath_goal);
	SG_PTNODE_NULLFREE(pCtx, ptn_OWNED);
}

/**
 * RENAME entry.
 *
 * The entry with this GID was RENAMED (but not MOVED) in
 * the goal changeset.
 *
 * There are several cases here:
 *     UNCHANGED, MOVED or ***similarly*** RENAMED in the dirty WD.
 * But they all work out the same (assuming that there is actually
 * some work to do (and we've already rejected divergent changes)).
 *
 * Move the entry on the disk and update the pendingtree.
 * It may be currently in the tree or in the parking lot.
 */
static void _pt_update__rename_entry(SG_context * pCtx,
									 struct _pt_update_data * pud,
									 const char * pszGid,
									 SG_treediff2_ObjectData * pOD,
									 SG_bool bTouchWD)
{
	SG_pathname * pPath_Parent_wd = NULL;
	SG_pathname * pPath_wd = NULL;
	SG_pathname * pPath_goal = NULL;
	const char * pszGid_Parent_wd = NULL;
	const char * pszEntryName_wd = NULL;
	const char * pszEntryName_goal = NULL;
	struct sg_ptnode * ptnParent_wd = NULL;
	struct sg_ptnode * ptn_wd = NULL;
	struct sg_ptnode * ptn_OWNED = NULL;
	SG_bool bFound;

	SG_diffstatus_flags dsFlags_goal_vs_baseline;
	SG_diffstatus_flags dsFlags_baseline_vs_wd;
	SG_diffstatus_flags dsFlags_composite;
	SG_diffstatus_flags dsFlags_baseline_vs_goal;

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_composite_dsFlags_by_part(pCtx, pOD,
																		   &dsFlags_goal_vs_baseline,
																		   &dsFlags_baseline_vs_wd,
																		   &dsFlags_composite)  );
	SG_ERR_CHECK(  SG_treediff2__Invert_dsFlags(pCtx, dsFlags_goal_vs_baseline, &dsFlags_baseline_vs_goal)  );
	SG_ASSERT(   (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__RENAMED)      );
	SG_ASSERT(  ((dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__MOVED) == 0)  );

	// get the full path of the version on disk.  this is where the
	// entry is in the dirty WD.  this may be in the tree-proper or
	// it may be in the parking lot.

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_parent_gid(pCtx, pOD, &pszGid_Parent_wd)  );
	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent_wd, &ptnParent_wd)  );
	SG_ASSERT(  (ptnParent_wd)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_name(pCtx, pOD, &pszEntryName_wd)  );
	SG_ERR_CHECK(  SG_rbtree__find(pCtx, ptnParent_wd->prbItems, pszGid, &bFound, (void *)&ptn_wd)  );
	SG_ASSERT(  (bFound)  );
	SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx, pud, ptnParent_wd, &pPath_Parent_wd)  );
	SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx, pud, ptn_wd, &pPath_wd)  );

	// get the destination path -- where it was renamed as in the goal changeset.

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, pOD, &pszEntryName_goal)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_goal, pPath_Parent_wd, pszEntryName_goal)  );

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   ("_pt_update__rename_entry: from [%s]\n"
								"                            to [%s]\n"),
							   SG_pathname__sz(pPath_wd),
							   SG_pathname__sz(pPath_goal))  );
#endif

	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
	{
		const char * pszMsg = NULL;

		if (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_RENAMED)
		{
			pszMsg = "SIMILAR_RENAME";
		}
		else /* if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__RENAMED) */
		{
			pszMsg = "RENAMED";
		}

		SG_ERR_CHECK(  _pt_log__move(pCtx, pud->pPendingTree,
									 pszGid,
									 SG_pathname__sz(pPath_wd),
									 SG_pathname__sz(pPath_goal),
									 pszMsg)  );
	}
	if (bTouchWD && (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT))
	{
		SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_Parent_wd, &bFound, NULL, NULL)  );
		SG_ASSERT(  (bFound)  );

		if (strcmp(SG_pathname__sz(pPath_goal), SG_pathname__sz(pPath_wd)) != 0)
		{
			SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_goal, &bFound, NULL, NULL)  );
			if (bFound)
				SG_ERR_THROW2(  SG_ERR_NOTIMPLEMENTED,
								(pCtx,
								 "_pt_update__rename_entry: deal with collisions caused by case-insensitive filesystems [goal %s]",
								 SG_pathname__sz(pPath_goal))  );

			SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx, pPath_wd, pPath_goal)  );
		}
	}

	// update the pendingtree.  note that we only have PTNODES for
	// the baseline/WD version; NOT the goal version.
	//
	// we have a simple RENAME in the goal so the PTNODE is already
	// in the right directory.  all we have to do is update the entryname.
	// we also do this for UNDONE_RENAMED entries so that we clear the "current" fields.

	SG_ASSERT(  (ptn_wd->pBaselineEntry)  );
	SG_ERR_CHECK(  SG_treenode_entry__set_entry_name(pCtx, ptn_wd->pBaselineEntry, pszEntryName_goal)  );
	SG_ASSERT(  (ptn_wd->pszCurrentName == NULL)  ||  (strcmp(ptn_wd->pszCurrentName, pszEntryName_goal) == 0)  );
	ptn_wd->pszCurrentName = NULL;

	ptn_wd->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;

fail:
	SG_PATHNAME_NULLFREE(pCtx,pPath_Parent_wd);
	SG_PATHNAME_NULLFREE(pCtx,pPath_wd);
	SG_PATHNAME_NULLFREE(pCtx,pPath_goal);
	SG_PTNODE_NULLFREE(pCtx, ptn_OWNED);
}

/**
 * Change ATTRBITS on entry.
 *
 * The ATTRBITS on the entry with this GID were changed in
 * the goal changeset.
 *
 * Update the bits on the disk and the pendingtree.
 */
static void _pt_update__change_attrbits_on_entry(SG_context * pCtx,
												 struct _pt_update_data * pud,
												 const char * pszGid,
												 SG_treediff2_ObjectData * pOD)
{
	SG_pathname * pPath_Parent = NULL;
	SG_pathname * pPath = NULL;
	const char * pszGid_Parent = NULL;
	const char * pszEntryName = NULL;
	struct sg_ptnode * ptnParent = NULL;
	struct sg_ptnode * ptn = NULL;
	SG_int64 attrBits_goal;
	SG_bool bFound;

	// get the absolute path of the parent directory as it currently
	// exists on disk (either in the WD or in the parking lot).
	// this is where the pendingtree CURRENTLY thinks it is.

	SG_ERR_CHECK(  _pt_update__determine_final_parent_gid(pCtx, pOD, &pszGid_Parent, NULL)  );
	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent, &ptnParent)  );
	SG_ASSERT(  (ptnParent)  );
	SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx, pud, ptnParent, &pPath_Parent)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_name(pCtx, pOD, &pszEntryName)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pPath_Parent, pszEntryName)  );

	SG_ERR_CHECK(  _pt_update__set_attrbits(pCtx, pud, pszGid, pOD, pPath, "UPDATE")  );

	// update the pendingtree.  note that we only have PTNODES for
	// the baseline/WD version; NOT the goal version.

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_attrbits(pCtx, pOD, &attrBits_goal)  );

	SG_ERR_CHECK(  SG_rbtree__find(pCtx, ptnParent->prbItems, pszGid, &bFound, (void *)&ptn)  );
	SG_ASSERT(  (bFound)  );
	SG_ASSERT(  (ptn->pBaselineEntry)  );

	SG_ERR_CHECK(  SG_treenode_entry__set_attribute_bits(pCtx, ptn->pBaselineEntry, attrBits_goal)  );
	ptn->iCurrentAttributeBits = attrBits_goal;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Parent);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

/**
 * Change XATTRS on entry.
 *
 * The XATTRS on the entry with this GID were changed in
 * the goal changeset.
 *
 * Update the XATTRS on the disk and the pendingtree.
 */
static void _pt_update__change_xattrs_on_entry(SG_context * pCtx,
											   struct _pt_update_data * pud,
											   const char * pszGid,
											   SG_treediff2_ObjectData * pOD)
{
	SG_pathname * pPath_Parent = NULL;
	SG_pathname * pPath = NULL;
	const char * pszGid_Parent = NULL;
	const char * pszEntryName = NULL;
	struct sg_ptnode * ptnParent = NULL;
	struct sg_ptnode * ptn = NULL;
	const char * pszHidXAttrs = NULL;
	SG_bool bFound;

	// get the absolute path of the parent directory as it currently
	// exists on disk (either in the WD or in the parking lot).
	// this is where the pendingtree CURRENTLY thinks it is.

	SG_ERR_CHECK(  _pt_update__determine_final_parent_gid(pCtx, pOD, &pszGid_Parent, NULL)  );
	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent, &ptnParent)  );
	SG_ASSERT(  (ptnParent)  );
	SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx, pud, ptnParent, &pPath_Parent)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_name(pCtx, pOD, &pszEntryName)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pPath_Parent, pszEntryName)  );

	SG_ERR_CHECK(  _pt_update__set_xattrs(pCtx, pud, pszGid, pOD, pPath, "UPDATE")  );

	// update the pendingtree.  note that we only have PTNODES for
	// the baseline/WD version; NOT the goal version.

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_xattrs(pCtx, pOD, &pszHidXAttrs)  );

	SG_ERR_CHECK(  SG_rbtree__find(pCtx, ptnParent->prbItems, pszGid, &bFound, (void *)&ptn)  );
	SG_ASSERT(  (bFound)  );
	SG_ASSERT(  (ptn->pBaselineEntry)  );

	// set the XATTRs HID directly in the TNE so that this PTNODE
	// will look CLEAN relative to the goal changeset after we
	// switch to the new baseline.
	//
	// in theory, the ptn->pszHidXAttrs would be NULL because we were
	// told that the xattrs have not changed in the WD. BUT, if the ptnode
	// was populated by a scan-dir, it will have put the HID in the
	// "current" fields -- even though it is the same as the baseline.
	// so, for that reason, we set it in both places to the same value
	// so that it looks "clean".

	SG_ERR_CHECK(  SG_treenode_entry__set_hid_xattrs(pCtx, ptn->pBaselineEntry, pszHidXAttrs)  );
	if (pszHidXAttrs && *pszHidXAttrs)
		SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, ptn->pPendingTree->pStrPool, pszHidXAttrs, &ptn->pszHidXattrs)  );
	else
		ptn->pszHidXattrs = NULL;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Parent);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

/**
 * Record that the contents of the directory are different between
 * the current baseline and goal changeset.  That is, the BLOB-HID
 * on the contents of the directory need to reflect the goal changeset
 * so that subsequent scan-dirs will be relative to the right version.
 *
 * We don't actually do anything to merge their contents; we're just
 * recording that the HID is different.
 */
static void _pt_update__update_dir_hid(SG_context * pCtx,
									   struct _pt_update_data * pud,
									   const char * pszGid,
									   SG_treediff2_ObjectData * pOD)
{
	const char * pszHid = NULL;
	struct sg_ptnode * ptn = NULL;

	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid, &ptn)  );
	SG_ASSERT(  (ptn)  );

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_content_hid(pCtx, pOD, &pszHid)  );

	if (ptn->pBaselineEntry)
	{
		// set the new HID directly in the TNE so that the entry will
		// look CLEAN relative to the new baseline after we switch to
		// the goal changeset.

		SG_ERR_CHECK(  SG_treenode_entry__set_hid_blob(pCtx, ptn->pBaselineEntry, pszHid)  );
	}

	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, ptn->pPendingTree->pStrPool, pszHid, &ptn->pszCurrentHid)  );

fail:
	return;
}

/**
 * Replace/update the target of a SYMLINK.
 *
 * That is, the content HID of a SYMLINK has changed and we want to
 * update the link on disk and the pendingtree to reflect this.
 */
static void _pt_update__update_symlink(SG_context * pCtx,
									   struct _pt_update_data * pud,
									   const char * pszGid,
									   SG_treediff2_ObjectData * pOD,
									   SG_bool bNeedFetch)
{
	SG_pathname * pPath_Parent = NULL;
	SG_pathname * pPath = NULL;
	const char * pszGid_Parent = NULL;
	const char * pszEntryName = NULL;
	const char * pszHid = NULL;
	struct sg_ptnode * ptnParent = NULL;
	struct sg_ptnode * ptn = NULL;
	SG_bool bFound;

	// get the absolute path of the parent directory as it currently
	// exists on disk (either in the WD or in the parking lot).
	// this is where the pendingtree CURRENTLY thinks it is.

	SG_ERR_CHECK(  _pt_update__determine_final_parent_gid(pCtx, pOD, &pszGid_Parent, NULL)  );
	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent, &ptnParent)  );
	SG_ASSERT(  (ptnParent)  );
	SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx, pud, ptnParent, &pPath_Parent)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_name(pCtx, pOD, &pszEntryName)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pPath_Parent, pszEntryName)  );

	if (bNeedFetch)
		SG_ERR_CHECK(  _pt_update__set_symlink(pCtx, pud, pszGid, pOD, pPath, "UPDATE")  );

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_content_hid(pCtx, pOD, &pszHid)  );

	SG_ERR_CHECK(  SG_rbtree__find(pCtx, ptnParent->prbItems, pszGid, &bFound, (void *)&ptn)  );
	SG_ASSERT(  (bFound)  );
	SG_ASSERT(  (ptn->pBaselineEntry)  );

	// set the new HID directly in the TNE so that the entry will
	// look CLEAN relative to the new baseline after we switch to
	// the goal changeset.  we DO NOT set the "current" fields in
	// the PTNODE.
	//
	// in theory, the ptn->pszCurrentHid would be NULL because we
	// were told that the file was not changed in the WD and
	// we are doing a simple UPDATE which changes the content to
	// the HID in the goal changeset.  BUT, if the ptnode was
	// populated by a scan-dir, it will have put the HID in the
	// "current" fields -- even though it is the same as the baseline.
	// so, for that reason, we need to set it in both places so that
	// it still looks "clean".
	// TODO 4/12/10 investigate setting the current value to null
	// TODO 4/12/10 instead of to a copy of the baseline value.

#if defined(DEBUG)
	{
		const char * pszHid_Test;

		SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, ptn->pBaselineEntry, &pszHid_Test)  );
		SG_ASSERT(  (ptn->pszCurrentHid == NULL)  ||  (strcmp(ptn->pszCurrentHid,pszHid_Test) == 0)  );
	}
#endif

	SG_ERR_CHECK(  SG_treenode_entry__set_hid_blob(pCtx, ptn->pBaselineEntry, pszHid)  );
	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, ptn->pPendingTree->pStrPool, pszHid, &ptn->pszCurrentHid)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Parent);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

/**
 * Replace/update the contents of a regular file.
 *
 * That is, the content HID of a regular file has changed in the goal
 * changeset (and did not in the WD) and we want to update the contents
 * on disk and the pendingtree to reflect this.
 *
 * This is a DESTRUCTIVE OVERWRITE WITHOUT BACKUP.
 *
 * If (bNeedFetch) is false, then the caller has determined that we
 * don't need to actually fetch the file contents from the REPO (probably
 * because the user made the same change and this is a dirty update).
 */
static void _pt_update__update_file(SG_context * pCtx,
									struct _pt_update_data * pud,
									const char * pszGid,
									SG_treediff2_ObjectData * pOD,
									SG_bool bNeedFetch)
{
	SG_pathname * pPath_Parent = NULL;
	SG_pathname * pPath = NULL;
	const char * pszGid_Parent = NULL;
	const char * pszEntryName = NULL;
	const char * pszHid = NULL;
	struct sg_ptnode * ptnParent = NULL;
	struct sg_ptnode * ptn = NULL;
	SG_bool bFound;

	// get the absolute path of the parent directory as it currently
	// exists on disk (either in the WD or in the parking lot).
	// this is where the pendingtree CURRENTLY thinks it is.

	SG_ERR_CHECK(  _pt_update__determine_final_parent_gid(pCtx, pOD, &pszGid_Parent, NULL)  );
	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent, &ptnParent)  );
	SG_ASSERT(  (ptnParent)  );
	SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx, pud, ptnParent, &pPath_Parent)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_name(pCtx, pOD, &pszEntryName)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pPath_Parent, pszEntryName)  );

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update__update_file: [bNeedFetch %d][%s]\n",
							   bNeedFetch, SG_pathname__sz(pPath))  );
#endif

	if (bNeedFetch)
		SG_ERR_CHECK(  _pt_update__fetch_file(pCtx, pud, pszGid, pOD, pPath, SG_FALSE, "UPDATE")  );

	SG_ERR_CHECK(  SG_rbtree__find(pCtx, ptnParent->prbItems, pszGid, &bFound, (void *)&ptn)  );
	SG_ASSERT(  (bFound)  );
	SG_ASSERT(  (ptn->pBaselineEntry)  );

	// set the new HID directly in the TNE so that the entry will
	// look CLEAN relative to the new baseline after we switch to
	// the goal changeset.

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_content_hid(pCtx, pOD, &pszHid)  );

#if defined(DEBUG)
	if (bNeedFetch)
	{
		// in theory, the ptn->pszCurrentHid would be NULL because we
		// were told that the file was not changed in the WD and
		// we are doing a simple UPDATE which changes the content to
		// the HID in the goal changeset.  BUT, if the ptnode was
		// populated by a scan-dir, it will have put the HID in the
		// "current" fields -- even though it is the same as the baseline.
		// so, for that reason, we need to set it in both places so that
		// it still looks "clean"

		const char * pszHid_Test;

		SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, ptn->pBaselineEntry, &pszHid_Test)  );
		SG_ASSERT(  (ptn->pszCurrentHid == NULL)  ||  (strcmp(ptn->pszCurrentHid,pszHid_Test) == 0)  );
	}
	else
	{
		// since we didn't have to fetch, we must assume that the dirty WD
		// value exactly matches the goal value (identical change made in
		// goal changeset and in dirty WD).

		SG_ASSERT(  (ptn->pszCurrentHid == NULL)  ||  (strcmp(ptn->pszCurrentHid,pszHid) == 0)  );
	}
#endif

	SG_ERR_CHECK(  SG_treenode_entry__set_hid_blob(pCtx, ptn->pBaselineEntry, pszHid)  );
	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, ptn->pPendingTree->pStrPool, pszHid, &ptn->pszCurrentHid)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Parent);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

/**
 * The contents of the file were changed in both the goal changeset
 * and in the dirty WD.  Try to AUTOMERGE them if possible.
 */
static void _pt_update__merge_files(SG_context * pCtx,
									struct _pt_update_data * pud,
									const char * pszGid,
									SG_treediff2_ObjectData * pOD)
{
	SG_pathname * pPath_Parent = NULL;
	SG_pathname * pPath = NULL;
	const char * pszGid_Parent = NULL;
	const char * pszEntryName = NULL;
	const char * pszHid_wd = NULL;
	const char * pszHid_goal = NULL;
	struct sg_ptnode * ptnParent = NULL;
//	struct sg_ptnode * ptn;
//	SG_bool bFound;

	// get the absolute path of the parent directory as it currently
	// exists on disk (either in the WD or in the parking lot).
	// this is where the pendingtree CURRENTLY thinks it is.

	SG_ERR_CHECK(  _pt_update__determine_final_parent_gid(pCtx, pOD, &pszGid_Parent, NULL)  );
	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent, &ptnParent)  );
	SG_ASSERT(  (ptnParent)  );
	SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx, pud, ptnParent, &pPath_Parent)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_name(pCtx, pOD, &pszEntryName)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pPath_Parent, pszEntryName)  );

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_content_hid(pCtx, pOD, &pszHid_goal)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_content_hid(pCtx, pOD, &pszHid_wd)  );

	SG_ERR_THROW2(  SG_ERR_NOTIMPLEMENTED,
					(pCtx, ("TODO AUTOMERGE: [gid %s]\n"
							"                [path %s]\n"
							"                [hid goal %s]\n"
							"                [hid wd   %s]\n"),
					 pszGid,
					 SG_pathname__sz(pPath),
					 pszHid_goal,
					 pszHid_wd)  );

	// TODO 2010/05/27 Also deal with timestamp cache.

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Parent);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

//////////////////////////////////////////////////////////////////

/**
 * Entry was LOST in the WD.  Override that and restore it
 * so that the GOAL version of the entry can replace it.
 * For example, if the GOAL version did a CHMOD on it, we
 * want to un-lose the file and then apply the new mode bits.
 */
static void _pt_update__restore_lost_entry(SG_context * pCtx,
										   struct _pt_update_data * pud,
										   const char * pszGid,
										   SG_treediff2_ObjectData * pOD,
										   SG_diffstatus_flags dsFlags_baseline_vs_goal)
{
	SG_treenode_entry_type tneType;

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_tneType(pCtx, pOD, &tneType)  );
	if (tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		// We already re-created the directory skeleton in [6].

		return;
	}

	// let the somewhat normal UPDATE code deal with the MOVE/RENAME.
	// that is, move/alter the PTNODE as necessary.  But don't let the
	// MOVE/RENAME code see that the entry doesn't actually exist on disk.

	if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__MOVED)
		SG_ERR_CHECK(  _pt_update__move_entry_with_possible_rename(pCtx, pud, pszGid, pOD, SG_FALSE)  );
	else if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__RENAMED)
		SG_ERR_CHECK(  _pt_update__rename_entry(pCtx, pud, pszGid, pOD, SG_FALSE)  );

	// fetch the GOAL version of the content into the GOAL pathname.

	if (tneType == SG_TREENODEENTRY_TYPE_SYMLINK)
	{
		SG_ERR_CHECK(  _pt_update__update_symlink(pCtx, pud, pszGid, pOD, SG_TRUE)  );
	}
	else if (tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
	{
		SG_ERR_CHECK(  _pt_update__update_file(pCtx, pud, pszGid, pOD, SG_TRUE)  );
	}

	// The __update_ calls did a fetch from the repo and used the
	// default attrbits/xattrs (rather than the proper values from
	// the goal), so we force set them here.

	SG_ERR_CHECK(  _pt_update__change_attrbits_on_entry(pCtx, pud, pszGid, pOD)  );
	SG_ERR_CHECK(  _pt_update__change_xattrs_on_entry(pCtx, pud, pszGid, pOD)  );

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * Step 7.1: Actually perform the simple parts of the structural changes.
 *
 *           This will reshape the WD by applying the various RENAMED, MOVES,
 *           ADDS, and SOME of the DELETES from the goal changeset.
 */
static void _pt_update__do__full_7_1(SG_context * pCtx,
									 struct _pt_update_data * pud,
									 SG_rbtree * prbDirsToBeDeleted)
{
	SG_treediff2_iterator * pIter = NULL;
	const char * pszGid_k;
	SG_treediff2_ObjectData * pOD_k;
	SG_diffstatus_flags dsFlags_goal_vs_baseline;
	SG_diffstatus_flags dsFlags_baseline_vs_wd;
	SG_diffstatus_flags dsFlags_composite;
	SG_diffstatus_flags dsFlags_baseline_vs_goal;
	SG_bool bNeedChangeAttrBits, bNeedChangeXAttrs;
	SG_bool bOK;

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: begin [7.1]\n")  );
#endif

	SG_ERR_CHECK(  SG_treediff2__iterator__first(pCtx, pud->pTreeDiff_composite, &bOK, &pszGid_k, &pOD_k, &pIter)  );
	while (bOK)
	{
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_composite_dsFlags_by_part(pCtx, pOD_k,
																			   &dsFlags_goal_vs_baseline,
																			   &dsFlags_baseline_vs_wd,
																			   &dsFlags_composite)  );
		SG_ERR_CHECK(  SG_treediff2__Invert_dsFlags(pCtx, dsFlags_goal_vs_baseline, &dsFlags_baseline_vs_goal)  );

#if TRACE_UPDATE
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "        [gid %s][baseline-vs-goal %d][baseline-vs-wd %d]\n",
								   pszGid_k,
								   dsFlags_baseline_vs_goal,
								   dsFlags_baseline_vs_wd)  );
#endif

		if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__ADDED)
		{
			// ADDED to goal changeset.
			// confirm that it did-not-exist in baseline and wd.
			SG_ASSERT(  (dsFlags_baseline_vs_wd == SG_DIFFSTATUS_FLAGS__ZERO)  );
			// by construction, an ADDED won't set the modifier bits.
			SG_ASSERT(  ((dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__MASK_MODIFIERS) == 0)  );

			SG_ERR_CHECK(  _pt_update__create_entry(pCtx, pud, pszGid_k, pOD_k)  );
			goto NEXT;
		}

		if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__DELETED)
		{
			SG_treenode_entry_type tneType;

			// this item was deleted from goal changeset.

			// if we have a directory, we defer the rmdir until the next pass
			// so that we can do it depth-first and make sure that anything that
			// needs to be moved out of the directory is handled before we do the rmdir.

			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_tneType(pCtx, pOD_k, &tneType)  );
			if (tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
			{
				SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prbDirsToBeDeleted, pszGid_k, (void *)pOD_k)  );
				goto NEXT;
			}
			else
			{
				// if also DELETED from the dirty WD, the on-disk WD is up-to-date, but we need to actually
				// delete the ptnode rather than having a "deleted" ptnode.

				if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__DELETED)
				{
					SG_ERR_CHECK(  _pt_update__delete_entry(pCtx, pud, pszGid_k, pOD_k, _de_flags__also_deleted)  );
					goto NEXT;
				}

				// if LOST from the dirty WD, the on-disk WD is up-to-date, but we need to actually delete
				// the ptnode rather than having a "lost" ptnode.

				if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__LOST)
				{
					SG_ERR_CHECK(  _pt_update__delete_entry(pCtx, pud, pszGid_k, pOD_k, _de_flags__lost)  );
					goto NEXT;
				}

				// if the entry is unchanged in the dirty WD, we delete it from disk and update the pendingtree.

				if (dsFlags_baseline_vs_wd == SG_DIFFSTATUS_FLAGS__ZERO)
				{
					SG_ERR_CHECK(  _pt_update__delete_entry(pCtx, pud, pszGid_k, pOD_k, _de_flags__unmodified)  );
					goto NEXT;
				}

				// if we get here, we have a conflict that should have been caught in
				// __check_for_individual_conflicts() because one side DELETED it and
				// one side changed it somehow.

				SG_ERR_THROW(  (SG_ERR_ASSERT)  );
			}
		}

		// If the entry was LOST in the WD, then we silently override that.  That is,
		// we pretend like it wasn't lost, but rather completely unchanged from the
		// baseline.  This lets us do a normal update using the goal version.

		if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__LOST)
		{
			// Defer handling these until [7.3] so that all the other
			// renames/moves are finished so that the transient paths
			// more closely resemble the final result.
			//
			//SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prbRestoreLostEntries, pszGid_k, (void *)pOD_k)  );

			SG_ERR_CHECK(  _pt_update__restore_lost_entry(pCtx, pud, pszGid_k, pOD_k, dsFlags_baseline_vs_goal)  );

			goto NEXT;
		}

		// If the entry was not changed in the GOAL changeset relative to the baseline,
		// leave the entry as it is/was in the clean/dirty WD.

		if (dsFlags_baseline_vs_goal == SG_DIFFSTATUS_FLAGS__ZERO)
		{
			goto NEXT;
		}

		//////////////////////////////////////////////////////////////////
		// The entry was changed somehow in the GOAL changeset and exists
		// (not deleted nor lost) in the WD and may be clean or dirty.
		//////////////////////////////////////////////////////////////////
		// 
		// See if the entry was MOVED or RENAMED or MOVED+RENAME in the goal changeset
		// relative to the baseline.  we treat these together because both are just
		// variations on a rename() call under the hood.
		//
		// In all cases we ignore those operations that have been mirrored
		// in the dirty WD because the entry already has the new name/location.

		if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__MOVED)
			SG_ERR_CHECK(  _pt_update__move_entry_with_possible_rename(pCtx, pud, pszGid_k, pOD_k, SG_TRUE)  );
		else if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__RENAMED)
			SG_ERR_CHECK(  _pt_update__rename_entry(pCtx, pud, pszGid_k, pOD_k, SG_TRUE)  );

		// If the file was edited between the baseline and the goal changeset,
		// either UPDATE the contents of the entry using the goal version or
		// MERGE the contents with the dirty WD version.

		if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__MODIFIED)
		{
			SG_treenode_entry_type tneType;

			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_tneType(pCtx, pOD_k, &tneType)  );
			if (tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
			{
				// the contents of the directory changed between versions.
				// we don't consider it noise now.  we use this to update
				// the BLOB-HID on the directory ptnode so that subsequent
				// scan-dirs will evaluate the directory relative to the new
				// goal changeset.

				SG_ERR_CHECK(  _pt_update__update_dir_hid(pCtx, pud, pszGid_k, pOD_k)  );
			}
			else if (tneType == SG_TREENODEENTRY_TYPE_SYMLINK)
			{
				if (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED)
				{
					// symlink changed between baseline and goal (baseline==>goal)
					// dirty symlink in WD has identical change  (baseline==>wd)
					// only need to update ptnode
					SG_ERR_CHECK(  _pt_update__update_symlink(pCtx, pud, pszGid_k, pOD_k, SG_FALSE)  );
				}
				else if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__MODIFIED)
				{
					// symlink changed between baseline and goal (baseline==>goal)
					// dirty symlink in WD has different change  (baseline==>wd)
					// 
					// We cannot MERGE the contents of the SYMLINKs.  That just doesn't make sense.
					SG_ERR_THROW2(  SG_ERR_CANNOT_MERGE_SYMLINK_VALUES,
									(pCtx, "TODO message....."));
				}
				else
				{
					// symlink changed between baseline and goal (baseline==>goal)
					// symlink target has not been changed in the WD
					// fetch the version from the goal changeset and update the ptnode
					SG_ERR_CHECK(  _pt_update__update_symlink(pCtx, pud, pszGid_k, pOD_k, SG_TRUE)  );
				}
			}
			else if (tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
			{
				if (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED)
				{
					// file changed between baseline and goal (baseline==>goal)
					// dirty file in WD has identical change  (baseline==>wd)
					// only need to update ptnode
					SG_ERR_CHECK(  _pt_update__update_file(pCtx, pud, pszGid_k, pOD_k, SG_FALSE)  );
				}
				else if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__MODIFIED)
				{
					// file changed between baseline and goal (baseline==>goal)
					// dirty file in WD has different change  (baseline==>wd)
					// need to auto-merge the file contents or bail
					SG_ERR_CHECK(  _pt_update__merge_files(pCtx, pud, pszGid_k, pOD_k)  );
				}
				else
				{
					// file changed between baseline and goal (baseline==>goal)
					// file has not been edited in the WD
					// fetch the version from the goal changeset and update the ptnode
					SG_ERR_CHECK(  _pt_update__update_file(pCtx, pud, pszGid_k, pOD_k, SG_TRUE)  );
				}
			}
		}
		else	// not _MODIFIED in GOAL
		{
			// If the file content was dirty in the WD,
			// we just leave the existing file in the WD so we don't need to update
			// the timestamp cache.  (That is, assuming that any rename/move/attrs/xattrs
			// changes don't affect the mtime on the file.)
		}
		
		// If the attrbits changed between the baseline and the goal *and*
		// they didn't make the identical attrbit change in the dirty WD,
		// we need to update the attrbits on the final result.

		bNeedChangeAttrBits = (   (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)
							   && ((dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_ATTRBITS) == 0));
		if (bNeedChangeAttrBits)
			SG_ERR_CHECK(  _pt_update__change_attrbits_on_entry(pCtx, pud, pszGid_k, pOD_k)  );

		bNeedChangeXAttrs =   (   (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)
							   && ((dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_XATTRS) == 0));
		if (bNeedChangeXAttrs)
			SG_ERR_CHECK(  _pt_update__change_xattrs_on_entry(pCtx, pud, pszGid_k, pOD_k)  );

NEXT:
		SG_ERR_CHECK(  SG_treediff2__iterator__next(pCtx, pIter, &bOK, &pszGid_k, &pOD_k)  );
	}

fail:
	SG_TREEDIFF2_ITERATOR_NULLFREE(pCtx, pIter);
}

/**
 * Step 7.2: If there were any directories that were deleted in the
 *           goal changeset (and so should be deleted in our final
 *           result), delete them now.  We do this depth-first.
 */
static void _pt_update__do__full_7_2(SG_context * pCtx,
									 struct _pt_update_data * pud,
									 SG_rbtree * prbDirsToBeDeleted)
{
	SG_rbtree_iterator * pIter = NULL;
	SG_rbtree * prbQueueForDelete = NULL;		// map[pPath --> pOD]
	SG_pathname * pPath_Parent = NULL;
	SG_pathname * pPath_Allocated = NULL;
	const SG_pathname * pPath = NULL;
	const SG_pathname * pPathParked = NULL;		// we do not own this
	const char * pszGid = NULL;
	SG_treediff2_ObjectData * pOD = NULL;
	const char * pszGid_Parent = NULL;
	const char * pszEntryName = NULL;
	const char * pszKey_Path = NULL;
	struct sg_ptnode * ptnParent = NULL;
	struct _inv_entry * pInvEntry = NULL;
	SG_bool bFound;
	SG_uint32 nrToBeDeleted;
	SG_bool bOK;

	SG_ERR_CHECK(  SG_rbtree__count(pCtx, prbDirsToBeDeleted, &nrToBeDeleted)  );

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   ("_pt_update: begin [7.2] [count %d]\n"),
							   nrToBeDeleted)  );
#endif

	if (nrToBeDeleted == 0)
		return;

	// build a reverse queue based on the current transient pathname
	// so that we can remove the children first.  (We couldn't build
	// the reverse queue in 7.1 because there may still have been
	// renames/moves for parent directories of the deleted directories.)

	SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS2(pCtx,
											 &prbQueueForDelete,
											 128, NULL,
											 SG_rbtree__compare_function__reverse_strcmp)  );

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, prbDirsToBeDeleted, &bOK, &pszGid, (void **)&pOD)  );
	while (bOK)
	{
		SG_ERR_CHECK(  _pt_update__determine_final_parent_gid(pCtx, pOD, &pszGid_Parent, NULL)  );
		SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pud->pPendingTree, pszGid_Parent, &ptnParent)  );
		SG_ASSERT(  (ptnParent)  );
		SG_ERR_CHECK(  SG_inv_dirs__fetch_source_binding(pCtx, pud->pInvDirs, pszGid, &bFound, &pInvEntry, NULL)  );
		if (bFound)
			SG_ERR_CHECK(  SG_inv_entry__get_parked_path__ref(pCtx, pInvEntry, &pPathParked)  );
		if (pPathParked)
		{
			pPath = pPathParked;
		}
		else
		{
			SG_ERR_CHECK(  _pt_update__get_transient_path(pCtx, pud, ptnParent, &pPath_Parent)  );
			// TODO 2010/06/30 Should this be __get_old_name or __get_name ?
			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, pOD, &pszEntryName)  );
			SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_Allocated, pPath_Parent, pszEntryName)  );

			pPath = pPath_Allocated;
		}

#if TRACE_UPDATE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "Queueing Directory for Delete based on Pathname [%s][%s]\n",
								   SG_pathname__sz(pPath), pszGid)  );
#endif

		SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prbQueueForDelete, SG_pathname__sz(pPath), pOD)  );
		SG_PATHNAME_NULLFREE(pCtx, pPath_Parent);
		SG_PATHNAME_NULLFREE(pCtx, pPath_Allocated);

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter, &bOK, &pszGid, (void **)&pOD)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);

	// actually delete the directories deepest first and update the pendingtree.

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, prbQueueForDelete, &bOK, &pszKey_Path, (void **)&pOD)  );
	while (bOK)
	{
		SG_diffstatus_flags dsFlags_goal_vs_baseline;
		SG_diffstatus_flags dsFlags_baseline_vs_wd;
		SG_diffstatus_flags dsFlags_composite;
		enum _de_flags deFlags;

		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_gid(pCtx, pOD, &pszGid)  );
#if TRACE_UPDATE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "Using Queued Directory for Delete [%s]\n",
								   pszKey_Path)  );
#endif
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_composite_dsFlags_by_part(pCtx, pOD,
																			   &dsFlags_goal_vs_baseline,
																			   &dsFlags_baseline_vs_wd,
																			   &dsFlags_composite)  );
		if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__DELETED)
			deFlags = _de_flags__also_deleted;
		else if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__LOST)
			deFlags = _de_flags__lost;
		else	// should be __ZERO after stripping off bogus __MODIFIED bit.
			deFlags = _de_flags__unmodified;

		SG_ERR_CHECK(  _pt_update__delete_entry(pCtx, pud, pszGid, pOD, deFlags)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter, &bOK, &pszKey_Path, (void **)&pOD)  );
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
	SG_PATHNAME_NULLFREE(pCtx, pPath_Parent);
	SG_PATHNAME_NULLFREE(pCtx, pPath_Allocated);
	SG_RBTREE_NULLFREE(pCtx, prbQueueForDelete);
}

/**
 * _do__full_composite_update: step 7.
 *
 * [7] Actually perform the structural changes.  This will reshape the WD
 *     by applying the various RENAMED, MOVES, DELETES and ADDS from the
 *     goal changeset.
 *
 *     In theory, when this step is finished, the parking lot should be be empty.
 *
 */
static void _pt_update__do__full_7(SG_context * pCtx,
								   struct _pt_update_data * pud)
{
	SG_rbtree * prbDirsToBeDeleted = NULL;			// map[gid --> pOD] of directories that need to be deleted

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: begin [7]\n")  );
#endif

	SG_ASSERT(  (pud->bInDoFull)  );

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prbDirsToBeDeleted)  );

	// Step 7.1: Apply the simple changes that we can do.  Because we're
	//           running on GID order, we cannot necessarily delete
	//           directories because we may have stuff within them that
	//           needs to be moved out (or deleted) first.

	SG_ERR_CHECK(  _pt_update__do__full_7_1(pCtx, pud, prbDirsToBeDeleted)  );

	// Step 7.2: If there were directories to be deleted, delete
	//           them now depth-first.

	SG_ERR_CHECK(  _pt_update__do__full_7_2(pCtx, pud, prbDirsToBeDeleted)  );

fail:
	SG_RBTREE_NULLFREE(pCtx, prbDirsToBeDeleted);
}

//////////////////////////////////////////////////////////////////

/**
 * _do__full_composite_update: step 9.
 *
 * [9] All is done.  Clean up the mess.
 *
 *     If __DO_IT, we need to cleanup the pendingtree in memory and save it to disk
 *     so that it has the new baseline and the dirt we carried-forward is properly
 *     reflected.
 *
 *     If not __DO_IT (we did a __LOG_IT), discard the parts of pendingtree that we
 *     trashed.
 */
static void _pt_update__do__full_9(SG_context * pCtx,
								   struct _pt_update_data * pud)
{
#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: begin [9]\n")  );
#endif

	SG_ASSERT(  (pud->bInDoFull)  );

	if (pud->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
	{
		SG_bool bDirty;

		SG_ERR_CHECK(  SG_strcpy(pCtx,
								 pud->bufHid_baseline, sizeof(pud->bufHid_baseline),
								 pud->bufHid_goalcset)  );

		SG_ERR_CHECK(  SG_pendingtree__set_single_wd_parent(pCtx, pud->pPendingTree, pud->bufHid_baseline)  );

		/* we need to undo all the IMPLICIT entries now.  if we don't,
		 * this revert function will accidentally perform an addremove
		 * function.
		 *
		 * TODO 4/7/10 Confirm the need to do the __undo_implicit().  This
		 * TODO 4/7/10 call and the above comment came from the original
		 * TODO 4/7/10 version of switch-baseline.
		 *
		 * TODO 4/7/10 Review the sequence of steps here to filter-out the
		 * TODO 4/7/10 clean nodes, free the prbItems and doing the save.
		 * TODO 4/7/10 This sequence of steps occur in several different
		 * TODO 4/7/10 places in sg_pendingtree.c
		 *
		 * We DO NOT call SG_pendingtree__scan() here because it implicitly
		 * includes an add-remove and we don't want that.
		 */

		SG_ERR_CHECK(  sg_ptnode__undo_implicit(pCtx, pud->pPendingTree->pSuperRoot)  );
		SG_ERR_CHECK(  sg_ptnode__unload_dir_entries_if_not_dirty(pCtx, pud->pPendingTree->pSuperRoot, &bDirty)  );

		if (!(pud->pPendingTree->pSuperRoot->prbItems))
			SG_PTNODE_NULLFREE(pCtx, pud->pPendingTree->pSuperRoot);

		SG_ERR_CHECK(  SG_pendingtree__save(pCtx, pud->pPendingTree)  );

#if TRACE_UPDATE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: end [9] actually saved pendingtree containing:\n")  );
		SG_ERR_IGNORE(  SG_pendingtree_debug__dump_existing_to_console(pCtx, pud->pPendingTree)  );
#endif

	}
	else
	{
		/* we don't change the saved pending changeset here. */

		SG_PTNODE_NULLFREE(pCtx, pud->pPendingTree->pSuperRoot);
	}

fail:
	return;
}

/**
 * The contents of the baseline and the goal changeset are different.
 *
 * We may or may not have dirt in the WD; if it is there, we want to try
 * to preserve it in the final result.
 *
 * The pendingtree in memory and the WD on disk are in sync.
 */
static void _pt_update__do__full_composite_update(SG_context * pCtx,
												   struct _pt_update_data * pud)
{
	SG_ASSERT(  (strcmp(pud->bufHid_goalcset, pud->bufHid_baseline) != 0)  );
	SG_ASSERT(  (pud->nrChanges_goalcset_vs_baseline > 0)  );
	SG_ASSERT(  ((!pud->bForce) || (pud->nrChanges_baseline_vs_wd == 0))  );

	pud->bInDoFull = SG_TRUE;		// save some later asserts

	// prepare some temp space to temporarily park stuff as we rearrange the tree.
	SG_ERR_CHECK(  SG_INV_DIRS__ALLOC(pCtx, pud->pPendingTree->pPathWorkingDirectoryTop, "update", &pud->pInvDirs)  );
	// defer allocation of ud.pVecCycles until we need it.

	//////////////////////////////////////////////////////////////////
	// Here now we begin in earnest trying to see if we can do the UPDATE.
	// The steps here are modeled on the REVERT code, but with differences
	// because of the greater complexity.
	//////////////////////////////////////////////////////////////////

	SG_ERR_CHECK(  _pt_update__do__full_1(pCtx, pud)  );
	SG_ERR_CHECK(  _pt_update__do__full_2(pCtx, pud)  );
	SG_ERR_CHECK(  _pt_update__do__full_3(pCtx, pud)  );
	SG_ERR_CHECK(  _pt_update__do__full_4(pCtx, pud)  );

	//////////////////////////////////////////////////////////////////
	// at this point we start making changes to the WD (or just
	// computing the log when --test given).
	//////////////////////////////////////////////////////////////////

#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "_pt_update: before [5] preparing to start changing pendingtree:\n")  );
	SG_ERR_IGNORE(  SG_pendingtree_debug__dump_existing_to_console(pCtx, pud->pPendingTree)  );
#endif

	SG_ERR_CHECK(  _pt_update__do__full_5(pCtx, pud)  );
	SG_ERR_CHECK(  _pt_update__do__full_6(pCtx, pud)  );
	SG_ERR_CHECK(  _pt_update__do__full_7(pCtx, pud)  );
	SG_ERR_CHECK(  _pt_update__do__full_9(pCtx, pud)  );

	SG_ERR_CHECK(  SG_inv_dirs__delete_parking_lot(pCtx,pud->pInvDirs)  );
	return;

fail:
	if (pud->pInvDirs)
	{
		// TODO 2010/06/30 Abandon the parking lot and leave the litter on the disk.
		// TODO            For now this is safer than deleting the only copy of something
		// TODO            on an otherwise unrelated hard error.  Revisit this later.
		SG_inv_dirs__abandon_parking_lot(pCtx, pud->pInvDirs);
	}
}

//////////////////////////////////////////////////////////////////

static void _pt_update__compute_composite_treediff(SG_context * pCtx,
												   struct _pt_update_data * pud)
{
	//////////////////////////////////////////////////////////////////
	// Normally, we think of UPDATE as being:
	//
	//      <baseline> -.-.-.-> <wd>
	//          |
	//          |
	//          |
	//          V
	//        <goal>
	//
	// ==========================
	// Figure 1.
	//
	// where the goal is a descendant of the baseline and the dirt
	// in the WD is another (micro) branch.  And we want to try
	// to carry-forward the dirt and produce:
	//
	//      <baseline>
	//          |
	//          |
	//          V
	//        <goal> -.-.-.-> <wd>
	//
	// ==========================
	// Figure 2.
	//
	// We allow the goal to be an ancestor of the baseline and try
	// to carry the dirt backwards.
	//
	//////////////////////////////////////////////////////////////////
	// Compute a COMPOSITE (Arbitrary CSET vs WD) TREEDIFF using the
	// GOAL as the arbitrary cset.  The current BASELINE is implied
	// between them.
	//
	//     <goal>
	//       |
	//       |
	//       V
	//   <baseline>
	//       .
	//       .
	//       V
	//      <wd>
	//
	// ==========================
	// Figure 3.
	//
	// This may look backwards, and it is.  This shows the changes
	// in the WD *relative to* the goal.  We can use this to weed
	// out some conflicts/complications that we can't/shouldn't
	// handle.  (The thought being that if the WD has a *lot* of
	// dirt, you should do a COMMIT and then a MERGE.)
	//
	// We DO NOT strip out the clean ptnodes that we see during
	// the __scan_dir() within the __do_diff() because we need
	// the complete ptnode tree in memory so that the changes made
	// by the goal changeset can be connected to the tree (and we
	// can reliably do a __find_by_gid() in step 7).

	SG_ERR_CHECK(  SG_pendingtree__do_diff__keep_clean_objects(pCtx,
																pud->pPendingTree,
																pud->bufHid_goalcset,
																NULL,
																&pud->pTreeDiff_composite)  );

	// We DO NOT use the __file_spec_filter stuff on this treediff;
	// we want the complete diff.

	SG_ERR_CHECK(  SG_treediff2__count_composite_changes_by_part(pCtx,
																 pud->pTreeDiff_composite,
																 &pud->nrChanges_total,
																 &pud->nrChanges_goalcset_vs_baseline,
																 &pud->nrChanges_baseline_vs_wd,
																 &pud->nrChanges_composite)  );
#if TRACE_UPDATE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   "_pt_update: nrChanges [total %d][goal-vs-baseline %d][baseline-vs-wd %d][composite %d][wd %s]\n",
							   pud->nrChanges_total,
							   pud->nrChanges_goalcset_vs_baseline,
							   pud->nrChanges_baseline_vs_wd,
							   pud->nrChanges_composite,
							   SG_pathname__sz(pud->pPendingTree->pPathWorkingDirectoryTop))  );
	if (pud->nrChanges_total > 0)
	{
		SG_ERR_IGNORE(  SG_treediff2__report_status_to_console(pCtx,pud->pTreeDiff_composite,SG_FALSE)  );
		SG_ERR_IGNORE(  SG_treediff2_debug__dump_to_console(pCtx,pud->pTreeDiff_composite,SG_FALSE)  );
	}
#endif

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * Look for divergent changes on an individual entry.  See Figure 1.
 * WD and GOAL are in essense (micro) branches off of the baseline.
 * Since both leaves can independently change entries, we need to
 * check for trivial conflicts.
 *
 * For example, if a file is renamed in both the WD and in the GOAL
 * (relative to the common baseline), and they were given 2 different
 * new names, we don't know what to do.  So we quit and advise them
 * to COMMIT the local WD dirt and then MERGE.  So that the MERGE
 * code can properly handle the conflict.
 */
static void _pt_update__check_for_individual_conflicts(SG_context * pCtx,
													   struct _pt_update_data * pud)
{
	SG_treediff2_iterator * pIter = NULL;
	const char * pszGid_k;
	SG_treediff2_ObjectData * pOD_k;
	SG_bool bOK;

	SG_ERR_CHECK(  SG_treediff2__iterator__first(pCtx, pud->pTreeDiff_composite, &bOK, &pszGid_k, &pOD_k, &pIter)  );
	while (bOK)
	{
		SG_diffstatus_flags dsFlags_goal_vs_baseline;
		SG_diffstatus_flags dsFlags_baseline_vs_wd;
		SG_diffstatus_flags dsFlags_composite;
		SG_diffstatus_flags dsFlags_baseline_vs_goal;
		const char * pszRepoPath = NULL;

		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_composite_dsFlags_by_part(pCtx, pOD_k,
																			   &dsFlags_goal_vs_baseline,
																			   &dsFlags_baseline_vs_wd,
																			   &dsFlags_composite)  );

		// Since we built the composite treediff using Figure 3 rather
		// than Figure 1, we need to swap the direction of the goal-vs-baseline
		// flags so that we can (sanely) work with baseline on the left of both.
		//
		// WARNING: some of the tests below are going to look somewhat inside-out
		// WARNING: because of this.

		SG_ERR_CHECK(  SG_treediff2__Invert_dsFlags(pCtx, dsFlags_goal_vs_baseline, &dsFlags_baseline_vs_goal)  );

#if TRACE_UPDATE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "_pt_update: check_for_individual_conflicts: [gid %s] [g-vs-b %x][b-vs-wd %x][comp %x] [b-vs-g %x]\n",
								   pszGid_k,
								   dsFlags_goal_vs_baseline,
								   dsFlags_baseline_vs_wd,
								   dsFlags_composite,
								   dsFlags_baseline_vs_goal)  );
#endif

		// look for specific conflicts.  See what was done to the entry in the
		// WD/pendingtree and see if that poses an issue for what was done in
		// between the baseline and the goal changeset.

		if (dsFlags_baseline_vs_wd == SG_DIFFSTATUS_FLAGS__ZERO)
			goto NEXT;

		// WD changes of type: __ADDED, __FOUND, __DELETED, and __LOST are somewhat
		// absolute -- for example, you can't do an ADD + RENAME in the WD.  and
		// creating/deleting something doesn't mesh well with a move/rename/xattr
		// on the other side.  so we quickly give up when both sides are dirty and
		// don't worry about the __MUTUALLY_EXCLUSIVE mask here.

		if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__ADDED)
		{
			// added to pendingtree, so it should not exist in goal.
			SG_ASSERT(  (dsFlags_baseline_vs_goal == SG_DIFFSTATUS_FLAGS__ZERO)  );
			goto NEXT;
		}

		if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__FOUND)
		{
			// entry was found in WD but is not under VC (and
			// we synthesized a temp GID for it to get it into the
			// treediff), so it cannot exist in the baseline or goal.
			// there may be an entry with the same name in the same
			// directory in the goal, but we can't tell that here.
			SG_ASSERT(  (dsFlags_baseline_vs_goal == SG_DIFFSTATUS_FLAGS__ZERO)  );
			goto NEXT;
		}

		if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__DELETED)
		{
			if (dsFlags_baseline_vs_goal == SG_DIFFSTATUS_FLAGS__ZERO)		// delete vs unchanged, allow
				goto NEXT;

			if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__DELETED)	// both deleted, allow
			{
				// from a micro bracnh point of view we have:
				//     baseline-->DEL-->goal
				//  +  baseline-->DEL-->wd
				// from a composite point of view, this would look like:
				//     goal-->ADD-->baseline-->DEL-->wd
				// which we define to be a
				//     goal-->UNDO_ADD-->wd
				SG_ASSERT(  (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_ADDED)  );
				goto NEXT;
			}

			SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_k,&pszRepoPath)  );
			SG_ERR_THROW2(  SG_ERR_UPDATE_CONFLICT,
							(pCtx, "'%s' was deleted from the working directory but modified in the target changeset.",
							 ((pszRepoPath) ? pszRepoPath : "(null)"))  );
		}

		if (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__LOST)
		{
			// entry was lost from the WD.  i'm guessing here that the UPDATE
			// should silently supply the version of the entry in the goal changeset.
			//
			// TODO think about this.  See also __is_entry_present_in_final_result().
			goto NEXT;
		}

		//////////////////////////////////////////////////////////////////

		// we already know that it changed in the WD somehow, so it cannot have
		// been just ADDED to the goal changeset (relative to the baseline).
		SG_ASSERT(  ((dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__ADDED) == 0)  );

		if (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__DELETED)
		{
			// the only possible value for the MASK_MUTUALLY_EXCUSIVE bit in the
			// baseline-vs-wd is:
			//
			// __MODIFIED with/without one/more of the MASK_MODIFIERS bits.
			//
			// --OR--
			//
			// it could have __ZERO WITH AT LEAST ONE OF THE MASK_MODIFIER bits.
			//
			// Ignore __MODIFIED bit on directories because they are just noise
			// in this context.

			SG_diffstatus_flags ds;
			SG_treenode_entry_type tneType;
			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_tneType(pCtx,pOD_k,&tneType)  );

			ds = dsFlags_baseline_vs_wd;
			if (tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
				ds &= ~SG_DIFFSTATUS_FLAGS__MODIFIED;

			if (ds != SG_DIFFSTATUS_FLAGS__ZERO)
			{
				SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_k,&pszRepoPath)  );
				SG_ERR_THROW2(  SG_ERR_UPDATE_CONFLICT,
								(pCtx, "'%s' was deleted from the goal changeset but modified in the working directory.",
								 ((pszRepoPath) ? pszRepoPath : "(null)"))  );
			}
		}

		//////////////////////////////////////////////////////////////////

		// the less absolute changes require different tactics.  for example,
		// one side could edit a file and the other side could rename it and
		// we'd allow that.  but if both sides renamed it differently, we'd
		// complain.

		if ( (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__MODIFIED) && (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__MODIFIED) )
		{
			// both sides modified/edited the entry.  because of the way the
			// composite was computed, we already know if they both changed
			// it to the same value.
			//
			// from a micro branch point of view we have:
			//     baseline-->MOD-->goal
			// +   baseline-->MOD-->wd
			// from a composite point of view, this would look like:
			//     goal-->InvMOD-->baseline-->MOD-->wd
			// if both MODs are the same, InvMOD and MOD cancel each other out and we have:
			//     goal-->UNDO_MOD-->wd

			if (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED)
			{
			}
			else
			{
				SG_treenode_entry_type tneType;

				SG_ERR_CHECK(  SG_treediff2__ObjectData__get_tneType(pCtx,pOD_k,&tneType)  );
				switch (tneType)
				{
				default:
					SG_ASSERT(  (0)  );

				case SG_TREENODEENTRY_TYPE_DIRECTORY:		// MOD bits on directories are just noise
					break;

				case SG_TREENODEENTRY_TYPE_SYMLINK:			// divergent symlink changes
					// TODO do we want to allow this to be OK and do
					// TODO the .yours / .mine trick in the directory
					// TODO rather than just giving up?
					SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_k,&pszRepoPath)  );
					SG_ERR_THROW2(  SG_ERR_UPDATE_CONFLICT,
									(pCtx, "Symbolic link '%s' was changed to a different values in both the working directory and the target changeset.",
									 ((pszRepoPath) ? pszRepoPath : "(null)"))  );

				case SG_TREENODEENTRY_TYPE_REGULAR_FILE:
					// TODO I assume that we will run the diff/merge code and try to
					// TODO automerge these versions and/or splat the .yours / .mine
					// TODO files in the directory (with the baseline being the ancestor).
					// TODO
					// TODO the diff/merge would not happen here, but for now, just bail.
					SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_k,&pszRepoPath)  );
					SG_ERR_THROW2(  SG_ERR_UPDATE_CONFLICT,
									(pCtx, "TODO Need file-auto-merge.  The file '%s' was changed in both the working directory and the target changeset.",
									 ((pszRepoPath) ? pszRepoPath : "(null)"))  );
				}
			}
		}

		if ( (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__RENAMED) && (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__RENAMED) )
		{
			if (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_RENAMED)
			{
			}
			else
			{
				SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_k,&pszRepoPath)  );
				SG_ERR_THROW2(  SG_ERR_UPDATE_CONFLICT,
								(pCtx, "'%s' was renamed to different values in both the working directory and the goal changeset.",
								 ((pszRepoPath) ? pszRepoPath : "(null)"))  );
			}
		}

		if ( (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__MOVED) && (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__MOVED) )
		{
			if (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_MOVED)
			{
			}
			else
			{
				SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_k,&pszRepoPath)  );
				SG_ERR_THROW2(  SG_ERR_UPDATE_CONFLICT,
								(pCtx, "'%s' was moved to different values in both the working directory and the goal changeset.",
								 ((pszRepoPath) ? pszRepoPath : "(null)"))  );
			}
		}

		if ( (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS) && (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS) )
		{
			if (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_XATTRS)
			{
			}
			else
			{
				SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_k,&pszRepoPath)  );
				SG_ERR_THROW2(  SG_ERR_UPDATE_CONFLICT,
								(pCtx, "XAttrs on '%s' were changed to different values in both the working directory and the goal changeset.",
								 ((pszRepoPath) ? pszRepoPath : "(null)"))  );
			}
		}

		if ( (dsFlags_baseline_vs_wd & SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS) && (dsFlags_baseline_vs_goal & SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS) )
		{
			if (dsFlags_composite & SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_ATTRBITS)
			{
			}
			else
			{
				SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_k,&pszRepoPath)  );
				SG_ERR_THROW2(  SG_ERR_UPDATE_CONFLICT,
								(pCtx, "AttrBits on '%s' were changed to different values in both the working directory and the goal changeset.",
								 ((pszRepoPath) ? pszRepoPath : "(null)"))  );
			}
		}

		// that takes care of the simple conflicts, but it won't deal with
		// hard stuff (like entryname collisions (2 different entries fighting
		// for the same entryname) or structural conflicts (moving dir_a into
		// dir_b on one side and moving dir_b into dir_a on the other)).

NEXT:
		SG_ERR_CHECK(  SG_treediff2__iterator__next(pCtx, pIter, &bOK, &pszGid_k, &pOD_k)  );
	}

fail:
	SG_TREEDIFF2_ITERATOR_NULLFREE(pCtx, pIter);
}

//////////////////////////////////////////////////////////////////

/**
 * Update the WD to match the goal changeset (optionally keeping or
 * deleting any dirt in the WD).
 */
static void _pt_update__using_goal(SG_context* pCtx,
								   struct _pt_update_data * pud)
{
	if (strcmp(pud->bufHid_goalcset, pud->bufHid_baseline) == 0)
	{
		// handle trivial UPDATE where goal hid is same as baseline
		SG_ERR_CHECK_RETURN(  _pt_update__do__goal_hid_equals_baseline_hid(pCtx, pud)  );
		return;
	}

	// make composite treediff "goal-->baseline-->wd" and extract component parts
	SG_ERR_CHECK_RETURN(  _pt_update__compute_composite_treediff(pCtx, pud)  );

	if (pud->nrChanges_goalcset_vs_baseline == 0)
	{
		// handle trivial UPDATE where the contents of goal same as baseline
		SG_ERR_CHECK_RETURN(  _pt_update__do__goal_content_identical_to_baseline(pCtx, pud)  );
		return;
	}

	//////////////////////////////////////////////////////////////////
	// There are changed between the baseline and the goal changeset.
	//////////////////////////////////////////////////////////////////

	// if there is dirt in the WD and --force, then we want to effectively
	// do a "REVERT --ALL" and before we begin the switch.
	if (pud->bForce && (pud->nrChanges_baseline_vs_wd > 0))
	{
		// TODO this step is *hard*.  if we want to really support the --test
		// TODO option, we need to be able to compute everything that needs
		// TODO be done without actually doing it.  The regular REVERT code
		// TODO starts with the current pendingtree and the WD on disk and
		// TODO works backward to the baseline.  It can reference both to
		// TODO fill in gaps in its knowledge (like ......
		SG_ERR_THROW2_RETURN(  SG_ERR_NOTIMPLEMENTED,
							   (pCtx, "TODO Support 'UPDATE --force' when WD is dirty and the BASELINE and GOAL are different.")  );
	}
	else
	{
		//////////////////////////////////////////////////////////////////
		// The WD is either clean or we want to preserve the dirt.
		// In either case, the pendingtree is in sync with the WD on disk.
		//////////////////////////////////////////////////////////////////

		// if there were entries that were changed (relative to the baseline) in both
		// the goal and the WD, see if there were any with simple conflicts and throw.
		if (pud->nrChanges_composite > 0)
			SG_ERR_CHECK_RETURN(  _pt_update__check_for_individual_conflicts(pCtx, pud)  );

		// if we are preserving dirt in the WD, we require the goal changeset to be a
		// descendant or ancestor of the baseline (so that merges are well defined).
		if (pud->nrChanges_baseline_vs_wd > 0)
			SG_ERR_CHECK_RETURN(  _pt_update__verify_changeset_relationship(pCtx, pud)  );

		SG_ERR_CHECK_RETURN(  _pt_update__do__full_composite_update(pCtx, pud)  );
	}
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__update_baseline(SG_context* pCtx,
									 SG_pendingtree* pPendingTree,
									 const char* pszHid_goalcset,
									 SG_bool bForce,
									 SG_pendingtree_action_log_enum eActionLog)
{
	struct _pt_update_data ud;
	const SG_varray * pva_wd_parents = NULL;		// we do not own this
	const char * psz_hid_baseline = NULL;			// we do not own this
	SG_uint32 nrParents = 0;

#if 1
	if (bForce)
	{
		SG_ERR_THROW2_RETURN(  SG_ERR_NOTIMPLEMENTED,
							   (pCtx, "TODO decide what --force or --clean should do.  See issue SPRAWL-539.")  );
	}
#endif

	SG_NULLARGCHECK_RETURN(pPendingTree);
	// pszHid may be null or blank -- if so, we lookup the head/tip.

	memset(&ud,0,sizeof(ud));

	ud.pPendingTree = pPendingTree;
	ud.bForce = bForce;

	SG_ARGCHECK_RETURN(  (eActionLog != SG_PT_ACTION__ZERO), eActionLog  );
	// eActionLog indicates whether we should actually do the work, just log what
	// should be done, or both.  We set it here for the body of the UPDATE.  We
	// don't really anticipate it being changed/set differently by a series of
	// commands.  That is, the log should either be a complete list of what SHOULD
	// be done -OR- a complete list of what we DID do, but not a mingling of the 2.
	// since we currently can only do 1 operation with a pendingtree, this test is
	// not that important; but later it may be.
	SG_ASSERT(  (ud.pPendingTree->eActionLog == SG_PT_ACTION__ZERO) || (ud.pPendingTree->eActionLog == eActionLog)  );
	ud.pPendingTree->eActionLog = eActionLog;

	// defer allocation of ud.pInvDirs until we need it.
	// defer allocation of ud.pVecCycles until we need it.

	// get the VArray of the parent CSet(s) of the WD.  we require that we have
	// exactly one parent.  Otherwise, we have an uncomitted MERGE that needs to
	// be dealt with before we can do an UPDATE.  Stuff parent[0] in our structure
	// and call it the "baseline".

	SG_ERR_CHECK_RETURN(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pva_wd_parents)  );
	SG_ERR_CHECK_RETURN(  SG_varray__count(pCtx, pva_wd_parents, &nrParents)  );
	SG_ASSERT(  (nrParents >= 1)  );
	if (nrParents > 1)
		SG_ERR_THROW2_RETURN(  SG_ERR_CANNOT_DO_WHILE_UNCOMMITTED_MERGE,
							   (pCtx, "Cannot UPDATE with uncommitted MERGE.")  );
	SG_ERR_CHECK_RETURN(  SG_varray__get__sz(pCtx, pva_wd_parents, 0, &psz_hid_baseline)  );
	SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx, ud.bufHid_baseline, sizeof(ud.bufHid_baseline), psz_hid_baseline)  );

	// if we were given a goal cset, use it.  otherwise, find the "best" head/tip
	// relative to the current baseline.

	if (pszHid_goalcset && *pszHid_goalcset)
	{
		SG_ERR_CHECK(  SG_strcpy(pCtx, ud.bufHid_goalcset, sizeof(ud.bufHid_goalcset), pszHid_goalcset)  );
		ud.dqRel = SG_DAGQUERY_RELATIONSHIP__UNKNOWN;
		SG_ERR_CHECK(  _pt_update__using_goal(pCtx, &ud)  );
	}
	else
	{
		// Find the appropriate head/tip relative to the current baseline.

		SG_dagquery_find_head_status dqfhs;

		SG_ERR_CHECK(  SG_dagquery__find_single_descendant_head(pCtx, ud.pPendingTree->pRepo,
																ud.bufHid_baseline,
																&dqfhs,
																ud.bufHid_goalcset, sizeof(ud.bufHid_goalcset))  );
		switch (dqfhs)
		{
		case SG_DAGQUERY_FIND_HEAD_STATUS__IS_LEAF:
			ud.dqRel = SG_DAGQUERY_RELATIONSHIP__SAME;
			SG_ERR_CHECK(  _pt_update__using_goal(pCtx, &ud)  );
			break;

		case SG_DAGQUERY_FIND_HEAD_STATUS__UNIQUE:
			ud.dqRel = SG_DAGQUERY_RELATIONSHIP__DESCENDANT;
			SG_ERR_CHECK(  _pt_update__using_goal(pCtx, &ud)  );
			break;

		case SG_DAGQUERY_FIND_HEAD_STATUS__MULTIPLE:
			SG_ERR_THROW2(  SG_ERR_MULTIPLE_HEADS_FROM_DAGNODE,
							(pCtx, "Cannot automatically assume goal changeset")  );
			break;

		default:
			SG_ASSERT(  (0)  );
			break;
		}
	}

fail:
	SG_TREEDIFF2_NULLFREE(pCtx, ud.pTreeDiff_composite);
	SG_INV_DIRS_NULLFREE(pCtx, ud.pInvDirs);
	SG_VECTOR_NULLFREE(pCtx, ud.pVecCycles);
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_PENDINGTREE__PRIVATE_UPDATE_BASELINE_H
