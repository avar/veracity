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

// sg_string.c
// a simple expandable string.  this is intended for holding
// regular and utf8 strings.  all lengths are measured in bytes
// *NOT* utf8 characters.
//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

static const SG_uint32 INITIAL_SIZE			= 256;
static const SG_uint32 MIN_CHUNK_SIZE		= 1024;
//static const SG_uint32 MIN_CHUNK_SIZE		= 10;		// for debug only
static const SG_uint32 INSURANCE_PADDING	= 4;
static const SG_uint32 MAX_GROW_SIZE		= (4 * 1024 * 1024);

//////////////////////////////////////////////////////////////////

struct _SG_string
{
	SG_uint32		m_uiSizeInUse;		// currently used
	SG_uint32		m_uiAllocated;		// amount allocated
	SG_uint32		m_uiChunkSize;		// realloc delta
	SG_byte	*		m_pBuf;
};

//////////////////////////////////////////////////////////////////

static SG_bool _sg_string__validate(const SG_string * pThis)
{
	// each of these tests assert for debugging purposes,
	// but we allow the code to go on.  that is, if your
	// debugger allows asserts to continue, we let this
	// work.

	if (!pThis)
	{
		SG_ASSERT( pThis && "Given NULL String pointer." );
		return SG_FALSE;
	}

	if (pThis->m_pBuf==NULL)
	{
		SG_ASSERT( (pThis->m_pBuf!=NULL) && "String buffer in inconsistent state." );
		return SG_FALSE;
	}

	if (pThis->m_pBuf[pThis->m_uiSizeInUse]!=0)
	{
		SG_ASSERT( (pThis->m_pBuf[pThis->m_uiSizeInUse]==0) && "String not null terminated." );
		return SG_FALSE;
	}

	return SG_TRUE;
}

#define RETURN_ERROR_IF_INVALID(pStr)	SG_ARGCHECK_RETURN((_sg_string__validate(pStr)), pStr)

//////////////////////////////////////////////////////////////////

static SG_bool _sg_string__needs_grow(SG_string * pThis, SG_uint32 additionalSpaceNeeded, SG_uint32* pGrowToSizeRounded)
{
	SG_uint32 totalSpaceNeeded;
	SG_uint32 totalSpaceRounded;

	totalSpaceNeeded = pThis->m_uiSizeInUse + additionalSpaceNeeded + INSURANCE_PADDING;
	if (totalSpaceNeeded <= pThis->m_uiAllocated)
		return SG_FALSE;

	totalSpaceRounded = ((totalSpaceNeeded + pThis->m_uiChunkSize - 1) / pThis->m_uiChunkSize) * pThis->m_uiChunkSize;

	if (pGrowToSizeRounded)
		*pGrowToSizeRounded = totalSpaceRounded;

	return SG_TRUE;
}

static void _sg_string__grow(SG_context * pCtx, SG_string * pThis, SG_uint32 additionalSpaceNeeded)
{
	SG_uint32 growToSizeRounded;
    SG_uint32 totalSpaceNew;
	SG_byte * pNewBuf;

	RETURN_ERROR_IF_INVALID(pThis);

    /*
     * This grow function usually just doubles the allocated
     * space.  There are two exceptions:
     *
     * 1.  Once the string gets big, it stops doubling
     * and just expands each time by MAX_GROW_SIZE,
     * which at the time of this writing is set to 4 M.
     *
     * 2.  If doubling is not enough to meet the stated need,
     * it gives more.
     */

	if (!_sg_string__needs_grow(pThis, additionalSpaceNeeded, &growToSizeRounded))
		return;

    if (pThis->m_uiAllocated > MAX_GROW_SIZE)
    {
        totalSpaceNew = pThis->m_uiAllocated + MAX_GROW_SIZE;
    }
    else
    {
        totalSpaceNew = pThis->m_uiAllocated * 2;
    }

    totalSpaceNew = SG_MAX(totalSpaceNew, growToSizeRounded);

#if 0
    if (pThis->m_uiAllocated)
    {
        printf("Growing from %d to %d\n", pThis->m_uiAllocated, totalSpaceNew);
    }
#endif

	SG_ERR_CHECK_RETURN(  SG_allocN(pCtx,totalSpaceNew,pNewBuf)  );

	if (pThis->m_pBuf)
	{
		if (pThis->m_uiSizeInUse > 0)
			memmove(pNewBuf,pThis->m_pBuf,pThis->m_uiSizeInUse*sizeof(SG_byte));
		SG_NULLFREE(pCtx, pThis->m_pBuf);
	}

	pThis->m_pBuf = pNewBuf;
	pThis->m_uiAllocated = totalSpaceNew;
}

//////////////////////////////////////////////////////////////////

void SG_string__alloc(SG_context * pCtx, SG_string** ppNew)
{
	SG_ERR_CHECK_RETURN(  SG_string__alloc__reserve(pCtx, ppNew, INITIAL_SIZE)  );		// do not use SG_STRING__ALLOC__RESERVE
}

void SG_string__make_space(SG_context * pCtx, SG_string* pstr, SG_uint32 space)
{
	SG_ERR_CHECK_RETURN(  _sg_string__grow(pCtx,pstr,space)  );
}

//////////////////////////////////////////////////////////////////

/**
 * SG_context needs an old-style version of SG_string__alloc__reserve()
 * that doesn't take a SG_context (so that it can create one).
 */
SG_error SG_string__alloc__reserve__err(SG_string** ppNew, SG_uint32 reserve_len)
{
	SG_string * pThis = NULL;

	if (!ppNew)
		return SG_ERR_INVALIDARG;

	pThis = (SG_string *)SG_malloc(sizeof(SG_string));
	if (!pThis)
		return SG_ERR_MALLOCFAILED;

	pThis->m_uiSizeInUse = 0;
	pThis->m_uiAllocated = reserve_len;
	pThis->m_uiChunkSize = MIN_CHUNK_SIZE;

	pThis->m_pBuf = (SG_byte *)SG_calloc(reserve_len,sizeof(SG_byte));
	if (!pThis->m_pBuf)
	{
		SG_string__free__no_ctx(pThis);
		return SG_ERR_MALLOCFAILED;
	}

	*ppNew = pThis;

	return SG_ERR_OK;
}

void SG_string__alloc__reserve(SG_context * pCtx, SG_string** ppNew, SG_uint32 reserve_len)
{
	SG_string * pThis = NULL;

	SG_NULLARGCHECK(ppNew);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx,pThis)  );

	pThis->m_uiSizeInUse = 0;
	pThis->m_uiAllocated = reserve_len;
	pThis->m_uiChunkSize = MIN_CHUNK_SIZE;

	SG_ERR_CHECK(  SG_allocN(pCtx,reserve_len,pThis->m_pBuf)  );

	*ppNew = pThis;

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pThis);
}

//////////////////////////////////////////////////////////////////

void SG_string__alloc__sz(SG_context * pCtx, SG_string** ppNew, const char * sz)
{
	SG_string* pThis = NULL;

	SG_ERR_CHECK(  SG_string__alloc(pCtx,&pThis)  );			// do not use SG_STRING__ALLOC
	SG_ERR_CHECK(  SG_string__append__sz(pCtx,pThis,sz)  );

	*ppNew = pThis;

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pThis);
}

void SG_string__alloc__format(SG_context * pCtx, SG_string** ppNew, const char * szFormat, ...)
{
	va_list ap;

	va_start(ap,szFormat);
	SG_STRING__ALLOC__VFORMAT(pCtx, ppNew, szFormat,ap);	// we use alloc wrapper because this function is a varargs and we can't make a macro for it
	va_end(ap);

	SG_ERR_CHECK_RETURN_CURRENT;
}

void SG_string__alloc__vformat(SG_context * pCtx, SG_string** ppNew, const char * szFormat, va_list ap)
{
	SG_string* pThis = NULL;

	SG_ERR_CHECK(  SG_string__alloc(pCtx, &pThis)  );			// do not use SG_STRING__ALLOC
	SG_ERR_CHECK(  SG_string__vsprintf(pCtx, pThis, szFormat, ap)  );

	*ppNew = pThis;

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pThis);
}

void SG_string__alloc__buf_len(SG_context * pCtx, SG_string** ppNew, const SG_byte * pBuf, SG_uint32 len)
{
	SG_string * pThis = NULL;

	SG_ERR_CHECK(  SG_string__alloc(pCtx, &pThis)  );			// do not use SG_STRING__ALLOC
	SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pThis,pBuf,len)  );

	*ppNew = pThis;

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pThis);
}

//////////////////////////////////////////////////////////////////

void SG_string__alloc__copy(SG_context * pCtx, SG_string** ppNew, const SG_string * pString)
{
	RETURN_ERROR_IF_INVALID(pString);

	// allocate a new string and insert the entire contents of this one.

	SG_ERR_CHECK_RETURN(  SG_string__alloc__buf_len(pCtx, ppNew, pString->m_pBuf,pString->m_uiSizeInUse)  );	// do not use SG_STRING__ALLOC__BUF_LEN
}

//////////////////////////////////////////////////////////////////

void SG_string__free(SG_context * pCtx, SG_string * pThis)
{
	if (!pThis)			// explicitly allow free(null)
		return;

	SG_NULLFREE(pCtx, pThis->m_pBuf);
	pThis->m_uiSizeInUse = 0;
	pThis->m_uiAllocated = 0;

	SG_NULLFREE(pCtx, pThis);
}

void SG_string__free__no_ctx(SG_string * pThis)
{
	if (!pThis)			// explicitly allow free(null)
		return;

	SG_free__no_ctx(pThis->m_pBuf);
	pThis->m_pBuf = NULL;

	pThis->m_uiSizeInUse = 0;
	pThis->m_uiAllocated = 0;

	SG_free__no_ctx(pThis);
}

//////////////////////////////////////////////////////////////////

void SG_string__set__string(SG_context * pCtx, SG_string * pThis, const SG_string * pStringToCopy)
{
	RETURN_ERROR_IF_INVALID(pThis);
	RETURN_ERROR_IF_INVALID(pStringToCopy);

	if (pThis == pStringToCopy)			// set from self???
		return;

	SG_ERR_CHECK_RETURN(  SG_string__set__buf_len(pCtx, pThis,pStringToCopy->m_pBuf,pStringToCopy->m_uiSizeInUse)  );
}

void SG_string__set__sz(SG_context * pCtx, SG_string * pThis, const char * sz)
{
	// we must test for overlap before clearing

	RETURN_ERROR_IF_INVALID(pThis);

	if (sz && *sz
		&& SG_string__does_buf_overlap_string(pThis,(SG_byte *)sz,(SG_uint32)strlen(sz)))
		SG_ERR_THROW_RETURN(  SG_ERR_OVERLAPPINGBUFFERS  );

	SG_ERR_CHECK_RETURN(  SG_string__clear(pCtx,pThis)  );

	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx,pThis,sz)  );
}

void SG_string__set__buf_len(SG_context * pCtx, SG_string * pThis, const SG_byte * pBuf, SG_uint32 len)
{
	// we must test for overlap before clearing

	RETURN_ERROR_IF_INVALID(pThis);

	if (pBuf && (len > 0)
		&& SG_string__does_buf_overlap_string(pThis,pBuf,len))
		SG_ERR_THROW_RETURN(  SG_ERR_OVERLAPPINGBUFFERS  );

	SG_ERR_CHECK_RETURN(  SG_string__clear(pCtx,pThis)  );

	SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx,pThis,pBuf,len)  );
}

//////////////////////////////////////////////////////////////////

static void _insert__buf_len_grow(SG_context * pCtx,
								  SG_string * pThis,
								  SG_uint32 byte_offset,
								  const SG_byte * pBuf,
								  SG_uint32 len,
								  SG_bool allowGrow)
{
	RETURN_ERROR_IF_INVALID(pThis);

	SG_ARGCHECK_RETURN( (byte_offset <= pThis->m_uiSizeInUse), byte_offset );		// cannot create sparse string.

	if (len == 0)
		return;

	SG_NULLARGCHECK_RETURN(pBuf);

	if (SG_string__does_buf_overlap_string(pThis,pBuf,len))
		SG_ERR_THROW_RETURN(  SG_ERR_OVERLAPPINGBUFFERS  );

	if (allowGrow)
	{
		SG_ERR_CHECK_RETURN(  _sg_string__grow(pCtx,pThis,len)  );
	}
	else
	{
		if (_sg_string__needs_grow(pThis, len, NULL))
			SG_ERR_THROW_RETURN(  SG_ERR_BUFFERTOOSMALL  );
	}

	if (byte_offset < pThis->m_uiSizeInUse)
		memmove(pThis->m_pBuf+byte_offset+len,pThis->m_pBuf+byte_offset,(pThis->m_uiSizeInUse-byte_offset)*sizeof(SG_byte));

	memmove(pThis->m_pBuf+byte_offset,pBuf,len*sizeof(SG_byte));
	pThis->m_uiSizeInUse += len;

	pThis->m_pBuf[pThis->m_uiSizeInUse] = 0;	// guarantee that buffer is always null terminated incase they want to print it.
}

//////////////////////////////////////////////////////////////////

void SG_string__append__string(SG_context * pCtx, SG_string * pThis, const SG_string * pString)
{
	RETURN_ERROR_IF_INVALID(pString);

	if (pThis == pString)				// append with self??
		SG_ERR_THROW_RETURN(  SG_ERR_OVERLAPPINGBUFFERS  );

	SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx,pThis,pString->m_pBuf,pString->m_uiSizeInUse)  );
}

void SG_string__append__format(SG_context * pCtx, SG_string* pThis, const char * szFormat, ...)
{
    SG_string* pstr_append = NULL;
	va_list ap;

	va_start(ap,szFormat);
	SG_ERR_CHECK(  SG_STRING__ALLOC__VFORMAT(pCtx, &pstr_append, szFormat,ap)  );
	va_end(ap);

    SG_ERR_CHECK(  SG_string__append__string(pCtx, pThis, pstr_append)  );

fail:
    SG_STRING_NULLFREE(pCtx, pstr_append);
}

void SG_string__append__sz(SG_context * pCtx, SG_string * pThis, const char * sz)
{
	RETURN_ERROR_IF_INVALID(pThis);

	SG_ERR_CHECK_RETURN(  SG_string__insert__sz(pCtx,pThis,pThis->m_uiSizeInUse,sz)  );
}

void SG_string__append__sz__no_grow(SG_context * pCtx, SG_string * pThis, const char * sz)
{
	SG_uint32 lenInBytes;

	RETURN_ERROR_IF_INVALID(pThis);

	if (!sz || !*sz)
		return;

	lenInBytes = (SG_uint32)strlen(sz);

	SG_ERR_CHECK_RETURN(  _insert__buf_len_grow(pCtx,pThis,pThis->m_uiSizeInUse,(SG_byte*)sz,lenInBytes, SG_FALSE)  );
}

void SG_string__append__buf_len(SG_context * pCtx, SG_string * pThis, const SG_byte * pBuf, SG_uint32 len)
{
	RETURN_ERROR_IF_INVALID(pThis);

	SG_ERR_CHECK_RETURN(  _insert__buf_len_grow(pCtx, pThis, pThis->m_uiSizeInUse, pBuf, len, SG_TRUE)  );
}

//////////////////////////////////////////////////////////////////

void SG_string__insert__string(SG_context * pCtx, SG_string * pThis, SG_uint32 byte_offset, const SG_string * pString)
{
	RETURN_ERROR_IF_INVALID(pString);

	if (pThis == pString)				// append with self??
		SG_ERR_THROW_RETURN(  SG_ERR_OVERLAPPINGBUFFERS  );

	SG_ERR_CHECK_RETURN(  SG_string__insert__buf_len(pCtx, pThis, byte_offset, pString->m_pBuf, pString->m_uiSizeInUse)  );
}

void SG_string__insert__sz(SG_context * pCtx, SG_string * pThis, SG_uint32 byte_offset, const char * sz)
{
	SG_uint32 lenInBytes;

	RETURN_ERROR_IF_INVALID(pThis);

	if (!sz || !*sz)
		return;

	lenInBytes = (SG_uint32)strlen(sz);

	SG_ERR_CHECK_RETURN(  SG_string__insert__buf_len(pCtx, pThis, byte_offset, (SG_byte *)sz, lenInBytes)  );
}

void SG_string__insert__buf_len(SG_context * pCtx, SG_string * pThis, SG_uint32 byte_offset, const SG_byte * pBuf, SG_uint32 len)
{
	SG_ERR_CHECK_RETURN(  _insert__buf_len_grow(pCtx, pThis, byte_offset, pBuf, len, SG_TRUE)  );
}

//////////////////////////////////////////////////////////////////

void SG_string__remove(SG_context * pCtx, SG_string * pThis, SG_uint32 byte_offset, SG_uint32 len)
{
	RETURN_ERROR_IF_INVALID(pThis);

	SG_ARGCHECK_RETURN(  (byte_offset < pThis->m_uiSizeInUse), byte_offset  );
	SG_ARGCHECK_RETURN(  (byte_offset+len <= pThis->m_uiSizeInUse), len  );

	if (len == 0)
		return;

	memmove(pThis->m_pBuf+byte_offset,pThis->m_pBuf+byte_offset+len,(pThis->m_uiSizeInUse-byte_offset-len)*sizeof(SG_byte));
	pThis->m_uiSizeInUse -= len;
	pThis->m_pBuf[pThis->m_uiSizeInUse] = 0;	// guarantee that buffer is always null terminated incase they want to print it.
}

void SG_string__clear(SG_context * pCtx, SG_string * pThis)
{
	SG_NULLARGCHECK_RETURN(pThis);

	// pretend like we have nothing in the buffer.
	// do not release/free the buffer, but we do zero it.

	pThis->m_uiSizeInUse = 0;
	if (pThis->m_pBuf)
		pThis->m_pBuf[pThis->m_uiSizeInUse] = 0;	// guarantee that buffer is always null terminated incase they want to print it.
}

SG_error SG_string__clear__err(SG_string * pThis)
{
	if (!pThis)
		return SG_ERR_INVALIDARG;

	// pretend like we have nothing in the buffer.
	// do not release/free the buffer, but we do zero it.

	pThis->m_uiSizeInUse = 0;
	if (pThis->m_pBuf)
		pThis->m_pBuf[pThis->m_uiSizeInUse] = 0;	// guarantee that buffer is always null terminated incase they want to print it.

	return SG_ERR_OK;
}

//////////////////////////////////////////////////////////////////

void SG_string__replace_bytes(SG_context * pCtx,
							  SG_string * pThis,
							  const SG_byte * pBytesToFind, SG_uint32 lenBytesToFind,
							  const SG_byte * pBytesToReplaceWith, SG_uint32 lenBytesToReplaceWith,
							  SG_uint32 count, SG_bool bAdvance,
							  SG_uint32 * pNrFound)
{
	// replace the first 'count' instances of the byte-string 'pBytesToFind[0..lenBytesToFind-1]'
	// with the byte-string 'pBytesToReplaceWith[0..lenBytesToReplaceWith-1]'.
	//
	// return the number of substitutions performed in '*pNrFound'.
	//
	// if bAdvance, we advance the search to the point past the replacement before starting the
	// next search.  if not, we do the next search at the beginning of the previous replacement.
	// for example, with "\\" --> "/" you can say true. but for "/./" --> "/" you should probably
	// say false so that "/././" collapses as expected.
	//
	// WARNING: given byte buffers MUST NOT point into our buffer.

	SG_uint32 nrFound, offset;

	RETURN_ERROR_IF_INVALID(pThis);

	SG_NULLARGCHECK_RETURN(pBytesToFind);						// do not allow search patern to be empty
	SG_ARGCHECK_RETURN( (lenBytesToFind > 0), lenBytesToFind );	// (but we do allow replace buffer to be empty)

	SG_ARGCHECK_RETURN( (count > 0), count );

	if (pThis->m_uiSizeInUse == 0)				// nothing in buffer (m_pBuf may also be NULL)
	{
		if (pNrFound)
			*pNrFound = 0;
		return;
	}

	// verify that we don't have overlapping buffers.

	if (SG_string__does_buf_overlap_string(pThis,pBytesToFind,lenBytesToFind))
		SG_ERR_THROW_RETURN(  SG_ERR_OVERLAPPINGBUFFERS  );

	if (lenBytesToReplaceWith > 0)
		if (SG_string__does_buf_overlap_string(pThis,pBytesToReplaceWith,lenBytesToReplaceWith))
			SG_ERR_THROW_RETURN(  SG_ERR_OVERLAPPINGBUFFERS  );

	// do 'count' search/replaces

	offset = 0;
	nrFound = 0;
	while (nrFound < count)
	{
		if (pThis->m_uiSizeInUse < offset + lenBytesToFind)
			break;

		if (memcmp(&pThis->m_pBuf[offset],pBytesToFind,(size_t)lenBytesToFind) == 0)
		{
			// match found, replace.

			if (lenBytesToFind == lenBytesToReplaceWith)
			{
				memmove(&pThis->m_pBuf[offset],pBytesToReplaceWith,(size_t)lenBytesToFind);
			}
			else
			{
				SG_ERR_CHECK_RETURN(  SG_string__remove(pCtx,pThis,offset,lenBytesToFind)  );
				if (lenBytesToReplaceWith > 0)
				{
					SG_ERR_CHECK_RETURN(  SG_string__insert__buf_len(pCtx,pThis,offset,pBytesToReplaceWith,lenBytesToReplaceWith)  );
				}
				if ((lenBytesToReplaceWith >= lenBytesToFind) && ! bAdvance)
				{
					++offset;
				}
			}

			if (bAdvance)
				offset += lenBytesToReplaceWith;
			nrFound ++;
		}
		else
		{
			offset++;
		}
	}

	if (pNrFound)
		*pNrFound = nrFound;
}

void SG_string__sizzle(SG_context * pCtx, SG_string ** ppThis, SG_byte ** pSz, SG_uint32 * pLength)
{
	SG_byte * sz = NULL;
	SG_uint32 length = 0;

	SG_NULL_PP_CHECK_RETURN(ppThis);
	SG_NULLARGCHECK_RETURN(pSz);

	sz = (*ppThis)->m_pBuf;
	length = (*ppThis)->m_uiSizeInUse;

	SG_ERR_CHECK( SG_free(pCtx,*ppThis) );

	*ppThis = NULL;
	*pSz = sz;
	if( pLength != NULL )
		*pLength = length;

	return;
fail:
	return;
}

//////////////////////////////////////////////////////////////////
// this returns a pointer to our internal buffer.  use this as a
// *VERY* temporary pointer.  we may realloc anytime you do something
// to us.

const char * SG_string__sz(const SG_string * pThis)
{
	SG_ASSERT( _sg_string__validate(pThis) );

	return (const char *)pThis->m_pBuf;
}

SG_uint32 SG_string__length_in_bytes(const SG_string * pThis)
{
	SG_ASSERT( _sg_string__validate(pThis) );

	return pThis->m_uiSizeInUse;
}

SG_uint32 SG_string__bytes_allocated(const SG_string * pThis)
{
	SG_ASSERT( _sg_string__validate(pThis) );

	return pThis->m_uiAllocated;
}

SG_uint32 SG_string__bytes_avail_before_grow(const SG_string * pThis)
{
	return SG_string__bytes_allocated(pThis) - INSURANCE_PADDING;
}

//////////////////////////////////////////////////////////////////

void SG_string__get_byte_l(SG_context * pCtx, const SG_string * pThis, SG_uint32 ndx, SG_byte * pbyteReturned)
{
	// get the nth byte in the buffer from the beginning.

	RETURN_ERROR_IF_INVALID(pThis);
	SG_NULLARGCHECK_RETURN(pbyteReturned);
	SG_ARGCHECK_RETURN( (ndx < pThis->m_uiSizeInUse), ndx );

	*pbyteReturned = pThis->m_pBuf[ndx];
}

void SG_string__get_byte_r(SG_context * pCtx, const SG_string * pThis, SG_uint32 ndx, SG_byte * pbyteReturned)
{
	// get the nth byte in the buffer from the end.

	SG_uint32 offset;

	RETURN_ERROR_IF_INVALID(pThis);
	SG_NULLARGCHECK_RETURN(pbyteReturned);
	SG_ARGCHECK_RETURN( (ndx < pThis->m_uiSizeInUse), ndx );

	offset = pThis->m_uiSizeInUse - ndx - 1;

	*pbyteReturned = pThis->m_pBuf[offset];
}

//////////////////////////////////////////////////////////////////

void SG_string__setChunkSize(SG_string * pThis, SG_uint32 size)
{
	if (!pThis)
		return;

	if (size < MIN_CHUNK_SIZE)
		size = MIN_CHUNK_SIZE;

	pThis->m_uiChunkSize = size;
}

//////////////////////////////////////////////////////////////////

SG_bool SG_string__does_buf_overlap_string(const SG_string * pThis, const SG_byte * pBuf, SG_uint32 lenBuf)
{
	// return true if the given {buffer,length} is somewhere within our allocated buffer.
	// this is used mainly for asserts to ensure that we don't have overlap
	// during inserts/deletes/etc.

	SG_ASSERT( _sg_string__validate(pThis) );
	SG_ASSERT( pBuf && (lenBuf > 0) );

	if (!pThis->m_pBuf)
		return SG_FALSE;

	if (pBuf+lenBuf <= pThis->m_pBuf)
		return SG_FALSE;

	if (pThis->m_pBuf+pThis->m_uiAllocated <= pBuf)
		return SG_FALSE;

	return SG_TRUE;
}

//////////////////////////////////////////////////////////////////

void SG_string__sprintf(SG_context * pCtx, SG_string * pThis, const char * szFormat, ...)
{
	// sprintf into our internal buffer.  grow it as necessary.

	va_list ap;

	va_start(ap,szFormat);
	SG_string__vsprintf(pCtx,pThis,szFormat,ap);
	va_end(ap);

	SG_ERR_CHECK_RETURN_CURRENT;
}

void SG_string__vsprintf(SG_context * pCtx, SG_string * pThis, const char * szFormat, va_list ap)
{
	// sprintf into our internal buffer.  grow it as necessary.
	//
	// we violate the coding guidelines and modify the string even
	// if we have an error.  (sometimes we get the error as we're
	// doing the formatting.)

	int lenResult;

	SG_ERR_CHECK_RETURN(  SG_string__clear(pCtx,pThis)  );

	if (!szFormat || !*szFormat)
		return;

	if (pThis->m_uiAllocated == 0)
	{
		// incase we've never had anything in us, pre-allocate some space.

		SG_ERR_CHECK_RETURN(  _sg_string__grow(pCtx,pThis,pThis->m_uiChunkSize)  );
	}

	// try to do format into the buffer that we already have.
	// if that causes a truncation, increase our buffer size
	// and try again.

	while (1)
	{
		errno = 0;

#if defined(MAC) || defined(LINUX)
		// for vsnprintf() return value is size of string excluding final NULL.

		lenResult = vsnprintf((char *)pThis->m_pBuf,(size_t)pThis->m_uiAllocated,szFormat,ap);

		if (lenResult < 0)				// some kind of formatter error??
			SG_ERR_THROW(  SG_ERR_ERRNO(errno)  );

		if (lenResult < (int)pThis->m_uiAllocated)
			break;

		// else we need (lenResult+1) buffer space

#endif
#if defined(WINDOWS)
		// for vsnprintf_s() we get additional parameter checking and (when 'count'
		// is not _TRUNCATE) the return value is -1 for any error or overflow.  i
		// think they also clear the buffer.  the do not set errno on overflow (when
		// _TRUNCATE).

		lenResult = vsnprintf_s((char *)pThis->m_pBuf,(size_t)pThis->m_uiAllocated,(size_t)pThis->m_uiAllocated-1,szFormat,ap);

		if ((lenResult < 0) && (errno != 0))		// some kind of formatter error??
			SG_ERR_THROW(  SG_ERR_ERRNO(errno)  );

		if (lenResult >= 0)
			break;

		// else (lenResult == -1 and errno == 0), means we have an overflow.
		// but windows doesn't tell us how much space we need.  so we make a
		// guess and try again.

		lenResult = (int)(pThis->m_uiAllocated + pThis->m_uiChunkSize);
#endif

		// when vsnprintf fails, we may be in an inconsistent state with some
		// some partial data (so m_pBuf[0]!=0 and m_uiSizeInUse==0).  this can
		// trigger asserts when we try to grow the buffer.

		pThis->m_pBuf[0] = 0;
		pThis->m_uiSizeInUse = 0;

		SG_ERR_CHECK(  _sg_string__grow(pCtx,pThis,(SG_uint32)lenResult)  );
	}

	// lenResult is length of string (not including terminating null)

	SG_ASSERT( pThis->m_pBuf[lenResult] == 0 );
	pThis->m_uiSizeInUse = (SG_uint32)lenResult;

	return;

fail:
	pThis->m_pBuf[0] = 0;
	pThis->m_uiSizeInUse = 0;
}

//////////////////////////////////////////////////////////////////

void SG_string__adopt_buffer(SG_context * pCtx, SG_string * pThis, char * pBuf, SG_uint32 len)
{
	SG_byte * pOld;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pBuf);
	SG_ARGCHECK_RETURN( (len > 0), len );

	pOld = pThis->m_pBuf;

	pThis->m_pBuf = (SG_byte *)pBuf;
	pThis->m_uiAllocated = len;
	pThis->m_uiSizeInUse = len - 1;

	SG_ASSERT( (pThis->m_pBuf[pThis->m_uiSizeInUse] == 0) );
	SG_ASSERT( (strlen((char *)pThis->m_pBuf) == pThis->m_uiSizeInUse) );

	SG_NULLFREE(pCtx, pOld);
}

//////////////////////////////////////////////////////////////////

void SG_string__contains_whitespace(SG_context * pCtx, const SG_string * pThis, SG_bool * pbFound)
{
	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pbFound);

	if (pThis->m_uiSizeInUse > 0)
	{
		if ( strchr((const char *)pThis->m_pBuf,' ') || strchr((const char *)pThis->m_pBuf,'\t') )
		{
			*pbFound = SG_TRUE;
			return;
		}
	}

	*pbFound = SG_FALSE;
}


void SG_string__split__asciichar(SG_context * pCtx, const SG_string * pThis, char separator, SG_uint32 maxSubstrings, char *** pppResults, SG_uint32 * pSubstringCount)
{
	const char * sz = NULL;
	if(pThis!=NULL)
		sz = SG_string__sz(pThis);
	SG_string__split__sz_asciichar(pCtx, sz, separator, maxSubstrings, pppResults, pSubstringCount);
}

void SG_string__split__sz_asciichar(SG_context * pCtx, const char * sz, char separator, SG_uint32 maxSubstrings, char *** pppResults, SG_uint32 * pSubstringCount)
{
	SG_int32 iLastSeparator = -1;
	char ** ppResults = NULL;
	SG_uint32 substringCount = 0;

	SG_ARGCHECK_RETURN( maxSubstrings >= 2 , maxSubstrings );
	SG_NULLARGCHECK_RETURN(pppResults);

	if(sz==NULL)
		sz="";

	SG_ERR_CHECK(  SG_allocN(pCtx, maxSubstrings+((pSubstringCount!=NULL)?0:1), ppResults)  );

	do
	{
		char * szCopy = NULL;
		SG_int32 i = iLastSeparator+1;
		while(sz[i]!='\0'&&sz[i]!=separator)
			++i;
		SG_ERR_CHECK(  SG_allocN(pCtx, i-iLastSeparator-1+1,szCopy)  );
		memcpy(szCopy,&sz[iLastSeparator+1],i-iLastSeparator-1);
		szCopy[i-iLastSeparator-1] = '\0';
		ppResults[substringCount] = szCopy;
		++substringCount;
		iLastSeparator = i;
	} while((sz[iLastSeparator]!='\0') && (substringCount < maxSubstrings));

	if( pSubstringCount != NULL )
		*pSubstringCount = substringCount;
	else
		ppResults[substringCount] = NULL;
	*pppResults = ppResults;

	return;
fail:
	while(substringCount>0)
	{
		--substringCount;
		SG_ERR_IGNORE(  SG_free(pCtx, ppResults[substringCount])  );
	}
	SG_ERR_IGNORE(  SG_free(pCtx, ppResults)  );
	return;
}
