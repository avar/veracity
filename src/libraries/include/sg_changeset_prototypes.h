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
 * @file sg_changeset_prototypes.h
 *
 * @details Routines for manipulating Changesets.
 *
 * Use these routines to extract information from a Changeset already
 * stored on disk.
 *
 * Use SG_committing as a transaction wrapper to create a new Changeset
 * and store it on disk.
 *
 * That is, anyone can call the various SG_changeset__get routines, but
 * only SG_committing should call the various SG_changeset__set routines.
 * In particular, SG_committing is responsible for updating our Blob-List.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_CHANGESET_PROTOTYPES_H
#define H_SG_CHANGESET_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Allocate a new/empty Changeset in memory.
 */
void SG_changeset__alloc__committing(SG_context * pCtx, SG_uint32 iDagNum, SG_changeset ** ppNew);

/**
 * Free a Changeset from memory.
 */
void SG_changeset__free(SG_context * pCtx, SG_changeset * pChangeset);

void SG_changeset__get_dagnum(SG_context * pCtx, const SG_changeset * pChangeset, SG_uint32 * piDagNum);

//////////////////////////////////////////////////////////////////

/**
 * Set the version of the software that created this Changeset.
 * That is, the version of the software that created the persistent
 * Changeset object in the repository -- not the version that is
 * simply allocating a Changeset structure in memory.
 */
void SG_changeset__set_version(SG_context * pCtx, SG_changeset * pChangeset, SG_changeset_version ver);

/**
 * Get the version of the software that created this Changeset object.
 */
void SG_changeset__get_version(SG_context * pCtx, const SG_changeset * pChangeset, SG_changeset_version * pver);

//////////////////////////////////////////////////////////////////

/**
 * A Changeset has one more more Parents (except for the initial/
 * first Changeset in a new dag, which has none).  In the typical
 * case, the changeset has one parent, the previous version of
 * things.  When there are multiple parents, this indicates a merge.
 *
 * In the Changeset DAG, there are backward links from this Changeset
 * to all of the parents.
 *
 * @defgroup SG_changeset__parent (CSET) Routines to get/set Changeset Parents.
 * @{
 */

/**
 * Add a Changeset Parent to this Changeset.
 *
 * We make a copy of the contents of pszidHidParent; you still own it.
 */
void SG_changeset__add_parent(SG_context * pCtx, SG_changeset * pChangeset, const char* pszidHidParent);

void SG_changeset__get_parents(
	SG_context * pCtx,
	const SG_changeset * pChangeset,
    SG_varray** ppva
	);

/**
 * @}
 */

//////////////////////////////////////////////////////////////////

/**
 * A Changeset has 2 Root Treenodes.
 *
 * <ol>
 * <li>User-Root Treenode.  This is an artificial directory containing
 * at most one treenode-entry -- the actual root directory of the
 * user's project.  This containing directory allows us to represent
 * the name, mode bits, GID, and etc of user's actual root directory.
 *
 * This differs a little bit from Vault where the user's actual root
 * directory is known as "$" within the repository and the user creates
 * and binds a working directory to "$".
 *
 * All user files and directories under version control are entries
 * and sub-entries of the actual root directory.
 * </li>
 *
 * <li>Config-Root Treenode.  This is a TBD.  It will be a directory of
 * any version-controlled control/config files that we may need and
 * that we don't want to store in the user's working directory.
 * </li>
 * </ol>
 *
 * Anytime a user file or sub-directory under version control is changed,
 * the containing Treenode will change; this will cause the containing
 * directory Treenode to change; and so on.  This bubble-up effect will
 * cause the User-Root Treenode to change.  So we expect a unique
 * User-Root Treenode for each Changeset where there are changes to files
 * or directories under version control,  If there are no changes to
 * version-controlled files from parent changeset(s), the User-Root
 * Treenode should be the same.
 *
 * We keep the actual user root directory in a separate Root Treenode
 * from the Root Treenode containing the config files because we don't
 * place any restrictions on the name of the actual root directory and
 * don't want any collisions with our control files.
 *
 * @defgroup SG_changeset__treenode (CSET) Routines to get/set Changeset Treenodes.
 * @{
 */

/**
 * Set the Root for the Changeset.
 *
 * You ***must*** set the Root -- even if you pass NULL.
 * That is, this routine must be called at least once.  You can call it
 * again to replace an existing value.
 *
 * For a tree DAG, pszidHid is the HID of the root treenode.
 * For a db DAG, pszidHid is the root of the dbdelta (formerly idtrie).
 *
 * If you call pszidHid with NULL, that implies that there is no
 * tree or database in this dag (in this commit ***and, therefore*** in the entire repository)
 * as of this snapshot.
 *
 * We make a copy of the contents of pszidHid; you still own it.
 */
void SG_changeset__set_root(SG_context * pCtx, SG_changeset * pChangeset, const char* pszidHid);

/**
 * Get the Root for the Changeset.
 *
 * You must NOT free the ID returned.
 */
void SG_changeset__get_root(SG_context * pCtx, const SG_changeset * pChangeset, const char** ppszidHid);

/**
 * @}
 */

//////////////////////////////////////////////////////////////////

/**
 * The Blob List is a list of blobs that this Changeset references
 * that were not referenced by at least one of the parent Changesets.
 * The purpose of this list is to allow for efficient push/pull
 * between Repositories.  That is, a recipient of this Changeset (who
 * already has at least one of the parents) is "probably" also going to
 * need these Blobs before they can do productive work.  The goal here
 * is to minimize the chattiness and/or the number of round-trips in
 * the exchange.
 *
 * Note that "the blob list" is now actually several lists, organized
 * by type.  The "type" is the type of the reference to the blob, not the type
 * of the blob.  A given blob might be a userfile in one changeset and a
 * treenode in another.  Unlikely, but possible.
 *
 * We say "probably" because Blobs are not owned by a Changeset.  An
 * individual Blob (and therefore its Blob HID) represents all instances
 * of objects that have identical content.  So adding a reference to a
 * particular Blob to this Changeset ***does not*** necessarily imply
 * that it was freshly created for this Changeset.
 *
 * This list doesn't tell anybody what changed or how.  You have to
 * walk the Root Treenodes and DB and compare them with those of a
 * parent Changeset for that.
 *
 * TODO It is possible that this list is not the absolute minimum.
 * That is, when we have a sparse Repository, it is possible that
 * this list contains Blobs that are newly referenced in the active
 * portion of the tree but that are also referenced in the sparse
 * portion of the parents.  And they look new to this Changeset
 * because we never needed to traverse the sparse portion of the
 * parents.  TODO Verify that this is OK.
 *
 * @defgroup SG_changeset__blob_list (CSET) Routines to get/set list of referenced Blob HIDs.
 * @{
 */

/**
 * Add a Blob ID to the Blob List.
 *
 * We silently ignore the given HID if it is already in the list.
 *
 * We make a copy of the contents of pszidHid; you still own it.
 */
void SG_changeset__add_blob_to_list(SG_context * pCtx, SG_changeset * pChangeset, const char* pszidHidBlob, SG_uint16 iBlobRefType, SG_int64 iBlobFullLength);

//////////////////////////////////////////////////////////////////

/**
 * A Changeset memory-object can be frozen/unfrozen.  We consider it "frozen"
 * when it is a representation of a Blob in the Repository; that is, one that
 * has been "saved" and therefore has a permanent HID.  We "freeze" the memory
 * object to prevent anyone from accidentally modifying the memory object.
 *
 * If you do modify a memory object and "re-save" it, you'll get a new HID.
 * This is not necessarily an error -- THE RE-SAVE CREATES A NEW CHANGESET
 * WITH A NEW HID.  it does NOT modify the existing one on disk.  That is,
 * a Changeset cannot be changed once it has been written to the Repo.
 *
 * We automatically freeze the memory-object when we think it is appropriate.
 * You cannot randomly freeze one.
 *
 * @defgroup SG_changeset__freeze (CSET) Routines to Freeze/Unfreeze Changesets.
 * @{
 */

/**
 * See if the given Changeset memory-object is frozen.
 */
void SG_changeset__is_frozen(SG_context * pCtx, const SG_changeset * pChangeset, SG_bool * pbFrozen);

/**
 * A frozen Changeset memory-object knows its HID.
 *
 * This routine returns a reference to our internal copy of the HID.  You ***MUST NOT***
 * free this.  This string becomes undefined if you free the changeset.
 *
 * This routine returns an error if it is not frozen.
 */
void SG_changeset__get_id_ref(SG_context * pCtx, const SG_changeset * pChangeset, const char** ppszidHidChangeset);

/**
 * @}
 */

//////////////////////////////////////////////////////////////////

/**
 * Save the Changeset memory-object to the Repository.
 *
 * This causes a Blob to be created containing the contents of this Changeset
 * and a HID to be assigned to it.  (Use __get_id to get a copy of the HID.)
 *
 * This causes the memory-object to be frozen.  You still own the Changeset
 * memory-object, but you cannot alter it further.
 *
 * Various validation checks will be performed to make sure that the Changeset
 * is well-formed and is up to spec.
 *
 * You probably should not call this routine directly; see SG_committing.
 *
 * TODO note about linking Changeset into DAG.
 */
void SG_changeset__save_to_repo(
	SG_context * pCtx,
	SG_changeset * pChangeset,
	SG_repo * pRepo,
	SG_repo_tx_handle* pRepoTx
	);

/**
 * Read a Changeset from the Repository into a Changeset memory-object.
 *
 * The Changeset memory-object is returned; you must free this.
 *
 * The Changeset memory-object is automatically frozen; you should not
 * try to alter it because it represents the essense of the stored/on-disk
 * Changeset.
 *
 * We optionally validate the HID given.  That is, we re-compute the HID
 * based upon the content we read from the disk and make sure that it
 * matches the HID that we used to fetch it.
 */
void SG_changeset__load_from_repo(
	SG_context * pCtx,
	SG_repo * pRepo,
	const char* pszidHidBlob,
	SG_changeset ** ppChangesetReturned
	);

void SG_changeset__load_from_staging(SG_context* pCtx,
									 const char* pszidHidBlob,
									 SG_staging_blob_handle* pStagingBlobHandle,
									 SG_changeset** ppChangesetReturned);

//////////////////////////////////////////////////////////////////

/**
 * Create a DAGNODE memory object from the contents of the given CHANGESET.
 * This DOES NOT in any way affect the DAG stored in the REPO; this is a
 * memory object only.  If this CHANGESET was created during an COMMIT,
 * the caller should use this DAGNODE to update the REPO after the CHANGESET
 * BLOB is written (see SG_committing).
 *
 * This routine extracts the child/parent relationships from the CHANGESET
 * and creates a DAGNODE with these EDGES.  It then freezes the DAGNODE so
 * that no other parents can be added.
 *
 * We require that the CHANGESET be frozen (we need the CHANGESET's HID
 * to have been already computed).
 *
 */
void SG_changeset__create_dagnode(
	SG_context * pCtx,
	const SG_changeset * pChangeset,
	SG_dagnode ** ppNewDagnode
	);

/**
 * Get the DAG GENERATION number for this CHANGESET.  This is the depth
 * of the CHANGESET/DAGNODE in the DAG.  This is a measure of the longest
 * path from this CHANGESET/DAGNODE to the root of the DAG.
 *
 * The initial checkin (with no parents) has generation 1.
 *
 * All other checkins have generation which is 1 more than the largest
 * parent generation.
 *
 * We require that the CHANGESET be frozen because the generation is
 * not computed until then.
 */
void SG_changeset__get_generation(SG_context * pCtx, const SG_changeset * pChangeset, SG_int32 * pGen);

void SG_changeset__get_bloblist_name(
        SG_context* pCtx,
        SG_uint16 iBlobRefType,
        const char** ppsz_key
        );

void SG_changeset__get_list_of_bloblists(
	SG_context * pCtx,
	const SG_changeset * pChangeset,
    SG_vhash** ppvh
	);

void SG_changeset__add_treepath(SG_context * pCtx, SG_changeset * pChangeset, const char* psz_gid, const char* psz_path);

void SG_changeset__get_treepaths(
	SG_context * pCtx,
	const SG_changeset * pChangeset,
    SG_vhash** ppvh
	);

END_EXTERN_C;

#endif//H_SG_CHANGESET_PROTOTYPES_H
