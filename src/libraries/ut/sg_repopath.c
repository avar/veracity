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

#include <sg.h>

void SG_repopath__foreach(
	SG_context* pCtx,
	const char* pszRepoPath, /**< Should be an absolute repo path.  It may or may not have the final slash */
	SG_repopath_foreach_callback cb,
	void* ctx
	)
{
	const char* pStart = NULL;
	const char* pNext = NULL;
	SG_uint32 iLen;

	SG_NULLARGCHECK_RETURN(pszRepoPath);
	SG_NULLARGCHECK_RETURN(cb);

	SG_ARGCHECK_RETURN( (pszRepoPath[0] == '@' && pszRepoPath[1] == '/'), pszRepoPath );


	/* First call cb with "@", for the super-root */
	SG_ERR_CHECK(  cb(pCtx, pszRepoPath, 1, ctx)  );

	pStart = pszRepoPath + 2;

	while (*pStart)
	{
		pNext = strchr(pStart, '/');
		if (pNext)
		{
			iLen = (SG_uint32)(pNext - pStart);
			if (0 == iLen)
			{
				SG_ERR_THROW_RETURN( SG_ERR_INVALIDARG ); // TODO invalid repo path.  two slashes in a row.
			}

			SG_ERR_CHECK(  cb(pCtx, pStart, iLen, ctx)  );

			pStart = pNext + 1;
		}
		else
		{
			iLen = (SG_uint32)strlen(pStart);
			if (0 == iLen)
			{
				SG_ERR_THROW_RETURN( SG_ERR_INVALIDARG ); // TODO invalid repo path.  two slashes in a row.
			}

			SG_ERR_CHECK(  cb(pCtx, pStart, iLen, ctx)  );

			break;
		}
	}

fail:
	return;
}

static void sg_repopath__split__cb__count(SG_context* pCtx, const char* psPart, SG_uint32 iLen, void* ctx)
{

	SG_NULLARGCHECK_RETURN(ctx);
	SG_NULLARGCHECK_RETURN(psPart);
	SG_ARGCHECK_RETURN((0 < iLen), iLen);

	{
		SG_uint32* piCount = (SG_uint32*) ctx;

		(*piCount)++;
	}
}

struct cb_pool_stuff
{
	SG_strpool* pPool;
	const char** apszParts;
	SG_uint32 count;
};

static void sg_repopath__split__cb__pool(SG_context* pCtx, const char* psPart, SG_uint32 iLen, void* ctx)
{

	SG_NULLARGCHECK_RETURN(ctx);
	SG_NULLARGCHECK_RETURN(psPart);
	SG_ARGCHECK_RETURN((0 < iLen), iLen);

	{
		struct cb_pool_stuff* ps = (struct cb_pool_stuff*) ctx;
		const char* pszPooledCopy = NULL;

		SG_ERR_CHECK_RETURN(  SG_strpool__add__buflen__sz(pCtx, ps->pPool, psPart, iLen, &pszPooledCopy)  );
		ps->apszParts[ps->count++] = pszPooledCopy;
	}
}

void SG_repopath__split(
	SG_context* pCtx,
	const char* pszRepoPath, /**< Should be an absolute repo path.  It may or may not have the final slash */
	SG_strpool* pPool,
	const char*** papszParts,
	SG_uint32* piCountParts
	)
{
	SG_uint32 count = 0;
	const char** apszParts = NULL;
	struct cb_pool_stuff cbs;

	SG_NULLARGCHECK_RETURN(pszRepoPath);
	SG_NULLARGCHECK_RETURN(pPool);
	SG_NULLARGCHECK_RETURN(papszParts);
	SG_NULLARGCHECK_RETURN(piCountParts);

	SG_ERR_CHECK(  SG_repopath__foreach(pCtx, pszRepoPath, sg_repopath__split__cb__count, &count)  );

	if (count == 0)
	{
		if (papszParts)
			*papszParts = NULL;
		*piCountParts = 0;
		return;
	}

	if (papszParts)
	{
		SG_ERR_CHECK(  SG_allocN(pCtx, count,apszParts)  );

		cbs.pPool = pPool;
		cbs.apszParts = apszParts;
		cbs.count = 0;

		SG_ERR_CHECK(  SG_repopath__foreach(pCtx, pszRepoPath, sg_repopath__split__cb__pool, &cbs)  );

		*papszParts = apszParts;
	}

	*piCountParts = count;

	return;

fail:
	SG_NULLFREE(pCtx, apszParts);
}

void SG_repopath__has_final_slash(SG_context* pCtx, const SG_string * pThis, SG_bool * pbHasFinalSlash)
{
	// return true (in *pbHasFinalSlash) if the pathname ends with a slash.
	SG_uint32 len;
	SG_bool bHasFinalSlash;
	SG_byte byteTest;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pbHasFinalSlash);

	// an empty string is never valid.

	len = SG_string__length_in_bytes(pThis);
	SG_ARGCHECK_RETURN((len > 0), len);

	SG_ERR_CHECK_RETURN(  SG_string__get_byte_r(pCtx, pThis,0,&byteTest)  );

	bHasFinalSlash = (byteTest == '/');

	if (pbHasFinalSlash)
		*pbHasFinalSlash = bHasFinalSlash;

}

void SG_repopath__ensure_final_slash(SG_context* pCtx, SG_string * pThis)
{
	SG_bool bHasFinalSlash = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pThis);

	SG_ERR_CHECK_RETURN(  SG_repopath__has_final_slash(pCtx, pThis,&bHasFinalSlash)  );

	if (bHasFinalSlash)
		return;

	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pThis,"/")  );

}
void SG_repopath__remove_final_slash(SG_context* pCtx, SG_string * pThis)
{
	SG_bool bHasFinalSlash = SG_TRUE;
	SG_uint32 len = 0;
	SG_NULLARGCHECK_RETURN(pThis);

	while (bHasFinalSlash)
	{
		len = SG_string__length_in_bytes(pThis);
		if (len == 0)
			return;

		SG_ERR_CHECK_RETURN(  SG_repopath__has_final_slash(pCtx, pThis,&bHasFinalSlash)  );

		if (!bHasFinalSlash)
			return;


		SG_ERR_CHECK_RETURN(  SG_string__remove(pCtx, pThis, len - 1, 1 )  );
	}
}
void SG_repopath__combine__sz(
	SG_context* pCtx,
	const char* pszAbsolute,
	const char* pszRelative,
	SG_bool bFinalSlash,
	SG_string** ppstrNew)
{
	SG_string* pstrNew = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pstrNew, pszAbsolute)  );
	SG_ERR_CHECK(  SG_repopath__ensure_final_slash(pCtx, pstrNew)  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pstrNew, pszRelative)  );
	if (bFinalSlash)
	{
		SG_ERR_CHECK(  SG_repopath__ensure_final_slash(pCtx, pstrNew)  );
	}
	*ppstrNew = pstrNew;
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pstrNew);
}

void SG_repopath__combine(
	SG_context* pCtx,
	const SG_string* pstrAbsolute,
	const SG_string* pstrRelative,
	SG_bool bFinalSlash,
	SG_string** ppstrNew)
{
	SG_ERR_CHECK_RETURN(  SG_repopath__combine__sz(pCtx, SG_string__sz(pstrAbsolute), SG_string__sz(pstrRelative), bFinalSlash, ppstrNew)  );
}

void SG_repopath__append_entryname(SG_context* pCtx, SG_string * pStrRepoPath, const char * szEntryName, SG_bool bFinalSlash)
{
	SG_NULLARGCHECK_RETURN( pStrRepoPath );
	SG_NONEMPTYCHECK_RETURN( szEntryName );

	SG_ERR_CHECK(  SG_repopath__ensure_final_slash(pCtx, pStrRepoPath)  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pStrRepoPath,szEntryName)  );
	if (bFinalSlash)
		SG_ERR_CHECK(  SG_repopath__ensure_final_slash(pCtx, pStrRepoPath)  );

fail:
	return;
}

/*
 * The rules of repopath comparison.
 * 1.  Ignore case.
 * 2.  The '/' character always sorts to the top.  So that @/repopath/subfile always sorts before @/repopath extra
 * 3.  Collapse multiple "/" characters.
 */

void SG_repopath__compare(SG_context* pCtx, const char * pszPath1, const char * pszPath2, SG_int32 * pNReturnVal)
{
	SG_int32 tmpNReturnVal = 0;
	SG_uint32 nPath1Length = 0;
	SG_uint32 nPath2Length = 0;
	SG_uint32 nPath1Index = 0;
	SG_uint32 nPath2Index = 0;
	SG_bool bPath1HadSlashLastChar = SG_FALSE;
	SG_bool bPath2HadSlashLastChar = SG_FALSE;

	SG_NULLARGCHECK_RETURN( pszPath1 );
	SG_NULLARGCHECK_RETURN( pszPath2 );
	tmpNReturnVal = 0;
	if (*pszPath1 == 0 && *pszPath2 == 0)
	{
		*pNReturnVal = tmpNReturnVal;
		return;
	}
	else if (*pszPath1 == 0)
		tmpNReturnVal = -1;
	else if (*pszPath2 == 0)
		tmpNReturnVal = 1;
	if (tmpNReturnVal != 0)
	{
		*pNReturnVal = tmpNReturnVal;
		return;
	}

	nPath1Length = strlen(pszPath1);
	nPath2Length = strlen(pszPath2);

	for (nPath1Index = 0, nPath2Index = 0; nPath1Index < nPath1Length && nPath2Index < nPath2Length; nPath1Index++, nPath2Index++)
	{
		while (bPath1HadSlashLastChar && pszPath1[nPath1Index] == '/' && nPath1Index < nPath1Length)
		{
			nPath1Index++;
		}
		while (bPath2HadSlashLastChar && pszPath2[nPath2Index] == '/' && nPath2Index < nPath2Length)
		{
			nPath2Index++;
		}
		//If we walked off the end of either path while ignoring multiple '/' characters, break out;
		if ( nPath1Index >= nPath1Length || nPath2Index >= nPath2Length)
			break;

		if (pszPath1[nPath1Index] != pszPath2[nPath2Index])
		{
			if (pszPath1[nPath1Index] == '/')
			{
				*pNReturnVal = -1;
				return;
			}
			else if (pszPath2[nPath2Index] == '/')
			{
				*pNReturnVal = 1;
				return;
			}
			if (pszPath1[nPath1Index] < pszPath2[nPath2Index])
			{
				*pNReturnVal = -1;
				return;
			}
			else
			{
				*pNReturnVal = 1;
				return;
			}
		}
		bPath1HadSlashLastChar = pszPath1[nPath1Index] == '/' ? SG_TRUE : SG_FALSE;
		bPath2HadSlashLastChar = pszPath2[nPath2Index] == '/' ? SG_TRUE : SG_FALSE;
	}
	//We walked off the end of one of the strings.
	//If we are at the end of both strings, they are exactly equal.
	if ( nPath1Index >= nPath1Length && nPath2Index >= nPath2Length)
			*pNReturnVal = 0;
	else
	{
		const char * pszTheStringWithRemainingChars = NULL;
		SG_uint32 nIndexOfRemainingContent = 0;
		SG_uint32 nLengthOfString = 0;
		SG_int32 nReturnVal = 0;
		if (nPath1Index >= nPath1Length)
		{
			nIndexOfRemainingContent = nPath2Index;
			pszTheStringWithRemainingChars = pszPath2;
			nLengthOfString = nPath2Length;
			nReturnVal = -1;
		}
		else
		{
			nIndexOfRemainingContent = nPath1Index;
			pszTheStringWithRemainingChars = pszPath1;
			nLengthOfString = nPath1Length;
			nReturnVal = 1;
		}
		while (pszTheStringWithRemainingChars[nIndexOfRemainingContent] == '/' && nIndexOfRemainingContent < nLengthOfString)
		{
			nIndexOfRemainingContent++;
		}
		if (nIndexOfRemainingContent >= nLengthOfString)
			*pNReturnVal = 0;
		else
			*pNReturnVal = nReturnVal;

	}
}

void SG_repopath__normalize(SG_context* pCtx, SG_string * pStrRepoPath, SG_bool bFinalSlash)
{
	SG_NULLARGCHECK_RETURN( pStrRepoPath );

	SG_ERR_CHECK( SG_string__replace_bytes(pCtx,
										pStrRepoPath,
										(const SG_byte *)"\\",1,
										(const SG_byte *)"/",1,
										SG_UINT32_MAX,SG_TRUE,
										NULL)  );

	//Erase anything that has a "./" placeholder in it.
	SG_ERR_CHECK( SG_string__replace_bytes(pCtx,
											pStrRepoPath,
											(const SG_byte *)"./",2,
											(const SG_byte *)"",0,
											SG_UINT32_MAX,SG_TRUE,
											NULL)  );

	if (bFinalSlash)
		SG_ERR_CHECK(  SG_repopath__ensure_final_slash(pCtx, pStrRepoPath)  );
	else
		SG_ERR_CHECK(  SG_repopath__remove_final_slash(pCtx, pStrRepoPath)  );

fail:
	return;
}

void SG_repopath__get_last(SG_context* pCtx,
	SG_string* pStrRepoPath,
	SG_string** ppResult)
{
	const char* pBegin;
	const char* pEnd;
	const char * sz;
	//SG_bool bIsRoot = SG_TRUE;
	SG_string* pstr = NULL;

	SG_NULLARGCHECK_RETURN(pStrRepoPath);

	sz = SG_string__sz(pStrRepoPath);

	pEnd = sz + strlen(sz) - 1;

	if ('/' == *pEnd)
	{
		pEnd--;
	}

	pBegin = pEnd;
	while (
		('/' != *pBegin)
		&& (pBegin > sz)
		)
	{
		pBegin--;
	}
	pBegin++;

	SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC__BUF_LEN(pCtx, &pstr, (SG_byte*) pBegin, (SG_uint32)(pEnd - pBegin + 1))  );

	*ppResult =pstr;

}

void SG_repopath__remove_last(SG_context* pCtx, SG_string * pStrRepoPath)
{
	const char * sz;
	const char * szSlash;
	SG_uint32 offset;
	SG_uint32 len;

	SG_NULLARGCHECK_RETURN(pStrRepoPath);

	// remove trailing slash, if present.
	SG_ERR_CHECK_RETURN(  SG_repopath__remove_final_slash(pCtx,pStrRepoPath)  );

	if (strcmp("@", SG_string__sz(pStrRepoPath)) == 0)
		return;

	// now look back to the previous slash.

	sz = SG_string__sz(pStrRepoPath);
	szSlash = strrchr(sz,'/');

	SG_ASSERT( szSlash && "Did not find parent directory slash.");

	offset = (SG_uint32)(szSlash + 1 - sz);

	len = SG_string__length_in_bytes(pStrRepoPath) - offset;
	SG_ERR_CHECK_RETURN(  SG_string__remove(pCtx,pStrRepoPath,offset,len)  );
}
