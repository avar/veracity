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
 * @file sg_uridispatch_private_prototypes.h
 */

#ifndef H_SG_URIDISPATCH_PRIVATE_PROTOTYPES_H
#define H_SG_URIDISPATCH_PRIVATE_PROTOTYPES_H

BEGIN_EXTERN_C;

#define seq(str1,str2) (strcmp(SG_string__sz(str1),str2)==0)

//////////////////////////////////////////////////////////////////


// In this function, pCtx will either be set to SG_ERR_MALLOCFAILED, or it will
// remain SG_ERR_OK, in which case we have taken ownership of the pointer.
void _request_handle__alloc(
    SG_context * pCtx,
    _request_handle ** ppNew,
    const _request_headers * pRequestHeaders,
    _request_chunk_cb  * pChunk,
    _request_finish_cb * pFinish,
    _request_aborted_cb * pOnAfterAborted,
    void * pContext);

void _request_handle__nullfree(_request_handle ** ppThis);

void _request_handle__finish(SG_context * pCtx, _request_handle * pRequestHandle, _response_handle ** ppResponseHandle);
void _request_handle__on_after_aborted(SG_context * pCtx, _request_handle * pRequestHandle);


// In this function, pCtx will either be set to SG_ERR_MALLOCFAILED, or it will
// remain SG_ERR_OK, in which case we have taken ownership of the pointer.
void _response_handle__alloc(
    SG_context * pCtx,
    _response_handle ** ppNew,
    const char * pHttpStatusCode,
    const char * pContentType,
    SG_uint64 contentLength,
    _response_chunk_cb  * pChunk,
    _response_finished_cb * pOnAfterFinished,
    _response_finished_cb * pOnAfterAborted,
    void * pContext);

void _response_handle__nullfree(SG_context *pCtx, _response_handle ** ppThis);

void _response_handle__on_after_finished(SG_context * pCtx, _response_handle * pResponseHandle);
void _response_handle__on_after_aborted(SG_context * pCtx, _response_handle * pResponseHandle);

void _response_handle__add_header(SG_context * pCtx, _response_handle *pResponseHandle, const char *headerName, const char *headerVal);


//////////////////////////////////////////////////////////////////


_request_chunk_cb _chunk_request_to_string;
_request_aborted_cb _free_string;


//////////////////////////////////////////////////////////////////


const char * _response_header_for(SG_contenttype); // Will return NULL for SG_contenttype__unspecified.


void _create_response_handle_for_string(
    SG_context * pCtx,
    const char * pHttpStatusCode,
    SG_contenttype contentType,
    SG_string ** ppString, // On success we've taken ownership and nulled the caller's copy.
    _response_handle ** ppResponseHandle);

void _create_response_handle_for_template(
    SG_context * pCtx,
	const _request_headers * pRequestHeaders,
    const char * pHttpStatusCode,
    const char * pPage,
    _replacer_cb * pReplacer,
    _response_handle ** ppResponseHandle);

void _create_response_handle_for_blob(
    SG_context * pCtx,
    const char * pHttpStatusCode,
    SG_contenttype contentType,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    const char * pHid,
    _response_handle ** ppResponseHandle);

void _create_response_handle_for_file(
	SG_context * pCtx,
	const char * pHttpStatusCode,
	SG_contenttype contentType,
	SG_pathname** ppPathFile, // On success we've taken ownership and nulled the caller's copy.
	SG_bool bDeleteOnSuccessfulFinish,
	_response_handle ** ppResponseHandle);

// ***WE WILL ALWAYS RETURN A USABLE RESPONSE HANDLE NO MATTER WHAT, NEVER NULL***
//
// If anything catestrophic happens, we'll return either
// MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE or GENERIC_BAILOUT_RESPONSE_HANDLE. Note
// that these are technically not valid _response_handle addresses, and cannot
// store a copy of pCtx for you. So you'll probably have to free pCtx right away
// after calling this, or else you'll have a memory leak. Any other value besides
// these two special cases will be a valid _response_handle address that you now
// own and that has a copy of pCtx in it.
//
// When we finish, pCtx will be in the same state as we received it.
//
// Don't call this when not in an error state. If pCtx is SG_ERR_OK, the
// response will be a "500 Internal Server Error" of some sort.
_response_handle * _create_response_handle_for_error(SG_context * pCtx);


//////////////////////////////////////////////////////////////////


void _dispatch(
    SG_context * pCtx,

    // Input parameters:
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,

    // Output parameters:
    _request_handle ** ppRequestHandle,
    _response_handle ** pResponseHandle);


//////////////////////////////////////////////////////////////////

void _getHostName(
	SG_context *pCtx,
	const char **hostname);

void _dispatch__activity_stream(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    _response_handle ** ppResponseHandle);

// Function defined in sg_uridispatch_history.c
void _dispatch__history(SG_context * pCtx,
    _request_headers * pRequestHeaders,
	SG_repo** ppRepo,
	const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
	_request_handle ** ppRequestHandle,
    _response_handle ** ppResponseHandle);

// Function defined in sg_uridispatch_withistory.c
void _dispatch__withistory(SG_context * pCtx,
    _request_headers * pRequestHeaders,
	SG_repo ** ppRepo,
	const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _response_handle ** ppResponseHandle);

// Function defined in sg_uridispatch_withistory.c
void _burndown_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement);

// Function defined in sg_uridispatch_sync.c
void _dispatch__sync(SG_context* pCtx,
					 _request_headers* pRequestHeaders,
					 const char* pszRepoDescriptorName,
					 SG_repo** ppRepo, // On success we've taken ownership and nulled the caller's copy.
					 const char** ppUriSubstrings,
					 SG_uint32 uriSubstringsCount,
					 _request_handle** ppRequestHandle,
					 _response_handle** ppResponseHandle);


// Function defined in sg_uridispatch_todo.c
void _dispatch__todo(SG_context * pCtx,
    _request_headers * pRequestHeaders,
	SG_repo **ppRepo,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _response_handle ** ppResponseHandle);

//////////////////////////////////////////////////////////////////


#define eq(str1,str2) (strcmp(str1,str2)==0)


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif //H_SG_UNZIP_PRIVATE_H
