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
 * @file sg_repo__private_utils.c
 *
 * @details This file is intended to have common functions that are *below* the
 * REPO API line.  That is, private routines that one or more repo implementations
 * have in common and that ARE NOT VISIBLE above the REPO API line.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_repo__private_utils.h"
#include <sghash.h>

//////////////////////////////////////////////////////////////////

void sg_repo_utils__list_hash_methods__from_sghash(SG_context * pCtx,
												   SG_vhash ** pp_vhash)
{
	SG_vhash * pvh_Allocated = NULL;
	char buf[SG_REPO_HASH_METHOD__MAX_BUFFER_LENGTH];
	SG_uint32 strlen_hashes;
	SG_uint32 k;

	SG_NULLARGCHECK_RETURN(pp_vhash);

	SG_ERR_CHECK_RETURN(  SG_VHASH__ALLOC(pCtx, &pvh_Allocated)  );

	for (k=0; (1); k++)
	{
		SG_error err = SGHASH_get_nth_hash_method_name(k, buf, sizeof(buf), &strlen_hashes);
		if (err == SG_ERR_INDEXOUTOFRANGE)
			break;
		else if (SG_IS_ERROR(err))
			SG_ERR_THROW(err);

		// add the hash-method name as a KEY in the vhash.
		//
		// we don't really care what the VALUE is in the PAIR
		// because the hash-method name contains all the info
		// that the caller needs, but we have to give the vhash
		// something, so we might as well give it the hash-length.

		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_Allocated, buf, (SG_int64)strlen_hashes)  );
	}

	*pp_vhash = pvh_Allocated;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh_Allocated);
}

//////////////////////////////////////////////////////////////////

void sg_repo_utils__get_hash_length__from_sghash(SG_context * pCtx,
												 const char * pszHashMethod,
												 SG_uint32 * p_strlen_hashes)
{
	SG_error err;
	SG_uint32 len;

	SG_NULLARGCHECK_RETURN(pszHashMethod);
	SG_NULLARGCHECK_RETURN(p_strlen_hashes);

	err = SGHASH_get_hash_length(pszHashMethod, &len);
	if (SG_IS_ERROR(err))
		SG_ERR_THROW_RETURN(err);

	*p_strlen_hashes = len;
}

//////////////////////////////////////////////////////////////////

void sg_repo_utils__hash_begin__from_sghash(SG_context * pCtx,
											const char * pszHashMethod,
											SG_repo_hash_handle ** ppHandle)
{
    SGHASH_handle * p_sghash_handle = NULL;
	SG_error err;

	SG_NONEMPTYCHECK_RETURN(pszHashMethod);
	SG_NULLARGCHECK_RETURN(ppHandle);

	err = SGHASH_init(pszHashMethod, &p_sghash_handle);
	if (SG_IS_ERROR(err))
		SG_ERR_THROW_RETURN(  err  );

	*ppHandle = (SG_repo_hash_handle *)p_sghash_handle;
}

void sg_repo_utils__hash_chunk__from_sghash(SG_context * pCtx,
											SG_repo_hash_handle * pHandle,
											SG_uint32 len_chunk,
											const SG_byte * p_chunk)
{
	SGHASH_handle * p_sghash_handle = (SGHASH_handle *)pHandle;
	SG_error err;

	SG_NULLARGCHECK_RETURN(p_sghash_handle);
	// we allow a null or zero-length buffer

	err = SGHASH_update(p_sghash_handle, p_chunk, len_chunk);
	if (SG_IS_ERROR(err))
		SG_ERR_THROW_RETURN(  err  );
}

void sg_repo_utils__hash_end__from_sghash(SG_context * pCtx,
										  SG_repo_hash_handle ** ppHandle,
										  char ** ppsz_hid_returned)
{
	SGHASH_handle * p_sghash_handle;
	char bufHid[SG_HID_MAX_BUFFER_LENGTH];
	SG_error err;

	SG_NULLARGCHECK_RETURN(ppHandle);
	SG_NULLARGCHECK_RETURN(*ppHandle);
	SG_NULLARGCHECK_RETURN(ppsz_hid_returned);

	p_sghash_handle = (SGHASH_handle *)*ppHandle;	// we own caller's structure now
	*ppHandle = NULL;								// regardless of what happens.

	// compute the hash into a stack buffer -- because SGHASH cannot
	// call our (audited) malloc -- and we want to do that, so our
	// callers won't have to know which version of free() to call.

	err = SGHASH_final(&p_sghash_handle, bufHid, sizeof(bufHid));
	SG_ASSERT(  (p_sghash_handle == NULL)  );		// regardless of success/fail, SGHASH should have freed and nulled our pointer

	if (SG_IS_ERROR(err))
		SG_ERR_THROW_RETURN(  err  );

	SG_ASSERT(  (bufHid[0])  );						// SGHASH should have populated our buffer with the hex digit string

	// return an allocated copy of the hex digit string using our version of malloc.
	SG_ERR_CHECK_RETURN(  SG_STRDUP(pCtx, bufHid, ppsz_hid_returned)  );
}

void sg_repo_utils__hash_abort__from_sghash(SG_context * pCtx,
											SG_repo_hash_handle ** ppHandle)
{
	SGHASH_handle * p_sghash_handle;

	SG_NULLARGCHECK_RETURN(ppHandle);
	if (!*ppHandle)
		return;

	p_sghash_handle = (SGHASH_handle *)*ppHandle;	// we own caller's structure now
	*ppHandle = NULL;								// regardless of what happens.

	SGHASH_abort(&p_sghash_handle);
	SG_ASSERT(  (p_sghash_handle == NULL)  );		// SGHASH should have freed and nulled our pointer
}

//////////////////////////////////////////////////////////////////

void sg_repo_utils__one_step_hash__from_sghash(SG_context * pCtx,
											   const char * pszHashMethod,
											   SG_uint32 lenBuf,
											   const SG_byte * pBuf,
											   char ** ppsz_hid_returned)
{
	SG_repo_hash_handle * pHandle = NULL;

	SG_NONEMPTYCHECK_RETURN(pszHashMethod);
	SG_NULLARGCHECK_RETURN(ppsz_hid_returned);
	// we allow a null or zero-length buffer

	SG_ERR_CHECK(  sg_repo_utils__hash_begin__from_sghash(pCtx,pszHashMethod,&pHandle)  );
	SG_ERR_CHECK(  sg_repo_utils__hash_chunk__from_sghash(pCtx,pHandle,lenBuf,pBuf)  );
	SG_ERR_CHECK(  sg_repo_utils__hash_end__from_sghash(pCtx,&pHandle,ppsz_hid_returned)  );
	return;

fail:
	SG_ERR_IGNORE(  sg_repo_utils__hash_abort__from_sghash(pCtx,&pHandle)  );
}
