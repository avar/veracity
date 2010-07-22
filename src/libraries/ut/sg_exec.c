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
 * @file sg_exec.c
 *
 * @details Routines for shelling-out or otherwise executing a
 * child or subordinate process.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#if defined(MAC) || defined(LINUX)
#include <unistd.h>
#include <sys/wait.h>
#endif

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#	define TRACE_EXEC			0
#else
#	define TRACE_EXEC			0
#endif

//////////////////////////////////////////////////////////////////

void _sg_exec__get_last(SG_context* pCtx, const char * sz, const char ** ppResult)
{
	const char * pResult = NULL;
	SG_NULLARGCHECK_RETURN(sz);
	SG_NULLARGCHECK_RETURN(ppResult);
	pResult = sz;
	while(*sz!='\0')
	{
		if('/'==*sz || '\\'==*sz)
			pResult = sz+1;
		sz += 1;
	}
	*ppResult = pResult;
}

void _sg_exec__get_last__string(SG_context* pCtx, const char * sz, SG_string ** ppResult)
{
	SG_ERR_CHECK_RETURN(  _sg_exec__get_last(pCtx, sz, &sz)  );
	SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC__BUF_LEN(pCtx, ppResult, (const SG_byte*)sz, strlen(sz))  );
}

#if defined(MAC) || defined(LINUX)
/**
 * Run the child process of synchronous fork/exec.
 * Our caller has already forked.  We are called to
 * do whatever file descriptor table juggling is required
 * and to exec the given command.
 *
 * THIS FUNCTION DOES NOT RETURN.  IT EITHER EXEC() THE
 * GIVEN COMMAND OR IT EXITS WITH AN EXIT STATUS.
 *
 * WE ARE GIVEN A SG_CONTEXT because we need to have one
 * for the routines that we call, but we cannot return a
 * stacktrace to our caller because we do not return.
 */
void _sync__run_child__files(
	SG_context* pCtx,
	const char * pCmd,
	const char ** pArgv,
	SG_file * pFileStdIn,
	SG_file * pFileStdOut,
	SG_file * pFileStdErr)
{
	int status;
	int fd;
	SG_bool bExecAttempted = SG_FALSE;

	if (pFileStdIn)
	{
		SG_ERR_CHECK(  SG_file__get_fd(pCtx,pFileStdIn,&fd)  );
		(void)close(STDIN_FILENO);
		status = dup2(fd,STDIN_FILENO);
		if (status == -1)
			SG_ERR_THROW(  SG_ERR_ERRNO(errno)  );
	}
	else
	{
		// TODO i'm thinking we should let the subordinate process borrow the
		// TODO STDIN of the parent, but i don't have a good reason to say that.
		// TODO
		// TODO Decide if we like this behavior.  Note that if we are in a GUI
		// TODO application, this may be of little use.
	}

	if (pFileStdOut)
	{
		SG_ERR_CHECK(  SG_file__get_fd(pCtx,pFileStdOut,&fd)  );
		(void)close(STDOUT_FILENO);
		status = dup2(fd,STDOUT_FILENO);
		if (status == -1)
			SG_ERR_THROW(  SG_ERR_ERRNO(errno)  );
	}
	else
	{
		// TODO again, i'm going to let the subordinate process borrow the parent's
		// TODO STDOUT.  This might be useful for our command-line client.
	}

	if (pFileStdErr)
	{
		SG_ERR_CHECK(  SG_file__get_fd(pCtx,pFileStdErr,&fd)  );
		(void)close(STDERR_FILENO);
		status = dup2(fd,STDERR_FILENO);
		if (status == -1)
			SG_ERR_THROW(  SG_ERR_ERRNO(errno)  );
	}
	else
	{
		// TODO again, i'm going to let the subordinate process borrow the parent's
		// TODO STDERR.  This might be useful for our command-line client.
	}

	// close all other files (including the other copy of the descriptors
	// that we just dup'd).  this prevents external applications from mucking
	// with any databases that the parent may have open....

	for (fd=STDERR_FILENO+1; fd < (int)FD_SETSIZE; fd++)
		close(fd);

	//////////////////////////////////////////////////////////////////

	execvp( pCmd, (char * const *)pArgv );

	//////////////////////////////////////////////////////////////////

	bExecAttempted = SG_TRUE;	// if we get here, exec() failed.
	SG_ERR_THROW(  SG_ERR_ERRNO(errno)  );

	// fall thru and print something about why the child could not be started.

fail:
#if TRACE_EXEC
	{
		SG_string * pStrErr = NULL;

		SG_context__err_to_string(pCtx,&pStrErr);	// IT IS IMPORTANT THAT THIS NOT CALL SG_ERR_IGNORE() because it will hide the info we are reporting.

		// TODO 2010/05/13 We've alredy closed STDERR in the child process, so
		// TODO            this message isn't really going anywhere....

		SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
								   ("execvp() %s:\n"
									"\tCommand: %s\n"
									"\tErrorInfo:\n"
									"%s\n"),
								   ((bExecAttempted)
									? "returned with error"
									: "not attempted because of earlier errors in child"),
								   pCmd,
								   SG_string__sz(pStrErr))  );
		SG_STRING_NULLFREE(pCtx, pStrErr);
		fflush(stderr);
	}
#endif

	// we we the child.  use _exit() so that none of the atexit() stuff gets run.
	// for example, on Mac/Linux this could cause the debug memory leak report
	// to run and dump a bogus report to stderr.

	_exit(-1);
}

static void _sync__run_parent__files(SG_context* pCtx,
									 int pid,
									 SG_exit_status * pChildExitStatus)
{
	int my_errno;
	int statusChild = 0;
	int result;

	//////////////////////////////////////////////////////////////////
	// HACK There is a problem just calling waitpid() on the Mac when
	// HACK in gdb.  We frequently get an interrupted system call.
	// HACK This causes us to abort everything.  I've think I've also
	// HACK seen this from a normal shell.  I think the problem is a
	// HACK race of some kind.
	// HACK
	// HACK The following is an attempt to spin a little until the child
	// HACK gets going enough so that we can actuall wait on it.  I think.
	//
	// TODO consider adding a counter or clock to this so that we don't
	// TODO get stuck here infinitely.  I don't think it'll happen, but
	// TODO it doesn't hurt....
	//////////////////////////////////////////////////////////////////

	while (1)
	{
		result = waitpid(pid,&statusChild,0);
		if (result == pid)
			goto done_waiting;

		my_errno = errno;

#if TRACE_EXEC
		SG_ERR_IGNORE(  SG_console(pCtx,
								   SG_CS_STDERR,
								   "waitpid: [result %d][errno %d]\n",
								   result,my_errno)  );
#endif

		switch (my_errno)
		{
		default:
		case ECHILD:		// we don't have a child
			SG_ERR_THROW_RETURN(  SG_ERR_ERRNO(my_errno)  );

		case EDEADLK:		// resource deadlock avoided
		case EAGAIN:		// resource temporarily unavailable
		case EINTR:			// interrupted system call
			break;			// try again
		}
	}

done_waiting:

#if TRACE_EXEC
	SG_ERR_IGNORE(  SG_console(pCtx,
							   SG_CS_STDERR,
							   "waitpid: child status [0x%x] [exited %x][signaled %x][stopped %x] [?exit? %d] [?term? %d] [?stop? %d]\n",
							   statusChild,
							   WIFEXITED(statusChild),WIFSIGNALED(statusChild),WIFSTOPPED(statusChild),
							   ((WIFEXITED(statusChild)) ? WEXITSTATUS(statusChild) : -1),
							   ((WIFSIGNALED(statusChild)) ? WTERMSIG(statusChild) : -1),
							   ((WIFSTOPPED(statusChild)) ? WSTOPSIG(statusChild) : -1))  );
#endif

	if (!WIFEXITED(statusChild))
		SG_ERR_THROW_RETURN(  SG_ERR_ABNORMAL_TERMINATION  );	// WTERMSIG() or WCOREDUMP() or ...

	// child exited normally (with or without error)

	*pChildExitStatus = WEXITSTATUS(statusChild);
}

void SG_exec__exec_async__files(
	SG_context* pCtx,
	const char * pCmd,
	const SG_exec_argvec * pArgVec,
	SG_file * pFileStdIn,
	SG_file * pFileStdOut,
	SG_file * pFileStdErr,
	SG_process_id * pProcessID)
{
	// The caller passes a SG_vector of SG_string for ARGV.
	// They DO NOT include the program-to-exec in the vector.

	SG_uint32 nrArgsGiven = 0;
	SG_uint32 nrArgsTotal;
	SG_uint32 kArg;
	const char ** pArgv = NULL;
	const char * pStrCmdEntryname = NULL;
	int pid;

	SG_NULLARGCHECK_RETURN(pCmd);
	SG_NULLARGCHECK_RETURN(pProcessID);

	if (pArgVec)
		SG_ERR_CHECK_RETURN(  SG_exec_argvec__length(pCtx,pArgVec,&nrArgsGiven)  );
	nrArgsTotal = nrArgsGiven + 2;		// +1 for program, +1 for terminating null.

	// We build a platform-appropriate "char * argv[]" to give to execv().

	SG_ERR_CHECK_RETURN(  SG_alloc(pCtx,nrArgsTotal,sizeof(char *),&pArgv)  );

	// On Linux/Mac, we want argv[0] to be the ENTRYNAME of the program.

	// TODO I'm thinking we should convert each arg from utf-8 to an os-buffer
	// TODO aka locale buffer and give that to exec().

	SG_ERR_CHECK(  _sg_exec__get_last(pCtx,pCmd,&pStrCmdEntryname)  );
	pArgv[0] = pStrCmdEntryname;

	for (kArg=0; kArg<nrArgsGiven; kArg++)
	{
		const SG_string * pStr_k;

		SG_ERR_CHECK(  SG_exec_argvec__get(pCtx,pArgVec,kArg,&pStr_k)  );
		if (!pStr_k || (SG_string__length_in_bytes(pStr_k) == 0))
		{
			// allow for blank or empty cell as a "" entry.  (though,
			// i'm not sure why.)

			pArgv[kArg+1] = "";
		}
		else
		{
			// TODO utf-8 --> os-buffer ??
			pArgv[kArg+1] = SG_string__sz(pStr_k);
		}
	}

	// pArgv[nrArgsTotal-1] should already be NULL

#if TRACE_EXEC
	{
		SG_uint32 k;
		SG_ERR_IGNORE(  SG_console(pCtx,
								   SG_CS_STDERR,
								   "Exec [%s]\n",
								   pCmd)  );
		for (k=0; k<nrArgsTotal; k++)
		{
			SG_ERR_IGNORE(  SG_console(pCtx,
									   SG_CS_STDERR,
									   "     Argv[%d] %s\n",
									   k,pArgv[k])  );
		}
	}
#endif

	// just incase we have anything buffered in our output FILE streams
	// (and the child is sharing them), flush them out before the child
	// gets started.

	fflush(stdout);
	fflush(stderr);

	pid = fork();
	if (pid == -1)
	{
		SG_ERR_THROW(  SG_ERR_ERRNO(errno)  );
	}
	else if (pid == 0)
	{
		// we are in the child process.
		//
		// note that we ***CANNOT*** use SG_ERR_CHECK() and the normal
		// goto fail, cleanup, and return -- whether we succeed or fail,
		// we must exit when we are finished.

		_sync__run_child__files(pCtx,pCmd,pArgv,pFileStdIn,pFileStdOut,pFileStdErr);
		_exit(-1);	// not reached
		return;		// not reached
	}

	// otherwise, we are the parent process.
	//For now, do nothing.
	*pProcessID = (SG_process_id)pid;

	// fall thru to common cleanup.

fail:
	SG_NULLFREE(pCtx, pArgv);
}

void SG_exec__exec_sync__files(
	SG_context* pCtx,
	const char * pCmd,
	const SG_exec_argvec * pArgVec,
	SG_file * pFileStdIn,
	SG_file * pFileStdOut,
	SG_file * pFileStdErr,
	SG_exit_status * pChildExitStatus)
{
	// The caller passes a SG_vector of SG_string for ARGV.
	// They DO NOT include the program-to-exec in the vector.

	SG_uint32 nrArgsGiven = 0;
	SG_uint32 nrArgsTotal;
	SG_uint32 kArg;
	const char ** pArgv = NULL;
	const char * pStrCmdEntryname = NULL;
	int pid;
	SG_exit_status exitStatus = 0;

	SG_NULLARGCHECK_RETURN(pCmd);
	SG_NULLARGCHECK_RETURN(pChildExitStatus);

	if (pArgVec)
		SG_ERR_CHECK_RETURN(  SG_exec_argvec__length(pCtx,pArgVec,&nrArgsGiven)  );
	nrArgsTotal = nrArgsGiven + 2;		// +1 for program, +1 for terminating null.

	// We build a platform-appropriate "char * argv[]" to give to execv().

	SG_ERR_CHECK_RETURN(  SG_alloc(pCtx,nrArgsTotal,sizeof(char *),&pArgv)  );

	// On Linux/Mac, we want argv[0] to be the ENTRYNAME of the program.

	// TODO I'm thinking we should convert each arg from utf-8 to an os-buffer
	// TODO aka locale buffer and give that to exec().

	SG_ERR_CHECK(  _sg_exec__get_last(pCtx,pCmd,&pStrCmdEntryname)  );
	pArgv[0] = pStrCmdEntryname;

	for (kArg=0; kArg<nrArgsGiven; kArg++)
	{
		const SG_string * pStr_k;

		SG_ERR_CHECK(  SG_exec_argvec__get(pCtx,pArgVec,kArg,&pStr_k)  );
		if (!pStr_k || (SG_string__length_in_bytes(pStr_k) == 0))
		{
			// allow for blank or empty cell as a "" entry.  (though,
			// i'm not sure why.)

			pArgv[kArg+1] = "";
		}
		else
		{
			// TODO utf-8 --> os-buffer ??
			pArgv[kArg+1] = SG_string__sz(pStr_k);
		}
	}

	// pArgv[nrArgsTotal-1] should already be NULL

#if TRACE_EXEC
	{
		SG_uint32 k;
		SG_ERR_IGNORE(  SG_console(pCtx,
								   SG_CS_STDERR,
								   "Exec [%s]\n",
								   pCmd)  );
		for (k=0; k<nrArgsTotal; k++)
		{
			SG_ERR_IGNORE(  SG_console(pCtx,
									   SG_CS_STDERR,
									   "     Argv[%d] %s\n",
									   k,pArgv[k])  );
		}
	}
#endif

	// just incase we have anything buffered in our output FILE streams
	// (and the child is sharing them), flush them out before the child
	// gets started.

	fflush(stdout);
	fflush(stderr);

	pid = fork();
	if (pid == -1)
	{
		SG_ERR_THROW(  SG_ERR_ERRNO(errno)  );
	}
	else if (pid == 0)
	{
		// we are in the child process.
		//
		// note that we ***CANNOT*** use SG_ERR_CHECK() and the normal
		// goto fail, cleanup, and return -- whether we succeed or fail,
		// we must exit when we are finished.

		_sync__run_child__files(pCtx,pCmd,pArgv,pFileStdIn,pFileStdOut,pFileStdErr);
		_exit(-1);	// not reached
		return;		// not reached
	}

	// otherwise, we are the parent process.

	SG_ERR_CHECK(  _sync__run_parent__files(pCtx,pid,&exitStatus)  );
#if 1
	// TODO 2010/05/13 This is causing a few problems for vscript.
	// TODO            Lets take this out.
	// TODO
	// TODO            Lets try using the rule:
	// TODO            [] if have an normal setup/argcheck error and don't get
	// TODO               to the point where we try to start the child, we
	// TODO               throw just like any other sglib function.
	// TODO
	// TODO            [] if the child gets killed/signalled/fails to start
	// TODO               or otherwise doesn't get to exit normally, we throw
	// TODO               abnormal-termination --OR-- return in a second status
	// TODO               arg { exited, signaled, ....}.  I'm not sure which is
	// TODO               more annoying or useful.
	// TODO
	// TODO            [] if the child manages to return an exit status
	// TODO               we return it in the arg.
	// TODO
	// TODO            TODO Confirm that we do something similar on Windows?
	if (exitStatus == 255)
	{
		// this usually means that the child could not exec the program
		// for whatever reason, such as no access to the exe or the exe
		// did not exist.
		//
		// i want this to be handled differently than the regular child
		// exited with an error.  i mean, if /bin/diff finds differences
		// it exits with 1 vs 0 when identical.  those outcomes are different
		// then not being able to find /bin/diff.
		//
		// we can't tell at this point if it was a EACCESS or a ENOENT.

		SG_ERR_THROW2(  SG_ERR_EXEC_FAILED,
						(pCtx,
						 "Cannot execute command [%s]",
						 pCmd)  );
	}
#endif

	*pChildExitStatus = (SG_exit_status)exitStatus;

	// fall thru to common cleanup.

fail:
	SG_NULLFREE(pCtx, pArgv);
}
#endif // defined(MAC) || defined(LINUX)

//////////////////////////////////////////////////////////////////

#if defined(WINDOWS)

void _sg_exec__append_arg(
	SG_context * pCtx,
	SG_string * pStrCmdLine,
	const SG_string * pStr_k)
{
	SG_string * pStr = NULL;

	if (!pStr_k || (SG_string__length_in_bytes(pStr_k) == 0))
	{
		SG_ERR_CHECK(  SG_string__append__sz(pCtx,pStrCmdLine,"\"\"")  );
	}
	else
	{
		SG_ERR_CHECK(  SG_string__append__sz(pCtx,pStrCmdLine,"\"")  );

		SG_ERR_CHECK(  SG_STRING__ALLOC__COPY(pCtx, &pStr, pStr_k)  );
		SG_ERR_CHECK(  SG_string__replace_bytes(pCtx, pStr,
			(const SG_byte*)"\"", 1,
			(const SG_byte*)"\\\"", 2,
			SG_UINT32_MAX, SG_TRUE, NULL)  );
		SG_ERR_CHECK(  SG_string__append__string(pCtx,pStrCmdLine,pStr)  );
		SG_STRING_NULLFREE(pCtx, pStr);

		SG_ERR_CHECK(  SG_string__append__sz(pCtx,pStrCmdLine,"\"")  );
	}

	SG_ERR_CHECK(  SG_string__append__sz(pCtx,pStrCmdLine," ")  );

fail:
	SG_STRING_NULLFREE(pCtx, pStr);
}

void SG_exec__exec_async__files(
	SG_context* pCtx,
	const char * pCmd,
	const SG_exec_argvec * pArgVec,
	SG_file * pFileStdIn,
	SG_file * pFileStdOut,
	SG_file * pFileStdErr,
	SG_process_id * pProcessID)
{
	// The caller passes a SG_vector of SG_string for ARGV.
	// They DO NOT include the program-to-exec in the vector.

	SG_string * pStrCmdLine = NULL;
	SG_string * pCmd__string = NULL;
	wchar_t * pWcharCmdLine = NULL;
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
	DWORD dwCreationFlags;
	//DWORD dwChildExitStatus;
	HANDLE hFileStdIn, hFileStdOut, hFileStdErr;

	SG_NULLARGCHECK_RETURN(pCmd);
	SG_NULLARGCHECK_RETURN(pProcessID);

	memset(&pi,0,sizeof(pi));
	memset(&si,0,sizeof(si));

	SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC(pCtx, &pStrCmdLine)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pCmd__string, pCmd)  );
	SG_ERR_CHECK(  _sg_exec__append_arg(pCtx, pStrCmdLine, pCmd__string)  );

	if (pArgVec)
	{
		SG_uint32 kArg, nrArgsGiven;
		SG_ERR_CHECK(  SG_exec_argvec__length(pCtx,pArgVec,&nrArgsGiven)  );
		for (kArg=0; kArg<nrArgsGiven; kArg++)
		{
			const SG_string * pStr_k = NULL;
			SG_ERR_CHECK(  SG_exec_argvec__get(pCtx,pArgVec,kArg,&pStr_k)  );
			SG_ERR_CHECK(  _sg_exec__append_arg(pCtx, pStrCmdLine, pStr_k)  );
		}
	}

#if TRACE_EXEC
	SG_ERR_IGNORE(  SG_console(pCtx,
							   SG_CS_STDERR,
							   "Exec [%s]\n",	// this is close; it won't have the UNC prefix
							   pCmd)  );
	SG_ERR_IGNORE(  SG_console(pCtx,
							   SG_CS_STDERR,
							   "    Args: [%s]\n",
							   SG_string__sz(pStrCmdLine))  );
#endif

	// convert UTF-8 version of the command pathname and the command line to wchar_t.
	SG_ERR_CHECK(  SG_utf8__extern_to_os_buffer__wchar(pCtx,SG_string__sz(pStrCmdLine),&pWcharCmdLine,NULL)  );

    si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;

	// deal with STDIN/STDOUT/STDERR in the child/subordinate process.
	//
	// if the caller gave us open files, we use the file handles within
	// them; otherwise, we clone our STDIN and friends.
	// we must assume that the files provided by the caller are correctly
	// opened and positioned....

	if (pFileStdIn)
		SG_ERR_CHECK(  SG_file__get_handle(pCtx,pFileStdIn,&hFileStdIn)  );
	else
		hFileStdIn = GetStdHandle(STD_INPUT_HANDLE);

	if (pFileStdOut)
		SG_ERR_CHECK(  SG_file__get_handle(pCtx,pFileStdOut,&hFileStdOut)  );
	else
		hFileStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	if (pFileStdErr)
		SG_ERR_CHECK(  SG_file__get_handle(pCtx,pFileStdErr,&hFileStdErr)  );
	else
		hFileStdErr = GetStdHandle(STD_ERROR_HANDLE);

	if (!DuplicateHandle(GetCurrentProcess(),hFileStdIn,GetCurrentProcess(),
						 &si.hStdInput,0,TRUE,DUPLICATE_SAME_ACCESS))
		SG_ERR_THROW(  SG_ERR_GETLASTERROR(GetLastError())  );

	if (!DuplicateHandle(GetCurrentProcess(),hFileStdOut,GetCurrentProcess(),
						 &si.hStdOutput,0,TRUE,DUPLICATE_SAME_ACCESS))
		SG_ERR_THROW(  SG_ERR_GETLASTERROR(GetLastError())  );

	if (!DuplicateHandle(GetCurrentProcess(),hFileStdErr,GetCurrentProcess(),
						 &si.hStdError,0,TRUE,DUPLICATE_SAME_ACCESS))
		SG_ERR_THROW(  SG_ERR_GETLASTERROR(GetLastError())  );

	dwCreationFlags = 0;
	if (!CreateProcessW(NULL,pWcharCmdLine,
						NULL,NULL,
						TRUE,dwCreationFlags,
						NULL,NULL,
						&si,&pi))
		SG_ERR_THROW2(  SG_ERR_GETLASTERROR(GetLastError()),
						(pCtx,
						 "Cannot execute command [%s] with args [%s]",
						 pCmd,
						 SG_string__sz(pStrCmdLine))  );

	*pProcessID = (SG_process_id) pi.dwProcessId;
	// fall thru to common cleanup.

fail:

	CloseHandle(pi.hProcess);

	SG_STRING_NULLFREE(pCtx, pStrCmdLine);
	SG_STRING_NULLFREE(pCtx, pCmd__string);
	SG_NULLFREE(pCtx, pWcharCmdLine);

	CloseHandle(si.hStdInput);
	CloseHandle(si.hStdOutput);
	CloseHandle(si.hStdError);
}


void SG_exec__exec_sync__files(
	SG_context* pCtx,
	const char * pCmd,
	const SG_exec_argvec * pArgVec,
	SG_file * pFileStdIn,
	SG_file * pFileStdOut,
	SG_file * pFileStdErr,
	SG_exit_status * pChildExitStatus)
{
	// The caller passes a SG_vector of SG_string for ARGV.
	// They DO NOT include the program-to-exec in the vector.

	SG_string * pStrCmdLine = NULL;
	SG_string * pCmd__string = NULL;
	wchar_t * pWcharCmdLine = NULL;
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
	DWORD dwCreationFlags;
	DWORD dwChildExitStatus;
	HANDLE hFileStdIn, hFileStdOut, hFileStdErr;

	SG_NULLARGCHECK_RETURN(pCmd);
	SG_NULLARGCHECK_RETURN(pChildExitStatus);

	memset(&pi,0,sizeof(pi));
	memset(&si,0,sizeof(si));

	SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC(pCtx, &pStrCmdLine)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pCmd__string, pCmd)  );
	SG_ERR_CHECK(  _sg_exec__append_arg(pCtx, pStrCmdLine, pCmd__string)  );

	if (pArgVec)
	{
		SG_uint32 kArg, nrArgsGiven;
		SG_ERR_CHECK(  SG_exec_argvec__length(pCtx,pArgVec,&nrArgsGiven)  );
		for (kArg=0; kArg<nrArgsGiven; kArg++)
		{
			const SG_string * pStr_k = NULL;
			SG_ERR_CHECK(  SG_exec_argvec__get(pCtx,pArgVec,kArg,&pStr_k)  );
			SG_ERR_CHECK(  _sg_exec__append_arg(pCtx, pStrCmdLine, pStr_k)  );
		}
	}

#if TRACE_EXEC
	SG_ERR_IGNORE(  SG_console(pCtx,
							   SG_CS_STDERR,
							   "Exec [%s]\n",	// this is close; it won't have the UNC prefix
							   pCmd)  );
	SG_ERR_IGNORE(  SG_console(pCtx,
							   SG_CS_STDERR,
							   "    Args: [%s]\n",
							   SG_string__sz(pStrCmdLine))  );
#endif

	// convert UTF-8 version of the command pathname and the command line to wchar_t.
	SG_ERR_CHECK(  SG_utf8__extern_to_os_buffer__wchar(pCtx,SG_string__sz(pStrCmdLine),&pWcharCmdLine,NULL)  );

    si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;

	// deal with STDIN/STDOUT/STDERR in the child/subordinate process.
	//
	// if the caller gave us open files, we use the file handles within
	// them; otherwise, we clone our STDIN and friends.
	// we must assume that the files provided by the caller are correctly
	// opened and positioned....

	if (pFileStdIn)
		SG_ERR_CHECK(  SG_file__get_handle(pCtx,pFileStdIn,&hFileStdIn)  );
	else
		hFileStdIn = GetStdHandle(STD_INPUT_HANDLE);

	if (pFileStdOut)
		SG_ERR_CHECK(  SG_file__get_handle(pCtx,pFileStdOut,&hFileStdOut)  );
	else
		hFileStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	if (pFileStdErr)
		SG_ERR_CHECK(  SG_file__get_handle(pCtx,pFileStdErr,&hFileStdErr)  );
	else
		hFileStdErr = GetStdHandle(STD_ERROR_HANDLE);

	if (!DuplicateHandle(GetCurrentProcess(),hFileStdIn,GetCurrentProcess(),
						 &si.hStdInput,0,TRUE,DUPLICATE_SAME_ACCESS))
		SG_ERR_THROW(  SG_ERR_GETLASTERROR(GetLastError())  );

	if (!DuplicateHandle(GetCurrentProcess(),hFileStdOut,GetCurrentProcess(),
						 &si.hStdOutput,0,TRUE,DUPLICATE_SAME_ACCESS))
		SG_ERR_THROW(  SG_ERR_GETLASTERROR(GetLastError())  );

	if (!DuplicateHandle(GetCurrentProcess(),hFileStdErr,GetCurrentProcess(),
						 &si.hStdError,0,TRUE,DUPLICATE_SAME_ACCESS))
		SG_ERR_THROW(  SG_ERR_GETLASTERROR(GetLastError())  );

	dwCreationFlags = 0;
	if (!CreateProcessW(NULL,pWcharCmdLine,
						NULL,NULL,
						TRUE,dwCreationFlags,
						NULL,NULL,
						&si,&pi))
		SG_ERR_THROW2(  SG_ERR_GETLASTERROR(GetLastError()),
						(pCtx,
						 "Cannot execute command [%s] with args [%s]",
						 pCmd,
						 SG_string__sz(pStrCmdLine))  );

	if (WaitForSingleObject(pi.hProcess,INFINITE) != WAIT_OBJECT_0)
		SG_ERR_THROW(  SG_ERR_GETLASTERROR(GetLastError())  );

	if (!GetExitCodeProcess(pi.hProcess,&dwChildExitStatus))
		SG_ERR_THROW(  SG_ERR_GETLASTERROR(GetLastError())  );

	*pChildExitStatus = (SG_exit_status)dwChildExitStatus;

	// fall thru to common cleanup.

fail:

	CloseHandle(pi.hProcess);

	SG_STRING_NULLFREE(pCtx, pStrCmdLine);
	SG_STRING_NULLFREE(pCtx, pCmd__string);
	SG_NULLFREE(pCtx, pWcharCmdLine);

	CloseHandle(si.hStdInput);
	CloseHandle(si.hStdOutput);
	CloseHandle(si.hStdError);
}
#endif

