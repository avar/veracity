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
 * @file sg_apple_unicode_prototypes.h
 *
 * @details Unicode-related routines that compute results using the
 * same data tables as Apple's OS X.  These results may differ than
 * those produced by ICU.  Apple's data tables are bug/feature frozen
 * with what they have in their kernel and HFS{,+,X} filesystem code.
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_APPLE_UNICODE_PROTOTYPES_H
#define H_SG_APPLE_UNICODE_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Compute Apple-style NFD on the given UTF-8 input string and return
 * a freshly-allocated UTF-8 result string.
 *
 * The caller must free the returned buffer.
 *
 * Returns SG_ERR_ERRNO(EINVAL) if the input is not valid UTF-8.
 *
 */
void SG_apple_unicode__compute_nfd__utf8(SG_context * pCtx,
										 const char * szUtf8Input, SG_uint32 lenInput,
										 char ** ppBufUtf8Result, SG_uint32 * pLenBufResult);

/**
 * Compute Apple-style NFC on the given UTF-8 input string and return
 * a freshly-allocated UTF-8 result string.
 *
 * The caller must free the returned buffer.
 *
 * Returns SG_ERR_ERRNO(EINVAL) if the input is not valid UTF-8.
 *
 */
void SG_apple_unicode__compute_nfc__utf8(SG_context * pCtx,
										 const char * szUtf8Input, SG_uint32 lenInput,
										 char ** ppBufUtf8Result, SG_uint32 * pLenBufResult);

/**
 * Inspects the given file/directory name and returns whether it contains
 * one or more Services-for-Macintosh (SFM) substitution characters.
 *
 * When a MAC references a file name containing an NTFS-invalid character
 * (such as a control character, "*", "<", etc, it replaces them with
 * Private Use Characters.
 */
void SG_apple_unicode__contains_sfm_chars__utf32(SG_context * pCtx,
												 const SG_int32 * pUtf32BufInput,
												 SG_bool * pbResult);

/**
 * Map any SFM character to its US-ASCII equivalent.  For non-SFM characters
 * the original character is returned.
 */
void SG_apple_unicode__map_sfm_char_to_ascii__utf32(SG_context * pCtx,
													const SG_int32 ch32In, SG_int32 * pCh32Out);

/**
 * Inspects the given file/directory name and returns whether it contains
 * one or more Unicode Ignorable characters.
 *
 * The given name is assumed to be UTF-32/UCS-4 and null terminated.
 * (This implies that we do not support null characters in file/directory
 * names.  I think the MAC might actually allow this, but I don't care.)
 *
 */
void SG_apple_unicode__contains_ignorables__utf32(SG_context * pCtx,
												  const SG_int32 * pUtf32BufInput,
												  SG_bool * pbResult);

/**
 * Test if the given UTF-32 character is an Ignorable.
 *
 */
void SG_apple_unicode__is_ignorable_char__utf32(SG_context * pCtx,
												const SG_int32 ch32In, SG_bool * pbResult);

/**
 * Compute the lowercase character of the given character.  Returns the original
 * character for non-letters.
 *
 */
void SG_apple_unicode__to_lower_char__utf32(SG_context * pCtx,
											SG_int32 ch32In, SG_int32 * pCh32Out);

/**
 * Copy the given file/directory name from the input to output buffer.  During
 * the copy it converts to lowercase and strips out any Unicode Ignorable
 * characters.
 *
 * Both strings are assumed to be UTF-32/UCS-4.  The input sting must be
 * null terminated.
 *
 * The output buffer will be null terminated and must be at least as large
 * as the string in the input buffer.
 *
 */
void SG_apple_unicode__fold_case_strip_ignorables__utf32(SG_context * pCtx,
														 const SG_int32 * pUtf32BufInput,
														 SG_int32 * pUtf32BufOutput);



//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_APPLE_UNICODE_PROTOTYPES_H
