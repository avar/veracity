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
 * @file sg_treenode.c
 *
 * @details Routines for manipulating the contents of a Treenode object.
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

/**
 * SG_treenode is a structure containing information on a
 * single treenode (a single directory under version control).  This
 * includes all of the files/sub-directories/etc that are present in
 * the directory and the various attributes for each of them.
 *
 * Treenodes are converted to JSON and stored in Treenode-type Blobs
 * in the Repository.
 *
 * We hide the top-level vhash within our opaque SG_treenode
 * structure to keep things tidy and to allow us to later add or
 * cache expensive/frequently used stuff.
 */
struct SG_treenode
{
	/**
	 * We represent the fields in a Treenode as a top-level vhash.
	 * We keep the array of Treenode-Entries in a sub-vhash; items
	 * in the sub-vhash are keyed by their GID.  Attributes for
	 * each Treenode-Entry are contained within a sub-sub-vhash.
	 */
	SG_vhash *				m_vhash;

	/**
	 * We allow the Treenode memory-object to be frozen/unfrozen.
	 * After we save a freshly created Treenode (and create a Blob
	 * and HID for it), we freeze it so that the caller cannot make
	 * further changes to it by accident (once the HID is computed,
	 * we're locked).  Also, we freeze the Treenode memory-object
	 * when we read it from a Blob on disk.
	 *
	 * We are considered "frozen" when we have a non-null HID.
	 *
	 * We do allow the caller to "unfreeze" a Treenode, but don't
	 * recommend it.
	 */
	char*					m_pszidHidFrozen;
};

//////////////////////////////////////////////////////////////////

/**
 * We store the following keys/values in the top-level vhash.  Some
 * keys are always present and some may be version-dependent.  Since
 * we don't know what future keys/values may be required, we leave
 * this an open-ended list.
 */

/* Always present. */
#define KEY_VERSION					"ver"

/* Present in Version 1 Treenodes (may also be used by future versions). */
#define KEY_TREENODE_ENTRIES		"tne"

//////////////////////////////////////////////////////////////////

#define FAIL_IF_FROZEN(pTreenode)                                               \
	SG_STATEMENT(                                                               \
		SG_bool b = SG_TRUE;                                                    \
		SG_ERR_CHECK_RETURN(  SG_treenode__is_frozen(pCtx,(pTreenode),&b)  );   \
		if (b)                                                                  \
			SG_ERR_THROW_RETURN(SG_ERR_INVALID_WHILE_FROZEN);                   \
	)

//////////////////////////////////////////////////////////////////

void SG_treenode__alloc(SG_context * pCtx, SG_treenode ** ppNew)
{
	SG_treenode * pTreenode;

	SG_NULLARGCHECK_RETURN(ppNew);

	*ppNew = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pTreenode)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pTreenode->m_vhash)  );

	// we assume that we are not frozen.

	*ppNew = pTreenode;

	return;
fail:
	SG_TREENODE_NULLFREE(pCtx, pTreenode);
}

//////////////////////////////////////////////////////////////////

void SG_treenode__free(SG_context * pCtx, SG_treenode * pTreenode)
{
	if (!pTreenode)
		return;

	if (pTreenode->m_vhash)
		SG_VHASH_NULLFREE(pCtx, pTreenode->m_vhash);

	if (pTreenode->m_pszidHidFrozen)
		SG_NULLFREE(pCtx, pTreenode->m_pszidHidFrozen);

	SG_NULLFREE(pCtx, pTreenode);
}

//////////////////////////////////////////////////////////////////

void SG_treenode__equal(
	SG_context * pCtx,
	const SG_treenode * pTreenode1,
	const SG_treenode * pTreenode2,
	SG_bool * pbResult
	)
{
	SG_bool bIsFrozen1, bIsFrozen2;

	SG_NULLARGCHECK_RETURN(pTreenode1);
	SG_NULLARGCHECK_RETURN(pTreenode2);
	SG_NULLARGCHECK_RETURN(pbResult);

	SG_ARGCHECK_RETURN( pTreenode1->m_vhash != NULL , pTreenode1 );
	SG_ARGCHECK_RETURN( pTreenode2->m_vhash != NULL , pTreenode2 );

	if (pTreenode1 == pTreenode2)
	{
		*pbResult = SG_TRUE;
		return;
	}

	// if both are frozen, then we can just compare the stored HIDs and be done.
	// if either is not frozen, then we must do it the hard way and compare the
	// vhashes.

	SG_ERR_CHECK_RETURN(  SG_treenode__is_frozen(pCtx, pTreenode1,&bIsFrozen1)  );
	if (bIsFrozen1)
	{
		SG_ERR_CHECK_RETURN(  SG_treenode__is_frozen(pCtx, pTreenode2,&bIsFrozen2)  );
		if (bIsFrozen2)
		{
			*pbResult = (0 == (strcmp(pTreenode1->m_pszidHidFrozen,pTreenode2->m_pszidHidFrozen)));
			return;
		}
	}

	SG_ERR_CHECK_RETURN(  SG_vhash__equal(pCtx, pTreenode1->m_vhash,pTreenode2->m_vhash,pbResult)  );
}

//////////////////////////////////////////////////////////////////

void SG_treenode__set_version(SG_context * pCtx, SG_treenode * pTreenode, SG_treenode_version ver)
{
	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_ARGCHECK_RETURN( pTreenode->m_vhash != NULL , pTreenode );

	FAIL_IF_FROZEN(pTreenode);

	SG_ERR_CHECK_RETURN(  SG_vhash__update__int64(pCtx, pTreenode->m_vhash,KEY_VERSION,(SG_int64)ver)  );
}

void SG_treenode__get_version(SG_context * pCtx, const SG_treenode * pTreenode, SG_treenode_version * pver)
{
	SG_int64 v;

	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_ARGCHECK_RETURN( pTreenode->m_vhash != NULL , pTreenode );
	SG_NULLARGCHECK_RETURN(pver);

	*pver = TN_VERSION__INVALID;

	SG_vhash__get__int64(pCtx,pTreenode->m_vhash,KEY_VERSION,&v);

	if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))  // key not found, return __INVALID
	{
		SG_context__err_reset(pCtx);
		return;
	}

	SG_ERR_CHECK_RETURN_CURRENT;

	if (v > 0)									// if valid range, return it, otherwise __INVALID.
		*pver = (SG_treenode_version)v;
}

//////////////////////////////////////////////////////////////////

static void _sg_treenode__lookup_entry_vhash(
	SG_context * pCtx,
	SG_treenode * pTreenode,
	SG_vhash ** ppvhSub,
	SG_bool bAddIfNecessary
	)
{
	// get the handle to the second-level vhash within the top-level treenode vhash
	// that contains the list of treenode-entries in the directory.
	//
	// create the second-level vhash if necessary.

	SG_vhash * pvhSub = NULL;

	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_ARGCHECK_RETURN( pTreenode->m_vhash != NULL , pTreenode );
	SG_NULLARGCHECK_RETURN(ppvhSub);

	*ppvhSub = NULL;

	SG_vhash__get__vhash(pCtx,pTreenode->m_vhash,KEY_TREENODE_ENTRIES,&pvhSub);
	if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))  // ENTRIES vhash not found in top-level vhash, optionally create it.
	{
		if (!bAddIfNecessary)
			SG_ERR_RETHROW_RETURN;
		SG_context__err_reset(pCtx);

		SG_ERR_CHECK_RETURN(  SG_VHASH__ALLOC(pCtx, &pvhSub)  );

        {
            SG_vhash * pvhSub_ref = pvhSub;
            SG_vhash__add__vhash(pCtx,pTreenode->m_vhash,KEY_TREENODE_ENTRIES,&pvhSub);
            if (SG_context__has_err(pCtx))
            {
                SG_VHASH_NULLFREE(pCtx, pvhSub);
                SG_ERR_RETHROW_RETURN;
            }
            else
            {
                *ppvhSub = pvhSub_ref;
                return;
            }
        }
	}

	// some other error looking up ENTRIES in top-level vhash.
	SG_ERR_CHECK_RETURN_CURRENT;

	if (!pvhSub)  // found bogus SG_VARIANT_TYPE_NULL rather than second-level vhash
		SG_ERR_THROW_RETURN(SG_ERR_VARIANT_INVALIDTYPE);

	// found ENTRIES second-level vhash

	*ppvhSub = pvhSub;
}

//////////////////////////////////////////////////////////////////

void SG_treenode__add_entry(
	SG_context * pCtx,
	SG_treenode * pTreenode,
	const char* pgidObject,
	SG_treenode_entry ** ppTreenodeEntry
	)
{
	SG_vhash * pvhSub = NULL;

	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_ARGCHECK_RETURN( pTreenode->m_vhash != NULL , pTreenode );

	SG_NULLARGCHECK_RETURN(pgidObject);
	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, pgidObject)  );

	SG_NULL_PP_CHECK_RETURN(ppTreenodeEntry);

	FAIL_IF_FROZEN(pTreenode);

	// get the second-level ENTRIES vhash.

	SG_ERR_CHECK_RETURN(  _sg_treenode__lookup_entry_vhash(pCtx, pTreenode,&pvhSub,SG_TRUE)  );

	// we use the Object's GID as the key in the second-level vhash.
	// add the treenode_entry as the value of the second-level vhash;
	// (this makes it a third-level vhash)

	SG_ERR_CHECK_RETURN(  SG_vhash__update__vhash(pCtx, pvhSub, pgidObject, (SG_vhash **)ppTreenodeEntry)  );
}

//////////////////////////////////////////////////////////////////

static void _my_verify_treenode_entries_cb(
	SG_context * pCtx,
	SG_UNUSED_PARAM(void * pctxVoid),
	SG_UNUSED_PARAM(const SG_vhash * pvhSub),
	const char * szKey,
	const SG_variant * pvar
	)
{
	SG_vhash * pvhEntry;
	SG_bool bValidFormat;

	SG_UNUSED(pctxVoid);
	SG_UNUSED(pvhSub);

	// we get called for each item in the ENTRIES second-level vhash.
	//
	// Each key should be an Object GID in hex.
	//
	// Each value should be a Treenode-Entry (a third-level sub-vhash)
	// containing the attributes for the individual entries.

	// Verify that the key looks like a valid GID.

	SG_gid__verify_format(pCtx,szKey,&bValidFormat);
	if (!bValidFormat)
		SG_ERR_THROW_RETURN(SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED);

	// Verify value is a vhash

	SG_variant__get__vhash(pCtx,pvar,&pvhEntry);
	if (SG_context__has_err(pCtx))  // we expect SG_ERR_VARIANT_INVALIDTYPE
		SG_ERR_RESET_THROW_RETURN(SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED);
	if (!pvhEntry)						// happens when SG_VARIANT_TYPE_NULL
		SG_ERR_THROW_RETURN(SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED);

	// Let the Treenode-Entry code validate the fields within.

	SG_ERR_CHECK_RETURN(  SG_treenode_entry__validate__v1(pCtx, (const SG_treenode_entry *)pvhEntry)  );
}

static void _sg_treenode_v1__validate(SG_context * pCtx, const SG_treenode * pTreenode)
{
	// validate the TN_VERSION_1 fields.

	SG_vhash * pvhSub = NULL;

	// ENTRIES second-level vhash should be present.  but if they
	// never added any entries (and empty directory), it may not
	// be present.

	_sg_treenode__lookup_entry_vhash(pCtx,(SG_treenode *)pTreenode,&pvhSub,SG_FALSE);
	if (!SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))
	{
		if (SG_context__has_err(pCtx))
			SG_ERR_RESET_THROW_RETURN(SG_ERR_TREENODE_VALIDATION_FAILED);

		// we have a valid ENTRIES second-level vhash.  walk it and
		// validate the Treenode-Entries in the Treenode.  that is, we want
		// to make sure that each entry is well-formed and is of a version
		// that we understand.  we can use __foreach here because we assume
		// that we the second-level vhash is essentially an associative
		// array of Treenode-Entries keyed by GID.  There are no other fields
		// in the ENTRIES second-level vhash.

		SG_vhash__foreach(pCtx,pvhSub,_my_verify_treenode_entries_cb,NULL);
		if (SG_context__has_err(pCtx))
			SG_ERR_RESET_THROW_RETURN(SG_ERR_TREENODE_VALIDATION_FAILED);
	}

	// TODO check any other TN_VERSION_1 keys (besides VERSION and ENTRIES)
	// TODO in the top-level vhash.

	SG_context__err_reset(pCtx);
	return;
}

static void _sg_treenode__validate(SG_context * pCtx, const SG_treenode * pTreenode, SG_treenode_version * pver)
{
	// validate the given treenode vhash and make sure that it is a
	// well-formed representation of a Treenode.  This is used after
	// parsing a JSON script of incomming information.  The JSON
	// parser only knows JSON syntax; it doesn't know anything about
	// what fields we have or need and the data types (and value
	// ranges) they have.
	//
	// We return the version number that we found on the Treenode and
	// do our validation to that version -- to make sure that it is a
	// well-formed instance of a Treenode of that version.
	//
	// If we get a newer version of a Treenode than this instance of
	// the software knows about, we return an error.
	//
	// we silently map all parsing/inspection errors to a generic
	// SG_ERR_TREENODE_VALIDATION_FAILED error code.   the caller
	// doesn't need to see our dirty laundry.

	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_NULLARGCHECK_RETURN(pver);

	SG_treenode__get_version(pCtx,pTreenode,pver);
	if (SG_context__has_err(pCtx))
		SG_ERR_RESET_THROW_RETURN(SG_ERR_TREENODE_VALIDATION_FAILED);

	switch (*pver)
	{
	default:
		SG_ERR_THROW_RETURN(SG_ERR_TREENODE_VALIDATION_FAILED);

	case TN_VERSION_1:
		_sg_treenode_v1__validate(pCtx,pTreenode);
		if (SG_context__has_err(pCtx))
			SG_ERR_RESET_THROW_RETURN(SG_ERR_TREENODE_VALIDATION_FAILED);
	}
}

//////////////////////////////////////////////////////////////////

/**
 * Allocate a new Treenode and populate with the given JSON stream.  We assume
 * that this was generated by our SG_treenode__to_json() routine.
 *
 * We assume that the string contains a single top-level (vhash) object.
 *
 * The caller must free our result.
 *
 * Our caller should freeze us ASAP.
 */
static void _sg_treenode__alloc__from_json(SG_context * pCtx, SG_treenode ** ppNew, const char * szString)
{
	SG_treenode * pTreenode;
	SG_treenode_version ver = TN_VERSION__INVALID;

	SG_NULLARGCHECK_RETURN(ppNew);
	SG_NONEMPTYCHECK_RETURN(szString);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pTreenode)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &pTreenode->m_vhash, szString)  );

	// validate the fields in the Treenode and verify that it is well-formed
	// and defines a version that we understand.

	SG_ERR_CHECK(  _sg_treenode__validate(pCtx, pTreenode,&ver)  );

	*ppNew = pTreenode;

	return;
fail:
	SG_TREENODE_NULLFREE(pCtx, pTreenode);
}

//////////////////////////////////////////////////////////////////

/**
 * Convert the given Treenode to a JSON stream in the given string.
 * The string should be empty (but we do not check or enforce this).
 *
 * We sort the Treenode by Object GID before exporting.
 *
 * Our caller should freeze us ASAP.
 */
static void _sg_treenode__to_json(SG_context * pCtx, SG_treenode * pTreenode, SG_string * pStr)
{
	SG_treenode_version ver = TN_VERSION__INVALID;

	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_ARGCHECK_RETURN( pTreenode->m_vhash != NULL , pTreenode );
	SG_NULLARGCHECK_RETURN(pStr);

	//////////////////////////////////////////////////////////////////
	// This must be the last step before we convert to JSON.
	//
	// sort the vhash in increasing object gid order.  this is necessary
	// so that when we create the HID from the JSON stream we get a
	// consistent value (independent of the order that treenode-entries
	// were added to the treenode).

	SG_ERR_CHECK_RETURN(  SG_vhash__sort(pCtx, pTreenode->m_vhash,SG_TRUE,SG_vhash_sort_callback__increasing)  );

	// call our __validate_vhash routine here to catch missing/invalid fields that
	// will cause us to puke during later import.

	_sg_treenode__validate(pCtx,pTreenode,&ver);
	if (SG_context__has_err(pCtx) || (ver == TN_VERSION__INVALID))
		SG_ERR_RESET_THROW_RETURN(SG_ERR_TREENODE_VALIDATION_FAILED);

	// export the vhash to a JSON stream.  the vhash becomes the top-level
	// object in the JSON stream.

	SG_ERR_CHECK_RETURN(  SG_vhash__to_json(pCtx, pTreenode->m_vhash,pStr)  );
}

//////////////////////////////////////////////////////////////////

void SG_treenode__count(SG_context * pCtx, const SG_treenode * pTreenode, SG_uint32 * pnrEntries)
{
	// return the number of treenode-entries in the treenode.

	SG_vhash * pvhSub = NULL;

	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_ARGCHECK_RETURN( pTreenode->m_vhash != NULL , pTreenode );
	SG_NULLARGCHECK_RETURN(pnrEntries);

	*pnrEntries = 0;

	// get the ENTRIES second-level vhash.

	_sg_treenode__lookup_entry_vhash(pCtx,(SG_treenode *)pTreenode,&pvhSub,SG_FALSE);
	if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))  // second-level vhash not present, assume this means no entries.
	{
		SG_context__err_reset(pCtx);
		return;
	}
	SG_ERR_CHECK_RETURN_CURRENT;

	// return count of items in the ENTRIES second-level vhash.

	SG_ERR_CHECK_RETURN(  SG_vhash__count(pCtx, pvhSub, pnrEntries)  );
}
void SG_treenode__get_treenode_entry__by_gid__ref(
	SG_context * pCtx,
	const SG_treenode * pTreenode,
	const char* pszidGidObject_ref,
	const SG_treenode_entry ** ppTreenodeEntry
	)
{
	SG_vhash * pvhSub = NULL;
	const SG_variant * pVariant;
	SG_bool bValidFormat;

	SG_gid__verify_format(pCtx,pszidGidObject_ref,&bValidFormat);
	if (!bValidFormat)
		SG_ERR_THROW_RETURN(SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED);

	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_ARGCHECK_RETURN( pTreenode->m_vhash != NULL , pTreenode );

	// get the ENTRIES second-level vhash.
	_sg_treenode__lookup_entry_vhash(pCtx,(SG_treenode *)pTreenode,&pvhSub,SG_FALSE);
	if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))  // second-level vhash not present, assume this means no entries.
		SG_ERR_RESET_THROW_RETURN(SG_ERR_NOT_FOUND);

	SG_ERR_CHECK_RETURN(  SG_vhash__get__variant(pCtx, pvhSub,pszidGidObject_ref,&pVariant)  );
	if (ppTreenodeEntry)	// if they want the treenode-entry, let them borrow ours.
	{
		SG_variant__get__vhash(pCtx,pVariant,(SG_vhash **)ppTreenodeEntry);
		if (SG_context__has_err(pCtx))  // we expect SG_ERR_VARIANT_INVALIDTYPE
		{
			*ppTreenodeEntry = NULL;
			SG_ERR_RESET_THROW_RETURN(SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED);
		}
		if (!*ppTreenodeEntry)  // happens when SG_VARIANT_TYPE_NULL
		{
			*ppTreenodeEntry = NULL;
			SG_ERR_THROW_RETURN(SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED);
		}
	}

}
void SG_treenode__get_nth_treenode_entry__ref(
	SG_context * pCtx,
	const SG_treenode * pTreenode,
	SG_uint32 n,
	const char** ppszidGidObject_ref,
	const SG_treenode_entry ** ppTreenodeEntry
	)
{
	// get the nth treenode-entry in the given treenode.
	//
	// we return a pointer to a GID buffer for the entry.  do not free this.
	//
	// we return a pointer to the treenode-entry.  do not free this.

	SG_vhash * pvhSub = NULL;
	const char * szKey;
	const SG_variant * pVariant;
	SG_bool bValidFormat;

	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_ARGCHECK_RETURN( pTreenode->m_vhash != NULL , pTreenode );

	// get the ENTRIES second-level vhash.

	_sg_treenode__lookup_entry_vhash(pCtx,(SG_treenode *)pTreenode,&pvhSub,SG_FALSE);
	if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))  // second-level vhash not present, assume this means no entries.
		SG_ERR_RESET_THROW_RETURN(SG_ERR_ARGUMENT_OUT_OF_RANGE);  // so n>count.

	// get the nth pair from the ENTRIES second-level vhash.

	SG_ERR_CHECK_RETURN(  SG_vhash__get_nth_pair(pCtx, pvhSub,n,&szKey,&pVariant)  );

	SG_gid__verify_format(pCtx,szKey,&bValidFormat);
	if (!bValidFormat)
		SG_ERR_THROW_RETURN(SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED);

	if (ppTreenodeEntry)	// if they want the treenode-entry, let them borrow ours.
	{
		SG_variant__get__vhash(pCtx,pVariant,(SG_vhash **)ppTreenodeEntry);
		if (SG_context__has_err(pCtx))  // we expect SG_ERR_VARIANT_INVALIDTYPE
			SG_ERR_RESET_THROW_RETURN(SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED);
		if (!*ppTreenodeEntry)  // happens when SG_VARIANT_TYPE_NULL
			SG_ERR_THROW_RETURN(SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED);
	}

	if (ppszidGidObject_ref)	// let them borrow our key
	{
		*ppszidGidObject_ref = szKey;
	}
}

//////////////////////////////////////////////////////////////////

void SG_treenode__is_frozen(SG_context * pCtx, const SG_treenode * pTreenode, SG_bool * pbFrozen)
{
	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_NULLARGCHECK_RETURN(pbFrozen);
	*pbFrozen = (pTreenode->m_pszidHidFrozen != NULL);
}

static void _sg_treenode__freeze(SG_context * pCtx, SG_treenode * pTreenode, char* pszidHid)
{
	// this is static because we don't trust anyone to just hand us
	// any old HID and say it matches the value that ***would be***
	// computed from the ***current*** contents of the vhash.
	//
	// we take ownership of the given HID -- you no longer own it.

	SG_ASSERT(pTreenode!=NULL);
	SG_ASSERT(pszidHid!=NULL);

	if (pTreenode->m_pszidHidFrozen)
		SG_NULLFREE(pCtx, pTreenode->m_pszidHidFrozen);

	pTreenode->m_pszidHidFrozen = pszidHid;
}

void SG_treenode__unfreeze(SG_context * pCtx, SG_treenode * pTreenode)
{
	SG_NULLARGCHECK_RETURN(pTreenode);

	if (pTreenode->m_pszidHidFrozen)
	{
		SG_NULLFREE(pCtx, pTreenode->m_pszidHidFrozen);
		pTreenode->m_pszidHidFrozen = NULL;
	}
}

void SG_treenode__get_vhash_ref(SG_UNUSED_PARAM(SG_context * pCtx), const SG_treenode * pTreenode, SG_vhash** ppvh)
{
	SG_UNUSED(pCtx);
    *ppvh = pTreenode->m_vhash;
}

void SG_treenode__get_id_ref(SG_context * pCtx, const SG_treenode * pTreenode, const char** ppszidHidTreenode)
{
	// the caller wants to know our HID -- this is only valid when we are frozen.
	//
	// we let them borrow a reference to the HID in our Treenode and avoid a malloc.
	// this pointer is only good until they free the treenode.

	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_NULLARGCHECK_RETURN(ppszidHidTreenode);

	if (!pTreenode->m_pszidHidFrozen)
		SG_ERR_THROW_RETURN(SG_ERR_INVALID_UNLESS_FROZEN);

    *ppszidHidTreenode = pTreenode->m_pszidHidFrozen;
}

void SG_treenode__get_id(SG_context * pCtx, const SG_treenode * pTreenode, char** ppszidHidTreenode)
{
	// the caller wants to know our HID -- this is only valid when we are frozen.
	//
	// we give them a copy of it, rather than a reference to the HID in our Treenode.
	// that way, they can free the Treenode without the HID getting trashed.

	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_NULLARGCHECK_RETURN(ppszidHidTreenode);

	if (!pTreenode->m_pszidHidFrozen)
		SG_ERR_THROW_RETURN(SG_ERR_INVALID_UNLESS_FROZEN);

	SG_ERR_CHECK_RETURN(  SG_strdup(pCtx, pTreenode->m_pszidHidFrozen, ppszidHidTreenode) );
}

//////////////////////////////////////////////////////////////////

void SG_treenode__save_to_repo(
	SG_context * pCtx,
	SG_treenode * pTreenode,
	SG_repo * pRepo,
	SG_repo_tx_handle* pRepoTx,
	SG_uint64* iBlobFullLength
	)
{
	SG_string * pString = NULL;
	char* pszHidComputed = NULL;

	SG_NULLARGCHECK_RETURN(pTreenode);
	SG_NULLARGCHECK_RETURN(pRepo);

	FAIL_IF_FROZEN(pTreenode);

	// serialize treenode into JSON string.

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );
	SG_ERR_CHECK(  _sg_treenode__to_json(pCtx, pTreenode,pString)  );

	// remember the length of the blob
	*iBlobFullLength = (SG_uint64)SG_string__length_in_bytes(pString);


	// create a blob in the repository using the JSON string.  this computes the HID and returns it.

	SG_repo__store_blob_from_memory(pCtx,
		pRepo,
        pRepoTx,
        NULL, // TODO need the gid for this treenode entry
        SG_FALSE,
		(const SG_byte *)SG_string__sz(pString),
		SG_string__length_in_bytes(pString),
		&pszHidComputed
		);
	if (!SG_context__has_err(pCtx) || SG_context__err_equals(pCtx, SG_ERR_BLOBFILEALREADYEXISTS))
	{
		// freeeze the treenode memory-object and effectively make it read-only
		// from this point forward.  we give up our ownership of the HID.

		SG_ASSERT(  pszHidComputed  );
		_sg_treenode__freeze(pCtx, pTreenode,pszHidComputed);
		pszHidComputed = NULL;

		SG_STRING_NULLFREE(pCtx, pString);
		return;  // return _OK or _BLOBFILEALREADYEXISTS
	}

fail:
	SG_NULLFREE(pCtx, pszHidComputed);
	SG_STRING_NULLFREE(pCtx, pString);
}

void SG_treenode__load_from_repo(
	SG_context * pCtx,
	SG_repo * pRepo,
	const char* pszidHidBlob,
	SG_treenode ** ppTreenodeReturned
	)
{
	// fetch contents of a Treenode-type blob and convert to an Treenode object.

	SG_byte * pbuf = NULL;
	SG_treenode * pTreenode = NULL;
	char* pszidHidCopy = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pszidHidBlob);
	SG_NULLARGCHECK_RETURN(ppTreenodeReturned);

	*ppTreenodeReturned = NULL;

	// fetch the Blob for the given HID and convert from a JSON stream into an
	// allocated Treenode object.

	SG_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pRepo,pszidHidBlob,&pbuf,NULL)  );
	SG_ERR_CHECK(  _sg_treenode__alloc__from_json(pCtx, &pTreenode,(const char *)pbuf)  );
	SG_NULLFREE(pCtx, pbuf);
	pbuf = NULL;

	// make a copy of the HID so that we can stuff it into the Treenode and freeze it.
	// we freeze it because the caller should not be able to modify the Treenode by
	// accident -- since we are a representation of something on disk already.

	SG_ERR_CHECK(  SG_strdup(pCtx, pszidHidBlob, &pszidHidCopy)  );
	_sg_treenode__freeze(pCtx, pTreenode,pszidHidCopy);

	*ppTreenodeReturned = pTreenode;
	return;
fail:
	SG_NULLFREE(pCtx, pbuf);
	SG_TREENODE_NULLFREE(pCtx, pTreenode);
	SG_NULLFREE(pCtx, pszidHidCopy);
}

void _sg_treenode__find_treenodeentry_recursive(SG_context* pCtx, SG_repo* pRepo, SG_treenode * pTreenodeParent, SG_string * pPathSoFar, const char* pszSearchPath, char** ppszGID, SG_treenode_entry** ptne)
{
	SG_uint32 count;
	SG_uint32 i;
	SG_string * pPathOfSubchild = NULL;
	SG_ERR_CHECK(  SG_treenode__count(pCtx, pTreenodeParent, &count)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pPathOfSubchild)  );

	for (i=0; i<count; i++)
	{
		const SG_treenode_entry* pEntry = NULL;
		const char* pszName = NULL;
		const char* pszGIDTemp = NULL;

		SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx, pTreenodeParent, i, &pszGIDTemp, &pEntry)  );
		SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx, pEntry, &pszName)  );

		if (SG_string__length_in_bytes(pPathSoFar) == 0)
		{
			SG_ERR_CHECK(  SG_string__append__sz(pCtx, pPathOfSubchild, pszName)  );
		}
		else
		{
			SG_ERR_CHECK(  SG_string__set__string(pCtx, pPathOfSubchild, pPathSoFar)  );
			SG_ERR_CHECK(  SG_repopath__append_entryname(pCtx, pPathOfSubchild, pszName, SG_FALSE)  );
		}


		// if they match, we've found it, set the treenode_entry and return
		if (strcmp(SG_string__sz(pPathOfSubchild), pszSearchPath) == 0)
		{

			SG_ERR_CHECK(  SG_treenode_entry__alloc__copy(pCtx, ptne, pEntry)  );
			SG_ERR_CHECK(SG_STRDUP(pCtx, pszGIDTemp, ppszGID));
			SG_STRING_NULLFREE(pCtx, pPathOfSubchild);
			return;
		}
		else
		{
			SG_uint32 iLenTop;

			SG_treenode * pTreenodeChild = NULL;

			//This is not the node we're looking for.
			//But it might be a parent of the node we're looking for.
			SG_ERR_CHECK(  SG_string__append__sz(pCtx, pPathOfSubchild, "/")  );

			iLenTop = (SG_uint32)strlen(SG_string__sz(pPathOfSubchild));

			// if we've found a parent continue on
			if (0 == strncmp(SG_string__sz(pPathOfSubchild), pszSearchPath, iLenTop))
			{
				const char* pszHidTreeNode = NULL;
				SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, pEntry, &pszHidTreeNode)  );
				SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszHidTreeNode, &pTreenodeChild)  );
				SG_ERR_CHECK(  _sg_treenode__find_treenodeentry_recursive(pCtx, pRepo, pTreenodeChild, pPathOfSubchild, pszSearchPath, ppszGID, ptne)  );
//				SG_NULLFREE(pCtx, pszHidTreeNode);
				SG_TREENODE_NULLFREE(pCtx, pTreenodeChild);
				SG_STRING_NULLFREE(pCtx, pPathOfSubchild);
				return;
			}
		}
	}

fail:
	//We didn't find what we wanted, but it's not really a "Failure"
	//NULL out our output params.
	SG_STRING_NULLFREE(pCtx, pPathOfSubchild);
	ppszGID = NULL;
	ptne = NULL;
	return;
}


void SG_treenode__find_treenodeentry_by_path(SG_context* pCtx, SG_repo* pRepo, SG_treenode * pTreenodeParent, const char* pszSearchPath, char** ppszGID, SG_treenode_entry** ptne)
{
	SG_string * pPathSoFar = NULL;
	SG_string * searchPathToNormalize = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &searchPathToNormalize)  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, searchPathToNormalize, pszSearchPath)  );
	SG_ERR_CHECK(  SG_repopath__normalize(pCtx, searchPathToNormalize, SG_FALSE)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pPathSoFar)  );
	SG_ERR_CHECK(  _sg_treenode__find_treenodeentry_recursive(pCtx, pRepo, pTreenodeParent, pPathSoFar, SG_string__sz(searchPathToNormalize), ppszGID, ptne)  );
	SG_STRING_NULLFREE(pCtx, pPathSoFar);
	SG_STRING_NULLFREE(pCtx, searchPathToNormalize);
	return;
fail:
	SG_STRING_NULLFREE(pCtx, pPathSoFar);
	SG_STRING_NULLFREE(pCtx, searchPathToNormalize);
	return;
}


//////////////////////////////////////////////////////////////////

#ifdef DEBUG
static void _sg_treenode_v1_debug__dump(
	SG_context * pCtx,
	const SG_treenode * pTreenode,
	SG_repo * pRepo,
	int indent,
	SG_bool bRecursive,
	SG_string * pStringResult
	)
{
	// for a version 1 treenode, we only have an array of treenode entries to worry about.
	//
	// walk all entries in the treenode and pretty-print them.
	// we assume that our caller has already pretty-printed a
	// line for us (since the containing directory has the info
	// (name and etc) on us.

	SG_error err;
	SG_uint32 kLimit, k;
	char bufErrorMessage[SG_ERROR_BUFFER_SIZE];
	char bufLine[SG_ERROR_BUFFER_SIZE*2];

	SG_ERR_IGNORE(  SG_treenode__count(pCtx,pTreenode,&kLimit)  );
	for (k=0; k < kLimit; k++)
	{
		const char* szGidObject = NULL;
		const SG_treenode_entry * pTreenodeEntry = NULL;

		SG_treenode__get_nth_treenode_entry__ref(pCtx,pTreenode,k,&szGidObject,&pTreenodeEntry);
		SG_context__get_err(pCtx, &err);
		if (SG_IS_ERROR(err))
		{
			SG_error__get_message(err,bufErrorMessage,SG_NrElements(bufErrorMessage));
			SG_ERR_IGNORE(  SG_sprintf(pCtx,bufLine,SG_NrElements(bufLine),"%*cTreenodeEntry[%d]: Error Fetching Entry: %s\n",
					   indent,' ',k,bufErrorMessage)  );
			SG_ERR_IGNORE(  SG_string__append__sz(pCtx,pStringResult,bufLine)  );
			SG_context__err_reset(pCtx);
		}
		else
		{
			SG_treenode_entry_debug__dump(pCtx, pTreenodeEntry,pRepo,szGidObject,indent,bRecursive,pStringResult);
		}

	}
}

void SG_treenode_debug__dump__by_hid(
	SG_context * pCtx,
	const char* szHidTreenodeBlob,
	SG_repo * pRepo,
	int indent,
	SG_bool bRecursive,
	SG_string * pStringResult
	)
{
	// pretty-print the contents of the treenode identified by the given HID.
	// we append our output to the given string.

	SG_error err;
	SG_treenode * pTreenode = NULL;
	SG_treenode_version ver;
	char bufErrorMessage[SG_ERROR_BUFFER_SIZE];
	char bufLine[SG_ERROR_BUFFER_SIZE*2];

	SG_treenode__load_from_repo(pCtx,pRepo,szHidTreenodeBlob,&pTreenode);
	SG_context__get_err(pCtx, &err);
	if (SG_IS_ERROR(err))
	{
		SG_error__get_message(err,bufErrorMessage,SG_NrElements(bufErrorMessage));
		SG_ERR_IGNORE(  SG_sprintf(pCtx,bufLine,SG_NrElements(bufLine),"%*cTreenode[hid %s]: Error Loading Treenode: %s\n",
				   indent,' ',((char*)szHidTreenodeBlob),bufErrorMessage)  );
		SG_ERR_IGNORE(  SG_string__append__sz(pCtx,pStringResult,bufLine)  );
		return;
	}

	// we already know that the version number is one that we support because
	// the __load_from_repo() will have already validated it.

	SG_ERR_IGNORE(  SG_treenode__get_version(pCtx,pTreenode,&ver)  );
	SG_ERR_IGNORE(  SG_sprintf(pCtx,bufLine,SG_NrElements(bufLine),"%*cTreenode[hid %s]: [ver %d]\n",
			   indent,' ',((char *)szHidTreenodeBlob),(SG_uint32)ver)  );
	SG_ERR_IGNORE(  SG_string__append__sz(pCtx,pStringResult,bufLine)  );

	switch (ver)
	{
	default:
		SG_ERR_IGNORE(  SG_sprintf(pCtx,bufLine,SG_NrElements(bufLine),"%*cTODO Update debug dump to support version %d.\n",
			indent+2,' ',(SG_uint32)ver)  );
		SG_ERR_IGNORE(  SG_string__append__sz(pCtx,pStringResult,bufLine)  );
		break;

	case TN_VERSION_1:
		_sg_treenode_v1_debug__dump(pCtx, pTreenode,pRepo,indent+2,bRecursive,pStringResult);
		break;
	}

	SG_TREENODE_NULLFREE(pCtx, pTreenode);
}
#endif//DEBUG
