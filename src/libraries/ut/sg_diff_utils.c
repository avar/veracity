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
 * @file sg_diff_utils.c
 *
 * @details routines to help with external diff/merge operations.
 * These are used by sg_diff_ext.c and sg_merge_ext.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

/**
 * Create a label for one side of a UNIFIED DIFF.  This will appear on one of
 * the header lines after the "+++ " or "--- ".  (We DO NOT create the "+++ "
 * portion.)
 *
 * Gnu-diff (when --label is not used) creates something like:
 *      +++ %s\t%s		with the file's relative pathname and date-time-modified.
 *
 *      --- d1/date2	1970-01-01 00:00:00.000000000 +0000
 *      +++ d2/date2	2009-04-30 13:10:57.000000000 +0000
 *
 * BZR's diff command creates something like:
 *
 *      --- d2/date2	2009-04-30 12:38:41 +0000
 *      +++ d2/date2	2009-04-30 13:10:57 +0000
 *
 * GIT's diff command creates something like:
 *
 *      --- a/eeeeee.txt
 *      +++ b/eeeeee.txt
 *
 * So, we can pretty much do whatever we want here.
 *
 * I'm going to try the following and see if we like it:
 * [] for a historical version, print:
 *      +++ %s\t%s    with the repo-path and the HID.
 *
 *    we don't have a valid date-stamp to print. (our caller might have
 *    the date-stamp on the changeset, but that doesn't have anything to
 *    to with the date on an indivdual file (the file might not have even
 *    been modified in that changeset).
 *
 *    the HID may be useful later.
 *
 * [] for a working-directory version, print:
 *      +++ %s\t%s    with the repo-path and the live date-time-stamp.
 *
 *    since the file is in the working-directory, the date stamp has
 *    some validity (it doesn't mean that they changed the file or reflect
 *    the last change (it could be date of the last get-latest for all we
 *    know)).
 *
 * [] just for sanity, we allow a repo-path only version:
 *      +++ %s
 *
 * In all cases we print the complete repo-path "@/a/b/c/foo.c".
 * The repo-path is as computed in SG_treediff2 and reflects all pathname
 * renames/moves.
 *
 * TODO do we care about feeding this output to PATCH and how it digests
 * pathnames and with the various -p0 -p1 ... arguments?
 *
 * We return a string that you must free.
 */
void SG_diff_utils__make_label(SG_context * pCtx,
							   const char * szRepoPath,
							   const char * szHid,
							   const char * szDate,
							   SG_string ** ppStringLabel)
{
	SG_string * pString = NULL;

	SG_NONEMPTYCHECK_RETURN(szRepoPath);

	if (szHid && szDate)			// allow zero or one, but not both
		SG_ERR_THROW_RETURN(  SG_ERR_INVALIDARG  );

	SG_NULLARGCHECK(ppStringLabel);

	SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC(pCtx,&pString)  );

	//////////////////////////////////////////////////////////////////
	// WARNING: All of the input arguments are in our internal NFC UTF-8 format
	// WARNING: (as is everything).  SG_exec__() will convert whatever we construct
	// WARNING: here to an os-buffer (utf-8, locale, or wchar_t) before giving it
	// WARNING: to execvp() or CreateProcessW().
	// WARNING:
	// WARNING: I *think* this is what we want.
	// WARNING:
	// WARNING: We can't be sure how the child process will handle the label text
	// WARNING: when it prints it to its STDOUT.  This may cause some confusion.
	// WARNING: For example, NFC/NFD issues on MAC or some '?' characters getting
	// WARNING: printed by cygwin's version of gnu-diff and etc.
	// WARNING:
	// WARNING: I don't know if there are actually any issues here or not or if
	// WARNING: so whether they will cause a problem.
	//////////////////////////////////////////////////////////////////

	if (szHid)
		SG_ERR_CHECK(  SG_string__sprintf(pCtx,
										  pString,"%s\t%s",
										  szRepoPath,szHid)  );
	else if (szDate)
		SG_ERR_CHECK(  SG_string__sprintf(pCtx,
										  pString,"%s\t%s",
										  szRepoPath,szDate)  );
	else
		SG_ERR_CHECK(  SG_string__set__sz(pCtx,pString,szRepoPath)  );

	*ppStringLabel = pString;

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pString);
}

//////////////////////////////////////////////////////////////////

/**
 * Export a blob-of-interest from the REPO into a temporary directory
 * so that we can let the external diff tool compare it with another
 * version somewhere.
 *
 * We create a file with a GID-based name rather than re-creating
 * the working-directory hierarchy in the temp directory.  This lets
 * us flatten the export in one directory without collisions and
 * avoids move/rename and added/deleted sub-directory issues.
 *
 * pPathTempSessionDir should be something like "$TMPDIR/gid_session/".
 * This should be a unique directory name such that everything being
 * exported is isolated from other runs.  (if we are doing a
 * changeset-vs-changeset diff, we may create lots of files on each
 * side -- and our command should not interfere with other diffs
 * in progress in other processes.)
 *
 * szVersion should be a value to let us group everything from cset[0]
 * in a different directory from stuff from cset[1].  this might be
 * simply "0" and "1" or it might be the cset's HIDs.
 *
 * szGidObject is the gid of the object.
 *
 * szHidBlob is the HID of the content.  Normally this is the content
 * of the file that will be compared (corresponding to a user-file
 * under version control).  However, we may also want to use this
 * to splat the XATTRs to a file so that they can be compared (on
 * non-apple systems) -- but this may be too weird.
 *
 * You are responsible for freeing the returned pathname and
 * deleting the file that we create.
 */
void SG_diff_utils__export_to_temp_file(SG_context * pCtx,
										SG_repo * pRepo,
										const SG_pathname * pPathTempSessionDir,	// value of "$TMPDIR/session/"
										const char * szVersion,						// an index (like 0 or 1, _older_ _newer_) or maybe cset HID
										const char * szGidObject,
										const char * szHidBlob,
										SG_pathname ** ppPathTempFile)
{
	SG_pathname * pPathFile = NULL;
	SG_file * pFile = NULL;
	SG_bool bDirExists;

	// mkdir $TMPDIR/session/version
	// create pathname for the temp file: $TMPDIR/session/version/object_gid
	//
	// TODO do we want to append the suffix from the file to this pathname so that
	// TODO tools like SGDM can use it?  what if the file is renamed and given a
	// TODO different suffix between versions?

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pPathFile,pPathTempSessionDir,szVersion)  );
	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathFile,&bDirExists,NULL,NULL)  );
	if (!bDirExists)
		SG_ERR_CHECK(  SG_fsobj__mkdir_recursive__pathname(pCtx,pPathFile)  );
	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pPathFile,szGidObject)  );

	// open the file and copy the contents of the blob into it

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx,pPathFile,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,0600,&pFile)  );
	SG_ERR_CHECK(  SG_repo__fetch_blob_into_file(pCtx,pRepo,szHidBlob,pFile,NULL)  );
	SG_ERR_CHECK(  SG_file__close(pCtx,&pFile)  );

	*ppPathTempFile = pPathFile;
	return;

fail:
	if (pFile)				// only if **WE** created the file, do we try to delete it on an error.
	{
		SG_FILE_NULLCLOSE(pCtx,pFile);
		SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx,pPathFile)  );
	}
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}
