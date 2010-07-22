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
 * @file sg_gid_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_GID_PROTOTYPES_H
#define H_SG_GID_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Compute/generate a new GID and return it as a null-terminated
 * string in an allocated buffer.  The returned string will be
 * SG_GID_ACTUAL_LENGTH characters long in a SG_GID_BUFFER_LENGTH
 * buffer.
 *
 * Formerly: SG_id__alloc__new_gid()
 */
void SG_gid__alloc(SG_context * pCtx, char ** ppszGid);

/**
 * Compute/generate a new GID and put it in the provided buffer.
 *
 * The buffer must be SG_GID_BUFFER_LENGTH or larger.
 *
 * Formerly: SG_id__generate_double_uuid()
 */
void SG_gid__generate(SG_context * pCtx,
					  char * bufGid, SG_uint32 lenBuf);

//////////////////////////////////////////////////////////////////

/**
 * HACK Remove this.  The old SG_id__generate_double_uuid() did not
 * take a buffer length.  There were places where the caller received
 * the buffer from its caller without length information.  This version
 * assumes the buffer length until we can fix the callers too.
 */
void SG_gid__generate_hack(SG_context * pCtx,
						   char * bufGid);

//////////////////////////////////////////////////////////////////

/**
 * Verify that the given string has the format of a valid GID.  This
 * only verifies the format of the string; it does not lookup the GID
 * anywhere.
 *
 * Returns SG_FALSE in pbValid if the given string does not look
 * like a proper GID.
 *
 * We throw on any other errors (such as null pointers).
 *
 * Formerly: SG_id__could_string_be_a_gid()
 */
void SG_gid__verify_format(SG_context * pCtx, const char * pszGid, SG_bool * pbValid);

/**
 * Verify that the given string has the format of a valid GID.
 * This version THROWS on any error.
 *
 * Formerly: SG_id__could_string_be_a_gid()
 */
void SG_gid__argcheck(SG_context * pCtx, const char * pszGid);

//////////////////////////////////////////////////////////////////

/**
 * Create a copy of the given GID in an allocated buffer.
 * You own the result and must free it.
 *
 * This is a convenience wrapper around SG_strdup() that
 * first verifies that your string is a properly formatted
 * GID.
 */
void SG_gid__alloc_clone(SG_context * pCtx, const char * pszGid_Input, char ** ppszGid_Copy);

/**
 * Copy the given GID into the given buffer.
 *
 * This is a convenience wrapper around SG_strcpy() that
 * first verifies that your string is a properly formattted
 * GID.
 */
void SG_gid__copy_into_buffer(SG_context * pCtx, char * pBufGid, SG_uint32 lenBufGid, const char * pszGid_Input);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_GID_PROTOTYPES_H
