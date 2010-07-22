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
 * @file sg_mrg__private_do_simple.h
 *
 * @details 
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_DO_SIMPLE_H
#define H_SG_MRG__PRIVATE_DO_SIMPLE_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

#define MARKER_UNHANDLED			0
#define MARKER_LCA_PASS				1
#define MARKER_SPCA_PASS			2
#define MARKER_ADDITION_PASS		3
#define MARKER_FIXUP_PASS			4

//////////////////////////////////////////////////////////////////

#if TRACE_MRG
static void _trace_msg__v_input(SG_context * pCtx,
								const char * pszLabel,
								const char * pszInterimCSetName,
								SG_mrg_cset * pMrgCSet_Ancestor,
								SG_vector * pVec_MrgCSet_Leaves)
{
	SG_mrg_cset * pMrgCSet_Leaf_k;
	SG_uint32 nrLeaves, kLeaf;

	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,("%s:\n"
												  "    Result:   [name    %s]\n"
												  "    Ancestor: [hidCSet %s] %s\n"),
							   pszLabel,
							   ((pszInterimCSetName) ? pszInterimCSetName : "(null)"),
							   pMrgCSet_Ancestor->bufHid_CSet,
							   SG_string__sz(pMrgCSet_Ancestor->pStringName))  );

	SG_ERR_CHECK_RETURN(  SG_vector__length(pCtx,pVec_MrgCSet_Leaves,&nrLeaves)  );

	for (kLeaf=0; kLeaf<nrLeaves; kLeaf++)
	{
		SG_ERR_CHECK_RETURN(  SG_vector__get(pCtx,pVec_MrgCSet_Leaves,kLeaf,(void **)&pMrgCSet_Leaf_k)  );
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,("    Leaf[%02d]: [hidCSet %s] %s\n"),
								   kLeaf,pMrgCSet_Leaf_k->bufHid_CSet,
								   SG_string__sz(pMrgCSet_Leaf_k->pStringName))  );
	}
}

static void _trace_msg__v_ancestor(SG_context * pCtx,
								   const char * pszLabel,
								   SG_int32 marker_value,
								   SG_mrg_cset * pMrgCSet_Ancestor,
								   const char * pszGid_Entry)
{
	SG_string * pString_temp = NULL;

	// compute repo-path as it existed in ancestor.
	SG_ERR_IGNORE(  SG_mrg_cset__compute_repo_path_for_entry(pCtx,
															 pMrgCSet_Ancestor,
															 pszGid_Entry,
															 SG_FALSE,
															 &pString_temp)  );
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "%s[%d]: evaluating [ancestor entry %s] [%s]\n",
							   pszLabel, marker_value, pszGid_Entry,
							   ((pString_temp) ? SG_string__sz(pString_temp) : "(unknown)"))  );

	SG_STRING_NULLFREE(pCtx,pString_temp);
}

static void _trace_msg__v_leaf_eq_neq(SG_context * pCtx,
									  const char * pszLabel,
									  SG_int32 marker_value,
									  SG_uint32 kLeaf,
									  SG_mrg_cset * pMrgCSet_Leaf_k,
									  const char * pszGid_Entry,
									  SG_mrg_cset_entry_neq neq)
{
	SG_string * pString_temp = NULL;

	// compute repo-path as it existed in the leaf.
	SG_ERR_IGNORE(  SG_mrg_cset__compute_repo_path_for_entry(pCtx,
															 pMrgCSet_Leaf_k,
															 pszGid_Entry,
															 SG_FALSE,
															 &pString_temp)  );
	if (neq == SG_MRG_CSET_ENTRY_NEQ__ALL_EQUAL)
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "%s[%d]: [entry %s] not changed in leaf[%02d] neq[%02x] [%s]\n",
								   pszLabel, marker_value, pszGid_Entry,kLeaf,neq,
								   ((pString_temp) ? SG_string__sz(pString_temp) : "(unknown)"))  );
	else
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "%s[%d]: [entry %s]     changed in leaf[%02d] neq[%02x] [%s]\n",
								   pszLabel, marker_value, pszGid_Entry,kLeaf,neq,
								   ((pString_temp) ? SG_string__sz(pString_temp) : "(unknown)"))  );

	SG_STRING_NULLFREE(pCtx, pString_temp);
}

static void _trace_msg__v_leaf_deleted(SG_context * pCtx,
									   const char * pszLabel,
									   SG_int32 marker_value,
									   SG_mrg_cset * pMrgCSet_Ancestor,
									   SG_uint32 kLeaf,
									   const char * pszGid_Entry)
{
	SG_string * pString_temp = NULL;

	// compute repo-path as it existed in the ancestor (since the entry (and possibly
	// the parent dir) was deleted in the leaf).
	SG_ERR_IGNORE(  SG_mrg_cset__compute_repo_path_for_entry(pCtx,
															 pMrgCSet_Ancestor,
															 pszGid_Entry,
															 SG_FALSE,
															 &pString_temp)  );
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "%s[%d]: [entry %s] deleted in leaf[%02d] [%s]\n",
							   pszLabel, marker_value, pszGid_Entry,kLeaf,
							   ((pString_temp) ? SG_string__sz(pString_temp) : "(unknown)"))  );

	SG_STRING_NULLFREE(pCtx, pString_temp);
}

static void _trace_msg__v_leaf_added(SG_context * pCtx,
									 const char * pszLabel,
									 SG_int32 marker_value,
									 SG_mrg_cset * pMrgCSet_Leaf_k,
									 SG_uint32 kLeaf,
									 SG_mrg_cset_entry * pMrgCSetEntry_Leaf_k)
{
	SG_string * pString_temp = NULL;

	// an entry was added by the leaf.
	// compute repo-path as it existed in the leaf.
	SG_ERR_IGNORE(  SG_mrg_cset__compute_repo_path_for_entry(pCtx,
															 pMrgCSet_Leaf_k,
															 pMrgCSetEntry_Leaf_k->bufGid_Entry,
															 SG_FALSE,
															 &pString_temp)  );
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "%s[%d]: [entry %s] added in leaf[%02d] [parent gid %s] [%s]\n",
							   pszLabel, marker_value, pMrgCSetEntry_Leaf_k->bufGid_Entry,kLeaf,
							   pMrgCSetEntry_Leaf_k->bufGid_Parent,
							   ((pString_temp) ? SG_string__sz(pString_temp) : "(unknown)"))  );

	SG_STRING_NULLFREE(pCtx, pString_temp);
}

static void _trace_msg__v_result(SG_context * pCtx,
								 const char * pszLabel,
								 SG_mrg_cset * pMrgCSet_Result,
								 SG_mrg_cset_stats * pMrgCSetStats_Result)
{
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "%s: [result-cset %s] stats: [Unc Chg Del Add Con]: [%d %d %d %d %d] Auto-Merge: [tbd norule fail ok]: [%d %d %d %d]\n",
							   pszLabel,
							   SG_string__sz(pMrgCSet_Result->pStringName),
							   pMrgCSetStats_Result->nrUnchanged,
							   pMrgCSetStats_Result->nrChanged,
							   pMrgCSetStats_Result->nrDeleted,
							   pMrgCSetStats_Result->nrAdded,
							   pMrgCSetStats_Result->nrConflicts,
							   pMrgCSetStats_Result->nrAutoMerge_TBD,
							   pMrgCSetStats_Result->nrAutoMerge_NoRule,
							   pMrgCSetStats_Result->nrAutoMerge_Fail,
							   pMrgCSetStats_Result->nrAutoMerge_OK)  );
}

static SG_rbtree_foreach_callback _trace_msg__dump_list_member__mrg_cset_entry;

static void _trace_msg__dump_list_member__mrg_cset_entry(SG_context * pCtx,
														 const char * pszKey_Gid_Entry,
														 void * pVoidAssocData_MrgCSetEntry,
														 SG_UNUSED_PARAM(void * pVoid_Data))
{
	SG_mrg_cset_entry * pMrgCSetEntry = (SG_mrg_cset_entry *)pVoidAssocData_MrgCSetEntry;
	SG_string * pString_temp = NULL;

	SG_UNUSED(pVoid_Data);

	SG_ASSERT(  (strcmp(pszKey_Gid_Entry,pMrgCSetEntry->bufGid_Entry) == 0)  );

	// compute the repo-path as it existed in the version of the tree that this entry belongs to.
	SG_ERR_IGNORE(  SG_mrg_cset__compute_repo_path_for_entry(pCtx,
															 pMrgCSetEntry->pMrgCSet,
															 pMrgCSetEntry->bufGid_Entry,
															 SG_TRUE,
															 &pString_temp)  );
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "    [gid %s] %s\n",
							   pszKey_Gid_Entry,
							   ((pString_temp) ? SG_string__sz(pString_temp) : "(unknown)"))  );

	SG_STRING_NULLFREE(pCtx, pString_temp);
}
	
static void _trace_msg__dump_delete_list(SG_context * pCtx,
										 const char * pszLabel,
										 SG_mrg_cset * pMrgCSet_Result)
{
	SG_uint32 nr = 0;

	if (pMrgCSet_Result->prbDeletes)
		SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx,pMrgCSet_Result->prbDeletes,&nr)  );

	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "%s: [result-cset %s] delete list: [count %d]\n",
							   pszLabel,SG_string__sz(pMrgCSet_Result->pStringName),nr)  );
	if (nr > 0)
		SG_ERR_IGNORE(  SG_rbtree__foreach(pCtx,pMrgCSet_Result->prbDeletes,_trace_msg__dump_list_member__mrg_cset_entry,NULL)  );
}

//////////////////////////////////////////////////////////////////

struct _sg_mrg_cset_entry_conflict_flag_msgs
{
	SG_mrg_cset_entry_conflict_flags		bit;
	const char *							pszMsg;
};

typedef struct _sg_mrg_cset_entry_conflict_flag_msgs SG_mrg_cset_entry_conflict_flags_msg;

static SG_mrg_cset_entry_conflict_flags_msg aFlagMsgs[] =
{ 
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_MOVE,				"REMOVE-vs-MOVE" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_RENAME,				"REMOVE-vs-RENAME" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_ATTRBITS,			"REMOVE-vs-ATTRBITS_CHANGE" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_XATTR,				"REMOVE-vs-XATTR_CHANGE" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_SYMLINK_EDIT,		"REMOVE-vs-SYMLINK_EDIT" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_FILE_EDIT,			"REMOVE-vs-FILE_EDIT" },

	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_CAUSED_ORPHAN,			"REMOVE-CAUSED-ORPHAN" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__MOVES_CAUSED_PATH_CYCLE,		"MOVES-CAUSED-PATH_CYCLE" },

	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_MOVE,				"DIVERGENT-MOVE" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_RENAME,				"DIVERGENT-RENAME" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_ATTRBITS,			"DIVERGENT-ATTRBITS_CHANGE" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_XATTR,				"DIVERGENT-XATTR_CHANGE" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_SYMLINK_EDIT,		"DIVERGENT-SYMLINK_EDIT" },

	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__TBD,				"DIVERGENT-FILE_EDIT_TBD" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__AUTO_OK,			"DIVERGENT-FILE_EDIT_AUTO_OK" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__AUTO_FAILED,		"DIVERGENT-FILE_EDIT_AUTO_FAILED" },
	{	SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__NO_RULE,			"DIVERGENT-FILE_EDIT_NO_RULE" },
};

static SG_uint32 nrFlagMsgs = SG_NrElements(aFlagMsgs);

static void _trace_msg__dump_entry_conflict_flags(SG_context * pCtx,
												  SG_mrg_cset_entry_conflict_flags flags,
												  SG_uint32 indent)
{
	SG_uint32 j;
	SG_mrg_cset_entry_conflict_flags flagsLocal = flags;

	for (j=0; j<nrFlagMsgs; j++)
	{
		if (flagsLocal & aFlagMsgs[j].bit)
		{
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,"%*c%s\n",indent,' ',aFlagMsgs[j].pszMsg)  );
			flagsLocal &= ~aFlagMsgs[j].bit;
		}
	}

	SG_ASSERT( (flagsLocal == 0) );		// make sure that table is complete.
}

//////////////////////////////////////////////////////////////////

static SG_rbtree_foreach_callback _trace_msg__dump_list_member__mrg_cset_entry_conflict;

static void _trace_msg__dump_list_member__mrg_cset_entry_conflict(SG_context * pCtx,
																  const char * pszKey_Gid_Entry,
																  void * pVoidAssocData_MrgCSetEntryConflict,
																  SG_UNUSED_PARAM(void * pVoid_Data))
{
	SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict = (SG_mrg_cset_entry_conflict *)pVoidAssocData_MrgCSetEntryConflict;
	SG_string * pString_temp = NULL;
	SG_uint32 j;
	SG_uint32 nrChanges = 0;
	SG_uint32 nrDeletes = 0;

	SG_UNUSED(pVoid_Data);

	SG_ASSERT(  (strcmp(pszKey_Gid_Entry,pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor->bufGid_Entry) == 0)  );

	if (pMrgCSetEntryConflict->pVec_MrgCSetEntry_Changes)
		SG_ERR_CHECK(  SG_vector__length(pCtx,pMrgCSetEntryConflict->pVec_MrgCSetEntry_Changes,&nrChanges)  );
	if (pMrgCSetEntryConflict->pVec_MrgCSet_Deletes)
		SG_ERR_CHECK(  SG_vector__length(pCtx,pMrgCSetEntryConflict->pVec_MrgCSet_Deletes,&nrDeletes)  );

	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "    conflict: [gid %s] [flags 0x%08x] [nr deletes %d] [nr changes %d]\n",
							   pszKey_Gid_Entry,pMrgCSetEntryConflict->flags,nrDeletes,nrChanges)  );

	SG_ERR_IGNORE(  _trace_msg__dump_entry_conflict_flags(pCtx,pMrgCSetEntryConflict->flags,8)  );

	// compute the repo-path of entry as it existed in the ANCESTOR version of the tree.
	SG_ERR_IGNORE(  SG_mrg_cset__compute_repo_path_for_entry(pCtx,
															 pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor->pMrgCSet,
															 pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor->bufGid_Entry,
															 SG_FALSE,
															 &pString_temp)  );
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "        Ancestor: %s\n",
							   ((pString_temp) ? SG_string__sz(pString_temp) : "(unknown)"))  );
	SG_STRING_NULLFREE(pCtx, pString_temp);

	// dump the branches/leaves where there was a change relative to the ancestor
	for (j=0; j<nrChanges; j++)
	{
		const SG_mrg_cset_entry * pMrgCSetEntry_Leaf_j;
		SG_int64 i64;
		SG_mrg_cset_entry_neq neq_Leaf_j;

		SG_ERR_CHECK(  SG_vector__get(pCtx,pMrgCSetEntryConflict->pVec_MrgCSetEntry_Changes,j,(void **)&pMrgCSetEntry_Leaf_j)  );
		SG_ERR_CHECK(  SG_vector_i64__get(pCtx,pMrgCSetEntryConflict->pVec_MrgCSetEntryNeq_Changes,j,&i64)  );
		neq_Leaf_j = (SG_mrg_cset_entry_neq)i64;

		// compute the repo-path of the entry as it existed in the branch/leaf version of the tree.
		SG_ERR_IGNORE(  SG_mrg_cset__compute_repo_path_for_entry(pCtx,
																 pMrgCSetEntry_Leaf_j->pMrgCSet,
																 pMrgCSetEntry_Leaf_j->bufGid_Entry,
																 SG_FALSE,
																 &pString_temp)  );
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "        changed: neq[%02x] [leaf hidCSet %s] [%s]\n",
								   neq_Leaf_j,
								   pMrgCSetEntry_Leaf_j->pMrgCSet->bufHid_CSet,
								   ((pString_temp) ? SG_string__sz(pString_temp) : "(unknown)"))  );

		if (neq_Leaf_j & SG_MRG_CSET_ENTRY_NEQ__ATTRBITS)
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
									   "            attrbits: 0x%0x --> 0x%0x\n",
									   ((SG_uint32)pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor->attrBits),
									   ((SG_uint32)pMrgCSetEntry_Leaf_j->attrBits))  );
		if (neq_Leaf_j & SG_MRG_CSET_ENTRY_NEQ__ENTRYNAME)
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
									   "            renamed: %s --> %s\n",
									   SG_string__sz(pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor->pStringEntryname),
									   SG_string__sz(pMrgCSetEntry_Leaf_j->pStringEntryname))  );
		if (neq_Leaf_j & SG_MRG_CSET_ENTRY_NEQ__GID_PARENT)
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
									   "            moved: %s --> %s\n",
									   pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor->bufGid_Parent,
									   pMrgCSetEntry_Leaf_j->bufGid_Parent)  );
		if (neq_Leaf_j & SG_MRG_CSET_ENTRY_NEQ__HID_XATTR)
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
									   "            xattr: %s --> %s\n",
									   pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor->bufHid_XAttr,
									   pMrgCSetEntry_Leaf_j->bufHid_XAttr)  );
		if (neq_Leaf_j & SG_MRG_CSET_ENTRY_NEQ__DELETED)
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
									   "            removed:\n")  );
		if (neq_Leaf_j & SG_MRG_CSET_ENTRY_NEQ__SYMLINK_HID_BLOB)
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
									   "           modified symlink: hid[ %s --> %s] content[TODO]\n",
									   pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor->bufHid_Blob,
									   pMrgCSetEntry_Leaf_j->bufHid_Blob)  );
		if (neq_Leaf_j & SG_MRG_CSET_ENTRY_NEQ__FILE_HID_BLOB)
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
									   "           modified file: hid[%s --> %s]\n",
									   pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor->bufHid_Blob,
									   pMrgCSetEntry_Leaf_j->bufHid_Blob)  );

		SG_STRING_NULLFREE(pCtx,pString_temp);
	}

	// dump the branches/leaves where there was a delete

	for (j=0; j<nrDeletes; j++)
	{
		const SG_mrg_cset * pMrgCSet_Leaf_j;

		SG_ERR_CHECK(  SG_vector__get(pCtx,pMrgCSetEntryConflict->pVec_MrgCSet_Deletes,j,(void **)&pMrgCSet_Leaf_j)  );
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "        deleted: [leaf hidCSet %s]\n",
								   pMrgCSet_Leaf_j->bufHid_CSet)  );
	}

fail:
	SG_STRING_NULLFREE(pCtx,pString_temp);
}

static void _trace_msg__dump_conflict_list(SG_context * pCtx,
										   const char * pszLabel,
										   SG_mrg_cset * pMrgCSet_Result)
{
	SG_uint32 nr = 0;

	if (pMrgCSet_Result->prbConflicts)
		SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx,pMrgCSet_Result->prbConflicts,&nr)  );

	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "%s: [result-cset %s] conflict list: [count %d]\n",
							   pszLabel,SG_string__sz(pMrgCSet_Result->pStringName),nr)  );
	if (nr > 0)
		SG_ERR_IGNORE(  SG_rbtree__foreach(pCtx,pMrgCSet_Result->prbConflicts,_trace_msg__dump_list_member__mrg_cset_entry_conflict,NULL)  );
}
#endif

//////////////////////////////////////////////////////////////////

/**
 * set all the marker values on all of the leaf CSETs to a known value
 * so that after we run the main loop, we can identify the entries that
 * we did not touch.
 */
static void _v_set_all_markers(SG_context * pCtx,
							   SG_vector * pVec_MrgCSet_Leaves,
							   SG_int64 newValue)
{
	SG_mrg_cset * pMrgCSet_Leaf_k;
	SG_uint32 nrLeaves, kLeaf;

	SG_ERR_CHECK_RETURN(  SG_vector__length(pCtx,pVec_MrgCSet_Leaves,&nrLeaves)  );
	
	for (kLeaf=0; kLeaf<nrLeaves; kLeaf++)
	{
		SG_ERR_CHECK_RETURN(  SG_vector__get(pCtx,pVec_MrgCSet_Leaves,kLeaf,(void **)&pMrgCSet_Leaf_k)  );
		SG_ERR_CHECK_RETURN(  SG_mrg_cset__set_all_markers(pCtx,pMrgCSet_Leaf_k,newValue)  );
	}
}

//////////////////////////////////////////////////////////////////

/**
 * context data for sg_rbtree_foreach_callback for:
 *     _v_process_ancestor_entry
 *     _v_process_leaf_k_entry_addition
 */
struct _v_data
{
	SG_mrg *					pMrg;							// we do not own this
	SG_vector *					pVec_MrgCSet_Leaves;			// we do not own this
	SG_mrg_cset *				pMrgCSet_Result;				// we own this
	SG_mrg_cset_stats *			pMrgCSetStats_Result;			// we own this

	// used by _v_process_ancestor_entry
	SG_mrg_cset *				pMrgCSet_Ancestor;				// we do not own this

	// used by _v_process_leaf_k_entry_addition
	SG_mrg_cset *				pMrgCSet_Leaf_k;				// we do not own this
	SG_uint32					kLeaf;

	// used by _v_find_orphans, _v_add_orphan_parents
	SG_rbtree *					prbMrgCSetEntry_OrphanParents;	// rbtree[gid-entry --> SG_mrg_cset_entry * pMrgCSetEntry_Ancestor]   orphan-to-add-list   we own rbtree, but do not own values

	// used by _v_find_path_cycles, _v_add_path_cycle_conflicts
	SG_rbtree *					prbMrgCSetEntry_DirectoryCycles;	// rbtree[gid-entry --> SG_mrg_cset_entry * pMrgCSetEntry_Result]	dirs involved in a cycle   we own rbtree, but do not own values

	SG_int32					use_marker_value;
};

typedef struct _v_data _v_data;

//////////////////////////////////////////////////////////////////

/**
 * Look at the corresponding entry in each of the k leaves and compare them
 * with the ancestor entry.  If we have an unchanged entry or an entry that
 * was only changed in one branch, handle it without a lot of fuss (or allocs).
 *
 * We do this because most of the entries in a tree won't change (or will only
 * be changed in one branch).
 */
static void _v_process_ancestor_entry__quick_scan(SG_context * pCtx,
												  const char * pszKey_Gid_Entry,
												  SG_mrg_cset_entry * pMrgCSetEntry_Ancestor,
												  _v_data * pData,
												  SG_bool * pbHandled)
{
	SG_uint32 nrLeaves, kLeaf;
	SG_uint32 kLeaf_Last_Change = SG_UINT32_MAX;
	SG_mrg_cset_entry * pMrgCSetEntry_Last_Change = NULL;
	SG_mrg_cset_entry_neq neq_Last_Change = SG_MRG_CSET_ENTRY_NEQ__ALL_EQUAL;

	SG_ERR_CHECK(  SG_vector__length(pCtx,pData->pVec_MrgCSet_Leaves,&nrLeaves)  );
	for (kLeaf=0; kLeaf<nrLeaves; kLeaf++)
	{
		SG_mrg_cset * pMrgCSet_Leaf_k = NULL;
		SG_mrg_cset_entry * pMrgCSetEntry_Leaf_k = NULL;
		SG_mrg_cset_entry_neq neq = SG_MRG_CSET_ENTRY_NEQ__ALL_EQUAL;
		SG_bool bFound_in_Leaf = SG_FALSE;

		SG_ERR_CHECK(  SG_vector__get(pCtx,pData->pVec_MrgCSet_Leaves,kLeaf,(void **)&pMrgCSet_Leaf_k)  );

		SG_ERR_CHECK(  SG_rbtree__find(pCtx,pMrgCSet_Leaf_k->prbEntries,pszKey_Gid_Entry,
									   &bFound_in_Leaf,(void **)&pMrgCSetEntry_Leaf_k)  );
		if (bFound_in_Leaf)
		{
			if (pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict)	// if any leaf already has a conflict (from sub-merge)
				goto not_handled;								// we are forced to do it the hard-way.

			pMrgCSetEntry_Leaf_k->markerValue = pData->use_marker_value;

			SG_ERR_CHECK(  SG_mrg_cset_entry__equal(pCtx,pMrgCSetEntry_Ancestor,pMrgCSetEntry_Leaf_k,&neq)  );
#if TRACE_MRG
			SG_ERR_IGNORE(  _trace_msg__v_leaf_eq_neq(pCtx,"_v_process_ancestor_entry__quick_scan",
													  pData->use_marker_value,
													  kLeaf,pMrgCSet_Leaf_k,pszKey_Gid_Entry,neq)  );
#endif
		}
		else
		{
			neq = SG_MRG_CSET_ENTRY_NEQ__DELETED;
#if TRACE_MRG
			SG_ERR_IGNORE(  _trace_msg__v_leaf_deleted(pCtx,"_v_process_ancestor_entry__quick_scan",
													   pData->use_marker_value,
													   pData->pMrgCSet_Ancestor,kLeaf,pszKey_Gid_Entry)  );
#endif
		}
		
		if (neq != SG_MRG_CSET_ENTRY_NEQ__ALL_EQUAL)
		{
			if (kLeaf_Last_Change != SG_UINT32_MAX)		// we found a second leaf/branch that changed the entry
				goto not_handled;

			kLeaf_Last_Change = kLeaf;
			pMrgCSetEntry_Last_Change = pMrgCSetEntry_Leaf_k;
			neq_Last_Change = neq;
		}
	}

	if (kLeaf_Last_Change == SG_UINT32_MAX)
	{
		// the entry was not changed in any branch.
		// clone the ancestor entry and add it to the result-cset.

#if TRACE_MRG
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "_v_process_ancestor_entry__quick_scan[%d]: cloning ancestor [entry %s]\n",
								   pData->use_marker_value,
								   pszKey_Gid_Entry)  );
#endif
		SG_ERR_CHECK(  SG_mrg_cset_entry__clone_and_insert_in_cset(pCtx,pMrgCSetEntry_Ancestor,pData->pMrgCSet_Result,NULL)  );

		pData->pMrgCSetStats_Result->nrUnchanged++;
		goto handled;
	}
	else
	{
		if (neq_Last_Change & SG_MRG_CSET_ENTRY_NEQ__DELETED)
		{
			// the only change was a delete.
#if TRACE_MRG
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
									   "_v_process_ancestor_entry__quick_scan[%d]: omitting leaf[%d] [entry %s]\n",
									   pData->use_marker_value,
									   kLeaf_Last_Change,pszKey_Gid_Entry)  );
#endif
			SG_ERR_CHECK(  SG_mrg_cset__register_delete(pCtx,pData->pMrgCSet_Result,pMrgCSetEntry_Ancestor)  );
			pData->pMrgCSetStats_Result->nrDeleted++;
			goto handled;
		}
		else
		{
			// the only change was a proper change.  it may have one or more NEQ
			// bits set, but we don't care because we are going to take the whole
			// change as is.

#if TRACE_MRG
			SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
									   "_v_process_ancestor_entry__quick_scan[%d]: cloning leaf[%d] [entry %s]\n",
									   pData->use_marker_value,
									   kLeaf_Last_Change,pszKey_Gid_Entry)  );
#endif
			SG_ERR_CHECK(  SG_mrg_cset_entry__clone_and_insert_in_cset(pCtx,pMrgCSetEntry_Last_Change,pData->pMrgCSet_Result,NULL)  );
			pData->pMrgCSetStats_Result->nrChanged++;
			goto handled;
		}
	}

	/*NOTREACHED*/

handled:
	*pbHandled = SG_TRUE;
	return;

not_handled:
	*pbHandled = SG_FALSE;
	return;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * look at all of the peer entries in each cset for the given gid-entry
 * and compare them.  assume that we will have a conflict of some kind
 * and allocate an entry-conflict structure and populate it with the
 * entries from the leaves that changed it relative to the ancestor.
 * 
 * we want to identify:
 * [] the csets that changed it;
 * [] the values that they changed it to;
 * [] the unique values that they changed it to (removing duplicates on
 *    a field-by-field basis).
 *
 * you own the returned entry-conflict structure.  (we DO NOT automatically
 * add it to the conflict-list in the result-cset.)
 *
 * note that we are given a vector of leaves; these are the branches
 * descended from the given ancestor.  when we are an outer-merge
 * (containing a sub-merge) one or more of our "leaves" will actually
 * be the result-entry for the sub-merge (rather than an actual leaf
 * from the point of view of the DAGLCA graph).  so, for example, the
 * result-entry might not have a file-hid-blob because we haven't
 * done the auto-merge yet.
 */
static void _v_process_ancestor_entry__build_candidate_conflict(SG_context * pCtx,
																const char * pszKey_Gid_Entry,
																SG_mrg_cset_entry * pMrgCSetEntry_Ancestor,
																SG_mrg_cset_entry * pMrgCSetEntry_Baseline,
																_v_data * pData,
																SG_mrg_cset_entry_conflict ** ppMrgCSetEntryConflict)
{
	SG_uint32 nrLeaves, kLeaf;
	SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict = NULL;

	SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__alloc(pCtx,pData->pMrgCSet_Result,
													 pMrgCSetEntry_Ancestor,
													 pMrgCSetEntry_Baseline,
													 &pMrgCSetEntryConflict)  );

	SG_ERR_CHECK(  SG_vector__length(pCtx,pData->pVec_MrgCSet_Leaves,&nrLeaves)  );
	for (kLeaf=0; kLeaf<nrLeaves; kLeaf++)
	{
		SG_mrg_cset * pMrgCSet_Leaf_k = NULL;
		SG_mrg_cset_entry * pMrgCSetEntry_Leaf_k = NULL;
		SG_mrg_cset_entry_neq neq = SG_MRG_CSET_ENTRY_NEQ__ALL_EQUAL;
		SG_bool bFound_in_Leaf = SG_FALSE;

		SG_ERR_CHECK(  SG_vector__get(pCtx,pData->pVec_MrgCSet_Leaves,kLeaf,(void **)&pMrgCSet_Leaf_k)  );

		SG_ERR_CHECK(  SG_rbtree__find(pCtx,pMrgCSet_Leaf_k->prbEntries,pszKey_Gid_Entry,
									   &bFound_in_Leaf,(void **)&pMrgCSetEntry_Leaf_k)  );
		if (bFound_in_Leaf)
		{
			pMrgCSetEntry_Leaf_k->markerValue = pData->use_marker_value;

			SG_ERR_CHECK(  SG_mrg_cset_entry__equal(pCtx,pMrgCSetEntry_Ancestor,pMrgCSetEntry_Leaf_k,&neq)  );
#if TRACE_MRG
			SG_ERR_IGNORE(  _trace_msg__v_leaf_eq_neq(pCtx,"_v_process_ancestor_entry__build_candidate_conflict",
													  pData->use_marker_value,
													  kLeaf,pMrgCSet_Leaf_k,pszKey_Gid_Entry,neq)  );
#endif
			if ((neq != SG_MRG_CSET_ENTRY_NEQ__ALL_EQUAL) || (pMrgCSetEntry_Leaf_k->pMrgCSetEntryConflict))
				SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__append_change(pCtx,pMrgCSetEntryConflict,pMrgCSetEntry_Leaf_k,neq)  );
		}
		else
		{
			neq = SG_MRG_CSET_ENTRY_NEQ__DELETED;
#if TRACE_MRG
			SG_ERR_IGNORE(  _trace_msg__v_leaf_deleted(pCtx,"_v_process_ancestor_entry__build_candidate_conflict",
													   pData->use_marker_value,
													   pData->pMrgCSet_Ancestor,kLeaf,pszKey_Gid_Entry)  );
#endif
			SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__append_delete(pCtx,pMrgCSetEntryConflict,pMrgCSet_Leaf_k)  );
		}
	}

	*ppMrgCSetEntryConflict = pMrgCSetEntryConflict;
	return;

fail:
	SG_MRG_CSET_ENTRY_CONFLICT_NULLFREE(pCtx, pMrgCSetEntryConflict);
}

/**
 * Look at the corresponding entry in each of the k leaves and compare them
 * with the ancestor entry.  We know/expect that 2 or more of them have
 * changed the entry some how.
 *
 * Build a SG_mrg_cset_entry_conflict with the details.  If we can internally
 * resolve it, we do so.  If not, we add the conflict to the CSET.
 *
 * Things we can handle quietly include things like a rename in one branch
 * and a chmod in another or 2 renames to the same new name.
 *
 * Things we can't include things like 2 renames to different new names,
 * for example.
 */
static void _v_process_ancestor_entry__hard_way(SG_context * pCtx,
												const char * pszKey_Gid_Entry,
												SG_mrg_cset_entry * pMrgCSetEntry_Ancestor,
												_v_data * pData)
{
	SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict = NULL;
	SG_mrg_cset_entry * pMrgCSetEntry_Composite = NULL;
	SG_mrg_cset_entry * pMrgCSetEntry_Baseline = NULL;		// we don't own this
	SG_uint32 nrDeletes, nrChanges;
	SG_uint32 nrUniqueAttrBitsChanges, nrUniqueEntrynameChanges, nrUniqueGidParentChanges, nrUniqueHidXAttrChanges, nrUniqueSymlinkHidBlobChanges, nrUniqueFileHidBlobChanges;
	SG_mrg_cset_entry_conflict_flags flags = SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__ZERO;
	const char * psz_temp;
	SG_bool bFound;

	// see if there is a corresponding pMrgCSetEntry for this entry in the baseline cset.
	// this may or may not exist.

	SG_ERR_CHECK(  SG_rbtree__find(pCtx, pData->pMrg->pMrgCSet_Baseline->prbEntries, pszKey_Gid_Entry,
								   &bFound, (void **)&pMrgCSetEntry_Baseline)  );

	// assume the worst and allocate a conflict-structure to hold info
	// on the branches/leaves that changed the entry.  if we later determine
	// that we can actually handle the change, we delete the conflict-structure
	// rather than adding it to the conflict-list in the resut-cset.

	SG_ERR_CHECK(  _v_process_ancestor_entry__build_candidate_conflict(pCtx,
																	   pszKey_Gid_Entry,
																	   pMrgCSetEntry_Ancestor,
																	   pMrgCSetEntry_Baseline,
																	   pData,
																	   &pMrgCSetEntryConflict)  );

	SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__count_deletes(pCtx,pMrgCSetEntryConflict,&nrDeletes)  );	// nr leaves that deleted it
	SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__count_changes(pCtx,pMrgCSetEntryConflict,&nrChanges)  );	// nr leaves that changed it somehow
	SG_ASSERT(  ((nrDeletes + nrChanges) > 0)  );

	if ((nrDeletes > 0) && (nrChanges == 0))			// a clean delete (by 1 or more leaves (and no other leaf made any changes to the entry))
	{
#if TRACE_MRG
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "_v_process_ancestor_entry__hard_way..: omitting leaf [deleted in %d leaves] [entry %s]\n",
								   nrDeletes, pszKey_Gid_Entry)  );
#endif
		SG_ERR_CHECK(  SG_mrg_cset__register_delete(pCtx,pData->pMrgCSet_Result,pMrgCSetEntry_Ancestor)  );
		pData->pMrgCSetStats_Result->nrDeleted++;
		goto handled_entry__eat_conflict;
	}

	SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__count_unique_attrbits(pCtx,pMrgCSetEntryConflict,&nrUniqueAttrBitsChanges)  );
	SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__count_unique_entryname(pCtx,pMrgCSetEntryConflict,&nrUniqueEntrynameChanges)  );
	SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__count_unique_gid_parent(pCtx,pMrgCSetEntryConflict,&nrUniqueGidParentChanges)  );
	SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__count_unique_hid_xattrs(pCtx,pMrgCSetEntryConflict,&nrUniqueHidXAttrChanges)  );
	SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__count_unique_symlink_hid_blob(pCtx,pMrgCSetEntryConflict,&nrUniqueSymlinkHidBlobChanges)  );
	SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__count_unique_file_hid_blob_or_result(pCtx,pMrgCSetEntryConflict,&nrUniqueFileHidBlobChanges)  );
	
#if TRACE_MRG
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_v_process_ancestor_entry__hard_way..[%d]: [deleted in %d leaves] [changed in %d leaves] [entry %s]\n",
							   pData->use_marker_value,
							   nrDeletes, nrChanges, pszKey_Gid_Entry)  );
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "                                           nr unique: [attrbits %d][rename %d][move %d][xattr %d][symlink blob %d][file blob %d]\n",
							   nrUniqueAttrBitsChanges,nrUniqueEntrynameChanges,nrUniqueGidParentChanges,nrUniqueHidXAttrChanges,nrUniqueSymlinkHidBlobChanges,nrUniqueFileHidBlobChanges)  );
#endif

	// we create a new instance of the entry for the result-cset.  for things which
	// were either not changed or only changed in one leaf, we know the right answer.
	// for things that were changed in multiple leaves, we have a hard/arbitrary choice.
	// I can see 2 ways of doing this:
	// [a] use the ancestor version and list all of the leaf values as candidates for
	//     the user to choose from.
	// [b] use the baseline version (if present) and list the remaining values as
	//     alternatives.
	// I originally thought that [a] would be best.  But now I'm thinking that [b]
	// would be better because it'll better reflect the merge as a series of deltas
	// to the current baseline.
	//
	// create the clone.  try to use the baseline.  fallback to the ancestor.
	// then overwrite various fields.

	if (pMrgCSetEntry_Baseline)
		SG_ERR_CHECK(  SG_mrg_cset_entry__clone_and_insert_in_cset(pCtx,pMrgCSetEntry_Baseline,pData->pMrgCSet_Result,&pMrgCSetEntry_Composite)  );
	else
		SG_ERR_CHECK(  SG_mrg_cset_entry__clone_and_insert_in_cset(pCtx,pMrgCSetEntry_Ancestor,pData->pMrgCSet_Result,&pMrgCSetEntry_Composite)  );

#if TRACE_MRG
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   "_v_process_ancestor_entry__hard_way..[%d]: [present in baseline %d]\n",
							   pData->use_marker_value,
							   (pMrgCSetEntry_Baseline != NULL))  );
#endif

	// if a field was only changed in one leaf (or in the same way by many leaves),
	// then we take the change without complaining.
	// 
	// if a field was changed in divergent ways, we need to think about it a little.
	//
	// [1] the values for RENAME, MOVE, ATTRBITS, XATTRS, and SYMLINKS are collapsable.
	//     (meaning in a 5-way merge with 5 diverent renames, the user should only have to
	//     pick 1 final value for the result; they should not have to vote for a new value
	//     for each pair-wise sub-merge.)
	//     so that if one of the leaves had a conflict on a field, we added all of possible
	//     values to our rbUnique, so our set of potential new values grows as we crawl out
	//     of the sub-merges.  then the preview/resolver can simply show the potential set
	//     without having to dive into the graph.
	//
	// [2] if there are multiple values for the file-hid-blob, then we need to build an
	//     auto-merge-plan and let it deal with it at the right time.
	//     (see SG_mrg_cset_entry_conflict__append_change())
	//
	//     we ZERO the file-hid-blob in the result-cset as an indicator that we need
	//     to wait for auto-merge.

	switch (nrUniqueGidParentChanges)
	{
	case 0:			// not moved in any leaf
		break;		// keep gid-parent from baseline/ancestor (since baseline value matches ancestor value).

	case 1:			// any/all moves moved it to one unique target directory.
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,NULL,pMrgCSetEntryConflict->prbUnique_GidParent,&bFound,&psz_temp,NULL)  );
		SG_ASSERT(  (bFound && psz_temp && *psz_temp)  );
		SG_ERR_CHECK(  SG_gid__copy_into_buffer(pCtx,
												pMrgCSetEntry_Composite->bufGid_Parent, SG_NrElements(pMrgCSetEntry_Composite->bufGid_Parent),
												psz_temp)  );
		if (nrDeletes > 0)
			flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_MOVE;
		break;
			
	default:		// divergent move to 2 or more target directories.  keep it in baseline/ancestor, but flag it.
		// TODO putting it in the directory that it was in in the baseline/ancestor is a little bit questionable.
		// TODO we might want to put it in one of the target directories since we have the conflict-rename-trick.
		// TODO this might cause the entry to become an orphan....  BUT DON'T CHANGE THIS WITHOUT LOOKING AT THE
		// TODO CYCLE-PATH STUFF.
		if (nrDeletes > 0)
			flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_MOVE;
		flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_MOVE;
		break;
	}

	switch (nrUniqueEntrynameChanges)
	{
	case 0:			// not renamed in any leaf.
		break;		// keep entryname from baseline/ancestor.

	case 1:			// any/all renames renamed it to one unique target name.
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,NULL,pMrgCSetEntryConflict->prbUnique_Entryname,&bFound,&psz_temp,NULL)  );
		SG_ASSERT(  (bFound && psz_temp && *psz_temp)  );
		SG_ERR_CHECK(  SG_string__set__sz(pCtx, pMrgCSetEntry_Composite->pStringEntryname, psz_temp)  );
		if (nrDeletes > 0)
			flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_RENAME;
		break;

	default:		// divergent rename to 2 or more target entrynames.  keep value as it was in baseline/ancestor, but flag it.
		if (nrDeletes > 0)
			flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_RENAME;
		flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_RENAME;
		break;
	}

	switch (nrUniqueAttrBitsChanges)
	{
	case 0:			// no changes from baseline/ancestor.
		break;

	case 1:			// 1 unique new value
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,NULL,pMrgCSetEntryConflict->prbUnique_AttrBits,&bFound,&psz_temp,NULL)  );
		SG_ASSERT(  (bFound && psz_temp && *psz_temp)  );
		SG_ERR_CHECK(  SG_int64__parse(pCtx,&pMrgCSetEntry_Composite->attrBits,psz_temp)  );
		if (nrDeletes > 0)
			flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_ATTRBITS;
		break;

	default:		// multiple new values; use value from baseline/ancestor and complain and let them pick a new value.
		if (nrDeletes > 0)
			flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_ATTRBITS;
		flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_ATTRBITS;
		break;
	}

	switch (nrUniqueHidXAttrChanges)
	{
	case 0:
		break;

	case 1:
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,NULL,pMrgCSetEntryConflict->prbUnique_HidXAttr,&bFound,&psz_temp,NULL)  );
		SG_ASSERT(  (bFound && psz_temp && *psz_temp)  );
		if (strcmp(psz_temp, SG_MRG_TOKEN__NO_XATTR) != 0)
			SG_ERR_CHECK(  SG_strcpy(pCtx,
									 pMrgCSetEntry_Composite->bufHid_XAttr, SG_NrElements(pMrgCSetEntry_Composite->bufHid_XAttr),
									 psz_temp)  );
		else /* we have a "*no-xattr*" entry */
			memset(pMrgCSetEntry_Composite->bufHid_XAttr,0,sizeof(pMrgCSetEntry_Composite->bufHid_XAttr));
		if (nrDeletes > 0)
			flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_XATTR;
		break;
			
	default:
		if (nrDeletes > 0)
			flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_XATTR;
		flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_XATTR;
		break;
	}

	switch (pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor->tneType)
	{
	default:	// quiets compiler
		SG_ASSERT( 0 );
		SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED);

	case SG_TREENODEENTRY_TYPE_DIRECTORY:
		break;

	case SG_TREENODEENTRY_TYPE_SYMLINK:			// cannot run diff3 on the pathname fragment in the symlinks

		switch (nrUniqueSymlinkHidBlobChanges)
		{
		case 0:
			break;

		case 1:			// 1 new unique value, just take it
			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,NULL,pMrgCSetEntryConflict->prbUnique_Symlink_HidBlob,&bFound,&psz_temp,NULL)  );
			SG_ASSERT(  (bFound && psz_temp && *psz_temp)  );
			SG_ERR_CHECK(  SG_strcpy(pCtx,
									 pMrgCSetEntry_Composite->bufHid_Blob, SG_NrElements(pMrgCSetEntry_Composite->bufHid_Blob),
									 psz_temp)  );
			if (nrDeletes > 0)
				flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_SYMLINK_EDIT;
			break;
			
		default:		// more than 1 new unique value, keep value from baseline/ancestor and complain since we can't auto-merge the symlink paths
			if (nrDeletes > 0)
				flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_SYMLINK_EDIT;
			flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_SYMLINK_EDIT;
			break;
		}
		break;

	case SG_TREENODEENTRY_TYPE_REGULAR_FILE:	// need to run diff3 or something on the content of the files.

		switch (nrUniqueFileHidBlobChanges)
		{
		case 0:
			break;

		case 1:			// 1 new unique value, just take it
			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,NULL,pMrgCSetEntryConflict->prbUnique_File_HidBlob_or_AutoMergePlanResult,&bFound,&psz_temp,NULL)  );
			SG_ASSERT(  (bFound && psz_temp && *psz_temp)  );
			SG_ERR_CHECK(  SG_strcpy(pCtx,
									 pMrgCSetEntry_Composite->bufHid_Blob, SG_NrElements(pMrgCSetEntry_Composite->bufHid_Blob),
									 psz_temp)  );
			if (nrDeletes > 0)
				flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_FILE_EDIT;
			break;
			
		default:		// more than 1 new unique value, need to auto-merge it
			if (nrDeletes > 0)
				flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_FILE_EDIT;
			flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__TBD;

			// zero bufHid_Blob in result-entry because there is more than one
			// possible value for it.  also forget that the inheritance for it.
			SG_ERR_CHECK(  SG_mrg_cset_entry__forget_inherited_hid_blob(pCtx,pMrgCSetEntry_Composite)  );
			
			SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__append_our_merge_plan(pCtx,pMrgCSetEntryConflict,pData->pMrg)  );
			break;
		}
		break;
	}

	if (flags == SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__ZERO)
	{
		// all of the structural changes are consistent.
		// no divergent changes.
		// no deletes-with-other-changes.
		// no need to merge file contents.

#if TRACE_MRG
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "_v_process_ancestor_entry__hard_way..[%d]: composite1 [flags 0x%08x] [parent gid %s] [attr 0x%0x] [blob %s] [xattr %s] [entry %s] %s\n",
								   pData->use_marker_value,
								   flags,
								   pMrgCSetEntry_Composite->bufGid_Parent,
								   ((SG_uint32)pMrgCSetEntry_Composite->attrBits),
								   pMrgCSetEntry_Composite->bufHid_Blob,
								   pMrgCSetEntry_Composite->bufHid_XAttr,
								   pMrgCSetEntry_Composite->bufGid_Entry,
								   SG_string__sz(pMrgCSetEntry_Composite->pStringEntryname))  );
#endif
		pData->pMrgCSetStats_Result->nrChanged++;
		goto handled_entry__eat_conflict;
	}
	else
	{
#if TRACE_MRG
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "_v_process_ancestor_entry__hard_way..[%d]: composite2 [flags 0x%08x] [parent gid %s] [attr 0x%0x] [blob ?] [xattr %s] [entry %s] %s\n",
								   pData->use_marker_value,
								   flags,
								   pMrgCSetEntry_Composite->bufGid_Parent,
								   ((SG_uint32)pMrgCSetEntry_Composite->attrBits),
								   pMrgCSetEntry_Composite->bufHid_XAttr,
								   pMrgCSetEntry_Composite->bufGid_Entry,
								   SG_string__sz(pMrgCSetEntry_Composite->pStringEntryname))  );
#endif
		goto record_conflict;
	}
	
	/*NOTREACHED*/

record_conflict:
	pMrgCSetEntry_Composite->pMrgCSetEntryConflict = pMrgCSetEntryConflict;
	pMrgCSetEntryConflict->pMrgCSetEntry_Composite = pMrgCSetEntry_Composite;
	pMrgCSetEntryConflict->flags = flags;
	SG_ERR_CHECK(  SG_mrg_cset__register_conflict(pCtx,pData->pMrgCSet_Result,pMrgCSetEntryConflict)  );
	pMrgCSetEntryConflict = NULL;					// the result-cset owns it now
	pData->pMrgCSetStats_Result->nrConflicts++;		// TODO do we want this or should we just use length of conflict-list?
	if (flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__TBD)
		pData->pMrgCSetStats_Result->nrAutoMerge_TBD++;
	return;

handled_entry__eat_conflict:
	// we were able to resolve the (potential) conflict and completely deal with the entry
	// so we don't need to add the conflict-struct to the result-cset.
	SG_MRG_CSET_ENTRY_CONFLICT_NULLFREE(pCtx, pMrgCSetEntryConflict);
	return;

fail:
	SG_MRG_CSET_ENTRY_CONFLICT_NULLFREE(pCtx, pMrgCSetEntryConflict);
	return;
}

static SG_rbtree_foreach_callback _v_process_ancestor_entry;

/**
 * we get called once for each entry (file/symlink/directory) present in the
 * ancestor version of the version control tree.
 *
 * look at each of the k leaves and see what was done to it in that branch.
 *
 * compose/clone a new SG_mrg_cset_entry to represent the merger and add it
 * to the result-cset.  if there are conflicts, also include enough info to
 * let the caller ask the user what to do.
 */
static void _v_process_ancestor_entry(SG_context * pCtx,
									  const char * pszKey_Gid_Entry,
									  void * pVoidAssocData_MrgCSetEntry_Ancestor,
									  void * pVoid_Data)
{
	SG_mrg_cset_entry * pMrgCSetEntry_Ancestor = (SG_mrg_cset_entry *)pVoidAssocData_MrgCSetEntry_Ancestor;
	_v_data * pData = (_v_data *)pVoid_Data;
	SG_bool bHandled = SG_FALSE;

#if TRACE_MRG
	SG_ERR_IGNORE(  _trace_msg__v_ancestor(pCtx,"_v_process_ancestor_entry",
										   pData->use_marker_value,
										   pData->pMrgCSet_Ancestor,
										   pszKey_Gid_Entry)  );
#endif

	// try to do a trivial clone for unchanged entries and entries only changed in one leaf/branch.
	SG_ERR_CHECK(  _v_process_ancestor_entry__quick_scan(pCtx,pszKey_Gid_Entry,pMrgCSetEntry_Ancestor,pData,&bHandled)  );
	if (bHandled)
		return;

	// if that fails, we have some kid of conflict or potential conflict.  do it the
	// hard way and assume the worst.

	SG_ERR_CHECK(  _v_process_ancestor_entry__hard_way(pCtx,pszKey_Gid_Entry,pMrgCSetEntry_Ancestor,pData)  );

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * Starting from the LCA (which does not contain the entry), find the
 * first SPCA that does contains the entry.
 *
 * TODO When a file/symlink/directory is ADDED to VC, it will first
 * TODO appear in a single/unique new dagnode in the overall DAG.
 * TODO This dagnode MAY OR MAY NOT be in the DAGLCA graph.  If the
 * TODO entry does not appear in the LCA dagnode but does appear in
 * TODO one or more leaves, it must therefore have a FIRST appearance
 * TODO in the DAGLCA graph in either an SPCA or LEAF node (when looking
 * TODO from the ROOT towards the LEAVES).
 * TODO
 * TODO If it first appears in a SPCA node, I'm going to ASSERT that
 * TODO that there are NO sibling SPCA nodes where it also appears.
 * TODO That is, if there are 2 sibling SPCA nodes that have the entry,
 * TODO then there must be a parent SPCA node that also contains it.
 * 
 **********************************************************************
 * 
 * TODO 2010/05/18 It may be better to try to drill down thru the
 * TODO            SPCAs and find a better/closer ancestor; this may 
 * TODO            help to reduce the amount of noise the user sees.
 * 
 **********************************************************************
 */
static void _v_find_entry_in_first_spca(SG_context * pCtx,
										_v_data * pData,
										const char * pszGidEntryToFind,
										SG_bool * pbFound,
										SG_mrg_cset_entry ** ppMrgCSetEntryInFirstSPCA)
{
	SG_daglca_iterator * pDagLcaIter = NULL;
	SG_rbtree * prbImmediateDescendants = NULL;
	SG_rbtree_iterator * pIter = NULL;
	const char * pszHid_node;
	const char * pszHid_child;
	SG_mrg_cset * pMrgCSet_child;
	SG_mrg_cset_entry * pMrgCSetEntryInFirstSPCA = NULL;
	SG_mrg_cset_entry * pMrgCSetEntry_in_child;
	SG_int32 gen;
	SG_daglca_node_type nodeType;
	SG_bool bFoundDagLcaNode, bFoundRbtree, bFoundChild, bFoundEntry;
	SG_bool bResult = SG_FALSE;

	// TODO 2010/05/18 It may be quicker to just get a vector of the SPCAs
	// TODO            sorted by generation and look for the first one
	// TODO            that contains the entry.  By definition, the one
	// TODO            with the lowest generation (closest to the root)
	// TODO            will appear before any descendant.

	// for each "ancestor" node (of type LCA or SPCA) in the abbreviated
	// daglca graph beginning with LCA and working deeper.

	bFoundDagLcaNode = SG_TRUE;
	SG_ERR_CHECK(  SG_daglca__iterator__first(pCtx,
											  &pDagLcaIter,
											  pData->pMrg->pDagLca,
											  SG_FALSE,
											  &pszHid_node,
											  &nodeType,
											  &gen,
											  &prbImmediateDescendants)  );
	SG_ASSERT(  (nodeType == SG_DAGLCA_NODE_TYPE__LCA)  );
	while (bFoundDagLcaNode)
	{
		// for each EDGE from ancestor to children.

		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,
												  &pIter,
												  prbImmediateDescendants,
												  &bFoundRbtree,
												  &pszHid_child,
												  NULL)  );
		while (bFoundRbtree)
		{
			// find corresponding MrgCSet for the child.

			SG_ERR_CHECK(  SG_rbtree__find(pCtx,
										   pData->pMrg->prbCSets,
										   pszHid_child,
										   &bFoundChild,
										   (void **)&pMrgCSet_child)  );
			SG_ASSERT(  (bFoundChild)  );

			// children of this ancestor can either be SPCAs or LEAVES.
			// we only care about the SPCAs.

			if (pMrgCSet_child->nodeType == SG_DAGLCA_NODE_TYPE__SPCA)
			{
				// see if this CSET knows about the goal entry.

				SG_ERR_CHECK(  SG_rbtree__find(pCtx,
											   pMrgCSet_child->prbEntries,
											   pszGidEntryToFind,
											   &bFoundEntry,
											   (void **)&pMrgCSetEntry_in_child)  );
				if (bFoundEntry)
				{
#if TRACE_MRG
					SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
											   "MergeFindSPCA: entry [gid %s] observed in SPCA [gid %s]\n",
											   pszGidEntryToFind,
											   pszHid_child)  );
#endif
					SG_ASSERT(  (pMrgCSetEntryInFirstSPCA == NULL)  );
					pMrgCSetEntryInFirstSPCA = pMrgCSetEntry_in_child;
					bResult = SG_TRUE;
#if defined(DEBUG)
					// When debugging, let this loop complete so that we know that there
					// are no siblings that have this entry.
#else
					goto FoundIt;
#endif
				}
			}

			// continue with next sibling.
			
			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,
													 pIter,
													 &bFoundRbtree,
													 &pszHid_child,
													 NULL)  );
		}
#if defined(DEBUG)
		if (bResult)
			goto FoundIt;
#endif

		SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
		SG_RBTREE_NULLFREE(pCtx, prbImmediateDescendants);

		// continue with next ancestor node in daglca graph.

		SG_ERR_CHECK(  SG_daglca__iterator__next(pCtx,
												 pDagLcaIter,
												 &bFoundDagLcaNode,
												 &pszHid_node,
												 &nodeType,
												 &gen,
												 &prbImmediateDescendants)  );
	}

	// if we fall out of both loops, the entry must be original to a leaf.

FoundIt:
	*pbFound = bResult;
	*ppMrgCSetEntryInFirstSPCA = pMrgCSetEntryInFirstSPCA;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
	SG_RBTREE_NULLFREE(pCtx, prbImmediateDescendants);
	SG_DAGLCA_ITERATOR_NULLFREE(pCtx, pDagLcaIter);
}

/**
 * When SPCAs are present, look for entries that were not present in the
 * LCA and that are present in one or more leaves.  These entries were not
 * handled by the pass that started with the LCA entries.
 *
 * In the simple case when there are no SPCAs, these entries can *ONLY*
 * appear in one branch and indicate that a file/symlink/directory was
 * created in that branch.  For example, in the figure below, if we are
 * merging C and X (with LCA A), a file "foo" created in B will show up as
 * an ADD in C (relative to A and X).  (And we do not consider it a DELETE
 * in X because it was not present in A.)
 *
 * In a more complex case, if we are merging { K, L, X, Y } with LCA A,
 * a file "foo" created in B will probably show up as an ADD in K, L, and Y.
 * We need to disregard the info for X (since "foo" not being in X DOES NOT
 * represent a DELETE).  We need to identify the FIRST SPCA where the entry
 * does occur (C) and use that as the basis for a "synthetic ancestor".
 * Then see what the cumulative changes are for K, L, and Y relative to C
 * and compute the per-file merge result.  For example, if it is deleted
 * in Y and renamed "foo.K" in K and renamed "foo.L" in L, we would have
 * both a divergent-rename conflict and a delete-vs-rename conflict (just
 * like if the file were present in A).
 *
 * Implicit in the latter example is that "foo" was ADDED relative to A.
 *
 *       A
 *      /|\.
 *     / | \.
 *    /  |  \.
 *   B   |   X
 *   |   |
 *   |   |
 *   |   |
 *   C   D
 *   |\ /|
 *   | X |
 *   |/ \|
 *   E   F
 *   |\ /|\.
 *   | X | \.
 *   |/ \|  \.
 *   G   H   Y
 *   |\ /|
 *   | X |
 *   |/ \|
 *   I   J
 *   |\ /|
 *   | X |
 *   |/ \|
 *   K   L
 */
static SG_rbtree_foreach_callback _v_check_for_spca_and_leaf_k;

static void _v_check_for_spca_and_leaf_k(SG_context * pCtx,
										 const char * pszKey_Gid_Entry,
										 void * pVoidAssocData_MrgCSetEntry_Leaf_k,
										 void * pVoid_Data)
{
	SG_mrg_cset_entry * pMrgCSetEntry_Leaf_k = (SG_mrg_cset_entry *)pVoidAssocData_MrgCSetEntry_Leaf_k;
	_v_data * pData = (_v_data *)pVoid_Data;
	SG_mrg_cset_entry * pMrgCSetEntryInFirstSPCA = NULL;
	SG_rbtree * prbLeavesInSubset = NULL;
	SG_uint32 nrLeavesInSubset;
	SG_uint32 nrLeavesInWholeGraph;
	SG_bool bFoundInSPCA = SG_FALSE;

	// we get called once for each entry in each leaf.
	// ignore the entries that were already handled by
	// ancestor loop.  when we handle an entry we mark
	// it handled in all leaves, so this will be true
	// regardless of which leaf we are visiting.

	if (pMrgCSetEntry_Leaf_k->markerValue != MARKER_UNHANDLED)
		return;

	SG_ERR_CHECK(  _v_find_entry_in_first_spca(pCtx, pData, pszKey_Gid_Entry,
											   &bFoundInSPCA,
											   &pMrgCSetEntryInFirstSPCA)  );
	if (!bFoundInSPCA)
		return;

	// the entry was first added in an SPCA/interior ancestor.
	// get the set of leaves that are descended from it because
	// we need to ignore the leaves that don't.  by construction,
	// they cannot have referenced it anyway.  (this keeps us from
	// treating it as DELETE in those unrelated leaves.)

	SG_ERR_CHECK(  SG_daglca__get_all_descendant_leaves(pCtx, pData->pMrg->pDagLca,
														pMrgCSetEntryInFirstSPCA->pMrgCSet->bufHid_CSet,
														&prbLeavesInSubset)  );
	SG_ERR_CHECK(  SG_rbtree__count(pCtx, prbLeavesInSubset, &nrLeavesInSubset)  );

#if TRACE_MRG
	SG_ERR_IGNORE(  SG_daglca_debug__dump_to_console(pCtx, pData->pMrg->pDagLca)  );
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "LeavesInSubset: [%d]\n", nrLeavesInSubset)  );
	SG_ERR_IGNORE(  SG_rbtree_debug__dump_keys_to_console(pCtx, prbLeavesInSubset)  );
#endif

	SG_ASSERT(  (nrLeavesInSubset > 1)  );
	SG_ERR_CHECK(  SG_vector__length(pCtx, pData->pVec_MrgCSet_Leaves, &nrLeavesInWholeGraph)  );
	if (nrLeavesInWholeGraph == nrLeavesInSubset)
	{
		// for a 2-way merge (as opposed to an n-way merge), both of
		// these will be 2; that is, all of the leaves in the entire
		// graph are descended from the SPCA (if they weren't, then
		// we wouldn't have had an SPCA).
		//
		// so we can use all of the normal process ancestor stuff,
		// but we substitute the ancestor entry from the SPCA rather
		// than the LCA.

		SG_ERR_CHECK(  _v_process_ancestor_entry(pCtx, pszKey_Gid_Entry,
												 pMrgCSetEntryInFirstSPCA,
												 pData)  );
	}
	else
	{
		// when we have an n-way merge it is possible to have an actual
		// subset that has fewer leaves than the whole graph.
		//
		// TODO Use the list of HIDs in prbLeavesInSubset to build a
		// TODO VECTOR of pMrgCSets and temporarily replace pData->pVec_MrgCSet_leaves
		// TODO and then use the process ancestor stuff.

		SG_ERR_THROW2(  SG_ERR_NOTIMPLEMENTED,
						(pCtx, "TODO handle non-trivial daglca subset for gid [%s]",
						 pszKey_Gid_Entry)  );
	}

fail:
	SG_RBTREE_NULLFREE(pCtx, prbLeavesInSubset);
}

//////////////////////////////////////////////////////////////////

/**
 * we get called once for each entry (file/symlink/directory) present
 * in a leaf version of the version control tree.
 *
 * if the entry was not "marked", then it was not referenced when we
 * walked the ancestor cset.  this means that it is not present in the
 * ancestor.  so it must be a newly added entry in this branch.
 *
 * so we need to clone a new SG_mrg_cset_entry and add it to the
 * result-cset.
 *
 * since ADDS are unique (with a unique GID), it should not be
 * possible for this entry to appear in 2 leaves without having
 * appeared in the ancestor.  (assuming that the ancestor is
 * sufficiently close to the 2 leaves -- that is, assuming that
 * we did not skip any SPCAs.)  HOWEVER, when there are SPCAs
 * present we have to allow for the entry to have been created
 * between the LCA and an SPCA such that the entry is present in
 * more than one leaf.
 */
static SG_rbtree_foreach_callback _v_process_leaf_k_entry_addition;

static void _v_process_leaf_k_entry_addition(SG_context * pCtx,
											 SG_UNUSED_PARAM(const char * pszKey_Gid_Entry),
											 void * pVoidAssocData_MrgCSetEntry_Leaf_k,
											 void * pVoid_Data)
{
	SG_mrg_cset_entry * pMrgCSetEntry_Leaf_k = (SG_mrg_cset_entry *)pVoidAssocData_MrgCSetEntry_Leaf_k;
	_v_data * pData = (_v_data *)pVoid_Data;

	SG_UNUSED(pszKey_Gid_Entry);

	if (pMrgCSetEntry_Leaf_k->markerValue != MARKER_UNHANDLED)
		return;

#if TRACE_MRG
	SG_ERR_IGNORE(  _trace_msg__v_leaf_added(pCtx,"_v_process_leaf_additions",
											 pData->use_marker_value,
											 pData->pMrgCSet_Leaf_k,pData->kLeaf,
											 pMrgCSetEntry_Leaf_k)  );
#endif

	pMrgCSetEntry_Leaf_k->markerValue = pData->use_marker_value;

	SG_ERR_CHECK_RETURN(  SG_mrg_cset_entry__clone_and_insert_in_cset(pCtx,pMrgCSetEntry_Leaf_k,
																	  pData->pMrgCSet_Result,NULL)  );

	pData->pMrgCSetStats_Result->nrAdded++;
}

//////////////////////////////////////////////////////////////////

/**
 * we get called once for each entry present in the result-cset.
 * we check to see if we are an orphan.  that is, don't have a parent
 * directory.  This can happen if one branch moves us to a directory
 * that another branch deleted.
 *
 * If that happens, we cancel the delete for the parent directory
 * and give it a delete-conflict.  If we do cancel the delete, we
 * need to recursively visit the parent because it too may now be
 * an orphan.
 *
 * Because we are iterating on the entry-list, we cache the entries
 * in a orphan-to-add-list and then add them the entry-list after we have
 * finished the walk.
 *
 * TODO We may get some false-positives here if the entry had a DIVERGENT_MOVE,
 * TODO because picked an arbitrary directory to put the entry in.  Perhaps,
 * TODO if we made a better choice, we could have avoided creating an orphan
 * TODO in the first place.
 */
static SG_rbtree_foreach_callback _v_find_orphans;

static void _v_find_orphans(SG_context * pCtx,
							SG_UNUSED_PARAM(const char * pszKey_Gid_Entry),
							void * pVoidAssocData_MrgCSetEntry_Result,
							void * pVoid_Data)
{
	SG_mrg_cset_entry * pMrgCSetEntry_Result = (SG_mrg_cset_entry *)pVoidAssocData_MrgCSetEntry_Result;
	_v_data * pData = (_v_data *)pVoid_Data;

	SG_mrg_cset_entry * pMrgCSetEntry_Result_Parent;		// this entry's parent in the result-cset
	SG_mrg_cset_entry * pMrgCSetEntry_Parent_Ancestor;		// this entry's parent in the ancestor-cset
	SG_bool bFound;

	SG_UNUSED(pszKey_Gid_Entry);

	if (pMrgCSetEntry_Result->bufGid_Parent[0] == 0)		// the root node
		return;

	// look in the result-cset and see if the parent entry is present in the entry-list.

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,
										  pData->pMrgCSet_Result->prbEntries,
										  pMrgCSetEntry_Result->bufGid_Parent,
										  &bFound,
										  (void **)&pMrgCSetEntry_Result_Parent)  );
	if (bFound)
		return;

#if TRACE_MRG
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_v_find_orphans: orphan [entry %s] [%s] needs [parent entry %s]\n",
							   pMrgCSetEntry_Result->bufGid_Entry,
							   SG_string__sz(pMrgCSetEntry_Result->pStringEntryname),
							   pMrgCSetEntry_Result->bufGid_Parent)  );
#endif

	if (!pData->prbMrgCSetEntry_OrphanParents)
		SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pData->prbMrgCSetEntry_OrphanParents)  );

	// since the parent entry is not in the result-cset's entry-list, it will be
	// in the result-cset's deleted-list and therefore there are NO versions of
	// the entry in any of the leaves/branches.  the only version of the entry is
	// the ancestor one.

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,
										  pData->pMrgCSet_Result->prbDeletes,
										  pMrgCSetEntry_Result->bufGid_Parent,
										  &bFound,
										  (void **)&pMrgCSetEntry_Parent_Ancestor)  );
	if (bFound)
	{
		// we are the first orphan to need this parent directory.
		// add it to the orphan-to-add-list and remove it from the delete-list.

		SG_ERR_CHECK_RETURN(  SG_rbtree__update__with_assoc(pCtx,pData->prbMrgCSetEntry_OrphanParents,
															pMrgCSetEntry_Parent_Ancestor->bufGid_Entry,
															pMrgCSetEntry_Parent_Ancestor,NULL)  );
		SG_ERR_CHECK_RETURN(  SG_rbtree__remove(pCtx,pData->pMrgCSet_Result->prbDeletes,pMrgCSetEntry_Result->bufGid_Parent)  );
		pData->pMrgCSetStats_Result->nrDeleted--;

		// see if parent directory is an orphan, too.

		SG_ERR_CHECK_RETURN(  _v_find_orphans(pCtx,
											  pMrgCSetEntry_Parent_Ancestor->bufGid_Entry,
											  pMrgCSetEntry_Parent_Ancestor,
											  pData)  );
	}
	else
	{
		// another peer-orphan has already taken care of adding our parent to the orphan-to-add-list.

#if defined(DEBUG)
		SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,
											  pData->prbMrgCSetEntry_OrphanParents,
											  pMrgCSetEntry_Result->bufGid_Parent,
											  &bFound,
											  (void **)&pMrgCSetEntry_Parent_Ancestor)  );
		SG_ASSERT(  (bFound)  );
#endif
	}
}

/**
 * we get called once for each parent entry that we need to un-delete
 * so that an orphaned entry will have a parent.
 */
static SG_rbtree_foreach_callback _v_add_orphan_parents;

static void _v_add_orphan_parents(SG_context * pCtx,
								  const char * pszKey_Gid_Entry,
								  void * pVoidAssocData_MrgCSetEntry_Parent_Ancestor,
								  void * pVoid_Data)
{
	SG_mrg_cset_entry * pMrgCSetEntry_Ancestor = (SG_mrg_cset_entry *)pVoidAssocData_MrgCSetEntry_Parent_Ancestor;
	_v_data * pData = (_v_data *)pVoid_Data;

	SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict = NULL;
	SG_mrg_cset_entry * pMrgCSetEntry_Baseline = NULL;			// we do not own this
	SG_mrg_cset_entry * pMrgCSetEntry_Composite = NULL;			// we do not own this
	SG_uint32 nrLeaves, kLeaf;
	SG_bool bFound;

	SG_ERR_CHECK(  SG_rbtree__find(pCtx, pData->pMrg->pMrgCSet_Baseline->prbEntries, pszKey_Gid_Entry,
								   &bFound, (void **)&pMrgCSetEntry_Baseline)  );

#if TRACE_MRG
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_v_add_orphan_parents: [entry %s] [ancestor entryname %s][baseline entryname %s]\n",
							   pszKey_Gid_Entry,
							   SG_string__sz(pMrgCSetEntry_Ancestor->pStringEntryname),
							   ((pMrgCSetEntry_Baseline) ? SG_string__sz(pMrgCSetEntry_Baseline->pStringEntryname) : "(null)"))  );
#endif

	SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__alloc(pCtx,
													 pData->pMrgCSet_Result,
													 pMrgCSetEntry_Ancestor,
													 pMrgCSetEntry_Baseline,
													 &pMrgCSetEntryConflict)  );

	// by definition, the entry was deleted in all branches/leaves.

	SG_ERR_CHECK(  SG_vector__length(pCtx,pData->pVec_MrgCSet_Leaves,&nrLeaves)  );
	for (kLeaf=0; kLeaf<nrLeaves; kLeaf++)
	{
		SG_mrg_cset * pMrgCSet_Leaf_k = NULL;

		SG_ERR_CHECK(  SG_vector__get(pCtx,pData->pVec_MrgCSet_Leaves,kLeaf,(void **)&pMrgCSet_Leaf_k)  );
		SG_ERR_CHECK(  SG_mrg_cset_entry_conflict__append_delete(pCtx,pMrgCSetEntryConflict,pMrgCSet_Leaf_k)  );
	}

	if (pMrgCSetEntry_Baseline)
		SG_ERR_CHECK(  SG_mrg_cset_entry__clone_and_insert_in_cset(pCtx,pMrgCSetEntry_Baseline,pData->pMrgCSet_Result,&pMrgCSetEntry_Composite)  );
	else
		SG_ERR_CHECK(  SG_mrg_cset_entry__clone_and_insert_in_cset(pCtx,pMrgCSetEntry_Ancestor,pData->pMrgCSet_Result,&pMrgCSetEntry_Composite)  );

	pMrgCSetEntry_Composite->pMrgCSetEntryConflict = pMrgCSetEntryConflict;
	pMrgCSetEntryConflict->pMrgCSetEntry_Composite = pMrgCSetEntry_Composite;
	pMrgCSetEntryConflict->flags = SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_CAUSED_ORPHAN;

	SG_ERR_CHECK(  SG_mrg_cset__register_conflict(pCtx,pData->pMrgCSet_Result,pMrgCSetEntryConflict)  );
	pMrgCSetEntryConflict = NULL;					// the result-cset owns it now

	pData->pMrgCSetStats_Result->nrConflicts++;		// TODO do we want this or should we just use length of conflict-list?

	return;

fail:
	SG_MRG_CSET_ENTRY_CONFLICT_NULLFREE(pCtx, pMrgCSetEntryConflict);
}

//////////////////////////////////////////////////////////////////

/**
 * we get called once for each entry present in the result-cset.
 * we check to see if we are a moved directory that is in a pathname cycle.
 * 
 * this can happen when dir_a is moved into dir_b in one branch and dir_b is moved into
 * dir_a in another branch, so that if we populated the WD, we'd have .../dir_a/dir_b/dir_a/dir_b/...
 * that is the essense of the problem; there may be other intermediate directories involved,
 * such as .../dir_a/.../dir_b/.../dir_a/.../dir_b/...
 *
 * TODO since this can only happen when dir_a and dir_b are moved (because we assume that
 * TODO the tree was well formed in the ancestor version of the tree), i think that it is
 * TODO safe to say that we can mark each of the participating directory-moves as a conflict
 * TODO (and move them back to where they were in the baseline/ancestor version of the tree (as we
 * TODO do for divergent moves)) and let the user decide what to do.  this should let us
 * TODO populate the WD at least.
 *
 * this is kind of obscure, but if we have to un-move a directory, it could now be an
 * orphan (if the parent directory in the baseline/ancestor version was also deleted in a branch).
 * so when we do the un-move, we do the orphan check and add the parent to the orphan-to-add-list.
 */
static SG_rbtree_foreach_callback _v_find_path_cycles;

static void _v_find_path_cycles(SG_context * pCtx,
								SG_UNUSED_PARAM(const char * pszKey_Gid_Entry),
								void * pVoidAssocData_MrgCSetEntry_Result,
								void * pVoid_Data)
{
	SG_mrg_cset_entry * pMrgCSetEntry_Result = (SG_mrg_cset_entry *)pVoidAssocData_MrgCSetEntry_Result;
	_v_data * pData = (_v_data *)pVoid_Data;

	SG_mrg_cset_entry * pMrgCSetEntry_Ancestor;
	SG_mrg_cset_entry * pMrgCSetEntry_Result_TreeAncestor;
	SG_bool bFound;

	SG_UNUSED(pszKey_Gid_Entry);

	if (pMrgCSetEntry_Result->bufGid_Parent[0] == 0)		// the root node
		return;

	if (pMrgCSetEntry_Result->tneType != SG_TREENODEENTRY_TYPE_DIRECTORY)
		return;

	if (pMrgCSetEntry_Result->pMrgCSetEntryConflict)
	{
		if (pMrgCSetEntry_Result->pMrgCSetEntryConflict->flags
			& (SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_MOVE | SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_MOVE))
		{
			// we already have a MOVE conflict.
			if (pMrgCSetEntry_Result->pMrgCSetEntryConflict->pMrgCSetEntry_Baseline)
			{
				// we arbitrarily set the new target directory to the where it was
				// in the baseline.  this lets the user see it where it where they
				// last saw it before the merge.
				pMrgCSetEntry_Ancestor = pMrgCSetEntry_Result->pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor;
			}
			else
			{
				// the entry was not present in baseline, so we (arbitrarily) set
				// the target directory to where it was in the ancestor.  so technically,
				// this entry doesn't look like it moved (at least until the user decides
				// how to resolve the conflict).
				return;
			}
		}
		else
		{
			// we already have a link to what this entry looked like in the ancestor version of the tree.
			pMrgCSetEntry_Ancestor = pMrgCSetEntry_Result->pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor;
		}
	}
	else
	{
		// find the ancestor version of this entry.
		SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,pData->pMrgCSet_Ancestor->prbEntries,
											  pMrgCSetEntry_Result->bufGid_Entry,
											  &bFound,
											  (void **)&pMrgCSetEntry_Ancestor)  );
		if (!bFound)	// this directory was not present in the ancestor version of the tree,
			return;		// so it cannot be a move.
	}

	if (strcmp(pMrgCSetEntry_Result->bufGid_Parent,pMrgCSetEntry_Ancestor->bufGid_Parent) == 0)
		return;			// not a move.

	// we have a moved-directory.  walk up the tree (in the result-cset) and see
	// whether we reach the root directory or if we visit this node again.

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,pData->pMrgCSet_Result->prbEntries,
										  pMrgCSetEntry_Result->bufGid_Parent,
										  &bFound,(void **)&pMrgCSetEntry_Result_TreeAncestor)  );
	while (bFound)
	{
		if (strcmp(pMrgCSetEntry_Result->bufGid_Entry,pMrgCSetEntry_Result_TreeAncestor->bufGid_Entry) == 0)
		{
			// Oops, we are our own tree-ancestor in the new result-cset.
			goto handle_cycle;
		}

		if (pMrgCSetEntry_Result_TreeAncestor->bufGid_Parent[0] == 0)	// we reached the root node
			break;
		
		SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,pData->pMrgCSet_Result->prbEntries,
											  pMrgCSetEntry_Result_TreeAncestor->bufGid_Parent,
											  &bFound,(void **)&pMrgCSetEntry_Result_TreeAncestor)  );
	}
	return;

handle_cycle:
	// we found a repo-path-cycle, so this directory is an ancestor of itself.
	// add it to the cycle-to-add-list.  we defer making the actual change to this
	// entry because we want to get the other participants marked too.

#if TRACE_MRG
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   "_v_find_path_cycles: moved directory in path cycle [entry %s] [%s]\n",
							   pMrgCSetEntry_Result->bufGid_Entry,
							   SG_string__sz(pMrgCSetEntry_Result->pStringEntryname))  );	// cannot call __compute_repo_path_for_entry (obviously)
#endif

	if (!pData->prbMrgCSetEntry_DirectoryCycles)
		SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pData->prbMrgCSetEntry_DirectoryCycles)  );

	SG_ERR_CHECK_RETURN(  SG_rbtree__add__with_assoc(pCtx,pData->prbMrgCSetEntry_DirectoryCycles,
													 pMrgCSetEntry_Result->bufGid_Entry,
													 pMrgCSetEntry_Result)  );
}

/**
 * we get called once for each entry we put in the cycle-to-add-list.
 *
 * convert the entry to a conflict (if it isn't already) and un-move it
 * back to the parent that it was in ancestor version of the tree.
 *
 * This un-move may cause us to become an orphan (if the parent was also
 * deleted by a branch/leaf), so we need to fix-up that too.
 */
static SG_rbtree_foreach_callback _v_add_path_cycle_conflicts;

static void _v_add_path_cycle_conflicts(SG_context * pCtx,
										const char * pszKey_Gid_Entry,
										void * pVoidAssocData_MrgCSetEntry_Result,
										void * pVoid_Data)
{
	SG_mrg_cset_entry * pMrgCSetEntry_Result = (SG_mrg_cset_entry *)pVoidAssocData_MrgCSetEntry_Result;
	_v_data * pData = (_v_data *)pVoid_Data;

	SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict_Allocated = NULL;
	SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict;
	SG_mrg_cset_entry * pMrgCSetEntry_Ancestor;
	SG_mrg_cset_entry * pMrgCSetEntry_Baseline;
	SG_bool bFound;

	if (pMrgCSetEntry_Result->pMrgCSetEntryConflict)
	{
		// a conflict of some kind (which may or may not include directory moves)

		pMrgCSetEntryConflict = pMrgCSetEntry_Result->pMrgCSetEntryConflict;
		pMrgCSetEntry_Ancestor = pMrgCSetEntryConflict->pMrgCSetEntry_Ancestor;
		pMrgCSetEntry_Baseline = pMrgCSetEntryConflict->pMrgCSetEntry_Baseline;

		if (pMrgCSetEntry_Baseline)
		{
			// if entry was present in the baseline, we arbitrarily chose the
			// parent directory where the entry was in the baseline version of
			// the tree.
			SG_ASSERT(  ((pMrgCSetEntryConflict->flags & (SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_MOVE
														  | SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_MOVE)) != 0)  );
		}
		else
		{
			// if the entry was not present in the baseline, we arbitrarily
			// chose the parent directory where the entry was in the ancestor
			// version of the tree.
			// 
			// since we used the ancestor version we didn't add this entry to
			// the path-cycle list.  so we should never see these types of conflicts.
			SG_ASSERT(  (pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_MOVE) == 0  );
			SG_ASSERT(  (pMrgCSetEntryConflict->flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_MOVE) == 0  );
		}

		SG_ASSERT(  (pMrgCSetEntry_Ancestor)  );
		SG_ASSERT(  (pMrgCSetEntryConflict->pMrgCSetEntry_Composite == pMrgCSetEntry_Result)  );
	}
	else
	{
		// convert non-conflict into a conflict.

		// find the baseline version (if present) and the ancestor version of this entry.

		SG_ERR_CHECK(  SG_rbtree__find(pCtx,
									   pData->pMrg->pMrgCSet_Baseline->prbEntries,
									   pszKey_Gid_Entry,
									   &bFound,
									   (void **)&pMrgCSetEntry_Baseline)  );
		SG_ERR_CHECK(  SG_rbtree__find(pCtx,
									   pData->pMrgCSet_Ancestor->prbEntries,
									   pszKey_Gid_Entry,
									   &bFound,
									   (void **)&pMrgCSetEntry_Ancestor)  );
		SG_ASSERT(  bFound  );

		SG_ERR_CHECK(  _v_process_ancestor_entry__build_candidate_conflict(pCtx,
																		   pszKey_Gid_Entry,
																		   pMrgCSetEntry_Ancestor,
																		   pMrgCSetEntry_Baseline,
																		   pData,
																		   &pMrgCSetEntryConflict_Allocated)  );
		SG_ERR_CHECK(  SG_mrg_cset__register_conflict(pCtx,pData->pMrgCSet_Result,pMrgCSetEntryConflict_Allocated)  );
		pMrgCSetEntryConflict = pMrgCSetEntryConflict_Allocated;	// the result-cset owns it now
		pMrgCSetEntryConflict_Allocated = NULL;
		pData->pMrgCSetStats_Result->nrConflicts++;		// TODO do we want this or should we just use length of conflict-list?
		
		pMrgCSetEntry_Result->pMrgCSetEntryConflict = pMrgCSetEntryConflict;
		pMrgCSetEntryConflict->pMrgCSetEntry_Composite = pMrgCSetEntry_Result;
	}

	// un-move the result-cset version of the entry to parent directory
	// where it was in either the baseline or the ancestor version of the tree.

	if (pMrgCSetEntry_Baseline)
		SG_ERR_CHECK(  SG_gid__copy_into_buffer(pCtx,
												pMrgCSetEntry_Result->bufGid_Parent,SG_NrElements(pMrgCSetEntry_Result->bufGid_Parent),
												pMrgCSetEntry_Baseline->bufGid_Parent)  );
	else
		SG_ERR_CHECK(  SG_gid__copy_into_buffer(pCtx,
												pMrgCSetEntry_Result->bufGid_Parent,SG_NrElements(pMrgCSetEntry_Result->bufGid_Parent),
												pMrgCSetEntry_Ancestor->bufGid_Parent)  );

	pMrgCSetEntryConflict->flags |= SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__MOVES_CAUSED_PATH_CYCLE;

	// see if the un-moved entry is now an orphan.  if so, put the parent on the orphan-to-add-list.
	SG_ERR_CHECK(  _v_find_orphans(pCtx,pMrgCSetEntry_Result->bufGid_Entry,pMrgCSetEntry_Result,pData)  );
	return;

fail:
	SG_MRG_CSET_ENTRY_CONFLICT_NULLFREE(pCtx, pMrgCSetEntryConflict_Allocated);
	return;
}

//////////////////////////////////////////////////////////////////

void SG_mrg__compute_simple2(SG_context * pCtx,
							 SG_mrg * pMrg,
							 const char * pszInterimCSetName,
							 SG_mrg_cset * pMrgCSet_Ancestor,
							 SG_mrg_cset * pMrgCSet_Leaf_0,
							 SG_mrg_cset * pMrgCSet_Leaf_1,
							 SG_mrg_cset ** ppMrgCSet_Result,
							 SG_mrg_cset_stats ** ppMrgCSetStats_Result)
{

	SG_vector * pVec = NULL;

	SG_NULLARGCHECK_RETURN(pMrg);
	// pszInterimCSetName is optional
	SG_NULLARGCHECK_RETURN(pMrgCSet_Ancestor);
	SG_NULLARGCHECK_RETURN(pMrgCSet_Leaf_0);
	SG_NULLARGCHECK_RETURN(pMrgCSet_Leaf_1);
	SG_NULLARGCHECK_RETURN(ppMrgCSet_Result);
	SG_NULLARGCHECK_RETURN(ppMrgCSetStats_Result);

	SG_ERR_CHECK_RETURN(  SG_vector__alloc(pCtx,&pVec,2)  );
	SG_ERR_CHECK(  SG_vector__append(pCtx,pVec,pMrgCSet_Leaf_0,NULL)  );
	SG_ERR_CHECK(  SG_vector__append(pCtx,pVec,pMrgCSet_Leaf_1,NULL)  );

	SG_ERR_CHECK(  SG_mrg__compute_simpleV(pCtx,
										   pMrg,
										   pszInterimCSetName,
										   pMrgCSet_Ancestor,
										   pVec,
										   ppMrgCSet_Result,
										   ppMrgCSetStats_Result)  );

fail:
	SG_VECTOR_NULLFREE(pCtx,pVec);
}

//////////////////////////////////////////////////////////////////

void SG_mrg__compute_simpleV(SG_context * pCtx,
							 SG_mrg * pMrg,
							 const char * pszInterimCSetName,
							 SG_mrg_cset * pMrgCSet_Ancestor,
							 SG_vector * pVec_MrgCSet_Leaves,
							 SG_mrg_cset ** ppMrgCSet_Result,
							 SG_mrg_cset_stats ** ppMrgCSetStats_Result)
{
	_v_data data;
	SG_uint32 kLeaf, nrLeaves;
	SG_uint32 nrSPCA;

	data.pMrg = NULL;
	data.pVec_MrgCSet_Leaves = NULL;
	data.pMrgCSet_Result = NULL;
	data.pMrgCSetStats_Result = NULL;
	data.pMrgCSet_Ancestor = NULL;
	data.pMrgCSet_Leaf_k = NULL;
	data.kLeaf = 0;
	data.prbMrgCSetEntry_OrphanParents = NULL;
	data.prbMrgCSetEntry_DirectoryCycles = NULL;

	SG_NULLARGCHECK_RETURN(pMrg);
	// pszInterimCSetName is optional
	SG_NULLARGCHECK_RETURN(pMrgCSet_Ancestor);
	SG_NULLARGCHECK_RETURN(pVec_MrgCSet_Leaves);
	SG_NULLARGCHECK_RETURN(ppMrgCSet_Result);
	SG_NULLARGCHECK_RETURN(ppMrgCSetStats_Result);

	SG_ERR_CHECK_RETURN(  SG_vector__length(pCtx,pVec_MrgCSet_Leaves,&nrLeaves)  );
	SG_ARGCHECK_RETURN(  (nrLeaves >= 2), nrLeaves  );

#if TRACE_MRG
	SG_ERR_IGNORE(  _trace_msg__v_input(pCtx,"SG_mrg__compute_simpleV",pszInterimCSetName,pMrgCSet_Ancestor,pVec_MrgCSet_Leaves)  );
#endif

	// reset all markers on all entries in all of the leaf csets.  (later we can
	// tell which entries we did not examine (e.g. for added entries))
	// this is probably only necessary if we are in a complex merge and one of
	// these leaves have already been used in a partial merge.

	SG_ERR_CHECK(  _v_set_all_markers(pCtx,pVec_MrgCSet_Leaves, MARKER_UNHANDLED)  );

	data.pMrg = pMrg;
	data.pVec_MrgCSet_Leaves = pVec_MrgCSet_Leaves;
	data.pMrgCSet_Ancestor = pMrgCSet_Ancestor;

	// create a CSET-like structure to contain the candidate merge.
	// this will be just like a regular SG_mrg_cset only it will be
	// completely synthesized in memory rather than reflecting a
	// tree on disk.
	//
	// note that bufHid_CSet will be empty.  cset-hid's are only
	// computed after a commit and MERGE *NEVER* AUTO-COMMITS.
	// (and besides, this merge may be part of a larger merge that
	// we don't know about.)
	//
	// also note that since we are synthesizing this, we don't yet
	// know anything about the root entries (pMrgCSetEntry_Root and
	// bufGid_Root), so they will be empty until we're done with
	// the scan.

	SG_ERR_CHECK(  SG_mrg_cset__alloc(pCtx,pszInterimCSetName,
									  SG_MRG_CSET_ORIGIN__VIRTUAL,
									  SG_DAGLCA_NODE_TYPE__NOBODY,	// virtual nodes are not part of the DAGLCA
									  &data.pMrgCSet_Result)  );
	SG_ERR_CHECK(  SG_mrg_cset_stats__alloc(pCtx,&data.pMrgCSetStats_Result)  );

	// take each entry in the ancestor and see if has been changed in any of the leaves
	// and create the appropriate entry in the result-cset.
	//
	// note that this will only get things that were present in the ancestor.  they may
	// or may not still be present in the leaves; they may or may not have been changed
	// in one or more branches.  IT WILL NOT GET entries that were added in one or more
	// branches.

	data.use_marker_value = MARKER_LCA_PASS;
	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,pMrgCSet_Ancestor->prbEntries,_v_process_ancestor_entry,(void **)&data)  );

	// look at the unmarked entries that are present in one or more leaves (and that
	// were not present in the LCA).
	// 
	// When there are SPCAs it is possible for an entry to have been created
	// somewhere between the LCA and one of the SPCAs such that it can look to
	// us like it was added in multiple leaves (relative to the LCA) when actually
	// it was effectively added in one of the SPCAs and inherited by all of that
	// SPCA's descendants.  (It could even have been deleted in one of those
	// leaves, so it is not sufficient to just see that it was only added in one.)

	data.use_marker_value = MARKER_SPCA_PASS;
	SG_ERR_CHECK(  SG_daglca__get_stats(pCtx, pMrg->pDagLca, NULL, &nrSPCA, NULL)  );
	if (nrSPCA > 0)
	{
		for (kLeaf=0; kLeaf<nrLeaves; kLeaf++)
		{
			data.kLeaf = kLeaf;
			SG_ERR_CHECK(  SG_vector__get(pCtx,pVec_MrgCSet_Leaves,kLeaf,(void **)&data.pMrgCSet_Leaf_k)  );

			SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,data.pMrgCSet_Leaf_k->prbEntries,_v_check_for_spca_and_leaf_k,(void **)&data)  );
		}
	}
	
	// look at the unmarked entries in each leaf.  these must be new entries added to the
	// tree within that branch and after the ancestor was created.  add them to the result-cset.

	data.use_marker_value = MARKER_ADDITION_PASS;
	for (kLeaf=0; kLeaf<nrLeaves; kLeaf++)
	{
		data.kLeaf = kLeaf;
		SG_ERR_CHECK(  SG_vector__get(pCtx,pVec_MrgCSet_Leaves,kLeaf,(void **)&data.pMrgCSet_Leaf_k)  );

		SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,data.pMrgCSet_Leaf_k->prbEntries,_v_process_leaf_k_entry_addition,(void **)&data)  );
	}

	data.use_marker_value = MARKER_FIXUP_PASS;

	// check for orphans.  this can happen when one leaf adds an entry to a directory or
	// moves an existing entry into a directory that was deleted by another leaf.  if we
	// have an orphan, we cancel the delete on the parent; this needs to add the parent
	// into the prbEntries, but we don't want to change the prbEntries while we are walking
	// it, so we put them in a orphan-to-add-list.

	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,data.pMrgCSet_Result->prbEntries,_v_find_orphans,(void **)&data)  );
	if (data.prbMrgCSetEntry_OrphanParents)
	{
		SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,data.prbMrgCSetEntry_OrphanParents,_v_add_orphan_parents,(void **)&data)  );
		SG_RBTREE_NULLFREE(pCtx, data.prbMrgCSetEntry_OrphanParents);
	}

	// check for path cycles.  that is, directories that are their own ancestor in the tree.

	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,data.pMrgCSet_Result->prbEntries,_v_find_path_cycles,(void **)&data)  );
	if (data.prbMrgCSetEntry_DirectoryCycles)
	{
		SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,data.prbMrgCSetEntry_DirectoryCycles,_v_add_path_cycle_conflicts,(void **)&data)  );
		SG_RBTREE_NULLFREE(pCtx, data.prbMrgCSetEntry_DirectoryCycles);

		// this is kind of obscure, but if we had to un-move dir_a or dir_b, they could now be
		// orphans (if the parent directory in the ancestor version was deleted in a branch).

		if (data.prbMrgCSetEntry_OrphanParents)
		{
			SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,data.prbMrgCSetEntry_OrphanParents,_v_add_orphan_parents,(void **)&data)  );
			SG_RBTREE_NULLFREE(pCtx, data.prbMrgCSetEntry_OrphanParents);
		}
	}


#if defined(DEBUG)
	{
		SG_uint32 nrDeletes_in_List = 0;
		if (data.pMrgCSet_Result->prbDeletes)
			SG_ERR_IGNORE(  SG_rbtree__count(pCtx,data.pMrgCSet_Result->prbDeletes,&nrDeletes_in_List)  );
		SG_ASSERT(  (data.pMrgCSetStats_Result->nrDeleted == nrDeletes_in_List)  );
	}
#endif
#if defined(DEBUG)
	{
		SG_uint32 nrConflicts_in_List = 0;
		if (data.pMrgCSet_Result->prbConflicts)
			SG_ERR_IGNORE(  SG_rbtree__count(pCtx,data.pMrgCSet_Result->prbConflicts,&nrConflicts_in_List)  );
		SG_ASSERT(  (data.pMrgCSetStats_Result->nrConflicts == nrConflicts_in_List)  );
	}
#endif

	// TODO check for portability issues.
	//
	// TODO check for collisions and type 12345 issues.
	// TODO rewrite type 12345 notes in terms of new sg_mrg__ routines.

#if TRACE_MRG
	// When we print the stats here, we haven't attempted any of the auto-merges.
	SG_ERR_IGNORE(  _trace_msg__v_result(pCtx,"SG_mrg__compute_simpleV",data.pMrgCSet_Result,data.pMrgCSetStats_Result)  );
	SG_ERR_IGNORE(  _trace_msg__dump_delete_list(pCtx,"SG_mrg__compute_simpleV",data.pMrgCSet_Result)  );
	SG_ERR_IGNORE(  _trace_msg__dump_conflict_list(pCtx,"SG_mrg__compute_simpleV",data.pMrgCSet_Result)  );
#endif

	// we *DO NOT* set pMrg->pMrgCSet_FinalResult or pMrg->pMrgCSetStats_FinalResult
	// because we don't know whether we are an internal sub-merge or the top-level
	// final merge.

	*ppMrgCSet_Result = data.pMrgCSet_Result;
	*ppMrgCSetStats_Result = data.pMrgCSetStats_Result;
	return;

fail:
	SG_MRG_CSET_NULLFREE(pCtx, (data.pMrgCSet_Result) );
	SG_MRG_CSET_STATS_NULLFREE(pCtx, (data.pMrgCSetStats_Result) );
	SG_RBTREE_NULLFREE(pCtx, data.prbMrgCSetEntry_OrphanParents);
	SG_RBTREE_NULLFREE(pCtx, data.prbMrgCSetEntry_DirectoryCycles);
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_DO_SIMPLE_H
