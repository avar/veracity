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
 * @file sghash.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SGHASH_H
#define H_SGHASH_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

typedef struct _sghash_algorithm SGHASH_algorithm;
typedef struct _sghash_handle    SGHASH_handle;

//////////////////////////////////////////////////////////////////

/**
 * Enumerate the various hash-methods that we support.
 *
 * Optionally copies the name of the nth hash-method into the supplied buffer.
 * If no buffer is supplied, the name is not returned.
 *
 * Optionally returns the string-length of a hash result.
 * This is the strlen() of a hash-result expressed as a hex digit string
 * (excluding the terminating NULL).  THIS IS NOT THE LENGTH OF THE
 * BINARY VERSION OF THE HASH RESULT.
 *
 * For example, we may return:
 *     "SHA1/160" and 40
 *     "SHA2/256" and 64.
 *
 * Returns SG_ERR_INDEXOUTOFRANGE if n exceeds the number of hash-methods defined.
 * Returns SG_ERR_INVALIDARG if the buffer isn't big enough to hold the name.
 *
 */
SG_error SGHASH_get_nth_hash_method_name(SG_uint32 n,
										 char * pBufResult, SG_uint32 lenBuf,
										 SG_uint32 * pStrlenHashes);

/**
 * Return the string-length of a hash result using this hash-method.
 * This is the strlen() of a hash-result expressed as a hex digit string
 * (excluding the terminating NULL).  THIS IS NOT THE LENGTH OF THE
 * BINARY VERSION OF THE HASH RESULT.
 *
 * The value after the "/" in the hash-method name is the bit-length
 * of a hash-result.  So, for example, for "SHA1/160", we have
 * ((160 / 8) * 2) == 40 hex digits.
 *
 * Returns SG_ERR_UNKNOWN_HASH_METHOD if the given name does not match an
 * available hash-method.
 */
SG_error SGHASH_get_hash_length(const char * pszHashMethod,
								SG_uint32 * pStrlenHashes);

/**
 * Begin a hash computation using the named hash-method.
 * If no hash-method is provided, we use the default hash-method (currently "SHA2/256").
 *
 * A hash handled is ALLOCATED and RETURNED.  You own this.  Call SGHASH_final() or
 * SGHASH_abort() to free it.
 *
 * Returns SG_ERR_UNKNOWN_HASH_METHOD if the given name does not match an
 * available hash-method.
 */
SG_error SGHASH_init(const char * pszHashMethod, SGHASH_handle ** ppHandle);

/**
 * Add content to the hash.
 */
SG_error SGHASH_update(SGHASH_handle * pHandle, const SG_byte * pBuf, SG_uint32 lenBuf);

/**
 * Finish the hash computation and ALLOCATE and fill the provided buffer
 * with a hex digit string representation of the hash result.  The provided
 * buffer must at least (strlen-hashes + 1).
 *
 * The hash handle is freed and your pointer is nulled.
 */
SG_error SGHASH_final(SGHASH_handle ** ppHandle, char * pBufResult, SG_uint32 lenBuf);

/**
 * Abandon a hash computation and free the hash handle.  Your pointer
 * is nulled.
 */
void SGHASH_abort(SGHASH_handle ** ppHandle);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SGHASH_H
