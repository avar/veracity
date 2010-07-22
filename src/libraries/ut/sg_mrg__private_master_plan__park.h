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
 * @file sg_mrg__private_master_plan__park.h
 *
 * @details We are used after the MERGE result has been
 * computed.  We are the first step in the WD-PLAN to
 * actually modify the WD to reflect the result of the
 * merge.  The stuff in this file is concerned with
 * looking for transient collisions that we might encounter
 * while juggling the WD and scheduling things to be temporarily
 * placed in the parking lot.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_MASTER_PLAN__PARK_H
#define H_SG_MRG__PRIVATE_MASTER_PLAN__PARK_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

// TODO 2010/05/04 I think we can get rid of this and just pass pMrg to
// TODO            the _sg_mrg__populate_inv_dirs__{source,target}__cb()
// TODO            routines.
struct _sg_mrg__populate_inv_dirs__data
{
	SG_mrg *			pMrg;
	SG_mrg_cset *		pMrgCSet;
};

typedef struct _sg_mrg__populate_inv_dirs__data _sg_mrg__populate_inv_dirs__data;

//////////////////////////////////////////////////////////////////

/**
 * We get called once for each entry in the BASELINE.
 *
 * Populate the SOURCE side of the "inverted tree" using
 * the contents of the BASELINE.
 */
static SG_rbtree_foreach_callback _sg_mrg__populate_inv_dirs__source__cb;

static void _sg_mrg__populate_inv_dirs__source__cb(SG_context * pCtx,
												   const char * pszKey_GidEntry,
												   void * pVoidValue_MrgCSetEntry,
												   void * pVoidData)
{
	SG_mrg_cset_entry * pMrgCSetEntry = (SG_mrg_cset_entry *)pVoidValue_MrgCSetEntry;
	_sg_mrg__populate_inv_dirs__data * pData = (_sg_mrg__populate_inv_dirs__data *)pVoidData;
	SG_string * pStringRepoPath_Parent = NULL;

	SG_ASSERT(  (strcmp(pszKey_GidEntry,pMrgCSetEntry->bufGid_Entry) == 0)  );
	SG_ASSERT(  (pMrgCSetEntry->pMrgCSet == pData->pMrgCSet)  );

	if (pMrgCSetEntry->bufGid_Parent[0])
	{
		// create repo-path for *PARENT* of this entry.
		SG_ERR_CHECK(  SG_mrg_cset__compute_repo_path_for_entry(pCtx,
																pData->pMrgCSet,
																pMrgCSetEntry->bufGid_Parent,
																SG_FALSE,
																&pStringRepoPath_Parent)  );

		SG_ERR_CHECK(  SG_inv_dirs__bind_source(pCtx, pData->pMrg->pInvDirs,
												pMrgCSetEntry->bufGid_Parent, SG_string__sz(pStringRepoPath_Parent),
												pszKey_GidEntry, SG_string__sz(pMrgCSetEntry->pStringEntryname),
												(pMrgCSetEntry->tneType == SG_TREENODEENTRY_TYPE_DIRECTORY),
												(void *)pMrgCSetEntry,
												SG_TRUE,
												NULL, NULL)  );
	}
	else
	{
		// The repo-root directory "@/" does not have a parent and
		// since its entryname is outside of the scope of the repo
		// it cannot have changed, so we don't need check for collisions
		// on it.
	}

fail:
	SG_STRING_NULLFREE(pCtx, pStringRepoPath_Parent);
}

/**
 * We get called once for each entry in the FinalResult.
 *
 * Populate the TARGET side of the "inverted tree" using
 * the contents of the FinalResult.
 */
static SG_rbtree_foreach_callback _sg_mrg__populate_inv_dirs__target__cb;

static void _sg_mrg__populate_inv_dirs__target__cb(SG_context * pCtx,
												   const char * pszKey_GidEntry,
												   void * pVoidValue_MrgCSetEntry,
												   void * pVoidData)
{
	SG_mrg_cset_entry * pMrgCSetEntry = (SG_mrg_cset_entry *)pVoidValue_MrgCSetEntry;
	_sg_mrg__populate_inv_dirs__data * pData = (_sg_mrg__populate_inv_dirs__data *)pVoidData;

	SG_ASSERT(  (strcmp(pszKey_GidEntry,pMrgCSetEntry->bufGid_Entry) == 0)  );
	SG_ASSERT(  (pMrgCSetEntry->pMrgCSet == pData->pMrgCSet)  );

	SG_ERR_CHECK(  SG_inv_dirs__bind_target(pCtx, pData->pMrg->pInvDirs,
											pMrgCSetEntry->bufGid_Parent,
											pszKey_GidEntry, SG_string__sz(pMrgCSetEntry->pStringEntryname),
											(void *)pMrgCSetEntry,
											SG_TRUE,
											NULL, NULL)  );

fail:
	return;
}

/**
 * During the in-memory merge computation we identified and resolved
 * (using unique names) any name collisions (multiple entries fighting
 * for the same entryname) so we should be collision free.
 *
 * However, we may have *TRANSIENT* collisions as we re-arrange the
 * WD from the current BASELINE CSet (source) to the FinalResult
 * (target).  For example, in a simple 2-way "BASELINE vs Other CSet",
 * if the Other CSet did a cycle of renames or moves on a set of
 * files/directories that are in both branches, then we may have to
 * use the parking lot to avoid order dependencies during the
 * juggling.
 *
 * Use the existing BASELINE CSet and the in-memory FinalResult
 * and build an "inverted tree" that maps:
 *
 *     (directory-gid-object, entryname) --> { entry-gid-object_source, entry-gid-object_target }
 *     and claim:
 *     (directory-gid-object, entryname) in either the SOURCE or TARGET versions of the tree.
 *
 *     NOTE on Terminology: for UPDATE, we call the current WD the "SOURCE"
 *                          and the goal changeset the "TARGET".
 *
 *     This tells us who "owns" each entryname in each directory in each version of the tree.
 */
static void _sg_mrg__populate_inv_dirs(SG_context * pCtx, SG_mrg * pMrg)
{
	const SG_pathname * pPathWorkingDirTop;			// we do not own this
	_sg_mrg__populate_inv_dirs__data data;

	memset(&data, 0, sizeof(data));
	data.pMrg = pMrg;

	SG_ERR_CHECK(  SG_pendingtree__get_working_directory_top__ref(pCtx, pMrg->pPendingTree, &pPathWorkingDirTop)  );
	SG_ERR_CHECK(  SG_INV_DIRS__ALLOC(pCtx, pPathWorkingDirTop, "merge_parkinglot", &pMrg->pInvDirs)  );

	data.pMrgCSet = pMrg->pMrgCSet_Baseline;
	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx, pMrg->pMrgCSet_Baseline->prbEntries,
									  _sg_mrg__populate_inv_dirs__source__cb, &data)  );

	// TODO 2010/04/28 The __foreach above causes all of the entries in the BASELINE
	// TODO            to be added to the "inverted tree".  The first time that we
	// TODO            visit an entry in a directory, we will scan the disk and pre-load
	// TODO            the entryname map (and set SG_INV_ENTRY_FLAGS__SCANNED on all of
	// TODO            the entries in the directory.  The entries in the BASELINE are
	// TODO            (assumed to be ACTIVE) and the flags updated to include
	// TODO            SG_INV_ENTRY_FLAGS__ACTIVE_SOURCE.
	// TODO
	// TODO            The __foreach below claims the target side of the "inverted tree"
	// TODO            map for everything in the FinalResult CSET.
	// TODO
	// TODO            In theory, since the WD is clean WRT the BASELINE before we
	// TODO            started the merge, all entries in the SOURCE side of the map
	// TODO            should have both __SCANNED and __ACTIVE_SOURCE.
	// TODO
	// TODO            *HOWEVER*, because of "ignores" the current WD may have junk
	// TODO            in it (like compiler intermediate files) that were not in the
	// TODO            BASELINE.  These will have __SCANNED but not __ACTIVE_SOURCE.
	// TODO
	// TODO            SO, we need to decide if we want to make the junk entries
	// TODO            have __NON_ACTIVE_SOURCE status and/or claim the corresponding
	// TODO            TARGET cell and set __NON_ACTIVE_TARGET (under the assumption
	// TODO            that the junk should remain untouched during the merge).
	// TODO
	// TODO            *HOWEVER* if the FinalResult CSet has an entry under version
	// TODO            control that matches one of these ignored names, we'll get a
	// TODO            TARGET-collision.  Do we want to step on the current junk or
	// TODO            force a "unique name" on the FinalResult version (like we do
	// TODO            for normal collisions) or do we want to move the junk to a
	// TODO            plain backup file (like we do for a dirty file on a REVERT)?

	data.pMrgCSet = pMrg->pMrgCSet_FinalResult;
	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx, pMrg->pMrgCSet_FinalResult->prbEntries,
									  _sg_mrg__populate_inv_dirs__target__cb, &data)  );

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * We get called once for each entry in the queue-for-parking list.
 * The keys on this list are the repo-path as it appeared in the
 * BASELINE version of the WD.  This list is reverse sorted so that
 * deeper entries appear before the parent directories.
 */
static SG_inv_dirs_foreach_in_queue_for_parking_callback _sg_mrg__plan__queue_for_parking__cb;

static void _sg_mrg__plan__queue_for_parking__cb(SG_context * pCtx,
												 const char * pszKey_RepoPath,
												 SG_inv_entry * pInvEntry,
												 void * pVoidData_Mrg)
{
	SG_mrg * pMrg = (SG_mrg *)pVoidData_Mrg;
	SG_mrg_cset_entry * pMrgCSetEntry_Source;
	const SG_pathname * pPathParked;					// we do not own this
	SG_pathname * pPathSource = NULL;
	const char * pszReason;
	SG_inv_entry_flags flags;

	SG_ERR_CHECK(  SG_inv_entry__get_flags(pCtx, pInvEntry, &flags)  );
	switch (flags & SG_INV_ENTRY_FLAGS__PARKED__MASK)
	{
	case SG_INV_ENTRY_FLAGS__PARKED__MASK:
		pszReason = "PARKED to avoid transient name collision and/or pathname loop.";
		break;

	case SG_INV_ENTRY_FLAGS__PARKED_FOR_SWAP:
		pszReason = "PARKED to avoid transient name collision.";
		break;

	case SG_INV_ENTRY_FLAGS__PARKED_FOR_CYCLE:
		pszReason = "PARKED to avoid transient pathname loop.";
		break;

	default:
		SG_ERR_THROW2_RETURN(  SG_ERR_INVALIDARG,
							   (pCtx, "Invalid PARKED flag for [%s]", pszKey_RepoPath)  );
	}

	SG_ERR_CHECK(  SG_inv_entry__get_parked_path__ref(pCtx, pInvEntry, &pPathParked)  );
	SG_ASSERT(  (pPathParked)  );

	SG_ERR_CHECK(  SG_inv_entry__get_assoc_data__source(pCtx, pInvEntry, (void **)&pMrgCSetEntry_Source)  );
	SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path2(pCtx,
																		  pMrg->pPendingTree,
																		  pszKey_RepoPath,
																		  &pPathSource)  );

#if defined(DEBUG)
	{
		const char * psz_gid_source;						// we do not own this

		SG_ERR_CHECK(  SG_inv_entry__get_gid__source__ref(pCtx, pInvEntry, &psz_gid_source)  );
		SG_ASSERT(  (strcmp(psz_gid_source, pMrgCSetEntry_Source->bufGid_Entry) == 0)  );
	}
#endif

#if TRACE_MRG
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   ("merge_queue_for_parking: [flags 0x%x][gid %s]\n"
								"                         [repo-path %s]\n"
								"                         from: %s\n"
								"                           to: %s\n"),
							   flags, pMrgCSetEntry_Source->bufGid_Entry,
							   pszKey_RepoPath,
							   SG_pathname__sz(pPathSource),
							   SG_pathname__sz(pPathParked))  );
#endif

	pMrgCSetEntry_Source->markerValue |= _SG_MRG__PLAN__MARKER__FLAGS__PARKED;

	SG_ERR_CHECK(  SG_wd_plan__move_rename(pCtx, pMrg->p_wd_plan,
										   pMrgCSetEntry_Source->bufGid_Entry,
										   pszKey_RepoPath,
										   SG_pathname__sz(pPathSource),
										   SG_pathname__sz(pPathParked),
										   pszReason)  );
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathSource);
}

static void _sg_mrg__plan__park(SG_context * pCtx, SG_mrg * pMrg)
{
	const SG_pathname * pPathWorkingDirectoryTop;
	SG_pathname * pPathCopy = NULL;
	SG_string * pStringRepoPath = NULL;
	SG_string * pString_sgtemp = NULL;
	SG_string * pString_parkinglot = NULL;
	SG_uint32 nrInQueue;

	// build "inverted tree" and claim all of the entrynames in
	// both the current (aka source (aka baseline)) and the
	// desired (aka target (aka final-result)) version of the WD.
	SG_ERR_CHECK(  _sg_mrg__populate_inv_dirs(pCtx, pMrg)  );

	// create parking lot pathnames for anything that has a
	// transient collision.
	SG_ERR_CHECK(  SG_inv_dirs__check_for_swaps(pCtx, pMrg->pInvDirs)  );

	SG_ERR_CHECK(  SG_inv_dirs__get_parking_stats(pCtx, pMrg->pInvDirs, &nrInQueue, NULL, NULL)  );
	if (nrInQueue)
	{
		// Schedule the parking-lot directory to be TEMPORARILY added
		// to the pendingtree so that we can let the pendingtree code
		// handle the juggling of the WD (and so that maybe we can
		// avoid under-the-hood manipulations of the pendingtree).
		//
		// We know that the parking lot was constructed with a directory
		// that looks like: <wd-root>/<sgtemp>/<parking-lot>

		const SG_pathname * pPathParkingLot;

		SG_ERR_CHECK(  SG_inv_dirs__get_parking_lot_path__ref(pCtx, pMrg->pInvDirs, &pPathParkingLot)  );
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathCopy, pPathParkingLot)  );

		SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathCopy, &pString_parkinglot)  );
		SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathCopy)  );
		SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathCopy, &pString_sgtemp)  );
		SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathCopy)  );

		SG_ERR_CHECK(  SG_pendingtree__get_working_directory_top__ref(pCtx, pMrg->pPendingTree,
																	  &pPathWorkingDirectoryTop)  );
		SG_ASSERT(  (strcmp(SG_pathname__sz(pPathWorkingDirectoryTop),
							SG_pathname__sz(pPathCopy)) == 0)  );

		SG_ERR_CHECK_RETURN(  SG_gid__generate(pCtx, pMrg->bufGid_Temp,            sizeof(pMrg->bufGid_Temp))  );
		SG_ERR_CHECK_RETURN(  SG_gid__generate(pCtx, pMrg->bufGid_Temp_ParkingLot, sizeof(pMrg->bufGid_Temp_ParkingLot))  );

		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStringRepoPath)  );

		// ADD @/<sgtemp>
		SG_ERR_CHECK(  SG_pathname__append__from_string(pCtx, pPathCopy, pString_sgtemp)  );
		SG_ERR_CHECK(  SG_string__sprintf(pCtx, pStringRepoPath, "@/%s/", SG_string__sz(pString_sgtemp))  );
		SG_ERR_CHECK(  SG_wd_plan__add_new_directory_to_pendingtree(pCtx, pMrg->p_wd_plan,
																	pMrg->bufGid_Temp,
																	SG_string__sz(pStringRepoPath),
																	SG_pathname__sz(pPathCopy),
																	"TEMPORARY Directory added for parking transient collisions.")  );

		// ADD @/<sgtemp>/<parking-lot>
		SG_ERR_CHECK(  SG_pathname__append__from_string(pCtx, pPathCopy, pString_parkinglot)  );
		SG_ERR_CHECK(  SG_string__sprintf(pCtx, pStringRepoPath, "@/%s/%s/", SG_string__sz(pString_sgtemp), SG_string__sz(pString_parkinglot))  );
		SG_ERR_CHECK(  SG_wd_plan__add_new_directory_to_pendingtree(pCtx, pMrg->p_wd_plan,
																	pMrg->bufGid_Temp_ParkingLot,
																	SG_string__sz(pStringRepoPath),
																	SG_pathname__sz(pPathCopy),
																	"TEMPORARY Directory added for parking transient collisions.")  );

		// create commands to move everything with a parking lot
		// pathname into the parking lot.
		SG_ERR_CHECK(  SG_inv_dirs__foreach_in_queue_for_parking(pCtx, pMrg->pInvDirs,
																 _sg_mrg__plan__queue_for_parking__cb,
																 (void *)pMrg)  );
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathCopy);
	SG_STRING_NULLFREE(pCtx, pString_sgtemp);
	SG_STRING_NULLFREE(pCtx, pString_parkinglot);
	SG_STRING_NULLFREE(pCtx, pStringRepoPath);
}

static void _sg_mrg__plan__remove_parkinglot(SG_context * pCtx, SG_mrg * pMrg)
{
	const SG_pathname * pPathWorkingDirectoryTop;
	const SG_pathname * pPathParkingLot;
	SG_pathname * pPathCopy = NULL;
	SG_string * pStringRepoPath = NULL;
	SG_uint32 nrInQueue;

	SG_ERR_CHECK(  SG_inv_dirs__get_parking_stats(pCtx, pMrg->pInvDirs, &nrInQueue, NULL, NULL)  );
	if (nrInQueue == 0)
		return;

	// Schedule commands to REMOVE the TEMPORARY parking-lot directory
	// from the pendingtree.
	//
	// We know that the parking lot was constructed with a directory
	// that looks like: <wd-root>/<sgtemp>/<parking-lot>

	SG_ERR_CHECK(  SG_inv_dirs__get_parking_lot_path__ref(pCtx, pMrg->pInvDirs, &pPathParkingLot)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathCopy, pPathParkingLot)  );

	SG_ERR_CHECK(  SG_pendingtree__get_working_directory_top__ref(pCtx, pMrg->pPendingTree,
																  &pPathWorkingDirectoryTop)  );

	// REMOVE @/<sgtemp>/<parking-lot>/
	SG_ERR_CHECK(  SG_workingdir__wdpath_to_repopath(pCtx, pPathWorkingDirectoryTop, pPathCopy, SG_TRUE, &pStringRepoPath)  );
	SG_ERR_CHECK(  SG_wd_plan__unadd_new_directory(pCtx, pMrg->p_wd_plan,
												   pMrg->bufGid_Temp_ParkingLot,
												   SG_string__sz(pStringRepoPath),
												   SG_pathname__sz(pPathCopy),
												   "Remove TEMPORARY Directory.")  );
	SG_STRING_NULLFREE(pCtx, pStringRepoPath);

	// REMOVE @/<sgtemp>/

	SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathCopy)  );
	SG_ERR_CHECK(  SG_workingdir__wdpath_to_repopath(pCtx, pPathWorkingDirectoryTop, pPathCopy, SG_TRUE, &pStringRepoPath)  );
	SG_ERR_CHECK(  SG_wd_plan__unadd_new_directory(pCtx, pMrg->p_wd_plan,
												   pMrg->bufGid_Temp,
												   SG_string__sz(pStringRepoPath),
												   SG_pathname__sz(pPathCopy),
												   "Remove TEMPORARY Directory.")  );

fail:
	SG_STRING_NULLFREE(pCtx, pStringRepoPath);
	SG_PATHNAME_NULLFREE(pCtx, pPathCopy);
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_MASTER_PLAN__PARK_H
