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
 * @file sg_pendingtree__private_scan_dir.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_PENDINGTREE__PRIVATE_SCAN_DIR_H
#define H_SG_PENDINGTREE__PRIVATE_SCAN_DIR_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#	define TRACE_SCAN_DIR		0
#else
#	define TRACE_SCAN_DIR		0
#endif

//////////////////////////////////////////////////////////////////

static void sg_pendingtree__scan_dir(SG_context* pCtx,
									 SG_pendingtree* pPendingTree,
									 SG_pathname* pPathLocalDirectory,
									 const SG_bool bAddRemove,
									 const SG_bool bMarkImplicit,
									 SG_bool bRecursive,
									 struct sg_ptnode* ptn_given,
									 const char* const* pszIncludes, SG_uint32 count_includes,	// TODO 2010/06/03 psz is not right
									 const char* const* pszExcludes, SG_uint32 count_excludes,	// TODO            for a char **
									 const char* const* pszIgnores,  SG_uint32 count_ignores);

//////////////////////////////////////////////////////////////////

struct _pt_sd
{
	SG_pendingtree *		pPendingTree;		// we do not own this
	SG_bool					bAddRemove;
	SG_bool					bMarkImplicit;
	SG_bool					bRecursive;
	SG_uint32				countIncludes;
	SG_uint32				countExcludes;
	SG_uint32				countIgnores;

	// TODO 2010/06/03 psz is not right prefix for a char **.

	const char * const*				pszIncludes;		// we do not own this
	const char * const*				pszExcludes;		// we do not own this
	const char * const*				pszIgnores;			// we do not own this

	struct sg_ptnode *		ptnDirectory;		// we do not own this

	SG_rbtree *				prbEntriesOnDisk;	// map[<entryname> --> _pt_sd_entry *]      we own this
	SG_pathname *			pPathDirectory;		// we own this
};

//////////////////////////////////////////////////////////////////

struct _pt_sd_entry
{
	SG_int64					mtime_ms;
	SG_pathname *				pPath;			// we own this
	SG_treenode_entry_type		tneType;
	SG_bool						bMatched;		// claimed by a ptnode during first pass of scan.
};

void _pt_sd_entry__free(SG_context * pCtx, struct _pt_sd_entry * pEntry)
{
	if (!pEntry)
		return;

	SG_PATHNAME_NULLFREE(pCtx, pEntry->pPath);
	SG_NULLFREE(pCtx, pEntry);
}

//////////////////////////////////////////////////////////////////

/**
 * A wrapper for SG_file_spec__should_include().
 *
 * Use the given Ignore/Include/Exclude rules to decide if we should
 * INCLUDE this entry.  This is based strictly upon:
 *      pData->pPathDirectory / pszEntryName
 *
 * The combined absolute pathname MAY OR MAY NOT actually exist on disk.
 */
static void _pt_sd__file_spec__should_include(SG_context * pCtx,
											  struct _pt_sd * pData,
											  const char * pszEntryName,
											  SG_file_spec_eval * pEval)
{
	SG_pathname * pPathAbsolute = NULL;
	SG_string * pStringRepoPath = NULL;
	SG_file_spec_eval eval;

	// I'm assuming that SG_file_spec__should_include() wants repo-paths.
	// This should prevent us from getting filter matches for stuff above the WD root.

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathAbsolute, pData->pPathDirectory, pszEntryName)  );
	SG_ERR_CHECK(  SG_workingdir__wdpath_to_repopath(pCtx, pData->pPendingTree->pPathWorkingDirectoryTop, pPathAbsolute, SG_FALSE, &pStringRepoPath)  );

	SG_ERR_CHECK( SG_file_spec__should_include(pCtx,
											   pData->pszIncludes, pData->countIncludes,
											   pData->pszExcludes, pData->countExcludes,
											   pData->pszIgnores,  pData->countIgnores,
											   SG_string__sz(pStringRepoPath),
											   &eval));

#if TRACE_SCAN_DIR
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_pt_sd__file_spec__should_include: [result %d] %s\n",
							   bInclude,SG_string__sz(pStringRepoPath))  );
#endif

	*pEval = eval;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathAbsolute);
	SG_STRING_NULLFREE(pCtx, pStringRepoPath);
}

//I pre-declare this here, since I need it.
static void _pt_log__add_item(SG_context * pCtx,
						   SG_pendingtree * pPendingTree,
						   const char * pszGid,
						   const char * pszPathDir,
						   const char * pszReason);
//////////////////////////////////////////////////////////////////

/**
 * We get called once for each entry in the directory.
 * We add the entryname and some simple info to an rbtree.
 */
static SG_dir_foreach_callback _pt_sd__read_directory_cb;

static void _pt_sd__read_directory_cb(SG_context * pCtx,
									  const SG_string * pStringEntryName,
									  SG_fsobj_stat * pfsStat,
									  void * pVoidData)
{
	struct _pt_sd * pData = (struct _pt_sd *)pVoidData;
	const char * pszEntryName = SG_string__sz(pStringEntryName);
	struct _pt_sd_entry * pEntry = NULL;

	// filter out the fixed noise.
	// DO NOT filter the IGNORE/INCLUDE/EXCLUDE stuff here.

	if (strcmp(pszEntryName,".") == 0)
		return;
	if (strcmp(pszEntryName,"..") == 0)
		return;
	if (strcmp(pszEntryName,".sgdrawer") == 0)	// TODO fix this hard-coded string constant here
		return;
	if (strcmp(pszEntryName,".sgtemp") == 0)	// TODO fix this hard-coded string constant here
		return;

	SG_ERR_CHECK(  SG_alloc1(pCtx,pEntry)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pEntry->pPath,pData->pPathDirectory,pszEntryName)  );
	SG_ERR_CHECK(  sg_pendingtree__fsobjtype_to_treenodeentrytype(pCtx,pfsStat->type,&pEntry->tneType)  );
	if (pEntry->tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
		pEntry->mtime_ms = pfsStat->mtime_ms;
	pEntry->bMatched = SG_FALSE;

#if TRACE_SCAN_DIR
	if (pEntry->tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
	{
		char bufTimestamp[100];

		SG_ERR_IGNORE(  SG_time__format_local__i64(pCtx, pEntry->mtime_ms, bufTimestamp, sizeof(bufTimestamp))  );

		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "_pt_sd__read_directory_cb: Found [type %d][%s][mtime %s]\n",
								   pEntry->tneType,
								   SG_pathname__sz(pEntry->pPath),
								   bufTimestamp)  );
	}
	else
	{
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "_pt_sd__read_directory_cb: Found [type %d][%s]\n",
								   pEntry->tneType,
								   SG_pathname__sz(pEntry->pPath))  );
	}
#endif

	SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx,pData->prbEntriesOnDisk,pszEntryName,(void *)pEntry)  );
	pEntry = NULL;		// pData->prbEntriesOnDisk now owns it

	return;

fail:
	SG_ERR_IGNORE(  _pt_sd_entry__free(pCtx, pEntry)  );
}

/**
 * Build an rbtree containing info for each entry in the directory on disk.
 * The caller must free this.
 */
static void _pt_sd__read_directory(SG_context * pCtx,
								   struct _pt_sd * pData,
								   SG_bool * pbExists,
								   SG_bool * pbIsDirectory)
{
	SG_fsobj_type fsObjType;
	SG_bool bExists;

	SG_ERR_CHECK_RETURN(  SG_fsobj__exists__pathname(pCtx,pData->pPathDirectory,&bExists,&fsObjType,NULL)  );
	if (!bExists)
	{
		*pbExists = SG_FALSE;
		*pbIsDirectory = SG_FALSE;
		return;
	}
	if (fsObjType != SG_FSOBJ_TYPE__DIRECTORY)
	{
		*pbExists = SG_TRUE;
		*pbIsDirectory = SG_FALSE;
		return;
	}

#if TRACE_SCAN_DIR
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_pt_sd__read_directory: Reading [%s]\n",
							   SG_pathname__sz(pData->pPathDirectory))  );
#endif

	SG_ERR_CHECK_RETURN(  SG_dir__foreach(pCtx, pData->pPathDirectory, SG_TRUE, _pt_sd__read_directory_cb,(void *)pData)  );

	*pbExists = SG_TRUE;
	*pbIsDirectory = SG_TRUE;
}

//////////////////////////////////////////////////////////////////

/**
 * Open the given pathname and compute the HID on the current contents.
 *
 * You own the returned HID.
 */
static void _pt__compute_file_hid(SG_context * pCtx,
								  SG_repo * pRepo,
								  const SG_pathname * pPath,
								  char ** ppszHid)
{
	SG_file * pFile = NULL;
	char * pszHid = NULL;

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile)  );
	SG_ERR_CHECK(  SG_repo__alloc_compute_hash__from_file(pCtx, pRepo, pFile, &pszHid)  );
	SG_FILE_NULLCLOSE(pCtx, pFile);

	*ppszHid = pszHid;
	return;

fail:
	SG_NULLFREE(pCtx, pszHid);
	SG_FILE_NULLCLOSE(pCtx, pFile);
}

/**
 * Our caller is trying to fill in the "current" fields in the ptnode
 * so that later we can see if any of them are different from the "baseline"
 * fields.  If there are differences, we say the file/symlink/directory is
 * "dirty".
 *
 * Our goal is to see if the file content has changed from the baseline.
 * This requires re-computing the HID on the file content, WHICH IS
 * VERY EXPENSIVE to do on every scan-dir.
 *
 * So, we try to use the file's mtime to see if the content could have
 * changed.
 *
 * When the baseline was established/populated (by GET, COMMIT, and
 * maybe UPDATE), we recorded each file's mtime in the timestamp cache.
 * So we have (baseline-hid, baseline-mtime).
 *
 * In the ptnode, if the file content is clean (or we haven't scanned a
 * change yet), current-hid is null.  If a previous scan-dir determined
 * that the file content changed, it stored the then-current-hid in the
 * ptnode *AND* updated the timestamp cache with the then-current-mtime.
 *
 * So, our task here is to determine if the now-current-mtime is different
 * from the cached mtime.  If so, we compute and store the now-current-hid
 * and update the timestamp cache.
 *
 * TODO 2010/05/27 In the event that the now-current-hid matches the
 * TODO            baseline-hid (say they '/usr/bin/touch' a file), we
 * TODO            have the option of either clearing the current-hid in
 * TODO            the ptnode or setting it to the computed value.  The
 * TODO            pre-timestamp-cache code always set it.  I'm not sure
 * TODO            that it matters one way or the other, but I'm going
 * TODO            clear it so that sg_ptnode__is_file_dirty() is a
 * TODO            little faster.
 *
 * NOTE: It is possible that the timestamp cache is not present or
 * NOTE: doesn't have an entry for this file (because we added it after
 * NOTE: we started dogfooding).  We silently allow this and force the
 * NOTE: HID computation.  And because we always update the cache
 * NOTE: whenever we do the computation, we will back-fill the cache.
 */
static void _pt_sd__update_ptnode__file_content(SG_context * pCtx,
												struct _pt_sd * pData,
												struct sg_ptnode * ptnSub,
												struct _pt_sd_entry * pEntry)
{
	char * pszHid_now_current = NULL;
	const char * pszHid_baseline = NULL;
	SG_fsobj_stat stat_now_current;
	SG_bool bCachedTimeValid = SG_FALSE;

	SG_ASSERT(  (ptnSub->type == SG_TREENODEENTRY_TYPE_REGULAR_FILE)  );

	SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pEntry->pPath, &stat_now_current)  );

	if ((ptnSub->pBaselineEntry == NULL) && (ptnSub->pszCurrentHid == NULL))
	{
		// the file was recently ADDED and not yet committed (so no baseline).
		// scan-dir has yet to see it, so no current-hid.
		//
		// so we force a compute so that the currnt part of the ptnode is complete.

#if TRACE_TIMESTAMP_CACHE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "RequireCompute for [%s]\n",
								   SG_pathname__sz(pEntry->pPath))  );
#endif

		SG_ERR_CHECK(  _pt__compute_file_hid(pCtx, pData->pPendingTree->pRepo, pEntry->pPath, &pszHid_now_current)  );
		SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pData->pPendingTree->pStrPool, pszHid_now_current, &ptnSub->pszCurrentHid)  );
		SG_ERR_CHECK(  SG_pendingtree__set_wd_file_timestamp__dont_save_pendingtree(pCtx,
																					pData->pPendingTree,
																					ptnSub->pszgid,
																					stat_now_current.mtime_ms)  );
		goto done;
	}

	if (ptnSub->pBaselineEntry)
		SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, ptnSub->pBaselineEntry, &pszHid_baseline)  );

	SG_ERR_CHECK(  SG_pendingtree__is_wd_file_timestamp_valid(pCtx, pData->pPendingTree, ptnSub->pszgid,
															  stat_now_current.mtime_ms,
															  &bCachedTimeValid)  );
	if (bCachedTimeValid)
	{
		// the cached mtime is still valid.  so whatever value we
		// have for baseline-hid or then-current-hid in the ptnode
		// is still valid.

#if TRACE_TIMESTAMP_CACHE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "Assuming timestamp cache is valid for [%s]\n",
								   SG_pathname__sz(pEntry->pPath))  );
#endif

#if 0 && defined(DEBUG)
		// verify that last statement.
		SG_ERR_CHECK(  _pt__compute_file_hid(pCtx, pData->pPendingTree->pRepo, pEntry->pPath, &pszHid_now_current)  );
		if (ptnSub->pszCurrentHid)
		{
			if (strcmp(pszHid_now_current, ptnSub->pszCurrentHid) != 0)
				SG_ERR_THROW2(  SG_ERR_ASSERT,
								(pCtx, "Assumed timestamp cache was valid, but wasn't for [%s] [now %s][then-current %s].",
								 SG_pathname__sz(pEntry->pPath),
								 pszHid_now_current,
								 ptnSub->pszCurrentHid)  );
		}
		else
		{
			if (strcmp(pszHid_now_current, pszHid_baseline) != 0)
				SG_ERR_THROW2(  SG_ERR_ASSERT,
								(pCtx, "Assumed timestamp cache was valid, but wasn't for [%s] [now %s][baseline %s].",
								 SG_pathname__sz(pEntry->pPath),
								 pszHid_now_current,
								 pszHid_baseline)  );
		}
#endif

		// this should not happen and could probably be an assert
		// (after dogfooding).  we want to make __is_file_dirty()'s
		// job easier.

		if (ptnSub->pszCurrentHid && pszHid_baseline && (strcmp(ptnSub->pszCurrentHid, pszHid_baseline) == 0))
			ptnSub->pszCurrentHid = NULL;
	}
	else
	{
#if TRACE_TIMESTAMP_CACHE
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "Timestamp cache is not valid for [%s]\n",
								   SG_pathname__sz(pEntry->pPath))  );
#endif
		// mtime has changed on file since our last run or the cache was invalid.

		SG_ERR_CHECK(  _pt__compute_file_hid(pCtx, pData->pPendingTree->pRepo, pEntry->pPath, &pszHid_now_current)  );
		if (pszHid_baseline && (strcmp(pszHid_now_current, pszHid_baseline) == 0))
		{
			// something like a '/usr/bin/touch' or they undid any changes to the file.

			ptnSub->pszCurrentHid = NULL;
		}
		else if ((ptnSub->pszCurrentHid == NULL) || (strcmp(pszHid_now_current, ptnSub->pszCurrentHid) != 0))
		{
			// set new unique hid value.  (avoid overwriting same value.)
			SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pData->pPendingTree->pStrPool, pszHid_now_current, &ptnSub->pszCurrentHid)  );
		}

		SG_ERR_CHECK(  SG_pendingtree__set_wd_file_timestamp__dont_save_pendingtree(pCtx,
																					pData->pPendingTree,
																					ptnSub->pszgid,
																					stat_now_current.mtime_ms)  );
	}

done:
	;
fail:
	SG_NULLFREE(pCtx, pszHid_now_current);
}

/**
 * We have determined that the given ptnode and the given entry on disk
 * should be associated with each other.  Update the various fields in
 * the ptnode from what is currently on disk.
 */
static void _pt_sd__update_ptnode(SG_context * pCtx,
								  struct _pt_sd * pData,
								  struct sg_ptnode * ptnSub,
								  struct _pt_sd_entry * pEntry)
{
	char* pszidFileHid = NULL;
	SG_string* pstrLink = NULL;

#if TRACE_SCAN_DIR
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_sd__update_ptnode: dir [gid %s][%s]\n"
								"                       sub [gid %s][%s]\n"),
							   pData->ptnDirectory->pszgid,
							   SG_pathname__sz(pData->pPathDirectory),
							   ptnSub->pszgid,
							   SG_pathname__sz(pEntry->pPath))  );
#endif

	SG_ASSERT(  (pEntry->bMatched == SG_FALSE)  );

	SG_ASSERT(  (ptnSub->pCurrentParent == pData->ptnDirectory)  );

	SG_ASSERT(  ((ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED) == 0)  );
	SG_ASSERT(  ((ptnSub->temp_flags & sg_PTNODE_TEMP_FLAG_IMPLICIT_DELETE) == 0)  );

	// get the XATTRS and ATTRBITS from the disk and update ptnSub.

#ifdef SG_BUILD_FLAG_FEATURE_XATTR
	SG_ERR_CHECK(  sg_ptnode__scan_xattrs(pCtx, ptnSub, pEntry->pPath)  );
#endif
	SG_ERR_CHECK(  sg_ptnode__scan_attrbits(pCtx, ptnSub, pEntry->pPath)  );

	if (SG_TREENODEENTRY_TYPE_REGULAR_FILE == ptnSub->type)
	{
		SG_ERR_CHECK(  _pt_sd__update_ptnode__file_content(pCtx, pData, ptnSub, pEntry)  );
	}
	else if (SG_TREENODEENTRY_TYPE_DIRECTORY == ptnSub->type)
	{
		if (pData->bRecursive)
		{
			SG_ERR_CHECK(  sg_ptnode__load_dir_entries(pCtx, ptnSub)  );
			SG_ERR_CHECK(  sg_pendingtree__scan_dir(pCtx,
													pData->pPendingTree,
													pEntry->pPath,
													pData->bAddRemove,
													pData->bMarkImplicit,
													pData->bRecursive,
													ptnSub,
													pData->pszIncludes, pData->countIncludes,
													pData->pszExcludes, pData->countExcludes,
													pData->pszIgnores,  pData->countIgnores)  );
		}
	}
	else if (SG_TREENODEENTRY_TYPE_SYMLINK == ptnSub->type)
	{
		// TODO 2010/05/27 Do we need to think about using the timestamp cache for symlinks?
		// TODO            Or does that even make sense?  Or do we have enough symlinks to bother?
		SG_ERR_CHECK(  SG_fsobj__readlink(pCtx, pEntry->pPath, &pstrLink)  );
		SG_ERR_CHECK(  SG_repo__alloc_compute_hash__from_string(pCtx, ptnSub->pPendingTree->pRepo, pstrLink, &pszidFileHid)  );
		SG_STRING_NULLFREE(pCtx, pstrLink);

		SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, pData->pPendingTree->pStrPool, pszidFileHid, &ptnSub->pszCurrentHid)  );
		SG_NULLFREE(pCtx, pszidFileHid);
	}

	return;

fail:
	SG_NULLFREE(pCtx, pszidFileHid);
	SG_STRING_NULLFREE(pCtx, pstrLink);
}

//////////////////////////////////////////////////////////////////

/**
 * We get called for each ptnode that should be converted to a DELETE
 * of some kind because we could not find a corresponding entry in the
 * current directory.  THIS IS NOT A VERB FOR DOING A DELETE, but rather
 * a routine to mark various nodes of the tree in memory as deleted
 * because we couldn't find something on the disk.
 *
 * Note that we DO NOT actually delete the ptnode from the parent's
 * list (ptnSub->pCurrentParent->prbItems), we only set some flags.
 *
 * We are possibly in a self-recursive call (deleting the contents of
 * a deleted directory).  We CANNOT respect the Ignore/Include/Exclude
 * rules -- saying don't delete the *.h inside a directory that you
 * ALREADY deleted doesn't make any sense.  Our caller should inspect
 * the Ignore/Include/Exclude stuff before we begin this dive.
 */
static SG_rbtree_foreach_callback _pt_sd__mark_ptnode_as_deleted;

static void _pt_sd__mark_ptnode_as_deleted(SG_context * pCtx,
										   SG_UNUSED_PARAM( const char * pszKey_GidObject ),
										   void * pVoidValue,
										   void * pVoidData)
{
	struct sg_ptnode * ptnSub = (struct sg_ptnode *)pVoidValue;
	struct _pt_sd * pData = (struct _pt_sd *)pVoidData;
	struct _pt_sd pt_sd;
	const char * pszName = NULL;

	SG_UNUSED( pszKey_GidObject );

	SG_ASSERT(  (ptnSub->pCurrentParent == pData->ptnDirectory)  );

	if (ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)		// this is only needed for recursive call
		return;

	SG_ERR_IGNORE(  sg_ptnode__get_name(pCtx,ptnSub,&pszName)  );

#if TRACE_SCAN_DIR
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_sd__mark_ptnode_as_deleted: dir [gid %s][%s]\n"
								"                                sub [gid %s][%s]\n"),
							   pData->ptnDirectory->pszgid,
							   SG_pathname__sz(pData->pPathDirectory),
							   ptnSub->pszgid,
							   pszName)  );
#endif

	if (ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
	{
		// if we already know ptnSub is DELETED, we don't update the flags.
		// this keeps us from incorrectly picking up an IMPLICIT flag during
		// a recursive dive for things that we know were EXPLICITLY deleted.
	}
	else
	{
		ptnSub->saved_flags |= sg_PTNODE_SAVED_FLAG_DELETED;
		if (pData->bMarkImplicit)
			ptnSub->temp_flags |= sg_PTNODE_TEMP_FLAG_IMPLICIT_DELETE;
	}

	if (ptnSub->type != SG_TREENODEENTRY_TYPE_DIRECTORY)
		return;

	if (!pData->bRecursive)
		return;

	SG_ERR_CHECK_RETURN(  sg_ptnode__load_dir_entries(pCtx, ptnSub)  );

	// build a clone of the pData so that we can change the parent directory pathname/ptnode
	// when we hand it to our children.  we don't bother with prbEntriesOnDisk because
	// our directory has been deleted and doesn't exist on disk.
	//
	// TODO i'm passing down the augmented pathname name in a new pt_sd.  currently,
	// TODO i'm only doing this so that the trace can print it.  i don't know yet if
	// TODO the include/exclude stuff will need it to do pathname-wildcard-matching.
	// TODO if not, we could ifdef part of this or pass null for the pathname.

	memset(&pt_sd,0,sizeof(pt_sd));
	pt_sd.pPendingTree  = pData->pPendingTree;
	pt_sd.bAddRemove    = pData->bAddRemove;
	pt_sd.bMarkImplicit = pData->bMarkImplicit;
	pt_sd.bRecursive    = pData->bRecursive;
	pt_sd.countIncludes = pData->countIncludes;
	pt_sd.countExcludes = pData->countExcludes;
	pt_sd.countIgnores  = pData->countIgnores;
	pt_sd.pszIncludes   = pData->pszIncludes;
	pt_sd.pszExcludes   = pData->pszExcludes;
	pt_sd.pszIgnores    = pData->pszIgnores;
	pt_sd.ptnDirectory  = ptnSub;
	//pt_sd.prbEntriesOnDisk is null
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pt_sd.pPathDirectory, pData->pPathDirectory, pszName)  );

	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx, ptnSub->prbItems, _pt_sd__mark_ptnode_as_deleted, (void *)&pt_sd)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pt_sd.pPathDirectory);
}

//////////////////////////////////////////////////////////////////

/**
 * We get called for each ptnode in a directory (based on GID).
 * We are also given a list of the entrynames currently found on disk.
 *
 * Our job is to associate an entry from the disk list with this ptnode
 * and "claim" it and update the various fields on the ptnode to reflect
 * any changes on disk.  This includes changes to XATTRS/ATTRBITS, file
 * contents, etc.
 *
 * We only look at the given ptnode and don't worry about siblings
 * which may share the same entryname.
 *
 * We DO NOT modify the ptnSub->pCurrentParent->prbItems because we
 * are in an iteration.
 */
static SG_rbtree_foreach_callback _pt_sd__inspect_child_ptnode_cb;

static void _pt_sd__inspect_child_ptnode_cb(SG_context * pCtx, const char * pszKey_GidObject, void * pVoidValue, void * pVoidData)
{
	struct sg_ptnode * ptnSub = (struct sg_ptnode *)pVoidValue;
	struct _pt_sd * pData = (struct _pt_sd *)pVoidData;
	const char * pszEntryName;
	struct _pt_sd_entry * pEntry;
	SG_bool bFoundOnDisk;
	SG_file_spec_eval eval;

	SG_ASSERT(  (strcmp(pszKey_GidObject,ptnSub->pszgid) == 0)  );

	SG_ERR_CHECK(  sg_ptnode__get_name(pCtx,ptnSub,&pszEntryName)  );

	if (ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_MOVED_AWAY)
	{
		// this ptnode is a phantom.  ignore it.

		return;
	}

	// Compute the absolute path and repo-path for this ptnode (without regard for whether
	// it currently exists on disk) and see if it matches any of the Ignore/Include/Exclude
	// rules.

	SG_ERR_CHECK(  _pt_sd__file_spec__should_include(pCtx,pData,pszEntryName,&eval)  );

#if TRACE_SCAN_DIR
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_sd__inspect_child_ptnode: dir [gid %s][%s]\n"
								"                              kid [gid %s][%s][saved_flags %d][temp_flags %d][eval %d]\n"),
							   pData->ptnDirectory->pszgid,
							   SG_pathname__sz(pData->pPathDirectory),
							   ptnSub->pszgid,
							   pszEntryName,
							   ptnSub->saved_flags,
							   ptnSub->temp_flags,
							   eval)  );
#endif

	// look up the entryname in the directory on disk.

	SG_ERR_CHECK(  SG_rbtree__find(pCtx,pData->prbEntriesOnDisk,pszEntryName,&bFoundOnDisk,(void **)&pEntry)  );

	if (ptnSub->saved_flags & sg_PTNODE_SAVED_FLAG_DELETED)
	{
		// this ptnode currently thinks the item has been deleted, but there is an apparent match for it
		// on disk.  we have to assume that it is something new that we may want to add and which should
		// be given a new GID.  we'll do this in a later step.

		if ((bFoundOnDisk) && (pEntry->tneType == ptnSub->type))	// TODO i'm not sure we actually need this.
			return;

		if (!pData->bAddRemove)
			return;

		// technically, we don't need to do anything for this ptnode (we already think it is deleted).
		// but if it is a directory, we want to dive and delete anything within it that isn't already dead.

		if (ptnSub->type != SG_TREENODEENTRY_TYPE_DIRECTORY)
			return;

		if (!pData->bRecursive)
			return;

		// TODO 2010/06/07 I'm wondering if all 5 cases are actually the same and all
		// TODO            are just "return".

		switch (eval)
		{
		default:
			SG_ASSERT(  (0)  );

		case SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED:
		case SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED:
			// if this ptnode represents a pathname that should be ignored/excluded, we stop now.
			// this case is a little weird because we really should dive into a deleted directory
			// and make sure that everything within it is deleted too.
			return;

		case SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED:
		case SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED:
			// TODO 2010/06/07 Confirm that we really need to do this.  We already think it is
			// TODO            deleted.
			SG_ERR_CHECK(  _pt_sd__mark_ptnode_as_deleted(pCtx,pszKey_GidObject,ptnSub,pData)  );
			return;

		case SG_FILE_SPEC_EVAL__MAYBE:
			// There were INCLUDES, but none matched this directory.
			//
			// We already know that the ptnode thinks it has been deleted and we didn't find a
			// directory on the disk with this name, so in theory all of the ptnodes within
			// ptnSub->prbItems should be deleted too.
			//
			// TODO 2010/06/07 Should we verify this?
			return;
		}
	}
	else							// we currently think the item is present
	{
		if ((bFoundOnDisk) && (pEntry->tneType == ptnSub->type))
		{
			// TODO i'm going to assume that we always update the ptnode regardless of
			// TODO the Ignore/Include/Exclude status.  that is, the --include is meant
			// TODO for add/remove and not is-dirty.

			SG_ERR_CHECK(  _pt_sd__update_ptnode(pCtx,pData,ptnSub,pEntry)  );

			// this PTNODE has a correspeonding ENTRY on disk with the same ENTRYNAME
			// AND they have the same type.  we have to ASSUME that they should be
			// associated with each other.

			pEntry->bMatched = SG_TRUE;
			return;
		}

		// either there is no entry with this entryname on disk or the type of the entry on
		// disk is different than what we were expecting.
		//
		// either way, we have nothing to associate with this ptnode and must consider the
		// ptnode to be a DELETE.  (if it was found on disk, we DO NOT claim it.)

		if (!pData->bAddRemove)
			return;

		// TODO 2010/06/07 Does it make any sense to treat these 5 cases any differently?
		// TODO            I mean we have established that we have a ptnode for a directory,
		// TODO            we haven't yet marked the ptnode directory as deleted, we know
		// TODO            that the directory does not exist on disk (or is not a directory),
		// TODO            so even if the said --ignore="foo.dir", foo.dir is still gone.
		// TODO            But I guess they could use thot delay us from discovering this?
		// TODO            This needs more thought.

		switch (eval)
		{
		default:
			SG_ASSERT(  (0)  );

		case SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED:
		case SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED:
			// if this ptnode represents a pathname that should be ignored/excluded, we stop now.
			return;

		case SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED:
		case SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED:
			SG_ERR_CHECK(  _pt_sd__mark_ptnode_as_deleted(pCtx,pszKey_GidObject,ptnSub,pData)  );
			return;

		case SG_FILE_SPEC_EVAL__MAYBE:
			// There were INCLUDES, but none matched this directory.
			return;
		}
	}

fail:
	return;
}

//////////////////////////////////////////////////////////////////

static SG_rbtree_foreach_callback _pt_sd__create_new_ptnode_for_unclaimed_entry_cb;

static void _pt_sd__create_new_ptnode_for_unclaimed_entry_cb(SG_context * pCtx, const char * pszKey_EntryName, void * pVoidValue, void * pVoidData)
{
	struct _pt_sd_entry * pEntry = (struct _pt_sd_entry *)pVoidValue;
	struct _pt_sd * pData = (struct _pt_sd *)pVoidData;
	struct sg_ptnode * ptnSub;
	SG_pathname * pPathSub = NULL;
	SG_string * pStringRepoPath = NULL;
	SG_rbtree_iterator * pIter = NULL;
	SG_fsobj_stat fsStat;
	SG_file_spec_eval eval;
	SG_bool local_bMarkImplicit;

	if (pEntry->bMatched)
		return;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathSub, pData->pPathDirectory, pszKey_EntryName)  );
	SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPathSub, &fsStat)  );
	SG_ERR_CHECK(  SG_workingdir__wdpath_to_repopath(pCtx, pData->pPendingTree->pPathWorkingDirectoryTop, pPathSub, SG_TRUE, &pStringRepoPath)  );
	SG_ERR_CHECK(  _pt_sd__file_spec__should_include(pCtx, pData, SG_string__sz(pStringRepoPath), &eval)  );

#if TRACE_SCAN_DIR
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("_pt_sd__create_new_ptnode: [eval 0x%x][%s]\n",
								eval, SG_string__sz(pStringRepoPath)))  );
#endif

	switch (eval)
	{
	default:		// quiets compiler
		SG_ASSERT(  (0)  );

	case SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED:
	case SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED:
		goto done;

	case SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED:
	case SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED:
		// We have a new (to us) entry on the disk that we want to either
		// list as ADDED or FOUND (this depends upon pData->bMarkImplicit).
		// We respect the passed in setting.
		local_bMarkImplicit = pData->bMarkImplicit;
		goto do_create_item;

	case SG_FILE_SPEC_EVAL__MAYBE:
		if (fsStat.type != SG_FSOBJ_TYPE__DIRECTORY)
			goto done;

		if (!pData->bRecursive)
			goto done;

		// there were INCLUDES and this directory did not match one of them.
		// so we don't want to ADD this item, but we want to provisionally
		// add a ptnode for it so that we can dive and maybe find some files
		// within it that should be added (which would cause us to be officially
		// added.
		local_bMarkImplicit = SG_TRUE;
		goto do_create_item;
	}

	//////////////////////////////////////////////////////////////////
	// NOT REACHED
	//////////////////////////////////////////////////////////////////

do_create_item:

	SG_ERR_CHECK(  sg_ptnode__create_item(pCtx, pData->ptnDirectory, pszKey_EntryName,
										  pData->bAddRemove, pData->bMarkImplicit,
										  &fsStat, &ptnSub)  );
	SG_ERR_CHECK(  _pt_log__add_item(pCtx, pData->pPendingTree, ptnSub->pszgid, SG_pathname__sz(pPathSub), "foundDuringScan" )  );
#ifdef SG_BUILD_FLAG_FEATURE_XATTR
	SG_ERR_CHECK(  sg_ptnode__scan_xattrs(pCtx, ptnSub, pPathSub)  );
#endif
	SG_ERR_CHECK(  sg_ptnode__scan_attrbits(pCtx, ptnSub, pPathSub)  );

	if ((pData->bRecursive) && (ptnSub->type == SG_TREENODEENTRY_TYPE_DIRECTORY))
	{
		SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &ptnSub->prbItems)  );
		SG_ERR_CHECK(  sg_pendingtree__scan_dir(pCtx,
												pData->pPendingTree,
												pPathSub,
												pData->bAddRemove,
												pData->bMarkImplicit,
												pData->bRecursive,
												ptnSub,
												pData->pszIncludes, pData->countIncludes,
												pData->pszExcludes, pData->countExcludes,
												pData->pszIgnores,  pData->countIgnores)  );

		if ((eval == SG_FILE_SPEC_EVAL__MAYBE) && (!pData->bMarkImplicit) && (ptnSub->temp_flags & sg_PTNODE_TEMP_FLAG_IMPLICIT_ADD))
		{
			// we provisionally created ptnSub for the current directory so that
			// we could scan the contents of the directory.  See if any of the
			// children were actually added and convert our status to actual.
			// it not, leave it a FOUND directory.
			//
			// TODO 2010/06/07 would it be better to remove any of the implicit
			// TODO            items within and then see if we have an empty prbItems?
			// TODO            We could also un-add this directory from our parent.

			SG_bool b;
			struct sg_ptnode * ptn_k;

			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, ptnSub->prbItems, &b, NULL, (void **)&ptn_k)  );
			while (b)
			{
				if ((ptn_k->temp_flags & sg_PTNODE_TEMP_FLAG_IMPLICIT_ADD) == 0)
				{
					ptnSub->temp_flags &= ~sg_PTNODE_TEMP_FLAG_IMPLICIT_ADD;
					break;
				}
				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter, &b, NULL, (void **)&ptn_k)  );
			}
		}
	}

done:
	;
fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
	SG_PATHNAME_NULLFREE(pCtx, pPathSub);
	SG_STRING_NULLFREE(pCtx, pStringRepoPath);
}

//////////////////////////////////////////////////////////////////

/**
 * The job of the "scan" operation is to fix the pending tree to match
 * the working directory.  Flags allow the caller to control whether
 * we want everything to be included or maybe we just want to scan for
 * some of the changes instead of all of them.
 *
 * When used for the purpose of a command like 'sg status', we expect
 * that the resulting pending tree will not be saved to disk.
 */
static void sg_pendingtree__scan_dir(SG_context* pCtx,
									 SG_pendingtree* pPendingTree,
									 SG_pathname* pPathLocalDirectory,
									 const SG_bool bAddRemove,
									 const SG_bool bMarkImplicit,
									 SG_bool bRecursive,
									 struct sg_ptnode* ptn_given,
									 const char* const* pszIncludes, SG_uint32 count_includes,	// TODO 2010/06/03 psz is not right
									 const char* const* pszExcludes, SG_uint32 count_excludes,	// TODO            for a char **
									 const char* const* pszIgnores,  SG_uint32 count_ignores)
{
	struct _pt_sd pt_sd;
	struct sg_ptnode * ptn = NULL;
	SG_bool bExists;
	SG_bool bIsDirectory;

	if (ptn_given)
		ptn = ptn_given;
	else
		SG_ERR_CHECK_RETURN(  sg_pendingtree__local_path_to_ptnode(pCtx, pPendingTree, pPathLocalDirectory, SG_TRUE, &ptn)  );

	//TODO remove this when we figure out how addremove should handle a path not
	//int the pending tree
	if (!ptn)
	{
		SG_ERR_THROW2_RETURN(  SG_ERR_NOTIMPLEMENTED,
							   (pCtx, "TODO sg_pendingtree__scan_dir has null ptn with [%s].", SG_pathname__sz(pPathLocalDirectory))  );
		return;
	}

	SG_ASSERT(	(ptn->type == SG_TREENODEENTRY_TYPE_DIRECTORY)	);

	if (!ptn->prbItems)
		SG_ERR_CHECK_RETURN(  sg_ptnode__load_dir_entries(pCtx, ptn)  );

	// build all the baggage that we need to dive with.

	pt_sd.pPendingTree	   = pPendingTree;
	pt_sd.bAddRemove	   = bAddRemove;
	pt_sd.bMarkImplicit    = bMarkImplicit;
	pt_sd.bRecursive	   = bRecursive;
	pt_sd.pszIncludes	   = pszIncludes;
	pt_sd.countIncludes	   = count_includes;
	pt_sd.pszExcludes	   = pszExcludes;
	pt_sd.countExcludes    = count_excludes;
	pt_sd.pszIgnores       = pszIgnores;
	pt_sd.countIgnores     = count_ignores;
	pt_sd.ptnDirectory	   = ptn;
	pt_sd.prbEntriesOnDisk = NULL;
	pt_sd.pPathDirectory   = NULL;
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pt_sd.prbEntriesOnDisk)	);
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pt_sd.pPathDirectory, pPathLocalDirectory)  );

	// Read the contents of the directory as it currently exists on disk (if it exists).


	SG_ERR_CHECK(  _pt_sd__read_directory(pCtx, &pt_sd, &bExists, &bIsDirectory)  );

	SG_ASSERT(	(bExists)  );		// TODO these are only needed for top-level call because
	SG_ASSERT(	(bIsDirectory)	);	// TODO all of the recursive calls should be correct.

	// Iterate over all of the PTNODES (indexed by GID rather than ENTRYNAME) for this directory.
	// Match up the ptnodes with an ENTRY in the prbEntriesOnDisk and do the right thing.  This
	// takes care of entries that were changed or deleted from the disk.

	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx, ptn->prbItems, _pt_sd__inspect_child_ptnode_cb, (void *)&pt_sd)	 );

	// Iterate over all of the ENTRIES that we found in the directory and deal with the "unclaimed" ones.
	// This handled entries that were added

	if (bAddRemove)
		SG_ERR_CHECK(  SG_rbtree__foreach(pCtx, pt_sd.prbEntriesOnDisk, _pt_sd__create_new_ptnode_for_unclaimed_entry_cb, (void *)&pt_sd)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pt_sd.pPathDirectory);
	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pt_sd.prbEntriesOnDisk, (SG_free_callback *)_pt_sd_entry__free);
}

//////////////////////////////////////////////////////////////////

/**
 * This is a wrapper for sg_pendingtree__scan_dir() that first loads
 * the IGNORES settings from LOCALSETTINGS before diving.  This should
 * only be called from SG_pendingtree.c and the MAIN VERBS that it contains.
 *
 * This will cause the scan_dir to PROPERLY IGNORE entrynames (such as foo~).
 * In general, this should be used by verbs that want to modify the pendingtree,
 * such as ADDREMOVE.  It should probably NOT be used by verbs whose output will
 * be filtered, such as TREEDIFF.
 */
static void sg_pendingtree__scan_dir__load_ignores(SG_context* pCtx,
												   SG_pendingtree* pPendingTree,
												   SG_pathname* pPathLocalDirectory,
												   const SG_bool bAddRemove,
												   const SG_bool bMarkImplicit,
												   SG_bool bRecursive,
												   struct sg_ptnode* ptn_given,
												   const char* const* pszIncludes, SG_uint32 count_includes,	// TODO 2010/06/03 psz is not right
												   const char* const* pszExcludes, SG_uint32 count_excludes)	// TODO            for a char **
{
	SG_stringarray * psaIgnores = NULL;
	const char * const * ppszIgnores;
	SG_uint32 count_ignores;

	SG_ERR_CHECK(  SG_file_spec__patterns_from_ignores_localsetting(pCtx, &psaIgnores)  );
	SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, psaIgnores, &ppszIgnores, &count_ignores)  );
	SG_ERR_CHECK(  sg_pendingtree__scan_dir(pCtx,
											pPendingTree,
											pPathLocalDirectory,
											bAddRemove,
											bMarkImplicit,
											bRecursive,
											ptn_given,
											pszIncludes, count_includes,
											pszExcludes, count_excludes,
											ppszIgnores, count_ignores)  );
fail:
	SG_STRINGARRAY_NULLFREE(pCtx, psaIgnores);
}


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_PENDINGTREE__PRIVATE_SCAN_DIR_H
