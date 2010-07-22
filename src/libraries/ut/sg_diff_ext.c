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
 * @file sg_diff_ext.c
 *
 * @details Routines to run a diff in a subordinate process.  For example,
 * exec'ing gnu-diff or exec'ing SGDM in batch-mode.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

#if 0 && defined(DEBUG)
#	define TRACE_DIFF		1
#else
#	define TRACE_DIFF		0
#endif

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
/**
 * Spawn GNU-DIFF as a subbordinate process to do the dirty work.
 * We assume it is installed in /usr/bin/diff.
 */
void SG_diff_ext__gnu_diff(SG_context * pCtx,
						   const SG_pathname * pPathname0,
						   const SG_pathname * pPathname1,
						   const char * szLabel0,
						   const char * szLabel1,
						   SG_file * pFileStdout, SG_file * pFileStderr,
						   SG_bool * pbFoundChanges)
{
	SG_exit_status childExitStatus;
	SG_exec_argvec * pArgVec = NULL;
	SG_fsobj_stat stat0, stat1;

	if (!pPathname0 && !pPathname1)		// zero or one can be null, but not both
		SG_ERR_THROW_RETURN(  SG_ERR_INVALIDARG  );

	SG_NONEMPTYCHECK_RETURN(szLabel0);
	SG_NONEMPTYCHECK_RETURN(szLabel1);

	if (pPathname0)		// if given, path must refer to a plain file (not a directory).
	{
		SG_ERR_CHECK_RETURN(  SG_fsobj__stat__pathname(pCtx,pPathname0,&stat0)  );
		SG_ARGCHECK_RETURN( (stat0.type == SG_FSOBJ_TYPE__REGULAR), "type of path 0" );
	}

	if (pPathname1)		// if given, path must refer to a plain file (not a directory).
	{
		SG_ERR_CHECK_RETURN(  SG_fsobj__stat__pathname(pCtx,pPathname1,&stat1)  );
		SG_ARGCHECK_RETURN( (stat1.type == SG_FSOBJ_TYPE__REGULAR), "type of path 1" );
	}

	// We do something like:
	//
	//     /usr/bin/diff -au --label "LABEL0" --label "LABEL1" "PATH0" "PATH1" > fileOutput
	//
	// (We don't have to worry about quoting arguments because SG_exec will take care of
	// that if necessary.)

	// TODO consider getting gnu-diff args from local settings.  (at least the -au part)

	SG_ERR_CHECK(  SG_exec_argvec__alloc(pCtx,&pArgVec)  );

	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,"-au")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,"--label")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,szLabel0)  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,"--label")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,szLabel1)  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,((pPathname0) ? SG_pathname__sz(pPathname0) : "/dev/null"))  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,((pPathname1) ? SG_pathname__sz(pPathname1) : "/dev/null"))  );

	// fork, exec, and wait for gnu-diff.
	//
	// we share our STDIN (unused).
	//
	// send child's STDOUT and STDERR to the given files.

	SG_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,
											 "/usr/bin/diff",pArgVec,
											 NULL,pFileStdout,pFileStderr,
											 &childExitStatus)  );

	// gnu-diff returns:
	// 0 no differences --> mapped to SG_ERR_OK
	// 1 differences    --> mapped to SG_ERR_ERRNO(1) or SG_ERR_GETLASTERROR(1)
	// 2 error          --> mapped to SG_ERR_ERRNO(2) or SG_ERR_GETLASTERROR(2)
	//
	// SG_exec has already trapped 255's and thrown.

	switch (childExitStatus)
	{
	case 0:
		*pbFoundChanges = SG_FALSE;
		break;

	case 1:
		*pbFoundChanges = SG_TRUE;
		break;

//	case 2:
	default:
		SG_ERR_THROW(  SG_ERR_EXTERNAL_TOOL_ERROR  );
	}

	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);

	return;

fail:
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);
}
#endif

//////////////////////////////////////////////////////////////////

#if 0 // NOT NEEDED
/**
 * find the file/symlink/directory in the working directory and fetch
 * the various attributes set on it.
 *
 * TODO figure out what to do with XAttrs.
 */
static SG_error _sg_diff_ext__get_all_attrs_from_wd(const SG_pathname * pPathWorkingDirectoryTop,
													const char * szRepoPath,
													SG_int64 * pAttrBits)
{
	SG_error err;
	SG_pathname * pPathToEntry = NULL;
	SG_int64 attrBitsBaseline = 0;			// TODO decide what the appropriate default for this is.
	SG_int64 attrBitsRead;

	SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx,pPathWorkingDirectoryTop,szRepoPath,&pPathToEntry)  );
	SG_ERR_CHECK(  SG_attributes__bits__read(pPathToEntry,attrBitsBaseline,&attrBitsRead)  );

	// TODO figure out what to do with XAttrs...

	*pAttrBits = attrBitsRead;

	SG_PATHNAME_NULLFREE(pCtx, pPathToEntry);
	return SG_ERR_OK;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathToEntry);
	return err;
}
#endif

//////////////////////////////////////////////////////////////////

static void _out(SG_context * pCtx, SG_file * pFile, const SG_string * pStr)
{
	if (pFile)
	{
		// TODO do we need to convert this UTF-8 data to an OS-buffer?
		SG_ERR_CHECK_RETURN(  SG_file__write(pCtx,pFile,SG_string__length_in_bytes(pStr),(SG_byte *)SG_string__sz(pStr),NULL)  );
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_console(pCtx,SG_CS_STDOUT,"%s",SG_string__sz(pStr))  );
	}
}

//////////////////////////////////////////////////////////////////

struct _do_1_data		// we do not own any of the pointers in this
{
	SG_repo *							pRepo;
	SG_treediff2 *						pTreeDiff;
	SG_diff_ext__compare_callback *		pfnCompare;
	SG_file *							pFileStdout;
	SG_file *							pFileStderr;
	SG_pathname *						pPathTempSessionDir;
	const SG_pathname *					pPathWorkingDirectoryTop;
};

/**
 * Launch external diff tool on 1 file entry.
 *
 * For a "modified" entry this is a pair of files.
 * For an "added" or "deleted" this is one file vs something like /dev/null
 * so that we get the trivial everything added/deleted output.
 */
static void _sg_diff_ext__do_1_file(SG_context * pCtx,
									struct _do_1_data * pDo1Data,
									const char * szGidObject,
									const SG_treediff2_ObjectData * pOD_opaque)
{
	SG_diffstatus_flags dsFlagsSet;
	SG_diffstatus_flags dsFlags_me;
	const char * szRepoPath;
	const char * szHidContent_a;
	const char * szHidContent_b;
	SG_pathname * pPathFile_a = NULL;
	SG_pathname * pPathFile_b = NULL;
	SG_string * pStringLabel_a = NULL;
	SG_string * pStringLabel_b = NULL;
	SG_treediff2_kind kind;
	SG_bool bFoundChanges;
	char bufDate[SG_TIME_FORMAT_LENGTH+1];

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_opaque,&szRepoPath)  );
	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx,pOD_opaque,&dsFlagsSet)  );

	// the calls to get the content HIDs  may return null for one or the other if there is
	// no value (like in an __ADD or __DELETE) or if we don't know what it is (such as when
	// sg_pendingtree doesn't need to compute it).

	dsFlags_me = (dsFlagsSet & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE);
	switch (dsFlags_me)
	{
	case SG_DIFFSTATUS_FLAGS__DELETED:		// lookup content HID of old version, export the
	case SG_DIFFSTATUS_FLAGS__MODIFIED:		// contents to a temp file and make a normal diff-label.
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_content_hid(pCtx,pOD_opaque,&szHidContent_a)  );
		SG_ASSERT_RELEASE_FAIL(  ((szHidContent_a) && (*szHidContent_a ))  );
		SG_ERR_CHECK(  SG_diff_utils__export_to_temp_file(pCtx,
														  pDo1Data->pRepo,
														  pDo1Data->pPathTempSessionDir,"_a_",
														  szGidObject,szHidContent_a,
														  &pPathFile_a)  );
		SG_ERR_CHECK(  SG_diff_utils__make_label(pCtx,szRepoPath,szHidContent_a,NULL,&pStringLabel_a)  );
		break;

	case SG_DIFFSTATUS_FLAGS__ADDED:		// make a trivial label for add
		SG_ERR_CHECK(  SG_time__format_utc__i64(pCtx,0,bufDate,SG_NrElements(bufDate))  );
		SG_ERR_CHECK(  SG_diff_utils__make_label(pCtx,szRepoPath,NULL,bufDate,&pStringLabel_a)  );
		break;

	default:
		SG_ASSERT_RELEASE_FAIL(  (0)  );
	}

	switch (dsFlags_me)
	{
	case SG_DIFFSTATUS_FLAGS__DELETED:		// make a trivial label for delete
		SG_ERR_CHECK(  SG_time__format_utc__i64(pCtx,0,bufDate,SG_NrElements(bufDate))  );
		SG_ERR_CHECK(  SG_diff_utils__make_label(pCtx,szRepoPath,NULL,bufDate,&pStringLabel_b)  );
		break;

	case SG_DIFFSTATUS_FLAGS__MODIFIED:
	case SG_DIFFSTATUS_FLAGS__ADDED:
		SG_ERR_CHECK(  SG_treediff2__get_kind(pCtx,pDo1Data->pTreeDiff,&kind)  );
		if (kind == SG_TREEDIFF2_KIND__CSET_VS_CSET)
		{
			// if we have a CSET-vs-CSET treediff, szHidContent_b should always be set and valid.
			// we want to fetch that version of the file into a temp file and use it for the diff.
			// (this should only fail if the repo is too sparse.)

			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_content_hid(pCtx,pOD_opaque,&szHidContent_b)  );
			SG_ASSERT_RELEASE_FAIL(  (szHidContent_b && *szHidContent_b)  );
			SG_ERR_CHECK(  SG_diff_utils__export_to_temp_file(pCtx,
															  pDo1Data->pRepo,
															  pDo1Data->pPathTempSessionDir,"_b_",
															  szGidObject,szHidContent_b,
															  &pPathFile_b)  );
			SG_ERR_CHECK(  SG_diff_utils__make_label(pCtx,szRepoPath,szHidContent_b,NULL,&pStringLabel_b)  );
		}
		else
		{
			// if we have a CSET-vs-PENDINGTREE or BASELINE-vs-PENDINGTREE treediff, the HID value
			// in the treediff MAY or MAY NOT be valid:
			//
			// [1] sometimes sg_pendingtree doesn't need to compute an HID for it.
			// [2] somteimes sg_pendingtree computes a value (ie during an "vv add") but because
			//     it hasn't been committed, there isn't a blob with that HID in the repo.
			// [3] (maybe) if the added file changes in the working-directory after it was added,
			//     we may or may not recompute it (i'm not sure about this).
			//
			// in any case, let's just use the file that's currently in the working-directory.

			SG_fsobj_stat st;

			// convert repo-path to pathname in the wd.

			if (!pDo1Data->pPathWorkingDirectoryTop)
				SG_ERR_CHECK(  SG_treediff2__get_working_directory_top(pCtx,pDo1Data->pTreeDiff,&pDo1Data->pPathWorkingDirectoryTop)  );

			SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx,pDo1Data->pPathWorkingDirectoryTop,szRepoPath,&pPathFile_b)  );
			SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx,pPathFile_b,&st)  );
			SG_ERR_CHECK(  SG_time__format_utc__i64(pCtx,st.mtime_ms,bufDate,SG_NrElements(bufDate))  );

			// TODO do we want to put the actual wd path in the label instead of the repo-path?

			SG_ERR_CHECK(  SG_diff_utils__make_label(pCtx,szRepoPath,NULL,bufDate,&pStringLabel_b)  );
		}
		break;

	default:
		SG_ASSERT_RELEASE_FAIL(  (0)  );
	}

	// actually do the comparison using external tool.

	SG_ERR_CHECK(  (*pDo1Data->pfnCompare)(pCtx,
										   pPathFile_a,pPathFile_b,
										   SG_string__sz(pStringLabel_a),SG_string__sz(pStringLabel_b),
										   pDo1Data->pFileStdout,pDo1Data->pFileStderr,
										   &bFoundChanges)  );

	// fall thru to common cleanup

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathFile_a);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile_b);
	SG_STRING_NULLFREE(pCtx, pStringLabel_a);
	SG_STRING_NULLFREE(pCtx, pStringLabel_b);
}

static SG_treediff2_foreach_callback _sg_diff_ext__do_1__cb;

/**
 * Create diff preamble for 1 entry from the status.  We output a series of
 * preamble header lines and then, if appropriate, launch the external diff
 * tool.
 */
static void _sg_diff_ext__do_1__cb(SG_context * pCtx,
								   SG_treediff2 * pTreeDiff,
								   SG_UNUSED_PARAM(const char * szGidObject),
								   const SG_treediff2_ObjectData * pOD_opaque,
								   void * pVoidDo1Data)
{
	struct _do_1_data * pDo1Data = (struct _do_1_data *)pVoidDo1Data;
	SG_diffstatus_flags dsFlagsSet;
	SG_diffstatus_flags dsFlags_me;
	SG_treenode_entry_type tneType;
	const char * szEntryType = NULL;
	const char * szRepoPath;
	SG_string * pStrWork = NULL;

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx,pOD_opaque,&dsFlagsSet)  );

	dsFlags_me = (dsFlagsSet & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE);
	switch (dsFlags_me)
	{
	case SG_DIFFSTATUS_FLAGS__LOST:		// we don't include lost/found items in diff output
	case SG_DIFFSTATUS_FLAGS__FOUND:	// even if there are attrbits or xattr changes.
		goto done;

	default:
		break;
	}

	// the treediff now contains entries for folders that were "modified";
	// meaning that the contents *within* the folder changed (and the folder's
	// hid content changed).  if that is the only reason why the folder is in
	// the treediff, silently ignore it.

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_tneType(pCtx,pOD_opaque,&tneType)  );
	if ((tneType == SG_TREENODEENTRY_TYPE_DIRECTORY) && (dsFlagsSet == SG_DIFFSTATUS_FLAGS__MODIFIED))
		goto done;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx,&pStrWork)  );

	SG_ERR_CHECK(  SG_string__sprintf(pCtx,pStrWork,"=== ================\n")  );
	SG_ERR_CHECK(  _out(pCtx,pDo1Data->pFileStdout,pStrWork)  );

	switch (tneType)
	{
	case SG_TREENODEENTRY_TYPE_DIRECTORY:		szEntryType = "directory";	break;
	case SG_TREENODEENTRY_TYPE_REGULAR_FILE:	szEntryType = "file";		break;
	case SG_TREENODEENTRY_TYPE_SYMLINK:			szEntryType = "symlink";	break;
	default:									SG_ASSERT_RELEASE_FAIL( (0) );
	}

	SG_ERR_CHECK(  SG_treediff2__ObjectData__get_repo_path(pCtx,pOD_opaque,&szRepoPath)  );

	if (dsFlagsSet & SG_DIFFSTATUS_FLAGS__RENAMED)
	{
		const char * szOldName;

		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx,pOD_opaque,&szOldName)  );
		SG_ERR_CHECK(  SG_string__sprintf(pCtx,
										  pStrWork,
										  "=== Renamed: %s %s (was %s)\n",
										  szEntryType,szRepoPath,szOldName)  );
		SG_ERR_CHECK(  _out(pCtx,pDo1Data->pFileStdout,pStrWork)  );
	}
	if (dsFlagsSet & SG_DIFFSTATUS_FLAGS__MOVED)
	{
		const char * szOldParentRepoPath;

		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_moved_from_repo_path(pCtx,pTreeDiff,pOD_opaque,&szOldParentRepoPath)  );
		SG_ERR_CHECK(  SG_string__sprintf(pCtx,
										  pStrWork,
										  "=== Moved: %s %s (from %s)\n",
										  szEntryType,szRepoPath,szOldParentRepoPath)  );
		SG_ERR_CHECK(  _out(pCtx,pDo1Data->pFileStdout,pStrWork)  );
	}
	if (dsFlagsSet & SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)
	{
		SG_int64 attr_old;
		SG_int64 attr_new;

		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_attrbits(pCtx,pOD_opaque,&attr_old)  );
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_attrbits(pCtx,pOD_opaque,&attr_new)  );

		// TODO convert these int64 attrbits to a set of human readable flags (like +x or -x)

		SG_ERR_CHECK(  SG_string__sprintf(pCtx,
										  pStrWork,
										  "=== Attributes: %s %s '%x' (was '%x')\n",
										  szEntryType,szRepoPath,
										  (SG_uint32)attr_new,(SG_uint32)attr_old)  );
		SG_ERR_CHECK(  _out(pCtx,pDo1Data->pFileStdout,pStrWork)  );
	}
	if (dsFlagsSet & SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)
	{
		// TODO decide how to show differences in XAttrs.  Do we want to splat them to a file
		// TODO and let gnu-diff just diff them or do we want to do something smarter???
		// TODO
		// TODO for now, just dump the HIDs for testing purposes.

		const char * szHidXAttr_old;
		const char * szHidXAttr_new;

		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_old_xattrs(pCtx,pOD_opaque,&szHidXAttr_old)  );
		SG_ERR_CHECK(  SG_treediff2__ObjectData__get_xattrs(pCtx,pOD_opaque,&szHidXAttr_new)  );

		SG_ERR_CHECK(  SG_string__sprintf(pCtx,
										  pStrWork,
										  "=== XAttrs: %s %s TODO '%s' (was '%s')\n",
										  szEntryType,szRepoPath,
										  szHidXAttr_new,szHidXAttr_old)  );
		SG_ERR_CHECK(  _out(pCtx,pDo1Data->pFileStdout,pStrWork)  );
	}

	switch (dsFlags_me)
	{
	case SG_DIFFSTATUS_FLAGS__ADDED:
		{
			SG_int64 attr_new = 0;
			const char * szHidXAttr_new = NULL;

			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_attrbits(pCtx,pOD_opaque,&attr_new)  );
			SG_ERR_CHECK(  SG_treediff2__ObjectData__get_xattrs(pCtx,pOD_opaque,&szHidXAttr_new)  );

			// TODO convert these int64 attrbits to a set of human readable flags (like +x or -x)

			SG_ERR_CHECK(  SG_string__sprintf(pCtx,
											  pStrWork,
											  "=== Added: %s %s (Attributes: '%x')\n",
											  szEntryType,szRepoPath,(SG_uint32)attr_new)  );
			SG_ERR_CHECK(  _out(pCtx,pDo1Data->pFileStdout,pStrWork)  );

			if (szHidXAttr_new && *szHidXAttr_new)
			{
				// TODO for now we just splat the HID of the XAttrs.  Figure out if we want this at all and
				// TODO how we want to format the content of the XAttrs in the header.

				SG_ERR_CHECK(  SG_string__sprintf(pCtx,
												  pStrWork,
												  "===     TODO XAttrs: '%s'\n",
												  szHidXAttr_new)  );
				SG_ERR_CHECK(  _out(pCtx,pDo1Data->pFileStdout,pStrWork)  );
			}
		}
		break;

	case SG_DIFFSTATUS_FLAGS__DELETED:
		{
			SG_ERR_CHECK(  SG_string__sprintf(pCtx,
											  pStrWork,
											  "=== Deleted: %s %s\n",
											  szEntryType,szRepoPath)  );
			SG_ERR_CHECK(  _out(pCtx,pDo1Data->pFileStdout,pStrWork)  );
		}
		break;

	case SG_DIFFSTATUS_FLAGS__MODIFIED:
		if (tneType != SG_TREENODEENTRY_TYPE_DIRECTORY)
		{
			SG_ERR_CHECK(  SG_string__sprintf(pCtx,
											  pStrWork,
											  "=== Modified: %s %s\n",
											  szEntryType,szRepoPath)  );
			SG_ERR_CHECK(  _out(pCtx,pDo1Data->pFileStdout,pStrWork)  );
		}
		break;

	default:
		goto done;
	}

	switch (tneType)
	{
	case SG_TREENODEENTRY_TYPE_REGULAR_FILE:
		// handle added/deleted/modified file.  shell out to gnu-diff or whatever
		// command line tool was chosen by the caller.
		SG_ERR_CHECK(  _sg_diff_ext__do_1_file(pCtx,pDo1Data,szGidObject,pOD_opaque)  );
		break;

	case SG_TREENODEENTRY_TYPE_DIRECTORY:
		// we have nothing to show for added/deleted/modified directories.
		break;

	case SG_TREENODEENTRY_TYPE_SYMLINK:
		// TODO decide if/how we want to show added/deleted/modified symlink values.
		// TODO we have hid_0 and hid_1 when modified.
		// TODO we have hid_1 when added to cset1
		// TODO we have nothing when added to wd
		// TODO we have hid_0 when deleted

		SG_ERR_CHECK(  SG_string__sprintf(pCtx,
										  pStrWork,
										  "===     TODO How do we show symlinks?\n")  );
		SG_ERR_CHECK(  _out(pCtx,pDo1Data->pFileStdout,pStrWork)  );
		break;

	default:
		SG_ASSERT_RELEASE_FAIL(  (0)  );
	}

done:
	// fall thru to common cleanup

fail:
	SG_STRING_NULLFREE(pCtx, pStrWork);
}

//////////////////////////////////////////////////////////////////

void SG_diff_ext__diff(SG_context * pCtx,
					   SG_treediff2 * pTreeDiff,
					   SG_diff_ext__compare_callback * pfnCompare,
					   SG_file * pFileStdout, SG_file * pFileStderr)
{
	SG_pathname * pPathTempSessionDir = NULL;
	char bufTidSession[SG_TID_MAX_BUFFER_LENGTH];
	struct _do_1_data Do1Data;

	SG_NULLARGCHECK(pTreeDiff);

	// TODO change this to lookup the compare function/external tool in the localsettings.
	// TODO maybe also allow it to vary by file type.

	if (!pfnCompare)
		pfnCompare = SG_internalfilediff__unified;

	// pick a space in /tmp for exporting temporary copies of the pairs of files
	// so that the external tool can compare them.
	//
	// we create "$TMPDIR/<gid_session>/"
	//
	// TODO investigate the use of SG_workingdir__get_temp_path() rather than /tmp (see also sg_mrg__private_file_mrg.h)

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__USER_TEMP_DIRECTORY(pCtx,&pPathTempSessionDir)  );
	SG_ERR_CHECK(  SG_tid__generate(pCtx, bufTidSession, sizeof(bufTidSession))  );
	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pPathTempSessionDir,bufTidSession)  );

	memset(&Do1Data,0,sizeof(Do1Data));

	SG_ERR_CHECK(  SG_treediff2__get_repo(pCtx,pTreeDiff,&Do1Data.pRepo)  );

	Do1Data.pTreeDiff = pTreeDiff;
	Do1Data.pfnCompare = pfnCompare;
	Do1Data.pFileStdout = pFileStdout;
	Do1Data.pFileStderr = pFileStderr;
	Do1Data.pPathTempSessionDir = pPathTempSessionDir;

	SG_ERR_CHECK(  SG_treediff2__foreach(pCtx,pTreeDiff,_sg_diff_ext__do_1__cb,&Do1Data)  );

	// fall thru to common cleanup

fail:
	if (pPathTempSessionDir)
	{
		// we may or may not be able to delete the tmp dir (they may be visiting it in another window)
		// so we have to allow this to fail and not mess up the real context.

		SG_ERR_IGNORE(  SG_fsobj__rmdir_recursive__pathname(pCtx,pPathTempSessionDir)  );
		SG_PATHNAME_NULLFREE(pCtx, pPathTempSessionDir);
	}
}

//////////////////////////////////////////////////////////////////

void _sg_internalfilediff__unified(
    SG_context * pCtx,
    SG_diff_options diffOptions,
    const SG_pathname * pPathname0,
    const SG_pathname * pPathname1,
    const char * szLabel0,
    const char * szLabel1,
    SG_file * pFileStdout, SG_file * pFileStderr,
    SG_bool * pbFoundChanges)
{
    SG_diff_t * pDiff = NULL;
    SG_tempfile * pTempfile = NULL;
    SG_bool eof = SG_FALSE;

    SG_ASSERT(pCtx!=NULL);
    SG_ARGCHECK_RETURN(pPathname0!=NULL || pPathname1!=NULL, pPathname1);
    SG_UNUSED(pFileStderr);

    if(pFileStdout==NULL)
    {
        SG_ERR_CHECK(  SG_tempfile__open_new(pCtx, &pTempfile)  );
        pFileStdout = pTempfile->pFile;
    }

    SG_ERR_CHECK(  SG_diff_file(pCtx, pPathname0, pPathname1, diffOptions, &pDiff)  );

    SG_ERR_CHECK(  SG_diff_file_output_unified(pCtx,
        pDiff,
        pPathname0, pPathname1,
        szLabel0, szLabel1,
        SG_DIFF_OPTION__ANY_EOL, 3, pFileStdout)  );

    if(pTempfile!=NULL)
    {
        SG_ERR_CHECK(  SG_file__seek(pCtx, pTempfile->pFile, 0)  );

        do
        {
            SG_byte buf[1024];
            SG_uint32 numBytesRead = 0;

            SG_zero(buf);
            SG_file__read(pCtx, pTempfile->pFile, sizeof(buf)-1, buf, &numBytesRead);

            if(SG_context__err_equals(pCtx, SG_ERR_EOF)||SG_context__err_equals(pCtx, SG_ERR_INCOMPLETEREAD))
            {
                SG_context__err_reset(pCtx);
                eof = SG_TRUE;
            }
            else
                SG_ERR_CHECK_CURRENT;

            // Known issue: UTF-8 characters might get split across chunk boundaries. See bug SPRAWL-605 in Jira.
            if(numBytesRead>0)
                SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s", buf)  );
        }
        while(!eof);

        SG_ERR_CHECK(  SG_tempfile__close_and_delete(pCtx, &pTempfile)  );
    }

    if(pbFoundChanges!=NULL)
        *pbFoundChanges = SG_diff_contains_diffs(pDiff);

    SG_DIFF_NULLFREE(pCtx, pDiff);

    return;
fail:
    SG_DIFF_NULLFREE(pCtx, pDiff);
    SG_ERR_IGNORE(  SG_tempfile__close_and_delete(pCtx, &pTempfile)  );
}

void SG_internalfilediff__unified(
    SG_context * pCtx,
    const SG_pathname * pPathname0,
    const SG_pathname * pPathname1,
    const char * szLabel0,
    const char * szLabel1,
    SG_file * pFileStdout, SG_file * pFileStderr,
    SG_bool * pbFoundChanges)
{
    SG_ERR_CHECK_RETURN(  _sg_internalfilediff__unified(
        pCtx,
        SG_DIFF_OPTION__ANY_EOL,
        pPathname0, pPathname1, szLabel0, szLabel1, pFileStdout, pFileStderr, pbFoundChanges)  );
}

void SG_internalfilediff__unified__ignore_whitespace(
    SG_context * pCtx,
    const SG_pathname * pPathname0,
    const SG_pathname * pPathname1,
    const char * szLabel0,
    const char * szLabel1,
    SG_file * pFileStdout, SG_file * pFileStderr,
    SG_bool * pbFoundChanges)
{
    SG_ERR_CHECK_RETURN(  _sg_internalfilediff__unified(
        pCtx,
        SG_DIFF_OPTION__ANY_EOL|SG_DIFF_OPTION__IGNORE_WHITESPACE,
        pPathname0, pPathname1, szLabel0, szLabel1, pFileStdout, pFileStderr, pbFoundChanges)  );
}

void SG_internalfilediff__unified__ignore_case(
    SG_context * pCtx,
    const SG_pathname * pPathname0,
    const SG_pathname * pPathname1,
    const char * szLabel0,
    const char * szLabel1,
    SG_file * pFileStdout, SG_file * pFileStderr,
    SG_bool * pbFoundChanges)
{
    SG_ERR_CHECK_RETURN(  _sg_internalfilediff__unified(
        pCtx,
        SG_DIFF_OPTION__ANY_EOL|SG_DIFF_OPTION__IGNORE_CASE,
        pPathname0, pPathname1, szLabel0, szLabel1, pFileStdout, pFileStderr, pbFoundChanges)  );
}

void SG_internalfilediff__unified__ignore_whitespace_and_case(
    SG_context * pCtx,
    const SG_pathname * pPathname0,
    const SG_pathname * pPathname1,
    const char * szLabel0,
    const char * szLabel1,
    SG_file * pFileStdout, SG_file * pFileStderr,
    SG_bool * pbFoundChanges)
{
    SG_ERR_CHECK_RETURN(  _sg_internalfilediff__unified(
        pCtx,
        SG_DIFF_OPTION__ANY_EOL|SG_DIFF_OPTION__IGNORE_WHITESPACE|SG_DIFF_OPTION__IGNORE_CASE,
        pPathname0, pPathname1, szLabel0, szLabel1, pFileStdout, pFileStderr, pbFoundChanges)  );
}
