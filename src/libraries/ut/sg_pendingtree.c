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

#include <sg.h>

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#	define TRACE_PENDINGTREE		0
#else
#	define TRACE_PENDINGTREE		0
#endif

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#	define TRACE_COMMIT				0
#else
#	define TRACE_COMMIT				0
#endif

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#	define TRACE_ADD				0
#else
#	define TRACE_ADD				0
#endif

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#	define TRACE_ADDREMOVE				0
#else
#	define TRACE_ADDREMOVE				0
#endif

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#define TRACE_REVERT		0
#else
#define TRACE_REVERT		0
#endif

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#define TRACE_REMOVE		0
#else
#define TRACE_REMOVE		0
#endif

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#    define TRACE_TIMESTAMP_CACHE	0
#    define STATS_TIMESTAMP_CACHE	0
#else
#    define TRACE_TIMESTAMP_CACHE	0
#    define STATS_TIMESTAMP_CACHE	0
#endif

//////////////////////////////////////////////////////////////////

/* TODO many of the SG_pendingtree__ functions save the tree when they
 * exit.  Not sure this is a good idea, because we can't do two
 * operations in a row.
 *
 * TODO [X] this is potentially bad because the chunk of code that says:
 *         // we don't change the saved pending changeset here.
 *         sg_ptnode__free(pPendingTree->pSuperRoot);
 *         pPendingTree->pSuperRoot = NULL;
 * only gets called when the main function is successful.  If something
 * low level takes an error, the free will get called by the containing
 * function and it may write out the data.
 *
 * We should probably change this.
 *
 */

#define sg_PTNODE_TEMP_FLAG_CHECKMYNAME    2

/* sg_PTNODE_TEMP_FLAG_COMMITTING means that the item is in the process
 * of being committed to the repo.  When a commit operation begins,
 * first we go through the tree and set this COMMITTING flag on
 * every item which is supposed to be included in the commit.  If
 * nothing has been specified, we assume we are supposed to commit all
 * changes and we just set the flag on every node.  For every file
 * which has been specified, we set the flag and bubble it up.  For
 * every directory that has been specified, we set the flag on it and
 * all its descendants and also bubble it up. */

#define sg_PTNODE_TEMP_FLAG_COMMITTING    4

#define sg_PTNODE_SAVED_FLAG_DELETED    8

#define sg_PTNODE_SAVED_FLAG_MOVED_AWAY    16

#define sg_PTNODE_TEMP_FLAG_IMPLICIT_DELETE 32

#define sg_PTNODE_TEMP_FLAG_REVERTING    64

#define sg_PTNODE_TEMP_FLAG_IMPLICIT_ADD 256

#define sg_PTNODE_TEMP_FLAG_PARKED_FOR_CYCLE 512

/* Some items in the ptnode are CURRENT.  They tell us information
 * which is reflected in the pending tree, in the working directory.
 * Other items are BASELINE.  They tell us information about how
 * things were in the baseline version where we started. */
struct sg_ptnode
{
	SG_pendingtree* pPendingTree; /* never null */

	struct sg_ptnode* pCurrentParent; /* null for the root treenode
									   * only.  This is the CURRENT
									   * parent. */
	
	const char* pszgidMovedFrom; /* non-null iff this item was moved here
								  * from somewhere else. used to check
								  * for split moves */

	SG_treenode_entry* pBaselineEntry; /* NULL if this item wasn't in
										* the baseline.  Also NULL for
										* the very top node in the
										* tree.  If non-NULL, this is
										* a copy which needs to be
										* freed. */

	SG_treenode_entry* pNewEntry; /* During commit, each item which
								   * appears in the committed tree
								   * gets a new entry, which is stored
								   * here.  Post-commit this entry is
								   * moved to pBaselineEntry. */

	const char* pszgid; /* This is here for convenience.  If
						 * pBaselineEntry is present, its gid will
						 * always be the same as this one.  If
						 * pCurrentParent is non-NULL, then this gid
						 * is the same gid which is used for the key
						 * in pCurrentParent->prbItems. Note that this
						 * member is not identified as current or
						 * baseline, because it cannot change. */
	
	const char* pszCurrentName; /* If NULL, use the name in the entry. */
	
	const char* pszCurrentHid;
    const char* pszHidXattrs;
	
    SG_int64 iCurrentAttributeBits;

	SG_treenode_entry_type type;
	SG_uint16 saved_flags;
	SG_uint16 temp_flags;
	
	/* Stuff that's valid only for type == DIRECTORY */
	SG_rbtree* prbItems;  /* NULL until it's needed. */

	struct sg_ptnode * ptnPhantomMe; /* transient. only set for MOVES during REVERT.
									  * this helps us undo moves when there are path
									  * cycles (and the tree is temporarily broken
									  * (either disconnected, cyclic, or both)). */

	/* Note that there is no local path here.  Here's why: If there
	 * were, then a rename operation on a directory node would have to
	 * go through all its child nodes and fix up the path.  Not so
	 * bad, but then a revert of that rename operation would have to
	 * go through and fix them all.  Better to just not have a path
	 * stored here at all.  Instead, when the path is needed (during
	 * commit, for adding a file blob), we simply calculate it during
	 * commit. */
};

struct _SG_pendingtree
{
	SG_string* pstrRepoDescriptorName;
	SG_pathname* pPathWorkingDirectoryTop;
	SG_strpool* pStrPool;
	SG_repo* pRepo;
	struct sg_ptnode* pSuperRoot;

	// 4/21/10 We hold the ".sgdrawer/wd.json" VFILE open and locked for the
	// duration of an operation.  We also keep the VHASH in memory.  These
	// contain was we used to keep in "repo.json" and "pending.json".
	SG_vfile * pvf_wd;
	SG_vhash * pvh_wd;
#if defined(DEBUG)
	SG_bool b_pvh_wd__parents__dirty;		// someone called __set_wd_parents() but not __save()
	SG_bool b_pvh_wd__issues__dirty;		// someone called __set_wd_issues() but not __save()
	SG_bool b_pvh_wd__timestamps__dirty;	// someone called __set_wd_timestamps() but not __save()
#endif

	SG_utf8_converter *			pConverter;
    SG_varray *					pvaWarnings;
	SG_portability_flags		portMask;
    SG_bool						bIgnoreWarnings;

	SG_pendingtree_action_log_enum		eActionLog;
	SG_varray *							pvaActionLog;	// varray[<vhash *>] of details

	/* TODO ndx ? */

	SG_bool bDebugExport;	// true if we are in __export_to_vhash and want extra info

#if STATS_TIMESTAMP_CACHE
	struct
	{
		SG_uint32 nr_valid;
		SG_uint32 nr_queries;
	} trace_timestamp_cache;
#endif
};

//////////////////////////////////////////////////////////////////

static void sg_ptnode__free(SG_context* pCtx, struct sg_ptnode* ptn);

#define SG_PTNODE_NULLFREE(pCtx, p)    SG_STATEMENT( SG_context__push_level(pCtx); sg_ptnode__free(pCtx, p); SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx);  p=NULL; )

//////////////////////////////////////////////////////////////////

static void sg_ptnode__get_name(SG_context* pCtx, 
	const struct sg_ptnode* ptn,
	const char** ppszName
	);

//////////////////////////////////////////////////////////////////

static void _sg_pendingtree__do_diff__unload_clean_objects(SG_context* pCtx, SG_pendingtree * pPendingTree,
														   const char * szHidArbitraryCSet,		/* optional */
														   SG_treediff2 * pTreeDiff_cache_owner,	/* optional */
														   SG_treediff2 ** ppTreeDiff);

//////////////////////////////////////////////////////////////////

/**
 * Allocate a utf-8-->? converter for testing for portability problems.
 * Since we internally store all entrynames in UTF-8, we *may* have a
 * problem expressing them on a platform that requires us to convert them
 * to an OS/native/locale/whatever (non-unicode) encoding.
 *
 * We allocate the proper converter and stuff it in the pendingtree, so
 * subsequent calls will just reference this value; only the first call
 * will actually allocate it.
 */
static void sg_pendingtree__configure_portability_converter(SG_context * pCtx, SG_pendingtree * pPendingTree)
{
	if (!pPendingTree->pConverter)
	{
		const char * pszCharSetName = NULL;

		// TODO determine if the REPO has a specific LOCALE/CHARSET defined and fetch it.
		// TODO for now, we assume UTF-8.

		SG_ERR_CHECK_RETURN(  SG_utf8_converter__alloc(pCtx,pszCharSetName,&pPendingTree->pConverter)  );
	}
}

//////////////////////////////////////////////////////////////////

static void sg_ptnode__check(SG_context* pCtx, 
    struct sg_ptnode* ptn
    )
{
	SG_bool b = SG_FALSE;
	const char* pszGid = NULL;
	struct sg_ptnode* ptnSub = NULL;
	SG_rbtree_iterator* pIterator = NULL;
	SG_portability_dir* pPortabilityDir = NULL;
	SG_portability_flags portWarningsLogged = SG_PORT_FLAGS__NONE;
	
	SG_NULLARGCHECK_RETURN(ptn);

	if (ptn->type != SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
        return;
    }

    if (!ptn->prbItems)
    {
        return;
    }

	// TODO jeff says: I think we need to revisit the NULL for relative-path in the call
	// TODO below.  If we don't have the repo-path for the parent directory, we can't do
	// TODO some of the pathname-length checks.  Granted we won't get them all because
	// TODO the "@/" is just a placeholder for whereever they create their working directory,
	// TODO but we can catch the cases where the part of the repo-path after the "@/" is
	// TODO really long and there's a long entryname and the result is > 256 and might
	// TODO cause problems on other platforms.

	SG_ERR_CHECK(  SG_portability_dir__alloc(pCtx,
											 ptn->pPendingTree->portMask,
											 NULL, /* no relative path needed, I think */
											 ptn->pPendingTree->pConverter,
											 ptn->pPendingTree->pvaWarnings,
											 &pPortabilityDir)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptn->prbItems, &b, &pszGid, (void**) &ptnSub)  );
    while (b)
    {
		if (
			(ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
			|| (ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
			|| (ptnSub->temp_flags & sg_PTNODE_TEMP_FLAG_IMPLICIT_ADD)
			)
		{
		}
		else
		{
            const char* pszName = NULL;
			SG_bool bIsDuplicate = SG_FALSE;

			SG_ERR_CHECK(  sg_ptnode__get_name(pCtx, ptnSub, &pszName)  );

            //////////////////////////////////////////////////////////////////
            // compute the portability and/or potential portability problems with this entryname.
            // this includes filenames that are/may be invalid on some platform and *possible*
            // collisions on some platforms due to things like case-folding.
            // 
            // we only log a complaint about an item if it is a new/renamed/moved item.
            // we always compute the info because we need the collision data.

            SG_ERR_CHECK(  SG_portability_dir__add_item(pCtx, 
														pPortabilityDir,
														NULL,
														pszName,
														((ptnSub->temp_flags & sg_PTNODE_TEMP_FLAG_CHECKMYNAME)?SG_TRUE:SG_FALSE),
														((ptnSub->temp_flags & sg_PTNODE_TEMP_FLAG_CHECKMYNAME)?SG_TRUE:SG_FALSE),
														&bIsDuplicate)  );

			// TODO the original SG_portability_dir__add_item() threw SG_ERR_RBTREE_DUPLICATEKEY when
            // TODO you have 2 identical strings in this directory.  THIS **CAN** HAPPEN IN THINGS
            // TODO LIKE "vv rename" BECAUSE WE DO THE PORTABILITY TEST BEFORE WE MODIFY THE DIRECTORY.
			// TODO
			// TODO I changed the portability code to return a flag for that case rather than throwing an error.
            // TODO
            // TODO I need to revisit the callers of this function and see if they'd rather have the error or
            // TODO a flag (or a different error code).  But for now, I throw the original error code

			if (bIsDuplicate)
				SG_ERR_THROW(SG_ERR_RBTREE_DUPLICATEKEY);
        }
        
        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszGid, (void**) &ptnSub)  );
    }
    SG_rbtree__iterator__free(pCtx, pIterator);
    pIterator = NULL;

	SG_ERR_CHECK(  SG_portability_dir__get_result_flags(pCtx, pPortabilityDir,NULL,NULL,&portWarningsLogged)  );

	SG_PORTABILITY_DIR_NULLFREE(pCtx, pPortabilityDir);

    if (ptn->pPendingTree->bIgnoreWarnings)
    {
    }
    else
    {
		if (portWarningsLogged != SG_PORT_FLAGS__NONE)
        {
            SG_ERR_THROW(  SG_ERR_PORTABILITY_WARNINGS  );
        }
    }

    return;

fail:
	SG_PORTABILITY_DIR_NULLFREE(pCtx, pPortabilityDir);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}

static void sg_ptnode__clone_for_move(
	SG_context* pCtx, 
	const struct sg_ptnode* ptnOrig,
	struct sg_ptnode** ppCopy
	)
{
	struct sg_ptnode* ptn;

	SG_ERR_CHECK(  SG_alloc1(pCtx, ptn)  );

	ptn->pPendingTree = ptnOrig->pPendingTree;
	SG_ERR_CHECK(  SG_TREENODE_ENTRY__ALLOC__COPY(pCtx, &ptn->pBaselineEntry, ptnOrig->pBaselineEntry)  );
	ptn->pszgid = ptnOrig->pszgid;
	ptn->pszCurrentName = ptnOrig->pszCurrentName;
	ptn->type = ptnOrig->type;
	ptn->pszCurrentHid = ptnOrig->pszCurrentHid;
    ptn->pszHidXattrs = ptnOrig->pszHidXattrs;
    ptn->iCurrentAttributeBits = ptnOrig->iCurrentAttributeBits;
	
	*ppCopy = ptn;
	
	return;

fail:
	if (ptn != NULL && ptn->pBaselineEntry != NULL)
		SG_TREENODE_ENTRY_NULLFREE(pCtx, ptn->pBaselineEntry);
	SG_PTNODE_NULLFREE(pCtx, ptn);
}

static void sg_ptnode__alloc__top(
	SG_context* pCtx, 
	SG_pendingtree* pPendingTree,
	const char* pszHid
	)
{
	struct sg_ptnode* ptn;
	
    SG_ASSERT(!pPendingTree->pSuperRoot);

	SG_ERR_CHECK(  SG_alloc1(pCtx, ptn)  );

	ptn->pPendingTree = pPendingTree;
	ptn->type = SG_TREENODEENTRY_TYPE_DIRECTORY;
	ptn->prbItems = NULL;
	ptn->pszgid = NULL;
	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pPendingTree->pStrPool, pszHid, &ptn->pszCurrentHid)  );

	pPendingTree->pSuperRoot = ptn;

	return;

fail:
	SG_PTNODE_NULLFREE(pCtx, ptn);
}

static void sg_ptnode__alloc__load(
	SG_context* pCtx, 
	struct sg_ptnode** ppNew,
	SG_pendingtree* pPendingTree,
	const SG_treenode_entry* pEntry,
	const char* pszCurrentName,
	SG_treenode_entry_type type,
	const char* pszGid,
	const char* pszHid,
	const char* pszHidXattrs,
    SG_int64 iAttributeBits,
	const char* pszgidMovedFrom,
	SG_uint16 saved_flags
	)
{
	struct sg_ptnode* ptn;
	
	SG_ERR_CHECK(  SG_alloc1(pCtx, ptn)  );

	ptn->pPendingTree = pPendingTree;
	ptn->type = type;
	ptn->saved_flags = saved_flags;

	if (pEntry)
	{
		SG_ERR_CHECK(  SG_TREENODE_ENTRY__ALLOC__COPY(pCtx, &ptn->pBaselineEntry, pEntry)  );
	}
	if (pszCurrentName)
	{
		SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pPendingTree->pStrPool, pszCurrentName, &ptn->pszCurrentName)  );
	}
	if (pszGid)
	{
		SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pPendingTree->pStrPool, pszGid, &ptn->pszgid)  );
	}
	if (pszHid)
	{
		SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pPendingTree->pStrPool, pszHid, &ptn->pszCurrentHid)  );
	}
	if (pszHidXattrs)
	{
		SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pPendingTree->pStrPool, pszHidXattrs, &ptn->pszHidXattrs)  );
    }
    ptn->iCurrentAttributeBits = iAttributeBits;
	if (pszgidMovedFrom)
	{
		SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pPendingTree->pStrPool, pszgidMovedFrom, &ptn->pszgidMovedFrom)  );
	}

	*ppNew = ptn;
	
	return;

fail:
	SG_PTNODE_NULLFREE(pCtx, ptn);
}

static void sg_ptnode__alloc__entry(
	SG_context* pCtx, 
	struct sg_ptnode** ppNew,
	SG_pendingtree* pPendingTree,
	const SG_treenode_entry* pEntry,
	const char* pszgid
	)
{
	struct sg_ptnode* ptn;

	SG_NULLARGCHECK_RETURN(ppNew);
	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(pEntry);

	SG_ERR_CHECK(  SG_alloc1(pCtx, ptn)  );

	ptn->pPendingTree = pPendingTree;
	SG_ERR_CHECK(  SG_treenode_entry__get_entry_type(pCtx, pEntry, &ptn->type)  );
	SG_ERR_CHECK(  SG_TREENODE_ENTRY__ALLOC__COPY(pCtx, &ptn->pBaselineEntry, pEntry)  );
	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, ptn->pPendingTree->pStrPool, pszgid, &ptn->pszgid)  );
    SG_ERR_CHECK(  SG_treenode_entry__get_attribute_bits(pCtx, pEntry, &ptn->iCurrentAttributeBits)  );
	ptn->prbItems = NULL;

	*ppNew = ptn;

	return;
	
fail:
	SG_PTNODE_NULLFREE(pCtx, ptn);
}

static void sg_ptnode__alloc__new__with_gid(
	SG_context* pCtx, 
	struct sg_ptnode** ppNew,
	SG_pendingtree* pPendingTree,
	const char* pszName,
	SG_treenode_entry_type type,
	const char * pszGid
	)
{
	struct sg_ptnode* ptn;
	
	SG_NULLARGCHECK_RETURN(ppNew);
	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(pszName);

	SG_ARGCHECK_RETURN((type == SG_TREENODEENTRY_TYPE_DIRECTORY)
		|| (type == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
		|| (type == SG_TREENODEENTRY_TYPE_SYMLINK), type);
	
	SG_ERR_CHECK(  SG_alloc1(pCtx, ptn)  );

	ptn->pPendingTree = pPendingTree;
	ptn->type = type;
	ptn->prbItems = NULL;
	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pPendingTree->pStrPool, pszName, &ptn->pszCurrentName)  );
	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pPendingTree->pStrPool, pszGid, &ptn->pszgid)  );
	
	*ppNew = ptn;

	return;

fail:
	SG_PTNODE_NULLFREE(pCtx, ptn);
}

static void sg_ptnode__alloc__new(
	SG_context* pCtx, 
	struct sg_ptnode** ppNew,
	SG_pendingtree* pPendingTree,
	const char* pszName,
	SG_treenode_entry_type type
	)
{
	char bufGid[SG_GID_BUFFER_LENGTH];

	SG_ERR_CHECK_RETURN(  SG_gid__generate(pCtx, bufGid, sizeof(bufGid))  );
	SG_ERR_CHECK_RETURN(  sg_ptnode__alloc__new__with_gid(pCtx, ppNew, pPendingTree, pszName, type, bufGid)  );
}

//static SG_free_callback sg_ptnode__free;

static void sg_ptnode__free(SG_context* pCtx, struct sg_ptnode* ptn)
{
	if (!ptn)
	{
		return;
	}

#if TRACE_PENDINGTREE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "sg_ptnode__free: [gid %s][saved %d][temp %d]\n",
							   ptn->pszgid,ptn->saved_flags,ptn->temp_flags)  );
#endif

	if (ptn->prbItems)
	{
		SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, ptn->prbItems, (SG_free_callback *)sg_ptnode__free);
	}

	SG_TREENODE_ENTRY_NULLFREE(pCtx, ptn->pBaselineEntry);
	SG_TREENODE_ENTRY_NULLFREE(pCtx, ptn->pNewEntry);

	SG_NULLFREE(pCtx, ptn);
}

/*
 * If a dir node is not dirty, then we don't need its info loaded,
 * so unload it.
 */
static void sg_ptnode__unload_dir_entries(SG_context* pCtx, struct sg_ptnode* ptn)
{
	SG_NULLARGCHECK_RETURN(ptn);

	SG_ARGCHECK_RETURN(ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY, ptn->type);

#if TRACE_PENDINGTREE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "sg_ptnode__unload_dir_entries: [gid %s][saved %d][temp %d]\n",
							   ptn->pszgid,ptn->saved_flags,ptn->temp_flags)  );
#endif

	if (ptn->prbItems)
	{
		SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, ptn->prbItems, (SG_free_callback *)sg_ptnode__free);
	}

}

static void sg_ptnode__is_file_dirty(
	SG_context* pCtx, 
	const struct sg_ptnode* ptn,
	SG_bool* pbDirty
	)
{
	const char* pszhid = NULL;
	
	if (!ptn->pBaselineEntry)
	{
		*pbDirty = SG_TRUE;
		return;
	}

	/* TODO compare length? */

	if (!ptn->pszCurrentHid)
	{
		*pbDirty = SG_FALSE;
		return;
	}
	
	SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, ptn->pBaselineEntry, &pszhid)  );

	if (0 == strcmp(pszhid, ptn->pszCurrentHid))
	{
		*pbDirty = SG_FALSE;
		return;
	}

	*pbDirty = SG_TRUE;
	return;

fail:
	return;
}

static void sg_ptnode__check_parent_dirty_non_recursive(
	SG_context* pCtx, 
	struct sg_ptnode* ptn,
	SG_bool* pbParentDirty
	);

static void sg_ptnode__unload_dir_entries_if_not_dirty(SG_context* pCtx, struct sg_ptnode* ptn, SG_bool* pbParentDirty)
{
	SG_bool b = SG_FALSE;
	const char* pszKey = NULL;
	struct sg_ptnode* ptnSub = NULL;
	SG_rbtree_iterator* pIterator = NULL;
	SG_bool bSelfDirty = SG_FALSE;
	
	if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		if (ptn->prbItems)
		{
			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptn->prbItems, &b, &pszKey, (void**) &ptnSub)  );
			while (b)
			{
				SG_ERR_CHECK(  sg_ptnode__unload_dir_entries_if_not_dirty(pCtx, ptnSub, &bSelfDirty)  );
				
				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, (void**) &ptnSub)  );
			}
			SG_rbtree__iterator__free(pCtx, pIterator);
			pIterator = NULL;

			if (!bSelfDirty)
			{
				SG_ERR_CHECK(  sg_ptnode__unload_dir_entries(pCtx, ptn)  );
			}
		}
	}
	else
	{
        /* TODO is_file_dirty happens to work for symlinks in this case as
         * well.  But this is a bit risky.  What if is_file_dirty changes?
         * */
		SG_ERR_CHECK(  sg_ptnode__is_file_dirty(pCtx, ptn, &bSelfDirty)  );
	}

	if (bSelfDirty)
	{
		*pbParentDirty = SG_TRUE;
	}
	else
	{
		SG_ERR_CHECK(  sg_ptnode__check_parent_dirty_non_recursive(pCtx, ptn, pbParentDirty)  );
	}
	
	return;

fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}

static void sg_ptnode__undo_implicit(SG_context* pCtx, struct sg_ptnode* ptn)
{
	SG_bool b = SG_FALSE;
	const char* pszKey = NULL;
	struct sg_ptnode* ptnSub = NULL;
	SG_rbtree_iterator* pIterator = NULL;
	SG_rbtree* prbRemove = NULL;

	if (ptn->temp_flags & sg_PTNODE_TEMP_FLAG_IMPLICIT_DELETE)
	{
		ptn->saved_flags &= ~sg_PTNODE_SAVED_FLAG_DELETED;
		ptn->temp_flags &= ~sg_PTNODE_TEMP_FLAG_IMPLICIT_DELETE;
	}
	
	if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		if (ptn->prbItems)
		{
			SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prbRemove)  );
			
			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptn->prbItems, &b, &pszKey, (void**) &ptnSub)  );
			while (b)
			{
				if (ptnSub->temp_flags & sg_PTNODE_TEMP_FLAG_IMPLICIT_ADD)
				{
					SG_ERR_CHECK(  SG_rbtree__add(pCtx, prbRemove, pszKey)  );
				}
				
				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, (void**) &ptnSub)  );
			}
			SG_rbtree__iterator__free(pCtx, pIterator);
			pIterator = NULL;

			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, prbRemove, &b, &pszKey, NULL)  );
			while (b)
			{
				SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, ptn->prbItems, pszKey, (void**) &ptnSub)  );
				SG_PTNODE_NULLFREE(pCtx, ptnSub);
				
				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, NULL)  );
			}
			SG_rbtree__iterator__free(pCtx, pIterator);
			pIterator = NULL;

			SG_RBTREE_NULLFREE(pCtx, prbRemove);

			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptn->prbItems, &b, &pszKey, (void**) &ptnSub)  );
			while (b)
			{
				SG_ERR_CHECK(  sg_ptnode__undo_implicit(pCtx, ptnSub)  );
				
				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, (void**) &ptnSub)  );
			}
			SG_rbtree__iterator__free(pCtx, pIterator);
			pIterator = NULL;
		}
	}

	return;

fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}

static void sg_ptnode__get_name(
	SG_context* pCtx, 
	const struct sg_ptnode* ptn,
	const char** ppszName
	)
{
	if (ptn->pszCurrentName)
	{
		*ppszName = ptn->pszCurrentName;
	}
	else if (ptn->pBaselineEntry)
	{
		SG_ERR_CHECK_RETURN(  SG_treenode_entry__get_entry_name(pCtx, ptn->pBaselineEntry, ppszName)  );
	}
	else
	{
		*ppszName = NULL;
	}
}

/**
 * Like sg_ptnode__get_name(), but we get the original version of the name,
 * ignoring any renames.
 */
static void sg_ptnode__get_old_name(
	SG_context* pCtx, 
	const struct sg_ptnode* ptn,
	const char** ppszName
	)
{
	if (ptn->pBaselineEntry)
	{
		SG_ERR_CHECK_RETURN(  SG_treenode_entry__get_entry_name(pCtx, ptn->pBaselineEntry, ppszName)  );
	}
	else
	{
		*ppszName = NULL;
	}
}

static void sg_ptnode__get_hid(
	SG_context* pCtx, 
	const struct sg_ptnode* ptn,
	const char** ppszhid
	)
{
	
	if (ptn->pszCurrentHid)
	{
		*ppszhid = ptn->pszCurrentHid;
	}
	else if (ptn->pBaselineEntry)
	{
		SG_ERR_CHECK_RETURN(  SG_treenode_entry__get_hid_blob(pCtx, ptn->pBaselineEntry, ppszhid)  );
	}
	else
	{
		*ppszhid = NULL;
	}
}

static void sg_ptnode__get_old_hid(
	SG_context* pCtx, 
	const struct sg_ptnode* ptn,
	const char** ppszhid
	)
{
	
	if (ptn->pBaselineEntry)
	{
		SG_ERR_CHECK_RETURN(  SG_treenode_entry__get_hid_blob(pCtx, ptn->pBaselineEntry, ppszhid)  );
	}
	else
	{
		*ppszhid = NULL;
	}
}

static void sg_ptnode__get_xattrs_hid(
	SG_context* pCtx, 
	const struct sg_ptnode* ptn,
	const char** ppszhid
	)
{
	SG_NULLARGCHECK_RETURN(ptn);
	SG_NULLARGCHECK_RETURN(ppszhid);

    *ppszhid = ptn->pszHidXattrs;
}

static void sg_ptnode__get_old_xattrs_hid(
	SG_context* pCtx, 
	const struct sg_ptnode* ptn,
	const char** ppszhid
	)
{
	if (ptn->pBaselineEntry)
	{
		SG_ERR_CHECK_RETURN(  SG_treenode_entry__get_hid_xattrs(pCtx, ptn->pBaselineEntry, ppszhid)  );
	}
	else
	{
		*ppszhid = NULL;
	}
}

static void sg_ptnode__find_entry_by_name_not_recursive__active(SG_context* pCtx, const struct sg_ptnode* ptnDir, const char* pszName, struct sg_ptnode** ppResult)
{
	SG_bool b = SG_FALSE;
	const char* pszKey = NULL;
	struct sg_ptnode* ptnSub = NULL;
	SG_rbtree_iterator* pIterator = NULL;
	const char* pszEntryName = NULL;
	struct sg_ptnode* ptnResult = NULL;

	SG_NULLARGCHECK_RETURN(ptnDir);
	SG_NULLARGCHECK_RETURN(ptnDir->prbItems);

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptnDir->prbItems, &b, &pszKey, (void**) &ptnSub)  );
	while (b)
	{
		if (
			(ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
			|| (ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
			)
		{
		}
		else
		{
			SG_ERR_CHECK(  sg_ptnode__get_name(pCtx, ptnSub, &pszEntryName)  );
			
			if (0 == strcmp(pszName, pszEntryName))
			{
				ptnResult = ptnSub;
				break;
			}
		}
		
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, (void**) &ptnSub)  );
	}
	SG_rbtree__iterator__free(pCtx, pIterator);
	pIterator = NULL;

	*ppResult = ptnResult;

	return;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}

static void sg_ptnode__find_entry_by_name_not_recursive__inactive(SG_context* pCtx, const struct sg_ptnode* ptnDir, const char* pszName, struct sg_ptnode** ppResult)
{
	SG_bool b = SG_FALSE;
	const char* pszKey = NULL;
	struct sg_ptnode* ptnSub = NULL;
	SG_rbtree_iterator* pIterator = NULL;
	const char* pszEntryName = NULL;
	struct sg_ptnode* ptnResult = NULL;
	
	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptnDir->prbItems, &b, &pszKey, (void**) &ptnSub)  );
	while (b)
	{
		if (
			(ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
			|| (ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
			)
		{
			SG_ERR_CHECK(  sg_ptnode__get_name(pCtx, ptnSub, &pszEntryName)  );
			
			if (0 == strcmp(pszName, pszEntryName))
			{
				ptnResult = ptnSub;
				break;
			}
		}
		else
		{
		}
		
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, (void**) &ptnSub)  );
	}
	SG_rbtree__iterator__free(pCtx, pIterator);
	pIterator = NULL;

	*ppResult = ptnResult;

	return;

fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}

void sg_ptnode__add_entry(SG_context* pCtx, struct sg_ptnode* ptnParent, struct sg_ptnode* ptnEntry)
{
#if TRACE_PENDINGTREE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"sg_ptnode__add_entry: [parent %s][child %s][child saved %d][child temp %d]\n",
							   ((ptnParent->pszgid) ? ptnParent->pszgid : "(null)"),
							   ((ptnEntry->pszgid) ? ptnEntry->pszgid : "(null)"),
							   ptnEntry->saved_flags,
							   ptnEntry->temp_flags)  );
#endif

	SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, ptnParent->prbItems, ptnEntry->pszgid, ptnEntry)  );

	ptnEntry->pCurrentParent = ptnParent;

fail:
	return;
}

void sg_ptnode__load_dir_entries(SG_context* pCtx, struct sg_ptnode* ptn)
{
	SG_uint32 count = 0;
	SG_uint32 i;
	struct sg_ptnode* pNewNode = NULL;
	const char* pszidGid = NULL;
	SG_treenode* pTreenode = NULL;
	const char* pszhid = NULL;
	
	SG_NULLARGCHECK_RETURN(ptn);
	SG_ARGCHECK_RETURN(ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY, ptn->type);

	if (ptn->prbItems)
		return;

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &ptn->prbItems)  );

	SG_ERR_CHECK(  sg_ptnode__get_hid(pCtx, ptn, &pszhid)  );
	
	if (pszhid)
	{
		SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, ptn->pPendingTree->pRepo, pszhid, &pTreenode)  );
		
		SG_ERR_CHECK(  SG_treenode__count(pCtx, pTreenode, &count)  );
		for (i=0; i<count; i++)
		{
			const SG_treenode_entry* pEntry = NULL;
			
			SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx, pTreenode, i, &pszidGid, &pEntry)  );
			SG_ERR_CHECK(  sg_ptnode__alloc__entry(pCtx, &pNewNode, ptn->pPendingTree, pEntry, pszidGid)  );
			SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx, ptn, pNewNode)  );
			pNewNode = NULL;
		}
		
		SG_TREENODE_NULLFREE(pCtx, pTreenode);
	}

	return;

fail:
	SG_TREENODE_NULLFREE(pCtx, pTreenode);
	SG_PTNODE_NULLFREE(pCtx, pNewNode);
}

void sg_ptnode__find_directory(
	SG_context* pCtx, 
	struct sg_ptnode* pNode,
	SG_bool bSynthesizeMissingParents,
	const char** ppszPathParts,
	SG_uint32 count_parts,
	SG_uint32 cur_part,
	struct sg_ptnode** ppResult
	)
{
	struct sg_ptnode* ptn = NULL;
	struct sg_ptnode* pNewNode = NULL;
	
	SG_NULLARGCHECK_RETURN(pNode);
	SG_NULLARGCHECK_RETURN(ppszPathParts);
	SG_NULLARGCHECK_RETURN(ppResult);
	SG_ARGCHECK_RETURN((count_parts > 0), count_parts);
	SG_ARGCHECK_RETURN((SG_TREENODEENTRY_TYPE_DIRECTORY == pNode->type), nNode->type);

	SG_ERR_CHECK(  sg_ptnode__load_dir_entries(pCtx, pNode)  );

	SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__active(pCtx, pNode, ppszPathParts[cur_part], &ptn)  );
	
	if (!ptn)
	{
		if (!bSynthesizeMissingParents)
			SG_ERR_THROW(  SG_ERR_PENDINGTREE_PARENT_DIR_MISSING_OR_INACTIVE  );

#if TRACE_PENDINGTREE
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "sg_ptnode__find_directory: synthesizing directory node for [%s]\n",
								   ppszPathParts[cur_part])  );
#endif

		SG_ERR_CHECK(  sg_ptnode__alloc__new(pCtx, &pNewNode, pNode->pPendingTree, ppszPathParts[cur_part], SG_TREENODEENTRY_TYPE_DIRECTORY)  );
		SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx, pNode, pNewNode)  );
		ptn = pNewNode;
		pNewNode = NULL;
	}
	
	if ((cur_part +1) < count_parts)
	{
		SG_ERR_CHECK_RETURN(  sg_ptnode__find_directory(pCtx, ptn, bSynthesizeMissingParents, ppszPathParts, count_parts, cur_part+1, ppResult)  );
		return;
	}
	else
	{
		SG_ARGCHECK_RETURN((SG_TREENODEENTRY_TYPE_DIRECTORY == ptn->type), ptn->type);

		SG_ERR_CHECK(  sg_ptnode__load_dir_entries(pCtx, ptn)  );
		
		*ppResult = ptn;
		return;
	}
	
fail:
	SG_PTNODE_NULLFREE(pCtx, pNewNode);
}

//////////////////////////////////////////////////////////////////

static void _sg_pendingtree__load_cached_or_baseline(SG_context* pCtx, SG_pendingtree* pPendingTree);

/**
 * core __alloc routine for pendingtree.
 * this ***DOES NOT*** call __load().
 */
static void _sg_pendingtree__alloc(
	SG_context* pCtx, 
	const SG_pathname* pPathWorkingDirectoryTop,   /* TODO as implemented below, this doesn't REALLY have to be the top. */
    SG_bool bIgnoreWarnings,
	SG_pendingtree** ppNew
	)
{
	SG_pendingtree* ptree = NULL;
	SG_pathname* pPathActualTop = NULL;
	char* pszidGidAnchorDirectory = NULL;
	
	SG_ERR_CHECK(  SG_alloc1(pCtx, ptree)  );

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &ptree->pvaWarnings)  );
    ptree->portMask = SG_PORT_FLAGS__ALL;  /* TODO get this from the repo config */
    ptree->bIgnoreWarnings = bIgnoreWarnings;
	SG_ERR_CHECK(  sg_pendingtree__configure_portability_converter(pCtx, ptree)  );

	SG_ERR_CHECK(  SG_STRPOOL__ALLOC(pCtx, &ptree->pStrPool, 32768)  ); /* TODO constant */
	
	SG_ERR_CHECK(  SG_workingdir__find_mapping(pCtx, pPathWorkingDirectoryTop, &pPathActualTop, &ptree->pstrRepoDescriptorName, &pszidGidAnchorDirectory)  );

	ptree->pPathWorkingDirectoryTop = pPathActualTop;
	pPathActualTop = NULL;

	SG_NULLFREE(pCtx, pszidGidAnchorDirectory);    /* TODO do something with pszidGidAnchorDirectory */

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, SG_string__sz(ptree->pstrRepoDescriptorName), &ptree->pRepo)  );

	*ppNew = ptree;

	return;

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, ptree);
	SG_NULLFREE(pCtx, pszidGidAnchorDirectory);
	SG_PATHNAME_NULLFREE(pCtx, pPathActualTop);
}

void SG_pendingtree__alloc_from_cwd(
	SG_context* pCtx,
    SG_bool bIgnoreWarnings,
	SG_pendingtree** ppNew
	)
{
	SG_pathname * pPathCwd = NULL;
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );
	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, ppNew)  );
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
}

void SG_pendingtree__alloc(
	SG_context* pCtx, 
	const SG_pathname* pPathWorkingDirectoryTop,   /* TODO as implemented below, this doesn't REALLY have to be the top. */
    SG_bool bIgnoreWarnings,
	SG_pendingtree** ppNew
	)
{
	SG_pendingtree * pNew = NULL;

	SG_ERR_CHECK(  _sg_pendingtree__alloc(pCtx, pPathWorkingDirectoryTop,
										  bIgnoreWarnings,
										  &pNew)  );
	SG_ERR_CHECK(  _sg_pendingtree__load_cached_or_baseline(pCtx, pNew)  );

	*ppNew = pNew;
	return;

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pNew);
}

//////////////////////////////////////////////////////////////////

void sg_ptnode__to_vhash(
	SG_context* pCtx, 
	const struct sg_ptnode* ptn,
	SG_vhash* pParent,
	SG_vhash** ppResult
	)
{
	SG_vhash* pvh = NULL;
	SG_vhash* pvhSub = NULL;
	SG_vhash* pvhEntries = NULL;
	SG_treenode_entry* pEntry = NULL;
	SG_rbtree_iterator* pIterator = NULL;
	
	SG_NULLARGCHECK_RETURN(ptn);

	/* TODO use #defines for these key strings */

	/* TODO add a version number */
	
	SG_ERR_CHECK(  SG_VHASH__ALLOC__SHARED(pCtx, &pvh, 10, pParent)  );

	if (ptn->pPendingTree->bDebugExport)
	{
		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx,pvh,"temp_flags",(SG_int64)ptn->temp_flags)  );
	}
	
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, "flags", (SG_int64) ptn->saved_flags)  );

	if (ptn->pBaselineEntry)
	{
		SG_ERR_CHECK(  SG_TREENODE_ENTRY__ALLOC__COPY(pCtx, &pEntry, ptn->pBaselineEntry)  );

		/* TODO this cast of a treenode entry to a vhash is in very
		 * poor taste.  The fact that a treenode entry is really a
		 * vhash is something that this code should not know. */
		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh, "entry", (SG_vhash**)&pEntry)  );
	}
	else
	{
		SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pvh, "entry")  );
	}
	
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, "type", ptn->type)  );

	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "name", ptn->pszCurrentName)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "hid", ptn->pszCurrentHid)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "xattrs", ptn->pszHidXattrs)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, "attrbits", ptn->iCurrentAttributeBits)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "gid", ptn->pszgid)  );

	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "moved_from", ptn->pszgidMovedFrom)  );

	if (ptn->prbItems)
	{
		SG_uint32 count;
		SG_bool b = SG_FALSE;
		const char* pszKey = NULL;
		struct sg_ptnode* ptnSub = NULL;

		SG_ERR_CHECK(  SG_rbtree__count(pCtx, ptn->prbItems, &count)  );
		
		SG_ERR_CHECK(  SG_VHASH__ALLOC__SHARED(pCtx, &pvhEntries, count, pParent)  );
		
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptn->prbItems, &b, &pszKey, (void**) &ptnSub)  );
		while (b)
		{
			SG_ASSERT(  (ptnSub)  );
			SG_ERR_CHECK(  sg_ptnode__to_vhash(pCtx, ptnSub, pParent, &pvhSub)  );
			
			SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhEntries, pszKey, &pvhSub)  );

			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, (void**) &ptnSub)  );
		}
		SG_rbtree__iterator__free(pCtx, pIterator);
		pIterator = NULL;

		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh, "entries", &pvhEntries)  );
	}
	else
	{
		SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pvh, "entries")  );
	}

	*ppResult = pvh;

	return;

fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
    SG_TREENODE_ENTRY_NULLFREE(pCtx, pEntry);
    SG_VHASH_NULLFREE(pCtx, pvhSub);
    SG_VHASH_NULLFREE(pCtx, pvhEntries);
}

static void sg_pendingtree__to_vhash(
	SG_context* pCtx, 
	const SG_pendingtree* pPendingTree,
	SG_vhash** ppResult
	)
{
	SG_vhash* pvh = NULL;
	
	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(ppResult);

	SG_ERR_CHECK(  sg_ptnode__to_vhash(pCtx, pPendingTree->pSuperRoot, NULL, &pvh)  );

	*ppResult = pvh;

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void sg_pendingtree__get_save_path(
	SG_context* pCtx, 
	SG_pendingtree* pPendingTree,
	SG_pathname** ppPath)
{
	SG_pathname* pDrawerPath = NULL;
	SG_pathname* pPendingTreePath = NULL;

	SG_ERR_CHECK(  SG_workingdir__get_drawer_path(pCtx, pPendingTree->pPathWorkingDirectoryTop, &pDrawerPath)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPendingTreePath, pDrawerPath, "wd.json")  );

	SG_PATHNAME_NULLFREE(pCtx, pDrawerPath);
	
	*ppPath = pPendingTreePath;

	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pDrawerPath);
}
	
void SG_pendingtree__get_working_directory_top(
	SG_context* pCtx,
	const SG_pendingtree* pPendingTree,
	SG_pathname** ppPath)
{
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, ppPath, SG_pathname__sz(pPendingTree->pPathWorkingDirectoryTop))   );
fail:
	return;
}

void SG_pendingtree__get_working_directory_top__ref(
	SG_context * pCtx,
	const SG_pendingtree * pPendingTree,
	const SG_pathname ** ppPath)
{
	SG_NULLARGCHECK_RETURN(  pPendingTree  );
	SG_NULLARGCHECK_RETURN(  ppPath  );

	*ppPath = pPendingTree->pPathWorkingDirectoryTop;
}

void SG_pendingtree__save(
	SG_context* pCtx, 
	SG_pendingtree* pPendingTree
	)
{
	SG_vhash * pvh_pending = NULL;

#if TRACE_PENDINGTREE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "SG_pendingtree__save: [pending non-empty %d][parents dirty %d][issues dirty %d][timestamps dirty %d]\n",
							   (pPendingTree->pSuperRoot != NULL),
							   pPendingTree->b_pvh_wd__parents__dirty,
							   pPendingTree->b_pvh_wd__issues__dirty,
							   pPendingTree->b_pvh_wd__timestamps__dirty)  );
#endif
#if STATS_TIMESTAMP_CACHE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "SG_pendingtree__save: timestamp cache stats: %d / %d\n",
							   pPendingTree->trace_timestamp_cache.nr_valid,
							   pPendingTree->trace_timestamp_cache.nr_queries)  );
#endif

	// update the "pending" sub-vhash based upon our in-memory pending tree.

	if (pPendingTree->pSuperRoot)
	{
		SG_ERR_CHECK(  sg_pendingtree__to_vhash(pCtx, pPendingTree, &pvh_pending)  );
		SG_ERR_CHECK(  SG_vhash__update__vhash(pCtx, pPendingTree->pvh_wd, "pending", &pvh_pending)  );
	}
	else
	{
		SG_vhash__remove(pCtx, pPendingTree->pvh_wd, "pending");
		SG_ERR_CHECK_CURRENT_DISREGARD(  SG_ERR_VHASH_KEYNOTFOUND  );
	}

	// NOTE: I'm assuming that any changes to the "parents" were made directly to
	// NOTE: sub-vhash in pPendingTree->pvh_wd, so that we don't have to do anything here
	// NOTE: other than write them out with everything else.

	// NOTE: I'm assuming that any changes to the "issues" were made directly to
	// NOTE: sub-vhash in pPendingTree->pvh_wd, so that we don't have to do anything here
	// NOTE: other than write them out with everything else.

	// NOTE: I'm assuming that any changes to the "timestamps" were made directly to
	// NOTE: sub-vhash in pPendingTree->pvh_wd, so that we don't have to do anything here
	// NOTE: other than write them out with everything else.

	// When we write out the VFILE, we release the lock on it and forget that we had it open.
	SG_ERR_CHECK(  SG_vfile__end(pCtx, &pPendingTree->pvf_wd, pPendingTree->pvh_wd)  );
	SG_VHASH_NULLFREE(pCtx, pPendingTree->pvh_wd);
#if defined(DEBUG)
	pPendingTree->b_pvh_wd__parents__dirty = SG_FALSE;
	pPendingTree->b_pvh_wd__issues__dirty = SG_FALSE;
	pPendingTree->b_pvh_wd__timestamps__dirty = SG_FALSE;
#endif

	return;
	
fail:
	SG_VHASH_NULLFREE(pCtx, pvh_pending);
}

void sg_ptnode__from_vhash(SG_context* pCtx, struct sg_ptnode** ppResult, SG_pendingtree* pPendingTree, const SG_vhash* pvh)
{
	struct sg_ptnode* ptn = NULL;
	const char* pszGid = NULL;
	const char* pszHid = NULL;
	const char* pszHidXattrs = NULL;
	SG_int64 type = 0;
    SG_int64 iAttributeBits = 0;
	SG_int64 flags = 0;
	SG_vhash* pvhOriginalEntry = NULL;
	SG_vhash* pvhEntries = NULL;
	SG_uint32 count;
	SG_uint32 i;
	const char* pszGidMovedFrom = NULL;
	const char* pszCurrentName = NULL;

	SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, "entry", &pvhOriginalEntry)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "name", &pszCurrentName)  );
	SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, "type", &type)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "hid", &pszHid)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "xattrs", &pszHidXattrs)  );
	SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, "attrbits", &iAttributeBits)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "gid", &pszGid)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "moved_from", &pszGidMovedFrom)  );
	SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, "flags", &flags)  );
	
	/* TODO this cast of a treenode entry to a vhash is in very
	 * poor taste.  The fact that a treenode entry is really a
	 * vhash is something that this code should not know. */
	SG_ERR_CHECK(  sg_ptnode__alloc__load(pCtx, &ptn, pPendingTree, (SG_treenode_entry*) pvhOriginalEntry, pszCurrentName, type, pszGid, pszHid, pszHidXattrs, iAttributeBits, pszGidMovedFrom, (SG_uint16) flags)  );
	
	SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, "entries", &pvhEntries)  );
	if (pvhEntries)
	{
		SG_ERR_CHECK(  SG_vhash__count(pCtx, pvhEntries, &count)  );

		SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS(pCtx, &ptn->prbItems, count, NULL)  );
		
		for (i=0; i<count; i++)
		{
			const char* pszName = NULL;
			const SG_variant* pV = NULL;
			SG_vhash* pvhSub = NULL;
			struct sg_ptnode* ptnSub = NULL;
			
			SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvhEntries, i, &pszName, &pV)  );
			SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pV, &pvhSub)  );
			SG_ERR_CHECK(  sg_ptnode__from_vhash(pCtx, &ptnSub, pPendingTree, pvhSub)  );
			SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx, ptn, ptnSub)  );
		}
	}

	*ppResult = ptn;
	
	return;

fail:
	SG_PTNODE_NULLFREE(pCtx, ptn);
}

void SG_pendingtree__from_vhash(
	SG_context* pCtx, 
	SG_pendingtree* pPendingTree,
	const SG_vhash* pvh
	)
{
    SG_ASSERT(!pPendingTree->pSuperRoot);

	SG_ERR_CHECK_RETURN(  sg_ptnode__from_vhash(pCtx, &pPendingTree->pSuperRoot, pPendingTree, pvh)  );
}

//////////////////////////////////////////////////////////////////

static void _sg_pendingtree__intern_vfile_contents(SG_context * pCtx,
												   SG_pendingtree * pPendingTree)
{
	SG_changeset* pcs = NULL;

	if (pPendingTree->pvh_wd)
	{
		// the VFile has a valid VHash in it.

		const SG_varray * pva_wd_parents = NULL;		// we don't own this
		SG_uint32 nrParents = 0;
		SG_bool bFound = SG_FALSE;

		SG_ERR_CHECK(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pva_wd_parents)  );
		SG_ERR_CHECK(  SG_varray__count(pCtx, pva_wd_parents, &nrParents)  );
		SG_ASSERT(  (nrParents >= 1)  );

		// we only have a "pending" sub-vhash when there is dirt in the WD.
		// if the WD is dirty, we re-load the dirt into the in-memory pendingtree.
		// if the WD is clean, we load minimal info from the changeset's superroot.
		SG_ERR_CHECK(  SG_vhash__has(pCtx, pPendingTree->pvh_wd, "pending", &bFound)  );
		if (bFound)
		{
			SG_vhash * pvh_pending;		// we don't own this
			SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pPendingTree->pvh_wd, "pending", &pvh_pending)  );
#if 0
			SG_ERR_CHECK(  SG_vhash_debug__dump_to_console(pCtx, pvh_pending)  );
#endif
			SG_ERR_CHECK(  SG_pendingtree__from_vhash(pCtx, pPendingTree, pvh_pending)  );
		}
		else
		{
			const char * psz_hid_parent_0;	// we don't own this
			const char* pszidUserSuperRoot = NULL;

			// I want to assert that (nrParents == 1), but I suppose that it is possible
			// for a merge to produce an empty pendingtree if both sides made the exact same changes.
			// (we'd still need the merge to resolve the parents and collapse the dag, but we might
			// not have any actual changes to the WD.
			// either way, we use parent[0] to populate the pendingtree.
			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_wd_parents, 0, &psz_hid_parent_0)  );

			SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pPendingTree->pRepo, psz_hid_parent_0, &pcs)  );
			SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs, &pszidUserSuperRoot)  );
			SG_ERR_CHECK(  sg_ptnode__alloc__top(pCtx, pPendingTree, pszidUserSuperRoot)  );
		}
	}
	else
	{
		// When a WD is first created (probably by SG_workingdir__create_and_get(),
		// we don't have a "wd.json" yet.  VFile fakes it and creates and locks the
		// file, but just returns NULL for the VHash.  We continue the illusion and
		// create one here.
		//
		// NOTE: We don't know what the baseline/parents should be, so we don't set
		// NOTE: anything for it.

		SG_ASSERT(  (pPendingTree->pvf_wd)  );
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pPendingTree->pvh_wd)  );
	}

fail:
	SG_CHANGESET_NULLFREE(pCtx, pcs);
}

static void _sg_pendingtree__load_cached_or_baseline(
	SG_context* pCtx, 
	SG_pendingtree* pPendingTree
	)
{
	SG_pathname* pPath = NULL;
	
	SG_ERR_CHECK(  sg_pendingtree__get_save_path(pCtx, pPendingTree, &pPath)  );
	
	SG_ASSERT(  (pPendingTree->pvf_wd == NULL)  );	// we should only be called once
	SG_ASSERT(  (pPendingTree->pvh_wd == NULL)  );

	// load the contents of the VFILE into a new VHASH.
	// then convert the VHASH (if present) into PTNODES and etc.

	SG_ERR_CHECK(  SG_vfile__begin(pCtx, pPath, SG_FILE_RDWR | SG_FILE_OPEN_OR_CREATE, &pPendingTree->pvh_wd, &pPendingTree->pvf_wd)  );
	SG_ERR_CHECK(  _sg_pendingtree__intern_vfile_contents(pCtx, pPendingTree)  );
	
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__reload_pendingtree_from_vfile(SG_context * pCtx,
												   SG_pendingtree * pPendingTree)
{
	SG_ASSERT(  (pPendingTree->pvf_wd)  );

#if defined(DEBUG)
	pPendingTree->b_pvh_wd__parents__dirty = SG_FALSE;
	pPendingTree->b_pvh_wd__issues__dirty = SG_FALSE;
	pPendingTree->b_pvh_wd__timestamps__dirty = SG_FALSE;
#endif
	SG_VHASH_NULLFREE(pCtx, pPendingTree->pvh_wd);
	SG_PTNODE_NULLFREE(pCtx, pPendingTree->pSuperRoot);

	// reload the contents of the VFILE into a new VHASH.
	// We ASSUME that we are still holding the file lock.
	// then convert the VHASH (if present) into PTNODES and etc.

	SG_ERR_CHECK(  SG_vfile__reread(pCtx, pPendingTree->pvf_wd, &pPendingTree->pvh_wd)  );
	SG_ERR_CHECK(  _sg_pendingtree__intern_vfile_contents(pCtx, pPendingTree)  );

fail:
	return;
}

//////////////////////////////////////////////////////////////////

static void sg_pendingtree__fsobjtype_to_treenodeentrytype(
    SG_context* pCtx,
    SG_fsobj_type t_fsobj,
    SG_treenode_entry_type* p_t_tne
    )
{
    SG_treenode_entry_type t = SG_TREENODEENTRY_TYPE__INVALID;

	if (t_fsobj == SG_FSOBJ_TYPE__DIRECTORY)
	{
        t = SG_TREENODEENTRY_TYPE_DIRECTORY;
	}
	else if (t_fsobj == SG_FSOBJ_TYPE__REGULAR)
	{
        t = SG_TREENODEENTRY_TYPE_REGULAR_FILE;
	}
	else if (t_fsobj == SG_FSOBJ_TYPE__SYMLINK)
	{
        t = SG_TREENODEENTRY_TYPE_SYMLINK;
	}
	else
	{
		SG_ERR_THROW_RETURN(  SG_ERR_INVALID_PENDINGTREE_OBJECT_TYPE  );
	}

    *p_t_tne = t;
}

static void sg_ptnode__get_repo_path(
	SG_context* pCtx, 
	struct sg_ptnode* ptn,
	SG_string** ppstr
	);

static void sg_ptnode__add_symlink_portability_warning(
    SG_context* pCtx, 
    struct sg_ptnode* ptn
    )
{
    SG_vhash* pvhItem = NULL;
    SG_string* pstrRepoPath = NULL;
    SG_pathname* pPath = NULL;

    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );

    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "type", "symlink")  );

	SG_ERR_CHECK(  sg_ptnode__get_repo_path(pCtx, ptn, &pstrRepoPath)  );
	SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx,
																		 ptn->pPendingTree->pPathWorkingDirectoryTop,
																		 SG_string__sz(pstrRepoPath),
																		 &pPath)  );
    SG_STRING_NULLFREE(pCtx, pstrRepoPath);
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path", SG_pathname__sz(pPath)) );
    SG_PATHNAME_NULLFREE(pCtx, pPath);

	if (!ptn->pPendingTree->pvaWarnings)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &ptn->pPendingTree->pvaWarnings)  );

    SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, ptn->pPendingTree->pvaWarnings, &pvhItem)  );

    return;

fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_STRING_NULLFREE(pCtx, pstrRepoPath);
    SG_VHASH_NULLFREE(pCtx, pvhItem);
}

static void sg_ptnode__create_item__with_gid(
	SG_context* pCtx, 
	struct sg_ptnode* ptn, 
	const char* pszName,
	SG_bool bAddRemove,
	SG_bool bMarkImplicitAdd, 
	const SG_fsobj_stat* pst,
	const char * pszGid,
	struct sg_ptnode** ppNewNode
	)
{
	struct sg_ptnode* pNewNode_Allocated = NULL;
	struct sg_ptnode* pNewNode;
    SG_treenode_entry_type t = 0;

    SG_ERR_CHECK(  sg_pendingtree__fsobjtype_to_treenodeentrytype(pCtx, pst->type, &t)  );

    SG_ERR_CHECK(  sg_ptnode__alloc__new__with_gid(pCtx, &pNewNode_Allocated, ptn->pPendingTree, pszName, t, pszGid)  );
	if (bMarkImplicitAdd)
		pNewNode_Allocated->temp_flags |= sg_PTNODE_TEMP_FLAG_IMPLICIT_ADD;
	if (bAddRemove)
		pNewNode_Allocated->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;
	
	SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx, ptn, pNewNode_Allocated)  );
	pNewNode = pNewNode_Allocated;
	pNewNode_Allocated = NULL;			// we don't own it any more.

	// TODO Jeff says (3/8/10): We need to revisit when we call sg_ptnode__check().
	// TODO                     We do it here when *each* item is added to the directory
	// TODO                     ptnode.  This feels very expensive.  We should do the
	// TODO                     __add_entry() now and defer the __check() until *after*
	// TODO                     our caller has done everything that they are going to do.
    SG_ERR_CHECK(  sg_ptnode__check(pCtx, ptn)  );

    if (SG_TREENODEENTRY_TYPE_SYMLINK == t)
    {
        /* TODO we need some sort of a mask mechanism to turn this warning off */
        SG_ERR_CHECK(  sg_ptnode__add_symlink_portability_warning(pCtx, pNewNode)  );
    }

	*ppNewNode = pNewNode;

	return;

fail:
    SG_PTNODE_NULLFREE(pCtx, pNewNode_Allocated);
}

static void sg_ptnode__create_item(
	SG_context* pCtx, 
	struct sg_ptnode* ptn, 
	const char* pszName,
	SG_bool bAddRemove,
	SG_bool bMarkImplicitAdd, 
	const SG_fsobj_stat* pst,
	struct sg_ptnode** ppNewNode
	)
{
	char bufGid[SG_GID_BUFFER_LENGTH];

	SG_ERR_CHECK_RETURN(  SG_gid__generate(pCtx, bufGid, sizeof(bufGid))  );
	SG_ERR_CHECK_RETURN(  sg_ptnode__create_item__with_gid(pCtx, ptn, pszName,
														   bAddRemove, bMarkImplicitAdd,
														   pst,
														   bufGid,
														   ppNewNode)  );
}

static void sg_pendingtree__local_path_to_ptnode(
	SG_context* pCtx, 
	SG_pendingtree* pPendingTree,
	const SG_pathname* pPathLocalDirectory,
	SG_bool bSynthesizeMissingParents,
	struct sg_ptnode** ppNode
	)
{
	SG_string* pStrRepoPath = NULL;
	const char** apszParts = NULL;
	SG_uint32 count_parts = 0;
	SG_strpool* pPool = NULL;
	struct sg_ptnode* ptn = NULL;

	SG_ERR_CHECK(  SG_workingdir__wdpath_to_repopath(pCtx, pPendingTree->pPathWorkingDirectoryTop, pPathLocalDirectory, SG_TRUE, &pStrRepoPath)  );
	SG_ERR_CHECK(  SG_repopath__ensure_final_slash(pCtx, pStrRepoPath)  );
	SG_ERR_CHECK(  SG_STRPOOL__ALLOC(pCtx, &pPool, 10)  );
	
	SG_ERR_CHECK(  SG_repopath__split(pCtx, SG_string__sz(pStrRepoPath), pPool, &apszParts, &count_parts)  );

	SG_ERR_CHECK(  sg_ptnode__find_directory(pCtx, pPendingTree->pSuperRoot, bSynthesizeMissingParents, apszParts, count_parts, 0, &ptn)  );
	SG_NULLFREE(pCtx, apszParts);
	SG_STRING_NULLFREE(pCtx, pStrRepoPath);

	SG_STRPOOL_NULLFREE(pCtx, pPool);

	*ppNode = ptn;

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pStrRepoPath);
	SG_NULLFREE(pCtx, apszParts);
	SG_STRPOOL_NULLFREE(pCtx, pPool);
}

static void sg_ptnode__scan_attrbits(SG_context* pCtx, struct sg_ptnode* ptn, SG_pathname* pPathLocal)
{
    SG_int64 iBaselineAttributeBits = 0;
    SG_int64 iCurrentAttributeBits = 0;

    if (ptn->pBaselineEntry)
    {
        SG_ERR_CHECK(  SG_treenode_entry__get_attribute_bits(pCtx, ptn->pBaselineEntry, &iBaselineAttributeBits)  );
    }

    SG_ERR_CHECK(  SG_attributes__bits__read(pCtx, pPathLocal, iBaselineAttributeBits, &iCurrentAttributeBits)  );

    ptn->iCurrentAttributeBits = iCurrentAttributeBits;

fail:
    return;
}

#ifdef SG_BUILD_FLAG_FEATURE_XATTR

static void sg_ptnode__get_xattrs(SG_context* pCtx, struct sg_ptnode* ptn, const SG_pathname* pPathLocal, char** ppszCurrentAttributesHid, SG_vhash** ppvh, SG_rbtree** pprb, SG_string** ppstr)
{
    SG_vhash* pvhAttrs = NULL;
    SG_vhash* pvhBaselineAttrs = NULL;
    const char* pszBaselineAttributesHid = NULL;
    SG_rbtree* prb = NULL;
    SG_string* pstr = NULL;
    char* pszCurrentAttributesHid = NULL;
    SG_uint32 count = 0;

    /* Read the xattrs from the local file */
    SG_ERR_CHECK(  SG_attributes__xattrs__read(pCtx, ptn->pPendingTree->pRepo, pPathLocal, &pvhAttrs, &prb)  );

    /* Compute the HID */
    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
    SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhAttrs,pstr)  );
    SG_ERR_CHECK(  SG_repo__alloc_compute_hash__from_string(pCtx, ptn->pPendingTree->pRepo, pstr, &pszCurrentAttributesHid)  );

    if (ptn->pBaselineEntry)
    {
		SG_ERR_CHECK(  SG_treenode_entry__get_hid_xattrs(pCtx, ptn->pBaselineEntry, &pszBaselineAttributesHid)  );
        if (pszBaselineAttributesHid)
        {
            if (0 != strcmp(pszCurrentAttributesHid, pszBaselineAttributesHid))
            {
                SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, ptn->pPendingTree->pRepo,pszBaselineAttributesHid,&pvhBaselineAttrs)  );
                SG_ERR_CHECK(  SG_attributes__xattrs__update_with_baseline(pCtx, pvhBaselineAttrs, pvhAttrs)  );

                SG_ERR_CHECK(  SG_string__clear(pCtx, pstr)  );
                SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhAttrs,pstr)  );
                SG_NULLFREE(pCtx, pszCurrentAttributesHid);
                SG_ERR_CHECK(  SG_repo__alloc_compute_hash__from_string(pCtx, ptn->pPendingTree->pRepo, pstr, &pszCurrentAttributesHid)  );
            }
        }
    }

    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvhAttrs, &count)  );
    if (count == 0)
    {
        SG_NULLFREE(pCtx, pszCurrentAttributesHid);
        SG_VHASH_NULLFREE(pCtx, pvhAttrs);
        SG_RBTREE_NULLFREE(pCtx, prb);
        SG_STRING_NULLFREE(pCtx, pstr);
    }

    SG_VHASH_NULLFREE(pCtx, pvhBaselineAttrs);

    *ppstr = pstr;
    *pprb = prb;
    *ppvh = pvhAttrs;
    *ppszCurrentAttributesHid = pszCurrentAttributesHid;

    return;

fail:
    SG_VHASH_NULLFREE(pCtx, pvhBaselineAttrs);
    SG_VHASH_NULLFREE(pCtx, pvhAttrs);
}

static void sg_ptnode__scan_xattrs(SG_context* pCtx, struct sg_ptnode* ptn, SG_pathname* pPathLocal)
{
    SG_vhash* pvhAttrs = NULL;
    SG_rbtree* prb = NULL;
    char* pszCurrentAttributesHid = NULL;
    SG_string* pstr = NULL;

    SG_ERR_CHECK(  sg_ptnode__get_xattrs(pCtx, ptn, pPathLocal, &pszCurrentAttributesHid, &pvhAttrs, &prb, &pstr)  );
    if (pszCurrentAttributesHid)
    {
        SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, ptn->pPendingTree->pStrPool, pszCurrentAttributesHid, &ptn->pszHidXattrs)  );
    }
    else
    {
        ptn->pszHidXattrs = NULL;
    }

    SG_NULLFREE(pCtx, pszCurrentAttributesHid);
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_RBTREE_NULLFREE(pCtx, prb);
    SG_VHASH_NULLFREE(pCtx, pvhAttrs);

    return;

fail:
    SG_NULLFREE(pCtx, pszCurrentAttributesHid);
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_RBTREE_NULLFREE(pCtx, prb);
    SG_VHASH_NULLFREE(pCtx, pvhAttrs);
}

static void sg_ptnode__commit_xattrs(
	SG_context* pCtx, 
	struct sg_ptnode* ptn,
	const SG_pathname* pPathLocal,
	SG_committing* ptx
	)
{
    SG_vhash* pvhAttrs = NULL;
    SG_rbtree* prb = NULL;
    char* pszCurrentAttributesHid = NULL;
	SG_bool b = SG_FALSE;
	const char* pszHid = NULL;
	SG_rbtree_iterator* pIterator = NULL;
    SG_byte* pPacked = NULL;
    char* pszActualHid = NULL;
    SG_string* pstr = NULL;

    SG_ERR_CHECK(  sg_ptnode__get_xattrs(pCtx, ptn, pPathLocal, &pszCurrentAttributesHid, &pvhAttrs, &prb, &pstr)  );
    SG_VHASH_NULLFREE(pCtx, pvhAttrs);
    if (!pszCurrentAttributesHid)
    {
        ptn->pszHidXattrs = NULL;
    }
    else
    {
        SG_ERR_CHECK(  SG_committing__add_bytes__string(pCtx, ptx, NULL, pstr, SG_BLOB_REFTYPE__TREEATTRIBS, &pszActualHid)  );
        SG_STRING_NULLFREE(pCtx, pstr);

        /* TODO should we check to make sure actual hid is what it should be ? */
        SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, ptn->pPendingTree->pStrPool, pszActualHid, &ptn->pszHidXattrs)  );
        SG_NULLFREE(pCtx, pszActualHid);

        SG_NULLFREE(pCtx, pszCurrentAttributesHid);

        /* And some of the attributes may require side blobs, so those need to get
         * saved as well.  */
        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, prb, &b, &pszHid, (void**) &pPacked)  );
        while (b)
        {
            SG_uint32 len;
            const SG_byte* pData = NULL;

            SG_ERR_CHECK(  SG_memoryblob__unpack(pCtx, pPacked, &pData, &len)  );
            SG_ERR_CHECK(  SG_committing__add_bytes__buflen(pCtx, ptx, NULL, pData, len, SG_BLOB_REFTYPE__TREEATTRIBS, &pszActualHid)  );
            /* TODO should we check to make sure actual hid is what it should be ? */
            SG_NULLFREE(pCtx, pszActualHid);

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszHid, (void**) &pPacked)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
    }

    SG_RBTREE_NULLFREE(pCtx, prb);

    return;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb);
    SG_NULLFREE(pCtx, pszCurrentAttributesHid);
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_NULLFREE(pCtx, pszActualHid);
    SG_VHASH_NULLFREE(pCtx, pvhAttrs);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}
#endif

//////////////////////////////////////////////////////////////////

#include "sg_pendingtree__private_scan_dir.h"

//////////////////////////////////////////////////////////////////

void SG_pendingtree__scan(SG_context* pCtx, 
						  SG_pendingtree* pPendingTree, 
						  SG_bool bRecursive, 
						  const char* const* paszIncludes, SG_uint32 count_includes,
						  const char* const* paszExcludes, SG_uint32 count_excludes)
{
	SG_bool bDirty = SG_FALSE;
	
	SG_ERR_CHECK(  sg_pendingtree__scan_dir__load_ignores(pCtx,
														  pPendingTree,
														  pPendingTree->pPathWorkingDirectoryTop,
														  SG_FALSE,		// bAddRemove
														  SG_FALSE,		// bMarkImplicit
														  bRecursive,
														  NULL,			// ptnode
														  paszIncludes, count_includes,
														  paszExcludes, count_excludes)  );

	SG_ERR_CHECK(  sg_ptnode__unload_dir_entries_if_not_dirty(pCtx, pPendingTree->pSuperRoot, &bDirty)  );

	if (!(pPendingTree->pSuperRoot->prbItems))
	{
		SG_PTNODE_NULLFREE(pCtx, pPendingTree->pSuperRoot);
	}
	
	SG_ERR_CHECK(  SG_pendingtree__save(pCtx, pPendingTree)  );

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * Do ADDREMOVE with no ITEMS were given in the ITEMLIST.
 * This implicitly assumes the current working directory within the WD.
 */
static void _sg_pendingtree__addremove__when_no_items(SG_context * pCtx,
													  SG_pendingtree * pPendingTree,
													  SG_bool bRecursive,
													  const char* const* paszIncludes, SG_uint32 count_includes,
													  const char* const* paszExcludes, SG_uint32 count_excludes,
													  const char* const* paszIgnores,  SG_uint32 count_ignores)
{
	SG_pathname * pPathCwd = NULL;
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

#if TRACE_ADDREMOVE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   "AddRemove: Scanning from where... CWD [%s]\n",
							   SG_pathname__sz(pPathCwd))  );
#endif

	SG_ERR_CHECK(  sg_pendingtree__scan_dir(pCtx,
											pPendingTree,
											pPathCwd,
											SG_TRUE,		// bAddRemove
											SG_FALSE,		// bMarkImplicit
											bRecursive,
											NULL,			// ptnode
											paszIncludes, count_includes,
											paszExcludes, count_excludes,
											paszIgnores,  count_ignores)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
}

/**
 * Do ADDREMOVE on one of the ITEMS from the ITEMLIST.
 * This item forms the top of the sub-tree that we look at.
 * Some of the arguments only apply to things that we respect
 * at this top-level.
 */
static void _sg_pendingtree__addremove__one_item(SG_context * pCtx,
												 SG_pendingtree * pPendingTree,
												 const char * pszItem_k,
												 SG_bool bRecursive,
												 const char* const* paszIncludes, SG_uint32 count_includes,
												 const char* const* paszExcludes, SG_uint32 count_excludes,
												 const char* const* paszIgnores,  SG_uint32 count_ignores)
{
	SG_string* pstrName = NULL;
	SG_pathname* pPathLocal = NULL;
	SG_pathname* pPathDir = NULL;
	struct sg_ptnode* ptnParent;
	struct sg_ptnode* pNewNode = NULL;
	struct sg_ptnode* ptnExistingNode = NULL;
	SG_string * pStringRepoPath = NULL;
	SG_fsobj_stat st;
	SG_file_spec_eval eval;
	SG_bool bExists;
	SG_string * pOutputString = NULL;
	SG_int32 compareResult = 0;

	SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pPathLocal, pszItem_k) );
	SG_ERR_CHECK(  SG_workingdir__wdpath_to_repopath(pCtx, pPendingTree->pPathWorkingDirectoryTop, pPathLocal, SG_TRUE, &pStringRepoPath)  );
	SG_ERR_CHECK(  SG_file_spec__should_include(pCtx,
												paszIncludes, count_includes,
												paszExcludes, count_excludes,
												paszIgnores,  count_ignores,
												SG_string__sz(pStringRepoPath),
												&eval)  );
#if TRACE_ADDREMOVE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "AddRemove: Considering item [eval 0x%x][%s]\n", eval, SG_string__sz(pStringRepoPath))  );
#endif

	switch (eval)
	{
	default:	// quiets compiler
		SG_ASSERT(  (0)  );
		goto fail;

	case SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED:
#if TRACE_ADDREMOVE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "AddRemove: item excluded [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
		goto done;

	case SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED:
#if TRACE_ADDREMOVE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "AddRemove: item ignored [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
		goto done;

	case SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED:
	case SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED:
		SG_ERR_CHECK(  SG_fsobj__exists_stat__pathname(pCtx, pPathLocal, &bExists, &st)  );
		if (!bExists)
			goto do_maybe_lost;

		// hard-add a ptnode for this entry regardless of whether it is a
		// file or folder.  if it is a folder, we'll deal with the contents
		// in a minute.

#if TRACE_ADDREMOVE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "AddRemove: including item [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
		SG_ERR_CHECK(  SG_repopath__compare(pCtx, SG_string__sz(pStringRepoPath), "@/", &compareResult)  );
		if ( compareResult == 0)
		{
			SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathLocal, SG_TRUE, &pNewNode)  );
			//Don't output any warning messages, just recurse.
		}
		else
		{
			SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathDir, pPathLocal)  );
			SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathDir)  );
			SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathDir, SG_TRUE, &ptnParent)  );
			SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathLocal, &pstrName)  );
			SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__active(pCtx, ptnParent, SG_string__sz(pstrName), &ptnExistingNode)  );
			if (ptnExistingNode != NULL)
			{
				SG_string__alloc__format(pCtx, &pOutputString, "Warning: The item %s is already controlled.\n", SG_pathname__sz(pPathLocal)  );
				SG_context__msg__emit(pCtx, SG_string__sz(pOutputString) );
				pNewNode = ptnExistingNode;
			}
			else
			{
				SG_ERR_CHECK(  sg_ptnode__create_item(pCtx, ptnParent, SG_string__sz(pstrName),
												  SG_TRUE,		// bAddRemove (actually just check-my-name)
												  SG_FALSE,		// bMarkImplicit
												  &st,
												  &pNewNode)  );
				SG_ERR_CHECK( _pt_log__add_item(pCtx, pPendingTree, pNewNode->pszgid, SG_pathname__sz(pPathLocal), "explicitlyAdded")  );
			}
		}
		if (st.type == SG_FSOBJ_TYPE__DIRECTORY)
			goto do_add_dir;
		else
			goto done;
			
	case SG_FILE_SPEC_EVAL__MAYBE:
		SG_ERR_CHECK(  SG_fsobj__exists_stat__pathname(pCtx, pPathLocal, &bExists, &st)  );
		if (!bExists)
			goto do_maybe_lost;
		
		if (st.type == SG_FSOBJ_TYPE__DIRECTORY)
		{
#if TRACE_ADDREMOVE
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "AddRemove: maybe including directory [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
			goto do_add_dir;
		}
		else
		{
#if TRACE_ADDREMOVE
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "AddRemove: omitting non-directory-maybe [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
			goto done;
		}
	}

	//////////////////////////////////////////////////////////////////
	// NOTREACHED
	//////////////////////////////////////////////////////////////////

do_add_dir:
	// do an ADDREMOVE starting with this sub-directory within the tree.
	// we may or may not have already done a hard-add on the sub-directory.
	// here we scan the contents of the sub-dir and add/remove anything
	// we find within.  if a child does get added, it will back-fill us.

	if (bRecursive)
	{
#if TRACE_ADDREMOVE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "AddRemove: Scanning directory [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
		SG_ERR_CHECK(  sg_pendingtree__scan_dir(pCtx,
												pPendingTree,
												pPathLocal,
												SG_TRUE,
												SG_FALSE,
												bRecursive,
												NULL,
												paszIncludes, count_includes,
												paszExcludes, count_excludes,
												paszIgnores,  count_ignores)  );
	}
	else
	{
#if TRACE_ADDREMOVE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "AddRemove: Skipping contents of directory because not recursive [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
	}
	goto done;

	//////////////////////////////////////////////////////////////////
	// NOTREACHED
	//////////////////////////////////////////////////////////////////

do_maybe_lost:	
	// They named something that isn't in the WD.  It is either bogus or
	// possibly refers to something that is currently LOST.
	// 
	// TODO 2010/06/05 This should probably look in the ptnode and see if
	// TODO            this is a LOST entry that should be REMOVED.

	SG_ERR_THROW2(  SG_ERR_NOTIMPLEMENTED,
					(pCtx, "ADDREMOVE given pathname to non-existing item [eval %d] [%s]",
					 eval, SG_string__sz(pStringRepoPath))  );

	//////////////////////////////////////////////////////////////////
	// NOTREACHED
	//////////////////////////////////////////////////////////////////

done:
	;
fail:
	SG_STRING_NULLFREE(pCtx, pOutputString);
	SG_STRING_NULLFREE(pCtx, pstrName);
	SG_STRING_NULLFREE(pCtx, pStringRepoPath);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathLocal);
}

void SG_pendingtree__addremove(SG_context* pCtx, 
							   SG_pendingtree* pPendingTree,
							   SG_uint32 count_items, const char** paszItems,
							   SG_bool bRecursive,
							   const char* const* paszIncludes, SG_uint32 count_includes,
							   const char* const* paszExcludes, SG_uint32 count_excludes,
							   SG_bool bTest)
{	
	SG_stringarray * psaIgnores = NULL;
	const char * const * paszIgnores;
	SG_uint32 count_ignores;
	SG_bool bDirty = SG_FALSE;

	SG_ERR_CHECK(  SG_file_spec__patterns_from_ignores_localsetting(pCtx, &psaIgnores)  );
	SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, psaIgnores, &paszIgnores, &count_ignores)  );

	if (count_items == 0)
	{
		SG_ERR_CHECK(  _sg_pendingtree__addremove__when_no_items(pCtx,
																 pPendingTree,
																 bRecursive,
																 paszIncludes, count_includes,
																 paszExcludes, count_excludes,
																 paszIgnores,  count_ignores)  );
	}
	else
	{
		SG_uint32 i;

		for (i=0; i<count_items; i++)
		{
			SG_ERR_CHECK(  _sg_pendingtree__addremove__one_item(pCtx,
																pPendingTree,
																paszItems[i],
																bRecursive,
																paszIncludes, count_includes,
																paszExcludes, count_excludes,
																paszIgnores,  count_ignores)  );
		}
	}

	/* TODO - this may cause problem if there are paszItems, not sure
	 *
	 * TODO 2010/06/05 The above TODO is very old.  Think about whether it is still relevant. 
	 *
	 */
	SG_ERR_CHECK(  sg_ptnode__unload_dir_entries_if_not_dirty(pCtx, pPendingTree->pSuperRoot, &bDirty)  );

	if (!(pPendingTree->pSuperRoot->prbItems))
	{
		SG_PTNODE_NULLFREE(pCtx, pPendingTree->pSuperRoot);
	}

	if (bTest == SG_FALSE)
		SG_ERR_CHECK(  SG_pendingtree__save(pCtx, pPendingTree)  );

fail:
	SG_STRINGARRAY_NULLFREE(pCtx, psaIgnores);
}

//////////////////////////////////////////////////////////////////

static void sg_ptnode__get_repo_path(
	SG_context* pCtx, 
	struct sg_ptnode* ptn,
	SG_string** ppstr
	)
{
	const char* pszName = NULL;
	
	if (ptn->pCurrentParent)
	{
		SG_ERR_CHECK(  sg_ptnode__get_repo_path(pCtx, ptn->pCurrentParent, ppstr)  );
	}
	else
	{
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, ppstr)  );
	}

	SG_ERR_CHECK(  sg_ptnode__get_name(pCtx, ptn, &pszName)  );
	if (pszName)
	{
#if 0		
		if (0 != SG_string__length_in_bytes(*ppstr))
		{
			SG_ERR_CHECK(  SG_repopath__ensure_final_slash(pCtx, *ppstr)  );
		}
#endif
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, *ppstr, pszName)  );
		if (SG_TREENODEENTRY_TYPE_DIRECTORY == ptn->type)
		{
			SG_ERR_CHECK(  SG_repopath__ensure_final_slash(pCtx, *ppstr)  );
		}
	}

fail:
	return;
}

/**
 * Like sg_ptnode__get_repo_path(), but we get the original version of
 * the path (before any pending changes (moves/renames) in a parent.
 */
/* This isn't used, which makes gcc 4.3.3 unhappy.
static SG_error sg_ptnode__get_old_repo_path(
	struct sg_ptnode* ptn,
	SG_string** ppstr
	)
{
	SG_error err;
	const char* pszName = NULL;
	
	if (ptn->pCurrentParent)
	{
		SG_ERR_CHECK(  sg_ptnode__get_old_repo_path(ptn->pCurrentParent, ppstr)  );
	}
	else
	{
		SG_ERR_CHECK(  SG_STRING__ALLOC(ppstr)  );
	}

	SG_ERR_CHECK(  sg_ptnode__get_old_name(ptn, &pszName)  );
	if (pszName)
	{
#if 0		
		if (0 != SG_string__length_in_bytes(*ppstr))
		{
			SG_ERR_CHECK(  SG_repopath__ensure_final_slash(*ppstr)  );
		}
#endif
		SG_ERR_CHECK(  SG_string__append__sz(*ppstr, pszName)  );
		if (SG_TREENODEENTRY_TYPE_DIRECTORY == ptn->type)
		{
			SG_ERR_CHECK(  SG_repopath__ensure_final_slash(*ppstr)  );
		}
	}
	
	return SG_ERR_OK;

fail:
	return err;
}
*/

static void sg_pendingtree__find_by_gid(
	SG_context* pCtx, 
	SG_pendingtree* pPendingTree,
	const char* pszGid,
	struct sg_ptnode** ppResult
	);

void SG_pendingtree__get_repo_path(
	SG_context* pCtx, 
	SG_pendingtree* pPendingTree,
	const char* pszgid,
	SG_string** ppResult
	)
{
	struct sg_ptnode* ptn = NULL;
	SG_string* pstr = NULL;

	SG_ERR_CHECK(  SG_gid__argcheck(pCtx, pszgid)  );

	SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, pPendingTree, pszgid, &ptn)  );
	if (!ptn)
	{
		SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
	}

	SG_ERR_CHECK(  sg_ptnode__get_repo_path(pCtx, ptn, &pstr)  );

	*ppResult = pstr;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__add_dont_save_pendingtree(SG_context* pCtx, 
											   SG_pendingtree* pPendingTree,
											   const SG_pathname* pPathRelativeTo,
											   SG_uint32 count_items, const char** paszItems,
											   SG_bool bRecursive,
											   const char* const* paszIncludes, SG_uint32 count_includes,
											   const char* const* paszExcludes, SG_uint32 count_excludes,
											   const char* const* paszIgnores,  SG_uint32 count_ignores)
{	
	SG_uint32 i;
	struct sg_ptnode* ptnParent;
	SG_string* pstrName = NULL;
	SG_pathname* pPathDir = NULL;
	SG_pathname* pPathLocal = NULL;
	struct sg_ptnode* pNewNode = NULL;
	struct sg_ptnode* ptnExistingNode = NULL;
	SG_string * pStringRepoPath = NULL;
	SG_file_spec_eval eval;
	SG_fsobj_stat st;
	SG_string * pOutputString = NULL;
	SG_int32 compareResult = 0;

	SG_ARGCHECK_RETURN(  (count_items > 0), count_items  );

	for (i=0; i<count_items; i++)
	{
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathLocal, pPathRelativeTo, paszItems[i])  );
		SG_ERR_CHECK(  SG_workingdir__wdpath_to_repopath(pCtx, pPendingTree->pPathWorkingDirectoryTop, pPathLocal, SG_TRUE, &pStringRepoPath)  );
		SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPathLocal, &st)  );
		SG_ERR_CHECK(  SG_file_spec__should_include(pCtx,
													paszIncludes, count_includes,
													paszExcludes, count_excludes,
													paszIgnores,  count_ignores,
													SG_string__sz(pStringRepoPath),
													&eval)  );
#if TRACE_ADD
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Add: considering [eval 0x%x][%s]\n", eval, SG_string__sz(pStringRepoPath))  );
#endif

		switch (eval)
		{
		default:	// quiets compiler
			SG_ASSERT(  (0)  );
			goto done_with_this_one;

		case SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED:
#if TRACE_ADD
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Add: item excluded [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
			goto done_with_this_one;

		case SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED:
#if TRACE_ADD
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Add: item ignored [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
			goto done_with_this_one;

		case SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED:
		case SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED:
#if TRACE_ADD
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Add: Adding [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
			SG_ERR_CHECK(  SG_repopath__compare(pCtx, SG_string__sz(pStringRepoPath), "@/", &compareResult)  );
			if ( compareResult == 0)
			{
				SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathLocal, SG_TRUE, &pNewNode)  );
				//Don't output any warning messages, just recurse.
			}
			else
			{
				SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathDir, pPathLocal)  );
				SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathDir)  );
				SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathDir, SG_TRUE, &ptnParent)  );
				SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathLocal, &pstrName)  );
				SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__active(pCtx, ptnParent, SG_string__sz(pstrName), &ptnExistingNode)  );
				if (ptnExistingNode != NULL)
				{
					SG_string__alloc__format(pCtx, &pOutputString, "Warning: The item %s is already controlled.\n", SG_pathname__sz(pPathLocal)  );
					SG_context__msg__emit(pCtx, SG_string__sz(pOutputString) );
					pNewNode = ptnExistingNode;
				}
				else
				{
					SG_ERR_CHECK(  sg_ptnode__create_item(pCtx, ptnParent, SG_string__sz(pstrName),
													  SG_TRUE,		// bAddRemove (actually just check-my-name)
													  SG_FALSE,		// bMarkImplicit
													  &st,
													  &pNewNode)  );
					SG_ERR_CHECK( _pt_log__add_item(pCtx, pPendingTree, pNewNode->pszgid, SG_pathname__sz(pPathLocal), "explicitlyAdded")  );
				}
			}
			if (st.type == SG_FSOBJ_TYPE__DIRECTORY)
				goto do_add_dir;
			else
				goto done_with_this_one;

		case SG_FILE_SPEC_EVAL__MAYBE:
			if (st.type == SG_FSOBJ_TYPE__DIRECTORY)
			{
#if TRACE_ADD
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Add: maybe including directory [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
				goto do_add_dir;
			}
			else
			{
#if TRACE_ADD
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Add: omitting not-directory-maybe [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
				goto done_with_this_one;
			}
		}
		
		//////////////////////////////////////////////////////////////////
		// NOT REACHED
		//////////////////////////////////////////////////////////////////

do_add_dir:
		if (bRecursive)
		{
#if TRACE_ADD
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Add: Scanning directory [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
			SG_ERR_CHECK(  sg_pendingtree__scan_dir(pCtx,
													pPendingTree,
													pPathLocal,
													SG_TRUE,	// bAddRemove
													SG_FALSE,	// bMarkImplicit
													bRecursive,
													pNewNode,
													paszIncludes, count_includes,
													paszExcludes, count_excludes,
													paszIgnores,  count_ignores)  );
		}
		else
		{
#if TRACE_ADD
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Add: Skipping contents of directory because not recursive [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
		}

done_with_this_one:
		SG_STRING_NULLFREE(pCtx, pstrName);
		SG_STRING_NULLFREE(pCtx, pOutputString);
		SG_PATHNAME_NULLFREE(pCtx, pPathDir);
		SG_PATHNAME_NULLFREE(pCtx, pPathLocal);
		SG_STRING_NULLFREE(pCtx, pStringRepoPath);
	}
	
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pstrName);
	SG_STRING_NULLFREE(pCtx, pOutputString);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathLocal);
	SG_STRING_NULLFREE(pCtx, pStringRepoPath);
}

void SG_pendingtree__add(SG_context* pCtx, 
						 SG_pendingtree* pPendingTree,
						 const SG_pathname* pPathRelativeTo,
						 SG_uint32 count_items, const char** paszItems,
						 SG_bool bRecursive,
						 const char* const* paszIncludes, SG_uint32 count_includes,
						 const char* const* paszExcludes, SG_uint32 count_excludes,
						 SG_bool bTest)
{
	SG_stringarray * psaIgnores = NULL;
	const char * const * paszIgnores;
	SG_uint32 count_ignores;

	SG_ERR_CHECK(  SG_file_spec__patterns_from_ignores_localsetting(pCtx, &psaIgnores)  );
	SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, psaIgnores, &paszIgnores, &count_ignores)  );

	SG_ERR_CHECK(  SG_pendingtree__add_dont_save_pendingtree(pCtx, pPendingTree, pPathRelativeTo,
															 count_items, paszItems,
															 bRecursive,
															 paszIncludes, count_includes,
															 paszExcludes, count_excludes,
															 paszIgnores,  count_ignores)  );
	if (bTest == SG_FALSE)
		SG_ERR_CHECK(  SG_pendingtree__save(pCtx, pPendingTree)  );

fail:
	SG_STRINGARRAY_NULLFREE(pCtx, psaIgnores);
}

void SG_pendingtree__add_dont_save_pendingtree__with_gid(SG_context * pCtx,
														 SG_pendingtree * pPendingTree,
														 const char * pszGid,
														 const SG_pathname * pPath)
{
	// TODO think about adding the includes/excludes stuff to this.
	// TODO currently the only caller of this didn't need them at the time.

	SG_string * pStringEntryname = NULL;
	SG_pathname * pPathDir = NULL;
	struct sg_ptnode * ptn;
	struct sg_ptnode * pNewNode;
	SG_fsobj_stat st;

	// split full pathname into <dir> and <entryname>

	SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPath, &pStringEntryname)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathDir, pPath)  );
	SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathDir)  );

	// find ptnode for <dir>.
	//
	// TODO I don't think we want to synthesize any missing parents.  But then
	// TODO the context that I created this for it didn't make sense.  Consider
	// TODO making this an argument.

	SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathDir, SG_FALSE, &ptn)  );

	// make sure the entry is present on the disk.
	//
	// TODO if it is a directory, do we want to support a bRecursive option to
	// TODO scan the directory?  It didn't make sense for my application.

	SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath, &st)  );

	// WE DO NOT look at the IGNORES list for this.

	SG_ERR_CHECK(  sg_ptnode__create_item__with_gid(pCtx, ptn, SG_string__sz(pStringEntryname),
													SG_TRUE,		// bAddRemove
													SG_FALSE,		// bMarkImplicit
													&st,
													pszGid,
													&pNewNode)  );

fail:
	SG_STRING_NULLFREE(pCtx, pStringEntryname);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir);
}


void SG_pendingtree__remove(SG_context* pCtx,
							SG_pendingtree* pPendingTree,
							const SG_pathname* pPathRelativeTo,
							SG_uint32 count_items, const char** paszItems,
							const char* const* paszIncludes, SG_uint32 count_includes,
							const char* const* paszExcludes, SG_uint32 count_excludes,
							SG_bool bTest)
{
	SG_stringarray * psaIgnores = NULL;
	const char * const * paszIgnores;
	SG_uint32 count_ignores;

	SG_ERR_CHECK(  SG_file_spec__patterns_from_ignores_localsetting(pCtx, &psaIgnores)  );
	SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, psaIgnores, &paszIgnores, &count_ignores)  );

	SG_ERR_CHECK(  SG_pendingtree__remove_dont_save_pendingtree(pCtx,
																pPendingTree,
																pPathRelativeTo,
																count_items, paszItems,
																paszIncludes, count_includes,
																paszExcludes, count_excludes,
																paszIgnores,  count_ignores,
																bTest)  );
	if (bTest == SG_FALSE)
		SG_ERR_CHECK(  SG_pendingtree__save(pCtx, pPendingTree)  );

fail:
	SG_STRINGARRAY_NULLFREE(pCtx, psaIgnores);
}

void SG_pendingtree__move_dont_save_pendingtree(
	SG_context* pCtx,
	SG_pendingtree* pPendingTree,
	const SG_pathname* pPathMoveMe,
	const SG_pathname* pPathMoveTo,
	SG_bool bForce
	)
{
	struct sg_ptnode* ptnParentDirOfItemBeingMoved = NULL;
	struct sg_ptnode* ptnMoveTo = NULL;
	struct sg_ptnode* ptnCheckForParentMove = NULL;
	struct sg_ptnode* ptnCloneOfItemBeingMoved = NULL;
	SG_string* pstrName = NULL;
	struct sg_ptnode* ptnItemBeingMoved = NULL;
	SG_pathname* pPathParentDirOfItemBeingMoved = NULL;
	SG_pathname* pPathMoved = NULL;
	SG_bool bExists = SG_FALSE;
    SG_fsobj_type t;

	SG_NULLARGCHECK_RETURN( pPendingTree );
	SG_NULLARGCHECK_RETURN( pPathMoveMe );
	SG_NULLARGCHECK_RETURN( pPathMoveTo );

    /* First make sure the paths exist */
	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathMoveTo, &bExists, &t, NULL)  );

    if (!bExists || t != SG_FSOBJ_TYPE__DIRECTORY)
    {
        SG_ERR_THROW(  SG_ERR_NOT_A_DIRECTORY  );
    }

	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathMoveMe, &bExists, NULL, NULL)  );
    if (!bExists)
    {
        SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
    }

    /* We want the parent directory of this item.  So copy the path and remove the last component */
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathParentDirOfItemBeingMoved, pPathMoveMe)  );
    SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathParentDirOfItemBeingMoved)  );

    /* Find the ptnodes */
    SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathParentDirOfItemBeingMoved, SG_TRUE, &ptnParentDirOfItemBeingMoved)  );
    if (!ptnParentDirOfItemBeingMoved)
    {
        SG_ERR_THROW(  SG_ERR_ITEM_NOT_UNDER_VERSION_CONTROL  );
    }
    SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathMoveTo, SG_TRUE, &ptnMoveTo)  );
    if (!ptnMoveTo)
    {
        SG_ERR_THROW(  SG_ERR_ITEM_NOT_UNDER_VERSION_CONTROL  );
    }

    if (ptnParentDirOfItemBeingMoved == ptnMoveTo)
    {
        SG_ERR_THROW(  SG_ERR_CANNOT_MOVE_INTO_CURRENT_PARENT  );
    }

    /* Get the base name from the original path */
    SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathMoveMe, &pstrName)  );

    /* leave the item in both places, one as a phantom */

    SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__active(pCtx, ptnParentDirOfItemBeingMoved, SG_string__sz(pstrName), &ptnItemBeingMoved)  );
    if (!ptnItemBeingMoved)
    {
        SG_ERR_THROW(  SG_ERR_ITEM_NOT_UNDER_VERSION_CONTROL  );
    }

    if (ptnItemBeingMoved == ptnMoveTo)
    {
        SG_ERR_THROW(  SG_ERR_CANNOT_MOVE_INTO_SUBFOLDER  );
    }
    ptnCheckForParentMove = ptnMoveTo;
    while (ptnCheckForParentMove != NULL)
    {
        if (ptnCheckForParentMove == ptnItemBeingMoved)
            SG_ERR_THROW(  SG_ERR_CANNOT_MOVE_INTO_SUBFOLDER  );
        else
        {
            if (ptnCheckForParentMove->pCurrentParent != NULL)
                ptnCheckForParentMove = ptnCheckForParentMove->pCurrentParent;
            else
                break;
        }
    }

    if (ptnItemBeingMoved->pszgidMovedFrom)
    {
        struct sg_ptnode* ptn2 = NULL;

        /* this item has already been moved.  So it already has
         * a phantom in its original location.  We don't need to
         * create a new one.  We just need to move this ptnode
         * into the new location.  Leave pszgidMovedFrom alone. */

        SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, ptnParentDirOfItemBeingMoved->prbItems, ptnItemBeingMoved->pszgid, (void**) &ptn2)  );
        SG_ASSERT(ptn2 == ptnItemBeingMoved);
        ptnItemBeingMoved->pCurrentParent = NULL;
        ptn2 = NULL;

        if (0 == strcmp(ptnMoveTo->pszgid, ptnItemBeingMoved->pszgidMovedFrom))
        {
            /* item is being moved back to where it came from.  we need
             * to get rid of the old phantom node. */

            SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, ptnMoveTo->prbItems, ptnItemBeingMoved->pszgid, (void**) &ptn2)  );
            SG_ASSERT(ptn2->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY);

            SG_PTNODE_NULLFREE(pCtx, ptn2);

            ptnItemBeingMoved->pszgidMovedFrom = NULL;
        }

        SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx, ptnMoveTo, ptnItemBeingMoved)  );
        ptnItemBeingMoved->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;

        SG_ERR_CHECK(  sg_ptnode__check(pCtx, ptnMoveTo)  );
    }
    else
    {
        if (bForce)
        {
            SG_bool bExists = SG_FALSE;
            SG_fsobj_type fsType;
            SG_string * pMovedObjName = NULL;
            SG_pathname * pObjectSittingInTheTargetDirectory = NULL;

            SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathMoveMe, &pMovedObjName)  );

            SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pObjectSittingInTheTargetDirectory, pPathMoveTo)  );
            SG_ERR_CHECK(  SG_pathname__append__from_string(pCtx, pObjectSittingInTheTargetDirectory, pMovedObjName)  );
            SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pObjectSittingInTheTargetDirectory, &bExists, &fsType, NULL)  );

            if (bExists)
            {
                const char * psz = SG_string__sz(pMovedObjName);
                SG_pendingtree__remove_dont_save_pendingtree(pCtx,
															 pPendingTree,
															 pPathMoveTo,
															 1, &psz,
															 NULL, 0,
															 NULL, 0,
															 NULL, 0,
															 SG_FALSE);
                if (SG_context__has_err(pCtx) && SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
                {
                    SG_context__err_reset(pCtx);
                    // something exists at the new location, but the pending tree doesn't know about it
                    if (fsType == SG_FSOBJ_TYPE__DIRECTORY)
                        SG_ERR_CHECK(  SG_fsobj__rmdir_recursive__pathname(pCtx, pObjectSittingInTheTargetDirectory)  );
                    else
                        SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pObjectSittingInTheTargetDirectory)  );
                }
                else
                {
                    SG_STRING_NULLFREE(pCtx, pMovedObjName);
                    SG_PATHNAME_NULLFREE(pCtx, pObjectSittingInTheTargetDirectory);
                    SG_ERR_CHECK_CURRENT;
                }
                SG_STRING_NULLFREE(pCtx, pMovedObjName);
                SG_PATHNAME_NULLFREE(pCtx, pObjectSittingInTheTargetDirectory);
            }
        }
        if (ptnItemBeingMoved->pBaselineEntry)
        {
            SG_ERR_CHECK(  sg_ptnode__clone_for_move(pCtx, ptnItemBeingMoved, &ptnCloneOfItemBeingMoved)  );

            SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx, ptnMoveTo, ptnCloneOfItemBeingMoved)  );
            ptnCloneOfItemBeingMoved->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;

            SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pPendingTree->pStrPool, ptnParentDirOfItemBeingMoved->pszgid, &ptnCloneOfItemBeingMoved->pszgidMovedFrom)  );

            ptnItemBeingMoved->saved_flags |= sg_PTNODE_SAVED_FLAG_MOVED_AWAY;
            ptnCloneOfItemBeingMoved->prbItems = ptnItemBeingMoved->prbItems;
            ptnItemBeingMoved->prbItems = NULL;
            ptnCloneOfItemBeingMoved = NULL;
            SG_ERR_CHECK(  sg_ptnode__check(pCtx, ptnMoveTo)  );
        }
        else
        {
            /* this item is newly added */
            struct sg_ptnode* ptn2 = NULL;

            SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, ptnParentDirOfItemBeingMoved->prbItems, ptnItemBeingMoved->pszgid, (void**) &ptn2)  );
            SG_ASSERT(ptn2 == ptnItemBeingMoved);
            ptnItemBeingMoved->pCurrentParent = NULL;
            ptn2 = NULL;

            SG_ERR_CHECK(  sg_ptnode__add_entry(pCtx, ptnMoveTo, ptnItemBeingMoved)  );
            ptnItemBeingMoved->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;

            SG_ERR_CHECK(  sg_ptnode__check(pCtx, ptnMoveTo)  );
        }
    }

    /* Do the local rename */
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathMoved, pPathMoveTo, SG_string__sz(pstrName))  );
    SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathMoved, &bExists, NULL, NULL)  );
    if (bExists)
    {
        SG_ERR_THROW(  SG_ERR_PENDINGTREE_OBJECT_ALREADY_EXISTS  );
    }
    SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx, pPathMoveMe, pPathMoved)  );

	SG_STRING_NULLFREE(pCtx, pstrName);
	SG_PATHNAME_NULLFREE(pCtx, pPathParentDirOfItemBeingMoved);
	SG_PATHNAME_NULLFREE(pCtx, pPathMoved);

	return;

fail:
    SG_PTNODE_NULLFREE(pCtx, ptnCloneOfItemBeingMoved);
	SG_PATHNAME_NULLFREE(pCtx, pPathParentDirOfItemBeingMoved);
	SG_PATHNAME_NULLFREE(pCtx, pPathMoved);
	SG_STRING_NULLFREE(pCtx, pstrName);
}

void SG_pendingtree__move(
	SG_context* pCtx,
	SG_pendingtree* pPendingTree,
	const SG_pathname** pArrayPathsToMove,
	SG_uint32 nCountMoves,
	const SG_pathname* pPathMoveTo,
	SG_bool bForce
	)
{
	SG_uint32 nPathIndex = 0;
	for (nPathIndex = 0; nPathIndex < nCountMoves; nPathIndex++)
	{
		SG_ERR_CHECK_RETURN(  SG_pendingtree__move_dont_save_pendingtree(pCtx, pPendingTree,
				pArrayPathsToMove[nPathIndex], pPathMoveTo, bForce)  );
	}
	SG_ERR_CHECK_RETURN(  SG_pendingtree__save(pCtx, pPendingTree)  );
}

void SG_pendingtree__rename_dont_save_pendingtree(
	SG_context* pCtx,
	SG_pendingtree* pPendingTree,
	const SG_pathname* pPathLocal,
	const char* pszNewName,
	SG_bool bIgnoreErrorNotFoundOnDisk
	)
{
	struct sg_ptnode* ptn = NULL;
	SG_string* pstrName = NULL;
	struct sg_ptnode* ptnRenameMe = NULL;
	SG_pathname* pPathDir = NULL;
	SG_pathname* pPathRenamed = NULL;
	SG_bool bExists;
	SG_bool bSkipLocalRename = SG_FALSE;

	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathLocal, &bExists, NULL, NULL)  );
	if (!bExists)
    {
		if (bIgnoreErrorNotFoundOnDisk)
			bSkipLocalRename = SG_TRUE;
		else
			SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
    }

	/* We want the parent directory of this item.  So copy the path and remove the last component */
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathDir, pPathLocal)  );
	SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathDir)  );

	/* Find the directory's ptnode */
	SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathDir, SG_TRUE, &ptn)  );

	/* Get the base name from the original path */
	SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathLocal, &pstrName)  );

	/* Rename the item */
	SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__active(pCtx, ptn, SG_string__sz(pstrName), &ptnRenameMe)  );
    if (!ptnRenameMe)
    {
        SG_ERR_THROW(  SG_ERR_ITEM_NOT_UNDER_VERSION_CONTROL  );
    }
	SG_STRING_NULLFREE(pCtx, pstrName);
	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pPendingTree->pStrPool, pszNewName, &ptnRenameMe->pszCurrentName)  );
    ptnRenameMe->temp_flags |= sg_PTNODE_TEMP_FLAG_CHECKMYNAME;

	// TODO the following call to __check() can return a _RBTREE_DUPLICATEKEY error when the entryname is an *exact*
	// TODO match for an existing entry in the directory.  Do we want to propagate that error or convert it
	// TODO into an _ALREADY_EXISTS ???
	//
	// TOOD check the other calls to __check() and see if they have similar issues.

    SG_ERR_CHECK(  sg_ptnode__check(pCtx, ptn)  );

	/* Do the local rename */
	if (!bSkipLocalRename)
	{
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathRenamed, pPathDir, pszNewName)  );
		SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathRenamed, &bExists, NULL, NULL)  );
		if (bExists)
		{
			SG_ERR_THROW(  SG_ERR_PENDINGTREE_OBJECT_ALREADY_EXISTS  );
		}
		SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx, pPathLocal, pPathRenamed)  );
	}

	SG_PATHNAME_NULLFREE(pCtx, pPathDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathRenamed);

	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathRenamed);
	SG_STRING_NULLFREE(pCtx, pstrName);
}

void SG_pendingtree__rename(
	SG_context* pCtx,
	SG_pendingtree* pPendingTree,
	const SG_pathname* pPathLocal,
	const char* pszNewName,
	SG_bool bIgnoreErrorNotFoundOnDisk
	)
{
	SG_ERR_CHECK_RETURN(  SG_pendingtree__rename_dont_save_pendingtree(pCtx, pPendingTree,
																	   pPathLocal, pszNewName,
																	   bIgnoreErrorNotFoundOnDisk)  );
	SG_ERR_CHECK_RETURN(  SG_pendingtree__save(pCtx, pPendingTree)  );
}

static void sg_ptnode__create_entry_for_commit(
	SG_context* pCtx,
	struct sg_ptnode* ptnSub
	)
{
	SG_treenode_entry* pEntry = NULL;

	if (!(ptnSub->temp_flags & sg_PTNODE_TEMP_FLAG_COMMITTING))
	{
		/* The original entry may not be present.  This can happen if
		 * the item is newly added but has been specifically excluded
		 * for this commit operation. */
		if (ptnSub->pBaselineEntry)
		{
			SG_ERR_CHECK(  SG_TREENODE_ENTRY__ALLOC__COPY(pCtx, &pEntry, ptnSub->pBaselineEntry)  );
		}
		else
		{
			/* this item is not supposed to be committed, and it
			 * has no original entry, so it is newly added but
			 * should be ignored now. */
		}
	}
	else
	{
		if (
			(ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
			|| (ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
			)
		{
			/* ignore this then */
		}
		else
		{
			const char* pszName = NULL;
			const char* pszHid = NULL;
			const char* pszAttributesHid = NULL;

			SG_ERR_CHECK(  sg_ptnode__get_name(pCtx, ptnSub, &pszName)  );
			SG_ERR_CHECK(  sg_ptnode__get_hid(pCtx, ptnSub, &pszHid)  );
			SG_ERR_CHECK(  sg_ptnode__get_xattrs_hid(pCtx, ptnSub, &pszAttributesHid)  );

			SG_ERR_CHECK(  SG_TREENODE_ENTRY__ALLOC(pCtx, &pEntry)  );
			SG_ERR_CHECK(  SG_treenode_entry__set_entry_name(pCtx, pEntry, pszName)  );
			SG_ERR_CHECK(  SG_treenode_entry__set_entry_type(pCtx, pEntry, ptnSub->type)  );
			SG_ERR_CHECK(  SG_treenode_entry__set_hid_blob(pCtx, pEntry, pszHid)  );
			SG_ERR_CHECK(  SG_treenode_entry__set_hid_xattrs(pCtx, pEntry, pszAttributesHid)  );
			SG_ERR_CHECK(  SG_treenode_entry__set_attribute_bits(pCtx, pEntry, ptnSub->iCurrentAttributeBits)  );
		}
	}

	ptnSub->pNewEntry = pEntry;

	return;

fail:
	SG_TREENODE_ENTRY_NULLFREE(pCtx, pEntry);
}

static void sg_ptnode__write_all_treepaths(
	SG_context* pCtx, 
	struct sg_ptnode* ptn,
	const SG_string* pRepoPath,
	SG_committing* ptx
    )
{
	SG_string* pMyRepoPath = NULL;
	SG_rbtree_iterator* pit = NULL;
    const char* psz_cur_name = NULL;
    const char* psz_old_name = NULL;

	SG_NULLARGCHECK_RETURN(ptn);
	SG_NULLARGCHECK_RETURN(ptx);
	SG_NULLARGCHECK_RETURN(pRepoPath);

	if (
		(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
		|| (ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
		)
	{
		/* ignore this then */

		return;
	}

    SG_ERR_CHECK(  sg_ptnode__get_name(pCtx, ptn, &psz_cur_name)  );
    SG_ERR_CHECK(  sg_ptnode__get_old_name(pCtx, ptn, &psz_old_name)  );

    if (
            !(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
            && !(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
       )
    {
        SG_ERR_CHECK(  SG_committing__add_treepath(pCtx, ptx, ptn->pszgid, SG_string__sz(pRepoPath))  );
    }

	if (SG_TREENODEENTRY_TYPE_DIRECTORY == ptn->type)
	{
        SG_ERR_CHECK(  sg_ptnode__load_dir_entries(pCtx, ptn)  );

		if (ptn->prbItems)
		{
            SG_bool b = SG_FALSE;
            const char* pszgid = NULL;
            const char* pszName = NULL;
            struct sg_ptnode* ptnSub = NULL;

			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, ptn->prbItems, &b, &pszgid, (void**) &ptnSub)  );

			while (b)
			{
				SG_ERR_CHECK(  sg_ptnode__get_name(pCtx, ptnSub, &pszName)  );

				if (pRepoPath)
				{
					SG_ERR_CHECK(  SG_repopath__combine__sz(pCtx, SG_string__sz(pRepoPath), pszName, (SG_TREENODEENTRY_TYPE_DIRECTORY == ptnSub->type), &pMyRepoPath)  );
				}
				else
				{
					SG_ASSERT('@' == pszName[0]);
					SG_ASSERT(0 == pszName[1]);

					SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pMyRepoPath, "@/")  );
				}

                SG_ERR_CHECK(  sg_ptnode__write_all_treepaths(pCtx, ptnSub, pMyRepoPath, ptx)  );

				SG_STRING_NULLFREE(pCtx, pMyRepoPath);

				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &pszgid, (void**) &ptnSub)  );
			}
			SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
		}

        // TODO should we unload the dir entries here?
	}

fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_STRING_NULLFREE(pCtx, pMyRepoPath);
}

static void sg_ptnode__commit(
	SG_context* pCtx,
	struct sg_ptnode* ptn,
	const SG_string* pRepoPath,
	SG_committing* ptx,
	SG_bool* pbParentDirty
	)
{
	SG_rbtree_iterator* pIterator = NULL;
	SG_string* pMyRepoPath = NULL;
	const char* pszgid = NULL;
	const char* pszName = NULL;
	SG_bool b;
	struct sg_ptnode* ptnSub = NULL;
	SG_treenode* pTreenode = NULL;
	SG_file* pFile = NULL;
	char* pszidHid = NULL;
	SG_pathname* pPathLocal = NULL;
	SG_treenode_entry* pEntry = NULL;
	SG_string* pstrLink = NULL;
    SG_bool b_path_is_new = SG_FALSE;
    SG_bool bFoundOnDisk = SG_FALSE;

	SG_NULLARGCHECK_RETURN(ptn);
	SG_NULLARGCHECK_RETURN(ptx);

	if (
		!(ptn->temp_flags & sg_PTNODE_TEMP_FLAG_COMMITTING)
		)
	{
		/* We do NOT clear *pbParentDirty here.  We simply don't set it. */
		return;
	}

	if (
		(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
		|| (ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
		)
	{
		/* ignore this then */
		*pbParentDirty = SG_TRUE;
		return;
	}

    if (pRepoPath)
    {
        const char* psz_cur_name = NULL;
        const char* psz_old_name = NULL;

        SG_ERR_CHECK(  sg_ptnode__get_name(pCtx, ptn, &psz_cur_name)  );
        SG_ERR_CHECK(  sg_ptnode__get_old_name(pCtx, ptn, &psz_old_name)  );

        if (
                !(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
                && !(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
                && (
                    ptn->pszgidMovedFrom
                    || (!ptn->pBaselineEntry)
                    || (0 != strcmp(psz_cur_name, psz_old_name))
                   )
           )
        {
            b_path_is_new = SG_TRUE;
            SG_ERR_CHECK(  SG_committing__add_treepath(pCtx, ptx, ptn->pszgid, SG_string__sz(pRepoPath))  );
        }
    }

    if (pRepoPath)
    {
        SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx,
																			 ptn->pPendingTree->pPathWorkingDirectoryTop,
																			 SG_string__sz(pRepoPath),
																			 &pPathLocal)  );
#ifdef SG_BUILD_FLAG_FEATURE_XATTR
        SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathLocal, &bFoundOnDisk, NULL, NULL)  );
        if (bFoundOnDisk)
        	SG_ERR_CHECK(  sg_ptnode__commit_xattrs(pCtx, ptn, pPathLocal, ptx)  );
#endif
    }

	if (SG_TREENODEENTRY_TYPE_DIRECTORY == ptn->type)
	{
		SG_bool bNeedToWriteMyTreenode = SG_FALSE;

		SG_ERR_CHECK(  SG_treenode__alloc(pCtx, &pTreenode)  );
		SG_ERR_CHECK(  SG_treenode__set_version(pCtx, pTreenode, TN_VERSION__CURRENT)  );

		if (ptn->prbItems)
		{
			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptn->prbItems, &b, &pszgid, (void**) &ptnSub)  );

			while (b)
			{
				SG_ERR_CHECK(  sg_ptnode__get_name(pCtx, ptnSub, &pszName)  );

				if (pRepoPath)
				{
					SG_ERR_CHECK(  SG_repopath__combine__sz(pCtx, SG_string__sz(pRepoPath), pszName, (SG_TREENODEENTRY_TYPE_DIRECTORY == ptnSub->type), &pMyRepoPath)  );
				}
				else
				{
					SG_ASSERT('@' == pszName[0]);
					SG_ASSERT(0 == pszName[1]);

					SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pMyRepoPath, "@/")  );
				}

				SG_ERR_CHECK(  sg_ptnode__commit(pCtx, ptnSub, pMyRepoPath, ptx, &bNeedToWriteMyTreenode)  );

				SG_ERR_CHECK(  sg_ptnode__create_entry_for_commit(pCtx, ptnSub)  );

				if (ptnSub->pNewEntry)
				{
					SG_ERR_CHECK(  SG_TREENODE_ENTRY__ALLOC__COPY(pCtx, &pEntry, ptnSub->pNewEntry)  );
					SG_ERR_CHECK(  SG_treenode__add_entry(pCtx, pTreenode, ptnSub->pszgid, &pEntry)  );
				}

				SG_STRING_NULLFREE(pCtx, pMyRepoPath);

				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszgid, (void**) &ptnSub)  );
			}
			SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
		}
		else
		{
            if (b_path_is_new)
            {
                SG_ERR_CHECK(  sg_ptnode__write_all_treepaths(pCtx, ptn, pRepoPath, ptx)  );
            }

			if (
				(!ptn->pBaselineEntry)
				&& (ptn->pCurrentParent)
				)
			{
				bNeedToWriteMyTreenode = SG_TRUE;
			}
		}

		if (bNeedToWriteMyTreenode)
		{
			SG_ERR_CHECK(  SG_committing__add_treenode(pCtx, ptx, pTreenode, &pszidHid)  );
		}

		SG_TREENODE_NULLFREE(pCtx, pTreenode);
	}
	else if (SG_TREENODEENTRY_TYPE_REGULAR_FILE == ptn->type)
	{
		SG_fsobj_stat stat;
		SG_bool bFileIsDirty = SG_FALSE;

		SG_ERR_CHECK(  sg_ptnode__is_file_dirty(pCtx, ptn, &bFileIsDirty)  );

		if (bFileIsDirty)
		{
			SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathLocal, SG_FILE_LOCK | SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile)  );

            /* TODO fix the b_dont_bother flag below according to the filename of this item */
			SG_ERR_CHECK(  SG_committing__add_file(pCtx, ptx, ptn->pszgid, SG_FALSE, pFile, SG_BLOB_REFTYPE__TREEUSERFILE, &pszidHid)  );

			SG_FILE_NULLCLOSE(pCtx, pFile);

			*pbParentDirty = SG_TRUE;
		}

		// set/update the timestamp cache for this file.  after the commit
		// has finished, this *actual* file in the WD will be associated
		// with this GID in the WD.  (that is, when we move forward to the
		// new baseline, this file will still be on disk with this datestamp.)
		//
		// TODO think about moving this into the above "if (dirty) ...".

		SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathLocal, &bFoundOnDisk, NULL, NULL)  );
		if (bFoundOnDisk)
		{
			SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPathLocal, &stat)  );
			SG_ERR_CHECK(  SG_pendingtree__set_wd_file_timestamp__dont_save_pendingtree(pCtx, ptn->pPendingTree, ptn->pszgid, stat.mtime_ms)  );
		}

	}
	else if (SG_TREENODEENTRY_TYPE_SYMLINK == ptn->type)
	{
		SG_bool bIsLinkDirty = SG_FALSE;

        /* TODO the logic here differs from the logic for a file, above.
         * should it?  for a file, we trust the hid which was calculated during
         * the last scan.  for a symlink, we just read it and check the hid
         * right now, since it's always going to be so short anyway. */

		SG_ERR_CHECK(  SG_fsobj__readlink(pCtx, pPathLocal, &pstrLink)  );
		SG_ERR_CHECK(  SG_repo__alloc_compute_hash__from_string(pCtx, ptn->pPendingTree->pRepo, pstrLink, &pszidHid)  );

		if (!ptn->pBaselineEntry)
		{
			bIsLinkDirty = SG_TRUE;
		}
		else
		{
			const char* pszBaselineHid = NULL;

			SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, ptn->pBaselineEntry, &pszBaselineHid)  );

			if (0 != strcmp(pszBaselineHid, pszidHid))
			{
				bIsLinkDirty = SG_TRUE;
			}
		}

		SG_NULLFREE(pCtx, pszidHid);

		if (bIsLinkDirty)
		{
			SG_ERR_CHECK(  SG_committing__add_bytes__string(pCtx, ptx, NULL, pstrLink, SG_BLOB_REFTYPE__TREESYMLINK, &pszidHid)  );
			*pbParentDirty = SG_TRUE;
		}
		SG_STRING_NULLFREE(pCtx, pstrLink);
	}
	else
	{
		SG_ERR_THROW_RETURN( SG_ERR_INVALID_PENDINGTREE_OBJECT_TYPE );
	}

	if (pszidHid)
	{
		/* TODO is pszidHid really different? If not, we could just
		 * leave pszCurrentHid null and not set *pbParentDirty */
		SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, ptn->pPendingTree->pStrPool, pszidHid, &ptn->pszCurrentHid)  );
		SG_NULLFREE(pCtx, pszidHid);
		*pbParentDirty = SG_TRUE;
	}

#if 1
	/* TODO the following extra check feels hack-ish, like we're
	 * trying to catch cases we missed above.  Shouldn't we have
	 * figured this out by now? */
	if (!*pbParentDirty)
	{
		SG_ERR_CHECK(  sg_ptnode__check_parent_dirty_non_recursive(pCtx, ptn, pbParentDirty)  );
	}
#endif

	SG_PATHNAME_NULLFREE(pCtx, pPathLocal);

	return;

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_STRING_NULLFREE(pCtx, pstrLink);
	SG_NULLFREE(pCtx, pszidHid);
	SG_PATHNAME_NULLFREE(pCtx, pPathLocal);
	SG_STRING_NULLFREE(pCtx, pMyRepoPath);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
    SG_TREENODE_ENTRY_NULLFREE(pCtx, pEntry);
	SG_TREENODE_NULLFREE(pCtx, pTreenode);
}

static void STUPID_HACK_TODO_sg_ptnode__find_by_gid(
	SG_context* pCtx, 
	struct sg_ptnode* ptn,
	const char* pszGid,
	SG_uint32 depth,
	struct sg_ptnode** ppResult
	)
{
	SG_rbtree_iterator* pIterator = NULL;
	struct sg_ptnode* ptnSub = NULL;

#if TRACE_PENDINGTREE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"%*c looking at:  [gid %s][saved %d][temp %d][prb %d]\n",
							   depth*4,' ',
							   ((ptn->pszgid) ? ptn->pszgid : "(null)"),
							   ptn->saved_flags,
							   ptn->temp_flags,
							   (ptn->prbItems != NULL))  );
#endif

	/* to ensure that this function is unambiguous, we should probably
	 * not return results which are MOVED_AWAY.  That happens to be
	 * one case where a gid appears twice in a pending tree. */

	if (
		!(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
		&& (ptn->pszgid)
		&& (0 == strcmp(pszGid, ptn->pszgid))
		)
	{
		*ppResult = ptn;
		return;
	}

	if (ptn->prbItems)
	{
        SG_bool b = SG_FALSE;

		SG_ERR_CHECK(  SG_rbtree__find(pCtx, ptn->prbItems, pszGid, &b, (void**) &ptnSub)  );

		if (b && (!(ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)) )
        {
            *ppResult = ptnSub;
            return;
        }
        else
		{
			const char* pszKey = NULL;

			/* Recurse downward into every entry */
			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptn->prbItems, &b, &pszKey, (void**) &ptnSub)  );
			while (b)
			{
				SG_ERR_CHECK(  STUPID_HACK_TODO_sg_ptnode__find_by_gid(pCtx, ptnSub, pszGid, depth+1, ppResult)  );

				if (*ppResult)
				{
					break;
				}

				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, (void**) &ptnSub)  );
			}
			SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
		}
	}

	return;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}

static void sg_pendingtree__find_by_gid(
	SG_context* pCtx, 
	SG_pendingtree* pPendingTree,
	const char* pszGid,
	struct sg_ptnode** ppResult
	)
{
	struct sg_ptnode* ptn = NULL;

#if TRACE_PENDINGTREE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "sg_pendingtree__find_by_gid: looking for [gid %s]\n",
							   pszGid)  );
#endif

	SG_ERR_CHECK(  STUPID_HACK_TODO_sg_ptnode__find_by_gid(pCtx, pPendingTree->pSuperRoot, pszGid, 1, &ptn)  );

	if (!ptn)
	{
#if 0 && defined(DEBUG)
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "PendingTreeFindByGid: About to throw NOT_FOUND for '%s'\n", pszGid)  );
		SG_ERR_IGNORE(  SG_pendingtree_debug__dump_existing_to_console(pCtx, pPendingTree)  );
#endif
		SG_ERR_THROW2(  SG_ERR_NOT_FOUND,
						(pCtx,"PendingTreeFindByGid '%s'",pszGid)  );
	}

#if TRACE_PENDINGTREE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "sg_pendingtree__find_by_gid: found ptn [parent %s][child %s][child saved %d][child temp %d]\n",
							   ((ptn->pCurrentParent->pszgid) ? ptn->pCurrentParent->pszgid : "(null)"),
							   ptn->pszgid,ptn->saved_flags,ptn->temp_flags)  );
#endif

	*ppResult = ptn;

fail:
	return;
}

void SG_pendingtree__find_repo_path_by_gid(SG_context * pCtx,
										   SG_pendingtree * pPendingTree,
										   const char * pszGid,
										   SG_string ** ppStrRepoPath)
{
	struct sg_ptnode * ptn = NULL;

	SG_ERR_CHECK_RETURN(  sg_pendingtree__find_by_gid(pCtx, pPendingTree, pszGid, &ptn)  );
	SG_ERR_CHECK_RETURN(  sg_ptnode__get_repo_path(pCtx, ptn, ppStrRepoPath)  );
}

/**
 * This function finds places where we are committing the destination
 * end of a move operation before the source end.  This is not
 * allowed, because the resulting tree will have the item appearing in
 * both places.
 *
 * We do, however, allow committing the source end without the
 * destination end.  The result is that the moved item is not in the
 * tree at all.  Not sure why anybody would want to do this, but it
 * doesn't violate a fundamental law, so we'll allow it.
 *
 * A corollary is that we allow committing the destination end without
 * the source end /if/ the source end was already committed in some
 * previous operation.
 *
 * TODO think about the revert operations and other stuff to make sure
 * this is all okay.  If any problems come up, we'll just require that
 * both ends always get committed at the same time.  But that will
 * require more bookkeeping in the ptnode structure.
 *
 */
static void sg_ptnode__fail_if_any_split_moves(
	SG_context* pCtx, 
	struct sg_ptnode* ptn,
	SG_uint16 flag
	)
{
	SG_rbtree_iterator* pIterator = NULL;
	struct sg_ptnode* ptnOriginalParent = NULL;

	if (!(ptn->temp_flags & flag))
	{
		return;
	}

	if (ptn->pszgidMovedFrom)
	{
		SG_ERR_CHECK(  sg_pendingtree__find_by_gid(pCtx, ptn->pPendingTree, ptn->pszgidMovedFrom, &ptnOriginalParent)  );

		if (
			!(ptnOriginalParent->temp_flags & flag)
			)
		{
			SG_ERR_THROW(  SG_ERR_SPLIT_MOVE  );
		}
	}

	if (ptn->prbItems)
	{
		SG_bool b = SG_FALSE;
		const char* pszKey = NULL;
		struct sg_ptnode* ptnSub = NULL;

		/* Recurse downward into every entry */
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptn->prbItems, &b, &pszKey, (void**) &ptnSub)  );
		while (b)
		{
			SG_ERR_CHECK(  sg_ptnode__fail_if_any_split_moves(pCtx, ptnSub, flag)  );

			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, (void**) &ptnSub)  );
		}
		SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
	}

	return;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}

//////////////////////////////////////////////////////////////////

/**
 * Set the COMMITTING flag on this ptnode and propagate to parent directories.
 */
static void _my_set_commit_flag(SG_context * pCtx, struct sg_ptnode * ptn)
{
	struct sg_ptnode * ptn_temp;

	SG_UNUSED(pCtx);

	ptn->temp_flags |= sg_PTNODE_TEMP_FLAG_COMMITTING;

	// the commit flag must be pushed back up to the parent directory so that
	// the commit of a file causes the parent directory to be commited too (so
	// that the directory HIDs change on the treenodes.
	//
	// TODO 2010/06/07 We may need to split the COMMITING BIT into 2 (one for
	// TODO            actually committing this node and one for there is dirt
	// TODO            within this directory to be committed, so keep diving).
	// TODO            Because if they said "vv commit --include='**.h' ."
	// TODO            they probably just want to commit the *.h changes and
	// TODO            not any moves/renames/xattr/attrbit changes on any of
	// TODO            the parent/ancestor directories that happen to contain
	// TODO            a .h file.  Right?  See sprawl-819.
	//
	// we set the flag on this node.  in theory, if we are in a straight
	// dive from the root, our parent's node should also have its flag set.
	// but if we got started from an ITEM-LIST, our parent may not have been
	// visited.  so we set the bits on our ancestor directories so that when
	// the bubble up happens they will be committed too.

	ptn_temp = ptn->pCurrentParent;
	while (ptn_temp && ((ptn_temp->temp_flags & sg_PTNODE_TEMP_FLAG_COMMITTING) == 0))
	{
		ptn_temp->temp_flags |= sg_PTNODE_TEMP_FLAG_COMMITTING;
		ptn_temp = ptn_temp->pCurrentParent;
	}
}

static void sg_ptnode__maybe_mark_ptnode_for_commit__recursive(SG_context * pCtx,
															   struct sg_ptnode * ptn,
															   SG_bool bRecursive,
															   const char* const* paszIncludes, SG_uint32 count_includes,
															   const char* const* paszExcludes, SG_uint32 count_excludes);

/**
 * Do the actual dirty work of walking the contents of the directory
 * and marking the contents.
 *
 * WE DO NOT TAKE A bRecursive FLAG because it only applied to the
 * top-level items named in SG_pendingtree__commit() and as discussed in
 * sg_ptnode__maybe_mark_ptnode_for_commit__recursive() we don't want to
 * propagate that.
 */
static void _my_commit_include_dir(SG_context * pCtx,
								   struct sg_ptnode * ptn,
								   SG_string * pStringRepoPath,
								   const char* const* paszIncludes, SG_uint32 count_includes,
								   const char* const* paszExcludes, SG_uint32 count_excludes)
{
	SG_rbtree_iterator * pIterator = NULL;
	const char * pszKey;
	struct sg_ptnode * ptnSub;
	SG_bool bFound = SG_FALSE;

	SG_ASSERT(  (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)  );

	if (!ptn->prbItems)
	{
#if TRACE_COMMIT
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MarkPtnodesForCommit: TODO determine if the directory [%s] is really empty or just not loaded.\n",
								   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#else
		SG_UNUSED(pStringRepoPath);
#endif
		return;
	}

#if TRACE_COMMIT
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   "MarkPtnodesForCommit: Considering contents of directory [%s]\n",
							   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#endif

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptn->prbItems, &bFound, &pszKey, (void**) &ptnSub)  );
	while (bFound)
	{
		SG_ERR_CHECK(  sg_ptnode__maybe_mark_ptnode_for_commit__recursive(pCtx,
																		  ptnSub,
																		  SG_TRUE,		// do not propagate bRecursive
																		  paszIncludes, count_includes,
																		  paszExcludes, count_excludes)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &bFound, &pszKey, (void**) &ptnSub)  );
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}

/**
 * A directory was EXPLICITLY INCLUDED.  (there were INCLUDES and we matched one of them.)
 *
 * TODO 2010/06/07 So we matched the directory, what does this imply for
 * TODO            the contents of the directory?  I'm going to assume for
 * TODO            the moment that it means that we add the directory and
 * TODO            then (optinally) dive into it WITHOUT any INCLUDES so
 * TODO            that we get the full contents.
 * TODO
 * TODO            That is, we treat:
 * TODO                vv add --include='**.dir' --include='**.c' .
 * TODO
 * TODO            like the combination of 2 commands:
 * TODO                vv add --include='**.c' .
 * TODO                find . -name="*.dir" | xargs vv add
 * TODO
 * TODO            I don't think it makes any sense to treat it like:
 * TODO                vv add --include='**.c' .
 * TODO                find . -name="*.dir" | xargs vv add --include='**.c'
 * TODO
 * TODO            See sprawl-818.
 *
 * We do, however, carry along the EXCLUDES.
 */
static void _my_commit_explicit_include_dir(SG_context * pCtx,
											struct sg_ptnode * ptn,
											SG_string * pStringRepoPath,
											SG_bool bRecursive,
											const char* const* paszExcludes, SG_uint32 count_excludes)
{
	if (!bRecursive)
	{
#if TRACE_COMMIT
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MarkPtnodesForCommit: Skipping dive on explicitly-included directory [%s] because not-recursive\n",
								   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#endif
		return;
	}

	SG_ERR_CHECK_RETURN(  _my_commit_include_dir(pCtx, ptn, pStringRepoPath,
												 NULL, 0,
												 paszExcludes, count_excludes)  );
}

/**
 * There were no INCLUDES, so we are including everything we can find.
 */
static void _my_commit_implicit_include_dir(SG_context * pCtx,
											struct sg_ptnode * ptn,
											SG_string * pStringRepoPath,
											SG_bool bRecursive,
											const char* const* paszExcludes, SG_uint32 count_excludes)
{
	if (!bRecursive)
	{
#if TRACE_COMMIT
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MarkPtnodesForCommit: Skipping dive on implicitly-included directory [%s] because not-recursive\n",
								   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#endif
		return;
	}

	SG_ERR_CHECK_RETURN(  _my_commit_include_dir(pCtx, ptn, pStringRepoPath,
												 NULL, 0,
												 paszExcludes, count_excludes)  );
}

/**
 * A provisional dir.  We had INCLUDES, but didn't exactly match this directory,
 * allow children to be inspected.
 */
static void _my_commit_provisional_include_dir(SG_context * pCtx,
											   struct sg_ptnode * ptn,
											   SG_string * pStringRepoPath,
											   SG_bool bRecursive,
											   const char* const* paszIncludes, SG_uint32 count_includes,
											   const char* const* paszExcludes, SG_uint32 count_excludes)
{
	if (!bRecursive)
	{
#if TRACE_COMMIT
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MarkPtnodesForCommit: Skipping provisional dive on directory [%s] because not-recursive\n",
								   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#endif
		return;
	}

	SG_ERR_CHECK_RETURN(  _my_commit_include_dir(pCtx, ptn, pStringRepoPath,
												 paszIncludes, count_includes,
												 paszExcludes, count_excludes)  );
}

/**
 * Evaluate the given PTNODE and see if it should be marked
 * for COMMITING based upon the INCLUDE/EXCLUDE args.
 *
 * WE DO NOT TAKE AN IGNORES LIST because we are not scanning
 * the WD; rather, we are walking the ptnodes representing things
 * already under version control.
 *
 * WE TAKE BUT DO NOT PROPAGATE the bRecursive FLAG because that
 * flag only applies to the list of ITEMS that are passed in to the
 * top-level of SG_pendingtree__commit() for things they explicitly
 * named; once we are diving inside of one of them, we have to keep
 * diving.
 *
 * If the PTNODE is a directory, dive into it and try to
 * mark all of the children.
 */
static void sg_ptnode__maybe_mark_ptnode_for_commit__recursive(SG_context * pCtx,
															   struct sg_ptnode * ptn,
															   SG_bool bRecursive,
															   const char* const* paszIncludes, SG_uint32 count_includes,
															   const char* const* paszExcludes, SG_uint32 count_excludes)
{
	SG_string * pStringRepoPath = NULL;
	SG_file_spec_eval eval = SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED;

	// Don't bother trying to filter the super-root since it doesn't
	// have a repo-path.

	if (ptn->pCurrentParent)
	{
		SG_ERR_CHECK(  sg_ptnode__get_repo_path(pCtx, ptn, &pStringRepoPath)  );
		SG_ASSERT(  (pStringRepoPath)  );

		if ((count_includes > 0) || (count_excludes > 0))
		{
			SG_ERR_CHECK(  SG_file_spec__should_include(pCtx,
														paszIncludes, count_includes,
														paszExcludes, count_excludes,
														NULL, 0,
														SG_string__sz(pStringRepoPath),
														&eval)  );
		}
	}

	switch (eval)
	{
	default:				// quiets compiler
		SG_ASSERT( (0) );

	case SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED:
	case SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED:
		// An EXCLUDE/IGNORE pattern can match either a file or folder.  Either way,
		// if we match an EXCLUDE/IGNORE pattern, we can just stop.  This means that
		// when we match a directory, we implicilty EXCLUDE everything within it.
#if TRACE_COMMIT
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MarkPtnodesForCommit: Excluding/Ignoring [%s]\n",
								   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#endif
		break;

	case SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED:
		SG_ERR_CHECK(  _my_set_commit_flag(pCtx, ptn)  );
		if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
			SG_ERR_CHECK(  _my_commit_explicit_include_dir(pCtx, ptn, pStringRepoPath,
														   bRecursive,
														   paszExcludes, count_excludes)  );
		break;

	case SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED:
		SG_ERR_CHECK(  _my_set_commit_flag(pCtx, ptn)  );
		if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
			SG_ERR_CHECK(  _my_commit_implicit_include_dir(pCtx, ptn, pStringRepoPath,
														   bRecursive,
														   paszExcludes, count_excludes)  );
		break;

	case SG_FILE_SPEC_EVAL__MAYBE:
		// a maybe means there were actual includes, but we didn't match one of them.
		if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
		{
			// for a directory, a "maybe" means we should dive and see if there are
			// children that match.
			SG_ERR_CHECK(  _my_commit_provisional_include_dir(pCtx, ptn, pStringRepoPath,
															  bRecursive,
															  paszIncludes, count_includes,
															  paszExcludes, count_excludes)  );
		}
		else
		{
			// for a non-directory, a "maybe" should be treated like an exclude/ignore.
#if TRACE_COMMIT
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
									   "MarkPtnodesForCommit: non-directory-maybe ==> Excluding/Ignoring  [%s]\n",
									   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#endif
		}
	}

fail:
	SG_STRING_NULLFREE(pCtx, pStringRepoPath);
}

/**
 * We are given a partial pathname and a list of entries/items within it.
 * Build the complete pathname, lookup the associated PTNODE and see if
 * it should be marked for COMMITTING based upon the INCLUDE/EXCLUDE args.
 *
 * WE DO NOT TAKE AN IGNORES LIST because we are not scanning
 * the WD; rather, we are walking the ptnodes representing things
 * already under version control.
 *
 * If the PTNODE is a directory, we treat it as the root of a sub-tree
 * and walk all of the stuff within it.
 */
static void sg_ptnode__maybe_mark_items_for_commit(SG_context * pCtx,
												   SG_pendingtree * pPendingTree,
												   const SG_pathname* pPathRelativeTo,
												   SG_uint32 count_items, const char** paszItems,
												   SG_bool bRecursive,
												   const char* const* paszIncludes, SG_uint32 count_includes,
												   const char* const* paszExcludes, SG_uint32 count_excludes)
{
	SG_pathname * pPathItem_k = NULL;
	SG_pathname * pPathParent_k = NULL;
	SG_string * pStringEntryname_k = NULL;
	struct sg_ptnode * ptn_k;
	struct sg_ptnode * ptnParent_k;
	SG_uint32 k;

	for (k=0; k<count_items; k++)
	{
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathItem_k, pPathRelativeTo, paszItems[k])  );

		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathParent_k, pPathItem_k)  );
		SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathParent_k)  );
		SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathParent_k, SG_TRUE, &ptnParent_k)  );

		SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathItem_k, &pStringEntryname_k)  );
		SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__active(pCtx, ptnParent_k, SG_string__sz(pStringEntryname_k), &ptn_k)  );

		// TODO 2010/06/08 Sprawl-826.  If we have a deleted file within a deleted directory,
		// TODO            can we request a partial-commit that only names the file?  We need
		// TODO            to an __inactive() lookup to get its ptnode.

		if (!ptn_k)
			SG_ERR_THROW2(  SG_ERR_NOT_FOUND,
							(pCtx, "Item [%s] not found in pendingtree.", SG_pathname__sz(pPathItem_k))  );

#if TRACE_COMMIT
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MarkItemsForCommit: Starting at [%s]\n",
								   SG_pathname__sz(pPathItem_k))  );
#endif

		SG_ERR_CHECK(  sg_ptnode__maybe_mark_ptnode_for_commit__recursive(pCtx,
																		  ptn_k,
																		  bRecursive,
																		  paszIncludes, count_includes,
																		  paszExcludes, count_excludes)  );

		SG_PATHNAME_NULLFREE(pCtx, pPathItem_k);
		SG_PATHNAME_NULLFREE(pCtx, pPathParent_k);
		SG_STRING_NULLFREE(pCtx, pStringEntryname_k);
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathItem_k);
	SG_PATHNAME_NULLFREE(pCtx, pPathParent_k);
	SG_STRING_NULLFREE(pCtx, pStringEntryname_k);
}

//////////////////////////////////////////////////////////////////

/**
 * Set the REVERTING flag on this ptnode.
 */
static void _my_set_revert_flag(SG_context * pCtx, struct sg_ptnode * ptn)
{
	SG_UNUSED(pCtx);

	ptn->temp_flags |= sg_PTNODE_TEMP_FLAG_REVERTING;

	// WE DEVIATE HERE FROM THE COMMIT VERSION.
	//
	// I don't think we want to force set the REVERTING flag in the ptnodes
	// of the parent directories.  In the case of a COMMIT, when we commit
	// changes to a file, we alter the treenode of the parent directory (and
	// change its HID) which alters its parent directory and so on.  However,
	// in the case of REVERT, the user has only said that they want to revert
	// the given ptnode -- that is completely independent of any changes
	// made to any of the parent directories.
	//
	// For example, if I have:
	//     @/dir1/file1
	// in the tree and do:
	//     vv rename dir1/file1 file2
	//     vv rename dir1 dir2
	// and say:
	//     vv revert dir2/file2
	// we should have just:
	//     @/dir2/file1
	// because there's no reason to assume that the rename of dir2 should be
	// reverted too.
}

static void sg_ptnode__maybe_mark_ptnode_for_revert__recursive(SG_context * pCtx,
															   struct sg_ptnode * ptn,
															   SG_bool bRecursive,
															   const char* const* paszIncludes, SG_uint32 count_includes,
															   const char* const* paszExcludes, SG_uint32 count_excludes);

/**
 * Do the actual dirty work of walking the contents of the directory
 * and marking the contents.
 *
 * WE DO NOT TAKE A bRecursive FLAG because it only applied to the
 * top-level items named in SG_pendingtree__revert() and as discussed in
 * sg_ptnode__maybe_mark_ptnode_for_revert__recursive() we don't want to
 * propagate that.
 */
static void _my_revert_include_dir(SG_context * pCtx,
								   struct sg_ptnode * ptn,
								   SG_string * pStringRepoPath,
								   const char* const* paszIncludes, SG_uint32 count_includes,
								   const char* const* paszExcludes, SG_uint32 count_excludes)
{
	SG_rbtree_iterator * pIterator = NULL;
	const char * pszKey;
	struct sg_ptnode * ptnSub;
	SG_bool bFound = SG_FALSE;

	SG_ASSERT(  (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)  );

	if (!ptn->prbItems)
	{
#if 0 && defined(DEBUG)
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MarkPtnodesForRevert: TODO determine if the directory [%s] is really empty or just not loaded.\n",
								   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#else
        SG_UNUSED(pStringRepoPath);
#endif
		return;
	}

#if TRACE_REVERT
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   "MarkPtnodesForRevert: Considering contents of directory [%s]\n",
							   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#endif

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptn->prbItems, &bFound, &pszKey, (void**) &ptnSub)  );
	while (bFound)
	{
		SG_ERR_CHECK(  sg_ptnode__maybe_mark_ptnode_for_revert__recursive(pCtx,
																		  ptnSub,
																		  SG_TRUE,		// do not propagate bRecursive
																		  paszIncludes, count_includes,
																		  paszExcludes, count_excludes)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &bFound, &pszKey, (void**) &ptnSub)  );
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}

/**
 * A directory was EXPLICITLY INCLUDED.  (there were INCLUDES and we matched one of them.)
 *
 * TODO 2010/06/07 So we matched the directory, what does this imply for
 * TODO            the contents of the directory?  I'm going to assume for
 * TODO            the moment that it means that we add the directory and
 * TODO            then (optinally) dive into it WITHOUT any INCLUDES so
 * TODO            that we get the full contents.
 * TODO
 * TODO            That is, we treat:
 * TODO                vv add --include='**.dir' --include='**.c' .
 * TODO
 * TODO            like the combination of 2 commands:
 * TODO                vv add --include='**.c' .
 * TODO                find . -name="*.dir" | xargs vv add
 * TODO
 * TODO            I don't think it makes any sense to treat it like:
 * TODO                vv add --include='**.c' .
 * TODO                find . -name="*.dir" | xargs vv add --include='**.c'
 * TODO
 * TODO            See sprawl-818.
 *
 * We do, however, carry along the EXCLUDES.
 */
static void _my_revert_explicit_include_dir(SG_context * pCtx,
											struct sg_ptnode * ptn,
											SG_string * pStringRepoPath,
											SG_bool bRecursive,
											const char* const* paszExcludes, SG_uint32 count_excludes)
{
	if (!bRecursive)
	{
#if TRACE_REVERT
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MarkPtnodesForRevert: Skipping dive on explicitly-included directory [%s] because not-recursive\n",
								   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#endif
		return;
	}

	SG_ERR_CHECK_RETURN(  _my_revert_include_dir(pCtx, ptn, pStringRepoPath,
												 NULL, 0,
												 paszExcludes, count_excludes)  );
}

/**
 * There were no INCLUDES, so we are including everything we can find.
 */
static void _my_revert_implicit_include_dir(SG_context * pCtx,
											struct sg_ptnode * ptn,
											SG_string * pStringRepoPath,
											SG_bool bRecursive,
											const char* const* paszExcludes, SG_uint32 count_excludes)
{
	if (!bRecursive)
	{
#if TRACE_REVERT
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MarkPtnodesForRevert: Skipping dive on implicitly-included directory [%s] because not-recursive\n",
								   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#endif
		return;
	}

	SG_ERR_CHECK_RETURN(  _my_revert_include_dir(pCtx, ptn, pStringRepoPath,
												 NULL, 0,
												 paszExcludes, count_excludes)  );
}

/**
 * A provisional dir.  We had INCLUDES, but didn't exactly match this directory,
 * allow children to be inspected.
 */
static void _my_revert_provisional_include_dir(SG_context * pCtx,
											   struct sg_ptnode * ptn,
											   SG_string * pStringRepoPath,
											   SG_bool bRecursive,
											   const char* const* paszIncludes, SG_uint32 count_includes,
											   const char* const* paszExcludes, SG_uint32 count_excludes)
{
	if (!bRecursive)
	{
#if TRACE_REVERT
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MarkPtnodesForRevert: Skipping provisional dive on directory [%s] because not-recursive\n",
								   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#endif
		return;
	}

	SG_ERR_CHECK_RETURN(  _my_revert_include_dir(pCtx, ptn, pStringRepoPath,
												 paszIncludes, count_includes,
												 paszExcludes, count_excludes)  );
}

/**
 * Evaluate the given PTNODE and see if it should be marked
 * for REVERTING based upon the INCLUDE/EXCLUDE args.
 *
 * WE DO NOT TAKE AN IGNORES LIST because we are not scanning
 * the WD; rather, we are walking the ptnodes representing things
 * already under version control.
 *
 * WE TAKE BUT DO NOT PROPAGATE the bRecursive FLAG because that
 * flag only applies to the list of ITEMS that are passed in to the
 * top-level of SG_pendingtree__revert() for things they explicitly
 * named; once we are diving inside of one of them, we have to keep
 * diving.
 *
 * If the PTNODE is a directory, dive into it and try to
 * mark all of the children.
 */
static void sg_ptnode__maybe_mark_ptnode_for_revert__recursive(SG_context * pCtx,
															   struct sg_ptnode * ptn,
															   SG_bool bRecursive,
															   const char* const* paszIncludes, SG_uint32 count_includes,
															   const char* const* paszExcludes, SG_uint32 count_excludes)
{
	SG_string * pStringRepoPath = NULL;
	SG_file_spec_eval eval = SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED;

	// Don't bother trying to filter the super-root since it doesn't
	// have a repo-path.

	if (ptn->pCurrentParent)
	{
		SG_ERR_CHECK(  sg_ptnode__get_repo_path(pCtx, ptn, &pStringRepoPath)  );
		SG_ASSERT(  (pStringRepoPath)  );

		if ((count_includes > 0) || (count_excludes > 0))
		{
			SG_ERR_CHECK(  SG_file_spec__should_include(pCtx,
														paszIncludes, count_includes,
														paszExcludes, count_excludes,
														NULL, 0,
														SG_string__sz(pStringRepoPath),
														&eval)  );
		}
	}

	switch (eval)
	{
	default:			// quiets compiler
		SG_ASSERT( (0) );

	case SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED:
	case SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED:
		// An EXCLUDE/IGNORE pattern can match either a file or folder.  Either way,
		// if we match an EXCLUDE/IGNORE pattern, we can just stop.  This means that
		// when we match a directory, we implicilty EXCLUDE everything within it.
#if TRACE_REVERT
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MarkPtnodesForRevert: Excluding/Ignoring [%s]\n",
								   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#endif
		break;

	case SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED:
		SG_ERR_CHECK(  _my_set_revert_flag(pCtx, ptn)  );
		if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
			SG_ERR_CHECK(  _my_revert_explicit_include_dir(pCtx, ptn, pStringRepoPath,
														   bRecursive,
														   paszExcludes, count_excludes)  );
		break;

	case SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED:
		SG_ERR_CHECK(  _my_set_revert_flag(pCtx, ptn)  );
		if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
			SG_ERR_CHECK(  _my_revert_implicit_include_dir(pCtx, ptn, pStringRepoPath,
														   bRecursive,
														   paszExcludes, count_excludes)  );
		break;

	case SG_FILE_SPEC_EVAL__MAYBE:
		// a maybe means there were actual includes, but we didn't match one of them.
		if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
		{
			// for a directory, a "maybe" means we should dive and see if there are
			// children that match.
			SG_ERR_CHECK(  _my_revert_provisional_include_dir(pCtx, ptn, pStringRepoPath,
															  bRecursive,
															  paszIncludes, count_includes,
															  paszExcludes, count_excludes)  );
		}
		else
		{
			// for a non-directory, a "maybe" should be treated like an exclude/ignore.
#if TRACE_REVERT
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
									   "MarkPtnodesForRevert: non-directory-maybe ==> Excluding/Ignoring  [%s]\n",
									   ((pStringRepoPath) ? SG_string__sz(pStringRepoPath) : "(null)"))  );
#endif
		}
	}

fail:
	SG_STRING_NULLFREE(pCtx, pStringRepoPath);
}

/**
 * We are given a partial pathname and a list of entries/items within it.
 * Build the complete pathname, lookup the associated PTNODE and see if
 * it should be marked for REVERTING based upon the INCLUDE/EXCLUDE args.
 *
 * WE DO NOT TAKE AN IGNORES LIST because we are not scanning
 * the WD; rather, we are walking the ptnodes representing things
 * already under version control.
 *
 * If the PTNODE is a directory, we treat it as the root of a sub-tree
 * and walk all of the stuff within it.
 */
static void sg_ptnode__maybe_mark_items_for_revert(SG_context * pCtx,
												   SG_pendingtree * pPendingTree,
												   const SG_pathname* pPathRelativeTo,
												   SG_uint32 count_items, const char** paszItems,
												   SG_bool bRecursive,
												   const char* const* paszIncludes, SG_uint32 count_includes,
												   const char* const* paszExcludes, SG_uint32 count_excludes)
{
	SG_pathname * pPathItem_k = NULL;
	SG_pathname * pPathParent_k = NULL;
	SG_string * pStringEntryname_k = NULL;
	struct sg_ptnode * ptn_k;
	struct sg_ptnode * ptnParent_k;
	SG_uint32 k;

	for (k=0; k<count_items; k++)
	{
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathItem_k, pPathRelativeTo, paszItems[k])  );

		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathParent_k, pPathItem_k)  );
		SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathParent_k)  );
		SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathParent_k, SG_TRUE, &ptnParent_k)  );

		SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathItem_k, &pStringEntryname_k)  );
		SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__active(pCtx, ptnParent_k, SG_string__sz(pStringEntryname_k), &ptn_k)  );
		if (!ptn_k)
			SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__inactive(pCtx, ptnParent_k, SG_string__sz(pStringEntryname_k), &ptn_k)  );

		// TODO 2010/06/08 Sprawl-825.  If we are trying to revert something that was
		// TODO            deleted, we need to do an __inactive() lookup to get its ptnode.
		// TODO            We also need to verify that the parent is active.
		// TODO            That is if they deleted the parent (or really any ancestor)
		// TODO            directory, we cannot un-delete this entry without first doing
		// TODO            the parents.
		// TODO
		// TODO            We should throw
		// TODO            SG_ERR_THROW(  SG_ERR_PENDINGTREE_PARENT_DIR_MISSING_OR_INACTIVE  );

		if (!ptn_k)
			SG_ERR_THROW2(  SG_ERR_NOT_FOUND,
							(pCtx, "Item [%s] not found in pendingtree.", SG_pathname__sz(pPathItem_k))  );

#if TRACE_REVERT
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MarkItemsForRevert: Starting at [%s]\n",
								   SG_pathname__sz(pPathItem_k))  );
#endif

		SG_ERR_CHECK(  sg_ptnode__maybe_mark_ptnode_for_revert__recursive(pCtx,
																		  ptn_k,
																		  bRecursive,
																		  paszIncludes, count_includes,
																		  paszExcludes, count_excludes)  );

		SG_PATHNAME_NULLFREE(pCtx, pPathItem_k);
		SG_PATHNAME_NULLFREE(pCtx, pPathParent_k);
		SG_STRING_NULLFREE(pCtx, pStringEntryname_k);
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathItem_k);
	SG_PATHNAME_NULLFREE(pCtx, pPathParent_k);
	SG_STRING_NULLFREE(pCtx, pStringEntryname_k);
}


static void sg_ptnode__check_parent_dirty_non_recursive(
	SG_context* pCtx,
	struct sg_ptnode* ptn,
	SG_bool* pbParentDirty
	)
{
	/* This function never clears the flag, it merely sets it when
	 * needed.  So if it is already set, we're done. */
	if (*pbParentDirty)
	{
		return;
	}

	if (
		(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
		|| (ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
		|| (ptn->pszgidMovedFrom)
		|| (
			(!ptn->pBaselineEntry)
			&& ptn->pCurrentParent
			)
		)
	{
		*pbParentDirty = SG_TRUE;
		return;
	}

	if (ptn->pszCurrentName)
	{
		const char* pszBaselineName = NULL;

		SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx, ptn->pBaselineEntry, &pszBaselineName)  );

		if (0 != strcmp(ptn->pszCurrentName, pszBaselineName))
		{
			*pbParentDirty = SG_TRUE;
			return;
		}
	}

    if (ptn->pBaselineEntry)
    {
        SG_int64 iBaselineAttributeBits = 0;
        const char* pszBaselineHidAttributes = NULL;

        SG_ERR_CHECK(  SG_treenode_entry__get_attribute_bits(pCtx, ptn->pBaselineEntry, &iBaselineAttributeBits)  );
        if (iBaselineAttributeBits != ptn->iCurrentAttributeBits)
        {
            *pbParentDirty = SG_TRUE;
            return;
        }

		SG_ERR_CHECK(  SG_treenode_entry__get_hid_xattrs(pCtx, ptn->pBaselineEntry, &pszBaselineHidAttributes)  );
        if (ptn->pszHidXattrs && pszBaselineHidAttributes)
        {
            if (0 != strcmp(ptn->pszHidXattrs, pszBaselineHidAttributes))
            {
                *pbParentDirty = SG_TRUE;
                return;
            }
        }
        else if (ptn->pszHidXattrs || pszBaselineHidAttributes)
        {
            *pbParentDirty = SG_TRUE;
            return;
        }
    }

fail:
	return;
}

static void sg_ptnode__post_commit_fixup(
	SG_context* pCtx,
	struct sg_ptnode* ptn,
	SG_bool* pbRemoveMe
	)
{
	SG_rbtree_iterator* pIterator = NULL;
	SG_rbtree* prbRemove = NULL;

	if (!(ptn->temp_flags & sg_PTNODE_TEMP_FLAG_COMMITTING))
	{
		*pbRemoveMe = SG_FALSE;
		return;
	}

	ptn->temp_flags &= ~sg_PTNODE_TEMP_FLAG_COMMITTING;
	if (
		(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
		|| (ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
		)
	{
		*pbRemoveMe = SG_TRUE;
		return;
	}

	if (ptn->pCurrentParent)
	{
		/*
		 * As long as this is not the top of the tree, we expect this
		 * node to have a new entry, because all of the cases where
		 * it does not have already been checked above.
		 *
		 * So, we take the new entry and make it the baseline entry,
		 * since the commit succeeded.  This means that later if this
		 * node is checked for dirty, it will claim that it is not
		 * dirty.
		 */

		SG_ASSERT(ptn->pNewEntry);
		SG_treenode_entry__free(pCtx, ptn->pBaselineEntry);
		ptn->pBaselineEntry = ptn->pNewEntry;
		ptn->pNewEntry = NULL;

		ptn->pszCurrentName = NULL;
		ptn->pszCurrentHid = NULL;
        ptn->pszHidXattrs = NULL;
        SG_ERR_CHECK(  SG_treenode_entry__get_hid_xattrs(pCtx, ptn->pBaselineEntry, &ptn->pszHidXattrs)  );
		ptn->pszgidMovedFrom = NULL;
        SG_ERR_CHECK(  SG_treenode_entry__get_attribute_bits(pCtx, ptn->pBaselineEntry, &ptn->iCurrentAttributeBits)  );
	}

	if (SG_TREENODEENTRY_TYPE_DIRECTORY == ptn->type)
	{
		SG_bool b = SG_FALSE;
		const char* pszKey = NULL;
		struct sg_ptnode* ptnSub = NULL;
		SG_uint32 count = 0;

		if (ptn->prbItems)
		{
			SG_ERR_CHECK(  SG_rbtree__count(pCtx, ptn->prbItems, &count)  );
		}

		if (count)
		{
			SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS(pCtx, &prbRemove, count, NULL)  );

			SG_ASSERT(ptn->prbItems);

			/* Recurse downward into every entry */
			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptn->prbItems, &b, &pszKey, (void**) &ptnSub)  );
			while (b)
			{
				SG_bool bRemoveThis = SG_FALSE;

				SG_ERR_CHECK(  sg_ptnode__post_commit_fixup(pCtx, ptnSub, &bRemoveThis)  );
				if (bRemoveThis)
				{
					SG_ERR_CHECK(  SG_rbtree__add(pCtx, prbRemove, pszKey)  );
				}

				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, (void**) &ptnSub)  );
			}
			SG_rbtree__iterator__free(pCtx, pIterator);
			pIterator = NULL;

			/* Now remove anything which asked to be removed */
			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, prbRemove, &b, &pszKey, NULL)  );
			while (b)
			{
				SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, ptn->prbItems, pszKey, (void**) &ptnSub)  );

				SG_PTNODE_NULLFREE(pCtx, ptnSub);

				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, (void**) &ptnSub)  );
			}
			SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);

			SG_RBTREE_NULLFREE(pCtx, prbRemove);
		}
	}

	return;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}

void sg_pendingtree__post_commit_fixup(
	SG_context* pCtx,
	SG_pendingtree* pPendingTree
	)
{
	SG_bool bRemoveTop = SG_FALSE;
	SG_bool bDirty = SG_FALSE;

	SG_ERR_CHECK(  sg_ptnode__post_commit_fixup(pCtx, pPendingTree->pSuperRoot, &bRemoveTop)  );
	SG_ERR_CHECK(  sg_ptnode__unload_dir_entries_if_not_dirty(pCtx, pPendingTree->pSuperRoot, &bDirty)  );

	if (!(pPendingTree->pSuperRoot->prbItems))
	{
		SG_PTNODE_NULLFREE(pCtx, pPendingTree->pSuperRoot);
	}

fail:
	return;
}

void sg_pendingtree__assoc(
	SG_context* pCtx,
    SG_repo* pRepo,
    const SG_audit* pq,
	const char * pszidChangesetHid,
    const char* const* paszAssocs,
    SG_uint32 count_assocs)
{
    char * pLeaf = NULL;
    SG_varray * pErrors = NULL;
    SG_varray * pDummy = NULL;
    SG_zingtx * pTx = NULL;
    SG_zingtemplate * pTemplate = NULL;
    SG_zingrecord * p_vc_changeset = NULL;
    SG_zingfieldattributes * pFieldAttributes = NULL;
    SG_uint32 i;
    char * psz_recid = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRepo);
    SG_NULLARGCHECK_RETURN(pq);
    SG_NULLARGCHECK_RETURN(pszidChangesetHid);
    SG_NULL_PP_CHECK_RETURN(paszAssocs);
    SG_ARGCHECK_RETURN(count_assocs>0, count_assocs);

    SG_ERR_CHECK(  SG_zing__get_leaf__merge_if_necessary(pCtx, pRepo, pq, SG_DAGNUM__WORK_ITEMS, &pLeaf, &pErrors, &pDummy)  );
    if(pErrors!=NULL)
        SG_ERR_THROW(SG_ERR_ZING_MERGE_CONFLICT);
    else
        SG_VARRAY_NULLFREE(pCtx, pDummy);
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, SG_DAGNUM__WORK_ITEMS, pq->who_szUserId, pLeaf, &pTx)  );
    if(pLeaf==NULL)
        SG_ERR_THROW(SG_ERR_ZING_RECORD_NOT_FOUND);
    SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pTx, pLeaf)  );
    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pTx, &pTemplate)  );

    SG_ERR_CHECK(  SG_zingtx__create_new_record(pCtx, pTx, "vc_changeset", &p_vc_changeset)  );
    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pTemplate, "vc_changeset", "commit", &pFieldAttributes)  );
    SG_ERR_CHECK(  SG_zingrecord__set_field__dagnode(pCtx, p_vc_changeset, pFieldAttributes, pszidChangesetHid)  );

    for(i=0;i<count_assocs;++i)
    {
        SG_ERR_CHECK(  SG_zingtx__lookup_recid(pCtx, pTx, "item", "id", paszAssocs[i], &psz_recid)  );
        SG_ERR_CHECK(  SG_zingrecord__add_link_and_overwrite_other_if_singular(pCtx, p_vc_changeset, "items", psz_recid)  );
        SG_NULLFREE(pCtx, psz_recid);
    }

    SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, pq->when_int64, &pTx, NULL, NULL, &pErrors)  );

    SG_NULLFREE(pCtx, pLeaf);

    return;
fail:
    SG_NULLFREE(pCtx, psz_recid);
    SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pTx)  );
    SG_NULLFREE(pCtx, pLeaf);
    SG_VARRAY_NULLFREE(pCtx, pDummy);
    SG_VARRAY_NULLFREE(pCtx, pErrors);
}

/* TODO why doesn't "sg commit ." work ?
 *
 * TODO 2010/05/18 Is the above comment still valid?
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
							SG_dagnode** ppdn)
{
	// We DO NOT need/use the IGNORES list because we are not scanning.

	SG_committing* ptx = NULL;
	SG_changeset * pChangesetOriginal = NULL;
	SG_dagnode * pDagnodeOriginal = NULL;
	const char* psz_hid_parent_k = NULL;
	const SG_varray * pva_wd_parents = NULL;		// we don't own this
	SG_bool bSomethingCommitted = SG_FALSE;
	SG_uint32 k, nrParents;
	const char * psz_hid_new_changeset = NULL;
	SG_uint32 nrUnresolved;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(pq);

	SG_ARGCHECK_RETURN(  ((   count_items==0)   || (   paszItems != NULL)), count_items     );
	SG_ARGCHECK_RETURN(  ((count_includes==0)   || (paszIncludes != NULL)), count_includes  );
	SG_ARGCHECK_RETURN(  ((count_excludes==0)   || (paszExcludes != NULL)), count_excludes  );
	SG_ARGCHECK_RETURN(  ((bRecursive==SG_TRUE) || (count_items > 0)),      bRecursive      );	// if not recursive, require at least one item.

	// verify that any/all merge issues have been resolved.

	SG_ERR_CHECK(  SG_pendingtree__count_unresolved_wd_issues(pCtx, pPendingTree, &nrUnresolved)  );
	if (nrUnresolved > 0)
		SG_ERR_THROW(  SG_ERR_CANNOT_COMMIT_WITH_UNRESOLVED_ISSUES  );

	// this will THROW if the vfile didn't know what the parents are.

	SG_ERR_CHECK(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pva_wd_parents)  );
	SG_ERR_CHECK(  SG_varray__count(pCtx, pva_wd_parents, &nrParents)  );
	SG_ASSERT(  (nrParents >= 1)  );

	// disallow partial-commits where we have a merge.

	if (nrParents > 1)
	{
		SG_bool bPartialCommit = ((count_items > 0) || (count_includes > 0) || (count_excludes > 0) || (!bRecursive));
		if (bPartialCommit)
			SG_ERR_THROW(  SG_ERR_CANNOT_PARTIAL_COMMIT_AFTER_MERGE  );
	}

	SG_ERR_CHECK(  SG_committing__alloc(pCtx, &ptx, pPendingTree->pRepo, SG_DAGNUM__VERSION_CONTROL, pq, CSET_VERSION_1)  );

	for (k=0; k<nrParents; k++)
	{
		SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_wd_parents, k, &psz_hid_parent_k)  );
		SG_ERR_CHECK(  SG_committing__add_parent(pCtx, ptx, psz_hid_parent_k)  );
	}

	if (count_items > 0)
	{
		SG_ERR_CHECK(  sg_ptnode__maybe_mark_items_for_commit(pCtx,
															  pPendingTree,
															  pPathRelativeTo,
															  count_items, paszItems,
															  bRecursive,
															  paszIncludes, count_includes,
															  paszExcludes, count_excludes)  );

		/* check the tree to see if the user is trying to commit only
		 * the target half of a move operation.  If so, fail.  We
		 * cannot allow this case because it can result in a situation
		 * where the same object (identified by its gid) exists in two
		 * places in the tree at the same time. */
		SG_ERR_CHECK(  sg_ptnode__fail_if_any_split_moves(pCtx, pPendingTree->pSuperRoot, sg_PTNODE_TEMP_FLAG_COMMITTING)  );
	}
	else
	{
		SG_ERR_CHECK(  sg_ptnode__maybe_mark_ptnode_for_commit__recursive(pCtx,
																		  pPendingTree->pSuperRoot,
																		  SG_TRUE,	// we've already asserted that bRecursive is true
																		  paszIncludes, count_includes,
																		  paszExcludes, count_excludes)  );
	}

	// write all dirty blobs (both user files and treenodes) to the repo.
	// this sets pPendingTree->pSuperRoot->pszCurrentHid.  this should
	// silently allow any/all of the blobs to already exist in the repo.
	//
	// bSomethingCommitted means that there was something dirty that needed
	// to be written; if this is false, it does not mean that we can stop
	// working because it could be that we are doing a merge where there
	// were no actual tree changes but we are bringing together the leaves.
	SG_ERR_CHECK(  sg_ptnode__commit(pCtx, pPendingTree->pSuperRoot, NULL, ptx, &bSomethingCommitted)  );

	if (!bSomethingCommitted && (nrParents == 1))
	{
		// don't allow trivial changeset where the only change is
		// the parent.
		//
		// WARNING: we've probably trashed the pendingtree in memory.

		SG_ERR_THROW(  SG_ERR_NOTHING_TO_COMMIT  );
	}

	SG_ERR_CHECK(  SG_committing__set_root(pCtx, ptx, pPendingTree->pSuperRoot->pszCurrentHid)  );

	// write the actual changeset and dagnode.  this computes the resulting
	// changeset's HID.  this eats any duplicate changeset/dagnode warning
	// silently continues if the changeset/dagnode already exists in the
	// repo and returns handles to the existing ones.
	SG_ERR_CHECK(  SG_committing__end(pCtx, ptx, &pChangesetOriginal, &pDagnodeOriginal)  );
	ptx = NULL;

	//////////////////////////////////////////////////////////////////
	// refurbish the pendingtree after the commit.  this should strip out
	// any of the dirt, clear the unresolved issues, and update the parents
	// array to reflect that we have a new baseline.

	SG_ERR_CHECK(  sg_pendingtree__post_commit_fixup(pCtx, pPendingTree)  );

	// after a commit, we have a single new "baseline".  stuff it into the pendingtree.
	SG_ERR_CHECK(  SG_changeset__get_id_ref(pCtx, pChangesetOriginal, &psz_hid_new_changeset)  );
	SG_ERR_CHECK(  SG_pendingtree__set_single_wd_parent(pCtx, pPendingTree, psz_hid_new_changeset)  );

	// If we had a merge, we need to clear "issues" section in wd.json.
	// Our goal here is to forget all of them (without regard to whether
	// they were resolved or unresolved) because after the commit and we
	// update to the new baseline, these issues are not longer appropriate.
	SG_ERR_CHECK(  SG_pendingtree__clear_wd_issues(pCtx, pPendingTree)  );

	SG_ERR_CHECK(  SG_pendingtree__save(pCtx, pPendingTree)  );

	// Update any work-item-tracking issues.

	if(paszAssocs!=NULL && count_assocs>0)
	{
		sg_pendingtree__assoc(pCtx, pPendingTree->pRepo, pq, psz_hid_new_changeset, paszAssocs, count_assocs);
		if(SG_context__has_err(pCtx))
		{
			//todo: ?
			SG_log_current_error(pCtx);
			SG_context__err_reset(pCtx);
		}
	}

	if (ppdn)
		*ppdn = pDagnodeOriginal;
	else
		SG_DAGNODE_NULLFREE(pCtx, pDagnodeOriginal);

	SG_CHANGESET_NULLFREE(pCtx, pChangesetOriginal);
	return;

fail:
    if (ptx)
    {
        SG_ERR_IGNORE(  SG_committing__abort(pCtx, ptx)  );
    }
	SG_DAGNODE_NULLFREE(pCtx, pDagnodeOriginal);
	SG_CHANGESET_NULLFREE(pCtx, pChangesetOriginal);
	return;
}

//////////////////////////////////////////////////////////////////

static void sg_pendingtree__calc_backup_name(
	SG_context* pCtx,
	const SG_pathname* pPath,
    SG_pathname** ppResult
    )
{
    SG_pathname* pPathBackup = NULL;
    SG_pathname* pPathParent = NULL;
	SG_string* pstrName = NULL;
    SG_string* pstrCandidate = NULL;
    SG_uint32 i;
    SG_bool bExists = SG_FALSE;


    /* get the base name */
	SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPath, &pstrName)  );

    /* and the parent dir */
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathParent, pPath)  );
	SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathParent)  );

    /* init the candidate string */
    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstrCandidate)  );

    /* now loop to find a good name */
    for (i=0; i<100; i++)
    {
        SG_ERR_CHECK(  SG_string__clear(pCtx, pstrCandidate)  );
        /* TODO make sure we don't have any ending slash issues here */
        SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstrCandidate, "%s~sg%02d~", SG_string__sz(pstrName), i)  );
        SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathBackup, pPathParent, SG_string__sz(pstrCandidate))  );
        SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathBackup, &bExists, NULL, NULL)  );
        if (!bExists)
        {
            break;
        }
        SG_PATHNAME_NULLFREE(pCtx, pPathBackup);
    }

    SG_PATHNAME_NULLFREE(pCtx, pPathParent);
    SG_STRING_NULLFREE(pCtx, pstrName);
    SG_STRING_NULLFREE(pCtx, pstrCandidate);

    if (pPathBackup)
    {
        *ppResult = pPathBackup;
        return;
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_TOO_MANY_BACKUP_FILES  );
    }

fail:
    SG_STRING_NULLFREE(pCtx, pstrName);
    SG_STRING_NULLFREE(pCtx, pstrCandidate);
    SG_PATHNAME_NULLFREE(pCtx, pPathBackup);
}

//////////////////////////////////////////////////////////////////

#include "sg_pendingtree__private_log.h"
#include "sg_pendingtree__private_revert.h"
#include "sg_pendingtree__private_resolve_issues.h"
#include "sg_pendingtree__private_update_baseline.h"

//////////////////////////////////////////////////////////////////

void SG_pendingtree__free(SG_context* pCtx, SG_pendingtree* pThis)
{
	if (!pThis)
	{
		return;
	}

#if 0 && defined(DEBUG)
	// A crude check to see if a caller did a __set_wd_parents()
	// without a __save().  However, it is possible for us to get
	// here as part of a fail-block cleanup/throw, so we don't assert.
	if (pThis->b_pvh_wd__parents__dirty)
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "SG_pendingtree__free: with b_pvh_wd__parents__dirty\n")  );

	// A crude check to see if a caller did a __set_wd_issues()
	// without a __save().  However, it is possible for us to get
	// here as part of a fail-block cleanup/throw, so we don't assert.
	if (pThis->b_pvh_wd__issues__dirty)
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "SG_pendingtree__free: with b_pvh_wd__issues__dirty\n")  );

	// A crude check to see if a caller did a __set_wd_timestamps()
	// without a __save().  However, it is possible for us to get
	// here as part of a fail-block cleanup/throw, so we don't assert.
	if (pThis->b_pvh_wd__timestamps__dirty)
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "SG_pendingtree__free: with b_pvh_wd__timestamps__dirty\n")  );
#endif

	if (pThis->pvf_wd)
	{
		SG_ERR_IGNORE(  SG_vfile__abort(pCtx, &pThis->pvf_wd)  );
	}
	SG_VHASH_NULLFREE(pCtx, pThis->pvh_wd);

    SG_UTF8_CONVERTER_NULLFREE(pCtx, pThis->pConverter);
	SG_PTNODE_NULLFREE(pCtx, pThis->pSuperRoot);

    SG_STRING_NULLFREE(pCtx, pThis->pstrRepoDescriptorName);
    SG_VARRAY_NULLFREE(pCtx, pThis->pvaWarnings);
	SG_VARRAY_NULLFREE(pCtx, pThis->pvaActionLog);
	SG_PATHNAME_NULLFREE(pCtx, pThis->pPathWorkingDirectoryTop);
	SG_REPO_NULLFREE(pCtx, pThis->pRepo);
	SG_STRPOOL_NULLFREE(pCtx, pThis->pStrPool);
	
	SG_NULLFREE(pCtx, pThis);
}

void SG_pendingtree__get_repo(SG_UNUSED_PARAM(SG_context* pCtx),
        SG_pendingtree* pPendingTree,
        SG_repo** ppRepo
        )
{
	SG_UNUSED(  pCtx  );
    *ppRepo = pPendingTree->pRepo;
}

//////////////////////////////////////////////////////////////////

/**
 * Update the information in the tree-diff for a single tree-node-entry
 * using the state in the pending-tree.
 *
 * This is similar to sg_ptnode__status() in spirit.
 */
static void _sg_ptnode__annotate_treediff2(SG_context* pCtx, struct sg_ptnode * ptn,
											   SG_treediff2 * pTreeDiff,
											   SG_bool * pbDirty)
{
	SG_rbtree_iterator * pIterator = NULL;
	SG_diffstatus_flags dsFlags = SG_DIFFSTATUS_FLAGS__ZERO;
	const char * szParentGid_base;
	const char * szParentGid_pend;
	const char * szHidContent_base;
	const char * szHidContent_pend;
	const char * szHidXAttrs_base;
	const char * szHidXAttrs_pend;
	const char * szEntryName_base;
	const char * szEntryName_pend;
	SG_int64 attrBits_base, attrBits_pend;
	SG_bool bInvalidateFolderContentHid = SG_FALSE;

	SG_NULLARGCHECK_RETURN(ptn);

	// get the baseline and current values of these fields.
	// if they haven't changed, both will point to the same string.
	// these may return a null string if we don't have a value for
	// them (for example when we have an __ADDED entry).

	SG_ERR_CHECK(  sg_ptnode__get_old_name(pCtx, ptn,&szEntryName_base)  );
	SG_ERR_CHECK(  sg_ptnode__get_name(pCtx, ptn,&szEntryName_pend)  );

	// the content HIDs may or may not be set.  in some cases
	// we are lazy and don't compute the HID because we don't
	// need it.  in other case (such as a folder) we *can't*
	// compute it because is sg_treenode's job and we don't
	// necessarily have all of the information needed to do it.

	SG_ERR_CHECK(  sg_ptnode__get_old_hid(pCtx, ptn,&szHidContent_base)  );
	SG_ERR_CHECK(  sg_ptnode__get_hid(pCtx, ptn,&szHidContent_pend)  );

	// the XAttr HIDs may or may not be null because the entry
	// may or may not have XAttrs on disk (and or on this platform).

	SG_ERR_CHECK(  sg_ptnode__get_old_xattrs_hid(pCtx, ptn,&szHidXAttrs_base)  );
	SG_ERR_CHECK(  sg_ptnode__get_xattrs_hid(pCtx, ptn,&szHidXAttrs_pend)  );

	attrBits_pend = ptn->iCurrentAttributeBits;
	if (ptn->pBaselineEntry)
		SG_ERR_CHECK(  SG_treenode_entry__get_attribute_bits(pCtx, ptn->pBaselineEntry,&attrBits_base)  );
	else
		attrBits_base = attrBits_pend;
	
	szParentGid_pend = ((ptn->pCurrentParent) ? ptn->pCurrentParent->pszgid : NULL);
	szParentGid_base = ((ptn->pszgidMovedFrom) ? ptn->pszgidMovedFrom : szParentGid_pend);


	if (ptn->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
	{
		if (ptn->temp_flags & sg_PTNODE_TEMP_FLAG_IMPLICIT_DELETE)
			dsFlags |= SG_DIFFSTATUS_FLAGS__LOST;
		else
			dsFlags |= SG_DIFFSTATUS_FLAGS__DELETED;
	}

	if (ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
	{
		// this is the ghost of the move.  the node for the entry
		// in the from-directory.  it is not in the tree proper, but this
		// is a placeholder for where it was.
		//
		// we do *not* set a dsFlag for this because we don't want the
		// ghost in the tree-diff data structures.  it will get the version
		// of the entry in the to-directory (and __MOVED will be set).

#if 0
		// TODO remove this.
		SG_console(SG_CS_STDERR,"MOVED_AWAY: [gid %s][name_base %s][name_pend %s][cur %s]\n",
				   ptn->pszgid,
				   ((szEntryName_base) ? szEntryName_base : "(NULL)"),
				   ((szEntryName_pend) ? szEntryName_pend : "(NULL)"),
				   ((ptn->pszCurrentName) ? ptn->pszCurrentName : "(NULL)"));

#endif
		dsFlags = SG_DIFFSTATUS_FLAGS__ZERO;
		goto skip_all;
	}

	if (   !(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
	    && ptn->pszCurrentName
		&& ptn->pBaselineEntry)
	{
#if 0
		// TODO remove this.
		SG_console(SG_CS_STDERR,"XXXXX: [name_base %s][name_pend %s][cur %s]\n",
				   ((szEntryName_base) ? szEntryName_base : "(NULL)"),
				   ((szEntryName_pend) ? szEntryName_pend : "(NULL)"),
				   ((ptn->pszCurrentName) ? ptn->pszCurrentName : "(NULL)"));
#endif
		if (strcmp( ((szEntryName_base) ? szEntryName_base : ""),
					((szEntryName_pend) ? szEntryName_pend : "")) != 0)
			dsFlags |= SG_DIFFSTATUS_FLAGS__RENAMED;
	}

	if (ptn->pCurrentParent && !ptn->pBaselineEntry)
	{
		if (ptn->temp_flags & sg_PTNODE_TEMP_FLAG_IMPLICIT_ADD)
			dsFlags |= SG_DIFFSTATUS_FLAGS__FOUND;
		else
			dsFlags |= SG_DIFFSTATUS_FLAGS__ADDED;
	}

	if (ptn->pszgidMovedFrom)
	{
		dsFlags |= SG_DIFFSTATUS_FLAGS__MOVED;
	}

	if (ptn->pBaselineEntry &&  !(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED))
	{
		if (strcmp( ((szHidXAttrs_base) ? szHidXAttrs_base : ""),
					((szHidXAttrs_pend) ? szHidXAttrs_pend : "")) != 0)
			dsFlags |= SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS;
	}

	if (ptn->pBaselineEntry &&  !(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED))
	{
		if (attrBits_base != attrBits_pend)
			dsFlags |= SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS;
	}

	if (ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		SG_bool bChildrenDirty = SG_FALSE;

		if (ptn->prbItems)
		{
			struct sg_ptnode * ptnChild;
			const char * pszgidChild;
			SG_bool bChildDirty;
			SG_bool b;

			// dive into the files/sub-directories within this directory and let them
			// annotate the tree-diff.  because we dive first and wait to see if any
			// of them were dirty before we annotate the entry for this directory, the
			// tree-diff will get our children before it gets our data.  this may cause
			// tree-diff a little problem because it normally gets data completely
			// top-down.
			//
			// the reason we defer the annotation of this directory is that we only
			// want to annotate dirty nodes -- we still want to try to keep the
			// tree-diff code from loading the entire tree if it doesn't have to
			// (we want that equal-content-hid-short-circuit to keep working).

			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, ptn->prbItems, &b, &pszgidChild, (void**) &ptnChild)  );
			while (b)
			{
				SG_ERR_CHECK(  _sg_ptnode__annotate_treediff2(pCtx, ptnChild,pTreeDiff,&bChildDirty)  );
				bChildrenDirty |= bChildDirty;

				// if this child is a move-ghost, we also need to mark the folder dirty.
				// we don't get this automatically from the recursive call because ghosts
				// don't set any flags.

				bChildrenDirty |= ((ptnChild->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY) != 0);

				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszgidChild, (void**) &ptnChild)  );
			}
			SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);

			// if one or more children changed, the Content-HID of this directory will change.
			// if none of the other MASK_ME bits are set, force the _MODIFIED bit.  this is a
			// bit contrived -- because the status/treediff code doesn't usually look at a _MODIFIED
			// state on a folder -- but it ensures that they get the message that it is dirty.

			if (bChildrenDirty && ((dsFlags & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE) == 0))
			{
				dsFlags |= SG_DIFFSTATUS_FLAGS__MODIFIED;
				bInvalidateFolderContentHid = SG_TRUE;
			}
		}
	}
	else if (ptn->type == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
	{
		if (ptn->pBaselineEntry && !(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY))
		{
			SG_bool bFileIsDirty = SG_FALSE;

			SG_ERR_CHECK(  sg_ptnode__is_file_dirty(pCtx, ptn, &bFileIsDirty)  );
			if (bFileIsDirty)
				dsFlags |= SG_DIFFSTATUS_FLAGS__MODIFIED;
		}
	}
	else if (ptn->type == SG_TREENODEENTRY_TYPE_SYMLINK)
	{
		if (ptn->pBaselineEntry && !(ptn->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY))
		{
			SG_bool bFileIsDirty = SG_FALSE;

            /* TODO is_file_dirty happens to work for symlinks in this case as
             * well.  But this is a bit risky.  What if is_file_dirty changes?
             * */
			SG_ERR_CHECK(  sg_ptnode__is_file_dirty(pCtx, ptn, &bFileIsDirty)  );

			if (bFileIsDirty)
				dsFlags |= SG_DIFFSTATUS_FLAGS__MODIFIED;
		}
	}
	else
	{
		SG_ERR_THROW_RETURN( SG_ERR_INVALID_PENDINGTREE_OBJECT_TYPE );
	}

	// if this node is dirty, tell the tree-diff about it.

	if (dsFlags)
	{
		// because we get called with the super-root (above "@/"),
		// we need to skip that one.

		if (ptn->pszgid)
		{
			// if we are a folder and anything changed within it, we invalidate the
			// content HID because we cannot compute it here.  the treediff code
			// would like to have it, but can cope.

			if (bInvalidateFolderContentHid)
				szHidContent_pend = NULL;

			// TODO for files/symlinks compute szHidContent_pend if not set.

			SG_ERR_CHECK(  SG_treediff2__pendingtree_annotate_cb(pCtx, pTreeDiff,dsFlags,
																 ptn->pszgid,
																 szParentGid_base,szParentGid_pend,
																 ptn->type,
																 szEntryName_base,szEntryName_pend,
																 szHidContent_base,szHidContent_pend,
																 szHidXAttrs_base,szHidXAttrs_pend,
																 attrBits_base,attrBits_pend)  );
		}
	}


skip_all:
	*pbDirty = (dsFlags != SG_DIFFSTATUS_FLAGS__ZERO);
	SG_ASSERT( !SG_context__has_err(pCtx) );
	return;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__get_wd_parents__ref(SG_context * pCtx,
										 SG_pendingtree * pPendingTree,
										 const SG_varray ** ppva_wd_parents)
{
	SG_varray * pvaSrc;

	SG_NULLARGCHECK_RETURN(pPendingTree );
	SG_NULLARGCHECK_RETURN(ppva_wd_parents);

	SG_ASSERT(  (pPendingTree->pvh_wd)  );		// __load_cached_or_baseline() should have been called

	// this will THROW if the vfile didn't know what the parents are.
	SG_ERR_CHECK_RETURN(  SG_vhash__get__varray(pCtx, pPendingTree->pvh_wd, "parents", &pvaSrc)  );

	*ppva_wd_parents = pvaSrc;					// you can borrow our copy, but don't change it.
}

void SG_pendingtree__get_wd_parents(SG_context * pCtx,
									SG_pendingtree * pPendingTree,
									SG_varray ** ppva_wd_parents)
{
	SG_varray * pvaAllocated = NULL;
	const SG_varray * pvaSrc = NULL;
	const char * pszHid_k = NULL;
	SG_uint32 k, nr;

	SG_NULLARGCHECK_RETURN(pPendingTree );
	SG_NULLARGCHECK_RETURN(ppva_wd_parents);

	SG_VARRAY__ALLOC(pCtx, &pvaAllocated);

	SG_ERR_CHECK(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pvaSrc)  );
	SG_ERR_CHECK(  SG_varray__count(pCtx, pvaSrc, &nr)  );
	for (k=0; k<nr; k++)
	{
		SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pvaSrc, k, &pszHid_k)  );
		SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pvaAllocated, pszHid_k)  );
	}

	*ppva_wd_parents = pvaAllocated;
	return;

fail:
	SG_VARRAY_NULLFREE(pCtx, pvaAllocated);
}

void SG_pendingtree__set_wd_parents(SG_context * pCtx,
									SG_pendingtree * pPendingTree,
									SG_varray ** ppva_new_wd_parents)
{
	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULL_PP_CHECK_RETURN(ppva_new_wd_parents);

	SG_ASSERT(  (pPendingTree->pvh_wd)  );

#if TRACE_PENDINGTREE
	{
		SG_uint32 k, nrParents;
		const char * psz_hid_parent_k;

		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "SG_pendingtree__set_wd_parents:\n")  );
		SG_ERR_IGNORE(  SG_varray__count(pCtx, *ppva_new_wd_parents, &nrParents)  );
		for (k=0; k<nrParents; k++)
		{
			SG_ERR_IGNORE(  SG_varray__get__sz(pCtx, *ppva_new_wd_parents, k, &psz_hid_parent_k)  );
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "        [%d]: %s\n", k, psz_hid_parent_k)  );
		}
	}
#endif
	// steal the callers VArray and stuff it into the VHash.

	SG_ERR_CHECK_RETURN(  SG_vhash__update__varray(pCtx, pPendingTree->pvh_wd, "parents", ppva_new_wd_parents)  );

#if defined(DEBUG)
	pPendingTree->b_pvh_wd__parents__dirty = SG_TRUE;
#endif
}

void SG_pendingtree__set_single_wd_parent(SG_context * pCtx,
										  SG_pendingtree * pPendingTree,
										  const char * psz_hid_parent)
{
	SG_varray * pva = NULL;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(psz_hid_parent);

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );
	SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, psz_hid_parent)  );
	SG_ERR_CHECK(  SG_pendingtree__set_wd_parents(pCtx, pPendingTree, &pva)  );		// this steals our varray

	return;

fail:
	SG_VARRAY_NULLFREE(pCtx, pva);
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__clear_wd_issues(SG_context * pCtx,
									 SG_pendingtree * pPendingTree)
{
	// if the "issues" section is present, remove it.

	SG_vhash__remove(pCtx, pPendingTree->pvh_wd, "issues");
	if (SG_context__has_err(pCtx))
	{
		if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))
			SG_context__err_reset(pCtx);
		else
			SG_ERR_RETHROW_RETURN;
	}

#if defined(DEBUG)
	pPendingTree->b_pvh_wd__issues__dirty = SG_TRUE;
#endif
}

void SG_pendingtree__set_wd_issues(SG_context * pCtx,
								   SG_pendingtree * pPendingTree,
								   SG_varray ** ppva_new_wd_issues)
{
	SG_uint32 count;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	// ppva_new_wd_issues can be null or refer to a null vhash.

	SG_ASSERT(  (pPendingTree->pvh_wd)  );

	SG_ERR_CHECK_RETURN(  SG_pendingtree__clear_wd_issues(pCtx, pPendingTree)  );

	if (!ppva_new_wd_issues)
		return;

	if (!*ppva_new_wd_issues)
		return;

	SG_ERR_CHECK_RETURN(  SG_varray__count(pCtx, (*ppva_new_wd_issues), &count)  );
	if (count > 0)
		SG_ERR_CHECK_RETURN(  SG_vhash__update__varray(pCtx, pPendingTree->pvh_wd, "issues", ppva_new_wd_issues)  );	// this steals our caller's varray
	else
		SG_VARRAY_NULLFREE(pCtx, (*ppva_new_wd_issues));	// this steals our caller's varray

#if defined(DEBUG)
	pPendingTree->b_pvh_wd__issues__dirty = SG_TRUE;
#endif
}

void SG_pendingtree__get_wd_issues__ref(SG_context * pCtx,
										SG_pendingtree * pPendingTree,
										SG_bool * pbHaveIssues,
										const SG_varray ** ppva_wd_issues)
{
	SG_bool bHaveIssues = SG_FALSE;
	SG_varray * pva = NULL;			// we do not own this

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(pbHaveIssues);
	SG_NULLARGCHECK_RETURN(ppva_wd_issues);

	SG_ASSERT(  (pPendingTree->pvh_wd)  );

	// return a read-only pointer to the in-memory version of the "issues"
	// section in wd.json (if it exists).
	//
	// if there is no "issues" section or if it points to a null vhash,
	// assume that we have no issues to report.

	SG_vhash__get__varray(pCtx, pPendingTree->pvh_wd, "issues", &pva);
	if (SG_context__has_err(pCtx))
	{
		if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))
			SG_context__err_reset(pCtx);
		else
			SG_ERR_RETHROW_RETURN;
	}
	else
	{
		if (pva)
		{
			SG_uint32 count;

			SG_ERR_CHECK_RETURN(  SG_varray__count(pCtx, pva, &count)  );
			bHaveIssues = (count > 0);
		}
	}

	*pbHaveIssues = bHaveIssues;
	*ppva_wd_issues = pva;
}

void SG_pendingtree__set_wd_issue_status__dont_save_pendingtree(SG_context * pCtx,
																SG_pendingtree * pPendingTree,
																const SG_vhash * pvhIssue,
																SG_pendingtree_wd_issue_status status)
{
	// update the pendingtree.issues[nr].status directly (without copying the issues varray).
	//
	// only reason we need this rather than letting the caller just update the pvhIssue
	// directly is so that we get the dirty bit set here.

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(pvhIssue);

	SG_ASSERT(  (pPendingTree->pvh_wd)  );

	SG_ERR_CHECK_RETURN(  SG_vhash__update__int64(pCtx, (SG_vhash *)pvhIssue, "status", (SG_int64)status)  );

#if defined(DEBUG)
	pPendingTree->b_pvh_wd__issues__dirty = SG_TRUE;
#endif
}

void SG_pendingtree__set_wd_issue_status(SG_context * pCtx,
										 SG_pendingtree * pPendingTree,
										 const SG_vhash * pvhIssue,
										 SG_pendingtree_wd_issue_status status)
{
	SG_ERR_CHECK_RETURN(  SG_pendingtree__set_wd_issue_status__dont_save_pendingtree(pCtx, pPendingTree, pvhIssue, status)  );
	SG_ERR_CHECK_RETURN(  SG_pendingtree__save(pCtx, pPendingTree)  );
}

void SG_pendingtree__count_unresolved_wd_issues(SG_context * pCtx,
												SG_pendingtree * pPendingTree,
												SG_uint32 * pNrUnresolved)
{
	SG_varray * pvaIssues;
	SG_uint32 nrUnresolved = 0;
	SG_bool bHasIssues;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(pNrUnresolved);

	SG_ASSERT(  (pPendingTree->pvh_wd)  );

	SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, pPendingTree->pvh_wd, "issues", &bHasIssues)  );
	if (bHasIssues)
	{
		SG_vhash * pvhIssue;
		SG_uint32 k, count;

		SG_ERR_CHECK_RETURN(  SG_vhash__get__varray(pCtx, pPendingTree->pvh_wd, "issues", &pvaIssues)  );
		SG_ERR_CHECK_RETURN(  SG_varray__count(pCtx, pvaIssues, &count)  );
		if (count > 0)
		{
			for (k=0; k<count; k++)
			{
				SG_int64 s;
				SG_pendingtree_wd_issue_status status;

				SG_ERR_CHECK_RETURN(  SG_varray__get__vhash(pCtx, pvaIssues, k, &pvhIssue)  );
				SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, pvhIssue, "status", &s)  );
				status = (SG_pendingtree_wd_issue_status)s;

				if ((status & SG_ISSUE_STATUS__MARKED_RESOLVED) == 0)
					nrUnresolved++;
			}
		}
	}

	*pNrUnresolved = nrUnresolved;
}

void SG_pendingtree__set_wd_issue_plan_step_status__dont_save_pendingtree(SG_context * pCtx,
																		  SG_pendingtree * pPendingTree,
																		  const SG_vhash * pvhStep,
																		  SG_mrg_automerge_result r)
{
	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(pvhStep);

	SG_ASSERT(  (pPendingTree->pvh_wd)  );
	
	SG_ERR_CHECK_RETURN(  SG_vhash__update__int64(pCtx, (SG_vhash *)pvhStep, "status", (SG_int64)r)  );

#if defined(DEBUG)
	pPendingTree->b_pvh_wd__issues__dirty = SG_TRUE;
#endif
}

void SG_pendingtree__find_wd_issue_by_gid(SG_context * pCtx,
										  SG_pendingtree * pPendingTree,
										  const char * pszGidToFind,
										  SG_bool * pbFound,
										  const SG_vhash ** ppvhIssue)
{
	SG_bool bHaveIssues = SG_FALSE;
	const SG_varray * pvaIssues;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGidToFind);
	SG_NULLARGCHECK_RETURN(pbFound);
	// ppvhIssue is optional

	SG_ERR_CHECK(  SG_pendingtree__get_wd_issues__ref(pCtx, pPendingTree, &bHaveIssues, &pvaIssues)  );
	if (bHaveIssues)
	{
		SG_uint32 k, nrIssues;

		SG_ERR_CHECK(  SG_varray__count(pCtx, pvaIssues, &nrIssues)  );
		for (k=0; k<nrIssues; k++)
		{
			const SG_vhash * pvhIssue_k;
			const char * pszGid_k;

			SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pvaIssues, k, (SG_vhash **)&pvhIssue_k)  );
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhIssue_k, "gid", &pszGid_k)  );
			if (strcmp(pszGid_k, pszGidToFind) == 0)
			{
				*pbFound = SG_TRUE;
				if (ppvhIssue)
					*ppvhIssue = pvhIssue_k;
				return;
			}
		}
	}

	*pbFound = SG_FALSE;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * do the actual work of computing a tree-diff using the pending-tree.
 *
 * you own the returned tree-diff.
 *
 * WARNING: we call scan_dir() but DO NOT use any of the --include/--exclude
 * file-spec stuff.  Diffs should be filtered afterwards.
 */
static void _sg_pendingtree__do_diff(SG_context* pCtx, SG_pendingtree * pPendingTree,
									 SG_bool bUnloadCleanObjects,
									 const char * szHidArbitraryCSet,		/* optional */
									 SG_treediff2 * pTreeDiff_cache_owner,	/* optional */
									 SG_treediff2 ** ppTreeDiff)
{
	SG_treediff2 * pTreeDiff = NULL;
	const SG_varray * pva_wd_parents = NULL;
	const char * psz_hid_baseline = NULL;
	SG_uint32 nrParents;
	SG_bool bDirty = SG_FALSE;
	SG_bool bDirty2 = SG_FALSE;

	SG_bool bAddRemove = SG_TRUE;
	SG_bool bMarkImplicit = SG_TRUE;
	SG_bool bRecursive = SG_TRUE;

	// read the complete tree into memory (read all of the treenodes) into ptnodes and walk the
	// working-directory and make any modifications that we detect.

	SG_ERR_CHECK(  sg_pendingtree__scan_dir(pCtx,
											pPendingTree,
											pPendingTree->pPathWorkingDirectoryTop,
											bAddRemove,
											bMarkImplicit,
											bRecursive,
											NULL,
											NULL, 0,
											NULL, 0,
											NULL, 0)  );

	if (bUnloadCleanObjects)
	{
		// strip out the clean objects from the ptnodes.

		SG_ERR_CHECK(  sg_ptnode__unload_dir_entries_if_not_dirty(pCtx, pPendingTree->pSuperRoot, &bDirty)  );
	}

	// get the HID of the baseline.

	SG_ERR_CHECK(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pva_wd_parents)  );
	SG_ERR_CHECK(  SG_varray__count(pCtx, pva_wd_parents, &nrParents)  );
	SG_ASSERT(  (nrParents >= 1)  );
	SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_wd_parents, 0, &psz_hid_baseline)  );

	// compute tree-diff as "baseline vs working-directory" or "arbitrary-cset vs working-directory".

	SG_ERR_CHECK(  SG_treediff2__alloc__shared(pCtx, pPendingTree->pRepo,pTreeDiff_cache_owner,&pTreeDiff)  );
	SG_ERR_CHECK(  SG_treediff2__begin_pendingtree_diff(pCtx, pTreeDiff,
														szHidArbitraryCSet,
														psz_hid_baseline,
														pPendingTree->pPathWorkingDirectoryTop)  );
	SG_ERR_CHECK(  _sg_ptnode__annotate_treediff2(pCtx, pPendingTree->pSuperRoot,pTreeDiff,&bDirty2)  );
	SG_ERR_CHECK(  SG_treediff2__finish_pendingtree_diff(pCtx, pTreeDiff)  );

	*ppTreeDiff = pTreeDiff;

	return;

fail:
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__do_diff__keep_clean_objects(SG_context* pCtx, SG_pendingtree * pPendingTree,
														 const char * szHidArbitraryCSet,		/* optional */
														 SG_treediff2 * pTreeDiff_cache_owner,	/* optional */
														 SG_treediff2 ** ppTreeDiff)
{
	SG_ERR_CHECK_RETURN(  _sg_pendingtree__do_diff(pCtx, pPendingTree,
												   SG_FALSE,
												   szHidArbitraryCSet,
												   pTreeDiff_cache_owner,
												   ppTreeDiff)  );
}

void SG_pendingtree__do_diff__keep_clean_objects__reload(SG_context* pCtx, SG_pendingtree * pPendingTree,
														 const char * szHidArbitraryCSet,		/* optional */
														 SG_treediff2 * pTreeDiff_cache_owner,	/* optional */
														 SG_treediff2 ** ppTreeDiff)
{
	SG_ERR_CHECK_RETURN(  SG_pendingtree__do_diff__keep_clean_objects(pCtx, pPendingTree,
																	  szHidArbitraryCSet,
																	  pTreeDiff_cache_owner,
																	  ppTreeDiff)  );
	SG_ERR_CHECK_RETURN(  SG_pendingtree__reload_pendingtree_from_vfile(pCtx, pPendingTree)  );
}

static void _sg_pendingtree__do_diff__unload_clean_objects(SG_context* pCtx, SG_pendingtree * pPendingTree,
														   const char * szHidArbitraryCSet,		/* optional */
														   SG_treediff2 * pTreeDiff_cache_owner,	/* optional */
														   SG_treediff2 ** ppTreeDiff)
{
	SG_ERR_CHECK_RETURN(  _sg_pendingtree__do_diff(pCtx, pPendingTree,
												   SG_TRUE,
												   szHidArbitraryCSet,
												   pTreeDiff_cache_owner,
												   ppTreeDiff)  );
}

//////////////////////////////////////////////////////////////////

/**
 * take 0, 1, or 2 cset HIDs and create a SG_treediff2 that reflects
 * the changes (committed or pending) between
 * [] 2 csets or
 * [] between 1 cset and the working directory or
 * [] between the baseline and the working directory.
 *
 * WARNING: we DO NOT take any of the --include/--exclude file-spec stuff.
 * We only compute status on a complete view of the tree and not on pre-filtered
 * views of the tree.  (We MIGHT be able to in the future, but not now.)  When we
 * are called with 0 or 1 CSET HID (and need to reference the WD/pendingtree),
 * we cause a sg_pendingtree__scan_dir() to occur; if that is filtered, then when
 * the diff gets computed, we'll be looking at an un-filtered treenode vs a filtered
 * WD which may cause us to report the filtered-out things as missing.  Furthermore,
 * when we are comparing 2 CSET HIDs, we won't have any filtering.  So in all 3
 * cases we would need to hack the treenodes to match the filtering.  SO INSTEAD,
 * we do not do any filtering here while computing the diff/status.  Our caller can
 * use SG_treediff2__filter__using_file_spec() do the dirty work on our results.
 */
void SG_pendingtree__diff_or_status(SG_context * pCtx, SG_pendingtree * pPendingTree,
									const char * psz_cset_1,
									const char * psz_cset_2,
									SG_treediff2 ** ppTreeDiff)
{
	SG_treediff2 * pTreeDiff = NULL;
	SG_bool bHave1 = ((psz_cset_1 != NULL) && (*psz_cset_1 != 0));
	SG_bool bHave2 = ((psz_cset_2 != NULL) && (*psz_cset_2 != 0));

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(ppTreeDiff);

	if (!bHave1)
	{
		SG_ASSERT(  (!bHave2)  );

		// baseline vs pendingtree (WD)
		SG_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree, NULL, &pTreeDiff)  );
	}
	else if (!bHave2)
	{
		// arbitrary-cset vs pendingtree (WD)
		SG_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree, psz_cset_1, &pTreeDiff)  );
	}
	else
	{
		// cset vs cset
		SG_ERR_CHECK(  SG_treediff2__alloc(pCtx, pPendingTree->pRepo,&pTreeDiff)  );

		// TODO 4/14/10 BUGBUG a cset-vs-cset diff does not need a WD.  we should not have
		// TODO 4/14/10 BUGBUG have to set one here -- and nothing in the diff should use it.
		SG_ERR_CHECK(  SG_treediff2__set_wd_top(pCtx, pTreeDiff,pPendingTree->pPathWorkingDirectoryTop)  );
		SG_ERR_CHECK(  SG_treediff2__compare_cset_vs_cset(pCtx, pTreeDiff, psz_cset_1, psz_cset_2)  );
	}

	*ppTreeDiff = pTreeDiff;
	return;

fail:
    SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
}


void SG_pendingtree__diff__shared(SG_context* pCtx, SG_pendingtree * pPendingTree,
								  const char * szHidArbitraryCSet,		/* optional */
								  SG_treediff2 * pTreeDiff_cache_owner,	/* optional */
								  SG_treediff2 ** ppTreeDiff)
{
	SG_treediff2 * pTreeDiff = NULL;

	SG_NULLARGCHECK_RETURN(  ppTreeDiff  );

	SG_ERR_CHECK(  _sg_pendingtree__do_diff__unload_clean_objects(pCtx, pPendingTree, szHidArbitraryCSet, pTreeDiff_cache_owner, &pTreeDiff)  );

	/* we don't change the saved pending changeset here. */
	SG_PTNODE_NULLFREE(pCtx, pPendingTree->pSuperRoot);

	*ppTreeDiff = pTreeDiff;

	return;

fail:
	// TODO see note [X] at top of file
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
}

void SG_pendingtree__diff(SG_context* pCtx, SG_pendingtree * pPendingTree,
						  const char * szHidArbitraryCSet,		/* optional */
						  SG_treediff2 ** ppTreeDiff)
{
	SG_ERR_CHECK_RETURN(  SG_pendingtree__diff__shared(pCtx, pPendingTree,
													   szHidArbitraryCSet,
													   NULL,
													   ppTreeDiff)  );
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__get_gid_from_local_path(SG_context* pCtx, SG_pendingtree* pTree,
											 SG_pathname* pLocalPathPathname, char** ppszgid)
{
	SG_pathname* ppnParentDir = NULL;
	struct sg_ptnode* ptnParentDir = NULL;
	struct sg_ptnode* ptnItem = NULL;
	SG_string* pstrItemName = NULL;

	SG_NULLARGCHECK_RETURN(pTree);
	SG_NULLARGCHECK_RETURN(pLocalPathPathname);

	/* We want the parent directory of this item.  So copy the path and remove the last component */
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &ppnParentDir, pLocalPathPathname)  );
	SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, ppnParentDir)  );

	/* Find the ptnodes */
	SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pTree, ppnParentDir, SG_TRUE, &ptnParentDir)  );
    if (!ptnParentDir)
    {
        SG_ERR_THROW(  SG_ERR_ITEM_NOT_UNDER_VERSION_CONTROL  );
    }

	/* Get the base name from the original path */
	SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pLocalPathPathname, &pstrItemName)  );

	/* leave the item in both places, one as a phantom */

	SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__active(pCtx, ptnParentDir, SG_string__sz(pstrItemName), &ptnItem)  );
    if (!ptnItem)
    {
        SG_ERR_THROW(  SG_ERR_ITEM_NOT_UNDER_VERSION_CONTROL  );
    }

	SG_ERR_CHECK(  SG_STRDUP(pCtx, ptnItem->pszgid, ppszgid)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, ppnParentDir);
	SG_STRING_NULLFREE(pCtx, pstrItemName);
	return;
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__get_warnings(SG_context* pCtx,
								  SG_pendingtree* pPendingTree,
								  SG_varray** ppResult)
{
    SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(ppResult);

    *ppResult = pPendingTree->pvaWarnings;
}

void SG_pendingtree__get_port_settings(SG_context * pCtx,
									   SG_pendingtree * pPendingTree,
									   SG_utf8_converter ** ppConverter,
									   SG_varray ** ppvaWarnings,
									   SG_portability_flags * pPortMask,
									   SG_bool * pbIgnoreWarnings)
{
	SG_NULLARGCHECK_RETURN(pPendingTree);

	if (ppConverter)
		*ppConverter = pPendingTree->pConverter;

	if (ppvaWarnings)
		*ppvaWarnings = pPendingTree->pvaWarnings;

	if (pPortMask)
		*pPortMask = pPendingTree->portMask;

	if (pbIgnoreWarnings)
		*pbIgnoreWarnings = pPendingTree->bIgnoreWarnings;
}

//////////////////////////////////////////////////////////////////

/**
 * build the pendingtree and dump it to a treediff and an enhanced vhash (like
 * we write to the vfile, but also include extra debug info).  we export both
 * of these at the same time so that the transient GIDs created for FOUND entries
 * match.
 */
void SG_pendingtree_debug__export(SG_context * pCtx, SG_pathname * pPathWorkingDir,
								  SG_bool bProcessIgnores,		// if true do filtering using "ignores" localsettings
								  SG_treediff2 ** ppTreeDiff,
								  SG_vhash ** ppvhExport)
{
	SG_pendingtree * pPendingTree = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_vhash* pvh = NULL;
	SG_bool bIgnoreIgnores = !bProcessIgnores;

	SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx,pPathWorkingDir,SG_TRUE,&pPendingTree)  );

	// scan directory.  strip out clean stuff.  build treediff of baseline-vs-working-directory

	SG_ERR_CHECK(  _sg_pendingtree__do_diff__unload_clean_objects(pCtx,pPendingTree, NULL, NULL, &pTreeDiff)  );
	SG_ERR_CHECK(  SG_treediff2__file_spec_filter(pCtx, pTreeDiff,
												  0, NULL,
												  SG_TRUE,
												  NULL, 0,
												  NULL, 0,
												  bIgnoreIgnores)  );

	// convert ptnodes into full vhash (with debug info)

	if (pPendingTree->pSuperRoot)
	{
		pPendingTree->bDebugExport = SG_TRUE;
		SG_ERR_CHECK(  sg_pendingtree__to_vhash(pCtx, pPendingTree, &pvh)  );

		/* we don't change the saved pending changeset here. */
		SG_PTNODE_NULLFREE(pCtx, pPendingTree->pSuperRoot);
	}

	SG_PENDINGTREE_NULLFREE(pCtx,pPendingTree);

	*ppTreeDiff = pTreeDiff;
	*ppvhExport = pvh;
	return;

fail:
	// TODO see note [X] at top of file
	SG_PENDINGTREE_NULLFREE(pCtx,pPendingTree);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_VHASH_NULLFREE(pCtx, pvh);
}

#if defined(DEBUG)
/**
 * dump an existing pendingtree to the console.
 */
void SG_pendingtree_debug__dump_existing_to_console(SG_context * pCtx, SG_pendingtree * pPendingTree)
{
	SG_vhash * pvh = NULL;
	SG_bool bDebug = pPendingTree->bDebugExport;

	SG_NULLARGCHECK_RETURN(pPendingTree);

	pPendingTree->bDebugExport = SG_TRUE;
	if (pPendingTree->pSuperRoot)
		SG_ERR_IGNORE(  sg_pendingtree__to_vhash(pCtx,pPendingTree,&pvh)  );
	pPendingTree->bDebugExport = bDebug;

	if (pvh)
		SG_ERR_IGNORE(  SG_vhash_debug__dump_to_console(pCtx,pvh)  );

	SG_VHASH_NULLFREE(pCtx, pvh);
}
#endif

void SG_pendingtree__get_gids_for_paths(SG_context * pCtx, SG_pendingtree * pPendingTree, SG_uint32 count_args, const char ** paszArgs,  SG_stringarray* ppReturnGIDs, SG_bool * bRootIsOneOfThem)
{
	SG_pathname * pPathName = NULL;
	struct sg_ptnode * ptnCurrentItem = NULL;
	struct sg_ptnode * ptn = NULL;
	SG_pathname * pPathDir = NULL;
	SG_string *pstrName = NULL;
	SG_uint32 i = 0;

	*bRootIsOneOfThem = SG_FALSE;
	SG_ERR_CHECK(  SG_pathname__alloc(pCtx, &pPathName)  );

	for (i = 0; i < count_args; i++)
	{
		/* First get a pathname for the item */
		SG_ERR_CHECK(  SG_pathname__set__from_sz(pCtx, pPathName, paszArgs[i])  );
		SG_ERR_IGNORE(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathName, SG_TRUE, &ptnCurrentItem)  );
		if (ptnCurrentItem == NULL)
		{
			/* We want the parent directory of this item.  So copy the path and remove the last component */
			SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathDir, pPathName)  );
			SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathDir)  );

			/* Find the directory's ptnode */
			SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathDir, SG_TRUE, &ptn)  );

			/* Get the base name from the original path */
			SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathName, &pstrName)  );

			/* Find the ptnode */
			SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__active(pCtx, ptn, SG_string__sz(pstrName), &ptnCurrentItem)  );
			if (SG_string__length_in_bytes(pstrName) == 0)
				ptnCurrentItem = ptn;
			if (!ptnCurrentItem)
				SG_ERR_THROW2(  SG_ERR_NOT_FOUND,
								(pCtx,"Looking for '%s' in the pending tree node for '%s'",
								 SG_string__sz(pstrName),
								 SG_pathname__sz(pPathDir))  );
			SG_PATHNAME_NULLFREE(pCtx, pPathDir);
			SG_STRING_NULLFREE(pCtx, pstrName);
		}
		if (ptnCurrentItem->pCurrentParent->pCurrentParent == NULL)
			*bRootIsOneOfThem = SG_TRUE;
		//SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathName, SG_TRUE, &ptnCurrentItem)  );
		SG_ERR_CHECK(  SG_stringarray__add(pCtx, ppReturnGIDs, ptnCurrentItem->pszgid)  );
	}
	SG_PATHNAME_NULLFREE(pCtx, pPathName);
	return;
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathName);
	return;

}

//////////////////////////////////////////////////////////////////

void SG_pendingtree_debug__get_wd_dot_json_stats(SG_context * pCtx,
												 const SG_pathname * pPathWorkingDir,
												 SG_uint32 * pNrParents,
												 SG_bool * pbHavePending)
{
	SG_pathname * pPathActualTop = NULL;
	SG_pathname * pPath = NULL;
	SG_string * pStrRepoDescriptorName = NULL;
	char * pszGidAnchorDirectory = NULL;
	SG_vhash * pvh_wd = NULL;
	SG_varray * pva_parents;	// we do not own this
	SG_bool bFound;
	
	SG_ERR_CHECK(  SG_workingdir__find_mapping(pCtx, pPathWorkingDir, &pPathActualTop, &pStrRepoDescriptorName, &pszGidAnchorDirectory)  );
	SG_ERR_CHECK(  SG_workingdir__get_drawer_path(pCtx, pPathActualTop, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath, "wd.json")  );
	SG_ERR_CHECK(  SG_vfile__slurp(pCtx, pPath, &pvh_wd)  );

	*pNrParents = 0;
	*pbHavePending = SG_FALSE;

	if (!pvh_wd)
		goto done;

	// "pending" is only present when there is dirt.
	SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_wd, "pending", pbHavePending)  );

	// "parents" should always be present.  (return zero if not)
	SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_wd, "parents", &bFound)  );
	if (bFound)
	{
		SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_wd, "parents", &pva_parents)  );
		SG_ERR_CHECK(  SG_varray__count(pCtx, pva_parents, pNrParents)  );
	}

done:
fail:
	SG_VHASH_NULLFREE(pCtx, pvh_wd);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_PATHNAME_NULLFREE(pCtx, pPathActualTop);
	SG_STRING_NULLFREE(pCtx, pStrRepoDescriptorName);
	SG_NULLFREE(pCtx, pszGidAnchorDirectory);
}

//////////////////////////////////////////////////////////////////

/**
 * HACK Un-add an entry from the pendingtree BUT DO NOT REMOVE IT FROM DISK.
 * HACK This is like "vv revert foo" following a "vv add foo", but
 * HACK without all the baggage of the full REVERT mechanism.  I need this
 * HACK routine in the middle of the MERGE code to get rid of a temporary
 * HACK directory.  As such, I have lots of open changes in the in-memory
 * HACK pendingtree and since the REVERT code assumes that it is in complete
 * HACK control of the pendingtree, I don't want to have to save my dirty
 * HACK version so that REVERT can load it and do one operation.
 */
void SG_pendingtree__hack__unadd_dont_save_pendingtree(SG_context * pCtx, SG_pendingtree * pPendingTree, const char * pszGid, const SG_pathname * pPath_Input)
{
	SG_pathname * pPathDir = NULL;
	SG_string * pStringEntryname = NULL;
	struct sg_ptnode * ptnDir;
	struct sg_ptnode * ptnItem;
	struct sg_ptnode * ptnItem2;

	// split input pathname into <dir> / <entryname>.
	// get the ptnode of <dir>.
	// get the ptnode for the item itself.

	SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPath_Input, &pStringEntryname)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathDir, pPath_Input)  );
	SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathDir)  );
	SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathDir, SG_FALSE, &ptnDir)  );
	SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__active(pCtx, ptnDir, SG_string__sz(pStringEntryname), &ptnItem)  );

	if (!ptnItem)
		SG_ERR_THROW2(  SG_ERR_NOT_FOUND,
						(pCtx, "Un-adding %s from pendingtree.", SG_pathname__sz(pPath_Input))  );

	SG_ASSERT(  (ptnItem->pCurrentParent == ptnDir)  );
	SG_ASSERT(  (strcmp(pszGid, ptnItem->pszgid) == 0)  );
	SG_ASSERT(  (ptnItem->pBaselineEntry == NULL)  );

	// I only needed this function to un-add a temporary directory that
	// I only added to help MERGE be a lot easier.
	//
	// TODO I don't know if we need this or not but sg_ptnode__remove_safely()
	// TODO has stuff to prevent a REMOVE if the item has been RENAMED, MOVED,
	// TODO and etc.  For now, I'm going to say no.

	if (ptnItem->type == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		// see if the
		SG_uint32 nrChildren = 0;

		if (ptnItem->prbItems)
			SG_ERR_CHECK(  SG_rbtree__count(pCtx, ptnItem->prbItems, &nrChildren)  );

		if (nrChildren != 0)
			SG_ERR_THROW2(  SG_ERR_INVALIDARG,
							(pCtx, "Cannot un-add directory %s because pendingtree is not empty.", SG_pathname__sz(pPath_Input))  );
	}

	SG_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, ptnDir->prbItems, pszGid, (void **)&ptnItem2)  );
	SG_ASSERT(  (ptnItem2 == ptnItem)  );
	SG_PTNODE_NULLFREE(pCtx, ptnItem2);

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDir);
	SG_STRING_NULLFREE(pCtx, pStringEntryname);
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__get_wd_timestamp_cache__ref(SG_context * pCtx,
												 SG_pendingtree * pPendingTree,
												 const SG_vhash ** ppvhTimestamps)
{
	SG_vhash * pvh;
	SG_bool bFound;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULLARGCHECK_RETURN(ppvhTimestamps);

	SG_ASSERT(  (pPendingTree->pvh_wd)  );

	// TODO 2010/05/26 At this point in time I'm going to make the timestamp cache
	// TODO            optional in the wd.json file (since we're dogfooding just
	// TODO            fine (albeit slowly) without it).  Callers can treat the
	// TODO            lack of a timestamp cache just like a cache miss.
	SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, pPendingTree->pvh_wd, "timestamps", &bFound)  );
	if (bFound)
		SG_ERR_CHECK_RETURN(  SG_vhash__get__vhash(pCtx, pPendingTree->pvh_wd, "timestamps", &pvh)  );
	else
		pvh = NULL;

	*ppvhTimestamps = pvh;
}

void SG_pendingtree__set_wd_timestamp_cache(SG_context * pCtx,
											SG_pendingtree * pPendingTree,
											SG_vhash ** ppvhTimestamps)
{
	// use this when you want to set the entire in-memory live
	// timestamp cache to a known list of values.

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NULL_PP_CHECK_RETURN(ppvhTimestamps);

	SG_ASSERT(  (pPendingTree->pvh_wd)  );

#if TRACE_TIMESTAMP_CACHE
	SG_ERR_IGNORE(  SG_vhash_debug__dump_to_console__named(pCtx, *ppvhTimestamps, "Timestamps")  );
#endif

	SG_ERR_CHECK_RETURN(  SG_vhash__update__vhash(pCtx, pPendingTree->pvh_wd, "timestamps", ppvhTimestamps)  );

#if defined(DEBUG)
	pPendingTree->b_pvh_wd__timestamps__dirty = SG_TRUE;
#endif
}

void SG_pendingtree__clear_wd_timestamp_cache(SG_context * pCtx,
											  SG_pendingtree * pPendingTree)
{
	SG_bool bFound;

	SG_NULLARGCHECK_RETURN(pPendingTree);

	SG_ASSERT(  (pPendingTree->pvh_wd)  );

#if TRACE_TIMESTAMP_CACHE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Clearing Timestamp cache.\n")  );
#endif

	SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, pPendingTree->pvh_wd, "timestamps", &bFound)  );
	if (bFound)
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__remove(pCtx, pPendingTree->pvh_wd, "timestamps")  );

#if defined(DEBUG)
		pPendingTree->b_pvh_wd__timestamps__dirty = SG_TRUE;
#endif
	}
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__set_wd_file_timestamp__dont_save_pendingtree(SG_context * pCtx,
																  SG_pendingtree * pPendingTree,
																  const char * pszGid,
																  SG_int64 mtime_ms)
{
	// use this when you want to directly update an individual item
	// in the live in-memory timestamp cache.
	//
	// pendingtree.timestamps.<gid>.mtime_ms = <mtime_ms>
	// pendingtree.timestamps.<gid>.clock_ms = <now_ms>

	SG_vhash * pvhTimestamps_Allocated = NULL;
	SG_vhash * pvhTimestamps;
	SG_vhash * pvhGid = NULL;
	SG_int64 iTimeNow;
	SG_bool bFound;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);

	SG_ASSERT(  (pPendingTree->pvh_wd)  );

	// we want to be able to say that we knew that the timestamp cache value
	// for this entry was valid as of this time.

	SG_ERR_CHECK(  SG_time__get_milliseconds_since_1970_utc(pCtx, &iTimeNow)  );

	// TODO 2010/05/29 Should we assert if (mtime_ms > iTimeNow) ?
	// TODO            Would this indicate a remote mount with clock skew issues ?

#if TRACE_TIMESTAMP_CACHE
	{
		char bufTimestamp[100];
		char bufClockNow[100];

		SG_ERR_IGNORE(  SG_time__format_local__i64(pCtx, mtime_ms, bufTimestamp, sizeof(bufTimestamp))  );
		SG_ERR_IGNORE(  SG_time__format_local__i64(pCtx, iTimeNow, bufClockNow,  sizeof(bufClockNow))  );
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "set_wd_file_timestamp: [gid %s][mtime %s][clock %s]\n",
								   pszGid, bufTimestamp, bufClockNow)  );
	}
#endif

	SG_ERR_CHECK(  SG_vhash__has(pCtx, pPendingTree->pvh_wd, "timestamps", &bFound)  );
	if (bFound)
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__get__vhash(pCtx, pPendingTree->pvh_wd, "timestamps", &pvhTimestamps)  );
	}
	else
	{
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhTimestamps_Allocated)  );
		pvhTimestamps = pvhTimestamps_Allocated;
		SG_ERR_CHECK_RETURN(  SG_vhash__add__vhash(pCtx, pPendingTree->pvh_wd, "timestamps", &pvhTimestamps_Allocated)  );	// this steals the vhash
	}

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhGid)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvhGid, "mtime_ms", mtime_ms)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvhGid, "clock_ms", iTimeNow)  );
	SG_ERR_CHECK(  SG_vhash__update__vhash(pCtx, pvhTimestamps, pszGid, &pvhGid)  );	// this steals our vhash

#if defined(DEBUG)
	pPendingTree->b_pvh_wd__timestamps__dirty = SG_TRUE;
#endif

fail:
	SG_VHASH_NULLFREE(pCtx, pvhTimestamps_Allocated);
	SG_VHASH_NULLFREE(pCtx, pvhGid);
}

void SG_pendingtree__get_wd_file_timestamp(SG_context * pCtx,
										   SG_pendingtree * pPendingTree,
										   const char * pszGid,
										   SG_bool * pbFoundEntry,
										   SG_int64 * p_mtime_ms,
										   SG_int64 * p_clock_ms)
{
	SG_vhash * pvhTimestamps;
	SG_vhash * pvhGid;
	SG_bool bFoundEntry = SG_FALSE;
	SG_bool bFoundCache = SG_FALSE;
	SG_int64 mtime_ms = 0;
	SG_int64 clock_ms = 0;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NULLARGCHECK_RETURN(pbFoundEntry);
	SG_NULLARGCHECK_RETURN(p_mtime_ms);
	SG_NULLARGCHECK_RETURN(p_clock_ms);

	SG_ASSERT(  (pPendingTree->pvh_wd)  );

	SG_ERR_CHECK(  SG_vhash__has(pCtx, pPendingTree->pvh_wd, "timestamps", &bFoundCache)  );
	if (bFoundCache)
	{
		SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pPendingTree->pvh_wd, "timestamps", &pvhTimestamps)  );
		SG_ERR_CHECK(  SG_vhash__has(pCtx, pvhTimestamps, pszGid, &bFoundEntry)  );
		if (bFoundEntry)
		{
			SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhTimestamps, pszGid, &pvhGid)  );

			SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvhGid, "mtime_ms", &mtime_ms)  );
			SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvhGid, "clock_ms", &clock_ms)  );
		}
	}

	*pbFoundEntry = bFoundEntry;
	*p_mtime_ms = mtime_ms;
	*p_clock_ms = clock_ms;

fail:
	return;
}

void SG_pendingtree__is_wd_file_timestamp_valid(SG_context * pCtx,
												SG_pendingtree * pPendingTree,
												const char * pszGid,
												SG_int64 mtime_ms_observed_now,
												SG_bool * pbResult)
{
	SG_int64 mtime_ms_cached;
	SG_int64 clock_ms_cached;
	SG_int64 iTimeNow;
	SG_int64 delta_sec;
	SG_bool bFoundEntry;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);
	SG_NULLARGCHECK_RETURN(pbResult);

#if STATS_TIMESTAMP_CACHE
	pPendingTree->trace_timestamp_cache.nr_queries++;
#endif

	SG_ERR_CHECK_RETURN(  SG_pendingtree__get_wd_file_timestamp(pCtx, pPendingTree, pszGid,
																&bFoundEntry, &mtime_ms_cached, &clock_ms_cached)  );
	if (!bFoundEntry)
	{
		// no info cached for this file.  we should force them to compute the HID.

		*pbResult = SG_FALSE;
		return;
	}

	if (mtime_ms_observed_now != mtime_ms_cached)
	{
		// the currently observed mtime is later than the cached value.
		// so the file has changed since we last checked the HID and we
		// should recompute the content HID.
		//
		// TODO technically this should be strictly "greater-than", but
		// TODO i'm going to bail if they are playing games with the
		// TODO system clock and/or have clock skew issues on a
		// TODO network-mounted file.

		*pbResult = SG_FALSE;
		return;
	}

	SG_ERR_CHECK_RETURN(  SG_time__get_milliseconds_since_1970_utc(pCtx, &iTimeNow)  );
	if (iTimeNow <= clock_ms_cached)
	{
		// the system clock has been set back.  the cache entry was
		// created in the future.  all bets are off.  just say no.
		//
		// TODO 2010/05/29 Is this really necessary?  I think I'm
		// TODO            just being paranoid.

		*pbResult = SG_FALSE;
		return;
	}

	// the cached timestamp and the observed timestamp are equal.
	// but we're not done yet.  it could be that:
	//
	// [a] the file *HAS NOT* changed on disk since we last computed the HID.
	//
	// [b] the file *HAS* changed, but the resolution of mtime on the
	//     filesystem isn't accurate enough for us to detect it.
	//
	// case [b] happens, for example, on filesystems that don't have
	// sub-second resolution (HFS+ or Ext3 or FAT) and we are running
	// in a test suite where we create a file, commit it, edit it, and
	// call status -- all within a few milli/micro seconds.
	//
	// case [a] gives us a performance boost in normal operations.  but
	// if we accidentally give the wrong answer, nothing breaks -- it
	// just runs a little slower.  if we give the wrong answer in case [b],
	// we get weird errors.
	//
	// we recorded the time on the clock when we created the cache entry
	// for this file -- we are asserting that the computed HID and the
	// cached mtime were in agreement at a given point in time.  if the
	// file changed a nanosecond after that assert (and the OS truncated
	// the mtime to a whole second), we're hosed.
	//
	// WARNING: this has nothing to do with the current clock; only with
	// WARNING: the cached values.  if they create a file and commit it
	// WARNING: (causing us to cache {mtime_1,clock_1} ) and then edit
	// WARNING: it all within the same second (thus not changing mtime_2),
	// WARNING: we have the ambiguity;  if they then wait
	// WARNING: 5 years to do the second commit, it doesn't matter the
	// WARNING: ambiguity is already present.
	//
	// so if the mtime is within a "grey area" interval around the clock
	// time, we err on the safe side and force it.  since this is
	// potentially a round-off/truncation problem, we do this in seconds
	// rather than milliseconds.
	//

	delta_sec = (clock_ms_cached / 1000) - (mtime_ms_cached / 1000);
	if (delta_sec <= 0)
	{
		*pbResult = SG_FALSE;
		return;
	}

	// assume case [a]

#if STATS_TIMESTAMP_CACHE
	pPendingTree->trace_timestamp_cache.nr_valid++;
#endif

	*pbResult = SG_TRUE;
	return;
}

void SG_pendingtree__clear_wd_file_timestamp(SG_context * pCtx,
											 SG_pendingtree * pPendingTree,
											 const char * pszGid)
{
	SG_vhash * pvhCache;
	SG_bool bFoundEntry = SG_FALSE;
	SG_bool bFoundCache = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pPendingTree);
	SG_NONEMPTYCHECK_RETURN(pszGid);

	SG_ASSERT(  (pPendingTree->pvh_wd)  );

	SG_ERR_CHECK(  SG_vhash__has(pCtx, pPendingTree->pvh_wd, "timestamps", &bFoundCache)  );
	if (bFoundCache)
	{
		SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pPendingTree->pvh_wd, "timestamps", &pvhCache)  );
		SG_ERR_CHECK(  SG_vhash__has(pCtx, pvhCache, pszGid, &bFoundEntry)  );
		if (bFoundEntry)
		{
			SG_ERR_CHECK(  SG_vhash__remove(pCtx, pvhCache, pszGid)  );

#if defined(DEBUG)
			pPendingTree->b_pvh_wd__timestamps__dirty = SG_TRUE;
#endif
		}
	}

#if TRACE_TIMESTAMP_CACHE
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   "clear_wd_file_timestamp: [gid %s] [cache present %d][entry in cache %d]\n",
							   pszGid, bFoundCache, bFoundEntry)  );
#endif

fail:
	return;
}


static void sg_ptnode__remove_safely(SG_context* pCtx,
									 struct sg_ptnode* ptn,
									 const SG_pathname* pPath,
									 const char* const* paszIncludes, SG_uint32 count_includes,
									 const char* const* paszExcludes, SG_uint32 count_excludes,
									 const char* const* paszIgnores,  SG_uint32 count_ignores,
									 SG_bool b_really,
									 SG_bool* pbRemoved)
{
    const char* pszBaselineHid = NULL;
    char* pszCurrentHid = NULL;
    SG_bool bExactMatch = SG_FALSE;
    SG_fsobj_stat st;
	SG_dir* pDir = NULL;
	SG_error errReadStat;
	SG_string* pStringFileName = NULL;
	SG_fsobj_stat fsStat;
    SG_pathname* pPathSub = NULL;
    struct sg_ptnode* ptnSub = NULL;
    SG_bool bRemoved = SG_FALSE;
    SG_string* pstrLink = NULL;
    const char* pszBaselineName = NULL;
	SG_string * pStringRepoPath = NULL;
	SG_file_spec_eval eval;

    SG_NULLARGCHECK( ptn );

	SG_ERR_CHECK(  SG_workingdir__wdpath_to_repopath(pCtx, ptn->pPendingTree->pPathWorkingDirectoryTop, pPath, SG_TRUE, &pStringRepoPath)  );

#if TRACE_REMOVE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"RemoveSafely: considering [gid %s][%s]\n", ptn->pszgid, SG_string__sz(pStringRepoPath))  );
#endif

    if (!ptn->pBaselineEntry)
    {
        bRemoved = SG_FALSE;
        goto done;
    }

    /* Let's assume it is not safe to delete something if it has been renamed. */
    SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx, ptn->pBaselineEntry, &pszBaselineName)  );
    if (ptn->pszCurrentName)
    {
        if (0 != strcmp(ptn->pszCurrentName, pszBaselineName))
        {
            bRemoved = SG_FALSE;
            goto done;
        }
    }

    /* We refuse to delete this if it has moved to a different parent */
    if (ptn->pszgidMovedFrom)
    {
        bRemoved = SG_FALSE;
        goto done;
    }

    /* TODO refuse to delete this if either the xattrs or the attrbits have changed (See sprawl-808)*/

    /* TODO we could assert here that pszBaselineName is the same as the final name in the path */

	SG_ERR_CHECK(  SG_file_spec__should_include(pCtx,
												paszIncludes, count_includes,
												paszExcludes, count_excludes,
												paszIgnores,  count_ignores,
												SG_string__sz(pStringRepoPath),
												&eval)  );
#if TRACE_REMOVE
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"RemoveSafely: considering [eval 0x%x][path %s]\n", eval, SG_string__sz(pStringRepoPath))  );
#endif

	switch (eval)
	{
	default:		// queits compiler
		SG_ASSERT(  (0)  );

	case SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED:
#if TRACE_ADDREMOVE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "RemoveSafely: item excluded [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
		bRemoved = SG_FALSE;
		goto done;

	case SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED:
#if TRACE_ADDREMOVE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "RemoveSafely: item ignored [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
		bRemoved = SG_FALSE;
		goto done;

	case SG_FILE_SPEC_EVAL__MAYBE:
		// a short cut.  they gave us a list of INCLUDES on the REMOVE.
		// we have an entry that does not match any of the INCLUDE patterns.
		// therefore this entry will not be deleted.  if we are in a dive,
		// we can stop looking.
		//
		// TODO one could argue that if we have a directory, we should keep
		// TODO diving and return status based upon whether there are any
		// TODO un-deleted files (that were ignored/excluded) or any dirty
		// TODO files (that were ignored), but we still have the current
		// TODO directory that we are not deleting and so we must report
		// TODO "unsafe" to our caller.
#if TRACE_ADDREMOVE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "RemoveSafely: aborting dive on maybe-entry [%s]\n", SG_string__sz(pStringRepoPath))  );
#endif
		bRemoved = SG_FALSE;
		goto done;

	case SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED:
	case SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED:
		// fall thru to the original code so that it doesn't look like
		// i changed it.
		//
		// TODO later i'd like to move the following stuff into functions
		// TODO for each type of entry....
		break;
	}

	SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath, &st)  );
	SG_ERR_CHECK(  _pt_log__remove_item(pCtx, ptn->pPendingTree, ptn->pszgid, SG_pathname__sz(pPath), "removed")  );
	if (st.type == SG_FSOBJ_TYPE__DIRECTORY)
	{
        SG_bool bRemovedAllSubEntries = SG_TRUE;

        if (ptn->type != SG_TREENODEENTRY_TYPE_DIRECTORY)
        {
            bRemoved = SG_FALSE;
            goto done;
        }

	    SG_ERR_CHECK(  sg_ptnode__load_dir_entries(pCtx, ptn)  );

	    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStringFileName)  );
        SG_ERR_CHECK(  SG_dir__open(pCtx, pPath, &pDir, &errReadStat, pStringFileName, &fsStat)  );

		if (!SG_IS_OK(errReadStat))
			SG_ERR_THROW(  errReadStat  );

        do
        {
            const char* pszName = SG_string__sz(pStringFileName);
            if (
                (0 != strcmp(".", pszName))
                && (0 != strcmp("..", pszName))
                )
            {
                SG_bool bRemovedThisSubEntry = SG_FALSE;

                SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathSub, pPath, pszName)  );

                SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__active(pCtx, ptn, pszName, &ptnSub)  );

                if (ptnSub)
                {
                    SG_ERR_CHECK(  sg_ptnode__remove_safely(pCtx,
															ptnSub,
															pPathSub,
															paszIncludes, count_includes,
															paszExcludes, count_excludes,
															paszIgnores,  count_ignores,
															b_really,
															&bRemovedThisSubEntry)  );
                }
                else
                {
					// the directory contains something that we don't know about.
                    bRemoved = SG_FALSE;
                    goto done;
                }

                if (!bRemovedThisSubEntry)
                {
                    bRemovedAllSubEntries = SG_FALSE;
                }
            }

            SG_PATHNAME_NULLFREE(pCtx, pPathSub);

            SG_dir__read(pCtx, pDir,pStringFileName,&fsStat);

			SG_context__get_err(pCtx, &errReadStat);
			SG_context__err_reset(pCtx);

        } while (SG_IS_OK(errReadStat));

        if (errReadStat != SG_ERR_NOMOREFILES)
        {
            SG_ERR_THROW(  errReadStat  );
        }

        SG_DIR_NULLCLOSE(pCtx, pDir);

        if (bRemovedAllSubEntries)
        {
            if (b_really)
            {
                SG_ERR_CHECK(  SG_fsobj__rmdir__pathname(pCtx, pPath)  );
            }
        }
        else
        {
            bRemoved = SG_FALSE;
            goto done;
        }
	}
	else if (st.type == SG_FSOBJ_TYPE__REGULAR)
	{
		SG_bool bCachedTimeValid;

        if (ptn->type != SG_TREENODEENTRY_TYPE_REGULAR_FILE)
        {
            bRemoved = SG_FALSE;
            goto done;
        }

        SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, ptn->pBaselineEntry, &pszBaselineHid)  );

		SG_ERR_CHECK(  SG_pendingtree__is_wd_file_timestamp_valid(pCtx, ptn->pPendingTree, ptn->pszgid,
																  st.mtime_ms,
																  &bCachedTimeValid)  );
		if (bCachedTimeValid)
		{
			if (ptn->pszCurrentHid)
			{
				// 2010/05/27 Since we no longer set ptn->pszCurrentHid in scan-dir when it
				//            matches the baseline, we should be able to assert this.
				SG_ASSERT(  (strcmp(ptn->pszCurrentHid, pszBaselineHid) != 0)  );

				// so if we have a current-hid it is different from the baseline.
				bExactMatch = SG_FALSE;
			}
			else
			{
				// by definition --> no current-hid means the file matches the baseline.
				bExactMatch = SG_TRUE;
			}

#if 0 && defined(DEBUG)
			{
				SG_bool bVerifyExactMatch;

				// verify that it is valid to use the timestamp cache.
				SG_ERR_CHECK(  _pt__compute_file_hid(pCtx, ptn->pPendingTree->pRepo, pPath, &pszCurrentHid)  );
				bVerifyExactMatch = (0 == strcmp(pszBaselineHid, pszCurrentHid));
				if (bVerifyExactMatch != bExactMatch)
					SG_ERR_THROW2(  SG_ERR_ASSERT,
									(pCtx, "Assumed timestamp cache was valid in RemoveSafely, but wasn't for [%s] [now %s][baseline %s] [predicted ExactMatch %d][verify ExactMatch %d].",
									 SG_pathname__sz(pPath),
									 pszCurrentHid,
									 pszBaselineHid,
									 bExactMatch,
									 bVerifyExactMatch)  );
			}
#endif

		}
		else
		{
			// TODO 2010/05/27 This routine gets called twice -- once with b_really set to FALSE
			// TODO            and once with TRUE.
			// TODO
			// TODO            When FALSE, we just look for problems and don't do anything.  We
			// TODO            should consider updating ptn->pszCurrentHid and the timestamp in
			// TODO            the cache.  This will make the second pass go much faster.

			SG_ERR_CHECK(  _pt__compute_file_hid(pCtx, ptn->pPendingTree->pRepo, pPath, &pszCurrentHid)  );
			bExactMatch = (0 == strcmp(pszBaselineHid, pszCurrentHid));
		}

        if (bExactMatch)
        {
            if (b_really)
            {
                SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath)  );

				SG_ERR_CHECK(  SG_pendingtree__clear_wd_file_timestamp(pCtx, ptn->pPendingTree, ptn->pszgid)  );
            }
        }
        else
        {
            bRemoved = SG_FALSE;
            goto done;
        }
	}
	else if (st.type == SG_FSOBJ_TYPE__SYMLINK)
	{
        if (ptn->type != SG_TREENODEENTRY_TYPE_SYMLINK)
        {
            bRemoved = SG_FALSE;
            goto done;
        }

        SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, ptn->pBaselineEntry, &pszBaselineHid)  );

        SG_ERR_CHECK(  SG_fsobj__readlink(pCtx, pPath, &pstrLink)  );
        SG_ERR_CHECK(  SG_repo__alloc_compute_hash__from_string(pCtx, ptn->pPendingTree->pRepo, pstrLink, &pszCurrentHid)  );
        SG_STRING_NULLFREE(pCtx, pstrLink);

        bExactMatch = (0 == strcmp(pszBaselineHid, pszCurrentHid));
        SG_NULLFREE(pCtx, pszCurrentHid);

        if (bExactMatch)
        {
            if (b_really)
            {
                SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath)  );
            }
        }
        else
        {
            bRemoved = SG_FALSE;
            goto done;
        }
	}
	else
	{
		SG_ERR_THROW(  SG_ERR_INVALID_PENDINGTREE_OBJECT_TYPE  );
	}

    /* If we got here, then everything was removed */
    bRemoved = SG_TRUE;

done:
    if (bRemoved)
    {
        if (pbRemoved)
        {
            *pbRemoved = SG_TRUE;
        }
    }
    else
    {
        if (pbRemoved)
        {
            *pbRemoved = SG_FALSE;
        }
        else
        {
			SG_ERR_THROW2(  SG_ERR_CANNOT_REMOVE_SAFELY,
							(pCtx, "%s cannot be safely removed.", SG_pathname__sz(pPath))  );
        }
    }

    /* fall through to common cleanup */

fail:

#if TRACE_PENDINGTREE
	if (SG_context__has_err(pCtx))
	{
		SG_error err;
		SG_context__get_err(pCtx,&err);
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"sg_ptnode__remove_safely: [gid %s][path %s] error [%d]\n",
								   ptn->pszgid,
								   SG_pathname__sz(pPath),
								   err)  );
	}
	else
	{
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"sg_ptnode__remove_safely: [gid %s][path %s] result [bRemoved %d]\n",
								   ptn->pszgid,
								   SG_pathname__sz(pPath),
								   bRemoved)  );
	}
#endif

	SG_STRING_NULLFREE(pCtx, pStringRepoPath);
    SG_STRING_NULLFREE(pCtx, pstrLink);
    SG_PATHNAME_NULLFREE(pCtx, pPathSub);
	SG_STRING_NULLFREE(pCtx, pStringFileName);
    SG_DIR_NULLCLOSE(pCtx, pDir);
    SG_NULLFREE(pCtx, pszCurrentHid);
}

//////////////////////////////////////////////////////////////////

void SG_pendingtree__remove_dont_save_pendingtree(SG_context* pCtx,
												  SG_pendingtree* pPendingTree,
												  const SG_pathname* pPathRelativeTo,
												  SG_uint32 count_items, const char** paszItems,
												  const char* const* paszIncludes, SG_uint32 count_includes,
												  const char* const* paszExcludes, SG_uint32 count_excludes,
												  const char* const* paszIgnores,  SG_uint32 count_ignores,
												  SG_bool bTest)
{
	SG_uint32 i;
	struct sg_ptnode* ptn = NULL;
	struct sg_ptnode* ptnItem = NULL;
	SG_string* pstrName = NULL;
	SG_pathname* pPathDir = NULL;
	SG_pathname* pPathLocal = NULL;
    SG_bool b_really = SG_FALSE;

	// TODO do we need to guard that they're not trying to delete "@/" ?

    b_really = SG_FALSE;

    /* The loop below gets executed twice.  The first time through,
     * b_really is false, and this flag gets passed down to sg_ptnode__remove_safely
     * which tells it not to really remove anything.  It's just a
     * verification pass to make sure everything will work before
     * we actually alter anything.  If that succeeds, we set b_really
     * to true and come back up and do it again for real.
     *
     * The bTest flag prevents us from going through the second pass.
     */

again:
    for (i=0; i<count_items; i++)
    {
        /* First get a pathname for the item */
        SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathLocal, pPathRelativeTo, paszItems[i])  );

        /* We want the parent directory of this item.  So copy the path and remove the last component */
        SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathDir, pPathLocal)  );
        SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathDir)  );

        /* Find the directory's ptnode */
        SG_ERR_CHECK(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathDir, SG_TRUE, &ptn)  );

        /* Get the base name from the original path */
        SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathLocal, &pstrName)  );

        /* Find the ptnode */
        SG_ERR_CHECK(  sg_ptnode__find_entry_by_name_not_recursive__active(pCtx, ptn, SG_string__sz(pstrName), &ptnItem)  );
        if (!ptnItem)
            SG_ERR_THROW2(  SG_ERR_NOT_FOUND,
                            (pCtx,"Looking for '%s' in the pending tree node for '%s'",
                             SG_string__sz(pstrName),
                             SG_pathname__sz(pPathDir))  );

        // Remove the local stuff, but fail if it is not safe to do so.
		//
		// If the item is under version control, we ignore the IGNORES list.
		// That is, if someone checked in "foo~" somehow, and now they want
		// to delete it, we should let them.
		//
		// On the other hand, if "foo~" IS NOT under version control (and they
		// just types "vv remove foo*" on the command line and the shell globbing
		// picked up "foo~", we may have a FOUND ptnode for it), we should silently
		// ignore this.

		if (ptnItem->temp_flags & sg_PTNODE_TEMP_FLAG_IMPLICIT_ADD)
			SG_ERR_CHECK(  sg_ptnode__remove_safely(pCtx,
													ptnItem,
													pPathLocal,
													paszIncludes, count_includes,
													paszExcludes, count_excludes,
													paszIgnores,  count_ignores,
													b_really,
													NULL)  );
		else
			SG_ERR_CHECK(  sg_ptnode__remove_safely(pCtx,
													ptnItem,
													pPathLocal,
													paszIncludes, count_includes,
													paszExcludes, count_excludes,
													NULL,         0,
													b_really,
													NULL)  );

        if (b_really)
        {
            ptnItem->saved_flags |= sg_PTNODE_SAVED_FLAG_DELETED;
        }

        SG_STRING_NULLFREE(pCtx, pstrName);

        SG_STRING_NULLFREE(pCtx, pstrName);
        SG_PATHNAME_NULLFREE(pCtx, pPathDir);
        SG_PATHNAME_NULLFREE(pCtx, pPathLocal);
    }

    if (!b_really && bTest == SG_FALSE)
    {
        b_really = SG_TRUE;
        goto again;
    }

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pstrName);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathLocal);
}
