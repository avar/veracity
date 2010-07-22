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

typedef struct
{
	SG_string* pstrRequestBody;
	SG_repo* pRepo;
} _POST_sync_request_body_state;

static void _POST_sync_request_body_state__free(SG_context* pCtx, _POST_sync_request_body_state* pState)
{
	if (pState)
	{
		SG_STRING_NULLFREE(pCtx, pState->pstrRequestBody);
		SG_REPO_NULLFREE(pCtx, pState->pRepo);
		SG_NULLFREE(pCtx, pState);
	}
}
#define _POST_SYNC_REQUEST_BODY_STATE__NULLFREE(pCtx,p) SG_STATEMENT(SG_context__push_level(pCtx); _POST_sync_request_body_state__free(pCtx,p); SG_context__pop_level(pCtx); p=NULL;)

//////////////////////////////////////////////////////////////////

static void _POST_sync_request_body__chunk(SG_context* pCtx,
										  _response_handle** ppResponseHandle,
										  SG_byte* pBuffer,
										  SG_uint32 bufferLength,
										  void* pState)
{
	SG_string* pstrRequestBody = ((_POST_sync_request_body_state*)pState)->pstrRequestBody;

	SG_UNUSED(ppResponseHandle);

	SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, pstrRequestBody, pBuffer, bufferLength)  );
}

static void _POST_sync_request_body__aborted(SG_context* pCtx, void *pState)
{
	_POST_SYNC_REQUEST_BODY_STATE__NULLFREE(pCtx, pState);
}

static void _POST_sync_request_body__finish(SG_context* pCtx,
										   const SG_audit* pAudit,
										   void* pVoidState,
										   _response_handle ** ppResponseHandle)
{
	_POST_sync_request_body_state* pRequestState = (_POST_sync_request_body_state*)pVoidState;
	SG_vhash* pvhRequest = NULL;
	SG_pathname* pPathTemp = NULL;
	char* pszFragballName = NULL;
	SG_vhash* pvhStatus = NULL;
	SG_pathname* pPathFragball = NULL;

	SG_UNUSED(pAudit);

	if (pRequestState->pstrRequestBody)
		SG_ERR_CHECK(  SG_vhash__alloc__from_json(pCtx, &pvhRequest, SG_string__sz(pRequestState->pstrRequestBody))  );

	SG_ERR_CHECK(  SG_pathname__alloc__user_temp_directory(pCtx, &pPathTemp)  ); // TODO: what's the right path?  Not user temp.
	SG_ERR_CHECK(  SG_server__pull_request_fragball(pCtx, pRequestState->pRepo, pvhRequest, pPathTemp, &pszFragballName, &pvhStatus) );

	SG_ERR_CHECK(  SG_pathname__alloc__pathname_sz(pCtx, &pPathFragball, pPathTemp, (const char*)pszFragballName)  );

	SG_ERR_CHECK(  _create_response_handle_for_file(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__fragball, &pPathFragball, SG_TRUE, ppResponseHandle)  );

	/* fall through */
fail:
	_POST_SYNC_REQUEST_BODY_STATE__NULLFREE(pCtx, pRequestState);
	SG_VHASH_NULLFREE(pCtx, pvhRequest);
	SG_PATHNAME_NULLFREE(pCtx, pPathTemp);
	SG_NULLFREE(pCtx, pszFragballName);
	SG_VHASH_NULLFREE(pCtx, pvhStatus);
	SG_PATHNAME_NULLFREE(pCtx, pPathFragball);
}

/**
 * Handle a POST to /repos/<descriptor>/sync.
 *
 * This handles two things:
 *
 * 1)  A fragball request (typically for pull or clone)
 *   - will always have an "application/fragball" Accept header
 *   - will have either:
 *     - a zero-length message body, indicating a request for the leaves of every DAG
 *     - a jsonified vhash in the message body, whose format is described by SG_server__pull_request_fragball
 *
 * 2)  A push_begin request
 *   - will always have an "application/json" Accept header
 *   - will always have an empty message body
 */
static void _POST_sync(SG_context* pCtx,
					   _request_headers * pRequestHeaders,
					   const char* pszRepoDescriptorName,
					   SG_repo** ppRepo,
					   _request_handle ** ppRequestHandle,
					   _response_handle** ppResponseHandle)
{
	_POST_sync_request_body_state* pState = NULL;
	SG_server* pServer = NULL;
	const char* pszPushId = NULL;
	SG_string* pstrResponse = NULL;


	if (pRequestHeaders->accept == SG_contenttype__fragball)
	{
		SG_ERR_CHECK(  SG_alloc1(pCtx, pState)  );
		pState->pRepo = *ppRepo;

		if (pRequestHeaders->contentLength > 0)
		{
			// A fragball was requested with specifics.

			// Fetch the fragball's specs.
			if (pRequestHeaders->contentLength > SG_STRING__MAX_LENGTH_IN_BYTES)
				SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_413_REQUEST_ENTITY_TOO_LARGE);

			SG_ERR_CHECK(  SG_STRING__ALLOC__RESERVE(pCtx, &pState->pstrRequestBody, (SG_uint32)pRequestHeaders->contentLength)  );

			SG_ERR_CHECK(  _request_handle__alloc(pCtx, ppRequestHandle, pRequestHeaders,
				_POST_sync_request_body__chunk,
				_POST_sync_request_body__finish,
				_POST_sync_request_body__aborted,
				pState)  );
		}
		else
		{
			// The default fragball was requested.
			SG_ERR_CHECK(  _POST_sync_request_body__finish(pCtx, NULL, pState, ppResponseHandle)  );
		}

		*ppRepo = NULL;
		pState = NULL;
	}
	else if (pRequestHeaders->accept == SG_contenttype__json)
	{
		// A push_begin request was made.

		SG_ERR_CHECK(  SG_server__alloc(pCtx, &pServer)  );

 		// Tell the server we're about to push.  We get back a push ID, which we'll send back to our requester.
 		SG_ERR_CHECK(  SG_server__push_begin(pCtx, pServer, pszRepoDescriptorName, &pszPushId)  );

		// TODO: This isn't actually json.
		// Return the push id.
		SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pstrResponse, pszPushId)  );
		SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &pstrResponse, ppResponseHandle)  );

		SG_REPO_NULLFREE(pCtx, *ppRepo);
		*ppRepo = NULL;
	}
	else
		SG_ERR_THROW2(SG_ERR_URI_HTTP_400_BAD_REQUEST, (pCtx, "Missing or invalid Accept header."));

	/* fall through */
fail:
	if (pState && SG_context__has_err(pCtx))
		pState->pRepo = NULL; // Repo's not ours to free if we're going to return an error.
	_POST_SYNC_REQUEST_BODY_STATE__NULLFREE(pCtx, pState);

	SG_SERVER_NULLFREE(pCtx, pServer);
	SG_NULLFREE(pCtx, pszPushId);
	SG_STRING_NULLFREE(pCtx, pstrResponse);
}

//////////////////////////////////////////////////////////////////////////

static void _DELETE_pushid(SG_context* pCtx,
						   _request_headers * pRequestHeaders,
						   SG_repo** ppRepo,
						   const char* pszPushId,
						   _request_handle ** ppRequestHandle,
						   _response_handle** ppResponseHandle)
{
	SG_server* pServer = NULL;

	SG_UNUSED(pRequestHeaders);
	SG_UNUSED(ppRequestHandle);

	SG_ERR_CHECK(  SG_server__alloc(pCtx, &pServer)  );

	SG_ERR_CHECK(  SG_server__push_end(pCtx, pServer, pszPushId)  );

	SG_ERR_CHECK(  _response_handle__alloc(pCtx, ppResponseHandle, SG_HTTP_STATUS_OK, NULL, 0, NULL, NULL, NULL, NULL)  );

	SG_REPO_NULLFREE(pCtx, *ppRepo);

	/* fall through */
fail:
	SG_SERVER_NULLFREE(pCtx, pServer);

}

//////////////////////////////////////////////////////////////////////////

typedef struct
{
	char bufFragballFilename[SG_TID_MAX_BUFFER_LENGTH];
	SG_file* pFileFragball;
	char* pszPushId;
} _POST_pushid_request_body_state;

static void _POST_pushid_request_body_state__free(SG_context* pCtx, _POST_pushid_request_body_state* pState)
{
	if (pState)
	{
		SG_FILE_NULLCLOSE(pCtx, pState->pFileFragball);
		SG_NULLFREE(pCtx, pState->pszPushId);
		SG_NULLFREE(pCtx, pState);
	}
}
#define _POST_PUSHID_REQUEST_BODY_STATE__NULLFREE(pCtx,p) SG_STATEMENT(SG_context__push_level(pCtx); _POST_pushid_request_body_state__free(pCtx,p); SG_context__pop_level(pCtx); p=NULL;)

static void _POST_pushid_request_body__chunk(SG_context* pCtx,
											 _response_handle** ppResponseHandle,
											 SG_byte* pBuffer,
											 SG_uint32 bufferLength,
											 void* pState)
{
	SG_file* pFileFragball = ((_POST_pushid_request_body_state*)pState)->pFileFragball;
	SG_uint32 lenWritten;

	SG_UNUSED(ppResponseHandle);

	SG_ERR_CHECK_RETURN(  SG_file__write(pCtx, pFileFragball, bufferLength, (const SG_byte*)pBuffer, &lenWritten)  );
	if (lenWritten != bufferLength)
		SG_ERR_THROW_RETURN(SG_ERR_INCOMPLETEWRITE);
}

static void _POST_pushid_request_body__aborted(SG_context* pCtx, void *pState)
{
	_POST_PUSHID_REQUEST_BODY_STATE__NULLFREE(pCtx, pState);
}

static void _POST_pushid_request_body__finish(SG_context* pCtx,
											  const SG_audit* pAudit,
											  void* pVoidState,
											  _response_handle ** ppResponseHandle)
{
	_POST_pushid_request_body_state* pState = (_POST_pushid_request_body_state*)pVoidState;
	SG_server* pServer = NULL;
	SG_vhash* pvhAddResult = NULL;
	SG_string* pstrResponse = NULL;

	SG_UNUSED(pAudit);

	SG_ERR_CHECK(  SG_server__alloc(pCtx, &pServer)  );

	SG_ERR_CHECK(  SG_server__push_add(pCtx, pServer, pState->pszPushId, (const char*)pState->bufFragballFilename, &pvhAddResult) );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstrResponse)  );
	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhAddResult, pstrResponse)  );

	SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &pstrResponse, ppResponseHandle)  );

	/* fall through */
fail:
	_POST_PUSHID_REQUEST_BODY_STATE__NULLFREE(pCtx, pState);
	SG_STRING_NULLFREE(pCtx, pstrResponse);
	SG_SERVER_NULLFREE(pCtx, pServer);
	SG_VHASH_NULLFREE(pCtx, pvhAddResult);
}

static void _push_commit(SG_context* pCtx,
						 const char* pszPushId,
						 _response_handle** ppResponseHandle)
{
	SG_server* pServer = NULL;
	SG_vhash* pvhCommitResult = NULL;
	SG_string* pstrResponse = NULL;

	SG_ERR_CHECK(  SG_server__alloc(pCtx, &pServer)  );

	SG_ERR_CHECK(  SG_server__push_commit(pCtx, pServer, pszPushId, &pvhCommitResult) );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstrResponse)  );
	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhCommitResult, pstrResponse)  );

	SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &pstrResponse, ppResponseHandle)  );

	/* fall through */
fail:
	SG_STRING_NULLFREE(pCtx, pstrResponse);
	SG_SERVER_NULLFREE(pCtx, pServer);
	SG_VHASH_NULLFREE(pCtx, pvhCommitResult);
}


static void _POST_pushid(SG_context* pCtx,
						 _request_headers * pRequestHeaders,
						 SG_repo** ppRepo,
						 const char* pszPushId,
						 _request_handle ** ppRequestHandle,
						 _response_handle** ppResponseHandle)
{
	SG_staging* pStaging = NULL;
	const SG_pathname* pStagingPathname = NULL;
	SG_pathname* pPathFragball = NULL;
	_POST_pushid_request_body_state* pRequestState = NULL;

	if (pRequestHeaders->contentLength == 0)
	{
		// push commit
		SG_ERR_CHECK(  _push_commit(pCtx, pszPushId, ppResponseHandle)  );
	}
	else
	{
		// push add
		SG_ERR_CHECK(  SG_alloc1(pCtx, pRequestState)  );
		SG_ERR_CHECK(  SG_strdup(pCtx, pszPushId, &pRequestState->pszPushId)  );

		SG_ERR_CHECK(  SG_tid__generate(pCtx, pRequestState->bufFragballFilename, SG_TID_MAX_BUFFER_LENGTH)  );
		SG_ERR_CHECK(  SG_staging__open(pCtx, pszPushId, &pStaging)  );
		SG_ERR_CHECK(  SG_staging__get_pathname(pCtx, pStaging, &pStagingPathname)  );
		SG_ERR_CHECK(  SG_pathname__alloc__pathname_sz(pCtx, &pPathFragball, pStagingPathname, pRequestState->bufFragballFilename)  );

		SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathFragball, SG_FILE_CREATE_NEW | SG_FILE_WRONLY, 0644, &pRequestState->pFileFragball)  );

		SG_ERR_CHECK(  _request_handle__alloc(pCtx, ppRequestHandle, pRequestHeaders,
			_POST_pushid_request_body__chunk,
			_POST_pushid_request_body__finish,
			_POST_pushid_request_body__aborted,
			pRequestState)  );
		pRequestState = NULL;
	}

	SG_REPO_NULLFREE(pCtx, *ppRepo);

	/* fall through */
fail:
	SG_STAGING_NULLFREE(pCtx, pStaging);
	SG_PATHNAME_NULLFREE(pCtx, pPathFragball);
	_POST_PUSHID_REQUEST_BODY_STATE__NULLFREE(pCtx, pRequestState);
}

//////////////////////////////////////////////////////////////////////////

static void _GET_pushid(SG_context* pCtx,
						_request_headers * pRequestHeaders,
						SG_repo** ppRepo,
						const char* pszPushId,
						_request_handle ** ppRequestHandle,
						_response_handle** ppResponseHandle)
{
	SG_staging* pStaging = NULL;
	SG_vhash* pvhStatus = NULL;
	SG_string* pstrResponse = NULL;

	SG_UNUSED(pRequestHeaders);
	SG_UNUSED(ppRequestHandle);

	SG_ERR_CHECK(  SG_staging__open(pCtx, pszPushId, &pStaging)  );

	SG_ERR_CHECK(  SG_staging__check_status(pCtx, pStaging, SG_TRUE, SG_TRUE, SG_TRUE, SG_TRUE, SG_TRUE, &pvhStatus)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstrResponse)  );
	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhStatus, pstrResponse)  );

	SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &pstrResponse, ppResponseHandle)  );

	SG_REPO_NULLFREE(pCtx, *ppRepo);
	pstrResponse = NULL;

	/* fall through */
fail:
	SG_STAGING_NULLFREE(pCtx, pStaging);
	SG_VHASH_NULLFREE(pCtx, pvhStatus);
	SG_STRING_NULLFREE(pCtx, pstrResponse);
}

//////////////////////////////////////////////////////////////////////////

static void _dispatch__sync__pushid(SG_context * pCtx,
									_request_headers * pRequestHeaders,
									SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
									const char ** ppUriSubstrings,
									SG_uint32 uriSubstringsCount,
									_request_handle ** ppRequestHandle,
									_response_handle ** ppResponseHandle)
{
	const char* pszPushId = ppUriSubstrings[0];

	if( uriSubstringsCount != 1 || (uriSubstringsCount==1 && eq(ppUriSubstrings[0],"")) )
		SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);

	if (eq(pRequestHeaders->pRequestMethod, "DELETE"))
		SG_ERR_CHECK_RETURN(  _DELETE_pushid(pCtx, pRequestHeaders, ppRepo, pszPushId, ppRequestHandle, ppResponseHandle)  );
	else if (eq(pRequestHeaders->pRequestMethod, "POST"))
		SG_ERR_CHECK_RETURN(  _POST_pushid(pCtx, pRequestHeaders, ppRepo, pszPushId, ppRequestHandle, ppResponseHandle)  );
	else if (eq(pRequestHeaders->pRequestMethod, "GET"))
		SG_ERR_CHECK_RETURN(  _GET_pushid(pCtx, pRequestHeaders, ppRepo, pszPushId, ppRequestHandle, ppResponseHandle)  );
	else
		SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
}

void _dispatch__sync(SG_context * pCtx,
					 _request_headers * pRequestHeaders,
					 const char* pszRepoDescriptorName,
					 SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
					 const char ** ppUriSubstrings,
					 SG_uint32 uriSubstringsCount,
					 _request_handle ** ppRequestHandle,
					 _response_handle ** ppResponseHandle)
{
	if( uriSubstringsCount == 0)
	{
 		if (eq(pRequestHeaders->pRequestMethod, "POST"))
			SG_ERR_CHECK_RETURN(  _POST_sync(pCtx, pRequestHeaders, pszRepoDescriptorName, ppRepo, ppRequestHandle, ppResponseHandle)  );
 		else
 			SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
	}
	else if (uriSubstringsCount == 1 && !eq(ppUriSubstrings[0],""))
		_dispatch__sync__pushid(pCtx, pRequestHeaders, ppRepo, ppUriSubstrings, uriSubstringsCount, ppRequestHandle, ppResponseHandle);
	else
		SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);
}
