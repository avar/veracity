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

// sg_misc_utils.c
// misc utils.
//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

void SG_strdup(SG_context * pCtx, const char * szSrc, char ** pszCopy)
{
	size_t len;
	char * szCopy;

	SG_NULLARGCHECK_RETURN(szSrc);
	SG_NULLARGCHECK_RETURN(pszCopy);

	len = strlen(szSrc);
	szCopy = SG_malloc((SG_uint32)(len+1));
	if (!szCopy)
		SG_ERR_THROW_RETURN(SG_ERR_MALLOCFAILED);

	memcpy(szCopy,szSrc,len+1);
	*pszCopy = szCopy;
}

void SG_strcpy(SG_context * pCtx, char * szDest, SG_uint32 lenDest, const char * szSrc)
{
	// strcpy() without the _CRT_SECURITY_NO_WARNINGS stuff on Windows.
	// also hides differences between Windows' strcpy_s() and Linux/Mac's
	// strcpy().
	//
	// copy upto the limit -- BUT ALWAYS NULL TERMINATE.
	// then null-out the rest of the buffer.
	//
	// WARNING: input buffers must not overlap.

	SG_uint32 lenSrc;
	SG_uint32 lenToCopy;

	SG_NULLARGCHECK_RETURN(szDest);
	SG_ARGCHECK_RETURN( lenDest>=1 , lenDest );
	SG_NULLARGCHECK_RETURN(szSrc);

	lenSrc = (SG_uint32)strlen(szSrc);

	if (lenSrc < lenDest)
	{
		lenToCopy = lenSrc;
	}
	else
	{
		lenToCopy = lenDest - 1;

		// TODO shouldn't this THROW an error?
		(void)SG_context__err__generic(pCtx, SG_ERR_BUFFERTOOSMALL, __FILE__, __LINE__);
	}

    /* the buffers must not overlap, but they can be adjacent to each other */
	SG_ASSERT( (szDest+lenDest <= szSrc) || (szSrc+lenSrc <= szDest) );

#if defined(WINDOWS)
	memcpy_s(szDest,lenDest,szSrc,lenToCopy);
#else
	memcpy(szDest,szSrc,lenToCopy);
#endif

    /* TODO why null out the rest of the buffer?  why not just add a single trailing zero?
	 * Jeff says: I did this because one (I don't remember which) of the MSFT _s routines
	 *            was documented as doing this and I thought we should have the same
	 *            behavior on all platforms.  We can change it.
	 */
	memset(szDest+lenToCopy,0,(lenDest-lenToCopy));
}

void SG_strcat(SG_context * pCtx, char * bufDest, SG_uint32 lenBufDest, const char * szSrc)
{
	// strcat() without the _CRT_SECURITY_NO_WARNINGS stuff on Windows.
	//
	// lenBufDest is the defined length of the target buffer to help
	// with buffer-overrun issues and make Windows happy.
	//

	SG_uint32 offsetDest;

	SG_NULLARGCHECK_RETURN(bufDest);
	SG_NULLARGCHECK_RETURN(szSrc);

	offsetDest = (SG_uint32)strlen(bufDest);

	if (lenBufDest > offsetDest)
		SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx, bufDest + offsetDest, lenBufDest - offsetDest, szSrc)  );
}

void SG_sprintf(SG_context * pCtx, char * pBuf, SG_uint32 lenBuf, const char * szFormat, ...)
{
	// a portable version of sprintf, sprintf_s, snprintf that won't overrun
	// the buffer and which produces cross-platform consistent results.
	//
	// we return an error if we had to truncate the buffer or if we have a
	// formatting error.
	//
	// we guarantee that the buffer will be null terminated.

	va_list ap;

	SG_NULLARGCHECK_RETURN(pBuf);
	SG_ARGCHECK_RETURN( lenBuf>0 , lenBuf );
	SG_NULLARGCHECK_RETURN(szFormat);

	va_start(ap,szFormat);
#if defined(MAC) || defined(LINUX)
	{
		// for vsnprintf() return value is size of string excluding final NULL.
		int lenResult = vsnprintf(pBuf,(size_t)lenBuf,szFormat,ap);
		if (lenResult < 0)
			(void)SG_context__err__generic(pCtx, SG_ERR_ERRNO(errno), __FILE__, __LINE__);
		else if (lenResult >= (int)lenBuf)
			(void)SG_context__err__generic(pCtx, SG_ERR_BUFFERTOOSMALL, __FILE__, __LINE__);
	}
#endif
#if defined(WINDOWS)
	{
		// for vsnprintf_s() we get additional parameter checking and (when 'count'
		// is not _TRUNCATE) the return value is -1 for any error or overflow.  i
		// think they also clear the buffer.  the do not set errno on overflow (when
		// _TRUNCATE).
		int lenResult = vsnprintf_s(pBuf,(size_t)lenBuf,(size_t)lenBuf-1,szFormat,ap);
		if (lenResult < 0)
		{
			if (errno == 0)
				(void)SG_context__err__generic(pCtx, SG_ERR_BUFFERTOOSMALL, __FILE__, __LINE__);
			else
				(void)SG_context__err__generic(pCtx, SG_ERR_ERRNO(errno), __FILE__, __LINE__);
		}
		else
		{
			SG_ASSERT(lenResult+1 <= (int)lenBuf);	// formatted result+null <= lenbuf
		}
	}
#endif
	va_end(ap);

	// clear the buffer on error.
	if (SG_context__has_err(pCtx))
		pBuf[0] = 0;
}

void SG_sprintf_truncate(
	SG_context * pCtx,
	char * pBuf,
	SG_uint32 lenBuf,
	const char * szFormat,
	...
	)
{
	va_list ap;

	va_start(ap,szFormat);
	SG_ERR_CHECK(  SG_vsprintf_truncate(pCtx, pBuf, lenBuf, szFormat, ap)  );
	// Fall through
fail:
	va_end(ap);
}

void SG_vsprintf_truncate(
	SG_context * pCtx,
	char * pBuf,
	SG_uint32 lenBuf,
	const char * szFormat,
	va_list ap
	)
{
	int lenResult;

	SG_NULLARGCHECK_RETURN(pBuf);
	SG_ARGCHECK_RETURN( lenBuf>0 , lenBuf );
	SG_NULLARGCHECK_RETURN(szFormat);

	// expand arguments into buffer, truncate if necessary.

#if defined(MAC) || defined(LINUX)
	// for vsnprintf() return value is size of string excluding final NULL.
	lenResult = vsnprintf(pBuf,(size_t)lenBuf,szFormat,ap);
	if (lenResult < 0)
		SG_ERR_THROW(SG_ERR_ERRNO(errno));
#endif
#if defined(WINDOWS)
	lenResult = vsnprintf_s(pBuf,(size_t)lenBuf,_TRUNCATE,szFormat,ap);

	if ((lenResult < 0) && (errno != 0))  // formatter error??
		SG_ERR_THROW(SG_ERR_ERRNO(errno));
#endif

	return;
fail:
	return;
}

SG_int32 SG_stricmp(const char * sz1, const char * sz2)
{
#if defined(WINDOWS)
	return _stricmp(sz1, sz2);
#else
	return strcasecmp(sz1, sz2);
#endif
}
SG_int32 SG_memicmp(const SG_byte * sz1, const SG_byte * sz2, SG_uint32 numBytes)
{
#if defined(WINDOWS)
	return _memicmp(sz1, sz2, numBytes);
#else
	return strncasecmp((const char *)sz1, (const char *)sz2, numBytes);
#endif
}

SG_bool SG_ascii__is_valid(const char * p)
{
	if( p == NULL )
		return SG_FALSE;
	while( *p != '\0' )
	{
		if( *p < 0 )
			return SG_FALSE;
		++p;
	}
	return SG_TRUE;
}

void SG_ascii__find__char(SG_context * pCtx, const char * sz, char c, SG_uint32 * pResult)
{
	const char * p = sz;

	SG_NULLARGCHECK_RETURN(sz);
	SG_ARGCHECK_RETURN( c>=0 , c );

	while( *p != '\0' )
	{
		if( *p < 0 )
		{
			(void)SG_context__err(pCtx, SG_ERR_INVALIDARG, __FILE__, __LINE__, "sz is invalid: it contains non-ASCII characters.");
			return;
		}
		if( *p==c )
		{
			*pResult = p-sz;
			return;
		}
		++p;
	}

	*pResult = SG_UINT32_MAX;
}

void SG_ascii__substring(SG_context * pCtx, const char *szSrc, SG_uint32 start, SG_uint32 len, char ** pszCopy)
{
	char * szCopy;

	SG_NULLARGCHECK_RETURN(szSrc);
	SG_NULLARGCHECK_RETURN(pszCopy);

	szCopy = SG_malloc(len+1);
	if (!szCopy)
		SG_ERR_THROW_RETURN(SG_ERR_MALLOCFAILED);

	memcpy(szCopy,szSrc+start,(size_t)len);
	szCopy[len] = '\0';
	*pszCopy = szCopy;
}

void SG_ascii__substring__to_end(SG_context * pCtx, const char *szSrc, SG_uint32 start, char ** pszCopy)
{
	size_t len;

	SG_NULLARGCHECK_RETURN(szSrc);
	SG_NULLARGCHECK_RETURN(pszCopy);

	len = strlen(szSrc);

	if( start >= len )
		SG_ERR_CHECK_RETURN(  SG_STRDUP(pCtx, "", pszCopy)  );
	else
		SG_ERR_CHECK_RETURN(  SG_ascii__substring(pCtx, szSrc, start, len-start, pszCopy)  );
}

//////////////////////////////////////////////////////////////////

char * SG_int64_to_sz(SG_int64 x, SG_int_to_string_buffer buffer)
{
	if(buffer==NULL) // Eh?!
		return NULL;

	if(x<0)
	{
		buffer[0]='-';
		SG_uint64_to_sz((SG_uint64)-x,buffer+1);
		return buffer;
	}
	else
	{
		return SG_uint64_to_sz((SG_uint64)x,buffer);
	}
}

char * SG_uint64_to_sz(SG_uint64 x, SG_int_to_string_buffer buffer)
{
	SG_uint32 len = 0;

	if(buffer==NULL) // Eh?!
		return NULL;

	// First construct the string backwords, since that's easier.
	do
	{
		buffer[len] = '0' + x%10;
		len+=1;

		x/=10;
	} while(x>0);
	buffer[len]='\0';

	// Second, reverse the string.
	{
		SG_uint32 i;
		for(i=0;i<len/2;++i)
		{
			buffer[i]^=buffer[len-1-i];
			buffer[len-1-i]^=buffer[i];
			buffer[i]^=buffer[len-1-i];
		}
	}

	return buffer;
}

char * SG_uint64_to_sz__hex(SG_uint64 x, SG_int_to_string_buffer buffer)
{
	const char * digits = "0123456789abcdef";
	SG_uint32 i;

	if(buffer==NULL) // Eh?!
		return NULL;

	for(i=0;i<16;++i)
		buffer[16-1-i] = digits[ (x>>(i*4)) & 0xf ];
	buffer[16] = '\0';

	return buffer;
}


void SG_int64__parse__strict(SG_context * pCtx, SG_int64* piResult, const char* psz)
{
	SG_bool bNeg = SG_FALSE;
	SG_int64 result = 0;
	const char* cur = psz;

	if (*cur == '-')
	{
		bNeg = SG_TRUE;
		cur++;
	}

	while (*cur)
	{
		if ('0' <= *cur && *cur <='9' )
		{
			result = result * 10 + (*cur) - '0';
			cur++;
		}
		else
		{
            SG_ERR_THROW_RETURN(  SG_ERR_INT_PARSE  );
		}
	}

	if (bNeg)
    {
		result = -result;
    }

	*piResult = result;
}

void SG_int64__parse__stop_on_nondigit(SG_UNUSED_PARAM(SG_context * pCtx), SG_int64* piResult, const char* psz)
{
	SG_bool bNeg = SG_FALSE;
	SG_int64 result = 0;
	const char* cur = psz;

	SG_UNUSED(pCtx);

	while (
            (*cur == ' ')
            || (*cur == ' ')
          )
    {
		cur++;
    }

	if (*cur == '+')
	{
		cur++;
	}
	else if (*cur == '-')
	{
		bNeg = SG_TRUE;
		cur++;
	}

	while (*cur)
	{
		if ('0' <= *cur && *cur <='9' )
		{
			result = result * 10 + (*cur) - '0';
			cur++;
		}
		else
		{
			break;
		}
	}

	if (bNeg)
    {
		result = -result;
    }

	*piResult = result;
}

void SG_int64__parse(SG_context * pCtx, SG_int64* piResult, const char* psz)
{
    SG_ERR_CHECK_RETURN(  SG_int64__parse__stop_on_nondigit(pCtx, piResult, psz)  );
}

void SG_uint32__parse__strict(SG_context * pCtx, SG_uint32* piResult, const char* psz)
{
    SG_int64 i = 0;

    SG_ERR_CHECK_RETURN(  SG_int64__parse__strict(pCtx, &i, psz)  );
    SG_ASSERT(SG_int64__fits_in_uint32(i));
    *piResult = (SG_uint32) i;
}

void SG_uint32__parse(SG_context * pCtx, SG_uint32* piResult, const char* psz)
{
    SG_int64 i = 0;

    SG_ERR_CHECK_RETURN(  SG_int64__parse(pCtx, &i, psz)  );
    SG_ASSERT(SG_int64__fits_in_uint32(i));
    *piResult = (SG_uint32) i;
}

void SG_double__parse(SG_context * pCtx, double* pResult, const char* sz)
{
	int result = 1;

#if defined(WINDOWS)
	result = sscanf_s(sz, "%lf", pResult);
#else
	result = sscanf(sz, "%lf", pResult);
#endif

	// TODO change this to an SG_ERR_CHECK/THROW...

	if( result != 1 )
		(void)SG_context__err(pCtx, SG_ERR_INVALIDARG,__FILE__, __LINE__, "sz is invalid: Unable to parse it as a double.");
}


/**
 *	free each char * in a counted char ** array, then free the array
 *	ignores any NULL entries found
 */
void SG_freeStringList(
	SG_context *pCtx,
	const char ***strings,	/**< pointer to the array; will be NULL on return */
	SG_uint32 count)	/**< count of strings in the list */
{
	SG_uint32 i;
	SG_NULLARGCHECK_RETURN(strings);

	for ( i = 0; i < count; ++i )
	{
		SG_ERR_CHECK_RETURN(  SG_NULLFREE(pCtx, (*strings)[i])  );
	}

	SG_ERR_CHECK_RETURN(  SG_NULLFREE(pCtx, *strings)  );
	*strings = NULL;
}
