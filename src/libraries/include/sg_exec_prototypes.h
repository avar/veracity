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
 * @file sg_exec_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_EXEC_PROTOTYPES_H
#define H_SG_EXEC_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Exec the given command with the given arguments in a subordinate
 * process.  That is, we fork and exec it.
 *
 * pArgVec contains a vector of SG_string pointers; one for each
 * argument.  This vector DOES NOT contain the program name in [0].
 * We will take care of that if it is appropriate for the platform
 * when we build the "char ** argv".
 *
 * pArgVec is optional, if you don't have any arguments, you
 * may pass NULL.
 *
 * pFileStdIn, pFileStdOut, and pFileStdErr point to opened files that
 * will be bound to the STDIN, STDOUT, and STDERR of the subordinate process.
 * These files should be opened for reading/writing as is
 * appropriate and positioned correctly.
 *
 * If these are NULL, the child will borrow the STDIN, STDOUT, and STDERR
 * of the current process (if that is possible on this platform).
 * NOTE: setting these to NULL may not make sense in a GUI app
 * since there probably isn't anyone watching them.
 *
 * If the child process is successfully started and it terminates
 * normally (with or without error), our return value is SG_ERR_OK
 * and the child's exit status is returned in pErrChildExitStatus.
 *
 * If we cannot fork(), the child cannot be started with exec(),
 * or the child abnormally terminates (gets signaled, dumps core,
 * etc), we return an error code (and we do not set the child's
 * exit status).
 */
void SG_exec__exec_sync__files(
	SG_context* pCtx,
	const char * pCmd,
	const SG_exec_argvec * pArgVec,
	SG_file * pFileStdIn,
	SG_file * pFileStdOut,
	SG_file * pFileStdErr,
	SG_exit_status * pChildExitStatus);

/**
 * Exec the given command with the given arguments in a background
 * process.  That is, we fork and exec it.  We do not wait for the return status.
 *
 * pArgVec contains a vector of SG_string pointers; one for each
 * argument.  This vector DOES NOT contain the program name in [0].
 * We will take care of that if it is appropriate for the platform
 * when we build the "char ** argv".
 *
 * pArgVec is optional, if you don't have any arguments, you
 * may pass NULL.
 *
 * pFileStdIn, pFileStdOut, and pFileStdErr point to opened files that
 * will be bound to the STDIN, STDOUT, and STDERR of the subordinate process.
 * These files should be opened for reading/writing as is
 * appropriate and positioned correctly.
 *
 * If these are NULL, the child will borrow the STDIN, STDOUT, and STDERR
 * of the current process (if that is possible on this platform).
 * NOTE: setting these to NULL may not make sense in a GUI app
 * since there probably isn't anyone watching them.
 *
 * If the child process is successfully started and it terminates
 * normally (with or without error), our return value is SG_ERR_OK
 * and the child's exit status is returned in pErrChildExitStatus.
 *
 * If we cannot fork(), the child cannot be started with exec(),
 * or the child abnormally terminates (gets signaled, dumps core,
 * etc), we return an error code (and we do not set the child's
 * exit status).
 */
void SG_exec__exec_async__files(
	SG_context* pCtx,
	const char * pCmd,
	const SG_exec_argvec * pArgVec,
	SG_file * pFileStdIn,
	SG_file * pFileStdOut,
	SG_file * pFileStdErr,
	SG_process_id * pProcessID);
//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_EXEC_PROTOTYPES_H
