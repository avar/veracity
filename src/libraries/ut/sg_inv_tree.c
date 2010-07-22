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
 * @file sg_inv_tree.c
 *
 * @details Inverted tree utilities pulled out of sg_pendingtree.c
 * Includes SG_inv_entry, SG_inv_dir, SG_inv_dirs.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#define TRACE_INV 0
#else
#define TRACE_INV 0
#endif

//////////////////////////////////////////////////////////////////

typedef SG_uint32 _inv_dir_flags;
#define SG_INV_DIR_FLAGS__ZERO					0x0000
#define SG_INV_DIR_FLAGS__SCANNED				0x0001	// SG_dir__foreach() already ran

/**
 * Information for an "entryname within a directory".  This lets us identify
 * which entry (in the gid-object sense) "owns" an entryname in each version
 * of the tree.
 *
 * Most of our data structures are tree/gid-based.  Here we need an inverted
 * map to let us match up different gid's in different trees by entryname.
 *
 * When an entryname appears in both versions of the tree, it should be the case that
 * Tree[Source].Entry[gid_source].EntryName == Tree[Target].Entry[gid_target].EntryName.
 *
 * If an entryname only appears in a directory in one version of the tree, then only
 * one of the gid's will be set.
 */
struct _inv_entry
{
	struct _inv_dir *		pInvDir_Back;			// a back-pointer						we do not own this

	void *					pVoidSource;			// opaque data associated with SOURCE
	void *					pVoidTarget;			// opaque data associated with TARGET

	SG_pathname *			pPathParked;			// only set when need to park this entry.      we own this

	SG_string *				pStrRepoPath_InSource;		// we own this

	SG_inv_entry_flags		flags;

	char					bufGidEntry_Source[SG_GID_BUFFER_LENGTH];
	char					bufGidEntry_Target[SG_GID_BUFFER_LENGTH];
};

/**
 * Information for a single directory that we scanned from disk.
 *
 * NOTE: We DO NOT keep a repo-path or an absolute path to the
 * directory on disk -- because the directory may move in the
 * filesystem before we get done.  (For example, if we are doing
 * a REVERT and a directory was moved by the uesr.)
 *
 * Note: The _inv_dir.prbEntries map may be incomplete.  We do not
 * force __MODIFIED or UNMODIFIED entries (that have the SAME entryname
 * and SAME parent in both versions of the tree) to be in the table.
 * (Since we are driven by a treediff, we won't even see some of these,
 * so we have to allow for it anyway.)  However, we should pick them
 * up when we do a directory scan -- but they may look incomplete
 * since they won't have a SOURCE or TARGET binding.
 *
 * We only have one of these for a directory whose contents have
 * changed because the set of entrynames and the entries associated
 * with those entrynames have changed within the directory.  That is,
 * if someone just does a chmod on a file within a directory, that
 * won't cause an _inv_dir to be created.
 */
struct _inv_dir
{
	struct _inv_dirs *		pInvDirs_Back;			// a back-pointer						we do not own this

	SG_portability_dir *	pPortDir;
	SG_vector *				pVecEntries;			// vec[(_inv_entry *)]					we own these			TODO do we really need this?
	SG_rbtree *				prbEntries;				// map[entry-name --> (_inv_entry *)]	we borrow these from vector

	_inv_dir_flags			flags;
	SG_portability_flags	portWarningsLogged;
};

/**
 * A flat list of all of the directories that we needed to scan or
 * otherwise deal with.  These are indexed by the directory gid as
 * usual.
 */
struct _inv_dirs
{
	SG_rbtree *				prbDirs;				// map[gid-dir-entry --> _inv_dir *]		we own these
	SG_rbtree *				prbSourceBindings;		// map[gid-entry --> _inv_entry *]			we do not own these
	SG_rbtree *				prbTargetBindings;		// map[gid-entry --> _inv_entry *]			we do not own these
	SG_rbtree *				prbQueueForParking;		// map[repo-path-src --> _inv_entry *]		we do not own these
	SG_pathname *			pPathWorkingDirTop;		// we own this
	SG_pathname *			pPathParkingLot;		// our parking lot (temp directory)         we own this
	SG_string *				pStringPurpose;			// we own this

	SG_uint32				nrParked;
	SG_uint32				nrParkedForSwap;
	SG_uint32				nrParkedForCycle;

	SG_portability_flags	portWarningsLogged;
};

//////////////////////////////////////////////////////////////////

static void _inv_dirs__queue_for_parking(SG_context * pCtx,
										 struct _inv_dirs * pInvDirs,
										 SG_inv_entry * pInvEntry);

//////////////////////////////////////////////////////////////////

void SG_inv_entry__free(SG_context * pCtx, SG_inv_entry * pInvEntry)
{
	if (!pInvEntry)
		return;

	// we do not own pInvEntry->pInvDir_Back

	SG_PATHNAME_NULLFREE(pCtx, pInvEntry->pPathParked);
	SG_STRING_NULLFREE(pCtx, pInvEntry->pStrRepoPath_InSource);
	SG_NULLFREE(pCtx, pInvEntry);
}

void SG_inv_entry__alloc(SG_context * pCtx, struct _inv_dir * pInvDir_Back, SG_inv_entry ** ppInvEntry)
{
	SG_inv_entry * pInvEntry = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pInvEntry)  );
	pInvEntry->pInvDir_Back = pInvDir_Back;

	// we only create pPathParked when needed.
	// we only set pStrRepoPath_InSource when we have a SOURCE binding.

	*ppInvEntry = pInvEntry;
	return;

fail:
	SG_INV_ENTRY_NULLFREE(pCtx, pInvEntry);
}

static void _inv_entry__set_source(SG_context * pCtx,
								   SG_inv_entry * pInvEntry,
								   const char * pszGidEntry, void * pVoidSource,
								   const char * pszRepoPathDir_InSource, const char * pszEntryName,
								   SG_bool bIsDirectory,
								   SG_bool bActive)
{
	SG_ASSERT(  (pInvEntry->bufGidEntry_Source[0] == 0)  );

	if (bActive)
		pInvEntry->flags |= SG_INV_ENTRY_FLAGS__ACTIVE_SOURCE;
	else
		pInvEntry->flags |= SG_INV_ENTRY_FLAGS__NON_ACTIVE_SOURCE;

	SG_ERR_CHECK_RETURN(  SG_gid__copy_into_buffer(pCtx, pInvEntry->bufGidEntry_Source, SG_NrElements(pInvEntry->bufGidEntry_Source), pszGidEntry)  );

	pInvEntry->pVoidSource = pVoidSource;

	// we are given the repo-path of the containing directory and our entryname.
	// it is up to us to build the full repo-path to the entry.  to be consistent,
	// we want to have final slashes on entries that are directories in the SOURCE
	// version of the tree.

	SG_ERR_CHECK_RETURN(  SG_repopath__combine__sz(pCtx,pszRepoPathDir_InSource,pszEntryName,bIsDirectory,
												   &pInvEntry->pStrRepoPath_InSource)  );
}

static void _inv_entry__set_target(SG_context * pCtx, SG_inv_entry * pInvEntry, const char * pszGidEntry, void * pVoidTarget, SG_bool bActive,
								   const char * pszEntryNameForErrorMessage)
{
	if (pInvEntry->bufGidEntry_Target[0] != 0)
		SG_ERR_THROW2_RETURN(  SG_ERR_ENTRYNAME_CONFLICT,
							   (pCtx, "Two different objects are claiming the entryname '%s' in the result.",
								pszEntryNameForErrorMessage)  );

	if (bActive)
		pInvEntry->flags |= SG_INV_ENTRY_FLAGS__ACTIVE_TARGET;
	else
		pInvEntry->flags |= SG_INV_ENTRY_FLAGS__NON_ACTIVE_TARGET;

	SG_ERR_CHECK_RETURN(  SG_gid__copy_into_buffer(pCtx, pInvEntry->bufGidEntry_Target, SG_NrElements(pInvEntry->bufGidEntry_Target), pszGidEntry)  );

	pInvEntry->pVoidTarget = pVoidTarget;
}

/**
 * allocate a pathname for this entry in the parking lot.
 *
 * this will be something of the form:
 *     <wd-top>/.sgtemp/<purpose>_<session>/<source-gid>
 *
 * We own this pathname.
 */
void SG_inv_entry__create_parked_pathname(SG_context * pCtx, SG_inv_entry * pInvEntry, SG_inv_entry_flags reason)
{
	SG_ASSERT(  (pInvEntry->flags & (SG_INV_ENTRY_FLAGS__ACTIVE_SOURCE | SG_INV_ENTRY_FLAGS__NON_ACTIVE_SOURCE))  );
	SG_ASSERT(  (pInvEntry->bufGidEntry_Source[0])  );

	if (pInvEntry->pPathParked == NULL)
	{
		const SG_pathname * pPathParkingLot = NULL;

		SG_ERR_CHECK_RETURN(  SG_inv_dirs__get_parking_lot_path__ref(pCtx,
																	 pInvEntry->pInvDir_Back->pInvDirs_Back,
																	 &pPathParkingLot)  );
		SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,
															  &pInvEntry->pPathParked,
															  pPathParkingLot,
															  pInvEntry->bufGidEntry_Source)  );

		SG_ERR_CHECK_RETURN(  _inv_dirs__queue_for_parking(pCtx,
														   pInvEntry->pInvDir_Back->pInvDirs_Back,
														   pInvEntry)  );
	}

	pInvEntry->flags |= (reason & (SG_INV_ENTRY_FLAGS__PARKED__MASK));

#if TRACE_INV
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_inv_entry__create_parked_pathname: [flags %x][park %s] for [%s]\n",
							   pInvEntry->flags,SG_pathname__sz(pInvEntry->pPathParked),
							   SG_string__sz(pInvEntry->pStrRepoPath_InSource))  );
#endif

	if (reason & SG_INV_ENTRY_FLAGS__PARKED_FOR_SWAP)
		pInvEntry->pInvDir_Back->pInvDirs_Back->nrParkedForSwap++;

	if (reason & SG_INV_ENTRY_FLAGS__PARKED_FOR_CYCLE)
		pInvEntry->pInvDir_Back->pInvDirs_Back->nrParkedForCycle++;

	pInvEntry->pInvDir_Back->pInvDirs_Back->nrParked++;
}

/**
 * For each _inv_entry (that has an entryname in the SOURCE version of the tree),
 * see if we need to create a park/swap path as an interlude.
 *
 * _inv_entry knows who owns a specfic entryname (within a directory) in the BEFORE
 * and AFTER versions of the tree.  if these are different entries, then we need to
 * introduce a "swap" pathname, so that we can move the current object to the "swap"
 * path and then move that to the final destination.  because we don't/can't know the
 * correct sequence of moves/renames to get everything switched around.
 *
 * We interleave this so that we move all of the entries that it need it to their
 * respective swap paths before we move any to their final destination.  This
 * allows us to ignore order dependencies.
 *
 * For example
 * if the user does (in order):
 *     /bin/mv f2 f1
 *     /bin/mv f3 f2
 *     /bin/m3 f4 f3
 * then to revert, we want to do:
 *     f3 --> f4, f2 --> f3, f1 --> f2
 * but we don't know the order, so we fake it by doing:
 *     f1 --> f1.swap, f2 --> f2.swap, f3 --> f3.swap (in any order)
 * and then doing
 *     f1.swap --> f2, f2.swap --> f3, f3.swap --> f4 (in any order)
 *
 * if the user does (in order);
 *     /bin/mv fa xxx
 *     /bin/mv fb fa
 *     /bin/mv xxx fb
 * so that we only see:
 *     fa --> fb, fb --> fa
 * then to revert, we do:
 *     fa --> fa.swap, fb --> fb.swap (in any order)
 * and then do:
 *     fa.swap --> fb, fb.swap --> fa (in any order)
 *
 * We only need the swap path for ENTRYNAMES that are referenced by different
 * GIDs in the SOURCE and TARGET versions of the tree.
 *
 * We have chosen to work strictly on the "source" side, we don't need to do
 * both sides.
 */
static SG_rbtree_foreach_callback _inv_entry__check_for_swap_cb;

static void _inv_entry__check_for_swap_cb(SG_context * pCtx,
										  SG_UNUSED_PARAM( const char * pszGidObject ),
										  void * pVoidInvEntry,
										  SG_UNUSED_PARAM( void * pVoidData ))
{
	//struct _inv_dirs * pInvDirs = (struct _inv_dirs *)pVoidData;
	SG_inv_entry * pInvEntry = (SG_inv_entry *)pVoidInvEntry;

	// for pInvEntry_k, there is some entryname_k (the directory knows the value,
	// but we don't care about it here).  entryname_k may or may not be associated
	// with an entry (gid_k_s) in the SOURCE version of the tree.  it may or may
	// not be associated with an entry (gid_k_t) in the TARGET version of the tree.
	//
	// 3/19/10: Correction: Originally, I said: if both gid_k_s and gid_k_t are set, they must be different.
	// 3/19/10: Correction: That is true for REVERT.  When we have an UPDATE with WD dirt and the WD contains
	// 3/19/10: Correction: a new file and we are tring to preserve/roll-forward the change, we need to try
	// 3/19/10: Correction: to claim the entryname in both the soure and target versions of the tree.  In
	// 3/19/10: Correction: that case, we don't need to park it.
	//
	// each entry gid_k_[st], if set,  may or may not be ACTIVE.  ACTIVE means they
	// the entry is participating in the change (the revert, switch-baseline, or
	// whatever); it means that the entry will be created/deleted/moved/renamed
	// during the change.

	SG_UNUSED(pszGidObject);
	SG_UNUSED(pVoidData);

	if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__ACTIVE_SOURCE)
	{
		// entryname_k (in this directory) belongs to gid_k_s in the SOURCE view; it will NOT belong
		// to that entry in the final TARGET view.

		SG_ASSERT(  (pInvEntry->flags & SG_INV_ENTRY_FLAGS__SCANNED)  );
		SG_ASSERT(  (pInvEntry->bufGidEntry_Source[0])  );

		if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__ACTIVE_TARGET)
		{
			SG_ASSERT(  (pInvEntry->bufGidEntry_Target[0])  );

			if (strcmp(pInvEntry->bufGidEntry_Source,pInvEntry->bufGidEntry_Target) != 0)
			{
				// prepare to move the gid_k_s to gid_k_s.swap so that gid_k_t can have entryname_k in TARGET view
				// (we want to dissassociate gid_k_s from entryname_k before entryname_k gets stepped on by gid_k_t)
				SG_ERR_CHECK_RETURN(  SG_inv_entry__create_parked_pathname(pCtx,pInvEntry,SG_INV_ENTRY_FLAGS__PARKED_FOR_SWAP)  );
			}
			else
			{
				// must be UPDATE with dirty WD and a newly added entryname in the WD and
				// we are preserving the dirt during the update.
			}
			return;
		}
		else if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__NON_ACTIVE_TARGET)
		{
			SG_ASSERT(  (pInvEntry->bufGidEntry_Target[0])  );
			SG_ASSERT(  (strcmp(pInvEntry->bufGidEntry_Source,pInvEntry->bufGidEntry_Target) != 0)  );

			// since gid_k_t is not active, it won't be changed and won't get entryname_k in TARGET view
			// (gid_k_s may get moved/renamed/whatever, but we don't have to worry about another entry
			// stepping on entryname_k in the filesystem before we get a chance to move/rename gid_k_s)
			return;
		}
		else
		{
			// there is no gid_k_t, so nobody uses entryname_k in the TARGET view of the tree.
			// so again, we don't have to worry about entryname_k getting stepped on before we
			// move/rename gid_k_s.
			return;
		}
	}
	else if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__NON_ACTIVE_SOURCE)
	{
		SG_ASSERT(  (pInvEntry->flags & SG_INV_ENTRY_FLAGS__SCANNED)  );
		SG_ASSERT(  (pInvEntry->bufGidEntry_Source[0])  );

		// gid_k_s is NOT participating in the change and *wants* to continue to hold entryname_k
		// in TARGET view.  *BUT* that may cause conflicts.

		if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__ACTIVE_TARGET)
		{
			SG_ASSERT(  (pInvEntry->bufGidEntry_Target[0])  );
			SG_ASSERT(  (strcmp(pInvEntry->bufGidEntry_Source,pInvEntry->bufGidEntry_Target) != 0)  );

			// gid_k_t is ACTIVE and *WANTS*/*NEEDS* to have entryname_k.  this is a conflict.
			//
			// TODO decide what to do here.  we could:
			// TODO [1] violate the non-active wishes of gid_k_s and move it to some bogus conflict/backup
			// TODO     path in the directory.
			// TODO [2] violate the change wishes of gid_k_t and refuse to change it.
			// TODO [3] violate the change wishes of gid_k_t and move it to some bogus conflict/backup path.
			// TODO [4] bail.

			// TODO make this a THROW2 and include enough info to let us know which entries collided.
			SG_ERR_THROW_RETURN(  SG_ERR_PENDINGTREE_PARTIAL_CHANGE_COLLISION  );
		}
		else if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__NON_ACTIVE_TARGET)
		{
			SG_ASSERT(  (pInvEntry->bufGidEntry_Target[0])  );
			SG_ASSERT(  (strcmp(pInvEntry->bufGidEntry_Source,pInvEntry->bufGidEntry_Target) != 0)  );

			// both gid_k_s and gid_k_t are sitting this one out.  so the fact that gid_k_s is
			// going to hold onto entryname_k as the SOURCE tree gets converted into the TARGET
			// tree is fine, because gid_k_t doesn't want to claim it.
			return;
		}
		else
		{
			// there is no gid_k_t, so nobody uses entryname_k in the TARGET view of the tree.
			// so we don't have to worry about entryname_k getting stepped on and gid_k_s can
			// keep it as the SOURCE tree gets converted into the TARGET tree.
			return;
		}
	}
	else
	{
		// because we are in a foreach iterating on source-bindings, this case can't happen.
		// neither gid_k_s nor gid_k_t exist.  this is not possible.
		SG_ASSERT(  (0)  );
		return;
	}
}

//////////////////////////////////////////////////////////////////

/**
 * We get called for each ENTRYNAME that appears in the list-of-entries for a directory.
 * This includes names that appear only in the SOURCE version of the tree, the TARGET
 * version of the tree, and ones that appear in BOTH versions (w/o regard to whether they
 * are owned by the same GID).
 *
 *
 */
static SG_rbtree_foreach_callback _inv_entry__check_for_portability_cb;

static void _inv_entry__check_for_portability_cb(SG_context * pCtx,
												 const char * pszEntryName,
												 void * pVoidInvEntry,
												 SG_UNUSED_PARAM( void * pVoidData ))
{
	SG_inv_entry * pInvEntry = (SG_inv_entry *)pVoidInvEntry;
	SG_bool bIsDuplicate;

	SG_UNUSED( pVoidData );

#if TRACE_INV
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "    _inv_entry__check_for_portability_cb: [flags %04x] %s\n",
							   pInvEntry->flags,
							   pszEntryName)  );
#endif

	if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__ACTIVE_SOURCE)
	{
		// entryname_k (in this directory) belongs to gid_k_s in the SOURCE view; it will NOT belong
		// to that entry in the final TARGET view.

		SG_ASSERT(  (pInvEntry->flags & SG_INV_ENTRY_FLAGS__SCANNED)  );
		SG_ASSERT(  (pInvEntry->bufGidEntry_Source[0])  );

		if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__ACTIVE_TARGET)
		{
			SG_ASSERT(  (pInvEntry->bufGidEntry_Target[0])  );
			SG_ASSERT(  (strcmp(pInvEntry->bufGidEntry_Source,pInvEntry->bufGidEntry_Target) != 0)  );

			// entryname_k (in this directory) belongs to gid_k_s in the SOURCE view; it will NOT belong
			// to that entry in the final TARGET view.  but entryname_k WILL belong to gid_k_t in the
			// TARGET view.  so entryname_k is present in both views.  add it to the collider.

			SG_ERR_CHECK_RETURN(  SG_portability_dir__add_item(pCtx,
															   pInvEntry->pInvDir_Back->pPortDir,
															   NULL,
															   pszEntryName,
															   SG_FALSE,
															   SG_TRUE,
															   &bIsDuplicate)  );
			SG_ASSERT(  (bIsDuplicate == SG_FALSE)  );
			return;
		}
		else if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__NON_ACTIVE_TARGET)
		{
			SG_ASSERT(  (pInvEntry->bufGidEntry_Target[0])  );
			SG_ASSERT(  (strcmp(pInvEntry->bufGidEntry_Source,pInvEntry->bufGidEntry_Target) != 0)  );

			// entryname_k (in this directory) belongs to gid_k_s in the SOURCE view; it will NOT belong
			// to that entry in the final TARGET view.  since gid_k_t is not active, it won't be changed
			// and won't get entryname_k in TARGET view.  so, entryname_k WON'T appear in the TARGET.
			// don't add it to the collider.
			return;
		}
		else
		{
			// there is no gid_k_t, so nobody uses entryname_k in the TARGET view of the tree.
			// so again, entryname_k WON'T appear in the TARGET.
			return;
		}
	}
	else if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__NON_ACTIVE_SOURCE)
	{
		SG_ASSERT(  (pInvEntry->flags & SG_INV_ENTRY_FLAGS__SCANNED)  );
		SG_ASSERT(  (pInvEntry->bufGidEntry_Source[0])  );

		// gid_k_s is NOT participating in the change and *wants* to continue to hold entryname_k
		// in TARGET view.  *BUT* that may cause conflicts.

		if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__ACTIVE_TARGET)
		{
			// NOTE: if __check_for_swap_cb() runs first, we won't ever see this case.

			SG_ASSERT(  (pInvEntry->bufGidEntry_Target[0])  );
			SG_ASSERT(  (strcmp(pInvEntry->bufGidEntry_Source,pInvEntry->bufGidEntry_Target) != 0)  );

			// gid_k_t is ACTIVE and *WANTS*/*NEEDS* to have entryname_k.  this is a HARD conflict.
			// but let's proceed for the moment and add it to the collider (so that any other
			// portability-type collisions get detected (and we assume that __check_for_swap_cb()
			// will barf on this later)).

			SG_ERR_CHECK_RETURN(  SG_portability_dir__add_item(pCtx,
															   pInvEntry->pInvDir_Back->pPortDir,
															   NULL,
															   pszEntryName,
															   SG_FALSE,
															   SG_TRUE,
															   &bIsDuplicate)  );
			SG_ASSERT(  (bIsDuplicate == SG_FALSE)  );
			return;
		}
		else if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__NON_ACTIVE_TARGET)
		{
			// both gid_k_s and gid_k_t are sitting this one out.  so the fact that gid_k_s is
			// going to hold onto entryname_k as the SOURCE tree gets converted into the TARGET
			// tree is fine, because gid_k_t doesn't want to claim it.  so add entryname_k to the
			// collider.

			SG_ERR_CHECK_RETURN(  SG_portability_dir__add_item(pCtx,
															   pInvEntry->pInvDir_Back->pPortDir,
															   NULL,
															   pszEntryName,
															   SG_FALSE,
															   SG_TRUE,
															   &bIsDuplicate)  );
			SG_ASSERT(  (bIsDuplicate == SG_FALSE)  );
			return;
		}
		else
		{
			// there is no gid_k_t, so nobody else uses entryname_k in the TARGET view of the tree.
			// so gid_k_s can keep it.  add entryname_k to the collider.

			SG_ERR_CHECK_RETURN(  SG_portability_dir__add_item(pCtx,
															   pInvEntry->pInvDir_Back->pPortDir,
															   NULL,
															   pszEntryName,
															   SG_FALSE,
															   SG_TRUE,
															   &bIsDuplicate)  );
			SG_ASSERT(  (bIsDuplicate == SG_FALSE)  );
			return;
		}
	}
	else if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__ACTIVE_TARGET)
	{
		// entryname_k was not used by an entry in the SOURCE view.
		// (this also implies that the scan did not find it.)
		//
		// entryname_k WILL be used by an entry in the TARGET view.

		SG_ASSERT(  ((pInvEntry->flags & SG_INV_ENTRY_FLAGS__SCANNED) == 0)  );

		SG_ERR_CHECK_RETURN(  SG_portability_dir__add_item(pCtx,
														   pInvEntry->pInvDir_Back->pPortDir,
														   NULL,
														   pszEntryName,
														   SG_FALSE,
														   SG_TRUE,
														   &bIsDuplicate)  );
		SG_ASSERT(  (bIsDuplicate == SG_FALSE)  );
		return;
	}
	else if (pInvEntry->flags & SG_INV_ENTRY_FLAGS__NON_ACTIVE_TARGET)
	{
		// entryname_k was not used by an entry in the SOURCE view.
		// (this also implies that the scan did not find it.)
		//
		// entryname_k WILL NOT be used by an entry in the TARGET view
		// because this enntry is not active.

		SG_ASSERT(  ((pInvEntry->flags & SG_INV_ENTRY_FLAGS__SCANNED) == 0)  );

		return;
	}
	else
	{
		// entryname_k was not part of a SOURCE- or TARGET-binding.  this implies that it
		// was found by SG_dir__read() during the scan.  and that the entry was either
		// simply __MODIFIED or UNMODIFIED (with same entryname and the same parent in both
		// versions of the tree (and not put in _inv_dir.prbEntries)).
		// so add entryname_k to the collider.

		SG_ASSERT(  (pInvEntry->flags & SG_INV_ENTRY_FLAGS__SCANNED)  );

		SG_ERR_CHECK_RETURN(  SG_portability_dir__add_item(pCtx,
														   pInvEntry->pInvDir_Back->pPortDir,
														   NULL,
														   pszEntryName,
														   SG_FALSE,
														   SG_TRUE,
														   &bIsDuplicate)  );
		SG_ASSERT(  (bIsDuplicate == SG_FALSE)  );
		return;
	}
}

//////////////////////////////////////////////////////////////////

void SG_inv_entry__get_assoc_data__source(SG_context * pCtx,
										  SG_inv_entry * pInvEntry,
										  void ** ppVoidSource)
{
	SG_NULLARGCHECK_RETURN(pInvEntry);
	SG_NULLARGCHECK_RETURN(ppVoidSource);

	*ppVoidSource = pInvEntry->pVoidSource;
}

void SG_inv_entry__get_assoc_data__target(SG_context * pCtx,
										  SG_inv_entry * pInvEntry,
										  void ** ppVoidTarget)
{
	SG_NULLARGCHECK_RETURN(pInvEntry);
	SG_NULLARGCHECK_RETURN(ppVoidTarget);

	*ppVoidTarget = pInvEntry->pVoidTarget;
}

void SG_inv_entry__get_flags(SG_context * pCtx,
							 SG_inv_entry * pInvEntry,
							 SG_inv_entry_flags * pFlags)
{
	SG_NULLARGCHECK_RETURN(pInvEntry);
	SG_NULLARGCHECK_RETURN(pFlags);

	*pFlags = pInvEntry->flags;
}

void SG_inv_entry__get_parked_path__ref(SG_context * pCtx,
										SG_inv_entry * pInvEntry,
										const SG_pathname ** ppPathParked)
{
	SG_NULLARGCHECK_RETURN(pInvEntry);
	SG_NULLARGCHECK_RETURN(ppPathParked);

	*ppPathParked = pInvEntry->pPathParked;
}

void SG_inv_entry__get_gid__source__ref(SG_context * pCtx, SG_inv_entry * pInvEntry, const char ** ppszGid)
{
	SG_NULLARGCHECK_RETURN(pInvEntry);
	SG_NULLARGCHECK_RETURN(ppszGid);

	*ppszGid = pInvEntry->bufGidEntry_Source;
}

void SG_inv_entry__get_repo_path__source__ref(SG_context * pCtx, SG_inv_entry * pInvEntry, const SG_string ** ppStringRepoPath)
{
	SG_NULLARGCHECK_RETURN(pInvEntry);
	SG_NULLARGCHECK_RETURN(ppStringRepoPath);

	*ppStringRepoPath = pInvEntry->pStrRepoPath_InSource;
}

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
/**
 * dump the _inv_entry to the console.
 *
 * we need to be given the entryname because we don't have it;
 * it is the key on _inv_dir that we are contained in.
 */
static void _inv_entry_debug__dump_to_console(SG_context * pCtx, SG_inv_entry * pInvEntry, SG_uint32 indent, const char * pszEntryName)
{
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "%*cInvEntry: [flags 0x%04x][source %65s][target %65s][bParked %d][entryname %s]\n",
							   indent,' ',
							   pInvEntry->flags,
							   pInvEntry->bufGidEntry_Source,
							   pInvEntry->bufGidEntry_Target,
							   (pInvEntry->pPathParked != NULL),
							   pszEntryName)  );
}
#endif

//////////////////////////////////////////////////////////////////

static void _inv_dir__free(SG_context * pCtx, struct _inv_dir * pInvDir)
{
	if (!pInvDir)
		return;

	// we do not own pInvDir->pInvDirs_Back

	SG_VECTOR_NULLFREE_WITH_ASSOC(pCtx, pInvDir->pVecEntries, (SG_free_callback *)SG_inv_entry__free);
	SG_RBTREE_NULLFREE(pCtx, pInvDir->prbEntries);
	SG_PORTABILITY_DIR_NULLFREE(pCtx, pInvDir->pPortDir);
	SG_NULLFREE(pCtx, pInvDir);
}

static void _inv_dir__alloc(SG_context * pCtx, struct _inv_dirs * pInvDirs_Back, struct _inv_dir ** ppInvDir)
{
	struct _inv_dir * pInvDir = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pInvDir)  );
	pInvDir->pInvDirs_Back = pInvDirs_Back;
	SG_ERR_CHECK(  SG_VECTOR__ALLOC(pCtx, &pInvDir->pVecEntries, 100)  );
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pInvDir->prbEntries)  );

	*ppInvDir = pInvDir;
	return;

fail:
	SG_ERR_IGNORE(  _inv_dir__free(pCtx, pInvDir)  );
}

//////////////////////////////////////////////////////////////////

/**
 * allocate an _inv_entry and add it to the _inv_dir.
 *
 * You do not own the returned pointer.
 */
static void _inv_dir__add(SG_context * pCtx,
						  struct _inv_dir * pInvDir,
						  const char * pszEntryName,
						  SG_inv_entry_flags flags,
						  SG_inv_entry ** ppInvEntry)
{
	SG_inv_entry * pInvEntry_Allocated = NULL;
	SG_inv_entry * pInvEntry;
	SG_bool bFound;

#if TRACE_INV
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_inv_dir__add: [flags 0x%04x][entryname %s]\n",
							   flags,pszEntryName)  );
#endif

	SG_ERR_CHECK(  SG_rbtree__find(pCtx,pInvDir->prbEntries,pszEntryName,&bFound,(void **)&pInvEntry)  );
	if (!bFound)
	{
		SG_ERR_CHECK(  SG_INV_ENTRY__ALLOC(pCtx, pInvDir, &pInvEntry_Allocated)  );

		SG_ERR_CHECK(  SG_vector__append(pCtx,pInvDir->pVecEntries,pInvEntry_Allocated,NULL)  );
		pInvEntry = pInvEntry_Allocated;
		pInvEntry_Allocated = NULL;			// vector owns this now
		SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx,pInvDir->prbEntries,pszEntryName,pInvEntry)  );	// we borrow from vector
	}
	else
	{
	}

	pInvEntry->flags |= flags;

	if (ppInvEntry)
		*ppInvEntry = pInvEntry;
	return;

fail:
	SG_INV_ENTRY_NULLFREE(pCtx,pInvEntry_Allocated);
}

//////////////////////////////////////////////////////////////////

/**
 * Bind the _inv_entry in _inv_dir ***BY ENTRYNAME NOT GID***
 * to the gid-object.  This is a reverse mapping from the way
 * we usually do it.
 *
 * Here we set the "source" side.  The gid of the object associated
 * with this entryname at the beginning of the operation.
 *
 * You DO NOT own the optionally returned _inv_entry.
 */
static void _inv_dir__bind_source(SG_context * pCtx,
								  struct _inv_dir * pInvDir,
								  const char * pszRepoPathDir_InSource,
								  const char * pszGidObject,
								  const char * pszEntryName,
								  SG_bool bIsDirectory,
								  void * pVoidSource,
								  SG_bool bActive,
								  SG_inv_entry ** ppInvEntry)
{
	SG_inv_entry * pInvEntry;
	SG_bool bFound;

	SG_ERR_CHECK(  SG_rbtree__find(pCtx,pInvDir->prbEntries,pszEntryName,&bFound,(void **)&pInvEntry)  );

	// for REVERT we are being given the GID of the entry that has this entryname
	// currently.  (when reverting a RENAME, this is the dirty/current value; for
	// a non-RENAME, this is the unchanged value.)  the "target" will be set to the
	// clean/original value in the baseline.
	//
	// for an UPDATE we should be given the current WD or baseline value;
	// the "target" will be set to the value in the destination baseline.
	//
	// TODO for now i'm going to assert this.  the idea is that we scanned the directory
	// TODO and loaded an _inv_entry for each entry on disk.
	//
	// TODO If all is as expected, SG_dir__read should have seen it.
	SG_ASSERT(  (bFound)  );

	SG_ERR_CHECK(  _inv_entry__set_source(pCtx,
										  pInvEntry,
										  pszGidObject,pVoidSource,
										  pszRepoPathDir_InSource,pszEntryName,
										  bIsDirectory,
										  bActive)  );

	if (ppInvEntry)
		*ppInvEntry = pInvEntry;

fail:
	return;
}

/**
 * Bind the _inv_entry in _inv_dir ***BY ENTRYNAME NOT GID***
 * to the gid-object.
 *
 * Here we set the "target" side.  this is the gid of the owner of
 * the entryname in the result.  (the REVERT result or the destination
 * baseline in a SWITCH-BASELINE.)
 *
 * You DO NOT own the optionally returned _inv_entry.
 */
static void _inv_dir__bind_target(SG_context * pCtx,
								  struct _inv_dir * pInvDir,
								  const char * pszGidObject,
								  const char * pszEntryName,
								  void * pVoidTarget,
								  SG_bool bActive,
								  SG_inv_entry ** ppInvEntry)
{
	SG_inv_entry * pInvEntry;
	SG_bool bFound;

	SG_ERR_CHECK(  SG_rbtree__find(pCtx,pInvDir->prbEntries,pszEntryName,&bFound,(void **)&pInvEntry)  );

	// it is valid for us to not find the entry.  we are being asked to claim
	// an entryname in a directory for something that isn't currently on disk.
	// it WILL/MIGHT be after the operation is completed, but it isn't now.

	if (!bFound)
		SG_ERR_CHECK(  _inv_dir__add(pCtx,pInvDir,pszEntryName,SG_INV_ENTRY_FLAGS__ZERO,&pInvEntry)  );

	SG_ERR_CHECK(  _inv_entry__set_target(pCtx,pInvEntry,pszGidObject,pVoidTarget,bActive,pszEntryName)  );

	if (ppInvEntry)
		*ppInvEntry = pInvEntry;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * We get called for each entry in the given directory on disk.
 * Load them into the _inv_dir.
 *
 * We don't know if this entry corresponds to anything that we know/care
 * about or not (something that has a GID-ENTRY, either already under
 * version control (in a previous changeset) or something in the pendingtree
 * (with a permanent/provisional GID-ENTRY)).  but this doesn't matter; what
 * matters is that the entry currently exists on disk with this name and that name
 * may cause us problems when we are shuffling things around.
 */
static SG_dir_foreach_callback _inv_dir__scan_current_cb;

static void _inv_dir__scan_current_cb(SG_context * pCtx,
									  const SG_string * pStringEntryName,		// we do not own this
									  SG_UNUSED_PARAM( SG_fsobj_stat * pfsStat),
									  void * pVoidData)
{
	struct _inv_dir * pInvDir = (struct _inv_dir *)pVoidData;
	const char * pszEntryName = SG_string__sz(pStringEntryName);

	SG_UNUSED(pfsStat);

	// ignore "." and ".."
	//
	// TODO we DO NOT ignore any of the ~sg00~ ignorables; we want a complete view of the directory.
	// TODO think about this some more.

	if ((strcmp(pszEntryName,".") == 0) || (strcmp(pszEntryName,"..") == 0))
		return;

	if ((strcmp(pszEntryName,".sgdrawer") == 0) || (strcmp(pszEntryName,".sgtemp") == 0))
		return;

	SG_ERR_CHECK_RETURN(  _inv_dir__add(pCtx, pInvDir, pszEntryName, SG_INV_ENTRY_FLAGS__SCANNED, NULL)  );
}

//////////////////////////////////////////////////////////////////

struct _port_settings__data
{
	SG_utf8_converter *			pConverter;			// we do not own this
    SG_varray *					pvaWarnings;		// we do not own this
	SG_portability_flags		portMask;
    SG_bool						bIgnoreWarnings;
};


/**
 * We get called for each directory whose contents will change as a result of
 * the operation (REVERT, SWITCH, whatever).  Our job is to look for *POTENTIAL*
 * PORTABILITY problems *BEFORE* we actually start modifying the WD.
 *
 * We are mainly concerned with potential collisions (such as when we predict
 * that "README" and "ReadMe" will both be in a directory).
 *
 * We are not concerned with invalid/improper entrynames (which might be valid
 * on this system, but invalid on a more restrictive system) because _addremove
 * and friends should have already warned the user when the questionable name
 * was first seen.
 */
static SG_rbtree_foreach_callback _inv_dir__check_for_portability_cb;

static void _inv_dir__check_for_portability_cb(SG_context * pCtx,
#if TRACE_INV
											   const char * pszGidObjectDir,
#else
											   SG_UNUSED_PARAM( const char * pszGidObjectDir ),
#endif
											   void * pVoidInvDir,
											   void * pVoidData)
{
	struct _inv_dir * pInvDir = (struct _inv_dir *)pVoidInvDir;
	struct _port_settings__data * pData = (struct _port_settings__data *)pVoidData;

#if !TRACE_INV
	SG_UNUSED( pszGidObjectDir );
#endif

//	SG_ASSERT(  (pInvDir->flags & SG_INV_DIR_FLAGS__SCANNED)  );

#if TRACE_INV
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_inv_dir__check_for_portability_cb: [dir gid %s] [dir flags 0x%04x]\n",
							   pszGidObjectDir,pInvDir->flags)  );
#endif

	SG_ERR_CHECK(  SG_portability_dir__alloc(pCtx,
											 pData->portMask,
											 NULL, // we don't bother with relative path, since we're only concerned with collisions (and not path-length issues)
											 pData->pConverter,
											 pData->pvaWarnings,
											 &pInvDir->pPortDir)  );
	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,pInvDir->prbEntries,_inv_entry__check_for_portability_cb,NULL)  );

	// remember if we had any warnings (popagate them to _inv_dirs), so that we can defer
	// the error-throw and log as much as we can.

	SG_ERR_CHECK(  SG_portability_dir__get_result_flags(pCtx,pInvDir->pPortDir,NULL,NULL,&pInvDir->portWarningsLogged)  );

#if TRACE_INV
	{
		SG_int_to_string_buffer tmp;
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "_inv_dir__check_for_portability_cb: [dir gid %s] [warnings 0x%s]\n",
								   pszGidObjectDir,
								   SG_uint64_to_sz__hex(pInvDir->portWarningsLogged, tmp))  );
	}
#endif

	pInvDir->pInvDirs_Back->portWarningsLogged |= pInvDir->portWarningsLogged;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
/**
 * dump the entire _inv_dir directory to the console.
 *
 * we need to be given the gid-object of the directory because we don't have it;
 * it is the key on the _inv_dirs table that we are contained in.
 */
static void _inv_dir_debug__dump_to_console(SG_context * pCtx, struct _inv_dir * pInvDir, SG_uint32 indent, const char * pszGidObjectDir)
{
	SG_inv_entry * pInvEntry;
	SG_rbtree_iterator * pIter = NULL;
	const char * pszKey;
	SG_bool bFound;

	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "%*cInvDir: [flags 0x%x][gid %s]\n",
							   indent,' ',
							   pInvDir->flags,
							   pszGidObjectDir)  );

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,&pIter,pInvDir->prbEntries,&bFound,&pszKey,(void **)&pInvEntry)  );
	while (bFound)
	{
		SG_ERR_IGNORE(  _inv_entry_debug__dump_to_console(pCtx,pInvEntry,indent+4,pszKey)  );
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bFound,&pszKey,(void **)&pInvEntry)  );
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx,pIter);
	SG_context__err_reset(pCtx);
}
#endif

//////////////////////////////////////////////////////////////////

void SG_inv_dirs__free(SG_context * pCtx, struct _inv_dirs * pInvDirs)
{
	if (!pInvDirs)
		return;

	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pInvDirs->prbDirs, (SG_free_callback *)_inv_dir__free);
	SG_RBTREE_NULLFREE(pCtx, pInvDirs->prbSourceBindings);
	SG_RBTREE_NULLFREE(pCtx, pInvDirs->prbTargetBindings);
	SG_RBTREE_NULLFREE(pCtx, pInvDirs->prbQueueForParking);
	SG_PATHNAME_NULLFREE(pCtx, pInvDirs->pPathWorkingDirTop);
	SG_PATHNAME_NULLFREE(pCtx, pInvDirs->pPathParkingLot);
	SG_STRING_NULLFREE(pCtx, pInvDirs->pStringPurpose);
	SG_NULLFREE(pCtx, pInvDirs);
}

void SG_inv_dirs__alloc(SG_context * pCtx,
						const SG_pathname * pPathWorkingDirTop,
						const char * pszPurpose,
						SG_inv_dirs ** ppInvDirs)
{
	SG_inv_dirs * pInvDirs = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pInvDirs)  );
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pInvDirs->prbDirs)  );
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pInvDirs->prbSourceBindings)  );
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pInvDirs->prbTargetBindings)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pInvDirs->pPathWorkingDirTop, pPathWorkingDirTop)  );
	// defer creation of pInvDirs->pPathParkingLot until we need it
	// defer creation of pInvDirs->prbQueueForParking until we need it
	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pInvDirs->pStringPurpose, pszPurpose)  );

	*ppInvDirs = pInvDirs;
	return;

fail:
	SG_INV_DIRS_NULLFREE(pCtx, pInvDirs);
}

//////////////////////////////////////////////////////////////////

void SG_inv_dirs__get_parking_lot_path__ref(SG_context * pCtx,
											struct _inv_dirs * pInvDirs,
											const SG_pathname ** ppPathParkingLot)
{
	if (!pInvDirs->pPathParkingLot)
		SG_ERR_CHECK_RETURN(  SG_workingdir__generate_and_create_temp_dir_for_purpose(pCtx,
																					  pInvDirs->pPathWorkingDirTop,
																					  SG_string__sz(pInvDirs->pStringPurpose),
																					  &pInvDirs->pPathParkingLot)  );

	if (ppPathParkingLot)
		*ppPathParkingLot = pInvDirs->pPathParkingLot;
}

//////////////////////////////////////////////////////////////////

static void _inv_dirs__queue_for_parking(SG_context * pCtx,
										 struct _inv_dirs * pInvDirs,
										 SG_inv_entry * pInvEntry)
{
	if (!pInvDirs->prbQueueForParking)
	{
		// Give us a reverse-sorted rbtree.  So that "@/foo/bar" appears before "@/foo".
		// So that when we get ready to park things we park the children before all of
		// the containing parent directories.

		SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC__PARAMS2(pCtx,
														&pInvDirs->prbQueueForParking,
														128,NULL,
														SG_rbtree__compare_function__reverse_strcmp)  );
	}

	SG_ERR_CHECK_RETURN(  SG_rbtree__add__with_assoc(pCtx,
													 pInvDirs->prbQueueForParking,
													 SG_string__sz(pInvEntry->pStrRepoPath_InSource),
													 pInvEntry)  );
}

//////////////////////////////////////////////////////////////////

/**
 * Lookup and/or insert _inv_dir for the directory with the given GID
 * in our flat list of directories.
 *
 * You DO NOT own the returned pointer.
 */
static void _inv_dirs__find_or_insert_dir(SG_context * pCtx,
										  struct _inv_dirs * pInvDirs,
										  const char * pszGidDir,
										  struct _inv_dir ** ppInvDir)
{
	struct _inv_dir * pInvDir_Allocated = NULL;
	struct _inv_dir * pInvDir;
	SG_bool bFound;

	SG_ERR_CHECK(  SG_rbtree__find(pCtx,pInvDirs->prbDirs,pszGidDir,&bFound,(void **)&pInvDir)  );
	if (!bFound)
	{
		SG_ERR_CHECK(  _inv_dir__alloc(pCtx, pInvDirs, &pInvDir_Allocated)  );
		SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx,pInvDirs->prbDirs,pszGidDir,pInvDir_Allocated)  );
		pInvDir = pInvDir_Allocated;
		pInvDir_Allocated = NULL;		// pInvDirs->prbDirs owns it now
	}

	if (ppInvDir)
		*ppInvDir = pInvDir;

fail:
	SG_ERR_IGNORE(  _inv_dir__free(pCtx, pInvDir_Allocated)  );
}

/**
 * Create an _inv_dir for the given directory (if necessary) and read the
 * current contents of the directory on disk (in the working directory).
 *
 * We return the allocated _inv_dir.  You DO NOT own this.
 */
static void _inv_dirs__scan_current_dir(SG_context * pCtx,
										struct _inv_dirs * pInvDirs,
										const char * pszGidDir,
										const char * pszRepoPathDir,
										struct _inv_dir ** ppInvDir)
{
	SG_pathname * pPathDir = NULL;
	struct _inv_dir * pInvDir;

	SG_ASSERT(  (pInvDirs)  );
	SG_ASSERT(  (pszGidDir && *pszGidDir)  );
	SG_ASSERT(  (pszRepoPathDir && *pszRepoPathDir)  );
	// ppInvDir is optional

	SG_ERR_CHECK(  _inv_dirs__find_or_insert_dir(pCtx,pInvDirs,pszGidDir,&pInvDir)  );

	// we only need to actually scan the disk once.  if this is a
	// duplicate request, just return the original result.

	if ( (pInvDir->flags & SG_INV_DIR_FLAGS__SCANNED) == 0 )
	{
		SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx,pInvDirs->pPathWorkingDirTop,pszRepoPathDir,&pPathDir)  );

#if TRACE_INV
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   ("_inv_dirs__scan_current_dir: scanning [repo-path %s][gid %s]\n"
									"                             absolute [%s]\n"),
								   pszRepoPathDir,pszGidDir,
								   SG_pathname__sz(pPathDir))  );
#endif

		SG_ERR_CHECK(  SG_dir__foreach(pCtx,pPathDir,SG_FALSE,_inv_dir__scan_current_cb,(void *)pInvDir)  );

		pInvDir->flags |= SG_INV_DIR_FLAGS__SCANNED;
	}

	if (ppInvDir)
		*ppInvDir = pInvDir;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDir);
}

//////////////////////////////////////////////////////////////////

/**
 * Given {entryname,entry-gid-object,parent-gid-object} in the "source" version of the tree,
 * "claim" the entryname in the directory.
 *
 * Let: e := _inv_entry(source := <entry-gid-object>, target := <unspecified>)
 *
 * We build 2 maps:
 * [1] _inv_dirs.dir[<parent-gid-object>].entry[<entryname>] --> e
 * [2] _inv_dirs.source[<entry-gid-object>]                  --> e
 */
void SG_inv_dirs__bind_source(SG_context * pCtx,
							  SG_inv_dirs * pInvDirs,
							  const char * pszGidDir, const char * pszRepoPathDir,
							  const char * pszGidEntry, const char * pszEntryName,
							  SG_bool bIsDirectory,
							  void * pVoidSource,
							  SG_bool bActive,
							  struct _inv_dir ** ppInvDir,
							  SG_inv_entry ** ppInvEntry)
{
	struct _inv_dir * pInvDir = NULL;
	SG_inv_entry * pInvEntry = NULL;

	SG_ERR_CHECK(  _inv_dirs__scan_current_dir(pCtx,pInvDirs,pszGidDir,pszRepoPathDir, &pInvDir)  );
	SG_ERR_CHECK(  _inv_dir__bind_source(pCtx,
										 pInvDir,pszRepoPathDir,
										 pszGidEntry,pszEntryName,
										 bIsDirectory,
										 pVoidSource,
										 bActive,
										 &pInvEntry)  );
	SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx,pInvDirs->prbSourceBindings,pszGidEntry,pInvEntry)  );

	if (ppInvDir)
		*ppInvDir = pInvDir;
	if (ppInvEntry)
		*ppInvEntry = pInvEntry;

fail:
	return;
}

/**
 * Claim the given entryname in the given directory in the "target" version of the tree.
 */
void SG_inv_dirs__bind_target(SG_context * pCtx,
							  SG_inv_dirs * pInvDirs,
							  const char * pszGidDir,
							  const char * pszGidEntry, const char * pszEntryName, void * pVoidTarget,
							  SG_bool bActive,
							  struct _inv_dir ** ppInvDir,
							  SG_inv_entry ** ppInvEntry)
{
	struct _inv_dir * pInvDir = NULL;
	SG_inv_entry * pInvEntry = NULL;

	SG_ERR_CHECK(  _inv_dirs__find_or_insert_dir(pCtx,pInvDirs,pszGidDir,&pInvDir)  );
	SG_ERR_CHECK(  _inv_dir__bind_target(pCtx,pInvDir,pszGidEntry,pszEntryName,pVoidTarget,bActive, &pInvEntry)  );
	SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx,pInvDirs->prbTargetBindings,pszGidEntry,pInvEntry)  );

	if (ppInvDir)
		*ppInvDir = pInvDir;
	if (ppInvEntry)
		*ppInvEntry = pInvEntry;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * lookup binding by the gid-mapping.
 *
 * You do not own the returned pointer.
 */
void SG_inv_dirs__fetch_source_binding(SG_context * pCtx,
									   SG_inv_dirs * pInvDirs,
									   const char * pszGidEntry,
									   SG_bool * pbFound,
									   SG_inv_entry ** ppInvEntry,
									   void ** ppVoidSource)
{
	SG_inv_entry * pInvEntry;

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,pInvDirs->prbSourceBindings,pszGidEntry,pbFound,(void **)&pInvEntry)  );

	if (!*pbFound)
		return;

	if (ppInvEntry)
		*ppInvEntry = pInvEntry;
	if (ppVoidSource)
		*ppVoidSource = pInvEntry->pVoidSource;
}

void SG_inv_dirs__fetch_target_binding(SG_context * pCtx,
									   SG_inv_dirs * pInvDirs,
									   const char * pszGidEntry,
									   SG_bool * pbFound,
									   SG_inv_entry ** ppInvEntry,
									   void ** ppVoidTarget)
{
	SG_inv_entry * pInvEntry;

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,pInvDirs->prbTargetBindings,pszGidEntry,pbFound,(void **)&pInvEntry)  );

	if (!*pbFound)
		return;

	if (ppInvEntry)
		*ppInvEntry = pInvEntry;
	if (ppVoidTarget)
		*ppVoidTarget = pInvEntry->pVoidTarget;
}

//////////////////////////////////////////////////////////////////

void SG_inv_dirs__check_for_swaps(SG_context * pCtx,
								  SG_inv_dirs * pInvDirs)
{
	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,pInvDirs->prbSourceBindings,_inv_entry__check_for_swap_cb,NULL)  );
}

//////////////////////////////////////////////////////////////////

void SG_inv_dirs__check_for_portability(SG_context * pCtx,
										SG_inv_dirs * pInvDirs,
										SG_pendingtree * pPendingTree)
{
	struct _port_settings__data data;

	memset(&data, 0, sizeof(data));

	SG_ERR_CHECK_RETURN(  SG_pendingtree__get_port_settings(pCtx, pPendingTree,
															&data.pConverter,
															&data.pvaWarnings,
															&data.portMask,
															&data.bIgnoreWarnings)  );
	if (data.bIgnoreWarnings)
		return;

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx, pInvDirs->prbDirs, _inv_dir__check_for_portability_cb, &data)  );

#if TRACE_INV
	{
		SG_int_to_string_buffer tmp;
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "_inv_dirs__check_for_portability: [net warnings 0x%s]\n",
								   SG_uint64_to_sz__hex(pInvDirs->portWarningsLogged,tmp))  );
	}
#endif
}

//////////////////////////////////////////////////////////////////

void SG_inv_dirs__get_portability_warnings_observed(SG_context * pCtx,
													SG_inv_dirs * pInvDirs,
													SG_portability_flags * pFlags)
{
	SG_NULLARGCHECK_RETURN(pInvDirs);
	SG_NULLARGCHECK_RETURN(pFlags);

	*pFlags = pInvDirs->portWarningsLogged;
}

void SG_inv_dirs__get_parking_stats(SG_context * pCtx,
									SG_inv_dirs * pInvDirs,
									SG_uint32 * pNrParked,
									SG_uint32 * pNrParkedForSwap,
									SG_uint32 * pNrParkedForCycle)
{
	SG_NULLARGCHECK_RETURN(pInvDirs);

	if (pNrParked)
		*pNrParked = pInvDirs->nrParked;

	if (pNrParkedForSwap)
		*pNrParkedForSwap = pInvDirs->nrParkedForSwap;

	if (pNrParkedForCycle)
		*pNrParkedForCycle = pInvDirs->nrParkedForCycle;
}

void SG_inv_dirs__foreach_in_queue_for_parking(SG_context * pCtx,
											   SG_inv_dirs * pInvDirs,
											   SG_inv_dirs_foreach_in_queue_for_parking_callback * pfn,
											   void * pVoidData)
{
	SG_uint32 nrInQueue = 0;

	SG_NULLARGCHECK_RETURN(pInvDirs);

	if (pInvDirs->prbQueueForParking)
		SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx, pInvDirs->prbQueueForParking, &nrInQueue)  );
	SG_ASSERT(  (nrInQueue == pInvDirs->nrParked)  );

	if (nrInQueue == 0)
		return;

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,
											 pInvDirs->prbQueueForParking,
											 (SG_rbtree_foreach_callback *)pfn,
											 pVoidData)  );
}

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
/**
 * dump the entire _inv_tree to the console.
 */
void SG_inv_dirs_debug__dump_to_console(SG_context * pCtx, SG_inv_dirs * pInvDirs)
{
	struct _inv_dir * pInvDir;
	SG_rbtree_iterator * pIter = NULL;
	const char * pszKey;
	SG_bool bFound;

	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "InvDirs: [wd-top %s][nrParked %d] [nrSwap %d][nrCycle %d]\n",
							   SG_pathname__sz(pInvDirs->pPathWorkingDirTop),
							   pInvDirs->nrParked,pInvDirs->nrParkedForSwap,pInvDirs->nrParkedForCycle)  );

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,&pIter,pInvDirs->prbDirs,&bFound,&pszKey,(void **)&pInvDir)  );
	while (bFound)
	{
		SG_ERR_IGNORE(  _inv_dir_debug__dump_to_console(pCtx,pInvDir,4,pszKey)  );
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bFound,&pszKey,(void **)&pInvDir)  );
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx,pIter);
	SG_context__err_reset(pCtx);
}
#endif

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
struct _verify_parking_lot_empty_data
{
	SG_uint32 nrEntries;
};

static SG_dir_foreach_callback _inv_dirs_debug__verify_parking_lot_empty_cb;

static void _inv_dirs_debug__verify_parking_lot_empty_cb(SG_context * pCtx,
														 const SG_string * pStringEntryName,
														 SG_UNUSED_PARAM( SG_fsobj_stat * pfsStat ),
														 void * pVoidData)
{
	struct _verify_parking_lot_empty_data * pData = (struct _verify_parking_lot_empty_data *)pVoidData;
	const char * pszEntryName = SG_string__sz(pStringEntryName);

	SG_UNUSED( pfsStat );

	// ignore "." and ".."

	if ((strcmp(pszEntryName,".") == 0) || (strcmp(pszEntryName,"..") == 0))
		return;

	pData->nrEntries++;

	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"_inv_dirs_debug__verify_parking_lot_empty_cb: found '%s'\n",pszEntryName)  );
}

/**
 * verify that the parking lot directory is empty.  after all of the structural changes
 * are made and the things that were parked have been moved back to the WD, the parking lot
 * directory should be empty -- if everything was accounted for.
 */
void SG_inv_dirs_debug__verify_parking_lot_empty(SG_context * pCtx, SG_inv_dirs * pInvDirs)
{
	struct _verify_parking_lot_empty_data data;
	SG_bool bHaveParkingLot  = (pInvDirs->pPathParkingLot != NULL);
	SG_bool bFound;

	if (!bHaveParkingLot)
		return;

	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pInvDirs->pPathParkingLot,&bFound,NULL,NULL)  );
	if (!bFound)
		return;

	memset(&data,0,sizeof(data));

	SG_ERR_CHECK(  SG_dir__foreach(pCtx,pInvDirs->pPathParkingLot,SG_FALSE,_inv_dirs_debug__verify_parking_lot_empty_cb,(void *)&data)  );

	if (data.nrEntries > 0)
		SG_ERR_THROW2(  SG_ERR_ASSERT,
						(pCtx, "Parking lot not empty [nr %d][%s]",
						 data.nrEntries,
						 SG_pathname__sz(pInvDirs->pPathParkingLot))  );

fail:
	return;
}
#endif

//////////////////////////////////////////////////////////////////

void SG_inv_dirs__delete_parking_lot(SG_context * pCtx, SG_inv_dirs * pInvDirs)
{
	SG_bool bHaveParkingLot  = (pInvDirs->pPathParkingLot != NULL);
	SG_bool bFound;

	if (!bHaveParkingLot)
		return;

#if defined(DEBUG)
	SG_ERR_IGNORE(  SG_inv_dirs_debug__verify_parking_lot_empty(pCtx, pInvDirs)  );
#endif

	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pInvDirs->pPathParkingLot,&bFound,NULL,NULL)  );
	if (bFound)
	{
		// this throws if the directory is not empty.
		SG_ERR_CHECK(  SG_fsobj__rmdir__pathname(pCtx, pInvDirs->pPathParkingLot)  );
	}
	
	pInvDirs->nrParked = 0;
	SG_PATHNAME_NULLFREE(pCtx, pInvDirs->pPathParkingLot);

fail:
	return;
}

void SG_inv_dirs__abandon_parking_lot(SG_context * pCtx, SG_inv_dirs * pInvDirs)
{
	pInvDirs->nrParked = 0;
	SG_PATHNAME_NULLFREE(pCtx, pInvDirs->pPathParkingLot);
}
