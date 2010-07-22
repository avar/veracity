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
 * @file sg_mrg__private_cset_entry.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_CSET_ENTRY_H
#define H_SG_MRG__PRIVATE_CSET_ENTRY_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_entry__free(SG_context * pCtx, SG_mrg_cset_entry * pMrgCSetEntry)
{
	if (!pMrgCSetEntry)
		return;

	SG_VARRAY_NULLFREE(pCtx, pMrgCSetEntry->pvaPortLog);
	SG_STRING_NULLFREE(pCtx, pMrgCSetEntry->pStringEntryname);
	SG_NULLFREE(pCtx, pMrgCSetEntry);
}

/**
 * Allocate a new SG_mrg_cset_entry and add it to the CSET's entry-RBTREE.
 * We do not fill in any of the other fields.  We just allocate it and insert
 * it in the rbtree and set the back link.
 *
 * You DO NOT own the returned pointer.
 */
static void _sg_mrg_cset_entry__naked_alloc(SG_context * pCtx,
											SG_mrg_cset * pMrgCSet,
											const char * pszGid_Self,
											SG_mrg_cset_entry ** ppMrgCSetEntry_Allocated)
{
	SG_mrg_cset_entry * pMrgCSetEntry_Allocated = NULL;
	SG_mrg_cset_entry * pMrgCSetEntry;

	SG_NULLARGCHECK_RETURN(pMrgCSet);
	SG_NONEMPTYCHECK_RETURN(pszGid_Self);
	SG_NULLARGCHECK_RETURN(ppMrgCSetEntry_Allocated);

	SG_ERR_CHECK(  SG_alloc1(pCtx,pMrgCSetEntry_Allocated)  );
	SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx,pMrgCSet->prbEntries,pszGid_Self,(void *)pMrgCSetEntry_Allocated)  );
	pMrgCSetEntry = pMrgCSetEntry_Allocated;		// rbtree owns it now.
	pMrgCSetEntry_Allocated = NULL;

	pMrgCSetEntry->pMrgCSet = pMrgCSet;		// link back to the cset

	*ppMrgCSetEntry_Allocated = pMrgCSetEntry;
	return;

fail:
	SG_MRG_CSET_ENTRY_NULLFREE(pCtx,pMrgCSetEntry_Allocated);
}


void SG_mrg_cset_entry__load(SG_context * pCtx,
							 SG_mrg_cset * pMrgCSet,
							 SG_repo * pRepo,
							 const char * pszGid_Parent,	// will be null for actual-root directory
							 const char * pszGid_Self,
							 const SG_treenode_entry * pTne_Self,
							 SG_mrg_cset_entry ** ppMrgCSetEntry_New)
{
	SG_mrg_cset_entry * pMrgCSetEntry;
	const char * psz_temp;

	SG_NULLARGCHECK_RETURN(pMrgCSet);
	SG_NULLARGCHECK_RETURN(pRepo);
	// pszGid_Parent can be null
	SG_NONEMPTYCHECK_RETURN(pszGid_Self);
	SG_NULLARGCHECK_RETURN(pTne_Self);
	// ppMrgCSetEntry_New is optional

	if (SG_MRG_CSET__IS_FROZEN(pMrgCSet))
		SG_ERR_THROW_RETURN(SG_ERR_INVALID_WHILE_FROZEN);

	SG_ERR_CHECK_RETURN(  _sg_mrg_cset_entry__naked_alloc(pCtx,pMrgCSet,pszGid_Self,&pMrgCSetEntry)  );

	SG_ERR_CHECK(  SG_treenode_entry__get_attribute_bits(pCtx,pTne_Self,&pMrgCSetEntry->attrBits)  );

	SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx,pTne_Self,&psz_temp)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pMrgCSetEntry->pStringEntryname, psz_temp)  );

	SG_ERR_CHECK(  SG_treenode_entry__get_entry_type(pCtx,pTne_Self,&pMrgCSetEntry->tneType)  );

	if (pszGid_Parent && *pszGid_Parent)
		SG_ERR_CHECK(  SG_gid__copy_into_buffer(pCtx,pMrgCSetEntry->bufGid_Parent,SG_NrElements(pMrgCSetEntry->bufGid_Parent),pszGid_Parent)  );

	SG_ERR_CHECK(  SG_gid__copy_into_buffer(pCtx,pMrgCSetEntry->bufGid_Entry,SG_NrElements(pMrgCSetEntry->bufGid_Entry),pszGid_Self)  );

	SG_ERR_CHECK(  SG_treenode_entry__get_hid_xattrs(pCtx,pTne_Self,&psz_temp)  );
	if (psz_temp && *psz_temp)
		SG_ERR_CHECK(  SG_strcpy(pCtx,pMrgCSetEntry->bufHid_XAttr,SG_NrElements(pMrgCSetEntry->bufHid_XAttr),psz_temp)  );

	// TODO do we care about the TNE version?

	// we don't set bufHid_Blob for directories.
	SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx,pTne_Self,&psz_temp)  );
	if (pMrgCSetEntry->tneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
		SG_ERR_CHECK(  SG_mrg_cset_entry__load_subdir(pCtx,pMrgCSet,pRepo,pszGid_Self,pMrgCSetEntry,psz_temp)  );
	else
		SG_ERR_CHECK(  SG_strcpy(pCtx,pMrgCSetEntry->bufHid_Blob,SG_NrElements(pMrgCSetEntry->bufHid_Blob),psz_temp)  );

	// set the origin cset to our cset as a way of saying that we are the leaf/lca

	pMrgCSetEntry->pMrgCSet_CloneOrigin = pMrgCSet;

	if (ppMrgCSetEntry_New)
		*ppMrgCSetEntry_New = pMrgCSetEntry;

fail:
	return;
}

void SG_mrg_cset_entry__load_subdir(SG_context * pCtx,
									SG_mrg_cset * pMrgCSet,
									SG_repo * pRepo,
									const char * pszGid_Self,
									SG_mrg_cset_entry * pMrgCSetEntry_Self,
									const char * pszHid_Blob)
{
	SG_uint32 nrEntriesInDir, k;
	SG_treenode * pTn = NULL;

	SG_NULLARGCHECK_RETURN(pMrgCSet);
	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NONEMPTYCHECK_RETURN(pszGid_Self);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry_Self);
	SG_ARGCHECK_RETURN(  (pMrgCSetEntry_Self->tneType == SG_TREENODEENTRY_TYPE_DIRECTORY), tneType  );

	// TODO this would be a good place to hook into a treenode-cache so that
	// TODO we don't have to hit the disk each time.  granted, in any given
	// TODO CSET a treenode will only be referenced once, but we may have
	// TODO many CSETs in memory (when doing an Octopus-style merge, for example).
	// TODO also, the pendingtree and/or treediff code may also have loaded it,
	// TODO so we may have many copies....

	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx,pRepo,pszHid_Blob,&pTn)  );

	// walk contents of directory and load each entry.

	SG_ERR_CHECK(  SG_treenode__count(pCtx,pTn,&nrEntriesInDir)  );
	for (k=0; k<nrEntriesInDir; k++)
	{
		const char * pszGid_k;
		const SG_treenode_entry * pTne_k;

		SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx,pTn,k,&pszGid_k,&pTne_k)  );
		SG_ERR_CHECK(  SG_mrg_cset_entry__load(pCtx,pMrgCSet,pRepo,pszGid_Self,pszGid_k,pTne_k,NULL)  );
	}

fail:
	SG_TREENODE_NULLFREE(pCtx, pTn);
}

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_entry__equal(SG_context * pCtx,
							  const SG_mrg_cset_entry * pMrgCSetEntry_1,
							  const SG_mrg_cset_entry * pMrgCSetEntry_2,
							  SG_mrg_cset_entry_neq * pNeq)
{
	SG_mrg_cset_entry_neq neq = SG_MRG_CSET_ENTRY_NEQ__ALL_EQUAL;

	SG_NULLARGCHECK_RETURN(pMrgCSetEntry_1);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry_2);
	SG_NULLARGCHECK_RETURN(pNeq);

#if defined(DEBUG)
	// they must call us with the same entry/object from both versions of the tree.

	SG_ASSERT(  (strcmp(pMrgCSetEntry_1->bufGid_Entry,pMrgCSetEntry_2->bufGid_Entry) == 0)
				&&  "SG_mrg_cset_entry__equal: Entries refer to different objects."  );

	// an entry/object cannot change type between 2 different versions of the tree.
	SG_ASSERT(  (pMrgCSetEntry_1->tneType == pMrgCSetEntry_2->tneType)
				&&  "SG_mrg_cset_entry__equal: Entry changed type between versions."  );
#endif

	if (pMrgCSetEntry_1->attrBits != pMrgCSetEntry_2->attrBits)
		neq |= SG_MRG_CSET_ENTRY_NEQ__ATTRBITS;

	if (strcmp(SG_string__sz(pMrgCSetEntry_1->pStringEntryname),SG_string__sz(pMrgCSetEntry_2->pStringEntryname)) != 0)
		neq |= SG_MRG_CSET_ENTRY_NEQ__ENTRYNAME;

	if (strcmp(pMrgCSetEntry_1->bufGid_Parent,pMrgCSetEntry_2->bufGid_Parent) != 0)
		neq |= SG_MRG_CSET_ENTRY_NEQ__GID_PARENT;

	if (strcmp(pMrgCSetEntry_1->bufHid_XAttr,pMrgCSetEntry_2->bufHid_XAttr) != 0)
		neq |= SG_MRG_CSET_ENTRY_NEQ__HID_XATTR;

	switch (pMrgCSetEntry_1->tneType)
	{
	default:
		SG_ASSERT( (0) );	// quiets compiler

	case SG_TREENODEENTRY_TYPE_DIRECTORY:
		// since we don't set bufHid_Blob for directories, we don't include it in the
		// equality check when we are looking at a directory.
		break;

	case SG_TREENODEENTRY_TYPE_SYMLINK:
		// we DO NOT attempt any form of auto-merge on the pathname fragments in the symlinks.
		if (strcmp(pMrgCSetEntry_1->bufHid_Blob,pMrgCSetEntry_2->bufHid_Blob) != 0)
			neq |= SG_MRG_CSET_ENTRY_NEQ__SYMLINK_HID_BLOB;
		break;

	case SG_TREENODEENTRY_TYPE_REGULAR_FILE:
		// we only know the blob-hid for versions of the file that we loaded from
		// the repo.  for partial results of sub-merges, we don't yet know the HID
		// because we haven't done the auto-merge yet -or- we may have a file type
		// cannot be auto-merged.  so we treat them as non-equal.

		if (pMrgCSetEntry_1->bufHid_Blob[0] == 0)
		{
			SG_ASSERT(  (pMrgCSetEntry_1->pMrgCSetEntryConflict)  );
			SG_ASSERT(  (pMrgCSetEntry_1->pMrgCSetEntryConflict->pPathAutoMergePlanResult)  );
			neq |= SG_MRG_CSET_ENTRY_NEQ__FILE_HID_BLOB;
		}
		else if (pMrgCSetEntry_2->bufHid_Blob[0] == 0)
		{
			SG_ASSERT(  (pMrgCSetEntry_2->pMrgCSetEntryConflict)  );
			SG_ASSERT(  (pMrgCSetEntry_2->pMrgCSetEntryConflict->pPathAutoMergePlanResult)  );
			neq |= SG_MRG_CSET_ENTRY_NEQ__FILE_HID_BLOB;
		}
		else if (strcmp(pMrgCSetEntry_1->bufHid_Blob,pMrgCSetEntry_2->bufHid_Blob) != 0)
		{
			neq |= SG_MRG_CSET_ENTRY_NEQ__FILE_HID_BLOB;
		}
		break;
	}

	*pNeq = neq;
}

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_entry__clone_and_insert_in_cset(SG_context * pCtx,
												 const SG_mrg_cset_entry * pMrgCSetEntry_Src,
												 SG_mrg_cset * pMrgCSet_Result,
												 SG_mrg_cset_entry ** ppMrgCSetEntry_Result)
{
	SG_mrg_cset_entry * pMrgCSetEntry;

	SG_NULLARGCHECK_RETURN(pMrgCSetEntry_Src);
	SG_NULLARGCHECK_RETURN(pMrgCSet_Result);
	// ppMrgCSetEntry_Result is optional.  (we add it to the RBTREE.)

	if (SG_MRG_CSET__IS_FROZEN(pMrgCSet_Result))
		SG_ERR_THROW_RETURN(SG_ERR_INVALID_WHILE_FROZEN);

	// we cannot insert new entries into a CSET that we loaded from disk.
	SG_ARGCHECK_RETURN( (pMrgCSet_Result->origin == SG_MRG_CSET_ORIGIN__VIRTUAL), pMrgCSet_Result->origin );

	SG_ERR_CHECK_RETURN(  _sg_mrg_cset_entry__naked_alloc(pCtx,pMrgCSet_Result,pMrgCSetEntry_Src->bufGid_Entry,&pMrgCSetEntry)  );

	pMrgCSetEntry->markerValue = pMrgCSetEntry_Src->markerValue;
	pMrgCSetEntry->attrBits    = pMrgCSetEntry_Src->attrBits;
	SG_ERR_CHECK(  SG_STRING__ALLOC__COPY(pCtx, &pMrgCSetEntry->pStringEntryname, pMrgCSetEntry_Src->pStringEntryname)  );
	pMrgCSetEntry->tneType     = pMrgCSetEntry_Src->tneType;

	SG_ERR_CHECK(  SG_gid__copy_into_buffer(pCtx,
											pMrgCSetEntry->bufGid_Entry, SG_NrElements(pMrgCSetEntry->bufGid_Entry),
											pMrgCSetEntry_Src->bufGid_Entry )  );

	// bufHid_Blob is not set for directories.
	if (pMrgCSetEntry_Src->tneType != SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		SG_ERR_CHECK(  SG_strcpy(pCtx,
								 pMrgCSetEntry->bufHid_Blob, SG_NrElements(pMrgCSetEntry->bufHid_Blob),
								 pMrgCSetEntry_Src->bufHid_Blob)  );

		// when cloning a file (and inheriting the hid-blob), remember the CSET
		// that we inherited it from.  this lets _preview properly credit the original
		// source of the version when drawing the "graph".

		if (pMrgCSetEntry_Src->tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
		{
			if (pMrgCSetEntry_Src->pMrgCSet_FileHidBlobInheritedFrom)
				pMrgCSetEntry->pMrgCSet_FileHidBlobInheritedFrom = pMrgCSetEntry_Src->pMrgCSet_FileHidBlobInheritedFrom;
			else
				pMrgCSetEntry->pMrgCSet_FileHidBlobInheritedFrom = pMrgCSetEntry_Src->pMrgCSet;
		}
	}

	// bufHid_XAttr is empty when the entry doesn't have an XATTRS.
	if (pMrgCSetEntry_Src->bufHid_XAttr[0])
		SG_ERR_CHECK(  SG_strcpy(pCtx,
								 pMrgCSetEntry->bufHid_XAttr, SG_NrElements(pMrgCSetEntry->bufHid_XAttr),
								 pMrgCSetEntry_Src->bufHid_XAttr)  );

	if (pMrgCSetEntry_Src->bufGid_Parent[0])
	{
		// this entry has a parent
		SG_ERR_CHECK(  SG_gid__copy_into_buffer(pCtx,
												pMrgCSetEntry->bufGid_Parent, SG_NrElements(pMrgCSetEntry->bufGid_Parent),
												pMrgCSetEntry_Src->bufGid_Parent)  );
	}
	else
	{
		// this entry does not have a parent.  it must be the root.
		// stash a copy of this entry's GID in the result-cset.
		// FWIW, we could probably have assumed this GID when we allocated
		// the result-cset (since the root entry cannot be moved away) but
		// we wouldn't have allocated the new pMrgCSetEntry yet.
		SG_ASSERT(  (pMrgCSet_Result->bufGid_Root[0] == 0)  );
		SG_ASSERT(  (pMrgCSet_Result->pMrgCSetEntry_Root == NULL)  );

		SG_ERR_CHECK(  SG_gid__copy_into_buffer(pCtx,
												pMrgCSet_Result->bufGid_Root, SG_NrElements(pMrgCSet_Result->bufGid_Root),
												pMrgCSetEntry_Src->bufGid_Entry)  );
		pMrgCSet_Result->pMrgCSetEntry_Root = pMrgCSetEntry;
	}

	// remember the origin cset from which we cloned this entry.  this
	// should transend clones-of-clones (sub-merges) so that this field
	// always points back to a Leaf or LCA.  we use this to help label
	// things when we have collisions.

	pMrgCSetEntry->pMrgCSet_CloneOrigin = pMrgCSetEntry_Src->pMrgCSet_CloneOrigin;

	if (ppMrgCSetEntry_Result)
		*ppMrgCSetEntry_Result = pMrgCSetEntry;

fail:
	return;
}

/**
 * Forget that we inherited the hid-blob from an entry in another CSET.
 */
static void SG_mrg_cset_entry__forget_inherited_hid_blob(SG_context * pCtx,
														 SG_mrg_cset_entry * pMrgCSetEntry)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry);

	pMrgCSetEntry->bufHid_Blob[0] = 0;
	pMrgCSetEntry->pMrgCSet_FileHidBlobInheritedFrom = NULL;
}

//////////////////////////////////////////////////////////////////

SG_rbtree_foreach_callback SG_mrg_cset_entry__make_unique_entrynames;

void SG_mrg_cset_entry__make_unique_entrynames(SG_context * pCtx,
											   SG_UNUSED_PARAM(const char * pszKey_Gid_Entry),
											   void * pVoidAssocData_MrgCSetEntry,
											   SG_UNUSED_PARAM(void * pVoidCallerData))
{
	SG_mrg_cset_entry * pMrgCSetEntry = (SG_mrg_cset_entry *)pVoidAssocData_MrgCSetEntry;
	SG_bool bHadCollision, bHadEntrynameConflict, bHadPortabilityIssue;
	SG_bool bDeleteConflict;
	SG_bool bDivergentMove;
	SG_bool bMovesCausedPathCycle;

	SG_UNUSED(pVoidCallerData);
	SG_UNUSED(pszKey_Gid_Entry);

	if (pMrgCSetEntry->bMadeUniqueEntryname)
		return;

	bHadCollision         = (pMrgCSetEntry->pMrgCSetEntryCollision != NULL);

	// TODO are there other conflict flag bits we want this for?  such as delete-vs-rename?
	bHadEntrynameConflict = ((pMrgCSetEntry->pMrgCSetEntryConflict)
							 && ((pMrgCSetEntry->pMrgCSetEntryConflict->flags & (SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_RENAME)) != 0));

	// TODO i'm going to assume here that if portability warnings are turned off, the flags
	// TODO here won't ever be set.
	bHadPortabilityIssue  = (pMrgCSetEntry->portFlagsLogged != 0);

	// This is a bit of a hack, but I'm going to do it for now.
	// 
	// If the entry was DELETED on one side and ALTERED in any
	// way on the other side, we have a DELETE-vs-* conflict.
	// We preserve/restore the altered entry (temporarily
	// cancelling the delete if you will) and register a conflict
	// issue.
	//
	// All of this gets properly recorded in the conflict issue list.
	// *BUT* when the pendingtree gets written out, we have a slight
	// problem.
	// 
	// If the delete was in the baseline (and the non-delete change
	// is being folded in), then we restore the entry.  The
	// pendingtree will see this as an ADD (with the original GID).
	// That is fine.
	//
	// But when the non-delete change was in the baseline (and the
	// delete is being folded in), we simply preserve the entry in
	// WD and record the conflict in the conflict issue list.  The
	// problem is that simply preserving the entry means that it
	// isn't changed between the baseline and the final result, so
	// the code in pendingtree to filter out unchanged entries kicks
	// in and prunes it.
	//
	// So I'm going to give is a unique name (at least for now) to
	// both draw attention to it and to ensure that the ptnode is
	// dirty.
	bDeleteConflict = ((pMrgCSetEntry->pMrgCSetEntryConflict)
					   && ((pMrgCSetEntry->pMrgCSetEntryConflict->flags & (SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__UNDELETE__MASK)) != 0));

	// We have a similar problem with divergent moves, we have to
	// put/leave the entry somewhere, so we leave it in the directory
	// that it was in in the baseline.  So without any other markings,
	// the "preserved" version will be filtered out by the pendingtree.
	bDivergentMove = ((pMrgCSetEntry->pMrgCSetEntryConflict)
					  && ((pMrgCSetEntry->pMrgCSetEntryConflict->flags & (SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_MOVE)) != 0));

	// same thing for these, we had to leave them somewhere because
	// we couldn't move them properly.
	//
	// TODO I'm not sure I like to do this for divergent-moves and
	// TODO moves-caused-path-cycle, but I don't have time to debate
	// TODO that now.  It might be better to have the pendingtree
	// TODO filtering code be aware of the ISSUES and/or to actually
	// TODO put the issue in the ptnode so that the dirty check is
	// TODO all right there.
	bMovesCausedPathCycle = ((pMrgCSetEntry->pMrgCSetEntryConflict)
							 && ((pMrgCSetEntry->pMrgCSetEntryConflict->flags & (SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__MOVES_CAUSED_PATH_CYCLE)) != 0));

	if (!bHadCollision && !bHadEntrynameConflict && !bHadPortabilityIssue && !bDeleteConflict && !bDivergentMove && !bMovesCausedPathCycle)
		return;

	// append "~<cset-label>~<gid-prefix>~".  for example: "~L0~g123456~".

	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pMrgCSetEntry->pStringEntryname, "~")  );
	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pMrgCSetEntry->pStringEntryname,
												SG_string__sz(pMrgCSetEntry->pMrgCSet_CloneOrigin->pStringName))  );
	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pMrgCSetEntry->pStringEntryname, "~")  );
	SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, pMrgCSetEntry->pStringEntryname,
													 (const SG_byte *)pMrgCSetEntry->bufGid_Entry, 7)  );
	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pMrgCSetEntry->pStringEntryname, "~")  );

	pMrgCSetEntry->bMadeUniqueEntryname = SG_TRUE;

#if TRACE_MRG
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   ("_make_unique_entrynames: [gid %s][collision %d][conflict %d][portability %d] %s\n"),
							   pMrgCSetEntry->bufGid_Entry,
							   bHadCollision,
							   bHadEntrynameConflict,
							   bHadPortabilityIssue,
							   SG_string__sz(pMrgCSetEntry->pStringEntryname))  );
#endif
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_CSET_ENTRY_H
