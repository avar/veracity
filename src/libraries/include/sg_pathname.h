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

// sg_pathname.h
// a class for manipulating pathnames.
//////////////////////////////////////////////////////////////////
//
// a Pathname is always stored in absolute normalized form.
//
//////////////////////////////////////////////////////////////////
// While I tried to make this class as platform-independent as possible,
// it just isn't possible to completely remove platform quirks.  Here
// is a brief summary of the platform differences that I've discovered
// so far:
//
// Linux/Mac
// [] allow arbitrary ':' and '\\' characters in pathnames.
// [] pathnames look like "/abc/def/ghi/"
// [] we support ~ and ~user expansion when constructing absolute pathnames.
// [] have . and .. directories
// [] we use utf8 to represent pathnames internally (TODO investigate to
//    see if we need to deal with locale and/or if all platforms use the
//    same form of utf8 (composed or non-composed))
// [] have different rules for when a final slash will appear on a pathname
//    after normalization.
// [] we cannot tell if a pathname represents a file or directory (a trailing
//    slash is a hint, but is not reliable/sufficient)
//
// Windows
// [] have volume, either 'C:' or //host/share/ for UNC paths.
// [] allow exactly one ':' to separate volume from path on that volume
// [] treat '/' and '\\' equivalently (we normalize to '/')
// [] have . and .. directories
// [] "foo." and "foo" are equivalent (both for files and directories)
// [] we use utf8 to represent pathnames internally (but talk to the OS
//    using unicode wide chars behind the scenes)
// [] have different rules for when a final slash will appear on a pathname
//    after normalization.
// [] have different pathname length limits (max path vs //?/ very long
//    paths
// [] have short and long form of filename (old 8.3 compatible name)
// [] we cannot tell if a pathname represents a file or directory (a trailing
//    slash is a hint, but is not reliable/sufficient)
//
//////////////////////////////////////////////////////////////////

#ifndef H_SG_PATHNAME_H
#define H_SG_PATHNAME_H

BEGIN_EXTERN_C

//////////////////////////////////////////////////////////////////

void SG_pathname__alloc(SG_context* pCtx, SG_pathname** ppNew);

void SG_pathname__alloc__copy(SG_context* pCtx, SG_pathname** ppNew, const SG_pathname * pPathname);

void SG_pathname__alloc__pathname_sz(SG_context* pCtx, SG_pathname **ppNew, const SG_pathname * pPathnameDirectory, const char * szFileName);

void SG_pathname__alloc__sz(SG_context* pCtx, SG_pathname ** ppNew, const char * utf8Path);

void SG_pathname__alloc__user_appdata_directory(SG_context* pCtx, SG_pathname** ppNew);

void SG_pathname__alloc__user_temp_directory(SG_context* pCtx, SG_pathname** ppNew);

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#define __SG_PATHNAME__ALLOC__(pp,expr)		SG_STATEMENT(	SG_pathname * _p = NULL;										\
															expr;															\
															_sg_mem__set_caller_data(_p,__FILE__,__LINE__,"SG_pathname");	\
															*(pp) = _p;														)

#define SG_PATHNAME__ALLOC(pCtx,ppNew)												__SG_PATHNAME__ALLOC__(ppNew, SG_pathname__alloc(pCtx,&_p)  )
#define SG_PATHNAME__ALLOC__COPY(pCtx,ppNew,pPathname)								__SG_PATHNAME__ALLOC__(ppNew, SG_pathname__alloc__copy(pCtx,&_p,pPathname) )
#define SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,ppNew,pPathnameDirectory,szFilename)	__SG_PATHNAME__ALLOC__(ppNew, SG_pathname__alloc__pathname_sz(pCtx,&_p,pPathnameDirectory,szFilename) )
#define SG_PATHNAME__ALLOC__SZ(pCtx,ppNew,utf8Path)									__SG_PATHNAME__ALLOC__(ppNew, SG_pathname__alloc__sz(pCtx,&_p,utf8Path) )
#define SG_PATHNAME__ALLOC__USER_APPDATA_DIRECTORY(pCtx,ppNew)						__SG_PATHNAME__ALLOC__(ppNew, SG_pathname__alloc__user_appdata_directory(pCtx,&_p) )
#define SG_PATHNAME__ALLOC__USER_TEMP_DIRECTORY(pCtx,ppNew)									__SG_PATHNAME__ALLOC__(ppNew, SG_pathname__alloc__user_temp_directory(pCtx,&_p) )

#else

#define SG_PATHNAME__ALLOC(pCtx,ppNew)												SG_pathname__alloc(pCtx,ppNew)
#define SG_PATHNAME__ALLOC__COPY(pCtx,ppNew,pPathname)								SG_pathname__alloc__copy(pCtx,ppNew,pPathname)
#define SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,ppNew,pPathnameDirectory,szFilename)	SG_pathname__alloc__pathname_sz(pCtx,ppNew,pPathnameDirectory,szFilename)
#define SG_PATHNAME__ALLOC__SZ(pCtx,ppNew,utf8Path)									SG_pathname__alloc__sz(pCtx,ppNew,utf8Path)
#define SG_PATHNAME__ALLOC__USER_APPDATA_DIRECTORY(pCtx,ppNew)						SG_pathname__alloc__user_appdata_directory(pCtx,ppNew)
#define SG_PATHNAME__ALLOC__USER_TEMP_DIRECTORY(pCtx,ppNew)									SG_pathname__alloc__user_temp_directory(pCtx,ppNew)

#endif

//////////////////////////////////////////////////////////////////

void SG_pathname__free(SG_context * pCtx, SG_pathname * pThis);

void SG_pathname__set__from_cwd(SG_context* pCtx, SG_pathname * pThis);
void SG_pathname__set__from_sz(SG_context* pCtx, SG_pathname * pThis, const char * utf8Path);
void SG_pathname__set__from_pathname(SG_context* pCtx, SG_pathname * pThis, const SG_pathname * pPathname);

/**
 *This implementation of append will always return a non-existent path
 *by appending a number to the name that was passed in.  This is used to 
 *have a temp directory to checkout to.
 */
void SG_pathname__append__force_nonexistant(SG_context* pCtx, SG_pathname * pThis,
									  const char * utf8Path);
void SG_pathname__append__from_sz(SG_context* pCtx, SG_pathname * pThis, const char * utf8Path);
void SG_pathname__append__from_string(SG_context* pCtx, SG_pathname * pThis, const SG_string *);

/**
 * Given two pathnames, return a string containing pSub expressed as a
 * relative path to pTop.
 */
void SG_pathname__make_relative(
    SG_context* pCtx,
	const SG_pathname* pTop,  /**< MUST have the final slash (or else error) */
	const SG_pathname* pSub,  /**< Must be a descendant subdirectory of pTop (or else error) */
	SG_string** ppStrRelative /**< Will not have a leading slash.  Will also not begin with a dot. Will have a final slash iff pSub has one. */
	);

/**
 * Ensure that the given pathname has a final slash.
 */
void SG_pathname__add_final_slash(SG_context* pCtx, SG_pathname * pThis);

/**
 * See if the given pathname has a final slash and if it can be removed.
 *
 * A final slash cannot be removed if the pathname refers to a root directory
 * such as "/" on Linux/Mac and "C:/" on Windows.
 */
void SG_pathname__has_final_slash(SG_context* pCtx, const SG_pathname * pThis, SG_bool * pbHasFinalSlash, SG_bool * pbCanRemoveFinalSlash);

/**
 * Remove the final slash on the given pathname.
 *
 * Returns SG_ERR_CANNOTTRIMROOTDIRECTORY if it is not possible to remove
 * the final slash.
 */
void SG_pathname__remove_final_slash(SG_context* pCtx, SG_pathname * pThis, SG_bool * pbHadFinalSlash);

/**
 * Remove the last component (the filename or last sub-dir) in the given
 * pathname.  We keep the trailing slash.
 *
 * Returns SG_ERR_CANNOTTRIMROOTDIRECTORY if it is not possible to remove
 * the last component because the pathname points to a root directory,
 * such as "C:/" or "//host/share/" on Windows or "/" on Linux/Mac.
 *
 */
void SG_pathname__remove_last(SG_context* pCtx, SG_pathname * pThis);

/**
 * Gives you the filename, ie everything after the last "/".
 * If the path has a trailing "/", you will get an empty string.
 */
void SG_pathname__filename(SG_context * pCtx, const SG_pathname * pThis, const char ** ppFilename);

/**
 * Return the last component in the path as a string.  If the path is
 * "/foo/bar/", this will return "bar".
 */
void SG_pathname__get_last(
    SG_context* pCtx,
	const SG_pathname * pThis,
	SG_string** ppResult
	);

void SG_pathname__clear(SG_context* pCtx, SG_pathname * pThis);

const char * SG_pathname__sz(const SG_pathname * pThis);

SG_uint32 SG_pathname__length_in_bytes(const SG_pathname * pThis);

SG_bool SG_pathname__is_set(const SG_pathname * pThis);

/**
 * Return TRUE if the given pathname points to a root directory.
 *
 */
void SG_pathname__is_root(SG_context* pCtx, const SG_pathname * pThis, SG_bool * pbIsRoot);

/**
 * Returns true if the string contains SPACES or TABS.
 */
void SG_pathname__contains_whitespace(SG_context* pCtx, const SG_pathname * pThis, SG_bool * pbFound);

#if defined(WINDOWS)
/**
 * WINDOWS ONLY
 *
 * Allocate a wchar_t buffer and convert the given pathname into
 * a wide string with the UNC \\?\ prefix, if necessary.
 *
 * We assume that the given pathname is an absolute UTF-8 path and
 * probably does not have the \\?\ prefix.  To keep things simple,
 * we don't put the \\?\ in the pathname when we build it -- we only
 * need the \\?\ when actually talking to the 'W' versions of the
 * Win32 filesystem API routines.  They allow us to have pathnames
 * longer than 260 characters and properly handle Unicode.
 *
 * The caller must free the returned buffer.
 *
 * *pLenResult, if given, will be set to the length of the returned
 * string <b>in wchars not bytes</b>.  THIS LENGTH INCLUDES THE TRAILING
 * wchar_t NULL.
 *
 */
void SG_pathname__to_unc_wchar(SG_context* pCtx,
							   const SG_pathname * pThis,
							   wchar_t ** ppBufResult,
							   SG_uint32 * pLenResult);
#endif

#if defined(WINDOWS)
/**
 * return OK if the pathname is a drive-letter-style pathname.
 *
 * return SG_ERR_INVALIDARG if pathname is a network share (UNC)
 * pathname (or something bogus).
 *
 * Optionally, return TRUE if pathname refers to drive root, such
 * as "C:/".
 *
 */
void SG_pathname__is_drive_letter_pathname(SG_context* pCtx, const SG_pathname * pThis,
										   SG_bool * pbIsRoot);

/**
 * return OK if the pathname is a network share style pathname.
 *
 * return SG_ERR_INVALIDARG if the pathname is a drive-letter
 * pathname (or something bogus).
 *
 * Optionally, return TRUE if the pathname refers to the share
 * root, such as //host/share or //host/share/ rather than a
 * file or directory within the share.
 *
 */
void SG_pathname__is_network_share_pathname(SG_context* pCtx, const SG_pathname * pThis,
											SG_bool * pbIsRoot);
#endif

//////////////////////////////////////////////////////////////////

END_EXTERN_C

#endif//H_SG_PATHNAME_H
