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
 * @file sg_uridispatch_private_typedefs.h
 *
 *
 * This file contains the definitions of the "handles" returned
 * by SG_uridispatch functions. In the public interface they are
 * void pointers, and the internals are not exposed.
 *
 * Also contains some other private definitions.
 */

#ifndef H_SG_URIDISPATCH_PRIVATE_TYPEDEFS_H
#define H_SG_URIDISPATCH_PRIVATE_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////


typedef struct _request_handle__struct _request_handle; /// The structure behind a "RequestHandle".
typedef struct _response_handle__struct _response_handle; /// The structure behind a "ResponseHandle".


// When implementing this callback, there are two ways an error can be
// thrown: using pCtx (the traditional approach), or using ppResponseHandle.
// The error information will eventially be passed out to the user by way
// of ppResponseHandle, so the support of the traditional pCtx output is
// for your convenience--we'll create the Response Handle for you.
// If an error is thrown, we will call your pOnAfterAborted function
// immediately afterward (regardless of which mechanism is used).
// Please do not return both kinds of errors at once. If you do, we'll throw
// out the one in ppResponseHandle.
typedef void _request_chunk_cb(SG_context * pCtx, _response_handle ** ppResponseHandle, SG_byte * pBuffer, SG_uint32 bufferLength, void * pContext);

// Do what you need to do to finish processing the request, clean up the
// memory in pContext, and return a Response Handle. If you don't return
// a Response Handle or throw an error, we'll assume the request completed
// successfully but with no output, and thusly create an empty "200 OK"
// response for you.
//
// As with a _request_chunk_cb, we'll handle errors you throw (using pCtx)
// and create a Response Handle for you that contains the appropriate
// error message.
// Also, as with a _request_chunk_cb, please do not both return a Response
// Handle and throw an error in pCtx. We'll discard the Response Handle.
// Note: even if you throw an error in pCtx, we will *not* subsequently call
// your pOnAfterAborted function. So clean up the memory no matter what.
typedef void _request_finish_cb(SG_context * pCtx, const SG_audit * pAudit, void * pContext, _response_handle ** ppResponseHandle);

// Clean up the memory in pContext.
// The request has been aborted, either by the user (of SG_uridispatch) or
// because of an error thrown by your pChunk callback.
// Do not throw an error from a _request_aborted_cb. It will be ignored.
typedef void _request_aborted_cb(SG_context * pCtx, void * pContext);

struct _request_handle__struct
{
    // For all intents and purposes, we never actually "own" this pointer (though technically we do
    // inbetween calls into SG_uridispatch__*), which is to say: we're never responsible for freeing it.
    SG_context * pCtx;

    SG_audit audit;
    SG_uint64 contentLength;

    _request_chunk_cb * pChunk;
    _request_finish_cb * pFinish;
    _request_aborted_cb * pOnAfterAborted;
    void * pContext;

    SG_uint64 processedLength;
};


// If you throw an error we will cut the response short, but we have no
// way of showing relaying any error information to the user (of
// SG_uridispatch), so we'll just call SG_log_current_error_urgent on it.
// Then we'll call your pOnAfterAborted function.
typedef void _response_chunk_cb(SG_context * pCtx, SG_uint64 processedLength, SG_byte * pBuffer, SG_uint32 bufferLength, void * pContext);

// Clean up the memory in pContext. None of your callbacks will ever be
// called into with it again.
// Do not throw an eror. It will be ignored.
typedef void _response_finished_cb(SG_context * pCtx, void * pContext);

struct _response_handle__struct
{
    // For all intents and purposes, we never actually "own" this pointer (though technically we do
    // inbetween calls into SG_uridispatch__*), which is to say: we're never responsible for freeing it.
    SG_context * pCtx;

    const char * pHttpStatusCode;
	SG_uint64	contentLength;

	SG_vhash *pHeaders;

    _response_chunk_cb  * pChunk;
    _response_finished_cb * pOnAfterFinished;
    _response_finished_cb * pOnAfterAborted;
    void * pContext;

    SG_uint64 processedLength;
};


//////////////////////////////////////////////////////////////////


typedef enum
{
    SG_contenttype__unspecified,

    SG_contenttype__json,
    SG_contenttype__text,
    SG_contenttype__xhtml,
	SG_contenttype__html,
	SG_contenttype__fragball,
    SG_contenttype__xml,
	SG_contenttype__default,
} SG_contenttype;


typedef struct
{
    const char * pUri;
	const char * pQueryString;
    const char * pRequestMethod;
	const char * pHost;
	const char * pUserAgent;
	SG_bool isSsl;
    SG_contenttype accept;
    SG_uint64 contentLength;
    const char * pFrom;
	SG_int64 ifModifiedSince;
	SG_int64 localIfModifiedSince;
} _request_headers;


#define SG_HTTP_STATUS_OK	"200 OK"
#define SG_HTTP_STATUS_MOVED_PERMANENTLY	"301 Moved Permanently"
#define SG_HTTP_STATUS_NOT_MODIFIED		"304 Not Modified"


// For when all else fails. These are not valid handles, so don't try to access members of them.
extern _response_handle * const MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE;
extern _response_handle * const GENERIC_BAILOUT_RESPONSE_HANDLE;


//////////////////////////////////////////////////////////////////


/**
 * Used for resource-specific template text replacement. A template keyword (e.g. TITLE) is passed in
 * via 'keyword'; the proper replacement is sent back in 'replacement'. If a replacement is not known,
 * leave the passed-in value of replacement alone.
 */
typedef void _replacer_cb(SG_context * pCtx, const _request_headers *pRequestHeaders, SG_string *keyword, SG_string *replacement);


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif
