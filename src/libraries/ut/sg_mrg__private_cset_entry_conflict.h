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
 * @file sg_mrg__private_cset_entry_conflict.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_CSET_ENTRY_CONFLICT_H
#define H_SG_MRG__PRIVATE_CSET_ENTRY_CONFLICT_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_entry_conflict__alloc(SG_context * pCtx,
									   SG_mrg_cset * pMrgCSet,
									   SG_mrg_cset_entry * pMrgCSetEntry_Ancestor,
									   SG_mrg_cset_entry * pMrgCSetEntry_Baseline,
									   SG_mrg_cset_entry_conflict ** ppMrgCSetEntryConflict)
{
	SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict = NULL;

	SG_NULLARGCHECK_RETURN(pMrgCSet);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry_Ancestor);
	// pMrgCSetEntry_Baseline may or may not be null
	SG_NULLARGCHECK_RETURN(ppMrgCSetEntryConflict);

	// we allocate and return this.  we DO NOT automatically add it to
	// the conflict-list in the cset.

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx,pMrgCSetEntryConflict)  );

	pMrgCSetEntryConflict->pMrgCSet = pMrgCSet;
	pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor = pMrgCSetEntry_Ancestor;
	pMrgCSetEntryConflict->pMrgCSetEntry_Baseline = pMrgCSetEntry_Baseline;
	// defer alloc of vectors until we need them.
	// defer alloc of rbtrees until we need them.

	*ppMrgCSetEntryConflict = pMrgCSetEntryConflict;
}

void SG_mrg_cset_entry_conflict__free(SG_context * pCtx,
									  SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict)
{
	if (!pMrgCSetEntryConflict)
		return;

	SG_VECTOR_I64_NULLFREE(pCtx, pMrgCSetEntryConflict->pVec_MrgCSetEntryNeq_Changes);
	SG_VECTOR_NULLFREE(pCtx, pMrgCSetEntryConflict->pVec_MrgCSetEntry_Changes);	// we do not own the pointers within
	SG_VECTOR_NULLFREE(pCtx, pMrgCSetEntryConflict->pVec_MrgCSet_Deletes);		// we do not own the pointers within

	// for the collapsable rbUnique's we own the vectors in the rbtree-values, but not the pointers within the vector
	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pMrgCSetEntryConflict->prbUnique_AttrBits,        (SG_free_callback *)SG_vector__free);
	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pMrgCSetEntryConflict->prbUnique_Entryname,       (SG_free_callback *)SG_vector__free);
	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pMrgCSetEntryConflict->prbUnique_GidParent,       (SG_free_callback *)SG_vector__free);
	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pMrgCSetEntryConflict->prbUnique_HidXAttr,        (SG_free_callback *)SG_vector__free);
	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pMrgCSetEntryConflict->prbUnique_Symlink_HidBlob, (SG_free_callback *)SG_vector__free);

	SG_RBTREE_NULLFREE(pCtx, pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult);

	// we delete the auto-merge result pathname (probably to a temp file)
	// but we do not delete the actual file.
	// [1] it may or may not exist.
	// [2] only our caller knows if the file will moved to the WD.
	SG_PATHNAME_NULLFREE(pCtx, pMrgCSetEntryConflict->pPathAutoMergePlanResult);

	// for each regular-file that is modified in multiple branches, we compute
	// an "auto-merge plan" -- a sequence of 2-way merges that cascade to a
	// final merge result.  whether we actually attempt the auto-merge depends
	// upon the file suffix and etc; we may just want to write a shell script
	// and let them figure it out.  here we free the plan (a vector of plan-items
	// containing pathnames); but we DO NOT delete the files from disk (if they
	// exist).
	SG_VECTOR_NULLFREE_WITH_ASSOC(pCtx, pMrgCSetEntryConflict->pVec_AutoMergePlan, (SG_free_callback *)SG_mrg_automerge_plan_item__free);

	SG_NULLFREE(pCtx, pMrgCSetEntryConflict);
}

//////////////////////////////////////////////////////////////////

/**
 * The values for RENAME, MOVE, ATTRBITS, XATTRS, and SYMLINKS are collapsable.  (see below)
 * In the corresponding rbUnique's we only need to remember the set of unique values for the
 * field.  THESE ARE THE KEYS IN THE prbUnique.
 *
 * As a convenience, we associate a vector of entries with each key.  These form a many-to-one
 * thing so that we can report all of the entries that have this value.
 *
 * Here we carry-forward the values from a sub-merge to the outer-merge by coping the keys
 * in the source-rbtree and insert in the destination-rbtree.
 */
static void _carry_forward_unique_values(SG_context * pCtx,
										 SG_rbtree * prbDest,
										 SG_rbtree * prbSrc)
{
	SG_rbtree_iterator * pIter = NULL;
	SG_vector * pVec_Allocated = NULL;
	const char * pszKey;
	SG_vector * pVec_Src;
	SG_vector * pVec_Dest;
	SG_uint32 j, nr;
	SG_bool bFound;


	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,&pIter,prbSrc,&bFound,&pszKey,(void **)&pVec_Src)  );
	while (bFound)
	{
		SG_ERR_CHECK(  SG_rbtree__find(pCtx,prbDest,pszKey,&bFound,(void **)&pVec_Dest)  );
		if (!bFound)
		{
			SG_ERR_CHECK(  SG_VECTOR__ALLOC(pCtx,&pVec_Allocated,3)  );
			SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx,prbDest,pszKey,pVec_Allocated)  );
			pVec_Dest = pVec_Allocated;
			pVec_Allocated = NULL;			// rbtree owns this now
		}

		SG_ERR_CHECK(  SG_vector__length(pCtx,pVec_Src,&nr)  );
		for (j=0; j<nr; j++)
		{
			SG_mrg_cset_entry * pMrgCSetEntry_x;

			SG_ERR_CHECK(  SG_vector__get(pCtx,pVec_Src,j,(void **)&pMrgCSetEntry_x)  );
			SG_ERR_CHECK(  SG_vector__append(pCtx,pVec_Dest,pMrgCSetEntry_x,NULL)  );

#if TRACE_MRG
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"_carry_forward_unique_value: [%s][%s]\n",
									   pszKey,
									   SG_string__sz(pMrgCSetEntry_x->pMrgCSet->pStringName))  );
#endif
		}

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bFound,&pszKey,NULL)  );
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx,pIter);
}

//////////////////////////////////////////////////////////////////

/**
 * Clone the leaf's auto-merge-plan and add it to ours.  We need to
 * insert the leaf's steps prior to ours so that the sub-merge results
 * are computed before we run the auto-merge on ours.
 */
static void _clone_automerge_plan(SG_context * pCtx,
								  SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict_Dest,
								  SG_mrg_cset_entry * pMrgCSetEntry_Leaf_k)
{
	SG_mrg_automerge_plan_item * pItem_Allocated = NULL;
	SG_uint32 k, nrSteps;
	SG_vector * pVec_Src;

	SG_ASSERT(  (   (pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict)
				 && (pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK)
				 && (pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->pVec_AutoMergePlan)
				 && (pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->pPathAutoMergePlanResult))  );

	if (!pMrgCSetEntryConflict_Dest->pVec_AutoMergePlan)
		SG_ERR_CHECK(  SG_VECTOR__ALLOC(pCtx,&pMrgCSetEntryConflict_Dest->pVec_AutoMergePlan,10)  );

	pVec_Src = pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->pVec_AutoMergePlan;
	SG_ERR_CHECK(  SG_vector__length(pCtx,pVec_Src,&nrSteps)  );
	for (k=0; k<nrSteps; k++)
	{
		const SG_mrg_automerge_plan_item * pItem_Src;

		SG_ERR_CHECK(  SG_vector__get(pCtx,pVec_Src,k,(void **)&pItem_Src)  );
		SG_ERR_CHECK(  SG_mrg_automerge_plan_item__copy(pCtx,&pItem_Allocated,pItem_Src)  );

#if TRACE_MRG
	SG_ERR_IGNORE(  SG_mrg_automerge_plan_item_debug__dump(pCtx,pItem_Allocated,"_clone_automerge_plan")  );
#endif

		SG_ERR_CHECK(  SG_vector__append(pCtx,pMrgCSetEntryConflict_Dest->pVec_AutoMergePlan,pItem_Allocated,NULL)  );
		pItem_Allocated = NULL;			// vector owns this now.
	}

	return;

fail:
	SG_MRG_AUTOMERGE_PLAN_ITEM_NULLFREE(pCtx,pItem_Allocated);
}

//////////////////////////////////////////////////////////////////

/**
 * The values for RENAME, MOVE, ATTRBITS, XATTRS, and SYMLINKS are collapsable.  (see below)
 * In the corresponding rbUnique's we only need to remember the set of unique values for the
 * field.  THESE ARE THE KEYS IN THE prbUnique.
 *
 * As a convenience, we associate a vector of entries with each key.  These form a many-to-one
 * thing so that we can report all of the entries that have this value.
 *
 * TODO since we should only process a cset once, we should not get any
 * TODO duplicates in the vector, but it might be possible.  i'm not going
 * TODO to worry about it now.  if this becomes a problem, consider doing
 * TODO a unique-insert into the vector -or- making the vector a sub-rbtree.
 *
 */
static void _update_1_rbUnique(SG_context * pCtx, SG_rbtree * prbUnique, const char * pszKey, SG_mrg_cset_entry * pMrgCSetEntry_Leaf_k)
{
	SG_vector * pVec_Allocated = NULL;
	SG_vector * pVec;
	SG_bool bFound;

	SG_ERR_CHECK(  SG_rbtree__find(pCtx,prbUnique,pszKey,&bFound,(void **)&pVec)  );
	if (!bFound)
	{
		SG_ERR_CHECK(  SG_VECTOR__ALLOC(pCtx,&pVec_Allocated,3)  );
		SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx,prbUnique,pszKey,pVec_Allocated)  );
		pVec = pVec_Allocated;
		pVec_Allocated = NULL;			// rbtree owns this now
	}

	SG_ERR_CHECK(  SG_vector__append(pCtx,pVec,pMrgCSetEntry_Leaf_k,NULL)  );

#if TRACE_MRG
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"_update_1_rbUnique: [%s][%s]\n",
									   pszKey,
									   SG_string__sz(pMrgCSetEntry_Leaf_k->pMrgCSet->pStringName))  );
#endif

	return;

fail:
	SG_VECTOR_NULLFREE(pCtx, pVec_Allocated);
}

void SG_mrg_cset_entry_conflict__append_change(SG_context * pCtx,
											   SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
											   SG_mrg_cset_entry * pMrgCSetEntry_Leaf_k,
											   SG_mrg_cset_entry_neq neq)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetEntryConflict);
	SG_NULLARGCHECK_RETURN(pMrgCSetEntry_Leaf_k);

	if (!pMrgCSetEntryConflict->pVec_MrgCSetEntry_Changes)
		SG_ERR_CHECK_RETURN(  SG_vector__alloc(pCtx,&pMrgCSetEntryConflict->pVec_MrgCSetEntry_Changes,2)  );

	SG_ERR_CHECK_RETURN(  SG_vector__append(pCtx,pMrgCSetEntryConflict->pVec_MrgCSetEntry_Changes,(void *)pMrgCSetEntry_Leaf_k,NULL)  );

	if (!pMrgCSetEntryConflict->pVec_MrgCSetEntryNeq_Changes)
		SG_ERR_CHECK_RETURN(  SG_vector_i64__alloc(pCtx,&pMrgCSetEntryConflict->pVec_MrgCSetEntryNeq_Changes,2)  );

	SG_ERR_CHECK_RETURN(  SG_vector_i64__append(pCtx,pMrgCSetEntryConflict->pVec_MrgCSetEntryNeq_Changes,(SG_int64)neq,NULL)  );

	//////////////////////////////////////////////////////////////////
	// add the value of the changed fields to the prbUnique_ rbtrees so that we can get a count of the unique new values.
	//
	//////////////////////////////////////////////////////////////////
	// the values for RENAME, MOVE, ATTRBITS, XATTRS, and SYMLINKS are collapsable.  that is, if we
	// have something like:
	//        A
	//       / \.
	//     L0   a0
	//         /  \.
	//        L1   L2
	//
	// and a rename in each Leaf, then we can either:
	// [a] prompt for them to choose L1 or L2's name and then
	//     prompt for them to choose L0 or the name from step 1.
	//
	// [b] prompt for them to choose L0, L1, or L2 in one question.
	//
	// unlike file-content-merging, the net-net is that we have 1 new value
	// that is one of the inputs (or maybe we let them pick a new onw), but
	// it is not a combination of them and so we don't need to display the
	// immediate ancestor in the prompt.
	//
	// so we carry-forward the unique values from the leaves for each of
	// these fields.  so the final merge-result may have more unique values
	// that it has direct parents.
	//////////////////////////////////////////////////////////////////

	if (neq & SG_MRG_CSET_ENTRY_NEQ__ATTRBITS)
	{
		SG_int_to_string_buffer buf;
		SG_int64_to_sz((SG_int64)pMrgCSetEntry_Leaf_k->attrBits, buf);

		if (!pMrgCSetEntryConflict->prbUnique_AttrBits)
			SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pMrgCSetEntryConflict->prbUnique_AttrBits)  );

		SG_ERR_CHECK_RETURN(  _update_1_rbUnique(pCtx,pMrgCSetEntryConflict->prbUnique_AttrBits,buf,pMrgCSetEntry_Leaf_k)  );

		if (pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict && pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->prbUnique_AttrBits)
			SG_ERR_CHECK_RETURN(  _carry_forward_unique_values(pCtx,
															   pMrgCSetEntryConflict->prbUnique_AttrBits,
															   pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->prbUnique_AttrBits)  );
	}

	if (neq & SG_MRG_CSET_ENTRY_NEQ__ENTRYNAME)
	{
		if (!pMrgCSetEntryConflict->prbUnique_Entryname)
			SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pMrgCSetEntryConflict->prbUnique_Entryname)  );

		SG_ERR_CHECK_RETURN(  _update_1_rbUnique(pCtx,
												 pMrgCSetEntryConflict->prbUnique_Entryname,
												 SG_string__sz(pMrgCSetEntry_Leaf_k->pStringEntryname),
												 pMrgCSetEntry_Leaf_k)  );

		if (pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict && pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->prbUnique_Entryname)
			SG_ERR_CHECK_RETURN(  _carry_forward_unique_values(pCtx,
															   pMrgCSetEntryConflict->prbUnique_Entryname,
															   pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->prbUnique_Entryname)  );
	}

	if (neq & SG_MRG_CSET_ENTRY_NEQ__GID_PARENT)
	{
		if (!pMrgCSetEntryConflict->prbUnique_GidParent)
			SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pMrgCSetEntryConflict->prbUnique_GidParent)  );

		SG_ERR_CHECK_RETURN(  _update_1_rbUnique(pCtx,pMrgCSetEntryConflict->prbUnique_GidParent,pMrgCSetEntry_Leaf_k->bufGid_Parent,pMrgCSetEntry_Leaf_k)  );

		if (pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict && pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->prbUnique_GidParent)
			SG_ERR_CHECK_RETURN(  _carry_forward_unique_values(pCtx,
															   pMrgCSetEntryConflict->prbUnique_GidParent,
															   pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->prbUnique_GidParent)  );
	}

	if (neq & SG_MRG_CSET_ENTRY_NEQ__HID_XATTR)
	{
		if (!pMrgCSetEntryConflict->prbUnique_HidXAttr)
			SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pMrgCSetEntryConflict->prbUnique_HidXAttr)  );

		if (pMrgCSetEntry_Leaf_k->bufHid_XAttr[0])
			SG_ERR_CHECK_RETURN(  _update_1_rbUnique(pCtx,pMrgCSetEntryConflict->prbUnique_HidXAttr,pMrgCSetEntry_Leaf_k->bufHid_XAttr,pMrgCSetEntry_Leaf_k)  );
		else
			SG_ERR_CHECK_RETURN(  _update_1_rbUnique(pCtx,pMrgCSetEntryConflict->prbUnique_HidXAttr,SG_MRG_TOKEN__NO_XATTR,pMrgCSetEntry_Leaf_k)  );

		if (pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict && pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->prbUnique_HidXAttr)
			SG_ERR_CHECK_RETURN(  _carry_forward_unique_values(pCtx,
															   pMrgCSetEntryConflict->prbUnique_HidXAttr,
															   pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->prbUnique_HidXAttr)  );
	}

	if (neq & SG_MRG_CSET_ENTRY_NEQ__SYMLINK_HID_BLOB)
	{
		if (!pMrgCSetEntryConflict->prbUnique_Symlink_HidBlob)
			SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pMrgCSetEntryConflict->prbUnique_Symlink_HidBlob)  );

		SG_ERR_CHECK_RETURN(  _update_1_rbUnique(pCtx,pMrgCSetEntryConflict->prbUnique_Symlink_HidBlob,pMrgCSetEntry_Leaf_k->bufHid_Blob,pMrgCSetEntry_Leaf_k)  );

		if (pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict && pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->prbUnique_Symlink_HidBlob)
			SG_ERR_CHECK_RETURN(  _carry_forward_unique_values(pCtx,
															   pMrgCSetEntryConflict->prbUnique_Symlink_HidBlob,
															   pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->prbUnique_Symlink_HidBlob)  );
	}

	//////////////////////////////////////////////////////////////////
	// we CANNOT DO the carry-forward for the file-content because we need to
	// do the pair-wise auto-merge with the ancestor.  so instead of flattening
	// the list of blob values, we build an auto-merge-plan that can be
	// executed later (either by us or by the user after we have exited).

	if (neq & SG_MRG_CSET_ENTRY_NEQ__FILE_HID_BLOB)
	{
		if (!pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult)
			SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult)  );

		// we only have the blob-hid for versions that we loaded from the repo.
		//
		// for partial results from sub-merges, we don't have a HID yet (because
		// we have not yet done the auto-merge -or- we may not have a handler
		// for that file type).  so we put the use the temp-file-name of the
		// final result in sub-merge's auto-merge-plan instead of the HID.
		//
		// we do this so that the unique-set will be complete and reflect all
		// of the incomming branches.  we don't want all of the sub-merges to
		// accidentally collapse to one (blank) hid.

		if (pMrgCSetEntry_Leaf_k->bufHid_Blob[0])
		{
			// we know what the blob-hid of the leaf is, so it must have been
			// something we loaded from the repo.  so either the leaf didn't
			// have any conflict -or- the conflict did not include the contents
			// of the file.
			//
			// we set the associated value to NULL because we haven't created
			// a temp file for it yet.

			SG_ASSERT(  (pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict == NULL)
						|| ((pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK) == 0)  );

			SG_ERR_CHECK_RETURN(  SG_rbtree__update__with_assoc(pCtx,
																pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult,
																pMrgCSetEntry_Leaf_k->bufHid_Blob,NULL,NULL)  );
		}
		else
		{
			// the Leaf has 2 or more versions of the file-content (and the flags probably has __DIVERGENT_FILE_EDIT__TBD).
			// add the Leaf's auto-merge-plan to our plan (so that those steps will happen before the steps we need).
			// add the temp-filename of the final merge-result to our unique list as a placeholder.

			SG_ERR_CHECK_RETURN(  _clone_automerge_plan(pCtx,pMrgCSetEntryConflict,pMrgCSetEntry_Leaf_k)  );

			SG_ERR_CHECK_RETURN(  SG_rbtree__update__with_assoc(pCtx,
																pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult,
																SG_pathname__sz(pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict->pPathAutoMergePlanResult),
																NULL,NULL)  );
		}
	}
}

void SG_mrg_cset_entry_conflict__append_delete(SG_context * pCtx,
											   SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
											   SG_mrg_cset * pMrgCSet_Leaf_k)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetEntryConflict);
	SG_NULLARGCHECK_RETURN(pMrgCSet_Leaf_k);

	if (!pMrgCSetEntryConflict->pVec_MrgCSet_Deletes)
		SG_ERR_CHECK_RETURN(  SG_vector__alloc(pCtx,&pMrgCSetEntryConflict->pVec_MrgCSet_Deletes,2)  );

	SG_ERR_CHECK_RETURN(  SG_vector__append(pCtx,pMrgCSetEntryConflict->pVec_MrgCSet_Deletes,(void *)pMrgCSet_Leaf_k,NULL)  );
}

//////////////////////////////////////////////////////////////////

/**
 * return the number of leaves that deleted the entry.
 */
void SG_mrg_cset_entry_conflict__count_deletes(SG_context * pCtx,
											   SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
											   SG_uint32 * pCount)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetEntryConflict);

	if (pMrgCSetEntryConflict->pVec_MrgCSet_Deletes)
		SG_ERR_CHECK_RETURN(  SG_vector__length(pCtx,pMrgCSetEntryConflict->pVec_MrgCSet_Deletes,pCount)  );
	else
		*pCount = 0;
}

/**
 * return the number of leaves that changed the entry in some way.
 */
void SG_mrg_cset_entry_conflict__count_changes(SG_context * pCtx,
											   SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
											   SG_uint32 * pCount)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetEntryConflict);

	if (pMrgCSetEntryConflict->pVec_MrgCSetEntry_Changes)
		SG_ERR_CHECK_RETURN(  SG_vector__length(pCtx,pMrgCSetEntryConflict->pVec_MrgCSetEntry_Changes,pCount)  );
	else
		*pCount = 0;
}

//////////////////////////////////////////////////////////////////

/**
 * if more than one leaf changed the attrbits, return the number of unique new values that it was set to.
 */
void SG_mrg_cset_entry_conflict__count_unique_attrbits(SG_context * pCtx,
													   SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
													   SG_uint32 * pCount)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetEntryConflict);

	if (pMrgCSetEntryConflict->prbUnique_AttrBits)
		SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx,pMrgCSetEntryConflict->prbUnique_AttrBits,pCount)  );
	else
		*pCount = 0;
}

/**
 * if more than one leaf renamed the entry, return the number of unique new values that it was set to.
 */
void SG_mrg_cset_entry_conflict__count_unique_entryname(SG_context * pCtx,
														SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
														SG_uint32 * pCount)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetEntryConflict);

	if (pMrgCSetEntryConflict->prbUnique_Entryname)
		SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx,pMrgCSetEntryConflict->prbUnique_Entryname,pCount)  );
	else
		*pCount = 0;
}

/**
 * if more than one leaf moved the entry (to another directory), return the number of unique new values that it was set to.
 */
void SG_mrg_cset_entry_conflict__count_unique_gid_parent(SG_context * pCtx,
														 SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
														 SG_uint32 * pCount)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetEntryConflict);

	if (pMrgCSetEntryConflict->prbUnique_GidParent)
		SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx,pMrgCSetEntryConflict->prbUnique_GidParent,pCount)  );
	else
		*pCount = 0;
}

/**
 * if more than one leaf changed the XATTRs, return the number of unique new values that it was set to.
 */
void SG_mrg_cset_entry_conflict__count_unique_hid_xattrs(SG_context * pCtx,
														 SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
														 SG_uint32 * pCount)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetEntryConflict);

	if (pMrgCSetEntryConflict->prbUnique_HidXAttr)
		SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx,pMrgCSetEntryConflict->prbUnique_HidXAttr,pCount)  );
	else
		*pCount = 0;
}

/**
 * if more than one leaf changed the contents of the symlink, return the number of
 * unique new values that it was set to.
 */
void SG_mrg_cset_entry_conflict__count_unique_symlink_hid_blob(SG_context * pCtx,
															   SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
															   SG_uint32 * pCount)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetEntryConflict);

	if (pMrgCSetEntryConflict->prbUnique_Symlink_HidBlob)
		SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx,pMrgCSetEntryConflict->prbUnique_Symlink_HidBlob,pCount)  );
	else
		*pCount = 0;
}

/**
 * if more than one leaf changed the contents of the file, return the number of
 * unique new values that it was set to.  For merges (or sub-merges) that reference
 * only real-in-the-repo versions, this will be the number of unique blob-hids;
 * when we are a merge that encompases a sub-merge, we may have blob-hids and
 * placeholders for the sub-merges.
 */
void SG_mrg_cset_entry_conflict__count_unique_file_hid_blob_or_result(SG_context * pCtx,
																	  SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
																	  SG_uint32 * pCount)
{
	SG_NULLARGCHECK_RETURN(pMrgCSetEntryConflict);

	if (pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult)
		SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx,pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult,pCount)  );
	else
		*pCount = 0;
}

//////////////////////////////////////////////////////////////////

/**
 * Append the plan-items for our leaves vs our ancestor.  Our plan-items
 * must appear after any plan-items required by sub-merges so that those
 * steps are completed before we need their output as our input.
 */
void SG_mrg_cset_entry_conflict__append_our_merge_plan(SG_context * pCtx,
													   SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
													   SG_mrg * pMrg)
{
	SG_mrg_automerge_plan_item * pItem_Allocated = NULL;
	SG_rbtree_iterator * pIter = NULL;
	const char * pszKey_HidBlob_or_AutoMergePlanResult_0;
	const char * pszKey_HidBlob_or_AutoMergePlanResult_1;
	const SG_pathname * pPath_0;
	const SG_pathname * pPath_1;
	SG_uint32 ndxLabels = 0;
	SG_uint32 ndxMerges = 1;
	SG_uint32 nrUnique;
	SG_uint32 nrLeaves;
	SG_bool bFound;

	SG_NULLARGCHECK_RETURN(pMrgCSetEntryConflict);

	SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__count_unique_file_hid_blob_or_result(pCtx,pMrgCSetEntryConflict,&nrUnique)  );
	SG_ASSERT(  (nrUnique > 1)  );

	if (!pMrgCSetEntryConflict->pVec_AutoMergePlan)
		SG_ERR_CHECK(  SG_VECTOR__ALLOC(pCtx,&pMrgCSetEntryConflict->pVec_AutoMergePlan,10)  );

	//////////////////////////////////////////////////////////////////
	// we don't care what kind of merge is happening.  all we care about is
	// how many branches/leaves changed the content of the entry.  that is,
	// how many ***UNIQUE*** BLOB-HIDs we have.
	//
	// That is, a simple 2-way merge, may look something like this and so we
	// have unique HIDs U0 and U1 for L0 and L1.
	//
	//      A
	//     / \.
	//   L0   L1
	//     \ /
	//      M
	//
	// In an Octopus-like merge, we may have something like this and unique
	// HIDs U0..Uj, j <= n-1.  That is, if multiple branches made the exact
	// same change, we don't have to deal with the duplicates.
	//
	//               A
	//    __________/ \__________
	//   /   /                   \.
	// L0  L1       ...           Ln-1
	//   \___\______   __________/
	//              \ /
	//               M
	//
	// So, we can treat both of these cases as
	//
	//                         F(A_A)
	//         _______________/      \__________
	//        /        /                        \.
	// F(A_U0)  F(A_U1)         ...              F(A_Uj)
	//        \________\______        __________/
	//                        \      /
	//                         F(A_M)
	//
	// Since all of these changes happened in parallel to the ancestor, we have
	// no expectation that there are any order dependencies.  (And we're probably
	// seeing them in HID-CSET or HID-BLOG order anyway and they are quite random.)
	// So, we can do a cascading merge.
	//
	//                         F(A_A)
	//       _________________/      \_________
	//      /         /
	// F(A_U0)   F(A_U1)
	//      \     /     .
	//      T(A_m1)   .
	//         \    .      .
	//         T(A_m2)   .
	//            \    .
	//            T(A_m3)
	//              ...
	//                F(A_M)
	//
	// That is, we can do a series of commands like this:
	//     diff3 foo.A_U0   foo.A_A foo.A_U1 > foo.A_m1
	//     diff3 foo.A_m1   foo.A_A foo.A_U2 > foo.A_m2
	//     ...
	//     diff3 foo.A_mj-1 foo.A_A foo.A_Uj > foo.A_M
	//
	// Each of the above lines is a "auto-merge-plan-item".  Here we build
	// a vector of them in the proper order.  At this point we are only building
	// the pathnames and recording the associated blob HIDs.
	//
	//////////////////////////////////////////////////////////////////
	//
	// If any of the Lx branches in this conflict are partial results from
	// an inner merge, our caller will have already taken care of the plan
	// steps for the inner merge (and seeded our plan) and added the final
	// sub-merge-result (instead of a blob HID) to our prbUnique.
	// For example,
	//
	//            F(A_A)
	//           /      \.
	//          /        F(a0_A)
	//         /        /       \.
	//        /      F(a0_U0)   F(a0_U1)
	//   F(A_U0)        \       /
	//        \          F(a0_M)
	//         \        /
	//          \      /
	//           F(A_M)
	//
	// Our plan should already have:
	//     diff3 foo.a0_U0 foo.a0_A foo.a0_U1 > foo.a0_M
	// And then we add:
	//     diff3 foo.A_U0  foo.A_A  foo.a0_M  > foo.A_M
	//
	//////////////////////////////////////////////////////////////////

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,&pIter,pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult,
											  &bFound,&pszKey_HidBlob_or_AutoMergePlanResult_0,(void **)&pPath_0)  );
	SG_ASSERT(  (bFound)  );

	SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bFound,&pszKey_HidBlob_or_AutoMergePlanResult_1,(void **)&pPath_1)  );
	SG_ASSERT(  (bFound)  );


	SG_ERR_CHECK(  SG_daglca__get_stats(pCtx,pMrg->pDagLca,NULL,NULL,&nrLeaves)  );
	if (nrLeaves == 2)
	{
		// a short-cut on the general cascade/recursive.
		//
		// when we have a 2-way merge (baseline vs other) we want to simplify
		// the way we label the versions of the file if we have to splat them
		// in the WD.  for the 2-way case we want to go with ~mine (left) and
		// ~other (right) rather than the more general ~A_Ux model.  (Because
		// we can and it is expected.)

		SG_bool bIsHid_0, bIsHid_1;

		SG_ERR_CHECK(  SG_mrg_automerge_plan_item__alloc(pCtx,pMrg,pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor,&pItem_Allocated)  );

		bIsHid_0 = (strchr(pszKey_HidBlob_or_AutoMergePlanResult_0,'/') == NULL);
		bIsHid_1 = (strchr(pszKey_HidBlob_or_AutoMergePlanResult_1,'/') == NULL);
		SG_ASSERT(  (bIsHid_0 && (pPath_1 == NULL))  );
		SG_ASSERT(  (bIsHid_1 && (pPath_0 == NULL))  );

		// decide which of the 2 HIDs correspond to the value of the baseline version.

		SG_ASSERT(  (pMrgCSetEntryConflict->pMrgCSetEntry_Baseline)  );
		if (strcmp(pszKey_HidBlob_or_AutoMergePlanResult_0, pMrgCSetEntryConflict->pMrgCSetEntry_Baseline->bufHid_Blob) == 0)
		{
			// key[0] is baseline ==> A_U0
			// key[1] is other    ==> A_U1

			SG_ASSERT(  (strcmp(pszKey_HidBlob_or_AutoMergePlanResult_1,
								pMrgCSetEntryConflict->pMrgCSetEntry_Baseline->bufHid_Blob) != 0)  );

			SG_ERR_CHECK(  SG_mrg_automerge_plan_item__set_mine_from_repo(pCtx,pItem_Allocated,ndxLabels++,
																		  pszKey_HidBlob_or_AutoMergePlanResult_0)  );
			SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx,pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult,
														 pszKey_HidBlob_or_AutoMergePlanResult_0,pItem_Allocated->pPath_Mine,NULL)  );

			SG_ERR_CHECK(  SG_mrg_automerge_plan_item__set_yours_from_repo(pCtx,pItem_Allocated,ndxLabels++,
																		   pszKey_HidBlob_or_AutoMergePlanResult_1)  );
			SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx,pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult,
														 pszKey_HidBlob_or_AutoMergePlanResult_1,pItem_Allocated->pPath_Yours,NULL)  );
		}
		else
		{
			// key[1] is baseline ==> A_U0
			// key[0] is other    ==> A_U1

			SG_ASSERT(  (strcmp(pszKey_HidBlob_or_AutoMergePlanResult_1,
								pMrgCSetEntryConflict->pMrgCSetEntry_Baseline->bufHid_Blob) == 0)  );

			SG_ERR_CHECK(  SG_mrg_automerge_plan_item__set_mine_from_repo(pCtx,pItem_Allocated,ndxLabels++,
																		  pszKey_HidBlob_or_AutoMergePlanResult_1)  );
			SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx,pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult,
														 pszKey_HidBlob_or_AutoMergePlanResult_1,pItem_Allocated->pPath_Mine,NULL)  );

			SG_ERR_CHECK(  SG_mrg_automerge_plan_item__set_yours_from_repo(pCtx,pItem_Allocated,ndxLabels++,
																		   pszKey_HidBlob_or_AutoMergePlanResult_0)  );
			SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx,pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult,
														 pszKey_HidBlob_or_AutoMergePlanResult_0,pItem_Allocated->pPath_Yours,NULL)  );
		}

		// final result gets named A_M
		SG_ERR_CHECK(  SG_mrg_automerge_plan_item__set_result(pCtx,pItem_Allocated)  );

		// remember the final _M file in the conflict so that our callers don't have to dig thru the vector.
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx,
												&pMrgCSetEntryConflict->pPathAutoMergePlanResult,
												pItem_Allocated->pPath_Result)  );

#if TRACE_MRG
		SG_ERR_IGNORE(  SG_mrg_automerge_plan_item_debug__dump(pCtx,pItem_Allocated,"SG_mrg_cset_entry_conflict__append_our_merge_plan (2-way)")  );
#endif

		SG_ERR_CHECK(  SG_vector__append(pCtx,pMrgCSetEntryConflict->pVec_AutoMergePlan,pItem_Allocated,NULL)  );
		pItem_Allocated = NULL;			// vector owns this now.
	}
	else
	{
		// the general case where we handle cascades and sub-merges.

		while (bFound)
		{
			SG_bool bIsHid;

			SG_ERR_CHECK(  SG_mrg_automerge_plan_item__alloc(pCtx,pMrg,pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor,&pItem_Allocated)  );

			// 2010/02/02 pszKey...[0] and pszKey...[1] can be either BLOB-HIDs or PATHNAMES for intermediate
			//            results.  when i originally wrote this, i used the __could_string_be_a_hid() function
			//            to tell me which it was.  now that we no longer have this method on the REPO API, i'm
			//            changing this to just look for a "/" in the string (since we keep all pathnames
			//            normalized); this should give us the same results.
			//
			// if we have an actual hid-blob in pszKey...[0] then we are referring to a blob in the repo.
			// if there is no associated value, then this is the first time that we have referenced this
			// blob in the plan.  so we need to create a temp file name for it (and eventually fetch the
			// blob into this file).  we update the value in the prbUnique so that we know where we put
			// this blob (which is usefull if/when preview wants to do a 2-way diff between the baseline
			// and the final-result).
			//
			// TODO in theory, this also prevents us from fetching the blob more than once if the same
			// TODO hid is referenced from more than one place in the DAG.

			bIsHid = (strchr(pszKey_HidBlob_or_AutoMergePlanResult_0,'/') == NULL);
			if (bIsHid && (pPath_0 == NULL))
				SG_ERR_CHECK(  SG_mrg_automerge_plan_item__set_mine_from_repo(pCtx,pItem_Allocated,ndxLabels++,
																			  pszKey_HidBlob_or_AutoMergePlanResult_0)  );
			else
				SG_ERR_CHECK(  SG_mrg_automerge_plan_item__set_mine_from_submerge(pCtx,pItem_Allocated,
																				  pszKey_HidBlob_or_AutoMergePlanResult_0)  );
			SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx,pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult,
														 pszKey_HidBlob_or_AutoMergePlanResult_0,pItem_Allocated->pPath_Mine,NULL)  );

			bIsHid = (strchr(pszKey_HidBlob_or_AutoMergePlanResult_1,'/') == NULL);
			if (bIsHid && (pPath_1 == NULL))
				SG_ERR_CHECK(  SG_mrg_automerge_plan_item__set_yours_from_repo(pCtx,pItem_Allocated,ndxLabels++,
																			   pszKey_HidBlob_or_AutoMergePlanResult_1)  );
			else
				SG_ERR_CHECK(  SG_mrg_automerge_plan_item__set_yours_from_submerge(pCtx,pItem_Allocated,
																				   pszKey_HidBlob_or_AutoMergePlanResult_1)  );
			SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx,pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult,
														 pszKey_HidBlob_or_AutoMergePlanResult_1,pItem_Allocated->pPath_Yours,NULL)  );

			if (ndxMerges < nrUnique-1)
			{
				// intermediate steps in cascade get _m1, _m2, ...
				SG_ERR_CHECK(  SG_mrg_automerge_plan_item__set_interior_result(pCtx,pItem_Allocated,ndxMerges++)  );

				// the _mx file becomes the left-side input for the next step in our plan.
				pszKey_HidBlob_or_AutoMergePlanResult_0 = SG_pathname__sz(pItem_Allocated->pPath_Result);
				pPath_0 = pItem_Allocated->pPath_Result;

				// next value in our prbUnique becomes the right-side for the next step.
				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bFound,&pszKey_HidBlob_or_AutoMergePlanResult_1,(void **)&pPath_1)  );
				SG_ASSERT(  (bFound)  );
			}
			else
			{
				// final step in cascade gets named _M
				SG_ERR_CHECK(  SG_mrg_automerge_plan_item__set_result(pCtx,pItem_Allocated)  );

				// remember the final _M file in the conflict so that our callers don't have to dig thru the vector.
				SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx,
														&pMrgCSetEntryConflict->pPathAutoMergePlanResult,
														pItem_Allocated->pPath_Result)  );

				// there should not be any more leaves
				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bFound,&pszKey_HidBlob_or_AutoMergePlanResult_1,NULL)  );
				SG_ASSERT(  (!bFound)  );
			}

#if TRACE_MRG
			SG_ERR_IGNORE(  SG_mrg_automerge_plan_item_debug__dump(pCtx,pItem_Allocated,"SG_mrg_cset_entry_conflict__append_our_merge_plan (n-way)")  );
#endif

			SG_ERR_CHECK(  SG_vector__append(pCtx,pMrgCSetEntryConflict->pVec_AutoMergePlan,pItem_Allocated,NULL)  );
			pItem_Allocated = NULL;			// vector owns this now.
		}
	}

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx,pIter);
	SG_MRG_AUTOMERGE_PLAN_ITEM_NULLFREE(pCtx,pItem_Allocated);
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_CSET_ENTRY_CONFLICT_H
