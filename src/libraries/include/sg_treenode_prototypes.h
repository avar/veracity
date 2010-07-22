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
 * @file sg_treenode_prototypes.h
 *
 * @details Routines for manipulating Treenodes.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_TREENODE_PROTOTYPES_H
#define H_SG_TREENODE_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Allocate a new/empty Treenode.
 */
void SG_treenode__alloc(SG_context *, SG_treenode ** ppNew);

/**
 * Feee a Treenode.
 */
void SG_treenode__free(SG_context * pCtx, SG_treenode * pTreenode);

//////////////////////////////////////////////////////////////////

/**
 * Compare two Treenodes for equality.  This compares all the
 * Treenode-Entries in both Treenodes and checks for identical
 * entries and no additions/deletions.
 *
 * Note: This is not a recursive routine.  Treenode-Entries that
 * represent sub-directories contain only the HID of the sub-Treenode
 * and not the actual sub-Treenode; so this equality test only
 * compares the HIDs not the actual sub-Treenode contents.  However,
 * two sub-Treenodes will be equal only when the have the same HID,
 * so we get the same effect as if we were recursive.
 */
void SG_treenode__equal(
	SG_context *,
	const SG_treenode * pTreenode1,
	const SG_treenode * pTreenode2,
	SG_bool * pbResult
	);

//////////////////////////////////////////////////////////////////

/**
 * Set the version of the software that created this Treenode.
 * That is, the version of the software that created the persistent
 * Treenode object in the Repository -- not the version that is
 * simply allocating a Treenode structure in memory.
 */
void SG_treenode__set_version(SG_context *, SG_treenode * pTreenode, SG_treenode_version ver);

/**
 * Get the version of the software that created this Treenode object.
 */
void SG_treenode__get_version(SG_context *, const SG_treenode * pTreenode, SG_treenode_version * pver);


//////////////////////////////////////////////////////////////////

/**
 * Add a Treenode-Entry to the given Treenode.  This adds a directory-entry
 * for a version-controlled file/sub-dir/etc to the containing directory.
 *
 * The entry is index by the entry's Object GID.  There can only be one
 * entry with a given Object GID in a directory.  If this GID is already
 * present, it will be replaced.
 *
 * The Treenode assumes ownership of the Treenode-Entry and will free it
 * when the Treenode is freed.  But you may continue to access and modify
 * fields within the entry.
 */
void SG_treenode__add_entry(
	SG_context *,
	SG_treenode * pTreenode,
	const char* pgidObject,
	SG_treenode_entry ** ppTreenodeEntry
	);

//////////////////////////////////////////////////////////////////

/**
 * Get the number of Treenode-Entries in the given Treenode.
 */
void SG_treenode__count(SG_context *, const SG_treenode * pTreenode, SG_uint32 * pnrEntries);


/**
 *
 * Get the nth Treenode-Entry in the given Treenode.
 *
 * DO NOT FREE EITHER OF THE RETURNED POINTERS.
 */
void SG_treenode__get_nth_treenode_entry__ref(
	SG_context * pCtx,
	const SG_treenode * pTreenode,
	SG_uint32 n,
	const char** ppszidGidObject_ref,
	const SG_treenode_entry ** ppTreenodeEntry
	);

/**
 * Look up a treenode_entry by gid.  Return a reference to the treenode_entry.
 *
 * DO NOT FREE THE RETURNED OBJECT.
 */
void SG_treenode__get_treenode_entry__by_gid__ref(
	SG_context * pCtx,
	const SG_treenode * pTreenode,
	const char* pszidGidObject_ref,
	const SG_treenode_entry ** ppTreenodeEntry
	);
//////////////////////////////////////////////////////////////////

/**
 * A Treenode memory-object can be frozen/unfrozen.  We consider it "frozen"
 * when it is a representation of a Blob in the Repository; that is, one that
 * has been "saved" and therefore has a permanent HID.  We "freeze" the memory
 * object to prevent anyone from accidentally modifying the memory object.
 *
 * If you do modify a memory object and "re-save" it, you'll get a new HID.
 * This is not necessarily an error -- THE RE-SAVE CREATES A NEW TREENODE
 * WITH A NEW HID.  it does NOT modify the existing one on disk.  That is,
 * a Treenode cannot be changed once it has been written to the Repo.
 *
 * We automatically freeze the memory-object when we think it is appropriate.
 * You cannot randomly freeze one.
 *
 * You may unfreeze one if you know what you're doing and think that it would
 * be more memory-efficient to unfreeze and modify rather than clone and modify.
 * This is a memory-object optimization only; it in no way affects the existing
 * Treenode with the existing HID in the Repo.
 *
 * @defgroup SG_treenode__freeze (TN) Routines to Freeze/Unfreeze Treenodes.
 * @{
 */

/**
 * See if the given Treenode memory-object is frozen.
 */
void SG_treenode__is_frozen(SG_context *, const SG_treenode * pTreenode, SG_bool * pbFrozen);

/**
 * Unfreeze the given Treenode.  See the discussion in the containing group for details.
 *
 * It is not an error to unfreeze a non-frozen Treenode.
 */
void SG_treenode__unfreeze(SG_context *, SG_treenode * pTreenode);

/**
 * A frozen Treenode memory-object knows its HID.
 *
 * This routine returns a copy of the HID; you are responsible for freeing it.
 *
 * This routine returns an error if it is not frozen.
 */
void SG_treenode__get_id(SG_context *, const SG_treenode * pTreenode, char** ppszidHidTreenode);

void SG_treenode__get_id_ref(SG_context *, const SG_treenode * pTreenode, const char** ppszidHidTreenode);

void SG_treenode__get_vhash_ref(SG_context *, const SG_treenode * pTreenode, SG_vhash** ppvh);

/**
 * @}
 */

//////////////////////////////////////////////////////////////////

/**
 * Save the Treenode memory-object to the Repository.
 *
 * This causes a Blob to be created containing the contents of this Treenode
 * and a HID to be assigned to it.  (Use __get_id to get a copy of the HID.)
 *
 * This causes the memory-object to be frozen.  You still own the Treenode
 * memory-object, but you cannot alter it further.
 *
 * Various validation checks will be performed to make sure that the Treenode
 * is well-formed and is up to spec.
 *
 * You probably should not call this routine directly; see SG_committing.
 */
void SG_treenode__save_to_repo(
	SG_context *,
	SG_treenode * pTreenode,
	SG_repo * pRepo,
	SG_repo_tx_handle* pRepoTx,
	SG_uint64* iBlobFullLength
	);

/**
 * Read a Treenode from the Repository into a Treenode memory-object.
 *
 * The Treenode memory-object is returned; you must free this.
 *
 * The Treenode memory-object is automatically frozen; you should not
 * try to alter it because it represents the essense of the stored/on-disk
 * Treenode.
 *
 * We optionally validate the HID given.  That is, we re-compute the HID
 * based upon the content we read from the disk and make sure that it
 * matches the HID that we used to fetch it.
 */
void SG_treenode__load_from_repo(
	SG_context *,
	SG_repo * pRepo,
	const char* pszidHidBlob,
	SG_treenode ** ppTreenodeReturned
	);

void SG_treenode__find_treenodeentry_by_path(
		SG_context* pCtx,
		SG_repo* pRepo,
		SG_treenode * pTreenodeParent,
		const char* pszSearchPath,
		char** ppszGID,
		SG_treenode_entry** pptne);

//////////////////////////////////////////////////////////////////

#if DEBUG
/**
 * DEBUG ROUTINE.
 *
 * Pretty print the contents of a Treenode by appending information
 * to the given string.  Indent gives indentation for each sub-directory.
 */
void SG_treenode_debug__dump__by_hid(SG_context * pCtx,
									 const char * szHidTreenodeBlob,
									 SG_repo * pRepo,
									 int indent,
									 SG_bool bRecursive,
									 SG_string * pStringResult);
#endif//DEBUG

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_TREENODE_PROTOTYPES_H
