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

// sg_pathname.c
// a class for manipulating pathnames.
//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

struct _SG_pathname
{
	SG_bool					m_bIsSet;	// only true when a __set__ was successful

	SG_string *				m_pStringPathname;
};

//////////////////////////////////////////////////////////////////

void SG_pathname__alloc(SG_context* pCtx, SG_pathname** ppNew)
{
	SG_pathname * pThis;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pThis)  );

	pThis->m_bIsSet = SG_FALSE;
	pThis->m_pStringPathname = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pThis->m_pStringPathname)  );

	*ppNew = pThis;

	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pThis);
}

void SG_pathname__alloc__copy(SG_context* pCtx, SG_pathname** ppNew, const SG_pathname * pPathname)
{
	SG_pathname * pThis = NULL;

	SG_NULLARGCHECK_RETURN(ppNew);
	SG_NULLARGCHECK_RETURN(pPathname);
	SG_NULLARGCHECK_RETURN(pPathname->m_bIsSet);

	SG_ERR_CHECK(  SG_pathname__alloc(pCtx, &pThis)  );		// do not convert to SG_PATHNAME__ALLOC
	SG_ERR_CHECK(  SG_pathname__set__from_pathname(pCtx, pThis,pPathname)  );

	*ppNew = pThis;

	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pThis);
}

void SG_pathname__alloc__pathname_sz(SG_context* pCtx, SG_pathname **ppNew,
										   const SG_pathname * pPathnameDirectory,
										   const char * szFileName)
{
	// we are given a (normalized, absolute) pathname to a directory (or so we
	// assume) and (what we assume to be) an entry-name (a file or subdir name).
	// we create a new pathname with the 2 parts concatenated -- "<directory>/<filename>".

	SG_pathname * pNew = NULL;

	SG_NULLARGCHECK_RETURN(ppNew);
	SG_ARGCHECK_RETURN(SG_pathname__is_set(pPathnameDirectory), pPathnameDirectory);
	SG_NULL_PP_CHECK_RETURN(szFileName);

	SG_ERR_CHECK_RETURN(  SG_pathname__alloc__copy(pCtx, &pNew, pPathnameDirectory)  );		// do not convert to SG_PATHNAME__ALLOC__COPY

	// append the filename -- this can fail if they give us something that
	// cannot be in the middle of a pathname ("C:/", for example).

	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pNew,szFileName)  );

	*ppNew = pNew;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pNew);
}

void SG_pathname__alloc__sz(SG_context* pCtx, SG_pathname ** ppNew, const char * utf8Path)
{
	// we are given a relative or absolute pathname in utf8Path,
	// clean it up, put it in canonical form, make absolute and
	// create a new pathname containing it.

	SG_pathname * pNew = NULL;

	SG_NULLARGCHECK_RETURN(ppNew);
	SG_NULL_PP_CHECK_RETURN(utf8Path);

	SG_ERR_CHECK_RETURN(  SG_pathname__alloc(pCtx, &pNew)  );		// do not convert to SG_PATHNAME__ALLOC

	// set the filename -- this can fail if they give us something that
	// cannot be in the middle of a pathname ("C:/", for example).

	SG_ERR_CHECK(  SG_pathname__set__from_sz(pCtx,pNew,utf8Path)  );

	*ppNew = pNew;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pNew);
}

void SG_pathname__free(SG_context * pCtx, SG_pathname * pThis)
{
	if (!pThis)			// explicitly allow free(null)
		return;

	SG_STRING_NULLFREE(pCtx, pThis->m_pStringPathname);
	pThis->m_bIsSet = SG_FALSE;

	SG_NULLFREE(pCtx, pThis);
}

//////////////////////////////////////////////////////////////////
// this returns a pointer to our internal buffer.  use this as a
// *VERY* temporary pointer.  we may realloc anytime you do something
// to us.

const char * SG_pathname__sz(const SG_pathname * pThis)
{
	// we call this a 'const char *' and a 'c str', but
	// really it is a utf8 path.

	SG_ASSERT( pThis );
	SG_ASSERT( pThis->m_bIsSet );

	return SG_string__sz(pThis->m_pStringPathname);
}

SG_uint32 SG_pathname__length_in_bytes(const SG_pathname * pThis)
{
	SG_ASSERT( pThis );
	SG_ASSERT( pThis->m_bIsSet );

	return SG_string__length_in_bytes(pThis->m_pStringPathname);
}

//////////////////////////////////////////////////////////////////

void SG_pathname__clear(SG_context* pCtx, SG_pathname * pThis)
{
	// clear our pathname (we don't release storage, just
	// zero the current length).

	SG_NULLARGCHECK_RETURN(pThis);

	pThis->m_bIsSet = SG_FALSE;

	SG_ERR_CHECK_RETURN(  SG_string__clear(pCtx, pThis->m_pStringPathname)  );
}

//////////////////////////////////////////////////////////////////

#if defined(WINDOWS)
void _sg_pathname__normalize_backslashes(SG_context* pCtx, SG_string * pString)
{
	// make sure all slashes are forward ones.
	// this works on utf8 strings because '/' and '\\' are never
	// part of a multi-byte sequence.
	//
	// we do not do this on Linux/Mac because '\\' is a valid
	// pathname character.  (and we shouldn't get paths using it
	// anyway.)

	SG_ERR_CHECK_RETURN(  SG_string__replace_bytes(pCtx,
									pString,
									(const SG_byte *)"\\",1,
									(const SG_byte *)"/",1,
									SG_UINT32_MAX,SG_TRUE,
									NULL)  );

}
#endif

//////////////////////////////////////////////////////////////////

void SG_pathname__set__from_cwd(SG_context* pCtx, SG_pathname * pThis)
{
	// put cwd into our pathname.
	// we construct a utf8 path.
	// we normalize it.

	SG_NULLARGCHECK_RETURN(pThis);

	SG_ERR_CHECK_RETURN(  SG_pathname__clear(pCtx,pThis)  );
	SG_ERR_CHECK_RETURN(  SG_fsobj__getcwd(pCtx,pThis->m_pStringPathname)  );

	pThis->m_bIsSet = SG_TRUE;
}

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
void _sg_pathname__alloc__user_home(SG_context* pCtx, SG_pathname ** pPathname )
{
	SG_string * pString = NULL;
	// first look at HOME.

	char * szEnv;
	const char * szHome;
	struct passwd * pw;
	SG_byte byteTest;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );

	szEnv = getenv("HOME");
	if (szEnv && *szEnv)
	{
		SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, pString, szEnv)  );

	}
	else
	{
		// second look at USER and use that to get info from system PW DB.

		szEnv = getenv("USER");
		if (szEnv && *szEnv)
		{
			pw = getpwnam(szEnv);
			if (pw)
			{
				SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, pString, pw->pw_dir)  );
			}
		}
		else
		{
			// third, hit the system PW DB using our uid.

			pw = getpwuid(getuid());
			if (pw)
			{
				SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, pString, pw->pw_dir)  );
			}

		}
	}

	// validate value received from system.
	// this should be an absolute path.

	szHome = SG_string__sz(pString);
	if (!szHome || !*szHome)				// no value ??
		SG_ERR_THROW_RETURN(SG_ERR_INVALIDHOMEDIRECTORY);

	if (*szHome != '/')						// not absolute ??
		SG_ERR_THROW_RETURN(SG_ERR_INVALIDHOMEDIRECTORY);	// TODO should we try to fix??

	// force our result to have a final slash.  that is, we know that
	// the user's home directory is, in fact, a directory.

	SG_ERR_CHECK_RETURN(  SG_string__get_byte_r(pCtx,pString,0,&byteTest)  );
	if (byteTest != '/')
		SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx,pString,"/")  );

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, pPathname, szHome)  );
	SG_STRING_NULLFREE(pCtx, pString);
	return;
fail:
	SG_STRING_NULLFREE(pCtx, pString);
	return;
}
#if 0
//Jeremy commented out these two functions, because we shouldn't be expanding ~
void _sg_pathname__get_raw_user_home(SG_context* pCtx, SG_string * pString,
										 const char * szUser)
{
	// look up user's home directory (used for ~/foo and ~user/foo expansion).
	// we do not force a final '/' after the user's home directory.
	// we do not validate the result.
	// we construct a utf8 path.

	struct passwd * pw;

	// TODO szUser is in UTF-8 (it came from "~user" in the caller's string).
	// TODO do we need to convert it to an os-buffer/locale before we call
	// TODO getpwnam() ???
	//
	// TODO do we need to convert the pw->pw_dir to UTF-8 ??  (3 places)
	//
	// TODO do we need to convert the results of getenv("HOME") and getenv("USER")
	// TODO to UTF-8 ??
	//
	// NOTE I think the answer to all 3 questions is yes, but i was in the middle
	// NOTE of something and couldn't test it.  so i just marked it.
	//
	// NOTE See also SG_pathname__alloc__user_temp() which does a getenv("TMPDIR").

	if (szUser && *szUser)
	{
		// look up named user by hitting the system PW DB.

		pw = getpwnam(szUser);
		if (pw)
		{
			SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx,pString, pw->pw_dir)  );
			return;
		}

		SG_ERR_THROW_RETURN(SG_ERR_USERNOTFOUND);
	}
	else		// no user means the current user.
	{
		// first look at HOME.

		char * szEnv;

		szEnv = getenv("HOME");
		if (szEnv && *szEnv)
		{
			SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, pString, szEnv)  );
			return;
		}

		// second look at USER and use that to get info from system PW DB.

		szEnv = getenv("USER");
		if (szEnv && *szEnv)
		{
			pw = getpwnam(szEnv);
			if (pw)
			{
				SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, pString, pw->pw_dir)  );
				return;
			}
		}

		// third, hit the system PW DB using our uid.

		pw = getpwuid(getuid());
		if (pw)
		{
			SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, pString, pw->pw_dir)  );
			return;
		}

		// if nothing worked, return error.

		SG_ERR_THROW_RETURN(SG_ERR_USERNOTFOUND);
	}
}

void _sg_pathname__get_user_home(SG_context* pCtx, SG_string * pString,
									 const char * szUser)
{
	// look up user's home directory (used for ~/foo and ~user/foo expansion).
	// we *DO* force a final '/' after the user's home directory.
	// we *DO* validate the result.
	// we construct a utf8 path.

	const char * szHome;
	SG_byte byteTest;

	// get raw value from system (either ENV or PW DB).

	SG_ERR_CHECK_RETURN(  _sg_pathname__get_raw_user_home(pCtx,pString,szUser)  );

	// validate value received from system.
	// this should be an absolute path.

	szHome = SG_string__sz(pString);
	if (!szHome || !*szHome)				// no value ??
		SG_ERR_THROW_RETURN(SG_ERR_INVALIDHOMEDIRECTORY);

	if (*szHome != '/')						// not absolute ??
		SG_ERR_THROW_RETURN(SG_ERR_INVALIDHOMEDIRECTORY);	// TODO should we try to fix??

	// force our result to have a final slash.  that is, we know that
	// the user's home directory is, in fact, a directory.

	SG_ERR_CHECK_RETURN(  SG_string__get_byte_r(pCtx,pString,0,&byteTest)  );
	if (byteTest != '/')
		SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx,pString,"/")  );

	return;
}
#endif
void _sg_pathname__make_absolute(SG_context* pCtx, SG_string * pString,
									 const char * utf8Path)
{
	// make given utf8 path absolute and return resulting string.
	// we do not do any normalization.

	if (utf8Path && utf8Path[0] == '/')
	{
		// we have the start of an absolute path

		SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx,pString,utf8Path)  );
		return;
	}
#if 0
	//Jeremy commented this out, because we shouldn't be expanding ~
	//That's a job for the shell.
	else if (utf8Path && utf8Path[0] == '~')
	{
		// we have either "~/foo..." or "~user/foo..." or "~user" or "~"

		const char * pFirstSlash;

		// find first '/'.

		pFirstSlash = index(utf8Path,'/');
		if (!pFirstSlash)	// we have "~user" or "~"
		{
			SG_ERR_CHECK_RETURN(  _sg_pathname__get_user_home(pCtx,pString,&utf8Path[1])  );
			return;
		}

		if (pFirstSlash == &utf8Path[1])	// we have "~/..."
		{
			SG_ERR_CHECK_RETURN(  _sg_pathname__get_user_home(pCtx,pString,NULL)  );
		}
		else								// we have "~user/..."
		{
			SG_string * pstrUser = NULL;

			SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC__BUF_LEN(pCtx,
															&pstrUser,
															(SG_byte *)&utf8Path[1],
															pFirstSlash-utf8Path-1)  );

			_sg_pathname__get_user_home(pCtx,pString,SG_string__sz(pstrUser));
			SG_STRING_NULLFREE(pCtx, pstrUser);
			SG_ERR_CHECK_RETURN_CURRENT;
		}

		// now take care of the stuff remaining after "~" or "~user" -- the "/...".
		// since we forced a final slash on the home directory, we can skip the
		// leading slash on the rest of the path.

		SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx,pString,pFirstSlash+1)  );
		return;
	}
#endif
	else
	{
		// we have a relative pathname, prepend cwd (which already has a final slash).
		// for empty paths, we return just cwd.

		SG_ERR_CHECK_RETURN(  SG_fsobj__getcwd(pCtx, pString)  );

		if (utf8Path && *utf8Path)
			SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx,pString,utf8Path)  );

		return;
	}
}

void _sg_pathname__flatten_dot(SG_context* pCtx, SG_string * pString)
{
	// flatten "." directories from string.
	// pString must be an absolute path.
	// we assume a utf8 path.

	const char * sz;

	SG_NULLARGCHECK_RETURN(pString);
	sz = SG_string__sz(pString);
	SG_NULLARGCHECK_RETURN(sz);

	SG_ARGCHECK_RETURN( (*sz == '/'), *sz);					// absolute paths must start with a slash

	// replace "...abc/./xyz..." with "...abc/xyz..."
	// we set bAdvance to false so that "/././" collapses as expected.

	SG_ERR_CHECK_RETURN(  SG_string__replace_bytes(pCtx,
												   pString,
												   (const SG_byte *)"/./",3,
												   (const SG_byte *)"/",1,
												   SG_UINT32_MAX,SG_FALSE,
												   NULL)  );

	// check for final "/." (may be "...abc/." or just "/.")
	// and convert to just "/" (giving "...abc/" or just "/")

	if (SG_string__length_in_bytes(pString) > 1)
	{
		SG_byte byte0, byte1;

		SG_ERR_CHECK_RETURN(  SG_string__get_byte_r(pCtx,pString,0,&byte0)  );	// the last byte in the string
		SG_ERR_CHECK_RETURN(  SG_string__get_byte_r(pCtx,pString,1,&byte1)  );	// the byte before it

		if ((byte0 == '.') && (byte1 == '/'))
			SG_ERR_CHECK_RETURN(  SG_string__remove(pCtx,
													pString,
													SG_string__length_in_bytes(pString)-1,1)  );
	}
}

void _sg_pathname__flatten_dotdot(SG_context* pCtx, SG_string * pString)
{
	// flatten ".."'s
	// we assume a utf8 path.

	SG_uint32 ndx;
	const char * sz;

	SG_NULLARGCHECK_RETURN(pString);
	sz = SG_string__sz(pString);
	SG_NULLARGCHECK_RETURN(sz);

	SG_ARGCHECK_RETURN( (*sz == '/'), *sz);

	ndx = 0;
	while (1)
	{
		SG_uint32 len = SG_string__length_in_bytes(pString);
		if (ndx+3 > len)
			break;

		// search forward for "/../" (or "/.." at end of string).

		sz = SG_string__sz(pString);
		if ((sz[ndx]=='/') && (sz[ndx+1]=='.') && (sz[ndx+2]=='.')
			&& ((sz[ndx+3]=='/') || (sz[ndx+3]==0)))
		{
			// found one.  search backwards for parent directory.

			SG_uint32 lenToDelete;
			SG_uint32 ndxParent = ndx;
			while (1)
			{
				SG_ARGCHECK_RETURN( (ndxParent != 0), ndxParent);		// "/.." at beginning of path or too many ..'s

				ndxParent--;
				if (sz[ndxParent] == '/')		// found parent
					break;
			}

			// ndxParent is position of leading '/' of parent
			// ndx is position of leading '/' of ..

			SG_ARGCHECK_RETURN( (ndxParent+1 != ndx), ndxParent);		// someone gave us //.. or //../

			if (sz[ndx+3]=='/')			// we have slash after /..
				lenToDelete = ndx+4 - ndxParent;
			else						// no slash after /..
				lenToDelete = ndx+3 - ndxParent;

			// we leave the slash before the parent so we don't have to
			// worry about losing a slash if there is no final slash.
			// hence the +/- 1's.

			SG_ERR_IGNORE(  SG_string__remove(pCtx,pString,ndxParent+1,lenToDelete-1)  );

			// since we collapsed the string in place, we reset
			// ndx to the leading slash on the (now current) directory.

			ndx = ndxParent;
		}
		else
		{
			ndx++;
		}
	}
}

void _sg_pathname__normalize(SG_context* pCtx, SG_string * pString, const char * utf8Path)
{
	// normalize path.

	// convert relative path to absolute.  expand "~" and "~user" expressions.

	SG_ERR_CHECK_RETURN(  _sg_pathname__make_absolute(pCtx,pString,utf8Path)  );

	// TODO should we flatten '/abc//def' into '/abc/def' ??

	// squish .'s

	SG_ERR_CHECK_RETURN(  _sg_pathname__flatten_dot(pCtx,pString)  );

	// squish ..'s

	SG_ERR_CHECK_RETURN(  _sg_pathname__flatten_dotdot(pCtx,pString)  );

	// TODO anything else???

}
#endif

#if defined(LINUX)
void SG_pathname__alloc__user_appdata_directory(SG_context* pCtx, SG_pathname** ppResult)
{
	SG_ERR_CHECK_RETURN(  _sg_pathname__alloc__user_home(pCtx, ppResult)  );		// do not convert to SG_PATHNAME__ALLOC__SZ
}
#endif

#if defined(MAC)
void SG_pathname__alloc__user_appdata_directory(SG_context* pCtx, SG_pathname** ppResult)
{
	/* TODO where should this really go? */
	SG_ERR_CHECK_RETURN(  _sg_pathname__alloc__user_home(pCtx, ppResult)  );		// do not convert to SG_PATHNAME__ALLOC__SZ
}
#endif

#if defined(WINDOWS)
void SG_pathname__alloc__user_appdata_directory(SG_context* pCtx, SG_pathname** ppResult)
{
	wchar_t buf[1+MAX_PATH];
	BOOL ret;
	SG_string * pString = NULL;
	SG_pathname* pPath = NULL;

	ret = SHGetSpecialFolderPathW(NULL, buf, CSIDL_LOCAL_APPDATA, TRUE);
	if (ret)
	{
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );
		SG_ERR_CHECK(  SG_utf8__intern_from_os_buffer__wchar(pCtx,pString,buf)  );

		SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx,&pPath, SG_string__sz(pString))  );			// do not convert to SG_PATHNAME__ALLOC__SZ
		*ppResult = pPath;
	}
	else
	{
		SG_ERR_CHECK(  SG_ERR_GETLASTERROR(GetLastError())  );
	}

	SG_STRING_NULLFREE(pCtx, pString);
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pString);
}
#endif

#if defined(WINDOWS)
void _sg_pathname__strip_leading_unc_stuff(SG_context* pCtx, wchar_t * pBuf, SG_uint32 lenBuf, wchar_t ** pBufTemp)
{
	// if they gave us a "\\?\" prefixed string, we strip it off
	// in case it is not well-formed.  we then let the system do all
	// of the hard stuff.
	//
	// we also do this because we NEVER store the prefix in the SG_pathname;
	// we only use it when we are about to call a Win32 API routine.  In
	// theory, the user should NEVER EVER see the prefix.

#define X_IS_SLASH(c)		(((c) == L'/') || ((c) == L'\\'))
#define X_IS_LETTER(c,u,l)	(((c) == (u)) || ((c) == (l)))

	if ((lenBuf >= 3) && (pBuf[2] == '?'))
	{
		/*
		 * we may have
		 *    "\\?\C:"
		 *    "\\?\C:\"
		 *    "\\?\C:\a\b\c"
		 *    "\\?\C:\a\b\c\"
		 *    "\\?\UNC\host\share"
		 *    "\\?\UNC\host\share\"
		 *    "\\?\UNC\host\share\a\b\c"
		 *    "\\?\UNC\host\share\a\b\c\"
		 * and etc.
		 */

		if (   !X_IS_SLASH(pBuf[0])
			|| !X_IS_SLASH(pBuf[1])
			|| !X_IS_SLASH(pBuf[3]))
			SG_ERR_THROW_RETURN(  SG_ERR_INVALIDARG  );

		if ((lenBuf >= 6) && (pBuf[5] == L':'))
		{
			// start at the "C:"
			*pBufTemp = &pBuf[4];
		}
		else if ((lenBuf >= 8)
				 && X_IS_LETTER(pBuf[4],L'U',L'u')
				 && X_IS_LETTER(pBuf[5],L'N',L'n')
				 && X_IS_LETTER(pBuf[6],L'C',L'c')
				 && X_IS_SLASH(pBuf[7]))
		{
			// start at the "\\host\share..."
			// by overwriting the "C" with a "\" in the "...UNC\host...".

			pBuf[6] = pBuf[7];
			*pBufTemp = &pBuf[6];
		}
		else
		{
			SG_ERR_THROW(  SG_ERR_INVALIDARG  );
			//pBufTemp = NULL; /* Quiets MSVC2005 */
		}
	}
	else
	{
		// use entire input buffer as is
		*pBufTemp = pBuf;
	}
fail:
	return;
}
void _sg_pathname__normalize(SG_context* pCtx, SG_string * pString, const char * utf8Path)
{
	// normalize path.

	wchar_t * pBuf = NULL;
	wchar_t * pBufFull = NULL;
	wchar_t * pBufTemp;
	SG_uint32 lenBuf;
	DWORD dwLenNeeded, dwLenResult;

	// windows has a GetFullPathNameW() that does all the work.  it takes
	// care of the drive/volume, UNC paths, relative paths, .'s, and ..'s.
	// but we have to convert to/from wchar_t's/Unicode to use it.
	//
	// we DO NOT give GetFullPathNameW() "\\?\" prefixed pathnames because
	// the prefix turns off all of the Win32 level normalization and we
	// want that to happen.  Also, the "\\?\" prefix is only defined by
	// Windows for absolute pathnames and we don't necessarily have one.
	//
	// therefore we cannot call SG_pathname__to_unc_wchar(), we use the
	// basic SG_utf8__extern_to_os_buffer__wchar() to get wchar_t's.

	SG_ERR_CHECK(  SG_utf8__extern_to_os_buffer__wchar(pCtx,utf8Path,&pBuf,&lenBuf)  );	// we must free pBuf.

	SG_ERR_CHECK(  _sg_pathname__strip_leading_unc_stuff(pCtx, pBuf, lenBuf, &pBufTemp  ) );

	// let the system compute the full pathname using the
	// portion of the wchar_t buf that we want them to see.
	//
	// lenNeeded includes the terminating null.

	dwLenNeeded = GetFullPathNameW(pBufTemp,0,NULL,NULL);
	if (dwLenNeeded == 0)
	{
		SG_ERR_CHECK(  SG_ERR_GETLASTERROR(GetLastError())  );
	}

	SG_ERR_CHECK(  SG_allocN(pCtx,dwLenNeeded+1,pBufFull)  );

	// result does not include the terminating null.

	dwLenResult = GetFullPathNameW(pBufTemp,dwLenNeeded+1,pBufFull,NULL);
	SG_ASSERT( (dwLenResult > 0) && (dwLenResult < dwLenNeeded+1) );
	SG_ERR_CHECK(  _sg_pathname__strip_leading_unc_stuff(pCtx, pBufFull, dwLenResult, &pBufTemp  ) );

	// WARNING: GetFullPathNameW() apparently eats some bogus pathnames,
	// WARNING: like "C:/abc/def/../../../" and returns "C:/" as if nothing
	// WARNING: was wrong.
	//
	// TODO do we want to get in the middle of this?  I'm going to punt for now.

	// properly INTERN the full wchar_t pathname to UTF-8.

	SG_ERR_CHECK(  SG_utf8__intern_from_os_buffer__wchar(pCtx,pString,pBufTemp)  );

	// it normalizes the absolute path with backslashes.  convert
	// to slashes to be consistent with other platforms.  (either
	// form will work on windows.)

	SG_ERR_CHECK(  _sg_pathname__normalize_backslashes(pCtx,pString)  );

	// WARNING: GetFullPathNameW doesn't always put final '/' on directories.
	// WARNING: "C:/abc/." gives a different result from "C:/abc/./" for example.
	//
	// TODO we should consider normalizing trailing '/' on //host/share.
	//
	// TODO anything else???

	SG_NULLFREE(pCtx, pBuf);
	SG_NULLFREE(pCtx, pBufFull);

	return;

fail:
	SG_NULLFREE(pCtx, pBuf);
	SG_NULLFREE(pCtx, pBufFull);
}
#endif

//////////////////////////////////////////////////////////////////

void SG_pathname__set__from_sz(SG_context* pCtx, SG_pathname * pThis, const char * utf8Path)
{
	// given a relative or absolute pathname in utf8Path,
	// clean it up, put it in canonical form and make absolute.

	SG_string * pStringTemp = NULL;

	SG_NULLARGCHECK_RETURN(pThis);

	// copy the given utf8Path into a temporary string and let
	// the normalization code do it's magic.  if we were given
	// a bogus pathname, we give up WITHOUT modifying our real
	// pathname.

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx,&pStringTemp)  );
	SG_ERR_CHECK(  _sg_pathname__normalize(pCtx,pStringTemp,utf8Path)  );

	// pathname was OK and we were able to normalize it.
	// replace the string in our class.

	SG_STRING_NULLFREE(pCtx, pThis->m_pStringPathname);
	pThis->m_pStringPathname = pStringTemp;
	pThis->m_bIsSet = SG_TRUE;

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pStringTemp);
}

//////////////////////////////////////////////////////////////////

void SG_pathname__set__from_pathname(SG_context* pCtx, SG_pathname * pThis,
										 const SG_pathname * pPathname)
{
	// copy the value in the given pathname.


	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pPathname);
	SG_NULLARGCHECK_RETURN(pPathname->m_bIsSet);

	if (pThis == pPathname)				// set from self???
		return;

	SG_ERR_CHECK_RETURN(  SG_pathname__clear(pCtx,pThis)  );

	// we don't need to normalize because we assume that the given
	// pathname is already normalized.

	SG_ERR_CHECK_RETURN(  SG_string__set__string(pCtx,
												 pThis->m_pStringPathname,
												 pPathname->m_pStringPathname)  );

	pThis->m_bIsSet = SG_TRUE;
}

//////////////////////////////////////////////////////////////////
void SG_pathname__append__force_nonexistant(SG_context* pCtx, SG_pathname * pThis,
									  const char * utf8Path)
{
	SG_bool bExists = SG_FALSE;
	SG_pathname * pPathTemp = NULL;
	SG_string * pTestName = NULL;
	SG_uint32 nNumberOfTimesThrough = 1;
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathTemp)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pTestName)  );

	while (SG_TRUE)
	{
		SG_ERR_CHECK(  SG_pathname__set__from_pathname(pCtx, pPathTemp, pThis)  );
		SG_ERR_CHECK(  SG_string__sprintf(pCtx, pTestName, "%s_%d", utf8Path, nNumberOfTimesThrough)  );
		SG_ERR_CHECK(  SG_pathname__append__from_string(pCtx, pPathTemp, pTestName)  );
		SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathTemp, &bExists, NULL, NULL)  );
		if (bExists == SG_FALSE)
			break;
		nNumberOfTimesThrough++;
	}
	SG_ERR_CHECK(  SG_pathname__set__from_pathname(pCtx, pThis, pPathTemp)  );
fail:
	SG_STRING_NULLFREE(pCtx, pTestName);
	SG_PATHNAME_NULLFREE(pCtx, pPathTemp);
}

//////////////////////////////////////////////////////////////////

void SG_pathname__append__from_sz(SG_context* pCtx, SG_pathname * pThis,
									  const char * utf8Path)
{
	// append RELATIVE PATH onto end of our pathname and re-normalize.
	// (we need to do this in case they append "../../foo")
	//
	// we take care of adding a slash between the parts, if necessary.
	// (since file and directory pathnames look alike (and we cannot
	// rely on them existing on disk), we cannot assume anything about
	// them and just treat them as strings.)
	//
	// WE DO NOT MODIFY THE INPUT PATHNAME IF THE NORMALIZATION FAILS.

	SG_string * pstrTemp = NULL;
	SG_byte byteTest;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pThis->m_bIsSet);

	// if they don't have anything to append, just return.

	if (!utf8Path || !*utf8Path)
		return;

	// we don't clear m_bIsSet unless the actual __set__ at the bottom fails.

	// we don't worry about overlapping buffers because we construct a copy
	// in a temporary string (and append as necessary) and then replace our
	// value.

	// validate that they gave us a path that makes sense for appending
	// to our existing absolute path.

#if defined(MAC) || defined(LINUX)
//  We no longer need this check, as files can legally start with ~
//	SG_ARGCHECK_RETURN(utf8Path[0] != '~', utf8Path);			// they gave us a ~ or ~user

	SG_ARGCHECK_RETURN(utf8Path[0] != '/', utf8Path);			// they gave us a leading slash not a relative path.
#endif

#if defined(WINDOWS)
	SG_ARGCHECK_RETURN(utf8Path[1] != ':', utf8Path);			// they gave us a drive label not a relative path

	// on windows, a single leading slash is technically a relative path.
	// but it is relative to the CWD -- not necessarily the pathname we
	// were given.  so we reject this.
	//
	// also it could be the start of a double slash UNC pathname, but
	// this is also not a relative path.

	SG_ARGCHECK_RETURN(utf8Path[0] != '/', utf8Path);			// they gave us a leading slash not a relative path.
#endif

	// copy our absolute pathname into a temporary string.

	SG_ERR_CHECK(  SG_STRING__ALLOC__COPY(pCtx, &pstrTemp, pThis->m_pStringPathname)  );

	// append slash (if necessary) onto end of temporary string.

	SG_ERR_CHECK(  SG_string__get_byte_r(pCtx, pstrTemp,0,&byteTest)  );
	if (byteTest != '/')
		SG_ERR_CHECK(  SG_string__append__sz(pCtx,pstrTemp,"/")  );

	// append given relative path onto the end of our temporary string.

	SG_ERR_CHECK(  SG_string__append__sz(pCtx,pstrTemp,utf8Path)  );

	// let normal __set__ routine normalize and update our class.

	SG_ERR_CHECK(  SG_pathname__set__from_sz(pCtx,pThis,SG_string__sz(pstrTemp))  );

	SG_STRING_NULLFREE(pCtx, pstrTemp);
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pstrTemp);
}

void SG_pathname__append__from_string(SG_context* pCtx, SG_pathname * pThis, const SG_string * pString)
{
	SG_ERR_CHECK_RETURN(  SG_pathname__append__from_sz(pCtx, pThis, SG_string__sz(pString))  );
}

void SG_pathname__make_relative(
	SG_context* pCtx,
	const SG_pathname* pTop,
	const SG_pathname* pSub,
	SG_string** ppstrRelative
	)
{
	const char* pszTop = NULL;
	const char* pszSub = NULL;
	SG_uint32 iLenTop;
	SG_uint32 iLenSub;
	SG_bool bHasFinalSlash = SG_FALSE;
	SG_bool bProperPrefix;

	SG_NULLARGCHECK_RETURN(pTop);
	SG_NULLARGCHECK_RETURN(pTop->m_bIsSet);

	SG_NULLARGCHECK_RETURN(pSub);
	SG_NULLARGCHECK_RETURN(pSub->m_bIsSet);

	SG_ERR_CHECK_RETURN(  SG_pathname__has_final_slash(pCtx, pTop, &bHasFinalSlash, NULL)  );
	if (!bHasFinalSlash)
		SG_ERR_THROW2_RETURN(  SG_ERR_CANNOT_MAKE_RELATIVE_PATH,
							   (pCtx, "Parent path '%s' must have final slash.", SG_pathname__sz(pTop))  );

	pszTop = SG_pathname__sz(pTop);
	pszSub = SG_pathname__sz(pSub);

	iLenTop = (SG_uint32)strlen(pszTop);

	// pTop must be a proper prefix of pSub.
	// if pSub equals pTop (give or take the final slash on pSub), return null but don't throw.
	//
	// TODO 4/2/10 Revisit this and see if we need to make this case insensitive
	// TODO 4/2/10 on Windows and MAC.

	bProperPrefix = (0 == strncmp(pszTop, pszSub, iLenTop));
	if (bProperPrefix)
	{
		const char * pszRemainder = pszSub + iLenTop;
		if (*pszRemainder == 0)
			*ppstrRelative = NULL;
		else
			SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC__SZ(pCtx, ppstrRelative, pszRemainder)  );

		return;
	}

	// allow pSub to be exactly equal to pTop but without the final slash.

	iLenSub = (SG_uint32)strlen(pszSub);
	if (iLenSub == (iLenTop-1))
	{
		bProperPrefix = (0 == strncmp(pszTop, pszSub, iLenSub));
		if (bProperPrefix)
		{
			*ppstrRelative = NULL;
			return;
		}
	}

	SG_ERR_THROW2_RETURN(  SG_ERR_CANNOT_MAKE_RELATIVE_PATH,
						   (pCtx, "Parent path '%s' is not parent of '%s'.",
							SG_pathname__sz(pTop),
							SG_pathname__sz(pSub))  );
}

//////////////////////////////////////////////////////////////////

void SG_pathname__add_final_slash(SG_context* pCtx, SG_pathname * pThis)
{
	SG_bool bHasFinalSlash = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pThis->m_bIsSet);

	SG_ERR_CHECK_RETURN(  SG_pathname__has_final_slash(pCtx,pThis,&bHasFinalSlash,NULL)  );

	if (bHasFinalSlash)
		return;

	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx,pThis->m_pStringPathname,"/")  );

	// TODO i'm going to assume that we don't need to re-normalize the string.

}

void SG_pathname__has_final_slash(SG_context* pCtx, const SG_pathname * pThis, SG_bool * pbHasFinalSlash, SG_bool * pbCanRemoveFinalSlash)
{
	// return true (in *pbHasFinalSlash) if the pathname ends with a slash.
	// (this includes '/' or 'C:/', but is primarily intended for things
	// like '/abc/def/')
	//
	// return true (in *pbCanRemoveFinalSlash) if we have a final slash and
	// it can be removed (not a root directory, for example).

	SG_uint32 len;
	SG_bool bHasFinalSlash;
	SG_bool bCanRemoveFinalSlash;
	SG_byte byteTest;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pThis->m_bIsSet);

	// an empty string is never valid.  normalization should never create an empty string.

	len = SG_string__length_in_bytes(pThis->m_pStringPathname);
	SG_ARGCHECK_RETURN(len != 0, len);

	SG_ERR_CHECK_RETURN(  SG_string__get_byte_r(pCtx,pThis->m_pStringPathname,0,&byteTest)  );

	bHasFinalSlash = (byteTest == '/');
	bCanRemoveFinalSlash = SG_FALSE;

	if (bHasFinalSlash)
	{
		// last byte was a slash, but make sure that we're not a root dir.

#if defined(MAC) || defined(LINUX)
		if (len == 1)					// just '/'
			bCanRemoveFinalSlash = SG_FALSE;
		else
			bCanRemoveFinalSlash = SG_TRUE;
#endif
#if defined(WINDOWS)
		// we can have either:
		//    C:\					CANNOT remove final slash
		//    C:\a\b\c\				can remove
		//    \\host\share\			can remove
		//    \\host\share\a\b\c\	can remove
		//
		//    "\\" by itself is not a valid Windows pathname, the OS requires that
		//    we have at least <host> and <share>.
		//
		//    "\\?\" and "\\?\UNC\" prefixes are not valid because we never put them
		//    into the SG_pathname; we only prepend them as we convert to wchar_t's
		//    immediately prior to calling a Win32 API routine.

		SG_ASSERT( (len > 2) && "Invalid Windows Pathname" );

		SG_ERR_CHECK_RETURN(  SG_string__get_byte_l(pCtx,pThis->m_pStringPathname,1,&byteTest)  );
		if ((len == 3) && (byteTest == ':'))	// just "x:/"
			bCanRemoveFinalSlash = SG_FALSE;	// cannot strip trailing '\' on "C:\"
		else
			bCanRemoveFinalSlash = SG_TRUE;
#endif
	}

	if (pbHasFinalSlash)
		*pbHasFinalSlash = bHasFinalSlash;
	if (pbCanRemoveFinalSlash)
		*pbCanRemoveFinalSlash = bCanRemoveFinalSlash;
}

void SG_pathname__remove_final_slash(SG_context* pCtx, SG_pathname * pThis, SG_bool * pbHadFinalSlash)
{
	SG_uint32 len;
	SG_bool bHasFinalSlash = SG_TRUE, bCanRemoveFinalSlash = SG_TRUE;

	// if the pathname ends with a slash, remove it.  BUT we do not do so if we
	// are pointing to the root directory ('/' or 'C:/') and doing so would give
	// us something that's not an absolute or valid pathname.

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pThis->m_bIsSet);

	SG_ERR_CHECK_RETURN(  SG_pathname__has_final_slash(pCtx,pThis,&bHasFinalSlash,&bCanRemoveFinalSlash)  );

	if (bHasFinalSlash)
	{
		if (!bCanRemoveFinalSlash)
			SG_ERR_THROW_RETURN(SG_ERR_CANNOTTRIMROOTDIRECTORY);

		// we have a final slash and it is safe to remove it.

		len = SG_string__length_in_bytes(pThis->m_pStringPathname);
		SG_ERR_CHECK_RETURN(  SG_string__remove(pCtx,pThis->m_pStringPathname,len-1,1)  );
	}

	if (pbHadFinalSlash)
		*pbHadFinalSlash = bHasFinalSlash;
}

void SG_pathname__get_last(
	SG_context* pCtx,
	const SG_pathname * pThis,
	SG_string** ppResult
	)
{
	const char* pBegin;
	const char* pEnd;
	const char * sz;
	SG_bool bIsRoot = SG_TRUE;
	SG_string* pstr = NULL;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pThis->m_bIsSet);

	SG_ERR_CHECK_RETURN(  SG_pathname__is_root(pCtx,pThis,&bIsRoot)  );
	if (bIsRoot)
		SG_ERR_THROW_RETURN(SG_ERR_CANNOTTRIMROOTDIRECTORY);

	sz = SG_string__sz(pThis->m_pStringPathname);

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

void SG_pathname__remove_last(SG_context* pCtx, SG_pathname * pThis)
{
	// remove the last component (the filename or the last sub-dir).
	// if the incoming pathname has a final slash, remove it first
	// (so that we actually do some work).  we leave the trailing
	// slash that is before the part we removed.
	//
	// this should be equivalent to __append__from_sz("../")

	const char * sz;
	const char * szSlash;
	SG_uint32 offset;
	SG_uint32 len;
	SG_bool bIsRoot = SG_TRUE;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pThis->m_bIsSet);

	// if we have a root directory, we cannot remove a component.
	//
	// for "C:/" on windows and "/" on Linux/Mac this is fairly
	// obvious and trivial.
	//
	// for "//host/share/" or "//host/share", this is important
	// since "//host/" is not valid.

	SG_ERR_CHECK_RETURN(  SG_pathname__is_root(pCtx,pThis,&bIsRoot)  );
	if (bIsRoot)
		SG_ERR_THROW_RETURN(SG_ERR_CANNOTTRIMROOTDIRECTORY);

	// remove trailing slash, if present.

	SG_ERR_CHECK_RETURN(  SG_pathname__remove_final_slash(pCtx,pThis,NULL)  );

	// now look back to the previous slash.

	sz = SG_string__sz(pThis->m_pStringPathname);
	szSlash = strrchr(sz,'/');

	SG_ASSERT( szSlash && "Did not find parent directory slash.");

	offset = (SG_uint32)(szSlash + 1 - sz);
	len = SG_string__length_in_bytes(pThis->m_pStringPathname) - offset;

	SG_ERR_CHECK_RETURN(  SG_string__remove(pCtx,pThis->m_pStringPathname,offset,len)  );
}

void SG_pathname__filename(SG_context * pCtx, const SG_pathname * pThis, const char ** ppFilename)
{
    SG_uint32 i;
    SG_byte c;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pThis);
    SG_NULLARGCHECK_RETURN(ppFilename);
    SG_ARGCHECK_RETURN(SG_string__length_in_bytes(pThis->m_pStringPathname)>0, pThis);
    i = SG_string__length_in_bytes(pThis->m_pStringPathname)-1;
    SG_ERR_CHECK(SG_string__get_byte_l(pCtx, pThis->m_pStringPathname, i, &c)  );
    while(c!='/'){
        --i;
        SG_ERR_CHECK(SG_string__get_byte_l(pCtx, pThis->m_pStringPathname, i, &c)  );
    }
    *ppFilename = SG_string__sz(pThis->m_pStringPathname)+i+1;

    return;
fail:
    ;
}

//////////////////////////////////////////////////////////////////

SG_bool SG_pathname__is_set(const SG_pathname * pThis)
{
	// return true if a call to one of the __set__ routines succeeded.
	// that is, we have a valid, normalized, absolute pathname.
	// the idea is that if __set__fails and you ignore the error, then
	// subsequent operations on the pathname should also fail.

	SG_ASSERT( pThis );

	return (pThis && pThis->m_bIsSet);
}

//////////////////////////////////////////////////////////////////

#if defined(WINDOWS)
void SG_pathname__to_unc_wchar(SG_context* pCtx,
							   const SG_pathname * pThis,
								   wchar_t ** ppBufResult,
								   SG_uint32 * pLenResult)
{
	const char * szUtf8_Input;
	SG_string * pStringUNC = NULL;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pThis->m_bIsSet);

	SG_NULLARGCHECK_RETURN(ppBufResult);

	*ppBufResult = NULL;
	if (pLenResult)
		*pLenResult = 0;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStringUNC)  );

	// since we normalized the pathname and made it absolute
	// (and converted the separators to '/' for cross-platform
	// consistency) when we created pThis, we should have either:
	//
	//     C:/foo/bar
	//     //hostname/sharename/foo/bar
	//
	// we want to construct a \\?\ prefixed pathname to let
	// us get past the 260 character limits and use full ~32000
	// unicode character pathnames.
	//
	// in addition to giving us the higher limits and unicode,
	// the \\?\ prefix tells windows to do minimal processing
	// on the pathname.  this means they won't handle ".", "..",
	// or relative paths (but already took care of those).  Also,
	// they won't handle forward slashes.
	//
	// so we want to create:
	//
	//     \\?\C:\foo\bar
	//     \\?\UNC\hostname\sharename\foo\bar
	//
	// http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx
	//
	// according to the following link, using \\?\ also turns off
	// various Win32-level normalizations -- like stripping trailing
	// space and dot.
	// TODO What about filename case?
	// Should this be considered a security risk....
	//
	// http://blogs.msdn.com/bclteam/archive/2007/02/13/long-paths-in-net-part-1-of-3-kim-hamilton.aspx

	szUtf8_Input = SG_pathname__sz(pThis);
	if (szUtf8_Input[1] == ':')
	{
		// we have "C:/foo/bar"
		SG_ERR_CHECK(  SG_string__append__sz(pCtx,pStringUNC,"\\\\?\\")  );
		SG_ERR_CHECK(  SG_string__append__sz(pCtx,pStringUNC,szUtf8_Input)  );
	}
	else if (szUtf8_Input[0] == '/'  &&  szUtf8_Input[1] == '/')
	{
		// we have "//hostname/sharename/foo/bar"
		SG_ERR_CHECK(  SG_string__append__sz(pCtx,pStringUNC,"\\\\?\\UNC")  );
		SG_ERR_CHECK(  SG_string__append__sz(pCtx,pStringUNC,&szUtf8_Input[1])  );
	}
	else
	{
		SG_ERR_THROW(  SG_ERR_INVALIDARG  );
	}

	// replace forward slashes with backslashes.

	SG_ERR_CHECK(  SG_string__replace_bytes(pCtx,
											pStringUNC,
											(const SG_byte *)"/",1,
											(const SG_byte *)"\\",1,
											SG_UINT32_MAX,SG_TRUE,NULL)  );

	// convert UTF8 UNC pathname to wchar_t

	SG_ERR_CHECK(  SG_utf8__extern_to_os_buffer__wchar(pCtx,
													   SG_string__sz(pStringUNC),
													   ppBufResult,pLenResult)  );

	SG_STRING_NULLFREE(pCtx, pStringUNC);
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pStringUNC);
}
#endif//WINDOWS

//////////////////////////////////////////////////////////////////

void SG_pathname__is_root(SG_context* pCtx, const SG_pathname * pThis, SG_bool * pbIsRoot)
{
	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pThis->m_bIsSet);

	SG_NULLARGCHECK_RETURN(pbIsRoot);

#if defined(MAC) || defined(LINUX)
	{
		SG_uint32 len;
		const char * szPathname;

		// an empty string is never valid.  normalization should never create an empty string.

		szPathname = SG_pathname__sz(pThis);
		len = SG_string__length_in_bytes(pThis->m_pStringPathname);

		SG_ARGCHECK_RETURN((len > 0) || (szPathname[0] == '/'), szPathname);

		*pbIsRoot = (len == 1);
		return;
	}
#endif

#if defined(WINDOWS)
	{

		SG_pathname__is_drive_letter_pathname(pCtx,pThis,pbIsRoot);
		if (!SG_context__has_err(pCtx))		// if we have a drive-letter path,
			return;							//   return answer.
		else
			SG_context__err_reset(pCtx);

		SG_pathname__is_network_share_pathname(pCtx,pThis,pbIsRoot);
		if (!SG_context__has_err(pCtx))		// if we have a network share path,
			return;							//   return answer.
		else
			SG_context__err_reset(pCtx);

		*pbIsRoot = SG_FALSE;		// should not happen.
		SG_ERR_THROW_RETURN(SG_ERR_INVALIDARG);
	}
#endif
}

#if defined(WINDOWS)
void SG_pathname__is_drive_letter_pathname(SG_context* pCtx,
										   const SG_pathname * pThis,
											   SG_bool * pbIsRoot)
{
	const char * szPathname;
	SG_uint32 len;
	SG_bool bHasDriveLetter;
	SG_bool bHasColon;
	SG_bool bHasSlash;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pThis->m_bIsSet);

	if (pbIsRoot)
		*pbIsRoot = SG_FALSE;

	// return OK if we have something of the form:
	//    C:/
	//    C:/a/b/c
	//
	// return ERROR if we have something of the form:
	//    //host/share
	//    //host/share/a/b/c
	//
	// return ERROR for anything else, like:
	//    C:x
	//
	// when we have a drive-letter pathname, also indicate if it
	// is a root pathname "C:/".
	//
	// remember we have a UTF8 string, but since drive letters are
	// limited to [A-Z], we don't have to worry about it.

	// the shortest possible absolute Windows pathname is "C:/"
	// or "//x/y".

	len = SG_string__length_in_bytes(pThis->m_pStringPathname);
	SG_ARGCHECK_RETURN(len >= 3, len);

	szPathname = SG_pathname__sz(pThis);

	bHasDriveLetter = (   (szPathname[0] >= 'A') && (szPathname[0] <= 'Z')
					   || (szPathname[0] >= 'a') && (szPathname[0] <= 'z') );
	bHasColon = (szPathname[1] == ':');
	bHasSlash = (szPathname[2] == '/');

	SG_ARGCHECK_RETURN(bHasDriveLetter, bHasDriveLetter);
	SG_ARGCHECK_RETURN(bHasColon, bHasColon);
	SG_ARGCHECK_RETURN(bHasSlash, bHasSlash);

	if (pbIsRoot)
		*pbIsRoot = (len == 3);
}

void SG_pathname__is_network_share_pathname(SG_context* pCtx,
											const SG_pathname * pThis,
												SG_bool * pbIsRoot)
{
	SG_uint32 len;
	const char * szPathname;
	const char * szNextSlash;
	const char * szNextSlash2;
	SG_bool bMyIsRoot = SG_FALSE;
	SG_bool bHasDoubleSlash;

	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pThis->m_bIsSet);

	if (pbIsRoot)
		*pbIsRoot = SG_FALSE;

	// return OK if we have something of the form:
	//    //host/share
	//    //host/share/a/b/c
	//
	// return ERROR if we have something of the form:
	//    C:/
	//    C:/a/b/c
	//
	// return ERROR if we have a bogus pathname (should not happen).
    //
	// when we have a share pathname, also indicate if it is a
	// root pathname "//host/share" or "//host/share/".
	//
	// remember we have a UTF8 string, but since drive letters are
	// limited to [A-Z], we don't have to worry about it.

	// the shortest possible absolute Windows pathname is "C:/"
	// or "//x/y".

	len = SG_string__length_in_bytes(pThis->m_pStringPathname);
	SG_ARGCHECK_RETURN(len >= 5, len);

	szPathname = SG_pathname__sz(pThis);

	bHasDoubleSlash = ((szPathname[0] == '/') && (szPathname[1] == '/'));
	if (!bHasDoubleSlash)
		return;

	// find the next slash after the leading "//".
	// if we don't find one, we have a bogus pathname.

	szNextSlash = strchr(&szPathname[2],'/');
	SG_NULLARGCHECK_RETURN(szNextSlash);

	SG_ARGCHECK_RETURN(szNextSlash != &szPathname[2], szPathname);		// can't have "///" prefix

	// we now have seen "//x/..."
	// look for next slash.

	szNextSlash2 = strchr(szNextSlash+1,'/');
	if (szNextSlash2)
	{
		SG_ARGCHECK_RETURN(szNextSlash2 != szNextSlash+1, szNextSlash2);	// "//x//" is invalid

		if (szNextSlash2 != &szPathname[len-1])
			bMyIsRoot = SG_FALSE;	// we now have seen "//x/y/...", so not a root.
		else
			bMyIsRoot = SG_TRUE;	// we have exactly "//host/share/" -- is root
	}
	else
	{
		bMyIsRoot = SG_TRUE;		// we have exactly "//host/share" -- is root
	}

	if (pbIsRoot)
		*pbIsRoot = bMyIsRoot;
}
#endif//WINDOWS

#if defined(WINDOWS)
void SG_pathname__alloc__user_temp_directory(SG_context* pCtx, SG_pathname** ppResult)
{
	size_t requiredSize;
	wchar_t* pwszTempPath = NULL;
	errno_t errno;
	SG_string* pString = NULL;
	SG_pathname* pPath = NULL;

	_wgetenv_s(&requiredSize, NULL, 0, L"TEMP");
	SG_ERR_CHECK_RETURN(  SG_alloc(pCtx, 1, requiredSize * sizeof(wchar_t), &pwszTempPath)  );

	errno = _wgetenv_s(&requiredSize, pwszTempPath, requiredSize, L"TEMP");
	if (errno)
		SG_ERR_CHECK(  SG_ERR_GETLASTERROR(GetLastError())  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx,&pString)  );
	SG_ERR_CHECK(  SG_utf8__intern_from_os_buffer__wchar(pCtx, pString, pwszTempPath)  );

	SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pPath, SG_string__sz(pString))  );		// do not convert to SG_PATHNAME__ALLOC__SZ

	SG_STRING_NULLFREE(pCtx, pString);
	SG_NULLFREE(pCtx, pwszTempPath);

	*ppResult = pPath;

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pString);
	SG_NULLFREE(pCtx, pwszTempPath);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}
#endif

#if defined(MAC) || defined(LINUX)
void SG_pathname__alloc__user_temp_directory(SG_context* pCtx, SG_pathname** ppResult)
{
	SG_pathname* pPath = NULL;
	char * szEnv;
	SG_string * pStringUtf8 = NULL;

	szEnv = getenv("TMPDIR");
	if (!szEnv || !*szEnv)
		szEnv = "/tmp";

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx,&pStringUtf8)  );
	SG_ERR_CHECK(  SG_utf8__intern_from_os_buffer__sz(pCtx,pStringUtf8,szEnv)  );

	SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx,&pPath, SG_string__sz(pStringUtf8))  );		// do not convert to SG_PATHNAME__ALLOC__SZ

	SG_STRING_NULLFREE(pCtx, pStringUtf8);

	*ppResult = pPath;

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pStringUtf8);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}
#endif

//////////////////////////////////////////////////////////////////

void SG_pathname__contains_whitespace(SG_context* pCtx, const SG_pathname * pThis, SG_bool * pbFound)
{
	SG_NULLARGCHECK_RETURN(pThis);
	SG_NULLARGCHECK_RETURN(pbFound);
	SG_NULLARGCHECK_RETURN(pThis->m_bIsSet);

	SG_ERR_CHECK_RETURN(  SG_string__contains_whitespace(pCtx, pThis->m_pStringPathname, pbFound)  );
}
