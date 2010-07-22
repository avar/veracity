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

// sg_file.c
// implementation of file stuff -- raw/unbuffered file io on a handle or fd.
//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

struct _SG_file
{
#if defined(MAC) || defined(LINUX)
	int				m_fd;
#endif
#if defined(WINDOWS)
	HANDLE			m_hFile;
#endif
};

#if defined(MAC) || defined(LINUX)
#define MY_IS_CLOSED(pThis)		((pThis)->m_fd == -1)
#define MY_SET_CLOSED(pThis)	SG_STATEMENT( (pThis)->m_fd = -1; )
#endif
#if defined(WINDOWS)
#define MY_IS_CLOSED(pThis)		((pThis)->m_hFile == INVALID_HANDLE_VALUE)
#define MY_SET_CLOSED(pThis)	SG_STATEMENT( (pThis)->m_hFile = INVALID_HANDLE_VALUE; )
#endif

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
/**
 * map flags_sg and perms_sg into generic posix-platform values.
 *
 */
void _sg_compute_posix_open_args(
	SG_context* pCtx,
	SG_file_flags flags_sg,
	SG_fsobj_perms perms_sg,
	int * pOFlag,
	mode_t * pMode
	)
{
	int oflag;
	mode_t mode;

	oflag = 0;
	if ((flags_sg & SG_FILE__ACCESS_MASK) == SG_FILE_RDONLY)
	{
		SG_ARGCHECK_RETURN( !(flags_sg & SG_FILE_TRUNC) , flags_sg );

		oflag |= O_RDONLY;
	}
	else if ((flags_sg & SG_FILE__ACCESS_MASK) == SG_FILE_WRONLY)
	{
		oflag |= O_WRONLY;
	}
	else if ((flags_sg & SG_FILE__ACCESS_MASK) == SG_FILE_RDWR)
	{
		oflag |= O_RDWR;
	}
	else
	{
		SG_ARGCHECK_RETURN( SG_FALSE , flags_sg );
	}

	if ((flags_sg & SG_FILE__OPEN_MASK) == SG_FILE_OPEN_EXISTING)
	{
		if (flags_sg & SG_FILE_TRUNC)
			oflag |= O_TRUNC;
	}
	else if ((flags_sg & SG_FILE__OPEN_MASK) == SG_FILE_CREATE_NEW)
	{
		SG_ARGCHECK_RETURN( !(flags_sg & SG_FILE_TRUNC) , flags_sg );

		oflag |= (O_CREAT|O_EXCL);
	}
	else if ((flags_sg & SG_FILE__OPEN_MASK) == SG_FILE_OPEN_OR_CREATE)
	{
		if (flags_sg & SG_FILE_TRUNC)
			oflag |= O_TRUNC;

		oflag |= O_CREAT;
	}
	else
	{
		SG_ARGCHECK_RETURN( SG_FALSE , flags_sg );
	}

	// TODO Linux offers a O_LARGEFILE flag.
	// TODO is it needed if we defined the
	// TODO LFS args on the compile line?

	// we defined perms to match Linux/Mac mode_t mode bits.

	mode = (mode_t)(perms_sg & SG_FSOBJ_PERMS__MASK);

	*pOFlag = oflag;
	*pMode = mode;
}

/**
 * my wrapper for open(2) for posix-based systems.
 *
 */
void _sg_file_posix_open(SG_context* pCtx, const SG_pathname * pPathname, SG_file_flags flags_sg, SG_fsobj_perms perms_sg, SG_file** ppResult)
{
	char * pBufOS = NULL;
	SG_file * pf = NULL;
	int oflag = 0;
	mode_t mode = 0;

	// convert generic flags/perms into something platform-specific.

	SG_ERR_CHECK(  _sg_compute_posix_open_args(pCtx, flags_sg, perms_sg, &oflag, &mode)  );

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pf)  );

	MY_SET_CLOSED(pf);

	SG_ERR_CHECK(  SG_utf8__extern_to_os_buffer__sz(pCtx, SG_pathname__sz(pPathname), &pBufOS)  );

	pf->m_fd = open(pBufOS,oflag,mode);
	if (MY_IS_CLOSED(pf))
		SG_ERR_THROW(SG_ERR_ERRNO(errno));

	SG_NULLFREE(pCtx, pBufOS);

	if (flags_sg & SG_FILE_LOCK)
	{
		/* TODO for now, I'm using flock here.
		 * fcntl might be better.
		 * there are tradeoffs both ways.
		 * flock doesn't work over NFS, but
		 * fcntl has really goofy semantics
		 * around the closing of a file. */

		int reslock;
		int locktype;

		locktype = (flags_sg & SG_FILE_RDONLY) ? LOCK_SH : LOCK_EX;
		locktype |= LOCK_NB;

		reslock = flock(pf->m_fd, locktype);
		if (0 != reslock)
		{
			/* TODO if the file was created, delete it.
			 * unfortunately, there seems to be a case
			 * where we don't know if we created the file
			 * or not:  SG_FILE_OPEN_OR_CREATE
			 *
			 * Alternatively, we may just want to be careless
			 * and let them have the file without the lock
			 * since locks are advisory only... (sigh)
			 */

			/* TODO should we get the err from errno instead?
			 * SG_ERR_THROW(SG_ERR_ERRNO(errno));
			 */
			SG_ERR_THROW(SG_ERR_FILE_LOCK_FAILED);
		}
	}

	*ppResult = pf;

	return;
fail:
	SG_NULLFREE(pCtx, pBufOS);
	if (pf && !MY_IS_CLOSED(pf))
		(void)close(pf->m_fd);
	SG_NULLFREE(pCtx, pf);
}
#endif



#if defined(WINDOWS)
/**
 * map flags and perms into Windows-specific values for CreateFileW.
 *
 */
void _sg_compute_createfile_args(
	SG_context* pCtx,
	SG_file_flags flags_sg,
	SG_fsobj_perms perms_sg,
	DWORD * pdwDesiredAccess,
	DWORD * pdwShareMode,
	DWORD * pdwCreationDisposition,
	DWORD * pdwFlagsAndAttributes
	)
{
	DWORD dwDesiredAccess;
	DWORD dwShareMode;
	DWORD dwCreationDisposition;
	DWORD dwFlagsAndAttributes;
	SG_bool bLock;

	// Windows FILE_SHARE_{READ,WRITE} seem to be the exact opposite
	// of a LOCK.  It seems like the file will be locked by default
	// ***unless*** you allow sharing.

	bLock = ((flags_sg & SG_FILE_LOCK) == SG_FILE_LOCK);

	if ((flags_sg & SG_FILE__ACCESS_MASK) == SG_FILE_RDONLY)
	{
		SG_ARGCHECK_RETURN( !(flags_sg & SG_FILE_TRUNC) , flags_sg );

		dwDesiredAccess = GENERIC_READ;
		dwShareMode = ((bLock) ? (FILE_SHARE_READ) : (FILE_SHARE_READ | FILE_SHARE_WRITE));
	}
	else if ((flags_sg & SG_FILE__ACCESS_MASK) == SG_FILE_WRONLY)
	{
		dwDesiredAccess = GENERIC_WRITE;
		dwShareMode = ((bLock) ? 0 : (FILE_SHARE_READ | FILE_SHARE_WRITE));
	}
	else if ((flags_sg & SG_FILE__ACCESS_MASK) == SG_FILE_RDWR)
	{
		dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
		dwShareMode = ((bLock) ? 0 : (FILE_SHARE_READ | FILE_SHARE_WRITE));
	}
	else
	{
		SG_ARGCHECK_RETURN( SG_FALSE , flags_sg );
	}

	dwCreationDisposition = 0;
	if ((flags_sg & SG_FILE__OPEN_MASK) == SG_FILE_OPEN_EXISTING)
	{
		if (flags_sg & SG_FILE_TRUNC)
			dwCreationDisposition = TRUNCATE_EXISTING;
		else
			dwCreationDisposition = OPEN_EXISTING;
	}
	else if ((flags_sg & SG_FILE__OPEN_MASK) == SG_FILE_CREATE_NEW)
	{
		SG_ARGCHECK_RETURN( !(flags_sg & SG_FILE_TRUNC) , flags_sg );

		dwCreationDisposition = CREATE_NEW;
	}
	else if ((flags_sg & SG_FILE__OPEN_MASK) == SG_FILE_OPEN_OR_CREATE)
	{
		if (flags_sg & SG_FILE_TRUNC)
			dwCreationDisposition = CREATE_ALWAYS;
		else
			dwCreationDisposition = OPEN_ALWAYS;
	}
	else
	{
		SG_ARGCHECK_RETURN( SG_FALSE , flags_sg );
	}

	if (SG_FSOBJ_PERMS__WINDOWS_IS_READONLY(perms_sg))
		dwFlagsAndAttributes = FILE_ATTRIBUTE_READONLY;
	else
		dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;

	*pdwDesiredAccess = dwDesiredAccess;
	*pdwShareMode = dwShareMode;
	*pdwCreationDisposition = dwCreationDisposition;
	*pdwFlagsAndAttributes = dwFlagsAndAttributes;
}

/**
 * my wrapper for CreateFileW()
 *
 */
void _sg_file_createfilew(SG_context* pCtx, const SG_pathname * pPathname, SG_file_flags flags_sg, SG_fsobj_perms perms_sg, SG_file** ppResult)
{
	SG_file * pf = NULL;
	wchar_t * pBuf = NULL;
	DWORD dwDesiredAccess;
	DWORD dwShareMode;
	DWORD dwCreationDisposition;
	DWORD dwFlagsAndAttributes;

	// convert generic flags/perms into something windows-specific.

	SG_ERR_CHECK(  _sg_compute_createfile_args(pCtx, flags_sg,perms_sg,
													 &dwDesiredAccess,&dwShareMode,
													 &dwCreationDisposition,&dwFlagsAndAttributes)  );

	SG_ERR_CHECK(  SG_alloc1(pCtx, pf)  );

	MY_SET_CLOSED(pf);

	// convert UTF-8 pathname into wchar_t so that we can use the W version of the windows API.

	SG_ERR_CHECK(  SG_pathname__to_unc_wchar(pCtx, pPathname,&pBuf,NULL)  );

	pf->m_hFile = CreateFileW(pBuf,
							  dwDesiredAccess,dwShareMode,
							  NULL, // lpSecurityAtributes
							  dwCreationDisposition,dwFlagsAndAttributes,
							  NULL);
	if (MY_IS_CLOSED(pf))
		SG_ERR_THROW(SG_ERR_GETLASTERROR(GetLastError()));

	SG_NULLFREE(pCtx, pBuf);

	*ppResult = pf;

	return;
fail:
	SG_NULLFREE(pCtx, pBuf);
	if (pf && !MY_IS_CLOSED(pf))
		(void)CloseHandle(pf->m_hFile);
	SG_NULLFREE(pCtx, pf);
}
#endif

//////////////////////////////////////////////////////////////////

void SG_file__open__pathname(SG_context* pCtx, const SG_pathname * pPathname, SG_file_flags flags_sg, SG_fsobj_perms perms_sg, SG_file** ppResult)
{
	SG_ARGCHECK_RETURN( SG_pathname__is_set(pPathname) , pPathname );
	SG_NULLARGCHECK_RETURN(ppResult);

#if defined(MAC) || defined(LINUX)
	SG_ERR_CHECK_RETURN(  _sg_file_posix_open(pCtx, pPathname,flags_sg,perms_sg,ppResult)  );
#endif
#if defined(WINDOWS)
	SG_ERR_CHECK_RETURN(  _sg_file_createfilew(pCtx, pPathname,flags_sg,perms_sg,ppResult)  );
#endif
}

//////////////////////////////////////////////////////////////////

void SG_file__truncate(SG_context* pCtx, SG_file* pFile)
{
	// seek absolute from beginning

	SG_NULLARGCHECK_RETURN(pFile);
	SG_ARGCHECK_RETURN( !MY_IS_CLOSED(pFile) , pFile );

#if defined(MAC) || defined(LINUX)
	{
		off_t pos;
		int res;

		// verify that we have LFS support.
		// off_t lseek(..., off_t offset, ...)
		// note that off_t is signed.

		SG_ASSERT(sizeof(off_t) >= sizeof(SG_uint64));

		pos = lseek(pFile->m_fd, (off_t)0, SEEK_CUR);
		if (pos == (off_t)-1)
			SG_ERR_THROW_RETURN(SG_ERR_ERRNO(errno));


		SG_ASSERT(sizeof(off_t) >= sizeof(SG_uint64));

		res = ftruncate(pFile->m_fd, pos);
		if (res < 0)
		{
			SG_ERR_THROW_RETURN(SG_ERR_ERRNO(errno));
		}
	}
#endif

#if defined(WINDOWS)
	{
		if (!SetEndOfFile(pFile->m_hFile))
			SG_ERR_THROW_RETURN(SG_ERR_GETLASTERROR(GetLastError()));
	}
#endif
}

void SG_file__seek(SG_context* pCtx, SG_file* pFile, SG_uint64 iPos)
{
	// seek absolute from beginning

	SG_NULLARGCHECK_RETURN(pFile);
	SG_ARGCHECK_RETURN( !MY_IS_CLOSED(pFile) , pFile );

#if defined(MAC) || defined(LINUX)
	{
		off_t res;

		// verify that we have LFS support.
		// off_t lseek(..., off_t offset, ...)
		// note that off_t is signed.

		SG_ASSERT(sizeof(off_t) >= sizeof(SG_uint64));
		SG_ASSERT(iPos <= 0x7fffffffffffffffULL);

		res = lseek(pFile->m_fd, (off_t)iPos, SEEK_SET);
		if (res == (off_t)-1)
			SG_ERR_THROW_RETURN(SG_ERR_ERRNO(errno));
	}
#endif
#if defined(WINDOWS)
	{
		LARGE_INTEGER li;

		// LARGE_INTEGER is a union/struct...
		// it contains a (signed) LONGLONG called QuadPart.
		SG_ASSERT(sizeof(LARGE_INTEGER) >= sizeof(SG_int64));
		SG_ASSERT(iPos <= 0x7fffffffffffffffULL);

		li.QuadPart = iPos;

		if (!SetFilePointerEx(pFile->m_hFile,li,NULL,FILE_BEGIN))
			SG_ERR_THROW_RETURN(SG_ERR_GETLASTERROR(GetLastError()));
	}
#endif
}

void SG_file__seek_end(SG_context* pCtx, SG_file* pFile, SG_uint64 * piPos)
{
	// seek to end of file

	SG_NULLARGCHECK_RETURN(pFile);
	SG_ARGCHECK_RETURN( !MY_IS_CLOSED(pFile) , pFile );

#if defined(MAC) || defined(LINUX)
	{
		off_t res;

		// verify that we have LFS support.
		// off_t lseek(..., off_t offset, ...)
		// note that off_t is signed.

		SG_ASSERT(sizeof(off_t) >= sizeof(SG_uint64));

		res = lseek(pFile->m_fd, (off_t)0, SEEK_END);
		if (res == (off_t)-1)
			SG_ERR_THROW_RETURN(SG_ERR_ERRNO(errno));

		if (piPos)
			*piPos = (SG_uint64)res;
	}
#endif
#if defined(WINDOWS)
	{
		LARGE_INTEGER zero;
		LARGE_INTEGER res;

		zero.QuadPart = 0;

		// LARGE_INTEGER is a union/struct...
		// it contains a (signed) LONGLONG called QuadPart.
		SG_ASSERT(sizeof(LARGE_INTEGER) >= sizeof(SG_int64));

		if (!SetFilePointerEx(pFile->m_hFile,zero,&res,FILE_END))
			SG_ERR_THROW_RETURN(SG_ERR_GETLASTERROR(GetLastError()));

		if (piPos)
			*piPos = (SG_uint64)res.QuadPart;
	}
#endif
}

void SG_file__tell(SG_context* pCtx, SG_file* pFile, SG_uint64 * piPos)
{
	// get current file position

	SG_NULLARGCHECK_RETURN(pFile);
	SG_ARGCHECK_RETURN( !MY_IS_CLOSED(pFile) , pFile );

#if defined(MAC) || defined(LINUX)
	{
		off_t res;

		// verify that we have LFS support.
		// off_t lseek(..., off_t offset, ...)
		// note that off_t is signed.

		SG_ASSERT(sizeof(off_t) >= sizeof(SG_uint64));

		res = lseek(pFile->m_fd, (off_t)0, SEEK_CUR);
		if (res == (off_t)-1)
			SG_ERR_THROW_RETURN(SG_ERR_ERRNO(errno));

		if (piPos)
			*piPos = (SG_uint64)res;
	}
#endif
#if defined(WINDOWS)
	{
		LARGE_INTEGER zero;
		LARGE_INTEGER res;

		zero.QuadPart = 0;

		// LARGE_INTEGER is a union/struct...
		// it contains a (signed) LONGLONG called QuadPart.
		SG_ASSERT(sizeof(LARGE_INTEGER) >= sizeof(SG_int64));

		if (!SetFilePointerEx(pFile->m_hFile,zero,&res,FILE_CURRENT))
			SG_ERR_THROW_RETURN(SG_ERR_GETLASTERROR(GetLastError()));

		if (piPos)
			*piPos = (SG_uint64)res.QuadPart;
	}
#endif
}

//////////////////////////////////////////////////////////////////

void SG_file__read(SG_context* pCtx, SG_file* pFile, SG_uint32 iNumBytesWanted, SG_byte* pBytes, SG_uint32* piNumBytesRetrieved)
{
	SG_uint32 iNumBytesRetrieved;

	SG_NULLARGCHECK_RETURN(pFile);
	SG_ARGCHECK_RETURN( !MY_IS_CLOSED(pFile) , pFile );

	// ssize_t read(..., size_t count);
	// on 64bit systems ssize_t and size_t may be 64bit.
	// on 32bit systems with LFS support, they are still 32bit.
	//
	// read() takes an unsigned arg.  windows ReadFile()
	// takes an unsigned arg.  so we have an unsigned arg.
	// read() returns signed result, so it can't do full
	// 32bit value, but ReadFile() can.
	//
	// so, to be consistent between platforms, we clamp buffer
	// length to max signed int.

	SG_ASSERT(sizeof(size_t) >= sizeof(SG_uint32));
	SG_ASSERT(iNumBytesWanted <= 0x7fffffff);

	SG_ASSERT(pBytes);

#if defined(MAC) || defined(LINUX)
	{
		ssize_t res;

		res = read(pFile->m_fd, pBytes, (size_t)iNumBytesWanted);
		if (res < 0)
			SG_ERR_THROW_RETURN(SG_ERR_ERRNO(errno));
		if (res == 0)
			SG_ERR_THROW_RETURN(SG_ERR_EOF);

		iNumBytesRetrieved = (SG_uint32)res;
	}
#endif
#if defined(WINDOWS)
	{
		DWORD nbr;

		if (!ReadFile(pFile->m_hFile, pBytes, (DWORD)iNumBytesWanted, &nbr, NULL))
			SG_ERR_THROW_RETURN(SG_ERR_GETLASTERROR(GetLastError()));
		if (nbr == 0)
			SG_ERR_THROW_RETURN(SG_ERR_EOF);

		iNumBytesRetrieved = (SG_uint32)nbr;
	}
#endif

	// if they gave us a place to return the number of bytes read,
	// then return it and no error.  (even for incomplete reads).

	if (piNumBytesRetrieved)
	{
		*piNumBytesRetrieved = iNumBytesRetrieved;
		return;
	}

	// when we don't have a place to return the number of bytes read.
	// we return OK only for a complete read.  we return an error for
	// an incomplete read. (there shouldn't be any complaints, because
	// we gave them a chance to get the paritial read info.)

	if (iNumBytesRetrieved != iNumBytesWanted)
		SG_ERR_THROW_RETURN(SG_ERR_INCOMPLETEREAD);
}

void SG_file__eof(SG_context* pCtx, SG_file* pFile, SG_bool* pEof)
{
    SG_byte b;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pFile);
    SG_ARGCHECK_RETURN( !MY_IS_CLOSED(pFile) , pFile );
    SG_NULLARGCHECK_RETURN(pEof);

    SG_file__read(pCtx, pFile, 1, &b, NULL);
    if(SG_context__err_equals(pCtx, SG_ERR_EOF))
    {
        SG_context__err_reset(pCtx);
        *pEof = SG_TRUE;
    }
    else
    {
        SG_uint64 pos;
        SG_ERR_CHECK_CURRENT;
        SG_ERR_CHECK(  SG_file__tell(pCtx, pFile, &pos)  );
        SG_ASSERT(pos>0);
        SG_ERR_CHECK(  SG_file__seek(pCtx, pFile, pos-1)  );
        *pEof = SG_FALSE;
    }

    return;
fail:
    ;
}

void SG_file__write(SG_context* pCtx, SG_file* pFile, SG_uint32 iNumBytes, const SG_byte* pBytes, SG_uint32 * piNumBytesWritten)
{
	SG_uint32 nbw;

	SG_NULLARGCHECK_RETURN(pFile);
	SG_ARGCHECK_RETURN( !MY_IS_CLOSED(pFile) , pFile );

	// see notes in SG_file__read() about 64 vs 32 bits.

	SG_ASSERT(sizeof(size_t) >= sizeof(SG_uint32));
	SG_ASSERT(iNumBytes <= 0x7fffffff);

	SG_ASSERT(pBytes);

#if defined(MAC) || defined(LINUX)
	{
		ssize_t res;

		res = write(pFile->m_fd, pBytes, (size_t)iNumBytes);
		if (res < 0)
			SG_ERR_THROW_RETURN(SG_ERR_ERRNO(errno));

		nbw = (SG_uint32)res;
	}
#endif
#if defined(WINDOWS)
	{
		if (!WriteFile(pFile->m_hFile, pBytes, (DWORD)iNumBytes, (LPDWORD)&nbw, NULL))
			SG_ERR_THROW_RETURN(SG_ERR_GETLASTERROR(GetLastError()));
	}
#endif

	// if they gave us a place to return the number of bytes written,
	// then return it and no error.  (even for incomplete writes).

	if (piNumBytesWritten)
	{
		*piNumBytesWritten = (SG_uint32)nbw;
		return;
	}

	// when we don't have a place to return the number of bytes written.
	// we return OK only for a complete write.  we return an error for
	// an incomplete write. (there shouldn't be any complaints, because
	// we gave them a chance to get the paritial write info.)

	if ((SG_uint32)nbw != iNumBytes)
		SG_ERR_THROW_RETURN(SG_ERR_INCOMPLETEWRITE);
}

void SG_file__write__sz(SG_context* pCtx, SG_file * pThis, const char * sz )
{
	SG_file__write(pCtx, pThis, strlen(sz), (const SG_byte*)sz, NULL);
}
void SG_file__write__format(SG_context* pCtx, SG_file* pFile, const char * szFormat, ...)
{
    va_list ap;
    va_start(ap,szFormat);
    SG_file__write__vformat(pCtx, pFile, szFormat, ap);
    va_end(ap);
}
void SG_file__write__vformat(SG_context* pCtx, SG_file* pFile, const char * szFormat, va_list ap)
{
    SG_string * pStr = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pFile);
    SG_NULLARGCHECK_RETURN(szFormat);

    SG_ERR_CHECK(  SG_STRING__ALLOC__VFORMAT(pCtx, &pStr, szFormat, ap)  );
    SG_ERR_CHECK(  SG_file__write__string(pCtx, pFile, pStr)  );
    SG_STRING_NULLFREE(pCtx, pStr);

    return;
fail:
    SG_STRING_NULLFREE(pCtx, pStr);
}
void SG_file__write__string(SG_context* pCtx, SG_file * pThis, const SG_string * pString )
{
	SG_file__write(pCtx, pThis, SG_string__length_in_bytes(pString), (const SG_byte*)SG_string__sz(pString), NULL);
}

void SG_file__close(SG_context* pCtx, SG_file** ppFile)
{
	if(ppFile==NULL || *ppFile==NULL)
		return;

	// TODO should we do the equivalent of fsync() this before closing???

	if (!MY_IS_CLOSED((*ppFile)))
	{
#if defined(MAC) || defined(LINUX)
		if (close((*ppFile)->m_fd) == -1)
			SG_ERR_THROW(SG_ERR_ERRNO(errno));
#endif
#if defined(WINDOWS)
		if (!CloseHandle((*ppFile)->m_hFile))
			SG_ERR_THROW(SG_ERR_GETLASTERROR(GetLastError()));
#endif

		MY_SET_CLOSED((*ppFile));
	}

	// we only free pFile if we successfully closed the file.

	SG_free(pCtx, (*ppFile));

	*ppFile	= NULL;

	return;
fail:
	// WARNING: we ***DO NOT*** free pFile if close() fails.
	// WARNING: close() can fail in weird cases, see "Man close(2)"
	// WARNING: on Linux.
	// WARNING:
	// WARNING: I'm not sure what the caller can do about it, but
	// WARNING: if we free their pointer and file handle, they
	// WARNING: can't do anything...

	return;
}

//////////////////////////////////////////////////////////////////

void SG_file__fsync(SG_context* pCtx, SG_file * pFile)
{
	// try to call fsync on the underlying file.

	SG_NULLARGCHECK_RETURN(pFile);
	SG_ARGCHECK_RETURN( !MY_IS_CLOSED(pFile) , pFile );

#if defined(MAC) || defined(LINUX)
	{
		if (fsync(pFile->m_fd) == -1)
			SG_ERR_THROW_RETURN(SG_ERR_ERRNO(errno));
		return;
	}
#endif
#if defined(WINDOWS)
	{
		// NOTE: Windows also has a FILE_FLAG_NO_BUFFERING flag that can be used in CreateFile().

		if (FlushFileBuffers(pFile->m_hFile) == 0)
			SG_ERR_THROW_RETURN(SG_ERR_GETLASTERROR(GetLastError()));
		return;
	}
#endif
}

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
void SG_file__get_fd(SG_context* pCtx, SG_file * pFile, int * pfd)
{
	SG_NULLARGCHECK_RETURN(pFile);
	SG_NULLARGCHECK_RETURN(pfd);
	*pfd = pFile->m_fd;
}
#endif

#if defined(WINDOWS)
void SG_file__get_handle(SG_context* pCtx, SG_file * pFile, HANDLE * phFile)
{
	SG_NULLARGCHECK_RETURN(pFile);
	SG_NULLARGCHECK_RETURN(phFile);
	*phFile = pFile->m_hFile;
}
#endif
