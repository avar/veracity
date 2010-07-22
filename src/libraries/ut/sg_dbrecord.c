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

#include <sg.h>

struct SG_dbrecord
{
	char* pid;
	SG_vhash* pvh;
};

void SG_dbrecord__alloc(SG_context* pCtx, SG_dbrecord** ppResult)
{
	SG_dbrecord * prec;
	SG_NULLARGCHECK_RETURN(ppResult);
	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, prec)  );
	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &prec->pvh)  );
	*ppResult = prec;

	return;
fail:
	SG_NULLFREE(pCtx, prec);
}

static void _sg_dbrecord__validate_vhash(SG_context* pCtx, const SG_dbrecord* prec)
{
	SG_uint32 count;
	SG_uint32 i;

	SG_ERR_CHECK(  SG_vhash__count(pCtx, prec->pvh, &count)  );

	for (i=0; i<count; i++)
	{
		const char* pszKey = NULL;
		const SG_variant* pv = NULL;

		SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, prec->pvh, i, &pszKey, &pv)  );

		if (!pszKey || !pv  || pv->type != SG_VARIANT_TYPE_SZ)
		{
			SG_ERR_THROW_RETURN(SG_ERR_DBRECORD_VALIDATION_FAILED);
		}
	}

	return;

fail:
    return;
}

static void _sg_dbrecord__alloc_e__from_json(SG_context* pCtx, SG_dbrecord ** ppNew, const char * szString)
{
	SG_dbrecord * prec;

	SG_NULLARGCHECK_RETURN(ppNew);
	SG_NONEMPTYCHECK_RETURN(szString);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, prec)  );
	SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &prec->pvh, szString)  );
	SG_ERR_CHECK(  _sg_dbrecord__validate_vhash(pCtx, prec)  );
	*ppNew = prec;

	return;
fail:
	SG_DBRECORD_NULLFREE(pCtx, prec);
}

void SG_dbrecord__get_hid__ref(SG_context* pCtx, const SG_dbrecord* prec, const char** ppid)
{
	SG_NULLARGCHECK_RETURN(prec);
	SG_NULLARGCHECK_RETURN(ppid);

	/* it is legal to call this when the item is not frozen */

	*ppid = prec->pid;
}

static void _sg_dbrecord__freeze(
	SG_context * pCtx,
	SG_dbrecord* prec,
	char* pid       /**< The dbrecord now owns this SG_id pointer */
	)
{
	SG_NULLARGCHECK_RETURN(prec);
	SG_NULLARGCHECK_RETURN(pid);

	if (prec->pid)
		SG_NULLFREE(pCtx, prec->pid);

	prec->pid = pid;
}

void SG_dbrecord__freeze(SG_context* pCtx, SG_dbrecord* prec, SG_repo * pRepo, const char** ppid)
{
	SG_string* pstr = NULL;
	char* psz_hid = NULL;

	SG_NULLARGCHECK_RETURN(prec);

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
	SG_ERR_CHECK(  SG_dbrecord__to_json(pCtx, prec, pstr)  );
	SG_ERR_CHECK(  SG_repo__alloc_compute_hash__from_bytes(pCtx,pRepo,
														   SG_string__length_in_bytes(pstr),
														   (SG_byte *)SG_string__sz(pstr),
														   &psz_hid)  );
	SG_STRING_NULLFREE(pCtx, pstr);
	_sg_dbrecord__freeze(pCtx, prec, psz_hid);
	*ppid = psz_hid;

	return;
fail:
	SG_STRING_NULLFREE(pCtx, pstr);
}


/**
 * Write a vhash table in JSON format.  This function handles all the details of
 * setting up the SG_jsonwriter and places the output stream in the given string.
 */
void SG_dbrecord__to_json(SG_context* pCtx, SG_dbrecord* prec, SG_string* pStr)
{
	SG_NULLARGCHECK_RETURN(prec);
	SG_ERR_CHECK_RETURN(  SG_vhash__sort(pCtx, prec->pvh, SG_FALSE, SG_vhash_sort_callback__increasing)  );
	SG_ERR_CHECK_RETURN(  SG_vhash__to_json(pCtx, prec->pvh, pStr)  );
}

/**
 *	We allocate the vhash pointer; caller owns thereafter
 */
void SG_dbrecord__to_vhash(SG_context* pCtx, SG_dbrecord* prec, SG_vhash** ppVh)
{
	SG_NULLARGCHECK_RETURN(prec);
	SG_NULLARGCHECK_RETURN(ppVh);

	SG_ERR_CHECK_RETURN(  SG_vhash__sort(pCtx, prec->pvh, SG_FALSE, SG_vhash_sort_callback__increasing)  );
	SG_ERR_CHECK(  SG_VHASH__ALLOC__COPY(pCtx, ppVh, prec->pvh)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, *ppVh);
}

void SG_dbrecord__free(SG_context * pCtx, SG_dbrecord* prec)
{
	if (!prec)
		return;

	SG_VHASH_NULLFREE(pCtx, prec->pvh);
	SG_NULLFREE(pCtx, prec->pid);

	SG_NULLFREE(pCtx, prec);
}

void SG_dbrecord__count_pairs(SG_context* pCtx, const SG_dbrecord* prec, SG_uint32* piResult)
{
	SG_NULLARGCHECK_RETURN(prec);
	SG_NULLARGCHECK_RETURN(piResult);
	SG_ERR_CHECK_RETURN(  SG_vhash__count(pCtx, prec->pvh, piResult)  );
}

void SG_dbrecord__add_pair__int64(SG_context* pCtx, SG_dbrecord* prec, const char* putf8Name, SG_int64 intValue)
{
	SG_int_to_string_buffer buf;
	SG_int64_to_sz(intValue,buf);
	SG_ERR_CHECK_RETURN(  SG_dbrecord__add_pair(pCtx, prec, putf8Name, buf)  );
}

void SG_dbrecord__add_pair(SG_context* pCtx, SG_dbrecord* prec, const char* putf8Name, const char* putf8Value)
{
	if (prec->pid)
    {
		SG_ERR_THROW_RETURN(SG_ERR_INVALID_WHILE_FROZEN);
    }
	SG_ERR_CHECK_RETURN(  SG_vhash__add__string__sz(pCtx, prec->pvh, putf8Name, putf8Value)  );
}

void SG_dbrecord__get_nth_pair(SG_context* pCtx, const SG_dbrecord* prec, SG_uint32 ndx, const char** pputf8Name, const char** pputf8Value)
{
	const char* pk_sz;
	const SG_variant* pv;
	const char* pv_sz;

	SG_NULLARGCHECK_RETURN(prec);
	SG_NULLARGCHECK_RETURN(pputf8Name);
	SG_NULLARGCHECK_RETURN(pputf8Value);

	SG_ERR_CHECK_RETURN(  SG_vhash__get_nth_pair(pCtx, prec->pvh, ndx, &pk_sz, &pv)  );

	SG_ERR_CHECK_RETURN(  SG_variant__get__sz(pCtx, pv, &pv_sz)  );

	*pputf8Name = pk_sz;
	*pputf8Value = pv_sz;
}

void SG_dbrecord__check_value(SG_context* pCtx, const SG_dbrecord* prec, const char* putf8Name, SG_bool* pb, const char** pputf8Value)
{
	SG_NULLARGCHECK_RETURN(prec);
	SG_NULLARGCHECK_RETURN(putf8Name);
	SG_NULLARGCHECK_RETURN(pb);
	SG_NULLARGCHECK_RETURN(pputf8Value);
	SG_ERR_CHECK_RETURN(  SG_vhash__check__sz(pCtx, prec->pvh, putf8Name, pb, pputf8Value)  );
}

void SG_dbrecord__get_value(SG_context* pCtx, const SG_dbrecord* prec, const char* putf8Name, const char** pputf8Value)
{
	SG_NULLARGCHECK_RETURN(prec);
	SG_NULLARGCHECK_RETURN(putf8Name);
	SG_NULLARGCHECK_RETURN(pputf8Value);
	SG_ERR_CHECK_RETURN(  SG_vhash__get__sz(pCtx, prec->pvh, putf8Name, pputf8Value)  );
}

void SG_dbrecord__get_value__int64(SG_context* pCtx, const SG_dbrecord* prec, const char* psz_name, SG_int64* piValue)
{
    const char* psz_value = NULL;
    SG_int64 iv = 0;
    SG_ERR_CHECK_RETURN(  SG_dbrecord__get_value(pCtx, prec, psz_name, &psz_value)  );
    SG_ERR_CHECK_RETURN(  SG_int64__parse__strict(pCtx, &iv, psz_value)  );
    *piValue = iv;
}

void SG_dbrecord__has_value(SG_context* pCtx, const SG_dbrecord* prec, const char* putf8Name, SG_bool* pb)
{
	SG_bool b = SG_FALSE;
	SG_NULLARGCHECK_RETURN(prec);
	SG_NULLARGCHECK_RETURN(putf8Name);
	SG_NULLARGCHECK_RETURN(pb);
	SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, prec->pvh, putf8Name, &b)  );
	*pb = b;
}

void SG_dbrecord__save_to_repo(SG_context* pCtx, SG_dbrecord * pRecord, SG_repo * pRepo, SG_repo_tx_handle* pRepoTx, SG_uint64* iBlobFullLength)
{
	SG_string * pString = NULL;
	char* pszHidComputed = NULL;
    SG_uint32 iLength = 0;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pRecord);

	// serialize the dbrecord into JSON string.

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );
	SG_ERR_CHECK(  SG_dbrecord__to_json(pCtx, pRecord,pString)  );

#if 0
    printf("%s\n", SG_string__sz(pString)); // TODO remove this
#endif

	// remember the length of the blob
    iLength = SG_string__length_in_bytes(pString);
	*iBlobFullLength = (SG_uint64) iLength;

	// create a blob in the repository using the JSON string.  this computes the HID and returns it.

	SG_repo__store_blob_from_memory(pCtx,
		pRepo,pRepoTx,
        NULL, // TODO pass the record id into here
        SG_FALSE,
		(const SG_byte *)SG_string__sz(pString),
		iLength,
		&pszHidComputed);
	if (!SG_context__has_err(pCtx) || SG_context__err_equals(pCtx, SG_ERR_BLOBFILEALREADYEXISTS))
	{
		// freeeze the dbrecord memory-object and effectively make it read-only
		// from this point forward.  we give up our ownership of the HID.

		SG_ASSERT(  pszHidComputed  );
		_sg_dbrecord__freeze(pCtx, pRecord, pszHidComputed);
		pszHidComputed = NULL;

		SG_STRING_NULLFREE(pCtx, pString);
		return;  // return _OK or __BLOBFILEALREADYEXISTS
	}

fail:
	SG_STRING_NULLFREE(pCtx, pString);
	SG_NULLFREE(pCtx, pszHidComputed);
}

void SG_dbrecord__load_from_repo(SG_context* pCtx, SG_repo * pRepo, const char* pszidHidBlob, SG_dbrecord ** ppResult)
{
	// fetch contents of a dbrecord-type blob and convert to a dbrecord object.

	SG_byte * pbuf = NULL;
	SG_dbrecord * pRecord = NULL;
	char* pszidHidCopy = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pszidHidBlob);
	SG_NULLARGCHECK_RETURN(ppResult);

	*ppResult = NULL;

	// fetch the Blob for the given HID and convert from a JSON stream into an
	// allocated dbrecord object.

	SG_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pRepo, pszidHidBlob, &pbuf, NULL)  );
	SG_ERR_CHECK(  _sg_dbrecord__alloc_e__from_json(pCtx, &pRecord, (const char *)pbuf)  );
	SG_NULLFREE(pCtx, pbuf);

	// make a copy of the HID so that we can stuff it into the dbrecord and freeze it.
	// we freeze it because the caller should not be able to modify the dbrecord by
	// accident -- since we are a representation of something on disk already.

	SG_ERR_CHECK(  SG_strdup(pCtx, pszidHidBlob, &pszidHidCopy)  );
	(void)_sg_dbrecord__freeze(pCtx, pRecord, pszidHidCopy);

	*ppResult = pRecord;

	return;
fail:
	SG_NULLFREE(pCtx, pbuf);
	SG_DBRECORD_NULLFREE(pCtx, pRecord);
	SG_NULLFREE(pCtx, pszidHidCopy);
}
