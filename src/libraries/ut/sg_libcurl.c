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
#include <curl/easy.h>

//////////////////////////////////////////////////////////////////////////

void SG_curl__global_init(SG_context* pCtx)
{
	CURLcode rc;

	rc = curl_global_init(CURL_GLOBAL_ALL);

	if (rc)
		SG_ERR_THROW_RETURN(SG_ERR_LIBCURL(rc));
}

void SG_curl__global_cleanup()
{
	(void) curl_global_cleanup();
}

void SG_curl__free(SG_UNUSED_PARAM(SG_context* pCtx), CURL* pCurl)
{
	SG_UNUSED(pCtx);
	curl_easy_cleanup(pCurl);
}

void SG_curl__alloc(SG_context* pCtx, CURL** ppCurl)
{
	CURL* pCurl = NULL;

	pCurl = curl_easy_init();

	if (!pCurl)
		SG_ERR_THROW_RETURN(SG_ERR_LIBCURL(CURLE_FAILED_INIT));

	*ppCurl = pCurl;
}

void SG_curl__easy_reset(SG_UNUSED_PARAM(SG_context* pCtx), CURL* pCurl)
{
	SG_UNUSED(pCtx);
	curl_easy_reset(pCurl);
}

void SG_curl__easy_setopt__sz(SG_context* pCtx, CURL* pCurl, CURLoption option, const char* pszVal)
{
	CURLcode rc = curl_easy_setopt(pCurl, option, pszVal);
	if (rc)
		SG_ERR_THROW_RETURN(SG_ERR_LIBCURL(rc));
}

void SG_curl__easy_setopt__int32(SG_context* pCtx, CURL* pCurl, CURLoption option, SG_int32 iVal)
{
	CURLcode rc = curl_easy_setopt(pCurl, option, iVal);
	if (rc)
		SG_ERR_THROW_RETURN(SG_ERR_LIBCURL(rc));
}
void SG_curl__easy_setopt__int64(SG_context* pCtx, CURL* pCurl, CURLoption option, SG_int64 iVal)
{
	CURLcode rc = curl_easy_setopt(pCurl, option, iVal);
	if (rc)
		SG_ERR_THROW_RETURN(SG_ERR_LIBCURL(rc));
}
void SG_curl__easy_setopt__pv(SG_context* pCtx, CURL* pCurl, CURLoption option, void* pv)
{
	CURLcode rc = curl_easy_setopt(pCurl, option, pv);
	if (rc)
		SG_ERR_THROW_RETURN(SG_ERR_LIBCURL(rc));
}
void SG_curl__easy_setopt__read_cb(SG_context* pCtx, CURL* pCurl, CURLoption option, curl_read_callback cb)
{
	CURLcode rc = curl_easy_setopt(pCurl, option, cb);
	if (rc)
		SG_ERR_THROW_RETURN(SG_ERR_LIBCURL(rc));
}
void SG_curl__easy_setopt__write_cb(SG_context* pCtx, CURL* pCurl, CURLoption option, curl_write_callback cb)
{
	CURLcode rc = curl_easy_setopt(pCurl, option, cb);
	if (rc)
		SG_ERR_THROW_RETURN(SG_ERR_LIBCURL(rc));
}
void SG_curl__easy_setopt__progress_cb(SG_context* pCtx, CURL* pCurl, CURLoption option, curl_progress_callback cb)
{
	CURLcode rc = curl_easy_setopt(pCurl, option, cb);
	if (rc)
		SG_ERR_THROW_RETURN(SG_ERR_LIBCURL(rc));
}

void SG_curl__easy_getinfo__int32(SG_context* pCtx, CURL* pCurl, CURLoption option, SG_int32* piVal)
{
	CURLcode rc = curl_easy_getinfo(pCurl, option, piVal);
	if (rc)
		SG_ERR_THROW_RETURN(SG_ERR_LIBCURL(rc));
}

void SG_curl__easy_perform(SG_context* pCtx, CURL* pCurl)
{
	CURLcode rc = curl_easy_perform(pCurl);
	if (rc)
		SG_ERR_THROW_RETURN(SG_ERR_LIBCURL(rc));
}

//////////////////////////////////////////////////////////////////////////

void SG_curl__set_one_header(SG_context* pCtx, CURL* pCurl, const char* pszHeader, struct curl_slist** ppHeaderList)
{
	struct curl_slist* pHeaderList = NULL;
	CURLcode rc;

	pHeaderList = curl_slist_append(pHeaderList, pszHeader);
	if (!pHeaderList)
		SG_ERR_THROW2_RETURN(SG_ERR_UNSPECIFIED, (pCtx, "Failed to add HTTP header."));

	rc = curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, pHeaderList);
	if (rc)
		SG_ERR_THROW(SG_ERR_LIBCURL(rc));

	SG_RETURN_AND_NULL(pHeaderList, ppHeaderList);

fail:
	if (pHeaderList)
		curl_slist_free_all(pHeaderList);
}

void SG_curl__free_headers(SG_UNUSED_PARAM(SG_context* pCtx), struct curl_slist* pHeaderList)
{
	SG_UNUSED(pCtx);
	if (pHeaderList)
		curl_slist_free_all(pHeaderList);
}

//////////////////////////////////////////////////////////////////////////

void SG_curl__state_file__alloc(SG_context* pCtx, SG_curl_state_file** ppState)
{
	SG_curl_state_file* pState = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pState)  );
	SG_ERR_CHECK(  SG_context__alloc(&pState->pCtx)  );

	SG_RETURN_AND_NULL(pState, ppState);

	/* fall through */
fail:
	SG_CURL_STATE_FILE_NULLFREE(pCtx, pState);
}

void SG_curl__state_file__free(SG_context* pCtx, SG_curl_state_file* pState)
{
	if (pState)
	{
		SG_FILE_NULLCLOSE(pCtx, pState->pFile);
		SG_CONTEXT_NULLFREE(pState->pCtx);
		SG_NULLFREE(pCtx, pState);
	}
}

void SG_curl__state_string__alloc(SG_context* pCtx, SG_curl_state_string** ppState)
{
	SG_curl_state_string* pState = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pState)  );
	SG_ERR_CHECK(  SG_context__alloc(&pState->pCtx)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pState->pstrResponse)  );

	SG_RETURN_AND_NULL(pState, ppState);

	/* fall through */
fail:
	SG_CURL_STATE_STRING_NULLFREE(pCtx, pState);
}

void SG_curl__state_string__alloc__reserve(SG_context* pCtx, SG_uint32 lenReserved, SG_curl_state_string** ppState)
{
	SG_curl_state_string* pState = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pState)  );
	SG_ERR_CHECK(  SG_context__alloc(&pState->pCtx)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC__RESERVE(pCtx, &pState->pstrResponse, lenReserved)  );

	SG_RETURN_AND_NULL(pState, ppState);

	/* fall through */
fail:
	SG_CURL_STATE_STRING_NULLFREE(pCtx, pState);
}

void SG_curl__state_string__free(SG_context* pCtx, SG_curl_state_string* pState)
{
	if (pState)
	{
		SG_CONTEXT_NULLFREE(pState->pCtx);
		SG_STRING_NULLFREE(pCtx, pState->pstrResponse);
		SG_NULLFREE(pCtx, pState);
	}
}

//////////////////////////////////////////////////////////////////////////

size_t SG_curl__send_file_chunk(char* buffer, size_t size, size_t nmemb, void* pVoidState)
{
	SG_curl_state_file* pState = (SG_curl_state_file*)pVoidState;
	SG_context* pCtx = pState->pCtx;

	SG_uint32 buf_len = size * nmemb;
	SG_uint32 len_read = 0;

	SG_file__read(pCtx, pState->pFile, buf_len, (SG_byte*)buffer, &len_read);
	SG_ERR_CHECK_CURRENT_DISREGARD(SG_ERR_EOF);

	return len_read;

fail:
	// Returning 0 here (rather than, say, CURL_READFUNC_ABORT) will stop the transfer
	// without replacing our error with a generic "the callback aborted" error.
	return 0;
}

size_t SG_curl__receive_string_chunk(char* buffer, size_t size, size_t nmemb, void* pVoidState)
{
	SG_string* pstrResponse = ((SG_curl_state_string*)pVoidState)->pstrResponse;
	SG_context* pCtx = ((SG_curl_state_string*)pVoidState)->pCtx;
	SG_uint32 real_size = size * nmemb;

	SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pstrResponse, (const SG_byte*)buffer, real_size)  );

	return real_size;

fail:
	// Returning 0 here (rather than, say, CURL_READFUNC_ABORT) will stop the transfer
	// without replacing our error with a generic "the callback aborted" error.
	return 0;
}

size_t SG_curl__receive_file_chunk(char* buffer, size_t size, size_t nmemb, void* pVoidState)
{
	SG_file* pFile = ((SG_curl_state_file*)pVoidState)->pFile;
	SG_context* pCtx = ((SG_curl_state_file*)pVoidState)->pCtx;
	SG_uint32 real_size = size * nmemb;
	SG_uint32 len_written = 0;

	SG_ERR_CHECK(  SG_file__write(pCtx, pFile, real_size, (SG_byte*)buffer, &len_written)  );

	if (len_written != real_size)
		SG_ERR_THROW(SG_ERR_INCOMPLETEWRITE);

	/* fall through */
fail:
	return real_size;
}

//////////////////////////////////////////////////////////////////////////

void SG_curl__throw_on_non200(SG_context* pCtx, CURL* pCurl)
{
	SG_int32 httpResponseCode = 0;

	SG_ERR_CHECK_RETURN(  SG_curl__easy_getinfo__int32(pCtx, pCurl, CURLINFO_RESPONSE_CODE, &httpResponseCode)  );
	if (httpResponseCode != 200)
		SG_ERR_THROW2_RETURN(SG_ERR_SERVER_HTTP_ERROR, (pCtx, "%d", httpResponseCode));
}

void SG_curl__perform(SG_context* pCtx, CURL* pCurl, SG_context* pRequestCtx, SG_context* pResponseCtx)
{
	SG_error err;

	SG_ERR_CHECK_RETURN(  SG_curl__easy_perform(pCtx, pCurl)  );

	// If there were errors in the request or response callbacks, deal with them.
	if (pRequestCtx && SG_context__has_err(pRequestCtx))
	{
		SG_ERR_CHECK_RETURN(  SG_context__get_err(pRequestCtx, &err)  );
		SG_ERR_THROW_RETURN(err);
	}
	if (pResponseCtx && SG_context__has_err(pResponseCtx))
	{
		SG_ERR_CHECK_RETURN(  SG_context__get_err(pResponseCtx, &err)  );
		SG_ERR_THROW_RETURN(err);
	}

	SG_ERR_CHECK_RETURN(  SG_curl__throw_on_non200(pCtx, pCurl)  );
}
