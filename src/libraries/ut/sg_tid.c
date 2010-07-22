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
 * @file sg_tid.c
 *
 * @details Minimal routines to create/manipulate Temporary IDs (TIDs).
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

void SG_tid__generate2(SG_context * pCtx,
					   char * bufTid, SG_uint32 lenBuf,
					   SG_uint32 nrRandomDigits)
{
	char bufGid[SG_GID_BUFFER_LENGTH];
	SG_uint32 lenRequired;

	SG_NULLARGCHECK_RETURN( bufTid );

	// Note: Arbitrarily enforce a 4 digit lower bound.
	SG_ARGCHECK_RETURN(  ((nrRandomDigits > 4) && (nrRandomDigits <= SG_TID_LENGTH_RANDOM_MAX)), nrRandomDigits  );

	lenRequired = SG_TID_LENGTH_PREFIX + nrRandomDigits + 1;	// +1 for trailing null

	SG_ARGCHECK_RETURN( (lenBuf >= lenRequired), lenBuf );

	// We create a real GID.  This gives us a lot of random hex digits.
	// We then use the first n hex digits to create a TID.  We assume
	// that a TID is narrower than a GID (or else we'd have to create
	// more than one GID).
	SG_ASSERT(  (SG_TID_LENGTH_RANDOM_MAX <= SG_GID_LENGTH_RANDOM)  );
	SG_ERR_CHECK_RETURN(  SG_gid__generate(pCtx, bufGid, sizeof(bufGid))  );

	SG_ASSERT(  (SG_TID_LENGTH_PREFIX == 1)  );

	bufTid[0] = 't';
	memcpy(&bufTid[SG_TID_LENGTH_PREFIX], &bufGid[SG_GID_LENGTH_PREFIX], nrRandomDigits);
	bufTid[SG_TID_LENGTH_PREFIX + nrRandomDigits] = 0;
}

void SG_tid__generate(SG_context * pCtx,
					  char * bufTid, SG_uint32 lenBuf)
{
	SG_ERR_CHECK_RETURN(  SG_tid__generate2(pCtx, bufTid, lenBuf, SG_TID_LENGTH_RANDOM_MAX)  );
}

//////////////////////////////////////////////////////////////////

void SG_tid__generate_hack(SG_context * pCtx,
						   char * bufTid)
{
	SG_ERR_CHECK_RETURN(  SG_tid__generate(pCtx, bufTid, SG_TID_MAX_BUFFER_LENGTH)  );
}

//////////////////////////////////////////////////////////////////

void SG_tid__alloc(SG_context * pCtx,
				   char ** ppszTid)
{
	char * pszTid_Allocated = NULL;

	SG_NULLARGCHECK_RETURN( ppszTid );

	SG_ERR_CHECK_RETURN(  SG_alloc(pCtx, 1, SG_TID_MAX_BUFFER_LENGTH, &pszTid_Allocated)  );
	SG_ERR_CHECK(  SG_tid__generate(pCtx, pszTid_Allocated, SG_TID_MAX_BUFFER_LENGTH)  );

	*ppszTid = pszTid_Allocated;
	return;

fail:
	SG_NULLFREE(pCtx, pszTid_Allocated);
}

void SG_tid__alloc2(SG_context * pCtx,
					char ** ppszTid,
					SG_uint32 nrRandomDigits)
{
	char * pszTid_Allocated = NULL;

	SG_NULLARGCHECK_RETURN( ppszTid );

	SG_ERR_CHECK_RETURN(  SG_alloc(pCtx, 1, SG_TID_MAX_BUFFER_LENGTH, &pszTid_Allocated)  );
	SG_ERR_CHECK(  SG_tid__generate2(pCtx, pszTid_Allocated, SG_TID_MAX_BUFFER_LENGTH, nrRandomDigits)  );

	*ppszTid = pszTid_Allocated;
	return;

fail:
	SG_NULLFREE(pCtx, pszTid_Allocated);
}

//////////////////////////////////////////////////////////////////

void SG_tid__generate2__suffix(SG_context * pCtx,
							   char * bufTid, SG_uint32 lenBuf,
							   SG_uint32 nrRandomDigits,
							   const char * pszSuffix)
{
	SG_uint32 lenSuffix;

	SG_NULLARGCHECK_RETURN( bufTid );
	SG_NONEMPTYCHECK_RETURN( pszSuffix );

	lenSuffix = strlen(pszSuffix);
	SG_ARGCHECK_RETURN(  ((nrRandomDigits + 1 + lenSuffix + 1) <= lenBuf), lenBuf  );

	SG_ERR_CHECK_RETURN(  SG_tid__generate2(pCtx, bufTid, lenBuf, nrRandomDigits)  );
	SG_ERR_CHECK_RETURN(  SG_strcat(pCtx, bufTid, lenBuf, ".")  );
	SG_ERR_CHECK_RETURN(  SG_strcat(pCtx, bufTid, lenBuf, pszSuffix)  );
}
