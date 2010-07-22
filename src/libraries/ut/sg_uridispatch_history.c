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
 * @file sg_uridispatch_log.c
 */

#include <sg.h>
#include "sg_uridispatch_private_typedefs.h"
#include "sg_uridispatch_private_prototypes.h"

//////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////

static void _GET__history_json(SG_context * pCtx, SG_repo* pRepo, SG_uint32 numRecords, const char* pszUser, const char* pszStamp, SG_int64 dateFrom, SG_int64 dateTo,
    _response_handle ** ppResponseHandle)
{
	SG_string *content = NULL;
	SG_varray* pvaResults = NULL;

	SG_ERR_CHECK(  SG_history__query(pCtx, NULL, pRepo, 0, NULL, NULL, 0, pszUser, pszStamp, numRecords, dateFrom, dateTo, SG_TRUE, SG_FALSE, &pvaResults)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &content)  );
	SG_ERR_CHECK(  SG_varray__to_json(pCtx, pvaResults, content)  );

	_create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &content, ppResponseHandle);

fail:
	SG_VARRAY_NULLFREE(pCtx, pvaResults);
	SG_NULLFREE(pCtx, content);
}

static void _history_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement)
{
	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "TITLE"))
	{
		SG_ERR_CHECK(  SG_string__set__sz(pCtx, replacement, "veracity / history")  );
	}

fail:
	;
}

typedef struct
{
	SG_string* pstrRequestBody;
	SG_repo* pRepo;
} _dispatch_history_multiple_request_body_state;

static void _dispatch_history_multiple_request_body_state__free(SG_context* pCtx, _dispatch_history_multiple_request_body_state* pState)
{
	if (pState)
	{
		SG_STRING_NULLFREE(pCtx, pState->pstrRequestBody);
		SG_REPO_NULLFREE(pCtx, pState->pRepo);
		SG_NULLFREE(pCtx, pState);
	}
}
#define _DISPATCH_HISTORY_MULTIPLE_REQUEST_BODY_STATE__NULLFREE(pCtx,p) SG_STATEMENT(SG_context__push_level(pCtx); _dispatch_history_multiple_request_body_state__free(pCtx,p); SG_context__pop_level(pCtx); p=NULL;)

static void _dispatch__history__multiple__request_body__chunk(SG_context* pCtx,
	_response_handle** ppResponseHandle,
	SG_byte* pBuffer,
	SG_uint32 bufferLength,
	void* pVoidState)
{
	SG_string* pstrRequestBody = ((_dispatch_history_multiple_request_body_state*)pVoidState)->pstrRequestBody;

	SG_UNUSED(ppResponseHandle);

	SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, pstrRequestBody, pBuffer, bufferLength)  );
}

static void _dispatch__history__multiple__request_body__aborted(SG_context* pCtx, void *pVoidState)
{
	_dispatch_history_multiple_request_body_state* pState = (_dispatch_history_multiple_request_body_state*)pVoidState;
	_DISPATCH_HISTORY_MULTIPLE_REQUEST_BODY_STATE__NULLFREE(pCtx, pState);
}

static void _dispatch__history__multiple__request_body__finish(SG_context* pCtx,
	const SG_audit* pAudit,
	void* pVoidState,
	_response_handle ** ppResponseHandle)
{
	_dispatch_history_multiple_request_body_state* pState = (_dispatch_history_multiple_request_body_state*)pVoidState;
	SG_string* pstrRequestBody = pState->pstrRequestBody;
	SG_server* pServer = NULL;
	SG_vhash* pvhRequest = NULL;
	SG_varray* pvaInfo = NULL;
	SG_vhash* pvhResponse = NULL;
	SG_string* pstrResponse = NULL;

	SG_UNUSED(pAudit);

	if (pstrRequestBody)
		SG_ERR_CHECK(  SG_vhash__alloc__from_json(pCtx, &pvhRequest, SG_string__sz(pstrRequestBody))  );

	SG_ERR_CHECK(  SG_server__alloc(pCtx, &pServer)  );
	SG_ERR_CHECK(  SG_server__get_dagnode_info(pCtx, pState->pRepo, pvhRequest, &pvaInfo)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhResponse)  );
	SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvhResponse, "varray", &pvaInfo)  ); // TODO: fix this
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstrResponse)  );
	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhResponse, pstrResponse)  );

	SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &pstrResponse, ppResponseHandle)  );

	/* fall through */
fail:
	_DISPATCH_HISTORY_MULTIPLE_REQUEST_BODY_STATE__NULLFREE(pCtx, pState);
	SG_SERVER_NULLFREE(pCtx, pServer);
	SG_VHASH_NULLFREE(pCtx, pvhRequest);
	SG_VARRAY_NULLFREE(pCtx, pvaInfo);
	SG_VHASH_NULLFREE(pCtx, pvhResponse);
	SG_STRING_NULLFREE(pCtx, pstrResponse);
}

/**
 * Get history info for a list of changesets.
 */
static void _dispatch__history__multiple(
	SG_context * pCtx,
	_request_headers * pRequestHeaders,
	SG_repo ** ppRepo,
	_request_handle ** ppRequestHandle)
{
	_dispatch_history_multiple_request_body_state* pState = NULL;

	if (0 == pRequestHeaders->contentLength)
		SG_ERR_THROW2(SG_ERR_URI_HTTP_400_BAD_REQUEST, (pCtx, "Message body missing."));

	if (pRequestHeaders->contentLength > SG_STRING__MAX_LENGTH_IN_BYTES)
		SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_413_REQUEST_ENTITY_TOO_LARGE);

	SG_ERR_CHECK(  SG_alloc1(pCtx, pState)  );
	pState->pRepo = *ppRepo;
	SG_ERR_CHECK(  SG_STRING__ALLOC__RESERVE(pCtx, &pState->pstrRequestBody, (SG_uint32)pRequestHeaders->contentLength)  );

	SG_ERR_CHECK(  _request_handle__alloc(pCtx, ppRequestHandle, pRequestHeaders,
		_dispatch__history__multiple__request_body__chunk,
		_dispatch__history__multiple__request_body__finish,
		_dispatch__history__multiple__request_body__aborted,
		pState)  );

	*ppRepo = NULL;
	return;
	
fail:
	_DISPATCH_HISTORY_MULTIPLE_REQUEST_BODY_STATE__NULLFREE(pCtx, pState);
}


void _dispatch__history(SG_context * pCtx,
    _request_headers * pRequestHeaders,
	SG_repo ** ppRepo,
	const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
	_request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle)
{

	SG_bool found = SG_FALSE;
	SG_vhash* params = NULL;
	SG_int64 nDateFrom = 0;
	SG_int64 nDateTo = SG_INT64_MAX;
	const char* pszTo = NULL;
	const char* pszFrom = NULL;
	const char* pszUser = NULL;
	const char* pszStamp = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"GET"))
		{
            if(pRequestHeaders->accept==SG_contenttype__json)
			{
				SG_uint32 maxResults = 50;
				SG_VHASH__ALLOC(pCtx, &params);

				if (pRequestHeaders->pQueryString)
				{
					SG_ERR_CHECK(  SG_querystring_to_vhash(pCtx, pRequestHeaders->pQueryString, params)  );
					SG_ERR_CHECK(  SG_vhash__has(pCtx, params, "max", &found)  );

					if (found)
					{
						 SG_ERR_CHECK(   SG_vhash__get__uint32(pCtx, params, "max", &maxResults)  );
						 if (maxResults == 0)
							  maxResults = SG_INT32_MAX;
					}
					SG_ERR_CHECK(  SG_vhash__has(pCtx, params, "from", &found)  );
					if (found)
					{
						SG_ERR_CHECK(   SG_vhash__get__sz(pCtx, params, "from", &pszFrom)  );
						SG_ERR_CHECK(  SG_time__parse(pCtx, pszFrom, &nDateFrom, SG_FALSE)  );
					}
					SG_ERR_CHECK(  SG_vhash__has(pCtx, params, "to", &found)  );
					if (found)
					{
						SG_ERR_CHECK(   SG_vhash__get__sz(pCtx, params, "to", &pszTo)  );
						SG_ERR_CHECK(  SG_time__parse(pCtx, pszTo, &nDateTo, SG_TRUE)  );
					}
					SG_ERR_CHECK(  SG_vhash__has(pCtx, params, "user", &found)  );
					if (found)
					{
						SG_ERR_CHECK(   SG_vhash__get__sz(pCtx, params, "user", &pszUser)  );

					}
					SG_ERR_CHECK(  SG_vhash__has(pCtx, params, "stamp", &found)  );
					if (found)
					{
						SG_ERR_CHECK(   SG_vhash__get__sz(pCtx, params, "stamp", &pszStamp)  );

					}

				}
                SG_ERR_CHECK(  _GET__history_json(pCtx, *ppRepo, maxResults, pszUser, pszStamp, nDateFrom, nDateTo, ppResponseHandle)  );
			}
            else
                SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "history.xhtml", _history_replacer, ppResponseHandle)  );
		}
		else if (eq(pRequestHeaders->pRequestMethod,"POST")) 
		{
			/* Get history for a list of changesets. */
			SG_ERR_CHECK(  _dispatch__history__multiple(pCtx, pRequestHeaders, ppRepo, ppRequestHandle)  );
		}
        else
            SG_ERR_THROW(  SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED  );
    }
    else
        SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);

fail:
	if (params)
		SG_VHASH_NULLFREE(pCtx, params);
}
