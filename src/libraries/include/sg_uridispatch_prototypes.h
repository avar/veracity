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
 * @file sg_uridispatch_prototypes.h
 *
 *
 * This is the part of sprawl's "SWIG-friendly api" designed specifically
 * for http servers to interact with. We also use it internally for our
 * own sprawl applications that have an http server embeded into them.
 */

#ifndef H_SG_URIDISPATCH_PROTOTYPES_H
#define H_SG_URIDISPATCH_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////


// Making the request:

	// Use this function if your request does not have a message body (aka "post data").
	void SG_uridispatch__request(
		const char * pUri,
		const char * pQueryString,
		const char * pRequestMethod,
		SG_bool isSsl,
		const char * pHost,
        const char * pAccept,
		const char * pUserAgent,
        const char * pFrom,
		const char * pIfModifiedSince,

		// Output parameter. Use this handle to get the response.
		void ** ppResponseHandle
		);


	// Use these this function if your request has a message body (aka "post data").
	void SG_uridispatch__begin_request(
		const char * pUri,
		const char * pQueryString,
		const char * pRequestMethod,
		SG_bool isSsl,
		const char * pHost,
        const char * pAccept,
		const char * pUserAgent,
		SG_uint64 contentLength,
        const char * pFrom,
		const char * pIfModifiedSince,

		// One of these will get set to non-null and the other one set to null:
		void ** ppRequestHandle,
		void ** ppResponseHandle);


	// The request's message body is handled in chunks so that we can handle large content lengths.
	void SG_uridispatch__chunk_request_body(
		SG_byte * pBuffer,
		SG_uint32 bufferLength,

		void ** ppRequestHandle,   // Gets set to NULL when the request is finished (note possible early termination).
		void ** ppResponseHandle); // Gets set (non-NULL) when the request is finished (note possible early termination).


	// For the indecisive. A way to say "nevermind".
	void SG_uridispatch__abort_request(
		void ** ppRequestHandle);


//////////////////////////////////////////////////////////////////


// Receiving the response:


	// This is the first thing you'll want to call in retreiving the response.
	void SG_uridispatch__get_response_headers(
		void ** pResponseHandle, // Gets set to NULL if you don't need to call SG_uridispatch__chunk_response_body.

		const char **statusCode,
		char *** pppHeaders,
		SG_uint32 *nHeaders);


	// Keep calling this until the Response Handle has been set to NULL.
	// If an error occurs, you may receive less bytes than the original content length, but never more.
	void SG_uridispatch__chunk_response_body(
		void ** ppResponseHandle, // Gets set to NULL when the response is finished.

		SG_byte * pBuffer,
		SG_uint32 bufferLength,

		SG_uint32 * pLengthGot);


	// For the indecisive. A way to say "nevermind".
	void SG_uridispatch__abort_response(
		void ** ppResponseHandle);


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif
