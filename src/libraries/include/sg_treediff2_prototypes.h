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
 * @file sg_treediff2_prototypes.h
 *
 * @details Routines to compute tree-diffs and facilitate merges.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_TREEDIFF2_PROTOTYPES_H
#define H_SG_TREEDIFF2_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_treediff2__alloc(SG_context * pCtx,
						 SG_repo * pRepo,
						 SG_treediff2 ** ppNew);

/**
 * allocate a treediff object but share the treenode cache from the
 * given treedfiff.  this is useful for merges so that we don't have
 * to hit the disk twice for the treenodes in the LCA.
 *
 * since we borrow the cache from the owner, don't free the owner treediff
 * until after you have freed this treediff.
 *
 * pTreeDiff_cache_owner is optional.  If present, we will share
 * the cache from the other treediff.
 */
void SG_treediff2__alloc__shared(SG_context * pCtx,
								 SG_repo * pRepo,
								 SG_treediff2 * pTreeDiff_cache_owner,
								 SG_treediff2 ** ppTreeDiff);

/**
 * low-level free for SG_treediff2.  used for testing cache sharing.
 * this should only be used in test code.
 */
void SG_treediff2__free_with_error(SG_context * pCtx, SG_treediff2 * pTreeDiff, SG_bool bForce);

/**
 * the real free routine for SG_treediff2 objects.
 */
void SG_treediff2__free(SG_context * pCtx, SG_treediff2 * pTreeDiff);

//////////////////////////////////////////////////////////////////

void SG_treediff2__get_repo(SG_context * pCtx,
							const SG_treediff2 * pTreeDiff,
							SG_repo ** ppRepo);
void SG_treediff2__get_kind(SG_context * pCtx,
							const SG_treediff2 * pTreeDiff,
							SG_treediff2_kind * pKind);
void SG_treediff2__get_working_directory_top(SG_context * pCtx,
											 const SG_treediff2 * pTreeDiff,
											 const SG_pathname ** ppPathWorkingDirectoryTop);
void SG_treediff2__get_hid_cset_original(SG_context * pCtx,
										 const SG_treediff2 * pTreeDiff,
										 const char ** pszHidCSetOriginal);

void SG_treediff2__foreach_with_diffstatus(SG_context * pCtx,
										   SG_treediff2 * pTreeDiff,
										   SG_diffstatus_flags dsMask,
										   SG_treediff2_foreach_callback pfnCB,
										   void * pVoidCallerData);
void SG_treediff2__report_status_to_vhash(SG_context * pCtx,
										  SG_treediff2 * pTreeDiff,
										  SG_vhash * pVhash,
										  SG_bool bIncludeUndoneEntries);
void SG_treediff2__foreach(SG_context * pCtx,
						   SG_treediff2 * pTreeDiff,
						   SG_treediff2_foreach_callback* pfnCB,
						   void * pVoidCallerData);

void SG_treediff2__iterator__first(SG_context * pCtx,
								   SG_treediff2 * pTreeDiff,
								   SG_bool * pbOK,
								   const char ** pszGidObject,
								   SG_treediff2_ObjectData ** ppOD_opaque,
								   SG_treediff2_iterator ** ppIter);

void SG_treediff2__iterator__next(SG_context * pCtx,
								  SG_treediff2_iterator * pIter,
								  SG_bool * pbOK,
								  const char ** pszGidObject,
								  SG_treediff2_ObjectData ** ppOD_opaque);

void SG_treediff2__iterator__free(SG_context * pCtx, SG_treediff2_iterator * pIter);

//////////////////////////////////////////////////////////////////

void SG_treediff2__count_changes(SG_context * pCtx,
								 SG_treediff2 * pTreeDiff,
								 SG_uint32 * pCount);

/**
 * For a COMPOSITE treediff, return the number of changes in each portion
 * of the diff.
 *
 * total:     number of overall changes (matches __count_changes()).
 *
 * baseline:  number of changes between arbitrary cset and the baseline.
 *
 * wd:        number of changes between the baseline and the WD.
 *
 * composite: number of items that changed in both halves.
 *
 */
void SG_treediff2__count_composite_changes_by_part(SG_context * pCtx,
												   SG_treediff2 * pTreeDiff,
												   SG_uint32 * pCount_total,
												   SG_uint32 * pCount_baseline,
												   SG_uint32 * pCount_pending,
												   SG_uint32 * pCount_composite);

//////////////////////////////////////////////////////////////////

/**
 * Compare the version control trees in the given changesets.
 *
 * It is an error to give the same changeset twice.
 *
 * This can fail if the REPO is sparse.
 */
void SG_treediff2__compare_cset_vs_cset(SG_context * pCtx,
										SG_treediff2 * pTreeDiff,
										const char * szHidCSet_0,
										const char * szHidCSet_1);

void SG_treediff2__set_wd_top(SG_context * pCtx,
							  SG_treediff2 * pTreeDiff,
							  const SG_pathname* pPathWorkingDirectoryTop);

/**
 * Begin a treediff against the pending tree.
 * This initializes fields within the SG_treediff2 structure
 * to begin receiving pending-tree data from SG_pendingtree.
 * The caller should call this, then call __annotate for each
 * changed item in the pending-tree, then call __finish.
 *
 * We allow 2 uses:
 *
 * [1] arbitrary-cset vs working-directory.  Here we compute
 *     the differences between the given cset and the baseline
 *     and adjust for changes in the pending-tree relative to
 *     the baseline.
 *
 * [2] baseline vs working-directory.
 *
 * szHidBaseline is required.
 *
 * szHidArbitraryCSet is optional.
 */
void SG_treediff2__begin_pendingtree_diff(SG_context * pCtx,
										  SG_treediff2 * pTreeDiff,
										  const char * szHidArbitraryCSet,
										  const char * szHidBaseline,
										  const SG_pathname * pPathWorkingDirectoryTop);

void SG_treediff2__finish_pendingtree_diff(SG_context * pCtx,
										   SG_treediff2 * pTreeDiff);

/**
 * Annotate an object in the treediff with values from the pending-tree.
 * The pending-tree code should provide baseline and current values
 * for each field.
 */
void SG_treediff2__pendingtree_annotate_cb(SG_context * pCtx,
										   SG_treediff2 * pTreeDiff,
										   SG_diffstatus_flags dsFlags,
										   const char * szGidObject,
										   const char * szGidObjectParent_base, const char * szGidObjectParent_pend,
										   SG_treenode_entry_type tneType,
										   const char * szEntryName_base, const char * szEntryName_pend,
										   const char * szHidContent_base, const char * szHidContent_pend,
										   const char * szHidXAttrs_base, const char * szHidXAttrs_pend,
										   SG_int64 attrBits_base, SG_int64 attrBits_pend);

//////////////////////////////////////////////////////////////////

/**
 * These generates a "status" report.  vv can use either of these to
 * have a one-line call and splat the resulting report to the console.
 */
/*void SG_treediff2__report_status_to_string(SG_context * pCtx,
										   SG_treediff2 * pTreeDiff,
										   SG_string * pStrReport,
										   SG_bool bIncludeUndoneEntries);
*/
void SG_treediff2__report_status_to_console(SG_context * pCtx,
											SG_treediff2 * pTreeDiff,
											SG_bool bIncludeUndoneEntries);

//////////////////////////////////////////////////////////////////

/**
 * Each of the SG_treediff2__ObjectData__get_* routines fetches individual
 * fields for an entry (file/symlink/folder) in the treediff result.
 */

void SG_treediff2__ObjectData__get_gid(SG_context * pCtx,
									   const SG_treediff2_ObjectData * pOD_opaque,
									   const char ** pszGidObject);

void SG_treediff2__ObjectData__get_repo_path(SG_context * pCtx,
											 const SG_treediff2_ObjectData * pOD_opaque,
											 const char ** pszRepoPath);

void SG_treediff2__ObjectData__get_dsFlags(SG_context * pCtx,
										   const SG_treediff2_ObjectData * pOD_opaque,
										   SG_diffstatus_flags * pdsFlags);

/**
 * Get the content HID of the entry before the changes.  This is the
 * original value in Ndx_Orig before.  You should be able to run a
 * content-diff using the content associated with this HID and the
 * content associated with the current content HID.
 */
void SG_treediff2__ObjectData__get_old_content_hid(SG_context * pCtx,
												   const SG_treediff2_ObjectData * pOD_opaque,
												   const char ** pszHid);

/**
 * Get the content HID of the entry after the changes.
 */
void SG_treediff2__ObjectData__get_content_hid(SG_context * pCtx,
											   const SG_treediff2_ObjectData * pOD_opaque,
											   const char ** pszHid);

void SG_treediff2__ObjectData__get_tneType(SG_context * pCtx,
										   const SG_treediff2_ObjectData * pOD_opaque,
										   SG_treenode_entry_type * ptneType);

/**
 * Get the entryname of the entry before any renames.
 */
void SG_treediff2__ObjectData__get_old_name(SG_context * pCtx,
											const SG_treediff2_ObjectData * pOD_opaque,
											const char ** pszName);

void SG_treediff2__ObjectData__get_name(SG_context * pCtx,
										const SG_treediff2_ObjectData * pOD_opaque,
										const char ** pszName);

void SG_treediff2__ObjectData__get_parent_gid(SG_context * pCtx,
											  const SG_treediff2_ObjectData * pOD_opaque,
											  const char ** pszGidParent);

void SG_treediff2__ObjectData__get_moved_from_gid(SG_context * pCtx,
												  const SG_treediff2_ObjectData * pOD_opaque,
												  const char ** pszOldParentGid);

void SG_treediff2__ObjectData__get_old_attrbits(SG_context * pCtx,
												const SG_treediff2_ObjectData * pOD_opaque,
												SG_int64 * pAttrbits);

void SG_treediff2__ObjectData__get_attrbits(SG_context * pCtx,
											const SG_treediff2_ObjectData * pOD_opaque,
											SG_int64 * pAttrbits);

void SG_treediff2__ObjectData__get_old_xattrs(SG_context * pCtx,
											  const SG_treediff2_ObjectData * pOD_opaque,
											  const char ** pszHid);

void SG_treediff2__ObjectData__get_xattrs(SG_context * pCtx,
										  const SG_treediff2_ObjectData * pOD_opaque,
										  const char ** pszHid);

//////////////////////////////////////////////////////////////////

/**
 * __get_moved_from_repo_path() returns the CURRENT repo-path of the OLD PARENT.
 * (not the old repo-path of the entry itself.)
 */
void SG_treediff2__ObjectData__get_moved_from_repo_path(SG_context * pCtx,
														const SG_treediff2 * pTreeDiff,
														const SG_treediff2_ObjectData * pOD_opqaue,
														const char ** pszOldParentRepoPath);

//////////////////////////////////////////////////////////////////

/**
 * find the entry in the treediff with the given object GID.
 *
 * You DO NOT own the pointer returned.
 */
void SG_treediff2__get_ObjectData(SG_context * pCtx,
								  const SG_treediff2 * pTreeDiff,
								  const char * szGidObject,
								  SG_bool * pbFound,
								  const SG_treediff2_ObjectData ** ppOD_opaque);

/**
 * find the entry in the treediff with the given repo-path.
 *
 * You DO NOT own either pointer returned.
 */
void SG_treediff2__find_by_repo_path(SG_context * pCtx,
									 SG_treediff2 * pTreeDiff,
									 const char * szRepoPathWanted,
									 SG_bool * pbFound,
									 const char ** pszGidObject_found,
									 const SG_treediff2_ObjectData ** ppOD_opaque_found);

//////////////////////////////////////////////////////////////////

/**
 * For a COMPOSITE treediff, return the dsFlags for each component
 * part of the diff and for the overall composite (which is what
 * we normally only report).
 *
 * baseline:  dsFlags for arbitrary-cset vs baseline (with baseline on the right).
 *
 * pending:   dsFlags for baseline vs WD (with WD on the right).
 *
 * composite: dsFlags for arbitrary-cset vs WD (with WD on the right).
 *
 */
void SG_treediff2__ObjectData__get_composite_dsFlags_by_part(SG_context * pCtx,
															 const SG_treediff2_ObjectData * pOD_opaque,
															 SG_diffstatus_flags * pdsFlags_baseline,
															 SG_diffstatus_flags * pdsFlags_pending,
															 SG_diffstatus_flags * pdsFlags_composite);

/**
 * Invert the flags.  If we are given the dsFlags for an "a-vs-b" diff,
 * what would the flags be if we had done "b-vs-a"?
 */
void SG_treediff2__Invert_dsFlags(SG_context * pCtx,
								  const SG_diffstatus_flags dsFlags_in,
								  SG_diffstatus_flags * pdsFlags_out);

/**
 *
 *
 */
void SG_treediff2__ObjectData__composite__get_parent_gids(SG_context * pCtx,
														  const SG_treediff2_ObjectData * pOD_opaque,
														  const char ** ppszGidParent_Orig,
														  const char ** ppszGidParent_Dest,
														  const char ** ppszGidParent_Pend);

//////////////////////////////////////////////////////////////////

void SG_treediff2__ObjectData__set_mark(SG_context * pCtx, SG_treediff2_ObjectData * pOD, SG_int32 newValue, SG_int32 * pOldValue);

void SG_treediff2__ObjectData__get_mark(SG_context * pCtx, const SG_treediff2_ObjectData * pOD, SG_int32 * pValue);

//////////////////////////////////////////////////////////////////

/**
 * SET a user-defined value on an entry in the treediff results.  this can be
 * any value you want; the treediff code does not look at it.
 *
 * the marker value is initialized to zero.
 */
void SG_treediff2__set_mark(SG_context * pCtx,
							SG_treediff2 * pTreeDiff,
							const char * szGidObject,
							SG_int32 newValue,
							SG_int32 * pOldValue);

/**
 * set the user-defined marker value on all entries in teh treediff result
 * to the given value.
 */
void SG_treediff2__set_all_markers(SG_context * pCtx,
								   SG_treediff2 * pTreeDiff,
								   SG_int32 value);

/**
 * get the current value of the user-defined marker for this entry.
 */
void SG_treediff2__get_mark(SG_context * pCtx,
							SG_treediff2 * pTreeDiff,
							const char * szGidObject,
							SG_int32 * pValue);

/**
 * call the given function for each entry in the treediff that has the matching value.
 */
void SG_treediff2__foreach_with_marker_value(SG_context * pCtx,
											 SG_treediff2 * pTreeDiff,
											 SG_int32 value,
											 SG_treediff2_foreach_callback pfnCB,
											 void * pVoidCallerData);

//////////////////////////////////////////////////////////////////

/**
 * Use the file-spec stuff to filter the things reported as dirty.
 * This effectively eliminates entries from the "dirty set" so you
 * should probably only call it once.
 */
void SG_treediff2__file_spec_filter(SG_context * pCtx,
									SG_treediff2 * pTreeDiff,
									SG_uint32 count_items, const char ** paszItems,
									SG_bool bRecursive,
									const char * const* ppszIncludes, SG_uint32 count_includes,
									const char * const* ppszExcludes, SG_uint32 count_excludes,
									SG_bool bIgnoreIgnores);


char* SG_treediff__get_status_section_name(SG_diffstatus_flags dsMask);

//////////////////////////////////////////////////////////////////

/**
 * Dump the Tree-Diff Raw Summary to a string suitable for printing.
 */
void SG_treediff2_debug__dump(SG_context * pCtx,
							  const SG_treediff2 * pTreeDiff,
							  SG_string * pStrOut,
							  SG_bool bAll);

/**
 * Dump the Tree-Diff Object Data to the console.  If bAll is TRUE, we dump everything we
 * know about.  If bAll is FALSE, we only dump the stuff with actual differences (the raw
 * summary).
 *
 * This should be callable from GDB.
 */
void SG_treediff2_debug__dump_to_console(SG_context * pCtx,
										 const SG_treediff2 * pTreeDiff,
										 SG_bool bAll);

/**
 * Compute some stats on the tree-diff.  You provide the stats structure.
 *
 * TODO decide if we want these to be debug-only or if we want them all the time.
 */
void SG_treediff2_debug__compute_stats(SG_context * pCtx,
									   SG_treediff2 * pTreeDiff,
									   SG_treediff2_debug__Stats * pStats);


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_TREEDIFF2_PROTOTYPES_H
