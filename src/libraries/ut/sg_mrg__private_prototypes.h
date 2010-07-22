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
 * @file sg_mrg__private_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_PROTOTYPES_H
#define H_SG_MRG__PRIVATE_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_entry__free(SG_context * pCtx, SG_mrg_cset_entry * pMrgCSetEntry);

/**
 * Load the contents of the SG_treenode_entry into a new SG_mrg_cset_entry
 * and add it to the flat list of entries in the SG_mrg_cset.
 */
void SG_mrg_cset_entry__load(SG_context * pCtx,
							 SG_mrg_cset * pMrgCSet,
							 SG_repo * pRepo,
							 const char * pszGid_Parent,		// will be null for actual-root directory
							 const char * pszGid_Self,
							 const SG_treenode_entry * pTne_Self,
							 SG_mrg_cset_entry ** ppMrgCSetEntry_New);

/**
 * Load the SG_treenode for a directory and convert each SG_treenode_entry
 * into a SG_mrg_cset_entry and load it into the flat list of entries in
 * the SG_mrg_cset.
 */
void SG_mrg_cset_entry__load_subdir(SG_context * pCtx,
									SG_mrg_cset * pMrgCSet,
									SG_repo * pRepo,
									const char * pszGid_Self,
									SG_mrg_cset_entry * pMrgCSetEntry_Self,
									const char * pszHid_Blob);

/**
 * Compare 2 entries for equality.  For files and symlinks we compare everything.
 * For directories, we omit the contents of the directory (the HID-BLOB).
 */
void SG_mrg_cset_entry__equal(SG_context * pCtx,
							  const SG_mrg_cset_entry * pMrgCSetEntry_1,
							  const SG_mrg_cset_entry * pMrgCSetEntry_2,
							  SG_mrg_cset_entry_neq * pNeq);

/**
 * Clone the given entry and add it to the flat list of entries in the
 * given SG_mrg_cset.
 *
 * We do not copy the HID-BLOB for directories because it is a computed
 * field and the value from the source entry probably won't match the
 * value that will be computed on the directory in the destination CSET.
 *
 * If the entry does not have a parent, we assume that it is the root "@/"
 * directory and set the _Root fields in the destination CSET.
 *
 * You DO NOT own the returned pointer.
 */
void SG_mrg_cset_entry__clone_and_insert_in_cset(SG_context * pCtx,
												 const SG_mrg_cset_entry * pMrgCSetEntry_Src,
												 SG_mrg_cset * pMrgCSet_Result,
												 SG_mrg_cset_entry ** ppMrgCSetEntry_Result);

/**
 * We get called once for each entry which is a member of a collision
 * equivalence class.
 *
 * We want to assign a unique entryname to this file/symlink/directory
 * so that we can completely populate the directory during the merge.
 *
 * That is, if we have 2 branches that each created a new file
 * called "foo", we can't put both in the directory.  So create
 * something like "foo~L0~" and "foo~L1~"
 * and let the user decide the names when they RESOLVE things.
 */
SG_rbtree_foreach_callback SG_mrg_cset_entry__make_unique_entrynames;

//////////////////////////////////////////////////////////////////

void SG_mrg_cset__free(SG_context * pCtx, SG_mrg_cset * pMrgCSet);

void SG_mrg_cset__alloc(SG_context * pCtx,
						const char * pszOptionalName,
						SG_mrg_cset_origin origin,
						SG_daglca_node_type nodeType,
						SG_mrg_cset ** ppMrgCSet);

/**
 * Load the entire contents of the version control tree as of
 * this CSET into memory.
 *
 * This loads all of the entries (files/symlinks/directories) into
 * a flat list.  Each SG_treenode_entry is converted to a SG_mrg_cset_entry
 * and they are added to SG_mrg_cset.prbEntries.
 */
void SG_mrg_cset__load_entire_cset(SG_context * pCtx,
								   SG_mrg_cset * pMrgCSet,
								   SG_repo * pRepo,
								   const char * pszHid_CSet);

/**
 * Set the value of SG_mrg_cset_entry.markerValue to the given newValue
 * on all entries in the tree.
 */
void SG_mrg_cset__set_all_markers(SG_context * pCtx,
								  SG_mrg_cset * pMrgCSet,
								  SG_int64 newValue);

/**
 * Create a string containing the full repo-path of the entry
 * as it appeared in this version of this version control tree.
 *
 * The caller must free the returned string.
 */
void SG_mrg_cset__compute_repo_path_for_entry(SG_context * pCtx,
											  SG_mrg_cset * pMrgCSet,
											  const char * pszGid_Entry,
											  SG_bool bIncludeCSetName,
											  SG_string ** ppStringResult);		// caller must free this

/**
 * Create a flat list of the directories in the CSET.  Within each directory, create
 * a flat list of the entries within the directory.  This is not a true hierarchy
 * (because we don't have links between the directories), but it can simulate one
 * when needed.
 *
 * This populates the SG_mrg_cset.prbDirs and .pMrgCSetDir_Root members.  We use the
 * flat list of entries in .prbEntries to drive this.
 *
 * Once this is computed and bound to the SG_mrg_cset, we must consider the
 * SG_mrg_cset frozen.
 */
void SG_mrg_cset__compute_cset_dir_list(SG_context * pCtx,
										SG_mrg_cset * pMrgCSet);

/**
 * Inspect all of the directories and look for real and potential collisions.
 */
void SG_mrg_cset__check_dirs_for_collisions(SG_context * pCtx,
											SG_mrg_cset * pMrgCSet,
											SG_mrg * pMrg);

/**
 * Register a conflict on the merge.  That is, add the conflict info for an entry
 * (the ancestor entry and all of the leaf versions that changed it in divergent
 * ways) to the conflict-list.
 *
 * Note that we do not distinguish between hard-conflicts (that can't be automatically
 * resolved) and candiate-conflicts (where we just need to do diff3 or something before
 * we can decide what to do).
 *
 * We take ownership of the conflict object given, if there are no errors.
 */
void SG_mrg_cset__register_conflict(SG_context * pCtx,
									SG_mrg_cset * pMrgCSet,
									SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict);

/**
 * Register that an entry was deleted on the merge.  We do this by adding the GID-ENTRY
 * of the entry in a deleted-list in the result-cset.  (We can't store the entry as it
 * appeared in the source-cset that deleted it, because it, well, deleleted it.)
 *
 * As a convenience, we remember the original entry from the ancestor in the deleted-list
 * in case it would be helpful.  (<gid-entry>, <entry-from-ancestor>).
 *
 * Also, since the entry may have been deleted in multiple branches, we DON'T try to store
 * (<gid-entry>,<cset-leaf>) because we'd just get duplicates.
 *
 * We do not own the entry (but then you didn't own it either).
 */
void SG_mrg_cset__register_delete(SG_context * pCtx,
								  SG_mrg_cset * pMrgCSet,
								  SG_mrg_cset_entry * pMrgCSetEntry_Ancestor);

/**
 * We allow entries to be added to the CSET at random.
 * But as soon as you compute the directory list, we consider it frozen.
 */
#define SG_MRG_CSET__IS_FROZEN(pMrgCSet)	((pMrgCSet)->prbDirs != NULL)

/**
 * Look at all entries in the (result) cset and make unique entrynames
 * for any that have issues.  This includes:
 * [] members of a collision (multiple entries in different branches
 *    that want to use the same entryname (such as independent ADDS
 *    with the same name))
 * [] divergent renames (an entry given different names in different
 *    branches (and where we have to arbitrarily pick a name))
 * [] optionally, entries that had portability warnings (such as
 *    "README" and "readme" comming together in a directory for the
 *    first time).
 */
void SG_mrg_cset__make_unique_entrynames(SG_context * pCtx,
										 SG_mrg_cset * pMrgCSet);

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_stats__alloc(SG_context * pCtx, SG_mrg_cset_stats ** ppMrgCSetStats);
void SG_mrg_cset_stats__free(SG_context * pCtx, SG_mrg_cset_stats * pMrgCSetStats);

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_dir__free(SG_context * pCtx,
						   SG_mrg_cset_dir * pMrgCSetDir);

void SG_mrg_cset_dir__alloc__from_entry(SG_context * pCtx,
										SG_mrg_cset * pMrgCSet,
										SG_mrg_cset_entry * pMrgCSetEntry,
										SG_mrg_cset_dir ** ppMrgCSetDir);

void SG_mrg_cset_dir__add_child_entry(SG_context * pCtx,
									  SG_mrg_cset_dir * pMrgCSetDir,
									  SG_mrg_cset_entry * pMrgCSetEntry);

SG_rbtree_foreach_callback SG_mrg_cset_dir__check_for_collisions;

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_entry_collision__alloc(SG_context * pCtx,
										SG_mrg_cset_entry_collision ** ppMrgCSetEntryCollision);

void SG_mrg_cset_entry_collision__free(SG_context * pCtx,
									   SG_mrg_cset_entry_collision * pMrgCSetEntryCollision);

void SG_mrg_cset_entry_collision__add_entry(SG_context * pCtx,
											SG_mrg_cset_entry_collision * pMrgCSetEntryCollision,
											SG_mrg_cset_entry * pMrgCSetEntry);

//////////////////////////////////////////////////////////////////

void SG_mrg_cset_entry_conflict__alloc(SG_context * pCtx,
									   SG_mrg_cset * pMrgCSet,
									   SG_mrg_cset_entry * pMrgCSetEntry_Ancestor,
									   SG_mrg_cset_entry * pMrgCSetEntry_Baseline,
									   SG_mrg_cset_entry_conflict ** ppMrgCSetEntryConflict);

void SG_mrg_cset_entry_conflict__free(SG_context * pCtx,
									  SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict);

/**
 * Add this leaf to the list of branches where this entry was deleted.
 */
void SG_mrg_cset_entry_conflict__append_delete(SG_context * pCtx,
											   SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
											   SG_mrg_cset * pMrgCSet_Leaf_k);

/**
 * Add a this leaf's entry as a CHANGED-ENTRY to the (potential) conflict.
 * We also store the NEQ flags relative to the ancestor entry.
 */
void SG_mrg_cset_entry_conflict__append_change(SG_context * pCtx,
											   SG_mrg_cset_entry_conflict * pMrgCSetEntryConflict,
											   SG_mrg_cset_entry * pMrgCSetEntry_Leaf_k,
											   SG_mrg_cset_entry_neq neq);

//////////////////////////////////////////////////////////////////

/**
 * Compute what NEEDS TO HAPPEN to perform a simple merge
 * of CSETs L0 and L1 relative to common ancestor A and
 * producing M.  (We do not actually produce M.)
 *
 * We assume a clean WD.
 *
 *     A
 *    / \.
 *  L0   L1
 *    \ /
 *     M
 *
 * The ancestor may or may not be the absolute LCA.  Our
 * computation may be part of a larger merge that will use
 * our result M as a leaf in another portion of the graph.
 *
 * We return a "candidate SG_mrg_cset" that relects what
 * *should* be in the merge result if we decide to apply it.
 * This HAS NOT be added to the SG_mrg.prbCSets and you are
 * responsible for freeing it.
 *
 * pszInterimCSetName is a label for your convenience (and
 * may be used in debug/log messages).
 */
void SG_mrg__compute_simple2(SG_context * pCtx,
							 SG_mrg * pMrg,
							 const char * pszInterimCSetName,
							 SG_mrg_cset * pMrgCSet_Ancestor,
							 SG_mrg_cset * pMrgCSet_Leaf_0,
							 SG_mrg_cset * pMrgCSet_Leaf_1,
							 SG_mrg_cset ** ppMrgCSet_Result,
							 SG_mrg_cset_stats ** ppMrgCSetStats_Result);

/**
 * Compute what NEEDS TO HAPPEN to perform a simple merge
 * of N CSets relative to a common ancestor A and producing
 * a single M.  (We do not actually produce M.)
 *
 * We assume a clean WD.
 *
 * When N > 2, this is conceptually similar to GIT's Octopus Merge.
 * And like them, we may complain and stop if there are conflicts
 * that would require too much manual involvement; that is, for
 * large values of N, we only attempt to merge orthogonal changes.
 *
 *               A
 *    __________/ \__________
 *   /   /                   \.
 * L0  L1       ...           Ln-1
 *   \___\______   __________/
 *              \ /
 *               M
 *
 * The ancestor may or may not be the absolute LCA.  Our
 * computation may be part of a larger merge that will use
 * our result M as a leaf in another portion of the graph.
 *
 * We return a "candidate SG_mrg_cset" that relects what
 * *should* be in the merge result if we decide to apply it.
 * This HAS NOT be added to the SG_mrg.prbCSets and you are
 * responsible for freeing it.
 *
 * pszInterimCSetName is a label for your convenience (and
 * may be used in debug/log messages).
 */
void SG_mrg__compute_simpleV(SG_context * pCtx,
							 SG_mrg * pMrg,
							 const char * pszInterimCSetName,
							 SG_mrg_cset * pMrgCSet_Ancestor,
							 SG_vector * pVec_MrgCSets_Leaves,
							 SG_mrg_cset ** ppMrgCSet_Result,
							 SG_mrg_cset_stats ** ppMrgCSetStats_Result);

//////////////////////////////////////////////////////////////////

/**
 * Resolve all of the __FILE_EDIT_TBD conflicts in the given result-cset.
 * Update the stats if we were able to auto-merge any.
 */
void SG_mrg__automerge_files(SG_context * pCtx, SG_mrg * pMrg, SG_mrg_cset * pMrgCSet, SG_mrg_cset_stats * pMrgCSetStats);

//////////////////////////////////////////////////////////////////

/**
 * A builtin handler that knows how to invoke gnu's diff3 to try to
 * auto-merge files.  This is a fall-back handler that doesn't need
 * any configuration settings in the localsettings.
 *
 * TODO we may or may not want to keep this.  Rather, we may want to
 * TODO default to a hacked version of SGDM that does this.
 * TODO
 * TODO Eventually, we will want to have a handler that reads the localsettings
 * TODO parses the args and builds the appropriate command line.  At that point,
 * TODO we may want to seed the localsettings properly and get rid of this hard-
 * TODO coded handler -- but then again, the user can't break this version....
 *
 * TODO Also, eventually, we may want to have an "internal" handler that
 * TODO is linked-in rather than being an exec()....
 */
SG_mrg_external_automerge_handler SG_mrg_external_automerge_handler__builtin_diff3;

//////////////////////////////////////////////////////////////////

/**
 * Construct a pathname to a personal TEMP directory (probably) within the WD.
 * Use that to allocate the directory on disk.  We may use this directory for
 * holding files as we run external tools, such as diff3, during a merge.
 *
 * We want this TEMP directory to be local to the WD (so that files can be
 * moved/linked rather than copied -- both for input to diff3 and for the
 * auto-merge result -- IF THAT IS APPROPRIATE.  (And so that we don't have
 * to create a bunch of trash in the user's WD.)
 *
 * We want this TEMP directory to be for this process/session, so we don't
 * have to worry (SO MUCH) about collisions and complications from other
 * processes simultaneously doing things to this WD.  But this is only the
 * intent, there's nothing we can do to keep others from messing with our
 * TEMP directory.
 *
 * We create a pathname and store it in the SG_mrg so that subsequent calls
 * on this SG_mrg all return the same value.  You DO NOT own the returned pathname.
 *
 * The first call will try to actually create the directory on disk.
 *
 * We create our TEMP directory inside of the directory returned by
 * SG_workingdir__get_temp_path().
 */
void SG_mrg__get_temp_dir(SG_context * pCtx, SG_mrg * pMrg, const SG_pathname ** ppPathTempDir);

/**
 * delete out TEMP directory and everything in it.
 * free the temp-path so that a subsequent call to
 * SG_mrg__get_temp_dir() will create a new session.
 */
void SG_mrg__rmdir_temp_dir(SG_context * pCtx, SG_mrg * pMrg);

/**
 * Create a pathname in our WD-and-SESSION-private TEMP dir for one of
 * versions to be used as input to the auto-merge.
 */
void SG_mrg__make_automerge_temp_file_pathname(SG_context * pCtx,
											   SG_mrg_automerge_plan_item * pItem,
											   const char * pszLabelSuffix,
											   SG_pathname ** ppPathReturned);

/**
 * Create a pathname in our WD-and-SESSION-private TEMP dir for one
 * of the versions of a file to be used as input to the diff in the
 * merge-preview.
 */
void SG_mrg__make_hid_blob_temp_file_pathname(SG_context * pCtx, SG_mrg * pMrg,
											  const char * pszHidBlob,
											  SG_pathname ** ppPathReturned);

/**
 * Export the contents of the given file entry (which reflects a specific version)
 * into a temp file so that an external tool can use it.
 *
 * You own the returned pathname and the file on disk.
 */
void SG_mrg__export_to_temp_file(SG_context * pCtx, SG_mrg * pMrg,
								 const char * pszHidBlob,
								 const SG_pathname * pPathTempFile);

//////////////////////////////////////////////////////////////////

/**
 * Show Preview Info for the given file entry.  This includes the status of the merge
 * (whether we auto-merged it or not), the auto-merge plan (if we made one), possibly
 * a little "graph" of the relevant parts of the DAG if it was modified in 2 or more
 * leaves/branches, and then optionally invoke an external diff tool to show the changes
 * in content from the baseline.
 *
 * NOTE that some of this INFO is "absolute" (relative to the LCA and the overall merge)
 * and some of it is "relative" to the current baseline in the WD.
 */
void SG_mrg_preview__show_file(SG_context * pCtx, SG_mrg_preview_data * pPreviewData,
							   SG_mrg_cset_entry * pMrgCSetEntry_Baseline,		// optional
							   SG_mrg_cset_entry * pMrgCSetEntry_FinalResult);	// required

/**
 * Show Preview Info for the given file entry where the file was present (and unchanged)
 * in the baseline and was deleted from the final merge-result.  We don't have a lot of
 * info for this, but we can do the trivial diff of the baseline version with /dev/null
 * so that the entire baseline version appears as deleted lines.
 */
void SG_mrg_preview__show_deleted_file(SG_context * pCtx, SG_mrg_preview_data * pPreviewData, SG_mrg_cset_entry * pMrgCSetEntry_Baseline);

//////////////////////////////////////////////////////////////////

/**
 * To be called after we have computed the in-memory merge and
 * have the MERGE-GOAL in pMrgCSet_FinalResult.
 *
 * Our job is to figure out the sequence of disk operations
 * required to convert the current WD (which has a CLEAN view
 * of the BASELINE) into the FinalResult such that the user
 * could (if they want to) RUN the master plan and then COMMIT.
 *
 * This WD juggling is, in spirit, very similar to what we need
 * to do in REVERT and UPDATE to transform the WD.
 */
void SG_mrg__prepare_master_plan(SG_context * pCtx, SG_mrg * pMrg);

//////////////////////////////////////////////////////////////////

#define SG_MRG_AUTOMERGE_PLAN_ITEM_NULLFREE(pCtx,p)			SG_STATEMENT( SG_context__push_level(pCtx);       SG_mrg_automerge_plan_item__free(pCtx, p);    SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL; )
#define SG_MRG_CSET_ENTRY_COLLISION_NULLFREE(pCtx,p)        SG_STATEMENT( SG_context__push_level(pCtx);      SG_mrg_cset_entry_collision__free(pCtx, p);    SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL; )
#define SG_MRG_CSET_ENTRY_CONFLICT_NULLFREE(pCtx,p)         SG_STATEMENT( SG_context__push_level(pCtx);       SG_mrg_cset_entry_conflict__free(pCtx, p);    SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL; )
#define SG_MRG_CSET_ENTRY_NULLFREE(pCtx,p)                  SG_STATEMENT( SG_context__push_level(pCtx);                SG_mrg_cset_entry__free(pCtx, p);    SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL; )
#define SG_MRG_CSET_DIR_NULLFREE(pCtx,p)                    SG_STATEMENT( SG_context__push_level(pCtx);                  SG_mrg_cset_dir__free(pCtx, p);    SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL; )
#define SG_MRG_CSET_STATS_NULLFREE(pCtx,p)                  SG_STATEMENT( SG_context__push_level(pCtx);                SG_mrg_cset_stats__free(pCtx, p);    SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL; )
#define SG_MRG_CSET_NULLFREE(pCtx,p)                        SG_STATEMENT( SG_context__push_level(pCtx);                      SG_mrg_cset__free(pCtx, p);    SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL; )

//////////////////////////////////////////////////////////////////

#if TRACE_MRG

static SG_rbtree_foreach_callback _trace_msg__dir_list_as_hierarchy1;

static void _trace_msg__dir_list_as_hierarchy(SG_context * pCtx, const char * pszLabel, SG_mrg_cset * pMrgCSet);

static void _trace_msg__dump_entry_conflict_flags(SG_context * pCtx,
												  SG_mrg_cset_entry_conflict_flags flags,
												  SG_uint32 indent);

static void _trace_msg__v_input(SG_context * pCtx,
								const char * pszLabel,
								const char * pszInterimCSetName,
								SG_mrg_cset * pMrgCSet_Ancestor,
								SG_vector * pVec_MrgCSet_Leaves);
static void _trace_msg__v_ancestor(SG_context * pCtx,
								   const char * pszLabel,
								   SG_int32 marker_value,
								   SG_mrg_cset * pMrgCSet_Ancestor,
								   const char * pszGid_Entry);
static void _trace_msg__v_leaf_eq_neq(SG_context * pCtx,
									  const char * pszLabel,
									  SG_int32 marker_value,
									  SG_uint32 kLeaf,
									  SG_mrg_cset * pMrgCSet_Leaf_k,
									  const char * pszGid_Entry,
									  SG_mrg_cset_entry_neq neq);
static void _trace_msg__v_leaf_deleted(SG_context * pCtx,
									   const char * pszLabel,
									   SG_int32 marker_value,
									   SG_mrg_cset * pMrgCSet_Ancestor,
									   SG_uint32 kLeaf,
									   const char * pszGid_Entry);
static void _trace_msg__v_leaf_added(SG_context * pCtx,
									 const char * pszLabel,
									 SG_int32 marker_value,
									 SG_mrg_cset * pMrgCSet_Leaf_k,
									 SG_uint32 kLeaf,
									 SG_mrg_cset_entry * pMrgCSetEntry_Leaf_k);
static void _trace_msg__v_result(SG_context * pCtx,
								 const char * pszLabel,
								 SG_mrg_cset * pMrgCSet_Result,
								 SG_mrg_cset_stats * pMrgCSetStats_Result);
static void _trace_msg__dump_delete_list(SG_context * pCtx,
										 const char * pszLabel,
										 SG_mrg_cset * pMrgCSet_Result);

#endif

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_PROTOTYPES_H
