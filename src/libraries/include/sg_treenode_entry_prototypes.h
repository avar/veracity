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
 * @file sg_treenode_entry_prototypes.h
 *
 * @details Routines for manipulating Treenode Entries.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_TREENODE_ENTRY_PROTOTYPES_H
#define H_SG_TREENODE_ENTRY_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Allocate a new treenode entry structure in memory.
 */
void SG_treenode_entry__alloc(SG_context *, SG_treenode_entry ** ppNew);

/**
 * Clone a treenode entry
 */
void SG_treenode_entry__alloc__copy(SG_context *, SG_treenode_entry ** ppNew, const SG_treenode_entry* ptne);

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#define __SG_TREENODE_ENTRY__ALLOC__(pp,expr)		SG_STATEMENT(	SG_treenode_entry * _p = NULL;										\
																	expr;																\
																	_sg_mem__set_caller_data(_p,__FILE__,__LINE__,"SG_treenode_entry");	\
																	*(pp) = _p;															)

#define SG_TREENODE_ENTRY__ALLOC(pCtx,ppNew)							__SG_TREENODE_ENTRY__ALLOC__(ppNew, SG_treenode_entry__alloc(pCtx,&_p)  )
#define SG_TREENODE_ENTRY__ALLOC__COPY(pCtx,ppNew,pTreenodeEntry)		__SG_TREENODE_ENTRY__ALLOC__(ppNew, SG_treenode_entry__alloc__copy(pCtx,&_p,pTreenodeEntry) )

#else

#define SG_TREENODE_ENTRY__ALLOC(pCtx,ppNew)							SG_treenode_entry__alloc(pCtx,ppNew)
#define SG_TREENODE_ENTRY__ALLOC__COPY(pCtx,ppNew,pTreenodeEntry)		SG_treenode_entry__alloc__copy(pCtx,ppNew,pTreenodeEntry)

#endif

//////////////////////////////////////////////////////////////////

/**
 * Free a Treenode Entry structure.
 *
 * You probably do not need to call this because the containing Treenode
 * should own them.
 */
void SG_treenode_entry__free(SG_context * pCtx, SG_treenode_entry * pTreenodeEntry);

/**
 * Set the type of the entry.
 *
 * For SG_TREENODE_VERSION_1 version entries, this can be REGULAR_FILE, DIRECTORY, and SYMLINK.
 */
void SG_treenode_entry__set_entry_type(
	SG_context *,
	SG_treenode_entry * pTreenodeEntry,
	SG_treenode_entry_type tneType
	);

/**
 * Fetch the type of the entry.
 *
 * For SG_TREENODE_VERSION_1 version entries, this can be REGULAR_FILE, DIRECTORY, and SYMLINK.
 */
void SG_treenode_entry__get_entry_type(
	SG_context *,
	const SG_treenode_entry * pTreenodeEntry,
	SG_treenode_entry_type * ptneType
	);
/**
 * Set the Blob HID in this entry.   This links this directory-entry with its
 * content.  For example, for user-files, this is the HID to the Blob containing
 * the content of the file.
 *
 * The value of the ID is copied to private storage.  You still own the given ID.
 */

void SG_treenode_entry__set_hid_blob(
	SG_context *,
	SG_treenode_entry * pTreenodeEntry,
	const char* pszidHidBlob
	);

/**
 * Fetch the HID of the Blob for this entry.
 *
 * The caller must NOT free the returned ID.
 */
void SG_treenode_entry__get_hid_blob(
	SG_context *,
	const SG_treenode_entry * pTreenodeEntry,
	const char** ppszidHidBlob  /**< Caller must NOT free this. */
	);

/**
 * Set the attribute bits in this entry.
 *
 * The value of the ID is copied to private storage.  You still own the given ID.
 */

void SG_treenode_entry__set_attribute_bits(
	SG_context *,
	SG_treenode_entry * pTreenodeEntry,
	SG_int64 bits
	);

/**
 * Fetch attribute bits for this entry.
 */
void SG_treenode_entry__get_attribute_bits(
	SG_context *,
	const SG_treenode_entry * pTreenodeEntry,
	SG_int64* piBits
	);

/**
 * Set the xattrs HID in this entry.   This links this directory-entry with the
 * blob containing its xattrs.
 *
 * The value of the ID is copied to private storage.  You still own the given ID.
 */

void SG_treenode_entry__set_hid_xattrs(
	SG_context *,
	SG_treenode_entry * pTreenodeEntry,
	const char* pszidHidBlob
	);

/**
 * Fetch the HID of the xattrs for this entry.
 *
 * The caller must NOT free the returned ID.
 */
void SG_treenode_entry__get_hid_xattrs(
	SG_context *,
	const SG_treenode_entry * pTreenodeEntry,
	const char** ppszidHidBlob  /**< Caller must NOT free this. */
	);

/**
 * Set the Entryname for this entry.  This is a UTF-8 string.
 * The string should be encoded as it was received from the filesystem;
 * you should not try to normalize it in any way.
 *
 * The given buffer will be copied into private storage.
 */
void SG_treenode_entry__set_entry_name(
	SG_context *,
	SG_treenode_entry * pTreenodeEntry,
	const char * szEntryname
	);

/**
 * Fetch the Entryname for this entry.  This is a UTF-8 string.
 *
 * The pointer returned is to a private buffer.  DO NOT FREE IT.
 */
void SG_treenode_entry__get_entry_name(
	SG_context *,
	const SG_treenode_entry * pTreenodeEntry,
	const char ** pszEntryname
	);

/**
 * Inspect Treenode-Entry and make sure that it is valid and that it
 * is a version recognized by the current version of the software.
 *
 * Returns the entry's version number.
 *
 * We return either: SG_ERR_INVALIDARG, SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED,
 * or SG_ERR_OK.
 */
void SG_treenode_entry__validate__v1(
	SG_context *,
	const SG_treenode_entry * pTreenodeEntry
	);

/**
 * Compare two Treenode-Entries for equality.
 */
void SG_treenode_entry__equal(
	SG_context *,
	const SG_treenode_entry * pTreenodeEntry1,
	const SG_treenode_entry * pTreenodeEntry2,
	SG_bool * pbResult
	);

//////////////////////////////////////////////////////////////////

#if 0 // not currently needed
/**
 * Use the given treenode-entry to compute an "HID Signature" for this entry.
 *
 * NOTE: This is a special routine used by the Tree-Diff code.  Normally, tree-node-entries
 * ***DO NOT*** have HIDs -- because they are just a row in the containing tree-node array.
 * The signature that we compute here can be used to uniquely identify a version of this entry.
 * If anything in the entry, such as the filename or the file content or the mode bits or etc,
 * changes then we'll get a different value.
 *
 * The computed HID is returned.  You must free this.
 */
void SG_treenode_entry__compute_special_hid(
	SG_context *,
	const SG_treenode_entry * pEntry,
	char ** pszHidSig
	);
#endif

//////////////////////////////////////////////////////////////////

#if DEBUG
/**
 * DEBUG ROUTINE.
 *
 * Pretty print the contents of a Treenode-Entry.
 */
void SG_treenode_entry_debug__dump(
	SG_context * pCtx,
	const SG_treenode_entry * pTreenodeEntry,
	SG_repo * pRepo,
	const char * szGidObject,
	int indent,
	SG_bool bRecursive,
	SG_string * pStringResult
	);

/**
 * Pretty print TNE to console.
 */
void SG_treenode_entry_debug__dump_to_console(SG_context * pCtx,
											  const SG_treenode_entry * pTreenodeEntry,
											  SG_repo * pRepo,
											  const char * szGidObject,
											  int indent,
											  SG_bool bRecursive);

#endif//DEBUG

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_TREENODE_ENTRY_PROTOTYPES_H
