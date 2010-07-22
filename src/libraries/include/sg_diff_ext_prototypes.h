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
 * @file sg_diff_ext_prototypes.h
 *
 * @details Routines to run a diff in a subordinate process.  For example,
 * exec'ing gnu-diff or exec'ing SGDM in batch-mode.
 *
 * TODO later add a user-configurable option to let them specify the name
 * of the exe and the command line args (with token substitution) and all
 * that stuff.  This would be in addition to the builtin methods.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_DIFF_EXT_PROTOTYPES_H
#define H_SG_DIFF_EXT_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
/**
 * Exec gnu-diff in the background on the named files.
 *
 * On MAC and LINUX we assume gnu-diff is in "/usr/bin/diff" and
 * use a hard-coded pathname to it.  This is a no-frills-should-
 * always-work thing.  I'm specifically not going to search the
 * user's path or look in /usr/local/bin or /bin or any of that.
 *
 * If either input pathname is NULL, we substitute something
 * like /dev/null so that gnu-diff will do the --new-file thing.
 *
 * The labels will be passed to gnu-diff for the usual ---/+++
 * unified format header lines.  We always pass these because
 * the pathnames are probably always temp files (at least n the
 * case of a changeset-vs-changeset diff) and the pathnames and
 * datestamps are meaningless.
 *
 * Bind STDOUT of the child process to the open file given.
 * Ensure that it is open for writing.
 * Output will written at the current seek position.
 * The seek pointer will be left at the end of the file.
 * (So, you can call this routine with a list of file pairs
 * and have all the output concatenated like in a patchfile.)
 *
 * If pFileOutput is NULL, the child process will write to OUR STDOUT.
 *
 * If the external tool has an error, we return SG_ERR_EXTERNAL_TOOL_ERROR.
 * Otherwise, we set pbFoundChanges if it reported differences -- THIS IS
 * NOT RELIABLE -- IT DEPENDS UPON THE EXIT STATUS OF THE CHILD.
 *
 * NOTE: I consider this to be a builtin diff-provider with a known set
 * of arguments and etc.  No user-config required.  (Since we haven't
 * written the user-profile/-config stuff yet. :-)  This method has
 * all the arguments hard-coded to something sane.
 */
SG_diff_ext__compare_callback SG_diff_ext__gnu_diff;
#endif

//////////////////////////////////////////////////////////////////

#if 0  // not yet
/**
 * TODO this needs to wait until i update SGDM to use the -t1 and -t2
 * TODO arguments for batch mode.
 *
 * TODO on Windows, SGDM doesn't have STDOUT (because it is a WIN32GUI
 * TODO application and STDOUT gets closed before WinMain() gets called).
 * TODO so i need to revisit this API a bit.
 *
 * Exec SGDM in the background in batch-mode on the named files.
 *
 * Usage matches that of the builtin gnu-diff method described above.
 *
 * Since the installation of SGDM varies (more so than gnu-diff) we
 * will do a little searching for the exe.
 */
SG_diff_ext__compare_callback SG_diff_ext__sgdm;
#endif // not yet

//////////////////////////////////////////////////////////////////

/**
 * Do a diff of the version control tree using an external tool.
 *
 * This is similar to "bzr diff" or "git diff" that diffs
 * everything that has changed between either the 2 changesets
 * or between an arbitrary changeset or the baseline and the
 * working-directory.
 *
 * If pfnCB is null, we assume SG_internal_file_diff__unified.
 *
 * The outputs of each individual file diff are concatenated
 * to the pFileStdout along with patchfile-like headers.
 */
void SG_diff_ext__diff(SG_context * pCtx,
					   SG_treediff2 * pTreeDiff,
					   SG_diff_ext__compare_callback * pfnCB,
					   SG_file * pFileStdout, SG_file * pFileStderr);


SG_diff_ext__compare_callback SG_internalfilediff__unified;
SG_diff_ext__compare_callback SG_internalfilediff__unified__ignore_whitespace;
SG_diff_ext__compare_callback SG_internalfilediff__unified__ignore_case;
SG_diff_ext__compare_callback SG_internalfilediff__unified__ignore_whitespace_and_case;


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_DIFF_EXT_PROTOTYPES_H
