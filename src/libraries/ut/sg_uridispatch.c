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
 * @file sg_uridispatch.c
 *
 *
 * Quite simply the implementation of sg_uridispatch_prototypes.h.
 * These functions mainly just handle the initialization and cleanup
 * processes needed by the general SG_uridispatch interface. The
 * functions that actually get and store content are located in
 * the other sg_uridispatch_*.c file(s).
 */

#include <sg.h>
#include "sg_uridispatch_private_typedefs.h"
#include "sg_uridispatch_private_prototypes.h"

//////////////////////////////////////////////////////////////////


#define MALLOC_FAIL_BAILOUT_CONTENT_TYPE "text/plain;charset=UTF-8"
#define MALLOC_FAIL_BAILOUT_CONTENT_LENGTH 26
#define MALLOC_FAIL_BAILOUT_MESSAGE_BODY "Memory allocation failure."

#define GENERIC_BAILOUT_CONTENT_TYPE "text/plain;charset=UTF-8"
#define GENERIC_BAILOUT_CONTENT_LENGTH 76
#define GENERIC_BAILOUT_MESSAGE_BODY "An unkown error occurred. More details might exist in the server's log file."

#define FAIL_UNIT_TESTS() SG_ASSERT(SG_FALSE && "Assert can be ignored. Here to make unit tests fail if it ever happens.")


static SG_int32 _find_in(const char * pFind, const char * pIn)
{
    SG_int32 i = 0;

    if(pFind==NULL||pIn==NULL)
        return -1;

    while(pIn[i]!='\0')
    {
        SG_int32 j = 0;
        while(pFind[j]!='\0' && pIn[i+j]!='\0' && pFind[j]==pIn[i+j])
            ++j;
        if(pFind[j]=='\0')
            return i;
        if(pIn[i+j]=='\0')
            return -1;
        ++i;
    }

    return -1;
}
static SG_contenttype _figure_accept_header(const char * pAccept)
{
    if(pAccept==NULL || pAccept[0]=='\0')
        return SG_contenttype__unspecified;

    //todo: Ugly, but works... for now.
    if(_find_in("application/json", pAccept)>=0)
        return SG_contenttype__json;
	else if ((_find_in("application/xml", pAccept)>=0) || 
		     (_find_in("text/xml", pAccept)>=0) ||
			 (_find_in("application/atom+xml", pAccept) >= 0)
			)
		return SG_contenttype__xml;
	else if(_find_in("application/fragball", pAccept)>=0)
		return SG_contenttype__fragball;
    else
        return SG_contenttype__unspecified;
}


//////////////////////////////////////////////////////////////////

void SG_uridispatch__request(const char * pUri, const char *pQueryString, const char * pRequestMethod, SG_bool isSsl, const char *host, const char * pAccept, const char * pUserAgent, const char * pFrom, const char *pIfModifiedSince, void ** ppResponseHandle)
{
    _request_handle * pRequestHandle = NULL;

    SG_uridispatch__begin_request(pUri, pQueryString, pRequestMethod, isSsl, host, pAccept, pUserAgent, 0, pFrom, pIfModifiedSince, (void**)&pRequestHandle, ppResponseHandle);

    if(pRequestHandle!=NULL) // This probably shouldn't happen...
    {
        SG_context * pCtx = pRequestHandle->pCtx;
        FAIL_UNIT_TESTS();
        SG_ERR_IGNORE(  SG_log_urgent(pCtx, "A Request Handle was created even though the request had no message body.")  );
        SG_ERR_IGNORE(  _request_handle__on_after_aborted(pCtx, pRequestHandle)  );
        _request_handle__nullfree(&pRequestHandle);
        SG_CONTEXT_NULLFREE(pCtx);
        if(ppResponseHandle!=NULL)
            *ppResponseHandle = GENERIC_BAILOUT_RESPONSE_HANDLE;
    }
}


static void _dupMinusSlashes(
	SG_context *pCtx,
	const char *uri,
	const char **ppDupUri)
{
	char *luri = NULL;
	char *last = NULL;

	SG_ERR_CHECK_RETURN(  SG_strdup(pCtx, uri, &luri)  );

	last = luri + strlen(luri) - 1;

	while ((last >= luri) && (*last == '/'))
		*last-- = '\0';

	*ppDupUri = (const char *)luri;
}



void SG_uridispatch__begin_request(const char * pUri, const char * pQueryString, const char * pRequestMethod, SG_bool isSsl, const char *host, const char * pAccept, const char * pUserAgent, SG_uint64 contentLength, const char * pFrom, const char *pIfModifiedSince, void ** ppRequestHandle__voidpp, void ** ppResponseHandle__voidpp)
{
    _request_handle ** ppRequestHandle = (_request_handle**)ppRequestHandle__voidpp;
    _response_handle ** ppResponseHandle = (_response_handle**)ppResponseHandle__voidpp;

    SG_context * pCtx = NULL;

    _request_headers requestHeaders;
    char ** ppUriSubstrings = NULL;
    SG_uint32 uriSubstringsCount = 0;
	const char *luri = NULL;

    // First check for disaster cases.
    {
        SG_error err;
        err = SG_context__alloc(&pCtx);

        if(SG_IS_ERROR(err) || pCtx==NULL)
        {
            if(ppRequestHandle)
                *ppRequestHandle = NULL;
            if(ppResponseHandle)
                *ppResponseHandle = MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE;
            return;
        }

        if(ppResponseHandle==NULL)
        {
            SG_ERR_IGNORE(  SG_log_urgent(pCtx,"SG_uridispatch__begin_request() called with (ppResponseHandle==NULL)!!")  );
            if(ppRequestHandle!=NULL)
                *ppRequestHandle = NULL;
            SG_CONTEXT_NULLFREE(pCtx);
            return;
        }

        if(ppRequestHandle==NULL)
        {
            SG_ERR_IGNORE(  SG_log_urgent(pCtx,"SG_uridispatch__begin_request() called with (ppResponseHandle==NULL)!!")  );
            if(ppResponseHandle!=NULL)
                *ppResponseHandle = GENERIC_BAILOUT_RESPONSE_HANDLE;
            SG_CONTEXT_NULLFREE(pCtx);
            return;
        }
    }

    // Tie up a few possible loose ends.

    if(*ppRequestHandle!=NULL)
    {
         // Just in case this wasn't null coming in...
        SG_ERR_IGNORE(  SG_log(pCtx, "SG_uridispatch__begin_request() called with non-null *ppRequestHandle.")  );
        *ppRequestHandle = NULL;
    }
    if(*ppResponseHandle!=NULL)
    {
         // Just in case this wasn't null coming in...
        SG_ERR_IGNORE(  SG_log(pCtx, "SG_uridispatch__begin_request() called with non-null *ppResponseHandle.")  );
        *ppResponseHandle = NULL;
    }

    // Ok, everything is normal. Proceed.

    SG_ERR_IGNORE(  SG_log_debug(pCtx, "Handling %s request: %s", pRequestMethod, pUri)  );

    // First gather up the request headers into a struct and break the
    // uri up into substrings as divided by slashes. This is all for the
    // convenience of the rest of the functions in SG_uridispatch_*.c
    // which do all the real work.
    {
		SG_int64	ifModifiedSince = -1;
		SG_int64	localIfModifiedSince = -1;

		if (pIfModifiedSince != NULL)
		{
			SG_time__parseRFC850(pCtx, pIfModifiedSince, &ifModifiedSince, &localIfModifiedSince);
			if (SG_context__has_err(pCtx))
			{
				ifModifiedSince = -1;
				localIfModifiedSince = -1;
				SG_context__err_reset(pCtx);
			}
		}

        if(pUri==NULL)
            requestHeaders.pUri = "";
        else if(pUri[0]=='/')
            requestHeaders.pUri = pUri+1;
        else
            requestHeaders.pUri = pUri;
        requestHeaders.pRequestMethod = pRequestMethod;
        requestHeaders.accept = _figure_accept_header(pAccept);
        requestHeaders.contentLength = contentLength;
		requestHeaders.ifModifiedSince = ifModifiedSince;
		requestHeaders.localIfModifiedSince = localIfModifiedSince;
        requestHeaders.pFrom = pFrom;
		requestHeaders.pQueryString = pQueryString;
		requestHeaders.pHost = host;
		requestHeaders.isSsl = isSsl;
		requestHeaders.pUserAgent = pUserAgent;

        if(! SG_context__has_err(pCtx))
			_dupMinusSlashes(pCtx, requestHeaders.pUri, &luri);

        if(! SG_context__has_err(pCtx))
		    SG_string__split__sz_asciichar(pCtx, luri, '/', 256, &ppUriSubstrings, &uriSubstringsCount);

		SG_ERR_IGNORE(  SG_NULLFREE(pCtx, luri)  );

        if(SG_context__has_err(pCtx))
        {
            SG_ASSERT(ppUriSubstrings==NULL);

            *ppResponseHandle = _create_response_handle_for_error(pCtx);
            if(*ppResponseHandle==MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE || *ppResponseHandle==GENERIC_BAILOUT_RESPONSE_HANDLE)
                SG_CONTEXT_NULLFREE(pCtx);
            return;
        }

        SG_ASSERT(ppUriSubstrings!=NULL);
    }

    // Now call _begin_request which does the real work. Then make sure
    // everything played nice and we got an appropriate response.
    {
        _dispatch(pCtx, &requestHeaders, (const char**)ppUriSubstrings, uriSubstringsCount, ppRequestHandle, ppResponseHandle);

        if(SG_context__has_err(pCtx))
        {
            if(*ppRequestHandle!=NULL)
            {
                FAIL_UNIT_TESTS();
                SG_ERR_IGNORE(  SG_log_urgent(pCtx, "Error thrown after Request Handle constructed!")  );
                SG_ERR_IGNORE(  _request_handle__on_after_aborted(pCtx, *ppRequestHandle)  );
                _request_handle__nullfree(ppRequestHandle);
            }
            if(*ppResponseHandle!=NULL)
            {
                FAIL_UNIT_TESTS();
                SG_ERR_IGNORE(  SG_log_urgent(pCtx, "Error thrown after Response Handle constructed!")  );
                SG_ERR_IGNORE(  _response_handle__on_after_aborted(pCtx, *ppResponseHandle)  );
                _response_handle__nullfree(pCtx, ppResponseHandle);
            }

            *ppResponseHandle = _create_response_handle_for_error(pCtx);
            SG_context__err_reset(pCtx);
        }
        else if((*ppRequestHandle!=NULL) && (*ppResponseHandle!=NULL))
        {
            FAIL_UNIT_TESTS();
            SG_ERR_IGNORE(  SG_log_urgent(pCtx, "_begin_request() generated a Request Handle AND a Response Handle. Discarding the Request Handle.")  );
            SG_ERR_IGNORE(  _request_handle__on_after_aborted(pCtx, *ppRequestHandle)  );
           _request_handle__nullfree(ppRequestHandle);
        }
        else if((*ppRequestHandle==NULL) && (*ppResponseHandle==NULL))
        {
            _response_handle__alloc(pCtx, ppResponseHandle, SG_HTTP_STATUS_OK, NULL, 0, NULL, NULL, NULL, NULL);
            if(SG_context__has_err(pCtx)||ppResponseHandle==NULL)
            {
                *ppResponseHandle = _create_response_handle_for_error(pCtx);
                SG_context__err_reset(pCtx);
            }
        }

        // When all is said and done, we should be returning either a Request Handle or a Response Handle.
        // We've just made sure of that.
        SG_ASSERT( (*ppRequestHandle==NULL) != (*ppResponseHandle==NULL) );
    }

    // Finally, don't forget to clean up the memory used by the uri substrings list!
	SG_ERR_IGNORE(  SG_freeStringList(pCtx, (const char ***)&ppUriSubstrings, uriSubstringsCount)  );

    // And in certain special cases we're done with the pCtx and about to lose our last copy of the pointer.
    if(*ppResponseHandle==MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE || *ppResponseHandle==GENERIC_BAILOUT_RESPONSE_HANDLE)
        SG_CONTEXT_NULLFREE(pCtx);
}

void SG_uridispatch__chunk_request_body(SG_byte * pBuffer, SG_uint32 bufferLength, void ** ppRequestHandle__voidpp, void ** ppResponseHandle__voidpp)
{
    _request_handle ** ppRequestHandle = (_request_handle**)ppRequestHandle__voidpp;
    _response_handle ** ppResponseHandle = (_response_handle**)ppResponseHandle__voidpp;

    SG_context * pCtx = NULL;

    // First check for disaster cases. And set pCtx from the request handle (if it exists).
    {
        if(ppRequestHandle==NULL || *ppRequestHandle==NULL)
        {
            SG_context__alloc(&pCtx);
            if(pCtx!=NULL)
                SG_log_urgent(pCtx,"SG_uridispatch__chunk_request_body() called with null Request Handle!!");
            if(ppResponseHandle!=NULL)
                *ppResponseHandle = GENERIC_BAILOUT_RESPONSE_HANDLE;
            SG_CONTEXT_NULLFREE(pCtx);
            return;
        }

        pCtx = (*ppRequestHandle)->pCtx;

        if(ppResponseHandle==NULL)
        {
            SG_ERR_IGNORE(  SG_log_urgent(pCtx,"SG_uridispatch__chunk_request_body() called with (ppResponseHandle==NULL)!!")  );
            SG_ERR_IGNORE(  _request_handle__on_after_aborted(pCtx, *ppRequestHandle)  );
            SG_CONTEXT_NULLFREE(pCtx);
            _request_handle__nullfree(ppRequestHandle);
            return;
        }
    }

    // Tie up a few possible loose ends.

    if(*ppResponseHandle!=NULL)
    {
         // Just in case this wasn't null coming in...
        SG_ERR_IGNORE(  SG_log(pCtx, "SG_uridispatch__begin_request() called with non-null *ppResponseHandle.")  );
        *ppResponseHandle = NULL;
    }

    if(pBuffer==NULL)
    {
        SG_ERR_IGNORE(  SG_log(pCtx, "SG_uridispatch__begin_request() called with null buffer.")  );
        return;
    }
    if(bufferLength==0)
    {
        SG_ERR_IGNORE(  SG_log(pCtx, "SG_uridispatch__begin_request() called with a zero-length buffer.")  );
        return;
    }

    // Ok, everything is normal. Proceed.

    {
        SG_bool isLastChunk = SG_FALSE;

        SG_ASSERT((*ppRequestHandle)->processedLength < (*ppRequestHandle)->contentLength);
        if((*ppRequestHandle)->processedLength + bufferLength >= (*ppRequestHandle)->contentLength)
        {
            bufferLength = (SG_uint32)((*ppRequestHandle)->contentLength - (*ppRequestHandle)->processedLength);
            isLastChunk = SG_TRUE;
        }

        SG_ASSERT((*ppRequestHandle)->pChunk!=NULL);
        (*ppRequestHandle)->pChunk(pCtx, ppResponseHandle, pBuffer, bufferLength, (*ppRequestHandle)->pContext);

        if(SG_context__has_err(pCtx))
        {
            if(*ppResponseHandle!=NULL)
            {
                FAIL_UNIT_TESTS();
                SG_ERR_IGNORE(  SG_log_urgent(pCtx, "Error thrown after Response Handle constructed!")  );
                SG_ERR_IGNORE(  _response_handle__on_after_aborted(pCtx, *ppResponseHandle)  );
                _response_handle__nullfree(pCtx, ppResponseHandle);
            }

            *ppResponseHandle = _create_response_handle_for_error(pCtx);
            SG_context__err_reset(pCtx);

            SG_ERR_IGNORE(  _request_handle__on_after_aborted(pCtx, *ppRequestHandle)  );
            _request_handle__nullfree(ppRequestHandle);
        }
        else if(*ppResponseHandle!=NULL)
        {
            SG_ERR_IGNORE(  _request_handle__on_after_aborted(pCtx, *ppRequestHandle)  );
            _request_handle__nullfree(ppRequestHandle);
        }
        else if(isLastChunk)
        {
            _request_handle__finish(pCtx, *ppRequestHandle, ppResponseHandle);
            if(SG_context__has_err(pCtx))
            {
                if(*ppResponseHandle!=NULL)
                {
                    FAIL_UNIT_TESTS();
                    SG_ERR_IGNORE(  SG_log_urgent(pCtx, "Error thrown after Response Handle constructed!")  );
                    SG_ERR_IGNORE(  _response_handle__on_after_aborted(pCtx, *ppResponseHandle)  );
                    _response_handle__nullfree(pCtx, ppResponseHandle);
                }

                *ppResponseHandle = _create_response_handle_for_error(pCtx);
                SG_context__err_reset(pCtx);
            }
            else if(*ppResponseHandle==NULL)
            {
				_response_handle__alloc(pCtx, ppResponseHandle, SG_HTTP_STATUS_OK, NULL, 0, NULL, NULL, NULL, NULL);
                if(SG_context__has_err(pCtx))
                {
                    *ppResponseHandle = _create_response_handle_for_error(pCtx);
                    SG_context__err_reset(pCtx);
                }
            }
            _request_handle__nullfree(ppRequestHandle);
        }
        else
        {
            (*ppRequestHandle)->processedLength += bufferLength;
        }
    }

    // When all is said and done, we should be returning either the original Request Handle or a Response Handle, but not both.
    // We've just made sure of that (all 4 cases in the previous if statement should check out).
    SG_ASSERT( (*ppRequestHandle==NULL) != (*ppResponseHandle==NULL) );

    // In certain special cases we're done with the pCtx and about to lose our last copy of the pointer.
    if(*ppResponseHandle==MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE || *ppResponseHandle==GENERIC_BAILOUT_RESPONSE_HANDLE)
        SG_CONTEXT_NULLFREE(pCtx);
}

void SG_uridispatch__abort_request(void ** ppRequestHandle__voidpp)
{
    _request_handle ** ppRequestHandle = (_request_handle**)ppRequestHandle__voidpp;

    SG_int_to_string_buffer tmp1, tmp2;
    SG_context * pCtx = NULL;

    if(ppRequestHandle==NULL || *ppRequestHandle==NULL)
        return;

    pCtx = (*ppRequestHandle)->pCtx;

    SG_ERR_IGNORE(  SG_log(pCtx, "POST aborted after %s/%s bytes received.",
        SG_uint64_to_sz((*ppRequestHandle)->processedLength,tmp1),
        SG_uint64_to_sz((*ppRequestHandle)->contentLength,tmp2))  );

    SG_ERR_IGNORE(  _request_handle__on_after_aborted(pCtx, *ppRequestHandle)  );
    SG_CONTEXT_NULLFREE(pCtx);
    _request_handle__nullfree(ppRequestHandle);
}


//////////////////////////////////////////////////////////////////

void SG_uridispatch__get_response_headers(
    void ** ppResponseHandle__voidpp,
	const char ** statusCode,
	char *** pppHeaders,
	SG_uint32 *nHeaders)
{
    _response_handle ** ppResponseHandle = (_response_handle**)ppResponseHandle__voidpp;
	SG_vhash *vhHeaders = NULL;
	SG_context *pCtx = NULL;
	SG_bool	tempCtx = SG_FALSE;
	const char *statcode = NULL;
	SG_uint32 i = 0;
	SG_string *buf = NULL;
	SG_int64	contentLength = 0;
	SG_bool	canFreeCtx = SG_TRUE;

    if(ppResponseHandle==NULL||*ppResponseHandle==NULL)
    {
		tempCtx = SG_TRUE;
        SG_context__alloc(&pCtx);
        if(pCtx!=NULL)
            SG_log_urgent(pCtx,"SG_uridispatch__get_response_headers() called with null Response Handle!!");

		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &vhHeaders)  );
    }
    else if(*ppResponseHandle==MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE)
    {
		tempCtx = SG_TRUE;
        SG_context__alloc(&pCtx);
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &vhHeaders)  );

        statcode = "500 Internal Server Error";

		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vhHeaders, "Content-Type", MALLOC_FAIL_BAILOUT_CONTENT_TYPE)  );
		contentLength = MALLOC_FAIL_BAILOUT_CONTENT_LENGTH;
    }
    else if(*ppResponseHandle==GENERIC_BAILOUT_RESPONSE_HANDLE)
    {
		tempCtx = SG_TRUE;
        SG_context__alloc(&pCtx);
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &vhHeaders)  );

        statcode = "500 Internal Server Error";

		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, vhHeaders, "Content-Type", GENERIC_BAILOUT_CONTENT_TYPE)  );
		contentLength = GENERIC_BAILOUT_CONTENT_LENGTH;
    }
    else
    {
        statcode = (*ppResponseHandle)->pHttpStatusCode;
		vhHeaders = (*ppResponseHandle)->pHeaders;
		(*ppResponseHandle)->pHeaders = NULL;

		contentLength = (*ppResponseHandle)->contentLength;

		pCtx = (*ppResponseHandle)->pCtx;
	}

	*statusCode = statcode;

	SG_ERR_CHECK(  SG_vhash__count(pCtx, vhHeaders, nHeaders)  );
	++(*nHeaders);		// one for content length
	*pppHeaders = (char **)SG_malloc(*nHeaders * sizeof(char *));

	for ( i = 0; i < (*nHeaders - 1); ++i )
	{
		const char *vari;
		const SG_variant *value;

		SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, vhHeaders, i, &vari, &value)  );

		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &buf)  );

		if (value->type == SG_VARIANT_TYPE_INT64)
		{
			SG_int64	ival = 0;

			SG_ERR_CHECK(  SG_variant__get__int64(pCtx, value, &ival)  );
			SG_ERR_CHECK(  SG_string__sprintf(pCtx, buf, "%s: %lld", vari, ival)  );
		}
		else if (value->type != SG_VARIANT_TYPE_NULL)
		{
			const char *strval = NULL;

			SG_ERR_CHECK(  SG_variant__get__sz(pCtx, value, &strval)  );
			SG_ERR_CHECK(  SG_string__sprintf(pCtx, buf, "%s: %s", vari, strval)  );
		}

		SG_ERR_CHECK(  SG_strdup(pCtx, SG_string__sz(buf), *pppHeaders + i)  );
		SG_STRING_NULLFREE(pCtx, buf);
	}

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &buf)  );
	SG_ERR_CHECK(  SG_string__sprintf(pCtx, buf, "Content-Length: %lld", contentLength)  );
	SG_ERR_CHECK(  SG_strdup(pCtx, SG_string__sz(buf), *pppHeaders + *nHeaders - 1)  );
	SG_STRING_NULLFREE(pCtx, buf);

	if (! tempCtx)
	{
        if((*ppResponseHandle)->contentLength==0) // && (*ppResponseHandle)->pOnAfterFinished==NULL)
        {
            pCtx = (*ppResponseHandle)->pCtx;
            // Note: We can do this because it is known that freeing the memmory owned by *ppResponseHandle
            // and (*ppResponseHandle)->pContext will not free any of the memory that we're passing out in the
            // parameters. If we do ever create headers that get alloc'ed by the Response Handle and therefor
            // need to be free'ed by it, we won't be able to do the clean-up here (else the user will get a
            // memory access violation). We'll probably want to: modify SG_uridispatch__chunk_response_body
            // to handle a zero-length content better, and make a note in the sg_uridispatch_prototypes.h
            // header file that the user may need to call "chunk" even if content length is zero.
            SG_ERR_IGNORE(  _response_handle__on_after_finished(pCtx, *ppResponseHandle)  );

            _response_handle__nullfree(pCtx, ppResponseHandle);
        }
		else
		{
			canFreeCtx = SG_FALSE;
		}
    }


fail:
	SG_VHASH_NULLFREE(pCtx, vhHeaders);
	SG_STRING_NULLFREE(pCtx, buf);

	if (canFreeCtx)
	{
		SG_CONTEXT_NULLFREE(pCtx);
	}
}

void SG_uridispatch__chunk_response_body(void ** ppResponseHandle__voidpp, SG_byte * pBuffer, SG_uint32 bufferLength, SG_uint32 * pLengthGot)
{
    _response_handle ** ppResponseHandle = (_response_handle**)ppResponseHandle__voidpp;

    SG_context * pCtx = NULL;

    if(ppResponseHandle==NULL||*ppResponseHandle==NULL)
    {
        SG_context__alloc(&pCtx);
        if(pCtx!=NULL)
            SG_log_urgent(pCtx,"SG_uridispatch__chunk_response_body() called with NULL handle!");
        SG_CONTEXT_NULLFREE(pCtx);
        return;
    }
    else if(*ppResponseHandle==MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE || *ppResponseHandle==GENERIC_BAILOUT_RESPONSE_HANDLE)
    {
        // This is a special case because even though we have a uridispatch__handle,
        // we don't have an SG_context. Also, if they don't get the whole message in
        // the first chunk, there's no thread-safe way to get another chunk.

        if(pBuffer!=NULL)
        {
            SG_uint32 contentLength = 0;
            const char * content = NULL;

            if(*ppResponseHandle==MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE)
            {
                contentLength = MALLOC_FAIL_BAILOUT_CONTENT_LENGTH;
                content = MALLOC_FAIL_BAILOUT_MESSAGE_BODY;
            }
            else
            {
                contentLength = GENERIC_BAILOUT_CONTENT_LENGTH;
                content = GENERIC_BAILOUT_MESSAGE_BODY;
            }

            memcpy(pBuffer, content, SG_MIN(bufferLength, contentLength));

            if(pLengthGot!=NULL)
                *pLengthGot = SG_MIN(bufferLength, contentLength);
        }

        *ppResponseHandle=NULL;
        return;
    }

    pCtx = (*ppResponseHandle)->pCtx;

    if(pLengthGot==NULL)
    {
        SG_ERR_IGNORE(  SG_log(pCtx, "SG_uridispatch__chunk_response_body() called with null pLengthGot.")  );
        return;
    }

    if(pBuffer==NULL)
    {
        SG_ERR_IGNORE(  SG_log(pCtx, "SG_uridispatch__chunk_response_body() called with null buffer.")  );
        *pLengthGot = 0;
        return;
    }
    if(bufferLength==0)
    {
        SG_ERR_IGNORE(  SG_log(pCtx, "SG_uridispatch__chunk_response_body() called with a zero-length buffer.")  );
        *pLengthGot = 0;
        return;
    }

    // Ok, if we've gotten this far, everything looks fairly normal. Now lets do the chunk.
    {
        SG_bool isLastChunk = SG_FALSE;

        SG_ASSERT((*ppResponseHandle)->processedLength < (*ppResponseHandle)->contentLength);

        if(((SG_uint64)bufferLength)>=((*ppResponseHandle)->contentLength-(*ppResponseHandle)->processedLength))
        {
            bufferLength = (SG_uint32)((*ppResponseHandle)->contentLength-(*ppResponseHandle)->processedLength);
            isLastChunk = SG_TRUE;
        }

        SG_ASSERT((*ppResponseHandle)->pChunk!=NULL);
        (*ppResponseHandle)->pChunk(pCtx, (*ppResponseHandle)->processedLength, pBuffer, bufferLength, (*ppResponseHandle)->pContext);
        if(SG_context__has_err(pCtx))
        {
            *pLengthGot = 0;

            SG_log_current_error_urgent(pCtx);
            SG_ERR_IGNORE(  _response_handle__on_after_aborted(pCtx, *ppResponseHandle)  );
            _response_handle__nullfree(pCtx, ppResponseHandle);
            SG_CONTEXT_NULLFREE(pCtx);
        }
        else
        {
            *pLengthGot = bufferLength;

            if(isLastChunk)
            {
                SG_ERR_IGNORE(  _response_handle__on_after_finished(pCtx, *ppResponseHandle)  );
                _response_handle__nullfree(pCtx, ppResponseHandle);
                SG_CONTEXT_NULLFREE(pCtx);
            }
            else
                (*ppResponseHandle)->processedLength += bufferLength;
        }
    }
}

void SG_uridispatch__abort_response(void ** ppResponseHandle__voidpp)
{
    _response_handle ** ppResponseHandle = (_response_handle**)ppResponseHandle__voidpp;

    if(ppResponseHandle==NULL || *ppResponseHandle==NULL)
        return;
    else if(*ppResponseHandle==MALLOC_FAIL_BAILOUT_RESPONSE_HANDLE || *ppResponseHandle==GENERIC_BAILOUT_RESPONSE_HANDLE)
    {
        *ppResponseHandle = NULL;
        return;
    }
    else
    {
        SG_context * pCtx = (*ppResponseHandle)->pCtx;
        SG_ERR_IGNORE(  _response_handle__on_after_aborted(pCtx, *ppResponseHandle)  );
        _response_handle__nullfree(pCtx, ppResponseHandle);
        SG_CONTEXT_NULLFREE(pCtx);
    }
}


//////////////////////////////////////////////////////////////////
// sg_lib_uridispatch__global_cleanup is prototyped in sg_lib__private.h, not sg_uridispatch_prototypes.h.

extern SG_pathname * _sg_uridispatch__templatePath;
void sg_lib_uridispatch__global_cleanup(SG_context * pCtx)
{
    SG_PATHNAME_NULLFREE(pCtx, _sg_uridispatch__templatePath);
}

