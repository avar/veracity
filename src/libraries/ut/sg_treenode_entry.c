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
 * @file sg_treenode_entry.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

/**
 * struct SG_treenode_entry { ...Do not define.... };
 *
 * A SG_treenode_entry represents all of the information about a single
 * versioned directory-entry in a Treenode.  But really, SG_treenode_entry
 * doesn't really exist.  We use the name as an opaque label for our
 * prototypes.  Internally, we use a vhash to store directory-entry
 * attributes.  We don't wrap this with a structure (like SG_treenode)
 * because we cannot put pointers into the containing SG_treenode vhash.
 *
 * The Treenode is the owner of all of each SG_treenode_entry, but we
 * allow the caller to have limited read/write access to it.
 *
 * The information in a Treenode-Entry is converted to JSON as part of
 * the overall serialization of the Treenode.  We do not do any direct
 * serialization here.
 *
 * The following fields are present in each Treenode-Entry.
 */

/**
 * The following keys are present in Version 1 Treenode-Entries (may also be
 * used by future versions).
 */
#define KEY_TYPE			"type"
#define KEY_HID_BLOB		"hidblob"
#define KEY_HID_XATTRS	    "hidxattrs"
#define KEY_ATTRIBUTE_BITS	"bits"
#define KEY_ENTRYNAME		"name"

//////////////////////////////////////////////////////////////////

void SG_treenode_entry__alloc(SG_context * pCtx, SG_treenode_entry ** ppNew)
{
	SG_vhash * pvh = NULL;

	SG_NULLARGCHECK_RETURN(ppNew);

	*ppNew = NULL;

	SG_ERR_CHECK_RETURN(  SG_VHASH__ALLOC(pCtx, &pvh)  );

	*ppNew = (SG_treenode_entry *)pvh;
}

void SG_treenode_entry__alloc__copy(SG_context * pCtx, SG_treenode_entry ** ppNew, const SG_treenode_entry* ptne)
{
	SG_string* pstr = NULL;
	SG_vhash* pvh = NULL;

	SG_NULLARGCHECK_RETURN(ptne);
	SG_NULLARGCHECK_RETURN(ppNew);

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, (const SG_vhash*) ptne, pstr)  );
	SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &pvh, SG_string__sz(pstr))  );

	*ppNew = (SG_treenode_entry*) pvh;

	SG_STRING_NULLFREE(pCtx, pstr);

	return;
fail:
	SG_STRING_NULLFREE(pCtx, pstr);
}

void SG_treenode_entry__free(SG_context * pCtx, SG_treenode_entry * pTreenodeEntry)
{
	SG_vhash* p = (SG_vhash*)pTreenodeEntry;
	SG_VHASH_NULLFREE(pCtx, p);
}

//////////////////////////////////////////////////////////////////

void SG_treenode_entry__set_entry_type(
	SG_context * pCtx,
	SG_treenode_entry * pTreenodeEntry,
	SG_treenode_entry_type tneType
	)
{
	SG_NULLARGCHECK_RETURN(pTreenodeEntry);
	SG_ERR_CHECK_RETURN(  SG_vhash__update__int64(pCtx, (SG_vhash *)pTreenodeEntry,KEY_TYPE,(SG_int64)tneType) );
}

void SG_treenode_entry__get_entry_type(
	SG_context * pCtx,
	const SG_treenode_entry * pTreenodeEntry,
	SG_treenode_entry_type * ptneType
	)
{
	SG_int64 v;

	SG_NULLARGCHECK_RETURN(pTreenodeEntry);
	SG_NULLARGCHECK_RETURN(ptneType);

	// type should ***always*** be present.

	SG_vhash__get__int64(pCtx,(const SG_vhash *)pTreenodeEntry,KEY_TYPE,&v);
	if (!SG_context__has_err(pCtx))
		*ptneType = (SG_treenode_entry_type)v;
	else if (SG_context__err_equals(pCtx, SG_ERR_VHASH_KEYNOTFOUND))
	{
		*ptneType = SG_TREENODEENTRY_TYPE__INVALID;
		SG_context__err_reset(pCtx);
	}
	else
		SG_ERR_RETHROW_RETURN;
}

//////////////////////////////////////////////////////////////////

void SG_treenode_entry__set_hid_blob(
	SG_context * pCtx,
	SG_treenode_entry * pTreenodeEntry,
	const char* pszidHidBlob
	)
{
	SG_NULLARGCHECK_RETURN(pTreenodeEntry);
	SG_ERR_CHECK_RETURN(  SG_vhash__update__string__sz(pCtx, (SG_vhash *)pTreenodeEntry, KEY_HID_BLOB, pszidHidBlob)  );
}

void SG_treenode_entry__get_hid_blob(
	SG_context * pCtx,
	const SG_treenode_entry * pTreenodeEntry,
	const char** ppszidHidBlob
	)
{
	SG_NULLARGCHECK_RETURN(pTreenodeEntry);
	SG_NULLARGCHECK_RETURN(ppszidHidBlob);

	// warning: this can return _OK with a NULL pid when the vhash
	// warning: n/v pair is of type SG_VARIANT_TYPE_NUL.

	SG_ERR_CHECK_RETURN(  SG_vhash__get__sz(pCtx, (const SG_vhash *)pTreenodeEntry, KEY_HID_BLOB, ppszidHidBlob)  );
}

void SG_treenode_entry__set_attribute_bits(
	SG_context * pCtx,
	SG_treenode_entry * pTreenodeEntry,
	SG_int64 bits
	)
{
	SG_NULLARGCHECK_RETURN(pTreenodeEntry);

    if (bits)
    {
        SG_ERR_CHECK_RETURN(  SG_vhash__update__int64(pCtx, (SG_vhash *)pTreenodeEntry,KEY_ATTRIBUTE_BITS,bits)  );
    }
    else
    {
        SG_bool b_has = SG_FALSE;

        SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, (const SG_vhash *)pTreenodeEntry, KEY_ATTRIBUTE_BITS, &b_has)  );
        if (b_has)
        {
            SG_ERR_CHECK_RETURN(  SG_vhash__remove(pCtx, (SG_vhash *)pTreenodeEntry, KEY_ATTRIBUTE_BITS)  );
        }
    }
}

void SG_treenode_entry__get_attribute_bits(
	SG_context * pCtx,
	const SG_treenode_entry * pTreenodeEntry,
	SG_int64* piBits
	)
{
    SG_bool b_has = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pTreenodeEntry);
	SG_NULLARGCHECK_RETURN(piBits);

    SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, (const SG_vhash *)pTreenodeEntry, KEY_ATTRIBUTE_BITS, &b_has)  );

    if (b_has)
    {
        SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, (const SG_vhash*)pTreenodeEntry, KEY_ATTRIBUTE_BITS, piBits)  );
    }
    else
    {
        *piBits = 0;
    }
}

//////////////////////////////////////////////////////////////////

void SG_treenode_entry__set_hid_xattrs(
	SG_context * pCtx,
	SG_treenode_entry * pTreenodeEntry,
	const char* pszidHidBlob
	)
{
	SG_NULLARGCHECK_RETURN(pTreenodeEntry);
    if (pszidHidBlob)
    {
        SG_ERR_CHECK_RETURN(  SG_vhash__update__string__sz(pCtx, (SG_vhash *)pTreenodeEntry, KEY_HID_XATTRS, pszidHidBlob)  );
    }
    else
    {
        SG_bool b_has = SG_FALSE;

        SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, (const SG_vhash *)pTreenodeEntry, KEY_HID_XATTRS, &b_has)  );
        if (b_has)
        {
            SG_ERR_CHECK_RETURN(  SG_vhash__remove(pCtx, (SG_vhash *)pTreenodeEntry, KEY_HID_XATTRS)  );
        }
    }
}

void SG_treenode_entry__get_hid_xattrs(
	SG_context * pCtx,
	const SG_treenode_entry * pTreenodeEntry,
	const char** ppszidHidBlob
	)
{
    SG_bool b_has = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pTreenodeEntry);
	SG_NULLARGCHECK_RETURN(ppszidHidBlob);

    SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, (const SG_vhash *)pTreenodeEntry, KEY_HID_XATTRS, &b_has)  );

    if (b_has)
    {
        // warning: this can return _OK with a NULL pid when the vhash
        // warning: n/v pair is of type SG_VARIANT_TYPE_NUL.

        SG_ERR_CHECK_RETURN(  SG_vhash__get__sz(pCtx, (const SG_vhash *)pTreenodeEntry, KEY_HID_XATTRS, ppszidHidBlob)  );
    }
    else
    {
        *ppszidHidBlob = NULL;
    }
}

//////////////////////////////////////////////////////////////////

void SG_treenode_entry__set_entry_name(
	SG_context * pCtx,
	SG_treenode_entry * pTreenodeEntry,
	const char * szEntryname
	)
{
	SG_NULLARGCHECK_RETURN(pTreenodeEntry);

	SG_NONEMPTYCHECK_RETURN(szEntryname);

	SG_ERR_CHECK_RETURN(  SG_vhash__update__string__sz(pCtx, (SG_vhash *)pTreenodeEntry, KEY_ENTRYNAME, szEntryname)  );
}

void SG_treenode_entry__get_entry_name(
	SG_context * pCtx,
	const SG_treenode_entry * pTreenodeEntry,
	const char ** pszEntryname
	)
{
	SG_NULLARGCHECK_RETURN(pTreenodeEntry);
	SG_NULLARGCHECK_RETURN(pszEntryname);

	SG_ERR_CHECK_RETURN(  SG_vhash__get__sz(pCtx, (const SG_vhash *)pTreenodeEntry, KEY_ENTRYNAME, pszEntryname)  );
}

//////////////////////////////////////////////////////////////////

static SG_bool _sg_treenode_entry__verify_type(SG_treenode_version ver,
											   SG_treenode_entry_type tneType)
{
	// Is the given Entry-Type valid for a Treenode-Entry of this Entry-Version?

	switch (ver)
	{
	case TN_VERSION_1:
		switch (tneType)
		{
		case SG_TREENODEENTRY_TYPE_REGULAR_FILE:
		case SG_TREENODEENTRY_TYPE_DIRECTORY:
		case SG_TREENODEENTRY_TYPE_SYMLINK:
			return SG_TRUE;

		default:
			return SG_FALSE;
		}
	default:
		return SG_FALSE;
	}
}

void SG_treenode_entry__validate__v1(SG_context * pCtx, const SG_treenode_entry * pTreenodeEntry)
{
	// Validate a SG_TREENODE_VERSION_1 version Treenode-Entry.

	SG_treenode_entry_type tneType = SG_TREENODEENTRY_TYPE__INVALID;
	const char* pid = NULL;
	const char * szEntryname = NULL;
    SG_int64 bits = 0;

	// lookup each field and see if the values are sensible.

	SG_ERR_CHECK_RETURN(  SG_treenode_entry__get_entry_type(pCtx, pTreenodeEntry,&tneType)  );
	if (!_sg_treenode_entry__verify_type(TN_VERSION_1,tneType))
		SG_ERR_THROW_RETURN(SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED);

	SG_ERR_CHECK_RETURN(  SG_treenode_entry__get_hid_blob(pCtx, pTreenodeEntry,&pid)  );
	if (!pid)
		SG_ERR_THROW_RETURN(SG_ERR_TREENODE_ENTRY_VALIDATION_FAILED);

	SG_ERR_CHECK_RETURN(  SG_treenode_entry__get_hid_xattrs(pCtx, pTreenodeEntry,&pid)  );

	/* TODO should we load the attributes vhash and validate it here ? */

	SG_ERR_CHECK_RETURN(  SG_treenode_entry__get_attribute_bits(pCtx, pTreenodeEntry,&bits)  );

	SG_ERR_CHECK_RETURN(  SG_treenode_entry__get_entry_name(pCtx, pTreenodeEntry,&szEntryname)  );
}

void SG_treenode_entry__equal(
	SG_context * pCtx,
	const SG_treenode_entry * pTreenodeEntry1,
	const SG_treenode_entry * pTreenodeEntry2,
	SG_bool * pbResult
	)
{
	SG_NULLARGCHECK_RETURN(pTreenodeEntry1);
	SG_NULLARGCHECK_RETURN(pTreenodeEntry2);

	SG_ERR_CHECK_RETURN(  SG_vhash__equal(pCtx, (const SG_vhash *)pTreenodeEntry1, (const SG_vhash *)pTreenodeEntry2, pbResult)  );
}

//////////////////////////////////////////////////////////////////

#if DEBUG
static void _sg_treenode_entry_v1_debug__dump(
	SG_context * pCtx,
	const SG_treenode_entry * pTreenodeEntry,
	SG_repo * pRepo,
	const char * szGidObject,
	int indent,
	SG_bool bRecursive,
	SG_string * pStringResult
	)
{
	// for a version 1 treenode-entry, dump the fields we have.
	SG_treenode_entry_type tneType;
	SG_int64 attrs;
	const char * szHidBlob;
	const char * szHidXAttrs;
	const char * szEntryname;

	char bufLine[SG_ERROR_BUFFER_SIZE*2];
	char bufHexBits[20];

	SG_ERR_IGNORE(  SG_treenode_entry__get_entry_type(pCtx,pTreenodeEntry,&tneType)  );
	SG_ERR_IGNORE(  SG_treenode_entry__get_hid_blob(pCtx,pTreenodeEntry,&szHidBlob)  );
	SG_ERR_IGNORE(  SG_treenode_entry__get_hid_xattrs(pCtx,pTreenodeEntry,&szHidXAttrs)  );
	SG_ERR_IGNORE(  SG_treenode_entry__get_attribute_bits(pCtx,pTreenodeEntry,&attrs)  );
	SG_ERR_IGNORE(  SG_treenode_entry__get_entry_name(pCtx,pTreenodeEntry,&szEntryname)  );

	SG_hex__format_uint64(bufHexBits,(SG_uint64)attrs);

	SG_ERR_IGNORE(  SG_sprintf(pCtx,bufLine,SG_NrElements(bufLine),
		"%*cTNE[gid %s]: [type %d][name %s][hid %s][hidXAtrrs %s][bits %s]\n",
		indent,' ',
		szGidObject,
		(SG_uint32)tneType,
		szEntryname,
		szHidBlob,
		((szHidXAttrs) ? szHidXAttrs : ""),
		bufHexBits)  );
	SG_ERR_IGNORE(  SG_string__append__sz(pCtx,pStringResult,bufLine)  );

	if (bRecursive && (tneType == SG_TREENODEENTRY_TYPE_DIRECTORY))
	{
		SG_treenode_debug__dump__by_hid(pCtx, szHidBlob,pRepo,indent+2,bRecursive,pStringResult);
	}
}

void SG_treenode_entry_debug__dump(
	SG_context * pCtx,
	const SG_treenode_entry * pTreenodeEntry,
	SG_repo * pRepo,
	const char * szGidObject,
	int indent,
	SG_bool bRecursive,
	SG_string * pStringResult
	)
{
	// pretty-print the contents of the treenode-entry and append to
	// the given string.

	if (!pTreenodeEntry || !pRepo || !szGidObject || !pStringResult)
		return;

    _sg_treenode_entry_v1_debug__dump(pCtx,pTreenodeEntry,pRepo,szGidObject,indent,bRecursive,pStringResult);
}

void SG_treenode_entry_debug__dump_to_console(SG_context * pCtx,
											  const SG_treenode_entry * pTreenodeEntry,
											  SG_repo * pRepo,
											  const char * szGidObject,
											  int indent,
											  SG_bool bRecursive)
{
	SG_string * pString = NULL;

	SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC(pCtx,&pString)  );
	SG_ERR_CHECK(  SG_treenode_entry_debug__dump(pCtx,pTreenodeEntry,pRepo,szGidObject,indent,bRecursive,pString)  );
	SG_ERR_CHECK(  SG_console(pCtx,SG_CS_STDERR,"%s",SG_string__sz(pString))  );

fail:
	SG_STRING_NULLFREE(pCtx,pString);
}

#endif

//////////////////////////////////////////////////////////////////
