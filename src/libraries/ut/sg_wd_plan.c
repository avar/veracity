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
 * @file sg_wd_plan.c
 *
 * @details Routines to create/maniuplate "wd plan".  A "wd plan" contains
 * a series of operations that we'd like to perform on the WD.  For example,
 * when doing a MERGE, we build a PLAN of operations before doing anything.
 * We can just report the PLAN when given a "--test" or we can use the plan
 * and then report it.
 *
 * NOTE: Currently, this is only used by MERGE.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

#if defined(DEBUG)
#	define TRACE_WD_PLAN 0
#else
#	define TRACE_WD_PLAN 0
#endif

//////////////////////////////////////////////////////////////////

struct sg_wd_plan
{
	SG_varray *				pvaScript;	// varray[vhash] -- the steps in the plan

	SG_wd_plan_stats		stats;
};

//////////////////////////////////////////////////////////////////

void SG_wd_plan__alloc(SG_context * pCtx, SG_wd_plan ** ppPlan)
{
	SG_wd_plan * pPlan = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pPlan)  );

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pPlan->pvaScript)  );

	*ppPlan = pPlan;
	return;

fail:
	SG_WD_PLAN_NULLFREE(pCtx, pPlan);
}

void SG_wd_plan__free(SG_context * pCtx, SG_wd_plan * pPlan)
{
	if (!pPlan)
		return;

	SG_VARRAY_NULLFREE(pCtx, pPlan->pvaScript);
	SG_NULLFREE(pCtx, pPlan);
}

//////////////////////////////////////////////////////////////////

void SG_wd_plan__move_rename(SG_context * pCtx,
							 SG_wd_plan * pPlan,
							 const char * pszGid,
							 const char * pszRepoPath,
							 const char * pszPathSrc,
							 const char * pszPathDest,
							 const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath );
	SG_NONEMPTYCHECK_RETURN( pszPathSrc );
	SG_NONEMPTYCHECK_RETURN( pszPathDest );
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",             "move_rename")  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",                pszGid       )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_baseline",  pszRepoPath  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_source",        pszPathSrc   )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_destination",   pszPathDest  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",             pszReason    )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	// NO stats for operations to/from parking lot.
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

void SG_wd_plan__remove_file(SG_context * pCtx,
							 SG_wd_plan * pPlan,
							 const char * pszGid,
							 const char * pszRepoPath,
							 const char * pszPath,
							 const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath );
	SG_NONEMPTYCHECK_RETURN( pszPath );
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",            "remove_file" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",               pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_baseline", pszRepoPath)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_destination",  pszPath    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",            pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	pPlan->stats.nrFilesDeleted++;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

void SG_wd_plan__remove_symlink(SG_context * pCtx,
								SG_wd_plan * pPlan,
								const char * pszGid,
								const char * pszRepoPath,
								const char * pszPath,
								const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath );
	SG_NONEMPTYCHECK_RETURN( pszPath );
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",            "remove_symlink" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",               pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_baseline", pszRepoPath)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_destination",  pszPath    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",            pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	pPlan->stats.nrFilesDeleted++;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

void SG_wd_plan__remove_directory(SG_context * pCtx,
								  SG_wd_plan * pPlan,
								  const char * pszGid,
								  const char * pszRepoPath,
								  const char * pszPath,
								  const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath );
	SG_NONEMPTYCHECK_RETURN( pszPath );
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",            "remove_directory" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",               pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_baseline", pszRepoPath)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_destination",  pszPath    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",            pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	pPlan->stats.nrDirsDeleted++;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

//////////////////////////////////////////////////////////////////

void SG_wd_plan__add_new_directory_to_pendingtree(SG_context * pCtx,
												  SG_wd_plan * pPlan,
												  const char * pszGid,
												  const char * pszRepoPath,
												  const char * pszPath,
												  const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath );
	SG_NONEMPTYCHECK_RETURN( pszPath );
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",               "add_new_directory" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",                  pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_destination", pszRepoPath)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_destination",     pszPath    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",               pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	// NO stats for tmp dirs for parking lot
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

void SG_wd_plan__unadd_new_directory(SG_context * pCtx,
									 SG_wd_plan * pPlan,
									 const char * pszGid,
									 const char * pszRepoPath,
									 const char * pszPath,
									 const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath );
	SG_NONEMPTYCHECK_RETURN( pszPath );
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",   "unadd_new_directory")  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",      pszGid)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath", pszRepoPath)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path",     pszPath)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",   pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	// NO stats for tmp dirs for parking lot
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

//////////////////////////////////////////////////////////////////

void SG_wd_plan__get_file_from_repo(SG_context * pCtx,
									SG_wd_plan * pPlan,
									const char * pszGid,
									const char * pszRepoPath_FinalResult,
									const char * pszPath,
									const char * pszHid_Blob,
									SG_int64 attrBits,
									const char * pszHid_XAttr,
									const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath_FinalResult );
	SG_NONEMPTYCHECK_RETURN( pszPath );
	SG_NONEMPTYCHECK_RETURN( pszHid_Blob );
	// pszHid_XAttr is optional
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",               "get_file" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",                  pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_finalresult", pszRepoPath_FinalResult)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_destination",     pszPath    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "hid",                  pszHid_Blob    )  );
	SG_ERR_CHECK(  SG_vhash__add__int64     (pCtx, pvhItem, "attrbits",             attrBits)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "xattrs",               pszHid_XAttr  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",               pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	pPlan->stats.nrFilesAdded++;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

void SG_wd_plan__get_file_from_auto_merge_result(SG_context * pCtx,
												 SG_wd_plan * pPlan,
												 const char * pszGid,
												 const char * pszRepoPath_FinalResult,
												 const char * pszPath,
												 const char * pszPath_AutoMergeResult,
												 SG_int64 attrBits,
												 const char * pszHid_XAttr,
												 const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath_FinalResult );
	SG_NONEMPTYCHECK_RETURN( pszPath );
	SG_NONEMPTYCHECK_RETURN( pszPath_AutoMergeResult );
	// pszHid_XAttr is optional
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",               "get_file_auto_merge" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",                  pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_finalresult", pszRepoPath_FinalResult)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_destnation",      pszPath    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_source",          pszPath_AutoMergeResult    )  );
	SG_ERR_CHECK(  SG_vhash__add__int64     (pCtx, pvhItem, "attrbits",             attrBits)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "xattrs",               pszHid_XAttr  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",               pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	pPlan->stats.nrFilesAdded++;
	pPlan->stats.nrFilesAutoMerged++;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

void SG_wd_plan__get_symlink(SG_context * pCtx,
							 SG_wd_plan * pPlan,
							 const char * pszGid,
							 const char * pszRepoPath_FinalResult,
							 const char * pszPath,
							 const char * pszHid_Blob,
							 SG_int64 attrBits,
							 const char * pszHid_XAttr,
							 const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath_FinalResult );
	SG_NONEMPTYCHECK_RETURN( pszPath );
	SG_NONEMPTYCHECK_RETURN( pszHid_Blob );
	// pszHid_XAttr is optional
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",               "get_symlink" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",                  pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_finalresult", pszRepoPath_FinalResult)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_destination",     pszPath    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "hid",                  pszHid_Blob    )  );
	SG_ERR_CHECK(  SG_vhash__add__int64     (pCtx, pvhItem, "attrbits",             attrBits)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "xattrs",               pszHid_XAttr  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",               pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	pPlan->stats.nrFilesAdded++;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

void SG_wd_plan__get_directory(SG_context * pCtx,
							   SG_wd_plan * pPlan,
							   const char * pszGid,
							   const char * pszRepoPath_FinalResult,
							   const char * pszPath,
							   SG_int64 attrBits,
							   const char * pszHid_XAttr,
							   const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath_FinalResult );
	SG_NONEMPTYCHECK_RETURN( pszPath );
	// pszHid_XAttr is optional
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",               "get_directory" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",                  pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_finalresult", pszRepoPath_FinalResult)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_destination",     pszPath    )  );
	SG_ERR_CHECK(  SG_vhash__add__int64     (pCtx, pvhItem, "attrbits",             attrBits)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "xattrs",               pszHid_XAttr  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",               pszReason  )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	pPlan->stats.nrDirsAdded++;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

//////////////////////////////////////////////////////////////////

void SG_wd_plan__alter_file_from_repo(SG_context * pCtx,
									  SG_wd_plan * pPlan,
									  const char * pszGid,
									  const char * pszRepoPath_FinalResult,  const char * pszRepoPath_Baseline,
									  const char * pszPath_FinalResult,      const char * pszPath_Baseline,
									  const char * pszHid_Blob_FinalResult,  const char * pszHid_Blob_Baseline,
									  SG_int64 attrBits_FinalResult,         SG_int64 attrBits_Baseline,
									  const char * pszHid_XAttr_FinalResult, const char * pszHid_XAttr_Baseline,
									  const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath_FinalResult );    SG_NONEMPTYCHECK_RETURN( pszRepoPath_Baseline );
	SG_NONEMPTYCHECK_RETURN( pszPath_FinalResult );        SG_NONEMPTYCHECK_RETURN( pszPath_Baseline );
	SG_NONEMPTYCHECK_RETURN( pszHid_Blob_FinalResult );    SG_NONEMPTYCHECK_RETURN( pszHid_Blob_Baseline );
	// pszHid_XAttr_{FinalResult,Baseline} are optional
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",               "alter_file" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",                  pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_finalresult", pszRepoPath_FinalResult  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_baseline",    pszRepoPath_Baseline     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_finalresult",     pszPath_FinalResult      )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_baseline",        pszPath_Baseline         )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "hid_finalresult",      pszHid_Blob_FinalResult  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "hid_baseline",         pszHid_Blob_Baseline     )  );
	SG_ERR_CHECK(  SG_vhash__add__int64     (pCtx, pvhItem, "attrbits_finalresult", attrBits_FinalResult     )  );
	SG_ERR_CHECK(  SG_vhash__add__int64     (pCtx, pvhItem, "attrbits_baseline",    attrBits_Baseline        )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "xattrs_finalresult",   pszHid_XAttr_FinalResult )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "xattrs_baseline",      pszHid_XAttr_Baseline    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",               pszReason                )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	pPlan->stats.nrFilesChanged++;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

void SG_wd_plan__alter_file_from_auto_merge_result(SG_context * pCtx,
												   SG_wd_plan * pPlan,
												   const char * pszGid,
												   const char * pszRepoPath_FinalResult,  const char * pszRepoPath_Baseline,
												   const char * pszPath_FinalResult,      const char * pszPath_Baseline,
												   const char * pszPath_AutoMergeResult,  const char * pszHid_Blob_Baseline,
												   SG_int64 attrBits_FinalResult,         SG_int64 attrBits_Baseline,
												   const char * pszHid_XAttr_FinalResult, const char * pszHid_XAttr_Baseline,
												   const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath_FinalResult );    SG_NONEMPTYCHECK_RETURN( pszRepoPath_Baseline );
	SG_NONEMPTYCHECK_RETURN( pszPath_FinalResult );        SG_NONEMPTYCHECK_RETURN( pszPath_Baseline );
	SG_NONEMPTYCHECK_RETURN( pszPath_AutoMergeResult );    SG_NONEMPTYCHECK_RETURN( pszHid_Blob_Baseline );
	// pszHid_XAttr_{FinalResult,Baseline} are optional
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",               "alter_file_auto_merge" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",                  pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_finalresult", pszRepoPath_FinalResult  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_baseline",    pszRepoPath_Baseline     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_finalresult",     pszPath_FinalResult      )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_baseline",        pszPath_Baseline         )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_source",          pszPath_AutoMergeResult  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "hid_baseline",         pszHid_Blob_Baseline     )  );
	SG_ERR_CHECK(  SG_vhash__add__int64     (pCtx, pvhItem, "attrbits_finalresult", attrBits_FinalResult     )  );
	SG_ERR_CHECK(  SG_vhash__add__int64     (pCtx, pvhItem, "attrbits_baseline",    attrBits_Baseline        )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "xattrs_finalresult",   pszHid_XAttr_FinalResult )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "xattrs_baseline",      pszHid_XAttr_Baseline    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",               pszReason                )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	pPlan->stats.nrFilesChanged++;
	pPlan->stats.nrFilesAutoMerged++;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

void SG_wd_plan__alter_symlink(SG_context * pCtx,
							   SG_wd_plan * pPlan,
							   const char * pszGid,
							   const char * pszRepoPath_FinalResult,  const char * pszRepoPath_Baseline,
							   const char * pszPath_FinalResult,      const char * pszPath_Baseline,
							   const char * pszHid_Blob_FinalResult,  const char * pszHid_Blob_Baseline,
							   SG_int64 attrBits_FinalResult,         SG_int64 attrBits_Baseline,
							   const char * pszHid_XAttr_FinalResult, const char * pszHid_XAttr_Baseline,
							   const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath_FinalResult );    SG_NONEMPTYCHECK_RETURN( pszRepoPath_Baseline );
	SG_NONEMPTYCHECK_RETURN( pszPath_FinalResult );        SG_NONEMPTYCHECK_RETURN( pszPath_Baseline );
	SG_NONEMPTYCHECK_RETURN( pszHid_Blob_FinalResult );    SG_NONEMPTYCHECK_RETURN( pszHid_Blob_Baseline );
	// pszHid_XAttr_{FinalResult,Baseline} are optional
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",               "alter_symlink" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",                  pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_finalresult", pszRepoPath_FinalResult  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_baseline",    pszRepoPath_Baseline     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_finalresult",     pszPath_FinalResult      )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_baseline",        pszPath_Baseline         )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "hid_finalresult",      pszHid_Blob_FinalResult  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "hid_baseline",         pszHid_Blob_Baseline     )  );
	SG_ERR_CHECK(  SG_vhash__add__int64     (pCtx, pvhItem, "attrbits_finalresult", attrBits_FinalResult     )  );
	SG_ERR_CHECK(  SG_vhash__add__int64     (pCtx, pvhItem, "attrbits_baseline",    attrBits_Baseline        )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "xattrs_finalresult",   pszHid_XAttr_FinalResult )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "xattrs_baseline",      pszHid_XAttr_Baseline    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",               pszReason                )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	pPlan->stats.nrFilesChanged++;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

void SG_wd_plan__alter_directory(SG_context * pCtx,
								 SG_wd_plan * pPlan,
								 const char * pszGid,
								 const char * pszRepoPath_FinalResult,  const char * pszRepoPath_Baseline,
								 const char * pszPath_FinalResult,      const char * pszPath_Baseline,
								 SG_int64 attrBits_FinalResult,         SG_int64 attrBits_Baseline,
								 const char * pszHid_XAttr_FinalResult, const char * pszHid_XAttr_Baseline,
								 const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NONEMPTYCHECK_RETURN( pszGid );
	SG_NONEMPTYCHECK_RETURN( pszRepoPath_FinalResult );    SG_NONEMPTYCHECK_RETURN( pszRepoPath_Baseline );
	SG_NONEMPTYCHECK_RETURN( pszPath_FinalResult );        SG_NONEMPTYCHECK_RETURN( pszPath_Baseline );
	// pszHid_XAttr_{FinalResult,Baseline} are optional
	SG_NONEMPTYCHECK_RETURN( pszReason );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",               "alter_directory" )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "gid",                  pszGid     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_finalresult", pszRepoPath_FinalResult  )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "repopath_baseline",    pszRepoPath_Baseline     )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_finalresult",     pszPath_FinalResult      )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_baseline",        pszPath_Baseline         )  );
	SG_ERR_CHECK(  SG_vhash__add__int64     (pCtx, pvhItem, "attrbits_finalresult", attrBits_FinalResult     )  );
	SG_ERR_CHECK(  SG_vhash__add__int64     (pCtx, pvhItem, "attrbits_baseline",    attrBits_Baseline        )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "xattrs_finalresult",   pszHid_XAttr_FinalResult )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "xattrs_baseline",      pszHid_XAttr_Baseline    )  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",               pszReason                )  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	pPlan->stats.nrDirsChanged++;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

//////////////////////////////////////////////////////////////////

void SG_wd_plan__move_entry(SG_context * pCtx,
							SG_wd_plan * pPlan,
							const SG_pathname * pPath_Source,
							const SG_pathname * pPath_Destination,
							const char * pszReason)
{
	SG_vhash * pvhItem = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NULLARGCHECK_RETURN( pPath_Source );
	SG_NULLARGCHECK_RETURN( pPath_Destination );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "action",           "move_entry")  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_source",      SG_pathname__sz(pPath_Source))  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "path_destination", SG_pathname__sz(pPath_Destination))  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhItem, "reason",           pszReason)  );

	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pPlan->pvaScript, &pvhItem)  );
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhItem);
}

//////////////////////////////////////////////////////////////////

void SG_wd_plan_debug__dump_script(SG_context * pCtx, const SG_wd_plan * pPlan, const char * pszLabel)
{
	SG_vhash * pvhItem;
	SG_string * pString = NULL;
	SG_uint32 k, count;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, ("WD_Plan Script: %s\n"), ((pszLabel) ? pszLabel : ""))  );

	SG_ERR_CHECK(  SG_varray__count(pCtx, pPlan->pvaScript, &count)  );
	for (k=0; k<count; k++)
	{
		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pPlan->pvaScript, k, &pvhItem)  );

		SG_ERR_CHECK(  SG_string__clear(pCtx, pString)  );
		SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvhItem, pString)  );
		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR ,"%s\n", SG_string__sz(pString))  );
	}

fail:
    SG_STRING_NULLFREE(pCtx, pString);
}

//void SG_wd_plan_debug__dump_status(SG_context * pCtx, SG_wd_plan * pPlan, const char * pszLabel)
//{
//	SG_rbtree_iterator * pIter = NULL;
//	const char * pszKey;
//	const char * pszValue;
//	SG_bool bOK;
//
//	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, ("WD_Plan Status: %s\n"), ((pszLabel) ? pszLabel : ""))  );
//
//	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, pPlan->prbStatus, &bOK, &pszKey, &pszValue)  );
//	while (bOK)
//	{
//		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "    [%s]: %s\n", pszKey, pszValue)  );
//
//		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter, &bOK, &pszKey, &pszValue)  );
//	}
//
//fail:
//	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
//}

//////////////////////////////////////////////////////////////////

static void _split_path(SG_context * pCtx, const SG_pathname * pPathIn,
						SG_pathname ** ppPathDir, SG_string ** ppStringEntryname)
{
	SG_string * pStringEntryname = NULL;
	SG_pathname * pPathDir = NULL;

	SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pPathIn, &pStringEntryname)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathDir,  pPathIn)  );
	SG_ERR_CHECK(  SG_pathname__remove_last(pCtx, pPathDir)  );

	*ppPathDir = pPathDir;
	*ppStringEntryname = pStringEntryname;
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pStringEntryname);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir);
}

/**
 * There are several places where we need to do a MOVE, RENAME, or MOVE+RENAME
 * and the SG_pendingtree API is a pain.
 *
 * There is a little ambiguity here because we did not distinguish them when
 * building the plan *AND* the low-level SG_fsobj routines don't care, it is
 * only the SG_pendingtree routines that make this artificial distinction.
 * So I'd like to fix the SG_pendingtree API rather than perpetuate this.
 *
 * TODO 2010/05/07 See SPRAWL-661 for fixing __move/__rename API.
 */
static void _do_move_rename(SG_context * pCtx, SG_pendingtree * pPendingTree,
							const SG_pathname * pPathSrc,
							const SG_pathname * pPathDest)
{
	SG_pathname * pPathSrc_Dir = NULL;
	SG_pathname * pPathDest_Dir = NULL;
	SG_string * pStringSrc_Entryname = NULL;
	SG_string * pStringDest_Entryname = NULL;
	SG_bool bSameDir, bSameEntryname;

	if (strcmp(SG_pathname__sz(pPathSrc),
			   SG_pathname__sz(pPathDest)) == 0)
		return;

	// split <old_dir>/<old_name> into parts
	SG_ERR_CHECK(  _split_path(pCtx, pPathSrc, &pPathSrc_Dir, &pStringSrc_Entryname)  );

	// split <new_dir>/<new_name> into parts
	SG_ERR_CHECK(  _split_path(pCtx, pPathDest, &pPathDest_Dir, &pStringDest_Entryname)  );

	bSameDir = (strcmp(SG_pathname__sz(pPathSrc_Dir),
					   SG_pathname__sz(pPathDest_Dir)) == 0);
	bSameEntryname = (strcmp(SG_string__sz(pStringSrc_Entryname),
							 SG_string__sz(pStringDest_Entryname)) == 0);

	if (bSameDir)
	{
		if (bSameEntryname)
		{
			SG_ASSERT(  (0)  );		// already handled above.
		}
		else
		{
			// RENAME <old_dir>/<old_name> ==> <old_dir>/<new_name>

			SG_ERR_CHECK(  SG_pendingtree__rename_dont_save_pendingtree(pCtx, pPendingTree,
																		pPathSrc,
																		SG_string__sz(pStringDest_Entryname),
																		SG_TRUE)  );
		}
	}
	else
	{
		if (bSameEntryname)
		{
			// MOVE <old_dir>/<old_name> ==> <new_dir>/

			SG_ERR_CHECK(  SG_pendingtree__move_dont_save_pendingtree(pCtx, pPendingTree,
																	  pPathSrc, pPathDest_Dir,
																	  SG_FALSE)  );
		}
		else
		{
			// MOVE+RENAME <old_dir>/<old_name> ==> <new_dir>/<new_name>
			//
			// TODO because of the problem in the pendingtree API, I can't
			// TODO do this in one operation.  Furthermore, we know that
			// TODO there are no entryname collisions with <new_dir>/<new_name>
			// TODO ***BUT*** we don't know about the <old_dir>/<new_name> or
			// TODO <new_dir>/<old_name>.
			// TODO
			// TODO 2010/05/07 for now, i'm going to ignore this and wait for 661
			// TODO            to get fixed.

			// MOVE <old_dir>/<old_name> ==> <new_dir>/<old_name>

			SG_ERR_CHECK(  SG_pendingtree__move_dont_save_pendingtree(pCtx, pPendingTree,
																	  pPathSrc, pPathDest_Dir,
																	  SG_FALSE)  );

			// RENAME <new_dir>/<old_name> ==> <new_dir>/<new_name>

			SG_ERR_CHECK(  SG_pathname__append__from_string(pCtx, pPathDest_Dir, pStringSrc_Entryname)  );
			SG_ERR_CHECK(  SG_pendingtree__rename_dont_save_pendingtree(pCtx, pPendingTree,
																		pPathDest_Dir,
																		SG_string__sz(pStringDest_Entryname),
																		SG_TRUE)  );
		}
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc_Dir);
	SG_PATHNAME_NULLFREE(pCtx, pPathDest_Dir);
	SG_STRING_NULLFREE(pCtx, pStringSrc_Entryname);
	SG_STRING_NULLFREE(pCtx, pStringDest_Entryname);
}

static SG_bool _my_equal(const char * psz1, const char * psz2)
{
	SG_bool bEmpty1 = !(psz1 && *psz1);
	SG_bool bEmpty2 = !(psz2 && *psz2);

	if (bEmpty1 && bEmpty2)
		return SG_TRUE;

	if (bEmpty1 || bEmpty2)
		return SG_FALSE;

	return (strcmp(psz1, psz2) == 0);
}

static void _apply_xattrs(SG_context * pCtx, SG_repo * pRepo,
						  const SG_pathname * pPath,
						  const char * pszHid_XAttrs, SG_bool bClearXAttrsIfNull)
{
	SG_vhash * pvhXAttrs = NULL;

	// attrbits and xattrs don't go thru the SG_pendingtree API.
	// we set them on the entry in the WD and then pendingtree
	// notices them during a scan_dir.

	if (pszHid_XAttrs && *pszHid_XAttrs)
	{
		SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, pRepo, pszHid_XAttrs, &pvhXAttrs)  );
#ifdef SG_BUILD_FLAG_FEATURE_XATTR
		SG_ERR_CHECK(  SG_attributes__xattrs__apply(pCtx, pPath, pvhXAttrs, pRepo)  );
#else
		pPath = NULL;	// quiet compiler on windows.
#endif
	}
	else if (bClearXAttrsIfNull)
	{
		// TODO clear existing XAttrs on disk.
	}

fail:
	SG_VHASH_NULLFREE(pCtx, pvhXAttrs);
}


//////////////////////////////////////////////////////////////////

static void _fetch_path(SG_context * pCtx, const SG_vhash * pvhItem, const char * pszKey, SG_pathname ** ppPath)
{
	const char * psz;

	SG_ERR_CHECK_RETURN(  SG_vhash__get__sz(pCtx, pvhItem, pszKey, &psz)  );
	SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__SZ(pCtx, ppPath, psz)  );
}

#define FETCHPATH(key,p) SG_STATEMENT(  SG_ERR_CHECK(  _fetch_path(pCtx, pvhItem, (key), (p))  );  )
#define FETCHSZ(key,p)   SG_STATEMENT(  SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhItem, (key), (p))  );  )
#define FETCHI64(key,p)  SG_STATEMENT(  SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvhItem, (key), (p))  );  )

//////////////////////////////////////////////////////////////////

#define DCLVERB(fn) static void fn(SG_context * pCtx, const SG_vhash * pvhItem, SG_pendingtree * pPendingTree)

//////////////////////////////////////////////////////////////////

DCLVERB(fn_move_rename)		// MOVE and/or RENAME entry in WD and in pendingtree.
{
	SG_pathname * pPathSrc = NULL;
	SG_pathname * pPathDest = NULL;

	FETCHPATH("path_source",      &pPathSrc);
	FETCHPATH("path_destination", &pPathDest);

	SG_ERR_CHECK(  _do_move_rename(pCtx, pPendingTree, pPathSrc, pPathDest)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc);
	SG_PATHNAME_NULLFREE(pCtx, pPathDest);
}

DCLVERB(fn_remove_generic)	// REMOVE entry from WD and pendingtree.
{
	SG_pathname * pPath = NULL;
	SG_pathname * pPath_Dir = NULL;
	SG_string * pString_Entryname = NULL;
	const char * pszEntryname;

	FETCHPATH("path_destination", &pPath);

	/**
	 * TODO 2010/05/08 Once again the SG_pendingtree API is a pain here.  I just want to remove
	 * TODO            a single file/symlink/directory but I need to split the path and pass the
	 * TODO            parent directory and an array of entrynames.
	 * TODO
	 * TODO            Move this to pendingtree.... see SPRAWL-662.
	 */
	SG_ERR_CHECK(  _split_path(pCtx, pPath, &pPath_Dir, &pString_Entryname)  );
	pszEntryname = SG_string__sz(pString_Entryname);

	SG_ERR_CHECK(  SG_pendingtree__remove_dont_save_pendingtree(pCtx,
																pPendingTree,
																pPath_Dir,
																1, &pszEntryname,
																NULL, 0,
																NULL, 0,
																NULL, 0, SG_FALSE)  );
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_PATHNAME_NULLFREE(pCtx, pPath_Dir);
	SG_STRING_NULLFREE(pCtx, pString_Entryname);
}

DCLVERB(fn_add_new_directory)	// ADD an EXISTING directory on disk to the PENDINGTREE.
{
	SG_pathname * pPath = NULL;
	const char * pszGid;

	FETCHPATH("path_destination", &pPath);
	FETCHSZ(  "gid",              &pszGid);

	SG_ERR_CHECK(  SG_pendingtree__add_dont_save_pendingtree__with_gid(pCtx, pPendingTree, pszGid, pPath)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

DCLVERB(fn_unadd_new_directory)	// UN-ADD a directory from the PENDINGTREE, but don't touch the WD.
{
	SG_pathname * pPath = NULL;
	const char * pszGid;

	FETCHPATH("path", &pPath);
	FETCHSZ(  "gid",  &pszGid);

	SG_ERR_CHECK(  SG_pendingtree__hack__unadd_dont_save_pendingtree(pCtx, pPendingTree, pszGid, pPath)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

DCLVERB(fn_get_file)			// Fetch a FILE from the REPO and do a special ADD with existing GID.
{
	SG_pathname * pPath = NULL;
	SG_file * pFile = NULL;
	SG_repo * pRepo;
	const char * pszGid;
	const char * pszHid_Blob;
	const char * pszHid_XAttrs;
	SG_int64 attrBits;

	FETCHPATH("path_destination", &pPath);
	FETCHSZ(  "gid",              &pszGid);
	FETCHSZ(  "hid",              &pszHid_Blob);
	FETCHSZ(  "xattrs",           &pszHid_XAttrs);
	FETCHI64( "attrbits",         &attrBits);

	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_WRONLY|SG_FILE_CREATE_NEW, 0600, &pFile)  );
	SG_ERR_CHECK(  SG_repo__fetch_blob_into_file(pCtx, pRepo, pszHid_Blob, pFile, NULL)  );
	SG_FILE_NULLCLOSE(pCtx, pFile);

	// set attrbits and xattrs (if there are any).
	// don't bother clearing XAttrs since we just created it and it won't have any.
	SG_ERR_CHECK(  SG_attributes__bits__apply(pCtx, pPath, attrBits)  );
	SG_ERR_CHECK(  _apply_xattrs(pCtx, pRepo, pPath, pszHid_XAttrs, SG_FALSE)  );

	// do special ADD so that the it gets the same GID as it had in the other branch.

	SG_ERR_CHECK(  SG_pendingtree__add_dont_save_pendingtree__with_gid(pCtx, pPendingTree, pszGid, pPath)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_FILE_NULLCLOSE(pCtx, pFile);
}

DCLVERB(fn_get_file_auto_merge)			// Fetch a FILE from an auto-merge-result and do a special ADD with existing GID.
{
	SG_pathname * pPath = NULL;
	SG_pathname * pPath_AutoMergeResult = NULL;
	SG_file * pFile = NULL;
	SG_repo * pRepo;
	const char * pszGid;
	const char * pszHid_XAttrs;
	SG_int64 attrBits;

	FETCHPATH("path_destination", &pPath);
	FETCHPATH("path_source",      &pPath_AutoMergeResult);
	FETCHSZ(  "gid",              &pszGid);
	FETCHSZ(  "xattrs",           &pszHid_XAttrs);
	FETCHI64( "attrbits",         &attrBits);

	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

	// steal the auto-merge-result file from the tmp directory and
	// move/rename it in the WD.  this saves a copy/delete.

	SG_ERR_CHECK(  _do_move_rename(pCtx, pPendingTree, pPath_AutoMergeResult, pPath)  );

	// set attrbits and xattrs (if there are any).
	// don't bother clearing XAttrs since A_A tmp file won't have any.
	SG_ERR_CHECK(  SG_attributes__bits__apply(pCtx, pPath, attrBits)  );
	SG_ERR_CHECK(  _apply_xattrs(pCtx, pRepo, pPath, pszHid_XAttrs, SG_FALSE)  );

	// do special ADD so that the it gets the same GID as it had in the other branch.

	SG_ERR_CHECK(  SG_pendingtree__add_dont_save_pendingtree__with_gid(pCtx, pPendingTree, pszGid, pPath)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_FILE_NULLCLOSE(pCtx, pFile);
}

DCLVERB(fn_get_symlink)			// Fetch a SYMLINK from the REPO and do a special ADD with existing GID.
{
	SG_pathname * pPath = NULL;
	SG_byte * pBytes = NULL;
	SG_string * pStringLink = NULL;
	SG_repo * pRepo;
	const char * pszGid;
	const char * pszHid_Blob;
	const char * pszHid_XAttrs;
	SG_int64 attrBits;
	SG_uint64 nrBytes;

	FETCHPATH("path_destination", &pPath);
	FETCHSZ(  "gid",              &pszGid);
	FETCHSZ(  "hid",              &pszHid_Blob);
	FETCHSZ(  "xattrs",           &pszHid_XAttrs);
	FETCHI64( "attrbits",         &attrBits);

	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

	SG_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pRepo, pszHid_Blob, &pBytes, &nrBytes)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC__BUF_LEN(pCtx, &pStringLink, pBytes, (SG_uint32)nrBytes)  );
	SG_NULLFREE(pCtx, pBytes);

	SG_ERR_CHECK(  SG_fsobj__symlink(pCtx, pStringLink, pPath)  );

	// set attrbits and xattrs (if there are any).
	// don't bother clearing XAttrs since we just created the symlink and it won't have any.
	SG_ERR_CHECK(  SG_attributes__bits__apply(pCtx, pPath, attrBits)  );
	SG_ERR_CHECK(  _apply_xattrs(pCtx, pRepo, pPath, pszHid_XAttrs, SG_FALSE)  );

	// do special ADD so that the it gets the same GID as it had in the other branch.

	SG_ERR_CHECK(  SG_pendingtree__add_dont_save_pendingtree__with_gid(pCtx, pPendingTree, pszGid, pPath)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_STRING_NULLFREE(pCtx, pStringLink);
	SG_NULLFREE(pCtx, pBytes);
}

DCLVERB(fn_get_directory)			// Fetch a DIRECTORY (non-recursive) from the REPO and do a special ADD with existing GID.
{
	SG_pathname * pPath = NULL;
	SG_repo * pRepo;
	const char * pszGid;
	const char * pszHid_XAttrs;
	SG_int64 attrBits;

	FETCHPATH("path_destination", &pPath);
	FETCHSZ(  "gid",              &pszGid);
	FETCHSZ(  "xattrs",           &pszHid_XAttrs);
	FETCHI64( "attrbits",         &attrBits);

	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

	SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath)  );

	// set attrbits and xattrs (if there are any).
	// don't bother clearing XAttrs since we just created the directory and it won't have any.
	SG_ERR_CHECK(  SG_attributes__bits__apply(pCtx, pPath, attrBits)  );
	SG_ERR_CHECK(  _apply_xattrs(pCtx, pRepo, pPath, pszHid_XAttrs, SG_FALSE)  );

	// do special ADD so that the it gets the same GID as it had in the other branch.

	SG_ERR_CHECK(  SG_pendingtree__add_dont_save_pendingtree__with_gid(pCtx, pPendingTree, pszGid, pPath)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

//////////////////////////////////////////////////////////////////

DCLVERB(fn_alter_file)			// convert existing WD/baseline FILE to final-result
{
	SG_pathname * pPathSrc  = NULL;
	SG_pathname * pPathDest = NULL;
	SG_file * pFile = NULL;
	SG_repo * pRepo;
	const char * pszHid_Blob;
	const char * pszHid_XAttrs_Src;
	const char * pszHid_XAttrs_Dest;
	SG_int64 attrBits_Src;
	SG_int64 attrBits_Dest;

	FETCHPATH("path_finalresult",     &pPathDest);
	FETCHPATH("path_baseline",        &pPathSrc);
	FETCHSZ(  "hid_finalresult",      &pszHid_Blob);
	FETCHSZ(  "xattrs_finalresult",   &pszHid_XAttrs_Dest);
	FETCHSZ(  "xattrs_baseline",      &pszHid_XAttrs_Src);
	FETCHI64( "attrbits_finalresult", &attrBits_Dest);
	FETCHI64( "attrbits_baseline",    &attrBits_Src);

	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

	// I know this sounds stupid, but there are a few
	// strange/redundant things here that are necessary
	// yet look like they could be omitted.
	//
	// [1] MOVE, RENAME, or MOVE+RENAME the file from pPathSrc to
	//     pPathDest.  The WD disk move is the stupid part, but we
	//     need to do this for the side-effects on the pendingtree.

	SG_ERR_CHECK(  _do_move_rename(pCtx, pPendingTree, pPathSrc, pPathDest)  );

	// [2] truncate pPathDest and fetch the content from the repo
	//     using the HID of the final-result.

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathDest, SG_FILE_WRONLY|SG_FILE_OPEN_EXISTING|SG_FILE_TRUNC, 0600, &pFile)  );
	SG_ERR_CHECK(  SG_repo__fetch_blob_into_file(pCtx, pRepo, pszHid_Blob, pFile, NULL)  );
	SG_FILE_NULLCLOSE(pCtx, pFile);

	// [3] apply any changes in attrbits/xattrs.  But since we did
	//     the MOVE/RENAME in [1] the file should still have the
	//     settings from the baseline, so we only need to do this
	//     if they are different.

	if (attrBits_Src != attrBits_Dest)
		SG_ERR_CHECK(  SG_attributes__bits__apply(pCtx, pPathDest, attrBits_Dest)  );
	if (_my_equal(pszHid_XAttrs_Src, pszHid_XAttrs_Dest))
		SG_ERR_CHECK(  _apply_xattrs(pCtx, pRepo, pPathDest, pszHid_XAttrs_Dest, SG_TRUE)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDest);
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc);
	SG_FILE_NULLCLOSE(pCtx, pFile);
}

DCLVERB(fn_alter_file_auto_merge)	// convert existing WD/baseline FILE to final-result (but use auto-merge-result for content)
{
	SG_pathname * pPathSrc  = NULL;
	SG_pathname * pPathDest = NULL;
	SG_pathname * pPathAutoMergeResult = NULL;
	SG_repo * pRepo;
	const char * pszHid_XAttrs;
	SG_int64 attrBits;

	FETCHPATH("path_finalresult",     &pPathDest);
	FETCHPATH("path_baseline",        &pPathSrc);
	FETCHPATH("path_source",          &pPathAutoMergeResult);
	FETCHSZ(  "xattrs_finalresult",   &pszHid_XAttrs);
	FETCHI64( "attrbits_finalresult", &attrBits);

	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

	// As above, there are a few strange/redundant things here:
	//
	// [1] MOVE, RENAME, or MOVE+RENAME the file from pPathSrc to pPathDest
	//     to get the side-effects on pendingtree.

	SG_ERR_CHECK(  _do_move_rename(pCtx, pPendingTree, pPathSrc, pPathDest)  );

	// [2] secretly delete the file we just moved and MOVE/RENAME
	//     the auto-merge-result file into the spot (to get the new
	//     content).  We do not tell pendingtree about the delete or
	//     move/rename (so the GID isn't affected).

	SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPathDest)  );
	SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx, pPathAutoMergeResult, pPathDest)  );

	// [3] set the attrbits/xattrs (because values from auto-merge-result-file aren't right)

	SG_ERR_CHECK(  SG_attributes__bits__apply(pCtx, pPathDest, attrBits)  );
	SG_ERR_CHECK(  _apply_xattrs(pCtx, pRepo, pPathDest, pszHid_XAttrs, SG_FALSE)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDest);
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc);
	SG_PATHNAME_NULLFREE(pCtx, pPathAutoMergeResult);
}

DCLVERB(fn_alter_symlink)			// convert existing WD/baseline SYMLINK to final-result
{
	SG_pathname * pPathSrc  = NULL;
	SG_pathname * pPathDest = NULL;
	SG_byte * pBytes = NULL;
	SG_string * pStringLink = NULL;
	SG_repo * pRepo;
	const char * pszHid_Blob;
	const char * pszHid_XAttrs;
	SG_int64 attrBits;
	SG_uint64 nrBytes;

	FETCHPATH("path_finalresult",     &pPathDest);
	FETCHPATH("path_baseline",        &pPathSrc);
	FETCHSZ(  "hid_finalresult",      &pszHid_Blob);
	FETCHSZ(  "xattrs_finalresult",   &pszHid_XAttrs);
	FETCHI64( "attrbits_finalresult", &attrBits);

	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

	// [1] MOVE, RENAME, or MOVE+RENAME the symlink from pPathSrc to
	//     pPathDest.  The WD disk move is the stupid part, but we
	//     need to do this for the side-effects on the pendingtree.

	SG_ERR_CHECK(  _do_move_rename(pCtx, pPendingTree, pPathSrc, pPathDest)  );

	// [2] set the new symlink target.  but since the OS won't let us
	//     modify the value of a symlink, we secretly delete it and
	//     recreate it with the new targetvalue using the HID of
	//     the final-result.

	SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPathDest)  );

	SG_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pRepo, pszHid_Blob, &pBytes, &nrBytes)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC__BUF_LEN(pCtx, &pStringLink, pBytes, (SG_uint32)nrBytes)  );
	SG_NULLFREE(pCtx, pBytes);
	SG_ERR_CHECK(  SG_fsobj__symlink(pCtx, pStringLink, pPathDest)  );

	// [3] set the attrbits/xattrs (since the symlink is new).

	SG_ERR_CHECK(  SG_attributes__bits__apply(pCtx, pPathDest, attrBits)  );
	SG_ERR_CHECK(  _apply_xattrs(pCtx, pRepo, pPathDest, pszHid_XAttrs, SG_FALSE)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDest);
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc);
	SG_STRING_NULLFREE(pCtx, pStringLink);
	SG_NULLFREE(pCtx, pBytes);
}

DCLVERB(fn_alter_directory)			// convert existing WD/baseline DIRECTORY (non-recursive) to final-result
{
	SG_pathname * pPathSrc  = NULL;
	SG_pathname * pPathDest = NULL;
	SG_repo * pRepo;
	const char * pszHid_XAttrs_Src;
	const char * pszHid_XAttrs_Dest;
	SG_int64 attrBits_Src;
	SG_int64 attrBits_Dest;

	FETCHPATH("path_finalresult",     &pPathDest);
	FETCHPATH("path_baseline",        &pPathSrc);
	FETCHSZ(  "xattrs_finalresult",   &pszHid_XAttrs_Dest);
	FETCHSZ(  "xattrs_baseline",      &pszHid_XAttrs_Src);
	FETCHI64( "attrbits_finalresult", &attrBits_Dest);
	FETCHI64( "attrbits_baseline",    &attrBits_Src);

	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

	// [1] MOVE, RENAME, or MOVE+RENAME existing directory to get
	//     the change in the filesystem and in the pendingtree.

	SG_ERR_CHECK(  _do_move_rename(pCtx, pPendingTree, pPathSrc, pPathDest)  );

	// [2] We don't touch the contents of the directory.

	// [3] apply any changes in attrbits/xattrs.  But since we did
	//     the MOVE/RENAME in [1] the directory should still have the
	//     settings from the baseline, so we only need to do this
	//     if they are different.

	if (attrBits_Src != attrBits_Dest)
		SG_ERR_CHECK(  SG_attributes__bits__apply(pCtx, pPathDest, attrBits_Dest)  );
	if (_my_equal(pszHid_XAttrs_Src, pszHid_XAttrs_Dest))
		SG_ERR_CHECK(  _apply_xattrs(pCtx, pRepo, pPathDest, pszHid_XAttrs_Dest, SG_TRUE)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDest);
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc);
}

DCLVERB(fn_move_entry)				// move/rename existing WD/baseline entry WITHOUT telling pendingtree
{
	SG_pathname * pPathSrc  = NULL;
	SG_pathname * pPathDest = NULL;

	SG_UNUSED(pPendingTree);

	FETCHPATH("path_destination",     &pPathDest);
	FETCHPATH("path_source",        &pPathSrc);

	SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx, pPathSrc, pPathDest)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDest);
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc);
}

//////////////////////////////////////////////////////////////////

#undef DCLVERB

//////////////////////////////////////////////////////////////////

static SG_varray_foreach_callback _execute_cb;

void _execute_cb(SG_context * pCtx,
				 void * pVoidData,
				 SG_UNUSED_PARAM( const SG_varray * pva ),
				 SG_UNUSED_PARAM( SG_uint32 ndx ),
				 const SG_variant * pv)
{
	SG_pendingtree * pPendingTree = (SG_pendingtree *)pVoidData;
	SG_vhash * pvhItem;
	const char * pszValue_Action;

	SG_UNUSED( pva );
	SG_UNUSED( ndx );

	SG_ERR_CHECK_RETURN(  SG_variant__get__vhash(pCtx, pv, &pvhItem)  );
	SG_ERR_CHECK_RETURN(  SG_vhash__get__sz(pCtx, pvhItem, "action", &pszValue_Action)  );

#define IFVERB(v,fn)	SG_STATEMENT( if (strcmp(pszValue_Action,(v))==0) { SG_ERR_CHECK_RETURN(  (fn)(pCtx, pvhItem, pPendingTree)  ); return; } )

	IFVERB("move_rename",             fn_move_rename);

	IFVERB("remove_file",             fn_remove_generic);	// I'm hoping that all 3 type of
	IFVERB("remove_symlink",          fn_remove_generic);	// entries can be handled by the
	IFVERB("remove_directory",        fn_remove_generic);	// same routine.

	IFVERB("get_file",                fn_get_file);
	IFVERB("get_file_auto_merge",     fn_get_file_auto_merge);
	IFVERB("get_symlink",             fn_get_symlink);
	IFVERB("get_directory",           fn_get_directory);

	IFVERB("alter_file",              fn_alter_file);
	IFVERB("alter_file_auto_merge",   fn_alter_file_auto_merge);
	IFVERB("alter_symlink",           fn_alter_symlink);
	IFVERB("alter_directory",         fn_alter_directory);

	IFVERB("add_new_directory",       fn_add_new_directory);
	IFVERB("unadd_new_directory",     fn_unadd_new_directory);

	IFVERB("move_entry",              fn_move_entry);

	// TODO other commands....

#undef IFVERB

#if TRACE_WD_PLAN
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TODO Unhandled VERB[%s]\n", pszValue_Action)  );
	SG_ASSERT(  (0)  );
#endif

}

void SG_wd_plan__execute(SG_context * pCtx, const SG_wd_plan * pPlan, SG_pendingtree * pPendingTree)
{
	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NULLARGCHECK_RETURN( pPendingTree );

	SG_ERR_CHECK_RETURN(  SG_varray__foreach(pCtx, pPlan->pvaScript, _execute_cb, (void *)pPendingTree)  );
}

//////////////////////////////////////////////////////////////////

void SG_wd_plan__get_stats__ref(SG_context * pCtx, const SG_wd_plan * pPlan, const SG_wd_plan_stats ** ppStats)
{
	SG_NULLARGCHECK_RETURN(pPlan);
	SG_NULLARGCHECK_RETURN(ppStats);

	*ppStats = &pPlan->stats;
}

//////////////////////////////////////////////////////////////////

#define DCLVERB(fn) static void fn(SG_context * pCtx, const SG_vhash * pvhItem, SG_string * pStr)

#define LABEL_FIELD			"%16s: "

static void format_1(SG_context * pCtx, SG_string * pStr,
					 const char * pszVerb,
					 const char * pszReason,
					 const char * pszArg1)
{
	SG_ERR_CHECK_RETURN(  SG_string__append__format(pCtx, pStr,
													(LABEL_FIELD "%s\n" "\t\t# %s\n"),
													pszVerb,
													pszArg1,
													pszReason)  );
}

static void format_2(SG_context * pCtx, SG_string * pStr,
					 const char * pszVerb,
					 const char * pszReason,
					 const char * pszArg1,
					 const char * pszArg2)
{
	SG_ERR_CHECK_RETURN(  SG_string__append__format(pCtx, pStr,
													(LABEL_FIELD "%s %s\n" "\t\t# %s\n"),
													pszVerb,
													pszArg1,
													pszArg2,
													pszReason)  );
}

//////////////////////////////////////////////////////////////////

DCLVERB(fn_format_move_rename)		// MOVE and/or RENAME entry in WD and in pendingtree.
{
	SG_pathname * pPathSrc = NULL;
	SG_pathname * pPathDest = NULL;
	const char * pszReason;

	FETCHPATH("path_source",      &pPathSrc);
	FETCHPATH("path_destination", &pPathDest);
	FETCHSZ(  "reason",           &pszReason);

	SG_ERR_CHECK(  format_2(pCtx, pStr,
							"move_rename", pszReason,
							SG_pathname__sz(pPathSrc), SG_pathname__sz(pPathDest))  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc);
	SG_PATHNAME_NULLFREE(pCtx, pPathDest);
}

DCLVERB(fn_format_remove_generic)	// REMOVE entry from WD and pendingtree.
{
	SG_pathname * pPath = NULL;
	const char * pszReason;

	FETCHPATH("path_destination", &pPath);
	FETCHSZ(  "reason",           &pszReason);

	SG_ERR_CHECK(  format_1(pCtx, pStr,
							"remove", pszReason,
							SG_pathname__sz(pPath))  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

DCLVERB(fn_format_add_new_directory)	// ADD an EXISTING directory on disk to the PENDINGTREE.
{
	// this is just our bookkeeping; the user doesn't need to see it.

	SG_UNUSED(pCtx);
	SG_UNUSED(pvhItem);
	SG_UNUSED(pStr);
}

DCLVERB(fn_format_unadd_new_directory)	// UN-ADD a directory from the PENDINGTREE, but don't touch the WD.
{
	// this is just our bookkeeping; the user doesn't need to see it.

	SG_UNUSED(pCtx);
	SG_UNUSED(pvhItem);
	SG_UNUSED(pStr);
}

DCLVERB(fn_format_get_file)			// Fetch a FILE from the REPO and do a special ADD with existing GID.
{
	SG_pathname * pPath = NULL;
	const char * pszReason;
	const char * pszHid_XAttrs;
	SG_int64 attrBits;

	FETCHPATH("path_destination", &pPath);
	FETCHSZ(  "reason",           &pszReason);
	FETCHSZ(  "xattrs",           &pszHid_XAttrs);
	FETCHI64( "attrbits",         &attrBits);

	SG_ERR_CHECK(  format_1(pCtx, pStr,
							"get_file", pszReason,
							SG_pathname__sz(pPath))  );
	if ((attrBits != 0) || (pszHid_XAttrs && *pszHid_XAttrs))
		SG_ERR_CHECK(  format_1(pCtx, pStr,
								"set_attributes", pszReason,
								SG_pathname__sz(pPath))  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

DCLVERB(fn_format_get_file_auto_merge)			// Fetch a FILE from an auto-merge-result and do a special ADD with existing GID.
{
	SG_pathname * pPath = NULL;
	const char * pszReason;
	const char * pszHid_XAttrs;
	SG_int64 attrBits;

	FETCHPATH("path_destination", &pPath);
	FETCHSZ(  "reason",           &pszReason);
	FETCHSZ(  "xattrs",           &pszHid_XAttrs);
	FETCHI64( "attrbits",         &attrBits);

	SG_ERR_CHECK(  format_1(pCtx, pStr,
							"auto_merge_file", pszReason,
							SG_pathname__sz(pPath))  );
	if ((attrBits != 0) || (pszHid_XAttrs && *pszHid_XAttrs))
		SG_ERR_CHECK(  format_1(pCtx, pStr,
								"set_attributes", pszReason,
								SG_pathname__sz(pPath))  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

DCLVERB(fn_format_get_symlink)			// Fetch a SYMLINK from the REPO and do a special ADD with existing GID.
{
	SG_pathname * pPath = NULL;
	const char * pszReason;
	const char * pszHid_XAttrs;
	SG_int64 attrBits;

	FETCHPATH("path_destination", &pPath);
	FETCHSZ(  "reason",           &pszReason);
	FETCHSZ(  "xattrs",           &pszHid_XAttrs);
	FETCHI64( "attrbits",         &attrBits);

	SG_ERR_CHECK(  format_1(pCtx, pStr,
							"get_symlink", pszReason,
							SG_pathname__sz(pPath))  );
	if ((attrBits != 0) || (pszHid_XAttrs && *pszHid_XAttrs))
		SG_ERR_CHECK(  format_1(pCtx, pStr,
								"set_attributes", pszReason,
								SG_pathname__sz(pPath))  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

DCLVERB(fn_format_get_directory)			// Fetch a DIRECTORY (non-recursive) from the REPO and do a special ADD with existing GID.
{
	SG_pathname * pPath = NULL;
	const char * pszReason;
	const char * pszHid_XAttrs;
	SG_int64 attrBits;

	FETCHPATH("path_destination", &pPath);
	FETCHSZ(  "reason",           &pszReason);
	FETCHSZ(  "xattrs",           &pszHid_XAttrs);
	FETCHI64( "attrbits",         &attrBits);

	SG_ERR_CHECK(  format_1(pCtx, pStr,
							"get_directory", pszReason,
							SG_pathname__sz(pPath))  );
	if ((attrBits != 0) || (pszHid_XAttrs && *pszHid_XAttrs))
		SG_ERR_CHECK(  format_1(pCtx, pStr,
								"set_attributes", pszReason,
								SG_pathname__sz(pPath))  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

//////////////////////////////////////////////////////////////////

DCLVERB(fn_format_alter_file)			// convert existing WD/baseline FILE to final-result
{
	SG_pathname * pPathSrc  = NULL;
	SG_pathname * pPathDest = NULL;
	const char * pszReason;
	const char * pszHid_Blob_FinalResult;
	const char * pszHid_Blob_Baseline;
	const char * pszHid_XAttrs_Src;
	const char * pszHid_XAttrs_Dest;
	SG_int64 attrBits_Src;
	SG_int64 attrBits_Dest;

	FETCHPATH("path_finalresult",     &pPathDest);
	FETCHPATH("path_baseline",        &pPathSrc);
	FETCHSZ(  "reason",               &pszReason);
	FETCHSZ(  "hid_finalresult",      &pszHid_Blob_FinalResult);
	FETCHSZ(  "hid_baseline",         &pszHid_Blob_Baseline);
	FETCHSZ(  "xattrs_finalresult",   &pszHid_XAttrs_Dest);
	FETCHSZ(  "xattrs_baseline",      &pszHid_XAttrs_Src);
	FETCHI64( "attrbits_finalresult", &attrBits_Dest);
	FETCHI64( "attrbits_baseline",    &attrBits_Src);

	if (strcmp(SG_pathname__sz(pPathSrc),
			   SG_pathname__sz(pPathDest)) != 0)
		SG_ERR_CHECK(  format_2(pCtx, pStr,
								"move_rename", pszReason,
								SG_pathname__sz(pPathSrc), SG_pathname__sz(pPathDest))  );
	if (strcmp(pszHid_Blob_Baseline,
			   pszHid_Blob_FinalResult) != 0)
		SG_ERR_CHECK(  format_1(pCtx, pStr,
								"alter_file", pszReason,
								SG_pathname__sz(pPathDest))  );
	if ((attrBits_Src != attrBits_Dest) || (!_my_equal(pszHid_XAttrs_Src, pszHid_XAttrs_Dest)))
		SG_ERR_CHECK(  format_1(pCtx, pStr,
								"set_attributes", pszReason,
								SG_pathname__sz(pPathDest))  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDest);
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc);
}

DCLVERB(fn_format_alter_file_auto_merge)	// convert existing WD/baseline FILE to final-result (but use auto-merge-result for content)
{
	SG_pathname * pPathSrc  = NULL;
	SG_pathname * pPathDest = NULL;
	const char * pszReason;
	const char * pszHid_XAttrs;
	SG_int64 attrBits;

	FETCHPATH("path_finalresult",     &pPathDest);
	FETCHPATH("path_baseline",        &pPathSrc);
	FETCHSZ(  "reason",               &pszReason);
	FETCHSZ(  "xattrs_finalresult",   &pszHid_XAttrs);
	FETCHI64( "attrbits_finalresult", &attrBits);

	if (strcmp(SG_pathname__sz(pPathSrc),
			   SG_pathname__sz(pPathDest)) != 0)
		SG_ERR_CHECK(  format_2(pCtx, pStr,
								"move_rename", pszReason,
								SG_pathname__sz(pPathSrc), SG_pathname__sz(pPathDest))  );
	SG_ERR_CHECK(  format_1(pCtx, pStr,
							"auto_merge_file", pszReason,
							SG_pathname__sz(pPathDest))  );
	if ((attrBits != 0) || (pszHid_XAttrs && *pszHid_XAttrs))
		SG_ERR_CHECK(  format_1(pCtx, pStr,
								"set_attributes", pszReason,
								SG_pathname__sz(pPathDest))  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDest);
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc);
}

DCLVERB(fn_format_alter_symlink)			// convert existing WD/baseline SYMLINK to final-result
{
	SG_pathname * pPathSrc  = NULL;
	SG_pathname * pPathDest = NULL;
	const char * pszHid_Blob_FinalResult;
	const char * pszHid_Blob_Baseline;
	const char * pszReason;
	const char * pszHid_XAttrs;
	SG_int64 attrBits;

	FETCHPATH("path_finalresult",     &pPathDest);
	FETCHPATH("path_baseline",        &pPathSrc);
	FETCHSZ(  "reason",               &pszReason);
	FETCHSZ(  "hid_finalresult",      &pszHid_Blob_FinalResult);
	FETCHSZ(  "hid_baseline",         &pszHid_Blob_Baseline);
	FETCHSZ(  "xattrs_finalresult",   &pszHid_XAttrs);
	FETCHI64( "attrbits_finalresult", &attrBits);

	if (strcmp(SG_pathname__sz(pPathSrc),
			   SG_pathname__sz(pPathDest)) != 0)
		SG_ERR_CHECK(  format_2(pCtx, pStr,
								"move_rename", pszReason,
								SG_pathname__sz(pPathSrc), SG_pathname__sz(pPathDest))  );
	if (strcmp(pszHid_Blob_Baseline,
			   pszHid_Blob_FinalResult) != 0)
		SG_ERR_CHECK(  format_1(pCtx, pStr,
								"alter_symlink", pszReason,
								SG_pathname__sz(pPathDest))  );
	if ((attrBits != 0) || (pszHid_XAttrs && *pszHid_XAttrs))
		SG_ERR_CHECK(  format_1(pCtx, pStr,
								"set_attributes", pszReason,
								SG_pathname__sz(pPathDest))  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDest);
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc);
}

DCLVERB(fn_format_alter_directory)			// convert existing WD/baseline DIRECTORY (non-recursive) to final-result
{
	SG_pathname * pPathSrc  = NULL;
	SG_pathname * pPathDest = NULL;
	const char * pszReason;
	const char * pszHid_XAttrs_Src;
	const char * pszHid_XAttrs_Dest;
	SG_int64 attrBits_Src;
	SG_int64 attrBits_Dest;

	FETCHPATH("path_finalresult",     &pPathDest);
	FETCHPATH("path_baseline",        &pPathSrc);
	FETCHSZ(  "reason",               &pszReason);
	FETCHSZ(  "xattrs_finalresult",   &pszHid_XAttrs_Dest);
	FETCHSZ(  "xattrs_baseline",      &pszHid_XAttrs_Src);
	FETCHI64( "attrbits_finalresult", &attrBits_Dest);
	FETCHI64( "attrbits_baseline",    &attrBits_Src);

	if (strcmp(SG_pathname__sz(pPathSrc),
			   SG_pathname__sz(pPathDest)) != 0)
		SG_ERR_CHECK(  format_2(pCtx, pStr,
								"move_rename", pszReason,
								SG_pathname__sz(pPathSrc), SG_pathname__sz(pPathDest))  );
	// We don't care about the HID the directory changing.
	//SG_ERR_CHECK(  format_1(pCtx, pStr,
	//						"alter_directory", pszReason,
	//						SG_pathname__sz(pPathDest))  );
	if ((attrBits_Src != attrBits_Dest) || (!_my_equal(pszHid_XAttrs_Src, pszHid_XAttrs_Dest)))
		SG_ERR_CHECK(  format_1(pCtx, pStr,
								"set_attributes", pszReason,
								SG_pathname__sz(pPathDest))  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDest);
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc);
}

DCLVERB(fn_format_move_entry)				// move/rename existing WD/baseline entry WITHOUT telling pendingtree
{
	SG_pathname * pPathSrc  = NULL;
	SG_pathname * pPathDest = NULL;
	const char * pszReason;

	FETCHPATH("path_destination",     &pPathDest);
	FETCHPATH("path_source",          &pPathSrc);
	FETCHSZ(  "reason",               &pszReason);

	if (strcmp(SG_pathname__sz(pPathSrc),
			   SG_pathname__sz(pPathDest)) != 0)
		SG_ERR_CHECK(  format_2(pCtx, pStr,
								"move_rename", pszReason,
								SG_pathname__sz(pPathSrc), SG_pathname__sz(pPathDest))  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDest);
	SG_PATHNAME_NULLFREE(pCtx, pPathSrc);
}

//////////////////////////////////////////////////////////////////

#undef DCLVERB

//////////////////////////////////////////////////////////////////

static SG_varray_foreach_callback _format_cb;

void _format_cb(SG_context * pCtx,
				void * pVoidData,
				SG_UNUSED_PARAM( const SG_varray * pva ),
				SG_UNUSED_PARAM( SG_uint32 ndx ),
				const SG_variant * pv)
{
	SG_string * pStr = (SG_string *)pVoidData;
	SG_vhash * pvhItem;
	const char * pszValue_Action;

	SG_UNUSED( pva );
	SG_UNUSED( ndx );

	SG_ERR_CHECK_RETURN(  SG_variant__get__vhash(pCtx, pv, &pvhItem)  );
	SG_ERR_CHECK_RETURN(  SG_vhash__get__sz(pCtx, pvhItem, "action", &pszValue_Action)  );

#define IFVERB(v,fn)	SG_STATEMENT( if (strcmp(pszValue_Action,(v))==0) { SG_ERR_CHECK_RETURN(  (fn)(pCtx, pvhItem, pStr)  ); return; } )

	IFVERB("move_rename",             fn_format_move_rename);

	IFVERB("remove_file",             fn_format_remove_generic);	// I'm hoping that all 3 type of
	IFVERB("remove_symlink",          fn_format_remove_generic);	// entries can be handled by the
	IFVERB("remove_directory",        fn_format_remove_generic);	// same routine.

	IFVERB("get_file",                fn_format_get_file);
	IFVERB("get_file_auto_merge",     fn_format_get_file_auto_merge);
	IFVERB("get_symlink",             fn_format_get_symlink);
	IFVERB("get_directory",           fn_format_get_directory);

	IFVERB("alter_file",              fn_format_alter_file);
	IFVERB("alter_file_auto_merge",   fn_format_alter_file_auto_merge);
	IFVERB("alter_symlink",           fn_format_alter_symlink);
	IFVERB("alter_directory",         fn_format_alter_directory);

	IFVERB("add_new_directory",       fn_format_add_new_directory);
	IFVERB("unadd_new_directory",     fn_format_unadd_new_directory);

	IFVERB("move_entry",              fn_format_move_entry);

	// TODO other commands....

#undef IFVERB

#if TRACE_WD_PLAN
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "TODO Unhandled VERB[%s]\n", pszValue_Action)  );
	SG_ASSERT(  (0)  );
#endif

}

void SG_wd_plan__format__to_string(SG_context * pCtx, const SG_wd_plan * pPlan, SG_string ** ppStrOutput)
{
	SG_string * pStr = NULL;

	SG_NULLARGCHECK_RETURN( pPlan );
	SG_NULLARGCHECK_RETURN( ppStrOutput );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStr)  );

	// DO NOT SORT THIS.
	// This varry order is the order that that operations MUST be performed in
	// so that files and directories are parked/renamed/moved in the proper order.
	// The output looks kind of like a STATUS (which is sorted) but it isn't.

	SG_ERR_CHECK(  SG_varray__foreach(pCtx, pPlan->pvaScript, _format_cb, (void *)pStr)  );

	*ppStrOutput = pStr;
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pStr);
}
