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
 * @file sg_committing_prototypes.h
 *
 * @details Routines to manage the transaction of creating a Changeset (a commit).
 *
 * When creating a Changeset you must do it within the context of a
 * CHANGESET TRANSACTION and a DAG TRANSACTION.  These are 2 independent
 * transactions that must happen in series.
 *
 * SG_committing is used to manage that process:
 *     <ul>
 *     <li>Use __alloc to allocate a SG_committing memory object and implicitly
 *         begin a CHANGESET TRANSACTION.</li>
 *     <li>Then use various __add routines to add stuff (User-files, Data Structures, and
 *         etc.) to the Repository.  The HIDs from these items will then be added to the
 *         CHANGESET being created.<li>
 *     <li>Use the various __set routines to set special properties
 *         on the CHANGESET (such as the HID of the checkin comment).</li>
 *     <li>Then use __end or __abort to complete or terminate the
 *         CHANGESET TRANSACTION;  after completing the first transaction,
 *         __end implicitly handles the DAG TRANSACTION.</li>
 *     </ul>
 *
 * The CHANGESET TRANSACTION is only concerned with the creation of BLOBS.
 * And if the Transaction is interrupted we *may* or *may not* leave some
 * unreferenced BLOBS laying around (this depends upon the storage layer)
 * and WE DO NOT CARE IF THERE ARE.
 *
 * After the CHANGESET TRANSACTION is completed, we cause the REPO's DAG to be
 * updated in an independent DAG TRANSACTION.  The CHANGESET (aka DAGNODE) is not
 * actually referencable until it has been added to the DAG.
 *
 * <b>Do not</b> use SG_changeset routines directly to create or modify a Changeset
 * or use SG_repo routines directly to add stuff to the Repository.
 * Please use SG_committing instead.
 *
 * Note that SG_committing only creates a Changset in the Repository.  It does not
 * update any NDX.
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_COMMITTING_PROTOTYPES_H
#define H_SG_COMMITTING_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Allocate a SG_committing structure in-memory and start a new Create
 * Changeset Transaction in the given Repository.
 *
 * @param ppNew  The returned structure.
 * You must call __end or __abort to either complete or abort the on-disk
 * transaction and free this data structure.
 *
 * @param pRepo  The Repository.
 *
 * @param verChangeset denotes the software version of the software
 * creating the Changeset.  The created Changeset will be held to the
 * standards of that version.  The Transaction/Changeset layer cannot
 * assume this value because they don't know what information the upper
 * layer will be providing to the Transaction.
 *
 * @see SG_changeset__parent
 *
 */
void SG_committing__alloc(SG_context * pCtx,
						  SG_committing ** ppNew,
						  SG_repo * pRepo,
						  SG_uint32 iDagNum,
						  const SG_audit* pq,
						  SG_changeset_version verChangeset);

/**
 * Complete the in-progress CHANGESET Transaction and create a CHANGESET
 * on disk and add a DAGNODE to the DAG using a DAG Transaction.  A CHANGESET is
 * only a Blob until it appears in the DAG.
 *
 * Upon successful completion, we free the memory associated with the pCommitting
 * object.
 *
 * If we encounter an error, we DO NOT call __abort().  This is to allow
 * you to see the abort error code/message.  You should then call __abort()
 * to free the memory and let us do whatever rollback we can do.
 *
 * If given, we return the Changeset object that we created.  The caller
 * must free this.  The CHANGESET returned will be frozen; the caller
 * should treat it as read-only.
 *
 * If given, we return the DAGNODE object that we created.  The caller
 * must free this.  The DAGNODE will be frozen.
 *
 */
void SG_committing__end(SG_context * pCtx,
						SG_committing * pCommitting,
						SG_changeset ** ppChangesetResult,
						SG_dagnode ** ppDagnodeResult);

/**
 * Abort the in-progress Transaction and free memory.
 *
 * This aborts the Changeset that is being constructed.  No blob will
 * be created for the Changeset and the Changeset will not appear in the
 * DAG.
 *
 * THIS IS NOT A ROLLBACK IN THE TRADITIONAL SENSE.
 *
 * Because the Repository consists of many
 * individual files and we cannot get anything resembling atomic
 * transactions in the filesystem, we cannot say that this abort will
 * undo anything that it started.  The only thing we can guarantee is
 * that we can hide a Blob in the Repository until it is completely
 * written (with a rename after it is complete).  We cannot guarantee
 * that we are the only process writing to the Repository (locks suck
 * and don't always work on remote filesystems).  Because Blobs are
 * stored under their HID -- a Blob may be shared many times -- it is
 * not the property of the Changeset being created.  When one of the
 * other routines in this file is asked to create a Blob, it tries to
 * create it -- but, because of sharing, it may already exist.  This
 * sharing may sound obscure, but it could be that someone is doing
 * the same checkin from 2 different shell windows -- we'll race to
 * create each of the Blobs in the Changeset.  Anyway, when we are
 * asked to create a Blob, we actually will try to create it and
 * silently succeed without error if it already exists.  Because of the
 * inherent races, we cannot delete the Blob during the abort, because
 * we cannot tell if a concurrent process lost the race and went on.
 * It is safe to have an unreferenced Blob; but it is *NOT* safe for us to
 * delete a Blob because we didn't complete.  (Because we can't tell if it
 * is actually unreferenced or *will be* referenced by another Chanageset
 * which is still being constructed.)  Things get a little more complicated
 * when you add Push/Pull operations between Repositories to the picture.
 *
 * The important thing here is that __abort will prevent the DAG from
 * being updated and will free the memory associated with this transaction.
 */
void SG_committing__abort(SG_context * pCtx,
						  SG_committing * pCommitting);

//////////////////////////////////////////////////////////////////

/**
 * This is used to add a reference to a Changeset Parent
 * to the Changeset that is being created.  A Changeset may have any
 * number of parents.
 *
 * You still own the given HID.
 */
void SG_committing__add_parent(SG_context * pCtx,
							   SG_committing * pCommitting,
							   const char* pszidHidParent);

//////////////////////////////////////////////////////////////////

/**
 * Add a Blob to the Repository using the bytes in the given string.
 *
 * If a Blob already exists for this given content, we silently continue.
 *
 * The computed HID is added to the Blob-List of the Changeset being created.
 *
 * You can use this for any string-based content.  This can include the
 * contents of a checkin comment, a small user-file that you happen to have
 * in memory, or a serialized data structure.  We make no assumptions about
 * the meaning of the content.
 *
 * You should *NOT* use this for data structures that have their own __add
 * routine, such as __add_treenode, because the data-type-specific __add
 * routines do extra data-integrity checking.
 *
 * If given, you must free the returned HID.
 *
 */
void SG_committing__add_bytes__string(SG_context * pCtx,
									  SG_committing * pCommitting,
                                      const char* psz_objectid,
									  SG_string * pString,
									  SG_uint16 iBlobRefType,
									  char** ppszidHidReturned);

/**
 * Add a Blob to the Repository using the bytes in the given buffer.
 *
 * If a Blob already exists for this given content, we silently continue.
 *
 * The computed HID is added to the Blob-List of the Changeset being created.
 *
 * You can use this for any memory buffer content.  This can include the
 * contents of a checkin comment, a small user-file that you happen to have
 * in memory, or a serialized data structure.  We make no assumptions about
 * the meaning of the content.
 *
 * You should *NOT* use this for data structures that have their own __add
 * routine, such as __add_treenode, because the data-type-specific __add
 * routines do extra data-integrity checking.
 *
 * If given, you must free the returned HID.
 *
 */
void SG_committing__add_bytes__buflen(SG_context * pCtx,
									  SG_committing * pCommitting,
                                      const char* psz_objectid,
									  const SG_byte * pBytes,
									  SG_uint32 len,
									  SG_uint16 iBlobRefType,
									  char** ppszidHidReturned);
/**
 * Add a Blob to the Repository using the content of the given file.
 * The file must be opened for reading; it will be rewound first.
 *
 * If a Blob already exists for this given content, we silently continue.
 *
 * The computed HID is added to the Blob-List of the Changeset being created.
 *
 * You can use this to store the contents of any file.  Normally we think of
 * this being used for adding new versions of user-files that were updated
 * in their working directory.  But it can also be used for checkin comments
 * that you got from an external editor, for example.  We make no assumptions
 * about the meaning of the content in the given file.
 *
 * If given, you must free the returned HID.
 *
 */
void SG_committing__add_file(SG_context * pCtx,
							 SG_committing * pCommitting,
                                      const char* psz_objectid,
							 SG_bool b_dont_bother,
							 SG_file * pFileRawData,
							 SG_uint16 iBlobRefType,
							 char** ppszidHidReturned);

/**
 * Add a Blob to the Repository using the content of the given Treenode object.
 * If a Blob already exists for this given content, we silently continue.
 *
 * The computed HID is added to the Blob-List of the Changeset being created.
 *
 * Only do this for a tree type dag.
 */
void SG_committing__add_treenode(SG_context * pCtx,
								 SG_committing * pCommitting,
								 SG_treenode * pTreenode,
								 char** ppszidHidReturned);

void SG_committing__add_treepath(SG_context * pCtx,
								 SG_committing * pCommitting,
                                 const char* psz_gid,
                                 const char* psz_path);

/**
 * Set the Root.  For a tree type dag, this is the HID of the root
 * treenode.  For a db type dag, this is the HID of the root
 * of the dbdelta, formerly idtrie.  This must be called even if nothing changed.
 * This will be added to the Blob-List.
 */
void SG_committing__set_root(SG_context * pCtx,
							 SG_committing * pCommitting,
							 const char* pszidHidTreenode);

/**
 * Only do this for a db type dag.
 * */
void SG_committing__add_dbrecord(SG_context * pCtx,
								 SG_committing * pCommitting,
								 SG_dbrecord * pRecord);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_COMMITTING_PROTOTYPES_H
