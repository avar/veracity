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
 * @file sg_treediff2.c
 *
 * @details Routines and stuff to compute tree-diffs.
 *
 * TODO Move the treenode cache/sharing out of this file and into a real
 * TODO treenode-cache so that lots of things can use it.
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

#if defined(DEBUG)
#	define TRACE_TREEDIFF			0
#	define TRACE_TREEDIFF_FILTER	0
#else
#	define TRACE_TREEDIFF			0
#	define TRACE_TREEDIFF_FILTER	0
#endif

//////////////////////////////////////////////////////////////////

typedef struct _objectData ObjectData;

typedef enum _od_type
{
	OD_TYPE_UNPOPULATED			= 0x00,		// a void (possibly temporarily)
	OD_TYPE_FILE				= 0x01,		// a regular file
	OD_TYPE_SYMLINK				= 0x02,		// a symlink
	OD_TYPE_UNSCANNED_FOLDER	= 0x04,		// a folder (something that we may or may not have to dive into)
	OD_TYPE_SCANNED_FOLDER		= 0x08,		// a folder that we have already dived into
	OD_TYPE_PENDINGTREE_FOLDER	= 0x10,		// a folder in baseline that was added because of pending-tree (we may or may not have to scan it during cset0-vs-baseline)
}	OD_Type;

typedef struct _od_inst			// an instance of an object in a version of the tree
{
	SG_int64					attrBits;
	SG_string *					pStrEntryName;			// we own this
	SG_string *					pStrRepoPath;			// we own this
	SG_int32					depthInTree;			// only used for Ndx_Orig,Ndx_Dest during scan, -1 when undefined/unknown
	OD_Type						typeInst;
	SG_treenode_entry_type		tneType;
	SG_diffstatus_flags			dsFlags;
	char						bufParentGid[SG_GID_BUFFER_LENGTH];
	char						bufHidContent[SG_HID_MAX_BUFFER_LENGTH];
	char						bufHidXAttrs[SG_HID_MAX_BUFFER_LENGTH];
}	OD_Inst;

/**
 * we define the following different "instances" or versions of an entry
 * in the tree.
 *
 * these are used in various (too many) combinations:
 * [] when SG_TREEDIFF2_KIND__CSET_VS_CSET
 *    Ndx_Orig is cset0 (and used to load tree from repo)
 *    Ndx_Dest is cset1 (during scanning) (and used to load tree from repo)
 *    Ndx_Pend is unused
 *    Ndx_Net  is cset1 (data moved here durring summary phase)
 *
 * [] when SG_TREEDIFF2_KIND__BASELINE_VS_WD
 *    Ndx_Orig is baseline (TODO may or may not be used to load info from repo (see if PT already has the info))
 *    Ndx_Dest is unused
 *    Ndx_Pend is pending-tree (synthesized from sg_pendingtree, no repo scanning required)
 *    Ndx_Net  is pending-tree (data moved here during summary phase)
 *
 * [] when SG_TREEDIFF2_KIND__CSET_VS_WD
 *    Ndx_Orig is cset0 (and used to load tree from repo)
 *    Ndx_Dest is baseline (and used to load tree from repo)
 *    Ndx_Pend is pending-tree (synthesized from sg_pendingtree, no repo sanning required)
 *    Ndx_Net  is the composition of the changes between cset0-and-baseline and between
 *                the baseline-and-pendingtree.
 */
typedef enum _ndx_inst
{
	Ndx_Orig = 0,				// Must be 0 for aCSetColumns
	Ndx_Dest = 1,				// Must be 1 for aCSetColumns
	Ndx_Pend = 2,
	Ndx_Net  = 3,
	__NR_NDX__					// Must be Last.
}	Ndx_Inst;

struct _objectData
{
	SG_int32					minDepthInTree;
	OD_Inst *					apInst[__NR_NDX__];
	char						bufGidObject[SG_GID_BUFFER_LENGTH];

	SG_int32					user_marker_data;		// this is only used by the _mark_ functions to let the caller tag an entry
};

//////////////////////////////////////////////////////////////////

typedef struct _td_cset_column
{
	SG_changeset *		pcsCSet;
	char				bufHidCSet[SG_HID_MAX_BUFFER_LENGTH];
}	TD_CSet_Column;

//////////////////////////////////////////////////////////////////

struct _SG_treediff2
{
	SG_repo *			pRepo;
	SG_pathname *		pPathWorkingDirectoryTop;		// only set when using pending-tree

	SG_treediff2_kind	kind;		// the type of tree-diff (when set it also implies we are frozen)

#define IS_FROZEN(p)	((p)->kind != SG_TREEDIFF2_KIND__ZERO)

	// we keep a rbtree of the tree-nodes that we load from disk so that we can make
	// sure that we free them all without having to worry about them when unwinding
	// our stack (and because the tree-node-entries are actually inside the tree-nodes)
	// and because a treenode might be referenced by both changesets and we don't want
	// to load it twice from disk.
	//
	// we also allow other SG_treediff2 instances to share a common tree-node cache.
	// this is usefull because it keeps us from having to hit the disk
	// for each treenode in the LCA.

	SG_treediff2 *		pTreeDiff_cache_owner;
	SG_rbtree *			prbTreenodesAllocated;		// map [hidTN --> pTreenode] we own pTreenodes
	SG_uint32			nrShares;					// crude reference count on cache shares.

	// per-cset data is stored in a "column" for each.  these are ordered because
	// the user is expecting us to report in LEFT-vs-RIGHT or OLDER-vs-NEWER terms.

	TD_CSet_Column		aCSetColumns[2];			// Ndx_Orig and Ndx_Dest

	// as we explore both trees, we must sync up files/folders by Object GID and
	// see how the corresponding entries compare.  note that we must do this in
	// Object GID space rather than pathname space so that we can detect moves and
	// renames and etc.
	//
	// this rbtree contains a ROW for each Object GID that we have observed.
	// this tree contains all of the ROWs that we have allocated.

	SG_rbtree *			prbObjectDataAllocated;		// map[gidObject --> pObjectData] we own pObjectData

	// we build work-queue containing a depth-sorted list of FOLDERS that
	// we ***MAY*** need to dive into.
	//
	// i say "may" here, because there are times when we can short-circuit
	// the full treewalk.  because of the bubble-up effect on the Blob HIDs
	// of sub-folders, we may not have to walk a part of the 2 versions of
	// the tree when we can detect that they are identical from the HIDs.
	//
	// as we process the work-queue ObjectData row-by-row, we remove an entry
	// as we process it and we (conditionally) add to the work-queue any
	// sub-folders that it may have.
	//
	// processing continues until we have removed all entries from the
	// work-queue.  this represents a kind of closure on the tree scan.
	//
	// we sort this by depth-in-the-tree so that we are more likely to scan
	// across (like a breadth-first search) rather than fully populating
	// one version before visiting the other version (like in a depth-first
	// treewalk).
	//
	// note: this does not prevent us from "accidentially" scanning one side
	// of an identical sub-folder pair.  in the case where a folder is moved
	// deeper in the tree (but otherwise unchanged), we will scan the shallower
	// side (thinking it is peerless) before seeing it on the other side.  in
	// this case, we go ahead and repeat the scan in the deeper version so that
	// all the (identical) children get populated on both sides (so that they
	// don't look like additions/deletions).  these will get pruned out later
	// with the regular identical-pair filter.

	SG_rbtree *			prbWorkQueue;		// map[(depth,gidObject) --> pObjectData] we borrow pointers

	// after we have processed the work-queue and reached closure, we build
	// a list of the ObjectData ROWS that have changes of some kind.

	SG_rbtree *			prbRawSummary;		// map[gidObject --> pObjectData] we borrow pointers
};

//////////////////////////////////////////////////////////////////

static void _od_inst__free(SG_context * pCtx, OD_Inst * pInst)
{
	if (!pInst)
		return;

	SG_STRING_NULLFREE(pCtx, pInst->pStrEntryName);
	SG_STRING_NULLFREE(pCtx, pInst->pStrRepoPath);

	SG_NULLFREE(pCtx, pInst);
}

static void _od_inst__alloc(SG_context * pCtx, const char * szEntryName, OD_Inst ** ppInst)
{
	OD_Inst * pInst = NULL;

	SG_ERR_CHECK(  SG_alloc(pCtx, 1,sizeof(OD_Inst),&pInst)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pInst->pStrEntryName,szEntryName)  );

	pInst->depthInTree = -1;

	// we let caller fill in the other fields.

	*ppInst = pInst;
	return;

fail:
	SG_ERR_IGNORE(  _od_inst__free(pCtx, pInst)  );
}

/**
 * clone an OD_Inst.
 *
 * We copy all of the fields so there is sharing of fields.
 *
 * We set the depth to -1 because it is not safe to assume
 * anything about the depth.  If you need the depth in the
 * copy to be valid compute it using the proper version of
 * the parent.
 */
static void _od_inst__clone(SG_context * pCtx, const OD_Inst * pSrc, OD_Inst ** ppCopy)
{
	OD_Inst * pInst = NULL;

	SG_ERR_CHECK(  _od_inst__alloc(pCtx,SG_string__sz(pSrc->pStrEntryName),&pInst)  );

	pInst->attrBits = pSrc->attrBits;
	pInst->typeInst = pSrc->typeInst;
	pInst->tneType = pSrc->tneType;
	pInst->dsFlags = pSrc->dsFlags;
	SG_ERR_CHECK(  SG_strcpy(pCtx, pInst->bufParentGid, SG_NrElements(pInst->bufParentGid), pSrc->bufParentGid )  );
	SG_ERR_CHECK(  SG_strcpy(pCtx, pInst->bufHidContent,SG_NrElements(pInst->bufHidContent),pSrc->bufHidContent)  );
	SG_ERR_CHECK(  SG_strcpy(pCtx, pInst->bufHidXAttrs, SG_NrElements(pInst->bufHidXAttrs), pSrc->bufHidXAttrs )  );

	pInst->depthInTree = -1;		// don't assume anything about the depth because of moves.
	pInst->pStrRepoPath = NULL;		// don't clone this because there may be moves/renames in parent folders in this version.

	*ppCopy = pInst;
	return;

fail:
	SG_ERR_IGNORE(  _od_inst__free(pCtx, pInst)  );
}

/**
 * compare 2 versions/instances of the entry and figure
 * out what is different between them.
 */
static void _od_inst__compute_dsFlags_from_pair(SG_context * pCtx,
												const ObjectData * pOD,
												SG_diffstatus_flags * pdsFlags)
{
	SG_diffstatus_flags dsFlags = SG_DIFFSTATUS_FLAGS__ZERO;
	const OD_Inst * pInst_orig = pOD->apInst[Ndx_Orig];
	const OD_Inst * pInst_dest = pOD->apInst[Ndx_Dest];

	// WARNING: We cannot set any of the SG_DIFFSTATUS_FLAGS__UNDONE_ flags
	// WARNING: because we don't have that information.  We have information
	// WARNING: for things that are different between the original (cset0)
	// WARNING: and the destination (cset1) changesets.  We do not know what
	// WARNING: (if anything) happened to the entry in any of the changesets
	// WARNING: *between* them.
	// WARNING:
	// WARNING: All we know is the initial and final values for this entry.
	//
	// TODO decide if this is important.

	SG_ASSERT_RELEASE_FAIL2(  (pInst_orig->tneType == pInst_dest->tneType),
					  (pCtx,
					   "Object [GID %s][name '%s' '%s'] different tneTypes [Ndx_Orig %d][Ndx_Dest %d]\n",
					   pOD->bufGidObject,
					   SG_string__sz(pInst_orig->pStrEntryName),SG_string__sz(pInst_dest->pStrEntryName),
					   (SG_uint32)pInst_orig->tneType,
					   (SG_uint32)pInst_dest->tneType)  );

	// make sure we have valid HIDs for the contents.
	// when we are comparing cset0 vs cset1, we should *never*
	// get a bogus HID.

	// TODO decide if we want to only check these when FILE/SYMLINK.
	// TODO
	// TODO also, because the _MODIFIED bit on a directory is a little
	// TODO special, think about this a bit more.
	//
	// TODO need to fetch the length_of_all_hids_in_this_repo to get these asserts to work properly.
	//SG_ASSERT_RELEASE_FAIL(  (strlen(pInst_orig->bufHidContent) == SG_ID_LENGTH_IN_HEX_CHARS)  );
	//SG_ASSERT_RELEASE_FAIL(  (strlen(pInst_dest->bufHidContent) == SG_ID_LENGTH_IN_HEX_CHARS)  );

	if (strcmp(pInst_orig->bufHidContent,pInst_dest->bufHidContent) != 0)
		dsFlags |= SG_DIFFSTATUS_FLAGS__MODIFIED;

	if (strcmp(SG_string__sz(pInst_orig->pStrEntryName),SG_string__sz(pInst_dest->pStrEntryName)) != 0)
		dsFlags |= SG_DIFFSTATUS_FLAGS__RENAMED;

	if (strcmp(pInst_orig->bufParentGid,pInst_dest->bufParentGid) != 0)
		dsFlags |= SG_DIFFSTATUS_FLAGS__MOVED;

	if (strcmp(pInst_orig->bufHidXAttrs,pInst_dest->bufHidXAttrs) != 0)
		dsFlags |= SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS;

	if (pInst_orig->attrBits != pInst_dest->attrBits)
		dsFlags |= SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS;

	*pdsFlags = dsFlags;
	return;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

static void _od__free(SG_context * pCtx, ObjectData * pOD)
{
	int ndx;

	if (!pOD)
		return;

	for (ndx=0; ndx<__NR_NDX__; ndx++)
		_od_inst__free(pCtx, pOD->apInst[ndx]);

	SG_NULLFREE(pCtx, pOD);
}
static void _od__free__cb(SG_context * pCtx, void * pVoid)
{
	_od__free(pCtx, (ObjectData *)pVoid);
}

static void _od__alloc(SG_context * pCtx, const char * szGidObject, ObjectData ** ppNewOD)
{
	ObjectData * pOD = NULL;

	SG_ERR_CHECK(  SG_alloc(pCtx, 1,sizeof(ObjectData),&pOD)  );
	SG_ERR_CHECK(  SG_strcpy(pCtx,
							 pOD->bufGidObject,
							 SG_NrElements(pOD->bufGidObject),
							 szGidObject)  );

	*ppNewOD = pOD;
	return;

fail:
	SG_ERR_IGNORE(  _od__free(pCtx, pOD)  );
}

//////////////////////////////////////////////////////////////////

/**
 * create (depth,ObjectGID) key and add entry to work-queue.
 */
static void _td__add_to_work_queue(SG_context * pCtx, SG_treediff2 * pTreeDiff, ObjectData * pOD)
{
	char buf[SG_GID_BUFFER_LENGTH + 20];

	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,
									 buf,SG_NrElements(buf),
									 "%08d.%s",
									 pOD->minDepthInTree,pOD->bufGidObject)  );

#if TRACE_TREEDIFF
	SG_console(pCtx,
			   SG_CS_STDERR,
			   "TD_ADQUE [GID %s][minDepth %d] type[%d,%d,%d] depth[%d,%d]\n",
			   pOD->bufGidObject,pOD->minDepthInTree,
			   (int)((pOD->apInst[Ndx_Orig]) ? (int)pOD->apInst[Ndx_Orig]->typeInst : -1),
			   (int)((pOD->apInst[Ndx_Dest]) ? (int)pOD->apInst[Ndx_Dest]->typeInst : -1),
			   (int)((pOD->apInst[Ndx_Pend]) ? (int)pOD->apInst[Ndx_Pend]->typeInst : -1),
			   ((pOD->apInst[Ndx_Orig]) ? pOD->apInst[Ndx_Orig]->depthInTree : -1),
			   ((pOD->apInst[Ndx_Dest]) ? pOD->apInst[Ndx_Dest]->depthInTree : -1));
	SG_ERR_DISCARD;
#endif

	SG_ERR_CHECK_RETURN(  SG_rbtree__add__with_assoc(pCtx,pTreeDiff->prbWorkQueue,buf,pOD)  );
}

/**
 * Is this object a candidate for short-circuit evaluation?  That is, can we
 * avoid diving into this pair of folders and recursively comparing everything
 * within.
 *
 * We must have a pair of unscanned folders with equal content HIDs.  If one has
 * already been scanned, we must scan the other so that the children don't look
 * like peerless objects.  (We either scan neither or both.)
 *
 * If we have pendingtree data, we don't know how much of the tree it populated.
 * If the dsFlags in the pendingtree version indicate a change *on* the folder
 * (such as a rename/move), we can still allow the short-circuit; only if it
 * indicates a change in the stuff *within* the folder do we force it to continue.
 * This is another instance of the accidental peerless problem.
 */
static void _td__can_short_circuit_from_work_queue(SG_UNUSED_PARAM( SG_context * pCtx ),
												   const ObjectData * pOD,
												   SG_bool * pbCanShortCircuit)
{
	SG_bool bEqual;

	SG_UNUSED( pCtx );
	if (!pOD->apInst[Ndx_Orig])
		goto no;
	if (!pOD->apInst[Ndx_Dest])
		goto no;

	if (pOD->apInst[Ndx_Orig]->typeInst != OD_TYPE_UNSCANNED_FOLDER)
		goto no;
	if (pOD->apInst[Ndx_Dest]->typeInst != OD_TYPE_UNSCANNED_FOLDER)
		goto no;

	bEqual = (0 == (strcmp(pOD->apInst[Ndx_Orig]->bufHidContent,
						   pOD->apInst[Ndx_Dest]->bufHidContent)));
	if (!bEqual)
		goto no;

	if (pOD->apInst[Ndx_Pend])
		if (pOD->apInst[Ndx_Pend]->dsFlags & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE)
			goto no;

	*pbCanShortCircuit = SG_TRUE;
	return;

no:
	*pbCanShortCircuit = SG_FALSE;
}

/**
 *
 */
static void _td__remove_from_work_queue(SG_context * pCtx,
										SG_treediff2 * pTreeDiff,
										ObjectData * pOD,
										SG_uint32 depthInQueue)
{
	char buf[SG_GID_BUFFER_LENGTH + 20];

	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,
									 buf,SG_NrElements(buf),
									 "%08d.%s",
									 depthInQueue,pOD->bufGidObject)  );

#if TRACE_TREEDIFF
	SG_console(pCtx,
			   SG_CS_STDERR,
			   "TD_RMQUE [GID %s][minDepth %d] (short circuit)\n",
			   pOD->bufGidObject,depthInQueue);
	SG_ERR_DISCARD;
#endif

	SG_ERR_CHECK_RETURN(  SG_rbtree__remove(pCtx,pTreeDiff->prbWorkQueue,buf)  );
}

/**
 *
 */
static void _td__assert_in_work_queue(SG_context * pCtx,
									  SG_treediff2 * pTreeDiff,
									  ObjectData * pOD,
									  SG_uint32 depthInQueue)
{
	char buf[SG_GID_BUFFER_LENGTH + 20];
	SG_bool bFound;
	ObjectData * pOD_test;

	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,
									 buf,SG_NrElements(buf),
									 "%08d.%s",
									 depthInQueue,pOD->bufGidObject)  );

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx, pTreeDiff->prbWorkQueue,buf,&bFound,(void **)&pOD_test)  );
	SG_ASSERT_RELEASE_RETURN2(  (bFound),
						(pCtx,
						 "Object [GID %s][name '%s' '%s'][depth %d] should have been in work-queue.",
						 pOD->bufGidObject,
						 SG_string__sz(pOD->apInst[Ndx_Orig]->pStrEntryName),
						 SG_string__sz(pOD->apInst[Ndx_Dest]->pStrEntryName),
						 depthInQueue)  );
}

/**
 * remove the first item in the work-queue and return it for processing.
 *
 * You DO NOT own the returned ObjectData pointer.
 */
static void _td__remove_first_from_work_queue(SG_context * pCtx,
											  SG_treediff2 * pTreeDiff,
											  SG_bool * pbFound,
											  ObjectData ** ppOD)
{
	const char * szKey;
	ObjectData * pOD;
	SG_bool bFound;

	SG_ERR_CHECK_RETURN(  SG_rbtree__iterator__first(pCtx,
													 NULL,pTreeDiff->prbWorkQueue,
													 &bFound,
													 &szKey,(void **)&pOD)  );
	if (bFound)
	{
		SG_ERR_CHECK_RETURN(  SG_rbtree__remove(pCtx, pTreeDiff->prbWorkQueue,szKey)  );

#if TRACE_TREEDIFF
		SG_console(pCtx,SG_CS_STDERR,"TD_RMQUE %s (head)\n",szKey);
		SG_ERR_DISCARD;
#endif
	}

	*pbFound = bFound;
	*ppOD = ((bFound) ? pOD : NULL);
}

//////////////////////////////////////////////////////////////////


/**
 * Compute depth-in-tree values for baseline entries.  This is only
 * needed when we populated the baseline column from the pendingtree
 * data (because we were able to avoid hitting the disk to fetch
 * baseline treenodes because the pendingtree code already had the
 * information).
 *
 * WE ASSUME: the baseline/pendingtree entries were supplied by
 * sg_pendingtree (during __annotate__) and that while sg_pendingtree
 * may not tell about the entire baseline (rather only the parts that
 * have changed in the pendingtree) that for each entry that it does
 * tell us about, it has also told us about ***all*** of its parents
 * in the baseline (which seems reasonable since the content HIDs
 * will have changed for the parent folders).
 */
static void _td__fixup_od_inst_depth(SG_context * pCtx,
									 SG_treediff2 * pTreeDiff,
									 ObjectData * pOD,
									 Ndx_Inst ndx)
{
	OD_Inst * pInst_self;
	ObjectData * pOD_parent;
	OD_Inst * pInst_parent;
	SG_bool bFound;

	pInst_self = pOD->apInst[ndx];

	if (!pInst_self)			// TODO should this be an error since we only
		return;					// TODO get called explicitly for nodes we want (rather than in a blanket foreach)

	if (pInst_self->depthInTree != -1)		// already have the answer
		return;

	if (!*pInst_self->bufParentGid)			// if no parent, we are root
	{
		pInst_self->depthInTree = 1;
		return;
	}

	SG_ERR_CHECK(  SG_rbtree__find(pCtx,
								   pTreeDiff->prbObjectDataAllocated,
								   pInst_self->bufParentGid,
								   &bFound,
								   (void **)&pOD_parent)  );
	SG_ASSERT_RELEASE_FAIL2(  (bFound),
					  (pCtx,
					   "Object [GID %s][name %s] parent [Gid %s] not in cache.\n",
					   pOD->bufGidObject,SG_string__sz(pInst_self->pStrEntryName),
					   pInst_self->bufParentGid)  );

	pInst_parent = pOD_parent->apInst[ndx];

#if defined(DEBUG)
	if (!pInst_parent)
	{
		// TODO remove this hack....
		// HACK there are a couple of cases where the parent isn't in the cache until
		// HACK i fix something in sg_pendingtree.  until then, fake a path so u0043
		// HACK doesn't crash.

		pInst_self->depthInTree = -2;
		return;
	}
#endif

	SG_ASSERT_RELEASE_FAIL2(  (pInst_parent),
					  (pCtx,
					   "Object [GID %s][name %s][ndx %d] parent [Gid %s] not in cache.\n",
					   pOD->bufGidObject,SG_string__sz(pInst_self->pStrEntryName),ndx,
					   pInst_self->bufParentGid)  );

	SG_ERR_CHECK(  _td__fixup_od_inst_depth(pCtx,pTreeDiff,pOD_parent,ndx)  );

	pInst_self->depthInTree = pInst_parent->depthInTree + 1;

	return;

fail:
	return;
}

/**
 * Visit all entries in the Objects and compute the depth for this column.
 * This is only needed when:
 * [1] the baseline column was populated by __annotate__ (via pending-tree)
 *     and didn't get proper depth values
 * [2] *and* we are using the baseline in a JOIN (between an arbitrary
 *     cset and the pendint-tree) and need to do the full scan.
 */
static void _td__fixup_depths(SG_context * pCtx, SG_treediff2 * pTreeDiff, Ndx_Inst ndx)
{
	SG_rbtree_iterator * pIter = NULL;
	SG_bool bFound;
	ObjectData * pOD;
	const char * szKey;

	// TODO decide if we can just iterate on the ones in prbRawSummary -- maybe
	// TODO have this as a flag.
	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,
											  &pIter,pTreeDiff->prbObjectDataAllocated,
											  &bFound,&szKey,(void **)&pOD)  );
	while (bFound)
	{
		if (pOD->apInst[ndx])
			SG_ERR_CHECK(  _td__fixup_od_inst_depth(pCtx,pTreeDiff,pOD,ndx)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter,&bFound,&szKey,(void **)&pOD)  );
	}

	// fall thru to common cleanup

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}

//////////////////////////////////////////////////////////////////

/**
 * Stuff instance data for a version of this file/folder into the corresponding column.
 * This must be either Ndx_Orig or Ndx_Dest.
 */
static void _td__set_od_column(SG_context * pCtx,
							   SG_treediff2 * pTreeDiff,
							   ObjectData * pOD,
							   SG_uint32 ndx,
							   const SG_treenode_entry * pTreenodeEntry,
							   const ObjectData * pOD_parent)
{
	const char * szEntryname;
	OD_Inst * pInst;
	const OD_Inst * pInst_other;
	const OD_Inst * pInst_pend;
	const OD_Inst * pInst_parent;
	SG_uint32 ndx_other;
	const char * sz;
	SG_bool bAlreadyPresent;

	SG_ASSERT_RELEASE_FAIL(  ((ndx == Ndx_Orig) || (ndx == Ndx_Dest))  );
	ndx_other = ! ndx;		// we assume Ndx_Orig==0 and Ndx_Dest==1.

	SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx, pTreenodeEntry,&szEntryname)  );

	bAlreadyPresent = (pOD->apInst[ndx] != NULL);
	if (bAlreadyPresent)
	{
		// we already have info for this GID in this column.  this could either mean
		// that the entry appears more than once in the tree (a VERY BAD THING) *or*
		// that the __pendingtree_annotate_cb() gave us info for this column.  the
		// latter case is ok, but we verify which is which.
		//
		// in the latter case, the Ndx_Pend column will also be set.
		//
		// we only include the entryname in this assert, because the repo-path has
		// not been computed yet.

		SG_ASSERT_RELEASE_FAIL2(  (pOD->apInst[Ndx_Pend] != NULL),
						  (pCtx,
						   "Object [GID %s][name %s] occurs in more than once place in this version [ndx %d] of the tree (and Ndx_Pend is not set).",
						   pOD->bufGidObject,szEntryname,ndx)  );

		// verify/supplement information that pending-tree gave us with stuff that we
		// got directly from the repo, if necessary.

		SG_ASSERT_RELEASE_FAIL2(  (strcmp(szEntryname,SG_string__sz(pOD->apInst[ndx]->pStrEntryName)) == 0),
						  (pCtx,
						   "Object [GID %s][name '%s' '%s'] changed in [ndx %d].",
						   pOD->bufGidObject,
						   szEntryname,SG_string__sz(pOD->apInst[ndx]->pStrEntryName),
						   ndx)  );


		// TODO verify other fields if i feel like it.

		// fall thru and let the rest of the function overwrite the other fields.
		// this shouldn't hurt anything.
	}
	else
	{
		SG_ERR_CHECK(  _od_inst__alloc(pCtx,szEntryname,&pOD->apInst[ndx])  );
	}

	pInst = pOD->apInst[ndx];

	pInst_parent = ((pOD_parent) ? pOD_parent->apInst[ndx] : NULL);

	pInst_other  = pOD->apInst[ndx_other];			// the peer to this object in cset0-vs-cset1
	pInst_pend   = pOD->apInst[Ndx_Pend];			// info from pending-tree for wd relative to baseline

	// an object can NEVER change types (ie from file to symlink or folder)

	SG_ERR_CHECK(  SG_treenode_entry__get_entry_type(pCtx, pTreenodeEntry,&pInst->tneType)  );
	if (pInst_other)
		SG_ASSERT_RELEASE_FAIL2(  (pInst->tneType == pInst_other->tneType),
						  (pCtx,
						   "Object [GID %s][name %s][ndx %d] changed type [from %d][to %d] vs [ndx_other %d]",
						   pOD->bufGidObject,szEntryname,ndx,
						   pInst->tneType,pInst_other->tneType,
						   ndx_other)  );
	if (pInst_pend)
		SG_ASSERT_RELEASE_FAIL2(  (pInst->tneType == pInst_pend->tneType),
						  (pCtx,
						   "Object [GID %s][name %s][ndx %d] changed type [from %d][to %d] in WD.",
						   pOD->bufGidObject,szEntryname,ndx,
						   pInst->tneType,pInst_pend->tneType)  );

	SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, pTreenodeEntry,&sz)  );
	SG_ERR_CHECK(  SG_strcpy(pCtx, pInst->bufHidContent,SG_NrElements(pInst->bufHidContent),sz)  );

	SG_ERR_CHECK(  SG_treenode_entry__get_hid_xattrs(pCtx, pTreenodeEntry,&sz)  );
	if (sz && *sz)
		SG_ERR_CHECK(  SG_strcpy(pCtx, pInst->bufHidXAttrs,SG_NrElements(pInst->bufHidXAttrs),sz)  );

	if (pOD_parent)
		SG_ERR_CHECK(  SG_strcpy(pCtx, pInst->bufParentGid,SG_NrElements(pInst->bufParentGid),pOD_parent->bufGidObject)  );

	SG_ERR_CHECK(  SG_treenode_entry__get_attribute_bits(pCtx, pTreenodeEntry,&pInst->attrBits)  );

	pInst->dsFlags = 0;		// we can't compute these until (potentially) both sides have been scanned.

	SG_ERR_CHECK(  _td__fixup_od_inst_depth(pCtx,pTreeDiff,pOD,ndx)  );

#if TRACE_TREEDIFF
	SG_console(pCtx,
			   SG_CS_STDERR,
			   "TD_SETCOL[%d]%*c[GID %s][name %s] [tneType %d][depth %d]\n",
			   ndx,
			   pInst->depthInTree,'\t',
			   pOD->bufGidObject,
			   szEntryname,
			   (SG_uint32)pInst->tneType,
			   pInst->depthInTree);
	SG_ERR_DISCARD;
#endif

	if (!pInst_other)
		pOD->minDepthInTree = pInst->depthInTree;
	else
		pOD->minDepthInTree = SG_MIN(pInst->depthInTree,pInst_other->depthInTree);

	switch (pInst->tneType)
	{
	default:
		SG_ASSERT_RELEASE_FAIL2(  (0),
						  (pCtx,
						   "Object [GID %s][name %s][ndx %d] of unknown type [%d].",
						   pOD->bufGidObject,szEntryname,ndx,(SG_uint32)pInst->tneType)  );
		break;

	case SG_TREENODEENTRY_TYPE_REGULAR_FILE:
		pInst->typeInst = OD_TYPE_FILE;
		break;

	case SG_TREENODEENTRY_TYPE_SYMLINK:
		pInst->typeInst = OD_TYPE_SYMLINK;
		break;

	case SG_TREENODEENTRY_TYPE_DIRECTORY:
		pInst->typeInst = OD_TYPE_UNSCANNED_FOLDER;
		if (!pInst_other)
		{
			// this is the first time that an instance has been populated for this
			// Object GID in either Ndx_Orig or Ndx_Dest.
			//
			// add the folder to the work-queue so that a later iteration of the main
			// loop will process the contents *within* the folder -- IF NECESSARY.

			SG_ERR_CHECK(  _td__add_to_work_queue(pCtx,pTreeDiff,pOD)  );
		}
		else if (pInst_other->typeInst == OD_TYPE_PENDINGTREE_FOLDER)
		{
			// we are in Ndx_Orig and the only reason pInst_other was set was
			// because of the info from pending-tree.  this is the first column
			// as far as adding it to the work-queue is concerned.

			SG_ERR_CHECK(  _td__add_to_work_queue(pCtx,pTreeDiff,pOD)  );
		}
		else if (pInst_other->typeInst == OD_TYPE_UNSCANNED_FOLDER)
		{
			// we are the second column (of Ndx_Orig and Ndx_Dest) that has been seen
			// properly (not counting Ndx_Pend and not counting the OD_TYPE_PENDINGTREE_FOLDER
			// data).  that is, the second column that we actually got the data directly from
			// the repo for.  the other side should have added an entry to the work-queue for
			// us.  short-circuit-eval if we can.

			SG_bool bCanShortCircuit;

			SG_ERR_CHECK(  _td__can_short_circuit_from_work_queue(pCtx,pOD,&bCanShortCircuit)  );
			if (bCanShortCircuit)
				SG_ERR_CHECK(  _td__remove_from_work_queue(pCtx,pTreeDiff,pOD,pInst_other->depthInTree)  );
			else
				SG_ERR_CHECK(  _td__assert_in_work_queue(pCtx,pTreeDiff,pOD,pInst_other->depthInTree)  );
		}
		else /* pInst_other->typeInst == OD_TYPE_SCANNED_FOLDER */
		{
			// we are the second column -- and the other side has already been scanned.
			// this usually only happens when a folder is moved deeper/shallower into the
			// tree.
			//
			// force scan on this side so that the children of the other side don't look
			// artificially peerless.

			SG_ERR_CHECK(  _td__add_to_work_queue(pCtx,pTreeDiff,pOD)  );
		}
		break;
	}

	return;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

static void _sg_treediff2__free_treenode(SG_UNUSED_PARAM(SG_context * pCtx), void * pVoid)
{
	SG_UNUSED(pCtx);
	SG_treenode__free(pCtx, (SG_treenode *)pVoid);
}

void SG_treediff2__free_with_error(SG_context * pCtx, SG_treediff2 * pTreeDiff, SG_bool bForce)
{
	if (!pTreeDiff)
		return;

#ifndef DEBUG
	SG_ASSERT_RELEASE_RETURN2(bForce, (pCtx, "SG_treediff2__free_with_error requires bForce==TRUE in production mode."));
#endif

	if (pTreeDiff->pTreeDiff_cache_owner == pTreeDiff)
	{
		// we are the owner of the cache and responsible for freeing it.
		// assert that no one else is using it.
		//
		// normally, __free() routines can't return an error value and always try
		// to do the free (usually true, since SG_free() and the OS free() don't
		// ever fail).

		if (pTreeDiff->nrShares > 1)
		{
#if TRACE_TREEDIFF
			SG_ERR_IGNORE(  SG_console(pCtx,
									   SG_CS_STDERR,
									   "SG_treediff2__free_with_error: Owner Assert [nrShares %d] should be 1.\n",
									   pTreeDiff->nrShares)  );
#endif

#ifdef DEBUG
			if (!bForce)
			{
				// we deviate from that model and allow an error to be thrown (when bForce
				// is false).  this is mainly to help the unittests.

				SG_ERR_THROW_RETURN(SG_ERR_ASSERT);
			}
#else
			SG_UNUSED(bForce);
#endif
		}

		SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pTreeDiff->prbTreenodesAllocated,_sg_treediff2__free_treenode);
		pTreeDiff->nrShares = 0;
	}
	else if (pTreeDiff->pTreeDiff_cache_owner)			// if init completed
	{
		// we are using the cache from another treediff.  decrement the
		// crude reference count.  again, we can't return a value or fail,
		// so we just log an error and continue.

		if (pTreeDiff->pTreeDiff_cache_owner->nrShares <= 1)
		{
#if TRACE_TREEDIFF
			SG_ERR_IGNORE(  SG_console(pCtx,
					   SG_CS_STDERR,
					   "SG_treediff2__free_with_error: Non-Owner Assert [nrShares %d] should be > 1.\n",
					   pTreeDiff->pTreeDiff_cache_owner->nrShares)  );
#endif

#ifdef DEBUG
			if (!bForce)
				SG_ERR_THROW_RETURN(SG_ERR_ASSERT);
#endif
		}

		pTreeDiff->pTreeDiff_cache_owner->nrShares--;
		pTreeDiff->pTreeDiff_cache_owner = NULL;
		pTreeDiff->prbTreenodesAllocated = NULL;
	}

	SG_PATHNAME_NULLFREE(pCtx, pTreeDiff->pPathWorkingDirectoryTop);

	SG_CHANGESET_NULLFREE(pCtx, pTreeDiff->aCSetColumns[Ndx_Orig].pcsCSet);
	SG_CHANGESET_NULLFREE(pCtx, pTreeDiff->aCSetColumns[Ndx_Dest].pcsCSet);
	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pTreeDiff->prbObjectDataAllocated,_od__free__cb);
	SG_RBTREE_NULLFREE(pCtx, pTreeDiff->prbWorkQueue);
	SG_RBTREE_NULLFREE(pCtx, pTreeDiff->prbRawSummary);
	SG_REPO_NULLFREE(pCtx, pTreeDiff->pRepo);

	SG_NULLFREE(pCtx, pTreeDiff);
}

void SG_treediff2__free(SG_context * pCtx, SG_treediff2 * pTreeDiff)
{
	SG_ERR_IGNORE(  SG_treediff2__free_with_error(pCtx, pTreeDiff,SG_TRUE)  );
}

void SG_treediff2__alloc__shared(SG_context * pCtx,
								 SG_repo * pRepo,	// we open our own copy of the repo using this as a reference.
								 SG_treediff2 * pTreeDiff_cache_owner,
								 SG_treediff2 ** ppTreeDiff)
{
	SG_treediff2 * pTreeDiff = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(ppTreeDiff);

	SG_ERR_CHECK(  SG_alloc(pCtx, 1,sizeof(SG_treediff2),&pTreeDiff)  );

	if (pTreeDiff_cache_owner)
	{
		// we are given a cache owner, reference it.  do an addRef on it.
		// grab the cache pointer.

		pTreeDiff->pTreeDiff_cache_owner = pTreeDiff_cache_owner;
		pTreeDiff->pTreeDiff_cache_owner->nrShares++;
		pTreeDiff->prbTreenodesAllocated = pTreeDiff_cache_owner->prbTreenodesAllocated;
	}
	else
	{
		// we are a new cache owner.  link to ourself.
		// allocate a new cache.

		pTreeDiff->pTreeDiff_cache_owner = pTreeDiff;
		pTreeDiff->nrShares = 1;
		SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pTreeDiff->prbTreenodesAllocated)  );
	}

	SG_ERR_CHECK(  SG_repo__open_repo_instance__copy(pCtx, pRepo, &pTreeDiff->pRepo)  );

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pTreeDiff->prbObjectDataAllocated)  );
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pTreeDiff->prbWorkQueue)  );
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pTreeDiff->prbRawSummary)  );

	*ppTreeDiff = pTreeDiff;
	return;

fail:
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
}

void SG_treediff2__alloc(SG_context * pCtx,
						 SG_repo * pRepo,
						 SG_treediff2 ** ppTreeDiff)
{
	SG_ERR_CHECK_RETURN(  SG_treediff2__alloc__shared(pCtx,pRepo,NULL,ppTreeDiff)  );
}

//////////////////////////////////////////////////////////////////

/**
 * Find the treenode for the given HID in the allocated treenode cache or load it
 * from disk and put it in the cache.
 *
 * This can fail if the repo is sparse.
 *
 * The treenode cache owns this treenode.  Do not free it directly.
 */
static void _td__load_treenode(SG_context * pCtx, SG_treediff2 * pTreeDiff, const char * szHid, const SG_treenode ** ppTreenode)
{
	SG_treenode * pTreenodeAllocated = NULL;
	SG_treenode * pTreenode;
	SG_bool bFound;

	SG_ERR_CHECK(  SG_rbtree__find(pCtx, pTreeDiff->prbTreenodesAllocated,szHid,&bFound,(void **)&pTreenode)  );
	if (!bFound)
	{
		SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pTreeDiff->pRepo,szHid,&pTreenodeAllocated)  );
		pTreenode = pTreenodeAllocated;
		SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pTreeDiff->prbTreenodesAllocated,szHid,pTreenodeAllocated)  );
		pTreenodeAllocated = NULL;		// rbtree now owns the thing we just allocated.
	}

	*ppTreenode = pTreenode;
	return;

fail:
	SG_TREENODE_NULLFREE(pCtx, pTreenodeAllocated);
}

//////////////////////////////////////////////////////////////////

/**
 * Find the ObjectData (ROW) for this Object GID from the allocated cache or
 * create one for it.
 *
 * The cache owns this pointer; do not free it directly.
 */
static void _td__load_object_data(SG_context * pCtx,
								  SG_treediff2 * pTreeDiff,
								  const char * szGidObject,
								  ObjectData ** ppObjectData,
								  SG_bool * pbCreated)
{
	ObjectData * pODAllocated = NULL;
	ObjectData * pOD;
	SG_bool bFound;

	SG_ERR_CHECK(  SG_rbtree__find(pCtx, pTreeDiff->prbObjectDataAllocated,szGidObject,&bFound,(void **)&pOD)  );
	if (!bFound)
	{
		SG_ERR_CHECK(  _od__alloc(pCtx, szGidObject,&pODAllocated)  );
		pOD = pODAllocated;
		SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pTreeDiff->prbObjectDataAllocated,szGidObject,pODAllocated)  );
		pODAllocated = NULL;		// rbtree now owns the thing we just allocated
	}

	*ppObjectData = pOD;
	if (pbCreated)
		*pbCreated = !bFound;

	return;

fail:
	SG_ERR_IGNORE(  _od__free(pCtx, pODAllocated)  );
}

//////////////////////////////////////////////////////////////////

/**
 * load the cset from the repo and remember the HID of the cset in our tables.
 */
static void _td__load_changeset(SG_context * pCtx, SG_treediff2 * pTreeDiff, const char * szHidCSet, SG_uint32 ndx)
{
	SG_ASSERT_RELEASE_FAIL(  ((ndx == Ndx_Orig) || (ndx == Ndx_Dest))  );

	// load changeset from disk and put it in our cset cache.

	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pTreeDiff->pRepo,szHidCSet,&pTreeDiff->aCSetColumns[ndx].pcsCSet)  );

	// remember the CSet's HID in this column.

	SG_ERR_CHECK(  SG_strcpy(pCtx,
							 pTreeDiff->aCSetColumns[ndx].bufHidCSet,
							 SG_NrElements(pTreeDiff->aCSetColumns[ndx].bufHidCSet),
							 szHidCSet)  );

	return;

fail:
	return;
}

/**
 * Process the changeset associated with the given NDX.
 * Get the super-root treenode and queue up the
 * user's actual "@" root directory for processing.
 */
static void _td__process_changeset(SG_context * pCtx, SG_treediff2 * pTreeDiff, SG_uint32 ndx)
{
	SG_changeset * pcs;
	const char * szHidSuperRoot;
	const SG_treenode * pTreenodeSuperRoot;
	const char * szGidObjectRoot = NULL;
	const char * szEntrynameRoot;
	const SG_treenode_entry * pTreenodeEntryRoot;
	SG_treenode_entry_type tneTypeRoot;
	SG_uint32 nrEntries;
	ObjectData * pODRoot;

	// lookup the HID of the super-root treenode from the changeset.

	SG_ASSERT_RELEASE_FAIL(  ((ndx == Ndx_Orig) || (ndx == Ndx_Dest))  );
	pcs = pTreeDiff->aCSetColumns[ndx].pcsCSet;
	SG_ASSERT_RELEASE_FAIL(  (pcs)  );

	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs,&szHidSuperRoot)  );
	if (!szHidSuperRoot || !*szHidSuperRoot)
		SG_ERR_THROW(  SG_ERR_MALFORMED_SUPERROOT_TREENODE  );

	// use the super-root treenode HID to load the treenode from disk
	// and add to the treenode cache (or just fetch it from the cache
	// if already present).

	SG_ERR_CHECK(  _td__load_treenode(pCtx, pTreeDiff,szHidSuperRoot,&pTreenodeSuperRoot)  );

	// verify we have a well-formed super-root.  that is, the super-root treenode
	// should have exactly 1 treenode-entry -- the "@" directory.

	SG_ERR_CHECK(  SG_treenode__count(pCtx, pTreenodeSuperRoot,&nrEntries)  );
	if (nrEntries != 1)
		SG_ERR_THROW(  SG_ERR_MALFORMED_SUPERROOT_TREENODE  );
	SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx,
													   pTreenodeSuperRoot,0,
													   &szGidObjectRoot,&pTreenodeEntryRoot)  );
	SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx, pTreenodeEntryRoot,&szEntrynameRoot)  );
	SG_ERR_CHECK(  SG_treenode_entry__get_entry_type(pCtx, pTreenodeEntryRoot,&tneTypeRoot)  );
	if ( (strcmp(szEntrynameRoot,"@") != 0) || (tneTypeRoot != SG_TREENODEENTRY_TYPE_DIRECTORY) )
		SG_ERR_THROW(  SG_ERR_MALFORMED_SUPERROOT_TREENODE  );

	// add/lookup row in the Object GID map.
	// and bind the column to our instance data.
	// since we the user's root doesn't have a parent, we set the Parent Object GID to NULL.

	SG_ERR_CHECK(  _td__load_object_data(pCtx, pTreeDiff,szGidObjectRoot,&pODRoot,NULL)  );
	SG_ERR_CHECK(  _td__set_od_column(pCtx, pTreeDiff,pODRoot,ndx,pTreenodeEntryRoot,NULL)  );

	return;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * ObjectData is a ROW containing one or more OD_Inst (instances) that each
 * contain a version of a sub-directory.  Here we are asked to load
 * the treenode for the sub-directory (using the treenode-entry blob HID)
 * and scan the contents.
 *
 * During the (directory) scan, we add ROWS to the ObjectData rows for each
 * file/sub-directory contained within the given sub-directory.
 *
 * We read the treenode entry from the repo, so we can only be called
 * on Ndx_Orig and Ndx_Dest.
 *
 * We do NOT go recursive.  Our job is to simply process the contents
 * of this directory.
 *
 * We only do this for one instance; only the caller knows if the peer
 * should be scanned too.
 */
static void _td__scan_folder_for_column(SG_context * pCtx,
										SG_treediff2 * pTreeDiff,
										ObjectData * pOD,
										SG_uint32 ndx)
{
	const SG_treenode * pTreenode;
	SG_uint32 kEntry, nrEntries;
	const char * szGidObject_child_k = NULL;
	const SG_treenode_entry * pTreenodeEntry_child_k;
	ObjectData * pOD_child_k;

	SG_ASSERT_RELEASE_FAIL(  ((ndx == Ndx_Orig) || (ndx == Ndx_Dest))  );

	if (   (pOD->apInst[ndx]->typeInst != OD_TYPE_UNSCANNED_FOLDER)
		&& (pOD->apInst[ndx]->typeInst != OD_TYPE_PENDINGTREE_FOLDER))
		return;

	pOD->apInst[ndx]->typeInst = OD_TYPE_SCANNED_FOLDER;

#if TRACE_TREEDIFF
	SG_console(pCtx,
			   SG_CS_STDERR,
			   "TD_PROCESS[%d]%*c%s (%s)\n",
			   ndx,
			   pOD->apInst[ndx]->depthInTree,'\t',
			   pOD->bufGidObject,
			   SG_string__sz(pOD->apInst[ndx]->pStrEntryName));
	SG_ERR_DISCARD;
#endif

	SG_ERR_CHECK(  _td__load_treenode(pCtx, pTreeDiff,pOD->apInst[ndx]->bufHidContent,&pTreenode)  );

	SG_ERR_CHECK(  SG_treenode__count(pCtx, pTreenode,&nrEntries)  );
	for (kEntry=0; kEntry<nrEntries; kEntry++)
	{
		SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx,
														   pTreenode,
														   kEntry,
														   &szGidObject_child_k,
														   &pTreenodeEntry_child_k)  );
		SG_ERR_CHECK(  _td__load_object_data(pCtx, pTreeDiff,szGidObject_child_k,&pOD_child_k,NULL)  );
		SG_ERR_CHECK(  _td__set_od_column(pCtx, pTreeDiff,pOD_child_k,ndx,pTreenodeEntry_child_k,pOD)  );

	}

	return;

fail:
	return;
}

/**
 * The work-queue contains folders that we need to visit.  We remove the first
 * entry in the queue and scan single/pair of directories and decide what to do
 * with each of the entries within them.  This may cause new items to be added
 * to the work-queue (for sub-directories), so we can't use a normal iterator.
 *
 * By "scan", I mean load the treeenode from the REPO and look at each of the
 * entries.  We do not have anything to do with the WD or pending-tree.
 *
 * We loop until we reach a kind of closure where we have visited as much of
 * the 2 versions of the tree as we need to.
 */
static void _td__compute_closure(SG_context * pCtx, SG_treediff2 * pTreeDiff)
{
	ObjectData * pOD = NULL;
	SG_bool bFound = SG_FALSE;

	while (1)
	{
		// remove the first item in the work-queue.  since this is depth-in-the-tree
		// sorted, this will be the shallowest thing that we haven't completely
		// scanned yet.
		//
		// we can have 1 or 2 columns populated.  (and because the user can
		// move a folder deeper in the tree) we don't know if a peerless one is
		// a temporary thing or not.)

		SG_ERR_CHECK_RETURN(  _td__remove_first_from_work_queue(pCtx,pTreeDiff,&bFound,&pOD)  );
		if (!bFound)
			return;

		if (pOD->apInst[Ndx_Orig])
			SG_ERR_CHECK_RETURN(  _td__scan_folder_for_column(pCtx,pTreeDiff,pOD,Ndx_Orig)  );

		if (pOD->apInst[Ndx_Dest])
			SG_ERR_CHECK_RETURN(  _td__scan_folder_for_column(pCtx,pTreeDiff,pOD,Ndx_Dest)  );
	}
}

//////////////////////////////////////////////////////////////////

/**
 * add this Object Data to the Raw Summary.
 */
static void _td__add_to_raw_summary(SG_context * pCtx, SG_treediff2 * pTreeDiff, ObjectData * pOD)
{
	SG_ERR_CHECK_RETURN(  SG_rbtree__add__with_assoc(pCtx,pTreeDiff->prbRawSummary,pOD->bufGidObject,pOD)  );
}

//////////////////////////////////////////////////////////////////

/**
 * compute the dsFlags value for the differences in the versions of
 * this entry in Ndx_Orig and Ndx_Dest.
 */
static void _od_inst__compute_cset0_cset1_dsFlags(SG_context * pCtx, ObjectData * pOD)
{
	SG_diffstatus_flags dsFlags = SG_DIFFSTATUS_FLAGS__ZERO;

	if (pOD->apInst[Ndx_Orig] && pOD->apInst[Ndx_Dest])
	{
		// entry is present in both versions of the tree.  we need to do a simple diff.

		SG_ASSERT_RELEASE_FAIL(  (pOD->apInst[Ndx_Orig]->dsFlags == SG_DIFFSTATUS_FLAGS__ZERO)  );
		SG_ASSERT_RELEASE_FAIL(  (pOD->apInst[Ndx_Dest]->dsFlags == SG_DIFFSTATUS_FLAGS__ZERO)  );
		SG_ASSERT_RELEASE_FAIL2(  (pOD->apInst[Ndx_Orig]->typeInst == pOD->apInst[Ndx_Dest]->typeInst),
						  (pCtx,
						   "Object [GID %s][name_dest %s] Invalid state [Ndx_Orig typeInst %d][Ndx_Dest typeInst %d]\n",
						   pOD->bufGidObject,SG_string__sz(pOD->apInst[Ndx_Dest]->pStrEntryName),
						   pOD->apInst[Ndx_Orig]->typeInst,pOD->apInst[Ndx_Dest]->typeInst)  );

		SG_ERR_CHECK(  _od_inst__compute_dsFlags_from_pair(pCtx,pOD,&dsFlags)  );
	}
	else if (pOD->apInst[Ndx_Orig])
	{
		// entry is present in cset0 but not in cset1.  this is a delete.  create an
		// entry in Ndx_Dest with __DELETED status (so that column is self-sufficient
		// and consistent with the way that a pending-tree-type diff is constructed).

		SG_ASSERT_RELEASE_FAIL(  (pOD->apInst[Ndx_Orig]->dsFlags == SG_DIFFSTATUS_FLAGS__ZERO)  );
		SG_ASSERT_RELEASE_FAIL2(  (   (pOD->apInst[Ndx_Orig]->typeInst == OD_TYPE_FILE)
						   || (pOD->apInst[Ndx_Orig]->typeInst == OD_TYPE_SYMLINK)
						   || (pOD->apInst[Ndx_Orig]->typeInst == OD_TYPE_SCANNED_FOLDER)),
						  (pCtx,
						   "Object [GID %s][name_orig %s] Invalid state [typeInst %d] in Ndx_Orig when Ndx_Dest is not present\n",
						   pOD->bufGidObject,SG_string__sz(pOD->apInst[Ndx_Orig]->pStrEntryName),
						   pOD->apInst[Ndx_Orig]->typeInst)  );

		// this clone will have depth -1.  we'll fix it up later.
		SG_ERR_CHECK(  _od_inst__clone(pCtx,pOD->apInst[Ndx_Orig],&pOD->apInst[Ndx_Dest])  );
		dsFlags = SG_DIFFSTATUS_FLAGS__DELETED;		// explicitly _DELETED to cset (and not just __LOST)
	}
	else if (pOD->apInst[Ndx_Dest])
	{
		// entry is present in cset1 but not in cset0.  this is an add.

		SG_ASSERT_RELEASE_FAIL(  (pOD->apInst[Ndx_Dest]->dsFlags == SG_DIFFSTATUS_FLAGS__ZERO)  );
		SG_ASSERT_RELEASE_FAIL2(  (   (pOD->apInst[Ndx_Dest]->typeInst == OD_TYPE_FILE)
						   || (pOD->apInst[Ndx_Dest]->typeInst == OD_TYPE_SYMLINK)
						   || (pOD->apInst[Ndx_Dest]->typeInst == OD_TYPE_SCANNED_FOLDER)),
						  (pCtx,
						   "Object [GID %s][name_dest %s] Invalid state [typeInst %d] in Ndx_Dest when Ndx_Orig is not present\n",
						   pOD->bufGidObject,SG_string__sz(pOD->apInst[Ndx_Dest]->pStrEntryName),
						   pOD->apInst[Ndx_Dest]->typeInst)  );

		dsFlags = SG_DIFFSTATUS_FLAGS__ADDED;		// explicitly __ADDED to cset (and not just __FOUND)
	}
	else
	{
		// ?? entry is not present either cset0 nor cset1.  this cannot happen.
		SG_ASSERT_RELEASE_FAIL(  (0)  );
	}

	pOD->apInst[Ndx_Dest]->dsFlags = dsFlags;

	return;

fail:
	return;
}

static SG_rbtree_foreach_callback _td__build_summary__cset0_cset1_cb;

/**
 * See note before _td__build_summary__cset0_cset1().
 */
static void _td__build_summary__cset0_cset1_cb(SG_context * pCtx,
											   SG_UNUSED_PARAM(const char * szKeyObjectGID),
											   void * pVoidData,
											   void * pVoidCtx)
{
	SG_treediff2 * pTreeDiff = (SG_treediff2 *)pVoidCtx;
	ObjectData * pOD = (ObjectData *)pVoidData;

	SG_UNUSED(szKeyObjectGID);

	SG_ASSERT_RELEASE_FAIL(  (!pOD->apInst[Ndx_Pend])  );

	SG_ERR_CHECK(  _od_inst__compute_cset0_cset1_dsFlags(pCtx,pOD)  );

	// if we have a change, we moved the instance to the Ndx_Net column (so that
	// it is in a known spot) and add the row to the summary.

	if (pOD->apInst[Ndx_Dest]->dsFlags != SG_DIFFSTATUS_FLAGS__ZERO)
	{
		pOD->apInst[Ndx_Net ] = pOD->apInst[Ndx_Dest];
		pOD->apInst[Ndx_Dest] = NULL;

		SG_ERR_CHECK(  _td__add_to_raw_summary(pCtx,pTreeDiff,pOD)  );
	}

	return;

fail:
	return;
}

/**
 * Run thru the list of all ObjectData rows that we visited and identify
 * the ones that represent a change of some kind and add them to the
 * "raw summary".
 *
 * This version is used when we are doing a cset-vs-cset diff and there
 * is NO pending-tree data.
 *
 * We have version info for each entry in Ndx_Orig and in Ndx_Dest and
 * need to compute the dsFlags (stored in Ndx_Dest).
 */
static void _td__build_summary__cset0_cset1(SG_context * pCtx, SG_treediff2 * pTreeDiff)
{
	SG_ASSERT_RELEASE_RETURN(  (pTreeDiff->kind == SG_TREEDIFF2_KIND__CSET_VS_CSET)  );

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,
											 pTreeDiff->prbObjectDataAllocated,
											 _td__build_summary__cset0_cset1_cb,
											 (void *)pTreeDiff)  );
}

//////////////////////////////////////////////////////////////////

static SG_rbtree_foreach_callback _td__build_summary__cset0_pendingtree_cb;

/**
 * See note before _td__build_summary__cset0_pendingtree().
 */
static void _td__build_summary__cset0_pendingtree_cb(SG_context * pCtx,
													 SG_UNUSED_PARAM(const char * szKeyObjectGID),
													 void * pVoidData,
													 void * pVoidCtx)
{
	SG_treediff2 * pTreeDiff = (SG_treediff2 *)pVoidCtx;
	ObjectData * pOD = (ObjectData *)pVoidData;
	OD_Inst * pInstAllocated = NULL;
	SG_diffstatus_flags dsFlags_a, dsFlags_b;

	SG_UNUSED(szKeyObjectGID);

	if (!pOD->apInst[Ndx_Orig] && !pOD->apInst[Ndx_Dest])
	{
		// if we only have Ndx_Pend data, then we must have an __ADDED/__FOUND
		// object that only exists in the pending-tree.

		SG_ERR_CHECK(  _od_inst__clone(pCtx,pOD->apInst[Ndx_Pend],&pOD->apInst[Ndx_Net])  );
		SG_ERR_CHECK(  _td__add_to_raw_summary(pCtx,pTreeDiff,pOD)  );

		return;
	}

	// compute the dsFlags for cset0-vs-baseline; this allocates an Ndx_Dest version
	// to be created if not already present (as is the case in __DELETED).

	SG_ERR_CHECK(  _od_inst__compute_cset0_cset1_dsFlags(pCtx,pOD)  );
	SG_ASSERT_RELEASE_FAIL(  (pOD->apInst[Ndx_Dest])  );

	if (!pOD->apInst[Ndx_Pend])
	{
		// entry has not changed in the pending-tree.  we want to just
		// inherit the changes from cset0-vs-baseline.

		if (pOD->apInst[Ndx_Dest]->dsFlags != SG_DIFFSTATUS_FLAGS__ZERO)
		{
			// TODO for now, we clone the Ndx_Dest column and create an Ndx_Net column.
			// TODO this is for debugging at the moment.  Later, we may want to just
			// TODO move it over.
			SG_ERR_CHECK(  _od_inst__clone(pCtx,pOD->apInst[Ndx_Dest],&pOD->apInst[Ndx_Net])  );
			SG_ERR_CHECK(  _td__add_to_raw_summary(pCtx,pTreeDiff,pOD)  );
		}

		return;
	}

	// otherwise, entry changed in the pending-tree (relative to baseline)

	if (pOD->apInst[Ndx_Dest]->dsFlags == SG_DIFFSTATUS_FLAGS__ZERO)
	{
		// entry did not change between cset0 and the baseline, but did
		// between the baseline and the pendingtree.  we want to just
		// inherit the changes given to us by sg_pendingtree.

		// we can assert that the flags on Ndx_Pend are non-zero because
		// sg_pendingtree only gives us dirty ones.
		SG_ASSERT_RELEASE_FAIL(  (pOD->apInst[Ndx_Pend]->dsFlags != SG_DIFFSTATUS_FLAGS__ZERO)  );

		// TODO again, i'm cloning it here mainly for debugging purposes.
		// TODO later, let's try to just move it.
		SG_ERR_CHECK(  _od_inst__clone(pCtx,pOD->apInst[Ndx_Pend],&pOD->apInst[Ndx_Net])  );
		SG_ERR_CHECK(  _td__add_to_raw_summary(pCtx,pTreeDiff,pOD)  );

		return;
	}

	// otherwise, entry changed in cset0-vs-basline *and* baseline-vs-pendingtree.
	// we must do a join/composition.  but first, we must make sure that the pendingtree
	// changes don't undo changes made to the baseline.

	//////////////////////////////////////////////////////////////////
	// background:
	//
	// dsFlags can be divided into 2 parts:
	// [1] the MASK_MUTUALLY_EXCLUSIVE part -- this tells about changes *TO* the content of the entry.
	//     such as: none, added, deleted, modified, etc.  only one of these can be true.  (be careful
	//     because we use a value of 0 for the Z (none) case, so some of the bit tests are
	//     "==" rather than "&".)
	//
	// [2] the MASK_MODIFIERS part -- this tells about changes *ON* the entry.  such as: moved,
	//     renamed, and changes to the attributes.  zero or more of these may be set at the same
	//     time.
	//
	// note: there are some combinations that can/should not happen, such as deleting something in
	// cset0-vs-baseline and then doing anything to it in baseline-vs-pendingtree.
	//
	// here is a table of the 2 sets of MASK_ME flags.  we only need to consider the cases marked
	// with a "Y" and can exclude the others.
	//
	//         b:   | baseline-vs-pendingtree
	//     a:       | Z A D M L F
	// -------------+----------------
	//   cset     Z | Y . Y Y Y .
	//            A | Y . Y Y Y .
	//    vs      D | . . . . . .   D row not possible (see example above)
	//            M | Y . Y Y Y .
	// baseline   L | . . . . . .	L row not possible because cset-vs-cset doesn't have Lost entries
	//            F | . . . . . .   F row not possible because cset-vs-cset doesn't have Found entries
	//                  ^       ^
	//                  A       ^   A column not possible because cannot add something already present
	//                          F   F column not possible because cannot find something already present
	//////////////////////////////////////////////////////////////////

	dsFlags_a = pOD->apInst[Ndx_Dest]->dsFlags;		// cset0-vs-baseline
	dsFlags_b = pOD->apInst[Ndx_Pend]->dsFlags;		// baseline-vs-pendingtree

	SG_ASSERT_RELEASE_FAIL2(  (   ((dsFlags_a & (SG_DIFFSTATUS_FLAGS__DELETED|SG_DIFFSTATUS_FLAGS__LOST|SG_DIFFSTATUS_FLAGS__FOUND)) == 0)
					   && ((dsFlags_b & (SG_DIFFSTATUS_FLAGS__ADDED|SG_DIFFSTATUS_FLAGS__FOUND)) == 0)),
					  (pCtx,
					   "Object [GID %s][name %s] impossible case  [dsFlags_a %d][dsFlags_b %d].\n",
					   pOD->bufGidObject,
					   SG_string__sz(pOD->apInst[Ndx_Pend]->pStrEntryName),
					   (SG_uint32)dsFlags_a,(SG_uint32)dsFlags_b)  );

	switch (dsFlags_a & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE)
	{
	case SG_DIFFSTATUS_FLAGS__ZERO:					// row Z
		switch (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE)
		{
		case SG_DIFFSTATUS_FLAGS__ZERO:					// column Z   Z+Z ==> Z
		case SG_DIFFSTATUS_FLAGS__MODIFIED:				// column M   Z+M ==> M
			// start with a copy of the pendingtree version and fixup the flags
			// to be a combination of the modifiers on both.
			SG_ERR_CHECK(  _od_inst__clone(pCtx,pOD->apInst[Ndx_Pend], &pInstAllocated)  );
			pInstAllocated->dsFlags = (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE);
			goto combine_modifiers;

		case SG_DIFFSTATUS_FLAGS__DELETED:				// column D   Z+D ==> D
		case SG_DIFFSTATUS_FLAGS__LOST:					// column L   Z+L ==> L
			// we clone the original version (rather than the pendingtree version)
			// because we want stuff to be cset0-relative.  also, we skip the modifiers
			// (there shouldn't be any on the pendingtree and we don't want the ones
			// applied to the baseline either.
			SG_ERR_CHECK(  _od_inst__clone(pCtx,pOD->apInst[Ndx_Orig], &pInstAllocated)  );
			pInstAllocated->dsFlags  = (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE);
			goto skip_modifiers;

		default:
			SG_ASSERT_RELEASE_FAIL(  (0)  );
		}

	case SG_DIFFSTATUS_FLAGS__ADDED:				// row A
		SG_ASSERT_RELEASE_FAIL2(  ((dsFlags_a & SG_DIFFSTATUS_FLAGS__MASK_MODIFIERS) == 0),
						  (pCtx,
						   "Object [GID %s][name %s] has modifiers on an add [dsFlags_a %d] ??\n",
						   pOD->bufGidObject,SG_string__sz(pOD->apInst[Ndx_Dest]->pStrEntryName),(SG_uint32)dsFlags_a)  );
		SG_ASSERT_RELEASE_FAIL2(  (!pOD->apInst[Ndx_Orig]),
						  (pCtx,
						   "Object [GID %s][name %s] should have null in Ndx_Orig.\n",
						   pOD->bufGidObject,SG_string__sz(pOD->apInst[Ndx_Dest]->pStrEntryName))  );

		switch (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE)
		{
		case SG_DIFFSTATUS_FLAGS__ZERO:					// column Z (implies move/rename/change)  A+Z ==> A' (absorbing m/r/c)
		case SG_DIFFSTATUS_FLAGS__MODIFIED:				// column M   A+M ==> A' (also absorb any m/r/c also present)
			// we clone the pendingtree version and simply absorb all of the modifiers
			// into the add -- as if we added something with all the new values.
			SG_ERR_CHECK(  _od_inst__clone(pCtx,pOD->apInst[Ndx_Pend], &pInstAllocated)  );
			pInstAllocated->dsFlags = SG_DIFFSTATUS_FLAGS__ADDED;
			goto skip_modifiers;

		case SG_DIFFSTATUS_FLAGS__DELETED:				// column D   A+D ==> eat it (actually make an UNDONE_ADDED ghost)
		case SG_DIFFSTATUS_FLAGS__LOST:					// column L   A+L ==> eat it
			// if we eat an A+D or A+L, then it won't appear in the treediff output.
			// this is fine for "vv status" and "vv diff" on a composed/joined
			// treediff, so that relative to the starting CSET it is as if nothing
			// happened -- which is true.
			//
			// TODO 12/01/2009: Jeff says: the new sg_mrg version of merge does not
			// TODO use the sg_treediff2 code, so this comment may no longer apply.
			// TODO the earlier sg_merge code was going to try to use 2 treediffs
			// TODO to do the merge.
			// ***BUT*** for a merge, an ADD in an SPCA that is carried forward in one
			// branch and deleted another one, will appear as only an ADD in the first
			// branch.  (implying that it was added in the branch below the SPCA rather
			// than in the SPCA.)
			//
			// what we need to do is not eat it but rather put it in as a 'special'
			// node so that status/diff can ignore it and merge can respect it.
			SG_ERR_CHECK(  _od_inst__clone(pCtx,pOD->apInst[Ndx_Dest], &pInstAllocated)  );
			pInstAllocated->dsFlags  = SG_DIFFSTATUS_FLAGS__UNDONE_ADDED;
			goto skip_modifiers;

		default:
			SG_ASSERT_RELEASE_FAIL(  (0)  );
		}

	case SG_DIFFSTATUS_FLAGS__MODIFIED:				// row M
		switch (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE)
		{
		case SG_DIFFSTATUS_FLAGS__ZERO:					// column Z   M+Z ==> M
			SG_ERR_CHECK(  _od_inst__clone(pCtx,pOD->apInst[Ndx_Pend], &pInstAllocated)  );
			pInstAllocated->dsFlags = SG_DIFFSTATUS_FLAGS__MODIFIED;
			goto combine_modifiers;

		case SG_DIFFSTATUS_FLAGS__MODIFIED:				// column M
			SG_ERR_CHECK(  _od_inst__clone(pCtx,pOD->apInst[Ndx_Pend], &pInstAllocated)  );
			if (strcmp(pOD->apInst[Ndx_Orig]->bufHidContent,pOD->apInst[Ndx_Pend]->bufHidContent) != 0)
				pInstAllocated->dsFlags = SG_DIFFSTATUS_FLAGS__MODIFIED;	// M1 + M2 ==> M'
			else
				pInstAllocated->dsFlags = SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED;		// M  - M  ==> Z (an undo if you will)
			goto combine_modifiers;

		case SG_DIFFSTATUS_FLAGS__DELETED:				// column D   M+D ==> D
		case SG_DIFFSTATUS_FLAGS__LOST:					// column L   M+L ==> L
			SG_ERR_CHECK(  _od_inst__clone(pCtx,pOD->apInst[Ndx_Orig], &pInstAllocated)  );
			pInstAllocated->dsFlags = (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE);
			goto skip_modifiers;

		default:
			SG_ASSERT_RELEASE_FAIL(  (0)  );
		}

	default:
		SG_ASSERT_RELEASE_FAIL(  (0)  );
	}

combine_modifiers:

#define BOTHSET(ds1,ds2,b)		(((ds1) & (b)) && ((ds2) & (b)))
#define ONE_SET(ds1,ds2,b)		(((ds1) & (b)) || ((ds2) & (b)))

	if (BOTHSET(dsFlags_a,dsFlags_b,SG_DIFFSTATUS_FLAGS__RENAMED))
	{
		if (strcmp(SG_string__sz(pOD->apInst[Ndx_Orig]->pStrEntryName),
				   SG_string__sz(pOD->apInst[Ndx_Pend]->pStrEntryName)) != 0)		// R1 + R2 ==> R'
			pInstAllocated->dsFlags |= SG_DIFFSTATUS_FLAGS__RENAMED;
		else
			pInstAllocated->dsFlags |= SG_DIFFSTATUS_FLAGS__UNDONE_RENAMED;
	}
	else if (ONE_SET(dsFlags_a,dsFlags_b,SG_DIFFSTATUS_FLAGS__RENAMED))
		pInstAllocated->dsFlags |= SG_DIFFSTATUS_FLAGS__RENAMED;

	if (BOTHSET(dsFlags_a,dsFlags_b,SG_DIFFSTATUS_FLAGS__MOVED))
	{
		if (strcmp(pOD->apInst[Ndx_Orig]->bufParentGid,
				   pOD->apInst[Ndx_Pend]->bufParentGid) != 0)						// M1 + M2 ==> M'
			pInstAllocated->dsFlags |= SG_DIFFSTATUS_FLAGS__MOVED;
		else
			pInstAllocated->dsFlags |= SG_DIFFSTATUS_FLAGS__UNDONE_MOVED;
	}
	else if (ONE_SET(dsFlags_a,dsFlags_b,SG_DIFFSTATUS_FLAGS__MOVED))
		pInstAllocated->dsFlags |= SG_DIFFSTATUS_FLAGS__MOVED;

	if (BOTHSET(dsFlags_a,dsFlags_b,SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS))
	{
		if (strcmp(pOD->apInst[Ndx_Orig]->bufHidXAttrs,
				   pOD->apInst[Ndx_Pend]->bufHidXAttrs) != 0)						// X1 + X2 ==> X'
			pInstAllocated->dsFlags |= SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS;
		else
			pInstAllocated->dsFlags |= SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_XATTRS;
	}
	else if (ONE_SET(dsFlags_a,dsFlags_b,SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS))
		pInstAllocated->dsFlags |= SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS;

	if (BOTHSET(dsFlags_a,dsFlags_b,SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS))
	{
		if (pOD->apInst[Ndx_Orig]->attrBits != pOD->apInst[Ndx_Pend]->attrBits)		// X1 + X2 ==> X'
			pInstAllocated->dsFlags |= SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS;
		else
			pInstAllocated->dsFlags |= SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_ATTRBITS;
	}
	else if (ONE_SET(dsFlags_a,dsFlags_b,SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS))
		pInstAllocated->dsFlags |= SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS;

skip_modifiers:
	if (pInstAllocated->dsFlags == SG_DIFFSTATUS_FLAGS__ZERO)
		goto eat_it;

	pOD->apInst[Ndx_Net] = pInstAllocated;
	pInstAllocated = NULL;

	SG_ERR_CHECK(  _td__add_to_raw_summary(pCtx,pTreeDiff,pOD)  );
	return;

eat_it:
	SG_ERR_IGNORE(  _od_inst__free(pCtx, pInstAllocated)  );
	SG_context__err_reset(pCtx);
	return;

fail:
	SG_ERR_IGNORE(  _od_inst__free(pCtx, pInstAllocated)  );
}

/**
 * Run thru the list of all ObjectData rows that we visited and identify
 * the ones that represent a change of some kind and add them to the
 * "raw summary".
 *
 * This version is used when we are doing an arbitrary-cset vs pending-tree
 * diff.
 *
 * We have version info for the cset0 version (in Ndx_Orig), the baseline
 * version (in Ndx_Dest), as well as the version that sg_pendingtree gave us
 * (in Ndx_Pend).  We loaded Ndx_Orig directly from the REPO.  Ndx_Dest we
 * got from both pendingtree and from the repo.
 *
 * We do not have dsFlags set yet on Ndx_Dest; sg_pendingtree set the dsFlags
 * on the Ndx_Pend version.
 *
 * We want to combine these 3 versions (representing ORIG, ORIG-vs-BASELINE,
 * and BASELINE-vs-PENDINGTREE), into a single ORIG-vs-PENDINGTREE and store
 * that in Ndx_Net.
 */
static void _td__build_summary__cset0_pendingtree(SG_context * pCtx, SG_treediff2 * pTreeDiff)
{
	SG_ASSERT_RELEASE_RETURN(  (pTreeDiff->kind == SG_TREEDIFF2_KIND__CSET_VS_WD)  );

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,
											 pTreeDiff->prbObjectDataAllocated,
											 _td__build_summary__cset0_pendingtree_cb,
											 (void *)pTreeDiff)  );
}

//////////////////////////////////////////////////////////////////

static SG_rbtree_foreach_callback _td__build_summary__baseline_pendingtree_cb;

/**
 * See note before _td__build_summary__baseline_pendingtree().
 */
static void _td__build_summary__baseline_pendingtree_cb(SG_context * pCtx,
														SG_UNUSED_PARAM(const char * szKeyObjectGID),
														void * pVoidData,
														void * pVoidCtx)
{
	SG_treediff2 * pTreeDiff = (SG_treediff2 *)pVoidCtx;
	ObjectData * pOD = (ObjectData *)pVoidData;

	SG_UNUSED(szKeyObjectGID);

	// The Ndx_Pend column is only populated when sg_pendingtree gave us something
	// and it only gives us info for changed entries, so we just look for them.

	if (!pOD->apInst[Ndx_Pend])
		return;

	// sg_pendingtree already computed the dsFlags for us.

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Pend]->dsFlags != SG_DIFFSTATUS_FLAGS__ZERO)  );

	// move to Ndx_Net so that results are in a consistent location.

	pOD->apInst[Ndx_Net] = pOD->apInst[Ndx_Pend];
	pOD->apInst[Ndx_Pend] = NULL;

	SG_ERR_CHECK_RETURN(  _td__add_to_raw_summary(pCtx,pTreeDiff,pOD)  );
}

/**
 * Run thru the list of all ObjectData rows that we visited and identify
 * the ones that represent a change of some kind and add them to the
 * "raw summary".
 *
 * This version is used when we are doing a simple baseline-vs-wd diff.
 * In this model, we have the baseline in Ndx_Orig (as cset0) and the
 * info from sg_pendingtree in Ndx_Pend.
 *
 * This is probably the simplest case, since sg_pendingtree did all of
 * the work for us.  We loaded data in the Ndx_Orig column from the REPO
 * so that we could control some of the fields.  But really, most of this
 * is just pending-tree data.
 */
static void _td__build_summary__baseline_pendingtree(SG_context * pCtx,
													 SG_treediff2 * pTreeDiff)
{
	SG_ASSERT_RELEASE_RETURN(  (pTreeDiff->kind == SG_TREEDIFF2_KIND__BASELINE_VS_WD)  );

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,
											 pTreeDiff->prbObjectDataAllocated,
											 _td__build_summary__baseline_pendingtree_cb,
											 (void *)pTreeDiff)  );
}

//////////////////////////////////////////////////////////////////

static void _td__fixup_od_inst_repo_path(SG_context * pCtx,
										 SG_treediff2 * pTreeDiff,
										 ObjectData * pOD,
										 Ndx_Inst ndx)
{
	OD_Inst * pInst_self;
	ObjectData * pOD_parent;
	OD_Inst * pInst_parent;
	SG_bool bFound;

	pInst_self = pOD->apInst[ndx];

	if (!pInst_self)
		return;

	if (pInst_self->pStrRepoPath)			// already have the answer
		return;

	if (!*pInst_self->bufParentGid)			// if no parent, we are root
	{
		SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pInst_self->pStrRepoPath,"@/")  );
		return;
	}

	SG_ERR_CHECK(  SG_rbtree__find(pCtx,
								   pTreeDiff->prbObjectDataAllocated,
								   pInst_self->bufParentGid,
								   &bFound,
								   (void **)&pOD_parent)  );
	SG_ASSERT_RELEASE_FAIL2(  (bFound),
					  (pCtx,
					   "Object [GID %s][name %s] parent [Gid %s] not in cache.\n",
					   pOD->bufGidObject,SG_string__sz(pInst_self->pStrEntryName),
					   pInst_self->bufParentGid)  );

	pInst_parent = pOD_parent->apInst[ndx];

#if defined(DEBUG)
	if (!pInst_parent)
	{
		// TODO review this hack and see if it is still needed....
		//
		// HACK there are a couple of cases where the parent isn't in the cache until
		// HACK i fix something in sg_pendingtree.  until then, fake a path so u0043
		// HACK doesn't crash.

		SG_ERR_CHECK_RETURN(  SG_repopath__combine__sz(pCtx,
													   "?/",
													   SG_string__sz(pInst_self->pStrEntryName),
													   (pInst_self->tneType == SG_TREENODEENTRY_TYPE_DIRECTORY),
													   &pInst_self->pStrRepoPath)  );
		return;
	}
#endif

	SG_ASSERT_RELEASE_FAIL2(  (pInst_parent),
					  (pCtx,
					   "Object [GID %s][name %s][ndx %d] parent [Gid %s] not in cache.\n",
					   pOD->bufGidObject,SG_string__sz(pInst_self->pStrEntryName),ndx,
					   pInst_self->bufParentGid)  );

	SG_ERR_CHECK(  _td__fixup_od_inst_repo_path(pCtx,pTreeDiff,pOD_parent,ndx)  );

	SG_ERR_CHECK(  SG_repopath__combine__sz(pCtx,
											SG_string__sz(pInst_parent->pStrRepoPath),
											SG_string__sz(pInst_self->pStrEntryName),
											(pInst_self->tneType == SG_TREENODEENTRY_TYPE_DIRECTORY),
											&pInst_self->pStrRepoPath)  );

	return;

fail:
	return;
}

/**
 * Compute the per-column repo-paths for each object.
 *
 * We only need to do the objects in the summary; in theory,
 * these will be the only ones visible to our callers.
 */
static void _td__set_repo_paths(SG_context * pCtx, SG_treediff2 * pTreeDiff, Ndx_Inst ndx)
{
	SG_rbtree_iterator * pIter = NULL;
	SG_bool bFound;
	ObjectData * pOD;
	const char * szKey;

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,
											  &pIter,
											  pTreeDiff->prbRawSummary,
											  &bFound,
											  &szKey,
											  (void **)&pOD)  );
	while (bFound)
	{
		if (pOD->apInst[ndx])
			SG_ERR_CHECK(  _td__fixup_od_inst_repo_path(pCtx,pTreeDiff,pOD,ndx)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter,&bFound,&szKey,(void **)&pOD)  );
	}

	// fall thru to common cleanup

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__compare_cset_vs_cset(SG_context * pCtx,
										SG_treediff2 * pTreeDiff,
										const char * szHidCSet_0,
										const char * szHidCSet_1)
{
	// TODO 4/14/10 BUGBUG In theory a cset-vs-cset diff does not need a WD;
	// TODO 4/14/10 BUGBUG it should be able to run completely from the repo.
	// TODO 4/14/10 BUGBUG We should verify that __set_wd_top() has not been
	// TODO 4/14/10 BUGBUG called and/or that we work when pTreeDiff->pPathWorkingDirectoryTop
	// TODO 4/14/10 BUGBUG is not set.
	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NONEMPTYCHECK_RETURN(szHidCSet_0);
	SG_NONEMPTYCHECK_RETURN(szHidCSet_1);

	if (IS_FROZEN(pTreeDiff))
		SG_ERR_THROW_RETURN(  SG_ERR_INVALID_WHILE_FROZEN  );

	if (strcmp(szHidCSet_0,szHidCSet_1) == 0)		// must be 2 different changesets
		SG_ERR_THROW2_RETURN(  SG_ERR_INVALIDARG,
							   (pCtx,"Comparing changset %s with itself.",szHidCSet_0)  );

#if TRACE_TREEDIFF
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"TreeDiff: CSET0_VS_CSET1 [cset0 %s][cset1 %s]\n",szHidCSet_0,szHidCSet_1)  );
#endif

	pTreeDiff->kind = SG_TREEDIFF2_KIND__CSET_VS_CSET;

	SG_ERR_CHECK(  _td__load_changeset(pCtx,pTreeDiff,szHidCSet_0,Ndx_Orig)  );
	SG_ERR_CHECK(  _td__load_changeset(pCtx,pTreeDiff,szHidCSet_1,Ndx_Dest)  );
	SG_ERR_CHECK(  _td__process_changeset(pCtx,pTreeDiff,Ndx_Orig)  );
	SG_ERR_CHECK(  _td__process_changeset(pCtx,pTreeDiff,Ndx_Dest)  );
	SG_ERR_CHECK(  _td__compute_closure(pCtx,pTreeDiff)  );
	SG_ERR_CHECK(  _td__build_summary__cset0_cset1(pCtx,pTreeDiff)  );

	// minor compliation: when __build_summary finds something in present
	// in Ndx_Orig and not present in Ndx_Dest, it synthesizes a __DELETED
	// entry and adds it to Ndx_Dest and adds it to the summary so that
	// the summary is complete.  in this case, the depthInTree is not set
	// on the synthesized entries (because __build_summary goes in GID
	// rather than depth order).  so after all of the __DELETED entries
	// (and their __DELETED parent folders) are synthesized, we make a
	// quick pass and compute the depths.
	//
	// TODO this is mainly for debugging at this point (the depths are
	// TODO only really needed for running the work-queue in a depth-sorted
	// TODO manner), so we might skip this later.
	//
	// we have to run this on the Ndx_Net column, because __build_summary
	// aready moved them over.

	SG_ERR_CHECK(  _td__fixup_depths(pCtx,pTreeDiff,Ndx_Net)  );

	// set the repo-paths on the stuff in Ndx_Net.  since we want to report
	// all repo-paths as result-relative, we usually only need these.  there
	// are a few cases where we need the origin-relative repo-path, but we
	// can compute those on demand.

	SG_ERR_CHECK(  _td__set_repo_paths(pCtx,pTreeDiff,Ndx_Net)  );

	return;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__set_wd_top(SG_context * pCtx,
							  SG_treediff2* pTreeDiff,
							  const SG_pathname* pPathWorkingDirectoryTop)
{
	SG_NULLARGCHECK_RETURN(pTreeDiff);

	SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__COPY(pCtx, &pTreeDiff->pPathWorkingDirectoryTop,pPathWorkingDirectoryTop)  );
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__begin_pendingtree_diff(SG_context * pCtx,
										  SG_treediff2 * pTreeDiff,
										  const char * szHidArbitraryCSet,
										  const char * szHidBaseline,
										  const SG_pathname * pPathWorkingDirectoryTop)
{
	SG_bool bIsSimple;

	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NONEMPTYCHECK_RETURN(szHidBaseline);

	if (IS_FROZEN(pTreeDiff))
		SG_ERR_THROW_RETURN(  SG_ERR_INVALID_WHILE_FROZEN  );

	bIsSimple = (!szHidArbitraryCSet || !*szHidArbitraryCSet || (strcmp(szHidArbitraryCSet,szHidBaseline)==0));

	if (bIsSimple)		// the trivial baseline vs pending-tree case
	{
#if TRACE_TREEDIFF
		SG_console(pCtx,SG_CS_STDERR,"pendingtree_diff: BASELINE_VS_WD [baseline %s] vs pendingtree\n",szHidBaseline);
		SG_ERR_DISCARD;
#endif

		pTreeDiff->kind = SG_TREEDIFF2_KIND__BASELINE_VS_WD;
		SG_ERR_CHECK(  _td__load_changeset(pCtx,pTreeDiff,szHidBaseline,Ndx_Orig)  );
	}
	else				// arbitrary-cset vs baseline vs pending-tree case
	{
#if TRACE_TREEDIFF
		SG_console(pCtx,
				   SG_CS_STDERR,"pendingtree_diff: CSET_VS_WD [cset0 %s][baseline %s] vs pendingtree\n",
				   szHidArbitraryCSet,szHidBaseline);
		SG_ERR_DISCARD;
#endif

		pTreeDiff->kind = SG_TREEDIFF2_KIND__CSET_VS_WD;
		SG_ERR_CHECK(  _td__load_changeset(pCtx,pTreeDiff,szHidArbitraryCSet,Ndx_Orig)  );
		SG_ERR_CHECK(  _td__load_changeset(pCtx,pTreeDiff,szHidBaseline,Ndx_Dest)  );
	}

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pTreeDiff->pPathWorkingDirectoryTop,pPathWorkingDirectoryTop)  );

	// fall thru to common cleanup

fail:
	return;
}

void SG_treediff2__finish_pendingtree_diff(SG_context * pCtx, SG_treediff2 * pTreeDiff)
{
	if (pTreeDiff->kind == SG_TREEDIFF2_KIND__BASELINE_VS_WD)
	{
		// TODO the depthsInTree fields are mainly for debugging at this point.  they are
		// TODO mainly for running the work-queue in a depth-sorted manner, so we might
		// TODO skip this later.

		SG_ERR_CHECK(  _td__fixup_depths(pCtx,pTreeDiff,Ndx_Orig)  );
		SG_ERR_CHECK(  _td__fixup_depths(pCtx,pTreeDiff,Ndx_Pend)  );

		// since pendingtree gives us everything that we need, we don't bother starting up
		// the work-queue and computing closure.  we just fixup some per-entry fields that
		// would have gotten computed along the way.

		SG_ERR_CHECK(  _td__build_summary__baseline_pendingtree(pCtx,pTreeDiff)  );
		SG_ERR_CHECK(  _td__set_repo_paths(pCtx,pTreeDiff,Ndx_Net)  );
	}
	else
	{
		// the pendingtree code didn't tell us the depths-in-tree for the baseline
		// versions of entries.  fix them now before we run the work-queue/closure
		// so that the depth-sorting will work.

		SG_ERR_CHECK(  _td__fixup_depths(pCtx,pTreeDiff,Ndx_Dest)  );

		SG_ERR_CHECK(  _td__process_changeset(pCtx,pTreeDiff,Ndx_Orig)  );
		SG_ERR_CHECK(  _td__process_changeset(pCtx,pTreeDiff,Ndx_Dest)  );
		SG_ERR_CHECK(  _td__compute_closure(pCtx,pTreeDiff)  );

		SG_ERR_CHECK(  _td__build_summary__cset0_pendingtree(pCtx,pTreeDiff)  );

		// TODO this is mainly for debugging at this point (the depths are
		// TODO only really needed for running the work-queue in a depth-sorted
		// TODO manner), so we might skip this later.

		SG_ERR_CHECK(  _td__fixup_depths(pCtx,pTreeDiff,Ndx_Net)  );

		SG_ERR_CHECK(  _td__set_repo_paths(pCtx,pTreeDiff,Ndx_Net)  );
	}

	// fall thru to common cleanup

fail:
	return;
}

//////////////////////////////////////////////////////////////////

struct _dump_data
{
	const SG_treediff2 *	pTreeDiff;
	SG_string *				pStrOut;
	SG_string *				pStrTemp;
};

static SG_rbtree_foreach_callback _td_debug__dump_cb;

static void _td_debug__dump_cb(SG_context * pCtx, SG_UNUSED_PARAM(const char * szKeyObjectGID), void * pVoidData, void * pVoidDumpData)
{
	struct _dump_data * pDumpData = (struct _dump_data *)pVoidDumpData;
	ObjectData * pOD = (ObjectData *)pVoidData;
	int ndx;

	SG_UNUSED(szKeyObjectGID);

	SG_ERR_CHECK(  SG_string__sprintf(pCtx,
									  pDumpData->pStrTemp,
									  "\t%s:\n",
									  pOD->bufGidObject)  );
	SG_ERR_CHECK(  SG_string__append__string(pCtx, pDumpData->pStrOut,pDumpData->pStrTemp)  );

	for (ndx=Ndx_Orig; ndx<__NR_NDX__; ndx++)
	{
		OD_Inst * pInst = pOD->apInst[ndx];

		if (!pInst)
		{
			SG_ERR_CHECK(  SG_string__sprintf(pCtx,
											  pDumpData->pStrTemp,
											  "\t\t[ndx %d]: Unpopulated\n",
											  ndx)  );
		}
		else
		{
			SG_ERR_CHECK(  SG_string__sprintf(pCtx,
											  pDumpData->pStrTemp,
											  "\t\t[ndx %d]: [dsFlags %x][entryname %s] [depth %d] [typeInst %d] [tneType %d] [parent gid %s] [blob hid %s][xattrs %s][attrbits %d] [repo-path %s]\n",
											  ndx,
											  (SG_uint32)pInst->dsFlags,
											  SG_string__sz(pInst->pStrEntryName),
											  pInst->depthInTree,
											  pInst->typeInst,
											  pInst->tneType,
											  pInst->bufParentGid,
											  pInst->bufHidContent,
											  pInst->bufHidXAttrs,
											  (SG_uint32)pInst->attrBits,
											  ((pInst->pStrRepoPath && SG_string__sz(pInst->pStrRepoPath)) ? SG_string__sz(pInst->pStrRepoPath) : "(null)"))  );
		}

		SG_ERR_CHECK(  SG_string__append__string(pCtx, pDumpData->pStrOut,pDumpData->pStrTemp)  );
	}

	return;

fail:
	return;
}

void SG_treediff2_debug__dump(SG_context * pCtx, const SG_treediff2 * pTreeDiff, SG_string * pStrOut, SG_bool bAll)
{
	SG_string * pStrTemp = NULL;
	struct _dump_data DumpData;

	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NULLARGCHECK_RETURN(pStrOut);

	if (!IS_FROZEN(pTreeDiff))
		SG_ERR_THROW_RETURN(  SG_ERR_INVALID_UNLESS_FROZEN  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStrTemp)  );

	DumpData.pTreeDiff = pTreeDiff;
	DumpData.pStrOut = pStrOut;
	DumpData.pStrTemp = pStrTemp;

	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,
									  ((bAll) ? pTreeDiff->prbObjectDataAllocated : pTreeDiff->prbRawSummary),
									  _td_debug__dump_cb,
									  (void *)&DumpData)  );

	SG_STRING_NULLFREE(pCtx, pStrTemp);
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pStrTemp);
}

void SG_treediff2_debug__dump_to_console(SG_context * pCtx, const SG_treediff2 * pTreeDiff, SG_bool bAll)
{
	SG_string * pStrOut = NULL;

	SG_NULLARGCHECK_RETURN(pTreeDiff);

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStrOut)  );
	SG_ERR_CHECK(  SG_treediff2_debug__dump(pCtx, pTreeDiff,pStrOut,bAll)  );
	SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR,"TreeDiff2:\n%s\n",SG_string__sz(pStrOut))  );

	// fall thru to common cleanup

fail:
	SG_STRING_NULLFREE(pCtx, pStrOut);
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__pendingtree_annotate_cb(SG_context * pCtx,
										   SG_treediff2 * pTreeDiff,
										   SG_diffstatus_flags dsFlags,
										   const char * szGidObject,
										   const char * szGidObjectParent_base, const char * szGidObjectParent_pend,
										   SG_treenode_entry_type tneType,
										   const char * szEntryName_base, const char * szEntryName_pend,
										   const char * szHidContent_base, const char * szHidContent_pend,
										   const char * szHidXAttrs_base, const char * szHidXAttrs_pend,
										   SG_int64 attrBits_base, SG_int64 attrBits_pend)
{
	ObjectData * pOD;
	SG_bool bCreated;
	OD_Inst * pInst_pend;
	OD_Inst * pInst_base;
	Ndx_Inst ndx_base = Ndx_Orig;
	SG_bool bNew;
	OD_Type typeInst_base = OD_TYPE_UNPOPULATED;

	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NONEMPTYCHECK_RETURN(szGidObject);

	// when arbitrary-cset vs pending-tree, we put arbitrary-cset in Ndx_Orig,
	// the common baseline in Ndx_Dest, and the pending-tree in Ndx_Pend.
	//
	// when only baseline vs pending-tree, we put the baseline in Ndx_Orig
	// and the pending-tree in Ndx_Pend.

	if (pTreeDiff->kind==SG_TREEDIFF2_KIND__CSET_VS_WD)
		ndx_base = Ndx_Dest;
	else if (pTreeDiff->kind==SG_TREEDIFF2_KIND__BASELINE_VS_WD)
		ndx_base = Ndx_Orig;
	else
		SG_ASSERT_RELEASE_FAIL(  (0)  );

	bNew = ((dsFlags & SG_DIFFSTATUS_FLAGS__ADDED) || (dsFlags & SG_DIFFSTATUS_FLAGS__FOUND));

#if TRACE_TREEDIFF
	SG_console(pCtx,
			   SG_CS_STDERR,
			   "SetPendingTreeInfo: [gid %s]\n\t[parentGid %s %s]\n\t[dsFlags %x][tneType %d]\n\t[name %s %s]\n\t[hid %s %s]\n\t[xattrs %s %s]\n\t[attr %d %d]\n",
			   szGidObject,
			   ((szGidObjectParent_base && *szGidObjectParent_base) ? szGidObjectParent_base : "(null)"),
			   ((szGidObjectParent_pend && *szGidObjectParent_pend) ? szGidObjectParent_pend : "(null)"),
			   (SG_int32)dsFlags,
			   (SG_int32)tneType,
			   ((szEntryName_base && *szEntryName_base) ? szEntryName_base : "(null)"),
			   ((szEntryName_pend && *szEntryName_pend) ? szEntryName_pend : "(null)"),
			   ((szHidContent_base && *szHidContent_base) ? szHidContent_base : "(null)"),
			   ((szHidContent_pend && *szHidContent_pend) ? szHidContent_pend : "(null)"),
			   ((szHidXAttrs_base && *szHidXAttrs_base) ? szHidXAttrs_base : "(null)"),
			   ((szHidXAttrs_pend && *szHidXAttrs_pend) ? szHidXAttrs_pend : "(null)"),
			   (SG_int32)attrBits_base,
			   (SG_int32)attrBits_pend);
	SG_ERR_DISCARD;
#endif

	SG_ERR_CHECK(  _td__load_object_data(pCtx,pTreeDiff,szGidObject,&pOD,&bCreated)  );

#if TRACE_TREEDIFF
	SG_console(pCtx,
			   SG_CS_STDERR,"DEBUG [GID %s][name %s] ptrs [%x][%x][%x][%x] dsFlags [%x][%x][%x][%x] [created %d]\n",
			   szGidObject,szEntryName_pend,
			   (SG_uint32)pOD->apInst[Ndx_Orig],
			   (SG_uint32)pOD->apInst[Ndx_Dest],
			   (SG_uint32)pOD->apInst[Ndx_Pend],
			   (SG_uint32)pOD->apInst[Ndx_Net ],
			   ((SG_uint32)pOD->apInst[Ndx_Orig] ? pOD->apInst[Ndx_Orig]->dsFlags : 0),
			   ((SG_uint32)pOD->apInst[Ndx_Dest] ? pOD->apInst[Ndx_Dest]->dsFlags : 0),
			   ((SG_uint32)pOD->apInst[Ndx_Pend] ? pOD->apInst[Ndx_Pend]->dsFlags : 0),
			   ((SG_uint32)pOD->apInst[Ndx_Net ] ? pOD->apInst[Ndx_Net ]->dsFlags : 0),
			   (SG_uint32)bCreated);
	SG_ERR_DISCARD;
#endif

	SG_ASSERT_RELEASE_FAIL2(  (bCreated),
					  (pCtx,
					   "Object [GID %s][name %s] already has baseline/pendingtree data.",
					   szGidObject,szEntryName_pend)  );

	// add the instance data for the baseline and pendingtree (WD) version of this entry.
	// we can't actually do anything with it right now because we haven't scanned the repo yet.

	SG_ERR_CHECK(  _od_inst__alloc(pCtx,szEntryName_pend,&pOD->apInst[Ndx_Pend])  );
	pInst_pend = pOD->apInst[Ndx_Pend];

	pInst_pend->tneType = tneType;

	switch (tneType)
	{
	case SG_TREENODEENTRY_TYPE_DIRECTORY:
		pInst_pend->typeInst = OD_TYPE_SCANNED_FOLDER;
		typeInst_base = ((ndx_base == Ndx_Dest) ? OD_TYPE_PENDINGTREE_FOLDER : OD_TYPE_SCANNED_FOLDER);
		break;

	case SG_TREENODEENTRY_TYPE_REGULAR_FILE:
		pInst_pend->typeInst = OD_TYPE_FILE;
		typeInst_base = OD_TYPE_FILE;
		break;

	case SG_TREENODEENTRY_TYPE_SYMLINK:
		pInst_pend->typeInst = OD_TYPE_SYMLINK;
		typeInst_base = OD_TYPE_SYMLINK;
		break;

	default:
		SG_ASSERT_RELEASE_FAIL2(  (0),
						  (pCtx,
						   "Object [GID %s][name %s][Ndx_Pend] of unknown type [%d].",
						   szGidObject,szEntryName_pend,(SG_uint32)tneType)  );
	}

	pInst_pend->attrBits = attrBits_pend;

	// there are times when the pending-tree code does not have the HID content, such
	// as when a file is added/found and it didn't need to compute it to compare with
	// the baseline.  or when we are looking at a folder (with changed children) and
	// we cannot compute the folder's content HID at this time.
	//
	// TODO we might want to fix this to always have the value if that would be helpful downstream.
	SG_ERR_CHECK(  SG_strcpy(pCtx, pInst_pend->bufHidContent,SG_NrElements(pInst_pend->bufHidContent),((szHidContent_pend) ? szHidContent_pend : ""))  );

	// XAttrs may be null if the file/folder doesn't have any on it.
	SG_ERR_CHECK(  SG_strcpy(pCtx, pInst_pend->bufHidXAttrs, SG_NrElements(pInst_pend->bufHidXAttrs ),((szHidXAttrs_pend) ? szHidXAttrs_pend : ""))  );

	// parent may be null if this is the root "@/" folder.
	SG_ERR_CHECK(  SG_strcpy(pCtx, pInst_pend->bufParentGid, SG_NrElements(pInst_pend->bufParentGid ),((szGidObjectParent_pend) ? szGidObjectParent_pend : ""))  );

	pInst_pend->depthInTree = -1;		// we don't use this for Ndx_Pend
	pInst_pend->dsFlags = dsFlags;		// flags for how pendingtree entry changed from baseline

	if (!bNew)
	{
		SG_ERR_CHECK(  _od_inst__alloc(pCtx,szEntryName_base,&pOD->apInst[ndx_base])  );
		pInst_base = pOD->apInst[ndx_base];

		pInst_base->tneType = tneType;
		pInst_base->typeInst = typeInst_base;
		pInst_base->attrBits = attrBits_base;

		SG_ERR_CHECK(  SG_strcpy(pCtx, pInst_base->bufHidContent,SG_NrElements(pInst_base->bufHidContent),szHidContent_base)  );
		SG_ERR_CHECK(  SG_strcpy(pCtx, pInst_base->bufHidXAttrs, SG_NrElements(pInst_base->bufHidXAttrs ),((szHidXAttrs_base) ? szHidXAttrs_base : ""))  );
		SG_ERR_CHECK(  SG_strcpy(pCtx, pInst_base->bufParentGid, SG_NrElements(pInst_base->bufParentGid ),((szGidObjectParent_base) ? szGidObjectParent_base : ""))  );

		pInst_base->depthInTree = -1;		// we don't know the correct value.  we'll compute it later if we need it.
		pInst_base->dsFlags = SG_DIFFSTATUS_FLAGS__ZERO;	// we'll set this later if we are doing a cset-vs-wd (with the baseline between them implied)
	}

	return;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__get_repo(SG_context * pCtx, const SG_treediff2 * pTreeDiff, SG_repo ** ppRepo)
{
	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NULLARGCHECK_RETURN(ppRepo);

	*ppRepo = pTreeDiff->pRepo;
}

void SG_treediff2__get_kind(SG_context * pCtx, const SG_treediff2 * pTreeDiff, SG_treediff2_kind * pKind)
{
	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NULLARGCHECK_RETURN(pKind);

	*pKind = pTreeDiff->kind;
}

void SG_treediff2__get_working_directory_top(SG_context * pCtx, const SG_treediff2 * pTreeDiff, const SG_pathname ** ppPathWorkingDirectoryTop)
{
	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NULLARGCHECK_RETURN(ppPathWorkingDirectoryTop);

	SG_ASSERT_RELEASE_RETURN(  ((pTreeDiff->kind == SG_TREEDIFF2_KIND__BASELINE_VS_WD) || (pTreeDiff->kind == SG_TREEDIFF2_KIND__CSET_VS_WD))  );

	*ppPathWorkingDirectoryTop = pTreeDiff->pPathWorkingDirectoryTop;
}

void SG_treediff2__get_hid_cset_original(SG_context * pCtx, const SG_treediff2 * pTreeDiff, const char ** pszHidCSetOriginal)
{
	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NULLARGCHECK_RETURN(pszHidCSetOriginal);

	if (!IS_FROZEN(pTreeDiff))
		SG_ERR_THROW_RETURN(  SG_ERR_INVALID_UNLESS_FROZEN  );

	*pszHidCSetOriginal = pTreeDiff->aCSetColumns[Ndx_Orig].bufHidCSet;
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__foreach_with_diffstatus(SG_context * pCtx,
										   SG_treediff2 * pTreeDiff,
										   SG_diffstatus_flags dsMask,
										   SG_treediff2_foreach_callback pfnCB,
										   void * pVoidCallerData)
{
	SG_rbtree_iterator * pIter = NULL;
	SG_bool bFound;
	ObjectData * pOD;
	const char * szKey;

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,
											  &pIter,pTreeDiff->prbRawSummary,
											  &bFound,&szKey,(void **)&pOD)  );
	while (bFound)
	{
		SG_ASSERT_RELEASE_FAIL(  (pOD->apInst[Ndx_Net])  );

		if (pOD->apInst[Ndx_Net]->dsFlags & dsMask)
			SG_ERR_CHECK(  (*pfnCB)(pCtx,pTreeDiff,szKey,(const SG_treediff2_ObjectData *)pOD,pVoidCallerData)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter,&bFound,&szKey,(void **)&pOD)  );
	}

	// fall thru to common cleanup

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}

void SG_treediff2__foreach(SG_context * pCtx,
						   SG_treediff2 * pTreeDiff,
						   SG_treediff2_foreach_callback* pfnCB,		// TODO shouldn't this be "cb * pfnCB" or is this now implied in C/C++?
						   void * pVoidCallerData)							//      Ian added the * so vc would compile it.
{
	SG_rbtree_iterator * pIter = NULL;
	SG_bool bFound;
	ObjectData * pOD;
	const char * szKey;

	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NULLARGCHECK_RETURN(pfnCB);

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,
											  &pIter,pTreeDiff->prbRawSummary,
											  &bFound,&szKey,(void **)&pOD)  );
	while (bFound)
	{
		SG_ASSERT_RELEASE_FAIL(  (pOD->apInst[Ndx_Net])  );

		SG_ERR_CHECK(  (*pfnCB)(pCtx,pTreeDiff,szKey,(const SG_treediff2_ObjectData *)pOD,pVoidCallerData)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter,&bFound,&szKey,(void **)&pOD)  );
	}

	// fall thru to common cleanup

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__iterator__first(SG_context * pCtx,
								   SG_treediff2 * pTreeDiff,
								   SG_bool * pbOK,
								   const char ** pszGidObject,
								   SG_treediff2_ObjectData ** ppOD_opaque,
								   SG_treediff2_iterator ** ppIter)
{
	SG_NULLARGCHECK_RETURN(pTreeDiff);

	if (!IS_FROZEN(pTreeDiff))
		SG_ERR_THROW_RETURN(  SG_ERR_INVALID_UNLESS_FROZEN  );

	SG_ERR_CHECK_RETURN(  SG_rbtree__iterator__first(pCtx,
													 (SG_rbtree_iterator **)ppIter,
													 pTreeDiff->prbRawSummary,
													 pbOK,
													 pszGidObject,
													 (void **)ppOD_opaque)  );
}

void SG_treediff2__iterator__next(SG_context * pCtx,
								  SG_treediff2_iterator * pIter,
								  SG_bool * pbOK,
								  const char ** pszGidObject,
								  SG_treediff2_ObjectData ** ppOD_opaque)
{
	SG_ERR_CHECK_RETURN(  SG_rbtree__iterator__next(pCtx,
													(SG_rbtree_iterator *)pIter,
													pbOK,
													pszGidObject,
													(void **)ppOD_opaque)  );
}

void SG_treediff2__iterator__free(SG_context * pCtx, SG_treediff2_iterator * pIter)
{
	SG_rbtree__iterator__free(pCtx, (SG_rbtree_iterator *)pIter);
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__count_changes(SG_context * pCtx,
								 SG_treediff2 * pTreeDiff,
								 SG_uint32 * pCount)
{
	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NULLARGCHECK_RETURN(pCount);

	if (!IS_FROZEN(pTreeDiff))
		SG_ERR_THROW_RETURN(  SG_ERR_INVALID_UNLESS_FROZEN  );

	SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx,pTreeDiff->prbRawSummary,pCount)  );
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__count_composite_changes_by_part(SG_context * pCtx,
												   SG_treediff2 * pTreeDiff,
												   SG_uint32 * pCount_total,
												   SG_uint32 * pCount_baseline,
												   SG_uint32 * pCount_pending,
												   SG_uint32 * pCount_composite)
{
	SG_treediff2_iterator * pIter = NULL;
	SG_uint32 total = 0;
	SG_uint32 baseline = 0;
	SG_uint32 pending = 0;
	SG_uint32 composite = 0;
	SG_bool bOK;
	SG_treediff2_ObjectData * pOD_opaque;

	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_ARGCHECK_RETURN(  (pTreeDiff->kind == SG_TREEDIFF2_KIND__CSET_VS_WD), pTreeDiff  );		// must have a COMPOSITE treediff

	SG_ERR_CHECK(  SG_treediff2__iterator__first(pCtx, pTreeDiff, &bOK, NULL, &pOD_opaque, &pIter)  );
	while (bOK)
	{
		const ObjectData * pOD = (ObjectData *)pOD_opaque;
		SG_bool bDest = (pOD->apInst[Ndx_Dest] && (pOD->apInst[Ndx_Dest]->dsFlags != SG_DIFFSTATUS_FLAGS__ZERO));
		SG_bool bPend = (pOD->apInst[Ndx_Pend] && (pOD->apInst[Ndx_Pend]->dsFlags != SG_DIFFSTATUS_FLAGS__ZERO));

		total++;
		if (bDest)
			baseline++;
		if (bPend)
			pending++;
		if (bDest && bPend)
			composite++;

		SG_ERR_CHECK(  SG_treediff2__iterator__next(pCtx, pIter, &bOK, NULL, &pOD_opaque)  );
	}

	if (pCount_total)
		*pCount_total = total;
	if (pCount_baseline)
		*pCount_baseline = baseline;
	if (pCount_pending)
		*pCount_pending = pending;
	if (pCount_composite)
		*pCount_composite = composite;

fail:
	SG_TREEDIFF2_ITERATOR_NULLFREE(pCtx, pIter);
}

void SG_treediff2__ObjectData__get_composite_dsFlags_by_part(SG_context * pCtx,
															 const SG_treediff2_ObjectData * pOD_opaque,
															 SG_diffstatus_flags * pdsFlags_baseline,
															 SG_diffstatus_flags * pdsFlags_pending,
															 SG_diffstatus_flags * pdsFlags_composite)
{
	const ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD);

	if (pdsFlags_baseline)
	{
		if (pOD->apInst[Ndx_Dest])					// cset & baseline might not exist when just an ADD in pendingtree
			*pdsFlags_baseline = pOD->apInst[Ndx_Dest]->dsFlags;
		else
			*pdsFlags_baseline = SG_DIFFSTATUS_FLAGS__ZERO;		// TODO decide if we want this to be an __INVALID flag
	}

	if (pdsFlags_pending)
	{
		if (pOD->apInst[Ndx_Pend])					// this may only be present when there is dirt in the WD
			*pdsFlags_pending = pOD->apInst[Ndx_Pend]->dsFlags;
		else
			*pdsFlags_pending = SG_DIFFSTATUS_FLAGS__ZERO;
	}

	if (pdsFlags_composite)
	{
		SG_ASSERT(  (pOD->apInst[Ndx_Net])  );

		*pdsFlags_composite = pOD->apInst[Ndx_Net]->dsFlags;
	}
}

void SG_treediff2__Invert_dsFlags(SG_context * pCtx,
								  const SG_diffstatus_flags dsFlags_in,
								  SG_diffstatus_flags * pdsFlags_out)
{
	SG_diffstatus_flags dsFlags_me;
	SG_diffstatus_flags dsFlags_mod;
	SG_diffstatus_flags dsFlags_out;

	SG_NULLARGCHECK_RETURN(pdsFlags_out);

	// For the moment, I'm only need this operation on simple
	// (non-composite) flags, so I'm not going to worry about
	// anything with __UNDONE__ bits.
	SG_ARGCHECK_RETURN(  ((dsFlags_in & SG_DIFFSTATUS_FLAGS__MASK_UNDONE) == SG_DIFFSTATUS_FLAGS__ZERO), dsFlags_in  );

	dsFlags_me  = (dsFlags_in & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE);
	dsFlags_mod = (dsFlags_in & SG_DIFFSTATUS_FLAGS__MASK_MODIFIERS);

	switch (dsFlags_me)
	{
	case SG_DIFFSTATUS_FLAGS__ADDED:
		dsFlags_out = SG_DIFFSTATUS_FLAGS__DELETED | dsFlags_mod;
		break;

	case SG_DIFFSTATUS_FLAGS__DELETED:
		dsFlags_out = SG_DIFFSTATUS_FLAGS__ADDED | dsFlags_mod;
		break;

	case SG_DIFFSTATUS_FLAGS__LOST:
		dsFlags_out = SG_DIFFSTATUS_FLAGS__FOUND | dsFlags_mod;
		break;

	case SG_DIFFSTATUS_FLAGS__FOUND:
		dsFlags_out = SG_DIFFSTATUS_FLAGS__LOST | dsFlags_mod;
		break;

	case SG_DIFFSTATUS_FLAGS__ZERO:
	case SG_DIFFSTATUS_FLAGS__MODIFIED:
		dsFlags_out = dsFlags_in;
		break;

	default:
		SG_ASSERT(  (0)  );
		return;
	}

	*pdsFlags_out = dsFlags_out;
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__ObjectData__composite__get_parent_gids(SG_context * pCtx,
														  const SG_treediff2_ObjectData * pOD_opaque,
														  const char ** ppszGidParent_Orig,
														  const char ** ppszGidParent_Dest,
														  const char ** ppszGidParent_Pend)
{
	const ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD);

	if (ppszGidParent_Orig)
	{
		if (pOD->apInst[Ndx_Orig])
			*ppszGidParent_Orig = pOD->apInst[Ndx_Orig]->bufParentGid;
		else
			*ppszGidParent_Orig = NULL;
	}
	if (ppszGidParent_Dest)
	{
		if (pOD->apInst[Ndx_Dest])
			*ppszGidParent_Dest = pOD->apInst[Ndx_Dest]->bufParentGid;
		else
			*ppszGidParent_Dest = NULL;
	}

	if (ppszGidParent_Pend)
	{
		if (pOD->apInst[Ndx_Pend])
			*ppszGidParent_Pend = pOD->apInst[Ndx_Pend]->bufParentGid;
		else
			*ppszGidParent_Pend = NULL;
	}
}

//////////////////////////////////////////////////////////////////

struct _sg_report_status_data
{
	SG_diffstatus_flags			dsMask;
	SG_stringarray *			pSection;   // scratch area for building output for a section
	SG_string *					pStrReport; // cummulative string with all output
};

struct _sg_report_status_data_vhash
{
	SG_diffstatus_flags			dsMask;
	SG_varray *					pCurrentStatusArray;			// The current array for one of the statuses
	SG_vhash  *					pOverallReport;			// cummulative vhash with all output
};


static SG_treediff2_foreach_callback _sg_report_status_for_section_vhash_cb;

/**
 * Append 1 line of text for the given Entry to the status report.
 * We assume that we are doing a by-section summary and that the
 * caller has taken care of section headers.
 */
static char* getObjectType(SG_treenode_entry_type type)
{
	if (type == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
		return "File";
	else if (type == SG_TREENODEENTRY_TYPE_DIRECTORY)
			return "Directory";
	else if (type == SG_TREENODEENTRY_TYPE_SYMLINK)
			return "Symlink";
	else
		return "Invalid";
}
static void _sg_report_status_for_section_vhash_cb(SG_context * pCtx,
											 SG_UNUSED_PARAM(SG_treediff2 * pTreeDiff),
											 const char * szGidObject,
											 const SG_treediff2_ObjectData * pOD_opaque,
											 void * pVoidRSData)
{
	SG_vhash * vh = NULL;
	ObjectData * pOD = (ObjectData *)pOD_opaque;
	const char* pszOldHid = NULL;
	SG_uint32 hashcount = 0;
	struct _sg_report_status_data_vhash * pRSData = (struct _sg_report_status_data_vhash *)pVoidRSData;

	SG_UNUSED(pTreeDiff);

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &vh)  );

	// we asume that pRSData->dsMask only has 1 bit set and that we are
	// generating a section-by-section summary.
	//
	// 2010/07/07 This 1 bit assumption is no longer valid.  Since we
	//            now combine _MODIFIED and _CHANGED_ATTRBITS and _CHANGED_XATTRS
	//            for reporting purposes.

	if (pRSData->dsMask & (SG_DIFFSTATUS_FLAGS__ADDED | SG_DIFFSTATUS_FLAGS__FOUND))
	{
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "path", SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "type", getObjectType(pOD->apInst[Ndx_Net]->tneType))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "hid", pOD->apInst[Ndx_Net]->bufHidContent)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "gid", szGidObject)  );

	}
	else if (pRSData->dsMask & (SG_DIFFSTATUS_FLAGS__DELETED | SG_DIFFSTATUS_FLAGS__LOST))
	{
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "path", SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "type", getObjectType(pOD->apInst[Ndx_Net]->tneType))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "hid", pOD->apInst[Ndx_Net]->bufHidContent)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "gid", szGidObject)  );

	}
	else if (pRSData->dsMask & (SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS))
	{
		if (pOD->apInst[Ndx_Net]->tneType != SG_TREENODEENTRY_TYPE_DIRECTORY)
		{
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "path", SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath))   );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "type", getObjectType(pOD->apInst[Ndx_Net]->tneType))   );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "hid", pOD->apInst[Ndx_Net]->bufHidContent)  );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "gid", szGidObject)  );
			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_content_hid(pCtx,(SG_treediff2_ObjectData*) pOD, &pszOldHid)   );
			if (pszOldHid)
				SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "old_hid", pszOldHid)  );
			if (pOD->apInst[Ndx_Net]->dsFlags & SG_DIFFSTATUS_FLAGS__MODIFIED)
				SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, vh, "modified", SG_TRUE)  );
			if (pOD->apInst[Ndx_Net]->dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)
				SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, vh, "changed_attr", SG_TRUE)  );
			if (pOD->apInst[Ndx_Net]->dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)
				SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, vh, "changed_xattr", SG_TRUE)  );
		}
		else	// otherwise a directory.  here we treat the _MODIFIED bit as noise.
		{
			if (pOD->apInst[Ndx_Net]->dsFlags & (SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS))
			{
				//This is a modified directory which has had attributes changed.
				SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "path", SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath))   );
				SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "type", getObjectType(pOD->apInst[Ndx_Net]->tneType))   );
				SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "hid", pOD->apInst[Ndx_Net]->bufHidContent)  );
				SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "gid", szGidObject)  );
				if (pOD->apInst[Ndx_Net]->dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)
					SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, vh, "changed_attr", SG_TRUE)  );
				if (pOD->apInst[Ndx_Net]->dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)
					SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, vh, "changed_xattr", SG_TRUE)  );
			}
		}
	}
	else if (pRSData->dsMask & SG_DIFFSTATUS_FLAGS__RENAMED)
	{
		const char * szOldName = NULL;

		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, (const SG_treediff2_ObjectData *)pOD,&szOldName)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "path", SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "oldname", szOldName)   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "type", getObjectType(pOD->apInst[Ndx_Net]->tneType))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "hid", pOD->apInst[Ndx_Net]->bufHidContent)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "gid", szGidObject)  );

	}
	else if (pRSData->dsMask & SG_DIFFSTATUS_FLAGS__MOVED)
	{
		const char * szOldParentRepoPath;
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_moved_from_repo_path(pCtx,
																		  pTreeDiff,
																		  (const SG_treediff2_ObjectData *)pOD,
																		  &szOldParentRepoPath)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "path", SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "from", szOldParentRepoPath)   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "type", getObjectType(pOD->apInst[Ndx_Net]->tneType))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "hid", pOD->apInst[Ndx_Net]->bufHidContent)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "gid", szGidObject)  );

	}

	// the __UNDONE_ cases here are primarily for debugging.
	// i don't think the user should ever see these.

	else if (pRSData->dsMask & (SG_DIFFSTATUS_FLAGS__UNDONE_ADDED | SG_DIFFSTATUS_FLAGS__UNDONE_FOUND))
	{
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "path", SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "type", getObjectType(pOD->apInst[Ndx_Net]->tneType))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "hid", pOD->apInst[Ndx_Net]->bufHidContent)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "gid", szGidObject)  );

	}
	else if (pRSData->dsMask & (SG_DIFFSTATUS_FLAGS__UNDONE_DELETED | SG_DIFFSTATUS_FLAGS__UNDONE_LOST))
	{
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "path", SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "type", getObjectType(pOD->apInst[Ndx_Net]->tneType))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "hid", pOD->apInst[Ndx_Net]->bufHidContent)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "gid", szGidObject)  );
	}
	else if (pRSData->dsMask & (SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED | SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_XATTRS))
	{
		if (pOD->apInst[Ndx_Net]->tneType != SG_TREENODEENTRY_TYPE_DIRECTORY)
		{
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "path", SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath))   );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "type", getObjectType(pOD->apInst[Ndx_Net]->tneType))   );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "hid", pOD->apInst[Ndx_Net]->bufHidContent)  );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "gid", szGidObject)  );
			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_content_hid(pCtx,(SG_treediff2_ObjectData*) pOD, &pszOldHid)   );
			if (pszOldHid)
				SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "old_hid", pszOldHid)  );
			if (pOD->apInst[Ndx_Net]->dsFlags & SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED)
				SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, vh, "undone_modified", SG_TRUE)  );
			if (pOD->apInst[Ndx_Net]->dsFlags & SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_ATTRBITS)
				SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, vh, "undone_changed_attr", SG_TRUE)  );
			if (pOD->apInst[Ndx_Net]->dsFlags & SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_XATTRS)
				SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, vh, "undone_changed_xattr", SG_TRUE)  );
		}
		else	// otherwise a directory.  here we treat the _UNDONE_MODIFIED bit as noise.
		{
			if (pOD->apInst[Ndx_Net]->dsFlags & (SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_XATTRS))
			{
				//This is a modified directory which has had attributes changed.
				SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "path", SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath))   );
				SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "type", getObjectType(pOD->apInst[Ndx_Net]->tneType))   );
				SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "hid", pOD->apInst[Ndx_Net]->bufHidContent)  );
				SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "gid", szGidObject)  );
				if (pOD->apInst[Ndx_Net]->dsFlags & SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_ATTRBITS)
					SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, vh, "undone_changed_attr", SG_TRUE)  );
				if (pOD->apInst[Ndx_Net]->dsFlags & SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_XATTRS)
					SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, vh, "undone_changed_xattr", SG_TRUE)  );
			}
		}
	}
	else if (pRSData->dsMask & SG_DIFFSTATUS_FLAGS__UNDONE_RENAMED)
	{
		const char * szOldName = NULL;

		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, (const SG_treediff2_ObjectData *)pOD,&szOldName)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "path", SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "oldname", szOldName)   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "type", getObjectType(pOD->apInst[Ndx_Net]->tneType))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "hid", pOD->apInst[Ndx_Net]->bufHidContent)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "gid", szGidObject)  );

	}
	else if (pRSData->dsMask & SG_DIFFSTATUS_FLAGS__UNDONE_MOVED)
	{
		const char * szOldParentRepoPath;
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_moved_from_repo_path(pCtx,
																		  pTreeDiff,
																		  (const SG_treediff2_ObjectData *)pOD,
																		  &szOldParentRepoPath)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "path", SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "from", szOldParentRepoPath)   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "type", getObjectType(pOD->apInst[Ndx_Net]->tneType))   );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "hid", pOD->apInst[Ndx_Net]->bufHidContent)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vh, "gid", szGidObject)  );
	}
	else
	{
		SG_ASSERT(  (0)  );
	}

	SG_ERR_CHECK(   SG_vhash__count(pCtx, vh, &hashcount)   );
	if (hashcount > 0)
	{
		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, vh, "flags", (int64)(pOD->apInst[Ndx_Net]->dsFlags))  );
		SG_ERR_CHECK(   SG_varray__append__vhash(pCtx, pRSData->pCurrentStatusArray, &vh)   );
	}
	else
		SG_VHASH_NULLFREE(pCtx, vh);
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, vh);
	return;
}

/**
 * WARNING: The dsFlags can have more than one bit set (a file can be
 *          RENAMED, MOVED, EDITED, and CHMOD'd, for example).  Only
 *          use this routine to get the label for one bit at a time.
 *
 */
char* SG_treediff__get_status_section_name(SG_diffstatus_flags dsMask)
{
	switch (dsMask)
	{
	case SG_DIFFSTATUS_FLAGS__ADDED:
		return "Added";

	case SG_DIFFSTATUS_FLAGS__DELETED:
		return "Removed";

	case SG_DIFFSTATUS_FLAGS__FOUND:
		return "Found";

	case SG_DIFFSTATUS_FLAGS__LOST:
		return "Lost";

	case SG_DIFFSTATUS_FLAGS__MOVED:
		return "Moved";

	case SG_DIFFSTATUS_FLAGS__MODIFIED:
		return "Modified";

	case SG_DIFFSTATUS_FLAGS__RENAMED:
		return "Renamed";

	case SG_DIFFSTATUS_FLAGS__UNDONE_ADDED:
		return "Undone_Added";

	case SG_DIFFSTATUS_FLAGS__UNDONE_DELETED:
		return "Undone_Removed";

	case SG_DIFFSTATUS_FLAGS__UNDONE_FOUND:
		return "Undone_Found";

	case SG_DIFFSTATUS_FLAGS__UNDONE_LOST:
		return "Undone_Lost";

	case SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED:
		return "Undone_Modified";

	case SG_DIFFSTATUS_FLAGS__UNDONE_MOVED:
		return "Undone_Moved";

	case SG_DIFFSTATUS_FLAGS__UNDONE_RENAMED:
		return "Undone_Renamed";

	// 2010/07/09 Now that we are combining _MODIFIED, _CHANGED_ATTRBITS,
	//            and _CHANGED_XATTRS and just calling them "Modified" in
	//            the status report, we don't need the cases for
	//            _CHANGED_ATTRBITS and _CHANGED_XATTRS, right?
	//
	//            The same is now true for _UNDONE_MODIFED,
	//            _UNDONE_CHANGED_ATTRBITS, and _UNDONE_CHANGED_XATTRS
	//            since they're all going to get labelled as "un-modified",
	//            right?
	case SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS:
	case SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS:
	case SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_ATTRBITS:
	case SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_XATTRS:
		SG_ASSERT( (0) );

	default:
		return "Unknown";
	}
}


int _diffreport_sort_callback__increasing(SG_context * pCtx,
										 const void * pVoid_ppv1, // const SG_variant ** ppv1
										 const void * pVoid_ppv2, // const SG_variant ** ppv2
										 SG_UNUSED_PARAM(void * pVoidCallerData))
{
	// see SG_qsort() for discussion.
	const char * pPath1 = NULL;
	const char * pPath2 = NULL;
	const SG_variant** ppv1 = (const SG_variant **)pVoid_ppv1;
	const SG_variant** ppv2 = (const SG_variant **)pVoid_ppv2;
	SG_vhash * pvh1 = NULL;
	SG_vhash * pvh2 = NULL;
	SG_bool b = SG_FALSE;
	int c = 0;

	SG_UNUSED(pVoidCallerData);

	if (*ppv1 == NULL && *ppv2 == NULL)
		return 0;
	if (*ppv1 == NULL)
		return -1;
	if (*ppv2 == NULL)
		return 1;

	SG_variant__get__vhash(pCtx, *ppv1, &pvh1);
	SG_variant__get__vhash(pCtx, *ppv2, &pvh2);
	SG_vhash__has(pCtx, pvh1, "path", &b );
	if (b == SG_TRUE)
		SG_vhash__get__sz(pCtx, pvh1, "path", &pPath1);
	SG_ASSERT(!SG_context__has_err(pCtx));
	SG_vhash__has(pCtx, pvh2, "path", &b );
	if (b == SG_TRUE)
		SG_vhash__get__sz(pCtx, pvh2, "path", &pPath2);
	SG_ASSERT(!SG_context__has_err(pCtx));
	SG_repopath__compare(pCtx, pPath1, pPath2, &c);
	SG_ASSERT(!SG_context__has_err(pCtx));

	// TODO decide if we want to do something more here...
//

	return c;
}

static void _sg_treediff2__report_status_for_section_varray(SG_context * pCtx,
													 SG_treediff2 * pTreeDiff,
													 SG_diffstatus_flags dsMask,
													 struct _sg_report_status_data_vhash * pRSData)
{
	SG_varray * pVarray = NULL;
	SG_uint32 hashcount = 0;

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pVarray)   );

	pRSData->dsMask = dsMask;
	pRSData->pCurrentStatusArray = pVarray;
	SG_ERR_CHECK(  SG_treediff2__foreach_with_diffstatus(pCtx, pTreeDiff,dsMask,_sg_report_status_for_section_vhash_cb,pRSData)  );

	// This trick allows _MODIFIED, _CHANGED_ATTRBITS, and _CHANGED_XATTRS
	// to all be grouped together and labelled as "Modified".
	//
	// 2010/07/09 We also do this for _UNDONE_MODIFED,
	//            _UNDONE_CHANGED_ATTRBITS, and _UNDONE_CHANGED_XATTRS
	//            so that they all get marked as "Undone_Modified".

	if (dsMask & SG_DIFFSTATUS_FLAGS__MODIFIED)
		dsMask = SG_DIFFSTATUS_FLAGS__MODIFIED;
	else if (dsMask & SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED)
		dsMask = SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED;

	SG_ERR_CHECK(  SG_varray__count(pCtx, pVarray, &hashcount)   );
	if (hashcount > 0)
	{
		SG_ERR_CHECK(  SG_varray__sort(pCtx, pVarray, _diffreport_sort_callback__increasing)  );
		SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pRSData->pOverallReport, SG_treediff__get_status_section_name(dsMask), &pVarray)   );
		pVarray = NULL;		// vhash owns this now.
	}

fail:
	SG_VARRAY_NULLFREE(pCtx, pVarray);
	return;
}

void SG_treediff2__report_status_to_vhash(SG_context * pCtx, SG_treediff2 * pTreeDiff, SG_vhash * pVhash, SG_bool bIncludeUndoneEntries)
{
	struct _sg_report_status_data_vhash RSData;

	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NULLARGCHECK_RETURN(pVhash);

	RSData.pOverallReport = pVhash;

    // TODO is this the order in which we want these to be printed?

	SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__ADDED,            &RSData)  );
	SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,         &RSData)  );
	SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__DELETED,          &RSData)  );
	SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__RENAMED,          &RSData)  );
	SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__MOVED,            &RSData)  );
	SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__LOST,             &RSData)  );
	SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__FOUND,            &RSData)  );

	if (bIncludeUndoneEntries)
	{
		SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__UNDONE_ADDED,            &RSData)  );
		SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED | SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_XATTRS | SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_ATTRBITS,         &RSData)  );
		SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__UNDONE_DELETED,          &RSData)  );
		SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__UNDONE_RENAMED,          &RSData)  );
		SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__UNDONE_MOVED,            &RSData)  );
		SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__UNDONE_LOST,             &RSData)  );
		SG_ERR_CHECK(  _sg_treediff2__report_status_for_section_varray(pCtx, pTreeDiff,SG_DIFFSTATUS_FLAGS__UNDONE_FOUND,            &RSData)  );
	}

	// fall thru to common cleanup
fail:
	return;
}


void SG_treediff2__report_status_to_console(SG_context * pCtx, SG_treediff2 * pTreeDiff, SG_bool bIncludeUndoneEntries)
{
	SG_string * pStrReport = NULL;
	SG_vhash * pvaTreeDiffs = NULL;
	const char * szReport;
	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvaTreeDiffs)  );

	SG_ERR_CHECK(  SG_treediff2__report_status_to_vhash(pCtx, pTreeDiff,pvaTreeDiffs,bIncludeUndoneEntries)  );

	SG_ERR_CHECK(  SG_status_formatter__vhash_to_string(pCtx, pvaTreeDiffs, NULL, &pStrReport)  );

	szReport = SG_string__sz(pStrReport);
	SG_ERR_CHECK(  SG_console(pCtx,
							  SG_CS_STDERR,"TreeDiff2 Status Report\n%s",
							  ((szReport && *szReport)
							   ? szReport
							   : "\t(no changes)\n"))  );

	// fall thru to common cleanup

fail:
	SG_VHASH_NULLFREE(pCtx, pvaTreeDiffs);
	SG_STRING_NULLFREE(pCtx, pStrReport);
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__get_ObjectData(SG_context * pCtx,
								  const SG_treediff2 * pTreeDiff, const char * szGidObject,
								  SG_bool * pbFound,
								  const SG_treediff2_ObjectData ** ppOD_opaque)
{
	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NONEMPTYCHECK_RETURN(szGidObject);

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,pTreeDiff->prbRawSummary,szGidObject,pbFound,(void **)ppOD_opaque)  );
}

void _sg_treediff2__get_ObjectData__from_cache(SG_context * pCtx,
								  const SG_treediff2 * pTreeDiff, const char * szGidObject,
								  SG_bool * pbFound,
								  const SG_treediff2_ObjectData ** ppOD_opaque)
{
	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NONEMPTYCHECK_RETURN(szGidObject);

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,pTreeDiff->prbObjectDataAllocated,szGidObject,pbFound,(void **)ppOD_opaque)  );
}


void SG_treediff2__ObjectData__get_gid(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, const char ** pszGidObject)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);
	SG_NULLARGCHECK_RETURN(pszGidObject);

	*pszGidObject = pOD->bufGidObject;
}

void SG_treediff2__ObjectData__get_repo_path(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, const char ** pszRepoPath)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);
	SG_NULLARGCHECK_RETURN(pszRepoPath);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );
	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net]->pStrRepoPath)  );

	*pszRepoPath = SG_string__sz(pOD->apInst[Ndx_Net]->pStrRepoPath);
}

void SG_treediff2__ObjectData__get_dsFlags(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, SG_diffstatus_flags * pdsFlags)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);
	SG_NULLARGCHECK_RETURN(pdsFlags);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	*pdsFlags = pOD->apInst[Ndx_Net]->dsFlags;
}

void SG_treediff2__ObjectData__get_old_content_hid(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, const char ** pszHid)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);
	SG_NULLARGCHECK_RETURN(pszHid);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	if (pOD->apInst[Ndx_Orig])
	{
		SG_ASSERT_RELEASE_RETURN(  (*pOD->apInst[Ndx_Orig]->bufHidContent)  );
		*pszHid = pOD->apInst[Ndx_Orig]->bufHidContent;
	}
	else if (pOD->apInst[Ndx_Dest])
	{
		SG_ASSERT_RELEASE_RETURN(  (*pOD->apInst[Ndx_Dest]->bufHidContent)  );
		*pszHid = pOD->apInst[Ndx_Dest]->bufHidContent;
	}
	else
	{
		*pszHid = NULL;
	}
}

void SG_treediff2__ObjectData__get_content_hid(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, const char ** pszHid)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);
	SG_NULLARGCHECK_RETURN(pszHid);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	*pszHid = NULL;
	if (*pOD->apInst[Ndx_Net]->bufHidContent)
		*pszHid = pOD->apInst[Ndx_Net]->bufHidContent;
}

void SG_treediff2__ObjectData__get_tneType(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, SG_treenode_entry_type * ptneType)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);
	SG_NULLARGCHECK_RETURN(ptneType);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	*ptneType = pOD->apInst[Ndx_Net]->tneType;
}

void SG_treediff2__ObjectData__get_old_name(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, const char ** pszName)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	// this is only needed when we have a __RENAME.  this could be in Ndx_Net
	// *or* in Ndx_Dest.

	if (pOD->apInst[Ndx_Orig])
		*pszName = SG_string__sz(pOD->apInst[Ndx_Orig]->pStrEntryName);
	else if (pOD->apInst[Ndx_Dest])
		*pszName = SG_string__sz(pOD->apInst[Ndx_Dest]->pStrEntryName);
	else
		*pszName = NULL;
}

void SG_treediff2__ObjectData__get_name(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, const char ** pszName)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	*pszName = SG_string__sz(pOD->apInst[Ndx_Net]->pStrEntryName);
}

void SG_treediff2__ObjectData__get_moved_from_gid(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, const char ** pszOldParentGid)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);
	SG_NULLARGCHECK_RETURN(pszOldParentGid);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	if (!*pOD->apInst[Ndx_Net]->bufParentGid)		// the root directory cannot have been moved, right?
		*pszOldParentGid = NULL;
	else if (pOD->apInst[Ndx_Orig])
		*pszOldParentGid = pOD->apInst[Ndx_Orig]->bufParentGid;
	else if (pOD->apInst[Ndx_Dest])
		*pszOldParentGid = pOD->apInst[Ndx_Dest]->bufParentGid;
}

void SG_treediff2__ObjectData__get_parent_gid(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, const char ** pszGidParent)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);
	SG_NULLARGCHECK_RETURN(pszGidParent);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	if (!*pOD->apInst[Ndx_Net]->bufParentGid)
		*pszGidParent = NULL;
	else
		*pszGidParent = pOD->apInst[Ndx_Net]->bufParentGid;
}

void SG_treediff2__ObjectData__get_old_attrbits(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, SG_int64 * pAttrBits)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);
	SG_NULLARGCHECK_RETURN(pAttrBits);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	if (pOD->apInst[Ndx_Orig])
		*pAttrBits = pOD->apInst[Ndx_Orig]->attrBits;
	else if (pOD->apInst[Ndx_Dest])
		*pAttrBits = pOD->apInst[Ndx_Dest]->attrBits;
	else
		*pAttrBits = pOD->apInst[Ndx_Net]->attrBits;	// no default since attrBits is not a pointer and we can't return null.
}

void SG_treediff2__ObjectData__get_attrbits(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, SG_int64 * pAttrBits)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);
	SG_NULLARGCHECK_RETURN(pAttrBits);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	*pAttrBits = pOD->apInst[Ndx_Net]->attrBits;
}

void SG_treediff2__ObjectData__get_old_xattrs(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, const char ** pszHid)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);
	SG_NULLARGCHECK_RETURN(pszHid);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	// XAttr HID may be empty.  this implies that there were no XAttrs on the file/folder.

	if (pOD->apInst[Ndx_Orig])
	{
		if (*pOD->apInst[Ndx_Orig]->bufHidXAttrs)
			*pszHid = pOD->apInst[Ndx_Orig]->bufHidXAttrs;
		else
			*pszHid = NULL;
	}
	else if (pOD->apInst[Ndx_Dest])
	{
		if (*pOD->apInst[Ndx_Dest]->bufHidXAttrs)
			*pszHid = pOD->apInst[Ndx_Dest]->bufHidXAttrs;
		else
			*pszHid = NULL;
	}
	else
		*pszHid = NULL;
}

void SG_treediff2__ObjectData__get_xattrs(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, const char ** pszHid)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD_opaque);
	SG_NULLARGCHECK_RETURN(pszHid);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	// XAttr HID may be empty.  this implies that there were no XAttrs on the file/folder.
	// this might also be because we are on Windows.
	// this might also be because sg_pendingtree was too lazy to compute them.

	if (*pOD->apInst[Ndx_Net]->bufHidXAttrs)
		*pszHid = pOD->apInst[Ndx_Net]->bufHidXAttrs;
	else
		*pszHid = NULL;
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__ObjectData__get_moved_from_repo_path(SG_context * pCtx,
														const SG_treediff2 * pTreeDiff,
														const SG_treediff2_ObjectData * pOD_opaque,
														const char ** pszOldParentRepoPath)
{
	const SG_treediff2_ObjectData * pOD_parent_opaque;
	const char * szOldParentGid = NULL;
	SG_bool bFound;

	SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_moved_from_gid(pCtx, pOD_opaque,&szOldParentGid)  );
	SG_ASSERT_RELEASE_RETURN(  (szOldParentGid && *szOldParentGid)  );		// root folder couldn't have been moved, right?
	SG_ERR_CHECK_RETURN(  SG_treediff2__get_ObjectData(pCtx, pTreeDiff,szOldParentGid,&bFound,&pOD_parent_opaque)  );
	//We didn't find the parent on the first try (probably because it was excluded).  Try again to load it from the cache.
	if (!bFound)
		SG_ERR_CHECK_RETURN(  _sg_treediff2__get_ObjectData__from_cache(pCtx, pTreeDiff,szOldParentGid,&bFound,&pOD_parent_opaque)  );
	SG_ASSERT_RELEASE_RETURN(  (bFound)  );

	SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_parent_opaque,pszOldParentRepoPath)  );
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__ObjectData__set_mark(SG_context * pCtx, SG_treediff2_ObjectData * pOD_opaque, SG_int32 newValue, SG_int32 * pOldValue)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD);

	if (pOldValue)
		*pOldValue = pOD->user_marker_data;

	pOD->user_marker_data = newValue;
}

void SG_treediff2__ObjectData__get_mark(SG_context * pCtx, const SG_treediff2_ObjectData * pOD_opaque, SG_int32 * pValue)
{
	const ObjectData * pOD = (const ObjectData *)pOD_opaque;

	SG_NULLARGCHECK_RETURN(pOD);
	SG_NULLARGCHECK_RETURN(pValue);

	*pValue = pOD->user_marker_data;
}

//////////////////////////////////////////////////////////////////

static SG_treediff2_foreach_callback _td_debug__do_stats_cb;

static void _td_debug__do_stats_cb(SG_context * pCtx,
								   SG_UNUSED_PARAM(SG_treediff2 * pTreeDiff),
								   SG_UNUSED_PARAM(const char * szGidObject),
								   const SG_treediff2_ObjectData * pOD_opaque,
								   void * pVoidStats)
{
	ObjectData * pOD = (ObjectData *)pOD_opaque;
	SG_treediff2_debug__Stats * pStats = (SG_treediff2_debug__Stats *)pVoidStats;
	SG_diffstatus_flags dsFlags;

	SG_UNUSED(pTreeDiff);
	SG_UNUSED(szGidObject);

	SG_ASSERT_RELEASE_RETURN(  (pOD->apInst[Ndx_Net])  );

	dsFlags = pOD->apInst[Ndx_Net]->dsFlags;

	SG_ASSERT_RELEASE_RETURN(  (dsFlags != SG_DIFFSTATUS_FLAGS__ZERO)  );

	if (dsFlags & SG_DIFFSTATUS_FLAGS__ADDED)				pStats->nrAdded++;
	if (dsFlags & SG_DIFFSTATUS_FLAGS__DELETED)				pStats->nrDeleted++;

	// we now set __MODIFIED on the directory when anything within the
	// directory changes (this includes content changes as well as
	// moves/renames/etc).  this ensures that the directory entry gets
	// put in the diff result set -- but because of the bubble-up effect
	// it makes it harder to determine if there were actual content changes.
	// therefore, i'm splitting up the counts.

	if (dsFlags & SG_DIFFSTATUS_FLAGS__MODIFIED)
	{
		if (pOD->apInst[Ndx_Net]->tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
			pStats->nrDirectoryModified++;
		else
			pStats->nrFileSymlinkModified++;
	}

	if (dsFlags & SG_DIFFSTATUS_FLAGS__LOST)				pStats->nrLost++;
	if (dsFlags & SG_DIFFSTATUS_FLAGS__FOUND)				pStats->nrFound++;

	if (dsFlags & SG_DIFFSTATUS_FLAGS__RENAMED)				pStats->nrRenamed++;
	if (dsFlags & SG_DIFFSTATUS_FLAGS__MOVED)				pStats->nrMoved++;
	if (dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)		pStats->nrChangedXAttrs++;
	if (dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	pStats->nrChangedAttrBits++;

	pStats->nrTotalChanges++;
}

void SG_treediff2_debug__compute_stats(SG_context * pCtx, SG_treediff2 * pTreeDiff, SG_treediff2_debug__Stats * pStats)
{
	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NULLARGCHECK_RETURN(pStats);

	if (!IS_FROZEN(pTreeDiff))
		SG_ERR_THROW_RETURN(  SG_ERR_INVALID_UNLESS_FROZEN  );

	memset(pStats,0,sizeof(SG_treediff2_debug__Stats));

	SG_ERR_CHECK_RETURN(  SG_treediff2__foreach(pCtx, pTreeDiff,_td_debug__do_stats_cb,pStats)  );

	if (pStats->nrAdded)					pStats->nrSectionsWithChanges++;
	if (pStats->nrDeleted)					pStats->nrSectionsWithChanges++;
	if (pStats->nrDirectoryModified
		|| pStats->nrFileSymlinkModified)	pStats->nrSectionsWithChanges++;
	if (pStats->nrLost)						pStats->nrSectionsWithChanges++;
	if (pStats->nrFound)					pStats->nrSectionsWithChanges++;
	if (pStats->nrRenamed)					pStats->nrSectionsWithChanges++;
	if (pStats->nrMoved)					pStats->nrSectionsWithChanges++;
	if (pStats->nrChangedXAttrs)			pStats->nrSectionsWithChanges++;
	if (pStats->nrChangedAttrBits)			pStats->nrSectionsWithChanges++;
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__find_by_repo_path(SG_context * pCtx,
									 SG_treediff2 * pTreeDiff,
									 const char * szRepoPathWanted,
									 SG_bool * pbFound,
									 const char ** pszGidObject_found,
									 const SG_treediff2_ObjectData ** ppOD_opaque_found)
{
	SG_treediff2_iterator * pIter = NULL;
	const char * szRepoPath_k;
	const char * szGidObject_k;
	const char * szGidObject_found = NULL;
	SG_treediff2_ObjectData * pOD_opaque_k;
	SG_treediff2_ObjectData * pOD_opaque_found = NULL;
	SG_bool bOK;
	SG_bool bFound = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NONEMPTYCHECK_RETURN(szRepoPathWanted);

	SG_ERR_CHECK(  SG_treediff2__iterator__first(pCtx, pTreeDiff,&bOK,&szGidObject_k,&pOD_opaque_k,&pIter)  );
	while (bOK)
	{
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_repo_path(pCtx, pOD_opaque_k,&szRepoPath_k)  );
		if (strcmp(szRepoPath_k,szRepoPathWanted) == 0)
		{
#if defined(DEBUG)
			// for now, in the debug version, return the first, but scan everything and
			// verify that there are no duplicates.
			if (szGidObject_found == NULL)
			{
				szGidObject_found = szGidObject_k;
				pOD_opaque_found = pOD_opaque_k;
				bFound = SG_TRUE;
			}
			else
			{
				SG_console(pCtx,SG_CS_STDERR,
						   "TreeDiffFindByRepoPath: found more than one answer for [%s] [gidObject %s %s]\n",
						   szRepoPathWanted,szGidObject_found,szGidObject_k);
				SG_ERR_DISCARD;
			}
#else
			szGidObject_found = szGidObject_k;
			pOD_opaque_found = pOD_opaque_k;
			bFound = SG_TRUE;
			break;
#endif
		}

		SG_ERR_CHECK(  SG_treediff2__iterator__next(pCtx, pIter,&bOK,&szGidObject_k,&pOD_opaque_k)  );
	}

	if (bFound)
	{
		if (pbFound)
			*pbFound = SG_TRUE;
		if (pszGidObject_found)
			*pszGidObject_found = szGidObject_found;
		if (ppOD_opaque_found)
			*ppOD_opaque_found = pOD_opaque_found;
	}
	else if (pbFound)
	{
		*pbFound = SG_FALSE;
	}
	else
	{
		SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
	}

	// fall thru to common cleanup

fail:
	SG_TREEDIFF2_ITERATOR_NULLFREE(pCtx, pIter);
}

//////////////////////////////////////////////////////////////////

void SG_treediff2__set_mark(SG_context * pCtx, SG_treediff2 * pTreeDiff, const char * szGidObject, SG_int32 newValue, SG_int32 * pOldValue)
{
	const SG_treediff2_ObjectData * pOD_opaque;

	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NONEMPTYCHECK_RETURN(szGidObject);

	SG_ERR_CHECK_RETURN(  SG_treediff2__get_ObjectData(pCtx, pTreeDiff,szGidObject,NULL, &pOD_opaque)  );
	SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__set_mark(pCtx, (SG_treediff2_ObjectData *)pOD_opaque, newValue, pOldValue)  );
}

void SG_treediff2__get_mark(SG_context * pCtx, SG_treediff2 * pTreeDiff, const char * szGidObject, SG_int32 * pValue)
{
	const SG_treediff2_ObjectData * pOD_opaque;

	SG_NULLARGCHECK_RETURN(pTreeDiff);
	SG_NONEMPTYCHECK_RETURN(szGidObject);

	SG_ERR_CHECK_RETURN(  SG_treediff2__get_ObjectData(pCtx,pTreeDiff,szGidObject,NULL, &pOD_opaque)  );
	SG_ERR_CHECK_RETURN(  SG_treediff2__ObjectData__get_mark(pCtx, pOD_opaque, pValue)  );
}

void SG_treediff2__foreach_with_marker_value(SG_context * pCtx,
											 SG_treediff2 * pTreeDiff,
											 SG_int32 value,
											 SG_treediff2_foreach_callback pfnCB,
											 void * pVoidCallerData)
{
	SG_rbtree_iterator * pIter = NULL;
	SG_bool bFound;
	ObjectData * pOD;
	const char * szKey;

	SG_NULLARGCHECK_RETURN(pTreeDiff);
	//SG_NULLARGCHECK_RETURN(pfnCB);

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,
											  &pIter,pTreeDiff->prbRawSummary,
											  &bFound,&szKey,(void **)&pOD)  );
	while (bFound)
	{
		SG_ASSERT_RELEASE_FAIL(  (pOD->apInst[Ndx_Net])  );

		if (pOD->user_marker_data == value)
			SG_ERR_CHECK(  (*pfnCB)(pCtx,pTreeDiff,szKey,(const SG_treediff2_ObjectData *)pOD,pVoidCallerData)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter,&bFound,&szKey,(void **)&pOD)  );
	}

	// fall thru to common cleanup

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}

void SG_treediff2__set_all_markers(SG_context * pCtx,
								   SG_treediff2 * pTreeDiff,
								   SG_int32 value)
{
	SG_rbtree_iterator * pIter = NULL;
	SG_bool bFound;
	ObjectData * pOD;
	const char * szKey;

	SG_NULLARGCHECK_RETURN(pTreeDiff);

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,
											  &pIter,pTreeDiff->prbRawSummary,
											  &bFound,&szKey,(void **)&pOD)  );
	while (bFound)
	{
		SG_ASSERT_RELEASE_FAIL(  (pOD->apInst[Ndx_Net])  );

		pOD->user_marker_data = value;		// SG_treediff2__set_mark(pCtx, pTreeDiff, szKey, value, NULL).

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter,&bFound,&szKey,(void **)&pOD)  );
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}

//////////////////////////////////////////////////////////////////

#define MY_FILTER_FLAGS__OMIT								0x010000
#define MY_FILTER_FLAGS__DESCENDANT_OF_DIRECTORY_MATCH		0x008000
#define MY_FILTER_FLAGS__NAMED_ITEM							0x004000
#define MY_FILTER_FLAGS__IMMEDIATE_CHILD_OF_NAMED_ITEM		0x002000
#define MY_FILTER_FLAGS__DESCENDANT_OF_NAMED_ITEM			0x001000

struct filter_items_data
{
	SG_uint32 count_items;
	SG_uint32 count_includes;
	SG_uint32 count_excludes;
	SG_uint32 count_ignores;

	SG_bool bRecursive;

	const char** paszItems;
	const char* const* paszIncludes;
	const char* const* paszExcludes;
	const char* const* paszIgnores;

	SG_rbtree * prbRepoPath;				// map[repo-path --> vector[] --> pObjectData]
	SG_rbtree * prbRawSummary;				// map[gidObject --> pObjectData]
	SG_rbtree * prbObjectDataAllocated;		// map[gidObject --> pObjectData]

	SG_pathname* pPathWorkingDirectoryTop;
};

//////////////////////////////////////////////////////////////////

/**
 * We are called for each item in the treediff result set.
 * Populate a version of the list sorted by repo-path.
 *
 * We strip off trailing slashes when we build the list.
 * Although we know the correct value for them here (because
 * we are walking thru the list of pOD's), the lookup code
 * won't necessarily know whether it should append one on
 * an explicitly named item, because it may not exist and
 * we can't necessarily stat() it.
 *
 * The only exception is for "@/".  I think we need to keep
 * the slash so that we're clear on the root directory being
 * a little different from stuff within the WD.
 */
static SG_rbtree_foreach_callback _build_repo_path_list;

static void _build_repo_path_list(SG_context * pCtx, const char * pszKey_Gid, void * pVoidAssoc, void * pVoidData)
{
	const ObjectData * pOD = (const ObjectData *)pVoidAssoc;
	struct filter_items_data * pFid = (struct filter_items_data *)pVoidData;
	SG_string * pStrRepoPath_Allocated = NULL;
	SG_string * pStrRepoPath;
	SG_vector * pVec_Allocated = NULL;
	SG_vector * pVec;
	SG_bool bFound;

	SG_ASSERT_RELEASE_FAIL(  (pOD->apInst[Ndx_Net])  );
	SG_UNUSED(pszKey_Gid);

	pStrRepoPath = pOD->apInst[Ndx_Net]->pStrRepoPath;

	if (pOD->apInst[Ndx_Net]->tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		SG_ERR_CHECK(  SG_STRING__ALLOC__COPY(pCtx, &pStrRepoPath_Allocated, pStrRepoPath)  );
		SG_ERR_CHECK(  SG_repopath__remove_final_slash(pCtx, pStrRepoPath_Allocated)  );
		if (SG_string__length_in_bytes(pStrRepoPath_Allocated) == 1)
			SG_ERR_CHECK(  SG_repopath__ensure_final_slash(pCtx, pStrRepoPath_Allocated)  );

		pStrRepoPath = pStrRepoPath_Allocated;
	}

	// because repo-paths may not be unique (say we both DELETE "@/foo" and ADD another one),
	// we build a vector of the values rather than having a single value.

#if TRACE_TREEDIFF_FILTER
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: Adding repo-path to index [%s]\n", SG_string__sz(pStrRepoPath))  );
#endif

	SG_ERR_CHECK(  SG_rbtree__find(pCtx, pFid->prbRepoPath,
								   SG_string__sz(pStrRepoPath),
								   &bFound, (void **)&pVec)  );
	if (!bFound)
	{
		SG_ERR_CHECK(  SG_VECTOR__ALLOC(pCtx, &pVec_Allocated, 3)  );
		SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pFid->prbRepoPath,
												  SG_string__sz(pStrRepoPath),
												  pVec_Allocated)  );
		pVec = pVec_Allocated;
		pVec_Allocated = NULL;
	}

	SG_ERR_CHECK(  SG_vector__append(pCtx, pVec, (void *)pOD, NULL)  );

fail:
	SG_STRING_NULLFREE(pCtx, pStrRepoPath_Allocated);
	SG_VECTOR_NULLFREE(pCtx, pVec_Allocated);
}

//////////////////////////////////////////////////////////////////

/**
 * Look at each of "named" items in the named item list
 * and find them in the repo-path-sorted list of items.
 * Tag them for later.
 */
static void _mark_named_items(SG_context * pCtx,
							  struct filter_items_data * pFid)
{
	SG_pathname * pPath_k = NULL;
	SG_string * pStrRepoPath_k = NULL;
	SG_uint32 k;

	for (k=0; k < pFid->count_items; k++)
	{
		SG_vector * pVec_k;
		SG_bool bFound;

		SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pPath_k, pFid->paszItems[k])  );

		// DO NOT put trailing slash on the repo-path.  In fact, strip
		// it off if given on the command line *UNLESS* it is "@/".
		SG_ERR_CHECK(  SG_workingdir__wdpath_to_repopath(pCtx, pFid->pPathWorkingDirectoryTop, pPath_k, SG_FALSE, &pStrRepoPath_k)  );
		SG_ERR_CHECK(  SG_repopath__remove_final_slash(pCtx, pStrRepoPath_k)  );
		if (SG_string__length_in_bytes(pStrRepoPath_k) == 1)
			SG_ERR_CHECK(  SG_repopath__ensure_final_slash(pCtx, pStrRepoPath_k)  );

#if TRACE_TREEDIFF_FILTER
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: Looking for named item [%s]\n", SG_string__sz(pStrRepoPath_k))  );
#endif

		SG_ERR_CHECK(  SG_rbtree__find(pCtx, pFid->prbRepoPath, SG_string__sz(pStrRepoPath_k), &bFound, (void **)&pVec_k)  );
		if (bFound)
		{
			SG_uint32 lenVec;
			SG_uint32 j;

			SG_ERR_CHECK(  SG_vector__length(pCtx, pVec_k, &lenVec)  );
			for (j=0; j<lenVec; j++)
			{
				SG_treediff2_ObjectData * pOD_k_j;
				SG_int32 marker = 0;

				SG_ERR_CHECK(  SG_vector__get(pCtx, pVec_k, j, (void **)&pOD_k_j)  );

#if TRACE_TREEDIFF_FILTER
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: Tagging Named Item [%s]\n", SG_string__sz(pStrRepoPath_k))  );
#endif

				SG_ERR_CHECK(  SG_treediff2__ObjectData__get_mark(pCtx, pOD_k_j, &marker)  );
				marker |= MY_FILTER_FLAGS__NAMED_ITEM;
				SG_ERR_CHECK(  SG_treediff2__ObjectData__set_mark(pCtx, pOD_k_j, marker, NULL)  );
			}
		}

		SG_PATHNAME_NULLFREE(pCtx, pPath_k);
		SG_STRING_NULLFREE(pCtx, pStrRepoPath_k);
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_k);
	SG_STRING_NULLFREE(pCtx, pStrRepoPath_k);
}

/**
 * We are called for each individual item in the repo-path-ordered list.
 * See if this item should be filtered out.
 *
 * Since the treediff is completely constructed before we do
 * this filtering, we don't have the chance to modify the dive
 * thru the filesystem (as we do on scan-dir), so we need to
 * propagate down some flags that would have been implicit if
 * we were diving.
 */
static void _process_filter_item2(SG_context * pCtx,
								  const char * pszKey_RepoPath,
								  ObjectData * pOD,
								  struct filter_items_data * pFid)
{
	SG_int32 marker = 0;

	// Throughout this routine we use the pszKey_RepoPath argument rather
	// than pOD->apInst[Ndx_Net]->pStrRepoPath because we've already
	// fixed-up any trailing slashes when we built the list.

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_mark(pCtx, (SG_treediff2_ObjectData *)pOD, &marker)  );

	if (pOD->apInst[Ndx_Net]->bufParentGid[0])
	{
		const SG_treediff2_ObjectData * pOD_Parent = NULL;
		SG_bool bFound_Parent = SG_FALSE;

		SG_ERR_CHECK(  SG_rbtree__find(pCtx, pFid->prbObjectDataAllocated, pOD->apInst[Ndx_Net]->bufParentGid,
									   &bFound_Parent, (void **)&pOD_Parent)  );
		SG_ASSERT(  (bFound_Parent)  );
		if (bFound_Parent)
		{
			SG_int32 marker_parent = 0;

			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_mark(pCtx, pOD_Parent, &marker_parent)  );

			// since the NON-RECURSIVE stuff keys off of a "named" item,
			// we need to record if this item is a descendant of one and
			// if so, how deep from it.

			if (marker_parent & MY_FILTER_FLAGS__NAMED_ITEM)
			{
				marker |= MY_FILTER_FLAGS__IMMEDIATE_CHILD_OF_NAMED_ITEM;
				if (!pFid->bRecursive)
				{
					marker |= MY_FILTER_FLAGS__OMIT;
#if TRACE_TREEDIFF_FILTER
					SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: omitting (non-recursive 1) [%s]\n", pszKey_RepoPath)  );
#endif
				}
			}
			else if (marker_parent & (MY_FILTER_FLAGS__IMMEDIATE_CHILD_OF_NAMED_ITEM | MY_FILTER_FLAGS__DESCENDANT_OF_NAMED_ITEM))
			{
				marker |= MY_FILTER_FLAGS__DESCENDANT_OF_NAMED_ITEM;
				if (!pFid->bRecursive)
				{
					marker |= MY_FILTER_FLAGS__OMIT;
#if TRACE_TREEDIFF_FILTER
					SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: omitting (non-recursive 2) [%s]\n", pszKey_RepoPath)  );
#endif
				}
			}

			if (marker_parent & SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED)
			{
				marker |= SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED;
				marker |= MY_FILTER_FLAGS__OMIT;
#if TRACE_TREEDIFF_FILTER
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: omitting (parent excluded) [%s]\n", pszKey_RepoPath)  );
#endif
			}
			else if (marker_parent & SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED)
			{
				marker |= SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED;
				marker |= MY_FILTER_FLAGS__OMIT;
#if TRACE_TREEDIFF_FILTER
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: omitting (parent ignored) [%s]\n", pszKey_RepoPath)  );
#endif
			}
			else if (marker_parent & (SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED | MY_FILTER_FLAGS__DESCENDANT_OF_DIRECTORY_MATCH))
			{
				marker |= MY_FILTER_FLAGS__DESCENDANT_OF_DIRECTORY_MATCH;
			}
		}
	}

	// If we already know to filter-out/omit this item, don't
	// bother looking at the repo-path.

	if ((marker & MY_FILTER_FLAGS__OMIT) == 0)
	{
		SG_file_spec_eval eval;
		SG_uint32 local_count_ignores;
		SG_uint32 local_count_includes;
		const char * const * local_paszIgnores;
		const char * const * local_paszIncludes;

		// If this item is somewhere within a directory that was
		// EXPLICITLY INCLUDED (matched), then when looking at this
		// item, we DO NOT use the INCLUDES (if present) because the
		// original match was on the ancestor directory, not us.

		if (marker & MY_FILTER_FLAGS__DESCENDANT_OF_DIRECTORY_MATCH)
		{
			local_count_includes = 0;
			local_paszIncludes = NULL;
		}
		else
		{
			local_count_includes = pFid->count_includes;
			local_paszIncludes = pFid->paszIncludes;
		}

		// If this item is under version control, we don't want
		// the IGNORES to apply.

		if (pOD->apInst[Ndx_Net]->dsFlags & SG_DIFFSTATUS_FLAGS__FOUND)
		{
			local_count_ignores = pFid->count_ignores;
			local_paszIgnores = pFid->paszIgnores;
		}
		else
		{
			local_count_ignores = 0;
			local_paszIgnores = NULL;
		}

		SG_ERR_CHECK(  SG_file_spec__should_include(pCtx,
													local_paszIncludes, local_count_includes,
													pFid->paszExcludes, pFid->count_excludes,
													local_paszIgnores,  local_count_ignores,
													pszKey_RepoPath,
													&eval)  );

		switch (eval)
		{
		default:			// quiets compiler
			SG_ASSERT(  (0)  );
			break;

		case SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED:
#if TRACE_TREEDIFF_FILTER
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: filtering [%s] because of EXCLUDE\n", pszKey_RepoPath)  );
#endif
			marker |= SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED;
			marker |= MY_FILTER_FLAGS__OMIT;
			break;

		case SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED:
#if TRACE_TREEDIFF_FILTER
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: filtering [%s] because of IGNORE\n", pszKey_RepoPath)  );
#endif
			marker |= SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED;
			marker |= MY_FILTER_FLAGS__OMIT;
			break;

		case SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED:
#if TRACE_TREEDIFF_FILTER
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: including [%s] because of EXPLICIT INCLUDE\n", pszKey_RepoPath)  );
#endif
			marker |= SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED;
			break;

		case SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED:
			// Item did not match an EXCLUDE/IGNORE and there were no --INCLUDES.
			// If there ARE NO items explicitly named on the command line, then
			// we implicitly include everything.
			// If there ARE items explicitly names, then we only want them and
			// (maybe) their descendants.
			if (pFid->count_items == 0)
			{
#if TRACE_TREEDIFF_FILTER
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: implicitly including [%s]\n", pszKey_RepoPath)  );
#endif
				marker |= SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED;
			}
			else if (marker & MY_FILTER_FLAGS__NAMED_ITEM)
			{
#if TRACE_TREEDIFF_FILTER
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: implicitly including named item [%s]\n", pszKey_RepoPath)  );
#endif
				marker |= SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED;
			}
			else if (marker & (MY_FILTER_FLAGS__IMMEDIATE_CHILD_OF_NAMED_ITEM | MY_FILTER_FLAGS__DESCENDANT_OF_NAMED_ITEM))
			{
				SG_ASSERT(  (pFid->bRecursive)  );		// parent setion already checked this
#if TRACE_TREEDIFF_FILTER
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: implicitly including descendant of named item [%s]\n", pszKey_RepoPath)  );
#endif
				marker |= SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED;
			}
			else
			{
#if TRACE_TREEDIFF_FILTER
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: omitting unnamed or non-descendant [%s]\n", pszKey_RepoPath)  );
#endif
				marker |= SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED;
				marker |= MY_FILTER_FLAGS__OMIT;
			}
			break;

		case SG_FILE_SPEC_EVAL__MAYBE:
			// A "maybe" means that there were INCLUDEs but this item
			// did not match one of them.  If we were doing a normal
			// "dive", we'd omit files here and defer the decision
			// for directories so that we could keep "diving" and look
			// for matching stuff within.  But since we have the
			// complete treediff and aren't actually diving, we can
			// just omit both types of items now.
#if TRACE_TREEDIFF_FILTER
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TreeDiff_Filter: omitting 'maybe' item [%s]\n", pszKey_RepoPath)  );
#endif
			marker |= MY_FILTER_FLAGS__OMIT;
			break;
		}
	}

	SG_ERR_CHECK(  SG_treediff2__ObjectData__set_mark(pCtx, (SG_treediff2_ObjectData *)pOD, marker, NULL)  );

	if (marker & MY_FILTER_FLAGS__OMIT)
	{
		// we can go ahead and remove the item from the filtered result
		// (even for directories) because the above parent lookup uses
		// prbObjectDataAllocated rather than prbRawSummary.

		// Please do not remove it from the cache, since we may need it if its
		// gid is referenced by another change (like move, for example).
		SG_ERR_CHECK(  SG_rbtree__remove(pCtx, pFid->prbRawSummary, pOD->bufGidObject)  );
	}

fail:
	return;
}

/**
 * We are called for each item in the repo-path-ordered list.
 * This is a VECTOR of all items that have the same repo-path.
 * See if these items should be filtered out.
 */
static SG_rbtree_foreach_callback _process_filter_item;

static void _process_filter_item(SG_context * pCtx, const char * pszKey_RepoPath, void * pVoidAssoc, void * pVoidData)
{
	SG_vector * pVec_k = (SG_vector *)pVoidAssoc;
	struct filter_items_data * pFid = (struct filter_items_data *)pVoidData;
	SG_uint32 lenVec;
	SG_uint32 j;

	SG_ERR_CHECK(  SG_vector__length(pCtx, pVec_k, &lenVec)  );
	for (j=0; j<lenVec; j++)
	{
		ObjectData * pOD_k_j;

		SG_ERR_CHECK(  SG_vector__get(pCtx, pVec_k, j, (void **)&pOD_k_j)  );
		SG_ERR_CHECK(  _process_filter_item2(pCtx, pszKey_RepoPath, pOD_k_j, pFid)  );
	}

fail:
	return;
}

/**
 * TODO 2010/04/28 While we're at it, could we fix the calling sequence.  For "items" we
 * TODO            have "argc, argv" and for "includes/excludes" we have "argv, argc".
 * TODO            Pick an ordering.
 */
void SG_treediff2__file_spec_filter(SG_context * pCtx,
									SG_treediff2 * pTreeDiff,
									SG_uint32 count_items, const char ** paszItems,
									SG_bool bRecursive,
									const char * const* ppszIncludes, SG_uint32 count_includes,
									const char * const* ppszExcludes, SG_uint32 count_excludes,
									SG_bool bIgnoreIgnores)
{
	SG_stringarray * psaIgnores = NULL;
	struct filter_items_data fid;

	memset(&fid, 0, sizeof(fid));

	SG_NULLARGCHECK_RETURN(pTreeDiff);

	fid.count_excludes = count_excludes;
	fid.count_includes = count_includes;
	fid.count_items = count_items;
	fid.paszItems = paszItems;
	fid.paszIncludes = ppszIncludes;
	fid.paszExcludes = ppszExcludes;
	fid.bRecursive = bRecursive;
	fid.prbRawSummary = pTreeDiff->prbRawSummary;
	fid.prbObjectDataAllocated = pTreeDiff->prbObjectDataAllocated;
	fid.pPathWorkingDirectoryTop = pTreeDiff->pPathWorkingDirectoryTop;

	if (!bIgnoreIgnores)
	{
		SG_ERR_CHECK(  SG_file_spec__patterns_from_ignores_localsetting(pCtx, &psaIgnores)  );
		SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, psaIgnores, &fid.paszIgnores, &fid.count_ignores)  );
	}

	if ((!bRecursive) || (fid.count_ignores > 0) || (count_items > 0) || (count_includes > 0) || (count_excludes > 0))
	{
		SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &fid.prbRepoPath)  );

		// TREEDIFFs are computed without any filtering (we DO NOT use the
		// INCLUDES/EXCLUDES/IGNORES/NONRECURSIVE args when computing the
		// diff), so we have a COMPLETE treediff.
		//
		// Here we want to filter the RESULTS.  We cannot just look at each
		// entry in isolation (and do a match on its repo-path) because an
		// item may be within an excluded/ignored directory.
		//
		// [] Mark each item as "maybe".
		// [] Build a list of the items sorted by repo-path (so that a
		//    directory is visited before anything within that directory).
		// [] If there are explicitly named items, we tag them (so that
		//    we can handle recursive/non-recursive feature).
		// [] Walk this list and eval each item.
		//    () Use the repo-path to see if the parent directory was
		//       filtered-out before bothering to look at this item directly.
		//    () Eval the repo-path of the item.
		//    () Update the item with a combined marker-flag/eval code.
		//    () Remove items that should be filtered out.

		SG_ERR_CHECK(  SG_treediff2__set_all_markers(pCtx, pTreeDiff, (SG_int32)SG_FILE_SPEC_EVAL__MAYBE)  );
		SG_ERR_CHECK(  SG_rbtree__foreach(pCtx, fid.prbRawSummary, _build_repo_path_list, &fid)  );
		SG_ERR_CHECK(  _mark_named_items(pCtx, &fid)  );
		SG_ERR_CHECK(  SG_rbtree__foreach(pCtx, fid.prbRepoPath,   _process_filter_item,  &fid)  );
	}

fail:
	SG_STRINGARRAY_NULLFREE(pCtx, psaIgnores);
	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, fid.prbRepoPath, (SG_free_callback *)SG_vector__free);
}
