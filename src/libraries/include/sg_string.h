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

// ut_string.h
// A very simple UTF8 string class.
//////////////////////////////////////////////////////////////////

#ifndef H_SG_STRING_H
#define H_SG_STRING_H

BEGIN_EXTERN_C

//////////////////////////////////////////////////////////////////

#define SG_STRING__MAX_LENGTH_IN_BYTES (SG_UINT32_MAX-1)

void SG_string__alloc(SG_context * pCtx, SG_string** ppNew);

/**
 * Alloc a new SG_string with a buffer of size reserve_len.
 */
void SG_string__alloc__reserve(SG_context * pCtx, SG_string** ppNew, SG_uint32 reserve_len);

/**
 * SG_context needs an old-style version of SG_string__alloc__reserve()
 * that doesn't take a SG_context (so that it can create one).
 */
SG_error SG_string__alloc__reserve__err(SG_string** ppNew, SG_uint32 reserve_len);

void SG_string__alloc__sz(SG_context * pCtx, SG_string** ppNew, const char * sz);
void SG_string__alloc__format(SG_context * pCtx, SG_string ** ppNew, const char * szFormat, ...);
void SG_string__alloc__vformat(SG_context * pCtx, SG_string ** ppNew, const char * szFormat, va_list);

/**
 * Alloc new SG_string with initial contents from pBuf.
 * len is the length of pBuf, not the length of SG_string's buffer.
 */
void SG_string__alloc__buf_len(SG_context * pCtx, SG_string** ppNew, const SG_byte * pBuf, SG_uint32 len);
void SG_string__alloc__copy(SG_context * pCtx, SG_string** ppNew, const SG_string * pString);

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#define __SG_STRING__ALLOC__(pp,expr)		SG_STATEMENT(	SG_string * _p = NULL;										\
															expr;														\
															_sg_mem__set_caller_data(_p,__FILE__,__LINE__,"SG_string");	\
															*(pp) = _p;													)

#define SG_STRING__ALLOC(pCtx,ppNew)								__SG_STRING__ALLOC__(ppNew, SG_string__alloc              (pCtx,&_p) )
#define SG_STRING__ALLOC__RESERVE(pCtx,ppNew,reserve_len)			__SG_STRING__ALLOC__(ppNew, SG_string__alloc__reserve     (pCtx,&_p,reserve_len) )
#define SG_STRING__ALLOC__RESERVE__ERR(ppNew,reserve_len)			__SG_STRING__ALLOC__(ppNew, SG_string__alloc__reserve__err(     _p,reserve_len) )
#define SG_STRING__ALLOC__SZ(pCtx,ppNew,sz)							__SG_STRING__ALLOC__(ppNew, SG_string__alloc__sz          (pCtx,&_p,sz) )
// we can't do this trick for SG_string__alloc__format()...
#define SG_STRING__ALLOC__VFORMAT(pCtx,ppNew,szFormat,va_list)		__SG_STRING__ALLOC__(ppNew, SG_string__alloc__vformat     (pCtx,&_p,szFormat,va_list) )
#define SG_STRING__ALLOC__BUF_LEN(pCtx,ppNew,pBuf,len)				__SG_STRING__ALLOC__(ppNew, SG_string__alloc__buf_len     (pCtx,&_p,pBuf,len) )
#define SG_STRING__ALLOC__COPY(pCtx,ppNew,pString)					__SG_STRING__ALLOC__(ppNew, SG_string__alloc__copy        (pCtx,&_p,pString) )

#else

#define SG_STRING__ALLOC(pCtx,ppNew)								SG_string__alloc              (pCtx,ppNew)
#define SG_STRING__ALLOC__RESERVE(pCtx,ppNew,reserve_len)			SG_string__alloc__reserve     (pCtx,ppNew,reserve_len)
#define SG_STRING__ALLOC__RESERVE__ERR(ppNew,reserve_len)			SG_string__alloc__reserve__err(     ppNew,reserve_len)
#define SG_STRING__ALLOC__SZ(pCtx,ppNew,sz)							SG_string__alloc__sz          (pCtx,ppNew,sz)
// we can't do this trick for SG_string__alloc__format()...
#define SG_STRING__ALLOC__VFORMAT(pCtx,ppNew,szFormat,va_list)		SG_string__alloc__vformat     (pCtx,ppNew,szFormat,va_list)
#define SG_STRING__ALLOC__BUF_LEN(pCtx,ppNew,pBuf,len)				SG_string__alloc__buf_len     (pCtx,ppNew,pBuf,len)
#define SG_STRING__ALLOC__COPY(pCtx,ppNew,pString)					SG_string__alloc__copy        (pCtx,ppNew,pString)

#endif

//////////////////////////////////////////////////////////////////

void SG_string__make_space(SG_context * pCtx, SG_string* pstr, SG_uint32 space);

void SG_string__free(SG_context * pCtx, SG_string * pThis);
void SG_string__free__no_ctx(SG_string * pThis);

void SG_string__set__string(SG_context * pCtx, SG_string * pThis, const SG_string * pStringToCopy);
void SG_string__set__sz(SG_context * pCtx, SG_string * pThis, const char * sz);
void SG_string__set__buf_len(SG_context * pCtx, SG_string * pThis, const SG_byte * pBuf, SG_uint32 len);

/**
 * Adopt the given buffer and replace the internal buffer for this string.
 * This allows you to set the contents of a string to an already allocated
 * buffer without paying for another malloc and memcpy.  We assume that the
 * len == strlen(pBuf)+1.
 *
 */
void SG_string__adopt_buffer(SG_context * pCtx, SG_string * pThis, char * pBuf, SG_uint32 len);

void SG_string__append__string(SG_context * pCtx, SG_string * pThis, const SG_string * pString);
void SG_string__append__sz(SG_context * pCtx, SG_string * pThis, const char * sz);
void SG_string__append__format(SG_context * pCtx, SG_string* pThis, const char * szFormat, ...);

/**
 * Append from the provided buffer, but never grow the string.
 * Returns SG_ERR_BUFFERTOOSMALL and leaves the string unaltered if a grow was needed.
 */
void SG_string__append__sz__no_grow(SG_context * pCtx, SG_string * pThis, const char * sz);

void SG_string__append__buf_len(SG_context * pCtx, SG_string * pThis, const SG_byte * pBuf, SG_uint32 len);

void SG_string__insert__string(SG_context * pCtx, SG_string * pThis, SG_uint32 byte_offset, const SG_string * pString);
void SG_string__insert__sz(SG_context * pCtx, SG_string * pThis, SG_uint32 byte_offset, const char * sz);
void SG_string__insert__buf_len(SG_context * pCtx, SG_string * pThis, SG_uint32 byte_offset, const SG_byte * pBuf, SG_uint32 len);

void SG_string__remove(SG_context * pCtx, SG_string * pThis, SG_uint32 byte_offset, SG_uint32 len);
void SG_string__clear(SG_context * pCtx, SG_string * pThis);

/**
 * We need this old-style version for SG_context.
 */
SG_error SG_string__clear__err(SG_string * pThis);


void SG_string__replace_bytes(SG_context * pCtx,
							  SG_string * pThis,
							  const SG_byte * pBytesToFind, SG_uint32 lenBytesToFind,
							  const SG_byte * pBytesToReplaceWith, SG_uint32 lenBytesToReplaceWith,
							  SG_uint32 count, SG_bool bAdvance,
							  SG_uint32 * pNrFound);

/**
 * This sizzlin'-amazing function:
 *  - Gives you the internal SG_byte * from the string, complete with ownership of
 *    the pointer (making it your responsibility to free it)!
 *  - Frees the (rest of the) SG_string, including nulling your pointer to it!
 *  - Tells you the length of the string, in bytes, in the optional "pLength"
 *    output parameter!
 */
void SG_string__sizzle(SG_context * pCtx, SG_string ** ppThis, SG_byte ** pSz, SG_uint32 * pLength);

const char * SG_string__sz(const SG_string * pThis);
SG_uint32 SG_string__length_in_bytes(const SG_string * pThis);
SG_uint32 SG_string__bytes_allocated(const SG_string * pThis);

/**
 * Returns the number of bytes available for use (excludes null terminator and insurance padding).
 */
SG_uint32 SG_string__bytes_avail_before_grow(const SG_string * pThis);

/**
 * Returns true if the string contains SPACES or TABS.
 */
void SG_string__contains_whitespace(SG_context * pCtx, const SG_string * pThis, SG_bool * pbFound);

/**
 * fetch the nth byte in the buffer from the beginning.
 * Valid indexes are 0 thru length-1.
 */
void SG_string__get_byte_l(SG_context * pCtx, const SG_string * pThis, SG_uint32 ndx, SG_byte * pbyteReturned);

/**
 * fetch the nth byte in the buffer from the end.
 * Values indexes are 0 thru length-1.
 */
void SG_string__get_byte_r(SG_context * pCtx, const SG_string * pThis, SG_uint32 ndx, SG_byte * pbyteReturned);

SG_bool SG_string__does_buf_overlap_string(const SG_string * pThis, const SG_byte * pBuf, SG_uint32 lenBuf);

void SG_string__sprintf(SG_context * pCtx, SG_string * pThis, const char * szFormat, ...);
void SG_string__vsprintf(SG_context * pCtx, SG_string * pThis, const char * szFormat, va_list ap);

// Output is an array of charstars.
// pSubstringCount output parameter is optional. If omitted, the array is null-terminated, otherwise not necessarily.
// You, the caller, own the array and all the charstars in it. Free them.
// There will be at least one charstar in the list.
void SG_string__split__asciichar(SG_context * pCtx, const SG_string * pThis, char separator, SG_uint32 maxSubstrings, char *** pppResults, SG_uint32 * pSubstringCount);
void SG_string__split__sz_asciichar(SG_context * pCtx, const char * sz, char separator, SG_uint32 maxSubstrings, char *** pppResults, SG_uint32 * pSubstringCount);

//////////////////////////////////////////////////////////////////

void SG_string__setChunkSize(SG_string * pThis, SG_uint32 size);

//////////////////////////////////////////////////////////////////

END_EXTERN_C

#endif//H_SG_STRING_H
