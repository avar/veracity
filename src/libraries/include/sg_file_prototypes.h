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

// sg_file.h
// Declarations for the file i/o portability layer.
// This module implements unbuffered IO.
//////////////////////////////////////////////////////////////////

#ifndef H_SG_FILE_PROTOTYPES_H
#define H_SG_FILE_PROTOTYPES_H

BEGIN_EXTERN_C

// NOTE: we do not support O_APPEND because (on Linux/Mac)
// NOTE: it causes a seek to the end before *EACH* write.
// NOTE: windows does not support auto-seeking files (when
// NOTE: we're using the low-level routines).  this may not
// NOTE: matter since we'll probably not be writing to files
// NOTE: shared by others, but still we shouldn't give the
// NOTE: illusion that we support this feature.  the caller
// NOTE: should call SG_file_seek_end() after opening the file.

//////////////////////////////////////////////////////////////////

// in SG_file__open(), perms are only used when we actually create a new file.  (see open(2)).

void SG_file__open__pathname(SG_context*, const SG_pathname * pPathname, SG_file_flags flags, SG_fsobj_perms perms, SG_file** pResult);

void SG_file__seek(SG_context*, SG_file* pFile, SG_uint64 iPos);  // seek absolute from beginning
void SG_file__seek_end(SG_context*, SG_file* pFile, SG_uint64 * piPos);  // seek to end, return position

void SG_file__truncate(SG_context*, SG_file* pFile);

void SG_file__tell(SG_context*, SG_file* pFile, SG_uint64* piPos);

void SG_file__read(SG_context*, SG_file* pFile, SG_uint32 iNumBytesWanted, SG_byte* pBytes, SG_uint32* piNumBytesRetrieved /*optional*/);
void SG_file__eof(SG_context*, SG_file* pFile, SG_bool * pEof);
void SG_file__write(SG_context*, SG_file* pFile, SG_uint32 iNumBytes, const SG_byte* pBytes, SG_uint32 * piNumBytesWritten /*optional*/);
void SG_file__write__format(SG_context*, SG_file* pFile, const char * szFormat, ...);
void SG_file__write__vformat(SG_context*, SG_file* pFile, const char * szFormat, va_list);
void SG_file__write__sz(SG_context*, SG_file* pFile, const char *);
void SG_file__write__string(SG_context*, SG_file * pFile, const SG_string *);

/**
 * Close the OS file associated with the SG_file handle and free
 * the SG_file handle.
 *
 * THIS MAY RETURN AN ERROR CODE IF THE OS CLOSE FAILS.  THIS IS
 * UNLIKELY, BUT IT DOES HAPPEN.  See "man close(2)" for details.
 *
 * Frees the file handle and nulls the caller's copy.
 */
void SG_file__close(SG_context*, SG_file** ppFile);

void SG_file__fsync(SG_context*, SG_file* pFile);

//////////////////////////////////////////////////////////////////

/**
 * Close the given file and null the pointer.
 *
 * This ignores any error codes from the close
 * WHICH MAY INTRODUCE A LEAK/ERROR.
 */
#define SG_FILE_NULLCLOSE(_pCtx, _pFile) SG_STATEMENT(\
	if(_pFile!=NULL)\
	{\
		SG_context__push_level(_pCtx);\
		SG_file__close(_pCtx, &_pFile);\
		SG_context__pop_level(_pCtx);\
		_pFile = NULL;\
	})

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
/**
 * Platform-dependent routine to get the FD inside the SG_file.
 *
 * Don't use this unless you ***REALLY*** need to.  This was
 * added to allow for stupid FD dup/dup2 tricks in the child
 * process in a fork/exec.
 *
 * You DO NOT own the FD file handle, so don't close() it directly
 * or try to do any fcntl() operations on it.
 */
void SG_file__get_fd(SG_context*, SG_file * pFile, int * pfd);
#endif

#if defined(WINDOWS)
/**
 * Platform-dependent routine to get the Windows File Handle
 * inside the SG_file.
 *
 * Don't use this unless you ***REALLY*** need to.  This was
 * added to allow for stupid DuplicateHandle/Inherit-Handle
 * tricks when calling CreateProcess().
 *
 * You DO NOT own the File Handle returned.
 */
void SG_file__get_handle(SG_context*, SG_file * pFile, HANDLE * phFile);
#endif

//////////////////////////////////////////////////////////////////

END_EXTERN_C

#endif//H_SG_FILE_PROTOTYPES_H
