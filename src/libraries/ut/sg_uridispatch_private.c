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
 * @file sg_uridispatch_private.c
 */

#include <sg.h>
#include <ctype.h>
#include "sg_uridispatch_private_typedefs.h"
#include "sg_uridispatch_private_prototypes.h"
#include "sg_zing__private.h"

_response_handle * const MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE = (_response_handle*)(void*)&MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE;
_response_handle * const GENERIC_BAILOUT_RESPONSE_HANDLE = (_response_handle*)(void*)&GENERIC_BAILOUT_RESPONSE_HANDLE;
static char *_hostname = NULL;

static void _new_wit_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement);

static void _dispatch__zing_dag_specific(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle);

static void _GET__changeset_stamp_json(  SG_context * pCtx,
	SG_repo* pRepo,
	char * pChangesetHid,
    _response_handle ** ppResponseHandle);

static void _GET__changeset_comments_json(
    SG_context * pCtx,
	SG_repo* pRepo,
	const char * pChangesetHid,
    _response_handle ** ppResponseHandle);

static void _GET__changeset_tags_json(  SG_context * pCtx,
	SG_repo* pRepo,
	char * pChangesetHid,
    _response_handle ** ppResponseHandle);


void _getHostName(
	SG_context *pCtx,
	const char **ppHostname)
{
	if (_hostname == NULL)
	{
		SG_ERR_CHECK_RETURN(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__SERVER_HOSTNAME,
			NULL, &_hostname, NULL)  );

		if ((_hostname == NULL) || eq(_hostname, ""))
			_hostname = "localhost";
	}

	*ppHostname = (const char *)_hostname;
}


//////////////////////////////////////////////////////////////////


void _request_handle__alloc(SG_context * pCtx, _request_handle ** ppNew,
    const _request_headers * pRequestHeaders,
    _request_chunk_cb  * pChunk, _request_finish_cb * pFinish, _request_aborted_cb * pOnAfterAborted,
    void * pContext)
{
    _request_handle * pNew = NULL;

    SG_NULLARGCHECK_RETURN(ppNew);
    SG_ASSERT(*ppNew==NULL);
    SG_ARGCHECK_RETURN(pRequestHeaders->contentLength>0, pRequestHeaders->contentLength); // A _request_handle is for chunking content!
    SG_NULLARGCHECK_RETURN(pChunk);
    SG_NULLARGCHECK_RETURN(pFinish);
    SG_NULLARGCHECK_RETURN(pOnAfterAborted);

    SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pNew)  );
    pNew->pCtx = pCtx;
	if (pRequestHeaders->pFrom)
    {
        // TODO is there a pRepo to pass to the following SG_audit__init call?
        SG_ERR_CHECK(  SG_audit__init(pCtx, &pNew->audit, NULL, SG_AUDIT__WHEN__NOW, pRequestHeaders->pFrom)  );
    }

    pNew->contentLength = pRequestHeaders->contentLength;
    pNew->pChunk = pChunk;
    pNew->pFinish = pFinish;
    pNew->pOnAfterAborted = pOnAfterAborted;
    pNew->pContext = pContext;
    pNew->processedLength = 0;

    *ppNew = pNew;

    return;
fail:
    SG_NULLFREE(pCtx, pNew);
}
void _request_handle__nullfree(_request_handle ** ppThis)
{
    if(ppThis==NULL || *ppThis==NULL)
        return;
    SG_free__no_ctx(*ppThis);
    *ppThis = NULL;
}
void _request_handle__finish(SG_context * pCtx, _request_handle * pRequestHandle, _response_handle ** ppResponseHandle)
{
    SG_NULLARGCHECK_RETURN(pRequestHandle);
    SG_ARGCHECK_RETURN(pRequestHandle->pFinish!=NULL, pRequestHandle);
    pRequestHandle->pFinish(pCtx, &pRequestHandle->audit, pRequestHandle->pContext, ppResponseHandle);
}
void _request_handle__on_after_aborted(SG_context * pCtx, _request_handle * pRequestHandle)
{
    SG_NULLARGCHECK_RETURN(pRequestHandle);
    SG_ARGCHECK_RETURN(pRequestHandle->pOnAfterAborted!=NULL, pRequestHandle);
    pRequestHandle->pOnAfterAborted(pCtx, pRequestHandle->pContext);
}

void _response_handle__alloc(SG_context * pCtx, _response_handle ** ppNew,
    const char * pHttpStatusCode, const char * pContentType, SG_uint64 contentLength,
    _response_chunk_cb  * pChunk, _response_finished_cb * pOnAfterFinished, _response_finished_cb * pOnAfterAborted,
    void * pContext)
{
    SG_NULLARGCHECK_RETURN(ppNew);
    SG_NULLARGCHECK_RETURN(pHttpStatusCode);
    //SG_ARGCHECK_RETURN(pContentType!=NULL || contentLength==0, pContentType);
    SG_ARGCHECK_RETURN(pChunk!=NULL || contentLength==0, pChunk);

    SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, *ppNew)  );
    (*ppNew)->pCtx = pCtx;
    (*ppNew)->pHttpStatusCode = pHttpStatusCode;
	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &(*ppNew)->pHeaders)  );

	(*ppNew)->contentLength = contentLength;

	if (pContentType)
	{
		SG_ERR_CHECK(  _response_handle__add_header(pCtx, *ppNew, "Content-Type", pContentType)  );
	}

    (*ppNew)->pChunk = pChunk;
    (*ppNew)->pOnAfterFinished = pOnAfterFinished;
    (*ppNew)->pOnAfterAborted = pOnAfterAborted;
    (*ppNew)->pContext = pContext;
    (*ppNew)->processedLength = 0;

fail:
	;
}
void _response_handle__nullfree(SG_context *pCtx, _response_handle ** ppThis)
{
    if(ppThis==NULL || *ppThis==NULL)
        return;

	SG_ERR_IGNORE(  SG_VHASH_NULLFREE(pCtx, (*ppThis)->pHeaders)  );
    SG_free__no_ctx(*ppThis);
    *ppThis = NULL;
}

void _response_handle__add_header(SG_context * pCtx, _response_handle *pResponseHandle, const char *headerName, const char *headerValue)
{
	SG_NULLARGCHECK_RETURN(pResponseHandle);
	SG_NULLARGCHECK_RETURN(headerName);

	if (headerValue == NULL)
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__remove(pCtx, pResponseHandle->pHeaders, headerName)  );
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_vhash__add__string__sz(pCtx, pResponseHandle->pHeaders, headerName, headerValue)  );
	}
}

void _response_handle__on_after_aborted(SG_context * pCtx, _response_handle * pResponseHandle)
{
    if(pResponseHandle!=NULL
        && pResponseHandle!=MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE
        && pResponseHandle!=GENERIC_BAILOUT_RESPONSE_HANDLE
        && pResponseHandle->pOnAfterAborted!=NULL)
    {
        pResponseHandle->pOnAfterAborted(pCtx, pResponseHandle->pContext);
    }
}
void _response_handle__on_after_finished(SG_context * pCtx, _response_handle * pResponseHandle)
{
    if(pResponseHandle!=NULL
        && pResponseHandle!=MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE
        && pResponseHandle!=GENERIC_BAILOUT_RESPONSE_HANDLE
        && pResponseHandle->pOnAfterFinished!=NULL)
    {
        pResponseHandle->pOnAfterFinished(pCtx, pResponseHandle->pContext);
    }
}


//////////////////////////////////////////////////////////////////

void _chunk_request_to_string(SG_context * pCtx, _response_handle ** ppResponseHandle, SG_byte * pBuffer, SG_uint32 bufferLength, void * pContext)
{
    SG_string * pRequestBody = (SG_string*)pContext;

    SG_ASSERT(pCtx!=NULL);
    SG_UNUSED(ppResponseHandle);
    SG_NULLARGCHECK_RETURN(pBuffer);
    SG_ARGCHECK_RETURN(bufferLength>0, bufferLength);
    SG_NULLARGCHECK_RETURN(pContext);

    SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, pRequestBody, pBuffer, bufferLength)  );
}

void _free_string(SG_context * pCtx, void * pContext)
{
    SG_ASSERT(pCtx!=NULL);
    SG_STRING_NULLFREE(pCtx, pContext);
}

const char * _response_header_for(SG_contenttype contentType)
{
    switch(contentType)
    {
    case SG_contenttype__json:
        return "application/json;charset=UTF-8";
    case SG_contenttype__text:
        return "text/plain;charset=UTF-8";
    case SG_contenttype__xhtml:
        return "application/xhtml+xml;charset=UTF-8";
    case SG_contenttype__xml:
        return "application/atom+xml;charset=UTF-8";
    case SG_contenttype__html:
        return "text/html;charset=UTF-8";
	case SG_contenttype__fragball:
		return "application/fragball";
    default:
        return NULL;
    }
}

void _chunk_response_from_string(SG_context * pCtx, SG_uint64 processedLength, SG_byte * pBuffer, SG_uint32 bufferLength, void * pContext)
{
    SG_UNUSED(pCtx);
    SG_NULLARGCHECK_RETURN(pContext);
    memcpy(pBuffer, SG_string__sz((SG_string*)pContext)+processedLength, bufferLength);
}
void _create_response_handle_for_string(
    SG_context * pCtx,
    const char * pHttpStatusCode,
    SG_contenttype contentType,
    SG_string ** ppString, // On success we've taken ownership and nulled the caller's copy.
    _response_handle ** ppResponseHandle)
{
    const char * pContentType = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NONEMPTYCHECK_RETURN(pHttpStatusCode);
    pContentType = _response_header_for(contentType);
    SG_ARGCHECK_RETURN(pContentType!=NULL, contentType);
    SG_NULL_PP_CHECK_RETURN(ppString);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);

    SG_ERR_CHECK_RETURN(  _response_handle__alloc(pCtx, ppResponseHandle,
        pHttpStatusCode, pContentType, SG_string__length_in_bytes(*ppString),
        _chunk_response_from_string, _free_string, _free_string,
        *ppString)  );
    *ppString = NULL;
}

static void  _addPendingLinks(
	SG_context *pCtx,
	SG_zingtx *pztx,
	SG_zingrecord *pRec,
	SG_vhash *linksToSet)
{
	SG_uint32 count = 0;
	SG_uint32 i = 0;
	SG_zinglinkattributes *pzla = NULL;
	SG_zinglinksideattributes *pzlsaMine = NULL;
	SG_zinglinksideattributes *pzlsaOther = NULL;
	SG_zingfieldattributes * pzfa = NULL;

	SG_ERR_CHECK(  SG_vhash__count(pCtx, linksToSet, &count)  );

	for ( i = 0; i < count; ++i )
	{
		const char *linkname = NULL;
		const char *recid = NULL;
		const SG_variant *vval = NULL;
		SG_zingrecord *newrec = NULL;

		SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, linksToSet, i, &linkname, &vval)  );
		recid = vval->v.val_sz;

		SG_ERR_CHECK(  SG_zingrecord__lookup_name(pCtx, pRec, linkname, &pzfa, &pzla, &pzlsaMine, &pzlsaOther)  );
		SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, recid, &newrec)  );

	    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaOther);
		SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);

		if (pzlsaMine->bSingular)
			SG_ERR_CHECK(  SG_zingrecord__set_singular_link_and_overwrite_other_if_singular(pCtx, pRec, linkname, recid)  );
		else
			SG_ERR_CHECK(  SG_zingrecord__add_link_and_overwrite_other_if_singular(pCtx, pRec, linkname, recid)  );

	    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaMine);
	}

fail:
	SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaOther);
	SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
	SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaMine);
}

static void _setFieldFromJson(
	SG_context *pCtx,
	SG_zingtemplate *pTemplate,
	const char *rectype,
	SG_zingrecord *pRec,
	const char *pKey,
	const SG_variant *pValue,
	SG_vhash *linksToSet)
{
    SG_zingfieldattributes * pFieldAttributes = NULL;
	SG_uint16 ft = SG_ZING_TYPE__UNKNOWN;
	SG_bool matched = SG_FALSE;
	SG_zinglinksideattributes *pzlsaMine = NULL;
	SG_zinglinksideattributes *pzlsaOther = NULL;
	SG_zinglinkattributes *pzla = NULL;
	SG_bool isLink = SG_FALSE;

	if (eq(pKey, "rectype") || eq(pKey, "recid"))
	{
		return;
	}

	SG_ERR_CHECK(  SG_zingrecord__lookup_name(pCtx, pRec, pKey, &pFieldAttributes, &pzla, &pzlsaMine, &pzlsaOther)  );

	isLink = (pzla && pzlsaOther);

	SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaOther);
	SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
	SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaMine);

	if (isLink)
	{
		if (linksToSet)
		{
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, linksToSet, pKey, pValue->v.val_sz)  );
		}
		return;
	}

	SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pTemplate, rectype, pKey, &pFieldAttributes)  );
	SG_ERR_CHECK(  sg_zingfieldattributes__get_type(pCtx, pFieldAttributes, &ft)  );

	switch (ft)
	{
		case SG_ZING_TYPE__INT:
			if (pValue->type == SG_VARIANT_TYPE_INT64)
			{
				SG_ERR_CHECK(  SG_zingrecord__set_field__int(pCtx, pRec, pFieldAttributes, pValue->v.val_int64)  );
				matched = SG_TRUE;
			}
			else if (pValue->type == SG_VARIANT_TYPE_SZ)
			{
				SG_int64 v = 0;
				SG_ERR_CHECK(  SG_int64__parse(pCtx, &v, pValue->v.val_sz)  );
				SG_ERR_CHECK(  SG_zingrecord__set_field__int(pCtx, pRec, pFieldAttributes, v)  );
				matched = SG_TRUE;
			}
			break;

		case SG_ZING_TYPE__STRING:
			if (pValue->type == SG_VARIANT_TYPE_SZ)
			{
				SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, pRec, pFieldAttributes, pValue->v.val_sz)  );
				matched = SG_TRUE;
			}
			break;

		case SG_ZING_TYPE__BOOL:
			if (pValue->type == SG_VARIANT_TYPE_BOOL)
			{
				SG_ERR_CHECK(  SG_zingrecord__set_field__bool(pCtx, pRec, pFieldAttributes, pValue->v.val_bool)  );
				matched = SG_TRUE;
			}
			else if (pValue->type == SG_VARIANT_TYPE_SZ)
			{
				SG_bool	v = SG_parse_bool(pValue->v.val_sz);
				SG_ERR_CHECK(  SG_zingrecord__set_field__bool(pCtx, pRec, pFieldAttributes, v)  );
				matched = SG_TRUE;
			}
			break;

// #define SG_ZING_TYPE__ATTACHMENT    4

		case SG_ZING_TYPE__DAGNODE:
		    if(pValue->type==SG_VARIANT_TYPE_SZ)
			{
				SG_ERR_CHECK(  SG_zingrecord__set_field__dagnode(pCtx, pRec, pFieldAttributes, pValue->v.val_sz)  );
				matched = SG_TRUE;
			}
			break;

		case SG_ZING_TYPE__DATETIME:
			if(pValue->type==SG_VARIANT_TYPE_INT64)
			{
				SG_ERR_CHECK(  SG_zingrecord__set_field__datetime(pCtx, pRec, pFieldAttributes, pValue->v.val_int64)  );
				matched = SG_TRUE;
			}
			else if (pValue->type == SG_VARIANT_TYPE_SZ)
			{
				SG_int64 v = 0;
				SG_ERR_CHECK(  SG_int64__parse(pCtx, &v, pValue->v.val_sz)  );
				SG_ERR_CHECK(  SG_zingrecord__set_field__datetime(pCtx, pRec, pFieldAttributes, pValue->v.val_int64)  );
				matched = SG_TRUE;
			}
			break;

		case SG_ZING_TYPE__USERID:
			if (pValue->type == SG_VARIANT_TYPE_SZ)
			{
				SG_ERR_CHECK(  SG_zingrecord__set_field__userid(pCtx, pRec, pFieldAttributes, pValue->v.val_sz)  );
				matched = SG_TRUE;
			}
			break;

		case SG_ZING_TYPE__UNKNOWN:
			if (pValue->type == SG_VARIANT_TYPE_SZ)
			{
				SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, pRec, pFieldAttributes, pValue->v.val_sz)  );
				matched = SG_TRUE;
			}
			break;
	}

	if (! matched)
        SG_ERR_THROW(SG_ERR_ZING_TYPE_MISMATCH);

fail:
	SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaOther);
	SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
	SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaMine);
}

static void _createRecordFromJson(
	SG_context *pCtx, SG_zingtx *pztx, const char *rectype, SG_string *json, SG_zingrecord **ppRec, const char **ppRecid, SG_vhash *linksToSet )
{
    SG_zingtemplate * pTemplate = NULL;
	SG_vhash *pVhash = NULL;
	SG_uint32 count = 0;
	SG_uint32 i;

    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pTemplate)  );
    SG_ERR_CHECK(  SG_zingtx__create_new_record(pCtx, pztx, rectype, ppRec)  );

    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, *ppRec, ppRecid)  );

    SG_VHASH__ALLOC__FROM_JSON(pCtx, &pVhash, SG_string__sz(json));
    if(SG_context__err_equals(pCtx, SG_ERR_JSONPARSER_SYNTAX))
        SG_ERR_RESET_THROW2(SG_ERR_URI_HTTP_400_BAD_REQUEST, (pCtx, "Request was not valid JSON."));
    else if (SG_context__has_err(pCtx))
        SG_ERR_RETHROW;

    SG_ERR_CHECK(  SG_vhash__count(pCtx, pVhash, &count)  );
    for(i=0;i<count;++i)
    {
        const char * pKey = NULL;
        const SG_variant * pValue = NULL;

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pVhash, i, &pKey, &pValue)  );

		SG_ERR_CHECK(  _setFieldFromJson(pCtx, pTemplate, rectype, *ppRec, pKey, pValue, linksToSet )  );

    }

fail:
    SG_VHASH_NULLFREE(pCtx, pVhash);
}


// Function defined in sg_uridispatch_ui.c:
void _read_template_file(
	SG_context *pCtx,
	const char *templateFn,
	SG_string **pContent,	/**< we allocate, we free on error, else caller owns it */
	const _request_headers *pRequestHeaders,
	_replacer_cb replacer);

static SG_contenttype selectHtmlType(
	const char *ua)
{
	SG_contenttype ct = SG_contenttype__xhtml;
	const char *msie = NULL;
	const char *sep = NULL;

	if (ua == NULL)
		return(ct);

	msie = strstr(ua, "MSIE");

	if (msie != NULL)
	{
		SG_uint32	num = 0;

		sep = msie + 4;

		if ((*sep != ' ') && (*sep != '\t'))
			return(ct);

		while ((*sep == ' ') || (*sep == '\t'))
			++sep;

		while ((*sep >= '0') && (*sep <= '9'))
			num = (num * 10) + (SG_uint32)(*sep++ - '0');

		if (num < 9)
			ct = SG_contenttype__html;
	}

	return(ct);
}

void _create_response_handle_for_template(
    SG_context * pCtx,
	const _request_headers *pRequestHeaders,
    const char * pHttpStatusCode,
    const char * pPage,
    _replacer_cb * pReplacer,
    _response_handle ** ppResponseHandle)
{
    SG_string * pContent = NULL;

    SG_ERR_CHECK(  _read_template_file(pCtx, pPage, &pContent, pRequestHeaders, pReplacer)  );
    SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, pHttpStatusCode, selectHtmlType(pRequestHeaders->pUserAgent), &pContent, ppResponseHandle)  );

    return;
fail:
    SG_STRING_NULLFREE(pCtx, pContent);
}

// -- _create_response_handle_for_blob -- //

    typedef struct
    {
        SG_repo * pRepo;
        SG_repo_fetch_blob_handle * pFetchBlobHandle;
    }_generic_blob_response__context;

    static void _generic_blob_response__context__alloc(
        SG_context * pCtx,
        _generic_blob_response__context ** ppNew,
        SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
        SG_repo_fetch_blob_handle ** ppFetchBlobHandle) // On success we've taken ownership and nulled the caller's copy.
    {
        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(ppNew);
        SG_ASSERT(*ppNew==NULL);
        SG_NULL_PP_CHECK_RETURN(ppRepo);
        SG_NULL_PP_CHECK_RETURN(ppFetchBlobHandle);

        SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, *ppNew)  );

        (*ppNew)->pRepo = *ppRepo;
        *ppRepo = NULL;
        (*ppNew)->pFetchBlobHandle = *ppFetchBlobHandle;
        *ppFetchBlobHandle = NULL;
    }

    static void _generic_blob_response__context__free(
        SG_context * pCtx,
        _generic_blob_response__context * p)
    {
        if(p==NULL)
            return;
        if(p->pFetchBlobHandle!=NULL)
            SG_ERR_IGNORE(  SG_repo__fetch_blob__abort(pCtx, p->pRepo, &p->pFetchBlobHandle)  );
        SG_REPO_NULLFREE(pCtx,p->pRepo);
        SG_NULLFREE(pCtx,p);
    }
#define _GENERIC_BLOB_RESPONSE__CONTEXT__NULLFREE(pCtx,p) SG_STATEMENT(SG_context__push_level(pCtx); _generic_blob_response__context__free(pCtx,p); SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL;)

    static void _generic_blob_response__chunk(SG_context * pCtx, SG_uint64 processedLength, SG_byte * pBuffer, SG_uint32 bufferLength, void * pContext__voidp)
    {
        _generic_blob_response__context * pContext = (_generic_blob_response__context*)pContext__voidp;

        SG_uint32 total_length_got = 0;
        SG_uint32 length_got = 0;
        SG_bool b_done = SG_FALSE;

        SG_ASSERT(pCtx!=NULL);
        SG_UNUSED(processedLength);
        SG_NULLARGCHECK_RETURN(pBuffer);
        SG_ARGCHECK_RETURN(bufferLength>0, bufferLength);
        SG_NULLARGCHECK_RETURN(pContext);

        while(!b_done && (total_length_got<bufferLength))
        {
            SG_ERR_CHECK_RETURN(  SG_repo__fetch_blob__chunk(pCtx,
                pContext->pRepo, pContext->pFetchBlobHandle,
                bufferLength-total_length_got, pBuffer+total_length_got,
                &length_got, &b_done)  );
            total_length_got+=length_got;
        }
    }

    static void _generic_blob_response__finished(SG_context * pCtx, void * pContext__voidp)
    {
        _generic_blob_response__context * pContext = (_generic_blob_response__context*)pContext__voidp;

        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(pContext);

        SG_ERR_CHECK(  SG_repo__fetch_blob__end(pCtx, pContext->pRepo, &pContext->pFetchBlobHandle)  );

        _GENERIC_BLOB_RESPONSE__CONTEXT__NULLFREE(pCtx, pContext);

        return;
    fail:
        _GENERIC_BLOB_RESPONSE__CONTEXT__NULLFREE(pCtx, pContext);
    }

    static void _generic_blob_response__aborted(SG_context * pCtx, void * pContext)
    {
        SG_ASSERT(pCtx!=NULL);
        _GENERIC_BLOB_RESPONSE__CONTEXT__NULLFREE(pCtx, pContext);
    }

void _create_response_handle_for_blob(
    SG_context * pCtx,
    const char * pHttpStatusCode,
    SG_contenttype contentType,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    const char * pHid,
    _response_handle ** ppResponseHandle)
{
    const char * pContentType = NULL;
    SG_repo_fetch_blob_handle * pFetchBlobHandle = NULL;
    SG_uint64 length = 0;
    _generic_blob_response__context * pContext = NULL;

    SG_NONEMPTYCHECK_RETURN(pHttpStatusCode);
    pContentType = _response_header_for(contentType);
    //SG_ARGCHECK_RETURN(pContentType!=NULL, contentType);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NONEMPTYCHECK_RETURN(pHid);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    SG_ERR_CHECK(  SG_repo__fetch_blob__begin(pCtx, *ppRepo, pHid, SG_TRUE, NULL, NULL, NULL, NULL, &length, &pFetchBlobHandle)  );

    SG_ERR_CHECK(  _generic_blob_response__context__alloc(pCtx, &pContext, ppRepo, &pFetchBlobHandle)  );

    SG_ERR_CHECK_RETURN(  _response_handle__alloc(pCtx, ppResponseHandle,
        pHttpStatusCode, pContentType, length,
        _generic_blob_response__chunk, _generic_blob_response__finished, _generic_blob_response__aborted,
        pContext)  );

    return;
fail:
    _GENERIC_BLOB_RESPONSE__CONTEXT__NULLFREE(pCtx, pContext);
    if(pFetchBlobHandle!=NULL)
        SG_ERR_IGNORE(  SG_repo__fetch_blob__abort(pCtx, *ppRepo, &pFetchBlobHandle)  );
}

typedef struct
{
	SG_file* pFileFragball;
	SG_pathname* pPathFile;
	SG_bool bDeleteWhenDone;
} _generic_file_response_context;

static void _generic_file_response__context__alloc(
	SG_context* pCtx,
	SG_file* pFileFragball,
	SG_pathname* pPathFile,
	SG_bool bDeleteWhenDone,
	_generic_file_response_context** ppState)
{
	_generic_file_response_context* pState = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pState)  );
	pState->pFileFragball = pFileFragball;
	pState->pPathFile = pPathFile;
	pState->bDeleteWhenDone = bDeleteWhenDone;

	SG_RETURN_AND_NULL(pState, ppState);
fail:
	SG_NULLFREE(pCtx, pState);
}

static void _generic_file_response__context__free(SG_context* pCtx, _generic_file_response_context* pState)
{
	if (pState)
	{
		SG_FILE_NULLCLOSE(pCtx, pState->pFileFragball);
		SG_PATHNAME_NULLFREE(pCtx, pState->pPathFile);
		SG_NULLFREE(pCtx, pState);
	}
}
#define _GENERIC_FILE_RESPONSE__CONTEXT__NULLFREE(pCtx,p) SG_STATEMENT(SG_context__push_level(pCtx); _generic_file_response__context__free(pCtx,p); SG_context__pop_level(pCtx); p=NULL;)

static void _generic_file_response__chunk(SG_context * pCtx, SG_uint64 processedLength, SG_byte * pBuffer, SG_uint32 bufferLength, void * pContext)
{
	_generic_file_response_context* pState = (_generic_file_response_context*)pContext;

	SG_uint32 total_length_got = 0;
	SG_uint32 length_got = 0;

	SG_ERR_CHECK_RETURN(  SG_file__seek(pCtx, pState->pFileFragball, processedLength)  );

	while(total_length_got<bufferLength)
	{
		SG_ERR_CHECK_RETURN(  SG_file__read(pCtx, pState->pFileFragball, bufferLength-total_length_got, pBuffer+total_length_got, &length_got)  );
		total_length_got+=length_got;
	}
}

static void _generic_file_response__finished(SG_context * pCtx, void * pContext)
{
	_generic_file_response_context* pState = pContext;

	if (pState->bDeleteWhenDone && pState->pPathFile)
	{
		SG_ERR_CHECK_RETURN(  SG_file__close(pCtx, &pState->pFileFragball)  );
		SG_ERR_CHECK_RETURN(  SG_fsobj__remove__pathname(pCtx, pState->pPathFile)  );
	}

	_GENERIC_FILE_RESPONSE__CONTEXT__NULLFREE(pCtx, pState);
}

static void _generic_file_response__aborted(SG_context * pCtx, void * pContext)
{
	_generic_file_response_context* pState = pContext;

	_GENERIC_FILE_RESPONSE__CONTEXT__NULLFREE(pCtx, pState);
}

void _create_response_handle_for_file(
	SG_context * pCtx,
	const char * pHttpStatusCode,
	SG_contenttype contentType,
	SG_pathname** ppPathFile, // On success we've taken ownership and nulled the caller's copy.
	SG_bool bDeleteOnSuccessfulFinish,
	_response_handle ** ppResponseHandle)
{
	const char * pContentType = NULL;
	SG_uint64 length = 0;
	SG_file* pFile = NULL;
	_generic_file_response_context* pResponseCtx = NULL;


	SG_NONEMPTYCHECK_RETURN(pHttpStatusCode);
	pContentType = _response_header_for(contentType);
	SG_NULL_PP_CHECK_RETURN(ppPathFile);
	SG_NULLARGCHECK_RETURN(ppResponseHandle);
	SG_ASSERT(*ppResponseHandle==NULL);

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, *ppPathFile, SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile)  );
	SG_ERR_CHECK(  SG_file__seek_end(pCtx, pFile, &length)  );

	SG_ERR_CHECK(  _generic_file_response__context__alloc(pCtx, pFile, *ppPathFile, bDeleteOnSuccessfulFinish, &pResponseCtx)  );

	SG_ERR_CHECK_RETURN(  _response_handle__alloc(pCtx, ppResponseHandle,
		pHttpStatusCode, pContentType, length,
		_generic_file_response__chunk, _generic_file_response__finished, _generic_file_response__aborted,
		pResponseCtx)  );

	pFile = NULL;
	pResponseCtx = NULL;
	*ppPathFile = NULL;

	/* fall through */
fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	_GENERIC_FILE_RESPONSE__CONTEXT__NULLFREE(pCtx, pResponseCtx);
}

_response_handle * _create_response_handle_for_error(SG_context * pCtx)
{
    SG_string * pString = NULL;
    SG_error err;

    if(pCtx==NULL)
        return GENERIC_BAILOUT_RESPONSE_HANDLE;

    SG_log_current_error_debug(pCtx);

    err = SG_context__err_to_string(pCtx, &pString);

    if(err==SG_ERR_MALLOCFAILED)
    {
        SG_ASSERT(pString==NULL);
        return MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE;
    }
    else if(SG_IS_ERROR(err) || pString==NULL)
    {
        SG_ASSERT(pString==NULL);
        return GENERIC_BAILOUT_RESPONSE_HANDLE;
    }
    else
    {
        const char * pHttpStatusCode = "500 Internal Server Error";
        _response_handle * pResponseHandle = NULL;

        //todo: Some of these ERR codes wanted to be more of special cases than
        // just setting the correct http status code. Also, we'll probably want
        // to do something different for debug versus release builds (in some
        // or all cases...?).
        if(SG_context__err_equals(pCtx,SG_ERR_URI_HTTP_400_BAD_REQUEST))
            pHttpStatusCode = "400 Bad Request";
        else if(SG_context__err_equals(pCtx,SG_ERR_URI_HTTP_401_UNAUTHORIZED))
            pHttpStatusCode = "401 Unauthorized";
        else if(SG_context__err_equals(pCtx,SG_ERR_URI_HTTP_404_NOT_FOUND))
            pHttpStatusCode = "404 Not Found";
        else if(SG_context__err_equals(pCtx,SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED))
            pHttpStatusCode = "405 Method Not Allowed";
        else if(SG_context__err_equals(pCtx,SG_ERR_URI_HTTP_413_REQUEST_ENTITY_TOO_LARGE))
            pHttpStatusCode = "413 Request Entity Too Large";
        else if(SG_context__err_equals(pCtx,SG_ERR_URI_HTTP_500_INTERNAL_SERVER_ERROR))
            pHttpStatusCode = "500 Internal Server Error";
        else if(SG_context__err_equals(pCtx,SG_ERR_URI_POSTDATA_MISSING))
            pHttpStatusCode = "400 Bad Request";

        SG_context__push_level(pCtx);
        {
            _create_response_handle_for_string(pCtx, pHttpStatusCode, SG_contenttype__text, &pString, &pResponseHandle);
            if(SG_context__has_err(pCtx) || pResponseHandle==NULL)
            {
                SG_log_current_error_urgent(pCtx);
                SG_STRING_NULLFREE(pCtx, pString);
                SG_ASSERT(pResponseHandle==NULL); // Otherwise memleak?
                if(SG_context__err_equals(pCtx, SG_ERR_MALLOCFAILED))
                    pResponseHandle = MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE;
                else
                    pResponseHandle = GENERIC_BAILOUT_RESPONSE_HANDLE;
            }
        }
        SG_context__pop_level(pCtx);

        SG_ASSERT(pResponseHandle!=NULL);
        return pResponseHandle;
    }
}

//////////////////////////////////////////////////////////////////


// -- _POST__tests_file_specific -- //
    typedef struct
    {
        SG_pathname * pPathname;
        SG_file * pFile;
    } _uri__tests_file_specific_POST__context;

    static void _uri__tests_file_specific_POST__context__alloc(
        SG_context * pCtx,
        _uri__tests_file_specific_POST__context ** ppNew,
        SG_pathname ** ppPathname,
        SG_file ** ppFile)
    {
        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(ppNew);
        SG_ASSERT(*ppNew==NULL);
        SG_NULL_PP_CHECK_RETURN(ppPathname);
        SG_NULL_PP_CHECK_RETURN(ppFile);

        SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, *ppNew)  );
        (*ppNew)->pPathname = *ppPathname;
        *ppPathname = NULL;
        (*ppNew)->pFile = *ppFile;
        *ppFile = NULL;
    }

    static void _uri__tests_file_specific_POST__context__free(
        SG_context * pCtx,
        _uri__tests_file_specific_POST__context * p)
    {
        if(p==NULL)
            return;
        SG_PATHNAME_NULLFREE(pCtx,p->pPathname);
        SG_FILE_NULLCLOSE(pCtx,p->pFile);
        SG_NULLFREE(pCtx,p);
    }
#define _URI__TESTS_FILES_POST__CONTEXT__NULLFREE(pCtx,p) SG_STATEMENT(SG_context__push_level(pCtx); _uri__tests_file_specific_POST__context__free(pCtx,p); SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL;)

    static void _uri__tests_file_specific_POST__chunk(SG_context * pCtx, _response_handle ** ppResponseHandle, SG_byte * pBuffer, SG_uint32 bufferLength, void * pContext__voidp)
    {
        _uri__tests_file_specific_POST__context * pContext = (_uri__tests_file_specific_POST__context*)pContext__voidp;

        SG_UNUSED(ppResponseHandle);

        SG_NULLARGCHECK_RETURN(pContext);
        SG_ARGCHECK_RETURN(pContext->pFile!=NULL, pContext);

        SG_ERR_CHECK_RETURN(  SG_file__write(pCtx, pContext->pFile, bufferLength, pBuffer, NULL)  );
    }

    static void _uri__tests_file_specific_POST__finish(SG_context * pCtx, const SG_audit * pAudit, void * pContext, _response_handle ** ppResponseHandle)
    {
        SG_ASSERT(pCtx!=NULL);
        SG_UNUSED(pAudit);
        SG_UNUSED(ppResponseHandle);
        _URI__TESTS_FILES_POST__CONTEXT__NULLFREE(pCtx, pContext);
    }

    static void _uri__tests_file_specific_POST__abort(SG_context * pCtx, void * pContext__voidp)
    {
        _uri__tests_file_specific_POST__context * pContext = (_uri__tests_file_specific_POST__context*)pContext__voidp;

        SG_ERR_CHECK(  SG_file__close(pCtx, &pContext->pFile)  );
        SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx,pContext->pPathname)  );

        _URI__TESTS_FILES_POST__CONTEXT__NULLFREE(pCtx, pContext);

        return;
    fail:
        SG_log_current_error_urgent(pCtx);
        _URI__TESTS_FILES_POST__CONTEXT__NULLFREE(pCtx, pContext);
    }

static void _POST__tests_file_specific(
    SG_context * pCtx,
    const _request_headers * pRequestHeaders,
    const char * pFilename,
    _request_handle ** ppRequestHandle)
{
    SG_pathname * pPathname = NULL;
    SG_file * pFile = NULL;
    _uri__tests_file_specific_POST__context * pContext = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NONEMPTYCHECK_RETURN(pFilename);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    //todo: we need to special case a zero-length file: create the file right here and now, and don't set up a handle for chunking

    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx,&pPathname,pFilename)  );
    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathname, SG_FILE_WRONLY|SG_FILE_CREATE_NEW, 0666, &pFile)  );

    SG_ERR_CHECK(  _uri__tests_file_specific_POST__context__alloc(pCtx, &pContext, &pPathname, &pFile)  );
    SG_ERR_CHECK(  _request_handle__alloc(pCtx, ppRequestHandle, pRequestHeaders,
        _uri__tests_file_specific_POST__chunk,
        _uri__tests_file_specific_POST__finish,
        _uri__tests_file_specific_POST__abort,
        pContext)  );

    return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPathname);
    SG_FILE_NULLCLOSE(pCtx, pFile);
    _URI__TESTS_FILES_POST__CONTEXT__NULLFREE(pCtx, pContext);
}

static void _dispatch__tests_files(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    if(uriSubstringsCount==0)
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    else if(uriSubstringsCount==1)
    {
        if(eq(pRequestHeaders->pRequestMethod,"POST"))
            SG_ERR_CHECK_RETURN(  _POST__tests_file_specific(pCtx, pRequestHeaders, ppUriSubstrings[0], ppRequestHandle)  );
        else
            SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    }
    else
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);
}

static void _dispatch__tests(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    if(uriSubstringsCount==0)
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    if(eq(ppUriSubstrings[0],"files") )
        SG_ERR_CHECK_RETURN(  _dispatch__tests_files(pCtx, pRequestHeaders, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle)  );
    else
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);
}


// -- _POST__changeset_specific comments, stamps, tags etc -- //
    typedef struct
    {
        SG_repo * pRepo;
        char * pChangesetHid;
        SG_string * pRequestBody;
    }_uri__changeset_specific_POST__context;

    static void _uri__changeset_specific_POST__context__alloc(
        SG_context * pCtx,
        _uri__changeset_specific_POST__context ** ppNew,
        SG_repo ** ppRepo,
        char ** ppChangesetHid,
        SG_string ** ppRequestBody)
    {
        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(ppNew);
        SG_ASSERT(*ppNew==NULL);
        SG_NULL_PP_CHECK_RETURN(ppRepo);
        SG_NULL_PP_CHECK_RETURN(ppChangesetHid);
        SG_NULL_PP_CHECK_RETURN(ppRequestBody);

        SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, *ppNew)  );

        (*ppNew)->pRepo = *ppRepo;
        *ppRepo = NULL;
        (*ppNew)->pChangesetHid = *ppChangesetHid;
        *ppChangesetHid = NULL;
        (*ppNew)->pRequestBody = *ppRequestBody;
        *ppRequestBody = NULL;
    }

    static void _uri__changeset_specific_POST__context__free(
        SG_context * pCtx,
        _uri__changeset_specific_POST__context * p)
    {
        if(p==NULL)
            return;
        SG_REPO_NULLFREE(pCtx,p->pRepo);
        SG_NULLFREE(pCtx, p->pChangesetHid);
        SG_STRING_NULLFREE(pCtx, p->pRequestBody);
        SG_NULLFREE(pCtx,p);
    }
#define _URI__CHANGESET_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx,p) SG_STATEMENT(SG_context__push_level(pCtx); _uri__changeset_specific_POST__context__free(pCtx,p); SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL;)

    static void _uri__changeset_specific_POST__chunk(SG_context * pCtx, _response_handle ** ppResponseHandle, SG_byte * pBuffer, SG_uint32 bufferLength, void * pContext__voidp)
    {
        _uri__changeset_specific_POST__context * pContext = (_uri__changeset_specific_POST__context*)pContext__voidp;

        SG_UNUSED(ppResponseHandle);

        SG_NULLARGCHECK_RETURN(pContext);
        SG_ARGCHECK_RETURN(pContext->pRequestBody!=NULL, pContext);

        SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, pContext->pRequestBody, pBuffer, bufferLength)  );
    }

    static void _uri__changeset_specific_comments_POST__finish(SG_context * pCtx, const SG_audit * pAudit, void * pContext__voidp, _response_handle ** ppResponseHandle)
    {
        _uri__changeset_specific_POST__context * pContext = (_uri__changeset_specific_POST__context*)pContext__voidp;

        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(pAudit);

        SG_NULLARGCHECK_RETURN(pContext);
        SG_ARGCHECK_RETURN(pContext->pRepo!=NULL, pContext);
        SG_ARGCHECK_RETURN(pContext->pChangesetHid!=NULL, pContext);
        SG_ARGCHECK_RETURN(pContext->pRequestBody!=NULL, pContext);

        SG_ERR_CHECK(  SG_vc_comments__add(pCtx, pContext->pRepo, pContext->pChangesetHid, SG_string__sz(pContext->pRequestBody), pAudit)  );
		//return the comments as part of the response
		SG_ERR_CHECK(  _GET__changeset_comments_json(pCtx, pContext->pRepo, pContext->pChangesetHid, ppResponseHandle)  );

        _URI__CHANGESET_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx, pContext);

        return;
    fail:
        _URI__CHANGESET_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx, pContext);
    }

	static void _uri__changeset_specific_stamps_POST__finish(SG_context * pCtx, const SG_audit * pAudit, void * pContext__voidp, _response_handle ** ppResponseHandle)
    {
        _uri__changeset_specific_POST__context * pContext = (_uri__changeset_specific_POST__context*)pContext__voidp;

	    SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(pAudit);

        SG_NULLARGCHECK_RETURN(pContext);
        SG_ARGCHECK_RETURN(pContext->pRepo!=NULL, pContext);
        SG_ARGCHECK_RETURN(pContext->pChangesetHid!=NULL, pContext);
        SG_ARGCHECK_RETURN(pContext->pRequestBody!=NULL, pContext);

		SG_ERR_CHECK(  SG_vc_stamps__add(pCtx, pContext->pRepo, pContext->pChangesetHid, SG_string__sz(pContext->pRequestBody), pAudit)  );
		//return the stamps as part of the response
		SG_ERR_CHECK(  _GET__changeset_stamp_json(pCtx, pContext->pRepo, pContext->pChangesetHid, ppResponseHandle)  );

        _URI__CHANGESET_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx, pContext);

        return;
    fail:
		 _URI__CHANGESET_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx, pContext);
    }

	static void _uri__changeset_specific_tags_POST__finish(SG_context * pCtx, const SG_audit * pAudit, void * pContext__voidp, _response_handle ** ppResponseHandle)
    {
        _uri__changeset_specific_POST__context * pContext = (_uri__changeset_specific_POST__context*)pContext__voidp;

	    SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(pAudit);

        SG_NULLARGCHECK_RETURN(pContext);
        SG_ARGCHECK_RETURN(pContext->pRepo!=NULL, pContext);
        SG_ARGCHECK_RETURN(pContext->pChangesetHid!=NULL, pContext);
        SG_ARGCHECK_RETURN(pContext->pRequestBody!=NULL, pContext);

		SG_vc_tags__add(pCtx, pContext->pRepo, pContext->pChangesetHid, SG_string__sz(pContext->pRequestBody), pAudit);

		if (SG_context__has_err(pCtx))
		{
			if (SG_context__err_equals(pCtx, SG_ERR_TAG_ALREADY_EXISTS) || SG_context__err_equals(pCtx, SG_ERR_ZING_CONSTRAINT))
			{
				char* pszhid = NULL;
				SG_string* content = NULL;
				SG_vhash* pvhResult = NULL;
				SG_vhash* pvhChangesetDescription;

				SG_context__err_reset(pCtx);

				SG_VHASH__ALLOC(pCtx, &pvhResult);
				SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pContext->pRepo, SG_string__sz(pContext->pRequestBody), &pszhid)  );

				SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &content)  );
				SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhResult, "duplicate", SG_string__sz(pContext->pRequestBody)) );
				SG_ERR_CHECK(  SG_history__get_changeset_description(pCtx, pContext->pRepo, pszhid, SG_FALSE, &pvhChangesetDescription)  );
				SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhResult, "changeset", &pvhChangesetDescription) );

				SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhResult, content) );
				SG_NULLFREE(pCtx, pszhid);

				SG_ERR_CHECK( _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &content, ppResponseHandle)  );
				SG_STRING_NULLFREE(pCtx, content);
				SG_VHASH_NULLFREE(pCtx, pvhChangesetDescription);
			}
			else
			{
				SG_ERR_RETHROW;
			}

		}
		else
		{
			//return the stamps as part of the response
			SG_ERR_CHECK(  _GET__changeset_tags_json(pCtx, pContext->pRepo, pContext->pChangesetHid, ppResponseHandle)  );
		}

        _URI__CHANGESET_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx, pContext);

        return;
    fail:
		 _URI__CHANGESET_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx, pContext);
    }
    static void _uri__changeset_specific_POST__abort(SG_context * pCtx, void * pContext)
    {
        _URI__CHANGESET_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx, pContext);
    }

	static void _POST__changeset_specific( SG_context * pCtx,
		_request_headers * pRequestHeaders,
		SG_repo ** ppRepo,
		char ** ppChangesetHid,
		_request_chunk_cb pChunk,
		_request_finish_cb pFinish,
		_request_aborted_cb pAborted,
		_request_handle ** ppRequestHandle)
	{
		SG_string * pRequestBody = NULL;
		_uri__changeset_specific_POST__context * pContext = NULL;

		SG_ASSERT(pCtx!=NULL);
		SG_NULLARGCHECK_RETURN(pRequestHeaders);
		SG_NULL_PP_CHECK_RETURN(ppChangesetHid);
		SG_NULL_PP_CHECK_RETURN(ppRepo);
		SG_NULLARGCHECK_RETURN(ppRequestHandle);
		SG_ASSERT(*ppRequestHandle==NULL);

		if(pRequestHeaders->pFrom==NULL || pRequestHeaders->pFrom[0]=='\0')
			SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_401_UNAUTHORIZED);

		if(pRequestHeaders->contentLength==0)
			SG_ERR_THROW_RETURN(SG_ERR_URI_POSTDATA_MISSING);
		if(pRequestHeaders->contentLength>SG_STRING__MAX_LENGTH_IN_BYTES)
			SG_ERR_THROW(SG_ERR_URI_HTTP_413_REQUEST_ENTITY_TOO_LARGE);

		//(*ppRequestHandle)->pContext
		SG_ERR_CHECK(  SG_STRING__ALLOC__RESERVE(pCtx, &pRequestBody, (SG_uint32)pRequestHeaders->contentLength)  );

		SG_ERR_CHECK(  _uri__changeset_specific_POST__context__alloc(pCtx, &pContext, ppRepo, ppChangesetHid, &pRequestBody)  );

		SG_ERR_CHECK(  _request_handle__alloc(pCtx, ppRequestHandle, pRequestHeaders,
			pChunk,
			pFinish,
			pAborted,
			pContext)  );

		return;
	fail:
		SG_STRING_NULLFREE(pCtx, pRequestBody);
		_URI__CHANGESET_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx, pContext);
	}

static void _POST__changeset_specific_comments(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    char ** ppChangesetHid, // On success we've taken ownership and nulled the caller's copy.
    _request_handle ** ppRequestHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULL_PP_CHECK_RETURN(ppChangesetHid);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    SG_ERR_CHECK_RETURN(_POST__changeset_specific(pCtx, pRequestHeaders, ppRepo, ppChangesetHid,
		_uri__changeset_specific_POST__chunk,
		_uri__changeset_specific_comments_POST__finish,
		_uri__changeset_specific_POST__abort, ppRequestHandle) );

}

static void _GET__changeset_comments_json(
    SG_context * pCtx,
	SG_repo* pRepo,
	const char * pChangesetHid,
    _response_handle ** ppResponseHandle)
{
	SG_varray* pvaComments = NULL;
	SG_string* content = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &content)  );
	SG_ERR_CHECK(   SG_history__get_changeset_comments(pCtx, pRepo, pChangesetHid, &pvaComments)  );

	SG_varray__to_json(pCtx, pvaComments, content);

	_create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &content, ppResponseHandle);
fail:
	SG_VARRAY_NULLFREE(pCtx, pvaComments);
	SG_STRING_NULLFREE(pCtx, content);
}

static char* _getObjectType(SG_treenode_entry_type type)
{
	if (type == SG_TREENODEENTRY_TYPE_REGULAR_FILE)
		return "File";
	else if (type == SG_TREENODEENTRY_TYPE_DIRECTORY)
			return "Directory";
	else if (type == SG_TREENODEENTRY_TYPE_SYMLINK)
			return "Symlink";
	else
		return "Invalid";
}

static void _GET__treenode_entry_json( SG_context * pCtx,
	 SG_repo* pRepo,
	 const char* pszChangeSetID,
	 SG_treenode_entry* pTreenodeEntry,
	 const char* pszPath,
	 const char* pszGid,
	 _response_handle ** ppResponseHandle)
{
	SG_string* content = NULL;
	SG_uint32 count;
	SG_uint32 k;
	const char* pszHid = NULL;
	const char* pszName = NULL;
	SG_treenode_entry_type ptneType;
	SG_vhash* pvhResults = NULL;
	SG_vhash* pvhChild = NULL;
	SG_varray* pvaChildren = NULL;
	SG_treenode* pTreenode = NULL;
	SG_string* pStrPath = NULL;

	SG_ERR_CHECK(  SG_treenode_entry__get_entry_type(pCtx, pTreenodeEntry, &ptneType)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &content)  );
	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhResults)  );
	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pvaChildren)  );

	SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, pTreenodeEntry, &pszHid)  );
	SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx, pTreenodeEntry, &pszName)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhResults, "name", pszName) );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhResults, "changeset_id", pszChangeSetID) );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhResults, "path", pszPath) );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhResults, "gid", pszGid)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhResults, "type", _getObjectType(ptneType))  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhResults, "hid", pszHid)  );


	if (ptneType == SG_TREENODEENTRY_TYPE_DIRECTORY)
	{
		SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszHid, &pTreenode)  );

		SG_ERR_CHECK(  SG_treenode__count(pCtx, pTreenode, &count)  );

		for (k=0; k < count; k++)
		{
			const char* szGidObject = NULL;
			const char* pszCurHid = NULL;
			const char* pszName = NULL;
			SG_treenode_entry_type ptneCurType;
			const SG_treenode_entry* ptneChild;

			SG_VHASH__ALLOC(pCtx, &pvhChild);
			SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx, pTreenode, k, &szGidObject, &ptneChild)  );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhChild, "gid", szGidObject)  );

			SG_treenode_entry__get_hid_blob(pCtx, ptneChild, &pszCurHid);
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhChild, "hid", pszCurHid)  );

			SG_treenode_entry__get_entry_type(pCtx, ptneChild, &ptneCurType);
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhChild, "type", _getObjectType(ptneCurType))  );

			SG_treenode_entry__get_entry_name(pCtx, ptneChild, &pszName);
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhChild, "name", pszName)  );

			SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pStrPath, pszPath)  );
			SG_ERR_CHECK(  SG_repopath__append_entryname(pCtx, pStrPath, pszName, SG_FALSE) );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhChild, "path", SG_string__sz(pStrPath))  );

			SG_varray__append__vhash(pCtx, pvaChildren, &pvhChild);

			//SG_NULLFREE(pCtx, szGidObject);
			SG_VHASH_NULLFREE(pCtx, pvhChild);
			SG_STRING_NULLFREE(pCtx, pStrPath);
		}
	}
	SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvhResults, "contents", &pvaChildren) );
	pvaChildren = NULL;

	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhResults, content) );
	SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &content, ppResponseHandle)  );

fail:
	SG_STRING_NULLFREE(pCtx, content);
	SG_TREENODE_NULLFREE(pCtx, pTreenode);
	SG_VHASH_NULLFREE(pCtx, pvhResults);
    SG_VARRAY_NULLFREE(pCtx, pvaChildren);

}

static void _dispatch__changeset_browse_path(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    char ** ppChangesetHid, // On success we've taken ownership and nulled the caller's copy.
    const char **ppUriSubstrings,
	SG_uint32 countSubstrings,
	_response_handle ** ppResponseHandle)
{
	SG_treenode_entry* tne = NULL;
	SG_changeset* pChangeset = NULL;
	const char* pszHidRoot = NULL;
	SG_treenode* ptn = NULL;
	char* pszGid = NULL;
	SG_pathname* path = NULL;
	SG_string* strPath = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULL_PP_CHECK_RETURN(ppChangesetHid);

	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, *ppRepo, *ppChangesetHid, &pChangeset)  );
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pChangeset, &pszHidRoot)  );
	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, *ppRepo, pszHidRoot, &ptn)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &strPath, "@/")  );
	if (countSubstrings == 0)
	{
		SG_treenode__find_treenodeentry_by_path(pCtx, *ppRepo, ptn, "@", &pszGid, &tne);
	}
	else
	{
		SG_uint32 i;

		for (i=1; i<countSubstrings; i++)
		{
			SG_ERR_CHECK(  SG_repopath__append_entryname(pCtx, strPath, ppUriSubstrings[i], SG_TRUE) );
		}
		SG_ERR_CHECK(  SG_repopath__normalize(pCtx, strPath, SG_FALSE)  );
		SG_treenode__find_treenodeentry_by_path(pCtx, *ppRepo, ptn, SG_string__sz(strPath), &pszGid, &tne);
		if (SG_context__has_err(pCtx) || !tne)
		{
			SG_ERR_RESET_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
		}

	}

	SG_ERR_CHECK( _GET__treenode_entry_json(pCtx, *ppRepo, *ppChangesetHid, tne, SG_string__sz(strPath), pszGid, ppResponseHandle)  );

fail:
	SG_TREENODE_ENTRY_NULLFREE(pCtx, tne);
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	SG_PATHNAME_NULLFREE(pCtx, path);
	SG_TREENODE_NULLFREE(pCtx, ptn);
	SG_NULLFREE(pCtx, pszGid);
	if (strPath)
		SG_STRING_NULLFREE(pCtx, strPath);


}


static void _dispatch__changeset_browse_gid(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    char ** ppChangesetHid, // On success we've taken ownership and nulled the caller's copy.
    const char **ppFolderGid,
	_response_handle ** ppResponseHandle)
{
	SG_treenode_entry* tne = NULL;

	SG_changeset* pChangeset = NULL;
	//const char* pszHidRoot = NULL;
	SG_treenode* ptn = NULL;
	char* pszGid = NULL;
	SG_pathname* path = NULL;
	SG_stringarray* pPaths = NULL;
	const char* pszPath = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULL_PP_CHECK_RETURN(ppChangesetHid);

	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, *ppRepo, *ppChangesetHid, &pChangeset)  );

	SG_repo__treendx__get_path_in_dagnode(pCtx, *ppRepo, SG_DAGNUM__VERSION_CONTROL, *ppFolderGid, *ppChangesetHid, &tne);
	if (SG_context__has_err(pCtx) || !tne)
	{
		SG_ERR_RESET_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
	}

	SG_ERR_CHECK (  SG_repo__treendx__get_all_paths(pCtx, *ppRepo, SG_DAGNUM__VERSION_CONTROL,  *ppFolderGid, &pPaths)  );

	SG_ERR_CHECK (  SG_stringarray__get_nth(pCtx, pPaths, 0, &pszPath)  );

	SG_ERR_CHECK( _GET__treenode_entry_json(pCtx, *ppRepo, *ppChangesetHid, tne, pszPath, *ppFolderGid, ppResponseHandle)  );
fail:
	SG_TREENODE_ENTRY_NULLFREE(pCtx, tne);
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	SG_TREENODE_NULLFREE(pCtx, ptn);
	SG_PATHNAME_NULLFREE(pCtx, path);
	SG_STRINGARRAY_NULLFREE(pCtx, pPaths);
	SG_NULLFREE(pCtx, pszGid);

}

static void _dispatch__changeset_specific_comments(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    char ** ppChangesetHid, // On success we've taken ownership and nulled the caller's copy.
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
	_response_handle ** ppResponseHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULL_PP_CHECK_RETURN(ppChangesetHid);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"POST"))
		{
            SG_ERR_CHECK(  _POST__changeset_specific_comments(pCtx, pRequestHeaders, ppRepo, ppChangesetHid, ppRequestHandle)  );
		}
        else
            SG_ERR_CHECK(  _GET__changeset_comments_json(pCtx, *ppRepo, *ppChangesetHid, ppResponseHandle)  );

    }
    else
	{
        SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
	}

fail:
	;
}

static void _GET__changeset_stamp_json(  SG_context * pCtx,
	SG_repo* pRepo,
	char * pChangesetHid,
    _response_handle ** ppResponseHandle)
{
	SG_varray* pvaStamps = NULL;
	SG_string* content = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pChangesetHid);

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &content)  );

	SG_ERR_CHECK(  SG_vc_stamps__lookup(pCtx, pRepo, pChangesetHid, &pvaStamps)  );

	SG_varray__to_json(pCtx, pvaStamps, content);

	_create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &content, ppResponseHandle);
fail:
	SG_VARRAY_NULLFREE(pCtx, pvaStamps);
	SG_STRING_NULLFREE(pCtx, content);
}


static void  _POST__changeset_specific_stamps(  SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo,
    char ** ppChangesetHid,
    _request_handle ** ppRequestHandle,
	_response_handle ** ppResponseHandle)
{

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULL_PP_CHECK_RETURN(ppChangesetHid);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
	SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    SG_ERR_CHECK_RETURN(_POST__changeset_specific(pCtx, pRequestHeaders, ppRepo, ppChangesetHid,
		_uri__changeset_specific_POST__chunk,
		_uri__changeset_specific_stamps_POST__finish,
		_uri__changeset_specific_POST__abort, ppRequestHandle) );
}

static void  _DELETE__changeset_specific_stamps(  SG_context * pCtx,
    SG_repo ** ppRepo,
	const char* pFrom,
    char ** ppChangesetHid,
	const char** ppStamp,
	SG_uint32 count,
	_response_handle ** ppResponseHandle)
{
	SG_audit q;

	if(pFrom==NULL || pFrom[0]=='\0')
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_401_UNAUTHORIZED);

	SG_ASSERT(pCtx!=NULL);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULL_PP_CHECK_RETURN(ppChangesetHid);
    SG_NULLARGCHECK_RETURN(ppStamp);
	SG_NULLARGCHECK_RETURN(ppStamp);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);

	SG_ERR_CHECK(  SG_audit__init(pCtx, &q, *ppRepo, SG_AUDIT__WHEN__NOW, pFrom)  );

	SG_ERR_CHECK( SG_vc_stamps__remove(pCtx, *ppRepo, &q, *ppChangesetHid, count, (const char* const*) ppStamp) );

	SG_ERR_CHECK(  _GET__changeset_stamp_json(pCtx, *ppRepo, *ppChangesetHid, ppResponseHandle)  );

fail:

	;

}

static void _dispatch__changeset_specific_stamps(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo,
    char ** ppChangesetHid,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
	_response_handle ** ppResponseHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULL_PP_CHECK_RETURN(ppChangesetHid);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

	SG_UNUSED(ppResponseHandle);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"POST"))
		{
            SG_ERR_CHECK(  _POST__changeset_specific_stamps(pCtx, pRequestHeaders, ppRepo, ppChangesetHid, ppRequestHandle, ppResponseHandle)  );
		}
        else
		{
            SG_ERR_CHECK(  _GET__changeset_stamp_json(pCtx, *ppRepo, *ppChangesetHid, ppResponseHandle)  );
		}

    }
    else
	{
		if(eq(pRequestHeaders->pRequestMethod,"DELETE"))
		{
			 SG_ERR_CHECK(  _DELETE__changeset_specific_stamps(pCtx, ppRepo,  pRequestHeaders->pFrom, ppChangesetHid, ppUriSubstrings, uriSubstringsCount, ppResponseHandle)  );
		}
		else
			SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
	}

fail:
	;
}

static void  _DELETE__changeset_specific_tags(  SG_context * pCtx,
    SG_repo ** ppRepo,
	const char* pFrom,
	char** ppChangesetHid,
  	const char** ppTag,
	SG_uint32 count,
   	_response_handle ** ppResponseHandle)
{
	SG_audit q;

	if(pFrom==NULL || pFrom[0]=='\0')
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_401_UNAUTHORIZED);

	SG_ASSERT(pCtx!=NULL);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULLARGCHECK_RETURN(ppTag);
	SG_NULLARGCHECK_RETURN(ppResponseHandle);

	SG_ERR_CHECK(  SG_audit__init(pCtx, &q, *ppRepo, SG_AUDIT__WHEN__NOW, pFrom)  );

	SG_ERR_CHECK(  SG_vc_tags__remove(pCtx, *ppRepo, &q, count, ppTag) );

	SG_ERR_CHECK(  _GET__changeset_tags_json(pCtx, *ppRepo, *ppChangesetHid, ppResponseHandle)  );

fail:

	;

}

static void  _POST__changeset_specific_tag(  SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo,
    char ** ppChangesetHid,
    _request_handle ** ppRequestHandle,
	_response_handle ** ppResponseHandle)
{

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULL_PP_CHECK_RETURN(ppChangesetHid);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
	SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    SG_ERR_CHECK_RETURN(_POST__changeset_specific(pCtx, pRequestHeaders, ppRepo, ppChangesetHid,
		_uri__changeset_specific_POST__chunk,
		_uri__changeset_specific_tags_POST__finish,
		_uri__changeset_specific_POST__abort, ppRequestHandle) );
}

static void _GET__changeset_tags_json(  SG_context * pCtx,
	SG_repo* pRepo,
	char * pChangesetHid,
    _response_handle ** ppResponseHandle)
{
	SG_varray* pvaTags = NULL;
	SG_string* content = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pChangesetHid);

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &content)  );

	SG_ERR_CHECK(  SG_vc_tags__lookup(pCtx, pRepo, pChangesetHid, &pvaTags)  );

	SG_varray__to_json(pCtx, pvaTags, content);

	_create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &content, ppResponseHandle);
fail:
	SG_VARRAY_NULLFREE(pCtx, pvaTags);
	SG_STRING_NULLFREE(pCtx, content);
}


static void _dispatch__changeset_specific_tags(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    char ** ppChangesetHid, // On success we've taken ownership and nulled the caller's copy.
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
	_response_handle ** ppResponseHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULL_PP_CHECK_RETURN(ppChangesetHid);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"POST"))
		{
            SG_ERR_CHECK(  _POST__changeset_specific_tag(pCtx, pRequestHeaders, ppRepo, ppChangesetHid, ppRequestHandle, ppResponseHandle)  );
		}

        else
            SG_ERR_CHECK(  _GET__changeset_tags_json(pCtx, *ppRepo, *ppChangesetHid, ppResponseHandle)  );

    }
    else
	{
		if(eq(pRequestHeaders->pRequestMethod,"DELETE"))
		{
			 SG_ERR_CHECK(  _DELETE__changeset_specific_tags(pCtx, ppRepo, pRequestHeaders->pFrom, ppChangesetHid, ppUriSubstrings, uriSubstringsCount, ppResponseHandle)  );
		}
        else
			SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
	}

fail:
	;
}


static void _changeset_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement)
{
	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "TITLE"))
	{
		//TODO put CSID in title?
		SG_ERR_CHECK(  SG_string__set__sz(pCtx, replacement, "veracity / changeset")  );
	}

fail:
	;
}

static void _browse_changeset_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement)
{
	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "TITLE"))
	{
		//TODO put CSID in title?
		SG_ERR_CHECK(  SG_string__set__sz(pCtx, replacement, "veracity / browse changeset")  );
	}

fail:
	;
}


static void _GET__changeset_json(SG_context * pCtx,
	SG_repo* pRepo,
	const char * pChangesetHid,
    _response_handle ** ppResponseHandle)
{
	SG_string *content = NULL;
	SG_vhash* pvhChangesetDescription = NULL;
	SG_vhash* pvhDiffHash = NULL;
	SG_vhash* pvhParents = NULL;
	SG_vhash* pvhResult = NULL;
	SG_varray* pvahids = NULL;
	SG_uint32 count;
    SG_pendingtree * pPendingTree = NULL;
	SG_treediff2* pTreeDiff = NULL;

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhResult)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &content)  );

	//get the change set description	
	SG_ERR_CHECK(  SG_history__get_changeset_description(pCtx, pRepo, pChangesetHid, SG_TRUE, &pvhChangesetDescription)  );

	//Get the parent's HIDs for a tree diff. 
	SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhChangesetDescription, "parents", &pvhParents)  );
	SG_ERR_CHECK(  SG_vhash__get_keys(pCtx, pvhParents, &pvahids) );
	SG_ERR_CHECK(  SG_varray__count(pCtx, pvahids, &count) );
	SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhResult, "description", &pvhChangesetDescription)  );
    pvhChangesetDescription = NULL;
	if (count > 0)
	{
		SG_uint32 i;

		for (i=0; i<count; i++)
		{
			const char* hid = NULL;
			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pvahids, i, &hid)  );
			SG_ERR_CHECK(  SG_treediff2__alloc(pCtx, pRepo, &pTreeDiff)  );
			SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhDiffHash)  );
			SG_ERR_CHECK(  SG_treediff2__compare_cset_vs_cset(pCtx, pTreeDiff,  hid, pChangesetHid)  );
			SG_ERR_CHECK(  SG_treediff2__report_status_to_vhash(pCtx, pTreeDiff, pvhDiffHash, SG_FALSE) );
			SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhResult, hid, &pvhDiffHash)  );
			SG_VHASH_NULLFREE(pCtx, pvhDiffHash);
			SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);	
		}
	}

	SG_vhash__to_json(pCtx, pvhResult, content);

	_create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &content, ppResponseHandle);

fail:
	SG_VHASH_NULLFREE(pCtx, pvhDiffHash);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_VHASH_NULLFREE(pCtx, pvhChangesetDescription);
	SG_VHASH_NULLFREE(pCtx, pvhResult);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRING_NULLFREE(pCtx, content);
	SG_VARRAY_NULLFREE(pCtx, pvahids);
}

static void _dispatch__changeset_specific_browsecs(
   SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    char ** ppChangesetHid, // On success we've taken ownership and nulled the caller's copy.
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
	_response_handle ** ppResponseHandle)
{

	SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

	if(eq(pRequestHeaders->pRequestMethod,"GET"))
	{
		if(pRequestHeaders->accept==SG_contenttype__json)
		{
			if (uriSubstringsCount == 0)
			{
				SG_ERR_CHECK(  _dispatch__changeset_browse_path(pCtx, pRequestHeaders, ppRepo, ppChangesetHid, NULL, 0, ppResponseHandle)  );

			}
			else if (eq("@", ppUriSubstrings[0]))
			{
				SG_ERR_CHECK(  _dispatch__changeset_browse_path(pCtx, pRequestHeaders, ppRepo, ppChangesetHid,  ppUriSubstrings, uriSubstringsCount, ppResponseHandle)  );
			}
			else
			{
				SG_ERR_CHECK(  _dispatch__changeset_browse_gid(pCtx, pRequestHeaders, ppRepo, ppChangesetHid, ppUriSubstrings, ppResponseHandle)  );

			}
		}
        else
               SG_ERR_CHECK(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "browsecs.xhtml", _browse_changeset_replacer, ppResponseHandle)  );
	}
	else
		SG_ERR_THROW(  SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED  );


fail:
	;
}

static void _dispatch__changeset_specific(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    char ** ppChangesetHid, // On success we've taken ownership and nulled the caller's copy.
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
	_response_handle ** ppResponseHandle
   )
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    if(uriSubstringsCount==0)
	{
		if(eq(pRequestHeaders->pRequestMethod,"GET"))
		{
			if(pRequestHeaders->accept==SG_contenttype__json)
                 SG_ERR_CHECK(  _GET__changeset_json(pCtx, *ppRepo, *ppChangesetHid, ppResponseHandle)  );
            else
                SG_ERR_CHECK(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "changeset.xhtml", _changeset_replacer, ppResponseHandle)  );
		}
		else
            SG_ERR_THROW(  SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED  );
	}
    else if(eq(ppUriSubstrings[0],"comments"))
	{
        SG_ERR_CHECK(  _dispatch__changeset_specific_comments(pCtx, pRequestHeaders, ppRepo, ppChangesetHid, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
	}
    else if(eq(ppUriSubstrings[0],"browsecs"))
	{
		SG_ERR_CHECK(  _dispatch__changeset_specific_browsecs(pCtx, pRequestHeaders, ppRepo, ppChangesetHid, ppUriSubstrings+1,uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
	}
	else if (eq(ppUriSubstrings[0],"stamps"))
	{
		SG_ERR_CHECK(  _dispatch__changeset_specific_stamps(pCtx, pRequestHeaders, ppRepo, ppChangesetHid, ppUriSubstrings+1,uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );

	}
	else if (eq(ppUriSubstrings[0],"tags"))
	{
		SG_ERR_CHECK(  _dispatch__changeset_specific_tags(pCtx, pRequestHeaders, ppRepo, ppChangesetHid, ppUriSubstrings+1,uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );

	}
	else
        SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
fail:
	SG_NULLFREE(pCtx, *ppChangesetHid);
}


static void _dispatch__repo_specific_blobs(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
	SG_bool bDownload,
    const char ** ppUriSubstrings,
	 SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
	_response_handle ** ppResponseHandle
   )
{
	SG_string * pContentDisposition = NULL;
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

   if (uriSubstringsCount < 1)
		SG_ERR_RESET_THROW(  SG_ERR_URI_HTTP_404_NOT_FOUND  );

   SG_ERR_CHECK(  _create_response_handle_for_blob(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__default, ppRepo, ppUriSubstrings[0], ppResponseHandle)  );
   if (bDownload)
   {
	   if ( uriSubstringsCount > 1 && ppUriSubstrings[1])
	   {
		   	//this should force a download dialog
	   		//Content-disposition: attachment; filename=fname.ext
	   		SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pContentDisposition, "attachment; filename=")  );
			SG_ERR_CHECK(  SG_string__append__sz(pCtx, pContentDisposition, ppUriSubstrings[1])  );

	   		SG_ERR_CHECK( _response_handle__add_header(pCtx, *ppResponseHandle, "Content-disposition", SG_string__sz(pContentDisposition))  );
	   		SG_STRING_NULLFREE(pCtx, pContentDisposition);
	   }
	   else
		   SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
   }

fail:
	SG_STRING_NULLFREE(pCtx, pContentDisposition);
}


static void _fetch_blob_to_zip_file(SG_context* pCtx, SG_repo* pRepo, const char* pszHid, SG_string* pstrPath, const char* pszName, SG_zip** pzip)
{
	SG_repo_fetch_blob_handle* pbh = NULL;
    SG_uint64 len = 0;
    SG_uint32 got = 0;
    SG_byte* p_buf = NULL;
    SG_uint64 left = 0;
	SG_string* pPathFile = NULL;
    SG_bool b_done = SG_FALSE;

	SG_ERR_CHECK(  SG_string__alloc__sz(pCtx, &pPathFile, SG_string__sz(pstrPath))  );

	if (strlen(SG_string__sz(pPathFile)) > 0)
	{
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pPathFile, "/") );
	}
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pPathFile, pszName) );

	SG_ERR_CHECK(  SG_zip__begin_file(pCtx, *pzip, SG_string__sz(pPathFile))  );
	SG_ERR_CHECK(  SG_repo__fetch_blob__begin(pCtx, pRepo,  pszHid, SG_TRUE, NULL, NULL, NULL, NULL, &len, &pbh)  );
	left = len;
	SG_ERR_CHECK(  SG_alloc(pCtx, SG_STREAMING_BUFFER_SIZE, 1, &p_buf)  );
	while (!b_done)
	{
		SG_uint32 want = SG_STREAMING_BUFFER_SIZE;
		if (want > left)
		{
			want = (SG_uint32) left;
		}
		SG_ERR_CHECK(  SG_repo__fetch_blob__chunk(pCtx, pRepo, pbh, want, p_buf, &got, &b_done)  );
		SG_ERR_CHECK(  SG_zip__write(pCtx, *pzip, p_buf, got)  );
		left -= got;
	}
	SG_ERR_CHECK(  SG_repo__fetch_blob__end(pCtx, pRepo, &pbh)  );
    SG_ASSERT(0 == left);
	SG_ERR_CHECK(  SG_zip__end_file(pCtx, *pzip)  );
	SG_NULLFREE(pCtx, p_buf);
	SG_STRING_NULLFREE(pCtx, pPathFile);
	return;

fail:
	SG_ERR_IGNORE(  SG_repo__fetch_blob__abort(pCtx, pRepo, &pbh)  );
	SG_STRING_NULLFREE(pCtx, pPathFile);
    SG_NULLFREE(pCtx, p_buf);
}

static void _zip_repo_treenode(SG_context* pCtx, SG_repo* pRepo, const char* pszHid, SG_string** ppstrPath, SG_zip** pzip)
{
	SG_treenode* pTreenode = NULL;
	SG_uint32 count = 0;
	SG_uint32 i;

	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszHid, &pTreenode)  );
	SG_ERR_CHECK(  SG_treenode__count(pCtx, pTreenode, &count)  );
	if (count == 0)
	{
		SG_string* pPathFolderEmpty = NULL;
		SG_ERR_CHECK(  SG_string__alloc__sz(pCtx, &pPathFolderEmpty, SG_string__sz(*ppstrPath))  );
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pPathFolderEmpty, "/") );
		SG_ERR_CHECK(  SG_zip__add_folder(pCtx, *pzip, SG_string__sz(pPathFolderEmpty))  );
		SG_STRING_NULLFREE(pCtx, pPathFolderEmpty);
	}
	for (i=0; i<count; i++)
	{
		const SG_treenode_entry* pEntry = NULL;
		SG_treenode_entry_type type;
		const char* pszName = NULL;
		const char* pszidHid = NULL;
		const char* pszgid = NULL;

		SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx, pTreenode, i, &pszgid, &pEntry)  );
		SG_ERR_CHECK(  SG_treenode_entry__get_entry_type(pCtx, pEntry, &type)  );
		SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx, pEntry, &pszName)  );
		SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, pEntry, &pszidHid)  );

		if (SG_TREENODEENTRY_TYPE_DIRECTORY == type)
		{
			SG_string* pPathFolder = NULL;
			SG_ERR_CHECK(  SG_string__alloc__sz(pCtx, &pPathFolder, SG_string__sz(*ppstrPath))  );

			if (strlen(SG_string__sz(pPathFolder)) > 0)
			{
				SG_ERR_CHECK(  SG_string__append__sz(pCtx, pPathFolder, "/") );
			}
			if (strcmp(pszName, "@") != 0)
			{
				SG_ERR_CHECK(  SG_string__append__sz(pCtx, pPathFolder, pszName) );
			}
			SG_ERR_CHECK(  _zip_repo_treenode(pCtx, pRepo, pszidHid, &pPathFolder, &(*pzip))  );
			SG_STRING_NULLFREE(pCtx, pPathFolder);
		}
		else if (SG_TREENODEENTRY_TYPE_REGULAR_FILE == type)
		{
			SG_ERR_CHECK(  _fetch_blob_to_zip_file(pCtx, pRepo, pszidHid, *ppstrPath, pszName, &(*pzip) ) );
		}
	}
fail:
	SG_TREENODE_NULLFREE(pCtx, pTreenode);
}

static void _zip_repo(SG_context* pCtx, SG_repo* pRepo, const char* pszRepoName, SG_pathname* pPathParentDir, const char* pszChangesetID, SG_pathname** ppPathZip)
{
	char* psz_hid_cs = NULL;
	const char* pszHidTreeNode = NULL;
	SG_string* pZipName = NULL;
	SG_string* pstrPathInit = NULL;
	SG_zip* pzip = NULL;
	SG_changeset* pChangeset = NULL;
	SG_rbtree* prbLeaves = NULL;
	SG_pathname* pPathZip = NULL;

	SG_NULLARGCHECK_RETURN(pPathParentDir);
	SG_NULLARGCHECK_RETURN(pszRepoName);
	SG_NULLARGCHECK_RETURN(pRepo);

	SG_ERR_CHECK(  SG_pathname__alloc__copy(pCtx, &pPathZip, pPathParentDir)  );

	if (pszChangesetID)
	{
		SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, pszChangesetID, &psz_hid_cs)  );
		if (!psz_hid_cs)
		{
			SG_ERR_THROW( SG_ERR_NOT_FOUND );
		}
	}
	else
	{
		SG_uint32 count = 0;
		const char* pszBasline = NULL;
		char* pszCopyBaseline = NULL;

		//TODO should we throw an error if two heads or do somethine else?
		SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, &prbLeaves)  );
		SG_ERR_CHECK(  SG_rbtree__count(pCtx, prbLeaves, &count)  );
		if (1 == count)
		{
			SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, prbLeaves, &pszBasline, NULL)  );
		}
		else
		{
			SG_ERR_THROW( SG_ERR_MULTIPLE_HEADS_FROM_DAGNODE );
		}

		if (!pszBasline)
		{
			SG_ERR_THROW(SG_ERR_NOT_FOUND);
		}

		SG_STRDUP(pCtx, pszBasline, &pszCopyBaseline);
		psz_hid_cs = pszCopyBaseline;
		SG_RBTREE_NULLFREE(pCtx, prbLeaves);

	}

	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, psz_hid_cs, &pChangeset)  );
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pChangeset, &pszHidTreeNode) );

	SG_ERR_CHECK(  SG_string__alloc__sz(pCtx, &pZipName, pszRepoName )  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pZipName, "-" )  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pZipName, psz_hid_cs)  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pZipName, ".zip")  );

	SG_ERR_CHECK(  SG_pathname__append__from_string(pCtx, pPathZip, pZipName)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pstrPathInit, "")  );
	SG_ERR_CHECK(  SG_zip__open(pCtx, pPathZip, &pzip)  );
	SG_ERR_CHECK(  _zip_repo_treenode(pCtx, pRepo, pszHidTreeNode, &pstrPathInit, &pzip) );
	SG_ERR_CHECK(  SG_zip__nullclose(pCtx, &pzip) );

	*ppPathZip = pPathZip;

fail:

	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	SG_STRING_NULLFREE(pCtx, pstrPathInit);
	SG_STRING_NULLFREE(pCtx, pZipName);
	SG_NULLFREE(pCtx, psz_hid_cs);
	if (pzip)
		 SG_zip__nullclose(pCtx, &pzip);
	if (prbLeaves)
		SG_RBTREE_NULLFREE(pCtx, prbLeaves);


}
 static void _GET__repo_zip_json(SG_context* pCtx, SG_repo* pRepo, const char* pszRepoName, const char* pszChangesetID, _response_handle ** ppResponseHandle)
 {
	SG_pathname* pPathZipTemp = NULL;
	SG_pathname* pPathZip = NULL;
	SG_string* pContentDisposition = NULL;
	SG_string* pstrZipName = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__USER_TEMP_DIRECTORY(pCtx, &pPathZipTemp)  );

	_zip_repo(pCtx, pRepo, pszRepoName, pPathZipTemp, pszChangesetID, &pPathZip);
	if (SG_context__has_err(pCtx))
	{
		SG_ERR_RESET_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
	}
	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pContentDisposition, "attachment; filename=")  );

	//Content-disposition: attachment; filename=fname.ext forces download
	SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathZip, &pstrZipName)  );
	SG_ERR_CHECK(  SG_string__append__string(pCtx, pContentDisposition, pstrZipName)  );
	SG_STRING_NULLFREE(pCtx, pstrZipName);
	SG_ERR_CHECK( _create_response_handle_for_file(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__default, &pPathZip, SG_TRUE, ppResponseHandle)  );

	SG_ERR_CHECK( _response_handle__add_header(pCtx, *ppResponseHandle, "Content-disposition", SG_string__sz(pContentDisposition))  );

fail:

	SG_PATHNAME_NULLFREE(pCtx, pPathZip);
	SG_PATHNAME_NULLFREE(pCtx, pPathZipTemp);
	if (pstrZipName)
		SG_STRING_NULLFREE(pCtx, pstrZipName);
	SG_STRING_NULLFREE(pCtx, pContentDisposition);

 }

static void _dispatch__repo_zip(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
	const char * pszRepoName,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
	_response_handle ** ppResponseHandle
   )
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);

	if(eq(pRequestHeaders->pRequestMethod,"GET"))
	{
		if(uriSubstringsCount==0)
             SG_ERR_CHECK(  _GET__repo_zip_json(pCtx, *ppRepo, pszRepoName, NULL, ppResponseHandle)  );

		else if (uriSubstringsCount == 1)
			SG_ERR_CHECK(  _GET__repo_zip_json(pCtx, *ppRepo, pszRepoName, ppUriSubstrings[0], ppResponseHandle)  );
		else
         	SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);

	}

	else
		 SG_ERR_THROW(  SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED  );
fail:
	;
}

static void _GET__diff_local(SG_context * pCtx,  SG_repo** ppRepo, const char ** ppArgs, SG_uint32 argCount, _response_handle ** ppResponseHandle)
{
	SG_tempfile* pTempFileA = NULL;
	SG_file* pFile = NULL;
	char buf_tid[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathResult = NULL;
	SG_string* pStringLabel_a = NULL;
	SG_string* pStringLabel_b = NULL;
	SG_bool bFoundChanges;
	SG_pathname* pPathCwd = NULL;
	SG_pathname* pPathLocal = NULL;
	SG_string* strPathRepo = NULL;
	SG_uint32 nStart = 2;
	SG_uint32 i;

	SG_diff_ext__compare_callback * pfnCompare = SG_internalfilediff__unified;

	//get the local path from the repo path
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &strPathRepo, "@/")  );
	for (i=nStart; i< argCount; i++)
	{
		SG_ERR_CHECK(  SG_repopath__append_entryname(pCtx, strPathRepo, ppArgs[i], SG_TRUE) );
	}
	SG_ERR_CHECK(  SG_repopath__normalize(pCtx, strPathRepo, SG_FALSE)  );
	SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx, pPathCwd,  SG_string__sz(strPathRepo), &pPathLocal) );

	//create a temp file for the result
	//we can't use tempfile for the result because it doesn't clean up well with the file response handler
	SG_ERR_CHECK(  SG_tid__generate2(pCtx, buf_tid, sizeof(buf_tid), 32)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__USER_TEMP_DIRECTORY(pCtx, &pPathResult)  );
	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPathResult, buf_tid)  );

	//TODO diff from baseline with only 1 arg
	//TODO url would be diff/@repopath

	//if we have a HID and a path as the passed in values, diff locally agaist
	//that path. Diff local url must have the form ...diff/hid/@repopath
	//fetch the blob into a temp file
	SG_ERR_CHECK(  SG_tempfile__open_new(pCtx, &pTempFileA)  );
	SG_ERR_CHECK(  SG_repo__fetch_blob_into_file(pCtx,*ppRepo, ppArgs[0],pTempFileA->pFile,NULL)  );
	SG_ERR_CHECK(  SG_tempfile__close(pCtx, pTempFileA)  );
	SG_ERR_CHECK(  SG_diff_utils__make_label(pCtx,SG_string__sz(strPathRepo),ppArgs[0],NULL,&pStringLabel_a)  );
	SG_ERR_CHECK(  SG_diff_utils__make_label(pCtx, SG_pathname__sz(pPathLocal), NULL, NULL, &pStringLabel_b)  );


	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathResult, SG_FILE_RDWR | SG_FILE_CREATE_NEW, SG_FSOBJ_PERMS__MASK, &pFile) );

	SG_ERR_CHECK(  (pfnCompare)(pCtx, pTempFileA->pPathname, pPathLocal,
										   SG_string__sz(pStringLabel_a),SG_string__sz(pStringLabel_b),
										   pFile,NULL,
										   &bFoundChanges)  );

	SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );
	SG_ERR_CHECK( _create_response_handle_for_file(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__default, &pPathResult, SG_TRUE, ppResponseHandle)  );


fail:
	//pathname and sgfile get freed in the response handle cleanup
	SG_STRING_NULLFREE(pCtx, pStringLabel_a);
	SG_STRING_NULLFREE(pCtx, pStringLabel_b);
	SG_STRING_NULLFREE(pCtx, strPathRepo);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_PATHNAME_NULLFREE(pCtx, pPathLocal);
	SG_STRING_NULLFREE(pCtx, pStringLabel_b);
	SG_ERR_IGNORE(  SG_tempfile__delete(pCtx, &pTempFileA)  );

}
static void _GET__diff(SG_context * pCtx,  SG_repo** ppRepo, const char ** ppHids, SG_uint32 hidCount, const char* szFilename, _response_handle ** ppResponseHandle)
{
	SG_tempfile* pTempFileA = NULL;
	SG_tempfile* pTempFileB = NULL;
	SG_file* pFile = NULL;
	char buf_tid[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathResult = NULL;
	SG_string* pStringLabel_a = NULL;
	SG_string* pStringLabel_b = NULL;
	SG_bool bFoundChanges;

	//TODO get this from a config file of some sort. For now just hardcode it
	SG_diff_ext__compare_callback * pfnCompare = SG_internalfilediff__unified;
	
	SG_ERR_CHECK(  SG_tid__generate2(pCtx, buf_tid, sizeof(buf_tid), 32)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__USER_TEMP_DIRECTORY(pCtx, &pPathResult)  );
	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPathResult, buf_tid)  );

	if (hidCount >= 2)
	{
		SG_ERR_CHECK(  SG_tempfile__open_new(pCtx, &pTempFileA)  );
		SG_ERR_CHECK(  SG_repo__fetch_blob_into_file(pCtx,*ppRepo, ppHids[0],pTempFileA->pFile,NULL)  );
		SG_ERR_CHECK(  SG_tempfile__close(pCtx, pTempFileA)  );
		SG_ERR_CHECK(  SG_diff_utils__make_label(pCtx,szFilename,ppHids[0],NULL,&pStringLabel_a)  );

		SG_ERR_CHECK(  SG_tempfile__open_new(pCtx, &pTempFileB)  );
		SG_ERR_CHECK(  SG_repo__fetch_blob_into_file(pCtx,*ppRepo, ppHids[1],pTempFileB->pFile,NULL)  );
		SG_ERR_CHECK(  SG_tempfile__close(pCtx, pTempFileB)  );
		SG_ERR_CHECK(  SG_diff_utils__make_label(pCtx,szFilename,ppHids[1],NULL,&pStringLabel_b)  );
	}

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathResult, SG_FILE_RDWR | SG_FILE_CREATE_NEW, SG_FSOBJ_PERMS__MASK, &pFile) );

	SG_ERR_CHECK(  (pfnCompare)(pCtx, pTempFileA->pPathname, pTempFileB->pPathname,
										   SG_string__sz(pStringLabel_a),SG_string__sz(pStringLabel_b),
										   pFile,NULL,
										   &bFoundChanges)  );

	SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );
	SG_ERR_CHECK( _create_response_handle_for_file(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__default, &pPathResult, SG_TRUE, ppResponseHandle)  );


fail:
	//pathname and sgfile get freed in the response handle cleanup
	SG_STRING_NULLFREE(pCtx, pStringLabel_a);
	SG_STRING_NULLFREE(pCtx, pStringLabel_b);
	SG_ERR_IGNORE(  SG_tempfile__delete(pCtx, &pTempFileA)  );
	SG_ERR_IGNORE(  SG_tempfile__delete(pCtx, &pTempFileB)  );
}
static void _dispatch__repo_specific_diff(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    const char ** ppUriSubstrings,
	SG_uint32 uriSubstringsCount,
	_response_handle ** ppResponseHandle
   )
{
	const char* pszName = "temp";

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);

	 if(uriSubstringsCount >= 2)
	 {
		if(eq(pRequestHeaders->pRequestMethod,"GET"))
		{
			if (uriSubstringsCount >= 3 && ppUriSubstrings[2])
				pszName = ppUriSubstrings[2];

			if (eq(ppUriSubstrings[1],"@") )
			{
				SG_ERR_CHECK(  _GET__diff_local(pCtx, ppRepo, ppUriSubstrings, uriSubstringsCount, ppResponseHandle) );
			}
			else
				SG_ERR_CHECK(  _GET__diff(pCtx, ppRepo, ppUriSubstrings, uriSubstringsCount, pszName, ppResponseHandle)  );

		}
		else
            SG_ERR_THROW(  SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED  );
	}

    else
        SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
fail:
    ;

}


static SG_bool _isRecId(SG_context *pCtx, SG_repo *repo, SG_uint32 dagnum, const char *recid)
{
	SG_rbtree *pLeaves = NULL;

    SG_uint32 leafCount = 0;
    const char * pBaseline = NULL;
	SG_bool	result = SG_FALSE;
	SG_varray *pQuery = NULL;
	SG_varray *pva_query_result = NULL;
	SG_stringarray *psa_fields = NULL;
	SG_uint32 numCurrentHids = 0;

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, repo, dagnum, &pLeaves)  );
    SG_ERR_CHECK(  SG_rbtree__count(pCtx, pLeaves, &leafCount)  );

    if(leafCount==0)
		goto fail;

    if(leafCount>1)
		goto fail;

    SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, pLeaves, &pBaseline, NULL)  );

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pQuery)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pQuery, SG_ZING_FIELD__RECID)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pQuery, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pQuery, recid)  );

    SG_ERR_CHECK(  SG_stringarray__alloc(pCtx, &psa_fields, 1)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "_rechid")  );

    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, repo, dagnum, pBaseline, pQuery, psa_fields, &pva_query_result)  );
    SG_VARRAY_NULLFREE(pCtx, pQuery);
    if(pva_query_result==NULL)
		goto fail;

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_query_result, &numCurrentHids )  );

    if(numCurrentHids != 1)
		goto fail;

	result = SG_TRUE;

fail:
	SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_RBTREE_NULLFREE(pCtx, pLeaves); // Waited until now to free this since it owns pBaseline.
	SG_VARRAY_NULLFREE(pCtx, pva_query_result);
	SG_VARRAY_NULLFREE(pCtx, pQuery);

	return(result);
}

static SG_bool _isRecRequest(SG_context *pCtx, SG_repo *repo, SG_uint32 dagnum, const char **ppUriSubstrings, SG_uint32 uriSubstringsCount)
{
	SG_bool	hasRecords = SG_FALSE;
	SG_bool isRecords = SG_FALSE;
	SG_bool isRecId = SG_FALSE;

	hasRecords = (uriSubstringsCount >= 1) && (eq("records", ppUriSubstrings[0]));
	isRecords = hasRecords && (uriSubstringsCount == 1);
	isRecId = hasRecords && (uriSubstringsCount == 2) && _isRecId(pCtx, repo, dagnum, ppUriSubstrings[1]);

	return(isRecords || isRecId);
}


static void _dispatch__repo_specific_wit(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
	_response_handle ** ppResponseHandle
   )
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

	if ((pRequestHeaders->accept!=SG_contenttype__json) &&
		eq(pRequestHeaders->pRequestMethod, "GET") &&
		_isRecRequest(pCtx, *ppRepo, SG_DAGNUM__WORK_ITEMS, ppUriSubstrings, uriSubstringsCount)
		)
	{
        SG_REPO_NULLFREE(pCtx, *ppRepo);
		SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "listwi.xhtml", _new_wit_replacer, ppResponseHandle)  );
	}
	else
	{
        SG_ERR_CHECK_RETURN(  _dispatch__zing_dag_specific(pCtx, pRequestHeaders, ppRepo, SG_DAGNUM__WORK_ITEMS, ppUriSubstrings, uriSubstringsCount, ppRequestHandle, ppResponseHandle)  );
	}
}


static void _dispatch__repo_specific_changesets(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
	_response_handle ** ppResponseHandle
   )
{
    char * pChangesetHid = NULL;


    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

	SG_repo__hidlookup__dagnode(pCtx, *ppRepo, SG_DAGNUM__VERSION_CONTROL, ppUriSubstrings[0], &pChangesetHid);
    if(pChangesetHid==NULL||SG_context__has_err(pCtx))
         SG_ERR_RESET_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
    SG_ERR_CHECK(  _dispatch__changeset_specific(pCtx, pRequestHeaders, ppRepo, &pChangesetHid, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );

fail:
    SG_NULLFREE(pCtx, pChangesetHid);

}

// -- _POST__zing_record -- //
    typedef struct
    {
        SG_repo * pRepo;
        SG_uint32 dagnum;
        SG_string * pRequestBody;
    } _uri__zing_record_POST__context;

    static void _uri__zing_record_POST__context__alloc(
        SG_context * pCtx,
        _uri__zing_record_POST__context ** ppNew,
        SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
        SG_uint32 dagnum,
        SG_string ** ppRequestBody) // On success we've taken ownership and nulled the caller's copy.
    {
        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(ppNew);
        SG_ASSERT(*ppNew==NULL);
        SG_NULL_PP_CHECK_RETURN(ppRepo);
        SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
        SG_NULL_PP_CHECK_RETURN(ppRequestBody);

        SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, *ppNew)  );
        (*ppNew)->pRepo = *ppRepo;
        *ppRepo = NULL;
        (*ppNew)->dagnum = dagnum;
        (*ppNew)->pRequestBody = *ppRequestBody;
        *ppRequestBody = NULL;
    }

    static void _uri__zing_record_POST__context__free(
        SG_context * pCtx,
        _uri__zing_record_POST__context * p)
    {
        if(p==NULL)
            return;
        SG_REPO_NULLFREE(pCtx,p->pRepo);
        SG_STRING_NULLFREE(pCtx, p->pRequestBody);
        SG_NULLFREE(pCtx,p);
    }
#define _URI__ZING_RECORD_POST__CONTEXT__NULLFREE(pCtx,p) SG_STATEMENT(SG_context__push_level(pCtx); _uri__zing_record_POST__context__free(pCtx,p); SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL;)

    static void _uri__zing_record_POST__chunk(SG_context * pCtx, _response_handle ** ppResponseHandle, SG_byte * pBuffer, SG_uint32 bufferLength, void * pContext__voidp)
    {
        _uri__zing_record_POST__context * pContext = (_uri__zing_record_POST__context*)pContext__voidp;

        SG_UNUSED(ppResponseHandle);

        SG_NULLARGCHECK_RETURN(pContext);
        SG_ARGCHECK_RETURN(pContext->pRequestBody!=NULL, pContext);

        SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, pContext->pRequestBody, pBuffer, bufferLength)  );
    }

    static void _uri__zing_record_POST__finish(SG_context * pCtx, const SG_audit * pAudit, void * pContext__voidp, _response_handle ** ppResponseHandle)
    {
        _uri__zing_record_POST__context * pContext = (_uri__zing_record_POST__context*)pContext__voidp;

        SG_rbtree * pLeaves = NULL;
        SG_uint32 leafCount = 0;
        const char * pBaseline = NULL;
        SG_zingtx * pZingTx = NULL;
        const char * pRectype = NULL;
        SG_zingrecord * pZingRecord = NULL;
        SG_vhash * pVhash = NULL;
        const char * pRecId = NULL;
        SG_string * pResponseBody = NULL;
		SG_vhash *linksToSet = NULL;

        SG_changeset * pChangeset = NULL;
        SG_dagnode * pDagnode = NULL;
		const char *szUserId = NULL;
		SG_vhash * pUserinfo = NULL;

        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(pAudit);

        SG_NULLARGCHECK_RETURN(pContext);
        SG_ARGCHECK_RETURN(pContext->pRepo!=NULL, pContext);
        SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(pContext->dagnum), pContext);
        SG_ARGCHECK_RETURN(pContext->pRequestBody!=NULL, pContext);

        SG_NULLARGCHECK_RETURN(ppResponseHandle);
        SG_ASSERT(*ppResponseHandle==NULL);

        SG_VHASH__ALLOC__FROM_JSON(pCtx, &pVhash, SG_string__sz(pContext->pRequestBody));
        if(SG_context__err_equals(pCtx, SG_ERR_JSONPARSER_SYNTAX))
            SG_ERR_RESET_THROW2(SG_ERR_URI_HTTP_400_BAD_REQUEST, (pCtx, "Request was not valid JSON."));
        else if (SG_context__has_err(pCtx))
            SG_ERR_RETHROW;

        SG_vhash__get__sz(pCtx, pVhash, SG_ZING_FIELD__RECTYPE, &pRectype);
        if(SG_context__has_err(pCtx))
            SG_ERR_RESET_THROW2(SG_ERR_URI_HTTP_400_BAD_REQUEST, (pCtx, "Record did not contain a valid rectype."));

        SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pContext->pRepo, pContext->dagnum, &pLeaves)  );
        SG_ERR_CHECK(  SG_rbtree__count(pCtx, pLeaves, &leafCount)  );
        if(leafCount==0)
            SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND); // Should never happen, but just in case, right?
        if(leafCount>1)
            SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED); //todo: ?

        SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, pLeaves, &pBaseline, NULL)  );

		SG_user__lookup_by_email(pCtx, pContext->pRepo, pAudit->who_szUserId, &pUserinfo);

		if (SG_context__has_err(pCtx))
		{
			if (SG_context__err_equals(pCtx, SG_ERR_ZING_RECTYPE_NOT_FOUND))
				szUserId = pAudit->who_szUserId;
			else
				SG_ERR_CHECK_CURRENT;
		}
		else if (pUserinfo == NULL)
		{
			szUserId = pAudit->who_szUserId;
		}
		else
		{
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pUserinfo, "recid", &szUserId)  );
		}

        SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pContext->pRepo, pContext->dagnum, szUserId, pBaseline, &pZingTx)  );
        if (pBaseline)
        {
            SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pZingTx, pBaseline)  );
        }
        SG_RBTREE_NULLFREE(pCtx, pLeaves);

		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &linksToSet)  );
		SG_ERR_CHECK(  _createRecordFromJson(pCtx, pZingTx, pRectype, pContext->pRequestBody, &pZingRecord, &pRecId, linksToSet)  );
		SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pResponseBody, pRecId)  );
        SG_VHASH_NULLFREE(pCtx, pVhash);

		SG_ERR_CHECK(  _addPendingLinks(pCtx, pZingTx, pZingRecord, linksToSet)  );
		SG_VHASH_NULLFREE(pCtx, linksToSet);

        SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, pAudit->when_int64, &pZingTx, &pChangeset, &pDagnode, NULL)  );

        SG_CHANGESET_NULLFREE(pCtx, pChangeset);
        SG_DAGNODE_NULLFREE(pCtx, pDagnode);

        SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__text, &pResponseBody, ppResponseHandle)  );
        _URI__ZING_RECORD_POST__CONTEXT__NULLFREE(pCtx, pContext);
        
        SG_VHASH_NULLFREE(pCtx, pUserinfo);

        return;
    fail:
		SG_VHASH_NULLFREE(pCtx, pUserinfo);
		SG_VHASH_NULLFREE(pCtx, linksToSet);
        SG_CHANGESET_NULLFREE(pCtx, pChangeset);
        SG_DAGNODE_NULLFREE(pCtx, pDagnode);
        SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pZingTx)  );
        SG_VHASH_NULLFREE(pCtx, pVhash);
        SG_RBTREE_NULLFREE(pCtx, pLeaves);
        SG_STRING_NULLFREE(pCtx, pResponseBody);
        _URI__ZING_RECORD_POST__CONTEXT__NULLFREE(pCtx, pContext);
    }

    static void _uri__zing_record_POST__aborted(SG_context * pCtx, void * pContext)
    {
        _URI__ZING_RECORD_POST__CONTEXT__NULLFREE(pCtx, pContext);
    }

static void _POST__zing_record(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    _request_handle ** ppRequestHandle)
{
    SG_string * pRequestBody = NULL;
    _uri__zing_record_POST__context * pContext = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    if(pRequestHeaders->pFrom==NULL || pRequestHeaders->pFrom[0]=='\0')
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_401_UNAUTHORIZED);

    if(pRequestHeaders->contentLength==0)
        SG_ERR_THROW_RETURN(SG_ERR_URI_POSTDATA_MISSING);
    if(pRequestHeaders->contentLength>SG_STRING__MAX_LENGTH_IN_BYTES)
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_413_REQUEST_ENTITY_TOO_LARGE);

    SG_ERR_CHECK(  SG_STRING__ALLOC__RESERVE(pCtx, &pRequestBody, (SG_uint32)pRequestHeaders->contentLength)  );

    SG_ERR_CHECK(  _uri__zing_record_POST__context__alloc(pCtx, &pContext, ppRepo, dagnum, &pRequestBody)  );

    SG_ERR_CHECK(  _request_handle__alloc(pCtx, ppRequestHandle, pRequestHeaders,
        _uri__zing_record_POST__chunk,
        _uri__zing_record_POST__finish,
        _uri__zing_record_POST__aborted,
        pContext)  );

    return;
fail:
    SG_STRING_NULLFREE(pCtx, pRequestBody);
    _URI__ZING_RECORD_POST__CONTEXT__NULLFREE(pCtx, pContext);
}

static void _DELETE__zing_record_specific(
    SG_context * pCtx,
    const char * pFrom,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    const char * pRecId,
    const char * pBaseline)
{
    SG_zingtx * pZingTx = NULL;
    SG_audit audit;
    SG_changeset * pChangeset = NULL;
    SG_dagnode * pDagnode = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
    SG_NONEMPTYCHECK_RETURN(pRecId);
    SG_NONEMPTYCHECK_RETURN(pBaseline);

    if(pFrom==NULL || pFrom[0]=='\0')
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_401_UNAUTHORIZED);

    SG_ERR_CHECK(  SG_audit__init(pCtx, &audit, *ppRepo, SG_AUDIT__WHEN__NOW, pFrom)  );
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, *ppRepo, dagnum, audit.who_szUserId, pBaseline, &pZingTx)  );
    if (pBaseline)
    {
        SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pZingTx, pBaseline)  );
    }
    SG_ERR_CHECK(  SG_zingtx__delete_record(pCtx, pZingTx, pRecId)  );

    SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, audit.when_int64, &pZingTx, &pChangeset, &pDagnode, NULL)  );
    SG_REPO_NULLFREE(pCtx, *ppRepo);

    SG_CHANGESET_NULLFREE(pCtx, pChangeset);
    SG_DAGNODE_NULLFREE(pCtx, pDagnode);

    return;
fail:
    SG_CHANGESET_NULLFREE(pCtx, pChangeset);
    SG_DAGNODE_NULLFREE(pCtx, pDagnode);
    SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pZingTx)  );
}

// -- _PUT__zing_record_specific -- //
    typedef struct
    {
        SG_repo * pRepo;
        SG_uint32 dagnum;
        char recId[SG_GID_BUFFER_LENGTH];
        SG_string * pRequestBody;
    } _uri__zing_record_specific_PUT__context;

    static void _uri__zing_record_specific_PUT__context__alloc(
        SG_context * pCtx,
        _uri__zing_record_specific_PUT__context ** ppNew,
        SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
        SG_uint32 dagnum,
        const char * pRecId,
        SG_string ** ppRequestBody) // On success we've taken ownership and nulled the caller's copy.
    {
        _uri__zing_record_specific_PUT__context * pNew = NULL;

        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(ppNew);
        SG_ASSERT(*ppNew==NULL);
        SG_NULL_PP_CHECK_RETURN(ppRepo);
        SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
        SG_NONEMPTYCHECK_RETURN(pRecId);
        SG_NULL_PP_CHECK_RETURN(ppRequestBody);

        SG_ERR_CHECK(  SG_alloc1(pCtx, pNew)  );
        SG_ERR_CHECK(  SG_strcpy(pCtx, pNew->recId, sizeof(pNew->recId), pRecId)  );
        pNew->pRepo = *ppRepo;
        *ppRepo = NULL;
        pNew->dagnum = dagnum;
        pNew->pRequestBody = *ppRequestBody;
        *ppRequestBody = NULL;

        *ppNew = pNew;

        return;
    fail:
        SG_NULLFREE(pCtx, pNew);
    }

    static void _uri__zing_record_specific_PUT__context__free(
        SG_context * pCtx,
        _uri__zing_record_specific_PUT__context * p)
    {
        if(p==NULL)
            return;
        SG_REPO_NULLFREE(pCtx,p->pRepo);
        SG_STRING_NULLFREE(pCtx, p->pRequestBody);
        SG_NULLFREE(pCtx,p);
    }
#define _URI__ZING_RECORD_SPECIFIC_PUT__CONTEXT__NULLFREE(pCtx,p) SG_STATEMENT(SG_context__push_level(pCtx); _uri__zing_record_specific_PUT__context__free(pCtx,p); SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL;)

    static void _uri__zing_record_specific_PUT__chunk(SG_context * pCtx, _response_handle ** ppResponseHandle, SG_byte * pBuffer, SG_uint32 bufferLength, void * pContext__voidp)
    {
        _uri__zing_record_specific_PUT__context * pContext = (_uri__zing_record_specific_PUT__context*)pContext__voidp;

        SG_UNUSED(ppResponseHandle);

        SG_NULLARGCHECK_RETURN(pContext);
        SG_ARGCHECK_RETURN(pContext->pRequestBody!=NULL, pContext);

        SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, pContext->pRequestBody, pBuffer, bufferLength)  );
    }

    static void _uri__zing_record_specific_PUT__finish(SG_context * pCtx, const SG_audit * pAudit, void * pContext__voidp, _response_handle ** ppResponseHandle)
    {
        _uri__zing_record_specific_PUT__context * pContext = (_uri__zing_record_specific_PUT__context*)pContext__voidp;

        SG_rbtree * pLeaves = NULL;
        SG_uint32 leafCount = 0;
        const char * pBaseline = NULL;
        SG_zingtx * pZingTx = NULL;
        SG_zingtemplate * pTemplate = NULL;
        SG_zingrecord * pRecord = NULL;
        const char * pRectype = NULL;
        SG_vhash * pVhash = NULL;
        SG_uint32 count, i;

        SG_changeset * pChangeset = NULL;
        SG_dagnode * pDagnode = NULL;

		SG_UNUSED(ppResponseHandle);

        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(pAudit);

        SG_NULLARGCHECK_RETURN(pContext);
        SG_ARGCHECK_RETURN(pContext->pRepo!=NULL, pContext);
        SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(pContext->dagnum), pContext);
        SG_ARGCHECK_RETURN(pContext->pRequestBody!=NULL, pContext);

        SG_VHASH__ALLOC__FROM_JSON(pCtx, &pVhash, SG_string__sz(pContext->pRequestBody));
        if(SG_context__err_equals(pCtx, SG_ERR_JSONPARSER_SYNTAX))
            SG_ERR_RESET_THROW2(SG_ERR_URI_HTTP_400_BAD_REQUEST, (pCtx, "Request was not valid JSON."));
        else if (SG_context__has_err(pCtx))
            SG_ERR_RETHROW;

        SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pContext->pRepo, pContext->dagnum, &pLeaves)  );
        SG_ERR_CHECK(  SG_rbtree__count(pCtx, pLeaves, &leafCount)  );
        if(leafCount==0)
            SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND); // Should never happen, but just in case, right?
        if(leafCount>1)
            SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED); //todo: ?

        SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, pLeaves, &pBaseline, NULL)  );
        SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pContext->pRepo, pContext->dagnum, pAudit->who_szUserId, pBaseline, &pZingTx)  );
        if (pBaseline)
        {
            SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pZingTx, pBaseline)  );
        }
        SG_RBTREE_NULLFREE(pCtx, pLeaves);

        SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pZingTx, &pTemplate)  );
        SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pZingTx, pContext->recId, &pRecord)  );
        SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pRecord, &pRectype)  );

        SG_ERR_CHECK(  SG_vhash__count(pCtx, pVhash, &count)  );
        for(i=0;i<count;++i)
        {
            const char * pKey = NULL;
            const SG_variant * pValue = NULL;
            SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pVhash, i, &pKey, &pValue)  );

			SG_ERR_CHECK(  _setFieldFromJson(pCtx, pTemplate, pRectype, pRecord, pKey, pValue, NULL)  );
        }
        SG_VHASH_NULLFREE(pCtx, pVhash);

        SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, pAudit->when_int64, &pZingTx, &pChangeset, &pDagnode, NULL)  );

        SG_CHANGESET_NULLFREE(pCtx, pChangeset);
        SG_DAGNODE_NULLFREE(pCtx, pDagnode);

        _URI__ZING_RECORD_SPECIFIC_PUT__CONTEXT__NULLFREE(pCtx, pContext);

        return;
    fail:
        SG_CHANGESET_NULLFREE(pCtx, pChangeset);
        SG_DAGNODE_NULLFREE(pCtx, pDagnode);
        SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pZingTx)  );
        SG_VHASH_NULLFREE(pCtx, pVhash);
        SG_RBTREE_NULLFREE(pCtx, pLeaves);
        _URI__ZING_RECORD_SPECIFIC_PUT__CONTEXT__NULLFREE(pCtx, pContext);
    }

    static void _uri__zing_record_specific_PUT__aborted(SG_context * pCtx, void * pContext)
    {
        _URI__ZING_RECORD_SPECIFIC_PUT__CONTEXT__NULLFREE(pCtx, pContext);
    }

static void _PUT__zing_record_specific(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    const char * pRecId,
    _request_handle ** ppRequestHandle)
{
    _uri__zing_record_specific_PUT__context * pContext = NULL;
    SG_string * pRequestBody = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
    SG_NONEMPTYCHECK_RETURN(pRecId);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    if(pRequestHeaders->pFrom==NULL || pRequestHeaders->pFrom[0]=='\0')
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_401_UNAUTHORIZED);

    if(pRequestHeaders->contentLength==0)
        SG_ERR_THROW_RETURN(SG_ERR_URI_POSTDATA_MISSING);
    if(pRequestHeaders->contentLength>SG_STRING__MAX_LENGTH_IN_BYTES)
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_413_REQUEST_ENTITY_TOO_LARGE);

    SG_ERR_CHECK(  SG_STRING__ALLOC__RESERVE(pCtx, &pRequestBody, (SG_uint32)pRequestHeaders->contentLength)  );

    SG_ERR_CHECK(  _uri__zing_record_specific_PUT__context__alloc(pCtx, &pContext, ppRepo, dagnum, pRecId, &pRequestBody)  );

    SG_ERR_CHECK(  _request_handle__alloc(pCtx, ppRequestHandle, pRequestHeaders,
        _uri__zing_record_specific_PUT__chunk,
        _uri__zing_record_specific_PUT__finish,
        _uri__zing_record_specific_PUT__aborted,
        pContext)  );

    return;
fail:
    SG_STRING_NULLFREE(pCtx, pRequestBody);
    _URI__ZING_RECORD_SPECIFIC_PUT__CONTEXT__NULLFREE(pCtx, pContext);
}

    typedef struct
    {
        SG_repo * pRepo;
        SG_uint32 dagnum;
        char recId[SG_GID_BUFFER_LENGTH];
		const char *linkName;
        SG_string * pRequestBody;
    } _uri__zing_record_specific_put_link__context;

    static void _uri__zing_record_specific_put_link__context__alloc(
        SG_context * pCtx,
        _uri__zing_record_specific_put_link__context ** ppNew,
        SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
        SG_uint32 dagnum,
        const char * pRecId,
		const char * linkName,
        SG_string ** ppRequestBody) // On success we've taken ownership and nulled the caller's copy.
    {
        _uri__zing_record_specific_put_link__context * pNew = NULL;
		char *lncopy = NULL;

        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(ppNew);
        SG_ASSERT(*ppNew==NULL);
        SG_NULL_PP_CHECK_RETURN(ppRepo);
        SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
        SG_NONEMPTYCHECK_RETURN(pRecId);
        SG_NULL_PP_CHECK_RETURN(ppRequestBody);

        SG_ERR_CHECK(  SG_alloc1(pCtx, pNew)  );
        SG_ERR_CHECK(  SG_strcpy(pCtx, pNew->recId, sizeof(pNew->recId), pRecId)  );
		SG_ERR_CHECK(  SG_strdup(pCtx, linkName, &lncopy)  );

		pNew->linkName = lncopy;
		lncopy = NULL;

        pNew->pRepo = *ppRepo;
        *ppRepo = NULL;
        pNew->dagnum = dagnum;
        pNew->pRequestBody = *ppRequestBody;
        *ppRequestBody = NULL;

        *ppNew = pNew;

        return;
    fail:
		SG_NULLFREE(pCtx, lncopy);
        SG_NULLFREE(pCtx, pNew);
    }

    static void _uri__zing_record_specific_put_link__context__free(
        SG_context * pCtx,
        _uri__zing_record_specific_put_link__context * p)
    {
        if(p==NULL)
            return;
        SG_REPO_NULLFREE(pCtx,p->pRepo);
        SG_STRING_NULLFREE(pCtx, p->pRequestBody);
		SG_NULLFREE(pCtx, p->linkName);
        SG_NULLFREE(pCtx,p);
    }
#define _URI__zing_record_specific_put_link__CONTEXT__NULLFREE(pCtx,p) SG_STATEMENT(SG_context__push_level(pCtx); _uri__zing_record_specific_put_link__context__free(pCtx,p); SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL;)

    static void _uri__zing_record_specific_put_link__chunk(SG_context * pCtx, _response_handle ** ppResponseHandle, SG_byte * pBuffer, SG_uint32 bufferLength, void * pContext__voidp)
    {
        _uri__zing_record_specific_put_link__context * pContext = (_uri__zing_record_specific_put_link__context*)pContext__voidp;

        SG_UNUSED(ppResponseHandle);

        SG_NULLARGCHECK_RETURN(pContext);
        SG_ARGCHECK_RETURN(pContext->pRequestBody!=NULL, pContext);

        SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, pContext->pRequestBody, pBuffer, bufferLength)  );
    }

    static void _uri__zing_record_specific_put_link__finish(SG_context * pCtx, const SG_audit * pAudit, void * pContext__voidp, _response_handle ** ppResponseHandle)
    {
        _uri__zing_record_specific_put_link__context * pContext = (_uri__zing_record_specific_put_link__context*)pContext__voidp;
	    char* psz_hid_cs_leaf = NULL;
		SG_zingtx* pztx = NULL;
		SG_zingrecord* newrec = NULL;
		SG_zingrecord *endrec = NULL;
		SG_dagnode* dagnode = NULL;
		SG_changeset* changeset = NULL;
		SG_zingtemplate* pzt = NULL;
		SG_zingfieldattributes* pzfa = NULL;
		SG_zinglinkattributes *pzla = NULL;
		SG_zinglinksideattributes *pzlsaMine = NULL;
		SG_zinglinksideattributes *pzlsaOther = NULL;
		SG_rbtree_iterator *rit = NULL;
		const char *rectype = NULL;
		const char *recid = NULL;
		void *data = NULL;
		SG_bool ok = SG_FALSE;
		SG_bool hasRecId = SG_FALSE;
		SG_vhash *recDetails = NULL;

		SG_UNUSED(ppResponseHandle);

		SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pContext->pRepo, pContext->dagnum, &psz_hid_cs_leaf)  );

		/* start a changeset */
		SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pContext->pRepo, pContext->dagnum, pAudit->who_szUserId, psz_hid_cs_leaf, &pztx)  );
		SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_hid_cs_leaf)  );
		SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pzt)  );

		SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, pContext->recId, &endrec)  );

		SG_ERR_CHECK(  SG_zingrecord__lookup_name(pCtx, endrec, pContext->linkName, &pzfa, &pzla, &pzlsaMine, &pzlsaOther)  );

		if ((! pzla) || (! pzlsaOther))
		{
			SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
		}

		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &rit, pzlsaOther->prb_rectypes, &ok, &rectype, &data)  );
		if (! ok)
		{
			SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
		}
		SG_RBTREE_ITERATOR_NULLFREE(pCtx, rit);

		SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &recDetails, SG_string__sz(pContext->pRequestBody))  );
		SG_ERR_CHECK(  SG_vhash__has(pCtx, recDetails, "recid", &hasRecId)  );

		if (hasRecId)
		{
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, recDetails, "recid", &recid)  );
			SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, recid, &newrec)  );
		}
		else
		{
			SG_ERR_CHECK(  _createRecordFromJson(pCtx, pztx, rectype, pContext->pRequestBody, &newrec, &recid, NULL)  );
		}

		SG_ERR_CHECK(  SG_zingrecord__add_link_and_overwrite_other_if_singular(pCtx, endrec, pContext->linkName, recid)  );

		SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, pAudit->when_int64, &pztx, &changeset, &dagnode, NULL)  );

	fail:
		SG_VHASH_NULLFREE(pCtx, recDetails);
	  SG_NULLFREE(pCtx, psz_hid_cs_leaf);
		SG_RBTREE_ITERATOR_NULLFREE(pCtx, rit);
		SG_DAGNODE_NULLFREE(pCtx, dagnode);
		SG_CHANGESET_NULLFREE(pCtx, changeset);
		SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pztx)  );
    SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaMine);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaOther);
      SG_ZINGFIELDATTRIBUTES_NULLFREE(pCtx, pzfa);
        _URI__zing_record_specific_put_link__CONTEXT__NULLFREE(pCtx, pContext);
    }

    static void _uri__zing_record_specific_put_link__aborted(SG_context * pCtx, void * pContext)
    {
        _URI__zing_record_specific_put_link__CONTEXT__NULLFREE(pCtx, pContext);
    }

static void _PUT__zing_record_specific_put_link(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    const char * pRecId,
	const char * linkName,
    _request_handle ** ppRequestHandle)
{
    _uri__zing_record_specific_put_link__context * pContext = NULL;
    SG_string * pRequestBody = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
    SG_NONEMPTYCHECK_RETURN(pRecId);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    if(pRequestHeaders->pFrom==NULL || pRequestHeaders->pFrom[0]=='\0')
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_401_UNAUTHORIZED);

    if(pRequestHeaders->contentLength==0)
        SG_ERR_THROW_RETURN(SG_ERR_URI_POSTDATA_MISSING);
    if(pRequestHeaders->contentLength>SG_STRING__MAX_LENGTH_IN_BYTES)
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_413_REQUEST_ENTITY_TOO_LARGE);

    SG_ERR_CHECK(  SG_STRING__ALLOC__RESERVE(pCtx, &pRequestBody, (SG_uint32)pRequestHeaders->contentLength)  );

    SG_ERR_CHECK(  _uri__zing_record_specific_put_link__context__alloc(pCtx, &pContext, ppRepo, dagnum, pRecId, linkName, &pRequestBody)  );

    SG_ERR_CHECK(  _request_handle__alloc(pCtx, ppRequestHandle, pRequestHeaders,
        _uri__zing_record_specific_put_link__chunk,
        _uri__zing_record_specific_put_link__finish,
        _uri__zing_record_specific_put_link__aborted,
        pContext)  );

    return;
fail:
    SG_STRING_NULLFREE(pCtx, pRequestBody);
    _URI__zing_record_specific_put_link__CONTEXT__NULLFREE(pCtx, pContext);

}

static void _GET__zing_record_specific_link(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    const char * pRecId,
	const char * linkName,
	_response_handle **ppResponseHandle)
{
    char* psz_hid_cs_leaf = NULL;
	SG_zingtx* pztx = NULL;
	SG_zingrecord *endrec = NULL;
	SG_dagnode* dagnode = NULL;
	SG_changeset* changeset = NULL;
	SG_zingtemplate* pzt = NULL;
	SG_zingfieldattributes* pzfa = NULL;
	SG_zinglinkattributes *pzla = NULL;
	SG_zinglinksideattributes *pzlsaMine = NULL;
	SG_zinglinksideattributes *pzlsaOther = NULL;
	SG_rbtree_iterator *rit = NULL;
	SG_rbtree *inLinks = NULL;
	const char *rectype = NULL;
	void *data = NULL;
	SG_bool ok = SG_FALSE;
	SG_varray *results = NULL;
	const char *cmtid = NULL;
	SG_zingrecord *cmt = NULL;
	SG_vhash *cmthash = NULL;
	SG_dbrecord *rec = NULL;
	SG_string *jsonresults = NULL;

	SG_UNUSED(ppResponseHandle);

	SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, *ppRepo, dagnum, &psz_hid_cs_leaf)  );

	/* start a changeset */
	SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, *ppRepo, dagnum,
		pRequestHeaders->pFrom ? pRequestHeaders->pFrom : "dummy@example.com", psz_hid_cs_leaf, &pztx)  );
	SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_hid_cs_leaf)  );
	SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pzt)  );

	SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, pRecId, &endrec)  );

	SG_zingrecord__lookup_name(pCtx, endrec, linkName, &pzfa, &pzla, &pzlsaMine, &pzlsaOther);

	if (SG_context__has_err(pCtx) && SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
	{
		SG_ERR_RESET_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
	}

	if ((! pzla) || (! pzlsaOther))
	{
		SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
	}

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &rit, pzlsaOther->prb_rectypes, &ok, &rectype, &data)  );
	if (! ok)
	{
		SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, rit);

	SG_ERR_CHECK(  SG_zingtx___find_links(pCtx, pztx, pzla, pRecId, pzlsaMine->bFrom, &inLinks)  );
	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &results)  );

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &rit, inLinks, &ok, &cmtid, &data)  );

	while (ok)
	{
		char	buf_recid_from[SG_GID_BUFFER_LENGTH];
		char	buf_recid_to[SG_GID_BUFFER_LENGTH];

	    SG_ERR_CHECK_RETURN(  SG_zinglink__unpack(pCtx, cmtid, buf_recid_from, buf_recid_to, NULL)  );

		if (pzlsaMine->bFrom)
			SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, buf_recid_to, &cmt)  );
		else
			SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, buf_recid_from, &cmt)  );

		SG_ERR_CHECK(  SG_zingrecord__to_dbrecord(pCtx, cmt, &rec)  );  // todo: free?
		SG_ERR_CHECK(  SG_dbrecord__to_vhash(pCtx, rec, &cmthash)  );
		SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, results, &cmthash)  );
		cmthash = NULL;
		SG_DBRECORD_NULLFREE(pCtx, rec);
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, rit, &ok, &cmtid, &data)  );
	}

	SG_RBTREE_ITERATOR_NULLFREE(pCtx, rit);

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &jsonresults)  );
	SG_ERR_CHECK(  SG_varray__to_json(pCtx, results, jsonresults)  );

	SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &jsonresults, ppResponseHandle)  );

fail:
	SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pztx)  );
	SG_DBRECORD_NULLFREE(pCtx, rec);
	SG_STRING_NULLFREE(pCtx, jsonresults);
	SG_VARRAY_NULLFREE(pCtx, results);
	SG_VHASH_NULLFREE(pCtx, cmthash);
	SG_NULLFREE(pCtx, psz_hid_cs_leaf);
	SG_RBTREE_NULLFREE(pCtx, inLinks);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, rit);
	SG_DAGNODE_NULLFREE(pCtx, dagnode);
	SG_CHANGESET_NULLFREE(pCtx, changeset);
	SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pztx)  );
	SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
	SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaMine);
	SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaOther);
	SG_ZINGFIELDATTRIBUTES_NULLFREE(pCtx, pzfa);

}

static void _dispatch__zing_record_specific(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    const char * pRecId,
    const char * pBaseline,
    const char * pRecHid,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
    SG_NULLARGCHECK_RETURN(pRecId);
    SG_NULLARGCHECK_RETURN(pRecHid);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"DELETE"))
            SG_ERR_CHECK_RETURN(  _DELETE__zing_record_specific(pCtx, pRequestHeaders->pFrom, ppRepo, dagnum, pRecId, pBaseline)  );
        else if(eq(pRequestHeaders->pRequestMethod,"GET"))
            SG_ERR_CHECK_RETURN(  _create_response_handle_for_blob(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, ppRepo, pRecHid, ppResponseHandle)  );
        else if(eq(pRequestHeaders->pRequestMethod,"PUT"))
            SG_ERR_CHECK_RETURN(  _PUT__zing_record_specific(pCtx, pRequestHeaders, ppRepo, dagnum, pRecId, ppRequestHandle)  );
        else
            SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    }
	else if (uriSubstringsCount == 1)
	{
		if (eq(pRequestHeaders->pRequestMethod,"PUT"))
			SG_ERR_CHECK_RETURN(  _PUT__zing_record_specific_put_link(pCtx, pRequestHeaders, ppRepo, dagnum, pRecId, ppUriSubstrings[0], ppRequestHandle)  );
		else if (eq(pRequestHeaders->pRequestMethod,"GET"))
			SG_ERR_CHECK_RETURN(  _GET__zing_record_specific_link(pCtx, pRequestHeaders, ppRepo, dagnum, pRecId, ppUriSubstrings[0], ppResponseHandle)  );
        else
            SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
	}
	else
		SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);
}


static SG_bool _strInField(
    const char *pattern,
    const SG_byte *blob,
    SG_uint32 bloblen)
{
    const char *string = (const char *)blob;
    SG_uint32 len = (bloblen / sizeof(char));
    const char *pptr, *sptr, *start;
    const char *end = string + len;

    for (start = string; start < end; start++)
    {
            /* find start of pattern in string */
            for ( ; ((start < end) && (toupper(*start) != toupper(*pattern))); start++)
                  ;
            if (end == start)
                  return SG_FALSE;

            pptr = pattern;
            sptr = start;

            while ((sptr < end) && (toupper(*sptr) == toupper(*pptr)))
            {
                  sptr++;
                  pptr++;

                  /* if end of pattern then pattern was found */

                  if (0 == *pptr)
                        return (SG_TRUE);
            }
      }
      return SG_FALSE;
}

static void _addLinks(
	SG_context *pCtx, 
	SG_vhash *rec,
	const char *linkname,
    SG_bool bSingular,
    SG_varray* pva_my_links
    )
{
	SG_varray *recids = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_my_links, &count)  );
    for (i=0; i<count; i++)
	{	
		const char *endrecid = NULL;
			
        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_my_links, i, &endrecid)  );

		if (bSingular)
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, rec, linkname, endrecid)  );
		else
		{
			SG_bool has = SG_FALSE;

			SG_ERR_CHECK(  SG_vhash__has(pCtx, rec, linkname, &has)  );

			if (has)
				SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, rec, linkname, &recids)  );
			else
				SG_ERR_CHECK(  SG_vhash__addnew__varray(pCtx, rec, linkname, &recids)  );

			SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, recids, endrecid)  );
		}
	}

fail:
    ;
}

static void my_find_links(
        SG_context* pCtx,
        SG_bool bFrom,
        SG_vhash* pvh_all_links,
        const char* psz_link_name,
        const char* psz_recid,
        SG_varray** ppva_my_links
        )
{
    SG_vhash* pvh_to_from = NULL;
    SG_varray* pva = NULL;
    SG_bool b = SG_FALSE;

    SG_ERR_CHECK(  SG_vhash__check__vhash(pCtx, pvh_all_links, psz_link_name, &b, &pvh_to_from)  );
    if (pvh_to_from)
    {
        SG_vhash* pvh_list = NULL;

        b = SG_FALSE;
        SG_ERR_CHECK(  SG_vhash__check__vhash(pCtx, pvh_to_from, bFrom ? "from" : "to", &b, &pvh_list)  );
        if (pvh_list)
        {
            b = SG_FALSE;

            SG_ERR_CHECK(  SG_vhash__check__varray(pCtx, pvh_list, psz_recid, &b, &pva)  );
        }
    }

    *ppva_my_links = pva;

fail:
    ;
}

static void _fillInLinks(
	SG_context *pCtx, 
    SG_bool bFrom,
    SG_vhash* pvh_all_links,
	SG_zingtemplate *pzt,
	SG_rbtree *prb_link_names,
	SG_vhash *rec
    )
{
	const char *recid = NULL;
	SG_zinglinksideattributes *pzlsaFrom = NULL;
	SG_zinglinksideattributes *pzlsaTo = NULL;
	SG_zinglinkattributes *pzla = NULL;
	SG_rbtree_iterator *it = NULL;
	SG_bool ok = SG_FALSE;
	const char *linkname = NULL;
    const char* psz_side_name = NULL;

	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, rec, "recid", &recid)  );

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &it, prb_link_names, &ok, &linkname, (void **)&psz_side_name)  );

	while (ok)
	{
        SG_varray* pva_my_links = NULL;

		SG_ERR_CHECK(  my_find_links(pCtx, bFrom, pvh_all_links, linkname, recid, &pva_my_links)  );

        if (pva_my_links)
        {
            SG_zingtemplate__get_link_attributes(pCtx, pzt, linkname, &pzla, &pzlsaFrom, &pzlsaTo);
            _addLinks(pCtx, rec, psz_side_name, pzlsaFrom->bSingular, pva_my_links);
        }

		SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaFrom);
		SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaTo);
		SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, it, &ok, &linkname, (void **)&psz_side_name)  );
	}

	SG_RBTREE_ITERATOR_NULLFREE(pCtx, it);

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, it);
	SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaFrom);
	SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsaTo);
	SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
}

// TODO this function retrieves every record in a dag.  very bad idea.
static void _dispatch__zing_dag_all_records(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    SG_rbtree * pLeaves = NULL;
    SG_uint32 leafCount = 0;
	SG_uint32 count = 0;
	SG_uint32 i = 0;
    const char * pBaseline = NULL;
	SG_string *results = NULL;
	SG_vhash * params = NULL;
	SG_bool found = SG_FALSE;
	SG_varray *pkeys = NULL;
	SG_string *arg = NULL;
    const char *searchStr = NULL;
	SG_zingtemplate *pzt = NULL;
	SG_stringarray *ztfields = NULL;
	SG_varray *zrecs = NULL;
	SG_string *qwhere = NULL;
	SG_rbtree *rectypes = NULL;
	SG_rbtree_iterator *it = NULL;
	const char *rectype = NULL;
	SG_bool ok = SG_FALSE;
	SG_varray *allrecs = NULL;
	const char *reqRectype = NULL;
	SG_rbtree *pFrom = NULL;
	SG_rbtree *pTo = NULL;
	SG_vhash *pvh_all_links = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

	if (pRequestHeaders->pQueryString)
	{
		SG_bool has = SG_FALSE;

		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &params)  );
		SG_ERR_CHECK(  SG_querystring_to_vhash(pCtx, pRequestHeaders->pQueryString, params)  );

		SG_ERR_CHECK(  SG_vhash__has(pCtx, params, "_", &has)  );

		if (has)
			SG_ERR_CHECK(  SG_vhash__remove(pCtx, params, "_")  );

        SG_ERR_CHECK(  SG_vhash__has(pCtx, params, "q", &has)  );

        if (has)
        {
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, params, "q", &searchStr)  );
            SG_ERR_CHECK(  SG_vhash__remove(pCtx, params, "q")  );
        }

		SG_ERR_CHECK(  SG_vhash__count(pCtx, params, &count)  );

		found = (count > 0);
	}

	if (found)
	{
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &qwhere)  );
		SG_ERR_CHECK(  SG_vhash__get_keys(pCtx, params, &pkeys)  );
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &arg)  );

		for ( i = 0; i < count; ++i )
		{
			const char *key = NULL;
			const char *val = NULL;

			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pkeys, i, &key)  );
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, params, key, &val)  );

			if (eq(key, "rectype"))
			{
				reqRectype = val;
				continue;
			}

			if (strlen(SG_string__sz(qwhere)) > 0)
				SG_ERR_CHECK(  SG_string__append__sz(pCtx, qwhere, " && ")  );

			SG_ERR_CHECK(  SG_string__sprintf(pCtx, arg, "%s == '%s'", key, val)  );

			SG_ERR_CHECK(  SG_string__append__string(pCtx, qwhere, arg)  );
		}
	}

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, *ppRepo, dagnum, &pLeaves)  );
    SG_ERR_CHECK(  SG_rbtree__count(pCtx, pLeaves, &leafCount)  );
    if(leafCount==0)
        SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
    if(leafCount>1)
        SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED); //todo: ?

    SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, pLeaves, &pBaseline, NULL)  );

	SG_ERR_CHECK(  sg_zing__load_template__csid(pCtx, *ppRepo, pBaseline, &pzt)  ); 

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &allrecs)  );
	SG_ERR_CHECK(  SG_zingtemplate__list_rectypes(pCtx, pzt, &rectypes)  );

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &it, rectypes, &ok, &rectype, NULL)  );

	while (ok)
	{
		const char *wherestr = NULL;

		if ((reqRectype == NULL) || eq(rectype, reqRectype))
		{
			if (eq(rectype, "item") && (qwhere != NULL))
				wherestr = SG_string__sz(qwhere);

            SG_ERR_CHECK(  SG_stringarray__alloc(pCtx, &ztfields, 1)  );
			SG_ERR_CHECK(  SG_stringarray__add(pCtx, ztfields, "*")  );

			SG_ERR_CHECK(  SG_zing__query(pCtx, *ppRepo, dagnum, pBaseline, rectype, wherestr, NULL, 0, 0, ztfields, &zrecs)  );

			if ((searchStr != NULL) && eq(rectype, "item"))
			{
				SG_ERR_CHECK(  SG_varray__count(pCtx, zrecs, &count)  );

				i = count;

				while (i > 0)
				{
					SG_bool found = SG_FALSE;
					const SG_vhash *rec = NULL;
					SG_uint32 j = 0;
					SG_uint32 fcount = 0;

					--i;

					SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, zrecs, i, (SG_vhash **)&rec)  );

					SG_vhash__count(pCtx, rec, &fcount);

					for ( j = 0; (j < fcount) && (! found); ++j )
					{
						const char *key = NULL;
						const SG_variant *value = NULL;
				
						SG_vhash__get_nth_pair(pCtx, rec, j, &key, &value);
				
						if (value->type == SG_VARIANT_TYPE_SZ)
						{
							const char *sz = NULL;
							SG_variant__get__sz(pCtx, value, &sz);

							if (_strInField(searchStr, (const SG_byte *)sz, strlen(sz) * sizeof(char)))
							{
								found = SG_TRUE;
							}
						}
					}

					if (! found)
					{
						SG_ERR_CHECK(  SG_varray__remove(pCtx, zrecs, i)  );
					}
				}
			}

			SG_ERR_CHECK(  SG_varray__count(pCtx, zrecs, &count)  );

			if (eq(rectype, "item"))
			{
				SG_ERR_CHECK(  SG_zingtemplate__list_all_links(pCtx, pzt, rectype, &pFrom, &pTo)  ); 
                SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh_all_links)  );
                SG_ERR_CHECK(  SG_zing__find_all_links(pCtx, *ppRepo, dagnum, pBaseline, pFrom, pvh_all_links)  );
                SG_ERR_CHECK(  SG_zing__find_all_links(pCtx, *ppRepo, dagnum, pBaseline, pTo, pvh_all_links)  );

				for ( i = 0; i < count; ++i )
				{
					SG_vhash *rec = NULL;

					SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, zrecs, i, &rec)  );
					SG_ERR_CHECK(  _fillInLinks(pCtx, SG_TRUE, pvh_all_links, pzt, pFrom, rec)  );
					SG_ERR_CHECK(  _fillInLinks(pCtx, SG_FALSE, pvh_all_links, pzt, pTo, rec)  );
				}

				SG_RBTREE_NULLFREE(pCtx, pFrom);
				SG_RBTREE_NULLFREE(pCtx, pTo);
                SG_VHASH_NULLFREE(pCtx, pvh_all_links);
			}

			SG_ERR_CHECK(  SG_varray__copy_items(pCtx, zrecs, allrecs)  );
			SG_VARRAY_NULLFREE(pCtx, zrecs);
			SG_STRINGARRAY_NULLFREE(pCtx, ztfields);
		}

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, it, &ok, &rectype, NULL)  );
	}

	SG_STRING__ALLOC(pCtx, &results);
	SG_varray__to_json(pCtx, allrecs, results);
	SG_VARRAY_NULLFREE(pCtx, allrecs);

	SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &results, ppResponseHandle)  );

	results = NULL;

fail:
	SG_RBTREE_NULLFREE(pCtx, pFrom);
	SG_RBTREE_NULLFREE(pCtx, pTo);
	SG_ZINGTEMPLATE_NULLFREE(pCtx, pzt);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, it);
	SG_RBTREE_NULLFREE(pCtx, rectypes);
	SG_STRINGARRAY_NULLFREE(pCtx, ztfields);
	SG_STRING_NULLFREE(pCtx, qwhere);
    SG_VARRAY_NULLFREE(pCtx, zrecs);
	SG_VARRAY_NULLFREE(pCtx, pkeys);
	SG_VHASH_NULLFREE(pCtx, params);
	SG_STRING_NULLFREE(pCtx, arg);
	SG_STRING_NULLFREE(pCtx, results);
    SG_RBTREE_NULLFREE(pCtx, pLeaves);
	SG_REPO_NULLFREE(pCtx, *ppRepo);
}

static void _dispatch__zing_dag_specific_records(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    SG_rbtree * pLeaves = NULL;
    SG_varray * pQuery = NULL;
    SG_uint32 numCurrentHids = 0; // If the record exists, this should come up 1... Duh! :-P
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva_query_result = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"POST"))
            SG_ERR_CHECK_RETURN(  _POST__zing_record(pCtx, pRequestHeaders, ppRepo, dagnum, ppRequestHandle)  );
		else if (eq(pRequestHeaders->pRequestMethod, "GET"))
        {
            if(pRequestHeaders->accept == SG_contenttype__json)
                SG_ERR_CHECK_RETURN(  _dispatch__zing_dag_all_records(pCtx, pRequestHeaders, ppRepo, dagnum, ppRequestHandle, ppResponseHandle)  );
            else
            {
                SG_REPO_NULLFREE(pCtx, *ppRepo);
				SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "listwi.xhtml", _new_wit_replacer, ppResponseHandle)  );
            }
        }
        else
            SG_ERR_THROW(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    }
	else if ((uriSubstringsCount == 1) && eq(ppUriSubstrings[0],"new"))
	{
		if(eq(pRequestHeaders->pRequestMethod,"GET"))
        {
			SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "newwi.xhtml", _new_wit_replacer, ppResponseHandle)  );
        }
        else
            SG_ERR_THROW(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
	}
    else
    {
        SG_uint32 leafCount = 0;
        const char * pBaseline = NULL;
        const char * pCurrentHidForReal = NULL;

        SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, *ppRepo, dagnum, &pLeaves)  );
        SG_ERR_CHECK(  SG_rbtree__count(pCtx, pLeaves, &leafCount)  );
        if(leafCount==0)
            SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
        if(leafCount>1)
            SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED); //todo: ?

        SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, pLeaves, &pBaseline, NULL)  );

        SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pQuery)  );
        SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pQuery, SG_ZING_FIELD__RECID)  );
        SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pQuery, "==")  );
        SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pQuery, ppUriSubstrings[0])  );
        SG_ERR_CHECK(  SG_stringarray__alloc(pCtx, &psa_fields, 1)  );
        SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "_rechid")  );

        SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, *ppRepo, dagnum, pBaseline, pQuery, psa_fields, &pva_query_result)  );
        SG_VARRAY_NULLFREE(pCtx, pQuery);
        if(pva_query_result==NULL)
            SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);

        SG_ERR_CHECK(  SG_varray__count(pCtx, pva_query_result, &numCurrentHids )  );
        if(numCurrentHids==0)
            SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);

        SG_ASSERT(numCurrentHids==1);
        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_query_result, 0, &pCurrentHidForReal)  );

        SG_ERR_CHECK( _dispatch__zing_record_specific(pCtx, pRequestHeaders, ppRepo, dagnum, ppUriSubstrings[0], pBaseline, pCurrentHidForReal, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
        SG_RBTREE_NULLFREE(pCtx, pLeaves); // Waited until now to free this since it owns pBaseline.

    }

fail:
	SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva_query_result);
    SG_VARRAY_NULLFREE(pCtx, pQuery);
    SG_RBTREE_NULLFREE(pCtx, pLeaves);
	SG_REPO_NULLFREE(pCtx, *ppRepo);
}

// -- _POST__zing_rectype_specific -- //
    typedef struct
    {
        SG_repo * pRepo;
        SG_zingtx * pZingTx;
        SG_string * pRectype;
        SG_string * pRequestBody;
    } _uri__zing_rectype_specific_POST__context;

    static void _uri__zing_rectype_specific_POST__context__alloc(
        SG_context * pCtx,
        _uri__zing_rectype_specific_POST__context ** ppNew,
        SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
        SG_zingtx ** ppZingTx, // On success we've taken ownership and nulled the caller's copy.
        SG_string ** ppRectype, // On success we've taken ownership and nulled the caller's copy.
        SG_string ** ppRequestBody) // On success we've taken ownership and nulled the caller's copy.
    {
        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(ppNew);
        SG_ASSERT(*ppNew==NULL);
        SG_NULL_PP_CHECK_RETURN(ppRepo);
        SG_NULL_PP_CHECK_RETURN(ppZingTx);
        SG_NULL_PP_CHECK_RETURN(ppRectype);
        SG_NULL_PP_CHECK_RETURN(ppRequestBody);

        SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, *ppNew)  );
        (*ppNew)->pRepo = *ppRepo;
        *ppRepo = NULL;
        (*ppNew)->pZingTx = *ppZingTx;
        *ppZingTx = NULL;
        (*ppNew)->pRectype = *ppRectype;
        *ppRectype = NULL;
        (*ppNew)->pRequestBody = *ppRequestBody;
        *ppRequestBody = NULL;
    }

    static void _uri__zing_rectype_specific_POST__context__free(
        SG_context * pCtx,
        _uri__zing_rectype_specific_POST__context * p)
    {
        if(p==NULL)
            return;
        SG_REPO_NULLFREE(pCtx,p->pRepo);
        SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &p->pZingTx)  );
        SG_STRING_NULLFREE(pCtx, p->pRectype);
        SG_STRING_NULLFREE(pCtx, p->pRequestBody);
        SG_NULLFREE(pCtx,p);
    }
#define _URI__ZING_RECTYPE_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx,p) SG_STATEMENT(SG_context__push_level(pCtx); _uri__zing_rectype_specific_POST__context__free(pCtx,p); SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL;)

    static void _uri__zing_rectype_specific_POST__chunk(SG_context * pCtx, _response_handle ** ppResponseHandle, SG_byte * pBuffer, SG_uint32 bufferLength, void * pContext__voidp)
    {
        _uri__zing_rectype_specific_POST__context * pContext = (_uri__zing_rectype_specific_POST__context*)pContext__voidp;

        SG_UNUSED(ppResponseHandle);

        SG_NULLARGCHECK_RETURN(pContext);
        SG_ARGCHECK_RETURN(pContext->pRequestBody!=NULL, pContext);

        SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, pContext->pRequestBody, pBuffer, bufferLength)  );
    }

    static void _uri__zing_rectype_specific_POST__finish(SG_context * pCtx, const SG_audit * pAudit, void * pContext__voidp, _response_handle ** ppResponseHandle)
    {
        _uri__zing_rectype_specific_POST__context * pContext = (_uri__zing_rectype_specific_POST__context*)pContext__voidp;

        SG_zingrecord * pZingRecord = NULL;
        SG_vhash * pVhash = NULL;
        const char * pRecId = NULL;
        SG_string * pResponseBody = NULL;

        SG_changeset * pChangeset = NULL;
        SG_dagnode * pDagnode = NULL;

        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(pAudit);

        SG_NULLARGCHECK_RETURN(pContext);
        SG_ARGCHECK_RETURN(pContext->pZingTx!=NULL, pContext);
        SG_ARGCHECK_RETURN(pContext->pRectype!=NULL, pContext);
        SG_ARGCHECK_RETURN(pContext->pRequestBody!=NULL, pContext);
        //todo: pContext->pRepo is never explicitly used. Did we need to store it? (Ie, does pZingTx somehow depend on its persistence or anything?)

        SG_NULLARGCHECK_RETURN(ppResponseHandle);
        SG_ASSERT(*ppResponseHandle==NULL);

		SG_ERR_CHECK(  _createRecordFromJson(pCtx, pContext->pZingTx, SG_string__sz(pContext->pRectype), pContext->pRequestBody, &pZingRecord, &pRecId, NULL)  );
        SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pResponseBody, pRecId)  );

        SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, pAudit->when_int64, &pContext->pZingTx, &pChangeset, &pDagnode, NULL)  );

        SG_CHANGESET_NULLFREE(pCtx, pChangeset);
        SG_DAGNODE_NULLFREE(pCtx, pDagnode);

        SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__text, &pResponseBody, ppResponseHandle)  );
        _URI__ZING_RECTYPE_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx, pContext);

        return;
    fail:
        SG_VHASH_NULLFREE(pCtx, pVhash);
        SG_STRING_NULLFREE(pCtx, pResponseBody);
        SG_CHANGESET_NULLFREE(pCtx, pChangeset);
        SG_DAGNODE_NULLFREE(pCtx, pDagnode);

        _URI__ZING_RECTYPE_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx, pContext);
    }

    static void _uri__zing_rectype_specific_POST__aborted(SG_context * pCtx, void * pContext)
    {
        _URI__ZING_RECTYPE_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx, pContext);
    }

static void _POST__zing_rectype_specific(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_zingtx ** ppZingTx, // On success we've taken ownership and nulled the caller's copy.
    const char * pRectype,
    _request_handle ** ppRequestHandle)
{
    SG_string * pRectypeStr = NULL;
    SG_string * pRequestBody = NULL;
    _uri__zing_rectype_specific_POST__context * pContext = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULL_PP_CHECK_RETURN(ppZingTx);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    if(pRequestHeaders->pFrom==NULL || pRequestHeaders->pFrom[0]=='\0')
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_401_UNAUTHORIZED);

    if(pRequestHeaders->contentLength==0)
        SG_ERR_THROW_RETURN(SG_ERR_URI_POSTDATA_MISSING);
    if(pRequestHeaders->contentLength>SG_STRING__MAX_LENGTH_IN_BYTES)
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_413_REQUEST_ENTITY_TOO_LARGE);

    SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pRectypeStr, pRectype)  );
    SG_ERR_CHECK(  SG_STRING__ALLOC__RESERVE(pCtx, &pRequestBody, (SG_uint32)pRequestHeaders->contentLength)  );

    SG_ERR_CHECK(  _uri__zing_rectype_specific_POST__context__alloc(pCtx, &pContext, ppRepo, ppZingTx, &pRectypeStr, &pRequestBody)  );

    SG_ERR_CHECK(  _request_handle__alloc(pCtx, ppRequestHandle, pRequestHeaders,
        _uri__zing_rectype_specific_POST__chunk,
        _uri__zing_rectype_specific_POST__finish,
        _uri__zing_rectype_specific_POST__aborted,
        pContext)  );

    return;
fail:
    SG_STRING_NULLFREE(pCtx, pRectypeStr);
    SG_STRING_NULLFREE(pCtx, pRequestBody);
    _URI__ZING_RECTYPE_SPECIFIC_POST__CONTEXT__NULLFREE(pCtx, pContext);
}

static void _dispatch__zing_rectype_specific(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_zingtx ** ppZingTx, // On success we've taken ownership and nulled the caller's copy.
    const char * pRectype,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULL_PP_CHECK_RETURN(ppZingTx);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"POST"))
            SG_ERR_CHECK_RETURN(  _POST__zing_rectype_specific(pCtx, pRequestHeaders, ppRepo, ppZingTx, pRectype, ppRequestHandle)  );
        else
            SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    }
    else
    {
        //todo
        SG_UNUSED(ppResponseHandle);
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);
    }
}

void _dispatch__zing_dag_specific_rectypes(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    SG_rbtree * pLeaves = NULL;
    SG_zingtx * pZingTx = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
        SG_ERR_THROW(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    else
    {
        SG_uint32 leafCount = 0;
        const char * pBaseline = NULL;
        SG_zingtemplate * pTemplate = NULL;
        SG_vhash * pVhash = NULL;
        SG_audit audit;

        SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, *ppRepo, dagnum, &pLeaves)  );
        SG_ERR_CHECK(  SG_rbtree__count(pCtx, pLeaves, &leafCount)  );
        if(leafCount==0)
            SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
        if(leafCount>1)
            SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED); //todo: ?

        SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, pLeaves, &pBaseline, NULL)  );
        // TODO REVIEW  Eric isn't sure the following code is correct
        if (pRequestHeaders->pFrom)
        {
            SG_ERR_CHECK(  SG_audit__init(pCtx, &audit, *ppRepo, SG_AUDIT__WHEN__NOW, pRequestHeaders->pFrom)  );
        }
        else
        {
            SG_ERR_CHECK(  SG_audit__init(pCtx, &audit, *ppRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
        }
        SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, *ppRepo, dagnum, audit.who_szUserId, pBaseline, &pZingTx)  );
        if (pBaseline)
        {
            SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pZingTx, pBaseline)  );
        }
        SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pZingTx, &pTemplate)  );
        SG_ERR_CHECK(  SG_zingtemplate__get_vhash(pCtx, pTemplate, &pVhash)  );
        SG_vhash__get__vhash(pCtx, pVhash, "rectypes", &pVhash);
        if(SG_context__has_err(pCtx))
            SG_ERR_RESET_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);

        SG_vhash__get__vhash(pCtx, pVhash, ppUriSubstrings[0], &pVhash);
        if(SG_context__has_err(pCtx))
            SG_ERR_RESET_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);

        SG_ERR_CHECK(  _dispatch__zing_rectype_specific(pCtx, pRequestHeaders, ppRepo, &pZingTx, ppUriSubstrings[0], ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );

        SG_RBTREE_NULLFREE(pCtx, pLeaves);
    }

    return;
fail:
    SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pZingTx)  );
    SG_RBTREE_NULLFREE(pCtx, pLeaves);
}

static void _GET__zing_dag_specific_template(
    SG_context * pCtx,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    _response_handle ** ppResponseHandle)
{
    SG_rbtree * pLeaves = NULL;
    SG_uint32 leafCount = 0;
    SG_string * pString = NULL;
    SG_changeset* pcs = NULL;
    SG_vhash* pvh_dbtop = NULL;
    const char* psz_hid_dbtop = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, *ppRepo, dagnum, &pLeaves)  );
    SG_ERR_CHECK(  SG_rbtree__count(pCtx, pLeaves, &leafCount)  );

    if(leafCount==0)
    {
        SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pString, "null")  );
        SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &pString, ppResponseHandle)  );
    }
    else if(leafCount>1)
        SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED); //todo: ?
    else
    {
        const char * pBaseline = NULL;
        const char * pCurrentHidForReal = NULL;

        SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, pLeaves, &pBaseline, NULL)  );

        SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, *ppRepo, pBaseline, &pcs)  );

        SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs,&psz_hid_dbtop)  );
        if (!psz_hid_dbtop)
        {
            SG_ERR_THROW(  SG_ERR_INVALIDARG  );
        }
        SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, *ppRepo, psz_hid_dbtop, &pvh_dbtop)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_dbtop, "template", &pCurrentHidForReal)  );

        SG_ERR_CHECK(  _create_response_handle_for_blob(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, ppRepo, pCurrentHidForReal, ppResponseHandle)  );
    }

    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_VHASH_NULLFREE(pCtx, pvh_dbtop);
    SG_RBTREE_NULLFREE(pCtx, pLeaves);

    return;
fail:
    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_VHASH_NULLFREE(pCtx, pvh_dbtop);
    SG_STRING_NULLFREE(pCtx, pString);
    SG_RBTREE_NULLFREE(pCtx, pLeaves);
}

static void _dispatch__zing_dag_specific_template(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    //_request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    //SG_NULLARGCHECK_RETURN(ppRequestHandle);
    //SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"GET"))
            SG_ERR_CHECK_RETURN(  _GET__zing_dag_specific_template(pCtx, ppRepo, dagnum, ppResponseHandle)  );
        else
            SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    }
    else
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);
}

    typedef struct
    {
        SG_repo * pRepo;
        SG_uint32 dagnum;
        SG_string * pRequestBody;
    } _uri__zing_dag_specific_link_PUT__context;




struct linkageInfo
{
	SG_zingtx *ztx;
	SG_varray *linkage;
};

static void _linkCallback(
	SG_context * pCtx,
	const char * key,
	void * pVoidAssoc,
	void * pVoidData )
{
	struct linkageInfo *li = (struct linkageInfo *)pVoidData;
	char	buf_recid_from[SG_GID_BUFFER_LENGTH];
	char	buf_recid_to[SG_GID_BUFFER_LENGTH];
	SG_vhash *data = NULL;
	SG_zingrecord *zrec = NULL;
	SG_dbrecord *rec = NULL;

	SG_UNUSED(pVoidAssoc);

    SG_ERR_CHECK_RETURN(  SG_zinglink__unpack(pCtx, key, buf_recid_from, buf_recid_to, NULL)  );

	SG_ERR_CHECK_RETURN(  SG_zingtx__get_record(pCtx, li->ztx, buf_recid_from, &zrec)  );
	SG_ERR_CHECK(  SG_zingrecord__to_dbrecord(pCtx, zrec, &rec)  );

	SG_ERR_CHECK(  SG_dbrecord__to_vhash(pCtx, rec, &data)  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, li->linkage, &data)  );

fail:
	SG_VHASH_NULLFREE(pCtx, data);
	SG_DBRECORD_NULLFREE(pCtx, rec);
}

static void _dispatch__zing_dag_specific_links(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
	SG_vhash * params = NULL;
	SG_bool found = SG_FALSE;
	const char *linkname = NULL;
	const char *toid = NULL;
	SG_rbtree *links = NULL;
	SG_zingtx *pZingTx = NULL;
	SG_rbtree *pLeaves = NULL;
	SG_uint32 leafCount = 0;
	const char *pBaseline = NULL;
	SG_zingrecord *zr = NULL;
    SG_zingfieldattributes* pzfa = NULL;
    SG_zinglinkattributes* pzla = NULL;
    SG_zinglinksideattributes* pzlsa_mine = NULL;
    SG_zinglinksideattributes* pzlsa_other = NULL;
	SG_varray *linkage = NULL;
	SG_string *linkstr = NULL;
	struct linkageInfo li;
    SG_audit audit;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

	SG_UNUSED(ppRequestHandle);
	SG_UNUSED(uriSubstringsCount);

	if (uriSubstringsCount > 0)
		SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);

    if(! eq(pRequestHeaders->pRequestMethod,"GET"))
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);

	SG_VHASH__ALLOC(pCtx, &params);
	SG_ERR_CHECK(  SG_querystring_to_vhash(pCtx, pRequestHeaders->pQueryString, params)  );

	SG_ERR_CHECK(  SG_vhash__has(pCtx, params, "linkname", &found)  );
	if (! found)
		SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
	SG_ERR_CHECK(  SG_vhash__has(pCtx, params, "to", &found)  );
	if (! found)
		SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);

	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, params, "linkname", &linkname)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, params, "to", &toid)  );

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, *ppRepo, dagnum, &pLeaves)  );
    SG_ERR_CHECK(  SG_rbtree__count(pCtx, pLeaves, &leafCount)  );
    if(leafCount==0)
        SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND); // Should never happen, but just in case, right?
    if(leafCount>1)
        SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED); //todo: ?

    SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, pLeaves, &pBaseline, NULL)  );
    // TODO REVIEW  Eric isn't sure the following code is correct
    if (pRequestHeaders->pFrom)
    {
        SG_ERR_CHECK(  SG_audit__init(pCtx, &audit, *ppRepo, SG_AUDIT__WHEN__NOW, pRequestHeaders->pFrom)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_audit__init(pCtx, &audit, *ppRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
    }
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, *ppRepo, dagnum, audit.who_szUserId, pBaseline, &pZingTx)  );
    if (pBaseline)
    {
        SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pZingTx, pBaseline)  );
    }
    SG_RBTREE_NULLFREE(pCtx, pLeaves);

	SG_zingtx__get_record(pCtx, pZingTx, toid, &zr);

    SG_ERR_CHECK(  SG_zingrecord__lookup_name(pCtx, zr, linkname, &pzfa, &pzla, &pzlsa_mine, &pzlsa_other)  );

	if (! pzla)
	{
        SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND); // Should never happen, but just in case, right?
	}

	SG_ERR_CHECK(  SG_zingtx___find_links(pCtx, pZingTx,
		pzla, toid, SG_FALSE, &links)  );

	// on empty, return an empty container, not 404

	SG_VARRAY__ALLOC(pCtx, &linkage);
	li.linkage = linkage;
	li.ztx = pZingTx;
	SG_rbtree__foreach(pCtx, links, _linkCallback, (void *)&li);

	SG_STRING__ALLOC(pCtx, &linkstr);
	SG_ERR_CHECK(  SG_varray__to_json(pCtx, linkage, linkstr)  );

	_create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json,
		&linkstr, ppResponseHandle);
	linkstr = NULL;

fail:
	SG_STRING_NULLFREE(pCtx, linkstr);
	SG_VARRAY_NULLFREE(pCtx, linkage);
	if (pZingTx != NULL)
		SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pZingTx)  );
	SG_VHASH_NULLFREE(pCtx, params);
	SG_RBTREE_NULLFREE(pCtx, pLeaves);
	SG_RBTREE_NULLFREE(pCtx, links);
    SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_mine);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_other);

}

static void _dispatch__zing_dag_specific(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_ARGCHECK_RETURN(SG_DAGNUM__IS_DB(dagnum), dagnum);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    else if(eq(ppUriSubstrings[0],"records" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__zing_dag_specific_records(pCtx, pRequestHeaders, ppRepo, dagnum, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
    else if(eq(ppUriSubstrings[0],"template" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__zing_dag_specific_template(pCtx, pRequestHeaders, ppRepo, dagnum, ppUriSubstrings+1, uriSubstringsCount-1, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"links"))
		SG_ERR_CHECK_RETURN(  _dispatch__zing_dag_specific_links(pCtx, pRequestHeaders, ppRepo, dagnum, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
    else
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);
}

/**
 *	if it's a GET with no further parameters, list all activity.  otherwise
 *  hand off to the normal zing_dag_specific workflow
 */
static void _dispatch__zing_dag_specific_activity(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    SG_uint32 dagnum,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    if ((uriSubstringsCount==0) && eq(pRequestHeaders->pRequestMethod,"GET"))
	{
		_dispatch__activity_stream(pCtx, pRequestHeaders, ppRepo, ppResponseHandle);
	}
	else
	{
		_dispatch__zing_dag_specific(pCtx, pRequestHeaders, ppRepo, dagnum, ppUriSubstrings, uriSubstringsCount, ppRequestHandle, ppResponseHandle);
	}
}

static void _GET__tag_json(SG_context * pCtx, SG_repo* pRepo, _response_handle ** ppResponseHandle)
{
	SG_string *content = NULL;
	SG_varray* pva_tags = NULL;

	SG_ERR_CHECK(  SG_vc_tags__list_all(pCtx, pRepo, &pva_tags)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &content)  );
	SG_ERR_CHECK(  SG_varray__to_json(pCtx, pva_tags, content)  );

	_create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &content, ppResponseHandle);

fail:
	SG_VARRAY_NULLFREE(pCtx, pva_tags);
	SG_NULLFREE(pCtx, content);
}

static void _tag_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement)
{
	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "TITLE"))
	{
		SG_ERR_CHECK(  SG_string__set__sz(pCtx, replacement, "veracity / tags")  );
	}

fail:
	;
}

void _dispatch__tags(SG_context * pCtx,
    _request_headers * pRequestHeaders,
	SG_repo ** ppRepo,
    _response_handle ** ppResponseHandle)
{

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

     if(eq(pRequestHeaders->pRequestMethod,"GET"))
	 {
        if(pRequestHeaders->accept==SG_contenttype__json)
		{
            SG_ERR_CHECK(  _GET__tag_json(pCtx, *ppRepo, ppResponseHandle)  );
		}
		else
		{
			SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "tags.xhtml", _tag_replacer, ppResponseHandle)  );
		}
	 }
    else
         SG_ERR_THROW(  SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED  );

fail:
	;
}

static void _GET__stamp_json(SG_context * pCtx, SG_repo* pRepo, _response_handle ** ppResponseHandle)
{
	SG_string *content = NULL;
	SG_varray* pva_stamps = NULL;

	SG_ERR_CHECK(  SG_vc_stamps__list_all_stamps(pCtx, pRepo, &pva_stamps)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &content)  );
	SG_ERR_CHECK(  SG_varray__to_json(pCtx, pva_stamps, content)  );

	_create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &content, ppResponseHandle);

fail:
	SG_VARRAY_NULLFREE(pCtx, pva_stamps);
	SG_NULLFREE(pCtx, content);
}



void _dispatch__stamps(SG_context * pCtx,
    _request_headers * pRequestHeaders,
	SG_repo ** ppRepo,
	_response_handle ** ppResponseHandle)
{

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

     if(eq(pRequestHeaders->pRequestMethod,"GET"))
	 {
        if(pRequestHeaders->accept==SG_contenttype__json)
		{
            SG_ERR_CHECK(  _GET__stamp_json(pCtx, *ppRepo, ppResponseHandle)  );
		}
		else
		{
			 SG_ERR_THROW(  SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED  );
		}
	 }
    else
         SG_ERR_THROW(  SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED  );

fail:
	;
}

static void _local_index_html_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement)
{
	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "TITLE"))
		SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, replacement, "veracity")  );
}


static void _dispatch__home(SG_context * pCtx,
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"GET"))
            SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "index.xhtml", _local_index_html_replacer, ppResponseHandle)  );
		else
			SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    }
    else
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);
}


static void _GET_repo(SG_context* pCtx,
					  SG_repo** ppRepo,
					  _response_handle** ppResponseHandle)
{
	SG_repo* pRepo = *ppRepo;

	char* pszRepoId = NULL;
	char* pszAdminId = NULL;
	char* pszHashMethod = NULL;

	SG_vhash* pvhResponse = NULL;
	SG_string* pstrResponse = NULL;
	_response_handle* pResponseHandle = NULL;

	SG_ERR_CHECK_RETURN(  SG_server__get_repo_info(pCtx, pRepo, &pszRepoId, &pszAdminId, &pszHashMethod)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhResponse)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhResponse, "RepoID", pszRepoId)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhResponse, "AdminID", pszAdminId)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhResponse, "HashMethod", pszHashMethod)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC__RESERVE(pCtx, &pstrResponse, 256)  );
	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhResponse, pstrResponse)  );

	SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &pstrResponse, &pResponseHandle)  );

	SG_REPO_NULLFREE(pCtx, pRepo);
	*ppRepo = NULL;
	SG_RETURN_AND_NULL(pResponseHandle, ppResponseHandle);

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszRepoId);
	SG_NULLFREE(pCtx, pszAdminId);
	SG_NULLFREE(pCtx, pszHashMethod);
	SG_VHASH_NULLFREE(pCtx, pvhResponse);
	SG_STRING_NULLFREE(pCtx, pstrResponse);
	_response_handle__nullfree(pCtx, &pResponseHandle);
}

static void _local_log_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement)
{
	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "TITLE"))
		SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, replacement, "veracity / log")  );
}


static void _dispatch__repo_specific(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
	const char* pszRepoDescriptorName,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULL_PP_CHECK_RETURN(ppRepo);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
	{
		if (eq(pRequestHeaders->pRequestMethod, "GET"))
			SG_ERR_CHECK_RETURN(  _GET_repo(pCtx, ppRepo, ppResponseHandle)  );
		else
			SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
	}
    else if(eq(ppUriSubstrings[0],"changesets" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__repo_specific_changesets(pCtx, pRequestHeaders, ppRepo, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"blobs" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__repo_specific_blobs(pCtx, pRequestHeaders, ppRepo, SG_FALSE, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"zip" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__repo_zip(pCtx, pRequestHeaders, ppRepo, pszRepoDescriptorName, ppUriSubstrings+1, uriSubstringsCount-1,  ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"diff" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__repo_specific_diff(pCtx, pRequestHeaders, ppRepo, ppUriSubstrings+1, uriSubstringsCount-1, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"blobs-download" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__repo_specific_blobs(pCtx, pRequestHeaders, ppRepo, SG_TRUE, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"history" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__history(pCtx, pRequestHeaders, ppRepo, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"withistory" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__withistory(pCtx, pRequestHeaders, ppRepo, ppUriSubstrings+1, uriSubstringsCount-1, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"burndown" ) )
        SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "burndown.xhtml",  _burndown_replacer, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"todo"))
        SG_ERR_CHECK_RETURN(  _dispatch__todo(pCtx, pRequestHeaders, ppRepo, ppUriSubstrings+1, uriSubstringsCount-1, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"log"))
        SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "log.xhtml",  _local_log_replacer, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"tags" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__tags(pCtx, pRequestHeaders, ppRepo, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"stamps" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__stamps(pCtx, pRequestHeaders, ppRepo, ppResponseHandle)  );
    else if(eq(ppUriSubstrings[0],"wiki" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__zing_dag_specific(pCtx, pRequestHeaders, ppRepo, SG_DAGNUM__WIKI, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
    else if(eq(ppUriSubstrings[0],"users" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__zing_dag_specific(pCtx, pRequestHeaders, ppRepo, SG_DAGNUM__USERS, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
    else if(eq(ppUriSubstrings[0],"wit" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__repo_specific_wit(pCtx, pRequestHeaders, ppRepo, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
    else if(eq(ppUriSubstrings[0],"activity" ) )
        SG_ERR_CHECK_RETURN(  _dispatch__zing_dag_specific_activity(pCtx, pRequestHeaders, ppRepo, SG_DAGNUM__SUP, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"sync" ) )
		SG_ERR_CHECK_RETURN(  _dispatch__sync(pCtx, pRequestHeaders, pszRepoDescriptorName, ppRepo, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
	else if (eq(ppUriSubstrings[0],"home"))
        SG_ERR_CHECK_RETURN(  _dispatch__home(pCtx, pRequestHeaders, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
    else
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);
}


static void _GET__repos(
    SG_context * pCtx,
    _response_handle ** ppResponseHandle)
{
    SG_vhash * pDescriptors = NULL;
    SG_varray * pDescriptorNames = NULL;
    SG_string * pString = NULL;
	SG_vhash * pResult = NULL;
	SG_repo* pRepo = NULL;
	SG_rbtree * prbLeaves = NULL;
	SG_rbtree_iterator * pIter = NULL;
	const char * pszHid_k = NULL;
	SG_uint32 count;
	SG_uint32 i;
	SG_bool bFound;
	SG_varray* pvaHids = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

	SG_VHASH__ALLOC(pCtx, &pResult);

    SG_ERR_CHECK(  SG_closet__descriptors__list(pCtx, &pDescriptors)  );
    SG_ERR_CHECK(  SG_vhash__get_keys(pCtx, pDescriptors, &pDescriptorNames)  );
	SG_ERR_CHECK(  SG_varray__count(pCtx, pDescriptorNames, &count)  );

	//get the heads so we can display the correct .zip links
	for (i=0; i<count; i++)
	{
		const char* pszRepoName = NULL;

		SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pDescriptorNames, i, &pszRepoName)  );

		// Tolerate malformed descriptors primarily because the test suite adds some.
		SG_repo__open_repo_instance(pCtx, pszRepoName,  &pRepo);
		if (SG_context__has_err(pCtx))
		{
			SG_ERR_DISCARD; // TODO: be more specific here, rather than disregarding *any* error.
			continue;
		}

		SG_VARRAY__ALLOC(pCtx, &pvaHids);
		SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, &prbLeaves)  );
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, prbLeaves, &bFound, &pszHid_k, NULL)  );

		while (bFound)
		{
			SG_varray__append__string__sz(pCtx, pvaHids, pszHid_k);

			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bFound,&pszHid_k,NULL)  );

		}
		SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pResult, pszRepoName, &pvaHids)  );
		SG_VARRAY_NULLFREE(pCtx, pvaHids);
		SG_REPO_NULLFREE(pCtx, pRepo);
		SG_RBTREE_NULLFREE(pCtx, prbLeaves);
		SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);

	}

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );
    SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pResult, pString)  );

    SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &pString, ppResponseHandle)  );

fail:
    SG_VARRAY_NULLFREE(pCtx, pDescriptorNames);
    SG_VHASH_NULLFREE(pCtx, pDescriptors);
	SG_VHASH_NULLFREE(pCtx, pResult);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_RBTREE_NULLFREE(pCtx, prbLeaves);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
	SG_STRING_NULLFREE(pCtx, pString);
	SG_VARRAY_NULLFREE(pCtx, pvaHids);
}

static void _dispatch__repo(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    SG_repo * pRepo = NULL;
	SG_pathname * pPathCwd = NULL;
	SG_string * pstrRepoDescriptorName = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
    {
        SG_ERR_THROW(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    }
    else
    {

		SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
		SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );
		SG_ERR_CHECK(  SG_workingdir__find_mapping(pCtx, pPathCwd, NULL, &pstrRepoDescriptorName, NULL)  );
        SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, SG_string__sz(pstrRepoDescriptorName), &pRepo)  );
        SG_ERR_CHECK(  _dispatch__repo_specific(pCtx, pRequestHeaders, SG_string__sz(pstrRepoDescriptorName), &pRepo, ppUriSubstrings, uriSubstringsCount, ppRequestHandle, ppResponseHandle)  );

	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
    SG_REPO_NULLFREE(pCtx, pRepo);
    SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
}

static void _repos_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement)
{
	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "TITLE"))
	{
		//TODO put CSID in title?
		SG_ERR_CHECK(  SG_string__set__sz(pCtx, replacement, "veracity / repos")  );
	}

fail:
	;
}

static void _dispatch__repos(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    SG_repo * pRepo = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"GET"))
		{
            if(pRequestHeaders->accept==SG_contenttype__json)
                SG_ERR_CHECK(  _GET__repos(pCtx, ppResponseHandle)  );
            else
                SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "repos.xhtml", _repos_replacer, ppResponseHandle)  );
		}

        else
            SG_ERR_THROW(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    }
    else
    {
		SG_repo__open_repo_instance(pCtx, ppUriSubstrings[0], &pRepo);
		if(pRepo==NULL || SG_context__has_err(pCtx))
			SG_ERR_RESET_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
        else
			SG_ERR_CHECK(  _dispatch__repo_specific(pCtx, pRequestHeaders, ppUriSubstrings[0], &pRepo, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
    }

fail:
    SG_REPO_NULLFREE(pCtx, pRepo);
}

// -- _PUT__local -- //
    static void _uri__local_PUT__commit(SG_context * pCtx, const SG_audit * pAudit, SG_varray * pPaths, _response_handle **ppResponseHandle)
    {
        SG_pathname* pPathCwd = NULL;
        SG_pendingtree* pPendingTree = NULL;
        const char ** ppPaths = NULL;
        SG_uint32 numPaths = 0;
        SG_uint32 i;
        SG_dagnode * pDagnode = NULL;
        const char * pDagnodeId = NULL;
        SG_string * pResponseBody = NULL;

        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(pAudit);
        SG_NULLARGCHECK_RETURN(pPaths);
        SG_NULLARGCHECK_RETURN(ppResponseHandle);
        SG_ASSERT(*ppResponseHandle==NULL);

        SG_ERR_CHECK(  SG_varray__count(pCtx, pPaths, &numPaths)  );
        if(numPaths<1)
            SG_ERR_THROW(SG_ERR_URI_HTTP_400_BAD_REQUEST);
        SG_ERR_CHECK(  SG_allocN(pCtx, numPaths, ppPaths)  );
        for(i=0;i<numPaths;++i)
        {
            const SG_variant * pPath = NULL;
            SG_ERR_CHECK(  SG_varray__get__variant(pCtx, pPaths, i, &pPath)  );
            if(pPath->type==SG_VARIANT_TYPE_SZ && pPath->v.val_sz!=NULL && pPath->v.val_sz[0]=='@')
                ppPaths[i] = pPath->v.val_sz + 2; //todo: ***"+2" IS TEMPORARY HACK SINCE PENDING TREE DOESN'T HANDLE ABSOLUTE PATHS***
            else
                SG_ERR_THROW(SG_ERR_URI_HTTP_400_BAD_REQUEST);
        }

        SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );

        SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

        SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, SG_TRUE, &pPendingTree)  ); //todo: passing in SG_TRUE for bIgnoreWarnigns ok?

        SG_ERR_CHECK(  SG_pendingtree__commit(pCtx,
											  pPendingTree,
											  pAudit,
											  pPathCwd,
											  numPaths, ppPaths,
											  SG_TRUE,				// bRecursive
											  NULL, 0,
											  NULL, 0,
											  NULL, 0,
											  &pDagnode)  );

        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

        SG_PATHNAME_NULLFREE(pCtx, pPathCwd);

        SG_NULLFREE(pCtx, ppPaths);

        SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pDagnode, &pDagnodeId)  );
        SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pResponseBody, pDagnodeId)  );
        SG_DAGNODE_NULLFREE(pCtx, pDagnode);
        SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__text, &pResponseBody, ppResponseHandle)  );

        return;

    fail:
        //if (pPendingTree && SG_context__err_equals(pCtx,SG_ERR_PORTABILITY_WARNINGS))
        //    SG_ERR_IGNORE(  sg_report_portability_warnings(pCtx,pPendingTree)  );
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
        SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
        SG_NULLFREE(pCtx, ppPaths);

        SG_STRING_NULLFREE(pCtx, pResponseBody);
        SG_DAGNODE_NULLFREE(pCtx, pDagnode);
    }
    static void _uri__local_PUT__revert(SG_context * pCtx, SG_varray * pPaths)
    {
        SG_pathname* pPathCwd = NULL;
        SG_pendingtree* pPendingTree = NULL;
        const char ** ppPaths = NULL;
        SG_uint32 numPaths = 0;
        SG_uint32 i;

        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(pPaths);

        SG_ERR_CHECK(  SG_varray__count(pCtx, pPaths, &numPaths)  );
        if(numPaths<1)
            SG_ERR_THROW(SG_ERR_URI_HTTP_400_BAD_REQUEST);
        SG_ERR_CHECK(  SG_allocN(pCtx, numPaths, ppPaths)  );
        for(i=0;i<numPaths;++i)
        {
            const SG_variant * pPath = NULL;
            SG_ERR_CHECK(  SG_varray__get__variant(pCtx, pPaths, i, &pPath)  );
            if(pPath->type==SG_VARIANT_TYPE_SZ && pPath->v.val_sz!=NULL && pPath->v.val_sz[0]=='@')
                ppPaths[i] = pPath->v.val_sz + 2; //todo: ***"+2" IS TEMPORARY HACK SINCE PENDING TREE DOESN'T HANDLE ABSOLUTE PATHS***
            else
                SG_ERR_THROW(SG_ERR_URI_HTTP_400_BAD_REQUEST);
        }

        SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );

        SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

        SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, SG_TRUE, &pPendingTree)  ); //todo: passing in SG_TRUE for bIgnoreWarnigns ok?

        SG_ERR_CHECK(  SG_pendingtree__revert(pCtx, pPendingTree, SG_PT_ACTION__DO_IT, pPathCwd, numPaths, ppPaths, SG_FALSE, NULL, 0, NULL, 0)  );

        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

        SG_PATHNAME_NULLFREE(pCtx, pPathCwd);

        SG_NULLFREE(pCtx, ppPaths);

        return;

    fail:
        //if (pPendingTree && SG_context__err_equals(pCtx,SG_ERR_PORTABILITY_WARNINGS))
        //    SG_ERR_IGNORE(  sg_report_portability_warnings(pCtx,pPendingTree)  );
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
        SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
        SG_NULLFREE(pCtx, ppPaths);
    }
    static void _uri__local_PUT__finish(SG_context * pCtx, const SG_audit * pAudit, void * pContext, _response_handle ** ppResponseHandle)
    {
        SG_string * pRequestBody = (SG_string*)pContext;

        SG_vhash * pVhash = NULL;
        SG_uint32 one = 0;
        const char * pKey = NULL;
        const SG_variant * pValue = NULL;

        SG_ASSERT(pCtx!=NULL);
        SG_NULLARGCHECK_RETURN(pAudit);
        SG_NULLARGCHECK_RETURN(pContext);
        SG_NULLARGCHECK_RETURN(ppResponseHandle);
        SG_ASSERT(*ppResponseHandle==NULL);

        SG_VHASH__ALLOC__FROM_JSON(pCtx, &pVhash, SG_string__sz(pRequestBody));
        if(SG_context__err_equals(pCtx, SG_ERR_JSONPARSER_SYNTAX))
            SG_ERR_RESET_THROW2(SG_ERR_URI_HTTP_400_BAD_REQUEST, (pCtx, "Request was not valid JSON."));
        else if (SG_context__has_err(pCtx))
            SG_ERR_RETHROW;

        SG_ERR_CHECK(  SG_vhash__count(pCtx, pVhash, &one)  );
        if(one!=1)
            SG_ERR_RESET_THROW(SG_ERR_URI_HTTP_400_BAD_REQUEST);

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pVhash, 0, &pKey, &pValue)  );
        if(eq(pKey,"commit") && pValue->type==SG_VARIANT_TYPE_VARRAY)
            SG_ERR_CHECK(  _uri__local_PUT__commit(pCtx, pAudit, pValue->v.val_varray, ppResponseHandle)  );
        else if(eq(pKey,"revert") && pValue->type==SG_VARIANT_TYPE_VARRAY)
            SG_ERR_CHECK(  _uri__local_PUT__revert(pCtx, pValue->v.val_varray)  );
        else
            SG_ERR_THROW(SG_ERR_URI_HTTP_400_BAD_REQUEST);

        SG_VHASH_NULLFREE(pCtx, pVhash);

        SG_STRING_NULLFREE(pCtx, pRequestBody);

        return;
    fail:
        SG_VHASH_NULLFREE(pCtx, pVhash);
        SG_STRING_NULLFREE(pCtx, pRequestBody);
    }
static void _PUT__local(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    _request_handle ** ppRequestHandle)
{
    SG_string * pRequestBody = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);

    if(pRequestHeaders->pFrom==NULL || pRequestHeaders->pFrom[0]=='\0')
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_401_UNAUTHORIZED);

    if(pRequestHeaders->contentLength==0)
        SG_ERR_THROW_RETURN(SG_ERR_URI_POSTDATA_MISSING);
    if(pRequestHeaders->contentLength>SG_STRING__MAX_LENGTH_IN_BYTES)
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_413_REQUEST_ENTITY_TOO_LARGE);

    SG_ERR_CHECK(  SG_STRING__ALLOC__RESERVE(pCtx, &pRequestBody, (SG_uint32)pRequestHeaders->contentLength)  );

    SG_ERR_CHECK(  _request_handle__alloc(pCtx, ppRequestHandle, pRequestHeaders,
        _chunk_request_to_string,
        _uri__local_PUT__finish,
        _free_string,
        pRequestBody)  );

    return;
fail:
    SG_STRING_NULLFREE(pCtx, pRequestBody);
}

// Function defined in sg_uridispatch_browsefs.c
void _dispatch__fs(SG_context * pCtx,
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _response_handle ** ppResponseHandle);

// Function defined in sg_uridispatch_log.c
void _dispatch__log(SG_context * pCtx,
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _response_handle ** ppResponseHandle);



// Function defined in sg_uridispatch_status.c
void _dispatch__status(SG_context * pCtx,
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _response_handle ** ppResponseHandle);

static void _new_wit_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement)
{
	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "TITLE"))
		SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, replacement, "veracity / work items")  );
}



static void _dispatch__local(SG_context * pCtx,
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"GET"))
            SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "index.xhtml", _local_index_html_replacer, ppResponseHandle)  );
        else if(eq(pRequestHeaders->pRequestMethod,"PUT"))
            SG_ERR_CHECK_RETURN(  _PUT__local(pCtx, pRequestHeaders, ppRequestHandle)  );
        else
            SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    }
    else if(eq(ppUriSubstrings[0],"fs"))
        SG_ERR_CHECK_RETURN(  _dispatch__fs(pCtx, pRequestHeaders, ppUriSubstrings+1, uriSubstringsCount-1, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"repo"))
        SG_ERR_CHECK_RETURN(  _dispatch__repo(pCtx, pRequestHeaders, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
	else if(eq(ppUriSubstrings[0],"status"))
        SG_ERR_CHECK_RETURN(  _dispatch__status(pCtx, pRequestHeaders, ppUriSubstrings+1, uriSubstringsCount-1, ppResponseHandle)  );
    else
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);
}

void _dispatch(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{
	SG_string *newUrl = NULL;
	SG_string *page = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppRequestHandle);
    SG_ASSERT(*ppRequestHandle==NULL);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

	//	the one place we still need to test for an empty first substring, as an emtpy uri
	//	will always split into one empty string, not a zero-length array
    if ((uriSubstringsCount==0) ||
		((uriSubstringsCount==1) && eq(ppUriSubstrings[0],"")))
	{
		if (pRequestHeaders->pHost && ! eq(pRequestHeaders->pHost, ""))
		{
			// we know enough to do a sensible redirect

			const char *proto = pRequestHeaders->isSsl ? "https" : "http";

			SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &page)  );
			SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &newUrl)  );
			SG_ERR_CHECK(  SG_string__sprintf(pCtx, newUrl, "%s://%s/home", proto, pRequestHeaders->pHost)  );

			SG_ERR_CHECK(  SG_string__sprintf(pCtx, page,
							"<html><head><title>veracity / redirecting</title></head>"
							"<body>Redirecting to <a href='%s'>%s</a>...</body></html>",
							SG_string__sz(newUrl), SG_string__sz(newUrl))
						);

			SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_MOVED_PERMANENTLY, selectHtmlType(pRequestHeaders->pUserAgent), &page, ppResponseHandle)  );
			SG_ERR_CHECK(  _response_handle__add_header(pCtx, *ppResponseHandle, "Location", SG_string__sz(newUrl))  );
			SG_ERR_IGNORE( SG_STRING_NULLFREE(pCtx, newUrl)  );

			return;
		}

        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
	}
	else if (eq(ppUriSubstrings[0],"home"))
        SG_ERR_CHECK_RETURN(  _dispatch__home(pCtx, pRequestHeaders, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
    else if (eq(ppUriSubstrings[0],"local"))
        SG_ERR_CHECK_RETURN(  _dispatch__local(pCtx, pRequestHeaders, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
    else if(eq(ppUriSubstrings[0],"repos"))
        SG_ERR_CHECK_RETURN(  _dispatch__repos(pCtx, pRequestHeaders, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle, ppResponseHandle)  );
    else if(eq(ppUriSubstrings[0],"tests"))
        SG_ERR_CHECK_RETURN(  _dispatch__tests(pCtx, pRequestHeaders, ppUriSubstrings+1, uriSubstringsCount-1, ppRequestHandle)  );
    else
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);

	return;

fail:
	SG_STRING_NULLFREE(pCtx, newUrl);
	SG_STRING_NULLFREE(pCtx, page);
	SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_500_INTERNAL_SERVER_ERROR);
}
