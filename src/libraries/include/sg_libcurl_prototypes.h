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
 * @file sg_libcurl_prototypes.h
 *
 * @details sglib's thin crunchy wrapper around libcurl, primarily
 *			to play nice with its error handling.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_LIBCURL_PROTOTYPES_H
#define H_SG_LIBCURL_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_curl__global_init(SG_context* pCtx);
void SG_curl__global_cleanup(void);

void SG_curl__alloc(SG_context* pCtx, CURL** ppCurl);
void SG_curl__free(SG_context* pCtx, CURL* ppCurl);

void SG_curl__easy_reset(SG_context* pCtx, CURL* pCurl);

void SG_curl__easy_setopt__sz(SG_context* pCtx, CURL* pCurl, CURLoption option, const char* pszVal);
void SG_curl__easy_setopt__int32(SG_context* pCtx, CURL* pCurl, CURLoption option, SG_int32 iVal);
void SG_curl__easy_setopt__int64(SG_context* pCtx, CURL* pCurl, CURLoption option, SG_int64 iVal);
void SG_curl__easy_setopt__pv(SG_context* pCtx, CURL* pCurl, CURLoption option, void* pv);
void SG_curl__easy_setopt__read_cb(SG_context* pCtx, CURL* pCurl, CURLoption option, curl_read_callback cb);
void SG_curl__easy_setopt__write_cb(SG_context* pCtx, CURL* pCurl, CURLoption option, curl_write_callback cb);
void SG_curl__easy_setopt__progress_cb(SG_context* pCtx, CURL* pCurl, CURLoption option, curl_progress_callback cb);

void SG_curl__easy_getinfo__int32(SG_context* pCtx, CURL* pCurl, CURLoption option, SG_int32* piVal);

void SG_curl__easy_perform(SG_context* pCtx, CURL* pCurl);

void SG_curl__set_one_header(SG_context* pCtx, CURL* pCurl, const char* pszHeader, struct curl_slist** ppHeaderList);

void SG_curl__free_headers(SG_context* pCtx, struct curl_slist* pHeaderList);

void SG_curl__state_file__alloc(SG_context* pCtx, SG_curl_state_file** ppState);
void SG_curl__state_file__free(SG_context* pCtx, SG_curl_state_file* pState);

void SG_curl__state_string__alloc(SG_context* pCtx, SG_curl_state_string** ppState);
void SG_curl__state_string__alloc__reserve(SG_context* pCtx, SG_uint32 lenReserved, SG_curl_state_string** ppState);
void SG_curl__state_string__free(SG_context* pCtx, SG_curl_state_string* pState);

size_t SG_curl__send_file_chunk(char* buffer, size_t size, size_t nmemb, void* pVoidState);
size_t SG_curl__receive_file_chunk(char* buffer, size_t size, size_t nmemb, void* pVoidState);
size_t SG_curl__receive_string_chunk(char* buffer, size_t size, size_t nmemb, void* pVoidState);

void SG_curl__throw_on_non200(SG_context* pCtx, CURL* pCurl);

void SG_curl__perform(SG_context* pCtx, CURL* pCurl, SG_context* pRequestCtx, SG_context* pResponseCtx);

#define SG_CURL_NULLFREE(pCtx,p)				SG_STATEMENT(	SG_context__push_level(pCtx); \
																SG_curl__free(pCtx, p); \
																SG_ASSERT(!SG_context__has_err(pCtx)); \
																SG_context__pop_level(pCtx); \
																p=NULL; )

#define SG_CURL_HEADERS_NULLFREE(pCtx,p)		SG_STATEMENT(	SG_context__push_level(pCtx); \
																SG_curl__free_headers(pCtx, p); \
																SG_ASSERT(!SG_context__has_err(pCtx)); \
																SG_context__pop_level(pCtx); \
																p=NULL; )

#define SG_CURL_STATE_FILE_NULLFREE(pCtx,p)		SG_STATEMENT(	SG_context__push_level(pCtx); \
																SG_curl__state_file__free(pCtx,p); \
																SG_ASSERT(!SG_context__has_err(pCtx)); \
																SG_context__pop_level(pCtx); p=NULL;  )

#define SG_CURL_STATE_STRING_NULLFREE(pCtx,p)	SG_STATEMENT(	SG_context__push_level(pCtx); \
																SG_curl__state_string__free(pCtx,p); \
																SG_ASSERT(!SG_context__has_err(pCtx)); \
																SG_context__pop_level(pCtx); p=NULL;  )

END_EXTERN_C;

#endif//H_SG_LIBCURL_PROTOTYPES_H
