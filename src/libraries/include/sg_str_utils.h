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

// sg_str_utils.h
// string utilities.
//////////////////////////////////////////////////////////////////

#ifndef H_SG_STR_UTILS_H
#define H_SG_STR_UTILS_H

BEGIN_EXTERN_C

//////////////////////////////////////////////////////////////////

#define SG_CR '\x0d'
#define SG_LF '\x0a'

#define SG_CR_STR "\x0d"
#define SG_LF_STR "\x0a"
#define SG_CRLF_STR "\x0d\x0a"

#if defined(WINDOWS)
#define SG_PLATFORM_NATIVE_EOL_STR SG_CRLF_STR
#define SG_PLATFORM_NATIVE_EOL_IS_CRLF (SG_TRUE)
#else
#define SG_PLATFORM_NATIVE_EOL_STR SG_LF_STR
#define SG_PLATFORM_NATIVE_EOL_IS_CRLF (SG_FALSE)
#endif

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#define SG_STRDUP(pCtx,src,ppszCopy)			SG_STATEMENT(  char * _p = NULL;										\
																   SG_strdup(pCtx,src,&_p);								\
																   _sg_mem__set_caller_data(_p,__FILE__,__LINE__,"SG_strdup");	\
																   *(ppszCopy) = _p;											)
#else
#define SG_STRDUP(pCtx,src,ppszCopy)			SG_strdup(pCtx,src,ppszCopy)
#endif

//////////////////////////////////////////////////////////////////

void SG_strdup(SG_context *, const char * pszSrc, char ** ppszCopy);

//////////////////////////////////////////////////////////////////

/**
 * Returns SG_ERR_OK if the Src string did not need to be truncated.
 *
 * If the string got truncated, returns SG_ERR_BUFFERTOOSMALL, but the
 * operation still happened.
 *
 */
void SG_strcpy(
	SG_context *,
	char * szDest,
	SG_uint32 lenDest, /**< This includes the null terminator.
						* Example: If you pass 20 for lenDest, then
						* exactly 20 bytes will be modified by this
						* routine, and the 20th byte (at szDest+19)
						* will be zero, regardless of the length of
						* szSrc. */
	const char * szSrc
	);

/**
 * Returns SG_ERR_OK if the Src string did not need to be truncated.
 *
 * If the string got truncated, returns SG_ERR_BUFFERTOOSMALL, but the
 * operation still happened.
 *
 */
void SG_strcat(
	SG_context *,
	char * bufDest,
	SG_uint32 lenDest,
	const char * szSrc
	);

void SG_sprintf(
	SG_context *,
	char * pBuf,
	SG_uint32 lenBuf,
	const char * szFormat,
	...
	);

/**
* Like SG_sprintf, but truncates anything longer than lenBuf.
*/
void SG_sprintf_truncate(SG_context *, char * pBuf, SG_uint32 lenBuf, const char * szFormat, ...);
void SG_vsprintf_truncate(SG_context *, char * pBuf, SG_uint32 lenBuf, const char * szFormat, va_list ap);

SG_int32 SG_stricmp(const char *, const char *);
SG_int32 SG_memicmp(const SG_byte *, const SG_byte *, SG_uint32 numBytes);

SG_bool SG_ascii__is_valid(const char *);

/**
 *	free each char * in a counted char ** array, then free the array
 *	ignores any NULL entries found
 */
void SG_freeStringList(
	SG_context *pCtx,
	const char ***strings,	/**< pointer to the array; will be NULL on return */
	SG_uint32 count);	/**< count of strings in the list */

/**
 * If not found, *pResult is set to SG_UINT32_MAX.
 * Note: Case sensitive.
 */
void SG_ascii__find__char(SG_context *, const char *, char, SG_uint32 * pResult);

void SG_ascii__substring(SG_context *, const char *, SG_uint32 start, SG_uint32 length, char ** ppResult);
void SG_ascii__substring__to_end(SG_context *, const char *, SG_uint32 start, char ** ppResult);

//////////////////////////////////////////////////////////////////

void SG_int64__parse(SG_context *, SG_int64* pResult, const char*);
void SG_int64__parse__strict(SG_context *, SG_int64* pResult, const char*);
void SG_int64__parse__stop_on_nondigit(SG_context *, SG_int64* pResult, const char*);
void SG_uint32__parse(SG_context *, SG_uint32* pResult, const char*);
void SG_uint32__parse__strict(SG_context *, SG_uint32* pResult, const char*);
void SG_double__parse(SG_context *, double* pResult, const char*);

/*
 * Need maximum twenty characters for biggest uint64 or int64
 * (positive or negative). Plus one character for the null
 * terminator.
 */
typedef char SG_int_to_string_buffer[21];

/**
 * Convert an SG_int64 to a string.  The main reason this routine
 * exists is because the printf sequence for SG_int64 varies
 * depending on whether the machine is 32 bit or 64 bit.  For
 * a 32 bit machine, a 64 bit integer is probably a "long long",
 * so we need to use %lld.  On a 64 bit machine, a 64 bit
 * integer is probably a "long", so we need to use %ld.
 *
 * Bottom line:  don't SG_sprintf on an SG_int64 directly.  Use
 * this routine instead.
 */
char * SG_int64_to_sz(SG_int64, SG_int_to_string_buffer);

/**
 * Converts (the decimal representation of) the given integer into
 * a null-terminated character array in the buffer provided, and
 * returns that buffer.
 */
char * SG_uint64_to_sz(SG_uint64, SG_int_to_string_buffer);

/**
 * Converts the hexidecimal representation of the given integer into
 * a null-terminated character array in the buffer provided, and
 * returns that buffer. The string is padded with leading zeros so
 * that 16 digits are shown.
 */
char * SG_uint64_to_sz__hex(SG_uint64, SG_int_to_string_buffer);

//////////////////////////////////////////////////////////////////

END_EXTERN_C

#endif//H_SG_STR_UTILS_H
