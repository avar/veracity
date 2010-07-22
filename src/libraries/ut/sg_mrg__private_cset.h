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
 * @file sg_mrg__private_cset.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_CSET_H
#define H_SG_MRG__PRIVATE_CSET_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_mrg_cset__free(SG_context * pCtx, SG_mrg_cset * pMrgCSet)
{
	if (!pMrgCSet)
		return;

	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pMrgCSet->prbEntries, (SG_free_callback *)SG_mrg_cset_entry__free);
	// we do not own pMrgCSetEntry_Root

	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pMrgCSet->prbDirs, (SG_free_callback *)SG_mrg_cset_dir__free);
	// we do not own pMrgCSetDir_Root

	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pMrgCSet->prbConflicts, (SG_free_callback *)SG_mrg_cset_entry_conflict__free);

	SG_RBTREE_NULLFREE(pCtx, pMrgCSet->prbDeletes);		// we do not own the assoc-data

	SG_STRING_NULLFREE(pCtx, pMrgCSet->pStringName);
	SG_NULLFREE(pCtx, pMrgCSet);
}

void SG_mrg_cset__alloc(SG_context * pCtx,
						const char * pszInformalCSetName,
						SG_mrg_cset_origin origin,
						SG_daglca_node_type nodeType,
						SG_mrg_cset ** ppMrgCSet)
{
	SG_mrg_cset * pMrgCSet = NULL;

	SG_NONEMPTYCHECK_RETURN(pszInformalCSetName);
	SG_NULLARGCHECK_RETURN(ppMrgCSet);

	SG_ERR_CHECK(  SG_alloc1(pCtx,pMrgCSet)  );
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx,&pMrgCSet->prbEntries)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx,&pMrgCSet->pStringName,pszInformalCSetName)  );

	pMrgCSet->origin = origin;
	pMrgCSet->nodeType = nodeType;

	// we do not allocate prbDirs or set pMrgCSetDir_Root until we actually need it.
	// we do not allocate prbConflicts until we actually need it.
	// we do not allocate prbDeletes until we actually need it.

	*ppMrgCSet = pMrgCSet;
	return;

fail:
	SG_MRG_CSET_NULLFREE(pCtx, pMrgCSet);
}

//////////////////////////////////////////////////////////////////

/**
 * Load the entire contents of the version control tree as of
 * this CSET into memory.
 */
void SG_mrg_cset__load_entire_cset(SG_context * pCtx,
								   SG_mrg_cset * pMrgCSet,
								   SG_repo * pRepo,
								   const char * pszHid_CSet)
{
	SG_changeset * pCSet = NULL;
	SG_treenode * pTn_SuperRoot = NULL;
	const char * pszHid_SuperRoot;
	const SG_treenode_entry * pTne_Root;
	const char * pszGid_Root;

	SG_NULLARGCHECK_RETURN(pMrgCSet);
	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NONEMPTYCHECK_RETURN(pszHid_CSet);

	// we cannot "load" into a virtual CSET.
	SG_ARGCHECK_RETURN(  (pMrgCSet->origin == SG_MRG_CSET_ORIGIN__LOADED), pMrgCSet->origin );

	// we only want to load once into this CSET.
	SG_ARGCHECK_RETURN(  (pMrgCSet->bufHid_CSet[0] == '\0'), pMrgCSet->bufHid_CSet  );
	SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx,
									pMrgCSet->bufHid_CSet,SG_NrElements(pMrgCSet->bufHid_CSet),
									pszHid_CSet)  );

	// load the changeset from disk.
	// get the HID of the super-root treenode.
	// load the super-root treenode.

	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx,pRepo,pMrgCSet->bufHid_CSet,&pCSet)  );

	SG_ERR_CHECK(  SG_changeset__get_root(pCtx,pCSet,&pszHid_SuperRoot)  );
	if (!pszHid_SuperRoot || !*pszHid_SuperRoot)
		SG_ERR_THROW(  SG_ERR_MALFORMED_SUPERROOT_TREENODE  );

	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx,pRepo,pszHid_SuperRoot,&pTn_SuperRoot)  );

	// the super-root treenode should have exactly 1 entry: the "@/" directory which is the actual-root.

#if defined(DEBUG)
	{
		SG_uint32 nrEntries_SuperRoot;

		SG_ERR_CHECK(  SG_treenode__count(pCtx,pTn_SuperRoot,&nrEntries_SuperRoot)  );
		if (nrEntries_SuperRoot != 1)
			SG_ERR_THROW(  SG_ERR_MALFORMED_SUPERROOT_TREENODE  );
	}
#endif

	// get the treenode-entry of the actual-root.
	// get the name of the actual-root and verify that it is exactly "@/".
	// get the GID of the actual-root.

	SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx,pTn_SuperRoot,0,&pszGid_Root,&pTne_Root)  );
	SG_ERR_CHECK(  SG_gid__copy_into_buffer(pCtx,pMrgCSet->bufGid_Root,SG_NrElements(pMrgCSet->bufGid_Root),pszGid_Root)  );

#if defined(DEBUG)
	{
		const char * pszEntryname_Root;
		SG_treenode_entry_type tneType_Root;

		SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx,pTne_Root,&pszEntryname_Root)  );
		SG_ERR_CHECK(  SG_treenode_entry__get_entry_type(pCtx,pTne_Root,&tneType_Root)  );
		if ( (strcmp(pszEntryname_Root,"@") != 0) || (tneType_Root != SG_TREENODEENTRY_TYPE_DIRECTORY) )
			SG_ERR_THROW(  SG_ERR_MALFORMED_SUPERROOT_TREENODE  );
	}
#endif

	// use the treenode-entry of the actual-root and load the entire version control tree into memory.
	// we store the pMrgCSetEntry of the actual-root directory in pMrgCSet as a convenience.

	SG_ERR_CHECK(  SG_mrg_cset_entry__load(pCtx,pMrgCSet,pRepo,
										   NULL,pMrgCSet->bufGid_Root,
										   pTne_Root,
										   &pMrgCSet->pMrgCSetEntry_Root)  );

fail:
	SG_CHANGESET_NULLFREE(pCtx, pCSet);
	SG_TREENODE_NULLFREE(pCtx, pTn_SuperRoot);
}

//////////////////////////////////////////////////////////////////

static SG_rbtree_foreach_callback _set_marker_cb;

static void _set_marker_cb(SG_UNUSED_PARAM(SG_context * pCtx),
						   SG_UNUSED_PARAM(const char * pszKey),
						   void * pVoidAssocData, void * pVoidCallerData)
{
	SG_mrg_cset_entry * pMrgCSetEntry = (SG_mrg_cset_entry *)pVoidAssocData;
	SG_int64 * pNewValue = (SG_int64 *)pVoidCallerData;
	SG_int64 newValue = ((pNewValue) ? *pNewValue : 0);

	SG_UNUSED(pCtx);
	SG_UNUSED(pszKey);

	pMrgCSetEntry->markerValue = newValue;
}

void SG_mrg_cset__set_all_markers(SG_context * pCtx,
								  SG_mrg_cset * pMrgCSet,
								  SG_int64 newValue)
{
	SG_NULLARGCHECK_RETURN(pMrgCSet);

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,pMrgCSet->prbEntries,_set_marker_cb,&newValue)  );
}

//////////////////////////////////////////////////////////////////

void SG_mrg_cset__compute_repo_path_for_entry(SG_context * pCtx,
											  SG_mrg_cset * pMrgCSet,
											  const char * pszGid_Entry,
											  SG_bool bIncludeCSetName,
											  SG_string ** ppStringResult)		// caller must free this
{
	SG_string * pString = NULL;
	SG_mrg_cset_entry * pMrgCSetEntry;
	SG_bool bFound, bEqual;

	SG_NULLARGCHECK_RETURN(pMrgCSet);
	SG_NONEMPTYCHECK_RETURN(pszGid_Entry);
	SG_NULLARGCHECK_RETURN(ppStringResult);

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,pMrgCSet->prbEntries,pszGid_Entry,&bFound,(void **)&pMrgCSetEntry)  );
	if (!bFound)
		SG_ERR_THROW_RETURN(SG_ERR_NOT_FOUND);

	// go recursive on our parent and build the parent's repo-path, then append our entryname.

	bEqual = (0 == strcmp(pszGid_Entry,pMrgCSet->bufGid_Root));
	if (bEqual)
	{
		if (bIncludeCSetName)
			SG_ERR_CHECK_RETURN(  SG_string__alloc__format(pCtx,
														   &pString,
														   "[hidCSet %s] repo-path: %s",
														   SG_string__sz(pMrgCSet->pStringName),
														   SG_string__sz(pMrgCSetEntry->pStringEntryname))  );
		else
			SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC__COPY(pCtx,
														 &pString,
														 pMrgCSetEntry->pStringEntryname)  );
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_mrg_cset__compute_repo_path_for_entry(pCtx,
																	   pMrgCSet,
																	   pMrgCSetEntry->bufGid_Parent,
																	   bIncludeCSetName,
																	   &pString)  );
		SG_ERR_CHECK(  SG_string__append__sz(pCtx,pString,"/")  );
		SG_ERR_CHECK(  SG_string__append__string(pCtx,pString,pMrgCSetEntry->pStringEntryname)  );
	}

	// verify our back pointer -- that is, this entry should only be a
	// member of this cset.  we do not share entry's in more than one
	// cset, we clone them as necessary.
	SG_ASSERT(  (pMrgCSetEntry->pMrgCSet == pMrgCSet)  );

	*ppStringResult = pString;
	return;

fail:
	SG_STRING_NULLFREE(pCtx,pString);
}

//////////////////////////////////////////////////////////////////

/**
 * We get called once for each entry in the flat list SG_mrg_cset.prbEntries.
 * For each entry that is a directory, add a SG_mrg_cset_dir node to the flat
 * list of directories in SG_mrg_cset.prbDirs.
 *
 * That is, we build a list of the directories, but we DO NOT connect them
 * into a link them together into a tree.
 */
static SG_rbtree_foreach_callback _compute_dir_list1__entry;

static void _compute_dir_list1__entry(SG_context * pCtx,
									  SG_UNUSED_PARAM(const char * pszKey_Gid_Entry),
									  void * pVoidAssocData_MrgCSetEntry,
									  void * pVoid_Data)
{
	SG_mrg_cset_entry * pMrgCSetEntry = (SG_mrg_cset_entry *)pVoidAssocData_MrgCSetEntry;
	SG_mrg_cset * pMrgCSet = (SG_mrg_cset *)pVoid_Data;

	SG_UNUSED(pszKey_Gid_Entry);

	if (pMrgCSetEntry->tneType != SG_TREENODEENTRY_TYPE_DIRECTORY)
		return;

	SG_ERR_CHECK_RETURN(  SG_mrg_cset_dir__alloc__from_entry(pCtx,pMrgCSet,pMrgCSetEntry,NULL)  );
}

/**
 * We get called once for each entry in SG_mrg_cset.prbEntries.
 * For each entry, add a child record to the appropraite SG_mrg_cset_dir node.
 */
static SG_rbtree_foreach_callback _compute_dir_list2__entry;

static void _compute_dir_list2__entry(SG_context * pCtx,
									  SG_UNUSED_PARAM(const char * pszKey_Gid_Entry),
									  void * pVoidAssocData_MrgCSetEntry,
									  void * pVoid_Data)
{
	SG_mrg_cset_entry * pMrgCSetEntry = (SG_mrg_cset_entry *)pVoidAssocData_MrgCSetEntry;
	SG_mrg_cset * pMrgCSet = (SG_mrg_cset *)pVoid_Data;
	SG_mrg_cset_dir * pMrgCSetDir;
	SG_bool bFound;

	SG_UNUSED(pszKey_Gid_Entry);

	if (pMrgCSetEntry->bufGid_Parent[0] == 0)		// the root node is not a member of a parent directory.
		return;

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,pMrgCSet->prbDirs,pMrgCSetEntry->bufGid_Parent,&bFound,(void **)&pMrgCSetDir)  );
	SG_ASSERT(  bFound  );

	SG_ERR_CHECK_RETURN(  SG_mrg_cset_dir__add_child_entry(pCtx,pMrgCSetDir,pMrgCSetEntry)  );
}

//////////////////////////////////////////////////////////////////

void SG_mrg_cset__compute_cset_dir_list(SG_context * pCtx,
										SG_mrg_cset * pMrgCSet)
{
	SG_NULLARGCHECK_RETURN(pMrgCSet);

	if (SG_MRG_CSET__IS_FROZEN(pMrgCSet))
		SG_ERR_THROW_RETURN(SG_ERR_INVALID_WHILE_FROZEN);

	SG_ASSERT( (!pMrgCSet->pMrgCSetDir_Root) );	// this should get set as the list is built.

	SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pMrgCSet->prbDirs)  );

	// build a semi-hierarchial view of the CSET from the flat data in prbEntries.
	// actually, we create a list of directories and link each entry into the
	// appropriate directory node.
	//
	// this list is still flat -- all directories are in the same list.  (so you need
	// to jump thru the list to do a proper tree-walk.)
	//
	// because the entries are in random (GID) order, we need to do this in 2 steps:
	// [1] look at each entry and create a SG_mrg_cset_dir node for each directory and add
	//     it to the directory list.
	// [2] look at each entry and add it to the appropriate directory node.

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,pMrgCSet->prbEntries,_compute_dir_list1__entry,(void *)pMrgCSet)  );
	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,pMrgCSet->prbEntries,_compute_dir_list2__entry,(void *)pMrgCSet)  );

	SG_ASSERT( (pMrgCSet->pMrgCSetDir_Root) );		// this should get set as the list is built.
}

//////////////////////////////////////////////////////////////////

void SG_mrg_cset__register_conflict(SG_context * pCtx,
									SG_mrg_cset * pMrgCSet,
									SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict)
{
	SG_NULLARGCHECK_RETURN(pMrgCSet);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntryConflict);

	if (SG_MRG_CSET__IS_FROZEN(pMrgCSet))
		SG_ERR_THROW_RETURN(SG_ERR_INVALID_WHILE_FROZEN);

	SG_ASSERT( (pMrgCSet == pMrgCSetEntryConflict->pMrgCSet) );

	if (!pMrgCSet->prbConflicts)
		SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pMrgCSet->prbConflicts)  );

	SG_ERR_CHECK_RETURN(  SG_rbtree__add__with_assoc(pCtx,pMrgCSet->prbConflicts,
													 pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor->bufGid_Entry,
													 (void *)pMrgCSetEntryConflict)  );
}

//////////////////////////////////////////////////////////////////

void SG_mrg_cset__register_delete(SG_context * pCtx,
								  SG_mrg_cset * pMrgCSet,
								  SG_mrg_cset_entry * pMrgCSetEntry_Ancestor)
{
	SG_NULLARGCHECK_RETURN(pMrgCSet);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry_Ancestor);

	if (SG_MRG_CSET__IS_FROZEN(pMrgCSet))
		SG_ERR_THROW_RETURN(SG_ERR_INVALID_WHILE_FROZEN);

	// we want to delete from the result-cset, not the ancestor-cset (which is frozen)
	SG_ASSERT( (pMrgCSet != pMrgCSetEntry_Ancestor->pMrgCSet) );

	if (!pMrgCSet->prbDeletes)
		SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pMrgCSet->prbDeletes)  );

	// the delete-list is mainly a list of <gid-entry> for each entry that has
	// been deleted in the merge.  we cannot (er, don't want to bother with)
	// saying which branches/leaves did the delete.  we record the ancestor-entry
	// in the assoc-data as a convenience.

	SG_ERR_CHECK_RETURN(  SG_rbtree__add__with_assoc(pCtx,pMrgCSet->prbDeletes,
													 pMrgCSetEntry_Ancestor->bufGid_Entry,
													 (void *)pMrgCSetEntry_Ancestor)  );
}

//////////////////////////////////////////////////////////////////

void SG_mrg_cset__make_unique_entrynames(SG_context * pCtx,
										 SG_mrg_cset * pMrgCSet)
{
	SG_NULLARGCHECK_RETURN(pMrgCSet);

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx, pMrgCSet->prbEntries,
											 SG_mrg_cset_entry__make_unique_entrynames, NULL)  );
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_CSET_H
