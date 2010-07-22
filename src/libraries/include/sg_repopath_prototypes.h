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
 * @file sg_repopath_typedefs.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_REPOPATH_PROTOTYPES_H
#define H_SG_REPOPATH_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_repopath__foreach(
	SG_context* pCtx,
	const char* pszRepoPath, /**< Should be an absolute repo path.  It may or may not have the final slash */
	SG_repopath_foreach_callback cb,
	void* ctx
	);

/**
 * Takes a repo path and splits it into all the parts between the slashes.
 * The first part is always "@".  For example, the path "@/foo/bar" will return
 * three parts:  @, foo, bar.
 */
void SG_repopath__split(
	SG_context* pCtx,
	const char* pszRepoPath, /**< Should be an absolute repo path.  It may or may not have the final slash */
	SG_strpool* pPool,       /**< strings in papszParts are allocated in this pool */
	const char*** papszParts, /**< caller must free this */
	SG_uint32* piCountParts
	);

void SG_repopath__has_final_slash(SG_context* pCtx, const SG_string * pThis, SG_bool * pbHasFinalSlash);

void SG_repopath__ensure_final_slash(SG_context* pCtx, SG_string * pThis);

void SG_repopath__remove_final_slash(SG_context* pCtx, SG_string * pThis);

void SG_repopath__combine__sz(
	SG_context* pCtx,
	const char* pszAbsolute,
	const char* pszRelative,
	SG_bool bFinalSlash,
	SG_string** ppstrNew
	);

void SG_repopath__combine(
	SG_context* pCtx,
	const SG_string* pstrAbsolute,
	const SG_string* pstrRelative,
	SG_bool bFinalSlash,
	SG_string** ppstrNew
	);

/**
 * Append an entryname to the given repo-path.  Optionally add a final slash.
 * Automatically ensures that there is a slash between the existing and new portion.
 */
void SG_repopath__append_entryname(SG_context* pCtx, SG_string * pStrRepoPath, const char * szEntryName, SG_bool bFinalSlash);


/**
 * Compare two repopaths.  This is just like strcmp, except:
 * 1.  All '/' characters are sorted to the top.  This avoids this crazy sorting case:
 * 		@/foo
 * 		@/foo space
 * 		@/foo space/subitem
 * 		@/foo space/subitem2
 * 		@/foo/subitem
 * 		@/foo/subitem2
 * 2.  Multiple '/' characters are treated as one.
 * 3.  It is currently case sensitive, and Unicode-ignorant.
 */
void SG_repopath__compare(SG_context* pCtx, const char * pszPath1, const char * pszPath2, SG_int32 * pNReturnVal);

/**
 * Normalize a repopath.  The following replacements will be made:
 * "\" gets converted to "/"
 * "./" gets deleted.
 *
 *  If bFinalSlash is false, the returned path will not have a trailing slash.
 */
void SG_repopath__normalize(SG_context* pCtx, SG_string * pStrRepoPath, SG_bool bFinalSlash);

void SG_repopath__remove_last(SG_context* pCtx, SG_string * pStrRepoPath);

void SG_repopath__get_last(SG_context* pCtx, SG_string* pStrRepoPath,	SG_string** ppResult);

END_EXTERN_C;

#endif//H_SG_REPOPATH_PROTOTYPES_H
