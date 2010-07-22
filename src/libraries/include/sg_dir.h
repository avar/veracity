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

// sg_dir.h
// routines to read a directory
//////////////////////////////////////////////////////////////////

#ifndef H_SG_DIR_H
#define H_SG_DIR_H

BEGIN_EXTERN_C

//////////////////////////////////////////////////////////////////

// SG_dir__open --	Open the named directory, in pPathname, and prepare
//					for reading entries.
//
//					Returns a "directory-handle", in *ppDirResult, that can
//					be used to enumerate files and subdirectories.  This
//					structure is allocated.  If __open() returns OK, you
//					must eventually call __close() to release OS handles
//					and memory.
//
//					The return code gives the result of the OPEN.
//
//					This routine also returns the first file/subdirectory
//					in the next 3 args.  (It essentially does the first
//					__read() automatically.)  (This is to hide platform
//					differences.)  See __read() for info on those args.

void SG_dir__open(SG_context* pCtx, const SG_pathname * pPathname,
					  SG_dir ** ppDirResult,
					  SG_error * pErrReadStat,
					  SG_string * pStringNameFirstFileRead,
					  SG_fsobj_stat * pStatFirstFileRead);

/**
 * SG_dir__close --	Close the "directory-handle" and release all OS handles
 * and memory.  This frees pThis.
 *
 */
void SG_dir__close(SG_context * pCtx, SG_dir * pThis);

/**
 * Close the given directory and null the pointer.
 */
#define SG_DIR_NULLCLOSE(pCtx, pDir)		SG_STATEMENT( if (pDir!=NULL)                                   \
														  {                                                 \
															  SG_ERR_IGNORE(  SG_dir__close(pCtx, pDir)  ); \
															  pDir = NULL;                                  \
														  }                                                 )

// SG_dir__read --	Read the next entry (file/subdirectory) in the opened
//					directory and optionally stat it.
//
//					The entryname is copied into the string provided.
//
//					If pStatResult is non-null, we will also attempt to
//					stat the file/subdirectory and place (normalized)
//					stat-data in the given structure.
//
//					The return code is for either the underlying read or
//					stat (or our post-processing).  If an error is returned,
//					you should not rely on either field.
//
//					Returns SG_ERR_NOMOREFILES when you reach the end of the
//					directory.
//
//					Entries in the directory are returned in an undefined
//					order (it's platform and filesystem specific).  We do
//					not do any filtering.

void SG_dir__read(SG_context* pCtx, SG_dir * pThis,
					  SG_string * pStringFileName,
					  SG_fsobj_stat * pStatResult);

//////////////////////////////////////////////////////////////////

typedef void (SG_dir_foreach_callback)(SG_context * pCtx, const SG_string * pStringEntryName, SG_fsobj_stat * pfsStat, void * pVoidData);

/**
 * Read the entire directory and call the callback on each entry.
 *
 * It is up to the callback to ignore "." and ".." if appropriate.
 *
 * Callback is given a read-only string containing the entryname;
 * it does not own it.
 *
 * If bWantStat is true, the callback will receive valid stat()
 * data for the entry; otherwise, the stat field will be null.
 */
void SG_dir__foreach(SG_context * pCtx,
					 const SG_pathname * pPathDir,
					 SG_bool bWantStat,
					 SG_dir_foreach_callback pfnCB,
					 void * pVoidData);

//////////////////////////////////////////////////////////////////

void SG_dir__list(
    SG_context* pCtx,
    const SG_pathname * pPathDir,
    const char* psz_match_begin,
    const char* psz_match_anywhere,
    const char* psz_match_end,
    SG_rbtree** pprb
    );

void SG_dir__count(
    SG_context* pCtx,
    const SG_pathname * pPathDir,
    const char* psz_match_begin,
    const char* psz_match_anywhere,
    const char* psz_match_end,
    SG_uint32* pi
    );

END_EXTERN_C

#endif//H_SG_DIR_H
