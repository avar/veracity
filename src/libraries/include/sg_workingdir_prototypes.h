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
 * @file sg_workingdir_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_WORKINGDIR_PROTOTYPES_H
#define H_SG_WORKINGDIR_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_workingdir__get_drawer_path(
	SG_context*,
	const SG_pathname* pPathWorkingDirectoryTop,
	SG_pathname** ppResult);

void SG_workingdir__set_mapping(
	SG_context*,
	const SG_pathname* pPathLocalDirectory,
	const char* pszNameRepoInstanceDescriptor, /**< The name of the repo instance descriptor */
	const char* pszidGidAnchorDirectory /**< The GID of the directory within the repo to which this is anchored.  Usually it's user root. */
	);

void SG_workingdir__find_mapping(
	SG_context*,
	const SG_pathname* pPathLocalDirectory,
	SG_pathname** ppPathMappedLocalDirectory, /**< Return the actual local directory that contains the mapping */
	SG_string** ppstrNameRepoInstanceDescriptor, /**< Return the name of the repo instance descriptor */
	char** ppszidGidAnchorDirectory /**< Return the GID of the repo directory */
	);

void SG_workingdir__create_and_get(
	SG_context*,
	const char* pszDescriptorName,
	const SG_pathname* pPathDirPutTopLevelDirInHere,
	SG_bool bCreateDrawer,
	const char* pszHidBaseline
	);

//////////////////////////////////////////////////////////////////

/**
 * Returns the entryname of the <sgtemp> directory.
 * This is something like ".sgtemp".  The assumption
 * is that it will be in the wd-root directory.
 */
void SG_workingdir__get_temp_directory_name(SG_context * pCtx,
											const char ** ppszEntryname);

/**
 * Returns a pathname to the per-working-directory TEMP directory.
 * This can be used to create temp files ON THE SAME FILESYSTEM AS
 * the working directory.  This can let auto-merge create the
 * merge-result-file using an external tool like diff3 without
 * creating a bunch of trash (all 3 input files) in a directory
 * within the user's WD.  In this case, having the input and/or
 * output files on the same filesystem, means that we can MV the
 * results into the user's actual directory without another file
 * copy.  It also means that we don't have to take disk space from
 * /tmp (and the root partition) when we create temp files.
 *
 * Note that the WD-TEMP directory is (probably) only used by
 * the WD that it belongs to, but there is nothing from stopping
 * it from being used by others -- so don't think that you have
 * any locks on this or don't have to worry about collisions.
 *
 * Also, if you create something in this directory, you probably
 * should delete it when you're done with it, since there's no
 * cron job to do this for you.
 *
 * We only create the pathname (something of the form: <wd-top>/.sgtemp/").
 * We DO NOT create anything on disk.
 *
 * See also,  __generate_and_create_temp_dir_for_purpose().
 *
 * You own the returned pathname.
 */
void SG_workingdir__get_temp_path(SG_context* pCtx,
								  const SG_pathname* pPathWorkingDirectoryTop,
								  SG_pathname** ppResult);

/**
 * Create a sub-directory within the per-working-directory TEMP directory.
 * The goal here is to create some private space for a particular task
 * (such as for the duration of a MERGE or REVERT command) where we *may*
 * want to leave it around for the user to inspect.  For example, during
 * a REVERT, we may want to move the modified files here rather than just
 * deleting them before we restore the baseline versions (and instead of
 * polluting the WD with backup files).
 *
 * We create a unique sub-directory and return a pathname to it.
 *
 * You own the returned pathname.
 *
 * You own the directory on disk.
 */
void SG_workingdir__generate_and_create_temp_dir_for_purpose(SG_context * pCtx,
															 const SG_pathname * pPathWorkingDirectoryTop,
															 const char * pszPurpose,
															 SG_pathname ** ppPathTempDir);

//////////////////////////////////////////////////////////////////

/**
 * Compute the absolute pathname to an entry using the working-directory-root
 * (the directory with .sgdrawer) and the entry's repo-path (the @/foo/bar).
 *
 * You own the resulting pathname.
 */
void SG_workingdir__construct_absolute_path_from_repo_path(SG_context * pCtx,
														   const SG_pathname * pPathWorkingDirectoryTop,
														   const char * pszRepoPath,
														   SG_pathname ** ppPathAbsolute);

/**
 * Compute the absolute pathname to an entry using the working directory
 * associated with the given pendingtree and the entry's repo-path.
 *
 * You own the resulting path.
 */
void SG_workingdir__construct_absolute_path_from_repo_path2(SG_context * pCtx,
															const SG_pendingtree * pPendingTree,
															const char * pszRepoPath,
															SG_pathname ** ppPathAbsolute);

//////////////////////////////////////////////////////////////////

void SG_workingdir__wdpath_to_repopath(SG_context* pCtx,
									   const SG_pathname* pPathWorkingDirectoryTop,
									   const SG_pathname* pPath,
									   SG_bool bFinalSlash,
									   SG_string** ppStrRepoPath);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif //H_SG_WORKINGDIR_PROTOTYPES_H
