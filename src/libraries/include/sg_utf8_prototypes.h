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
 * @file sg_utf8_prototypes.h
 *
 * @details Unicode and UTF-8 functions and wrappers for ICU functions.
 *
 */

#ifndef H_SG_UTF8_PROTOTYPES_H
#define H_SG_UTF8_PROTOTYPES_H

BEGIN_EXTERN_C

//////////////////////////////////////////////////////////////////

/**
 * Convert the given UTF-8 string into a minimal c-style escaped string.
 * One that is suitable for printing debug messages without hiding
 * any non-printable characters or getting slugs.
 *
 * We will allocate a buffer to contain the formatted version.  You must
 * free this.
 *
 */
void SG_utf8__sprintf_utf8_with_escape_sequences(SG_context * pCtx,
												 const char * szUtf8In,
												 char ** ppBufReturned);

//////////////////////////////////////////////////////////////////

/**
 * Compute length of a UTF-8 string <b>in BYTES</b> not characters.
 * We add 1 for the terminating null to the count.
 *
 */
SG_uint32 SG_utf8__length_in_bytes(const char* s);

/**
 * Compute length of a UTF-8 string <b>in CHARACTERS</b> not bytes.
 * We DO NOT include the terminating null in the count.
 *
 */
void SG_utf8__length_in_characters__sz(SG_context * pCtx, const char* s, SG_uint32* pResult);

/**
 * Compute length of a UTF-8 string <b>in CHARACTERS</b> not bytes.
 * We stop at either the terminating null or the number of BYTES given.
 * We DO NOT include the terminating null in the count.
 *
 */
void SG_utf8__length_in_characters__buflen(SG_context * pCtx, const char* s, SG_uint32 len_bytes, SG_uint32* pResult);

//////////////////////////////////////////////////////////////////

/**
 * Apply callback function to each CHARACTER in the given UTF-8 string.
 * We extract each character as a Unicode 32-bit character (decoding the
 * UTF-8).
 *
 * We stop iterating if the callback function returns anything other than
 * SG_ERR_OK (and we return that error code).
 *
 */
void SG_utf8__foreach_character(SG_context * pCtx,
								const char * szUtf8,
								SG_utf8__foreach_character_callback * pfn,
								void * pVoidCallbackData);

//////////////////////////////////////////////////////////////////

void SG_utf8__copy(char* dest, const char* src);
int  SG_utf8__compare(const char* s1, const char* s2);
SG_qsort_compare_function SG_utf8__compare__lexicographical;

void SG_utf8__to_utf32__sz(SG_context * pCtx,
						   const char * szUtf8,
						   SG_int32 ** ppBufResult,	// caller needs to free this
						   SG_uint32 * pLenResult);
void SG_utf8__to_utf32__buflen(SG_context * pCtx,
							   const char * szUtf8,
							   SG_uint32 len_bytes,
							   SG_int32 ** ppBufResult,	// caller needs to free this
							   SG_uint32 * pLenResult);
void SG_utf8__from_utf32(SG_context * pCtx,
						 const SG_int32 * pBufUnicode,
						 char ** pszUtf8Result,		// caller needs to free this
						 SG_uint32 * pLenResult);

//////////////////////////////////////////////////////////////////

#if defined(WINDOWS)
// WARNING: wchar_t is ***NOT*** portable (it is different sizes on different
// WARNING: platforms).  Nothing in the library should use wchar_t strings
// WARNING: EXCEPT for code which calls the "W" version Windows-specific
// WARNING: filesystem operations (like CreateFileW).  This should be localized
// WARNING: to sg_file and sg_fsobj.

/**
 * We assume that the caller wants to use the given string with the "W" version
 * of a Win32 API routine.  We need to take our (generally NFC) UTF-8 and convert
 * it to (Windows version of) UTF-16/UCS-2 data.
 *
 * We match the behavior of SG_utf8__extern_to_os_buffer__sz() so that we get
 * the same NFC/NFD treatment.
 *
 * Convert UTF8 string into an allocated wchar_t buffer.
 * The caller must free this buffer.
 * Optionally returns the length of the buffer <b>including the terminating
 * NULL in wchar_t's</b> not bytes.
 */
void SG_utf8__extern_to_os_buffer__wchar(SG_context * pCtx,
										 const char * szUtf8,
										 wchar_t ** ppBufResult,	// caller needs to free this
										 SG_uint32 * pLenResult);

/**
 * Intern the given OS wide-char string and perform any normalization required
 * to get our internal UTF-8 (generally NFC) representation.
 *
 * We match the behavior of SG_utf8__intern_from_os_buffer__sz() so that we get
 * the same NFC/NFD treatment.
 *
 * Convert wchar_t buffer into UTF-8 and store in the given string.
 * We completely replace the contents of the given string.
 *
 * This routine tries to avoid unnecessary mallocs and memcpys.
 *
 */
void SG_utf8__intern_from_os_buffer__wchar(SG_context * pCtx,
										   SG_string * pString,
										   const wchar_t * pBufUcs2);

#define SG_UTF8__INTERN_FROM_OS_BUFFER		SG_utf8__intern_from_os_buffer__wchar
#endif

#if defined(LINUX) || defined(MAC)
/**
 * Extern the given (probably NFC) UTF-8 string into an OS representation.
 * This may be UTF-8 NFD or a locale-based value.
 *
 */
void SG_utf8__extern_to_os_buffer__sz(SG_context * pCtx, const char * szUtf8, char ** pszOS);

/**
 * Intern the given OS string.  Depending upon the platform, we convert
 * the OS string from a locale charenc, UTF-8 NFD, or otherwise into our
 * internal UTF-8 (generally NFC) representation.
 *
 * We assume that the OS string was given to us by something like getcwd()
 * or readdir() and is in a platform-specific encoding.
 *
 */
void SG_utf8__intern_from_os_buffer__sz(SG_context * pCtx, SG_string * pThis, const char * szOS);

#define SG_UTF8__INTERN_FROM_OS_BUFFER		SG_utf8__intern_from_os_buffer__sz
#endif

#if defined(LINUX)
/**
 * Extern the given UTF-8 buffer into an OS locale/charset/codepage representation
 * and make substitutions for any unicode characters that are not present in the
 * character set.
 *
 * This is not needed on Windows because we use WideCharToMultiByte() to do
 * conversions on Windows.  This is not needed on MAC because the locale is
 * always set to UTF-8 on the MAC.
 *
 */
void SG_utf8__extern_to_os_buffer_with_substitutions(SG_context * pCtx, const char * szUtf8, char ** pszOS);
#endif

//////////////////////////////////////////////////////////////////

/**
 * Use ICU to convert a single UTF-32 chararcter to lowercase.
 */
void SG_utf32__to_lower_char(SG_context * pCtx, SG_int32 ch32In, SG_int32 * pCh32Out);

//////////////////////////////////////////////////////////////////

/**
 * Return the ICU error message for the given error code.
 * This is a static string.
 *
 */
const char * SG_utf8__get_icu_error_msg(SG_error e);

//////////////////////////////////////////////////////////////////

// On Linux (and possibly other Unixes (excluding OS X)) we have to deal
// with the user's LOCALE and convert filenames between the LOCALE's
// CHARSET and UTF-8 that we use internally for everything.  (Much like
// we have to convert to wchar_t on Windows.
//
// We also have to do this for command line arguments and probably for
// names stuffed into/fetched from the usual file picker dialogs.
//
// Here are special versions of the charset<-->utf8 routines that lookup
// the LOCALE's CHARSET from the user's environment, remember it in static
// storage, and use a persistent converter -- to allow us to avoid all the
// ICU setup/teardown for every filename.
//
// On non-unix platforms (where we don't have to do anything) or on
// platforms where the locale is always UTF-8 (maybe like MAC), these
// routines are pretty much pass thru (a strdup with maybe validation).

/**
 * Determine if the LOCALE CHARSET in the user's ENVIRONMENT is UTF-8.
 * This might be used to allow the caller to avoid unnecessary
 * mallocs and string copying.
 *
 */
void SG_utf8__is_locale_env_charset_utf8(SG_context * pCtx, SG_bool * pbIsEnvCharSetUtf8);

/**
 * Fetch the canonical name of the LOCALE CHARSET in the user's ENVIRONMENT.
 * This is a pointer to a static buffer that you do not own.
 *
 */
void SG_utf8__get_locale_env_charset_canonical_name(SG_context * pCtx, const char ** pszCanonicalName);

/**
 * Try to convert a CharSet/CharEnc/CharMap/CodeSet/CodePage-based
 * string into UTF-8 using the LOCALE CHARSET in the user's ENVIRONMENT.
 * This will allocate a new buffer for the result.  The caller is
 * responsible for freeing this.  If the user's environment is set to
 * UTF-8, we basically do a strdup() -- but we reserve the right to
 * validate the UTF-8 input.
 *
 * YOU PROBABLY DON'T WANT TO CALL THIS DIRECTLY.  YOU PROBABLY WANT
 * TO USE ONE OF THE SG_string WRAPPERS THAT TAKES CARE OF ALL OF THIS
 * FOR YOU.
 *
 */
void SG_utf8__from_locale_env_charset__sz(SG_context * pCtx,
										  const char * szCharSetBuffer,
										  char ** pszUtf8Result);

/**
 * Try to convert a UTF-8 string into the LOCALE CHARSET in the user's
 * ENVIRONMENT.  This will allocate a new buffer for the result.  The
 * caller is responsible for freeing this.  If the user's environment
 * is set to UTF-8, we basically do a strdup() -- but we reserve the
 * right to validate the UTF-8 input.
 *
 */
void SG_utf8__to_locale_env_charset__sz(SG_context * pCtx,
										const char * szUtf8Buffer,
										char ** pszCharSetBuffer);

//////////////////////////////////////////////////////////////////

#if defined(LINUX) && defined(DEBUG)
/**
 * Debug routine to change what we believe the locale charset is.
 * This is intended to replace the global converter so that subsequent
 * filesystem routines will use it instead of the one loaded during
 * initialization.
 *
 * This does not affect the user's environment.
 *
 */
void SG_utf8_debug__set_locale_env_charset(SG_context * pCtx,
										   const char * szCharSetName);
#endif

//////////////////////////////////////////////////////////////////

/**
 * Allocate a CHARACTER SET to/from UTF-8 converter.  This is intended
 * to create a converter that may be used many times (such as when
 * converting all UTF-8 entrynames in a directory to a specific locale
 * character set to test for collisions on another platform).  The
 * converter is based upon the given input string and ***DOES NOT***
 * reference the user's environment in any way.
 *
 * You would use this with the setting for the per-repo character set
 * from the repo's config.
 *
 * The caller must free the returned converter.
 *
 * The converter is intended for brief use (converting an entryname)
 * rather than for large, chunked buffers of data.
 *
 * This returns an error if a converter for the given character set name
 * cannot be created.
 *
 */
void SG_utf8_converter__alloc(SG_context * pCtx,
							  const char * szCharSetName,
							  SG_utf8_converter ** ppConverter);

#if defined(LINUX)
/**
 * Allocate a converter that will do substitutions rather than returning
 * an error when there are invalid/undefined characters.  that is, when
 * converting from UTF-8 to a character encoding/code page, make a substitution
 * when the UTF-8 string contains a character that is not defined in the
 * destination code page (such as a greek letter when mapping to latin-1).
 *
 */
void SG_utf8_converter__alloc_substituting(SG_context * pCtx,
										   const char * szCharSetName,
										   SG_utf8_converter ** ppConverter);
#endif

void SG_utf8_converter__free(SG_context * pCtx, SG_utf8_converter * pConverter);

/**
 * Determine if the character set referenced in the converter is UTF-8.
 * That is, is this a trivial/pass-thru?
 *
 */
void SG_utf8_converter__is_charset_utf8(SG_context * pCtx,
										const SG_utf8_converter * pConverter,
										SG_bool * pbIsCharSetUtf8);

/**
 * Fetch the canonical name of the converter.  This is a pointer to a
 * static buffer that you do not own.  This may or may not match the
 * string used to construct the converter.
 *
 */
void SG_utf8_converter__get_canonical_name(SG_context * pCtx,
										   const SG_utf8_converter * pConverter,
										   const char ** pszCanonicalName);

/**
 * Try to convert a CharSet/CharEnc/CharMap/CodeSet/CodePage-based
 * string into UTF-8 using the given converter.
 *
 * Caller needs to free the result buffer.
 *
 * Returns SG_ERR_ILLEGAL_CHARSET_CHAR if we have a byte in the input
 * buffer that is not a valid character/undefined in the CharSet/CodePage
 * and thus cannot be converted into a Unicode character.  (This happens
 * where there are holes in the Code Page, for example 0x80..0x9f in Latin1.)
 *
 */
void SG_utf8_converter__from_charset__sz(SG_context * pCtx,
										 SG_utf8_converter * pConverter,
										 const char * szCharSetBuffer,
										 char ** pszUtf8BufferResult);

/**
 * Try to convert a UTF-8 string into a CharSet/CodePage-based string
 * using the given CharSet/CodePage.
 *
 * Caller needs to free the result buffer.
 *
 * Returns SG_ERR_UNMAPPABLE_UNICODE_CHAR if we have a Unicode character
 * in the input buffer that does not exist in (and cannot be mapped to) the
 * CharSet/CodePage.
 *
 */
void SG_utf8_converter__to_charset__sz(SG_context * pCtx,
									   SG_utf8_converter * pConverter,
									   const char * szUtf8Buffer,
									   char ** pszCharSetBufferResult);

void SG_utf8_converter__from_charset__buf_len(
    SG_context * pCtx,
    SG_utf8_converter * pConverter,
    const SG_byte * pBuffer,
    SG_uint32 bufferLength,
    char ** ppResult);

//////////////////////////////////////////////////////////////////

void SG_utf8__import_buffer(
    SG_context * pCtx,
    const SG_byte * pRawBuffer,
    SG_uint32 rawBufSize,
    char ** ppSz,
    SG_string ** ppRawBufferFormatUsed);

void SG_utf8__import_buffer_in_format(
    SG_context * pCtx,
    const SG_byte * pRawBuffer,
    SG_uint32 rawBufSize,
    const char * szCharSetName,
    char ** ppSz)

//////////////////////////////////////////////////////////////////

END_EXTERN_C

#endif//H_SG_UTF8_PROTOTYPES_H
