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
 * @file sg_tid_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_TID_PROTOTYPES_H
#define H_SG_TID_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Compute/generate a new maximum-width TID and return it as a
 * null-terminated string in an allocated buffer.
 *
 * The returned buffer will be of size SG_TID_MAX_BUFFER_LENGTH
 * (including the terminating null).
 */
void SG_tid__alloc(SG_context * pCtx, char ** ppszTid);

/**
 * Compute/generate a new maximum-width TID and put it in the
 * provided buffer.
 *
 * The size of your buffer must have be at least SG_TID_MAX_BUFFER_LENGTH
 * bytes (which includes the terminating null).
 */
void SG_tid__generate(SG_context * pCtx,
					  char * bufTid, SG_uint32 lenBuf);

//////////////////////////////////////////////////////////////////

/**
 * Compute/generate a new variable-width TID and return it in
 * the allocated buffer.
 *
 * You can request the number of hex digits in the random part; this
 * must not exceed SG_TID_LENGTH_RANDOM_MAX.
 */
void SG_tid__alloc2(SG_context * pCtx,
					char ** ppszTid,
					SG_uint32 nrRandomDigits);

/**
 * Compute/generate a new variable-width TID and put it in the
 * provided buffer.
 *
 * You can request the number of hex digits in the random part; this
 * must not exceed SG_TID_LENGTH_RANDOM_MAX.
 *
 * The size of your buffer must have be at least
 * (SG_TID_LENGTH_PREFIX + nrRandomDigits + 1).  As a convenience,
 * you should probably juse provide a buffer with at least
 * SG_TID_MAX_BUFFER_LENGTH bytes.
 */
void SG_tid__generate2(SG_context * pCtx,
					   char * bufTid, SG_uint32 lenBuf,
					   SG_uint32 nrRandomDigits);

/**
 * Generate a variable-width TID, append the given suffix, and put
 * it in the buffer provided.
 */
void SG_tid__generate2__suffix(SG_context * pCtx,
							   char * bufTid, SG_uint32 lenBuf,
							   SG_uint32 nrRandomDigits,
							   const char * pszSuffix);

//////////////////////////////////////////////////////////////////

/**
 * HACK Remove this.  The old SG_id__generate_double_uuid() did not
 * take a buffer length.  There were places where the caller was using
 * a GID when really it just wanted (what we now call) a TID and received
 * the buffer from its caller without length information.  And in some
 * cases prepended a prefix to the buffer and passed a pointer to the
 * middle of the buffer.
 *
 * Here we *ASSUME* that the buffer contains SG_TID_MAX_BUFFER_LENGTH bytes;
 * this may not be true.
 */
void SG_tid__generate_hack(SG_context * pCtx,
						   char * bufGid);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_TID_PROTOTYPES_H
