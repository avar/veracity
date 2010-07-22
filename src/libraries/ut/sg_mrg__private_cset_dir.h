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
 * @file sg_mrg__private_cset_dir.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_CSET_DIR_H
#define H_SG_MRG__PRIVATE_CSET_DIR_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

#if TRACE_MRG
struct _dh_data
{
	SG_uint32			indent;
	SG_mrg_cset *		pMrgCSet;
};

static void _trace_msg__dir_list_as_hierarchy__print_entry_metadata(SG_context * pCtx, SG_uint32 indent, const SG_mrg_cset_entry * pMrgCSetEntry)
{
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,("%*c[gid-entry %s] [parent gid %s] [attr 0x%x][blob %s][xattr %s] %s\n"),
							   indent,' ',
							   pMrgCSetEntry->bufGid_Entry,
							   pMrgCSetEntry->bufGid_Parent,
							   ((SG_uint32)pMrgCSetEntry->attrBits),
							   pMrgCSetEntry->bufHid_Blob,
							   pMrgCSetEntry->bufHid_XAttr,
							   SG_string__sz(pMrgCSetEntry->pStringEntryname))  );

	if (pMrgCSetEntry->pMrgCSetEntryConflict)
	{
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,("%*c***CONFLICT*** [flags 0x%08x]\n"),
								   indent+20,' ',
								   pMrgCSetEntry->pMrgCSetEntryConflict->flags)  );
		SG_ERR_IGNORE(  _trace_msg__dump_entry_conflict_flags(pCtx,pMrgCSetEntry->pMrgCSetEntryConflict->flags,indent+24)  );
	}

	if (pMrgCSetEntry->pMrgCSetEntryCollision)
	{
		SG_uint32 j,nr;
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,("%*c***COLLISION*** with:\n"),indent+20,' ')  );
		SG_ERR_CHECK_RETURN(  SG_vector__length(pCtx,pMrgCSetEntry->pMrgCSetEntryCollision->pVec_MrgCSetEntry,&nr)  );
		for (j=0; j<nr; j++)
		{
			SG_mrg_cset_entry * pMrgCSetEntry_j;

			SG_ERR_CHECK_RETURN(  SG_vector__get(pCtx,pMrgCSetEntry->pMrgCSetEntryCollision->pVec_MrgCSetEntry,j,(void **)&pMrgCSetEntry_j)  );
			if (pMrgCSetEntry_j != pMrgCSetEntry)
				SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,("%*c[gid-entry %s]\n"),indent+24,' ',pMrgCSetEntry_j->bufGid_Entry)  );
		}
	}

	if (pMrgCSetEntry->portFlagsLogged)
	{
		char buf[100];
		SG_hex__format_uint64(buf,pMrgCSetEntry->portFlagsLogged);
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,("%*c***PORTABILITY*** [flags %s]\n"),
								   indent+20,' ',buf)  );
	}

}

/**
 * dump the meta-data on the given entry (as part of a nested dump).
 * if the entry is a directory, recurse and dump the contents of each entry
 * within the dirctory.
 */
static void _trace_msg__dir_list_as_hierarchy1(SG_context * pCtx,
											   const char * pszKey_Gid_Entry,
											   void * pVoidAssocData_MrgCSetDir,
											   void * pVoid_Data)
{
	SG_mrg_cset_dir * pMrgCSetDir = (SG_mrg_cset_dir *)pVoidAssocData_MrgCSetDir;
	struct _dh_data * pdhData = (struct _dh_data *)pVoid_Data;

	SG_rbtree_iterator * pIter = NULL;
	const char * pszKey_Child;
	SG_mrg_cset_entry * pMrgCSetEntry_Child;
	SG_mrg_cset_dir * pMrgCSetDir_Child;
	SG_bool bHaveChild;
	SG_bool bFound_Child_Dir;

	SG_ASSERT( (pMrgCSetDir->pMrgCSetEntry_Self) );
	SG_ASSERT( (strcmp(pszKey_Gid_Entry,pMrgCSetDir->bufGid_Entry_Self) == 0) );

	SG_ERR_IGNORE(  _trace_msg__dir_list_as_hierarchy__print_entry_metadata(pCtx,pdhData->indent,pMrgCSetDir->pMrgCSetEntry_Self)  );

	// dump info for all entries within this directory.

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,&pIter,pMrgCSetDir->prbEntry_Children,&bHaveChild,&pszKey_Child,(void **)&pMrgCSetEntry_Child)  );
	while (bHaveChild)
	{
		if (pMrgCSetEntry_Child->tneType != SG_TREENODEENTRY_TYPE_DIRECTORY)
		{
			SG_ERR_IGNORE(  _trace_msg__dir_list_as_hierarchy__print_entry_metadata(pCtx,pdhData->indent+4,pMrgCSetEntry_Child)  );
		}
		else
		{
			struct _dh_data dh_data;

			dh_data.indent = pdhData->indent + 4;
			dh_data.pMrgCSet = pdhData->pMrgCSet;

			SG_ERR_CHECK(  SG_rbtree__find(pCtx,pdhData->pMrgCSet->prbDirs,pszKey_Child,&bFound_Child_Dir,(void **)&pMrgCSetDir_Child)  );
			SG_ASSERT( bFound_Child_Dir );

			SG_ERR_CHECK(  _trace_msg__dir_list_as_hierarchy1(pCtx,pszKey_Child,(void *)pMrgCSetDir_Child,&dh_data)  );
		}

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bHaveChild,&pszKey_Child,(void **)&pMrgCSetEntry_Child)  );
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx,pIter);
}

/**
 * dump the flat directory list in the CSET *as if* it was a nested hierarchy.
 */
static void _trace_msg__dir_list_as_hierarchy(SG_context * pCtx,
											  const char * pszLabel,
											  SG_mrg_cset * pMrgCSet)
{
	struct _dh_data dh_data;

	dh_data.indent = 8;
	dh_data.pMrgCSet = pMrgCSet;

	SG_NULLARGCHECK_RETURN(pMrgCSet);

	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,("%s: Directory Hierarchy:\n"
												  "    CSET: [name %s] [hidCSet %s]\n"),
							   pszLabel,
							   SG_string__sz(pMrgCSet->pStringName),
							   ((pMrgCSet->bufHid_CSet[0]) ? pMrgCSet->bufHid_CSet : "(null)"))  );
	if (!pMrgCSet->pMrgCSetDir_Root)
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,("%*c<hierarchy not computed>\n"),dh_data.indent,' ')  );
	else
		SG_ERR_IGNORE(  _trace_msg__dir_list_as_hierarchy1(pCtx,
														   pMrgCSet->pMrgCSetDir_Root->bufGid_Entry_Self,
														   pMrgCSet->pMrgCSetDir_Root,
														   &dh_data)  );
}
#endif

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_dir__free(SG_context * pCtx,
						   SG_mrg_cset_dir * pMrgCSetDir)
{
	if (!pMrgCSetDir)
		return;

	SG_RBTREE_NULLFREE(pCtx, pMrgCSetDir->prbEntry_Children);		// we do not own the actual entries
	SG_PORTABILITY_DIR_NULLFREE(pCtx, pMrgCSetDir->pPortabilityDir);
	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pMrgCSetDir->prbCollisions, (SG_free_callback *)SG_mrg_cset_entry_collision__free);

	// we do not own pMrgCSet
	// we do not own pMrgCSetEntry_Self

	SG_NULLFREE(pCtx, pMrgCSetDir);
}

void SG_mrg_cset_dir__alloc__from_entry(SG_context * pCtx,
										SG_mrg_cset * pMrgCSet,
										SG_mrg_cset_entry * pMrgCSetEntry,
										SG_mrg_cset_dir ** ppMrgCSetDir)
{
	SG_mrg_cset_dir * pMrgCSetDir_Allocated = NULL;
	SG_mrg_cset_dir * pMrgCSetDir = NULL;

	SG_NULLARGCHECK_RETURN(pMrgCSet);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry);
	// ppMrgCSetDir is optional; the rbtree owns it anyway.

	SG_ASSERT(  (pMrgCSetEntry->tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)  );

	SG_ERR_CHECK(  SG_alloc1(pCtx,pMrgCSetDir_Allocated)  );
	SG_ERR_CHECK(  SG_gid__copy_into_buffer(pCtx,
											pMrgCSetDir_Allocated->bufGid_Entry_Self,SG_NrElements(pMrgCSetDir_Allocated->bufGid_Entry_Self),
											pMrgCSetEntry->bufGid_Entry)  );
	SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx,pMrgCSet->prbDirs,pMrgCSetDir_Allocated->bufGid_Entry_Self,(void *)pMrgCSetDir_Allocated)  );
	pMrgCSetDir = pMrgCSetDir_Allocated;		// rbtree owns it now
	pMrgCSetDir_Allocated = NULL;

	pMrgCSetDir->pMrgCSetEntry_Self = pMrgCSetEntry;

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx,&pMrgCSetDir->prbEntry_Children)  );
	pMrgCSetDir->pMrgCSet = pMrgCSet;

	if (pMrgCSetEntry->bufGid_Parent[0] == '\0')
	{
		SG_ASSERT(  (pMrgCSet->pMrgCSetDir_Root == NULL)  );
		pMrgCSet->pMrgCSetDir_Root = pMrgCSetDir;
	}

	// we defer creating pPortabilityDir until we need it.
	// we defer creating prbCollisions until we need it.

	if (ppMrgCSetDir)
		*ppMrgCSetDir = pMrgCSetDir;
	return;

fail:
	SG_MRG_CSET_DIR_NULLFREE(pCtx, pMrgCSetDir_Allocated);
}

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_dir__add_child_entry(SG_context * pCtx,
									  SG_mrg_cset_dir * pMrgCSetDir,
									  SG_mrg_cset_entry * pMrgCSetEntry)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetDir);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry);

	SG_ERR_CHECK_RETURN(  SG_rbtree__add__with_assoc(pCtx,
													 pMrgCSetDir->prbEntry_Children,
													 pMrgCSetEntry->bufGid_Entry,
													 pMrgCSetEntry)  );
}

//////////////////////////////////////////////////////////////////

struct _v_dir_data__check_for_collision
{
	SG_mrg *						pMrg;
	SG_mrg_cset *					pMrgCSet;					// we do not own this
	SG_utf8_converter *				pConverterRepoCharSet;		// we do not own this
	SG_varray *						pvaWarnings;				// we do not own this
	SG_portability_flags			portMask;
};

typedef struct _v_dir_data__check_for_collision _v_dir_data__check_for_collision;

//////////////////////////////////////////////////////////////////

/**
 * we get called once for each entry within the portability collider that
 * had an issue.  (Since we turned off the _INDIV_ checks, we will only
 * have collisions to think about.)
 */
static SG_portability_dir_foreach_callback _collect_portability_data;

static void _collect_portability_data(SG_context * pCtx,
									  SG_UNUSED_PARAM(const char * pszEntryname),
									  SG_UNUSED_PARAM(SG_portability_flags flagsWarnings),
									  SG_portability_flags flagsLogged,
									  void * pVoidAssocData_MrgCSetEntry,
									  SG_UNUSED_PARAM(void * pVoid_DirData))
{
	SG_mrg_cset_entry * pMrgCSetEntry = (SG_mrg_cset_entry *)pVoidAssocData_MrgCSetEntry;
	//_v_dir_data__check_for_collision * pDirData = (_v_dir_data__check_for_collision *)pVoid_DirData;

	SG_UNUSED(pszEntryname);
	SG_UNUSED(flagsWarnings);
	SG_UNUSED(pVoid_DirData);

	// TODO for now, just remember the logged-flags.  later i need to figure out what to do with them...

	pMrgCSetEntry->portFlagsLogged = flagsLogged;

#if TRACE_MRG
	{
		char buf[100];
		SG_hex__format_uint64(buf,flagsLogged);
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "_collect_portability_data: [entry %s] [flags %s] [%s]\n",
								   pMrgCSetEntry->bufGid_Entry,
								   buf,
								   SG_string__sz(pMrgCSetEntry->pStringEntryname))  );
	}
#else
	pCtx = NULL;	// quiets compiler on Windows.
#endif

}

//////////////////////////////////////////////////////////////////

static void _handle_new_collision(SG_context * pCtx,
								  SG_mrg_cset_dir * pMrgCSetDir,
								  SG_mrg_cset_entry * pMrgCSetEntry1,
								  SG_mrg_cset_entry * pMrgCSetEntry2)
{
	SG_mrg_cset_entry_collision * pMrgCSetEntryCollision = NULL;

	SG_NULLARGCHECK_RETURN(pMrgCSetDir);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry1);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry2);
	SG_ARGCHECK_RETURN( (pMrgCSetEntry1->pMrgCSetEntryCollision == NULL), pMrgCSetEntry1 );
	SG_ARGCHECK_RETURN( (pMrgCSetEntry2->pMrgCSetEntryCollision == NULL), pMrgCSetEntry2 );

	SG_ASSERT(  (strcmp(SG_string__sz(pMrgCSetEntry1->pStringEntryname),SG_string__sz(pMrgCSetEntry2->pStringEntryname)) == 0)  );

	// effectively create a subset of prbEntries containing 1 item
	// for each unique entrynane -- this is basically an equivalence
	// class for all of the files/symlinks/dirs that have the same
	// entryname.
	//
	// add the first 2 entries that we found with this entryname to
	// the class; if there are other entries with this entryname
	// they will be added to this equivalence class as our caller
	// finds them in their loop.
	//
	// also tell the CSET that this directory contains at least one
	// collision.

	SG_ERR_CHECK_RETURN(  SG_mrg_cset_entry_collision__alloc(pCtx,&pMrgCSetEntryCollision)  );
	SG_ERR_CHECK(  SG_mrg_cset_entry_collision__add_entry(pCtx,pMrgCSetEntryCollision,pMrgCSetEntry1)  );
	SG_ERR_CHECK(  SG_mrg_cset_entry_collision__add_entry(pCtx,pMrgCSetEntryCollision,pMrgCSetEntry2)  );

	if (!pMrgCSetDir->prbCollisions)
		SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pMrgCSetDir->prbCollisions)  );
	SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pMrgCSetDir->prbCollisions,
											  SG_string__sz(pMrgCSetEntry1->pStringEntryname),
											  (void *)pMrgCSetEntryCollision)  );	// dir owns collision now
	return;

fail:
	SG_MRG_CSET_ENTRY_COLLISION_NULLFREE(pCtx, pMrgCSetEntryCollision);
}

/**
 * record the association between 2 entries that collided.
 */
static void _handle_collision(SG_context * pCtx,
							  SG_mrg_cset_dir * pMrgCSetDir,
							  SG_mrg_cset_entry * pMrgCSetEntry1,
							  SG_mrg_cset_entry * pMrgCSetEntry2)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetDir);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry1);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry2);

#if TRACE_MRG
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_handle_collisions Exact match collision [entry-a %s][entry-b %s] %s\n",
							   pMrgCSetEntry1->bufGid_Entry,
							   pMrgCSetEntry2->bufGid_Entry,
							   SG_string__sz(pMrgCSetEntry1->pStringEntryname))  );
#endif

	// see if either entry already has a collision.  this can
	// happen in n-way merges, for example.  if so, just augment
	// the existing collision with the new value (by construction
	// we know that both can't already have a collision).

	if (pMrgCSetEntry1->pMrgCSetEntryCollision)
	{
		SG_ERR_CHECK_RETURN(  SG_mrg_cset_entry_collision__add_entry(pCtx,
																	 pMrgCSetEntry1->pMrgCSetEntryCollision,
																	 pMrgCSetEntry2)  );
	}
	else if (pMrgCSetEntry2->pMrgCSetEntryCollision)
	{
		// TODO this case may not be necessary since our caller processes the
		// TODO entries in GID-ENTRY order, we may never see this case.
		SG_ERR_CHECK_RETURN(  SG_mrg_cset_entry_collision__add_entry(pCtx,
																	 pMrgCSetEntry2->pMrgCSetEntryCollision,
																	 pMrgCSetEntry1)  );
	}
	else
	{
		SG_ERR_CHECK_RETURN(  _handle_new_collision(pCtx,pMrgCSetDir,pMrgCSetEntry1,pMrgCSetEntry2)  );
	}
}

/**
 * we get called once for each SG_mrg_cset_dir directory in the result-cset.
 * we want to look at all of the entries within our directory and check for
 * entryname (both real and potential) collisions.
 */
SG_rbtree_foreach_callback SG_mrg_cset_dir__check_for_collisions;

void SG_mrg_cset_dir__check_for_collisions(SG_context * pCtx,
										   SG_UNUSED_PARAM(const char * pszKey_Gid_Entry),
										   void * pVoidAssocData_MrgCSetDir,
										   void * pVoid_Data)
{
	SG_mrg_cset_dir * pMrgCSetDir = (SG_mrg_cset_dir *)pVoidAssocData_MrgCSetDir;
	_v_dir_data__check_for_collision * pDirData = (_v_dir_data__check_for_collision *)pVoid_Data;

	SG_rbtree_iterator * pIter = NULL;
	SG_string * pString_DirRepoPath = NULL;
	const char * pszGid_Entry_Child;
	SG_mrg_cset_entry * pMrgCSetEntry_Child;
	SG_bool bCheckMyName_Indiv = SG_FALSE;		// since all entrynames come from an ancestor or leaf, we shouldn't complain about them.
	SG_bool bCheckMyName_Collision = SG_TRUE;
	SG_bool bFound;

	// TODO for now check everything-vs-everything for potential collisions.
	// TODO later we may want to make this a little tighter and avoid reporting
	// TODO problems for things they have already agreed to.  for example, if
	// TODO ancestor or a single branch/leaf had a "Readme.txt" and a "README.TXT"
	// TODO and they were OK with it, and both files come into the merge (from
	// TODO the same cset) and without any renames/moves, then we shouldn't warn
	// TODO about it.  but if those 2 files came together for the first time from
	// TODO different csets, then we should raise a red flag.
	// TODO
	// TODO for now, we just raise the flag for everything.

	SG_UNUSED(pszKey_Gid_Entry);

	SG_NULLARGCHECK_RETURN(pMrgCSetDir);

	SG_ERR_CHECK(  SG_mrg_cset__compute_repo_path_for_entry(pCtx,pMrgCSetDir->pMrgCSet,
															pMrgCSetDir->bufGid_Entry_Self,
															SG_FALSE,
															&pString_DirRepoPath)  );

	// build a portability-collider for the directory and add all of
	// the entries in the directory to it.
	//
	// note that we only get exact-match conflicts as we are populating it.
	// we have to wait until we're finished before we get the potential
	// collisions.

	SG_ERR_CHECK(  SG_portability_dir__alloc(pCtx,
											 pDirData->portMask,
											 SG_string__sz(pString_DirRepoPath),
											 pDirData->pConverterRepoCharSet,
											 pDirData->pvaWarnings,
											 &pMrgCSetDir->pPortabilityDir)  );

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, pMrgCSetDir->prbEntry_Children,
											  &bFound, &pszGid_Entry_Child, (void **)&pMrgCSetEntry_Child)  );
	while (bFound)
	{
		SG_bool bIsDuplicate = SG_FALSE;
		SG_mrg_cset_entry * pMrgCSetEntry_FirstChild = NULL;	// what the kth-child collided with

		// if (pMrgCSetEntry_Child->pMrgCSetEntryConflict)
		// {
		//     // TODO 2010/04/26 In the case of divergent MOVES and RENAMED, we arbitrarily
		//     // TODO            assigned a parent directory and entryname to this entry
		//     // TODO            using either the baseline or the ancestor version of the tree
		//     // TODO            (in _v_process_ancestor_entry__hard_way()).
		//     // TODO
		//     // TODO            It is that chosen entryname that will be used here.  One could
		//     // TODO            argue that we should also add all of the other entrynames in
		//     // TODO            pMrgCSetEntry_Child->pMrgCSetEntryConflict->prbUnique_Entryname
		//     // TODO            as aliases so that we will trigger potential false (and non-false)
		//     // TODO            positives for other entries in the directory.  For example, if we
		//     // TODO            have:
		//     // TODO                    A
		//     // TODO                   / \.
		//     // TODO                  B   C
		//     // TODO                   \ /
		//     // TODO                    M
		//     // TODO
		//     // TODO            with:
		//     // TODO                A: add f1, f2
		//     // TODO                B: rename f1 fx   and   rename f2 fy
		//     // TODO                C: rename f1 fy
		//     // TODO
		//     // TODO            If B is the baseline, we will see the divergent rename on f1
		//     // TODO            and use the fx version, so the rename of f2 will not cause a
		//     // TODO            warning.  If the user chooses to resolve the f1 conflict by
		//     // TODO            picking fy, they will then get a collision conflict for the f2 version.
		//     // TODO
		//     // TODO            If C is the baseline, they will see both conflicts immediately.
		//     //
		//     // TODO 2010/04/26 There is a similar argument for divergent MOVES, but that doesn't
		//     // TODO            Belong in this loop; it would need to be done in an outer loop
		//     // TODO            where we are iterating over different directories.
		//     //
		//     // TODO 2010/04/26 We actually probably need to do BOTH sets of loops, so that
		//     // TODO            a divergent MOVE combined with a divergent RENAME would add
		//     // TODO            ALL of the name variations to ALL of the potential directories.
		// }

		if (!pMrgCSetEntry_Child->pvaPortLog)
			SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pMrgCSetEntry_Child->pvaPortLog)  );
		SG_ERR_CHECK(  SG_portability_dir__add_item__with_assoc(pCtx,
																pMrgCSetDir->pPortabilityDir,
																pMrgCSetEntry_Child->bufGid_Entry,
																SG_string__sz(pMrgCSetEntry_Child->pStringEntryname),
																bCheckMyName_Indiv,
																bCheckMyName_Collision,
																pMrgCSetEntry_Child,
																pMrgCSetEntry_Child->pvaPortLog,
																&bIsDuplicate,
																(void **)&pMrgCSetEntry_FirstChild)  );

		// TODO 2010/05/21 There's a problem here for exact match duplicates.  Since they don't
		// TODO            get added to the collider, they won't have a port-flags or port-log.
		// TODO            They will only have the collision bit.  So, for example, if we merge
		// TODO            "foo", "foo", and "FOO", the first "foo" and the "FOO" will have both
		// TODO            a collision bit and a port-flags/port-log; the second "foo" will just
		// TODO            have a collision bit.  I don't know if I care about this right now.

		if (bIsDuplicate)
			SG_ERR_CHECK(  _handle_collision(pCtx,pMrgCSetDir,pMrgCSetEntry_FirstChild,pMrgCSetEntry_Child)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter,
												 &bFound, &pszGid_Entry_Child, (void **)&pMrgCSetEntry_Child)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx,pIter);

	// extract the potential collisions from the collider and apply to the entries.

	SG_ERR_CHECK(  SG_portability_dir__foreach_with_issues(pCtx,pMrgCSetDir->pPortabilityDir,SG_FALSE,
														   _collect_portability_data,pDirData)  );

fail:
	SG_STRING_NULLFREE(pCtx, pString_DirRepoPath);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}

/**
 * TODO this funtion really belongs in SG_mrg__private_cset.h
 * TODO but to do this I need to rename and move _v_dir_data__check_for_collision
 * TODO to SG_mrg__private_typedefs.h
 **/
void SG_mrg_cset__check_dirs_for_collisions(SG_context * pCtx,
											SG_mrg_cset * pMrgCSet,
											SG_mrg * pMrg)
{
	_v_dir_data__check_for_collision data;

	SG_NULLARGCHECK_RETURN(pMrgCSet);
	SG_NULLARGCHECK_RETURN(pMrg);

	data.pMrg     = pMrg;
	data.pMrgCSet = pMrgCSet;

	SG_ERR_CHECK_RETURN(  SG_pendingtree__get_port_settings(pCtx, pMrg->pPendingTree,
															&data.pConverterRepoCharSet,
															&data.pvaWarnings,
															&data.portMask,
															NULL /*bIgnoreWarnings*/)  );

	// loop over all directories in the cset.
	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx, pMrgCSet->prbDirs,
											 SG_mrg_cset_dir__check_for_collisions,&data)  );
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_CSET_DIR_H
