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
 * @file sg_mrg__private_build_wd_issues.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_BUILD_WD_ISSUES_H
#define H_SG_MRG__PRIVATE_BUILD_WD_ISSUES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Build a VARRAY of conflict reason messages.
 * Returns NULL when there are no reasons.
 *
 * TODO do we want this to be debug only?
 */
static void _make_conflict_reasons(SG_context * pCtx, SG_varray ** ppvaConflictReasons, SG_mrg_cset_entry_conflict_flags flagsConflict)
{
	SG_varray * pva = NULL;

	if (flagsConflict)
	{
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );

#define M(f,bit,msg)	SG_STATEMENT( if ((f) & (bit)) SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx,pva,(msg))  ); )

		// TODO make these messages nicer....

		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_MOVE,						"REMOVE-vs-MOVE" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_RENAME,					"REMOVE-vs-RENAME" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_ATTRBITS,					"REMOVE-vs-ATTRBITS_CHANGE" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_XATTR,						"REMOVE-vs-XATTR_CHANGE" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_SYMLINK_EDIT,				"REMOVE-vs-SYMLINK_EDIT" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_FILE_EDIT,					"REMOVE-vs-FILE_EDIT" );

		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_CAUSED_ORPHAN,				"REMOVE-CAUSED-ORPHAN" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__MOVES_CAUSED_PATH_CYCLE,				"MOVES-CAUSED-PATH_CYCLE" );

		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_MOVE,						"DIVERGENT-MOVE" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_RENAME,					"DIVERGENT-RENAME" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_ATTRBITS,					"DIVERGENT-ATTRBITS_CHANGE" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_XATTR,						"DIVERGENT-XATTR_CHANGE" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_SYMLINK_EDIT,				"DIVERGENT-SYMLINK_EDIT" );

		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__TBD,			"DIVERGENT-FILE_EDIT_TBD" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__AUTO_FAILED,	"DIVERGENT-FILE_EDIT_AUTO_FAILED" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__NO_RULE,		"DIVERGENT-FILE_EDIT_NO_RULE" );
		M(flagsConflict, SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__AUTO_OK,		"DIVERGENT-FILE_EDIT_AUTO_MERGED" );

#undef M
	}

	*ppvaConflictReasons = pva;
	return;

fail:
	SG_VARRAY_NULLFREE(pCtx, pva);
}

//////////////////////////////////////////////////////////////////

static void _make_undeletes(SG_context * pCtx,
							SG_varray ** ppvaConflictUndeletes,
							SG_mrg_cset_entry * pMrgCSetEntry_FinalResult)
{
	SG_varray * pva = NULL;
	SG_uint32 k, kLimit;
	const SG_vector * pVec = pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pVec_MrgCSet_Deletes;

	if (pVec)
	{
		SG_ERR_CHECK(  SG_vector__length(pCtx, pVec, &kLimit)  );
		if (kLimit > 0)
		{
			SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );

			for (k=0; k<kLimit; k++)
			{
				SG_mrg_cset * pMrgCSet_k;

				SG_ERR_CHECK(  SG_vector__get(pCtx, pVec,k, (void **)&pMrgCSet_k)  );
				SG_ASSERT(  (pMrgCSet_k->bufHid_CSet[0] != 0)  );		// hid is only set for actual branch/leaf csets; it is null for merge-results.
				SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, pMrgCSet_k->bufHid_CSet)  );
			}
		}
	}

	*ppvaConflictUndeletes = pva;
	return;

fail:
	SG_VARRAY_NULLFREE(pCtx, pva);
}

//////////////////////////////////////////////////////////////////

/**
 * Used to display conflict details when the list of candidates are given
 * by keys in the given rbUnique *and* the associated values are a vector
 * of entries that have that key (whatever it may be).  This type of rbUnique
 * is used to support a many-to-one relationship.  For example, when we have
 * a divergent XAttrs and we want to list all of the HID-XATTRs (the keys)
 * that were present in the tree and we want to list the leaves/branches
 * where they occured -- keeping in mind that 2 or more leaves may have
 * changed the XAttrs to the same value.
 *
 * We build a VHASH[<key> --> VARRAY[]]
 *
 * For example, if branch L0 (with hid0) renames "foo" to "foo.0" and the
 * branches L1 (hid1) and L2 (hid2) rename "foo" to "foo.x", we need to
 * create (handwave):
 *     {
 *       "foo.x" : [ "hid1", "hid2" ],
 *       "foo.0" : [ "hid0" ],
 *     }
 */
static void _make_rbUnique(SG_context * pCtx,
						   SG_vhash ** ppvhDivergent,
						   SG_rbtree * prbUnique)
{
	SG_vhash * pvhDivergent = NULL;
	SG_varray * pvaDuplicates_k = NULL;
	SG_rbtree_iterator * pIter = NULL;

	if (prbUnique)
	{
		const char * pszKey_k;
		const SG_vector * pVec_k;
		SG_bool bFound;

		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhDivergent)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, prbUnique, &bFound, &pszKey_k, (void **)&pVec_k)  );
		while (bFound)
		{
			SG_uint32 nrDuplicates_k;
			const SG_mrg_cset_entry * pMrgCSetEntry_j;
			SG_uint32 j;

			SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pvaDuplicates_k)  );

			SG_ERR_CHECK(  SG_vector__length(pCtx, pVec_k, &nrDuplicates_k)  );
			for (j=0; j<nrDuplicates_k; j++)
			{
				SG_ERR_CHECK(  SG_vector__get(pCtx, pVec_k, j, (void **)&pMrgCSetEntry_j)  );
				SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pvaDuplicates_k, pMrgCSetEntry_j->pMrgCSet->bufHid_CSet)  );
			}

			SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvhDivergent, pszKey_k, &pvaDuplicates_k)  );	// this steals our VARRAY

			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter, &bFound, &pszKey_k, (void **)&pVec_k)  );
		}
	}

	*ppvhDivergent = pvhDivergent;
	pvhDivergent = NULL;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
	SG_VARRAY_NULLFREE(pCtx, pvaDuplicates_k);
	SG_VHASH_NULLFREE(pCtx, pvhDivergent);
}

/**
 * Used to display file-merge-plan when auto-merge failed or not attempted.
 *
 * WARNING: The entrynames created here must match the scheme used in the
 * WARNING: WD_Plan to populate the WD during the merge-apply.  See
 * WARNING: _sg_mrg__plan__alter_file_from_divergent_edit()
 */
static void _make_file_merge_plan(SG_context * pCtx,
								  SG_varray ** ppva,
								  SG_mrg_cset_entry * pMrgCSetEntry_FinalResult)
{
	SG_varray * pva = NULL;
	SG_vhash * pvh = NULL;
	SG_string * pString = NULL;
	SG_uint32 nrItems;

	SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pVec_AutoMergePlan)  );

	SG_ERR_CHECK(  SG_vector__length(pCtx, pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pVec_AutoMergePlan, &nrItems)  );
	SG_ASSERT(  (nrItems > 0)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );
	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );

	if (nrItems == 1)
	{
		const SG_mrg_automerge_plan_item * pItem_0;

		SG_ERR_CHECK(  SG_vector__get(pCtx, pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->pVec_AutoMergePlan, 0, (void **)&pItem_0)  );

		// in the normal 2-way merge case, only 1 step is possible.
		//
		// we use the traditional .yours/.mine scheme in the same
		// directory along side the original file.

		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );

		// TODO 2010/07/13 Write the HIDs of each version of the file to the VHASH.
		// TODO            This will allow us to have an UNRESOLVE option that could
		// TODO            recreate the ~mine/~other/~ancestor files after a RESOLVE
		// TODO            (that deleted them).

		SG_ERR_CHECK(  SG_string__sprintf(pCtx, pString, "%s~ancestor", SG_string__sz(pMrgCSetEntry_FinalResult->pStringEntryname))  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "ancestor", SG_string__sz(pString))  );

		SG_ERR_CHECK(  SG_string__sprintf(pCtx, pString, "%s~mine", SG_string__sz(pMrgCSetEntry_FinalResult->pStringEntryname))  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "mine", SG_string__sz(pString))  );

		SG_ERR_CHECK(  SG_string__sprintf(pCtx, pString, "%s~other", SG_string__sz(pMrgCSetEntry_FinalResult->pStringEntryname))  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "other", SG_string__sz(pString))  );

		SG_ERR_CHECK(  SG_string__sprintf(pCtx, pString, "%s", SG_string__sz(pMrgCSetEntry_FinalResult->pStringEntryname))  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "result", SG_string__sz(pString))  );

		// The "status" field in this step in the plan is used to indicate
		// if the content merge (either automatic or manual) has been successfully
		// performed.  Here we write either SG_MRG_AUTOMERGE_RESULT__{NOT_ATTEMPTED,CONFLICT}.
		// When the user does a RESOLVE, we will look at this and (maybe offer to) skip
		// launching the external merge tool (like DiffMerge).  After they do the merge,
		// we update this field so that if they get interrupted, they won't have to redo
		// the text merge.

		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, "status", pItem_0->result)  );

		SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pva, &pvh)  );		// this steals our VHASH
	}
	else
	{
		// TODO 2010/05/21 We haven't decided how we want to splat
		// TODO            all of the various intermediate versions
		// TODO            of the files in the WD in the general
		// TODO            n-way case.

		SG_ERR_THROW2(  SG_ERR_NOTIMPLEMENTED,
						(pCtx, "TODO MakeFileMergePlan: [count %d]", nrItems)  );
	}

	SG_STRING_NULLFREE(pCtx, pString);

	*ppva = pva;
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pString);
	SG_VARRAY_NULLFREE(pCtx, pva);
	SG_VHASH_NULLFREE(pCtx, pvh);
}

/**
 * Used to display collision details when multiple entries want the
 * same entryname in a directory.
 */
static void _make_collisions(SG_context * pCtx,
							 SG_varray ** ppva,
							 SG_vector * pVec_MrgCSetEntry)
{
	SG_varray * pva = NULL;
	SG_uint32 k, count;

	SG_ERR_CHECK(  SG_vector__length(pCtx, pVec_MrgCSetEntry, &count)  );
	SG_ASSERT(  (count > 0)  );

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );
	for (k=0; k<count; k++)
	{
		SG_mrg_cset_entry * pMrgCSetEntry_k;

		SG_ERR_CHECK(  SG_vector__get(pCtx, pVec_MrgCSetEntry, k, (void **)&pMrgCSetEntry_k)  );
		SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, pMrgCSetEntry_k->bufGid_Entry)  );
	}

	*ppva = pva;
	return;

fail:
	SG_VARRAY_NULLFREE(pCtx, pva);
}

//////////////////////////////////////////////////////////////////

/**
 * We get called once for each entry in the final-result.
 * We get called in RANDOM GID order.
 *
 * If this entry has a conflict, write the details to the WD_ISSUES.
 */
static SG_rbtree_foreach_callback _sg_mrg__prepare_issues__cb;

static void _sg_mrg__prepare_issues__cb(SG_context * pCtx,
										const char * pszKey_GidEntry,
										void * pVoidValue_MrgCSetEntry,
										void * pVoidData_Mrg)
{
	SG_mrg_cset_entry * pMrgCSetEntry_FinalResult = (SG_mrg_cset_entry *)pVoidValue_MrgCSetEntry;
	SG_mrg * pMrg = (SG_mrg *)pVoidData_Mrg;
	SG_vhash * pvhIssue = NULL;
	SG_string * pStringRepoPath = NULL;
	SG_varray * pva = NULL;
	SG_vhash * pvh = NULL;
	SG_mrg_cset_entry_conflict_flags flagsConflict;
	SG_portability_flags flagsPortability;
	SG_bool bHaveConflicts, bHaveCollision, bHavePortability, bHaveIssues, bOnlyAutoOK;
	SG_pendingtree_wd_issue_status status;

	flagsConflict = ((pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict)
					 ? pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->flags
					 : SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__ZERO);
	bHaveConflicts = (flagsConflict != SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__ZERO);

	bHaveCollision = (pMrgCSetEntry_FinalResult->pMrgCSetEntryCollision != NULL);

	flagsPortability = pMrgCSetEntry_FinalResult->portFlagsLogged;
	bHavePortability = (flagsPortability != SG_PORT_FLAGS__NONE);

	bHaveIssues = (bHaveConflicts || bHaveCollision || bHavePortability);
	if (!bHaveIssues)
		return;

	// if the ONLY problem was a divergent file edit and we successfully auto-merged
	// the content, we go ahead and generate the issue and mark the issue resolved.
	// this allow the user to see that we did the auto-merge (using --listall) but
	// not require them to manually mark it resolved.

	bOnlyAutoOK = (   (flagsConflict == SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__AUTO_OK)
				   && !bHaveCollision
				   && !bHavePortability);
	status = ((bOnlyAutoOK)
			  ? SG_ISSUE_STATUS__MARKED_RESOLVED
			  : SG_ISSUE_STATUS__ZERO);

	// this is the repo-path as it should exist in the final-result and in the WD
	// at the conclusion of the merge.  once the user starts resolving issues and/or
	// making more dirt in the WD, all bets are off.  (so we include the gid-parent
	// in the issue.)

	SG_ERR_CHECK(  SG_mrg_cset__compute_repo_path_for_entry(pCtx,
															pMrg->pMrgCSet_FinalResult,
															pszKey_GidEntry,
															SG_FALSE,
															&pStringRepoPath)  );

	// begin an ISSUE:
	// {
	//    "gid"                      : "<gid>",
	//    "status"                   : <status>,
	//
	//    "conflict_flags"           : <conflict_flags>,
	//    "collision_flags"          : <collision_flags>,
	//    "portability_flags"        : <portability_flags>,
	//
	//    "made_unique_entryname"    : <made_unique_entryname>,
	//    "entryname"                : "<entryname>",
	//    "gid_parent"               : "<gid_parent_directory>",
	//    "repopath"                 : "<repopath>",
	//
	//    "attrbits"                 : <attrBits>,
	//    "hid_xattrs"               : "<hid_xattrs>",

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhIssue)  );

	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhIssue, "gid",               pszKey_GidEntry)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(     pCtx, pvhIssue, "status",            status)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(     pCtx, pvhIssue, "conflict_flags",    flagsConflict)  );	// don't include __AUTO_OK bit
	SG_ERR_CHECK(  SG_vhash__add__bool(      pCtx, pvhIssue, "collision_flags",   bHaveCollision)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(     pCtx, pvhIssue, "portability_flags", flagsPortability)  );

	// indicate if we had to artificially resolve the entryname (either because
	// of a divergent rename or a collision or portability issue).
	SG_ERR_CHECK(  SG_vhash__add__bool(      pCtx, pvhIssue, "made_unique_entryname", pMrgCSetEntry_FinalResult->bMadeUniqueEntryname)  );

	// TODO 2010/07/10 Yank the repo-path and entryname fields.
	// TODO            Anyone who needs them should compute them
	// TODO            dynamically using the pendingtree.

	// the final entryname chosen (either from the branches or artificially by us)
	// should be the same as the final component in the repo-path, but it might be
	// convenient to have it as a separate field.  also note that this entryname
	// field is currently correct as of the time that we write out these ISSUES.
	// *but* it is time-sensitive, since the user may rename it or otherwise
	// manipulate the WD after the MERGE and before they RESOLVE this ISSUE.  so
	// if we have a "--fix" option, it may need to disregard this field and use
	// the GIDs to lookup the entry in the pendingtree.
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhIssue, "entryname",         SG_string__sz(pMrgCSetEntry_FinalResult->pStringEntryname))  );

	// the parent directory chosen for the entry (either from the branches or
	// artificially by us) should be the same as that implied by the repo-path.
	// but note that both the gid_parent and the repo-path are time-sensitive.
	// if the user manipulates the WD and/or resolves other issues, this entry
	// may or may not be in this directory or have this repo-path.
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhIssue, "gid_parent",        pMrgCSetEntry_FinalResult->bufGid_Parent)  );

	// the repo-path for the entry that we computed at the time the merge was
	// completed.  as with the entryname and gid_parent, this is is time-senstive.
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhIssue, "repopath",          SG_string__sz(pStringRepoPath))  );

	SG_ERR_CHECK(  SG_vhash__add__int64(     pCtx, pvhIssue, "attrbits",          pMrgCSetEntry_FinalResult->attrBits)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhIssue, "hid_xattrs",        ((pMrgCSetEntry_FinalResult->bufHid_XAttr[0])
																				   ? pMrgCSetEntry_FinalResult->bufHid_XAttr
																				   : SG_MRG_TOKEN__NO_XATTR))  );

	if (bHaveConflicts)
	{
		// add an array of messages for each bit in conflict_flags.
		// this is mostly for display purposes; use conflict_flags in code.
		//
		//    "conflict_reasons" : [ "<name1>", "<name2>", ... ],

		SG_ERR_CHECK(  _make_conflict_reasons(pCtx, &pva, flagsConflict)  );
		SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvhIssue, "conflict_reasons", &pva)  );

		if (flagsConflict & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__UNDELETE__MASK)
		{
			// entry was deleted in one or more branches and modified/needed in other branch(es).
			// we treat all the various way this can happen the same way since we don't really
			// care whether it was a __DELETE_VS_MOVE or a __DELETE_VS_RENAME.  all that really
			// matters (to the RESOLVER) is whether to keep the entry (as modified) or to delete
			// it from the final merge result.
			//
			// as a convenience to the user (and mostly for display purposes), we list the
			// branch(es) in which it was deleted.
			//
			// "conflict_deletes" : [ "<hid_cset_k>", ... ],

			SG_ERR_CHECK(  _make_undeletes(pCtx, &pva, pMrgCSetEntry_FinalResult)  );
			SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvhIssue, "conflict_deletes", &pva)  );
		}

		if (flagsConflict & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__MOVES_CAUSED_PATH_CYCLE)
		{
			// TODO decide if we want to list an array of the permuted repopaths.
			// TODO this may involve identifying the other moved entries that participated
			// TODO in the cycle.
		}

		if (flagsConflict & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_MOVE)
		{
			// we DO NOT bother to list the repo-paths of the various parent directory
			// choices since these may be invalid by the time the user is ready to
			// resolve this issue.  the resolver should use the gid-parent-x and the
			// pendingtree/WD to find the actual location of the parent directory at
			// the time of the resovlve.
			//
			// "conflict_divergent_moves" : { "<gid_parent_1>" : [ "<hid_cset_j>", ... ],
			//                                "<gid_parent_2>" : [ "<hid_cset_k>", ... ],
			//                                ... },

			SG_ERR_CHECK(  _make_rbUnique(pCtx, &pvh, pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->prbUnique_GidParent)  );
			SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhIssue, "conflict_divergent_moves", &pvh)  );
		}

		if (pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_RENAME)
		{
			// "conflict_divergent_renames" : { "<name_1>" : [ "<hid_cset_j>", ... ],
			//                                  "<name_2>" : [ "<hid_cset_k>", ... ],
			//                                  ... },

			SG_ERR_CHECK(  _make_rbUnique(pCtx, &pvh, pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->prbUnique_Entryname)  );
			SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhIssue, "conflict_divergent_renames", &pvh)  );
		}

		if (flagsConflict & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_ATTRBITS)
		{
			// "conflict_divergent_attrbits" : { "<attrbits_1>" : [ "<hid_cset_j>", ... ],
			//                                   "<attrbits_2>" : [ "<hid_cset_k>", ... ],
			//                                   ... },

			SG_ERR_CHECK(  _make_rbUnique(pCtx, &pvh, pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->prbUnique_AttrBits)  );
			SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhIssue, "conflict_divergent_attrbits", &pvh)  );
		}

		if (flagsConflict & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_XATTR)
		{
			// "conflict_divergent_hid_xattrs" : { "<hid_xattrs_1>" : [ "<hid_cset_j>", ... ],
			//                                     "<hid_xattrs_2>" : [ "<hid_cset_k>", ... ],
			//                                     ... },
			//
			// TODO 2010/05/20 I'm putting the HIDs of the various XATTR choices in the ISSUE.
			// TODO            And above we put the HID of the XATTR in the final-result.
			// TODO            Would it be better/more useful to fetch them from the REPO and
			// TODO            include them in the ISSUE?  I'm not sure that it's safe (or
			// TODO            necessary) to express the full XATTR in JSON....

			SG_ERR_CHECK(  _make_rbUnique(pCtx, &pvh, pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->prbUnique_HidXAttr)  );
			SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhIssue, "conflict_divergent_hid_xattrs", &pvh)  );
		}

		if (flagsConflict & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_SYMLINK_EDIT)
		{
			// "hid_symlink" : "<hid_blob>",
			// "conflict_divergent_hid_symlink" : { "<hid_blob_1>" : [ "<hid_cset_j>", ... ],
			//                                      "<hid_blob_2>" : [ "<hid_cset_k>", ... ],
			//                                      ... },
			//
			// TODO 2010/05/20 I'm putting the HIDs of the symlink content blob.  This is not
			// TODO            very display friendly.  Think about including the link target
			// TODO            in the ISSUE.

			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhIssue, "hid_symlink", pMrgCSetEntry_FinalResult->bufHid_Blob)  );

			SG_ERR_CHECK(  _make_rbUnique(pCtx, &pvh, pMrgCSetEntry_FinalResult->pMrgCSetEntryConflict->prbUnique_Symlink_HidBlob)  );
			SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhIssue, "conflict_divergent_hid_symlink", &pvh)  );
		}

		if (flagsConflict & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK__NOT_OK)
		{
			// when auto-merge failed or was not attempted, we splat the various versions of
			// the file into the WD usng the details in the auto-merge plan.  create a view
			// of that here in the ISSUE.
			//
			// "conflict_file_merge_plan" : [ { "ancestor" : "<entryname_ancestor_0>",
			//                                  "mine"     : "<entryname_mine_0>",
			//                                  "yours"    : "<entryname_yours_0>",
			//                                  "result"   : "<entryname_result_0>",
			//                                  "status"   : <status_0> },
			//                                { "ancestor" : "<entryname_ancestor_1>",
			//                                  "mine"     : "<entryname_mine_1>",
			//                                  "yours"    : "<entryname_yours_1>",
			//                                  "result"   : "<entryname_result_1>",
			//                                  "status"   : <status_1> },
			//                                ... ],

			SG_ERR_CHECK(  _make_file_merge_plan(pCtx, &pva, pMrgCSetEntry_FinalResult)  );
			SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvhIssue, "conflict_file_merge_plan", &pva)  );
		}
	}

	if (bHaveCollision)
	{
		// list the set of entries (including this entry) fighting for this entryname in this directory.
		// we only list the GIDs of the entries; we assume that each of them will have their own ISSUE
		// reported (with another copy of this list).  we CANNOT give the CSET HID for the leaf which
		// made the change that caused the collision to happen because we do not know it (and it may be
		// the result of combination of factors).
		//
		//     "collisions" : [ "<gid_1>", "<gid_2>", ... ],

		SG_ASSERT(  (pMrgCSetEntry_FinalResult->pMrgCSetEntryCollision->pVec_MrgCSetEntry)  );
		SG_ERR_CHECK(  _make_collisions(pCtx, &pva, pMrgCSetEntry_FinalResult->pMrgCSetEntryCollision->pVec_MrgCSetEntry)  );
		SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvhIssue, "collisions", &pva)  );	// this steals our varray
	}

	if (bHavePortability)
	{
		// add the entry's portability-log to the ISSUE.
		//
		//     "portability_details" : <see portability.c>
		//
		// TODO 2010/05/21 Is there any reason to try to clone the port-log and put that
		// TODO            in the ISSUE rather than just stealing this one?  Are there any
		// TODO            steps following this one that might need the log?

		SG_ASSERT(  (pMrgCSetEntry_FinalResult->pvaPortLog)  );
		SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvhIssue, "portability_details", &pMrgCSetEntry_FinalResult->pvaPortLog)  );

		// TODO 2010/07/10 Shouldn't we NULL pMrgCSetEntry_FinalResult->pvaPortLog since we stole it?
	}

	if (!pMrg->pvaIssues)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pMrg->pvaIssues)  );
	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pMrg->pvaIssues, &pvhIssue)  );	// this steals our vhash

fail:
	SG_STRING_NULLFREE(pCtx, pStringRepoPath);
	SG_VARRAY_NULLFREE(pCtx, pva);
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_VHASH_NULLFREE(pCtx, pvhIssue);
}

//////////////////////////////////////////////////////////////////

/**
 * Write an ISSUE to the array of ISSUES for each unresolved issue (conflict).
 * This is varary[vhash] and will be written to the wd.json so that later
 * commands can help the user RESOLVE them before they COMMIT.
 */
static void _sg_mrg__prepare_issues(SG_context * pCtx, SG_mrg * pMrg)
{
	// we only need to look at each entry in the final-result for conflicts.
	// unlike when building wd_plan, we do not need to look at the things
	// present in the baseline that were deleted from the final-result.

	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx, pMrg->pMrgCSet_FinalResult->prbEntries,
									  _sg_mrg__prepare_issues__cb, pMrg)  );

fail:
	return;
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_BUILD_WD_ISSUES_H
