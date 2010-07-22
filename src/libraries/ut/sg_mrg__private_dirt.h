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
 * @file sg_mrg__private_dirt.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_DIRT_H
#define H_SG_MRG__PRIVATE_DIRT_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Ask the PendingTree to diff the WD with the BASELINE and see if
 * the WD is dirty.
 *
 * If there is dirt, see if we can handle it.
 *
 * We *really* want for the WD to be CLEAN before we start the
 * merge, but that is a bit too strict.  We should be able to
 * cope with FOUND items (such as editor backups and compiler
 * trash files) and properly ignore them.  But we may have to
 * get more involved because someone could actually ADD/COMMIT
 * a trash file with the same name in another branch and then
 * we'd have a collision/conflict that wouldn't appear during
 * the cset-merge-proper, but rather would only be detected when
 * we are applying the merge.
 *
 * THROWS if there is dirt we can't handle.
 */
static void _mrg__look_for_dirt(SG_context * pCtx,
								SG_mrg * pMrg)
{
	SG_treediff2 * pTreeDiff = NULL;
	SG_treediff2_iterator * pIter = NULL;
	SG_treediff2_ObjectData * pOD;
	const char * pszGid;
	SG_uint32 nrChanges;
	SG_diffstatus_flags dsFlags;
	SG_treenode_entry_type tneType;
	SG_bool bOK;

	// Compute treediff between baseline and the WD.  WE DO NOT DO ANY FILTERING ON THIS.
	// Specifically, we DO NOT strip out the IGNORES.

	SG_ERR_CHECK(  SG_pendingtree__do_diff__keep_clean_objects__reload(pCtx,pMrg->pPendingTree, NULL, NULL, &pTreeDiff)  );
	SG_ERR_CHECK(  SG_treediff2__count_changes(pCtx,pTreeDiff,&nrChanges)  );
	if (nrChanges == 0)
	{
		SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
		return;
	}

	SG_ERR_CHECK(  SG_treediff2__iterator__first(pCtx, pTreeDiff, &bOK, &pszGid, &pOD, &pIter)  );
	while (bOK)
	{
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD, &dsFlags)  );
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_tneType(pCtx, pOD, &tneType)  );

		if ((tneType == SG_TREENODEENTRY_TYPE_DIRECTORY) && (dsFlags == SG_DIFFSTATUS_FLAGS__MODIFIED))
		{
			// ignore the __MODIFIED bit for directories because it is noise
			// and only indicates that something *within* the directory changed.
			// we require an exact match on the flags.
#if TRACE_MRG_DIRT
			const char * pszRepoPath;
			SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_repo_path(pCtx, pOD, &pszRepoPath)  );
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "MRG_DIRT: Ignoring MODIFIED bit on directory [%s]\n", pszRepoPath)  );
#endif
		}
		else if (dsFlags == SG_DIFFSTATUS_FLAGS__FOUND)
		{
#if TRACE_MRG_DIRT
			const char * pszRepoPath;
			SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_repo_path(pCtx, pOD, &pszRepoPath)  );
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "MRG_DIRT: Allowing FOUND item [%s]\n", pszRepoPath)  );
#endif
		}
		else
		{
			const char * pszRepoPath;
			SG_ERR_IGNORE(  SG_treediff2__ObjectData__get_repo_path(pCtx, pOD, &pszRepoPath)  );
#if TRACE_MRG_DIRT
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "MRG_DIRT: Disallowing dirty item [0x%x][%s]\n", dsFlags, pszRepoPath)  );
#endif
			SG_ERR_THROW2(  SG_ERR_MERGE_REQUESTED_CLEAN_WD,
							(pCtx, "The entry [%s] is dirty [dsFlags 0x%x].",
							 pszRepoPath, dsFlags)  );
		}

		SG_ERR_CHECK(  SG_treediff2__iterator__next(pCtx, pIter, &bOK, &pszGid, &pOD)  );
	}
	SG_TREEDIFF2_ITERATOR_NULLFREE(pCtx, pIter);

	// whatever dirt we found is permitted

	pMrg->pTreeDiff_Baseline_WD_Dirt = pTreeDiff;
	return;
		
fail:
	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_TREEDIFF2_ITERATOR_NULLFREE(pCtx, pIter);
}

//////////////////////////////////////////////////////////////////

/**
 * An entry was FOUND in the WD that wasn't under version control in the BASELINE.
 * 
 * We should try to just leave it alone in the WD so that it appears as FOUND in
 * the WD after the MERGE is applied using our final result.
 *
 * HOWEVER, there are various ways that the FOUND item can interfere with the MERGE.
 *
 * TODO 2010/07/02 For some types of interference (such as a FOUND foo.c~ vs an
 * TODO            ADDED version in the other branch), we *might* want to just
 * TODO            move the FOUND version to a foo.c~~sg00~ file and go on -OR-
 * TODO            we might just want to throw an interference error and quit.
 * TODO            For others (such as a FOUND foo.c in a directory that the
 * TODO            other branch deleted) we just need to stop so that we don't
 * TODO            orphan the file or worse delete their only copy of something
 * TODO            that they forgot to ADD before their last COMMIT.
 * TODO
 * TODO            Think about some of these cases after we have some experience
 * TODO            with this in practice and see if we want to try to do more 
 * TODO            MOVES than THROWS.
 */
static void _mrg__try_to_handle_dirt_cb__found(SG_context * pCtx,
											   SG_mrg * pMrg,
											   const char * pszStaleRepoPath,
											   const char * pszGidParent,
											   const char * pszGidObject,
											   const char * pszEntryname)
{
	SG_mrg_cset_dir * pMrgCSetDirParent;			// we do not own this
	SG_rbtree_iterator * pIter = NULL;
	SG_portability_dir * pPort = NULL;
	SG_bool bParentWillBePresentInFinalResult;

	// NOTE: Because this is a FOUND item, the GID is fabricated freshly for
	//       this treediff instance, so it won't match anything in any of
	//       the csets.  We only drag "pszGidObject" along here because the
	//       portability collider wants a unique key.

	// see what happened to the FOUND item's parent directory in the final result.

	SG_ERR_CHECK(  SG_rbtree__find(pCtx,
								   pMrg->pMrgCSet_FinalResult->prbDirs,
								   pszGidParent,
								   &bParentWillBePresentInFinalResult,
								   (void **)&pMrgCSetDirParent)  );
	if (bParentWillBePresentInFinalResult)
	{
		SG_mrg_cset_entry * pMrgCSetEntryChild;			// we do not own this
		SG_utf8_converter * pConverterRepoCharSet;		// we do not own this
		SG_varray * pvaWarnings;						// we do not own this
		const char * pszGidEntryChild;
		SG_portability_flags portMask;
		SG_portability_flags portWarningsLogged = SG_PORT_FLAGS__NONE;
		SG_bool bOK;
		SG_bool bIsDuplicate;
		SG_bool bIgnoreWarnings;

		// The parent directory will be present in the final result.
		// 
		// Does this entryname collide with something under version
		// control (something ADDED by a non-baseline branch, for
		// example)?

		// build a portability-collider with the complete contents of the directory
		// in the final result.  don't worry about pathname length issues, or invalid
		// filename issues, or collisions amongst them (all that has already been
		// taken care of elsewhere).

		SG_ERR_CHECK(  SG_pendingtree__get_port_settings(pCtx, pMrg->pPendingTree,
														 &pConverterRepoCharSet, &pvaWarnings, &portMask, &bIgnoreWarnings)  );
		SG_ERR_CHECK(  SG_portability_dir__alloc(pCtx, portMask, NULL, pConverterRepoCharSet, pvaWarnings, &pPort)  );
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, pMrgCSetDirParent->prbEntry_Children,
												  &bOK, &pszGidEntryChild, (void **)&pMrgCSetEntryChild)  );
		while (bOK)
		{
			SG_ERR_CHECK(  SG_portability_dir__add_item__with_assoc(pCtx, pPort,
																	pMrgCSetEntryChild->bufGid_Entry,
																	SG_string__sz(pMrgCSetEntryChild->pStringEntryname),
																	SG_FALSE, SG_TRUE, pMrgCSetEntryChild, NULL,
																	&bIsDuplicate, NULL)  );
			SG_ASSERT(  (!bIsDuplicate)  );

			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter, 
													 &bOK, &pszGidEntryChild, (void **)&pMrgCSetEntryChild)  );
		}

		// now add the FOUND item's entryname to the collider and see if
		// collides (or potentially collides) with stuff from the final result.

		SG_ERR_CHECK(  SG_portability_dir__add_item__with_assoc(pCtx, pPort,
																pszGidObject,
																pszEntryname,
																SG_FALSE, SG_TRUE, NULL, NULL,
																&bIsDuplicate, (void **)&pMrgCSetEntryChild)  );
		if (bIsDuplicate)
		{
			// This FOUND entryname conflicts exactly with something under version control.
			//
			// TODO 2010/07/02 We may want to examine the FOUND item and see if it
			// TODO            *exactly* matches something that was ADDED in the other
			// TODO            branch and silently allow it.  That is, assume both
			// TODO            users were going to add the exact same file and just
			// TODO            make it work.
			// TODO
			// TODO            But for now, just throw as if they are different somehow.

			SG_ERR_THROW2(  SG_ERR_MERGE_REQUESTED_CLEAN_WD,
							(pCtx, "An entry found in the working directory interferes with the merge [%s]; entryname collision.",
							 pszStaleRepoPath)  );
		}

		if (!bIgnoreWarnings)
		{
			SG_ERR_CHECK(  SG_portability_dir__get_result_flags(pCtx, pPort, NULL, NULL, &portWarningsLogged)  );
			if (portWarningsLogged != SG_PORT_FLAGS__NONE)
			{
				SG_ERR_THROW2(  SG_ERR_MERGE_REQUESTED_CLEAN_WD,
								(pCtx, "An entry found in the working directory possibly interferes with the merge [%s]; potential portability issue.",
								 pszStaleRepoPath)  );
			}
		}

		// no collisions or potential collisions, so we allow it.

#if TRACE_MRG_DIRT
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "MRG_DIRT: Allow [%s]\n",
								   pszStaleRepoPath)  );
#endif
	}
	else
	{
		// The parent directory in the WD won't be present in the final result.

		SG_bool bParentDeleted = SG_FALSE;

		if (pMrg->pMrgCSet_FinalResult->prbDeletes)
			SG_ERR_CHECK(  SG_rbtree__find(pCtx, pMrg->pMrgCSet_FinalResult->prbDeletes, pszGidParent,
										   &bParentDeleted, NULL)  );

		if (bParentDeleted)
		{
			// The parent directory WAS under version control in the BASELINE and will
			// be DELETED from the final result.  This will create an orphan of this
			// FOUND file.
			//
			// We could probably assert that the parent is either not in the treediff
			// or if it is, does not have FOUND status.

			SG_ERR_THROW2(  SG_ERR_MERGE_REQUESTED_CLEAN_WD,
							(pCtx, "An entry found in the working directory interferes with the merge [%s]; parent directory deleted.",
							 pszStaleRepoPath)  );
		}
		else
		{
			// The parent directory wasn't present in the BASELINE and won't be present
			// in the final result.  So it must have been a FOUND directory, right?

			SG_bool bParentInTreeDiff;
			const SG_treediff2_ObjectData * pOD_Parent;
			SG_diffstatus_flags dsFlagsParent;

			SG_ERR_CHECK(  SG_treediff2__get_ObjectData(pCtx, pMrg->pTreeDiff_Baseline_WD_Dirt, pszGidParent,
														&bParentInTreeDiff, &pOD_Parent)  );
			if (!bParentInTreeDiff)
				SG_ERR_THROW2(  SG_ERR_ASSERT,
								(pCtx, "Parent directory of entry [%s] not found in treediff.", pszStaleRepoPath)  );
			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx,pOD_Parent,&dsFlagsParent)  );
			if ((dsFlagsParent & SG_DIFFSTATUS_FLAGS__FOUND) == 0)
				SG_ERR_THROW2(  SG_ERR_ASSERT,
								(pCtx, "Parent directory of entry [%s] has unexpected status [0x%x].",
								 pszStaleRepoPath, dsFlagsParent)  );

			// we cannot decide if this entry interferes with the merge,
			// rather we need to wait for our parent directory to be
			// processed.
			//
			// This case happens for each of the files in something
			// like ".../Debug/*.obj" because we probably have an IGNORE
			// for "Debug".

#if TRACE_MRG_DIRT
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
									   "MRG_DIRT: Conditionally Allow [%s]\n",
									   pszStaleRepoPath)  );
#endif
		}
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
	SG_PORTABILITY_DIR_NULLFREE(pCtx, pPort);
}

//////////////////////////////////////////////////////////////////

static SG_treediff2_foreach_callback _mrg__try_to_handle_dirt_cb;

static void _mrg__try_to_handle_dirt_cb(SG_context * pCtx,
										SG_treediff2 * pTreeDiff,
										const char * pszGidObject,
										const SG_treediff2_ObjectData * pOD,
										void * pVoidData_Mrg)
{
	SG_mrg * pMrg = (SG_mrg *)pVoidData_Mrg;
	SG_diffstatus_flags dsFlags;
	const char * pszStaleRepoPath;
	const char * pszEntryname;
	const char * pszGidParent;
	SG_treenode_entry_type tneType;

	SG_UNUSED( pTreeDiff );

	// NOTE: We cannot use the repo-path stored in the treediff to actually do
	//       anything because parent directories in the path may be moving in
	//       the final result.  We only get the repo-path here so we can print
	//       it in various messages.
	//
	//       The only thing we can rely on is the entryname and the (baseline)
	//       parent GID.

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_repo_path(pCtx, pOD, &pszStaleRepoPath)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_name(pCtx, pOD, &pszEntryname)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_parent_gid(pCtx, pOD, &pszGidParent)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_tneType(pCtx, pOD, &tneType)  );

#if TRACE_MRG_DIRT
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   "MRG_DIRT: Looking for interference for [%s] in [%s / %s]\n",
							   pszStaleRepoPath, pszGidParent, pszEntryname)  );
#endif

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD, &dsFlags)  );
	if ((tneType == SG_TREENODEENTRY_TYPE_DIRECTORY) && (dsFlags == SG_DIFFSTATUS_FLAGS__MODIFIED))
	{
		// ignore the __MODIFIED bit for directories because it is noise
		// and only indicates that something *within* the directory changed.
		// we require an exact match on the flags.
	}
	else if (dsFlags & SG_DIFFSTATUS_FLAGS__FOUND)
	{
		SG_ERR_CHECK(  _mrg__try_to_handle_dirt_cb__found(pCtx, pMrg, pszStaleRepoPath, pszGidParent, pszGidObject, pszEntryname)  );
	}
	else
	{
		// the filter in _mrg__look_for_dirt() is different.
		SG_ERR_THROW(  SG_ERR_ASSERT  );
	}

fail:
	return;
}


static void _mrg__handle_dirt(SG_context * pCtx, SG_mrg * pMrg)
{
	if (!pMrg->pTreeDiff_Baseline_WD_Dirt)
		return;

	SG_ERR_CHECK_RETURN(  SG_treediff2__foreach(pCtx, pMrg->pTreeDiff_Baseline_WD_Dirt, _mrg__try_to_handle_dirt_cb, pMrg)  );
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_DIRT_H
