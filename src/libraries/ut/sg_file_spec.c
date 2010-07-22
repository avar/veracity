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


SG_bool sg_file_spec__is_space(char c)
{
	if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
		return SG_TRUE;

	return SG_FALSE;
}

void sg_file_spec__normalize_pattern(SG_context* pCtx, SG_string** pstrPattern)
{
	//take out any single quotes
	SG_ERR_CHECK( SG_string__replace_bytes(pCtx,
									*pstrPattern,
									(const SG_byte *)"'",1,
									(const SG_byte *)"",0,
									SG_UINT32_MAX,SG_TRUE,
									NULL)  );


	//normalize all slashes to /
	SG_ERR_CHECK( SG_string__replace_bytes(pCtx,
									*pstrPattern,
									(const SG_byte *)"\\",1,
									(const SG_byte *)"/",1,
									SG_UINT32_MAX,SG_TRUE,
									NULL)  );


fail:

	return;
}



void SG_file_spec__read_patterns_from_file(SG_context* pCtx, const char* pszPathname, SG_stringarray** psaPatterns)
{
	SG_pathname* pPath;
	SG_file* pFile = NULL;
	SG_byte* pBuf = NULL;
	SG_uint64 len = 0;
	SG_uint32 got = 0;
	char* currLine = NULL; /* Contents of current line */
	SG_byte* p;

	SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pPath, pszPathname)  );

	SG_ERR_CHECK( SG_file__open__pathname(pCtx, pPath, SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile)  );
	SG_ERR_CHECK( SG_file__seek_end(pCtx, pFile, &len) );
	SG_ERR_CHECK( SG_file__seek(pCtx, pFile, 0) );

	SG_ERR_CHECK(  SG_alloc(pCtx, (SG_uint32)len + 1, 1, &pBuf)  );

	//curline size shouldn't be bigger than the file
	SG_ERR_CHECK(  SG_alloc(pCtx, (SG_uint32)len + 1, 1, &currLine)  );
	if (!*psaPatterns)
	{
		SG_ERR_CHECK( SG_STRINGARRAY__ALLOC(pCtx, &(*psaPatterns), (SG_uint32)len +1) );
	}

	SG_file__read(pCtx, pFile, (SG_uint32)len, pBuf, &got);
	if ( SG_context__err_equals(pCtx, SG_ERR_EOF) || 0 == got)
	{
		SG_context__err_reset(pCtx);
	}

	SG_ERR_CHECK_CURRENT;
	SG_FILE_NULLCLOSE(pCtx, pFile);

	p = pBuf;
	while (*p)
	 {
		SG_uint64 index = 0;
		SG_bool isNewline = 0;
		while (*p)
		{
		  /* ignore leading white space */
		  if (index == 0 && sg_file_spec__is_space(*p))
		  {
			  p++;
			  continue;
		  }

		  if (!isNewline)
		  {
			if (*p == '\r' || *p == '\n')
			  isNewline = 1;
		  }

		  /* Already found CR or LF */
		  else if (*p != '\r' && *p != '\n')
			  break;

		  currLine[index++] = *p++;
		}

		/*trim off any trailing whitespace*/
		while (index > 0 && sg_file_spec__is_space(currLine[index -1]))
		{
			currLine[--index] = '\0';
		}

		/* Terminate the string */
		currLine[index] = '\0';
		if (currLine && currLine[0] != '#' && currLine[0] != ';' && currLine[0] != '\0')
		{
			SG_stringarray__add(pCtx, *psaPatterns, currLine);
		}

	  }

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_NULLFREE(pCtx, currLine);
	SG_NULLFREE(pCtx, pBuf);
	if (pFile)
		SG_FILE_NULLCLOSE(pCtx, pFile);


}

void SG_file_spec__patterns_from_ignores_localsetting(SG_context* pCtx, SG_stringarray** psaPatterns)
{
    SG_varray* pva_ignores = NULL;
    SG_uint32 count_ignores = 0;
    SG_stringarray * pIgnorePatterns = NULL;
    SG_uint32 i;
    SG_string * pTemp = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(psaPatterns);

    // TODO we need repo here
    SG_ERR_CHECK(  SG_localsettings__get__varray(pCtx, SG_LOCALSETTING__IGNORES, NULL, &pva_ignores, NULL)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_ignores, &count_ignores)  );
    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &pIgnorePatterns, count_ignores)  );
    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pTemp)  );
    for(i=0;i<count_ignores;++i)
    {
        const char * sz = NULL;
        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_ignores, i, &sz)  );
        SG_ERR_CHECK(  SG_string__set__sz(pCtx, pTemp, "**/")  );
        SG_ERR_CHECK(  SG_string__append__sz(pCtx, pTemp, sz)  );
        SG_ERR_CHECK(  SG_stringarray__add(pCtx, pIgnorePatterns, SG_string__sz(pTemp))  );
    }
    SG_STRING_NULLFREE(pCtx, pTemp);
    SG_VARRAY_NULLFREE(pCtx, pva_ignores);

    *psaPatterns = pIgnorePatterns;

    return;
fail:
    SG_STRING_NULLFREE(pCtx, pTemp);
	SG_STRINGARRAY_NULLFREE(pCtx, pIgnorePatterns);
}

void _sg_file_spec__is_match(SG_context* pCtx, const char* const* ppszPatterns, SG_uint32 nCount, const char* pszRepoPath, SG_bool* bMatch)
{
	SG_string* pPattern = NULL;
	SG_bool b = SG_FALSE;
	SG_uint32 i;

	// TODO 2010/06/05 Move the string alloc outside this loop.
	//
	// TODO 2010/06/05 Think about an assert that we have a repo-path starting with "@/".
	//
	// TODO 2010/06/05 Error checking on __wildcmp call and have it return void

	for (i = 0; i < nCount; i++)
	{
		SG_ERR_CHECK( SG_string__alloc__sz(pCtx, &pPattern, ppszPatterns[i]) );

		SG_ERR_CHECK( sg_file_spec__normalize_pattern(pCtx, &pPattern)  );

		SG_ERR_CHECK( sg_file_spec__wildcmp(pCtx, SG_string__sz(pPattern), pszRepoPath, &b)  );

		SG_STRING_NULLFREE(pCtx, pPattern);

		if (b)
			break;
	}

	*bMatch = b;

fail:
	SG_STRING_NULLFREE(pCtx, pPattern);
}

// TODO 2010/06/05 Put a "class" around all of SG_file_spec and put the 3 pairs
// TODO            of (patterns,counts) in it and go ahead and normalize the
// TODO            patterns in the ctor.  See also sprawl-812.

/* right now this assumes repo pathnames */
void SG_file_spec__should_include(SG_context* pCtx,
								  const char* const* paszIncludePatterns, SG_uint32 count_includes,
								  const char* const* paszExcludePatterns, SG_uint32 count_excludes,
								  const char* const* paszIgnorePatterns,  SG_uint32 count_ignores,
								  const char* pszRepoPath,
								  SG_file_spec_eval * pEval)
{
	SG_bool bMatched = SG_FALSE;

	// TODO 2010/06/05 Think about an assert that we have a repo-path starting with "@/".

	// If we have a hard-match against any of the 3 types of patterns,
	// we know the answer.

	if (count_excludes > 0)
	{
        SG_ERR_CHECK_RETURN(  _sg_file_spec__is_match(pCtx, paszExcludePatterns, count_excludes, pszRepoPath, &bMatched)  );
		if (bMatched)
		{
			*pEval = SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED;
			return;
		}
	}

    if (count_includes > 0)
	{
        SG_ERR_CHECK_RETURN(  _sg_file_spec__is_match(pCtx, paszIncludePatterns, count_includes, pszRepoPath, &bMatched)  );
		if (bMatched)
		{
			*pEval = SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED;
			return;
		}
	}

    if (count_ignores > 0)
    {
        SG_ERR_CHECK_RETURN(  _sg_file_spec__is_match(pCtx, paszIgnorePatterns, count_ignores, pszRepoPath, &bMatched)  );
		if (bMatched)
		{
			*pEval = SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED;
			return;
		}
	}

	// We didn't match any of the patterns.  We have 2 types of "maybe":

	if (count_includes == 0)
	{
		// [1] Where there are no INCLUDES, we want to say yes to everything.

		*pEval = SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED;
		return;
	}
	else
	{
		// [2] When there were real INCLUDES (and we didn't match any of them)
		//     we want to say NO for files, but MAYBE for directories so that
		//     they can maybe keep diving if it makes sense.
		//
		//     For example, if they say "vv add --include='**.h' src", the caller
		//     will want to keep diving thru the directory tree looking for files
		//     with the .h suffix.  They will want to ADD all of the .h files and
		//     only those directories that contained some (and their parents).
		//     They do not want to ADD every directory in the tree, but rather
		//     back-fill them on the way back up.
		//
		//     Since we don't know the filetype (and we cannot stat() it here
		//     because it may not exist or may refer to a ptnode rather than
		//     something in the WD), we just return MAYBE.  The caller will have
		//     deal with it as they see fit.

		*pEval = SG_FILE_SPEC_EVAL__MAYBE;
		return;
	}
}

void sg_file_spec__wildcmp(SG_context* pCtx, const char* pszPattern, const char* pszTest, SG_bool * pbResult)
{
  const char* pszOne = NULL;
  const char* pszTwo = NULL;

  SG_NULLARGCHECK(pszPattern);
  SG_ARGCHECK(  ((pszTest) && (pszTest[0] == '@') && (pszTest[1] == '/')), pszTest  );  // we require a repo-path

  while ((*pszTest) && (*pszTest == '@' || *pszTest == '/'))
  {
	 pszTest++;
  }
  while ((*pszPattern) &&  (*pszPattern == '/' || *pszPattern == '@'))
  {
	 pszPattern++;
  }
  while ((*pszTest) && (*pszPattern != '*'))
  {
    if ((*pszPattern != *pszTest) && (*pszPattern != '?'))
	{
		*pbResult = SG_FALSE;
		return;
    }
    pszPattern++;
    pszTest++;
  }

  while (*pszTest)
  {
	if(*pszPattern == '/')
	{
		pszPattern++;
	}
	if (*pszTest == '/')
	{
		pszTest++;
	}
    if (*pszPattern == '*')
	{
		if (!*++pszPattern)
		{
			*pbResult = SG_TRUE;
			return;
		}
		pszTwo = pszPattern;
		pszOne = pszTest+1;
    }
	else if ((*pszPattern == *pszTest) || (*pszPattern == '?'))
	{
		pszPattern++;
		pszTest++;
    }
	else
	{
		pszPattern = pszTwo;
		pszTest = pszOne++;
    }
  }

  while (*pszPattern == '*')
  {
		pszPattern++;
  }

  *pbResult = (!*pszPattern);
  return;

fail:
  *pbResult = SG_FALSE;
}
