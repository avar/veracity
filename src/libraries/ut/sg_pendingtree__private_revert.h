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
 * @file sg_pendingtree__private_revert.h
 *
 * @details
 *
 */

// TODO investigate all of the _CHECKMYNAME flags that we set here.

//////////////////////////////////////////////////////////////////

#ifndef H_SG_PENDINGTREE__PRIVATE_REVERT_H
#define H_SG_PENDINGTREE__PRIVATE_REVERT_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

struct _pt_revert_data
{
	SG_pendingtree *		pPendingTree;		// we do not own this.
	SG_treediff2 *			pTreeDiff;			// we own this.   (treediff between baseline and WD)

	SG_inv_dirs *			pInvDirs;			// we own this.
	SG_vector *				pVecCycles;			// vec[struct sg_ptnode *] of moved dirs marked PARKED_FOR_CYCLE    we do not own these.

	enum
	{
		_PT_REVERT_PASS__SCAN_AND_BIND,
		_PT_REVERT_PASS__CHECK_FOR_CONFLICTS,
		_PT_REVERT_PASS__APPLY_STRUCTURAL_CHANGES,
	}						pass;
};

//////////////////////////////////////////////////////////////////

static void _pt_revert__revert_attrbits(SG_context * pCtx,
										SG_UNUSED_PARAM( struct _pt_revert_data * prd ),
										const SG_pathname * pPathAbsolute,
										struct sg_ptnode * ptn,
										const char * pszReason);

static void _pt_revert__revert_xattrs(SG_context * pCtx,
									  struct _pt_revert_data * prd,
									  const SG_pathname * pPathAbsolute,
									  struct sg_ptnode * ptn,
									  const char * pszReason);

//////////////////////////////////////////////////////////////////

static void _pt_revert__bind_right_side_of_diff_to_source(SG_context * pCtx,
									struct _pt_revert_data * prd,
									const char * pszGidObject,
									const SG_treediff2_ObjectData * pOD,
									SG_bool bActive)
{
	const char * pszGidObject_CurrentParent;
	const SG_treediff2_ObjectData * pOD_CurrentParent;
	const char * pszRepoPath_CurrentParent_CurrentPath;
	const char * pszEntryName_Current;
	SG_treenode_entry_type tneType;
	SG_bool bFound;

	// REVERT is modeled on making a diff(baseline,wd) and then UNDOing {all, some} of the changes.
	//        So the "baseline" is on the left and the "wd" is on the right.
	//        So in this context, the CURRENT means the current WD and Ndx_Net in the treediff.
	//        The given entry is present in the WD in the current parent.
	//
	// The entry is present in the "current" part of the TreeDiff/ObjectData.
	// This is the right side of the diff and reflects the current state in
	// the working directory.
	//
	// Get the "current" values and set the "source" fields in _inv_dir/_inv_entry.

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_name(pCtx,pOD,&pszEntryName_Current)  );

	// Get gid-object of the CURRENT parent directory and the repo-path of that directory
	// *as they currently exist* in the CURRENT version of the tree and on disk.

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_parent_gid(pCtx,pOD,&pszGidObject_CurrentParent)  );
	SG_ERR_CHECK(  SG_treediff2__get_ObjectData(pCtx,prd->pTreeDiff,pszGidObject_CurrentParent,&bFound,&pOD_CurrentParent)  );
	SG_ASSERT(  (bFound)  );	// TODO think about reverting the initial commit which would un-add "@/"....
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_CurrentParent,&pszRepoPath_CurrentParent_CurrentPath)  );

	SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_tneType(pCtx, pOD, &tneType)  );

#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_pt_revert__bind_right_side_of_diff_to_source: [gid %s] claming source [active %d][%s][%s] [tneType %d]\n",
							   pszGidObject,
							   bActive,
							   pszRepoPath_CurrentParent_CurrentPath,
							   pszEntryName_Current,
							   tneType)  );
#endif

	SG_ERR_CHECK(  SG_inv_dirs__bind_source(pCtx,prd->pInvDirs,
											pszGidObject_CurrentParent,pszRepoPath_CurrentParent_CurrentPath,
											pszGidObject,pszEntryName_Current,
											(tneType == SG_TREENODEENTRY_TYPE_DIRECTORY),
											(void *)pOD,
											bActive,
											NULL,NULL)  );

fail:
	return;
}

static void _pt_revert__bind_left_side_of_diff_to_target(SG_context * pCtx,
									struct _pt_revert_data * prd,
									const char * pszGidObject,
									const SG_treediff2_ObjectData * pOD,
									SG_bool bActive)
{
	const char * pszGidObject_OriginalParent;
	const SG_treediff2_ObjectData * pOD_OriginalParent;
	const char * pszRepoPath_OriginalParent_CurrentPath;
	const char * pszEntryName_Original;
	SG_bool bFound;

	// REVERT is modeled on making a diff(baseline,wd) and then UNDOing {all, some} of the changes.
	//        So the "baseline" is on the left and the "wd" is on the right.
	//        So in this context, the ORIGINAL means the baseline and Ndx_Orig in the treediff.
	//        The given entry needs to exist after everything is done, but it may or may not
	//        exist in the current WD and it may or may not be in the same directory and the
	//        final directory may or may not exist in the current WD and may or may not have
	//        the same pathname in the result.
	//
	// The entry is present in the "original" part of the TreeDiff/ObjectData.
	// This is the left side of the diff and reflects the original/baseline
	// state.
	//
	// Get the "original" values and set the "target" fields in _inv_dir/_inv_entry.

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx,pOD,&pszEntryName_Original)  );

	// Get gid-object of the ORIGINAL parent directory and the CURRENT repo-path of that directory
	// *as they exist* in the current version of the tree and on disk.  (this is necessary so
	// that we can find the directory on disk when one or more of its parents were renamed/moved.)

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_moved_from_gid(pCtx,pOD,&pszGidObject_OriginalParent)  );
	SG_ERR_CHECK(  SG_treediff2__get_ObjectData(pCtx,prd->pTreeDiff,pszGidObject_OriginalParent,&bFound,&pOD_OriginalParent)  );
	SG_ASSERT(  (bFound)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_OriginalParent,&pszRepoPath_OriginalParent_CurrentPath)  );

#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_pt_revert__bind_left_side_of_diff_to_target: [gid %s] claming target [active %d][%s][%s]\n",
							   pszGidObject,
							   bActive,
							   pszRepoPath_OriginalParent_CurrentPath,
							   pszEntryName_Original)  );
#endif

	SG_ERR_CHECK(  SG_inv_dirs__bind_target(pCtx,prd->pInvDirs,
											pszGidObject_OriginalParent,
											pszGidObject,pszEntryName_Original,(void *)pOD,
											bActive,
											NULL,NULL)  );

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * Since we parked entries that have collisions in the swap directory,
 * we cannot use the normal sg_ptnode__get_repo_path() and the
 * repo_path --> absolute_path code.  rather, we need something that
 * computes the "transient absolute path".  for example, if we have:
 *     @/dir1/dir2/dir3/dir4/dir5/dir6/file1
 *     @/dirA/dirB/fileA
 * and the user does:
 *     vv rm @/dir1/dir2/dir3/dir4/dir5
 *     vv mv @/dir1/dir2/dir3 @/dir1/dir2/xxx
 *     vv mv @/dirA @/dir1/dir2/dir3
 * then we have an collision/order problem on the name @/dir3.
 * so we move dir3 --> <swap> as we juggle everything and revert
 * the changes.  When we try to un-delete dir5 (and everything
 * within it) (and depending on the random gids involved), dir3
 * may or may not have been handled:
 * [1] it could refer to dirA, if we got to dir5 before dirA
 * [2] it could be missing, if dirA has already been restored
 * [3] it could refer to the original dir3, if it has already been
 *     restored.
 *
 * So we need to be able to say the "transient absolute pathname"
 * for dir5 is <swap>/dir4/dir5 in cases [1] and [2] and
 * <wd>/dir1/dir2/dir3/dir4/dir5 in case [3].
 */
static void _pt_revert__get_transient_path(SG_context * pCtx,
										   struct _pt_revert_data * prd,
										   struct sg_ptnode * ptn,
										   SG_pathname ** ppPath)
{
	SG_pathname * pPath_Allocated = NULL;
	SG_inv_entry * pInvEntry = NULL;
	const char * pszEntryName = NULL;
	const SG_pathname * pPathParked = NULL;		// we do not own this
	SG_bool bFound, bExists;

	if (!ptn->pszgid || !ptn->pszgid[0])
	{
		// ptn is the super-root
		SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__COPY(pCtx,ppPath,prd->pPendingTree->pPathWorkingDirectoryTop)  );
		return;
	}

	SG_ERR_CHECK_RETURN(  SG_inv_dirs__fetch_source_binding(pCtx,prd->pInvDirs,ptn->pszgid,&bFound,&pInvEntry,NULL)  );
	if (bFound)
		SG_ERR_CHECK(  SG_inv_entry__get_parked_path__ref(pCtx, pInvEntry, &pPathParked)  );
	if (pPathParked)
	{
		// if the swap path still exists on disk, then we haven't restored it into the working-directory yet.
		//
		// TODO 3/23/10 does this "if exists" cause our answer to change when doing "--test" only vs
		// TODO 3/23/10 when we are actually doing it and have "--verbose" turned on?
		// TODO
		// TODO 3/25/10 when __process_structual_changes does (or pretends to do) the work on an
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

	// TODO 3/23/10 should the __get_name, strcmp, __append__from_sz all be under the "if pCurrentParent" ?
	// TODO 3/25/10 OR should we just ASSERT(ptn->pCurrentParent) ?

	if (ptn->pCurrentParent)
		SG_ERR_CHECK(  _pt_revert__get_transient_path(pCtx,prd,ptn->pCurrentParent,&pPath_Allocated)  );

	SG_ERR_CHECK(  sg_ptnode__get_name(pCtx,ptn,&pszEntryName)  );	// get current or fallback to baseline
	if (pszEntryName && (strcmp(pszEntryName,"@") != 0))
		SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pPath_Allocated,pszEntryName)  );

	*ppPath = pPath_Allocated;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx,pPath_Allocated);
}

//////////////////////////////////////////////////////////////////

static void _pt_revert__backup_entry_for_un_add_or_un_find(SG_context * pCtx,
														   struct _pt_revert_data * prd,
														   const char * pszGidObject,
														   const SG_treediff2_ObjectData * pOD,
														   struct sg_ptnode * ptn)
{
	SG_pathname * pPathTrans = NULL;
	SG_pathname * pPathBackup = NULL;
	SG_inv_entry * pInvEntry = NULL;
	const char * pszEntryName_Current = NULL;
	const SG_pathname * pPathParked = NULL;		// we do not own this
	SG_inv_entry_flags inv_entry_flags;
	SG_bool bFound;

	SG_ERR_CHECK(  SG_inv_dirs__fetch_source_binding(pCtx,prd->pInvDirs,pszGidObject,&bFound,&pInvEntry,NULL)  );
	SG_ASSERT(  (bFound)  );

	// we don't care about PARKED_FOR_CYCLE.  we only need that flag so that we
	// can safely get from the SOURCE tree layout to the TARGET tree layout without
	// worrying about ordering problems as we un-wind the moves; there is no issue
	// about collisions and/or backing up stuff.
	//
	// we DO care about PARKED_FOR_SWAP.  this flag means that there is contention
	// for a particular ENTRYNAME that we need to address.

	SG_ERR_CHECK(  SG_inv_entry__get_flags(pCtx, pInvEntry, &inv_entry_flags)  );
	if (inv_entry_flags & SG_INV_ENTRY_FLAGS__PARKED_FOR_SWAP)
	{
		SG_ERR_CHECK(  SG_inv_entry__get_parked_path__ref(pCtx, pInvEntry, &pPathParked)  );
		SG_ASSERT(  (pPathParked)  );

		// someone else wanted our entryname so we moved the source to swap.
		// currently, both parties must be active for this to happen.  see
		// _inv_entry__check_for_swap_cb() and TODO [4] for other possibilities.

		SG_ASSERT(  (inv_entry_flags & SG_INV_ENTRY_FLAGS__ACTIVE_SOURCE)  );
		SG_ASSERT(  (inv_entry_flags & SG_INV_ENTRY_FLAGS__ACTIVE_TARGET)  );

		// get the current disk path of the SOURCE (aka TARGET) parent directory.
		// we can't use the normal repo-path-->absolute-path code because an
		// ancestor might be parked in swap.

		SG_ERR_CHECK(  _pt_revert__get_transient_path(pCtx,prd,ptn->pCurrentParent,&pPathTrans)  );

		// we can't use the normal sg_pendingtree__clear_obstacle() because we
		// might not have seen it yet in the loop that called us (and therefore
		// the other entry hasn't renamed itself to the contested entryname).
		//
		// we can't use the normal sg_pendingtree__rename_to_backup() because
		// the swap path is not in the result directory and __rename_to_backup()
		// wants to do a local rename within a directory.

		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_name(pCtx,pOD,&pszEntryName_Current)  );
		SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pPathTrans,pszEntryName_Current)  );
		SG_ERR_CHECK(  sg_pendingtree__calc_backup_name(pCtx,pPathTrans,&pPathBackup)  );

#if TRACE_REVERT
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   ("_pt_revert__backup_entry: backing-up [%s]\n"
									"                                  as [%s]\n"),
								   SG_pathname__sz(pPathParked),
								   SG_pathname__sz(pPathBackup))  );
#endif
		if (prd->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
			SG_ERR_CHECK(  _pt_log__move(pCtx,prd->pPendingTree,
										 ptn->pszgid,
										 SG_pathname__sz(pPathParked),
										 SG_pathname__sz(pPathBackup),
										 "NAME_CONTENTION")  );
		if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
			SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx,pPathParked,pPathBackup)  );
	}
	else
	{
		// no one else wanted our entryname in the directory.  so we have nothing
		// to do.  we may remove the ptnode (for ADDS), but we don't need to do
		// anything the directory.
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx,pPathTrans);
	SG_PATHNAME_NULLFREE(pCtx,pPathBackup);
}

static void _pt_revert__restore_entry_for_un_delete_or_un_lost(SG_context * pCtx,
															   struct _pt_revert_data * prd,
#if TRACE_REVERT
															   const char * pszGidObject,
#else
															   SG_UNUSED_PARAM( const char * pszGidObject ),
#endif
															   struct sg_ptnode * ptn)
{
	SG_pathname * pPathTrans = NULL;
	SG_file * pFile = NULL;
	SG_string * pStringLink = NULL;
	SG_byte * pBytes = NULL;
	const char * pszEntryName_old = NULL;
	struct sg_ptnode * ptnDesiredParent = NULL;
#ifdef DEBUG
	SG_bool bFound;
#endif

#if !TRACE_REVERT
	SG_UNUSED( pszGidObject );
#endif

	// we must assume that any entryname collisions were properly handled by
	// the SOURCE side and parked in swap; that is, we can blindly restore the
	// missing entries without collision/order problems.
	//
	// we also assume that _undelete_skeleton() has done its job and that our
	// parent directory exists on disk (either in the WD or somewhere in the swap).
	//
	// we cannot use sg_ptnode__get() because we still cannot use repo-path-->absolute
	// yet (because an ancestor directory (with a higher gid) may still be in the swap).
	// we also don't want the recursive stuff.

	// get the current disk path of the DESIRED parent directory.  (because we are after the swap
	// parking and the restore has not finished yet, we can't use the normal repo-path-->absolute-path
	// code because an ancestor directory might still be parked in swap.

	if (ptn->pszgidMovedFrom)
	{
		// pending-tree allows LOST+MOVE.  (currently, pending-tree does not allow DELETE+MOVE,
		// but that doesn't matter for our purposes here.)  but i'm going to assert that we have
		// a LOST for now.

		SG_ASSERT(  (ptn->temp_flags & sg_PTNODE_TEMP_FLAG_IMPLICIT_DELETE)  );

		// we have a +MOVE, so we need choose the TARGET parent directory where we were moved from.
		// This is a little more lookup than the normal case.

		SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx,prd->pPendingTree,ptn->pszgidMovedFrom,&ptnDesiredParent)  );

		// TODO do we need to assert that the TARGET parent will exist when we're done?
		// TODO that is, is it possible for the TARGET parent to be INACTIVE (excluded
		// TODO from the RESTORE) and was DELETED/LOST?
	}
	else
	{
		// otherwise the SOURCE parent and the TARGET parent are the same (not MOVED).

		ptnDesiredParent = ptn->pCurrentParent;
	}

	SG_ERR_CHECK(  _pt_revert__get_transient_path(pCtx,prd,ptnDesiredParent,&pPathTrans)  );

#if defined(DEBUG)
	if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
	{
		// if all is right, our DESIRED parent directory will already exist.
		SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathTrans,&bFound,NULL,NULL)  );
		SG_ASSERT(  (bFound)  );
	}
#endif

	// because we were not present in the SOURCE version of the tree, we did not claim a
	// SOURCE-entryname and thus cannot be parked in swap.  furthermore, since we claimed the
	// TARGET-entryname, we own it cleanly and can create the directory without having to
	// worry about backup names or collisions.
	//
	// we fetch the old-name because we want the TARGET version.  for DELETES, this should be
	// the same as the SOURCE version (because we don't allow DELETE+RENAME); for LOSTS, they
	// could be different, but that doesn't matter.

	SG_ERR_CHECK(  sg_ptnode__get_old_name(pCtx, ptn, &pszEntryName_old)  );
	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPathTrans, pszEntryName_old)  );

	if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		// _undelete_skeleton() should already have done the mkdir.

#if defined(DEBUG)
		if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		{
			SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathTrans,&bFound,NULL,NULL)  );
			SG_ASSERT(  (bFound)  );
		}
#endif
	}
	else if (ptn->type == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
	{
		const char* pszidhid = NULL;

		SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, ptn->pBaselineEntry, &pszidhid)  );

#if TRACE_REVERT
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   ("_pt_revert__restore_entry: restoring file contents [gid %s]\n"
									"                                                   [hid %s]\n"
									"                                                to %s\n"),
								   pszGidObject,pszidhid,SG_pathname__sz(pPathTrans))  );
#endif
		if (prd->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
			SG_ERR_CHECK(  _pt_log__restore_file(pCtx,prd->pPendingTree,
												 ptn->pszgid,
												 SG_pathname__sz(pPathTrans),
												 pszidhid,
												 "RESTORE")  );
		if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		{
			SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathTrans, SG_FILE_WRONLY | SG_FILE_CREATE_NEW, SG_FSOBJ_PERMS__MASK, &pFile)  );
			SG_ERR_CHECK(  SG_repo__fetch_blob_into_file(pCtx, ptn->pPendingTree->pRepo, pszidhid,  pFile, NULL)  );
			SG_FILE_NULLCLOSE(pCtx, pFile);
		}
	}
	else if (ptn->type == SG_TREENODEENTRY_TYPE_SYMLINK)
	{
		const char* pszidhid = NULL;
		SG_uint64 iLenBytes = 0;

		SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, ptn->pBaselineEntry, &pszidhid)  );

#if TRACE_REVERT
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   ("_pt_revert__restore_entry: restoring symlink [gid %s]\n"
									"                                             [hid %s]\n"
									"                                          to %s\n"),
								   pszGidObject,pszidhid,SG_pathname__sz(pPathTrans))  );
#endif

		SG_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, ptn->pPendingTree->pRepo, pszidhid, &pBytes, &iLenBytes)  );
		SG_ERR_CHECK(  SG_STRING__ALLOC__BUF_LEN(pCtx, &pStringLink, pBytes, (SG_uint32) iLenBytes)  );
		SG_NULLFREE(pCtx, pBytes);

		if (prd->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
			SG_ERR_CHECK(  _pt_log__restore_symlink(pCtx,prd->pPendingTree,
													ptn->pszgid,
													SG_pathname__sz(pPathTrans),
													SG_string__sz(pStringLink),
													"RESTORE")  );
		if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
			SG_ERR_CHECK(  SG_fsobj__symlink(pCtx, pStringLink, pPathTrans)  );

		SG_STRING_NULLFREE(pCtx, pStringLink);
	}
	else
	{
		SG_ERR_THROW(  SG_ERR_INVALID_PENDINGTREE_OBJECT_TYPE  );
	}

	// restore the mode bits that were saved with the entry.

	SG_ERR_CHECK(  _pt_revert__revert_attrbits(pCtx,prd,pPathTrans,ptn,"RESTORE")  );

	// restore XATTRs that were saved with the entry.

	SG_ERR_CHECK(  _pt_revert__revert_xattrs(pCtx,prd,pPathTrans,ptn,"RESTORE")  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTrans);
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_STRING_NULLFREE(pCtx, pStringLink);
	SG_NULLFREE(pCtx, pBytes);
}

//////////////////////////////////////////////////////////////////

/**
 * See if the directory with the given GID is a parent/ancestor of the given ptnode.
 * Also, return a flag indicating whether there were ANY moves to a parent/ancestor.
 *
 * ptn_Start contains the current ptnode.  We search the chain of parents back to
 * the root directory to see if one of the ancestors has the requested GID.
 *
 * TODO we could probably make this faster by passing in the start-ptnode and the
 * TODO desired-ptnode and just doing pointer compares -- rather than string
 * TODO compares on the GIDs.  But that makes a few assumptions that I'm not ready
 * TODO to make.
 */
static void _pt_revert__is_ancestor_in_tree(SG_context * pCtx,
											struct _pt_revert_data * prd,
											struct sg_ptnode * ptn_Start,
											const char * pszGidObject_Desired,
											SG_bool * pbFoundAncestor,
											SG_bool * pbHadMoves)
{
	struct sg_ptnode * ptn_Parent = NULL;
	SG_bool bActive;
	SG_bool bMoved;

	SG_ASSERT(  (ptn_Start->type == SG_TREENODEENTRY_TYPE_DIRECTORY)  );

	bActive = ((ptn_Start->temp_flags & sg_PTNODE_TEMP_FLAG_REVERTING) != 0);
	bMoved  = (bActive  &&  (ptn_Start->pszgidMovedFrom));

	if (bMoved)
		SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx,prd->pPendingTree,ptn_Start->pszgidMovedFrom,&ptn_Parent)  );
	else
		ptn_Parent = ptn_Start->pCurrentParent;

	if (!ptn_Parent  ||  !ptn_Parent->pszgid)
	{
		*pbFoundAncestor = SG_FALSE;
		*pbHadMoves = bMoved;
	}
	else if (strcmp(ptn_Parent->pszgid,pszGidObject_Desired) == 0)
	{
		*pbFoundAncestor = SG_TRUE;
		*pbHadMoves = bMoved;
	}
	else
	{
		SG_bool bFoundAncestor;
		SG_bool bAncestorHadMoves;

		SG_ERR_CHECK(  _pt_revert__is_ancestor_in_tree(pCtx,prd,ptn_Parent,pszGidObject_Desired,&bFoundAncestor,&bAncestorHadMoves)  );

		*pbFoundAncestor = bFoundAncestor;
		*pbHadMoves = (bMoved | bAncestorHadMoves);
	}

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * the entry appears in the currently-dirty working-directory, but not in the baseline.
 *
 * we only claim the "source"; that is, we only "own" this entryname in the current
 * view of the tree -- after the REVERT is complete we won't own the entryname.
 *
 * Ideally, the "revert of an add" should just forget about the entry (causing
 * it to appear as a "found" item after everything is finished).
 *
 * BUT, if another entry owns this entryname in the original baseline (and it is
 * being reverted too, then we need to forget about it *and* move it to a backup name.
 * For example, if they did:
 *     vv mv foo bar
 *     vv add foo
 * then we need to do:
 *     mv foo foo~sg00~
 *     mv bar foo
 *
 * TODO in this example, we have chosen to give the un-added file the ~sg00~ backup
 * TODO name and let the foo-->bar file have the original foo name after the un-rename.
 * TODO this might be an arbitrary choice -- perhaps we should give them both ~sg*~ names...
 */
static void _pt_revert__revert_added(SG_context * pCtx,
									 struct _pt_revert_data * prd,
									 const char * pszGidObject,
									 const SG_treediff2_ObjectData * pOD,
									 SG_diffstatus_flags dsFlags,
									 struct sg_ptnode * ptn)
{
	struct sg_ptnode * ptn_owned = NULL;
	SG_bool bActive = ((ptn->temp_flags & sg_PTNODE_TEMP_FLAG_REVERTING) != 0);

#if !defined(DEBUG)
    SG_UNUSED(dsFlags);
#endif

	switch (prd->pass)
	{
	case _PT_REVERT_PASS__SCAN_AND_BIND:
		{
			// we DO NOT filter out the non-reverting changes.  we want a complete
			// picture of entryname ownership for ALL of the structural changes.
			// but we do mark the ones that will be reverted as "active".

			// since we build a simple (non-composite) treediff, we should only see
			// plain ADDS (ADD+RENAME and/or ADD+MOVE can only occur in a composite
			// treediff).
			SG_ASSERT(  ((dsFlags & (SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED)) == 0)  );

			SG_ERR_CHECK(  _pt_revert__bind_right_side_of_diff_to_source(pCtx,prd,pszGidObject,pOD,bActive)  );
		}
		break;

	case _PT_REVERT_PASS__CHECK_FOR_CONFLICTS:
		{
			// un-ADDing this entry will not require us to create an entry on the disk.
			// we already asserted that the parent directory cannot change (via a MOVE
			// or RENAME).  so there is nothing to do here.
		}
		break;

	case _PT_REVERT_PASS__APPLY_STRUCTURAL_CHANGES:
		{
			// un-ADD this entry from the pendingtree.  afterwards, it MAY appear as a
			// FOUND entry and/or we may back-it-up using the ~sg00~ trick.  whatever
			// we do, should *try* to avoid deleting the user's file if possible.

			SG_ERR_CHECK(  _pt_revert__backup_entry_for_un_add_or_un_find(pCtx,prd,pszGidObject,pOD,ptn)  );

			if (bActive)
			{
				// remove this entry from pendingtree so that it will look like a FOUND item after we're done.

#if TRACE_REVERT
				SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
										   "_pt_revert__revert_added: un-adding [%s]\n",
										   pszGidObject)  );
#endif

				// since the entry will become a "found" rather than "added" node, we
				// get rid of its timestamp cache entry (knowing that scan-dir doesn't
				// create entries for "found" items).

				SG_ERR_CHECK(  SG_pendingtree__clear_wd_file_timestamp(pCtx, ptn->pPendingTree, ptn->pszgid)  );

#if 0
				// TODO XREF[A]:
				// TODO in the original REVERT code, an un-ADD removes the PTNODE.  This causes
				// TODO sg_pendingtree__find_by_gid() to return NULL and causes us problems.
				// TODO this is best seen when you delete (implicitly or explicitly) multiple
				// TODO levels of directories and want to find something nested inside it.
				// TODO
				// TODO so, is it correct to just change the __ADDED to a __FOUND by changing
				// TODO the temp_flags?
				// TODO
				// TODO see also XREF[B] and XREF[C].
				SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx,ptn->pCurrentParent->prbItems,pszGidObject,(void **)&ptn_owned)  );
				SG_ASSERT(  (ptn_owned == ptn)  );
				SG_PTNODE_NULLFREE(pCtx,ptn_owned);
#else
				ptn->temp_flags |= sg_PTNODE_TEMP_FLAG_IMPLICIT_ADD;
#endif
			}
		}
		break;

	default:
		break;
	}

fail:
	SG_PTNODE_NULLFREE(pCtx,ptn_owned);
}

/**
 * the entry does not appear in the currently-dirty working-directory, but it does appear
 * in the baseline.
 *
 * the REVERT should re-create this entry from data in the repo.
 *
 * we only claim the original entryname (in the original parent directory) in the "target".
 * we do not claim anything in the "source".
 *
 * TODO if a directory is deleted that contains a bunch of stuff,  the contents within the
 * TODO directory are marked "IMPLICIT" and deleted too.  So, each of them should appear
 * TODO in the pendingtree too, right?  So, we shouldn't have to dive on a directory because
 * TODO each of the things within will get processed in turn.  (Now we may have to do some
 * TODO magic to get the parent to get processed before the children....)
 */
static void _pt_revert__revert_deleted(SG_context * pCtx,
									   struct _pt_revert_data * prd,
									   const char * pszGidObject,
									   const SG_treediff2_ObjectData * pOD,
									   SG_diffstatus_flags dsFlags,
									   struct sg_ptnode * ptn)
{
	SG_bool bActive = ((ptn->temp_flags & sg_PTNODE_TEMP_FLAG_REVERTING) != 0);

#if !defined(DEBUG)
    SG_UNUSED(dsFlags);
#endif

	switch (prd->pass)
	{
	case _PT_REVERT_PASS__SCAN_AND_BIND:
		{
			// we DO NOT filter out the non-reverting changes.  we want a complete
			// picture of entryname ownership for ALL of the structural changes.
			// but we do mark the ones that will be reverted as "active".

			// TODO the user should not be able to do a RENAME+DELETE, so both the "original"
			// TODO and "current" entrynames should be the same, so the following assert
			// TODO is probably not necessary.
			SG_ASSERT(  ((dsFlags & SG_DIFFSTATUS_FLAGS__RENAMED) == 0)  );

			// TODO likewise, they should not be able to do a MOVE+DELETE, so both the "original"
			// TODO and "current" parent directories should be the same.  so the following assert
			// TODO is also probably not necessary.
			SG_ASSERT(  ((dsFlags & SG_DIFFSTATUS_FLAGS__MOVED) == 0)  );

			SG_ERR_CHECK(  _pt_revert__bind_left_side_of_diff_to_target(pCtx,prd,pszGidObject,pOD,bActive)  );
		}
		break;

	case _PT_REVERT_PASS__CHECK_FOR_CONFLICTS:
		{
			SG_ASSERT(  (ptn->pCurrentParent)  );		// the user-root directory could not have been deleted

			// if we are un-deleting, this entry make sure that the parent directory is/will be present;
			// that is, if the the parent was also deleted/lost make sure that it WILL be restored too.
			// (we can't just check to see if the parent directory exists because we haven't started
			// any of the restores yet.)

			if (bActive)
			{
				// TODO we are assuming that we still don't allow a moved entry to be deleted in the same commit,
				// TODO so we don't have to worry about which parent in the following.
				SG_bool bActiveParent = ((ptn->pCurrentParent->temp_flags & sg_PTNODE_TEMP_FLAG_REVERTING) != 0);

				if (!bActiveParent)
				{
					const char * pszGid_Parent;
					const SG_treediff2_ObjectData * pOD_Parent;
					SG_bool bFound_Parent;
					SG_diffstatus_flags dsFlags_Parent;

					SG_ERR_CHECK(  SG_treediff2__ObjectData__get_parent_gid(pCtx,pOD,&pszGid_Parent)  );
					SG_ASSERT(  (strcmp(pszGid_Parent,ptn->pCurrentParent->pszgid) == 0)  );

					SG_ERR_CHECK(  SG_treediff2__get_ObjectData(pCtx,prd->pTreeDiff,pszGid_Parent,&bFound_Parent,&pOD_Parent)  );
					SG_ASSERT(  (bFound_Parent)  );		// parent dir should also be in treediff because dir contents changed
					SG_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx,pOD_Parent,&dsFlags_Parent)  );

					if (dsFlags_Parent & (SG_DIFFSTATUS_FLAGS__DELETED | SG_DIFFSTATUS_FLAGS__LOST))
						SG_ERR_THROW(  SG_ERR_PENDINGTREE_PARTIAL_CHANGE_COLLISION  );
				}
			}
		}
		break;

	case _PT_REVERT_PASS__APPLY_STRUCTURAL_CHANGES:
		{
			// un-delete entry.  restore it from the repo to the target directory.

			if (bActive)
			{
				SG_ERR_CHECK(  _pt_revert__restore_entry_for_un_delete_or_un_lost(pCtx,prd,pszGidObject,ptn)  );

				ptn->saved_flags &= ~sg_PTNODE_SAVED_FLAG_DELETED;
				ptn->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;
			}
		}
		break;

	default:
		break;
	}

fail:
	return;
}

/**
 * the entry was FOUND in the currently-dirty working-directory but it was not ADDED
 * (and also not in the baseline).  it is just a random file/symlink/directory that
 * got created in the directory; we know nothing about it.
 *
 * REVERT has nothing to do here.  Just like when we REVERT and ADDED file, we leave
 * it in the directory so that it will appear as a FOUND entry.  However, we do do
 * the swap/backup trick (like for ADDED files), so that another entry that needs
 * this entryname can have it.
 *
 * TODO is it appropriate to move the found item to a ~sg*~ name when another
 * TODO entry (that we are tracking) needs the entryname?  I mean, we really don't
 * TODO know anything about this entry -- we just found it in the directory.
 */
static void _pt_revert__revert_found(SG_context * pCtx,
									 struct _pt_revert_data * prd,
									 const char * pszGidObject,
									 const SG_treediff2_ObjectData * pOD,
									 SG_diffstatus_flags dsFlags,
									 struct sg_ptnode * ptn)
{
	SG_bool bActive = ((ptn->temp_flags & sg_PTNODE_TEMP_FLAG_REVERTING) != 0);

#if !defined(DEBUG)
    SG_UNUSED(dsFlags);
#endif

	switch (prd->pass)
	{
	case _PT_REVERT_PASS__SCAN_AND_BIND:
		{
			// we DO NOT filter out the non-reverting changes.  we want a complete
			// picture of entryname ownership for ALL of the structural changes.
			// but we do mark the ones that will be reverted as "active".

			// we found an entry in a particular directory.  that's where it is and that's
			// all we know about it.  so it can't have been moved/renamed.
			SG_ASSERT(  ((dsFlags & (SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED)) == 0)  );

			SG_ERR_CHECK(  _pt_revert__bind_right_side_of_diff_to_source(pCtx,prd,pszGidObject,pOD,bActive)  );
		}
		break;

	case _PT_REVERT_PASS__CHECK_FOR_CONFLICTS:
		{
			// there is nothing for us to do for a FOUND entry.
		}
		break;

	case _PT_REVERT_PASS__APPLY_STRUCTURAL_CHANGES:
		{
			// we don't really "un-find" it.  that's just plain silly.  we leave it
			// in the directory so that it will still have FOUND status after we're
			// finished.  just like we do for un-ADDED entries.

			SG_ERR_CHECK(  _pt_revert__backup_entry_for_un_add_or_un_find(pCtx,prd,pszGidObject,pOD,ptn)  );

			// bActive doesn't really matter here because the entry wasn't registered.

#if TRACE_REVERT
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
									   "_pt_revert__revert_found: leaving [%s]\n",
									   pszGidObject)  );
#endif
		}
		break;

	default:
		break;
	}

fail:
	return;
}

/**
 * the entry cannot be found in the currently-dirty working-directory, but it did appear
 * in the baseline.  (they probably moved/deleted it without telling us about it.
 *
 * the REVERT should re-create it as it was.
 *
 * note that they could have done a "vv move" or "vv rename" on it *AND THEN*
 * done a "/bin/rm" on it.  in this cse, we'd have LOST+RENAME or LOST+MOVE (or
 * LOST+RENAME+MOVE).  but none of that matters because we want the orginal name/location
 * as it appeared in the baseline.
 */
static void _pt_revert__revert_lost(SG_context * pCtx,
									struct _pt_revert_data * prd,
									const char * pszGidObject,
									const SG_treediff2_ObjectData * pOD,
									SG_diffstatus_flags dsFlags,
									struct sg_ptnode * ptn)
{
	SG_bool bActive = ((ptn->temp_flags & sg_PTNODE_TEMP_FLAG_REVERTING) != 0);

#if !defined(DEBUG)
    SG_UNUSED(dsFlags);
#endif

	switch (prd->pass)
	{
	case _PT_REVERT_PASS__SCAN_AND_BIND:
		{
			// we DO NOT filter out the non-reverting changes.  we want a complete
			// picture of entryname ownership for ALL of the structural changes.
			// but we do mark the ones that will be reverted as "active".

			SG_ERR_CHECK(  _pt_revert__bind_left_side_of_diff_to_target(pCtx,prd,pszGidObject,pOD,bActive)  );
		}
		break;

	case _PT_REVERT_PASS__CHECK_FOR_CONFLICTS:
		{
			// we are reverting a LOST entry so we need to restore it.  make sure that the parent directory
			// is present; that is, if the the parent was also deleted/lost make sure that it WILL be restored too.
			// (we can't just check to see if the parent directory exists because we haven't started
			// any of the restores yet.)  we have to be careful here because we could have LOST+MOVED.

			if (bActive)
			{
				const char * pszGid_Parent = NULL;
				struct sg_ptnode * ptn_Parent = NULL;
				SG_bool bActive_Parent;

				if (dsFlags & SG_DIFFSTATUS_FLAGS__MOVED)			// LOST+MOVED
					SG_ERR_CHECK(  SG_treediff2__ObjectData__get_moved_from_gid(pCtx,pOD,&pszGid_Parent)  );
				else
					SG_ERR_CHECK(  SG_treediff2__ObjectData__get_parent_gid(pCtx,pOD,&pszGid_Parent)  );
				SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx,prd->pPendingTree,pszGid_Parent,&ptn_Parent)  );

				bActive_Parent = ((ptn_Parent->temp_flags & sg_PTNODE_TEMP_FLAG_REVERTING) != 0);
				if (bActive_Parent)
				{
					// if the original parent is active, it will be restored/reverted too.  so we can assume that
					// it will (eventually) be as it was so that this entry can be placed in it.
				}
				else
				{
					// the original parent is not active, it will not be restored/reverted.  make sure that that
					// directory is currently present in the tree.

					const SG_treediff2_ObjectData * pOD_Parent;
					SG_bool bFound_Parent;
					SG_diffstatus_flags dsFlags_Parent;

					SG_ERR_CHECK(  SG_treediff2__get_ObjectData(pCtx,prd->pTreeDiff,pszGid_Parent,&bFound_Parent,&pOD_Parent)  );
					SG_ASSERT(  (bFound_Parent)  );		// parent dir should also be in treediff because dir contents changed
					SG_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx,pOD_Parent,&dsFlags_Parent)  );

					if (dsFlags_Parent & (SG_DIFFSTATUS_FLAGS__DELETED | SG_DIFFSTATUS_FLAGS__LOST))
						SG_ERR_THROW(  SG_ERR_PENDINGTREE_PARTIAL_CHANGE_COLLISION  );
				}
			}
		}
		break;

	case _PT_REVERT_PASS__APPLY_STRUCTURAL_CHANGES:
		{
			// un-lose the entry.  restore it from the repo to the target directory.

			if (bActive)
			{
				SG_ERR_CHECK(  _pt_revert__restore_entry_for_un_delete_or_un_lost(pCtx,prd,pszGidObject,ptn)  );

				if (ptn->pszgidMovedFrom)
				{
					// LOST+MOVED.  so in addition to clearing various flags, we need to move this ptn
					// back to the TARGET parent and delete the phantom/moved-away ptnode.  (just like
					// in __revert_moved().

					struct sg_ptnode * ptn_MovedFromParent = NULL;
					struct sg_ptnode * ptnMe = NULL;
					struct sg_ptnode * ptnPhantomMe = NULL;

					// remove ptnode from our current parent

					SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx,ptn->pCurrentParent->prbItems,pszGidObject,(void **)&ptnMe)  );
					SG_ASSERT(  (ptnMe == ptn)  );

					// remove the old phantom ptnode from our old parent

					SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx,prd->pPendingTree,ptn->pszgidMovedFrom,&ptn_MovedFromParent)  );
					SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx,ptn_MovedFromParent->prbItems,pszGidObject,(void **)&ptnPhantomMe)  );
					SG_PTNODE_NULLFREE(pCtx,ptnPhantomMe);

					// add this ptnode back to the old parent

					ptn->pszgidMovedFrom = NULL;		// the un-MOVE
					SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx,ptn_MovedFromParent,ptn)  );
				}

				ptn->pszCurrentName = NULL;				// the un-RENAME (if necessary)

				ptn->saved_flags &= ~sg_PTNODE_SAVED_FLAG_DELETED;
				ptn->temp_flags &= ~sg_PTNODE_TEMP_FLAG_IMPLICIT_DELETE;
				ptn->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;
			}
		}
		break;

	default:
		break;
	}

fail:
	return;
}

/**
 * the entry was RENAMED (but not MOVED) in the currently-dirty working-direcory
 * relative to the original baseline.   since there was no MOVE, both entrynames
 * are in the same directory.
 *
 * the REVERT should undo the rename.
 */
static void _pt_revert__revert_renamed(SG_context * pCtx,
									   struct _pt_revert_data * prd,
									   const char * pszGidObject,
									   const SG_treediff2_ObjectData * pOD,
									   SG_diffstatus_flags dsFlags,
									   struct sg_ptnode * ptn)
{
	const char * pszEntryName_Original;
	SG_pathname * pPathTransSource = NULL;
	SG_pathname * pPathTransTarget = NULL;
#ifdef DEBUG
	SG_bool bFound;
#endif
	SG_bool bActive = ((ptn->temp_flags & sg_PTNODE_TEMP_FLAG_REVERTING) != 0);

#if !defined(DEBUG)
    SG_UNUSED(dsFlags);
#endif

	SG_ASSERT(  ((dsFlags & SG_DIFFSTATUS_FLAGS__ADDED) == 0)  );
	SG_ASSERT(  ((dsFlags & SG_DIFFSTATUS_FLAGS__DELETED) == 0)  );
	SG_ASSERT(  ((dsFlags & SG_DIFFSTATUS_FLAGS__LOST) == 0)  );
	SG_ASSERT(  ((dsFlags & SG_DIFFSTATUS_FLAGS__FOUND) == 0)  );
	SG_ASSERT(  ((dsFlags & SG_DIFFSTATUS_FLAGS__MOVED) == 0)  );	// we do RENAME-only, see __revert_moved()

	switch (prd->pass)
	{
	case _PT_REVERT_PASS__SCAN_AND_BIND:
		{
			// we DO NOT filter out the non-reverting changes.  we want a complete
			// picture of entryname ownership for ALL of the structural changes.
			// but we do mark the ones that will be reverted as "active".

			// claim the CURRENT entryname as the "source".
			// claim the ORIGINAL entryname as the "target".

			SG_ERR_CHECK(  _pt_revert__bind_right_side_of_diff_to_source(pCtx,prd,pszGidObject,pOD,bActive)  );
			SG_ERR_CHECK(  _pt_revert__bind_left_side_of_diff_to_target(pCtx,prd,pszGidObject,pOD,bActive)  );
		}
		break;

	case _PT_REVERT_PASS__CHECK_FOR_CONFLICTS:
		{
			// to un-RENAME an entry, we just need to change the name within the current
			// directory.  that directory must exist on disk (either in the original repo-path
			// location or in a swap path) and it should exist in both the before and after
			// views of tree -- so the parent dir cannot have been ADDED/FOUND/DELETED/LOST.  and so
			// we should be able to do the job without any conflicts because of the parent.
			//
			// note: _inv_entry__check_for_swap_cb() already checked that we have clear title
			// to our original (aka TARGET) entryname.  so there are no name collisions.
		}
		break;

	case _PT_REVERT_PASS__APPLY_STRUCTURAL_CHANGES:
		{
			if (bActive)
			{
				// because we are after the swap parking -and- the restore has not finished yet,
				// we can't use the normal repo-path-->absolute-path code because an ancestor
				// directory might still be parked in swap.

				// get the current disk path of the current SOURCE entry.  this may be in the
				// current SOURCE parent directory -or- it may have been moved to swap.

				SG_ERR_CHECK(  _pt_revert__get_transient_path(pCtx,prd,ptn,&pPathTransSource)  );

				// get the current disk path of the SOURCE (aka TARGET) parent directory and
				// append the TARGET entryname.  the resulting pathname should not exist on disk.

				SG_ERR_CHECK(  _pt_revert__get_transient_path(pCtx,prd,ptn->pCurrentParent,&pPathTransTarget)  );
#if defined(DEBUG)
				if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
				{
					SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathTransTarget,&bFound,NULL,NULL)  );
					SG_ASSERT(  (bFound)  );
				}
#endif
				SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx,pOD,&pszEntryName_Original)  );
				SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pPathTransTarget,pszEntryName_Original)  );
#if defined(DEBUG)
				if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
				{
					SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathTransTarget,&bFound,NULL,NULL)  );
//					SG_ASSERT(  (!bFound)  );
					if (bFound)
						SG_ERR_THROW2(  SG_ERR_NOTIMPLEMENTED,
										(pCtx,
										 "_pt_revert__revert_rename: deal with collisions caused by case-insensitive filesystems [pPathTransTarget %s]",
										 SG_pathname__sz(pPathTransTarget))  );
				}
#endif

#if TRACE_REVERT
				SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
										   ("_pt_revert__revert_rename: restoring entry [gid %s]\n"
											"                     renamed path (source) %s\n"
											"                    original path (target) %s\n"),
										   pszGidObject,
										   SG_pathname__sz(pPathTransSource),
										   SG_pathname__sz(pPathTransTarget))  );
#endif
				if (prd->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
					SG_ERR_CHECK(  _pt_log__move(pCtx,prd->pPendingTree,
												 ptn->pszgid,
												 SG_pathname__sz(pPathTransSource),
												 SG_pathname__sz(pPathTransTarget),
												 "UNDO_RENAME")  );
				if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
					SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx,pPathTransSource,pPathTransTarget)  );

				ptn->pszCurrentName = NULL;
				ptn->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;
			}
		}
		break;

	default:
		break;
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx,pPathTransSource);
	SG_PATHNAME_NULLFREE(pCtx,pPathTransTarget);
}

//////////////////////////////////////////////////////////////////

static void _pt_revert__revert_moved__apply(SG_context * pCtx,
											struct _pt_revert_data * prd,
											struct sg_ptnode * ptn)
{
	const char * pszEntryName_Original;
	struct sg_ptnode * ptn_MovedFromParent;
	struct sg_ptnode * ptnMe;
	struct sg_ptnode * ptnPhantomMe = NULL;
	SG_pathname * pPathTransSource = NULL;
	SG_pathname * pPathTransTarget = NULL;
#ifdef DEBUG
	SG_bool bFound;
#endif

	// because we are after the swap parking -and- the restore has not finished yet,
	// we can't use the normal repo-path-->absolute-path code because an ancestor
	// directory might still be parked in swap.

	// get the current disk path of the current SOURCE entry.  this may be in the
	// current SOURCE parent directory -or- it may have been moved to swap.

	SG_ERR_CHECK(  _pt_revert__get_transient_path(pCtx,prd,ptn,&pPathTransSource)  );

	// get the ptnode of the moved-from parent.  we cannot use __find_by_gid() right
	// now (because the tree may be temporarily broken (disconnected, cyclic, or both)).

	SG_ASSERT(  (ptn->ptnPhantomMe)  );
	ptn_MovedFromParent = ptn->ptnPhantomMe->pCurrentParent;
	SG_ASSERT(  (ptn_MovedFromParent)  );

	// get the current disk path of the TARGET ( != SOURCE) parent directory and
	// append the TARGET entryname (this covers MOVES and MOVES+RENAME).

	SG_ERR_CHECK(  _pt_revert__get_transient_path(pCtx,prd,ptn_MovedFromParent,&pPathTransTarget)  );
#if defined(DEBUG)
	if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
	{
		SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathTransTarget,&bFound,NULL,NULL)  );
		SG_ASSERT(  (bFound)  );
	}
#endif

	SG_ERR_CHECK(  sg_ptnode__get_old_name(pCtx,ptn,&pszEntryName_Original)  );
	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pPathTransTarget,pszEntryName_Original)  );
#if defined(DEBUG)
	if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
	{
		SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathTransTarget,&bFound,NULL,NULL)  );
//		SG_ASSERT(  (!bFound)  );
		if (bFound)
			SG_ERR_THROW2(  SG_ERR_NOTIMPLEMENTED,
							(pCtx,
							 "_pt_revert__revert_move: deal with collisions caused by case-insensitive filesystems [pPathTransTarget %s]",
							 SG_pathname__sz(pPathTransTarget))  );
	}
#endif

#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_revert__revert_move: restoring entry [gid %s]\n"
								"                     moved path (source) %s\n"
								"                  original path (target) %s\n"),
							   ptn->pszgid,
							   SG_pathname__sz(pPathTransSource),
							   SG_pathname__sz(pPathTransTarget))  );
#endif
	if (prd->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
		SG_ERR_CHECK(  _pt_log__move(pCtx,prd->pPendingTree,
									 ptn->pszgid,
									 SG_pathname__sz(pPathTransSource),
									 SG_pathname__sz(pPathTransTarget),
									 "UNDO_MOVE")  );
	if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx,pPathTransSource,pPathTransTarget)  );

	// remove ptnode from our current parent

	SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx,ptn->pCurrentParent->prbItems,ptn->pszgid,(void **)&ptnMe)  );
	SG_ASSERT(  (ptnMe == ptn)  );
#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_pt_revert__revert_move: removing child [parent %s][child %s][child saved %d][child temp %d]\n",
							   ptn->pCurrentParent->pszgid,
							   ptn->pszgid,ptn->saved_flags,ptn->temp_flags)  );
#endif

	// remove the old phantom ptnode from our old parent

	SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx,ptn_MovedFromParent->prbItems,ptn->pszgid,(void **)&ptnPhantomMe)  );
	SG_ASSERT(  (ptnPhantomMe == ptn->ptnPhantomMe)  );
#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_pt_revert__revert_move: removing child [parent %s][child %s][child saved %d][child temp %d]\n",
							   ptn_MovedFromParent->pszgid,
							   ptnPhantomMe->pszgid,ptnPhantomMe->saved_flags,ptnPhantomMe->temp_flags)  );
#endif
	SG_ASSERT(  (ptnPhantomMe->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)  );
	SG_PTNODE_NULLFREE(pCtx,ptnPhantomMe);
	ptn->ptnPhantomMe = NULL;

	// add this ptnode back to the old parent

	ptn->pszgidMovedFrom = NULL;		// the un-MOVE
	SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx,ptn_MovedFromParent,ptn)  );

	ptn->pszCurrentName = NULL;			// the un-RENAME (if necessary)
	ptn->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;

fail:
	SG_PATHNAME_NULLFREE(pCtx,pPathTransSource);
	SG_PATHNAME_NULLFREE(pCtx,pPathTransTarget);
}

//////////////////////////////////////////////////////////////////

/**
 * the entry was MOVED (and possibly RENAMED) in the currently-dirty working-direcory
 * relative to its name/location in the original baseline.
 *
 * handle the REVERT of MOVE or MOVE+RENAME actions.
 */
static void _pt_revert__revert_moved(SG_context * pCtx,
									 struct _pt_revert_data * prd,
									 const char * pszGidObject,
									 const SG_treediff2_ObjectData * pOD,
									 SG_diffstatus_flags dsFlags,
									 struct sg_ptnode * ptn)
{
	SG_bool bActive = ((ptn->temp_flags & sg_PTNODE_TEMP_FLAG_REVERTING) != 0);

#if !defined(DEBUG)
    SG_UNUSED(dsFlags);
#endif

	SG_ASSERT(  ((dsFlags & SG_DIFFSTATUS_FLAGS__ADDED) == 0)  );
	SG_ASSERT(  ((dsFlags & SG_DIFFSTATUS_FLAGS__DELETED) == 0)  );
	SG_ASSERT(  ((dsFlags & SG_DIFFSTATUS_FLAGS__LOST) == 0)  );
	SG_ASSERT(  ((dsFlags & SG_DIFFSTATUS_FLAGS__FOUND) == 0)  );

	switch (prd->pass)
	{
	case _PT_REVERT_PASS__SCAN_AND_BIND:
		{
			// we DO NOT filter out the non-reverting changes.  we want a complete
			// picture of entryname ownership for ALL of the structural changes.
			// but we do mark the ones that will be reverted as "active".

			// claim the CURRENT entryname in the CURRENT directory as the "source".
			// claim the ORIGINAL entryname in the ORIGINAL directory as the "target".

			SG_ERR_CHECK(  _pt_revert__bind_right_side_of_diff_to_source(pCtx,prd,pszGidObject,pOD,bActive)  );
			SG_ERR_CHECK(  _pt_revert__bind_left_side_of_diff_to_target(pCtx,prd,pszGidObject,pOD,bActive)  );
			return;
		}

	case _PT_REVERT_PASS__CHECK_FOR_CONFLICTS:
		{
			// to undo a MOVE we need to ensure that the original (aka TARGET) parent
			// directory WILL exist after everything is finished.

			if (bActive)
			{
				struct sg_ptnode * ptn_MovedFromParent = NULL;
				SG_bool bActive_MovedFromParent;
				SG_bool bFoundPhantomMe;

				// get the ptnode of the moved-from parent.
				SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx,prd->pPendingTree,ptn->pszgidMovedFrom,&ptn_MovedFromParent)  );

				bActive_MovedFromParent = ((ptn_MovedFromParent->temp_flags & sg_PTNODE_TEMP_FLAG_REVERTING) != 0);
				if (bActive_MovedFromParent)
				{
					// if the original parent is active, it will be restored/reverted too.  so we can assume that
					// it will (eventually) be as it was so that this entry can be placed in it.
				}
				else
				{
					// the original parent is not active, it will not be restored/reverted.  make sure that that
					// directory is currently present in the tree.

					const SG_treediff2_ObjectData * pOD_MovedFromParent;
					SG_bool bFound_MovedFromParent;
					SG_diffstatus_flags dsFlags_MovedFromParent;

					SG_ERR_CHECK(  SG_treediff2__get_ObjectData(pCtx,prd->pTreeDiff,ptn->pszgidMovedFrom,&bFound_MovedFromParent,&pOD_MovedFromParent)  );
					SG_ASSERT(  (bFound_MovedFromParent)  );		// parent dir should also be in treediff because dir contents changed
					SG_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx,pOD_MovedFromParent,&dsFlags_MovedFromParent)  );

					if (dsFlags_MovedFromParent & (SG_DIFFSTATUS_FLAGS__DELETED | SG_DIFFSTATUS_FLAGS__LOST))
						SG_ERR_THROW(  SG_ERR_PENDINGTREE_PARTIAL_CHANGE_COLLISION  );
				}

				// find the phantom/moved-away ptnode for this ptnode in the moved-from parent.
				// temporarily store this PTNODE POINTER in our ptnode so that we will have it
				// when get ready to apply the changes.  this is necessary because in some screwcases
				// (when we need to use the PARKED_FOR_CYCLE code), as we apply the changes, the ptnode
				// tree can become temporarily disconnected or cyclic.  besides being distrubing, it
				// causes __find_by_gid() to fail.

				SG_ERR_CHECK(  SG_rbtree__find(pCtx,ptn_MovedFromParent->prbItems,pszGidObject,&bFoundPhantomMe,(void *)&ptn->ptnPhantomMe)  );
				SG_ASSERT(  (bFoundPhantomMe)  );
				SG_ASSERT(  (ptn->ptnPhantomMe->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)  );

				if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
				{
					// for moved directories, if the directory has a moved parent/ancestor then it is possible
					// for us to get pathname cycles.  in theory, if everything is being reverted, then both the
					// SOURCE and TARGET views of the tree should be consistent (and cycle-free).  but for a
					// partial revert, one of the ancestor moves could be omitted and we might stumble into the
					// case where we want to try to un-move this directory and make it a parent/ancestor of itself.

					SG_bool bIsAncestor, bAncestorHadMoves;

					SG_ERR_CHECK(  _pt_revert__is_ancestor_in_tree(pCtx,prd,ptn,pszGidObject,&bIsAncestor,&bAncestorHadMoves)  );
					if (bIsAncestor)
						SG_ERR_THROW(  SG_ERR_PENDINGTREE_PARTIAL_CHANGE_COLLISION  );

					// even when there are no cycles (in the SOURCE view -or- in the TARGET view), we still may
					// have problems un-winding all of the changes -- because of a transient cycle.  for example,
					// if we have:
					//      @/dir_1/dir_2/dir_3/dir_4/
					//      @/dir_1_dir_5/
					// and do:
					//      mv dir_1/dir_2/dir_3 dir_1/dir_5/
					//      mv dir_1/dir_2       dir_1/dir_5/dir_3/dir_4/
					// giving:
					//      @/dir_1/dir_5/dir_3/dir_4/dir_2/
					// (so 2 and 3 are tangled)
					//
					// to undo these moves, we either need to know the ordering (have to undo dir_2 *before* dir_3
					// or we get an error in the filesystem) --or-- we have to park one or both of them so that we
					// can undo them without worrying about the order.
					//
					// i'm going to do the latter and force park the entry.  this will produce some false positives
					// because we're only going to ask if ANY ancestor was moved.  it may be the case that the
					// ancestor's move may be harmless and not tangled with our move (but then again, it could be some
					// convoluted permutation that cause a cycle of cycles)).  but that doesn't matter, we REGISTER
					// to park this directory knowing that we can unpark them all later.

					if (bAncestorHadMoves)
					{
						SG_inv_entry * pInvEntry = NULL;
						SG_bool bFound;

						SG_ERR_CHECK(  SG_inv_dirs__fetch_source_binding(pCtx,prd->pInvDirs,pszGidObject,&bFound,&pInvEntry,NULL)  );
						SG_ASSERT(  (bFound)  );
						SG_ERR_CHECK_RETURN(  SG_inv_entry__create_parked_pathname(pCtx,pInvEntry,SG_INV_ENTRY_FLAGS__PARKED_FOR_CYCLE)  );

						// add this ptnode to the list of potential cycles so that we can defer it until [7b].

						if (!prd->pVecCycles)
							SG_ERR_CHECK(  SG_VECTOR__ALLOC(pCtx,&prd->pVecCycles,10)  );
						SG_ERR_CHECK(  SG_vector__append(pCtx,prd->pVecCycles,ptn,NULL)  );
						ptn->temp_flags |= sg_PTNODE_TEMP_FLAG_PARKED_FOR_CYCLE;
					}
				}
			}
		}
		break;

	case _PT_REVERT_PASS__APPLY_STRUCTURAL_CHANGES:
		{
			if (bActive)
			{
				// defer the parked-for-cycle ones to step [7b].
				if ((ptn->temp_flags & sg_PTNODE_TEMP_FLAG_PARKED_FOR_CYCLE) == 0)
					SG_ERR_CHECK(  _pt_revert__revert_moved__apply(pCtx,prd,ptn)  );
			}
		}
		break;

	default:
		break;
	}

fail:
	return;
}

//////////////////////////////////////////////////////////////////

static SG_treediff2_foreach_callback _pt_revert__process_structural_changes;

static void _pt_revert__process_structural_changes(SG_context * pCtx,
												   SG_UNUSED_PARAM( SG_treediff2 * pTreeDiff ),
												   const char * pszGidObject,
												   const SG_treediff2_ObjectData * pOD,
												   void * pVoidData)
{
	struct _pt_revert_data * prd = (struct _pt_revert_data *)pVoidData;
	struct sg_ptnode * ptn = NULL;
	SG_diffstatus_flags dsFlags;

	SG_UNUSED( pTreeDiff );

	// TODO XREF[C] see XREF[A]
	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx,prd->pPendingTree,pszGidObject,&ptn)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx,pOD,&dsFlags)  );

#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "ProcessStructuralChanges: [pass %d][gid %s][dsFlags 0x%x][ptn s flags %d][ptn t flags %d]\n",
							   prd->pass,pszGidObject,dsFlags,ptn->saved_flags,ptn->temp_flags)  );
#endif

	switch (dsFlags & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE)
	{
	case SG_DIFFSTATUS_FLAGS__ADDED:
		SG_ERR_CHECK(  _pt_revert__revert_added(pCtx,prd,pszGidObject,pOD,dsFlags,ptn)  );
		return;

	case SG_DIFFSTATUS_FLAGS__DELETED:
		SG_ERR_CHECK(  _pt_revert__revert_deleted(pCtx,prd,pszGidObject,pOD,dsFlags,ptn)  );
		return;

	case SG_DIFFSTATUS_FLAGS__FOUND:
		SG_ERR_CHECK(  _pt_revert__revert_found(pCtx,prd,pszGidObject,pOD,dsFlags,ptn)  );
		return;

	case SG_DIFFSTATUS_FLAGS__LOST:
		SG_ERR_CHECK(  _pt_revert__revert_lost(pCtx,prd,pszGidObject,pOD,dsFlags,ptn)  );
		return;

	default:		// __MODIFIED or __ZERO

		// __MODIFIED or UNMODIFIED (and LOST) entries can have __RENAMED and/or __MOVED (because we are
		// assuming a simple (non-composite) treediff.

		if (dsFlags & SG_DIFFSTATUS_FLAGS__MOVED)			// MOVED or MOVED+RENAMED
		{
			SG_ERR_CHECK(  _pt_revert__revert_moved(pCtx,prd,pszGidObject,pOD,dsFlags,ptn)  );
			return;
		}
		else if (dsFlags & SG_DIFFSTATUS_FLAGS__RENAMED)	// only RENAMED
		{
			SG_ERR_CHECK(  _pt_revert__revert_renamed(pCtx,prd,pszGidObject,pOD,dsFlags,ptn)  );
			return;
		}

		// there were no structural changes.
		//
		// the only thing left are __MODIFIED, __CHANGED_XATTRS, and __CHANGED_ATTRBITS.
		// these don't matter because they have the SAME entryname and the SAME parent
		// directory in both the SOURCE and TARGET views of the tree -- just like all of
		// the unmodified entries that do not appear in the treediff at all.
		//
		// we DO NOT claim anything for them (in either side).
		//
		// That is, we DO NOT put them into the inverted tree/index.  We DO NOT cause the
		// entryname to appear in the _inv_dir.prbEntries map.  BUT, it MAY get loaded if
		// the _inv_dir code needs to scan the directory.

		return;
	}

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * Use the hierarchial view of the pendingtree to do a depth-first walk and move
 * all entries that need to be parked/swapped to their associated "parked" pathname.
 * We have to do this depth-first because as soon as we start this the whole
 * "repo-path --> absolute-path" stuff is invalid.
 *
 * For example, if we have:
 *      @/dir1/file1
 *      @/dir1/file2
 *      @/dir2/x
 * And the user does:
 *      vv rename ./dir1/file1 ./dir1/ftemp
 *      vv rename ./dir1/file2 ./dir1/file1
 *      vv rename ./dir1/ftemp ./dir1/file2
 *      vv rename ./dir1       ./dtemp
 *      vv rename ./dir2       ./dir1
 *      vv rename ./dtemp      ./dir2
 * Then the treediff sees:
 *      file1 --> file2
 *      file2 --> file1
 *      dir1  --> dir2
 *      dir2  --> dir1
 * Both of these files and both of these directories will have swap pathnames.
 * We must park the files before we park the directories, because parking the
 * directory also moves the contents of the directory.
 *
 * TODO 03/19/10 See about using the new pInvDirs->prbQueueForParking queue
 * TODO 03/19/10 to drive the move rather than this dive thru the entire
 * TODO 03/19/10 pendingtree.
 */
static void _pt_revert__move_to_parking_lot(SG_context * pCtx, struct _pt_revert_data * prd, const struct sg_ptnode * ptn, SG_uint32 depth)
{
	SG_rbtree_iterator * pIter = NULL;
	SG_pathname * pPath_in_wd = NULL;
	SG_bool bFound;

	// if this ptnode is the phantom part of a move, we ignore it.
	// all activity happens in the move-destination ptnode.

	if (ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
		return;

	// dive first so that we can still use the repo-path-->absolute-path code.

	if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		if (ptn->prbItems)
		{
			const char * pszKey;
			const struct sg_ptnode * ptnSub;

			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,&pIter,ptn->prbItems,&bFound,&pszKey,(void **)&ptnSub)  );
			while (bFound)
			{
				SG_ERR_CHECK(  _pt_revert__move_to_parking_lot(pCtx,prd,ptnSub,depth+1)  );
				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bFound,&pszKey,(void **)&ptnSub)  );
			}
			SG_RBTREE_ITERATOR_NULLFREE(pCtx,pIter);
		}
	}

	if (ptn->pszgid)			// the super-root doesn't have a gid.
	{
		SG_inv_entry * pInvEntry = NULL;
		const SG_treediff2_ObjectData * pOD = NULL;
		const SG_pathname * pPathParked = NULL;		// we do not own this
		SG_inv_entry_flags inv_entry_flags;

		SG_ERR_CHECK(  SG_inv_dirs__fetch_source_binding(pCtx,prd->pInvDirs,ptn->pszgid,&bFound,&pInvEntry,(void **)&pOD)  );
		if (bFound)
			SG_ERR_CHECK(  SG_inv_entry__get_parked_path__ref(pCtx, pInvEntry, &pPathParked)  );
		if (pPathParked)
		{
			const char * pszRepoPath;

			SG_ASSERT(  (ptn->pCurrentParent)  );		// actual root @/ cannot be moved/renamed and so should never need swapping
			SG_ASSERT(  (pOD)  );

			SG_ERR_CHECK(  SG_inv_entry__get_flags(pCtx, pInvEntry, &inv_entry_flags)  );
			SG_ASSERT(  (inv_entry_flags & SG_INV_ENTRY_FLAGS__PARKED__MASK)  );

			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD,&pszRepoPath)  );
			SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx,
																				 prd->pPendingTree->pPathWorkingDirectoryTop,
																				 pszRepoPath,
																				 &pPath_in_wd)  );
#if TRACE_REVERT
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
									   ("_pt_revert__move_to_parking_lot: %*c [reason %x][%s]\n"
										"                                 %*c wd   [%s]\n"
										"                                 %*c park [%s]\n"),
									   depth*4,' ',inv_entry_flags,pszRepoPath,
									   depth*4,' ',SG_pathname__sz(pPath_in_wd),
									   depth*4,' ',SG_pathname__sz(pPathParked))  );
#endif
			if (prd->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
				SG_ERR_CHECK(  _pt_log__move(pCtx,prd->pPendingTree,
											 ptn->pszgid,
											 SG_pathname__sz(pPath_in_wd),
											 SG_pathname__sz(pPathParked),
											 "PARK_FOR_SWAP")  );
			if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
				SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx,pPath_in_wd,pPathParked)  );

			SG_PATHNAME_NULLFREE(pCtx,pPath_in_wd);
		}
	}

	return;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx,pIter);
	SG_PATHNAME_NULLFREE(pCtx,pPath_in_wd);
}

static void _pt_revert__undelete_skeleton(SG_context * pCtx, struct _pt_revert_data * prd, const struct sg_ptnode * ptn, SG_uint32 depth)
{
	//////////////////////////////////////////////////////////////////
	// TODO 3/25/10 Investigate if it would be quicker to use the linear
	// TODO 3/25/10 treediff and build a queue of needed skeleton dirs
	// TODO 3/25/10 and then create them in sorted order.  Like the UPDATE
	// TODO 3/25/10 code does.  See _pt_update__do__full_{6,6_1,6_2}().
	//////////////////////////////////////////////////////////////////

	SG_rbtree_iterator * pIter = NULL;
	SG_pathname * pPathTrans = NULL;
	struct sg_ptnode * ptnDesiredParent = NULL;
	SG_bool bFound;

	SG_ASSERT(  (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)  );

	// if this ptnode is the phantom part of a move, we ignore it.
	// all activity happens in the move-destination ptnode.

	if (ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
		return;

	if (ptn->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
	{
		// this node is either DELETED or LOST.  and in the case of LOST it could
		// be LOST, LOST+MOVED, LOST+RENAMED, or LOST+MOVED+RENAMED.

		SG_bool bActive = ((ptn->temp_flags & sg_PTNODE_TEMP_FLAG_REVERTING) != 0);

		if (bActive)
		{
			const char * pszEntryName_old;

			// get the current disk path of the DESIRED parent directory.  (because we are after the swap
			// parking and before the restore, we can't use the normal repo-path-->absolute-path code
			// because an ancestor might be parked in swap.

			if (ptn->pszgidMovedFrom)
			{
				// pending-tree allows LOST+MOVE.  (currently, pending-tree does not allow DELETE+MOVE,
				// but that doesn't matter for our purposes here.)  but i'm going to assert that we have
				// a LOST for now.

				SG_ASSERT(  (ptn->temp_flags & sg_PTNODE_TEMP_FLAG_IMPLICIT_DELETE)  );

				// we have a +MOVE, so we need choose the TARGET parent directory where we were moved from.
				// This is a little more lookup than the normal case.

				SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx,prd->pPendingTree,ptn->pszgidMovedFrom,&ptnDesiredParent)  );
			}
			else
			{
				// otherwise the SOURCE parent and the TARGET parent are the same (not MOVED).

				ptnDesiredParent = ptn->pCurrentParent;
			}

			SG_ERR_CHECK(  _pt_revert__get_transient_path(pCtx,prd,ptnDesiredParent,&pPathTrans)  );

#if defined(DEBUG)
			if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
			{
				// if all is right, our DESIRED parent directory will already exist.
				SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathTrans,&bFound,NULL,NULL)  );
				SG_ASSERT(  (bFound)  );
			}
#endif

			// because we were not present in the SOURCE version of the tree, we did not claim a
			// SOURCE-entryname and thus cannot be parked in swap.  furthermore, since we claimed the
			// TARGET-entryname, we own it cleanly and can create the directory without having to
			// worry about backup names or collisions.
			//
			// we fetch the old-name because we want the TARGET version.  for DELETES, this should be
			// the same as the SOURCE version (because we don't allow DELETE+RENAME); for LOSTS, they
			// could be different, but that doesn't matter.

			SG_ERR_CHECK(  sg_ptnode__get_old_name(pCtx, ptn, &pszEntryName_old)  );
			SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPathTrans, pszEntryName_old)  );

#if TRACE_REVERT
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
									   ("_pt_revert__undelete_skeleton: %*c [%s]\n"),
									   depth*4,' ',SG_pathname__sz(pPathTrans))  );
#endif
			if (prd->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
				SG_ERR_CHECK(  _pt_log__mkdir(pCtx,prd->pPendingTree,
											  ptn->pszgid,
											  SG_pathname__sz(pPathTrans),
											  "SKELETON")  );
			if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
			{
				// TODO this can throw SG_ERR_DIR_ALREADY_EXISTS -- this should not happen because
				// TODO we already determined that it was missing (hence the DELETED or LOST status),
				// TODO so i'm going to just let this throw.
				//
				// TODO we might get this if the portability-warnings are turned off/ignored, maybe?
				//
				// TODO do we need to call SG_pathname__remove_final_slash() on this?
				SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathTrans)  );

				// NOTE: We do not bother to restore attrbits and xattrs on this directory at this point.
				// NOTE: Nor reset the _FLAG_DELETED bit on the ptnode.
				// NOTE: We save these for _pt_revert__process_structural_change() to deal with so that we
				// NOTE: can maybe use the same code as we do for un-deleting files/symlinks.
			}

			SG_PATHNAME_NULLFREE(pCtx, pPathTrans);
		}
	}

	// TODO do i need to call sg_ptnode__load_dir_entries()?  it was called in sg_ptnode__get()
	// TODO that i was looking at as i wrote this, but i don't think it is necessary since we've
	// TODO already done the treediff.  so if ptn->prbItems is NULL and you don't think it should
	// TODO be, then maybe we do.

	if (ptn->prbItems)
	{
		const char * pszKey;
		const struct sg_ptnode * ptnSub;

		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,&pIter,ptn->prbItems,&bFound,&pszKey,(void **)&ptnSub)  );
		while (bFound)
		{
			if (ptnSub->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
				SG_ERR_CHECK(  _pt_revert__undelete_skeleton(pCtx,prd,ptnSub,depth+1)  );
			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bFound,&pszKey,(void **)&ptnSub)  );
		}
		SG_RBTREE_ITERATOR_NULLFREE(pCtx,pIter);
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx,pIter);
	SG_PATHNAME_NULLFREE(pCtx, pPathTrans);
}

//////////////////////////////////////////////////////////////////

static void _pt_revert__revert_attrbits(SG_context * pCtx,
										struct _pt_revert_data * prd,
										const SG_pathname * pPathAbsolute,
										struct sg_ptnode * ptn,
										const char * pszReason)
{
	SG_int64 attrBitsBaseline;

	SG_ERR_CHECK(  SG_treenode_entry__get_attribute_bits(pCtx,ptn->pBaselineEntry,&attrBitsBaseline)  );

#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_revert__revert_attrbits: setting ATTRBITS to [0x%x]\n"
								"                                             for %s\n"),
							   ((SG_uint32)attrBitsBaseline),
							   SG_pathname__sz(pPathAbsolute))  );
#endif
	if (prd->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
		SG_ERR_CHECK(  _pt_log__attrbits(pCtx,prd->pPendingTree,
										 ptn->pszgid,
										 SG_pathname__sz(pPathAbsolute),
										 attrBitsBaseline,
										 pszReason)  );
	if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		SG_ERR_CHECK(  SG_attributes__bits__apply(pCtx,pPathAbsolute,attrBitsBaseline)  );

	ptn->iCurrentAttributeBits = attrBitsBaseline;

fail:
	return;
}

static void _pt_revert__revert_xattrs(SG_context * pCtx,
									  struct _pt_revert_data * prd,
									  const SG_pathname * pPathAbsolute,
									  struct sg_ptnode * ptn,
									  const char * pszReason)
{
	SG_vhash * pvhBaselineXAttrs = NULL;
	const char * pszHidBaselineXAttrs;

    SG_ERR_CHECK(  SG_treenode_entry__get_hid_xattrs(pCtx, ptn->pBaselineEntry, &pszHidBaselineXAttrs)  );

#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_revert__revert_xattrs: settings XATTRs to [hid %s]\n"
								"                                          for %s\n"),
							   ((pszHidBaselineXAttrs) ? pszHidBaselineXAttrs : "(null)"),
							   SG_pathname__sz(pPathAbsolute))  );
#endif

    if (pszHidBaselineXAttrs)
        SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, prd->pPendingTree->pRepo,pszHidBaselineXAttrs,&pvhBaselineXAttrs)  );

	if (prd->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
		SG_ERR_CHECK(  _pt_log__xattrs(pCtx,prd->pPendingTree,
									   ptn->pszgid,
									   SG_pathname__sz(pPathAbsolute),
									   pszHidBaselineXAttrs,
									   pvhBaselineXAttrs,
									   pszReason)  );
#ifdef SG_BUILD_FLAG_FEATURE_XATTR
	if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		SG_ERR_CHECK(  SG_attributes__xattrs__apply(pCtx, pPathAbsolute, pvhBaselineXAttrs, prd->pPendingTree->pRepo)  );
#else
	// TODO 3/29/10 What to do on NON-XATTR platforms?
#endif

	if (pszHidBaselineXAttrs)
		SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, prd->pPendingTree->pStrPool, pszHidBaselineXAttrs, &ptn->pszHidXattrs)  );
	else
		ptn->pszHidXattrs = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvhBaselineXAttrs);
}

//////////////////////////////////////////////////////////////////

/**
 * the contents of the entry were not changes.  we may or may not have some XATTR and/or ATTRBITS
 * changes to restore.
 */
static void _pt_revert__revert_unmodified(SG_context * pCtx,
										  struct _pt_revert_data * prd,
										  SG_UNUSED_PARAM( const char * pszGidObject ),
										  SG_UNUSED_PARAM( const SG_treediff2_ObjectData * pOD ),
										  SG_diffstatus_flags dsFlags,
										  struct sg_ptnode * ptn)
{
	SG_string * pStringRepoPath = NULL;
	SG_pathname * pPathAbsolute = NULL;

	SG_UNUSED( pszGidObject );
	SG_UNUSED( pOD );

	if ((dsFlags & (SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)) == 0)
		return;

	// see note about needing to use sg_ptnode__get_repo_path() rather than SG_treediff2__.

	SG_ERR_CHECK(  sg_ptnode__get_repo_path(pCtx,ptn,&pStringRepoPath)  );
	SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx,
																		 prd->pPendingTree->pPathWorkingDirectoryTop,
																		 SG_string__sz(pStringRepoPath),
																		 &pPathAbsolute)  );

	if (dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)
		SG_ERR_CHECK(  _pt_revert__revert_attrbits(pCtx,prd,pPathAbsolute,ptn,"UNDO_CHANGE")  );

	if (dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)
		SG_ERR_CHECK(  _pt_revert__revert_xattrs(pCtx,prd,pPathAbsolute,ptn,"UNDO_CHANGE")  );

	// TODO I'm going to argue that we don't need to update the timestamp cache for these.

fail:
	SG_STRING_NULLFREE(pCtx,pStringRepoPath);
	SG_PATHNAME_NULLFREE(pCtx,pPathAbsolute);
}

/**
 * the contents of the entry have been changed and we want to revert it.
 *
 * for files and (maybe) symlinks, we want to backup the entry before restoring the contents from
 * the repo.  (because we don't want to blindly delete the user's only copy of something that they
 * changed.)
 *
 * Note: we also get a __MODIFIED bit for directories, but this is just noise (indicating that
 * something *within* the directory changed.  We ignore the __MODIFIED bit for them.
 *
 * We also need to restore the original XATTRs and ATTRBITS.
 */
static void _pt_revert__revert_modified(SG_context * pCtx,
										struct _pt_revert_data * prd,
										const char * pszGidObject,
										const SG_treediff2_ObjectData * pOD,
										SG_diffstatus_flags dsFlags,
										struct sg_ptnode * ptn)
{
	SG_string * pStringRepoPath = NULL;
	SG_pathname * pPathAbsolute = NULL;
	SG_pathname * pPathBackup = NULL;
	SG_file * pFile = NULL;
	SG_string * pStringLink = NULL;
	SG_byte * pBytes = NULL;
	const char * pszHid;

	if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		SG_ERR_CHECK(  _pt_revert__revert_unmodified(pCtx,prd,pszGidObject,pOD,dsFlags,ptn)  );

		ptn->pszCurrentHid = NULL;

		return;
	}

	// see note about needing to use sg_ptnode__get_repo_path() rather than SG_treediff2__.

	SG_ERR_CHECK(  sg_ptnode__get_repo_path(pCtx,ptn,&pStringRepoPath)  );
	SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx,
																		 prd->pPendingTree->pPathWorkingDirectoryTop,
																		 SG_string__sz(pStringRepoPath),
																		 &pPathAbsolute)  );

	// backup the dirty file/symlink before restore.

	SG_ERR_CHECK(  sg_pendingtree__calc_backup_name(pCtx,pPathAbsolute,&pPathBackup)  );

#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_revert__revert_modified: repo-path [%s]\n"
								"                            backing-up [%s]\n"
								"                                    as [%s]\n"),
							   SG_string__sz(pStringRepoPath),
							   SG_pathname__sz(pPathAbsolute),
							   SG_pathname__sz(pPathBackup))  );
#endif
	if (prd->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
		SG_ERR_CHECK(  _pt_log__move(pCtx,prd->pPendingTree,
									 ptn->pszgid,
									 SG_pathname__sz(pPathAbsolute),
									 SG_pathname__sz(pPathBackup),
									 "BACKUP_BEFORE_RESTORE")  );
	if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx,pPathAbsolute,pPathBackup)  );

	// restore the file/symlink from the repo.

	SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, ptn->pBaselineEntry, &pszHid)  );

#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_revert__revert_modified: restoring [gid %s]\n"
								"                                       [hid %s]\n"
								"                                    to %s\n"),
							   pszGidObject,pszHid,SG_pathname__sz(pPathAbsolute))  );
#endif

	if (ptn->type == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
	{
		if (prd->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
			SG_ERR_CHECK(  _pt_log__restore_file(pCtx,prd->pPendingTree,
												 ptn->pszgid,
												 SG_pathname__sz(pPathAbsolute),
												 pszHid,
												 "RESTORE")  );
		if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		{
			SG_fsobj_stat stat_now_current;

			SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathAbsolute, SG_FILE_WRONLY | SG_FILE_CREATE_NEW, SG_FSOBJ_PERMS__MASK, &pFile)  );
			SG_ERR_CHECK(  SG_repo__fetch_blob_into_file(pCtx, ptn->pPendingTree->pRepo, pszHid, pFile, NULL)  );
			SG_FILE_NULLCLOSE(pCtx, pFile);

			// update the timestamp cache for the restored file.

			SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPathAbsolute, &stat_now_current)  );
			SG_ERR_CHECK(  SG_pendingtree__set_wd_file_timestamp__dont_save_pendingtree(pCtx,
																					ptn->pPendingTree,
																					ptn->pszgid,
																					stat_now_current.mtime_ms)  );
		}

		ptn->pszCurrentHid = NULL;
	}
	else
	{
		SG_uint64 iLenBytes = 0;

		SG_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, ptn->pPendingTree->pRepo, pszHid, &pBytes, &iLenBytes)  );
		SG_ERR_CHECK(  SG_STRING__ALLOC__BUF_LEN(pCtx, &pStringLink, pBytes, (SG_uint32) iLenBytes)  );
		SG_NULLFREE(pCtx, pBytes);

		if (prd->pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)
			SG_ERR_CHECK(  _pt_log__restore_symlink(pCtx,prd->pPendingTree,
													ptn->pszgid,
													SG_pathname__sz(pPathAbsolute),
													SG_string__sz(pStringLink),
													"RESTORE")  );
		if (prd->pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
			SG_ERR_CHECK(  SG_fsobj__symlink(pCtx, pStringLink, pPathAbsolute)  );

		ptn->pszCurrentHid = NULL;

		SG_STRING_NULLFREE(pCtx, pStringLink);
	}

	// since we just create the entry on the disk, we always apply the ATTRBITS and XATTRS as
	// they were recorded in the repo.

	SG_ERR_CHECK(  _pt_revert__revert_attrbits(pCtx,prd,pPathAbsolute,ptn,"RESTORE")  );

	SG_ERR_CHECK(  _pt_revert__revert_xattrs(pCtx,prd,pPathAbsolute,ptn,"RESTORE")  );

fail:
	SG_STRING_NULLFREE(pCtx, pStringRepoPath);
	SG_PATHNAME_NULLFREE(pCtx, pPathAbsolute);
	SG_PATHNAME_NULLFREE(pCtx, pPathBackup);
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_STRING_NULLFREE(pCtx, pStringLink);
	SG_NULLFREE(pCtx, pBytes);
}

//////////////////////////////////////////////////////////////////

static SG_vector_foreach_callback _pt_revert__process_cyclic_directory_moves;

static void _pt_revert__process_cyclic_directory_moves(SG_context * pCtx, SG_UNUSED_PARAM( SG_uint32 ndx ), void * pVoidAssoc, void * pVoidData)
{
	struct sg_ptnode * ptn = (struct sg_ptnode *)pVoidAssoc;
	struct _pt_revert_data * prd = (struct _pt_revert_data *)pVoidData;

	SG_UNUSED( ndx );

	SG_ASSERT(  (ptn->temp_flags & sg_PTNODE_TEMP_FLAG_PARKED_FOR_CYCLE)  );

#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_pt_revert__process_cyclic_directory_moves: handling deferred revert for [gid %s]\n",
							   ptn->pszgid)  );
#endif

	SG_ERR_CHECK(  _pt_revert__revert_moved__apply(pCtx,prd,ptn)  );

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * Process any non-structural changes on the entry.  This includes restoring modified files/symlinks
 * and any changes to XATTR or ATTRBITS.
 *
 * We have to be careful with the repo-path here.   Because we have already handled the MOVES/RENAMES/etc,
 * the entry now has its ORIGINAL entryname and is in its ORIGINAL directory in the WD on disk.  so we
 * cannot use the SG_treediff2__ObjectData__get_repo_path() because it computes values based upon the
 * CURRENT settings.  Use the PTNODE version because (in theory) the PTNODES have been updated during the
 * earlier steps in the REVERT.  (The TreeDiff has not.)
 */
static SG_treediff2_foreach_callback _pt_revert__process_nonstructural_changes;

static void _pt_revert__process_nonstructural_changes(SG_context * pCtx,
													  SG_UNUSED_PARAM( SG_treediff2 * pTreeDiff ),
													  const char * pszGidObject,
													  const SG_treediff2_ObjectData * pOD,
													  void * pVoidData)
{
	struct _pt_revert_data * prd = (struct _pt_revert_data *)pVoidData;
	struct sg_ptnode * ptn = NULL;
	SG_diffstatus_flags dsFlags;

	SG_UNUSED( pTreeDiff );

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx,pOD,&dsFlags)  );
	switch (dsFlags & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE)
	{
	case SG_DIFFSTATUS_FLAGS__ADDED:		// TODO XREF[B]: ptn may no longer exist (see XREF[A])
	case SG_DIFFSTATUS_FLAGS__DELETED:
	case SG_DIFFSTATUS_FLAGS__FOUND:
	case SG_DIFFSTATUS_FLAGS__LOST:
		SG_ASSERT(  ((dsFlags & (SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)) == 0)  );
		return;

	default:
		break;
	}

	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx,prd->pPendingTree,pszGidObject,&ptn)  );

#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "ProcessContentChanges: [gid %s][dsFlags 0x%x][ptn s flags %d][ptn t flags %d]\n",
							   pszGidObject,dsFlags,ptn->saved_flags,ptn->temp_flags)  );
#endif

	if (!(ptn->temp_flags & sg_PTNODE_TEMP_FLAG_REVERTING))
		return;

	if (dsFlags & SG_DIFFSTATUS_FLAGS__MODIFIED)
		SG_ERR_CHECK(  _pt_revert__revert_modified(pCtx,prd,pszGidObject,pOD,dsFlags,ptn)  );
	else
		SG_ERR_CHECK(  _pt_revert__revert_unmodified(pCtx,prd,pszGidObject,pOD,dsFlags,ptn)  );

fail:
	return;
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__revert(SG_context* pCtx,
							SG_pendingtree* pPendingTree,
							SG_pendingtree_action_log_enum eActionLog,
							const SG_pathname* pPathRelativeTo,
							SG_uint32 count_items, const char** paszItems,
							SG_bool bRecursive,
							const char* const* paszIncludes, SG_uint32 count_includes,
							const char* const* paszExcludes, SG_uint32 count_excludes)
{
	// We DO NOT need/use the IGNORES list because we are not scanning.

	struct _pt_revert_data rd;
	SG_uint32 count_changes;
	SG_bool bDirty;
	SG_uint32 nrParents;
	const SG_varray * pva_wd_parents = NULL;
	SG_uint32 nrParked;

	memset(&rd,0,sizeof(rd));

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_ARGCHECK_RETURN(  ((   count_items==0)   || (   paszItems != NULL)), count_items     );
	SG_ARGCHECK_RETURN(  ((count_includes==0)   || (paszIncludes != NULL)), count_includes  );
	SG_ARGCHECK_RETURN(  ((count_excludes==0)   || (paszExcludes != NULL)), count_excludes  );
	SG_ARGCHECK_RETURN(  ((bRecursive==SG_TRUE) || (count_items > 0)),      bRecursive      );	// if not recursive, require at least one item.

#if TRACE_REVERT
	{
		SG_uint32 k;

		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   ("SG_pendingtree__revert:\n"
									"\t[eActionLog %d]\n"
									"\t[pPathRelativeTo %s]\n"
									"\t[count_items %d]\n"),
								   eActionLog,
								   ((pPathRelativeTo) ? SG_pathname__sz(pPathRelativeTo) : "(null)"),
								   count_items)  );
		for (k=0; k<count_items; k++)
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
									   ("\t\titem[%d]: %s\n"),
									   k,paszItems[k])  );
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   ("\t[bRecursive %d]\n"
									"\t[count_includes %d]\n"),
								   bRecursive,
								   count_includes)  );
		for (k=0; k<count_includes; k++)
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
									   ("\t\tinclude[%d]: %s\n"),
									   k,paszIncludes[k])  );
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   ("\t[count_excludes %d]\n"),
								   count_excludes)  );
		for (k=0; k<count_excludes; k++)
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
									   ("\t\texclude[%d]: %s\n"),
									   k,paszExcludes[k])  );
	}
#endif

	SG_ERR_CHECK_RETURN(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pva_wd_parents)  );
	SG_ERR_CHECK_RETURN(  SG_varray__count(pCtx, pva_wd_parents, &nrParents)  );
	SG_ASSERT(  (nrParents >= 1)  );

	// for now we disallow partial-reverts when we have a merge.  there are
	// various reasons to allow this and to disallow this.  for example, what
	// does it mean?  for an individual file that was auto-merged and then
	// edited by the user, do we revert it to the auto-merge-result or to the
	// version in one of the input csets and if so, which one?

	if (nrParents > 1)
	{
		SG_bool bHaveArgs = ((count_items > 0) || (count_includes > 0) || (count_excludes > 0) || (!bRecursive));
		if (bHaveArgs)
			SG_ERR_THROW_RETURN(  SG_ERR_CANNOT_PARTIAL_REVERT_AFTER_MERGE  );
	}


	rd.pPendingTree = pPendingTree;

	// eActionLog indicates whether we should actually do the work, just log what
	// should be done, or both.  We set it here for the body of the REVERT.  We
	// don't really anticipate it being changed/set differently by a series of
	// commands.  That is, the log should either be a complete list of what SHOULD
	// be done -OR- a complete list of what we DID do, but not a mingling of the 2.
	SG_ASSERT(  (rd.pPendingTree->eActionLog == SG_PT_ACTION__ZERO) || (rd.pPendingTree->eActionLog == eActionLog)  );
	rd.pPendingTree->eActionLog = eActionLog;

	SG_ERR_CHECK(  SG_INV_DIRS__ALLOC(pCtx, pPendingTree->pPathWorkingDirectoryTop, "revert", &rd.pInvDirs)  );
	// defer allocation of rd.pVecCycles until we need it.

	// Compute diff between the baseline and the WD.  this lets us know if the WD is clean or dirty.
	// The BASELINE will be on the left and the WD (baseline+dirt) will be on the right.  REVERT is
	// then a {complete, partial} UNDO.
	//
	// We DO NOT use the __file_spec_filter on this treediff; we want the complete diff.  The --include
	// --exclude stuff gets handled by __mark_all / __mark_some later.  (handwave)

	SG_ERR_CHECK(  _sg_pendingtree__do_diff__unload_clean_objects(pCtx,pPendingTree, NULL, NULL, &rd.pTreeDiff)  );

	SG_ERR_CHECK(  SG_treediff2__count_changes(pCtx,rd.pTreeDiff,&count_changes)  );
	if (count_changes == 0)
		goto done;

#if TRACE_REVERT
	{
		SG_vhash * pvh = NULL;

		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "SG_pendingtree__revert: [count_changes %d] [wd %s]\n",
								   count_changes,
								   SG_pathname__sz(pPendingTree->pPathWorkingDirectoryTop))  );
		SG_ERR_IGNORE(  SG_treediff2__report_status_to_console(pCtx,rd.pTreeDiff,SG_FALSE)  );
		SG_ERR_IGNORE(  SG_treediff2_debug__dump_to_console(pCtx,rd.pTreeDiff,SG_FALSE)  );
		pPendingTree->bDebugExport = SG_TRUE;
		SG_ERR_IGNORE(  sg_pendingtree__to_vhash(pCtx,pPendingTree,&pvh)  );
		pPendingTree->bDebugExport = SG_FALSE;
		SG_ERR_IGNORE(  SG_vhash_debug__dump_to_console(pCtx,pvh)  );
		SG_VHASH_NULLFREE(pCtx,pvh);
	}
#endif

	if (count_items > 0)
	{
		SG_ERR_CHECK(  sg_ptnode__maybe_mark_items_for_revert(pCtx,
															  pPendingTree,
															  pPathRelativeTo,
															  count_items, paszItems,
															  bRecursive,
															  paszIncludes, count_includes,
															  paszExcludes, count_excludes)  );

		/* check the tree to see if the user is trying to revert only
		 * the target half of a move operation.  If so, fail.  We
		 * cannot allow this case because it can result in a situation
		 * where the same object (identified by its gid) exists in two
		 * places in the tree at the same time. */

		// TODO Jeff says: 3/5/10 Is this _split_moves stuff still needed?
		// TODO            I mean, how could they have requested only half
		// TODO            of the move?  The phantom node is not addressable,
		// TODO            right?

		SG_ERR_CHECK(  sg_ptnode__fail_if_any_split_moves(pCtx, pPendingTree->pSuperRoot, sg_PTNODE_TEMP_FLAG_REVERTING)  );
	}
	else					// revert everything
	{
		SG_ERR_CHECK(  sg_ptnode__maybe_mark_ptnode_for_revert__recursive(pCtx,
																		  pPendingTree->pSuperRoot,
																		  bRecursive,
																		  paszIncludes, count_includes,
																		  paszExcludes, count_excludes)  );
	}

	//////////////////////////////////////////////////////////////////
	// We process the structural differences in PASSES.  We have to do this because there
	// may be order-dependencies that we don't know (and don't want to know) about.
	// So we do things in passes so that we can eliminate the dependencies.
	//
	// For example, if they did something like:
	//     vv mv foo bar
	//     vv mv xxx foo
	// then we might have a problem depending the order that we choose
	// to revert them.  We don't know the order that the user performed the operations
	// on disk originally; all we know is the pendingtree/treediff of everything that
	// *is currently* different from the baseline.  Also, because our rbtrees are
	// keyed by gid-object (which are quite random), we might get different results
	// each time we are run.
	//
	// [1] Scan each difference and build an "inverted tree" that maps
	//         (directory-gid-object, entryname) --> { entry-gid-object_source, entry-gid-object_target }
	//         Based upon the type of difference in the treediff, claim the (directory-gid-object, entryname)
	//         in the SOURCE or TARGET version of the tree.
	//
	//         NOTE: Terminology: For REVERT, we call the current/dirty version the "SOURCE"
	//               and the desired clean/baseline version the "TARGET".
	//
	//         NOTE: The SG_treediff2 is a flat (1-level, non-hierarchial) list of changes.
	//               The "inverted tree (_inv_dirs, _inv_dir, _inv_entry)" is a flat (2-level,
	//               {dir, entry}) view of the structural changes.
	//               The pendingtree (SG_pendingtree, sg_ptnode) is a hierarchial view of the
	//               "interesting" portion of the tree (as it currently exists (SOURCE)).

	rd.pass = _PT_REVERT_PASS__SCAN_AND_BIND;
	SG_ERR_CHECK(  SG_treediff2__foreach(pCtx,rd.pTreeDiff,_pt_revert__process_structural_changes,&rd)  );

#if 0 && TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"between [1] and [2]\n")  );
	SG_ERR_IGNORE(  SG_inv_dirs_debug__dump_to_console(pCtx,rd.pInvDirs)  );
#endif

	// [2] Identify entrynames that are claimed by different entries in the 2 trees and create a "swap"
	//         pathname in the parking lot.
	//
	//         If there is a conflict that cannot be resolved, stop now.  This can happen, for example,
	//         during a PARTIAL REVERT if we are requested to undo foo-->bar but not xxx-->foo because
	//         there would be 2 foo's in the directory.  TODO see TODO labeled [4] in __check_for_swap_cb.

	SG_ERR_CHECK(  SG_inv_dirs__check_for_swaps(pCtx,rd.pInvDirs)  );

	// [3] See if there are any other types of conflicts due to a PARTIAL REVERT.  For example,
	//     if they did:
	//         vv mv @/dir1/dir2/dir3/file1 @/file1
	//         vv rm @/dir1
	//     and they only want to revert the mv and not the (recursive) rmdir.  Then we won't have
	//     the "dir3" directory in the WD to un-move "file1".
	//
	//     If there is a conflict such as the one above that cannot be resolved, stop now.

	rd.pass = _PT_REVERT_PASS__CHECK_FOR_CONFLICTS;
	SG_ERR_CHECK(  SG_treediff2__foreach(pCtx,rd.pTreeDiff,_pt_revert__process_structural_changes,&rd)  );

	// [4] If we need to check for potential collisions because of portability issues, do it now
	//     *before* we have made any changes to the WD.
	//
	//     For example, if they did something like:
	//         vv mv foo bar
	//         vv mv xxx FOO
	//     and then only want to revert foo --> bar
	//     then the REVERT will have a problem on a case-insensitive filesystem.
	//
	//     BUT the bigger problem is that suppose we are on Linux and they then COMMIT after the
	//     partial REVERT, then Windows and Mac users will have problems when they get ready to
	//     populate their WD.
	//
	//     If the first mv had never been done, the second mv should have given them a portability
	//     warning.  Our goal here is to see that they get that warning on the partial REVERT.
	//
	//     There are other cases that don't involve a parital REVERT (using __FOUND entries, for
	//     example).

	if (!rd.pPendingTree->bIgnoreWarnings)
	{
		SG_portability_flags flags_logged;

		SG_ERR_CHECK(  SG_inv_dirs__check_for_portability(pCtx,rd.pInvDirs,rd.pPendingTree)  );
		SG_ERR_CHECK(  SG_inv_dirs__get_portability_warnings_observed(pCtx, rd.pInvDirs, &flags_logged)  );
		if (flags_logged)
			SG_ERR_THROW(  SG_ERR_PORTABILITY_WARNINGS  );
	}
	else
	{
		// TODO (MAYBE) if we were told to ignore portability warnings and the user does a
		// TODO (MAYBE) partial REVERT (or has FOUND files) we could get the case where we
		// TODO (MAYBE) need to populate a directory with "README" and "ReadMe".  Nothing in
		// TODO (MAYBE) our prelude here will complain about it.  If we are on a Windows or
		// TODO (MAYBE) MAC system, we won't find out about it UNTIL we are part way thru
		// TODO (MAYBE) undoing their WD -- AND THEN THE FILESYSTEM WILL GIVE US A HARD ERROR.
		// TODO (MAYBE)
		// TODO (MAYBE) So:
		// TODO (MAYBE) [1] we'll crap out half way thru and leave them a mess.
		// TODO (MAYBE) [2] or worse we'll overwrite one with the other.
		// TODO (MAYBE) [3] our "--test" result will be incomplete.
		// TODO (MAYBE)
		// TODO (MAYBE) Do we want to secretly run the portability code and then look at
		// TODO (MAYBE) collisions and see if they would ACTUALLY be a problem on the
		// TODO (MAYBE) WD's filesystem?  Or something similar?
	}

	//////////////////////////////////////////////////////////////////
	// at this point we start making changes to the WD.
	//////////////////////////////////////////////////////////////////

	// TODO 2010/05/20 This function is too big and it is misleading.  Each of
	// TODO            functions that we call in [5]..[9] all have to run when
	// TODO            either __DO_IT or __LOG_IT -- even though it looks like
	// TODO            we are always going to make changes.  Convert the way we
	// TODO            build the log to use SG_wd_plan to build a plan and then
	// TODO            optionally execute it -- rather than intertwining them
	// TODO            as I have here.  Convert caller to expect a WD_Plan rather
	// TODO            than this LOG format.

	// [5] Use the hierarchial view to do a depth-first walk and move the entries that have a "parked/swap"
	//         pathname to their new/temporary location.  We have to do this depth-first because the
	//         repo-path --> absolute-path stuff is temporarily invalid while we have things are parked.

	SG_ERR_CHECK(  SG_inv_dirs__get_parking_stats(pCtx, rd.pInvDirs, &nrParked, NULL, NULL)  );
	if (nrParked > 0)
		SG_ERR_CHECK(  _pt_revert__move_to_parking_lot(pCtx,&rd,rd.pPendingTree->pSuperRoot,0)  );

#if TRACE_REVERT
	if (rd.pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
	{
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"after __move_to_parking_lot: [nrParked %d]\n",nrParked)  );
		SG_ERR_IGNORE(  SG_inv_dirs_debug__dump_to_console(pCtx,rd.pInvDirs)  );
	}
#endif

	// [6] Use the hierarchial view to identify the directories that were deleted/lost and re-build
	//         the skeleton of WD.  We need to do this first in case there are things which were moved
	//         out of a directory before the directory was deleted.  This pass should just re-create
	//         the directories; we'll worry about attrbits/xattrs/etc later.
	//         We create the directories either in the WD-absolute-path or a transient-path under
	//         a parent in parked in swap.

	SG_ERR_CHECK(  _pt_revert__undelete_skeleton(pCtx,&rd,rd.pPendingTree->pSuperRoot,0)  );

	// [7] Use the treediff view to actually perform the structural changes.  This will reshape
	//         the WD by UNDO-ing all of the RENAMES, MOVES, DELETES, and ADDS.  This will operate
	//         within the WD and pull everything that was parked back into the tree.  We do this
	//         in 2 parts:
	//         [7a] RENAMES, MOVES (excluding cyclic directory MOVES), DELETES, and ADDS.
	//         [7b] cyclic directory MOVES (because the tree is kinda broken during this).
	//         In theory, when this step is finished, the parking lot should be be empty.

#if TRACE_REVERT
	if (rd.pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
	{
		SG_vhash * pvh = NULL;

		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "SG_pendingtree__revert: after undelete_skeleton and before apply_structural_changes\n")  );
		pPendingTree->bDebugExport = SG_TRUE;
		SG_ERR_IGNORE(  sg_pendingtree__to_vhash(pCtx,pPendingTree,&pvh)  );
		pPendingTree->bDebugExport = SG_FALSE;
		SG_ERR_IGNORE(  SG_vhash_debug__dump_to_console(pCtx,pvh)  );
		SG_VHASH_NULLFREE(pCtx,pvh);
	}
#endif

	rd.pass = _PT_REVERT_PASS__APPLY_STRUCTURAL_CHANGES;
	SG_ERR_CHECK(  SG_treediff2__foreach(pCtx,rd.pTreeDiff,_pt_revert__process_structural_changes,&rd)  );

	if (rd.pVecCycles)
		SG_ERR_CHECK(  SG_vector__foreach(pCtx,rd.pVecCycles,_pt_revert__process_cyclic_directory_moves,&rd)  );

#if defined(DEBUG)
	if (rd.pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
		SG_ERR_IGNORE(  SG_inv_dirs_debug__verify_parking_lot_empty(pCtx,rd.pInvDirs)  );
#endif

	// [8] Use the treediff view to UNDO changes *to* entries.  That is, we fetch the original
	//         contents of MODIFIED files and symlinks.  We also UNDO any XATTR or ATTRBIT changes
	//         that were made.

	SG_ERR_CHECK(  SG_treediff2__foreach(pCtx,rd.pTreeDiff,_pt_revert__process_nonstructural_changes,&rd)  );

done:
	// [9] All is done.  if __DO_IT, we need to cleanup the pendingtree
	//         in memory and save it to disk, so that it matches any
	//         remaining dirt in the working directory.  if not __DO_IT
	//         (and we only did a __LOG_IT), then we didn't modify the
	//         working directory, but have probably trashed the in
	//         memory pendingtree, so invalidate as much of it as we
	//         can without actually deleting the pendingtree pointer.

	if (rd.pPendingTree->eActionLog & SG_PT_ACTION__DO_IT)
	{
		/* we need to undo all the IMPLICIT entries now.  if we don't,
		 * this revert function will accidentally perform an addremove
		 * function. */

		SG_ERR_CHECK(  sg_ptnode__undo_implicit(pCtx, pPendingTree->pSuperRoot)  );
		SG_ERR_CHECK(  sg_ptnode__unload_dir_entries_if_not_dirty(pCtx, pPendingTree->pSuperRoot, &bDirty)  );

		if (!(pPendingTree->pSuperRoot->prbItems))
			SG_PTNODE_NULLFREE(pCtx, pPendingTree->pSuperRoot);

		if (nrParents > 1)
		{
			const char * psz_hid_parent_0;

			// We were looking at an "uncommitted merge".
			//
			// TODO 2010/05/24 Since we currently only allow "REVERT --ALL" after a MERGE,
			// TODO            we should always have an empty pendingtree *and* we should
			// TODO            reset the parents and clear the list of issues.  Later, when
			// TODO            we do allow PARTIAL REVERTS, we'll need to figure out which
			// TODO            or all of the parents to keep and which of the issues to
			// TODO            "CANCEL" (which is stronger than a "MARK RESOLVED".
			//
			SG_ASSERT(  (!pPendingTree->pSuperRoot)  );

			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_wd_parents, 0, &psz_hid_parent_0)  );
			SG_ERR_CHECK(  SG_pendingtree__set_single_wd_parent(pCtx, pPendingTree, psz_hid_parent_0)  );

			SG_ERR_CHECK(  SG_pendingtree__clear_wd_issues(pCtx, pPendingTree)  );
		}

		SG_ERR_CHECK(  SG_pendingtree__save(pCtx, pPendingTree)  );
	}
	else
	{
		SG_ASSERT(  (rd.pPendingTree->eActionLog & SG_PT_ACTION__LOG_IT)  );

		/* we don't change the saved pending changeset here. */
		SG_PTNODE_NULLFREE(pCtx, pPendingTree->pSuperRoot);
	}

fail:
	SG_TREEDIFF2_NULLFREE(pCtx, rd.pTreeDiff);
	SG_INV_DIRS_NULLFREE(pCtx, rd.pInvDirs);
	SG_VECTOR_NULLFREE(pCtx, rd.pVecCycles);
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_PENDINGTREE__PRIVATE_REVERT_H
