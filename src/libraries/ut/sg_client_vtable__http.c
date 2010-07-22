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

/*
 *
 * @file sg_client_vtable__http.c
 *
 */

#include <sg.h>
#include "sg_client__api.h"

#include "sg_client_vtable__http.h"

//////////////////////////////////////////////////////////////////////////

#define SYNC_URL_SUFFIX "/sync"

//////////////////////////////////////////////////////////////////////////

struct _sg_client_http_push_handle
{
	char* pszPushId;
};
typedef struct _sg_client_http_push_handle sg_client_http_push_handle;
static void _handle_free(SG_context * pCtx, sg_client_http_push_handle* pPush)
{
	if (pPush)
	{
		if ( (pPush)->pszPushId )
			SG_NULLFREE(pCtx, pPush->pszPushId);

		SG_NULLFREE(pCtx, pPush);
	}
}
#define _NULLFREE_PUSH_HANDLE(pCtx,p) SG_STATEMENT(  _handle_free(pCtx, p); p=NULL;  )

struct _sg_client_http_instance_data
{
	CURL* pCurl;
};
typedef struct _sg_client_http_instance_data sg_client_http_instance_data;

//////////////////////////////////////////////////////////////////////////

static void _get_sync_url(SG_context* pCtx, const char* pszBaseUrl, const char* pszUrlSuffix, const char* pszPushId, char** ppszUrl)
{
	SG_uint32 len_base_url;
	SG_uint32 len_full_url;
	char* pszUrl = NULL;

	len_base_url = strlen(pszBaseUrl);
	len_full_url = len_base_url + strlen(pszUrlSuffix);
	if (pszPushId)
		len_full_url += strlen(pszPushId) + 1;

	SG_ERR_CHECK(  SG_allocN(pCtx, len_full_url + 1, pszUrl)  );
	memcpy(pszUrl, pszBaseUrl, len_full_url + 1);
	SG_ERR_CHECK(  SG_strcat(pCtx, pszUrl, len_full_url + 1, pszUrlSuffix)  );

	if (pszPushId)
	{
		SG_ERR_CHECK(  SG_strcat(pCtx, pszUrl, len_full_url + 1, "/")  );
		SG_ERR_CHECK(  SG_strcat(pCtx, pszUrl, len_full_url + 1, pszPushId)  );
	}

	SG_RETURN_AND_NULL(pszUrl, ppszUrl);

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszUrl);
}

//////////////////////////////////////////////////////////////////////////

void sg_client__http__open(SG_context* pCtx,
						   SG_client * pClient,
						   const SG_vhash* pvh_credentials)
{
	sg_client_http_instance_data* pMe = NULL;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_ARGCHECK_RETURN(pvh_credentials || pvh_credentials == NULL_CREDENTIAL, pvh_credentials);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pMe)  );
	pClient->p_vtable_instance_data = (sg_client__vtable__instance_data *)pMe;

	SG_ERR_CHECK_RETURN(  SG_curl__alloc(pCtx, &pMe->pCurl)  );
}

void sg_client__http__close(SG_context * pCtx, SG_client * pClient)
{
	sg_client_http_instance_data* pMe = NULL;

	if (!pClient)
		return;

	pMe = (sg_client_http_instance_data*)pClient->p_vtable_instance_data;

	SG_CURL_NULLFREE(pCtx, pMe->pCurl);
	SG_NULLFREE(pCtx, pMe);
}

void sg_client__http__list_repo_instances(SG_context* pCtx,
										  SG_client * pClient,
										  SG_vhash** ppResult)
{
	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(ppResult);

    /* TODO call SG_server__list_repo_instances */

    SG_ERR_THROW_RETURN(SG_ERR_NOTIMPLEMENTED);
}

//////////////////////////////////////////////////////////////////////////

void sg_client__http__push_begin(SG_context* pCtx,
								 SG_client * pClient,
								 SG_pathname** pFragballDirPathname,
								 SG_client_push_handle** ppPush)
{
	sg_client_http_instance_data* pMe = NULL;
	char* pszUrl = NULL;
	struct curl_slist* pHeaderList = NULL;
	SG_curl_state_string* pResponseState = NULL;
	sg_client_http_push_handle* pPush = NULL;
	SG_pathname* pPathUserTemp = NULL;
	SG_pathname* pPathFragballDir = NULL;
	char bufTid[SG_TID_MAX_BUFFER_LENGTH];

	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(pFragballDirPathname);
	SG_NULLARGCHECK_RETURN(ppPush);

	pMe = (sg_client_http_instance_data*)pClient->p_vtable_instance_data;

	// Get the URL we're going to post to
	SG_ERR_CHECK(  _get_sync_url(pCtx, pClient->psz_remote_repo_spec, SYNC_URL_SUFFIX, NULL, &pszUrl)  );

	// Set HTTP options
	SG_ERR_CHECK(  SG_curl__easy_reset(pCtx, pMe->pCurl)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_POST, 1)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__sz(pCtx, pMe->pCurl, CURLOPT_URL, pszUrl)  );
	SG_ERR_CHECK(  SG_curl__set_one_header(pCtx, pMe->pCurl, "Accept:application/json", &pHeaderList)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_POSTFIELDSIZE, 0)  );

	// Set up state and callbacks to handle response
	SG_ERR_CHECK(  SG_curl__state_string__alloc(pCtx, &pResponseState)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__pv(pCtx, pMe->pCurl, CURLOPT_WRITEDATA, pResponseState)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__write_cb(pCtx, pMe->pCurl, CURLOPT_WRITEFUNCTION, SG_curl__receive_string_chunk)  );

	// Do it
	SG_ERR_CHECK(  SG_curl__perform(pCtx, pMe->pCurl, NULL, pResponseState->pCtx)  );

	// Alloc a push handle.  Stuff the push ID we received into it.
	SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(sg_client_http_push_handle), &pPush)  );
	SG_ERR_CHECK(  SG_string__sizzle(pCtx, &pResponseState->pstrResponse, (SG_byte**)&pPush->pszPushId, NULL)  );

	// Create a temporary local directory for stashing fragballs before shipping them over the network.
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__USER_TEMP_DIRECTORY(pCtx, &pPathUserTemp)  );
	SG_ERR_CHECK(  SG_tid__generate(pCtx, bufTid, SG_TID_MAX_BUFFER_LENGTH)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFragballDir, pPathUserTemp, bufTid)  );
	SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathFragballDir)  );

	// Tell caller where to stash fragballs for this push.
	SG_RETURN_AND_NULL(pPathFragballDir, pFragballDirPathname);

	// Return the new push handle.
	*ppPush = (SG_client_push_handle*)pPush;
	pPush = NULL;

	/* fall through */
fail:
	_NULLFREE_PUSH_HANDLE(pCtx, pPush);
	SG_NULLFREE(pCtx, pszUrl);
	SG_CURL_HEADERS_NULLFREE(pCtx, pHeaderList);
	SG_PATHNAME_NULLFREE(pCtx, pPathUserTemp);
	SG_PATHNAME_NULLFREE(pCtx, pPathFragballDir);
	SG_CURL_STATE_STRING_NULLFREE(pCtx, pResponseState);
}

//////////////////////////////////////////////////////////////////////////

void sg_client__http__push_add(SG_context* pCtx,
							   SG_client * pClient,
							   SG_client_push_handle* pPush,
							   SG_pathname** ppPath_fragball,
							   SG_vhash** ppResult)
{
	sg_client_http_instance_data* pMe = NULL;
	sg_client_http_push_handle* pMyPush = (sg_client_http_push_handle*)pPush;
	SG_curl_state_file* pRequestState = NULL;
	SG_curl_state_string* pResponseState = NULL;
	char* pszUrl = NULL;
	SG_uint64 lenFragball;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(pPush);
	SG_NULLARGCHECK_RETURN(ppResult);

	pMe = (sg_client_http_instance_data*)pClient->p_vtable_instance_data;

	SG_ERR_CHECK(  _get_sync_url(pCtx, pClient->psz_remote_repo_spec, SYNC_URL_SUFFIX, pMyPush->pszPushId, &pszUrl)  );
	SG_ERR_CHECK(  SG_curl__easy_reset(pCtx, pMe->pCurl)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__sz(pCtx, pMe->pCurl, CURLOPT_URL, pszUrl)  );

	if (!ppPath_fragball || !*ppPath_fragball)
	{
		/* get the push's current status */
		SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_HTTPGET, 1)  );

		SG_ERR_CHECK(  SG_curl__state_string__alloc(pCtx, &pResponseState)  );
		SG_ERR_CHECK(  SG_curl__easy_setopt__pv(pCtx, pMe->pCurl, CURLOPT_WRITEDATA, pResponseState)  );
		SG_ERR_CHECK(  SG_curl__easy_setopt__write_cb(pCtx, pMe->pCurl, CURLOPT_WRITEFUNCTION, SG_curl__receive_string_chunk)  );

		SG_ERR_CHECK(  SG_curl__perform(pCtx, pMe->pCurl, NULL, pResponseState->pCtx)  );

		SG_ERR_CHECK(  SG_vhash__alloc__from_json(pCtx, ppResult, SG_string__sz(pResponseState->pstrResponse))  );
	}
	else
	{
		/* add the fragball to the push */
		SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_POST, 1)  );

		SG_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, *ppPath_fragball, &lenFragball, NULL)  );
		SG_ERR_CHECK(  SG_curl__easy_setopt__int64(pCtx, pMe->pCurl, CURLOPT_POSTFIELDSIZE_LARGE, lenFragball)  );

		SG_ERR_CHECK(  SG_alloc1(pCtx, pRequestState)  );

		SG_ERR_CHECK(  SG_file__open__pathname(pCtx, *ppPath_fragball, SG_FILE_OPEN_EXISTING | SG_FILE_RDONLY, SG_FSOBJ_PERMS__UNUSED, &pRequestState->pFile)  );
		SG_ERR_CHECK(  SG_context__alloc(&pRequestState->pCtx)  );

		SG_ERR_CHECK(  SG_curl__easy_setopt__pv(pCtx, pMe->pCurl, CURLOPT_READDATA, pRequestState)  );
		SG_ERR_CHECK(  SG_curl__easy_setopt__read_cb(pCtx, pMe->pCurl, CURLOPT_READFUNCTION, SG_curl__send_file_chunk)  );

		SG_ERR_CHECK(  SG_curl__state_string__alloc(pCtx, &pResponseState)  );
		SG_ERR_CHECK(  SG_curl__easy_setopt__pv(pCtx, pMe->pCurl, CURLOPT_WRITEDATA, pResponseState)  );
		SG_ERR_CHECK(  SG_curl__easy_setopt__write_cb(pCtx, pMe->pCurl, CURLOPT_WRITEFUNCTION, SG_curl__receive_string_chunk)  );

		SG_ERR_CHECK(  SG_curl__perform(pCtx, pMe->pCurl, pRequestState->pCtx, pResponseState->pCtx)  );

		SG_ERR_CHECK(  SG_vhash__alloc__from_json(pCtx, ppResult, SG_string__sz(pResponseState->pstrResponse))  );

		SG_PATHNAME_NULLFREE(pCtx, *ppPath_fragball);
	}

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszUrl);
	SG_CURL_STATE_FILE_NULLFREE(pCtx, pRequestState);
	SG_CURL_STATE_STRING_NULLFREE(pCtx, pResponseState);
}

//////////////////////////////////////////////////////////////////////////

void sg_client__http__push_remove(SG_context* pCtx,
								  SG_client * pClient,
								  SG_client_push_handle* pPush,
								  SG_vhash* pvh_stuff_to_be_removed_from_staging_area,
								  SG_vhash** ppResult)
{
	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(pPush);
	SG_NULLARGCHECK_RETURN(pvh_stuff_to_be_removed_from_staging_area);
	SG_NULLARGCHECK_RETURN(ppResult);

	SG_ERR_THROW_RETURN(SG_ERR_NOTIMPLEMENTED);
}

void sg_client__http__push_commit(SG_context* pCtx,
								  SG_client * pClient,
								  SG_client_push_handle* pPush,
								  SG_vhash** ppResult)
{
	sg_client_http_instance_data* pMe = NULL;
	SG_curl_state_string* pResponseState = NULL;
	char* pszUrl = NULL;
	sg_client_http_push_handle* pMyPush = (sg_client_http_push_handle*)pPush;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(pPush);
	SG_NULLARGCHECK_RETURN(ppResult);

	pMe = (sg_client_http_instance_data*)pClient->p_vtable_instance_data;

	SG_ERR_CHECK(  _get_sync_url(pCtx, pClient->psz_remote_repo_spec, SYNC_URL_SUFFIX, pMyPush->pszPushId, &pszUrl)  );

	SG_ERR_CHECK(  SG_curl__easy_reset(pCtx, pMe->pCurl)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__sz(pCtx, pMe->pCurl, CURLOPT_URL, pszUrl)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_POST, 1)  );

	SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_POSTFIELDSIZE, 0)  );

	SG_ERR_CHECK(  SG_curl__state_string__alloc(pCtx, &pResponseState)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__pv(pCtx, pMe->pCurl, CURLOPT_WRITEDATA, pResponseState)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__write_cb(pCtx, pMe->pCurl, CURLOPT_WRITEFUNCTION, SG_curl__receive_string_chunk)  );

	SG_ERR_CHECK(  SG_curl__perform(pCtx, pMe->pCurl, NULL, pResponseState->pCtx)  );

	SG_ERR_CHECK(  SG_vhash__alloc__from_json(pCtx, ppResult, SG_string__sz(pResponseState->pstrResponse))  );

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszUrl);
	SG_CURL_STATE_STRING_NULLFREE(pCtx, pResponseState);
}

void sg_client__http__push_end(SG_context * pCtx,
							   SG_client * pClient,
							   SG_client_push_handle** ppPush)
{
	sg_client_http_instance_data* pMe = NULL;
	sg_client_http_push_handle* pMyPush = NULL;
	char* pszUrl = NULL;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(ppPush);

	pMe = (sg_client_http_instance_data*)pClient->p_vtable_instance_data;
	pMyPush = (sg_client_http_push_handle*)*ppPush;

	// Get the remote URL
	SG_ERR_CHECK(  _get_sync_url(pCtx, pClient->psz_remote_repo_spec, SYNC_URL_SUFFIX, pMyPush->pszPushId, &pszUrl)  );

	SG_ERR_CHECK(  SG_curl__easy_reset(pCtx, pMe->pCurl)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__sz(pCtx, pMe->pCurl, CURLOPT_URL, pszUrl)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__sz(pCtx, pMe->pCurl, CURLOPT_CUSTOMREQUEST, "DELETE")  );

	SG_ERR_CHECK(  SG_curl__easy_perform(pCtx, pMe->pCurl)  );
	SG_ERR_CHECK(  SG_curl__throw_on_non200(pCtx, pMe->pCurl)  );

	// fall through
fail:
	// We free the push handle even on failure, because SG_push_handle is opaque outside this file:
	// this is the only way to free it.
	_NULLFREE_PUSH_HANDLE(pCtx, pMyPush);
	*ppPush = NULL;
	SG_NULLFREE(pCtx, pszUrl);
}

void sg_client__http__pull_request_fragball(SG_context* pCtx,
											SG_client* pClient,
											SG_vhash* pvhRequest,
											const SG_pathname* pStagingPathname,
											char** ppszFragballName,
											SG_vhash** ppvhStatus)
{
	sg_client_http_instance_data* pMe = NULL;
	char* pszFragballName = NULL;
	SG_pathname* pPathFragball = NULL;
	SG_curl_state_file* pReceiveState = NULL;
	SG_string* pstrRequest = NULL;
	char* pszUrl = NULL;
	struct curl_slist* pHeaderList = NULL;

	SG_NULLARGCHECK_RETURN(pClient);

	SG_UNUSED(ppvhStatus); // TODO

	pMe = (sg_client_http_instance_data*)pClient->p_vtable_instance_data;

	SG_ERR_CHECK(  SG_allocN(pCtx, SG_TID_MAX_BUFFER_LENGTH, pszFragballName)  );
	SG_ERR_CHECK(  SG_tid__generate(pCtx, pszFragballName, SG_TID_MAX_BUFFER_LENGTH)  );

	SG_ERR_CHECK(  SG_curl__state_file__alloc(pCtx, &pReceiveState)  );

	SG_ERR_CHECK(  SG_pathname__alloc__pathname_sz(pCtx, &pPathFragball, pStagingPathname, (const char*)pszFragballName)  );
	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathFragball, SG_FILE_CREATE_NEW | SG_FILE_WRONLY, 0644, &pReceiveState->pFile)  );

	SG_ERR_CHECK(  _get_sync_url(pCtx, pClient->psz_remote_repo_spec, SYNC_URL_SUFFIX, NULL, &pszUrl)  );

	SG_ERR_CHECK(  SG_curl__easy_reset(pCtx, pMe->pCurl)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_POST, 1)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__sz(pCtx, pMe->pCurl, CURLOPT_URL, pszUrl)  );

	SG_ERR_CHECK(  SG_curl__set_one_header(pCtx, pMe->pCurl, "Accept:application/fragball", &pHeaderList)  );

	if (pvhRequest)
	{
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstrRequest)  );
		SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhRequest, pstrRequest)  );

 		SG_ERR_CHECK(  SG_curl__easy_setopt__sz(pCtx, pMe->pCurl, CURLOPT_POSTFIELDS, SG_string__sz(pstrRequest))  );
 		SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_POSTFIELDSIZE, SG_string__length_in_bytes(pstrRequest))  );
	}
	else
	{
		SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_POSTFIELDSIZE, 0)  );
	}

	SG_ERR_CHECK(  SG_curl__easy_setopt__pv(pCtx, pMe->pCurl, CURLOPT_WRITEDATA, pReceiveState)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__write_cb(pCtx, pMe->pCurl, CURLOPT_WRITEFUNCTION, SG_curl__receive_file_chunk)  );

	SG_ERR_CHECK(  SG_curl__easy_perform(pCtx, pMe->pCurl)  );
	SG_ERR_CHECK(  SG_curl__throw_on_non200(pCtx, pMe->pCurl)  );

	SG_RETURN_AND_NULL(pszFragballName, ppszFragballName);

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszFragballName);
	SG_PATHNAME_NULLFREE(pCtx, pPathFragball);
	SG_CURL_STATE_FILE_NULLFREE(pCtx, pReceiveState);
	SG_NULLFREE(pCtx, pszUrl);
	SG_STRING_NULLFREE(pCtx, pstrRequest);
	SG_CURL_HEADERS_NULLFREE(pCtx, pHeaderList);
}

typedef struct _clone_progress_state
{
	SG_context* pCtx;
	double lastComplete;
} clone_progress_state;

static void _emit_clone_progress(SG_context* pCtx, clone_progress_state* pState, double nowComplete)
{
	double lastComplete = pState->lastComplete;

	if (!lastComplete && nowComplete)
	{
		SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "done\n")  );
		SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "Downloading repository...")  );
		pState->lastComplete = .01;
	}

	if ( (nowComplete - lastComplete) > .1 )
	{
		if (lastComplete < .1)
			SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, " [10..")  );
		if (nowComplete > .2 && lastComplete < .2)
			SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "20..")  );
		if (nowComplete > .3 && lastComplete < .3)
			SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "30..")  );
		if (nowComplete > .4 && lastComplete < .4)
			SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "40..")  );
		if (nowComplete > .5 && lastComplete < .5)
			SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "50..")  );
		if (nowComplete > .6 && lastComplete < .6)
			SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "60..")  );
		if (nowComplete > .7 && lastComplete < .7)
			SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "70..")  );
		if (nowComplete > .8 && lastComplete < .8)
			SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "80..")  );
		if (nowComplete > .9 && lastComplete < .9)
			SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "90..")  );

		pState->lastComplete = nowComplete;
	}

	if (nowComplete == 1 && lastComplete != 1)
	{
		SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "100]")  );
		pState->lastComplete = 1;
	}
}

static int _clone_progress_callback(
	void *clientp,
	double dltotal,
	double dlnow,
	double ultotal,
	double ulnow)
{
	double nowComplete;
	clone_progress_state* pState = (clone_progress_state*)clientp;
	SG_context* pCtx = pState->pCtx;

	SG_UNUSED(ultotal);
	SG_UNUSED(ulnow);

	if (!dltotal)
		nowComplete = 0;
	else
		nowComplete = dlnow / dltotal;

	SG_ERR_CHECK(  _emit_clone_progress(pCtx, pState, nowComplete)  );

	return 0;

fail:
	return -1;
}

void sg_client__http__pull_clone(
	SG_context* pCtx,
	SG_client* pClient,
	const SG_pathname* pStagingPathname,
	char** ppszFragballName)
{
	sg_client_http_instance_data* pMe = NULL;
	SG_vhash* pvhRequest = NULL;
	char* pszFragballName = NULL;
	SG_pathname* pPathFragball = NULL;
	SG_curl_state_file* pReceiveState = NULL;
	SG_string* pstrRequest = NULL;
	char* pszUrl = NULL;
	struct curl_slist* pHeaderList = NULL;
	clone_progress_state* pProgressState = NULL;

	SG_NULLARGCHECK_RETURN(pClient);

	pMe = (sg_client_http_instance_data*)pClient->p_vtable_instance_data;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pProgressState)  );
	pProgressState->pCtx = pCtx;

	SG_ERR_CHECK(  SG_allocN(pCtx, SG_TID_MAX_BUFFER_LENGTH, pszFragballName)  );
	SG_ERR_CHECK(  SG_tid__generate(pCtx, pszFragballName, SG_TID_MAX_BUFFER_LENGTH)  );

	SG_ERR_CHECK(  SG_curl__state_file__alloc(pCtx, &pReceiveState)  );

	SG_ERR_CHECK(  SG_pathname__alloc__pathname_sz(pCtx, &pPathFragball, pStagingPathname, (const char*)pszFragballName)  );
	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathFragball, SG_FILE_CREATE_NEW | SG_FILE_WRONLY, 0644, &pReceiveState->pFile)  );

	SG_ERR_CHECK(  _get_sync_url(pCtx, pClient->psz_remote_repo_spec, SYNC_URL_SUFFIX, NULL, &pszUrl)  );

	SG_ERR_CHECK(  SG_curl__easy_reset(pCtx, pMe->pCurl)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_POST, 1)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__sz(pCtx, pMe->pCurl, CURLOPT_URL, pszUrl)  );

	SG_ERR_CHECK(  SG_curl__set_one_header(pCtx, pMe->pCurl, "Accept:application/fragball", &pHeaderList)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhRequest)  );
	SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pvhRequest, "clone")  );
	SG_ERR_CHECK(  SG_STRING__ALLOC__RESERVE(pCtx, &pstrRequest, 50)  );
	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhRequest, pstrRequest)  );

	SG_ERR_CHECK(  SG_curl__easy_setopt__sz(pCtx, pMe->pCurl, CURLOPT_POSTFIELDS, SG_string__sz(pstrRequest))  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_POSTFIELDSIZE, SG_string__length_in_bytes(pstrRequest))  );

	SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_NOPROGRESS, 0)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__pv(pCtx, pMe->pCurl, CURLOPT_PROGRESSDATA, pProgressState)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__progress_cb(pCtx, pMe->pCurl, CURLOPT_PROGRESSFUNCTION, _clone_progress_callback)  );

	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "Requesting repository from server...")  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__pv(pCtx, pMe->pCurl, CURLOPT_WRITEDATA, pReceiveState)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__write_cb(pCtx, pMe->pCurl, CURLOPT_WRITEFUNCTION, SG_curl__receive_file_chunk)  );

	SG_ERR_CHECK(  SG_curl__easy_perform(pCtx, pMe->pCurl)  );
	SG_ERR_CHECK(  SG_curl__throw_on_non200(pCtx, pMe->pCurl)  );
	if (pProgressState->lastComplete < 1)
		SG_ERR_CHECK(  _emit_clone_progress(pCtx, pProgressState, 1)  );

	SG_RETURN_AND_NULL(pszFragballName, ppszFragballName);

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszFragballName);
	SG_VHASH_NULLFREE(pCtx, pvhRequest);
	SG_PATHNAME_NULLFREE(pCtx, pPathFragball);
	SG_CURL_STATE_FILE_NULLFREE(pCtx, pReceiveState);
	SG_NULLFREE(pCtx, pszUrl);
	SG_STRING_NULLFREE(pCtx, pstrRequest);
	SG_CURL_HEADERS_NULLFREE(pCtx, pHeaderList);
	SG_NULLFREE(pCtx, pProgressState);
	SG_ERR_IGNORE(  SG_context__msg__emit(pCtx, "\n")  );
}

void sg_client__http__get_repo_info(SG_context* pCtx,
									SG_client* pClient,
									char** ppszRepoId,
									char** ppszAdminId,
									char** ppszHashMethod)
{
	sg_client_http_instance_data* pMe = NULL;
	SG_vhash* pvh = NULL;
	SG_curl_state_string* pState = NULL;
	const char* pszRepoId = NULL;
	const char* pszAdminId = NULL;
	const char* pszHashMethod = NULL;


	SG_NULLARGCHECK_RETURN(pClient);

	pMe = (sg_client_http_instance_data*)pClient->p_vtable_instance_data;

	SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_HTTPGET, 1)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__sz(pCtx, pMe->pCurl, CURLOPT_URL, pClient->psz_remote_repo_spec)  );

	SG_ERR_CHECK(  SG_curl__state_string__alloc__reserve(pCtx, 256, &pState)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__pv(pCtx, pMe->pCurl, CURLOPT_WRITEDATA, pState)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__write_cb(pCtx, pMe->pCurl, CURLOPT_WRITEFUNCTION, SG_curl__receive_string_chunk)  );

	SG_ERR_CHECK(  SG_curl__perform(pCtx, pMe->pCurl, NULL, pState->pCtx)  );

	SG_ERR_CHECK(  SG_vhash__alloc__from_json(pCtx, &pvh, SG_string__sz(pState->pstrResponse))  );

	if (ppszRepoId)
	{
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "RepoID", &pszRepoId)  );
		SG_ERR_CHECK(  SG_STRDUP(pCtx, pszRepoId, ppszRepoId)  );
	}
	if (ppszAdminId)
	{
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "AdminID", &pszAdminId)  );
		SG_ERR_CHECK(  SG_STRDUP(pCtx, pszAdminId, ppszAdminId)  );
	}
	if (ppszHashMethod)
	{
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "HashMethod", &pszHashMethod)  );
		SG_ERR_CHECK(  SG_STRDUP(pCtx, pszHashMethod, ppszHashMethod)  );
	}

	/* fall through */
fail:
	SG_CURL_STATE_STRING_NULLFREE(pCtx, pState);
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void sg_client__http__get_dagnode_info(
	SG_context* pCtx,
	SG_client* pClient,
	SG_vhash* pvhRequest,
	SG_varray** ppvaInfo)
{
	sg_client_http_instance_data* pMe = NULL;
	SG_curl_state_string* pReceiveStrState = NULL;
	SG_string* pstrRequest = NULL;
	char* pszUrl = NULL;
	struct curl_slist* pHeaderList = NULL;
	SG_vhash* pvhResponse = NULL;
	SG_varray* pvaRef = NULL;
	SG_varray* pva = NULL;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(pvhRequest);

	pMe = (sg_client_http_instance_data*)pClient->p_vtable_instance_data;

	SG_ERR_CHECK(  _get_sync_url(pCtx, pClient->psz_remote_repo_spec, "/history", NULL, &pszUrl)  );

	SG_ERR_CHECK(  SG_curl__easy_reset(pCtx, pMe->pCurl)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_POST, 1)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__sz(pCtx, pMe->pCurl, CURLOPT_URL, pszUrl)  );

	SG_ERR_CHECK(  SG_curl__state_string__alloc(pCtx, &pReceiveStrState)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__pv(pCtx, pMe->pCurl, CURLOPT_WRITEDATA, pReceiveStrState)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__write_cb(pCtx, pMe->pCurl, CURLOPT_WRITEFUNCTION, SG_curl__receive_string_chunk)  );

	SG_ERR_CHECK(  SG_curl__set_one_header(pCtx, pMe->pCurl, "Accept:application/json", &pHeaderList)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstrRequest)  );
	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhRequest, pstrRequest)  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__sz(pCtx, pMe->pCurl, CURLOPT_POSTFIELDS, SG_string__sz(pstrRequest))  );
	SG_ERR_CHECK(  SG_curl__easy_setopt__int32(pCtx, pMe->pCurl, CURLOPT_POSTFIELDSIZE, SG_string__length_in_bytes(pstrRequest))  );

	SG_ERR_CHECK(  SG_curl__easy_perform(pCtx, pMe->pCurl)  );
	SG_ERR_CHECK(  SG_curl__throw_on_non200(pCtx, pMe->pCurl)  );

	SG_ERR_CHECK(  SG_vhash__alloc__from_json(pCtx, &pvhResponse, SG_string__sz(pReceiveStrState->pstrResponse))  );
	SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvhResponse, "varray", &pvaRef)  ); // TODO: fix this
	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );
	SG_ERR_CHECK(  SG_varray__copy_items(pCtx, pvaRef, pva)  );
	SG_RETURN_AND_NULL(pva, ppvaInfo);

	/* fall through */
fail:
	SG_CURL_STATE_STRING_NULLFREE(pCtx, pReceiveStrState);
	SG_NULLFREE(pCtx, pszUrl);
	SG_STRING_NULLFREE(pCtx, pstrRequest);
	SG_CURL_HEADERS_NULLFREE(pCtx, pHeaderList);
	SG_VARRAY_NULLFREE(pCtx, pva);
	SG_VHASH_NULLFREE(pCtx, pvhResponse);
}