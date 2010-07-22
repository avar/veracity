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
 * @file sghash.c
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg_defines.h>
#include <sg_stdint.h>
#include <sg_error_typedefs.h>
#include "sghash.h"
#include "sghash__private.h"

//////////////////////////////////////////////////////////////////

extern const SGHASH_algorithm SGHASH_alg__sha1;
extern const SGHASH_algorithm SGHASH_alg__sha2_256;
extern const SGHASH_algorithm SGHASH_alg__sha2_384;
extern const SGHASH_algorithm SGHASH_alg__sha2_512;
extern const SGHASH_algorithm SGHASH_alg__skein_256;
extern const SGHASH_algorithm SGHASH_alg__skein_512;
extern const SGHASH_algorithm SGHASH_alg__skein_1024;

static const SGHASH_algorithm * SGHASH_alg__list[] = { &SGHASH_alg__sha1,
													   &SGHASH_alg__sha2_256,
													   &SGHASH_alg__sha2_384,
													   &SGHASH_alg__sha2_512,
													   &SGHASH_alg__skein_256,
													   &SGHASH_alg__skein_512,
													   &SGHASH_alg__skein_1024,
};

#define SGHASH_ALGORITHM__DEFAULT		SGHASH_alg__sha2_256

//////////////////////////////////////////////////////////////////

SG_error SGHASH_get_nth_hash_method_name(SG_uint32 n,
										 char * pBufResult, SG_uint32 lenBuf,
										 SG_uint32 * pStrlenHashes)
{
	if (n >= SG_NrElements( SGHASH_alg__list ))
		return SG_ERR_INDEXOUTOFRANGE;

	if (pBufResult)
	{
		SG_uint32 lenHashMethodName = strlen(SGHASH_alg__list[n]->pszHashMethod);

		if (lenBuf < (lenHashMethodName + 1))
			return SG_ERR_INVALIDARG;

#if defined(WINDOWS)
		memcpy_s(pBufResult, lenBuf, SGHASH_alg__list[n]->pszHashMethod, lenHashMethodName+1);
#else
		memcpy(pBufResult, SGHASH_alg__list[n]->pszHashMethod, lenHashMethodName+1);
#endif
	}

	if (pStrlenHashes)
		*pStrlenHashes = SGHASH_alg__list[n]->strlen_Hash;

	return SG_ERR_OK;
}

//////////////////////////////////////////////////////////////////

SG_error SGHASH_get_hash_length(const char * pszHashMethod,
								SG_uint32 * pStrlenHashes)
{
	int kLimit = SG_NrElements( SGHASH_alg__list );
	int k;

	if (!pStrlenHashes)
		return SG_ERR_INVALIDARG;

	for (k=0; k<kLimit; k++)
	{
		if (strcmp(pszHashMethod, SGHASH_alg__list[k]->pszHashMethod) == 0)
		{
			*pStrlenHashes = SGHASH_alg__list[k]->strlen_Hash;
			return SG_ERR_OK;
		}
	}

	return SG_ERR_UNKNOWN_HASH_METHOD;
}

//////////////////////////////////////////////////////////////////

static SG_error _do_init(const SGHASH_algorithm * pVTable, SGHASH_handle ** ppHandle)
{
	SGHASH_handle * pHandle = (SGHASH_handle *)malloc( sizeof(SGHASH_handle) + pVTable->sizeof_AlgCtx );

	if (!pHandle)
		return SG_ERR_MALLOCFAILED;

	pHandle->pVTable = pVTable;

	(*pHandle->pVTable->fn_hash_init)( (void *)pHandle->variable );

	*ppHandle = pHandle;
	return SG_ERR_OK;
}

SG_error SGHASH_init(const char * pszHashMethod, SGHASH_handle ** ppHandle)
{
	int kLimit = SG_NrElements( SGHASH_alg__list );
	int k;

	if (!ppHandle)
		return SG_ERR_INVALIDARG;

	if (!pszHashMethod || !*pszHashMethod)
		return _do_init( &SGHASH_ALGORITHM__DEFAULT, ppHandle);

	for (k=0; k<kLimit; k++)
		if (strcmp(pszHashMethod, SGHASH_alg__list[k]->pszHashMethod) == 0)
			return _do_init( SGHASH_alg__list[k], ppHandle);

	return SG_ERR_UNKNOWN_HASH_METHOD;
}

SG_error SGHASH_update(SGHASH_handle * pHandle, const SG_byte * pBuf, SG_uint32 lenBuf)
{
	if (!pHandle)
		return SG_ERR_INVALIDARG;

	(*pHandle->pVTable->fn_hash_update)( (void *)pHandle->variable, pBuf, lenBuf);
	return SG_ERR_OK;
}

SG_error SGHASH_final(SGHASH_handle ** ppHandle, char * pBufResult, SG_uint32 lenBuf)
{
	SGHASH_handle * pHandle;
	SG_error err;

	if (!ppHandle || !*ppHandle)
		return SG_ERR_INVALIDARG;

	pHandle = *ppHandle;			// we own the handle now
	*ppHandle = NULL;				// regardless of what happens

	if (!pBufResult)
	{
		err = SG_ERR_INVALIDARG;
	}
	else if (lenBuf < pHandle->pVTable->strlen_Hash + 1)
	{
		err = SG_ERR_INVALIDARG;
	}
	else
	{
		(*pHandle->pVTable->fn_hash_final)( (void *)pHandle->variable, pBufResult);
		assert( strlen(pBufResult) == pHandle->pVTable->strlen_Hash );
		err = SG_ERR_OK;
	}

	free(pHandle);

	return err;
}

void SGHASH_abort(SGHASH_handle ** ppHandle)
{
	SGHASH_handle * pHandle;

	if (!ppHandle || !*ppHandle)
		return;

	pHandle = *ppHandle;			// we own the handle now
	*ppHandle = NULL;				// regardless of what happens

	free(pHandle);
}

