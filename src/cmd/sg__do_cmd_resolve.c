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
 * @file sg__do_cmd_resolve.c
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_typedefs.h"
#include "sg_prototypes.h"

//////////////////////////////////////////////////////////////////

struct _resolve_data
{
	SG_pathname *		pPathCwd;
	SG_pendingtree *	pPendingTree;
	SG_stringarray *	psaGids;			// list of the GIDs of the issues that we want to operate on.
	SG_bool				bIgnoreWarnings;
};

//////////////////////////////////////////////////////////////////

static void _resolve__lookup_issue(SG_context * pCtx,
								   struct _resolve_data * pData,
								   const char * pszGid,
								   const SG_vhash ** ppvhIssue)
{
	const SG_vhash * pvhIssue;
	SG_bool bFound;

	if (!pData->pPendingTree)		// previous iteration probably freed it after a SAVE
		SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pData->pPathCwd, pData->bIgnoreWarnings, &pData->pPendingTree)  );
			
	SG_ERR_CHECK(  SG_pendingtree__find_wd_issue_by_gid(pCtx, pData->pPendingTree, pszGid, &bFound, &pvhIssue)  );
	if (!bFound)
	{
		// Since we may have released the VFILE lock during the outer loop,
		// this could technically happen but only if they did something
		// like 'vv revert --all' in another shell while the external merge
		// tool was running.
		//
		// TODO 2010/07/11 I don't like this error message, but I doubt anybody
		// TODO            will ever see it.  Think about taking the GID and
		// TODO            finding a repo-path for the item using just the
		// TODO            PTNODEs and printing it.

		SG_ERR_THROW2(  SG_ERR_ASSERT,
						(pCtx, "RESOLVE failed to find ISSUE for GID %s.", pszGid)  );
	}

	*ppvhIssue = pvhIssue;

fail:
	return;
}

/**
 * We need to check/recheck the RESOLVED/UNRESOLVED status each time
 * after we release the VFILE lock (because the user could have done
 * another RESOLVE in another shell window while the external merge
 * tool was running).
 */
static void _resolve__is_resolved(SG_context * pCtx,
								  const SG_vhash * pvhIssue,
								  SG_bool * pbIsResolved)
{
	SG_int64 s64;
	SG_pendingtree_wd_issue_status status;

	SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, pvhIssue, "status", &s64)  );
	status = (SG_pendingtree_wd_issue_status)s64;

	*pbIsResolved = ((status & SG_ISSUE_STATUS__MARKED_RESOLVED) == SG_ISSUE_STATUS__MARKED_RESOLVED);
}

//////////////////////////////////////////////////////////////////

struct _resolve__step_pathnames
{
	SG_pathname * pPath_Mine;
	SG_pathname * pPath_Other;
	SG_pathname * pPath_Ancestor;
	SG_pathname * pPath_Result;
};

typedef struct _resolve__step_pathnames _resolve__step_pathnames;

static void _resolve__step_pathnames__free(SG_context * pCtx,
										   _resolve__step_pathnames * pStepPathnames)
{
	if (!pStepPathnames)
		return;

	SG_PATHNAME_NULLFREE(pCtx, pStepPathnames->pPath_Mine);
	SG_PATHNAME_NULLFREE(pCtx, pStepPathnames->pPath_Other);
	SG_PATHNAME_NULLFREE(pCtx, pStepPathnames->pPath_Ancestor);
	SG_PATHNAME_NULLFREE(pCtx, pStepPathnames->pPath_Result);
	SG_NULLFREE(pCtx, pStepPathnames);
}

#define _RESOLVE__STEP_PATHNAMES__NULLFREE(pCtx,p)  SG_STATEMENT(SG_context__push_level(pCtx);  _resolve__step_pathnames__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)

/**
 * Compute the pathnames to the various input/output files for 1 step
 * in the file content merge plan.
 *
 * When we computed the merge and modified the WD, we put the various
 * 'foo~mine' and etc files in the same directory where we put the
 * (candidate) merge result.  If there are multiple steps in the plan,
 * the intermediate (sub-results) need to be placed in this directory
 * too.
 *
 * The final result can go in this directory.  *BUT* if there was also
 * a MOVE/RENAME conflict (so the ultimate final location is yet to be
 * determined), the final result may get moved/renamed when we deal
 * with the structural issue in [2].
 *
 * Since it is possible that the user could have done a "vv rename foo ..."
 * or "vv move foo ..." to manually deal with the structural conflict, we
 * respect that and dynamically compute the final destination (and ignore
 * the "result" field in the last step).
 *
 * pStrRepoPath_FinalResult should be NON-NULL when we are the final
 * step in the plan.
 */
static void _resolve__step_pathnames__compute(SG_context * pCtx,
											  struct _resolve_data * pData,
											  const SG_vhash * pvhIssue,
											  const SG_vhash * pvhStep,
											  SG_string * pStrRepoPath_Result,
											  _resolve__step_pathnames ** ppStepPathnames)
{
	_resolve__step_pathnames * pStepPathnames = NULL;
	SG_string * pStrRepoPath_Parent = NULL;
	SG_pathname * pPath_Parent = NULL;
	const SG_pathname * pPath_WorkingDirectoryTop;
	const char * pszGidParent;
	const char * pszEntryname_Mine;
	const char * pszEntryname_Other;
	const char * pszEntryname_Ancestor;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pStepPathnames)  );

	// lookup the parent directory where we initially placed all
	// of the files, find where it is currently in the WD, and
	// build absolute paths for each of the mine/other/ancestor
	// files.

	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhIssue, "gid_parent", &pszGidParent)  );
	SG_ERR_CHECK(  SG_pendingtree__find_repo_path_by_gid(pCtx, pData->pPendingTree, pszGidParent, &pStrRepoPath_Parent)  );
	SG_ERR_CHECK(  SG_pendingtree__get_working_directory_top__ref(pCtx, pData->pPendingTree, &pPath_WorkingDirectoryTop)  );
	SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx,
																		 pPath_WorkingDirectoryTop,
																		 SG_string__sz(pStrRepoPath_Parent),
																		 &pPath_Parent)  );

	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhStep, "mine",     &pszEntryname_Mine)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhStep, "other",    &pszEntryname_Other)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhStep, "ancestor", &pszEntryname_Ancestor)  );

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pStepPathnames->pPath_Mine,     pPath_Parent, pszEntryname_Mine    )  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pStepPathnames->pPath_Other,    pPath_Parent, pszEntryname_Other   )  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pStepPathnames->pPath_Ancestor, pPath_Parent, pszEntryname_Ancestor)  );
	
	if (pStrRepoPath_Result)
	{
		SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx,
																			 pPath_WorkingDirectoryTop,
																			 SG_string__sz(pStrRepoPath_Result),
																			 &pStepPathnames->pPath_Result)  );
	}
	else
	{
		const char * pszEntryname_InternalResult;
		
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhStep, "result", &pszEntryname_InternalResult)  );
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pStepPathnames->pPath_Result, pPath_Parent, pszEntryname_InternalResult)  );
	}

	*ppStepPathnames = pStepPathnames;
	pStepPathnames = NULL;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_Parent);
	SG_STRING_NULLFREE(pCtx, pStrRepoPath_Parent);
	_RESOLVE__STEP_PATHNAMES__NULLFREE(pCtx, pStepPathnames);
}

static void _resolve__step_pathnames__delete_temp_files(SG_context * pCtx,
														_resolve__step_pathnames * pStepPathnames)
{
	SG_ERR_CHECK_RETURN(  SG_fsobj__remove__pathname(pCtx, pStepPathnames->pPath_Mine)  );
	SG_ERR_CHECK_RETURN(  SG_fsobj__remove__pathname(pCtx, pStepPathnames->pPath_Other)  );
	SG_ERR_CHECK_RETURN(  SG_fsobj__remove__pathname(pCtx, pStepPathnames->pPath_Ancestor)  );

	// DO NOT DELETE pPath_Result because it is either the final result
	// or to be used as input to the next step in the plan.
}

//////////////////////////////////////////////////////////////////

/**
 * Given an issue that has a file-content conflict and has just been resolved,
 * go thru all of the steps in the merge plan and delete the ~mine files.
 */
static void _resolve__delete_temp_files(SG_context * pCtx,
										struct _resolve_data * pData,
										const char * pszGid,
										const SG_vhash * pvhIssue)
{
	_resolve__step_pathnames * pStepPathnames = NULL;
	const SG_varray * pvaPlan;
	SG_uint32 kStep, nrSteps;

	SG_UNUSED( pszGid );

	SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvhIssue, "conflict_file_merge_plan", (SG_varray **)&pvaPlan)  );
	SG_ERR_CHECK(  SG_varray__count(pCtx, pvaPlan, &nrSteps)  );

	for (kStep=0; kStep<nrSteps; kStep++)
	{
		const SG_vhash * pvhStep;

		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pvaPlan, kStep, (SG_vhash **)&pvhStep)  );

		SG_ERR_CHECK(  _resolve__step_pathnames__compute(pCtx, pData, pvhIssue, pvhStep, NULL, &pStepPathnames)  );
		SG_ERR_CHECK(  _resolve__step_pathnames__delete_temp_files(pCtx, pStepPathnames)  );
		_RESOLVE__STEP_PATHNAMES__NULLFREE(pCtx, pStepPathnames);
	}

fail:
	_RESOLVE__STEP_PATHNAMES__NULLFREE(pCtx, pStepPathnames);
}

//////////////////////////////////////////////////////////////////

/**
 * Print detailed info for the given ISSUE.
 *
 * Optionally return the current repo-path of the entry.
 */
static void _resolve__list(SG_context * pCtx,
						   struct _resolve_data * pData,
						   const SG_vhash * pvhIssue,
						   SG_string ** ppStrRepoPath)
{
	SG_string * pStrOutput = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStrOutput)  );
	SG_ERR_CHECK(  SG_pendingtree__format_issue(pCtx, pData->pPendingTree, pvhIssue, pStrOutput, ppStrRepoPath)  );
	SG_ERR_IGNORE(  SG_console__raw(pCtx, SG_CS_STDOUT, SG_string__sz(pStrOutput))  );

fail:
	SG_STRING_NULLFREE(pCtx, pStrOutput);
}

/**
 * Print detailed info for each ISSUE in the array.
 */
static void _resolve__do_list(SG_context * pCtx, struct _resolve_data * pData)
{
	SG_uint32 k, kLimit;

	SG_ERR_CHECK(  SG_stringarray__count(pCtx, pData->psaGids, &kLimit)  );
	for (k=0; k<kLimit; k++)
	{
		const char * pszGid_k;
		const SG_vhash * pvhIssue_k;
		SG_bool bFound;

		SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pData->psaGids, k, &pszGid_k)  );
		SG_ERR_CHECK(  SG_pendingtree__find_wd_issue_by_gid(pCtx, pData->pPendingTree, pszGid_k, &bFound, &pvhIssue_k)  );
		if (!bFound)
		{
			// This should never happen because we are still holding the
			// pendingtree lock and still have the original pendingtree
			// structure in memory.

			SG_ERR_THROW2(  SG_ERR_ASSERT,
							(pCtx, "RESOLVE failed to find ISSUE for GID %s.", pszGid_k)  );
		}

		SG_ERR_CHECK(  _resolve__list(pCtx, pData, pvhIssue_k, NULL)  );
	}

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * Update the RESOLVED/UNRESOLVED status for this ISSUE and do an
 * incremental save on the pendingtree.
 */
static void _resolve__mark(SG_context * pCtx,
						   struct _resolve_data * pData,
						   const SG_vhash * pvhIssue,
						   SG_bool bMarkResolved)
{
	SG_int64 s;
	SG_pendingtree_wd_issue_status status;

	SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, pvhIssue, "status", &s)  );
	status = (SG_pendingtree_wd_issue_status)s;

	if (bMarkResolved)
		status |= SG_ISSUE_STATUS__MARKED_RESOLVED;
	else
		status &= ~SG_ISSUE_STATUS__MARKED_RESOLVED;

	// update the status on the ISSUE and save the pendingtree now.
	// since this trashes stuff within in it, go ahead and free it
	// so no one trips over the trash.

	SG_ERR_CHECK_RETURN(  SG_pendingtree__set_wd_issue_status(pCtx, pData->pPendingTree, pvhIssue, status)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pData->pPendingTree);
}

static void _resolve__do_mark_1(SG_context * pCtx,
								struct _resolve_data * pData,
								const char * pszGid,
								SG_bool bMarkResolved)
{
	const SG_vhash * pvhIssue;
	SG_bool bNeedToDeleteTempFiles = SG_FALSE;

	SG_ERR_CHECK(  _resolve__lookup_issue(pCtx, pData, pszGid, &pvhIssue)  );

	if (bMarkResolved)
	{
		SG_int64 i64;
		SG_mrg_cset_entry_conflict_flags conflict_flags;

		// see if there could possibly be ~mine files for this issue that we should delete.

		SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvhIssue, "conflict_flags", &i64)  );
		conflict_flags = (SG_mrg_cset_entry_conflict_flags)i64;
		if (conflict_flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK__NOT_OK)
			bNeedToDeleteTempFiles = SG_TRUE;
	}

	SG_ERR_CHECK(  _resolve__mark(pCtx, pData, pvhIssue, bMarkResolved)  );

	// After the pendingtree has been written out, try to delete the temp files.

	if (bNeedToDeleteTempFiles)
	{
		SG_ERR_CHECK(  _resolve__lookup_issue(pCtx, pData, pszGid, &pvhIssue)  );
		SG_ERR_CHECK(  _resolve__delete_temp_files(pCtx, pData, pszGid, pvhIssue)  );
	}

fail:
	return;
}

/**
 * Update the RESOLVED/UNRESOLVED status for each ISSUE in the array
 * and then SAVE the pendingtree.
 */
static void _resolve__do_mark(SG_context * pCtx, struct _resolve_data * pData, SG_bool bMarkResolved)
{
	SG_uint32 k, kLimit;

	SG_ERR_CHECK(  SG_stringarray__count(pCtx, pData->psaGids, &kLimit)  );
	for (k=0; k<kLimit; k++)
	{
		const char * pszGid_k;

		SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pData->psaGids, k, &pszGid_k)  );
		SG_ERR_CHECK(  _resolve__do_mark_1(pCtx, pData, pszGid_k, bMarkResolved)  );
	}

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * All of the info that we need to launch an external tool to perform a TEXT MERGE.
 */
struct _resolve__external_tool
{
	char * pszName;		// a human-friendly name for the tool
	char * pszExe;		// name of the executable (either a full path or the name of a command in their PATH)

	// TODO add fields for the arglist and etc.
};

typedef struct _resolve__external_tool _resolve__external_tool;

static void _resolve__external_tool__free(SG_context * pCtx, _resolve__external_tool * pET)
{
	if (!pET)
		return;

	SG_NULLFREE(pCtx, pET->pszName);
	SG_NULLFREE(pCtx, pET->pszExe);
	SG_NULLFREE(pCtx, pET);
}

#define _RESOLVE__EXTERNAL_TOOL__NULLFREE(pCtx,p)  SG_STATEMENT(SG_context__push_level(pCtx);  _resolve__external_tool__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)

/**
 * Find the appropriate external tool to let the user perform a TEXT MERGE
 * on a file.
 *
 * TODO 2010/07/13 For now, this is hard-coded to use DiffMerge.
 * TODO            Later we want to allow them to have multiple
 * TODO            tools configured and/or to use the file suffix
 * TODO            and so on.
 */
static void _resolve__external_tool__lookup(SG_context * pCtx,
											struct _resolve_data * pData,
											const char * pszGid,
											const SG_vhash * pvhIssue,
											SG_string * pStrRepoPath,
											_resolve__external_tool ** ppET)
{
	_resolve__external_tool * pET = NULL;
	SG_repo * pRepo;

	SG_UNUSED( pszGid );
	SG_UNUSED( pvhIssue );

	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pData->pPendingTree, &pRepo)  );

	SG_ERR_CHECK(  SG_alloc1(pCtx, pET)  );

	// TODO 2010/07/13 Use localsettings to determine WHICH external tool we should use.
	// TODO            (This could be based upon suffixes and/or whatever.)
	// TODO            Then -- for THAT tool -- lookup the program executable path and
	// TODO            the argument list.
	// TODO            Substitute the given pathnames into the argument list.
	// TODO
	// TODO            For now, we just hard-code DiffMerge.

	SG_ERR_CHECK(  SG_strdup(pCtx, "DiffMerge", &pET->pszName)  );

	SG_localsettings__get__sz(pCtx, "merge/diffmerge/program", pRepo, &pET->pszExe, NULL);
	if (SG_context__has_err(pCtx) || (!pET->pszExe) || (!*pET->pszExe))
	{
		SG_context__err_reset(pCtx);
		SG_ERR_THROW2(  SG_ERR_NO_MERGE_TOOL_CONFIGURED,
						(pCtx, "'%s'  Use 'vv localsetting set merge/diffmerge/program' and retry -or- manually merge content and then use 'vv resolve --mark'.",
						 SG_string__sz(pStrRepoPath))  );
	}

	// TODO 2010/07/13 Get argvec.

	*ppET = pET;
	return;

fail:
	_RESOLVE__EXTERNAL_TOOL__NULLFREE(pCtx, pET);
}

//////////////////////////////////////////////////////////////////

static void _resolve__fix__run_external_file_merge_1(SG_context * pCtx,
													 struct _resolve_data * pData,
													 _resolve__external_tool * pET,
													 _resolve__step_pathnames * pStepPathnames,
													 SG_string * pStrRepoPath,
													 SG_bool * pbMergedText)
{
	SG_exec_argvec * pArgVec = NULL;
	SG_exit_status exitStatus;

	SG_UNUSED( pData );

#if 0 && defined(DEBUG)
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   ("RESOLVE File Merge: %s\n"
								"          Mine: %s\n"
								"         Other: %s\n"
								"      Ancestor: %s\n"
								"        Result: %s\n"),
							   SG_string__sz(pStrRepoPath),
							   SG_pathname__sz(pStepPathnames->pPath_Mine),
							   SG_pathname__sz(pStepPathnames->pPath_Other),
							   SG_pathname__sz(pStepPathnames->pPath_Ancestor),
							   SG_pathname__sz(pStepPathnames->pPath_Result))  );
#endif


	SG_ERR_CHECK(  SG_exec_argvec__alloc(pCtx, &pArgVec)  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, "-r")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, SG_pathname__sz(pStepPathnames->pPath_Result))  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, "-t1")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, "Mine")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, "-t2")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, SG_string__sz(pStrRepoPath))  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, "-t3")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, "Other")  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, SG_pathname__sz(pStepPathnames->pPath_Mine))  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, SG_pathname__sz(pStepPathnames->pPath_Ancestor))  );
	SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, SG_pathname__sz(pStepPathnames->pPath_Other))  );

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT, "RESOLVE: Preparing to launch external merge tool: %s\n", pET->pszName)  );
	SG_ERR_CHECK(  SG_exec__exec_sync__files(pCtx, pET->pszExe, pArgVec, NULL, NULL, NULL, &exitStatus)  );
	*pbMergedText = (exitStatus == 0);

fail:
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);
}

enum _fix_status
{
	FIX_USER_MERGED = 1,
	FIX_USER_ABORTED = 2,
	FIX_LOST_RACE = 3,
	FIX_TODO = 4
};
	
/**
 * Release VFILE lock and invoke external merge tool for this file.
 *
 * TODO 2010/07/12 The MERGE-PLAN is an array and allows for
 * TODO            multiple steps (for an n-way sub-merge cascade).
 * TODO            But we don't have that part turned on yet in
 * TODO            sg_mrg__private_biuld_wd_issues.h:_make_file_merge_plan(),
 * TODO            so for now, we only expect 1 step.
 * TODO
 * TODO            Also, when we do have multiple steps, we might want to
 * TODO            be able to use the 'status' field to see which steps
 * TODO            were already performed in an earlier RESOLVE.
 * TODO
 * TODO            Also, when we want to support more than 1 step we need
 * TODO            to copy pvaPlan because when we release the pendingtree
 * TODO            the pvhIssue becomes invalidated too.
 */
static void _resolve__fix__run_external_file_merge(SG_context * pCtx,
												   struct _resolve_data * pData,
												   const char * pszGid,
												   const SG_vhash * pvhIssue,
												   SG_string * pStrRepoPath,
												   enum _fix_status * pFixStatus)
{
	_resolve__step_pathnames * pStepPathnames = NULL;
	_resolve__external_tool * pET = NULL;
	const SG_varray * pvaPlan;
	const SG_vhash * pvhStep_0;
	SG_int64 r64;
	SG_uint32 nrSteps;
	SG_mrg_automerge_result result;
	SG_bool bMerged = SG_FALSE;
	SG_bool bIsResolved = SG_FALSE;

	SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvhIssue, "conflict_file_merge_plan", (SG_varray **)&pvaPlan)  );
	SG_ERR_CHECK(  SG_varray__count(pCtx, pvaPlan, &nrSteps)  );

	if (nrSteps > 1)
		SG_ERR_THROW2(  SG_ERR_ASSERT,
						(pCtx, "TODO RESOLVE more than 1 step in auto-merge plan for '%s'.", SG_string__sz(pStrRepoPath))  );

	//////////////////////////////////////////////////////////////////
	// Get Step[0]

	SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pvaPlan, 0, (SG_vhash **)&pvhStep_0)  );

	// see if the user has already performed the merge and maybe got interrupted.

	SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvhStep_0, "status", &r64)  );
	result = (SG_mrg_automerge_result)r64;
	if (result == SG_MRG_AUTOMERGE_RESULT__SUCCESSFUL)
	{
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   "TODO Print message about previous successful manual merge of the file content and ask if they want to redo it for '%s'.\n",
								   SG_string__sz(pStrRepoPath))  );

		*pFixStatus = FIX_USER_MERGED;
		goto done;
	}

	SG_ERR_CHECK(  _resolve__step_pathnames__compute(pCtx, pData, pvhIssue, pvhStep_0, pStrRepoPath, &pStepPathnames)  );

	// While we still have a handle to the pendingtree, lookup the
	// specifics on the external tool that we should invoke.  these
	// details come from localsettings.

	SG_ERR_CHECK(  _resolve__external_tool__lookup(pCtx, pData, pszGid, pvhIssue, pStrRepoPath, &pET)  );

	// Free the PENDINGTREE so that we release the VFILE lock.

	pvhIssue = NULL;
	pvaPlan = NULL;
	pvhStep_0 = NULL;
	SG_PENDINGTREE_NULLFREE(pCtx, pData->pPendingTree);

	//////////////////////////////////////////////////////////////////
	// Invoke the external tool.

	SG_ERR_CHECK(  _resolve__fix__run_external_file_merge_1(pCtx,
															pData,
															pET,
															pStepPathnames,
															pStrRepoPath,
															&bMerged)  );
	if (!bMerged)
	{
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT, "RESOLVE: Aborting the merge of this file.\n")  );
		*pFixStatus = FIX_USER_ABORTED;
		goto done;
	}

	//////////////////////////////////////////////////////////////////
	// Reload the PENDINGTREE and re-fetch the ISSUE and updated the STATUS on
	// this step in the PLAN.
	// 
	// We duplicate some of the "see if someone else resolved this issue while
	// we were without the lock" stuff.

	SG_ERR_CHECK(  _resolve__lookup_issue(pCtx, pData, pszGid, &pvhIssue)  );
	SG_ERR_CHECK(  _resolve__is_resolved(pCtx, pvhIssue, &bIsResolved)  );
	if (bIsResolved)
	{
		// Someone else marked it resolved while were waiting for
		// the user to edit the file and while we didn't have the
		// file lock.  We should stop here.

		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT, "RESOLVE: Aborting the merge of this file (due to race condition).\n")  );
		*pFixStatus = FIX_LOST_RACE;
		goto done;
	}

	// re-fetch the current step and update the "result" status for it
	// and flush the pendingtree back disk.
	//
	// we only update the step status -- we DO NOT alter the __DIVERGENT_FILE_EDIT__
	// conflict_flags.

	SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvhIssue, "conflict_file_merge_plan", (SG_varray **)&pvaPlan)  );
	SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pvaPlan, 0, (SG_vhash **)&pvhStep_0)  );
	SG_ERR_CHECK(  SG_pendingtree__set_wd_issue_plan_step_status__dont_save_pendingtree(pCtx, pData->pPendingTree,
																						pvhStep_0,
																						SG_MRG_AUTOMERGE_RESULT__SUCCESSFUL)  );
	SG_ERR_CHECK(  SG_pendingtree__save(pCtx, pData->pPendingTree)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pData->pPendingTree);

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT, "RESOLVE: The file content portion of the merge was successful.\n")  );
	*pFixStatus = FIX_USER_MERGED;

	// we defer the delete of the temp input files until we completely
	// resolve the issue.  (This gives us more options if we allow the
	// resolve to be restarted after interruptions.)

done:
	;
fail:
	_RESOLVE__EXTERNAL_TOOL__NULLFREE(pCtx, pET);
	_RESOLVE__STEP_PATHNAMES__NULLFREE(pCtx, pStepPathnames);
}

//////////////////////////////////////////////////////////////////

/**
 * Try to resolve structural problems for this ISSUE which
 * includes one branch deleting the entry and the other
 * branch altering it in some way.
 *
 * (If it was a delete vs unaltered, it wouldn't be a conflict
 * now would it.)
 *
 * The merge-apply code temporarily un-deleted the entry and
 * may have given it a temporary name/location.
 *
 * Ask the user if they want to delete it or take the altered
 * version.
 * 
 * TODO 2010/07/14 For a 2-way merge we cannot have both
 * TODO            DELETE-vs-x *AND* DIVERGENT_x
 * TODO            problems (because there are only 2 terms).
 * TODO            However, when we fully support n-way merges, 
 * TODO            we have to allow for both at the same time
 * TODO            (since we may have 3 or more terms).
 */
static void _resolve__fix__structural__delete(SG_context * pCtx,
											  struct _resolve_data * pData,
											  const char * pszGid,
											  const SG_vhash * pvhIssue,
											  SG_string * pStrRepoPath,
											  SG_mrg_cset_entry_conflict_flags conflict_flags,
											  SG_bool bCollisions,
											  SG_portability_flags portability_flags,
											  enum _fix_status * pFixStatus)
{
	SG_UNUSED( pData );
	SG_UNUSED( pszGid );
	SG_UNUSED( pvhIssue );
	SG_UNUSED( conflict_flags );
	SG_UNUSED( bCollisions );
	SG_UNUSED( portability_flags );

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT,
							   "TODO _resolve__fix: handle DELETE-vs-x structural issues for '%s'.  For now, fix manually and use 'vv resolve --mark'.\n",
							   SG_string__sz(pStrRepoPath))  );

	*pFixStatus = FIX_TODO;
}

/**
 * Try to resolve non-delete-related structural problems
 * for this ISSUE.
 */
static void _resolve__fix__structural__non_delete(SG_context * pCtx,
												  struct _resolve_data * pData,
												  const char * pszGid,
												  const SG_vhash * pvhIssue,
												  SG_string * pStrRepoPath,
												  SG_mrg_cset_entry_conflict_flags conflict_flags,
												  SG_bool bCollisions,
												  SG_portability_flags portability_flags,
												  enum _fix_status * pFixStatus)
{
	SG_UNUSED( pData );
	SG_UNUSED( pszGid );
	SG_UNUSED( pvhIssue );
	SG_UNUSED( conflict_flags );
	SG_UNUSED( bCollisions );
	SG_UNUSED( portability_flags );

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT,
							   "TODO _resolve__fix: handle non-delete structural issues for '%s'.  For now, fix manually and use 'vv resolve --mark'.\n",
							   SG_string__sz(pStrRepoPath))  );

	*pFixStatus = FIX_TODO;
}

//////////////////////////////////////////////////////////////////

/**
 * Try to FIX the ISSUE.
 *
 * Alter something in the pendingtree/issue/WD and then SAVE the pendingtree.
 * We allow this to be an incremental save after just this issue.  We also
 * allow the VFILE lock to be released while the external merge (DiffMerge)
 * tool is running.  (Not because DiffMerge needs it, but rather so that they
 * could do other STATUS/DIFF commands in another shell while doing the text
 * merge.)
 */	
static void _resolve__fix(SG_context * pCtx,
						  struct _resolve_data * pData,
						  const char * pszGid,
						  enum _fix_status * pFixStatus)
{
	const SG_vhash * pvhIssue;
	SG_string * pStrRepoPath = NULL;
	SG_int64 i64;
	SG_mrg_cset_entry_conflict_flags conflict_flags;
	SG_portability_flags portability_flags;
	SG_bool bIsResolved = SG_FALSE;
	SG_bool bCollisions;

	// Fetch the ISSUE using the current pendingtree (allocating one if
	// necessary) and print detailed info about the ISSUE on the console.

	SG_ERR_CHECK(  _resolve__lookup_issue(pCtx, pData, pszGid, &pvhIssue)  );
	SG_ERR_CHECK(  _resolve__list(pCtx, pData, pvhIssue, &pStrRepoPath)  );

	// TODO 2010/07/12 We should have a --prompt option to allow them to
	// TODO            skip an issue.  Like "/bin/rm -i *".

	// Skip the issue if it is already resolved.  In theory, we should not
	// get this (because we filtered the pData->psaGids by status as we
	// parsed the command line arguments), but if they did another resolve
	// in another shell while we didn't have the lock, it could happen.

	SG_ERR_CHECK(  _resolve__is_resolved(pCtx, pvhIssue, &bIsResolved)  );
	if (bIsResolved)
	{
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT,
								   "Issue already resolved; nothing to be done for '%s'.\n",
								   SG_string__sz(pStrRepoPath))  );
		goto done;
	}

	// There are 2 main types of problems:
	// [1] Conflicts within the text of a file (where the builtin auto-merge
	//     failed or was not used) and for which we need to ask them to manually
	//     merge the content (using an external tool like DiffMerge).
	// [2] Structural changes, including: MOVEs, RENAMEs, CHMODs, XATTRs,
	//     entryname collisions, potential entryname collisions, and etc.
	//
	// We could also have both -- both edit conflicts and rename conflicts,
	// for example.
	//
	// Do these in 2 steps so that we can release the VFILE lock while they
	// are editing the file.

	//////////////////////////////////////////////////////////////////
	// [1]
	//////////////////////////////////////////////////////////////////

	SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvhIssue, "conflict_flags", &i64)  );
	conflict_flags = (SG_mrg_cset_entry_conflict_flags)i64;
	if (conflict_flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK__NOT_OK)
	{
		SG_ERR_CHECK(  _resolve__fix__run_external_file_merge(pCtx, pData, pszGid, pvhIssue, pStrRepoPath, pFixStatus)  );
		pvhIssue = NULL;

		if (*pFixStatus != FIX_USER_MERGED)
			goto done;

		// the above MAY have freed and reloaded the pendingtree (and
		// invalidated pvhIssue), so re-fetch it and/or re-set our variables.

		SG_ERR_CHECK(  _resolve__lookup_issue(pCtx, pData, pszGid, &pvhIssue)  );
		SG_ERR_CHECK(  _resolve__is_resolved(pCtx, pvhIssue, &bIsResolved)  );
		if (bIsResolved)
		{
			// Someone else marked it resolved while were waiting for
			// the user to edit the file and while we didn't have the
			// file lock.  We should stop here.

			*pFixStatus = FIX_LOST_RACE;
			goto done;
		}

		SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvhIssue, "conflict_flags", &i64)  );
		conflict_flags = (SG_mrg_cset_entry_conflict_flags)i64;
	}

#if 0 && defined(DEBUG)
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
							   "RESOLVE: Issue between [1] and [2]: '%s'\n",
							   SG_string__sz(pStrRepoPath))  );
	SG_ERR_IGNORE(  SG_vhash_debug__dump_to_console(pCtx, pvhIssue)  );
#endif

	//////////////////////////////////////////////////////////////////
	// [2]
	//////////////////////////////////////////////////////////////////

	SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvhIssue, "collision_flags", &bCollisions)  );
	SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvhIssue, "portability_flags", &i64)  );
	portability_flags = (SG_portability_flags)i64;

	if (conflict_flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__UNDELETE__MASK)
	{
		SG_ERR_CHECK(  _resolve__fix__structural__delete(pCtx, pData,
														 pszGid, pvhIssue, pStrRepoPath,
														 conflict_flags, bCollisions, portability_flags,
														 pFixStatus)  );
		if (*pFixStatus != FIX_USER_MERGED)
			goto done;
	}
	else if (bCollisions || (portability_flags) || (conflict_flags & ~SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK))
	{
		SG_ERR_CHECK(  _resolve__fix__structural__non_delete(pCtx, pData,
															 pszGid, pvhIssue, pStrRepoPath,
															 conflict_flags, bCollisions, portability_flags,
															 pFixStatus)  );
		if (*pFixStatus != FIX_USER_MERGED)
			goto done;
	}

	// mark the issue as RESOLVED and save the pendingtree.

	SG_ERR_CHECK(  _resolve__mark(pCtx, pData, pvhIssue, SG_TRUE)  );
	
	*pFixStatus = FIX_USER_MERGED;

	//////////////////////////////////////////////////////////////////
	// We've completely resolved the issue, if there were ~mine files,
	// we can delete them.

	if (conflict_flags & SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK__NOT_OK)
	{
		SG_ERR_CHECK(  _resolve__lookup_issue(pCtx, pData, pszGid, &pvhIssue)  );
		SG_ERR_CHECK(  _resolve__delete_temp_files(pCtx, pData, pszGid, pvhIssue)  );
	}
	
done:
	;
fail:
	SG_STRING_NULLFREE(pCtx, pStrRepoPath);
}

/**
 * Try to FIX each ISSUE in the array and update the WD and allow
 * incremental saves of the pendingtree.
 */
static void _resolve__do_fix(SG_context * pCtx, struct _resolve_data * pData)
{
	SG_uint32 k, kLimit;

	SG_ERR_CHECK(  SG_stringarray__count(pCtx, pData->psaGids, &kLimit)  );
	for (k=0; k<kLimit; k++)
	{
		const char * pszGid_k;
		enum _fix_status fixStatus;

		SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pData->psaGids, k, &pszGid_k)  );

		SG_ERR_CHECK(  _resolve__fix(pCtx, pData, pszGid_k, &fixStatus)  );

		// If everything went well, we go on to the next ISSUE in the list.
		// If the user aborted the merge (or if we had a problem of some kind
		// we should probably just stop).

		if ((fixStatus != FIX_USER_MERGED) && (k+1 < kLimit))
		{
			// TODO 2010/07/12 Ask if they want to continue anyway.

			break;
		}
	}

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * Assuming that we have something of the form:
 * 
 *     vv resolve [--foo] <arg_0> [<arg_1> [<arg_2> ...]]
 *
 * where each <arg_x> is an absolute or relative path in the WD
 * (probably not a repo-path).
 *
 * Use the PENDINGTREE to lookup each path and get the entry's GID.
 * Use the GID to search for an ISSUE in the list of issues.  If we
 * find it, add the GID to the stringarray we are building.  If not,
 * throw an error.
 */
static void _resolve__map_args_to_gids(SG_context * pCtx,
									   struct _resolve_data * pData,
									   SG_uint32 count_args, const char ** paszArgs,
									   SG_bool bWantResolved,
									   SG_bool bWantUnresolved)
{
	SG_pathname * pPath_k = NULL;
	char * pszGid_k = NULL;
	SG_uint32 kArg;
	SG_bool bWantBoth = (bWantResolved && bWantUnresolved);

	SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &pData->psaGids, count_args)  );

	for (kArg=0; kArg<count_args; kArg++)
	{
		const SG_vhash * pvhIssue_k;
		SG_bool bFound;
		SG_bool bDuplicate;
		SG_bool bWantThisOne;

		// take each <arg_k> and get a full pathname for it and
		// search for it in the pendingtree and get its GID.
		// in theory, if an entry has an issue, it is dirty and
		// should have a ptnode.

		if (paszArgs[kArg][0] == '@')
			SG_ERR_CHECK(  SG_workingdir__construct_absolute_path_from_repo_path2(pCtx, pData->pPendingTree, paszArgs[kArg], &pPath_k)  );
		else
			SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath_k, paszArgs[kArg])  );

		SG_ERR_CHECK(  SG_pendingtree__get_gid_from_local_path(pCtx, pData->pPendingTree, pPath_k, &pszGid_k)  );
#if 0 && defined(DEBUG)
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
								   ("Mapped arg[%d] '%s' to:\n"
									"\t%s\n"
									"\t[gid %s]\n"),
								   kArg, paszArgs[kArg],
								   SG_pathname__sz(pPath_k),
								   pszGid_k)  );
#endif

		// see if there is an ISSUE for this GID.

		SG_ERR_CHECK(  SG_pendingtree__find_wd_issue_by_gid(pCtx, pData->pPendingTree, pszGid_k, &bFound, &pvhIssue_k)  );
		if (!bFound)
			SG_ERR_THROW2(  SG_ERR_ISSUE_NOT_FOUND,
							(pCtx, "No issue found for '%s': %s",
							 paszArgs[kArg],
							 SG_pathname__sz(pPath_k))  );

		if (bWantBoth)
			bWantThisOne = SG_TRUE;
		else
		{
			SG_int64 s;
			SG_pendingtree_wd_issue_status status;
			SG_bool bResolved;

			SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, pvhIssue_k, "status", &s)  );
			status = (SG_pendingtree_wd_issue_status)s;
			bResolved = ((status & SG_ISSUE_STATUS__MARKED_RESOLVED) == SG_ISSUE_STATUS__MARKED_RESOLVED);

			bWantThisOne = ((bWantResolved && bResolved) || (bWantUnresolved && !bResolved));
		}

		if (bWantThisOne)
		{
			// check for duplicate args on command line. (or rather, args that
			// map to the same GID.)

			SG_ERR_CHECK(  SG_stringarray__find(pCtx, pData->psaGids, pszGid_k, 0, &bDuplicate, NULL)  );
			if (bDuplicate)
				SG_ERR_THROW2(  SG_ERR_DUPLICATE_ISSUE,
								(pCtx, "Argument '%s' maps to an issue already named.", paszArgs[kArg])  );

			SG_ERR_CHECK(  SG_stringarray__add(pCtx, pData->psaGids, pszGid_k)  );
		}

		SG_NULLFREE(pCtx, pszGid_k);
		SG_PATHNAME_NULLFREE(pCtx, pPath_k);
	}

	return;

fail:
	SG_NULLFREE(pCtx, pszGid_k);
	SG_PATHNAME_NULLFREE(pCtx, pPath_k);
}

//////////////////////////////////////////////////////////////////

/**
 * build an ordered stringarray of the GIDs of all of the issues.
 * i need to have the list of GIDs be independent of pPendingTree
 * and pvaIssues so that FIX can do incremental SAVES of the
 * pendingtree (which trashes the ptnodes and/or requires a reload).
 */
static void _resolve__get_all_issue_gids(SG_context * pCtx,
										 struct _resolve_data * pData,
										 SG_bool bWantResolved,
										 SG_bool bWantUnresolved)
{
	const SG_varray * pvaIssues;			// varray[pvhIssue *] of all issues -- we do not own this
	SG_bool bHaveIssues;
	SG_uint32 k;
	SG_uint32 nrIssues = 0;
	SG_bool bWantBoth = (bWantResolved && bWantUnresolved);

	SG_ERR_CHECK(  SG_pendingtree__get_wd_issues__ref(pCtx, pData->pPendingTree, &bHaveIssues, &pvaIssues)  );
	if (bHaveIssues)
		SG_ERR_CHECK(  SG_varray__count(pCtx, pvaIssues, &nrIssues)  );

	SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &pData->psaGids, nrIssues)  );

	for (k=0; k<nrIssues; k++)
	{
		const SG_vhash * pvhIssue_k;
		const char * pszGid_k;
		SG_bool bWantThisOne;

		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pvaIssues, k, (SG_vhash **)&pvhIssue_k)  );

		if (bWantBoth)
			bWantThisOne = SG_TRUE;
		else
		{
			SG_int64 s;
			SG_pendingtree_wd_issue_status status;
			SG_bool bResolved;

			SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, pvhIssue_k, "status", &s)  );
			status = (SG_pendingtree_wd_issue_status)s;
			bResolved = ((status & SG_ISSUE_STATUS__MARKED_RESOLVED) == SG_ISSUE_STATUS__MARKED_RESOLVED);

			bWantThisOne = ((bWantResolved && bResolved) || (bWantUnresolved && !bResolved));
		}

		if (bWantThisOne)
		{
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhIssue_k, "gid", &pszGid_k)  );
			SG_ERR_CHECK(  SG_stringarray__add(pCtx, pData->psaGids, pszGid_k)  );
		}
	}

fail:
	return;
}

//////////////////////////////////////////////////////////////////

/**
 * Handle the RESOLVE command.
 *
 *
 */
void do_cmd_resolve(SG_context * pCtx,
					SG_option_state * pOptSt,
					SG_uint32 count_args, const char** paszArgs)
{
	struct _resolve_data data;
	SG_uint32 sum = 0;
	SG_bool bAll = SG_FALSE;
	SG_bool bWantResolved = SG_FALSE;
	SG_bool bWantUnresolved = SG_FALSE;
	SG_bool bReqArg = SG_FALSE;

	memset(&data, 0, sizeof(data));
	data.pPathCwd = NULL;
	data.pPendingTree = NULL;
	data.psaGids = NULL;
	data.bIgnoreWarnings = SG_TRUE;			// TODO what should this be?

	// allow at most ONE of the command options.
	//
	// the --{List,Mark,Unmark}All options do not allow ARGs.
	// 
	// the --{Mark,Unmark} require at least one ARG.
	// the --List allows 0 or more ARGs.
	//
	// if no command option, allow 0 or more ARGs.
	//
	// most commands do not require there to be issues; rather
	// they just don't do anything.
	//
	// WARNING: We set sg_cl_options[].has_arg to 0 for all of
	//          our commands options so that we get all of the
	//          pathnames in ARGs rather than bound to the option.
	//          That is, I want to be able to say:
	//               vv resolve --mark foo bar
	//          rather than:
	//               vv resolve --mark foo --mark bar
	//
	//          It also allows me to have:
	//               vv resolve --list
	//          and
	//               vv resolve --list foo

	if (pOptSt->bListAll)		{ sum++; bAll = SG_TRUE;  bWantResolved = SG_TRUE;  bWantUnresolved = SG_TRUE;  bReqArg = SG_FALSE; }
	if (pOptSt->bMarkAll)		{ sum++; bAll = SG_TRUE;  bWantResolved = SG_TRUE;  bWantUnresolved = SG_TRUE;  bReqArg = SG_FALSE; }
//	if (pOptSt->bUnmarkAll)		{ sum++; bAll = SG_TRUE;  bWantResolved = SG_TRUE;  bWantUnresolved = SG_TRUE;  bReqArg = SG_FALSE; }
	if (pOptSt->bList)
	{
		if (count_args == 0)	{ sum++; bAll = SG_FALSE; bWantResolved = SG_FALSE; bWantUnresolved = SG_TRUE;  bReqArg = SG_FALSE; }
		else					{ sum++; bAll = SG_FALSE; bWantResolved = SG_TRUE;  bWantUnresolved = SG_TRUE;  bReqArg = SG_FALSE; }
	}
	if (pOptSt->bMark)			{ sum++; bAll = SG_FALSE; bWantResolved = SG_FALSE; bWantUnresolved = SG_TRUE;  bReqArg = SG_TRUE;  }
//	if (pOptSt->bUnmark)		{ sum++; bAll = SG_FALSE; bWantResolved = SG_TRUE;  bWantUnresolved = SG_FALSE; bReqArg = SG_TRUE;  }
	if (sum == 0)				{        bAll = SG_FALSE; bWantResolved = SG_FALSE; bWantUnresolved = SG_TRUE;  bReqArg = SG_FALSE; }

	if (sum > 1)
		SG_ERR_THROW(  SG_ERR_USAGE  );

	if (bReqArg && (count_args == 0))
		SG_ERR_THROW(  SG_ERR_USAGE  );

	if (bAll && (count_args > 0))
		SG_ERR_THROW(  SG_ERR_USAGE  );

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &data.pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, data.pPathCwd)  );

	// Do a complete scan first.  This ensures that the pendingtree knows
	// about everything that is dirty in the WD and helps ensure that every
	// issue in the issues list has a ptnode in the pendingtree.
	// 
	// TODO 2010/07/16 Technically, this should NOT be required.  But it
	// TODO            helps.  The problem is that when a file is edited
	// TODO            we don't automatically get the notification, rather
	// TODO            we do a status aka scan (and/or use the timestamp
	// TODO            cache) when various commands start which detect
	// TODO            file content changes.  So the fact that the MERGE
	// TODO            may have written a bunch of merged/edited files
	// TODO            doesn't necessarily mean that they are listed in
	// TODO            the pendingtree -- because the user may have edited
	// TODO            them again (or edited other files) since the merge
	// TODO            completed.  So we scan.
	// TODO
	// TODO            See also the comment in sg.c:do_cmd_commit() for sprawl-809.
	// TODO
	// TODO            What this scan is helping to hide is a problem where
	// TODO            we're hitting the issues list for GIDs and then
	// TODO            using SG_pendingtree__find_repo_path_by_gid() to
	// TODO            dynamically convert it into a "live/current" repo-path.
	// TODO            and it assumes that it is only called for dirty entries
	// TODO            (or rather, for entries that have a ptnode).  We need
	// TODO            to fix that.

	SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, data.pPathCwd, data.bIgnoreWarnings, &data.pPendingTree)  );
	SG_ERR_CHECK(  SG_pendingtree__scan(pCtx, data.pPendingTree, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, data.pPendingTree);

	// Now load the pendingtree for real.

	SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, data.pPathCwd, data.bIgnoreWarnings, &data.pPendingTree)  );

	if (count_args > 0)
		SG_ERR_CHECK(  _resolve__map_args_to_gids(pCtx, &data, count_args, paszArgs, bWantResolved, bWantUnresolved)  );
	else
		SG_ERR_CHECK(  _resolve__get_all_issue_gids(pCtx, &data, bWantResolved, bWantUnresolved)  );

	//////////////////////////////////////////////////////////////////

	if (pOptSt->bListAll || pOptSt->bList)
	{
		SG_ERR_CHECK(  _resolve__do_list(pCtx, &data)  );
	}
	else if (pOptSt->bMarkAll || pOptSt->bMark)
	{
		SG_ERR_CHECK(  _resolve__do_mark(pCtx, &data, SG_TRUE)  );
	}
//	else if (pOptSt->bUnmarkAll || pOptSt->bUnmark)
//	{
//		SG_ERR_CHECK(  _resolve__do_mark(pCtx, &data, SG_FALSE)  );
//	}
	else // no command option given -- assume we want to FIX the issues
	{
		SG_ERR_CHECK(  _resolve__do_fix(pCtx, &data)  );
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, data.pPathCwd);
	SG_PENDINGTREE_NULLFREE(pCtx, data.pPendingTree);
	SG_STRINGARRAY_NULLFREE(pCtx, data.psaGids);
}



