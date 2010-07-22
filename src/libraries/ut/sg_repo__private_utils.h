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
 * @file sg_repo__private_utils.h
 *
 * @details Prototypes for private utility routines for REPOs that
 * are *below* the REPO API line.  Stuff in this file SHOULD NOT
 * be seen by code above the REPO API line.
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_REPO__PRIVATE_UTILS_H
#define H_SG_REPO__PRIVATE_UTILS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Generic routine to fetch all of the hash-methods defined in lib-SGHASH.
 *
 * We *assume* that a REPO implementation will use lib-SGHASH for all of
 * its hashing needs and that it will want to advertise ALL of the available
 * hash-methods.
 *
 * You own the returned vhash.
 */
void sg_repo_utils__list_hash_methods__from_sghash(SG_context * pCtx,
												   SG_vhash ** pp_vhash);

//////////////////////////////////////////////////////////////////

/**
 * Return the value that strlen() would return for any hash generated
 * with this hash-method.
 */
void sg_repo_utils__get_hash_length__from_sghash(SG_context * pCtx,
												 const char * pszHashMethod,
												 SG_uint32 * p_strlen_hashes);

//////////////////////////////////////////////////////////////////

/**
 * Generic routine to {begin,chunk,end,abort} a hash computation using
 * lib-SGHASH.  All repo-implementations can choose to use this common
 * gateway to lib-SGHASH -- or they are free to choose another hash
 * library.
 */
void sg_repo_utils__hash_begin__from_sghash(SG_context * pCtx,
											const char * pszHashMethod,
											SG_repo_hash_handle ** ppHandle);

void sg_repo_utils__hash_chunk__from_sghash(SG_context * pCtx,
											SG_repo_hash_handle * pHandle,
											SG_uint32 len_chunk,
											const SG_byte * p_chunk);

void sg_repo_utils__hash_end__from_sghash(SG_context * pCtx,
										  SG_repo_hash_handle ** ppHandle,
										  char ** ppsz_hid_returned);

void sg_repo_utils__hash_abort__from_sghash(SG_context * pCtx,
											SG_repo_hash_handle ** ppHandle);

//////////////////////////////////////////////////////////////////

/**
 * Convenience wrapper that calls {begin,chunk,end} on a buffer
 * and computes a hash.
 */
void sg_repo_utils__one_step_hash__from_sghash(SG_context * pCtx,
											   const char * pszHashMethod,
											   SG_uint32 lenBuf,
											   const SG_byte * pBuf,
											   char ** ppsz_hid_returned);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_REPO__PRIVATE_UTILS_H
