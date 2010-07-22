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
 * @file sg_mrg__private_temp_dir.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_TEMP_DIR_H
#define H_SG_MRG__PRIVATE_TEMP_DIR_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_mrg__get_temp_dir(SG_context * pCtx, SG_mrg * pMrg, const SG_pathname ** ppPathTempDir)
{
	const SG_pathname * pPathWorkingDirectoryTop;		// we do not own this

	SG_NULLARGCHECK_RETURN(pMrg);
	SG_NULLARGCHECK_RETURN(ppPathTempDir);

	if (!pMrg->pPathOurTempDir)
	{
		// create "<wd-top>/.sgtemp/merge_<session>/"
		//
		// we use this to hold the various versions of files so that the
		// external auto-merge tool can merge them.  We give the scratch
		// space a little structure so that if auto-merge fails, we have
		// something that the user can navigate if they want to manually
		// fix the problem and move the final result to the WD.
		//
		// so for example, we might have a series of:
		//     "<wd-top>/.sgtemp/merge_<session>/<gid-entry>/{A, U0, U1, ...}"
		//
		// this allows us to avoid polluting the WD with all the parts.

		SG_ERR_CHECK_RETURN(  SG_pendingtree__get_working_directory_top__ref(pCtx, pMrg->pPendingTree, &pPathWorkingDirectoryTop)  );
		SG_ERR_CHECK_RETURN(  SG_workingdir__generate_and_create_temp_dir_for_purpose(pCtx,
																					  pPathWorkingDirectoryTop,
																					  "merge",
																					  &pMrg->pPathOurTempDir)  );
	}

	*ppPathTempDir = pMrg->pPathOurTempDir;
}

//////////////////////////////////////////////////////////////////

void SG_mrg__rmdir_temp_dir(SG_context * pCtx, SG_mrg * pMrg)
{
	SG_pathname * pPath = NULL;

	SG_NULLARGCHECK_RETURN(pMrg);

	if (pMrg->pPathOurTempDir == NULL)
		return;

	pPath = pMrg->pPathOurTempDir;		// we own it now
	pMrg->pPathOurTempDir = NULL;

	SG_ERR_CHECK(  SG_fsobj__rmdir_recursive__pathname(pCtx,pPath)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx,pPath);
}

//////////////////////////////////////////////////////////////////

/**
 * Create a pathname in our WD-and-SESSION-private TEMP dir for one of
 * versions of a file to be used as input to the auto-merge.
 *
 * We also create the containing directories on the disk if necessary.
 * We DO NOT create the file.
 *
 * This is something like: "<wd-top>/.sgtemp/merge_<session>/<gid-entry>/<label>"
 * We put all of the versions of the file to be auto-merged in the same sub-directory
 * so that the we might sanely name the versions and so that if necessary, the user
 * can CD to the directory and manually do the work.

 * Since the entry-list is flat (and there may be more than one
 * file with the same name in the whole tree), we use the gid-entry
 * in the resulting pathname rather than the filename.
 *
 * The label reflects the file's place in the plan.  That is, we will
 * have 4 versions of the contents of this entry on disk in 4 different
 * files so we can't name them all "foo.c".  So we give each component
 * a name to relect it's place in the plan.
 *
 * For example, a plan might eventually look like:
 *     cd <wd-top>/.sgtemp/merge_<session>/<gid-entry>
 *     diff3 a0_U0 a0_A a0_U1 > a0_M
 *     diff3  A_U0  A_A a0_M  >  A_M
 *
 * to handle something like:
 *            A_A
 *           /   \.
 *          /     \.
 *         /       a0_A
 *        /       /    \.
 *       /   a0_U0      a0_U1
 *      /         \    /
 *  A_U0           a0_M
 *      \         /
 *       \       /
 *        \     /
 *         \   /
 *          A_M
 *
 * TODO think about adding the file's suffix (either the one from
 * TODO this version of the file's name -or- one unified suffix
 * TODO taken from the set of name (when there were also RENAMES)).
 */
void SG_mrg__make_automerge_temp_file_pathname(SG_context * pCtx,
											   SG_mrg_automerge_plan_item * pItem,
											   const char * pszLabelSuffix,
											   SG_pathname ** ppPathReturned)
{
	SG_pathname * pPathTempFile_Allocated = NULL;
	const SG_pathname * pPathTempDir = NULL;
	SG_string * pString_Allocated = NULL;
	SG_bool bDirExists;

	// Build the pathname for the temp file as something like:
	// "<wd-top/.sgtemp/merge_<session>/<gid-entry>/<ancestor-cset-name>_<label>"

	SG_ERR_CHECK(  SG_mrg__get_temp_dir(pCtx,pItem->pMrg,&pPathTempDir)  );
	SG_ERR_CHECK(  SG_pathname__alloc__pathname_sz(pCtx,&pPathTempFile_Allocated,pPathTempDir,
												   pItem->pMrgCSetEntry_Ancestor->bufGid_Entry)  );
	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathTempFile_Allocated,&bDirExists,NULL,NULL)  );
	if (!bDirExists)
		SG_ERR_CHECK(  SG_fsobj__mkdir_recursive__pathname(pCtx,pPathTempFile_Allocated)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx,&pString_Allocated)  );
	SG_ERR_CHECK(  SG_string__append__string(pCtx,pString_Allocated,
											 pItem->pMrgCSetEntry_Ancestor->pMrgCSet->pStringName)  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx,pString_Allocated,"_")  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx,pString_Allocated,pszLabelSuffix)  );

	SG_ERR_CHECK(  SG_pathname__append__from_string(pCtx,pPathTempFile_Allocated,pString_Allocated)  );

	SG_STRING_NULLFREE(pCtx,pString_Allocated);

	*ppPathReturned = pPathTempFile_Allocated;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTempFile_Allocated);
	SG_STRING_NULLFREE(pCtx,pString_Allocated);
}

//////////////////////////////////////////////////////////////////

/**
 * Create a pathname in our WD-and-SESSION-private TEMP dir for one
 * of the versions of a file to be used as input to the diff in the
 * merge-preview.
 *
 * We also create the containing directories on disk as necessary.
 * We DO NOT create the file.
 *
 * This is something like: "<wd-top>/.sgtemp/merge_<session>/blobs/<hid-blob>"
 *
 * We use this "blobs" directory for all of the various "loose" files/blobs
 * that we need.  We occasionally fetch the contents of a file version so that
 * preview can show some file diffs;  we also need them to get the symlink target
 * and/or the xattr text when we want to show them.
 *
 * This "blobs" directory is distinct from the gid-entry-based auto-merge
 * directories.
 */
void SG_mrg__make_hid_blob_temp_file_pathname(SG_context * pCtx, SG_mrg * pMrg,
											  const char * pszHidBlob,
											  SG_pathname ** ppPathReturned)
{
	SG_pathname * pPathTempFile_Allocated = NULL;
	const SG_pathname * pPathTempDir = NULL;
	SG_bool bDirExists;

	// Build the pathname for the temp file as something like:
	// "<wd-top>/.sgtemp/merge_<session>/blobs/<hid-blob>"

	SG_ERR_CHECK(  SG_mrg__get_temp_dir(pCtx,pMrg,&pPathTempDir)  );
	SG_ERR_CHECK(  SG_pathname__alloc__pathname_sz(pCtx,&pPathTempFile_Allocated,pPathTempDir,"blobs")  );
	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathTempFile_Allocated,&bDirExists,NULL,NULL)  );
	if (!bDirExists)
		SG_ERR_CHECK(  SG_fsobj__mkdir_recursive__pathname(pCtx,pPathTempFile_Allocated)  );

	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pPathTempFile_Allocated,pszHidBlob)  );

	*ppPathReturned = pPathTempFile_Allocated;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTempFile_Allocated);
}

//////////////////////////////////////////////////////////////////

/**
 * Export the contents of the given file entry (which reflects a specific version)
 * into a temp file so that an external tool can use it.
 *
 * You own the returned pathname and the file on disk.
 */
void SG_mrg__export_to_temp_file(SG_context * pCtx, SG_mrg * pMrg,
								 const char * pszHidBlob,
								 const SG_pathname * pPathTempFile)
{
	SG_file * pFile = NULL;
	SG_repo * pRepo;
	SG_bool bExists;

	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathTempFile,&bExists,NULL,NULL)  );
	if (bExists)
	{
#if TRACE_MRG
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "Skipping export of [blob %s] to [%s]\n",pszHidBlob,SG_pathname__sz(pPathTempFile))  );
#endif
	}
	else
	{
#if TRACE_MRG
		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   "Exporting          [blob %s] to [%s]\n",pszHidBlob,SG_pathname__sz(pPathTempFile))  );
#endif

		SG_ERR_CHECK(  SG_file__open__pathname(pCtx,pPathTempFile,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,0400,&pFile)  );
		SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pMrg->pPendingTree, &pRepo)  );
		SG_ERR_CHECK(  SG_repo__fetch_blob_into_file(pCtx,pRepo,pszHidBlob,pFile,NULL)  );
		SG_FILE_NULLCLOSE(pCtx,pFile);
	}

	return;

fail:
	if (pFile)			// only if **WE** created the file, do we try to delete it on error.
	{
		SG_FILE_NULLCLOSE(pCtx,pFile);
		SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx,pPathTempFile)  );
	}
}


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_TEMP_DIR_H
