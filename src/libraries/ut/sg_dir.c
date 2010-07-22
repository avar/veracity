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

// sg_dir.c
// routines to read a directory.
//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

struct _SG_dir
{
	SG_pathname *		m_pPathnameDirectory;

#if defined(MAC) || defined(LINUX)
	DIR *				m_dir;
#endif
#if defined(WINDOWS)
	HANDLE				m_hDir;
#endif
};

#if defined(MAC) || defined(LINUX)
#define MY_IS_CLOSED(pThis)		((pThis)->m_dir == NULL)
#define MY_SET_CLOSED(pThis)	SG_STATEMENT( (pThis)->m_dir = NULL; )
#endif
#if defined(WINDOWS)
#define MY_IS_CLOSED(pThis)		((pThis)->m_hDir == INVALID_HANDLE_VALUE)
#define MY_SET_CLOSED(pThis)	SG_STATEMENT( (pThis)->m_hDir = INVALID_HANDLE_VALUE; )
#endif

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
void _sg_dir__open_posix(SG_context* pCtx, SG_dir * pThis,
							 SG_error *pErrReadStat,
							 SG_string * pStringNameFirstFileRead,
							 SG_fsobj_stat * pStatFirstFileRead)
{
	char * pBufOS = NULL;

	SG_ERR_CHECK(  SG_utf8__extern_to_os_buffer__sz(pCtx, SG_pathname__sz(pThis->m_pPathnameDirectory), &pBufOS)  );

	pThis->m_dir = opendir(pBufOS);
	if (MY_IS_CLOSED(pThis))
		SG_ERR_THROW2(  SG_ERR_ERRNO(errno),
						(pCtx,"Calling opendir() on '%s'",SG_pathname__sz(pThis->m_pPathnameDirectory))  );

	SG_NULLFREE(pCtx, pBufOS);

	// from this point forward, we return SG_ERR_OK as the result of the
	// opendir() and return a valid SG_dir pointer (which the caller must
	// __close() later.
	//
	// we return status for the read/stat in *pErrReadStat.  this allows
	// readdir/stat to fail for an individual entry and not affect the
	// caller's scan of the directory.
	//
	// force first call to readdir() to be consistent with windows.
	// SG_dir__read() takes care of locale issues on returned entryname
	// automatically.

	SG_dir__read(pCtx, pThis,pStringNameFirstFileRead,pStatFirstFileRead);
	SG_context__get_err(pCtx, pErrReadStat);
	SG_context__err_reset(pCtx);	// the pCtx returned has only __open results, not __read.

	return;

fail:
	SG_NULLFREE(pCtx, pBufOS);
}
#endif

#if defined(WINDOWS)
void _sg_dir__open_windows(SG_context* pCtx, SG_dir * pThis,
							   SG_error *pErrReadStat,
							   SG_string * pStringNameFirstFileRead,
							   SG_fsobj_stat * pStatFirstFileRead)
{
	WIN32_FIND_DATAW data;
	SG_pathname * pPathnameWild = NULL;
	wchar_t * pBufWild = NULL;

	// we need to create "<directory>/*" so that Find{First,Next}File() will
	// enumerate them properly.  then convert this to wchar_t's.

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathnameWild,pThis->m_pPathnameDirectory,"*")  );
	SG_ERR_CHECK(  SG_pathname__to_unc_wchar(pCtx, pPathnameWild,&pBufWild,NULL)  );

	// initialize an enumeration.  this automatically gives us data for
	// the first file/subdir.  (this routine is what CRT stat() routine
	// calls. so we save a little work.)

	pThis->m_hDir = FindFirstFileW(pBufWild,&data);
	if (MY_IS_CLOSED(pThis))
		SG_ERR_THROW2(  SG_ERR_GETLASTERROR(GetLastError()),
						(pCtx,"Calling FindFirstFileW() on '%s'",SG_pathname__sz(pPathnameWild))  );

	// from this point forward, we return SG_ERR_OK as the result of the
	// opendir() and return a valid SG_dir pointer (which the caller must
	// __close() later.
	//
	// we return status for the read/stat in *pErrReadStat.  this allows
	// readdir/stat to fail for an individual entry and not affect the
	// caller's scan of the directory.  (granted this is kind of bogus in
	// this case (since windows gave us the data for the first entry when
	// we set up the enumeration), but does allow us to handle parsing
	// errors when decoding the WIN32_FIND_DATAW.)

	// properly INTERN data.cFileName into string.

	SG_utf8__intern_from_os_buffer__wchar(pCtx, pStringNameFirstFileRead,data.cFileName);
	SG_context__get_err(pCtx, pErrReadStat);
	SG_context__err_reset(pCtx);

	if (SG_IS_OK(*pErrReadStat)  &&  pStatFirstFileRead)
	{
		SG_fsobj__find_data_to_stat(pCtx, &data, pStatFirstFileRead);
		SG_context__get_err(pCtx, pErrReadStat);
		SG_context__err_reset(pCtx);
	}

	SG_NULLFREE(pCtx, pBufWild);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameWild);

	return;

fail:
	SG_NULLFREE(pCtx, pBufWild);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameWild);
}
#endif

void SG_dir__open(SG_context* pCtx, const SG_pathname * pPathnameDirectory,
					  SG_dir ** ppDirResult,
					  SG_error * pErrReadStat,
					  SG_string * pStringNameFirstFileRead,
					  SG_fsobj_stat * pStatFirstFileRead /*optional*/)
{
	// open the named directory and prepare for reading entries.
	//
	// also, go ahead and do the first __read() (and optionally stat() it).
	//
	// Linux/Mac have opendir().  opendir() takes the name of the
	// actual directory to open and read.  it does not fetch the first
	// file/subdir.
	//
	// Windows has FindFirstFile()/FindNextFile().  this is somewhat
	// different.  FirstFirstFile() starts a search and returns info for
	// the first file/subdir.  it fails if you give it a pathname that
	// ends with a slash.  if you call it with a directory name, you
	// essesentially get a stat() on that directory.  so we need to
	// append '/*' to the directory to get it to enumerate all the
	// files/subdirs within it.

	SG_dir * pThis = NULL;

	SG_ARGCHECK_RETURN(SG_pathname__is_set(pPathnameDirectory), pPathnameDirectory);
	SG_NULLARGCHECK_RETURN(ppDirResult);
	SG_NULLARGCHECK_RETURN(pErrReadStat);
	SG_NULLARGCHECK_RETURN(pStringNameFirstFileRead);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pThis)  );

	MY_SET_CLOSED(pThis);
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pThis->m_pPathnameDirectory,pPathnameDirectory)  );

#if defined(MAC) || defined(LINUX)
	SG_ERR_CHECK(  _sg_dir__open_posix(pCtx, pThis,pErrReadStat,pStringNameFirstFileRead,pStatFirstFileRead)  );
#endif
#if defined(WINDOWS)
	SG_ERR_CHECK(  _sg_dir__open_windows(pCtx, pThis,pErrReadStat,pStringNameFirstFileRead,pStatFirstFileRead)  );
#endif

	*ppDirResult = pThis;

	return;

fail:
	SG_DIR_NULLCLOSE(pCtx, pThis);
}

//////////////////////////////////////////////////////////////////

void SG_dir__close(SG_context * pCtx, SG_dir * pThis)
{
	if (!pThis)
		return;

	if (!MY_IS_CLOSED(pThis))
	{
#if defined(MAC) || defined(LINUX)
		(void)closedir(pThis->m_dir);
#endif
#if defined(WINDOWS)
		(void)FindClose(pThis->m_hDir);
#endif
		MY_SET_CLOSED(pThis);
	}

	SG_PATHNAME_NULLFREE(pCtx, pThis->m_pPathnameDirectory);

	SG_NULLFREE(pCtx, pThis);
}

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
void _sg_dir__read_posix(SG_context* pCtx, SG_dir * pThis,
							 SG_string * pStringFileName,
							 SG_fsobj_stat * pStatResult /*optional*/)
{
	struct dirent * pDirent;

	// readdir() returns NULL on error or EOF.  it only sets errno on error.
	//
	// it is unclear from the manpages if we need to zero errno before
	// calling readdir(), but i had to when running some tests on Linux.

	errno = 0;
	pDirent = readdir(pThis->m_dir);
	if (!pDirent)
	{
		if (errno)
			SG_ERR_THROW_RETURN( SG_ERR_ERRNO(errno) );
		else
			SG_ERR_THROW_RETURN( SG_ERR_NOMOREFILES );
	}

	// properly INTERN the entryname that readdir() gave us.  this takes
	// care of locale charenc/utf-8 and/or nfd/nfc issues.

	SG_ERR_CHECK_RETURN(  SG_utf8__intern_from_os_buffer__sz(pCtx, pStringFileName,pDirent->d_name)  );

	// TODO create a version of SG_fsobj__stat__pathname_sz() that takes the already-interned
	// TODO version of the entryname.

	if (pStatResult)
		SG_ERR_CHECK_RETURN( SG_fsobj__stat__pathname_sz(pCtx, pThis->m_pPathnameDirectory,pDirent->d_name,pStatResult) );

}
#endif
#if defined(WINDOWS)
void _sg_dir__read_windows(SG_context* pCtx, SG_dir * pThis,
							   SG_string * pStringFileName,
							   SG_fsobj_stat * pStatResult /*optional*/)
{
	WIN32_FIND_DATAW data;

	if (!FindNextFileW(pThis->m_hDir,&data))
	{
		if (GetLastError() == ERROR_NO_MORE_FILES)	// map this one to be consistent with
			SG_ERR_THROW_RETURN(  SG_ERR_NOMOREFILES );				// Linux/Mac.
		else
			SG_ERR_THROW_RETURN( SG_ERR_GETLASTERROR(GetLastError()) );
	}

	// properly INTERN data.cFileName into string.

	SG_ERR_CHECK_RETURN(  SG_utf8__intern_from_os_buffer__wchar(pCtx, pStringFileName,data.cFileName)  );

	if (pStatResult)
		SG_ERR_CHECK_RETURN( SG_fsobj__find_data_to_stat(pCtx, &data,pStatResult) );
}
#endif

void SG_dir__read(SG_context* pCtx, SG_dir * pThis,
					  SG_string * pStringFileName,
					  SG_fsobj_stat * pStatResult)
{
	// read the next file/subdir from this directory.
	// we read in directory order -- this may or may
	// not be sorted.
	//
	// optionally also stat the file.  (this may seem
	// weird, but we get the information for free on
	// windows.)
	SG_NULLARGCHECK_RETURN(pThis);
	SG_ARGCHECK_RETURN(!MY_IS_CLOSED(pThis), pThis);
	SG_NULLARGCHECK_RETURN(pStringFileName);


#if defined(MAC) || defined(LINUX)
	SG_ERR_CHECK_RETURN( _sg_dir__read_posix(pCtx, pThis,pStringFileName,pStatResult) );
#endif
#if defined(WINDOWS)
	SG_ERR_CHECK_RETURN( _sg_dir__read_windows(pCtx, pThis,pStringFileName,pStatResult) );
#endif
}

//////////////////////////////////////////////////////////////////

void SG_dir__foreach(SG_context * pCtx,
					 const SG_pathname * pPathDir,
					 SG_bool bWantStat,
					 SG_dir_foreach_callback pfnCB,
					 void * pVoidData)
{
	SG_string * pStringEntryName = NULL;
	SG_dir * pDir = NULL;
	SG_error errReadStatus;
	SG_fsobj_stat fsStat;
	SG_fsobj_stat * pfsStat = ((bWantStat) ? &fsStat : NULL);

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStringEntryName)  );

	SG_ERR_CHECK(  SG_dir__open(pCtx, pPathDir, &pDir, &errReadStatus, pStringEntryName, pfsStat)  );

	if (!SG_IS_OK(errReadStatus))
		SG_ERR_THROW(  errReadStatus  );

	do
	{
		SG_ERR_CHECK(  (*pfnCB)(pCtx, pStringEntryName, pfsStat, pVoidData)  );

		SG_dir__read(pCtx, pDir, pStringEntryName, pfsStat);

	} while (!SG_context__has_err(pCtx));

	if (!SG_context__err_equals(pCtx, SG_ERR_NOMOREFILES))
		SG_ERR_RETHROW;

	SG_context__err_reset(pCtx);

fail:
	SG_DIR_NULLCLOSE(pCtx, pDir);
	SG_STRING_NULLFREE(pCtx, pStringEntryName);
}

void _sg_dir__filter(
    SG_context* pCtx,
    const SG_pathname * pPathDir,
    const char* psz_match_begin,
    const char* psz_match_anywhere,
    const char* psz_match_end,
    SG_rbtree** pprb,
    SG_uint32* piCount
    )
{
    SG_rbtree* prb = NULL;
	SG_string * pStringEntryName = NULL;
	SG_dir * pDir = NULL;
	SG_error errReadStatus;
    SG_uint32 count = 0;

    if (pprb)
    {
        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );
    }

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStringEntryName)  );

	SG_ERR_CHECK(  SG_dir__open(pCtx, pPathDir, &pDir, &errReadStatus, pStringEntryName, NULL)  );

	if (!SG_IS_OK(errReadStatus))
    {
		SG_ERR_THROW(  errReadStatus  );
    }

	do
	{
        const char* psz_name = SG_string__sz(pStringEntryName);
        SG_uint32 len_name = (SG_uint32) strlen(psz_name);

        if (psz_match_begin)
        {
            if (0 != strncmp(psz_name, psz_match_begin, strlen(psz_match_begin)))
            {
                goto next;
            }
        }

        if (psz_match_end)
        {
            SG_uint32 len_match = (SG_uint32) strlen(psz_match_end);
            SG_uint32 pos = 0;

            if (len_match > len_name)
            {
                goto next;
            }

            pos = len_name - len_match;

            if (0 != strcmp(psz_name + pos, psz_match_end))
            {
                goto next;
            }
        }

        if (psz_match_anywhere)
        {
            if (!strstr(psz_name, psz_match_anywhere))
            {
                goto next;
            }
        }

        if (prb)
        {
            SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, SG_string__sz(pStringEntryName))  );
        }
        count++;

next:
		SG_dir__read(pCtx, pDir, pStringEntryName, NULL);

	} while (!SG_context__has_err(pCtx));

	if (!SG_context__err_equals(pCtx, SG_ERR_NOMOREFILES))
    {
		SG_ERR_RETHROW;
    }

	SG_context__err_reset(pCtx);

    if (pprb)
    {
        *pprb = prb;
        prb = NULL;
    }

    if (piCount)
    {
        *piCount = count;
    }

fail:
	SG_DIR_NULLCLOSE(pCtx, pDir);
	SG_STRING_NULLFREE(pCtx, pStringEntryName);
    SG_RBTREE_NULLFREE(pCtx, prb);
    return;
}

void SG_dir__list(
    SG_context* pCtx,
    const SG_pathname * pPathDir,
    const char* psz_match_begin,
    const char* psz_match_anywhere,
    const char* psz_match_end,
    SG_rbtree** pprb
    )
{
    SG_ERR_CHECK_RETURN(  _sg_dir__filter(pCtx, pPathDir, psz_match_begin, psz_match_anywhere, psz_match_end, pprb, NULL)  );
}

void SG_dir__count(
    SG_context* pCtx,
    const SG_pathname * pPathDir,
    const char* psz_match_begin,
    const char* psz_match_anywhere,
    const char* psz_match_end,
    SG_uint32* piCount
    )
{
    SG_ERR_CHECK_RETURN(  _sg_dir__filter(pCtx, pPathDir, psz_match_begin, psz_match_anywhere, psz_match_end, NULL, piCount)  );
}


