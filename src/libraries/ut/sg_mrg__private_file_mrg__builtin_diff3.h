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
 * @file sg_mrg__private_file_mrg__builtin_diff3.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_FILE_MRG__BUILTIN_DIFF3_H
#define H_SG_MRG__PRIVATE_FILE_MRG__BUILTIN_DIFF3_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
static void _builtin_diff3(SG_context * pCtx, SG_mrg_automerge_plan_item * pItem)
{
	SG_exit_status childExitStatus;
	SG_exec_argvec * pArgVec = NULL;
	SG_file * pFile_Output = NULL;

	SG_ERR_CHECK(  SG_exec_argvec__alloc(pCtx,&pArgVec)  );

	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,"--merge")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,"--show-all")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,"--text")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pItem->pPath_Mine    ))  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pItem->pPath_Ancestor))  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pItem->pPath_Yours   ))  );

	// Open the output-pathname for writing so that we can make it the STDOUT of the child process.
	SG_ERR_CHECK(  SG_file__open__pathname(pCtx,pItem->pPath_Result,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,0600,&pFile_Output)  );

	SG_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,"/usr/bin/diff3",pArgVec,NULL,pFile_Output,NULL,&childExitStatus)  );

	// gnu-diff3 returns:
	// 0 --> auto-merge successful
	// 1 --> auto-merge found conflicts
	// 2 --> hard errors of some kind
	//
	// SG_exec has already trapped 255's and thrown.

	switch (childExitStatus)
	{
	case 0:
		pItem->result = SG_MRG_AUTOMERGE_RESULT__SUCCESSFUL;
		break;

	case 1:
		pItem->result = SG_MRG_AUTOMERGE_RESULT__CONFLICT;
		break;

//	case 2:
	default:
		SG_ERR_THROW(  SG_ERR_EXTERNAL_TOOL_ERROR  );
	}

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile_Output);
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);
}
#endif

//////////////////////////////////////////////////////////////////

#if defined(WINDOWS)
static void _builtin_diff3(SG_context * pCtx, SG_mrg_automerge_plan_item * pItem)
{
	SG_exit_status childExitStatus;
	SG_exec_argvec * pArgVec = NULL;
	SG_file * pFile_Output = NULL;

	SG_ERR_CHECK(  SG_exec_argvec__alloc(pCtx,&pArgVec)  );

	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,"--merge")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,"--show-all")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,"--text")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pItem->pPath_Mine    ))  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pItem->pPath_Ancestor))  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pItem->pPath_Yours   ))  );


	// Open the output-pathname for writing so that we can make it the STDOUT of the child process.
	SG_ERR_CHECK(  SG_file__open__pathname(pCtx,pItem->pPath_Result,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,0600,&pFile_Output)  );

	SG_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,"diff3.exe",pArgVec,NULL,pFile_Output,NULL,&childExitStatus)  );

	// gnu-diff3 returns:
	// 0 --> auto-merge successful
	// 1 --> auto-merge found conflicts
	// 2 --> hard errors of some kind

	switch (childExitStatus)
	{
	case 0:
		pItem->result = SG_MRG_AUTOMERGE_RESULT__SUCCESSFUL;
		break;

	case 1:
		pItem->result = SG_MRG_AUTOMERGE_RESULT__CONFLICT;
		break;

//	case 2:
	default:
		SG_ERR_THROW(  SG_ERR_EXTERNAL_TOOL_ERROR  );
	}

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile_Output);
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);
}
#endif

//////////////////////////////////////////////////////////////////

void SG_mrg_external_automerge_handler__builtin_diff3(SG_context * pCtx, SG_mrg_automerge_plan_item * pItem)
{
	SG_fsobj_stat stat;

#if TRACE_MRG
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,
							   ("builtin_diff3:\n"
								"    Ancestor: %s\n"
								"    Mine:     %s\n"
								"    Yours:    %s\n"
								"    Merged:   %s\n"),
							   SG_pathname__sz(pItem->pPath_Ancestor),
							   SG_pathname__sz(pItem->pPath_Mine),
							   SG_pathname__sz(pItem->pPath_Yours),
							   SG_pathname__sz(pItem->pPath_Result))  );
#endif

	SG_NULLARGCHECK_RETURN(pItem);
	SG_NULLARGCHECK_RETURN(pItem->pPath_Ancestor);
	SG_NULLARGCHECK_RETURN(pItem->pPath_Mine);
	SG_NULLARGCHECK_RETURN(pItem->pPath_Yours);
	SG_NULLARGCHECK_RETURN(pItem->pPath_Result);

	SG_ERR_CHECK_RETURN(  SG_fsobj__stat__pathname(pCtx,pItem->pPath_Ancestor,&stat)  );
	SG_ARGCHECK_RETURN(   (stat.type == SG_FSOBJ_TYPE__REGULAR), "File Type of Ancestor Pathname"  );

	SG_ERR_CHECK_RETURN(  SG_fsobj__stat__pathname(pCtx,pItem->pPath_Mine,    &stat)  );
	SG_ARGCHECK_RETURN(   (stat.type == SG_FSOBJ_TYPE__REGULAR), "File Type of Input1 Pathname"  );

	SG_ERR_CHECK_RETURN(  SG_fsobj__stat__pathname(pCtx,pItem->pPath_Yours,   &stat)  );
	SG_ARGCHECK_RETURN(   (stat.type == SG_FSOBJ_TYPE__REGULAR), "File Type of Input2 Pathname"  );

	SG_ERR_CHECK_RETURN(  _builtin_diff3(pCtx,pItem)  );

#if TRACE_MRG
	SG_ERR_IGNORE(  SG_console(pCtx,SG_CS_STDERR,("builtin_diff3: [AutoMergeResult %d]\n"),(pItem->result))  );
#endif
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_FILE_MRG__BUILTIN_DIFF3_H
