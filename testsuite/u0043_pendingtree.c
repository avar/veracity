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
 * @file u0043_pendingtree.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"
#include "unittests_pendingtree.h"

//////////////////////////////////////////////////////////////////



void u0043_pendingtree__commit_all(SG_context * pCtx, 
	const SG_pathname* pPathWorkingDir,
	SG_bool bWeThinkWeHaveChanges
	)
{
	SG_pendingtree* pPendingTree = NULL;
    SG_audit q;
	SG_repo * pRepo;
	SG_treediff2_debug__Stats tdStats;
	SG_treediff2 * pTreeDiff_before_commit = NULL;
	SG_treediff2 * pTreeDiff_after_commit = NULL;
	char bufBaseline_before_commit[SG_HID_MAX_BUFFER_LENGTH];
	char bufBaseline_after_commit[SG_HID_MAX_BUFFER_LENGTH];

	// get the changeset HID of the baseline before the commit.

	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufBaseline_before_commit,sizeof(bufBaseline_before_commit))  );

	// compute treediff of baseline-vs-pendingtree before the commit.
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,NULL,&pTreeDiff_before_commit)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_treediff2_debug__compute_stats(pCtx, pTreeDiff_before_commit,&tdStats)  );
	VERIFY_COND("commit_all",(bWeThinkWeHaveChanges == (tdStats.nrTotalChanges > 0)));

#if 0	// less noise
	INFOP("commit_all",("treediff [baseline before commit %s] vs pendingtree",bufBaseline_before_commit));
	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx, pTreeDiff_before_commit,SG_FALSE)  );
#endif
	
	// actually do the commit
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
    VERIFY_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );
    VERIFY_ERR_CHECK(  SG_audit__init(pCtx, &q, pRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
	SG_ERR_CHECK(  unittests_pendingtree__commit(pCtx, pPendingTree, &q, NULL, 0, NULL, NULL, 0, NULL, 0, NULL, 0, NULL)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	// get the changeset HID of the baseline after the commit.
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufBaseline_after_commit,sizeof(bufBaseline_after_commit))  );
	if (strcmp(bufBaseline_before_commit,bufBaseline_after_commit) == 0)
	{
		VERIFY_COND("commit_all",(!bWeThinkWeHaveChanges));
	}
	else
	{
		// construct a (non-pendingtree) treediff using the 2 changeset HIDs.
		VERIFY_ERR_CHECK(  SG_treediff2__get_repo(pCtx, pTreeDiff_before_commit,&pRepo)  );
		VERIFY_ERR_CHECK(  SG_treediff2__alloc(pCtx, pRepo,&pTreeDiff_after_commit)  );
		VERIFY_ERR_CHECK(  SG_treediff2__compare_cset_vs_cset(pCtx, pTreeDiff_after_commit,bufBaseline_before_commit,bufBaseline_after_commit)  );
#if 0	// less noise
		INFOP("commit_all",("treediff [baseline before commit %s][baseline after commit [%s]",bufBaseline_before_commit,bufBaseline_after_commit));
		VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx, pTreeDiff_after_commit,SG_FALSE)  );
#endif

		// verify that the baseline-vs-pendingtree and the old-baseline-vs-new-baseline
		// tree-diffs are equivalent....

		VERIFY_ERR_CHECK_DISCARD(  _ut_pt__compare_2_treediffs_for_equivalence(pCtx,
																						  pTreeDiff_before_commit,
																						  pTreeDiff_after_commit,
																						  SG_FALSE)  );
	}

	// fall thru to common cleanup

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_before_commit);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_after_commit);
}

void u0043_pendingtree__commit(SG_context * pCtx, 
	const SG_pathname* pPathWorkingDir,
	...
	)
{
	SG_pendingtree* pPendingTree = NULL;
    SG_repo* pRepo = NULL;
	va_list marker;
	SG_uint32 count = 0;
	const char** paszItems = NULL;
	SG_uint32 i = 0;
    SG_audit q;
	
	va_start( marker, pPathWorkingDir );
	while( SG_TRUE)
	{
		char* psz = va_arg( marker, char*);

		if (!psz)
		{
			break;
		}
		
		count++;
	}
	va_end( marker );

	SG_ERR_CHECK(  SG_alloc(pCtx, count,sizeof(char*),&paszItems)  );

	va_start( marker, pPathWorkingDir );
	for (i=0; i<count; i++)
	{
		paszItems[i++] = va_arg( marker, char*);
	}
	va_end( marker );
	
	SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );

    SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );
    SG_ERR_CHECK(  SG_audit__init(pCtx, &q, pRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
	SG_ERR_CHECK(  unittests_pendingtree__commit(pCtx, pPendingTree, &q, pPathWorkingDir, count, paszItems, NULL, 0, NULL, 0, NULL, 0, NULL)  );

	SG_NULLFREE(pCtx, paszItems);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	return;

fail:
	SG_NULLFREE(pCtx, paszItems);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

void u0043_pendingtree__delete_dir(SG_context * pCtx, 
	SG_pathname* pPath,
	const char* pszName
	)
{
	SG_pathname* pPathFile = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__rmdir__pathname(pCtx, pPathFile)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	return;

fail:
	return;
}

void u0043_pendingtree__delete_dir_recursive(SG_context * pCtx, 
	SG_pathname* pPath,
	const char* pszName
	)
{
	SG_pathname* pPathFile = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__rmdir_recursive__pathname(pCtx, pPathFile)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	return;

fail:
	return;
}

/**
 * create a symlink of the form: <pPath>/<pszName> ==> <pszContent>
 */
void u0043_pendingtree__create_symlink(SG_context * pCtx, 
	SG_pathname* pPath,
	const char* pszName,
    const char* pszContent
	)
{
	SG_pathname* pPathFile = NULL;
    SG_string* pstr = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
    VERIFY_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pstr, pszContent)  );
    VERIFY_ERR_CHECK(  SG_fsobj__symlink(pCtx, pstr, pPathFile)  );

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

/**
 * replace the target of a symlink.  that is, change the value of
 * the target of the existing symlink: <pPath>/<pszName> to point
 * to <pszContent>.
 *
 * TODO do we want to add something like this to SG_fsobj?
 */
void u0043_pendingtree__replace_symlink(SG_context * pCtx, 
	SG_pathname* pPath,
	const char* pszName,
    const char* pszContent
	)
{
	SG_pathname* pPathFile = NULL;
    SG_string* pstr = NULL;
    SG_fsobj_type t;
	SG_bool bExists = SG_FALSE;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
    VERIFY_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pstr, pszContent)  );

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, &t, NULL)  );
	if (bExists)
	{
		VERIFY_COND("link", (SG_FSOBJ_TYPE__SYMLINK == t));
		VERIFY_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPathFile)  );
	}

    VERIFY_ERR_CHECK(  SG_fsobj__symlink(pCtx, pstr, pPathFile)  );

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}


#if defined(MAC) || defined(LINUX)

void u0043_pendingtree_test__symlink(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_treediff2_debug__Stats tdStats;
    SG_fsobj_type t;
	SG_pathname* pPathGetDir = NULL;
	SG_pathname* pPathFile = NULL;
	char bufOtherName[SG_TID_MAX_BUFFER_LENGTH];
	SG_bool bExists = SG_FALSE;
    SG_string* pstrLink = NULL;
    SG_pendingtree * pPendingTree = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__create_symlink(pCtx, pPathWorkingDir, "foo", "nowhere")  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_symlink(pCtx, pvh, "foo")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Scan.  This should find nothing. */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );
	
	/* The pending changeset should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

    /* Let's modify it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "foo")  );
    VERIFY_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPathFile)  );
    SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	VERIFY_ERR_CHECK(  u0043_pendingtree__create_symlink(pCtx, pPathWorkingDir, "foo", "somewhere else")  );

    /* status */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,NULL,&pTreeDiff)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"TreeDiffStatus at %s:%d\n",__FILE__,__LINE__)  );
	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx, pTreeDiff,SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__compute_stats(pCtx, pTreeDiff,&tdStats)  );
	VERIFY_COND("status count",(tdStats.nrFileSymlinkModified == 1));
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);

    /* commit */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now get it */
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufOtherName, sizeof(bufOtherName), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGetDir, pPathTopDir, bufOtherName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathGetDir)  );
	VERIFY_ERR_CHECK(  unittests_workingdir__create_and_get(pCtx, bufName, pPathGetDir, SG_FALSE, NULL)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathGetDir, "foo")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, &t, NULL)  );
	VERIFY_COND("here", (bExists));
    VERIFY_COND("link", (SG_FSOBJ_TYPE__SYMLINK == t));

    VERIFY_ERR_CHECK(  SG_fsobj__readlink(pCtx, pPathFile, &pstrLink)  );
    VERIFY_COND("matches", (0 == strcmp(SG_string__sz(pstrLink), "somewhere else"))  );
	
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_STRING_NULLFREE(pCtx, pstrLink);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathGetDir);

	return;

fail:
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathGetDir);
}

#endif

void u0043_pendingtree_test__unsafe_remove_file(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pendingtree* pPendingTree = NULL;
    SG_bool bExists = SG_FALSE;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );

    /* add stuff */
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now modify the file */
	VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pPathWorkingDir, "a.txt", 4)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );

    {
		const char* pszName = "a.txt";
		
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, NULL, 0, NULL, 0, SG_FALSE),
											  SG_ERR_CANNOT_REMOVE_SAFELY  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}
   
	/* The pending changeset should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

    /* file should still be there */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

void u0043_pendingtree_test__simple_1_file(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo with one file in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Now modify one of the files */
	VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pPathWorkingDir, "a.txt", 4)  );

	/* The pending changeset should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Scan.  This should detect the modified file. */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );
	
	/* The pending changeset should now have something in it. */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit again, and verify that the pending changeset is once again empty. */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void u0043_pendingtree_test__simple_multiple_files(SG_context * pCtx, SG_pathname* pPathTopDir, SG_uint32 count)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_uint32 i;
	SG_vhash* pvh = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );

	for (i=0; i<count; i++)
	{
		char bufFileName[SG_TID_MAX_BUFFER_LENGTH];

		VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufFileName, sizeof(bufFileName), 32)  );
		VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, bufFileName, i + 8)  );
	}
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, count, __LINE__)  );
	
	SG_VHASH_NULLFREE(pCtx, pvh);
	
	return;

fail:
	return;
}

#if defined(MAC) || defined(LINUX)

void u0043_pendingtree_test__chmod(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
    SG_pathname* pPathFile = NULL;
    SG_pathname* pPathGetFile = NULL;
	char bufGet1[SG_TID_MAX_BUFFER_LENGTH];
    SG_pathname* pPathGet1 = NULL;
	char bufGet2[SG_TID_MAX_BUFFER_LENGTH];
    SG_pathname* pPathGet2 = NULL;
    SG_vhash* pvh = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_treediff2_debug__Stats tdStats;
    SG_bool bExists = SG_FALSE;
    SG_fsobj_stat st;
    SG_pendingtree * pPendingTree = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* Create the repo with one file in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );

    /* get its path */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );

    /* Make sure it goes into the repo WITHOUT the X bit set */
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPathFile, &st)  );
    st.perms &= ~S_IXUSR;
    VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pPathFile, st.perms)  );

    /* go ahead and add the file to the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* The pending changeset should have something in it right now */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Grab the entire repo tree as a vhash.  Make sure its top dir has one item */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

    /* chmod+x the working file */
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPathFile, &st)  );
    st.perms |= S_IXUSR;
    VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pPathFile, st.perms)  );

    /* check status */
    VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,NULL,&pTreeDiff)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"TreeDiffStatus at %s:%d\n",__FILE__,__LINE__)  );
	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx, pTreeDiff,SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__compute_stats(pCtx, pTreeDiff,&tdStats)  );
	VERIFY_COND("status count",(tdStats.nrChangedAttrBits == 1));
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);

	/* The pending changeset should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Scan.  This should detect the file with the attrbit change. */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );
	
	/* The pending changeset should now have something in it. */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now get it */
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufGet1, sizeof(bufGet1), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGet1, pPathTopDir, bufGet1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathGet1)  );
	VERIFY_ERR_CHECK(  unittests_workingdir__create_and_get(pCtx, bufName, pPathGet1, SG_FALSE, NULL)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGetFile, pPathGet1, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathGetFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));

    /* verify attrbits on the working file */
    st.perms = 0;
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPathGetFile, &st)  );
    VERIFY_COND("x", (st.perms & S_IXUSR));
	SG_PATHNAME_NULLFREE(pCtx, pPathGetFile);

    /* now put it back */
    st.perms &= ~S_IXUSR;
    VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pPathFile, st.perms)  );
	
	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Scan.  This should detect the file with the change. */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );
	
	/* The pending changeset should now have something in it. */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now get it */
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufGet2, sizeof(bufGet2), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGet2, pPathTopDir, bufGet2)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathGet2)  );
	VERIFY_ERR_CHECK(  unittests_workingdir__create_and_get(pCtx, bufName, pPathGet2, SG_FALSE, NULL)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGetFile, pPathGet2, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathGetFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));

    /* verify attrbits on the working file */
    st.perms = 0;
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPathGetFile, &st)  );
    VERIFY_COND("x", !(st.perms & S_IXUSR));
	SG_PATHNAME_NULLFREE(pCtx, pPathGetFile);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathGet1);
	SG_PATHNAME_NULLFREE(pCtx, pPathGet2);
	SG_PATHNAME_NULLFREE(pCtx, pPathGetFile);
	
	return;

fail:
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_PATHNAME_NULLFREE(pCtx, pPathGet1);
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}
#endif

#ifdef SG_BUILD_FLAG_TEST_XATTR

void u0043_pendingtree_test__xattrs(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
    SG_pathname* pPathFile = NULL;
	char bufGet1[SG_TID_MAX_BUFFER_LENGTH];
    SG_pathname* pPathGet1 = NULL;
    SG_pathname* pPathGet1File = NULL;
	char bufGet2[SG_TID_MAX_BUFFER_LENGTH];
    SG_pathname* pPathGet2 = NULL;
	SG_pathname* pPathGet2File = NULL;
	char bufGet3[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathGet3 = NULL;
	SG_pathname* pPathGet3File = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_treediff2_debug__Stats tdStats;
    SG_vhash* pvh = NULL;
    SG_uint32 count;
    SG_uint32 len32;
    SG_bool bExists = SG_FALSE;
    SG_rbtree* prb = NULL;
    const SG_byte* pPacked = NULL;
    const SG_byte* pBytes = NULL;
	SG_bool bFoundInTreeDiffByRepoPath;
	SG_pendingtree * pPendingTree = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* Create the repo with one file in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	INFO2("test__xattr working directory", SG_pathname__sz(pPathWorkingDir));
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* The pending changeset should have something in it right now */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Grab the entire repo tree as a vhash.  Make sure its top dir has one item */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

    /* Add some attributes to the working file */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pPathFile, "com.sourcegear.test1", 4, (SG_byte*) "123456789")  );
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pPathFile, "unrecognized_attribute", 7, (SG_byte*) "123456789")  );	// this n/v pair won't be in the repo.
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pPathFile, "com.sourcegear.test2", 3, (SG_byte*) "hello")  );

    /* check status */
    VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,NULL,&pTreeDiff)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"TreeDiffStatus at %s:%d\n",__FILE__,__LINE__)  );
	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx, pTreeDiff,SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__compute_stats(pCtx, pTreeDiff,&tdStats)  );
	VERIFY_COND("status count",(tdStats.nrChangedXAttrs == 1));
	VERIFY_ERR_CHECK(  SG_treediff2__find_by_repo_path(pCtx, pTreeDiff,"@/a.txt",&bFoundInTreeDiffByRepoPath,NULL,NULL)  );
	VERIFY_COND("find_by_repo_path",(bFoundInTreeDiffByRepoPath));
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);

	/* The pending changeset should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Scan.  This should detect the file with the xattrs. */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );
	
	/* The pending changeset should now have something in it. */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now get it */
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufGet1, sizeof(bufGet1), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGet1, pPathTopDir, bufGet1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathGet1)  );
	VERIFY_ERR_CHECK(  unittests_workingdir__create_and_get(pCtx, bufName, pPathGet1, SG_FALSE, NULL)  );

	INFO2("test__xattr sandbox[1]",SG_pathname__sz(pPathGet1));
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGet1File, pPathGet1, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathGet1File, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));

    /* verify xattrs on the file */
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx, pPathGet1File, NULL, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFY_COND("count", (2 == count));
#if 1 && defined(DEBUG)
	VERIFY_ERR_CHECK(  SG_rbtree_debug__dump_keys_to_console(pCtx, prb)  );
#endif
    VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx, prb, "com.sourcegear.test1", NULL, (void**) &pPacked)  );
    VERIFY_ERR_CHECK(  SG_memoryblob__unpack(pCtx, pPacked, &pBytes, &len32)  );
    VERIFY_COND("len32", (4 == len32));
    VERIFY_COND("byte", ('1' == pBytes[0]));
    VERIFY_COND("byte", ('2' == pBytes[1]));
    VERIFY_COND("byte", ('3' == pBytes[2]));
    VERIFY_COND("byte", ('4' == pBytes[3]));
    SG_RBTREE_NULLFREE(pCtx, prb);

	SG_PATHNAME_NULLFREE(pCtx, pPathGet1File);

	//////////////////////////////////////////////////////////////////

    /* now remove one from the file */
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__remove(pCtx, pPathFile, "com.sourcegear.test1")  );
	
	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Scan.  This should detect the file with the xattrs. */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );
	
	/* The pending changeset should now have something in it. */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now get it */
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufGet2, sizeof(bufGet2), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGet2, pPathTopDir, bufGet2)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathGet2)  );
	VERIFY_ERR_CHECK(  unittests_workingdir__create_and_get(pCtx, bufName, pPathGet2, SG_FALSE, NULL)  );

	INFO2("test__xattr sandbox[2]",SG_pathname__sz(pPathGet2));
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGet2File, pPathGet2, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathGet2File, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));

    /* verify xattrs on the file */
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx, pPathGet2File, NULL, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFY_COND("count", (1 == count));
#if 1 && defined(DEBUG)
	VERIFY_ERR_CHECK(  SG_rbtree_debug__dump_keys_to_console(pCtx, prb)  );
#endif
    VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx, prb, "com.sourcegear.test2", NULL, (void**) &pPacked)  );
    VERIFY_ERR_CHECK(  SG_memoryblob__unpack(pCtx, pPacked, &pBytes, &len32)  );
    VERIFY_COND("len32", (3 == len32));
    VERIFY_COND("byte", ('h' == pBytes[0]));
    VERIFY_COND("byte", ('e' == pBytes[1]));
    VERIFY_COND("byte", ('l' == pBytes[2]));
    SG_RBTREE_NULLFREE(pCtx, prb);

	//////////////////////////////////////////////////////////////////

	/* now remove the other one -- after this there should be no (recognized) XATTRS on the file. */
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__remove(pCtx, pPathFile, "com.sourcegear.test2")  );
	
	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Scan.  This should detect the file with the xattrs. */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );
	
	/* The pending changeset should now have something in it. */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now get it (into a clean sandbox) */
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufGet3, sizeof(bufGet3), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGet3, pPathTopDir, bufGet3)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathGet3)  );
	VERIFY_ERR_CHECK(  unittests_workingdir__create_and_get(pCtx, bufName, pPathGet3, SG_FALSE, NULL)  );

	INFO2("test__xattr sandbox[3]",SG_pathname__sz(pPathGet3));
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGet3File, pPathGet3, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathGet3File, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));

    /* verify xattrs on the file */
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx, pPathGet3File, NULL, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFY_COND("count", (0 == count));
#if 1 && defined(DEBUG)
	VERIFY_ERR_CHECK(  SG_rbtree_debug__dump_keys_to_console(pCtx, prb)  );
#endif
    SG_RBTREE_NULLFREE(pCtx, prb);

	//////////////////////////////////////////////////////////////////

	// fall thru to common cleanup

	//////////////////////////////////////////////////////////////////

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathGet1);
	SG_PATHNAME_NULLFREE(pCtx, pPathGet1File);
	SG_PATHNAME_NULLFREE(pCtx, pPathGet2);
	SG_PATHNAME_NULLFREE(pCtx, pPathGet2File);
	SG_PATHNAME_NULLFREE(pCtx, pPathGet3);
	SG_PATHNAME_NULLFREE(pCtx, pPathGet3File);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
    SG_RBTREE_NULLFREE(pCtx, prb);
	SG_VHASH_NULLFREE(pCtx, pvh);
}

#endif

void u0043_pendingtree_test__simple_1_file_removed(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* Create the repo with one file in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* The pending changeset should have something in it right now */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Grab the entire repo tree as a vhash.  Make sure its top dir has one item */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Delete the local working file */
	VERIFY_ERR_CHECK(  _ut_pt__delete_file(pCtx, pPathWorkingDir, "a.txt")  );

	/* The pending tree should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Call addremove.  This should detect the missing working file. */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* Now the pending changeset should have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit all */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty again */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Grab the tree again and verify that it's empty */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 0, __LINE__)  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	
	return;

fail:
	return;
}

void u0043_pendingtree_test__remove_directory(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pendingtree* pPendingTree = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir1, "f1")  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "f1", 20)  );
#if defined(LINUX) || defined(MAC)
	VERIFY_ERR_CHECK(  u0043_pendingtree__create_symlink(pCtx, pPathDir1, "foo", "nowhere")  );
#endif
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "f0", 20)  );
	

	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

    /* Now remove d1 */
    {
		const char* pszName = "d1";
		
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}	

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;

fail:
    SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;
}

void u0043_pendingtree_test__unsafe_remove_directory__found(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pendingtree* pPendingTree = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir1, "f1")  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "f1", 20)  );
#if defined(MAC) || defined(LINUX)
	VERIFY_ERR_CHECK(  u0043_pendingtree__create_symlink(pCtx, pPathDir1, "foo", "nowhere")  );
#endif
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "f0", 20)  );
	
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "f2", 20)  );

    {
		const char* pszName = "d1";
		
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, NULL, 0, NULL, 0, SG_FALSE),
											  SG_ERR_CANNOT_REMOVE_SAFELY  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}	

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;

fail:
    SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;
}

void u0043_pendingtree_test__unsafe_remove_directory__modified(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_fsobj_stat stat_a, stat_b;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir1, "f1")  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "f1", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "f0", 20)  );
	
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPathFile, &stat_a)  );
	VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pPathDir1, "f1", 4)  );
	VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPathFile, &stat_b)  );

	{
		SG_int_to_string_buffer buf_a;
		SG_int_to_string_buffer buf_b;

		INFOP("unsafe_remove_directory__modified",
			  ("f1 [mtime[a] %s] [mtime[b] %s]",
			   SG_int64_to_sz(stat_a.mtime_ms, buf_a),
			   SG_int64_to_sz(stat_b.mtime_ms, buf_b)));
	}

    {
		const char* pszName = "d1";
		
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, NULL, 0, NULL, 0, SG_FALSE),
											  SG_ERR_CANNOT_REMOVE_SAFELY  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}	

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;

fail:
    SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;
}

void u0043_pendingtree_test__simple_two_subdirs_move(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathDir2 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pathname* pPathNewHome = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvhSubDir = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_bool bExists = SG_FALSE;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir2, pPathWorkingDir, "d2")  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir2)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "a.txt", 20)  );
	
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 2, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d1")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d2")  );

	/* Make sure a.txt is in d1 and not in d2 */
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d1", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "a.txt")  );
	
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d2", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 0, __LINE__)  );
	
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Now move file a.txt from d1 to d2 */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir1, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname**)&pPathFile, 1, pPathDir2, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	/* verify that the local file was moved */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (!bExists));
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathNewHome, pPathDir2, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathNewHome, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	
	/* The changeset should now have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit and verify that the changeset is empty again */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 2, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d1")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d2")  );

	/* Make sure a.txt is in d2 and not in d1 */
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d1", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 0, __LINE__)  );
	
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d2", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "a.txt")  );
	
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewHome);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewHome);

	return;
}

void u0043_pendingtree_test__2_files_commit_only_1(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the working directory with two files in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "b.txt", 50)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* The pending changeset should now have something in it. */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit just one file */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit(pCtx, pPathWorkingDir, "a.txt", NULL)  );

	/* The pending changeset should STILL have something in it. */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Now commit the other file */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit(pCtx, pPathWorkingDir, "b.txt", NULL)  );
	
	/* The pending changeset should now be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 2, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "b.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return;

fail:
	return;
}

void u0043_pendingtree_test__2_files_commit_change_but_not_remove(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the working directory with two files in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "b.txt", 50)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* The pending changeset should now have something in it. */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit all */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* The pending changeset should now be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now modify a.txt */
	VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pPathWorkingDir, "a.txt", 4)  );

	/* And remove b.txt */
	VERIFY_ERR_CHECK(  _ut_pt__delete_file(pCtx, pPathWorkingDir, "b.txt")  );

	/* Do addremove */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* The pending changeset should now have something in it. */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit just the modified file, not the deleted one */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit(pCtx, pPathWorkingDir, "a.txt", NULL)  );

	/* The pending changeset should STILL have something in it. */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 2, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "b.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Commit all */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* The pending changeset should now be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
}

void u0043_pendingtree_test__delete_file_add_dir_with_same_name(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvhSubDir = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* Create the repo with one file in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "foo", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* The pending changeset should have something in it right now */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Grab the entire repo tree as a vhash.  Make sure its top dir has one item */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "foo")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Delete the local working file */
	VERIFY_ERR_CHECK(  _ut_pt__delete_file(pCtx, pPathWorkingDir, "foo")  );

	/* The pending tree should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now create a directory where the file was */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );

	/* with a file in it */
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "bar", 20)  );

	/* Call addremove.  This should detect the missing working file and the new directory. */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	
	/* Now the pending changeset should have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	// TODO See SPRAWL-279.  We should have a LOST status for the file "foo" and an ADDED status
	// TODO for the directory "foo" (and the file "foo/bar").  After the commit is the file version
	// TODO of "foo" still LOST or do we silently convert it into a real DELETE?
	// TODO
	// TODO see also: __delete_dir_add_file_with_same_name()
	// TODO see also: __rename_file_add_dir_with_same_name()
	// TODO see also: __move_file_add_dir_with_same_name()
	//////////////////////////////////////////////////////////////////

	/* Commit */
	/* TODO what should happen if we try to commit just the new directory without the delete? */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "foo")  );

	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "foo", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "bar")  );

	SG_VHASH_NULLFREE(pCtx, pvh);
	
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;
}

void u0043_pendingtree_test__simple_rename(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pathname* pPathNewName = NULL;
	SG_vhash* pvh = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_bool bExists = SG_FALSE;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo with one file in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Now rename the file */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "foo", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	/* verify that the local file was moved */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (!bExists));
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathNewName, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathNewName, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	
	/* The changeset should now have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit and verify that the changeset is empty again */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "foo")  );

	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewName);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewName);

	return;
}

void u0043_pendingtree_test__commit_with_no_actual_changes(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	char szBaseline1[SG_HID_MAX_BUFFER_LENGTH];
	char szBaseline2[SG_HID_MAX_BUFFER_LENGTH];
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo with one file in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Get the current baseline */
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir, szBaseline1, sizeof(szBaseline1))  );
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Scan.  This should detect nothing. */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );
	
	/* The pending changeset should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Commit again.  This should do nothing. */
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_FALSE),
										  SG_ERR_NOTHING_TO_COMMIT  );

	/* Get the current baseline and verify that it has not changed. */
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir, szBaseline2, sizeof(szBaseline2))  );
	VERIFY_COND("no change", (0 == strcmp(szBaseline1, szBaseline2)));
	
	/* The pending changeset should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);


	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
}

void u0043_pendingtree_test__rename_file_add_dir_with_same_name(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvhSubDir = NULL;
	SG_pendingtree* pPendingTree = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* Create the repo with one file in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "foo", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* The pending changeset should have something in it right now */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Grab the entire repo tree as a vhash.  Make sure its top dir has one item */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "foo")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Now rename the file */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "bar", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	/* Now the pending changeset should have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Now create a directory where the file was */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );

	/* with a file in it */
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "bar", 20)  );

	/* Call addremove.  This should detect the renamed working file and the new directory. */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	
	/* Now the pending changeset should STILL have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit */
	/* TODO what should happen if we try to commit just the new directory without the rename? */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 2, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "foo")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "bar")  );

	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "foo", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "bar")  );

	SG_VHASH_NULLFREE(pCtx, pvh);
	
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;
}

void u0043_pendingtree_test__move_file_add_dir_with_same_name(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathDir2 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvhSubDir = NULL;
	SG_pendingtree* pPendingTree = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* Create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "foo", 20)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir2, pPathWorkingDir, "d2")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir2)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* The pending changeset should have something in it right now */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Grab the entire repo tree as a vhash.  Make sure its top dir has one item */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 2, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "foo")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d2")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Now move the file into d2 */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPathDir2, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	/* Now the pending changeset should have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Now create a directory where the file was */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );

	/* with a file in it */
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "fiddle", 20)  );

	/* Call addremove.  This should detect the moved file and the new directory. */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	
	/* Now the pending changeset should STILL have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit */
	/* TODO what should happen if we try to commit just the new directory without the rename? */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 2, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "foo")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d2")  );

	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "foo", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "fiddle")  );

	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d2", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "foo")  );

	SG_VHASH_NULLFREE(pCtx, pvh);
	
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);

	return;
}

void u0043_pendingtree_test__delete_dir_add_file_with_same_name(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* Create the repo with one dir in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	// mkdir @/foo
	// create @/foo/bar
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "bar", 5)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* The pending changeset should have something in it right now */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Grab the entire repo tree as a vhash.  Make sure its top dir has one item */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "foo")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Delete the local working directory */
	//VERIFY_ERR_CHECK(  u0043_pendingtree__delete_dir(pCtx, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__delete_dir_recursive(pCtx, pPathWorkingDir, "foo")  );

	/* The pending tree should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now create a file where the dir was */
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "foo", 33)  );

	/* Call addremove.  This should detect the dir which is now a file. */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	
	/* Now the pending changeset should have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After various delete_dir and add_file changes.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/foo",            _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/foo/bar",        _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/foo",            _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/foo",       SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/foo/bar",   SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/foo",       SG_DIFFSTATUS_FLAGS__ADDED)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////

	/* Commit */
	/* TODO what should happen if we try to commit just the new directory without the delete? */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "foo")  );

	SG_VHASH_NULLFREE(pCtx, pvh);
	
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
}

void u0043_pendingtree_test__delete_dir_add_new_dir_with_same_name(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* Create the repo with one dir in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	// mkdir @/foo
	// create @/foo/bar
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "bar", 5)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* The pending changeset should have something in it right now */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Grab the entire repo tree as a vhash.  Make sure its top dir has one item */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "foo")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Delete the local working directory */
	//VERIFY_ERR_CHECK(  u0043_pendingtree__delete_dir(pCtx, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__delete_dir_recursive(pCtx, pPathWorkingDir, "foo")  );

	/* The pending tree should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now create a new directory and file where the dir was */
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "replacement", 7)  );

	/* Call addremove.  This should detect the changes. */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	
	/* Now the pending changeset should have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.
	// 
	// since we DID NOT call add-remove after we deleted the first directory and before we
	// created the second version, pendingtree cannot tell that it is a different directory
	// and will just see it as having changed contents.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After various delete_dir and add_new_dir changes.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/foo",             _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/foo/bar",         _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/foo/replacement", _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/foo",             SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/foo/bar",         SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/foo/replacement", SG_DIFFSTATUS_FLAGS__ADDED)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////

	/* Commit */
	/* TODO what should happen if we try to commit just the new directory without the delete? */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
}

void u0043_pendingtree_test__delete_dir_add_new_dir_with_same_name2(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* Create the repo with one dir in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	// mkdir @/foo
	// create @/foo/bar
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "bar", 5)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* The pending changeset should have something in it right now */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Commit everything */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	/* Now the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Grab the entire repo tree as a vhash.  Make sure its top dir has one item */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "foo")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Delete the local working directory */
	//VERIFY_ERR_CHECK(  u0043_pendingtree__delete_dir(pCtx, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__delete_dir_recursive(pCtx, pPathWorkingDir, "foo")  );

	/* Call addremove.  This should detect that @/foo and @/foo/bar are gone. */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* Now the pending changeset should have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Now create a new directory and file where the dir was */
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "replacement", 7)  );

	/* Call addremove.  This should detect the changes. */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	
	/* Now the pending changeset should have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.
	// 
	// since we DID call add-remove after we deleted the first directory and before we
	// created the second version, pendingtree should be able to tell that it is a different directory
	// and list both.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After various delete_dir and add_new_dir 2 changes.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/foo",             _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/foo/bar",         _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/foo",             _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/foo/replacement", _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/foo",             SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/foo/bar",         SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/foo",             SG_DIFFSTATUS_FLAGS__ADDED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/foo/replacement", SG_DIFFSTATUS_FLAGS__ADDED)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////

	/* Commit */
	/* TODO what should happen if we try to commit just the new directory without the delete? */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
}

void u0043_pendingtree_test__attempt_commit_split_move(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathDir2 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pathname* pPathNewHome = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvhSubDir = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_bool bExists = SG_FALSE;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir2, pPathWorkingDir, "d2")  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir2)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "a.txt", 20)  );
	
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 2, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d1")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d2")  );

	/* Make sure a.txt is in d1 and not in d2 */
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d1", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "a.txt")  );
	
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d2", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 0, __LINE__)  );
	
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Now move file a.txt from d1 to d2 */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir1, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPathDir2, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	/* verify that the local file was moved */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (!bExists));
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathNewHome, pPathDir2, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathNewHome, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	
	/* The changeset should now have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Try to commit d2 only */
	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  u0043_pendingtree__commit(pCtx, pPathWorkingDir, "d2")  );		// split move should fail

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewHome);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewHome);

	return;
}

void u0043_pendingtree_test__rename_conflict(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pathname* pPathNewName = NULL;
	SG_vhash* pvh = NULL;
	SG_pendingtree* pPendingTree = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "b.txt", 40)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 2, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "b.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Now rename a.txt to clash with b.txt */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "b.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	/* TODO if we commit now, what's in the pending tree? */
	
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewName);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewName);

	return;
}

void u0043_pendingtree_test__simple_get(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathGetDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_vhash* pvh = NULL;
	char bufOtherName[SG_TID_MAX_BUFFER_LENGTH];
	SG_bool bExists = SG_FALSE;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "b.txt", 40)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now get it */
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufOtherName, sizeof(bufOtherName), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGetDir, pPathTopDir, bufOtherName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathGetDir)  );
	VERIFY_ERR_CHECK(  unittests_workingdir__create_and_get(pCtx, bufName, pPathGetDir, SG_FALSE, NULL)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathGetDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathGetDir);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathGetDir);

	return;
}

void u0043_pendingtree_test__file_has_not_really_changed(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo with one file in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Delete the local working file */
	VERIFY_ERR_CHECK(  _ut_pt__delete_file(pCtx, pPathWorkingDir, "a.txt")  );

	/* Put the local working file back. */
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	
	/* The pending changeset should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Scan.  This should realize that the file has not really changed. */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );
	
	/* The pending changeset should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return;

fail:
	return;
}

void u0043_pendingtree_test__rename_to_same_name_conflict(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pathname* pPathNewName = NULL;
	SG_vhash* pvh = NULL;
	SG_pendingtree* pPendingTree = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Now rename a.txt to a.txt */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "a.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	/* The pending changeset should still be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewName);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewName);

	return;
}

void u0043_pendingtree_test__move_to_same_dir(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathDir2 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pathname* pPathNewHome = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvhSubDir = NULL;
	SG_pendingtree* pPendingTree = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir2, pPathWorkingDir, "d2")  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir2)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "a.txt", 20)  );
	
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 2, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d1")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d2")  );

	/* Make sure a.txt is in d1 and not in d2 */
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d1", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "a.txt")  );
	
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d2", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 0, __LINE__)  );
	
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Now move file a.txt from d1 to d1 */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir1, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPathDir1, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewHome);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewHome);

	return;
}

void u0043_pendingtree_test__move_to_nonexistent_dir(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathDir2 = NULL;
	SG_pathname* pPathDir3 = NULL;
	SG_pathname* pPathDir4 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pathname* pPathNewHome = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvhSubDir = NULL;
	SG_pendingtree* pPendingTree = NULL;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir2, pPathWorkingDir, "d2")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir3, pPathWorkingDir, "d3")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir4, pPathDir3, "d4")  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir2)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "a.txt", 20)  );
	
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 2, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d1")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d2")  );

	/* Make sure a.txt is in d1 and not in d2 */
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d1", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "a.txt")  );
	
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d2", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 0, __LINE__)  );
	
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Now move file a.txt from d1 to d4, which doesn't exist */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir1, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPathDir4, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir3);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir4);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewHome);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewHome);

	return;
}

void u0043_pendingtree_test__simple_add(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	SG_pendingtree* pPendingTree = NULL;
	const char *pszName = "a.txt";
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo with one file in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__add(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_FALSE, NULL, 0, NULL, 0, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	/* The pending changeset should now have something in it. */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return;

fail:
	return;
}

void u0043_pendingtree_test__add_but_no_file(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	SG_pendingtree* pPendingTree = NULL;
	const char *pszName = "a.txt";
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo with one file in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	//VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_pendingtree__add(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_FALSE, NULL, 0, NULL, 0, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return;

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	
	return;
}

void u0043_pendingtree_test__add_same_file_twice(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	SG_pendingtree* pPendingTree = NULL;
	const char *pszName = "a.txt";
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo with one file in it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__add(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_FALSE, NULL, 0, NULL, 0, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	//This should not fail.
	VERIFY_ERR_CHECK(  SG_pendingtree__add(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_FALSE, NULL, 0, NULL, 0, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return;

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	
	return;
}

void u0043_pendingtree_test__status(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathDir2 = NULL;
	SG_pathname* pPathDir3 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvhSubDir = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_treediff2_debug__Stats tdStats;

	char szgid_d1[SG_GID_BUFFER_LENGTH];
	char szgid_d2[SG_GID_BUFFER_LENGTH];
	char szgid_d3[SG_GID_BUFFER_LENGTH];

	char szgid_ftopa[SG_GID_BUFFER_LENGTH];
	char szgid_ftopb[SG_GID_BUFFER_LENGTH];
	char szgid_ftopc[SG_GID_BUFFER_LENGTH];
	
	char szgid_f1a[SG_GID_BUFFER_LENGTH];
	char szgid_f1b[SG_GID_BUFFER_LENGTH];
	char szgid_f1c[SG_GID_BUFFER_LENGTH];
	
	char szgid_f2a[SG_GID_BUFFER_LENGTH];
	char szgid_f2b[SG_GID_BUFFER_LENGTH];
	char szgid_f2c[SG_GID_BUFFER_LENGTH];
	
	char szgid_f3a[SG_GID_BUFFER_LENGTH];
	char szgid_f3b[SG_GID_BUFFER_LENGTH];
	char szgid_f3c[SG_GID_BUFFER_LENGTH];
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir2, pPathWorkingDir, "d2")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir3, pPathWorkingDir, "d3")  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir2)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir3)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "ftopa", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "ftopb", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "ftopc", 20)  );
	
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "f1a", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "f1b", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "f1c", 20)  );
	
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir2, "f2a", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir2, "f2b", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir2, "f2c", 20)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir3, "f3a", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir3, "f3b", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir3, "f3c", 20)  );
	
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );

	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 6, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d1")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d2")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d3")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "ftopa")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "ftopb")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "ftopc")  );

	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvh, "d1", szgid_d1, sizeof(szgid_d1))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvh, "d2", szgid_d2, sizeof(szgid_d2))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvh, "d3", szgid_d3, sizeof(szgid_d3))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvh, "ftopa", szgid_ftopa, sizeof(szgid_ftopa))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvh, "ftopb", szgid_ftopb, sizeof(szgid_ftopb))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvh, "ftopc", szgid_ftopc, sizeof(szgid_ftopc))  );

	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d1", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 3, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "f1a")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "f1b")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "f1c")  );

	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhSubDir, "f1a", szgid_f1a, sizeof(szgid_f1a))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhSubDir, "f1b", szgid_f1b, sizeof(szgid_f1b))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhSubDir, "f1c", szgid_f1c, sizeof(szgid_f1c))  );
	
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d2", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 3, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "f2a")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "f2b")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "f2c")  );
	
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhSubDir, "f2a", szgid_f2a, sizeof(szgid_f2a))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhSubDir, "f2b", szgid_f2b, sizeof(szgid_f2b))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhSubDir, "f2c", szgid_f2c, sizeof(szgid_f2c))  );
	
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d3", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 3, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "f3a")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "f3b")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir, "f3c")  );

	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhSubDir, "f3a", szgid_f3a, sizeof(szgid_f3a))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhSubDir, "f3b", szgid_f3b, sizeof(szgid_f3b))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhSubDir, "f3c", szgid_f3c, sizeof(szgid_f3c))  );
	
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* Status should have nothing in it right now */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,NULL,&pTreeDiff)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"TreeDiffStatus at %s:%d\n",__FILE__,__LINE__)  );
	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx, pTreeDiff,SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__compute_stats(pCtx, pTreeDiff,&tdStats)  );
	VERIFY_COND("status should be empty",(tdStats.nrTotalChanges == 0));
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);

	//////////////////////////////////////////////////////////////////

	/* MODIFIED:  Modify f2c */
	VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pPathDir2, "f2c", 4)  );

	/* MODIFIED:  Modify f1a */
	VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pPathDir1, "f1a", 4)  );

	/* MODIFIED:  Modify f2a */
	VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pPathDir2, "f2a", 4)  );

	/* MOVED:  Move file f1a from d1 to d3 */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir1, "f1a")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPathDir3, SG_FALSE)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	/* RENAMED:  Rename f2a */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir2, "f2a")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "formerly_f2a", SG_FALSE)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	/* LOST:  Remove f3b */
	VERIFY_ERR_CHECK(  _ut_pt__delete_file(pCtx, pPathDir3, "f3b")  );

	/* FOUND:  Add f3d */
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir3, "f3d", 44)  );
	
	/* ADDED:  Add f2d officially */
	{
		const char* pszName_f2d = "f2d";
		
		VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir2, "f2d", 44)  );
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__add(pCtx, pPendingTree, pPathDir2, 1, &pszName_f2d, SG_FALSE, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

	/* DELETED:  Remove f1c officially */
	{
		const char* pszName_f1c = "f1c";
		
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathDir1, 1, &pszName_f1c, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

	/* FOUND:  Add f1c */
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir1, "f1c", 44)  );
	
	/* RENAMED:  Rename d3 */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathDir3, "d3prime", SG_FALSE)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	/* Now check status */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,NULL,&pTreeDiff)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"TreeDiffStatus at %s:%d\n",__FILE__,__LINE__)  );
	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx, pTreeDiff,SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__compute_stats(pCtx, pTreeDiff,&tdStats)  );
	VERIFY_COND("status count",(tdStats.nrSectionsWithChanges == 7));

	/* Now check all the paths by gid */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_d1, "@/d1/")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_d2, "@/d2/")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_d3, "@/d3prime/")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_ftopa, "@/ftopa")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_ftopb, "@/ftopb")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_ftopc, "@/ftopc")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f1a, "@/d3prime/f1a")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f1b, "@/d1/f1b")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f1c, "@/d1/f1c")  );		// this is the gid of the deleted version; not the found version
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f2a, "@/d2/formerly_f2a")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f2b, "@/d2/f2b")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f2c, "@/d2/f2c")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f3a, "@/d3prime/f3a")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f3b, "@/d3prime/f3b")  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f3c, "@/d3prime/f3c")  );

#define MY_VERIFY_IN_SECTION(l,gid,dsWanted)															\
	SG_STATEMENT(																						\
		const SG_treediff2_ObjectData * pOD_opaque;														\
		SG_diffstatus_flags dsSet;																		\
		SG_bool bFound;																					\
		VERIFY_ERR_CHECK(  SG_treediff2__get_ObjectData(pCtx, pTreeDiff,gid,&bFound,&pOD_opaque)  ); 			\
		VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD_opaque,&dsSet)  );					\
		VERIFY_COND((l),((dsSet & dsWanted) == dsWanted));													)

#define MY_VERIFY_MOVED_FROM(l,gid,gidOldParent)														\
	SG_STATEMENT(																						\
		const SG_treediff2_ObjectData * pOD_opaque;														\
		const char * szOldParentGid;																	\
		SG_bool bFound;																					\
		VERIFY_ERR_CHECK(  SG_treediff2__get_ObjectData(pCtx, pTreeDiff,gid,&bFound,&pOD_opaque)  );			\
		VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_moved_from_gid(pCtx, pOD_opaque,&szOldParentGid)  );	\
		VERIFY_COND((l),(strcmp(gidOldParent,szOldParentGid)==0));											)

#define MY_VERIFY_NAMES(l,gid,sz1,sz2)																	\
	SG_STATEMENT(																						\
		const SG_treediff2_ObjectData * pOD_opaque;														\
		const char * szOldName;																			\
		const char * szNewName;																			\
		SG_bool bFound;																					\
		VERIFY_ERR_CHECK(  SG_treediff2__get_ObjectData(pCtx, pTreeDiff,gid,&bFound,&pOD_opaque)  );			\
		VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, pOD_opaque,&szOldName)  );			\
		VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_name(pCtx, pOD_opaque,&szNewName)  );				\
		VERIFY_COND((l),(strcmp(sz1,szOldName)==0));															\
		VERIFY_COND((l),(strcmp(sz2,szNewName)==0));															)

#define MY_VERIFY_REPO_PATH(l,gid,sz)																	\
	SG_STATEMENT(																						\
		const SG_treediff2_ObjectData * pOD_opaque;														\
		const char * szRepoPath;																		\
		SG_bool bFound;																					\
		VERIFY_ERR_CHECK(  SG_treediff2__get_ObjectData(pCtx, pTreeDiff,gid,&bFound,&pOD_opaque)  );			\
		VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_repo_path(pCtx, pOD_opaque,&szRepoPath)  );			\
		VERIFY_COND((l),(strcmp(sz,szRepoPath)==0));															)

#define MY_VERIFY__IN_SECTION__REPO_PATH(l,gid,dsWanted,sz)												\
	SG_STATEMENT(																						\
		MY_VERIFY_IN_SECTION(l,gid,dsWanted);															\
		MY_VERIFY_REPO_PATH(l,gid,sz);																	)
	

	/* Check the moves */
	VERIFY_COND("moved count",(tdStats.nrMoved == 1));

	MY_VERIFY_IN_SECTION("moved",szgid_f1a,SG_DIFFSTATUS_FLAGS__MOVED);
	MY_VERIFY_MOVED_FROM("moved from",szgid_f1a,szgid_d1);
	
	/* Check the adds */
	VERIFY_COND("added count",(tdStats.nrAdded == 1));
	// TODO verify that "f2d" was added.
	
	/* Check the renames */
	VERIFY_COND("rename count",(tdStats.nrRenamed == 2));
	MY_VERIFY__IN_SECTION__REPO_PATH("renames",szgid_d3,SG_DIFFSTATUS_FLAGS__RENAMED,"@/d3prime/");
	MY_VERIFY_NAMES("renames",szgid_d3,"d3","d3prime");

	MY_VERIFY__IN_SECTION__REPO_PATH("renames",szgid_f2a,SG_DIFFSTATUS_FLAGS__RENAMED,"@/d2/formerly_f2a");
	MY_VERIFY_NAMES("renames",szgid_f2a,"f2a","formerly_f2a");
	
	/* Check the deletes */
	VERIFY_COND("delete count",(tdStats.nrDeleted == 1));
	MY_VERIFY__IN_SECTION__REPO_PATH("deleted",szgid_f1c,SG_DIFFSTATUS_FLAGS__DELETED,"@/d1/f1c");

	/* Check the modifieds */
	VERIFY_COND("modified count",(tdStats.nrFileSymlinkModified == 3));
	MY_VERIFY__IN_SECTION__REPO_PATH("modified",szgid_f1a,SG_DIFFSTATUS_FLAGS__MODIFIED,"@/d3prime/f1a");
	MY_VERIFY__IN_SECTION__REPO_PATH("modified",szgid_f2c,SG_DIFFSTATUS_FLAGS__MODIFIED,"@/d2/f2c");
	MY_VERIFY__IN_SECTION__REPO_PATH("modified",szgid_f2a,SG_DIFFSTATUS_FLAGS__MODIFIED,"@/d2/formerly_f2a");

	/* Check the lost */
	VERIFY_COND("lost count",(tdStats.nrLost == 1));
	MY_VERIFY__IN_SECTION__REPO_PATH("lost",szgid_f3b,SG_DIFFSTATUS_FLAGS__LOST,"@/d3prime/f3b");

	/* Check the found */
	VERIFY_COND("found count",(tdStats.nrFound == 2));
//	VERIFY_ERR_CHECK(  u0043_pendingtree__verify_vhash_has_path_in_section(pCtx, pvhStatusSummary, SG_STATUS_TYPE__FOUND, "@/d3prime/f3d")  );
//	MY_VERIFY__IN_SECTION__REPO_PATH("found",szgid_f1c,SG_DIFFSTATUS_FLAGS__FOUND,"@/d1/f1c");	// this doesn't work because szgid_f1c is for the deleted version; not the found version

	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);

	/* Now addremove.  This should convert all lost/found to
	 * delete/add.  And it should find all the modified files. */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	/* Now check status again.  Now lost/found this time. */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree, NULL, &pTreeDiff)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"TreeDiffStatus at %s:%d\n",__FILE__,__LINE__)  );
	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx, pTreeDiff,SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__compute_stats(pCtx, pTreeDiff,&tdStats)  );
	VERIFY_COND("status count", (tdStats.nrSectionsWithChanges == 5));
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);

	/* Check the counts on everything */
	VERIFY_COND("moved count",(tdStats.nrMoved == 1));
	VERIFY_COND("added count",(tdStats.nrAdded == 3));
	VERIFY_COND("rename count",(tdStats.nrRenamed ==2));
	VERIFY_COND("delete count",(tdStats.nrDeleted == 2));
	VERIFY_COND("modified count",(tdStats.nrFileSymlinkModified == 3));

	/* Now commit */
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Status should have nothing in it right now */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,NULL,&pTreeDiff)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"TreeDiffStatus at %s:%d\n",__FILE__,__LINE__)  );
	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx, pTreeDiff,SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__compute_stats(pCtx, pTreeDiff,&tdStats)  );
	VERIFY_COND("status count", (tdStats.nrTotalChanges == 0));
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);

	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir3);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	return;

fail:
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir3);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	return;
}

void u0043_pendingtree_test__deep_rename_move_delete(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	// this is like u0043_pendingtree_test__status() but it goes a
	// little deeper into the tree and tests rename/move/delete
	// combinations (screwcases) with a deep tree.

	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathDir2 = NULL;
	SG_pathname* pPathDir3 = NULL;
	SG_pathname* pPathDir4 = NULL;
	SG_pathname* pPathDir5 = NULL;
	SG_pathname* pPathDir6 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_vhash* pvhRoot = NULL;
	SG_vhash* pvhDir1 = NULL;
	SG_vhash* pvhDir2 = NULL;
	SG_vhash* pvhDir3 = NULL;
	SG_vhash* pvhDir4 = NULL;
	SG_vhash* pvhDir5 = NULL;
	SG_vhash* pvhDir6 = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_treediff2_debug__Stats tdStats;

	char szgid_d1[SG_GID_BUFFER_LENGTH];
	char szgid_d2[SG_GID_BUFFER_LENGTH];
	char szgid_d3[SG_GID_BUFFER_LENGTH];
	char szgid_d4[SG_GID_BUFFER_LENGTH];
	char szgid_d5[SG_GID_BUFFER_LENGTH];
	char szgid_d6[SG_GID_BUFFER_LENGTH];

	char szgid_f6a[SG_GID_BUFFER_LENGTH];
	char szgid_f6b[SG_GID_BUFFER_LENGTH];
	char szgid_f6c[SG_GID_BUFFER_LENGTH];
	char szgid_f6d[SG_GID_BUFFER_LENGTH];
	char szgid_f6e[SG_GID_BUFFER_LENGTH];
	char szgid_f6f[SG_GID_BUFFER_LENGTH];
	//char szgid_f6g[SG_GID_BUFFER_LENGTH];

	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */

	// Create <wd>/d1/d2/d3/d4/d5/d6
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir2, pPathDir1, "d2")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir3, pPathDir2, "d3")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir4, pPathDir3, "d4")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir5, pPathDir4, "d5")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir6, pPathDir5, "d6")  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir2)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir3)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir4)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir5)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir6)  );

	// Create <wd>/d1/d2/d3/d4/d5/d6/f6[abcdef]
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir6, "f6a", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir6, "f6b", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir6, "f6c", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir6, "f6d", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir6, "f6e", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir6, "f6f", 20)  );
	
	// Commit everything we have created in the file system.
	// Afterwards, the pendingtree should be empty.
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,NULL,&pTreeDiff)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"TreeDiffStatus at %s:%d\n",__FILE__,__LINE__)  );
	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx, pTreeDiff,SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__compute_stats(pCtx, pTreeDiff,&tdStats)  );
	VERIFY_COND("status count",(tdStats.nrTotalChanges == 0));
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);

	//////////////////////////////////////////////////////////////////
	// changeset committed.  verify contents of repo.
	//////////////////////////////////////////////////////////////////

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvhRoot)  );

	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhRoot, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvhRoot, "d1")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhRoot, "d1", szgid_d1, sizeof(szgid_d1))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvhRoot, "d1", &pvhDir1)  );

	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhDir1, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvhDir1, "d2")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhDir1, "d2", szgid_d2, sizeof(szgid_d2))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvhDir1, "d2", &pvhDir2)  );

	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhDir2, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvhDir2, "d3")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhDir2, "d3", szgid_d3, sizeof(szgid_d3))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvhDir2, "d3", &pvhDir3)  );

	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhDir3, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvhDir3, "d4")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhDir3, "d4", szgid_d4, sizeof(szgid_d4))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvhDir3, "d4", &pvhDir4)  );

	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhDir4, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvhDir4, "d5")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhDir4, "d5", szgid_d5, sizeof(szgid_d5))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvhDir4, "d5", &pvhDir5)  );

	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhDir5, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvhDir5, "d6")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhDir5, "d6", szgid_d6, sizeof(szgid_d6))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvhDir5, "d6", &pvhDir6)  );

	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhDir6, 6, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhDir6, "f6a")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhDir6, "f6b")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhDir6, "f6c")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhDir6, "f6d")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhDir6, "f6e")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhDir6, "f6f")  );

	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhDir6, "f6a", szgid_f6a, sizeof(szgid_f6a))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhDir6, "f6b", szgid_f6b, sizeof(szgid_f6b))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhDir6, "f6c", szgid_f6c, sizeof(szgid_f6c))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhDir6, "f6d", szgid_f6d, sizeof(szgid_f6d))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhDir6, "f6e", szgid_f6e, sizeof(szgid_f6e))  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__get_gid(pCtx, pvhDir6, "f6f", szgid_f6f, sizeof(szgid_f6f))  );

	SG_VHASH_NULLFREE(pCtx, pvhRoot);
	pvhDir1 = NULL;
	pvhDir2 = NULL;
	pvhDir3 = NULL;
	pvhDir4 = NULL;
	pvhDir5 = NULL;
	pvhDir6 = NULL;

	//////////////////////////////////////////////////////////////////
	// begin making changes to working-directory for second changeset.
	//////////////////////////////////////////////////////////////////

	/* MODIFIED:  Modify file .../d6/f6a */
	VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pPathDir6, "f6a", 4)  );

	/* MOVED:  Move file .../d6/f6b from d6 to d3 */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir6, "f6b")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPathDir3, SG_FALSE)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	/* RENAMED:  Rename file .../d6/f6c as formerly_f6c */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir6, "f6c")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "formerly_f6c", SG_FALSE)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	/* DELETED:  Remove f6d officially */
	{
		const char* pszName_f6d = "f6d";
		
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathDir6, 1, &pszName_f6d, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

	/* ADDED:  Add f6g officially */
	{
		const char* pszName_f6g = "f6g";
		
		VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir6, pszName_f6g, 44)  );
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__add(pCtx, pPendingTree, pPathDir6, 1, &pszName_f6g, SG_FALSE, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

	/* MOVED: Move directory .../d4/d5 to d3 */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathDir5, 1, pPathDir3, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	// NOTE: now pPathDir5 and pPathDir6 are stale, so we reset them.
	SG_PATHNAME_NULLFREE(pCtx, pPathDir5);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir6);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir5, pPathDir3, "d5")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir6, pPathDir5, "d6")  );

	/* RENAMED:  Rename d2 to d2prime */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathDir2, "d2prime", SG_FALSE)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	//////////////////////////////////////////////////////////////////
	// verify that the pending changes match what we expect
	//////////////////////////////////////////////////////////////////

	// verify the status summary is consistent.
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,NULL,&pTreeDiff)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"TreeDiffStatus at %s:%d\n",__FILE__,__LINE__)  );
	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx, pTreeDiff,SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__compute_stats(pCtx, pTreeDiff,&tdStats)  );
 	VERIFY_COND("status count",(tdStats.nrSectionsWithChanges == 5));

// TODO update the following commented-out lines to use the new sg_treediff2 code
// TODO to lookup differences rather than the obsolete sg_status code.
//	VERIFY_ERR_CHECK(  SG_status__get_summary_vhash(pCtx, pStatus,&pvhStatusSummary)  );

	/* Check the moves */
	VERIFY_COND("moved count",(tdStats.nrMoved == 2));
//	VERIFY_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhStatusSummary, szgid_f6b, &pvhItem)  );
//	VERIFY_ERR_CHECK_DISCARD(  u0043_pendingtree__verify_vhash_sz(pvhItem, SG_STATUS_ENTRY_KEY__MOVED_FROM_GID, szgid_d6)  );
//	VERIFY_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhStatusSummary, szgid_d5, &pvhItem)  );
//	VERIFY_ERR_CHECK_DISCARD(  u0043_pendingtree__verify_vhash_sz(pvhItem, SG_STATUS_ENTRY_KEY__MOVED_FROM_GID, szgid_d4)  );
	
	/* Check the adds */
	VERIFY_COND("added count",(tdStats.nrAdded == 1));
//	VERIFY_ERR_CHECK_DISCARD(  u0043_pendingtree__verify_vhash_has_path_in_section(pvhStatusSummary, SG_STATUS_TYPE__ADDED, "@/d1/d2prime/d3/d5/d6/f6g")  );
	
	/* Check the renames */
	VERIFY_COND("rename count",(tdStats.nrRenamed == 2));
//	VERIFY_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhStatusSummary, szgid_f6c, &pvhItem)  );
//	VERIFY_ERR_CHECK_DISCARD(  u0043_pendingtree__verify_vhash_sz(pvhItem, SG_STATUS_ENTRY_KEY__RENAMED_FROM_NAME, "f6c")  );
//	VERIFY_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhStatusSummary, szgid_d2, &pvhItem)  );
//	VERIFY_ERR_CHECK_DISCARD(  u0043_pendingtree__verify_vhash_sz(pvhItem, SG_STATUS_ENTRY_KEY__RENAMED_FROM_NAME, "d2")  );
	
	/* Check the deletes */
	VERIFY_COND("delete count",(tdStats.nrDeleted == 1));
//	VERIFY_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhStatusSummary, szgid_f6d, &pvhItem)  );
//	VERIFY_ERR_CHECK_DISCARD(  u0043_pendingtree__verify_vhash_sz(pvhItem, SG_STATUS_ENTRY_KEY__REPO_PATH, "@/d1/d2prime/d3/d5/d6/f6d")  );
	
	/* Check the modifieds */
	VERIFY_COND("modified count",(tdStats.nrFileSymlinkModified == 1));
//	VERIFY_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvhStatusSummary, szgid_f6a, &pvhItem)  );
//	VERIFY_ERR_CHECK_DISCARD(  u0043_pendingtree__verify_vhash_sz(pvhItem, SG_STATUS_ENTRY_KEY__REPO_PATH, "@/d1/d2prime/d3/d5/d6/f6a")  );
	
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);


	/* Now check all the paths by gid */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_d1,  "@/d1/")  );
	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_d2,  "@/d1/d2prime/")  );
	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_d3,  "@/d1/d2prime/d3/")  );
	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_d4,  "@/d1/d2prime/d3/d4/")  );
	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_d5,  "@/d1/d2prime/d3/d5/")  );
	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_d6,  "@/d1/d2prime/d3/d5/d6/")  );
	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f6a, "@/d1/d2prime/d3/d5/d6/f6a")  );
	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f6b, "@/d1/d2prime/d3/f6b")  );
	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f6c, "@/d1/d2prime/d3/d5/d6/formerly_f6c")  );
	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f6d, "@/d1/d2prime/d3/d5/d6/f6d")  );	// deleted
	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f6e, "@/d1/d2prime/d3/d5/d6/f6e")  );
	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f6f, "@/d1/d2prime/d3/d5/d6/f6f")  );
	// we cannot verify f6g because it was added after we looked up all of the Permanent Object GIDs.
	// to do so, we would need to repeat all of the __get_entire_repo_manifest()... code.
	//VERIFY_ERR_CHECK_DISCARD(  _ut_pt__verify_gid_path(pCtx, pPendingTree, szgid_f6g, "@/d1/d2prime/d3/d5/d6/f6g")  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);


	/* Now commit */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );		// scan for lost/found/modified
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Status should have nothing in it right now */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,NULL,&pTreeDiff)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"TreeDiffStatus at %s:%d\n",__FILE__,__LINE__)  );
	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx, pTreeDiff,SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__compute_stats(pCtx, pTreeDiff,&tdStats)  );
	VERIFY_COND("status count",(tdStats.nrTotalChanges == 0));
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir3);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir4);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir5);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir6);

	return;

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_VHASH_NULLFREE(pCtx, pvhRoot);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir3);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir4);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir5);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir6);

	return;
}

void u0043_pendingtree_test__scan_should_ignore_some_stuff(SG_context * pCtx, SG_pathname* pPathTopDir, SG_bool bIgnoreBackups)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_d1 = NULL;
	SG_pathname* pPath_f1 = NULL;
	SG_pathname* pPath_f2 = NULL;
#if defined(MAC) || defined(LINUX)
	SG_pathname* pPath_link1 = NULL;
#endif
	SG_pendingtree* pPendingTree = NULL;
    SG_fsobj_type t;
    SG_bool bExists = SG_FALSE;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1, pPathWorkingDir, "d1")  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "f1", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "f2", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "f3", 20)  );
#if defined(MAC) || defined(LINUX)
	VERIFY_ERR_CHECK(  u0043_pendingtree__create_symlink(pCtx, pPathWorkingDir, "link1", "f1")  );
#endif

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f1, pPathWorkingDir, "f1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f2, pPathWorkingDir, "f2")  );
#if defined(MAC) || defined(LINUX)
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_link1, pPathWorkingDir, "link1")  );
#endif

	VERIFY_ERR_CHECK(  _ut_pt__new_repo3(pCtx, bufName, pPathWorkingDir, bIgnoreBackups)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	
    /* create a "found" file */
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "f4", 20)  );

    /* remove f2 */
	VERIFY_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath_f2)  );

#if defined(MAC) || defined(LINUX)
    /* replace link1 with a directory */
	VERIFY_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath_link1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_link1)  );
#endif

#if defined(MAC) || defined(LINUX)
    /* replace f1 with a symlink */
	VERIFY_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath_f1)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__create_symlink(pCtx, pPathWorkingDir, "f1", "nowhere")  );
#endif

    /* replace d1 with a file */
    VERIFY_ERR_CHECK(  SG_fsobj__rmdir__pathname(pCtx, pPath_d1)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "d1", 33)  );

    /* leave f3 untouched */

    /* the pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

    /* now scan */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );

    /* nothing should be found, so it should STILL be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

    /* now addremove */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

    /* pending changeset should now have stuff in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After various entries created.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/d1",            _ut_pt__PTNODE_DELETED)  );	// replace dir
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1",            _ut_pt__PTNODE_ZERO)  );		//     with file
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/f2",            _ut_pt__PTNODE_DELETED)  );	// DELETED or LOST ?
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/f3",            _ut_pt__PTNODE_ZERO)  );		// unchanged
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/f4",            _ut_pt__PTNODE_ZERO)  );		// FOUND

#if defined(MAC) || defined(LINUX)
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<l>@/link1",         _ut_pt__PTNODE_DELETED)  );	// replace link
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/link1",         _ut_pt__PTNODE_ZERO)  );		//     with dir

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/f1",            _ut_pt__PTNODE_DELETED)  );	// replace file
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<L>@/f1",            _ut_pt__PTNODE_ZERO)  );		//     with link
#else
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<L>@/link1",         _ut_pt__PTNODE_ZERO)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/f1",            _ut_pt__PTNODE_ZERO)  );
#endif

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/d1",   SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1",   SG_DIFFSTATUS_FLAGS__ADDED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/f2",   SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/f3",   SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/f4",   SG_DIFFSTATUS_FLAGS__ADDED)  );
#if defined(MAC) || defined(LINUX)
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<l>@/link1",   SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/link1",   SG_DIFFSTATUS_FLAGS__ADDED)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/f1",   SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<L>@/f1",   SG_DIFFSTATUS_FLAGS__ADDED)  );
#else
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<L>@/link1",   SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/f1",   SG_DIFFSTATUS_FLAGS__ZERO)  );
#endif

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////

	/* Now revert */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	//////////////////////////////////////////////////////////////////

	/* The changeset should now be empty again */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f1, &bExists, &t, NULL)  );
	VERIFY_COND("here", (bExists));
    VERIFY_COND("type", (SG_FSOBJ_TYPE__REGULAR == t));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f2, &bExists, &t, NULL)  );
	VERIFY_COND("here", (bExists));
    VERIFY_COND("type", (SG_FSOBJ_TYPE__REGULAR == t));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1, &bExists, &t, NULL)  );
	VERIFY_COND("here", (bExists));
    VERIFY_COND("type", (SG_FSOBJ_TYPE__DIRECTORY == t));

#if defined(MAC) || defined(LINUX)
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_link1, &bExists, &t, NULL)  );
	VERIFY_COND("here", (bExists));
    VERIFY_COND("type", (SG_FSOBJ_TYPE__SYMLINK == t));
#endif

	//////////////////////////////////////////////////////////////////
	// verify that the pending tree matches our expectation after the revert is finished.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After REVERT completed.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1",            _ut_pt__PTNODE_ZERO)  );		// restored dir
	if (!bIgnoreBackups) VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1~sg00~",      _ut_pt__PTNODE_FOUND)  );		// backed up file
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/f2",            _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/f3",            _ut_pt__PTNODE_ZERO)  );		// unchanged
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/f4",            _ut_pt__PTNODE_FOUND)  );		// FOUND

#if defined(MAC) || defined(LINUX)
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<L>@/link1",         _ut_pt__PTNODE_ZERO)  );		// restored link
	if (!bIgnoreBackups) VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/link1~sg00~",   _ut_pt__PTNODE_FOUND)  );		// backed up dir

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/f1",            _ut_pt__PTNODE_ZERO)  );		// restored file
	if (!bIgnoreBackups) VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<L>@/f1~sg00~",      _ut_pt__PTNODE_FOUND)  );		// backed up link
#else
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<L>@/link1",         _ut_pt__PTNODE_ZERO)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/f1",            _ut_pt__PTNODE_ZERO)  );
#endif

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1",         SG_DIFFSTATUS_FLAGS__ZERO)  );
	if (!bIgnoreBackups) VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1~sg00~",   SG_DIFFSTATUS_FLAGS__FOUND)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/f2",         SG_DIFFSTATUS_FLAGS__ZERO)  );		// un-deleted
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/f3",         SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/f4",         SG_DIFFSTATUS_FLAGS__FOUND)  );		// was ADDED, now is just FOUND
#if defined(MAC) || defined(LINUX)
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<L>@/link1",      SG_DIFFSTATUS_FLAGS__ZERO)  );
	if (!bIgnoreBackups) VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/link1~sg00~",SG_DIFFSTATUS_FLAGS__FOUND)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/f1",         SG_DIFFSTATUS_FLAGS__ZERO)  );
	if (!bIgnoreBackups) VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<L>@/f1~sg00~",   SG_DIFFSTATUS_FLAGS__FOUND)  );
#else
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<L>@/link1",      SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/f1",         SG_DIFFSTATUS_FLAGS__ZERO)  );
#endif

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////

    /* TODO IS THIS STILL A VALID COMMENT: if revert were a LOT more heavy-handed, we would check here to see if f4 is now gone */

	SG_PATHNAME_NULLFREE(pCtx, pPath_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
#if defined(MAC) || defined(LINUX)
	SG_PATHNAME_NULLFREE(pCtx, pPath_link1);
#endif
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

    return;

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPath_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
#if defined(MAC) || defined(LINUX)
	SG_PATHNAME_NULLFREE(pCtx, pPath_link1);
#endif
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

    return;
}

void u0043_pendingtree_test__partial_revert(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_d1 = NULL;
	SG_pathname* pPath_d2 = NULL;
	SG_pathname* pPath_f0a = NULL;
	SG_pathname* pPath_f0b = NULL;
	SG_pathname* pPath_f0d = NULL;
	SG_pathname* pPath_f1a = NULL;
	SG_pathname* pPath_f1c = NULL;
	SG_pathname* pPath_f2a = NULL;
	SG_pathname* pPath_f2b = NULL;
	SG_pathname* pPath_f2c = NULL;
	SG_pathname* pPath_formerly_f0b = NULL;
	SG_pathname* pPath_formerly_f2b = NULL;
	SG_pathname* pPath_newhome_f0a = NULL;
	SG_pendingtree* pPendingTree = NULL;
    SG_bool bExists = SG_FALSE;
    SG_uint64 len_orig_f1c = 0;
    SG_uint64 len_orig_f2c = 0;
    SG_uint64 len_mod_f1c = 0;
    SG_uint64 len_mod_f2c = 0;
    SG_uint64 len_now_f1c = 0;
    SG_uint64 len_now_f2c = 0;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

    /* create the stuff to go into the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d2, pPathWorkingDir, "d2")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d2)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "f0a", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "f0b", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "f0c", 20)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1, "f1a", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1, "f1b", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1, "f1c", 20)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d2, "f2a", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d2, "f2b", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d2, "f2c", 20)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
    /* create paths */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f0a, pPathWorkingDir, "f0a")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f0b, pPathWorkingDir, "f0b")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f0d, pPathWorkingDir, "f0d")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f1a, pPath_d1, "f1a")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f1c, pPath_d1, "f1c")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f2a, pPath_d2, "f2a")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f2b, pPath_d2, "f2b")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f2c, pPath_d2, "f2c")  );

    /* ---------------------------------------------------------------- */
    /* now make a bunch of changes all over */

    /* move f0a to d1 */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_f0a, 1, pPath_d1, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_newhome_f0a, pPath_d1, "f0a")  );

    /* rename f0b */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPath_f0b, "formerly_f0b", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_formerly_f0b, pPathWorkingDir, "formerly_f0b")  );

    /* add f0d */
	{
		const char* pszName = "f0d";
		
		VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "f0d", 44)  );
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__add(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_FALSE, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

    /* remove f1a */
	{
		const char* pszName = "f1a";
		
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPath_d1, 1, &pszName, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

    /* modify f1c */
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath_f1c, &len_orig_f1c, NULL)  );
	VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pPath_d1, "f1c", 17)  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath_f1c, &len_mod_f1c, NULL)  );
    VERIFY_COND("len", (len_orig_f1c < len_mod_f1c));

    /* remove f2a */
	{
		const char* pszName = "f2a";
		
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPath_d2, 1, &pszName, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

    /* rename f2b */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPath_f2b, "formerly_f2b", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_formerly_f2b, pPathWorkingDir, "formerly_f2b")  );

    /* modify f2c */
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath_f2c, &len_orig_f2c, NULL)  );
	VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pPath_d2, "f2c", 17)  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath_f2c, &len_mod_f2c, NULL)  );
    VERIFY_COND("len", (len_orig_f2c < len_mod_f2c));

    /* add f2d */
	{
		const char* pszName = "f2d";
		
		VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d2, "f2d", 44)  );
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__add(pCtx, pPendingTree, pPath_d2, 1, &pszName, SG_FALSE, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

    /* ---------------------------------------------------------------- */
    /* verify the changes happened */

    /* make sure f1a is gone */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f1a, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (!bExists));

    /* make sure f2a is gone */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f2a, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (!bExists));

    /* make sure f2b is gone */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f2b, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (!bExists));

    /* ---------------------------------------------------------------- */
    /* now revert the changes in d2 only */
    {
        const char* pszName = "d2";

        VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
        VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_TRUE, NULL, 0, NULL, 0)  );
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
    }

    /* ---------------------------------------------------------------- */
    /* make sure stuff in d2 was reverted */

    /* make sure f2a is back */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f2a, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (bExists));

    /* make sure f2b is back */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f2b, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (bExists));

    /* make sure formerly f2b is gone */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_formerly_f2b, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (!bExists));

    /* make sure f2c is back the way it was */
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath_f2c, &len_now_f2c, NULL)  );
    VERIFY_COND("len", (len_now_f2c == len_orig_f2c));

    /* TODO make sure f2d is gone? */

    /* ---------------------------------------------------------------- */
    /* and make sure the other stuff was not reverted */

    /* make sure f0a is still gone */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f0a, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (!bExists));

    /* make sure f0a is now in d1 */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_newhome_f0a, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (bExists));

    /* make sure f0b is still gone */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f0b, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (!bExists));

    /* make sure formerly f0b is still there */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_formerly_f0b, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (bExists));

    /* make sure f0d is still there */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f0d, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (bExists));

    /* make sure f1a is still gone */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f1a, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (!bExists));

    /* make sure f1c is still modified */
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath_f1c, &len_now_f1c, NULL)  );
    VERIFY_COND("len", (len_now_f1c == len_mod_f1c));

    /* ---------------------------------------------------------------- */

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

    SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
    SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
    SG_PATHNAME_NULLFREE(pCtx, pPath_d2);

    SG_PATHNAME_NULLFREE(pCtx, pPath_f0a);
    SG_PATHNAME_NULLFREE(pCtx, pPath_f0b);
    SG_PATHNAME_NULLFREE(pCtx, pPath_f0d);

    SG_PATHNAME_NULLFREE(pCtx, pPath_f1a);
    SG_PATHNAME_NULLFREE(pCtx, pPath_f1c);

    SG_PATHNAME_NULLFREE(pCtx, pPath_f2a);
    SG_PATHNAME_NULLFREE(pCtx, pPath_f2b);
    SG_PATHNAME_NULLFREE(pCtx, pPath_f2c);

    SG_PATHNAME_NULLFREE(pCtx, pPath_formerly_f0b);
    SG_PATHNAME_NULLFREE(pCtx, pPath_formerly_f2b);
    SG_PATHNAME_NULLFREE(pCtx, pPath_newhome_f0a);
}

/**
 * make a bunch of changes and delete a part of the tree and
 * revert stuff within the deleted directory, but don't revert
 * the deleted directory -- this should throw an error because
 * we can't repopulate the stuff.
 */
void u0043_pendingtree_test__partial_revert_conflict(SG_context * pCtx, SG_pathname * pPathTopDir, SG_bool bImplicit)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_d1 = NULL;
	SG_pathname* pPath_d1_d2 = NULL;
	SG_pathname* pPath_d1_d2_d3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f1 = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_bool bExists;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)	 );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)	);

	INFOP("partial_revert_conflict",("[WD %s][bImplicit %d]",SG_pathname__sz(pPathWorkingDir),bImplicit));

	// mkdir @/d1
	// mkdir @/d1/d2
	// mkdir @/d1/d2/d3
	// mkdir @/d1/d2/d3/d4
	// create @/d1/d2/d3/d4/f1
	
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1, pPathWorkingDir, "d1")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2, pPath_d1, "d2")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2)	 );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3, pPath_d1_d2, "d3")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3)	);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4, pPath_d1_d2_d3, "d4")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3_d4)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f1, pPath_d1_d2_d3_d4, "f1")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f1", 20)  );

	// commit all of this

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)	);
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)	 );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

	if (bImplicit)
	{
		// delete @/d1/d2/d3/ (which implicitly gets d4 and f1)

		const char * pszName = "d1/d2/d3";

		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, NULL, 0, NULL, 0, SG_FALSE)	 );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}
	else
	{
		// delete @/d1/d2/d3/d4/f1
		// delete @/d1/d2/d3/d4/
		// delete @/d1/d2/d3/

		const char * apszName[] = { "d1/d2/d3/d4/f1",
									"d1/d2/d3/d4",
									"d1/d2/d3" 
		};
		SG_uint32 nr = SG_NrElements(apszName);

		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, nr, apszName, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}
	
	// verify the stuff is gone

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("delete of d1/d2/d3/d4/f1", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3/d4", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3", (!bExists));

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After various changes.")  );

	if (bImplicit)
	{
		VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/d1/d2/d3/d4/f1",         _ut_pt__PTNODE_LOST)  );
		VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/d1/d2/d3/d4",            _ut_pt__PTNODE_LOST)  );
	}
	else
	{
		VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/d1/d2/d3/d4/f1",         _ut_pt__PTNODE_DELETED)  );
		VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/d1/d2/d3/d4",            _ut_pt__PTNODE_DELETED)  );
	}
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/d1/d2/d3",               _ut_pt__PTNODE_DELETED)  );

	// VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
	// VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	if (bImplicit)
	{
		VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/d1/d2/d3/d4/f1",       SG_DIFFSTATUS_FLAGS__LOST)  );
		VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/d1/d2/d3/d4",          SG_DIFFSTATUS_FLAGS__LOST)  );
	}
	else
	{
		VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/d1/d2/d3/d4/f1",       SG_DIFFSTATUS_FLAGS__DELETED)  );
		VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/d1/d2/d3/d4",          SG_DIFFSTATUS_FLAGS__DELETED)  );
	}
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/d1/d2/d3",             SG_DIFFSTATUS_FLAGS__DELETED)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////

	{
		// revert the implicit/explicit delete of f1 only -- this should throw an error since d3 and d4 don't exist

		const char* pszName = "d1/d2/d3/d4/f1";

		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
#if 1
		// TODO 2010/06/08 Before I converted SG_file_spec__should_include() to return an EVAL rather
		// TODO            than a BOOL, the old mark_some_reverting code threw parent-dir-missing-or-inactive.
		// TODO            After the change we need to fix sprawl-825 and do inactive lookups in maybe_mark_items_for_revert.
		// TODO            For now, they just return not-found.
		VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_TRUE, NULL, 0, NULL, 0),
											  SG_ERR_NOT_FOUND);
#else
		VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_TRUE, NULL, 0, NULL, 0),
											  SG_ERR_PENDINGTREE_PARENT_DIR_MISSING_OR_INACTIVE);
#endif
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f1);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);
	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
}

/**
 * create a nested tree.
 * move some deep file out to "@/".
 * delete a nested set of directories.
 * (CAUSING AN IMPLICIT DELETE OF THE STUFF UNDER THE GIVEN DIRECTORY.)
 *
 * revert.
 *
 * make sure that intermediate directories get recreated before we move the file back into the directories.
 *
 * the files we move get moved to @/ and this is currently causing problems.
 */
void u0043_pendingtree_test__revert__implicit_nested_delete_with_moves_to_root(SG_context * pCtx, SG_pathname * pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_d1 = NULL;
	SG_pathname* pPath_d1_d2 = NULL;
	SG_pathname* pPath_d1_d2_d3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f1 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f2 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f4 = NULL;
	SG_pathname* pPath_f1 = NULL;
	SG_pathname* pPath_f2 = NULL;
	SG_pathname* pPath_f3 = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_bool bExists;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)	 );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)	);

	// mkdir @/d1
	// mkdir @/d1/d2
	// mkdir @/d1/d2/d3
	// mkdir @/d1/d2/d3/d4
	// create @/d1/d2/d3/d4/f1
	// create @/d1/d2/d3/d4/f2
	// create @/d1/d2/d3/d4/f3
	
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1, pPathWorkingDir, "d1")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2, pPath_d1, "d2")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2)	 );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3, pPath_d1_d2, "d3")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3)	);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4, pPath_d1_d2_d3, "d4")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3_d4)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f1, pPath_d1_d2_d3_d4, "f1")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f1", 20)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f2, pPath_d1_d2_d3_d4, "f2")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f2", 25)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f3, pPath_d1_d2_d3_d4, "f3")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f3", 30)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f4, pPath_d1_d2_d3_d4, "f4")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f4", 35)  );

	// commit all of this

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)	);
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)	 );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

	// move @/d1/d2/d3/d4/f[123] to @/f[123]
	// (leave @/d1/d2/d3/d4/f4 as it is)

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f1, pPathWorkingDir, "f1")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f2, pPathWorkingDir, "f2")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_f3, pPathWorkingDir, "f3")	);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_f1, 1, pPathWorkingDir, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_f2, 1, pPathWorkingDir, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_f3, 1, pPathWorkingDir, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	// verify that they moved
	
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f1", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f2", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f3, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f3", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f1", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f2", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_f3, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f3", (bExists));

	//////////////////////////////////////////////////////////////////

	{
		// delete @/d1/d2/ (and implicitly delete everything under it)

		const char * pszName = "d1/d2";

		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, NULL, 0, NULL, 0, SG_FALSE)	 );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

	// verify the stuff is gone

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f4, &bExists, NULL, NULL)	 );
	VERIFY_COND("delete of d1/d2/d3/d4/f4", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3/d4", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2", (!bExists));

	//////////////////////////////////////////////////////////////////

	// revert everything.  this should cause __undelete_skeleton to re-build the tree
	// before f[123] get moved back into d4.

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_f3);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

/**
 * create a nested tree.
 * move some deep file out.
 * delete a nested set of directories.
 * (CAUSING AN IMPLICIT DELETE OF THE STUFF UNDER THE GIVEN DIRECTORY.)
 *
 * revert.
 * 
 * make sure that intermediate directories get recreated before we move the file back into the directories.
 *
 * the files we move get moved to @/dx.
 */
void u0043_pendingtree_test__revert__implicit_nested_delete_with_moves_to_another_dir(SG_context * pCtx, SG_pathname * pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_dx = NULL;
	SG_pathname* pPath_d1 = NULL;
	SG_pathname* pPath_d1_d2 = NULL;
	SG_pathname* pPath_d1_d2_d3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f1 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f2 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f4 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_d5 = NULL;
	SG_pathname* pPath_dx_f1 = NULL;
	SG_pathname* pPath_dx_f2 = NULL;
	SG_pathname* pPath_dx_f3 = NULL;
	SG_pathname* pPath_dx_d5 = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_bool bExists;

	INFOP("bugbug",("TODO verify that @/d1/d2/d3/d4 (implied) is in the treediff status report that we produce."));

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)	 );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)	);

	// mkdir @/dx
	// mkdir @/d1
	// mkdir @/d1/d2
	// mkdir @/d1/d2/d3
	// mkdir @/d1/d2/d3/d4
	// create @/d1/d2/d3/d4/f1
	// create @/d1/d2/d3/d4/f2
	// create @/d1/d2/d3/d4/f3
	// create @/d1/d2/d3/d4/d5
	
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx, pPathWorkingDir, "dx")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_dx)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1, pPathWorkingDir, "d1")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2, pPath_d1, "d2")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2)	 );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3, pPath_d1_d2, "d3")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3)	);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4, pPath_d1_d2_d3, "d4")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3_d4)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f1, pPath_d1_d2_d3_d4, "f1")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f1", 20)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f2, pPath_d1_d2_d3_d4, "f2")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f2", 25)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f3, pPath_d1_d2_d3_d4, "f3")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f3", 30)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f4, pPath_d1_d2_d3_d4, "f4")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f4", 35)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_d5, pPath_d1_d2_d3_d4, "d5")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3_d4_d5)  );

	// commit all of this

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)	);
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)	 );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

	// move @/d1/d2/d3/d4/f[123] to @/dx/f[123]
	// (leave @/d1/d2/d3/d4/f4 as it is)
	// move @/d1/d2/d3/d4/d5 to @/dx/d5

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_f1, pPath_dx, "f1")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_f2, pPath_dx, "f2")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_f3, pPath_dx, "f3")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_d5, pPath_dx, "d5")	);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_f1, 1, pPath_dx, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_f2, 1, pPath_dx, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_f3, 1, pPath_dx, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_d5, 1, pPath_dx, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	// verify that they moved
	
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f1", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f2", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f3, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f3", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_d5, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/d5", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f1", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f2", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f3, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f3", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_d5, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d5", (bExists));

	//////////////////////////////////////////////////////////////////

	{
		// delete @/d1/d2/ (and implicitly delete everything under it)

		const char * pszName = "d1/d2";

		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, NULL, 0, NULL, 0, SG_FALSE)	 );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

	// verify the stuff is gone

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f4, &bExists, NULL, NULL)	 );
	VERIFY_COND("delete of d1/d2/d3/d4/f4", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3/d4", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2", (!bExists));

	//////////////////////////////////////////////////////////////////

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After delete of @/d1/d2 (with IMPLICIT delete of stuff under it)")  );

	// NOTE: as of Eric's 20100105 changes, the implicitly deleted stuff appears as LOST.

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/d1/d2",                  _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/d1/d2/d3",               _ut_pt__PTNODE_LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/d1/d2/d3/d4",            _ut_pt__PTNODE_LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/d1/d2/d3/d4/f1",sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/d1/d2/d3/d4/f2",sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/d1/d2/d3/d4/f3",sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/d1/d2/d3/d4/f4",         _ut_pt__PTNODE_LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D><phantom>@/d1/d2/d3/d4/d5",sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );

	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1",                     SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/d1/d2",                  SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/d1/d2/d3",               SG_DIFFSTATUS_FLAGS__LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/d1/d2/d3/d4",            SG_DIFFSTATUS_FLAGS__LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F><phantom>@/d1/d2/d3/d4/f1",SG_DIFFSTATUS_FLAGS__MOVED)  ); // source of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F><phantom>@/d1/d2/d3/d4/f2",SG_DIFFSTATUS_FLAGS__MOVED)  ); // source of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F><phantom>@/d1/d2/d3/d4/f3",SG_DIFFSTATUS_FLAGS__MOVED)  ); // source of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/d1/d2/d3/d4/f4",         SG_DIFFSTATUS_FLAGS__LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx",                     SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f1",                  SG_DIFFSTATUS_FLAGS__MOVED)  ); // destination of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f2",                  SG_DIFFSTATUS_FLAGS__MOVED)  ); // destination of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f3",                  SG_DIFFSTATUS_FLAGS__MOVED)  ); // destination of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx/d5",                  SG_DIFFSTATUS_FLAGS__MOVED)  ); // destination of move

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////

	// revert everything.  this should cause __undelete_skeleton to re-build the tree
	// before f[123] and d5 get moved back into d4.

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	// verify that the pending tree matches our expectation after the revert is finished.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After REVERT completed.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1",            _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3",      _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3/d4",   _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f1",_ut_pt__PTNODE_ZERO)  );	// unchanged
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f2",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f3",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f4",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3/d4/d5",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f1",         _ut_pt__PTNODE_ZERO)  );  // should not exist
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f2",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f3",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/dx/d5",         _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1",            SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2",         SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2/d3",      SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2/d3/d4",   SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f1",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f2",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f3",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f4",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2/d3/d4/d5",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx",            SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f1",         SG_DIFFSTATUS_FLAGS__ZERO)  );	// should not exist
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f2",         SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f3",         SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx/d5",         SG_DIFFSTATUS_FLAGS__ZERO)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_d5);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_f3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_d5);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);
	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
}

/**
 * create a nested tree.
 * move some deep file out.
 * delete a nested set of directories -- BY EXPLICITLY DELETING EVERYTHING.
 * 
 * revert.
 * 
 * make sure that intermediate directories get recreated before we move the file back into the directories.
 *
 * the files we move get moved to @/dx.
 */
void u0043_pendingtree_test__revert__explicit_delete_with_moves_to_another_dir(SG_context * pCtx, SG_pathname * pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_dx = NULL;
	SG_pathname* pPath_d1 = NULL;
	SG_pathname* pPath_d1_d2 = NULL;
	SG_pathname* pPath_d1_d2_d3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f1 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f2 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f4 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_d5 = NULL;
	SG_pathname* pPath_dx_f1 = NULL;
	SG_pathname* pPath_dx_f2 = NULL;
	SG_pathname* pPath_dx_f3 = NULL;
	SG_pathname* pPath_dx_d5 = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_bool bExists;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)	 );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)	);

	// mkdir @/dx
	// mkdir @/d1
	// mkdir @/d1/d2
	// mkdir @/d1/d2/d3
	// mkdir @/d1/d2/d3/d4
	// create @/d1/d2/d3/d4/f1
	// create @/d1/d2/d3/d4/f2
	// create @/d1/d2/d3/d4/f3
	// create @/d1/d2/d3/d4/d5
	
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx, pPathWorkingDir, "dx")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_dx)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1, pPathWorkingDir, "d1")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2, pPath_d1, "d2")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2)	 );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3, pPath_d1_d2, "d3")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3)	);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4, pPath_d1_d2_d3, "d4")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3_d4)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f1, pPath_d1_d2_d3_d4, "f1")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f1", 20)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f2, pPath_d1_d2_d3_d4, "f2")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f2", 25)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f3, pPath_d1_d2_d3_d4, "f3")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f3", 30)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f4, pPath_d1_d2_d3_d4, "f4")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f4", 35)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_d5, pPath_d1_d2_d3_d4, "d5")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3_d4_d5)  );

	// commit all of this

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)	);
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)	 );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

	// move @/d1/d2/d3/d4/f[123] to @/dx/f[123]
	// (leave @/d1/d2/d3/d4/f4 as it is)
	// move @/d1/d2/d3/d4/d5 to @/dx/d5

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_f1, pPath_dx, "f1")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_f2, pPath_dx, "f2")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_f3, pPath_dx, "f3")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_d5, pPath_dx, "d5")	);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_f1, 1, pPath_dx, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_f2, 1, pPath_dx, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_f3, 1, pPath_dx, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_d5, 1, pPath_dx, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	// verify that they moved
	
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f1", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f2", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f3, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f3", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_d5, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/d5", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f1", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f2", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f3, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f3", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_d5, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d5", (bExists));

	//////////////////////////////////////////////////////////////////

	{
		// explicitly delete @/d1/d2/ and everything under it

		const char * apszName[] = { "d1/d2/d3/d4/f4",
									"d1/d2/d3/d4",
									"d1/d2/d3",
									"d1/d2" };
		SG_uint32 nrNames = SG_NrElements(apszName);

		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, nrNames, apszName, NULL, 0, NULL, 0, SG_FALSE)	 );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

	// verify the stuff is gone

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f4, &bExists, NULL, NULL)	 );
	VERIFY_COND("delete of d1/d2/d3/d4/f4", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3/d4", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2", (!bExists));

	//////////////////////////////////////////////////////////////////

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After explicity delete of @/d1/d2 and everything below it.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/d1/d2",                  _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/d1/d2/d3",               _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/d1/d2/d3/d4",            _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/d1/d2/d3/d4/f1",sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/d1/d2/d3/d4/f2",sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/d1/d2/d3/d4/f3",sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/d1/d2/d3/d4/f4",         _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D><phantom>@/d1/d2/d3/d4/d5",sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );

	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1",                     SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/d1/d2",                  SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/d1/d2/d3",               SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/d1/d2/d3/d4",            SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F><phantom>@/d1/d2/d3/d4/f1",SG_DIFFSTATUS_FLAGS__MOVED)  ); // source of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F><phantom>@/d1/d2/d3/d4/f2",SG_DIFFSTATUS_FLAGS__MOVED)  ); // source of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F><phantom>@/d1/d2/d3/d4/f3",SG_DIFFSTATUS_FLAGS__MOVED)  ); // source of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/d1/d2/d3/d4/f4",         SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D><phantom>@/d1/d2/d3/d4/d5",SG_DIFFSTATUS_FLAGS__MOVED)  ); // source of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx",                     SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f1",                  SG_DIFFSTATUS_FLAGS__MOVED)  ); // destination of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f2",                  SG_DIFFSTATUS_FLAGS__MOVED)  ); // destination of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f3",                  SG_DIFFSTATUS_FLAGS__MOVED)  ); // destination of move
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx/d5",                  SG_DIFFSTATUS_FLAGS__MOVED)  ); // destination of move

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////
	// revert everything.  this should cause __undelete_skeleton to re-build the tree
	// before f[123] and d5 get moved back into d4.

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	// verify that the pending tree matches our expectation after the revert is finished.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After REVERT completed.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1",            _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3",      _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3/d4",   _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f1",_ut_pt__PTNODE_ZERO)  );	// unchanged
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f2",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f3",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f4",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3/d4/d5",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f1",         _ut_pt__PTNODE_ZERO)  );  // should not exist
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f2",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f3",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/dx/d5",         _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1",            SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2",         SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2/d3",      SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2/d3/d4",   SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f1",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f2",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f3",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f4",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2/d3/d4/d5",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx",            SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f1",         SG_DIFFSTATUS_FLAGS__ZERO)  );	// should not exist
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f2",         SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f3",         SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx/d5",         SG_DIFFSTATUS_FLAGS__ZERO)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_d5);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_f3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_d5);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);
	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
}

/**
 * create a nested tree.  make some FOUND and LOST entries.
 * revert.
 *
 * the files we move get moved to @/dx.
 */
void u0043_pendingtree_test__revert__lost_found(SG_context * pCtx, SG_pathname * pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_dx = NULL;
	SG_pathname* pPath_d1 = NULL;
	SG_pathname* pPath_d1_d2 = NULL;
	SG_pathname* pPath_d1_d2_d3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f1 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f2 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f4 = NULL;
	SG_pathname* pPath_dx_f1 = NULL;
	SG_pathname* pPath_dx_f2 = NULL;
	SG_pathname* pPath_dx_f3 = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_bool bExists;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)	 );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)	);

	// mkdir @/dx
	// mkdir @/d1
	// mkdir @/d1/d2
	// mkdir @/d1/d2/d3
	// mkdir @/d1/d2/d3/d4
	// create @/d1/d2/d3/d4/f1
	// create @/d1/d2/d3/d4/f2
	// create @/d1/d2/d3/d4/f3
	// create @/d1/d2/d3/d4/f4
	
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx, pPathWorkingDir, "dx")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_dx)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1, pPathWorkingDir, "d1")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2, pPath_d1, "d2")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2)	 );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3, pPath_d1_d2, "d3")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3)	);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4, pPath_d1_d2_d3, "d4")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3_d4)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f1, pPath_d1_d2_d3_d4, "f1")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f1", 20)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f2, pPath_d1_d2_d3_d4, "f2")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f2", 25)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f3, pPath_d1_d2_d3_d4, "f3")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f3", 30)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f4, pPath_d1_d2_d3_d4, "f4")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f4", 35)  );

	// commit all of this

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)	);
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)	 );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

	// move @/d1/d2/d3/d4/f[123] to @/dx/f[123]  -- BUT WITHOUT TELLING PENDINGTREE, SO THEY WILL BE LOST/FOUND.
	// (leave @/d1/d2/d3/d4/f4 as it is)

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_f1, pPath_dx, "f1")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_f2, pPath_dx, "f2")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_f3, pPath_dx, "f3")	);

	VERIFY_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx, pPath_d1_d2_d3_d4_f1, pPath_dx_f1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx, pPath_d1_d2_d3_d4_f2, pPath_dx_f2)  );
	VERIFY_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx, pPath_d1_d2_d3_d4_f3, pPath_dx_f3)  );
	
	// verify that they moved
	
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f1", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f2", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f3, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f3", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f1", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f2", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f3, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f3", (bExists));

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After LOST/FOUND entries created.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1",            _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3",      _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3/d4",   _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/d1/d2/d3/d4/f1",_ut_pt__PTNODE_LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/d1/d2/d3/d4/f2",_ut_pt__PTNODE_LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/d1/d2/d3/d4/f3",_ut_pt__PTNODE_LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f1",         _ut_pt__PTNODE_FOUND)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f2",         _ut_pt__PTNODE_FOUND)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f3",         _ut_pt__PTNODE_FOUND)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2/d3/d4",   SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/d1/d2/d3/d4/f1",SG_DIFFSTATUS_FLAGS__LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/d1/d2/d3/d4/f2",SG_DIFFSTATUS_FLAGS__LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/d1/d2/d3/d4/f3",SG_DIFFSTATUS_FLAGS__LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx",            SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f1",         SG_DIFFSTATUS_FLAGS__FOUND)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f2",         SG_DIFFSTATUS_FLAGS__FOUND)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f3",         SG_DIFFSTATUS_FLAGS__FOUND)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////
	// revert everything.  this should recreate the LOST entries (in d4), but leave the FOUND
	// entries in dx.

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("restore of d1/d2/d3/d4/f1", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("restore of d1/d2/d3/d4/f2", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f3, &bExists, NULL, NULL)	 );
	VERIFY_COND("restore of d1/d2/d3/d4/f3", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("dx/f1", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("dx/f2", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f3, &bExists, NULL, NULL)	 );
	VERIFY_COND("dx/f3", (bExists));

	//////////////////////////////////////////////////////////////////
	// verify that the pending tree matches our expectation after the revert is finished.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After REVERT completed.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1",            _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3",      _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3/d4",   _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f1",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f2",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f3",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f1",         _ut_pt__PTNODE_FOUND)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f2",         _ut_pt__PTNODE_FOUND)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f3",         _ut_pt__PTNODE_FOUND)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2/d3/d4",   SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f1",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f2",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f3",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f4",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx",            SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f1",         SG_DIFFSTATUS_FLAGS__FOUND)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f2",         SG_DIFFSTATUS_FLAGS__FOUND)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f3",         SG_DIFFSTATUS_FLAGS__FOUND)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_f3);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);
}

/**
 * create a nested tree.
 * move and rename some entries.
 * then lose some of them.
 * 
 * revert.
 */
void u0043_pendingtree_test__revert__sadistic_lost_found(SG_context * pCtx, SG_pathname * pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_dx = NULL;
	SG_pathname* pPath_d1 = NULL;
	SG_pathname* pPath_d1_d2 = NULL;
	SG_pathname* pPath_d1_d2_d3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f1 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f2 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f3_renamed = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f4 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_d5 = NULL;
	SG_pathname* pPath_dx_f1 = NULL;
	SG_pathname* pPath_dx_f2 = NULL;
	SG_pathname* pPath_dx_f2_renamed = NULL;
	SG_pathname* pPath_dx_d5 = NULL;
	SG_pathname* pPath_dx_d5_renamed = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_bool bExists;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)	 );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)	);

	// mkdir @/dx
	// mkdir @/d1
	// mkdir @/d1/d2
	// mkdir @/d1/d2/d3
	// mkdir @/d1/d2/d3/d4
	// create @/d1/d2/d3/d4/f1
	// create @/d1/d2/d3/d4/f2
	// create @/d1/d2/d3/d4/f3
	// create @/d1/d2/d3/d4/f4
	
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx, pPathWorkingDir, "dx")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_dx)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1, pPathWorkingDir, "d1")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2, pPath_d1, "d2")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2)	 );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3, pPath_d1_d2, "d3")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3)	);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4, pPath_d1_d2_d3, "d4")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3_d4)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f1, pPath_d1_d2_d3_d4, "f1")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f1", 20)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f2, pPath_d1_d2_d3_d4, "f2")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f2", 25)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f3, pPath_d1_d2_d3_d4, "f3")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f3", 30)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f4, pPath_d1_d2_d3_d4, "f4")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f4", 35)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_d5, pPath_d1_d2_d3_d4, "d5")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3_d4_d5)  );

	// commit all of this

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)	);
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)	 );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	// Step 1:
	// 
	// move @/d1/d2/d3/d4/f1 to @/dx/f1
	// move+rename @/d1/d2/d3/d4/f2 to @/dx/f2_renamed
	// rename @/d1/d2/d3/d4/f3 to f3_renamed
	// (leave @/d1/d2/d3/d4/f4 as it is)
	// move+rename @/d1/d2/d3/d4/d5 to @/dx/d5_renamed

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_f1,                  pPath_dx, "f1")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_f2,                  pPath_dx, "f2")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_f2_renamed,          pPath_dx, "f2_renamed")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f3_renamed, pPath_d1_d2_d3_d4, "f3_renamed")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_d5,                  pPath_dx, "d5")	);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx_d5_renamed,          pPath_dx, "d5_renamed")	);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_f1, 1, pPath_dx, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_f2, 1, pPath_dx, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPath_dx_f2, "f2_renamed", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPath_d1_d2_d3_d4_f3, "f3_renamed", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPath_d1_d2_d3_d4_d5, 1, pPath_dx, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPath_dx_d5, "d5_renamed", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	// verify that they moved and/or were renamed (also verify intermediate steps)
	
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f1", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/f2", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("rename of dx/f2", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f3, &bExists, NULL, NULL)	 );
	VERIFY_COND("rename of d1/d2/d3/d4/f3", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_d5, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of d1/d2/d3/d4/d5", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_d5, &bExists, NULL, NULL)	 );
	VERIFY_COND("rename of dx/d5", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("move of f1", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f2_renamed, &bExists, NULL, NULL)	 );
	VERIFY_COND("move+rename of f2", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f3_renamed, &bExists, NULL, NULL)	 );
	VERIFY_COND("rename of f3", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_d5_renamed, &bExists, NULL, NULL)	 );
	VERIFY_COND("move+rename of d5", (bExists));

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD (part 1).

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"Part 1: After sadistic move+rename entries created.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2",                  _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3",               _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3/d4",            _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/d1/d2/d3/d4/f1",sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/d1/d2/d3/d4/f2",sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f3_renamed", _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f1",                  _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dx/f2_renamed",          _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/dx/d5_renamed",          _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2/d3/d4",              SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F><phantom>@/d1/d2/d3/d4/f1",  SG_DIFFSTATUS_FLAGS__MOVED)  );     // moved away
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F><phantom>@/d1/d2/d3/d4/f2",  SG_DIFFSTATUS_FLAGS__MOVED|SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f3_renamed",   SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D><phantom>@/d1/d2/d3/d4/d5",  SG_DIFFSTATUS_FLAGS__MOVED|SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx",                       SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f1",                    SG_DIFFSTATUS_FLAGS__MOVED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dx/f2_renamed",            SG_DIFFSTATUS_FLAGS__MOVED|SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx/d5_renamed",            SG_DIFFSTATUS_FLAGS__MOVED|SG_DIFFSTATUS_FLAGS__RENAMED)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////
	// Step 2:
	//
	// delete some stuff -- BUT WITHOUT TELLING PENDINGTREE, so they will become LOST
	// (in addition to the MOVE, RENAME, or MOVE+RENAME that we already did).
	// rm    @/dx/f1
	// rm    @/dx/f2_renamed
	// rm    @/d1/d2/d3/d4/f3_renamed
	// rmdir @/dx/d5_renamed

	VERIFY_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath_dx_f1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath_dx_f2_renamed)  );
	VERIFY_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath_d1_d2_d3_d4_f3_renamed)  );
	VERIFY_ERR_CHECK(  SG_fsobj__rmdir__pathname(pCtx, pPath_dx_d5_renamed)  );

	// verify that they are actually gone from the disk.

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("/bin/rm of f1", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f2_renamed, &bExists, NULL, NULL)	 );
	VERIFY_COND("/bin/rm of f2_renamed", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f3_renamed, &bExists, NULL, NULL)	 );
	VERIFY_COND("/bin/rm of f3_renamed", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_d5_renamed, &bExists, NULL, NULL)	 );
	VERIFY_COND("/bin/rm of d5_renamed", (!bExists));

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD (part 2).

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"Part 2: After /bin/rm after sadistic move+rename entries created.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2",                  _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3",               _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3/d4",            _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/d1/d2/d3/d4/f1",sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/d1/d2/d3/d4/f2",sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/d1/d2/d3/d4/f3_renamed", _ut_pt__PTNODE_LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/dx/f1",                  _ut_pt__PTNODE_LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/dx/f2_renamed",          _ut_pt__PTNODE_LOST)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/dx/d5_renamed",          _ut_pt__PTNODE_LOST)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2/d3/d4",              SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F><phantom>@/d1/d2/d3/d4/f1",  SG_DIFFSTATUS_FLAGS__LOST|SG_DIFFSTATUS_FLAGS__MOVED)  );     // moved away
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F><phantom>@/d1/d2/d3/d4/f2",  SG_DIFFSTATUS_FLAGS__LOST|SG_DIFFSTATUS_FLAGS__MOVED|SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/d1/d2/d3/d4/f3_renamed",   SG_DIFFSTATUS_FLAGS__LOST|SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D><phantom>@/d1/d2/d3/d4/d5",  SG_DIFFSTATUS_FLAGS__LOST|SG_DIFFSTATUS_FLAGS__MOVED|SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx",                       SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/dx/f1",                    SG_DIFFSTATUS_FLAGS__LOST|SG_DIFFSTATUS_FLAGS__MOVED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/dx/f2_renamed",            SG_DIFFSTATUS_FLAGS__LOST|SG_DIFFSTATUS_FLAGS__MOVED|SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/dx/d5_renamed",            SG_DIFFSTATUS_FLAGS__LOST|SG_DIFFSTATUS_FLAGS__MOVED|SG_DIFFSTATUS_FLAGS__RENAMED)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////
	// revert everything.  this should recreate the LOST entries (in d4) and undo all of the MOVEs and RENAMEs.
	// dx should be empty.

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("restore of d1/d2/d3/d4/f1", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("restore of d1/d2/d3/d4/f2", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f3, &bExists, NULL, NULL)	 );
	VERIFY_COND("restore of d1/d2/d3/d4/f3", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_d5, &bExists, NULL, NULL)	 );
	VERIFY_COND("restore of d1/d2/d3/d4/d5", (bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f1, &bExists, NULL, NULL)	 );
	VERIFY_COND("dx/f1", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f2, &bExists, NULL, NULL)	 );
	VERIFY_COND("dx/f2", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_f2_renamed, &bExists, NULL, NULL)	 );
	VERIFY_COND("dx/f2_renamed", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_d5, &bExists, NULL, NULL)	 );
	VERIFY_COND("dx/d5", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx_d5_renamed, &bExists, NULL, NULL)	 );
	VERIFY_COND("dx/d5_renamed", (!bExists));

	//////////////////////////////////////////////////////////////////
	// verify that the pending tree matches our expectation after the revert is finished.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After REVERT completed.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1",            _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3",      _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3/d4",   _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f1",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f2",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f3",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f4",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1/d2/d3/d4/d5",_ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2/d3/d4",   SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f1",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f2",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f3",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f4",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1/d2/d3/d4/d5",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx",            SG_DIFFSTATUS_FLAGS__ZERO)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////
	// since we reverted everything, the pendingtree should be empty.

	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);


fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f3_renamed);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_d5);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_f2_renamed);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_d5);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx_d5_renamed);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);
}

void u0043_pendingtree_test__revert__rename_needs_backup(SG_context * pCtx, SG_pathname* pPathTopDir, SG_bool bIgnoreBackups)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_vhash* pvh = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_bool bExists = SG_FALSE;
	SG_uint64 len_orig = 0;
	SG_uint64 len_now = 0;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );

	VERIFY_ERR_CHECK(  _ut_pt__new_repo3(pCtx, bufName, pPathWorkingDir, bIgnoreBackups)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* get the original length for comparison later */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_orig, NULL)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* And rename the file */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "foo", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

    /* now create another file in its place */
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 77)  );

	/* Now revert */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	/* verify that a.txt is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that the backup file exists */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt~sg00~")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

    /* ---------------------------------------- */
	/* And rename the file */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "foo", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

    /* now create another file in its place */
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 99)  );

	/* Now revert */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	/* verify that a.txt is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that the NEW backup file exists */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt~sg01~")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	SG_context__err_reset(pCtx);
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	return;
}

void u0043_pendingtree_test__revert(SG_context * pCtx, SG_pathname* pPathTopDir, SG_bool bIgnoreBackups)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pathname* pPath_deleteme = NULL;
	SG_pathname* pPath_exe = NULL;
	SG_pathname* pPath_exe2 = NULL;
	SG_pathname* pPath_xattr = NULL;
	SG_pathname* pPath_xattr2 = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvhSubDir = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_bool bExists = SG_FALSE;
	SG_uint64 len_orig = 0;
	SG_uint64 len_now = 0;
#if defined(MAC) || defined(LINUX)
	SG_fsobj_stat st;
#endif
#ifdef SG_BUILD_FLAG_TEST_XATTR
    SG_rbtree* prb = NULL;
    SG_uint32 count = 0;
#endif
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );

	// mkdir @/d1
	// create @/a.txt
	// create @/deleteme
	// create @/exe
	// create @/exe2 with +x
	// create @/xattr
	// create @/xattr with XATTRs
	
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1,       pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_deleteme,  pPathWorkingDir, "deleteme")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_exe,       pPathWorkingDir, "exe")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_exe2,      pPathWorkingDir, "exe2")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_xattr,     pPathWorkingDir, "xattr")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_xattr2,    pPathWorkingDir, "xattr2")  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "deleteme", 71)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "exe", 71)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "exe2", 75)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "xattr", 71)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "xattr2", 80)  );

#if defined(LINUX) || defined(MAC)
    // Make sure exe goes into the repo WITHOUT the X bit set
	// 
	// chmod -x @/exe

    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath_exe, &st)  );
    st.perms &= ~S_IXUSR;
    VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pPath_exe, st.perms)  );

    // Make sure exe2 goes into the repo WITH the X bit set
	// 
	// chmod +x @/exe2

    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath_exe2, &st)  );
    st.perms |= S_IXUSR;
    VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pPath_exe2, st.perms)  );
#endif

#ifdef SG_BUILD_FLAG_TEST_XATTR
	// setxattr @/xattr2 "com.sourcegear.test3=Hello_World"
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pPath_xattr2, "com.sourcegear.test3", 3, (SG_byte*) "Hello_World")  );

    /* verify the xattr is on disk */
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx, pPath_xattr2, NULL, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFY_COND("count", (1 == count));
    SG_RBTREE_NULLFREE(pCtx, prb);
#endif

	//////////////////////////////////////////////////////////////////

	VERIFY_ERR_CHECK(  _ut_pt__new_repo3(pCtx, bufName, pPathWorkingDir, bIgnoreBackups)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	//////////////////////////////////////////////////////////////////

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 7, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d1")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "deleteme")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "exe")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "xattr")  );

	/* Make sure a.txt is in top and not in d1 */
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d1", &pvhSubDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir, 0, __LINE__)  );
	
	SG_VHASH_NULLFREE(pCtx, pvh);

	/* get the original length for comparison later */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_orig, NULL)  );

	//////////////////////////////////////////////////////////////////

	// append to @/a.txt
	// move @/a.txt --> @/d1/foo
	// remove @/deleteme
	// chmod +x @/exe
	// setxattr @/xattr

	/* Now modify the file */
	VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pPathWorkingDir, "a.txt", 4)  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );

	VERIFY_COND("length should be greater now", (len_now > len_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	
	/* And rename the file */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "foo", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* And move it */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPathDir1, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that a.txt does not exist in its original location */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (!bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that foo does not exist in the top dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "foo")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (!bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that foo does exist in d1 */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir1, "foo")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	
	/* The changeset should now have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Remove one file */
	{
		const char* pszName_deleteme = "deleteme";
		
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName_deleteme, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

    /* verify the file is gone */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_deleteme, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (!bExists));

#if defined(MAC) || defined(LINUX)
	// chmod +x @/exe
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath_exe, &st)  );
    st.perms |= S_IXUSR;
    VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pPath_exe, st.perms)  );

	// chmod -x @/exe2
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath_exe2, &st)  );
    st.perms &= ~S_IXUSR;
    VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pPath_exe2, st.perms)  );
#endif

#ifdef SG_BUILD_FLAG_TEST_XATTR
	// setxattr @/xattr "com.sourcegear.test2=Champaign"
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pPath_xattr, "com.sourcegear.test2", 3, (SG_byte*) "Champaign")  );

    /* verify the xattr is on disk */
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx, pPath_xattr, NULL, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFY_COND("count", (1 == count));
    SG_RBTREE_NULLFREE(pCtx, prb);

	// remove XATTR from @/xattr2
	VERIFY_ERR_CHECK(  SG_fsobj__xattr__remove(pCtx, pPath_xattr2, "com.sourcegear.test3")  );
	
    /* verify the xattr is gone */
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx, pPath_xattr2, NULL, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFY_COND("count", (0 == count));
    SG_RBTREE_NULLFREE(pCtx, prb);
#endif

    /* scan, to find the chmod and the xattr */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After entries bunch of changes.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@",                 _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1",              _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/foo",    sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/foo",          _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/deleteme",        _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/exe",             _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/exe2",            _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/xattr",           _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/xattr2",          _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@",                 SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1",              SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F><phantom>@/foo",    SG_DIFFSTATUS_FLAGS__MOVED|SG_DIFFSTATUS_FLAGS__RENAMED|SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/foo",          SG_DIFFSTATUS_FLAGS__MOVED|SG_DIFFSTATUS_FLAGS__RENAMED|SG_DIFFSTATUS_FLAGS__MODIFIED)  );
#if defined(MAC) || defined(LINUX)
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/exe",             SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/exe2",            SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)  );
#endif
#if defined(SG_BUILD_FLAG_TEST_XATTR)
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/xattr",            SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/xattr2",           SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)  );
#endif

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////

	/* Now revert */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	/* The changeset should now be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

	/* verify that a.txt is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that foo does not exist in d1 */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir1, "foo")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (!bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

    /* verify the removed file is back */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_deleteme, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (bExists));

#if defined(MAC) || defined(LINUX)
	// verify -x restored on @/exe
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath_exe, &st)  );
    VERIFY_COND("x", (!(st.perms & S_IXUSR)));

	// verify +x restored on @/exe
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath_exe2, &st)  );
    VERIFY_COND("x", ((st.perms & S_IXUSR)));
#endif

#ifdef SG_BUILD_FLAG_TEST_XATTR
    /* verify the xattr is gone on @/xattr */
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx, pPath_xattr, NULL, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFY_COND("count", (0 == count));
    SG_RBTREE_NULLFREE(pCtx, prb);

	// verify restored xattrs on @/xattr2
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx, pPath_xattr2, NULL, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFY_COND("count", (1 == count));
    SG_RBTREE_NULLFREE(pCtx, prb);
#endif

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After entries bunch of changes.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/d1",              _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/a.txt",           _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/deleteme",        _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/exe",             _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/exe2",            _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/xattr",           _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/xattr2",          _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/d1",              SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/a.txt",           SG_DIFFSTATUS_FLAGS__ZERO)  );
#if defined(MAC) || defined(LINUX)
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/exe",             SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/exe2",            SG_DIFFSTATUS_FLAGS__ZERO)  );
#endif
#if defined(SG_BUILD_FLAG_TEST_XATTR)
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/xattr",            SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/xattr2",           SG_DIFFSTATUS_FLAGS__ZERO)  );
#endif

	if (!bIgnoreBackups)
	{
		// because we backup the modified version of "@/a.txt" before we restore it,
		// we will have a __FOUND "@/a.txt~sg00~" and "@" will be __MODIFIED.
		VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@",                                    _ut_pt__PTNODE_ZERO)  );
		VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/a.txt~sg00~",                        _ut_pt__PTNODE_FOUND)  );
		VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@",                 SG_DIFFSTATUS_FLAGS__MODIFIED)  );
		VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/a.txt~sg00~",     SG_DIFFSTATUS_FLAGS__FOUND)  );
	}
	
	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPath_deleteme);
	SG_PATHNAME_NULLFREE(pCtx, pPath_exe);
	SG_PATHNAME_NULLFREE(pCtx, pPath_exe2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_xattr);
	SG_PATHNAME_NULLFREE(pCtx, pPath_xattr2);
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_VHASH_NULLFREE(pCtx,pvhPending);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
}

void u0043_pendingtree_test__switch_baseline(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufCSet_0[SG_HID_MAX_BUFFER_LENGTH];
	char bufCSet_1[SG_HID_MAX_BUFFER_LENGTH];
	char bufCSet_AfterSwitchTo_0[SG_HID_MAX_BUFFER_LENGTH];
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathDir2 = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pathname* pPath_deleteme = NULL;
	SG_pathname* pPath_exe = NULL;
	SG_pathname* pPath_xattr = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvhSubDir1 = NULL;
	SG_vhash* pvhSubDir2 = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_treediff2 * pTreeDiff_0_1 = NULL;
	SG_treediff2 * pTreeDiff_0_wd = NULL;
	SG_repo* pRepo = NULL;
	SG_bool bExists = SG_FALSE;
	SG_uint64 len_orig = 0;
	SG_uint64 len_now = 0;
	SG_uint32 countChanges_0_1, countChanges_0_wd;
#if defined(MAC) || defined(LINUX)
	SG_fsobj_stat st;
#endif
#ifdef SG_BUILD_FLAG_TEST_XATTR
    SG_rbtree* prb = NULL;
    SG_uint32 count = 0;
#endif
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("switch_baseline working directory", SG_pathname__sz(pPathWorkingDir));

	//////////////////////////////////////////////////////////////////
	// create the repo and populated it with some initial files.
	// commit this to create CSET_0.
	//////////////////////////////////////////////////////////////////

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1,       pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir2,       pPathWorkingDir, "d2")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_deleteme,  pPathWorkingDir, "deleteme")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_exe,       pPathWorkingDir, "exe")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_xattr,     pPathWorkingDir, "xattr")  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir2)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "b.txt", 21)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "c.txt", 22)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "deleteme", 71)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "exe", 71)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "xattr", 71)  );

#if defined(LINUX) || defined(MAC)
    /* Make sure exe goes into the repo WITHOUT the X bit set */
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath_exe, &st)  );
    st.perms &= ~S_IXUSR;
    VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pPath_exe, st.perms)  );
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath_exe, &st)  );
    VERIFYP_COND("initial value of x", (!(st.perms & S_IXUSR)),
			("current value of mode bits [%o]",st.perms));
#endif

	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);
	
	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 8, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d1")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d2")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "b.txt")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "c.txt")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "deleteme")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "exe")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "xattr")  );

	/* Make sure files are in top and not in d1 nor d2 */
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d1", &pvhSubDir1)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir1, 0, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d2", &pvhSubDir2)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir2, 0, __LINE__)  );
	
	SG_VHASH_NULLFREE(pCtx, pvh);

	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_0,sizeof(bufCSet_0))  );	// get HID of CSET_0
	INFOP("switch_baseline",("CSET_0 is %s",bufCSet_0));

	// we now have CSET_0
	//////////////////////////////////////////////////////////////////
	// modify the tree in known ways and commit it.  this will be CSET_1.

	/* get the original length of a.txt for comparison later */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_orig, NULL)  );

	/* Now modify the file a.txt */
	VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pPathWorkingDir, "a.txt", 4)  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );

	VERIFY_COND("length should be greater now", (len_now > len_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	
	/* Rename the file b.txt --> bR.txt */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "b.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "bR.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that b.txt does not exist with its original name */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "b.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (!bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* Move the file @/c.txt --> @/d1/c.txt */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "c.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPathDir1, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that c.txt does not exist in the top dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "c.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (!bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that @/d1/c.txt does exist in d1 */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir1, "c.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	
	/* The changeset should now have something in it */
	VERIFY_WD_JSON_PENDING_DIRTY(pCtx, pPathWorkingDir);

	/* Remove one file */
	{
		const char* pszName_deleteme = "deleteme";
		
		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName_deleteme, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

    /* verify the file is gone */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_deleteme, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (!bExists));

	/* add a new file. */
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "d.txt", 23)  );

#if defined(MAC) || defined(LINUX)
    /* chmod+x one file */
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath_exe, &st)  );
    st.perms |= S_IXUSR;
    VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pPath_exe, st.perms)  );
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath_exe, &st)  );
    VERIFYP_COND("+x not set after chmod", ((st.perms & S_IXUSR)),
			("current value of mode bits [%o]",st.perms));
#endif

#ifdef SG_BUILD_FLAG_TEST_XATTR
    /* add one xattr */
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pPath_xattr, "com.sourcegear.test2", 3, (SG_byte*) "Champaign")  );

    /* verify the xattr is gone */
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx, pPath_xattr, NULL, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFYP_COND("xattr count after setxattr", (1 == count),
		   ("count [%d]",count));
	SG_RBTREE_NULLFREE(pCtx, prb);
#endif

    /* scan, to find the chmod and the xattr */
	VERIFY_ERR_CHECK(  _ut_pt__scan(pCtx, pPathWorkingDir)  );

	/* commit everything and make sure all is right. */
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  _ut_pt__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvh, 7, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d1")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_dir(pCtx, pvh, "d2")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "a.txt")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "bR.txt")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "d.txt")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "exe")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvh, "xattr")  );

	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d1", &pvhSubDir1)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir1, 1, __LINE__)  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_has_file(pCtx, pvhSubDir1, "c.txt")  );
	VERIFY_ERR_CHECK(  _ut_pt__verifytree__dir_get_dir(pCtx, pvh, "d2", &pvhSubDir2)  );
	VERIFY_ERR_CHECK(  _ut_pt__verify_vhash_count(pCtx, pvhSubDir2, 0, __LINE__)  );
	
	SG_VHASH_NULLFREE(pCtx, pvh);

	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_1,sizeof(bufCSet_1))  );	// get HID of CSET_1
	INFOP("switch_baseline",("CSET_1 is %s",bufCSet_1));

	// we now have CSET_1
	//////////////////////////////////////////////////////////////////

	// dump tree-diff of CSET_0 vs CSET_1.  this is for debug info only.  it
	// should reflect everything that we just did during our second commit.

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );
	VERIFY_ERR_CHECK(  SG_treediff2__alloc(pCtx,pRepo,&pTreeDiff_0_1)  );
	VERIFY_ERR_CHECK(  SG_treediff2__compare_cset_vs_cset(pCtx,pTreeDiff_0_1,bufCSet_0,bufCSet_1)  );
	VERIFY_ERR_CHECK(  SG_treediff2__count_changes(pCtx,pTreeDiff_0_1,&countChanges_0_1)  );
	INFOP("switch_baseline",("TreeDiff CSet_0 vs CSet_1 is: [nr changes %d]",countChanges_0_1));
	SG_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff_0_1,SG_FALSE)  );
	SG_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff_0_1,SG_TRUE)  );

	//////////////////////////////////////////////////////////////////
	// now do a "switch-baseline" to make working-directory look like CSET_0.

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__update_baseline(pCtx, pPendingTree,bufCSet_0, SG_FALSE,
													   SG_PT_ACTION__DO_IT)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	//////////////////////////////////////////////////////////////////
	// verify that WD now thinks that CSET_0 is the baseline and
	// that the WD is clean w/r/t the baseline.

	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_AfterSwitchTo_0,sizeof(bufCSet_AfterSwitchTo_0))  );
	INFOP("switch_baseline",("Baseline after switch to CSet_0 is %s",bufCSet_AfterSwitchTo_0));
	VERIFY_COND("baseline value after switch to 0", (strcmp(bufCSet_0,bufCSet_AfterSwitchTo_0) == 0));

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx,pPendingTree,NULL,&pTreeDiff_0_wd)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	VERIFY_ERR_CHECK(  SG_treediff2__count_changes(pCtx,pTreeDiff_0_wd,&countChanges_0_wd)  );
	VERIFY_COND("treediff should be empty after switch-baseline",(countChanges_0_wd == 0));
	INFOP("switch_baseline",("TreeDiff CSet_0 vs WD (after switch-back) is: [nr changes %d]",countChanges_0_wd));
	SG_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff_0_wd,SG_FALSE)  );
	SG_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff_0_wd,SG_TRUE)  );

	//////////////////////////////////////////////////////////////////

	/* The pendingtree should now be empty because the WD should be exactly what it was in CSET_0. */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* verify that a.txt is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that the rename of b.txt was handled.  that is, that we now have a b.txt and not a bR.txt. */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "b.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("@/b.txt not found when should be", (bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "bR.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("@/bR.txt found when shouldn't be", (!bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that c.txt is in top-level and not d1 */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "c.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir1, "c.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (!bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify the added file d.txt is now gone. */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "d.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("not here", (!bExists));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	
    /* verify the removed file is back */
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_deleteme, &bExists, NULL, NULL)  );
    VERIFY_COND("b", (bExists));

#if defined(MAC) || defined(LINUX)
    /* verify the chmod +x is back to -x */
    VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath_exe, &st)  );
    VERIFYP_COND("x after switch-baseline", (!(st.perms & S_IXUSR)),
			("current value of mode bits [%o]",st.perms));
#endif

#ifdef SG_BUILD_FLAG_TEST_XATTR
    /* verify the xattr is gone */
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx, pPath_xattr, NULL, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFYP_COND("xattr count after switch-baseline", (0 == count),
			("current count of xattrs [%d]",count));
    SG_RBTREE_NULLFREE(pCtx, prb);
#endif

	//////////////////////////////////////////////////////////////////
	// TODO switch back to CSet_1 and verify that everything looks right again


fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPath_deleteme);
	SG_PATHNAME_NULLFREE(pCtx, pPath_exe);
	SG_PATHNAME_NULLFREE(pCtx, pPath_xattr);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_0_1);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_0_wd);

	return;
}

//////////////////////////////////////////////////////////////////

struct _u0043_pendingtree_text__ctm__data
{
	SG_int32				sum;
	const char *			szLabel;
};

static SG_treediff2_foreach_callback u0043_pendingtree_text__ctm_cb;

static void u0043_pendingtree_text__ctm_cb(SG_context * pCtx,
										   SG_UNUSED_PARAM(SG_treediff2 * pTreeDiff),
										   SG_UNUSED_PARAM(const char * szGidObject),
										   const SG_treediff2_ObjectData * pOD_opaque,
										   void * pVoidCallerData)
{
	struct _u0043_pendingtree_text__ctm__data * pData = (struct _u0043_pendingtree_text__ctm__data *)pVoidCallerData;
	const char * szRepoPath;
	SG_diffstatus_flags dsFlags;

	SG_UNUSED(pTreeDiff);
	SG_UNUSED(szGidObject);

	pData->sum++;

	VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_repo_path(pCtx, pOD_opaque,&szRepoPath)  );
	VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD_opaque,&dsFlags)  );
	
	VERIFYP_COND(pData->szLabel,(0),("Entry [repo-path %s][dsFlags %x] not accounted for in test.",szRepoPath,dsFlags));

	return;

fail:
	return;
}

/**
 * this is used after we have checked the treediff answers against the expected set of answers in XA or YA.
 * each time an answer is checked, the entry is marked with a 1.  after we have checked them all, we look
 * to see if there are any entries with marker value 0 remaining.
 *
 * Anything that we find implied that the treediff included something that we didn't expect (or that our
 * answer sheet is incomplete).
 */
static void u0043_pendingtree_text__check_treediff_markers(SG_context * pCtx,
														   SG_treediff2 * pTreeDiff, const char * szLabel)
{
	struct _u0043_pendingtree_text__ctm__data data;

	data.sum = 0;
	data.szLabel = szLabel;

	SG_treediff2__foreach_with_marker_value(pCtx, pTreeDiff,0,u0043_pendingtree_text__ctm_cb,&data);
}

void u0043_pendingtree_test__arbitrary_cset_vs_wd(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	// create a series of changesets and a pending tree with changes
	// and test the treediff-status and pendingtree-status joining
	// that produces an arbitrary-cset-vs-wd status.

	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	char bufCSet_0[SG_HID_MAX_BUFFER_LENGTH];
	char bufCSet_1[SG_HID_MAX_BUFFER_LENGTH];
	char bufCSet_2[SG_HID_MAX_BUFFER_LENGTH];
	char bufCSet_3[SG_HID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathTemp = NULL;
	SG_string * pStrEntryName = NULL;
	SG_repo* pRepo = NULL;
	SG_vhash * pvhManifest_1 = NULL;
	SG_vhash * pvhManifest_2 = NULL;
	SG_vhash * pvhManifest_3 = NULL;
	SG_pendingtree * pPendingTree = NULL;
	SG_uint32 kRow;
	SG_string * pStringStatus01 = NULL;
	SG_string * pStringStatus12 = NULL;
	SG_string * pStringStatus13 = NULL;
	SG_string * pStringStatus1wd = NULL;
	SG_treediff2 * pTreeDiff_join_before_commit = NULL;
	SG_treediff2 * pTreeDiff_join_after_commit = NULL;
	SG_treediff2 * pTreeDiff_check = NULL;
	SG_string * pStr_check = NULL;
	const SG_treediff2_ObjectData * pOD_opaque_check;
	const char * szGidObject_check;
	SG_diffstatus_flags dsFlags_check;
	SG_int64 attrbits_check;
	const char * szHidXAttrs_check;
	const char * szOldName_check;
	const char * szOldParentRepoPath_check;
	SG_bool bFound;

	// only bother with xattr and attrbits tests on Mac and Linux.
	//
	// TODO split this "AA" into 2 flags -- one for XATTRs and one for ATTRBITS.
	// TODO and allow the attrbits tests on mac and linux.
	// TODO the xattr tests should still be controlled by the build flag.
#ifdef SG_BUILD_FLAG_TEST_XATTR
#	define AA 1
#else
#	define AA 0
#endif

	//////////////////////////////////////////////////////////////////
	// aRowX[] -- data used to create <cset1> from the initial commit <cset0>

	struct _row_x
	{
		const char *			szDir;
		const char *			szFile;
		
		SG_pathname *			pPathDir;
	};

#define X(d,f)	{ d, f, NULL }

	struct _row_x aRowX[] = {
		X(	"d0000",		"f0000"	),	// add <wd>/d0000/f0000

		X(	"d1000",		"f1000"	),
		X(	"d1001",		"f1001"	),
		X(	"d1002",		"f1002"	),
		X(	"d1003",		"f1003"	),
		X(	"d1004",		"f1004"	),
		X(	"d1005",		"f1005"	),
		X(	"d1006",		"f1006"	),
		X(	"d1007",		"f1007"	),
		X(	"d1008",		"f1008"	),
		X(	"d1009",		"f1009"	),
		X(	"d1010",		"f1010"	),
		X(	"d1011",		"f1011"	),
		X(	"d1012",		"f1012"	),
		X(	"d1013",		"f1013"	),
		X(	"d1014",		"f1014"	),
		X(	"d1015",		"f1015"	),
		X(	"d1016",		"f1016"	),
		X(	"d1017",		"f1017"	),
		X(	"d1018",		"f1018"	),
		X(	"d1019",		"f1019"	),
		X(	"d1020",		"f1020"	),

		X(	"d1030",				NULL	),
		X(	"d1030/aa",				NULL	),
		X(	"d1030/aa/bb",			NULL	),
		X(	"d1030/aa/bb/cc",		NULL	),
		X(	"d1030/aa/bb/cc/dd",	NULL	),
		X(	"d1030/aa/bb/cc/dd/ee",	"f1030"	),

		X(	"d1031",				NULL	),
		X(	"d1031/aa",				NULL	),
		X(	"d1031/aa_mov",			NULL	),
		X(	"d1031/aa/bb",			NULL	),
		X(	"d1031/aa/bb/cc",		NULL	),
		X(	"d1031/aa/bb/cc_mov",	NULL	),
		X(	"d1031/aa/bb/cc/dd",	NULL	),
		X(	"d1031/aa/bb/cc/dd/ee",	"f1031"	),

		X(	"d1032",				NULL	),
		X(	"d1032/aa",				NULL	),
		X(	"d1032/aa_mov",			NULL	),
		X(	"d1032/aa/bb",			NULL	),
		X(	"d1032/aa/bb/cc",		NULL	),
		X(	"d1032/aa/bb/cc_mov",	NULL	),
		X(	"d1032/aa/bb/cc/dd",	NULL	),
		X(	"d1032/aa/bb/cc/dd/ee",	"f1032"	),

		X(	"d1033",				NULL	),
		X(	"d1033/aa",				NULL	),
		X(	"d1033/aa_mov",			NULL	),
		X(	"d1033/aa/bb",			NULL	),
		X(	"d1033/aa/bb/cc",		NULL	),
		X(	"d1033/aa/bb/cc_mov",	NULL	),
		X(	"d1033/aa/bb/cc/dd",	NULL	),
		X(	"d1033/aa/bb/cc/dd/ee",	"f1033"	),

		X(	"d2000",		"f2000"	),
		X(	"d2001",		"f2001"	),
		X(	"d2002",		"f2002"	),
		X(	"d2003",		"f2003"	),
		X(	"d2004",		"f2004"	),
		X(	"d2005",		"f2005"	),
		X(	"d2006",		"f2006"	),
		X(	"d2007",		"f2007"	),
		X(	"d2008",		"f2008"	),
		X(	"d2009",		"f2009"	),
		X(	"d2010",		"f2010"	),
		X(	"d2011",		"f2011"	),
		X(	"d2012",		"f2012"	),
		X(	"d2013",		"f2013"	),
		X(	"d2014",		"f2014"	),
		X(	"d2015",		"f2015"	),
		X(	"d2016",		"f2016"	),
		X(	"d2017",		"f2017"	),
		X(	"d2018",		"f2018"	),
		X(	"d2019",		"f2019"	),
		X(	"d2020",		"f2020"	),

		X(	"d3000",		"f3000"	),
		X(	"d3001",		"f3001"	),
		X(	"d3002",		"f3002"	),
		X(	"d3003",		"f3003"	),
		X(	"d3004",		"f3004"	),
		X(	"d3005",		"f3005"	),
		X(	"d3006",		"f3006"	),
		X(	"d3007",		"f3007"	),
		X(	"d3008",		"f3008"	),
		X(	"d3009",		"f3009"	),

		X(	"d3000Mov",		NULL	),
		X(	"d3000Mov2",	NULL	),
		X(	"d3001Mov",		NULL	),
		X(	"d3002Mov",		NULL	),
		X(	"d3003Mov",		NULL	),
		X(	"d3004Mov",		NULL	),
		X(	"d3005Mov",		NULL	),
		X(	"d3006Mov",		NULL	),
		X(	"d3007Mov",		NULL	),
		X(	"d3008Mov",		NULL	),
		X(	"d3009Mov",		NULL	),

#if AA
		X(	"a4000",		"f4000"	),
		X(	"a4001",		"f4001"	),
		X(	"a4002",		"f4002"	),
		X(	"a4003",		"f4003"	),
		X(	"a4004",		"f4004"	),
		X(	"a4005",		"f4005"	),
		X(	"a4006",		"f4006"	),
		X(	"a4007",		"f4007"	),
		X(	"a4008",		"f4008"	),
		X(	"a4009",		"f4009"	),

		X(	"a4010",		"f4010"	),
		X(	"a4011",		"f4011"	),
		X(	"a4012",		"f4012"	),
		X(	"a4013",		"f4013"	),
		X(	"a4014",		"f4014"	),
		X(	"a4015",		"f4015"	),
		X(	"a4016",		"f4016"	),
		X(	"a4016Mov",		NULL	),
		X(	"a4017",		"f4017"	),
		X(	"a4018",		"f4018"	),
		X(	"a4019",		"f4019"	),

		X(	"a5000",		"f5000"	),
		X(	"a5001",		"f5001"	),
		X(	"a5002",		"f5002"	),
		X(	"a5003",		"f5003"	),
		X(	"a5004",		"f5004"	),
		X(	"a5005",		"f5005"	),
		X(	"a5006",		"f5006"	),
		X(	"a5007",		"f5007"	),
		X(	"a5008",		"f5008"	),
		X(	"a5009",		"f5009"	),

		X(	"a5005Mov",		NULL	),
		X(	"a5006Mov",		NULL	),
		X(	"a5007Mov",		NULL	),
		X(	"a5008Mov",		NULL	),
		X(	"a5009Mov",		NULL	),

		X(	"a5010",		"f5010"	),
		X(	"a5011",		"f5011"	),
		X(	"a5012",		"f5012"	),
		X(	"a5013",		"f5013"	),
		X(	"a5014",		"f5014"	),

		X(	"a5010Mov",		NULL	),
		X(	"a5011Mov",		NULL	),
		X(	"a5012Mov",		NULL	),
		X(	"a5013Mov",		NULL	),
		X(	"a5014Mov",		NULL	),
#endif

		X(	"d6000",		"f6000"	),
		X(	"d6001",		"f6001"	),
		X(	"d6002",		"f6002"	),
		X(	"d6003",		"f6003"	),
		X(	"d6004",		"f6004"	),

		X(	"d6000Mov",		NULL	),
		X(	"d6001Mov",		NULL	),
		X(	"d6002Mov",		NULL	),
		X(	"d6003Mov",		NULL	),
		X(	"d6004Mov",		NULL	),

#if AA
		X(	"a7000",		"f7000"	),
		X(	"a7001",		"f7001"	),
		X(	"a7002",		"f7002"	),
		X(	"a7003",		"f7003"	),
		X(	"a7004",		"f7004"	),
		X(	"a7005",		"f7005"	),
		X(	"a7006",		"f7006"	),
		X(	"a7007",		"f7007"	),
		X(	"a7008",		"f7008"	),
		X(	"a7009",		"f7009"	),

		X(	"a7005Mov",		NULL	),
		X(	"a7006Mov",		NULL	),
		X(	"a7007Mov",		NULL	),
		X(	"a7008Mov",		NULL	),
		X(	"a7009Mov",		NULL	),

		X(	"a7010",		"f7010"	),
		X(	"a7011",		"f7011"	),
		X(	"a7012",		"f7012"	),
		X(	"a7013",		"f7013"	),
		X(	"a7014",		"f7014"	),

		X(	"a7010Mov",		NULL	),
		X(	"a7011Mov",		NULL	),
		X(	"a7012Mov",		NULL	),
		X(	"a7013Mov",		NULL	),
		X(	"a7014Mov",		NULL	),
#endif

		X(	"d8000",		"f8000"	),
		X(	"d8001",		"f8001"	),
		X(	"d8002",		"f8002"	),
		X(	"d8003",		"f8003"	),
		X(	"d8004",		"f8004"	),

		X(	"d8000Mov",		NULL	),
		X(	"d8001Mov",		NULL	),
		X(	"d8002Mov",		NULL	),
		X(	"d8003Mov",		NULL	),
		X(	"d8004Mov",		NULL	),

		X(	"d9000",		"f9000"	),
		X(	"d9001",		"f9001"	),
		X(	"d9002",		"f9002"	),
		X(	"d9003",		"f9003"	),
		X(	"d9003Mov",		NULL	),
		X(	"d9004",		"f9004"	),
		X(	"d9004Mov",		NULL	),
		X(	"d9005",		"f9005"	),
		X(	"d9006",		"f9006"	),
		X(	"d9007",		"f9007"	),
		X(	"d9007Mov",		NULL	),
		X(	"d9008",		"f9008"	),
		X(	"d9008Mov",		NULL	),
		X(	"d9009",		"f9009"	),
#if AA
		X(	"a9010",		"f9010"	),
		X(	"a9011",		"f9011"	),
		X(	"a9012",		"f9012"	),
		X(	"a9013",		"f9013"	),
		X(	"a9014",		"f9014"	),
		X(	"a9014Mov",		NULL	),
		X(	"a9015",		"f9015"	),
		X(	"a9015Mov",		NULL	),
#endif

	};

#undef X

	SG_uint32 nrRowsX = SG_NrElements(aRowX);

	//////////////////////////////////////////////////////////////////
	// aRowY[] -- data used to create <cset2> from <cset1>

	struct _row_y
	{
		const char *			szDir1;
		const char *			szDir2;
		const char *			szFile1;
		const char *			szFile2;
		SG_diffstatus_flags		dsFlags;

		SG_pathname *			pPathDir;
		SG_pathname *			pPathFile;
		const char *			szLastDir;
		const char *			szLastFile;
	};

#define Y(d1,d2,f1,f2,ds)	{ d1, d2, f1, f2, ds, NULL, NULL, d1, f1 }

	struct _row_y aRowY[] = {

		// ME_MASK in isolation (this will be [ADM]'s in some of the comments in sg_status.c)
		// we don't bother with Lost/Found records here because we are going to commit these
		// changes and L/F records don't appear in a treediff-type status.
		
		Y(	"d0000",	NULL,		"f0000",	NULL,		SG_DIFFSTATUS_FLAGS__DELETED	),	// del <wd>/d0000/f0000
		Y(	"d0000",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__DELETED	),	// del <wd>/d0000

		Y(	"dAdd1",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),	// add <wd>/dAdd1
		Y(	"dAdd2",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd3",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd4",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd5",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd6",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd7",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd8",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd9",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),

		Y(	"dAdd10",	NULL,		"fAdd10",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),	// add <wd>/dAdd10/fAdd10 (implicitly adds <wd>/dAdd10 too)
		Y(	"dAdd11",	NULL,		"fAdd11",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd12",	NULL,		"fAdd12",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd13",	NULL,		"fAdd13",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd13_M",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd14",	NULL,		"fAdd14",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd14_M",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd15",	NULL,		"fAdd15",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd15_M",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd16",	NULL,		"fAdd16",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd17",	NULL,		"fAdd17",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd18",	NULL,		"fAdd18",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd19",	NULL,		"fAdd19",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),

#if AA
		Y(	"aAdd20",	NULL,		"fAdd20",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),	// add <wd>/dAdd20/fAdd20 (implicitly adds <wd>/dAdd20 too) with XATTRS(+test1)
		Y(	"aAdd21",	NULL,		"fAdd21",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"aAdd22",	NULL,		"fAdd22",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"aAdd23",	NULL,		"fAdd23",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"aAdd24",	NULL,		"fAdd24",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),

		Y(	"aAdd25",	NULL,		"fAdd25",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// add <wd>/dAdd25/fAdd25 with chmod(+x)
		Y(	"aAdd26",	NULL,		"fAdd26",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"aAdd27",	NULL,		"fAdd27",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"aAdd28",	NULL,		"fAdd28",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"aAdd29",	NULL,		"fAdd29",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),

		Y(	"aAdd30",	NULL,		"fAdd30",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),	// add <wd>/dAdd30/fAdd30 (implicitly adds <wd>/dAdd30 too) with XATTRS and ATTRBITS
		Y(	"aAdd31",	NULL,		"fAdd31",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"aAdd32",	NULL,		"fAdd32",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"aAdd33",	NULL,		"fAdd33",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"aAdd34",	NULL,		"fAdd34",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),

#endif

		Y(	"d2000",	NULL,		"f2000",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),	// mod <wd>/d2000/f2000
		Y(	"d2001",	NULL,		"f2001",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2002",	NULL,		"f2002",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2003",	NULL,		"f2003",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2004",	NULL,		"f2004",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2004_M",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED	),
		Y(	"d2005",	NULL,		"f2005",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2006",	NULL,		"f2006",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2007",	NULL,		"f2007",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2008",	NULL,		"f2008",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2009",	NULL,		"f2009",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2010",	NULL,		"f2010",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2011",	NULL,		"f2011",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2012",	NULL,		"f2012",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2013",	NULL,		"f2013",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2014",	NULL,		"f2014",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2015",	NULL,		"f2015",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2016",	NULL,		"f2016",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2017",	NULL,		"f2017",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2018",	NULL,		"f2018",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2019",	NULL,		"f2019",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2020",	NULL,		"f2020",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),

		// MODIFIERS_MASK in isolation (these will be know as Z's in some of the comments in sg_status.c)

		Y(	"d1000",	"d1000Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1000 --> <wd>/d1000Ren
		Y(	"d1001",	"d1001Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1002",	"d1002Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1003",	"d1003Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1004",	"d1004Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),

		Y(	"d1005",	NULL,		"f1005",	"f1005Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1005/f1005 --> <wd>/d1005/f1005Ren
		Y(	"d1006",	NULL,		"f1006",	"f1006Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1007",	NULL,		"f1007",	"f1007Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1008",	NULL,		"f1008",	"f1008Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1009",	NULL,		"f1009",	"f1009Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),

		Y(	"d1010",	NULL,		"f1010",	"f1010Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1010/f1010 --> <wd>/d1010/f1010Ren and
		Y(	"d1010",	"d1010Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1010 --> <wd>/d1010Ren YIELDING --> <wd>/d1010Ren/f1010Ren
		Y(	"d1011",	NULL,		"f1011",	"f1011Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1011",	"d1011Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1012",	NULL,		"f1012",	"f1012Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1012",	"d1012Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1013",	NULL,		"f1013",	"f1013Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1013",	"d1013Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1014",	NULL,		"f1014",	"f1014Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1014",	"d1014Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),

		Y(	"d1015",	"d1015Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1015 --> <wd>/d1015Ren and
		Y(	"d1015Ren",	NULL,		"f1015",	"f1015Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1015Ren/f1015 --> <wd>/d1015Ren/f1015Ren
		Y(	"d1016",	"d1016Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1016Ren",	NULL,		"f1016",	"f1016Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1017",	"d1017Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1017Ren",	NULL,		"f1017",	"f1017Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1018",	"d1018Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1018Ren",	NULL,		"f1018",	"f1018Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1019",	"d1019Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1019Ren",	NULL,		"f1019",	"f1019Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1020",	"d1020Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1020Ren",	NULL,		"f1020",	"f1020Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),

		Y(	"d1030/aa",					"aa_ren",		NULL,	NULL,	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1030/aa --> <wd>/d1030/aa_ren
		Y(	"d1030/aa_ren/bb",			"bb_ren",		NULL,	NULL,	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1030/aa_ren/bb --> <wd>/d1030/aa_ren/bb_ren
		Y(	"d1030/aa_ren/bb_ren/cc",	"cc_ren",		NULL,	NULL,	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1030/aa_ren/bb_ren/cc --> <wd>/d1030/aa_ren/bb_ren/cc_ren

		Y(	"d1031/aa",						"d1031/aa_mov",						NULL,	NULL,	SG_DIFFSTATUS_FLAGS__MOVED		),	// mov <wd>/d1031/aa                   --> <wd>/d1031/aa_mov/aa
		Y(	"d1031/aa_mov/aa/bb",			"bb_ren",							NULL,	NULL,	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1031/aa_mov/aa/bb         --> <wd>/d1031/aa_mov/aa/bb_ren
		Y(	"d1031/aa_mov/aa/bb_ren/cc",	"d1031/aa_mov/aa/bb_ren/cc_mov",	NULL,	NULL,	SG_DIFFSTATUS_FLAGS__MOVED		),	// mov <wd>/d1031/aa_mov/aa/bb_ren/cc  --> <wd>/d1031/aa_mov/aa/bb_ren/cc_mov/cc

		Y(	"d1032/aa",						"d1032/aa_mov",							NULL,	NULL,	SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d1032/aa_mov/aa/bb",			"bb_ren",								NULL,	NULL,	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1032/aa_mov/aa/bb_ren/cc",	"d1032/aa_mov/aa/bb_ren/cc_mov",		NULL,	NULL,	SG_DIFFSTATUS_FLAGS__MOVED		),

		Y(	"d1033/aa/bb/cc/dd/ee",			NULL,									"f1033",NULL,	SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d1033/aa",						"d1033/aa_mov",							NULL,	NULL,	SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d1033/aa_mov/aa/bb",			"bb_ren",								NULL,	NULL,	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1033/aa_mov/aa/bb_ren/cc",	"d1033/aa_mov/aa/bb_ren/cc_mov",		NULL,	NULL,	SG_DIFFSTATUS_FLAGS__MOVED		),

		Y(	"d3000",	"d3000Mov",	"f3000",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),	// mov <wd>/d3000/f3000 --> <wd>/d3000Mov/f3000
		Y(	"d3001",	"d3001Mov",	"f3001",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3002",	"d3002Mov",	"f3002",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3003",	"d3003Mov",	"f3003",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3004",	"d3004Mov",	"f3004",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3005",	"d3005Mov",	"f3005",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3006",	"d3006Mov",	"f3006",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3007",	"d3007Mov",	"f3007",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3008",	"d3008Mov",	"f3008",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3009",	"d3009Mov",	"f3009",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),

		// i'm going to skip directory moves so that i don't have to deal with nested vhashes.

#if AA
		Y(	"a4000",	NULL,		"f4000",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),	// do xattr on <wd>/d4000/f4000
		Y(	"a4001",	NULL,		"f4001",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),
		Y(	"a4002",	NULL,		"f4002",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),
		Y(	"a4003",	NULL,		"f4003",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),
		Y(	"a4004",	NULL,		"f4004",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),

		Y(	"a4005",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),	// do xattr on <wd>/d4005
		Y(	"a4006",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),
		Y(	"a4007",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),
		Y(	"a4008",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),
		Y(	"a4009",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),

		Y(	"a4010",	NULL,		"f4010",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS	),	// do attrbits on <wd>/d4010/f4010
		Y(	"a4011",	NULL,		"f4011",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS	),
		Y(	"a4012",	NULL,		"f4012",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS	),
		Y(	"a4013",	NULL,		"f4013",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS	),
		Y(	"a4014",	NULL,		"f4014",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS	),
		// since we currently only support chmod +x or -x, i'm going to skip testing directory.

		// combination of xattr and attrbits (these will also be Z's)

		Y(	"a4015",	NULL,		"f4015",		NULL,	(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),	// do xattr and attrbits on <wd>/d4015/f4015
		Y(	"a4016",	NULL,		"f4016",		NULL,	(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"a4017",	NULL,		"f4017",		NULL,	(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"a4018",	NULL,		"f4018",		NULL,	(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"a4019",	NULL,		"f4019",		NULL,	(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),

		// combine xattr and attrbit changes with rename and move

		Y(	"a5000",	NULL,		"f5000",	"f5000Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// change xattr &/| attrbits and rename <wd>/d5000/f5000 --> <wd>/d5000/f5000Ren
		Y(	"a5001",	NULL,		"f5001",	"f5001Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5002",	NULL,		"f5002",	"f5002Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5003",	NULL,		"f5003",	"f5003Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5004",	NULL,		"f5004",	"f5004Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)		),

		Y(	"a5005",	"a5005Mov",	"f5005",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// change xattr &/| attrbits and move <wd>/d5005/f5005 --> <wd>/d5005Mov/f5005
		Y(	"a5006",	"a5006Mov",	"f5006",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5007",	"a5007Mov",	"f5007",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5008",	"a5008Mov",	"f5008",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5009",	"a5009Mov",	"f5009",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)		),

		Y(	"a5010",	"a5010Mov",	"f5010",	"f5010Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// change xattr &/| attrbits and rename and move <wd>/d5010/f5010 --> <wd>/d5010Mov/f5010Ren
		Y(	"a5011",	"a5011Mov",	"f5011",	"f5011Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5012",	"a5012Mov",	"f5012",	"f5012Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5013",	"a5013Mov",	"f5013",	"f5013Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5014",	"a5014Mov",	"f5014",	"f5014Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
#endif

		// combine rename and move (without attrbits or xattrs) for testing on windows

		Y(	"d6000",	"d6000Mov",	"f6000",	"f6000Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),	// rename and move <wd>/d6000/f6000 --> <wd>/d6000Mov/f6000Ren
		Y(	"d6001",	"d6001Mov",	"f6001",	"f6001Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),
		Y(	"d6002",	"d6002Mov",	"f6002",	"f6002Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),
		Y(	"d6003",	"d6003Mov",	"f6003",	"f6003Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),
		Y(	"d6004",	"d6004Mov",	"f6004",	"f6004Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),

		// ME_ and MOD_ MASK combined.  (only M's are really possible because A's would just
		// have the correct initial values and D's won't be present when the join happens later.)

#if AA
		Y(	"a7000",	NULL,		"f7000",	"f7000Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// modify, change xattr &/| attrbits and rename <wd>/d7000/f7000 --> <wd>/d7000/f7000Ren
		Y(	"a7001",	NULL,		"f7001",	"f7001Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7002",	NULL,		"f7002",	"f7002Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7003",	NULL,		"f7003",	"f7003Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7004",	NULL,		"f7004",	"f7004Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)		),

		Y(	"a7005",	"a7005Mov",	"f7005",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// modify, change xattr &/| attrbits and move <wd>/d7005/f7005 --> <wd>/d7005Mov/f7005
		Y(	"a7006",	"a7006Mov",	"f7006",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7007",	"a7007Mov",	"f7007",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7008",	"a7008Mov",	"f7008",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7009",	"a7009Mov",	"f7009",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)		),

		Y(	"a7010",	"a7010Mov",	"f7010",	"f7010Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// modify, change xattr &/| attrbits and rename and move <wd>/d7010/f7010 --> <wd>/d7010Mov/f7010Ren
		Y(	"a7011",	"a7011Mov",	"f7011",	"f7011Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7012",	"a7012Mov",	"f7012",	"f7012Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7013",	"a7013Mov",	"f7013",	"f7013Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7014",	"a7014Mov",	"f7014",	"f7014Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
#endif

		// combine modify, rename and move (without attrbits or xattrs) for testing on windows

		Y(	"d8000",	"d8000Mov",	"f8000",	"f8000Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),	// rename and move <wd>/d8000/f8000 --> <wd>/d8000Mov/f8000Ren
		Y(	"d8001",	"d8001Mov",	"f8001",	"f8001Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),
		Y(	"d8002",	"d8002Mov",	"f8002",	"f8002Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),
		Y(	"d8003",	"d8003Mov",	"f8003",	"f8003Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),
		Y(	"d8004",	"d8004Mov",	"f8004",	"f8004Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),

		// the remaining members in aRowX will be effectively Z's w/o MODIFIERS.  that is,
		// they won't appear in the treediff status at all.
	};

#undef Y

	SG_uint32 nrRowsY = SG_NrElements(aRowY);


	struct _row_y_answer
	{
		const char *			szRepoPath;
		const char *			szOldName_when_rename;
		const char *			szOldParentRepoPath_when_moved;
		SG_diffstatus_flags		dsFlags;
		SG_bool					bAttrBitsNonZero;
		SG_bool					bXAttrsNonZero;
	};

#define YA(rp,nm,prp,ds,bA,bX)	{ rp, nm, prp, ds, bA, bX }
#define YA_ADD(rp)				{ rp, NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_FALSE }
#define YA_DEL(rp)				{ rp, NULL, NULL, SG_DIFFSTATUS_FLAGS__DELETED, SG_FALSE, SG_FALSE }
#define YA_MOD(rp)				{ rp, NULL, NULL, SG_DIFFSTATUS_FLAGS__MODIFIED, SG_FALSE, SG_FALSE }
#define YA_REN(rp,nm)			{ rp, nm, NULL, SG_DIFFSTATUS_FLAGS__RENAMED, SG_FALSE, SG_FALSE }
#define YA_MOV(rp,prp)			{ rp, NULL, prp, SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE }

#define YA_MOD_REN(rp,nm)		{ rp, nm, NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED, SG_FALSE, SG_FALSE }
#define YA_MOD_MOV(rp,prp)		{ rp, NULL, prp, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE }

	struct _row_y_answer aRowYA[] = {

		// these rows correspond to the effects produced by the aRowY table above.
		// they are not necessarily line-for-line because some lines in aRowY may
		// do more than one thing (such as recursive delete) or many lines may act
		// on one object (such as sequence of moves and renames on one entry).
		//
		// these are for the cset1-vs-pendingtree (before cset2 is created).

		YA_MOD( "@/" ),

		YA_DEL( "@/d0000/f0000" ),
		YA_DEL( "@/d0000/" ),

		YA_ADD( "@/dAdd1/" ),
		YA_ADD( "@/dAdd2/" ),
		YA_ADD( "@/dAdd3/" ),
		YA_ADD( "@/dAdd4/" ),
		YA_ADD( "@/dAdd5/" ),
		YA_ADD( "@/dAdd6/" ),
		YA_ADD( "@/dAdd7/" ),
		YA_ADD( "@/dAdd8/" ),
		YA_ADD( "@/dAdd9/" ),

		YA_ADD( "@/dAdd10/" ),
		YA_ADD( "@/dAdd10/fAdd10" ),
		YA_ADD( "@/dAdd11/" ),
		YA_ADD( "@/dAdd11/fAdd11" ),
		YA_ADD( "@/dAdd12/" ),
		YA_ADD( "@/dAdd12/fAdd12" ),
		YA_ADD( "@/dAdd13/" ),
		YA_ADD( "@/dAdd13/fAdd13" ),
		YA_ADD( "@/dAdd13_M/" ),
		YA_ADD( "@/dAdd14/" ),
		YA_ADD( "@/dAdd14/fAdd14" ),
		YA_ADD( "@/dAdd14_M/" ),
		YA_ADD( "@/dAdd15/" ),
		YA_ADD( "@/dAdd15/fAdd15" ),
		YA_ADD( "@/dAdd15_M/" ),
		YA_ADD( "@/dAdd16/" ),
		YA_ADD( "@/dAdd16/fAdd16" ),
		YA_ADD( "@/dAdd17/" ),
		YA_ADD( "@/dAdd17/fAdd17" ),
		YA_ADD( "@/dAdd18/" ),
		YA_ADD( "@/dAdd18/fAdd18" ),
		YA_ADD( "@/dAdd19/" ),
		YA_ADD( "@/dAdd19/fAdd19" ),

#if AA
		YA_ADD( "@/aAdd20/" ),
		YA(     "@/aAdd20/fAdd20", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),		// an ADD with attrs just shows up as an _ADD, it does not have _CHANGED_ set.
		YA_ADD( "@/aAdd21/" ),
		YA(     "@/aAdd21/fAdd21", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),
		YA_ADD( "@/aAdd22/" ),
		YA(     "@/aAdd22/fAdd22", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),
		YA_ADD( "@/aAdd23/" ),
		YA(     "@/aAdd23/fAdd23", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),
		YA_ADD( "@/aAdd24/" ),
		YA(     "@/aAdd24/fAdd24", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),

		YA_ADD( "@/aAdd25/" ),
		YA(     "@/aAdd25/fAdd25", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
		YA_ADD( "@/aAdd26/" ),
		YA(     "@/aAdd26/fAdd26", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
		YA_ADD( "@/aAdd27/" ),
		YA(     "@/aAdd27/fAdd27", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
		YA_ADD( "@/aAdd28/" ),
		YA(     "@/aAdd28/fAdd28", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
		YA_ADD( "@/aAdd29/" ),
		YA(     "@/aAdd29/fAdd29", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),

		YA_ADD( "@/aAdd30/" ),
		YA(     "@/aAdd30/fAdd30", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
		YA_ADD( "@/aAdd31/" ),
		YA(     "@/aAdd31/fAdd31", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
		YA_ADD( "@/aAdd32/" ),
		YA(     "@/aAdd32/fAdd32", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
		YA_ADD( "@/aAdd33/" ),
		YA(     "@/aAdd33/fAdd33", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
		YA_ADD( "@/aAdd34/" ),
		YA(     "@/aAdd34/fAdd34", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
#endif

		YA_MOD( "@/d2000/" ),
		YA_MOD( "@/d2000/f2000" ),
		YA_MOD( "@/d2001/" ),
		YA_MOD( "@/d2001/f2001" ),
		YA_MOD( "@/d2002/" ),
		YA_MOD( "@/d2002/f2002" ),
		YA_MOD( "@/d2003/" ),
		YA_MOD( "@/d2003/f2003" ),
		YA_MOD( "@/d2004/" ),
		YA_MOD( "@/d2004/f2004" ),
		YA_ADD( "@/d2004_M/" ),
		YA_MOD( "@/d2005/" ),
		YA_MOD( "@/d2005/f2005" ),
		YA_MOD( "@/d2006/" ),
		YA_MOD( "@/d2006/f2006" ),
		YA_MOD( "@/d2007/" ),
		YA_MOD( "@/d2007/f2007" ),
		YA_MOD( "@/d2008/" ),
		YA_MOD( "@/d2008/f2008" ),
		YA_MOD( "@/d2009/" ),
		YA_MOD( "@/d2009/f2009" ),
		YA_MOD( "@/d2010/" ),
		YA_MOD( "@/d2010/f2010" ),
		YA_MOD( "@/d2011/" ),
		YA_MOD( "@/d2011/f2011" ),
		YA_MOD( "@/d2012/" ),
		YA_MOD( "@/d2012/f2012" ),
		YA_MOD( "@/d2013/" ),
		YA_MOD( "@/d2013/f2013" ),
		YA_MOD( "@/d2014/" ),
		YA_MOD( "@/d2014/f2014" ),
		YA_MOD( "@/d2015/" ),
		YA_MOD( "@/d2015/f2015" ),
		YA_MOD( "@/d2016/" ),
		YA_MOD( "@/d2016/f2016" ),
		YA_MOD( "@/d2017/" ),
		YA_MOD( "@/d2017/f2017" ),
		YA_MOD( "@/d2018/" ),
		YA_MOD( "@/d2018/f2018" ),
		YA_MOD( "@/d2019/" ),
		YA_MOD( "@/d2019/f2019" ),
		YA_MOD( "@/d2020/" ),
		YA_MOD( "@/d2020/f2020" ),

		YA_REN( "@/d1000Ren/", "d1000" ),
		YA_REN( "@/d1001Ren/", "d1001" ),
		YA_REN( "@/d1002Ren/", "d1002" ),
		YA_REN( "@/d1003Ren/", "d1003" ),
		YA_REN( "@/d1004Ren/", "d1004" ),

		YA_MOD( "@/d1005/" ),
		YA_REN( "@/d1005/f1005Ren", "f1005" ),
		YA_MOD( "@/d1006/" ),
		YA_REN( "@/d1006/f1006Ren", "f1006" ),
		YA_MOD( "@/d1007/" ),
		YA_REN( "@/d1007/f1007Ren", "f1007" ),
		YA_MOD( "@/d1008/" ),
		YA_REN( "@/d1008/f1008Ren", "f1008" ),
		YA_MOD( "@/d1009/" ),
		YA_REN( "@/d1009/f1009Ren", "f1009" ),

		YA_MOD_REN( "@/d1010Ren/",         "d1010" ),		// dir is modified because stuff within it changed too.
		YA_REN(     "@/d1010Ren/f1010Ren", "f1010" ),
		YA_MOD_REN( "@/d1011Ren/",         "d1011" ),
		YA_REN(     "@/d1011Ren/f1011Ren", "f1011" ),
		YA_MOD_REN( "@/d1012Ren/",         "d1012" ),
		YA_REN(     "@/d1012Ren/f1012Ren", "f1012" ),
		YA_MOD_REN( "@/d1013Ren/",         "d1013" ),
		YA_REN(     "@/d1013Ren/f1013Ren", "f1013" ),
		YA_MOD_REN( "@/d1014Ren/",         "d1014" ),
		YA_REN(     "@/d1014Ren/f1014Ren", "f1014" ),

		YA_MOD_REN( "@/d1015Ren/",         "d1015" ),
		YA_REN(     "@/d1015Ren/f1015Ren", "f1015" ),
		YA_MOD_REN( "@/d1016Ren/",         "d1016" ),
		YA_REN(     "@/d1016Ren/f1016Ren", "f1016" ),
		YA_MOD_REN( "@/d1017Ren/",         "d1017" ),
		YA_REN(     "@/d1017Ren/f1017Ren", "f1017" ),
		YA_MOD_REN( "@/d1018Ren/",         "d1018" ),
		YA_REN(     "@/d1018Ren/f1018Ren", "f1018" ),
		YA_MOD_REN( "@/d1019Ren/",         "d1019" ),
		YA_REN(     "@/d1019Ren/f1019Ren", "f1019" ),
		YA_MOD_REN( "@/d1020Ren/",         "d1020" ),
		YA_REN(     "@/d1020Ren/f1020Ren", "f1020" ),

		YA_MOD(     "@/d1030/" ),
		YA_MOD_REN( "@/d1030/aa_ren/",                  "aa" ),
		YA_MOD_REN( "@/d1030/aa_ren/bb_ren/",           "bb" ),
		YA_REN(     "@/d1030/aa_ren/bb_ren/cc_ren/",    "cc" ),

		YA_MOD(     "@/d1031/" ),
		YA_MOD(     "@/d1031/aa_mov/" ),
		YA_MOD_MOV( "@/d1031/aa_mov/aa/",                  "@/d1031/" ),
		YA_MOD_REN( "@/d1031/aa_mov/aa/bb_ren/",           "bb" ),
		YA_MOD(     "@/d1031/aa_mov/aa/bb_ren/cc_mov/" ),
		YA_MOV(     "@/d1031/aa_mov/aa/bb_ren/cc_mov/cc/", "@/d1031/aa_mov/aa/bb_ren/" ),

		YA_MOD(     "@/d1032/" ),
		YA_MOD(     "@/d1032/aa_mov/" ),
		YA_MOD_MOV( "@/d1032/aa_mov/aa/",                  "@/d1032/" ),
		YA_MOD_REN( "@/d1032/aa_mov/aa/bb_ren/",           "bb" ),
		YA_MOD(     "@/d1032/aa_mov/aa/bb_ren/cc_mov/" ),
		YA_MOV(     "@/d1032/aa_mov/aa/bb_ren/cc_mov/cc/", "@/d1032/aa_mov/aa/bb_ren/" ),

		YA_MOD(     "@/d1033/" ),
		YA_MOD(     "@/d1033/aa_mov/" ),
		YA_MOD_MOV( "@/d1033/aa_mov/aa/",                            "@/d1033/" ),
		YA_MOD_REN( "@/d1033/aa_mov/aa/bb_ren/",                     "bb" ),
		YA_MOD(     "@/d1033/aa_mov/aa/bb_ren/cc_mov/" ),
		YA_MOD_MOV( "@/d1033/aa_mov/aa/bb_ren/cc_mov/cc/",           "@/d1033/aa_mov/aa/bb_ren/" ),
		YA_MOD(     "@/d1033/aa_mov/aa/bb_ren/cc_mov/cc/dd/" ),
		YA_MOD(     "@/d1033/aa_mov/aa/bb_ren/cc_mov/cc/dd/ee/" ),
		YA_MOD(     "@/d1033/aa_mov/aa/bb_ren/cc_mov/cc/dd/ee/f1033" ),

		YA_MOD( "@/d3000/" ),
		YA_MOD( "@/d3000Mov/" ),
		YA_MOV( "@/d3000Mov/f3000", "@/d3000/" ),
		YA_MOD( "@/d3001/" ),
		YA_MOD( "@/d3001Mov/" ),
		YA_MOV( "@/d3001Mov/f3001", "@/d3001/" ),
		YA_MOD( "@/d3002/" ),
		YA_MOD( "@/d3002Mov/" ),
		YA_MOV( "@/d3002Mov/f3002", "@/d3002/" ),
		YA_MOD( "@/d3003/" ),
		YA_MOD( "@/d3003Mov/" ),
		YA_MOV( "@/d3003Mov/f3003", "@/d3003/" ),
		YA_MOD( "@/d3004/" ),
		YA_MOD( "@/d3004Mov/" ),
		YA_MOV( "@/d3004Mov/f3004", "@/d3004/" ),
		YA_MOD( "@/d3005/" ),
		YA_MOD( "@/d3005Mov/" ),
		YA_MOV( "@/d3005Mov/f3005", "@/d3005/" ),
		YA_MOD( "@/d3006/" ),
		YA_MOD( "@/d3006Mov/" ),
		YA_MOV( "@/d3006Mov/f3006", "@/d3006/" ),
		YA_MOD( "@/d3007/" ),
		YA_MOD( "@/d3007Mov/" ),
		YA_MOV( "@/d3007Mov/f3007", "@/d3007/" ),
		YA_MOD( "@/d3008/" ),
		YA_MOD( "@/d3008Mov/" ),
		YA_MOV( "@/d3008Mov/f3008", "@/d3008/" ),
		YA_MOD( "@/d3009/" ),
		YA_MOD( "@/d3009Mov/" ),
		YA_MOV( "@/d3009Mov/f3009", "@/d3009/" ),

#if AA
		YA_MOD( "@/a4000/" ),
		YA(     "@/a4000/f4000", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),	// xattr should now be non-zero
		YA_MOD( "@/a4001/" ),
		YA(     "@/a4001/f4001", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA_MOD( "@/a4002/" ),
		YA(     "@/a4002/f4002", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA_MOD( "@/a4003/" ),
		YA(     "@/a4003/f4003", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA_MOD( "@/a4004/" ),
		YA(     "@/a4004/f4004", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		YA( "@/a4005/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA( "@/a4006/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA( "@/a4007/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA( "@/a4008/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA( "@/a4009/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		YA_MOD( "@/a4010/" ),
		YA(     "@/a4010/f4010", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),	// attrbits should now be non-zero
		YA_MOD( "@/a4011/" ),
		YA(     "@/a4011/f4011", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),
		YA_MOD( "@/a4012/" ),
		YA(     "@/a4012/f4012", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),
		YA_MOD( "@/a4013/" ),
		YA(     "@/a4013/f4013", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),
		YA_MOD( "@/a4014/" ),
		YA(     "@/a4014/f4014", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),

		YA_MOD( "@/a4015/" ),
		YA(     "@/a4015/f4015", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a4016/" ),
		YA(     "@/a4016/f4016", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a4017/" ),
		YA(     "@/a4017/f4017", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a4018/" ),
		YA(     "@/a4018/f4018", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a4019/" ),
		YA(     "@/a4019/f4019", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),

		YA_MOD( "@/a5000/" ),
		YA(     "@/a5000/f5000Ren", "f5000", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5001/" ),
		YA(     "@/a5001/f5001Ren", "f5001", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5002/" ),
		YA(     "@/a5002/f5002Ren", "f5002", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5003/" ),
		YA(     "@/a5003/f5003Ren", "f5003", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,                                       SG_TRUE,  SG_FALSE ),
		YA_MOD( "@/a5004/" ),
		YA(     "@/a5004/f5004Ren", "f5004", NULL, SG_DIFFSTATUS_FLAGS__RENAMED                                         | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE  ),

		YA_MOD( "@/a5005/"    ),
		YA_MOD( "@/a5005Mov/" ),
		YA(     "@/a5005Mov/f5005", NULL, "@/a5005/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5006/"    ),
		YA_MOD( "@/a5006Mov/" ),
		YA(     "@/a5006Mov/f5006", NULL, "@/a5006/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5007/"    ),
		YA_MOD( "@/a5007Mov/" ),
		YA(     "@/a5007Mov/f5007", NULL, "@/a5007/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5008/"    ),
		YA_MOD( "@/a5008Mov/" ),
		YA(     "@/a5008Mov/f5008", NULL, "@/a5008/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,                                       SG_TRUE,  SG_FALSE ),
		YA_MOD( "@/a5009/"    ),
		YA_MOD( "@/a5009Mov/" ),
		YA(     "@/a5009Mov/f5009", NULL, "@/a5009/", SG_DIFFSTATUS_FLAGS__MOVED                                         | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE  ),

		YA_MOD( "@/a5010/"    ),
		YA_MOD( "@/a5010Mov/" ),
		YA(     "@/a5010Mov/f5010Ren", "f5010", "@/a5010/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5011/"    ),
		YA_MOD( "@/a5011Mov/" ),
		YA(     "@/a5011Mov/f5011Ren", "f5011", "@/a5011/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5012/"    ),
		YA_MOD( "@/a5012Mov/" ),
		YA(     "@/a5012Mov/f5012Ren", "f5012", "@/a5012/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5013/"    ),
		YA_MOD( "@/a5013Mov/" ),
		YA(     "@/a5013Mov/f5013Ren", "f5013", "@/a5013/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5014/"    ),
		YA_MOD( "@/a5014Mov/" ),
		YA(     "@/a5014Mov/f5014Ren", "f5014", "@/a5014/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
#endif

		YA_MOD( "@/d6000/"    ),
		YA_MOD( "@/d6000Mov/" ),
		YA(     "@/d6000Mov/f6000Ren", "f6000", "@/d6000/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d6001/"    ),
		YA_MOD( "@/d6001Mov/" ),
		YA(     "@/d6001Mov/f6001Ren", "f6001", "@/d6001/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d6002/"    ),
		YA_MOD( "@/d6002Mov/" ),
		YA(     "@/d6002Mov/f6002Ren", "f6002", "@/d6002/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d6003/"    ),
		YA_MOD( "@/d6003Mov/" ),
		YA(     "@/d6003Mov/f6003Ren", "f6003", "@/d6003/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d6004/"    ),
		YA_MOD( "@/d6004Mov/" ),
		YA(     "@/d6004Mov/f6004Ren", "f6004", "@/d6004/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),

#if AA
		YA_MOD( "@/a7000/" ),
		YA(     "@/a7000/f7000Ren", "f7000", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7001/" ),
		YA(     "@/a7001/f7001Ren", "f7001", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7002/" ),
		YA(     "@/a7002/f7002Ren", "f7002", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7003/" ),
		YA(     "@/a7003/f7003Ren", "f7003", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,                                       SG_TRUE, SG_FALSE ),
		YA_MOD( "@/a7004/" ),
		YA(     "@/a7004/f7004Ren", "f7004", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED                                         | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		YA_MOD( "@/a7005/" ),
		YA_MOD( "@/a7005Mov/" ),
		YA(     "@/a7005Mov/f7005", NULL, "@/a7005/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7006/" ),
		YA_MOD( "@/a7006Mov/" ),
		YA(     "@/a7006Mov/f7006", NULL, "@/a7006/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7007/" ),
		YA_MOD( "@/a7007Mov/" ),
		YA(     "@/a7007Mov/f7007", NULL, "@/a7007/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7008/" ),
		YA_MOD( "@/a7008Mov/" ),
		YA(     "@/a7008Mov/f7008", NULL, "@/a7008/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,                                       SG_TRUE, SG_FALSE ),
		YA_MOD( "@/a7009/" ),
		YA_MOD( "@/a7009Mov/" ),
		YA(     "@/a7009Mov/f7009", NULL, "@/a7009/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED                                         | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		YA_MOD( "@/a7010/" ),
		YA_MOD( "@/a7010Mov/" ),
		YA(     "@/a7010Mov/f7010Ren", "f7010", "@/a7010/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7011/" ),
		YA_MOD( "@/a7011Mov/" ),
		YA(     "@/a7011Mov/f7011Ren", "f7011", "@/a7011/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7012/" ),
		YA_MOD( "@/a7012Mov/" ),
		YA(     "@/a7012Mov/f7012Ren", "f7012", "@/a7012/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7013/" ),
		YA_MOD( "@/a7013Mov/" ),
		YA(     "@/a7013Mov/f7013Ren", "f7013", "@/a7013/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7014/" ),
		YA_MOD( "@/a7014Mov/" ),
		YA(     "@/a7014Mov/f7014Ren", "f7014", "@/a7014/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
#endif

		YA_MOD( "@/d8000/" ),
		YA_MOD( "@/d8000Mov/" ),
		YA(     "@/d8000Mov/f8000Ren", "f8000", "@/d8000/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d8001/" ),
		YA_MOD( "@/d8001Mov/" ),
		YA(     "@/d8001Mov/f8001Ren", "f8001", "@/d8001/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d8002/" ),
		YA_MOD( "@/d8002Mov/" ),
		YA(     "@/d8002Mov/f8002Ren", "f8002", "@/d8002/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d8003/" ),
		YA_MOD( "@/d8003Mov/" ),
		YA(     "@/d8003Mov/f8003Ren", "f8003", "@/d8003/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d8004/" ),
		YA_MOD( "@/d8004Mov/" ),
		YA(     "@/d8004Mov/f8004Ren", "f8004", "@/d8004/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
	};

#undef YA
#undef YA_ADD
#undef YA_DEL
#undef YA_REN
#undef YA_MOV
#undef YA_MOD
#undef YA_MOD_REN
#undef YA_MOD_MOV	

	SG_uint32 nrRowsYA = SG_NrElements(aRowYA);

	//////////////////////////////////////////////////////////////////
	// aRowZ[] -- data used to populate WD (from <cset2> baseline) so
	// that we can test JOIN( cset1-vs-cset2, cset2-vs-wd ).

	struct _row_z
	{
		const char *			szDir1;
		const char *			szDir2;
		const char *			szFile1;
		const char *			szFile2;
		SG_diffstatus_flags		dsFlags;
		const char *			szContentParam;
		const char *			szXAttrParam;

		SG_pathname *			pPathDir;
		SG_pathname *			pPathFile;
		const char *			szLastDir;
		const char *			szLastFile;
	};

#define Z(d1,d2,f1,f2,ds,cp,xp)	{ d1, d2, f1, f2, ds, cp, xp, NULL, NULL, d1, f1 }

	struct _row_z aRowZ[] = {

		//////////////////////////////////////////////////////////////////
		// create some peerless entries.
		//
		// these entries will be present in cset2-vs-wd, but not in cset1-vs-cset.
		// these will be handled by _process_join_cb__bw().
		//
		// these are of several types:
		//
		// [1] additions

		Z(	"zAdd10",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),				NULL,	NULL),	// add <wd>/zAdd10/
		Z(	"zAdd20",	NULL,		"z_Add20",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),				NULL,	NULL),	// add <wd>/zAdd20/fAdd20 (as a 20 line file)

#if AA
		Z(	"zAdd11",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		NULL,	"1"),	// add <wd>/zAdd11/ with XATTR set

		Z(	"zAdd21",	NULL,		"z_Add21",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		NULL,	"1"),	// add <wd>/zAdd21/fAdd21 with XATTR
		Z(	"zAdd22",	NULL,		"z_Add22",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),	NULL,	NULL),
		Z(	"zAdd23",	NULL,		"z_Add23",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),	NULL,	"1"),
#endif		

		// [2] changes to something that existed in the tree as of cset1 and that
		// was not modified by cset2.  we reserved the 9000 series for this.

		Z(	"d9000",	NULL,		"f9000",	NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),				NULL,	NULL),	// delete <wd>/d9000/f9000
		Z(	"d9000",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),				NULL,	NULL),	// delete <wd>/d9000
		Z(	"d9001",	NULL,		"f9001",	NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),				NULL,	NULL),	// delete <wd>/d9001/f9001


		Z(	"d9002",	NULL,		"f9002",	"f9002Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED),				NULL,	NULL),	// rename <wd>/d9002/f9002 --> <wd>/d9002/f9002Ren

		Z(	"d9003",	"d9003Mov",	"f9003",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),				NULL,	NULL),	// move <wd>/d9003/f9003 --> <wd>/d9003Mov/f9003
		Z(	"d9004",	"d9004Mov",	"f9004",	"f9004Ren",	(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__RENAMED),			NULL,	NULL),	// rename <wd>/d9004/f9004 --> f9004Ren and move it to <wd>/d9004Mov

		Z(	"d9005",	NULL,		"f9005",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED),			"+",	NULL),	// modify (add 4 lines to) <wd>/d9005/f9005
		Z(	"d9006",	NULL,		"f9006",	"f9006Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED),			"+",	NULL),	// modify (add 4 lines to) <wd>/d9006/f9006 and rename
		Z(	"d9007",	"d9007Mov",	"f9007",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__MOVED),				"+",	NULL),	// modify (add 4 lines to) <wd>/d9007/f9007 and move
		Z(	"d9008",	"d9008Mov",	"f9008",	"f9008Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED),				"+",	NULL),	// modify (add 4 lines to) <wd>/d9008/f9008 and rename and move

		Z(	"d9009",	"d9009Ren",	NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__RENAMED),				NULL,	NULL),	// rename <wd>/d9009 --> <wd>/d9009Ren

#if AA
		Z(	"a9010",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		NULL,	"1"),	// set xattrs on <wd>/d9010
		Z(	"a9011",	NULL,		"f9011",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		NULL,	"1"),	// set xattrs on <wd>/d9011/f9011
		Z(	"a9012",	NULL,		"f9012",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),	NULL,	NULL),	// set +x bit on <wd>/d9012/f9012
		Z(	"a9013",	NULL,		"f9013",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),	NULL,	"1"),	// set xattrs and +x bit on <wd>/d9013/f9013

		Z(	"a9014",	"a9014Mov",	"f9014",	"f9014Ren",	(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),	NULL,	"1"),

		Z(	"a9015",	"a9015Mov",	"f9015",	"f9015Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),	"+",	"1"),
#endif

		//////////////////////////////////////////////////////////////////
		// we don't touch some of the things changed in cset2 so that they
		// will appear as peerless entries and be handled by _process_join_cb__cc().
		//
		// ignore

		// TODO to be continued...

		//////////////////////////////////////////////////////////////////
		// change some things that were changed in cset2.  this stresses
		// the *real* JOIN code in _process_join_cb__cc_bw().

		// [1] start with things that were added in cset2.  modify them somehow.
		// verify that the join is something added, but with different settings.

		Z(	"dAdd1",	"dAdd1_R",	NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__RENAMED),				NULL,	NULL),	// A  +  Z+R ==> A'
		Z(	"dAdd2",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),				NULL,	NULL),	// A  +  D ==> nil

		Z(	"dAdd10",	NULL,		"fAdd10",	NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),				NULL,	NULL),	// A  +  D ==> nil
		Z(	"dAdd18",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),				NULL,	NULL),	// A  +  D ==> nil    (also delete of non-empty directory) (fAdd18 was also added in Y, so it won't appear in Z results)

		Z(	"dAdd11",	NULL,		"fAdd11",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED),			"+",	NULL),	// A  +  Mod(append4) ==> A'

		Z(	"dAdd12",	NULL,		"fAdd12",	"fAdd12_R",	(SG_DIFFSTATUS_FLAGS__RENAMED),				NULL,	NULL),	// A  +  Z+R ==> A'
		Z(	"dAdd13",	"dAdd13_M",	"fAdd13",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),				NULL,	NULL),	// A  +  Z+Mov ==> A'
		Z(	"dAdd14",	"dAdd14_M",	"fAdd14",	"fAdd14_R",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED),				NULL,	NULL),	// A  +  Z+Mov+R ==> A'
		Z(	"dAdd15",	"dAdd15_M",	"fAdd15",	"fAdd15_R",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED),				"+",	NULL),	// A  +  Mod(append4)+Mov+R ==> A'
#if AA
		Z(	"dAdd16",	NULL,		"fAdd16",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		NULL,	"2"),	// A  +  Z+X(+test2=Z) ==> A'
		Z(	"dAdd17",	NULL,		"fAdd17",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),	NULL,	NULL),	// A  +  Z+chmod(+x) ==> A'(+x)

		Z(	"aAdd20",	NULL,		"fAdd20",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		NULL,	"2"),	// A(+test1=Y)  +  Z+X(+test2=Z)  ==> A'(+test1=Y,+test2=Z)
		Z(	"aAdd21",	NULL,		"fAdd21",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		NULL,	"1"),	// A(+test1=Y)  +  Z+X(+test1=Z) ==> A'(+test1=Z)
		Z(	"aAdd22",	NULL,		"fAdd22",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		NULL,	"x"),	// A(+test1=Y)  +  Z+X(-test1,-test2) ==> A'

		Z(	"aAdd25",	NULL,		"fAdd25",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),	NULL,	NULL),	// A(+x)  +  Z+chmod(-x) ==> A'(-x)

		Z(	"aAdd30",	NULL,		"fAdd30",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		"+",	"x"),	// A(+x,+test1=Y)  +  Mod(append4)+X(-test1,-test2)+chmod(-x) ==> A'(-x)
#endif

		// [2] start with something that was Modified in cset2 (the content was changed)
		// and apply various changes and verify the result.  in som cases, we'll get a
		// slightly different modified item.  in other cases (when we undo the content
		// change), we get just the remaining settings (or nothing).

		Z(	"d2000",	NULL,		"f2000",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED),			"+",	NULL),	// Mod(append4)  +  Mod(append4)  ==>  Mod'(append8)
		Z(	"d2001",	NULL,		"f2001",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED),			"u",	NULL),	// Mod(append4)  +  Mod(undo)     ==>  nil

		Z(	"d2002",	NULL,		"f2002",	NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),				NULL,	NULL),	// Mod(append4)  +  D  ==>  D

		Z(	"d2003",	NULL,		"f2003",	"f2003_R",	(SG_DIFFSTATUS_FLAGS__RENAMED),				NULL,	NULL),	// Mod(append4)  +  Z+R(a,b)  ==>  Mod'(append4)+R(a,b)
		Z(	"d2004",	"d2004_M",	"f2004",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),				NULL,	NULL),	// Mod(append4)  +  Z+Mov(a,b)  ==>  Mod'(append4)+Mov(a,b)

		// [3] start with something that was a Z (renamed and/or moved) in cset2 and rename/move it
		// again -or- undo the rename/move.

		Z(	"d1000Ren",	"d1000Ren2",NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__RENAMED),				NULL,	NULL),	// Z+R(a,b)  +  Z+R(b,c)  ==>  Z+R(a,c)
		Z(	"d1005",	NULL,		"f1005Ren",	"f1005Ren2",(SG_DIFFSTATUS_FLAGS__RENAMED),				NULL,	NULL),	// Z+R(a,b)  +  Z+R(b,c)  ==>  Z+R(a,c)

		Z(	"d1001Ren",	"d1001",	NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__RENAMED),				NULL,	NULL),	// Z+R(a,b)  +  Z+R(b,a)  ==>  nil
		Z(	"d1006",	NULL,		"f1006Ren",	"f1006",	(SG_DIFFSTATUS_FLAGS__RENAMED),				NULL,	NULL),	// Z+R(a,b)  +  Z+R(b,a)  ==>  nil

		Z(	"d1002Ren",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),				NULL,	NULL),	// Z+R(a,b)  +  D  ==> D	(also delete of non-empty directory) (f1002 will appear as implied delete in Z)
		Z(	"d1007",	NULL,		"f1007Ren",	NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),				NULL,	NULL),	// Z+R(a,b)  +  D  ==> D

		Z(	"d1008",	NULL,		"f1008Ren",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED),			"+",	NULL),	// Z+R(a,b)  +  Mod(append4)  ==>  Mod'(append4)+R(a,b)
		Z(	"d1015Ren",	NULL,		"f1015Ren",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED),			"+",	NULL),	// Z+R(a,b)  +  Mod(append4)  ==>  Mod'(append4)+R(a,b)

		
		Z(	"d1030/aa_ren/bb_ren/cc_ren/dd",				"dd_ren",	NULL,		NULL,			(SG_DIFFSTATUS_FLAGS__RENAMED),		NULL,	NULL),
		Z(	"d1030/aa_ren/bb_ren/cc_ren/dd_ren/ee",			NULL,		"f1030",	"f1030_ren",	(SG_DIFFSTATUS_FLAGS__RENAMED),		NULL,	NULL),

		Z(	"d1031/aa_mov/aa/bb_ren/cc_mov/cc/dd",			"dd_ren",	NULL,		NULL,			(SG_DIFFSTATUS_FLAGS__RENAMED),		NULL,	NULL),
		Z(	"d1031/aa_mov/aa/bb_ren/cc_mov/cc/dd_ren/ee",	NULL,		"f1031",	"f1031_ren",	(SG_DIFFSTATUS_FLAGS__RENAMED),		NULL,	NULL),

		Z(	"d1032/aa_mov/aa/bb_ren/cc_mov/cc/dd/ee",		NULL,		"f1032",	NULL,			(SG_DIFFSTATUS_FLAGS__DELETED),		NULL,	NULL),

		Z(	"d1033/aa_mov/aa/bb_ren/cc_mov/cc/dd/ee",		NULL,		"f1033",	NULL,			(SG_DIFFSTATUS_FLAGS__DELETED),		NULL,	NULL),
		

		Z(	"d3000Mov",	"d3000Mov2","f3000",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),				NULL,	NULL),	// Z+Mov(a,b)  +  Z+Mov(b,c)  ==>  Z+Mov(a,c)
		Z(	"d3001Mov",	"d3001",	"f3001",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),				NULL,	NULL),	// Z+Mov(a,b)  +  Z+Mov(b,a)  ==>  nil

		Z(	"d3002Mov",	NULL,		"f3002",	NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),				NULL,	NULL),	// Z+Mov(a,b)  +  D  ==> D

		Z(	"d3003Mov",	NULL,		"f3003",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED),			"+",	NULL),	// Z+Mov(a,b)  +  Mod(append4)  ==>  Mod'(append4)+Mov(a,b)

#if AA
		// [4] start with something that had XATTR or ATTRBITS added in cset2 and
		// change them again or undo them.

		Z(	"a4000",	NULL,		"f4000",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		NULL,	"2"),	// Z+X(+test1=Y)  +  Z+X(+test2=Z)  ==>  Z+X(+test1=Y,+test2=Z)
		Z(	"a4001",	NULL,		"f4001",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		NULL,	"1"),	// Z+X(+test1=Y)  +  Z+X(+test1=Z)  ==>  Z+X(+test1=Z)
		Z(	"a4002",	NULL,		"f4002",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		NULL,	"x"),	// Z+X(+test1=Y)  +  Z+X(-test1,-test2)  ==>  nil

		Z(	"a4010",	NULL,		"f4010",	NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),	NULL,	NULL),	// Z+chmod(+x)  +  Z+chmod(-x)  ==>  nil
		
		Z(	"a4015",	NULL,		"f4015",	"f4015Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED),				NULL,	NULL),	// Z+X(+test1=Y)+chmod(+x)  +  Z+R(a,b)  ==>  Z+X(+test1=Y)+chmod(+x)+R(a,b)
		Z(	"a4016",	"a4016Mov",	"f4016",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),				NULL,	NULL),	// Z+X(+test1=Y)+chmod(+x)  +  Z+Mov(a,b)  ==>  Z+X(+test1=Y)+chmod(+x)+Mov(a,b)

		Z(	"a4017",	NULL,		"f4017",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED),			"+",	NULL),	// Z+X(+test1=Y)+chmod(+x)  +  Mod(append4)  ==>  Mod'(append4)+X(+test1=Y)+chmod(+x)
#endif

	};

	SG_uint32 nrRowsZ = SG_NrElements(aRowZ);


	struct _row_join_answer
	{
		const char *			szRepoPath;
		const char *			szOldName_when_rename;
		const char *			szOldParentRepoPath_when_moved;
		SG_diffstatus_flags		dsFlags;
		SG_bool					bAttrBitsNonZero;
		SG_bool					bXAttrsNonZero;
	};

#define JA(rp,nm,prp,ds,bA,bX)	{ rp, nm, prp, ds, bA, bX }
#define JA_ADD(rp)				{ rp, NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_FALSE }
#define JA_DEL(rp)				{ rp, NULL, NULL, SG_DIFFSTATUS_FLAGS__DELETED, SG_FALSE, SG_FALSE }
#define JA_MOD(rp)				{ rp, NULL, NULL, SG_DIFFSTATUS_FLAGS__MODIFIED, SG_FALSE, SG_FALSE }
#define JA_REN(rp,nm)			{ rp, nm, NULL, SG_DIFFSTATUS_FLAGS__RENAMED, SG_FALSE, SG_FALSE }
#define JA_MOV(rp,prp)			{ rp, NULL, prp, SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE }

#define JA_MOD_REN(rp,nm)		{ rp, nm, NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED, SG_FALSE, SG_FALSE }
#define JA_MOD_MOV(rp,prp)		{ rp, NULL, prp, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE }

	struct _row_join_answer aRowJA[] = {

		// these rows correspond to the effects produced by the join of the aRowY
		// and aRowZ table above.  that is, using aRowX we create CSET1 and using
		// aRowY we create CSET2 and using aRowZ we (eventually) create CSET3.
		// we compute the join on CSET1 and the pendingtree after aRowZ is applied
		// (with CSET2 being the implied baseline between them).
		// 
		// they are not necessarily line-for-line because some lines in aRowZ may
		// do more than one thing (such as recursive delete) or many lines may act
		// on one object (such as sequence of moves and renames on one entry).

		JA_MOD( "@/" ),

		// some stuff new in Z

		JA_ADD( "@/zAdd10/" ),
		JA_ADD( "@/zAdd20/" ),
		JA_ADD( "@/zAdd20/z_Add20" ),
#if AA
		JA(     "@/zAdd11/",        NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),		// dir added with non-zero xattrs

		JA_ADD( "@/zAdd21/" ),
		JA(     "@/zAdd21/z_Add21", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),
		JA_ADD( "@/zAdd22/" ),
		JA(     "@/zAdd22/z_Add22", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
		JA_ADD( "@/zAdd23/" ),
		JA(     "@/zAdd23/z_Add23", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),

#endif

		// some stuff that was in the tree from X but not changed in Y and that we changed in Z

		JA_DEL( "@/d9000/" ),
		JA_DEL( "@/d9000/f9000" ),
		JA_MOD( "@/d9001/" ),
		JA_DEL( "@/d9001/f9001" ),

		JA_MOD( "@/d9002/" ),
		JA_REN( "@/d9002/f9002Ren", "f9002" ),

		JA_MOD( "@/d9003/" ),
		JA_MOD( "@/d9003Mov/" ),
		JA_MOV( "@/d9003Mov/f9003", "@/d9003/"),

		JA_MOD( "@/d9004/" ),
		JA_MOD( "@/d9004Mov/" ),
		JA(     "@/d9004Mov/f9004Ren", "f9004", "@/d9004/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),

		JA_MOD( "@/d9005/" ),
		JA_MOD( "@/d9005/f9005" ),

		JA_MOD(     "@/d9006/" ),
		JA_MOD_REN( "@/d9006/f9006Ren", "f9006" ),

		JA_MOD(     "@/d9007/" ),
		JA_MOD(     "@/d9007Mov/" ),
		JA_MOD_MOV( "@/d9007Mov/f9007", "@/d9007/" ),

		JA_MOD(     "@/d9008/" ),
		JA_MOD(     "@/d9008Mov/" ),
		JA(         "@/d9008Mov/f9008Ren", "f9008", "@/d9008/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),

		JA_REN( "@/d9009Ren/", "d9009" ),

#if AA
		JA(     "@/a9010/",        NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),		// changed xattrs and now should be non-zero

		JA_MOD( "@/a9011/" ),
		JA(     "@/a9011/f9011",   NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		JA_MOD( "@/a9012/" ),
		JA(     "@/a9012/f9012",   NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),		// changed attrbits and now should be non-zero

		JA_MOD( "@/a9013/" ),
		JA(     "@/a9013/f9013",   NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),

		JA_MOD( "@/a9014/" ),
		JA_MOD( "@/a9014Mov/" ),
		JA(     "@/a9014Mov/f9014Ren", "f9014", "@/a9014/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),

		JA_MOD( "@/a9015/" ),
		JA_MOD( "@/a9015Mov/" ),
		JA(     "@/a9015Mov/f9015Ren", "f9015", "@/a9015/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
#endif

		// these were deleted in Y and are inherited

		JA_DEL( "@/d0000/f0000" ),
		JA_DEL( "@/d0000/" ),

		// these were changed somehow in Y and again in Z

		JA_ADD( "@/dAdd1_R/" ),
		JA(     "@/dAdd2/", NULL, NULL, SG_DIFFSTATUS_FLAGS__UNDONE_ADDED, SG_FALSE, SG_FALSE ),

		JA_ADD( "@/dAdd10/" ),
		JA(     "@/dAdd10/fAdd10", NULL, NULL, SG_DIFFSTATUS_FLAGS__UNDONE_ADDED, SG_FALSE, SG_FALSE ),

		JA(     "@/dAdd18/", NULL, NULL, SG_DIFFSTATUS_FLAGS__UNDONE_ADDED, SG_FALSE, SG_FALSE ),
		JA(     "@/dAdd18/fAdd18", NULL, NULL, SG_DIFFSTATUS_FLAGS__UNDONE_ADDED, SG_FALSE, SG_FALSE ),		// implicitly deleted because parent directory was deleted

		JA_ADD( "@/dAdd11/" ),				// A + Mod ==> A'
		JA_ADD( "@/dAdd11/fAdd11" ),		// A + Mod ==> A'

		JA_ADD( "@/dAdd12/" ),
		JA_ADD( "@/dAdd12/fAdd12_R" ),		// A + R ==> A'

		JA_ADD( "@/dAdd13/" ),
		JA_ADD( "@/dAdd13_M/" ),
		JA_ADD( "@/dAdd13_M/fAdd13" ),		// A + Mov ==> A'

		JA_ADD( "@/dAdd14/" ),
		JA_ADD( "@/dAdd14_M/" ),
		JA_ADD( "@/dAdd14_M/fAdd14_R" ),	// A + Mov+R ==> A'

		JA_ADD( "@/dAdd15/" ),
		JA_ADD( "@/dAdd15_M/" ),
		JA_ADD( "@/dAdd15_M/fAdd15_R" ),

		// these were added in Y regardless of AA.  and modified in Z iff AA.

#if AA
		JA_ADD( "@/dAdd16/" ),
		JA(     "@/dAdd16/fAdd16", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),
		JA_ADD( "@/dAdd17/" ),
		JA(     "@/dAdd17/fAdd17", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
#else
		JA_ADD( "@/dAdd16/" ),
		JA_ADD( "@/dAdd16/fAdd16" ),
		JA_ADD( "@/dAdd17/" ),
		JA_ADD( "@/dAdd17/fAdd17" ),
#endif

		// these were added in Y only if AA.  and modified in Z only if AA.

#if AA
		JA_ADD( "@/aAdd20/" ),
		JA(     "@/aAdd20/fAdd20", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),
		JA_ADD( "@/aAdd21/" ),
		JA(     "@/aAdd21/fAdd21", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),
		JA_ADD( "@/aAdd22/" ),
		JA(     "@/aAdd22/fAdd22", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_FALSE ),	// A+xattr + Z-xattr ==> A

		JA_ADD( "@/aAdd25/" ),
		JA(     "@/aAdd25/fAdd25", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_FALSE ),	// A+x + Z-x ==> A

		JA_ADD( "@/aAdd30/" ),
		JA(     "@/aAdd30/fAdd30", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_FALSE ),	// A+x+xattr + M-x-xattr ==> A'
#endif

		// these were added in Y and modified in Z.
		
		JA_MOD( "@/d2000/" ),
		JA_MOD( "@/d2000/f2000" ),
		JA_MOD( "@/d2001/" ),
		JA(     "@/d2001/f2001", NULL, NULL, SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED, SG_FALSE, SG_FALSE ),
		JA_MOD( "@/d2002/" ),
		JA_DEL( "@/d2002/f2002" ),
		JA_MOD( "@/d2003/" ),
		JA(     "@/d2003/f2003_R", "f2003", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED, SG_FALSE, SG_FALSE ),
		JA_MOD( "@/d2004/" ),
		JA_ADD( "@/d2004_M/" ),
		JA(     "@/d2004_M/f2004", NULL, "@/d2004/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),

		JA_REN( "@/d1000Ren2/", "d1000" ),			// R(a,b) + R(b,c) ==> R(a,c)

		JA_MOD( "@/d1005/" ),
		JA_REN( "@/d1005/f1005Ren2", "f1005" ),		// R(a,b) + R(b,c) ==> R(a,c)

		JA(     "@/d1001/", "d1001", NULL, SG_DIFFSTATUS_FLAGS__UNDONE_RENAMED, SG_FALSE, SG_FALSE ),

		JA_MOD( "@/d1006/" ),
		JA(     "@/d1006/f1006", "f1006", NULL, SG_DIFFSTATUS_FLAGS__UNDONE_RENAMED, SG_FALSE, SG_FALSE ),

		JA_DEL( "@/d1002/" ),			// R + D ==> D ignoring rename
		JA(     "@/d1002/f1002", NULL, NULL, SG_DIFFSTATUS_FLAGS__DELETED, SG_FALSE, SG_FALSE ),		// implicitly deleted because parent directory was deleted

		JA_MOD( "@/d1007/" ),
		JA_DEL( "@/d1007/f1007" ),		// R + D ==> D ignoring rename

		JA_MOD( "@/d1008/" ),
		JA    ( "@/d1008/f1008Ren", "f1008", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED, SG_FALSE, SG_FALSE ),

		JA_MOD_REN( "@/d1015Ren/",         "d1015" ),
		JA_MOD_REN( "@/d1015Ren/f1015Ren", "f1015" ),


		JA_MOD(     "@/d1030/" ),
		JA_MOD_REN( "@/d1030/aa_ren/",                                  "aa" ),
		JA_MOD_REN( "@/d1030/aa_ren/bb_ren/",                           "bb" ),
		JA_MOD_REN( "@/d1030/aa_ren/bb_ren/cc_ren/",                    "cc" ),
		JA_MOD_REN( "@/d1030/aa_ren/bb_ren/cc_ren/dd_ren/",             "dd" ),
		JA_MOD(     "@/d1030/aa_ren/bb_ren/cc_ren/dd_ren/ee/" ),
		JA_REN(     "@/d1030/aa_ren/bb_ren/cc_ren/dd_ren/ee/f1030_ren", "f1030" ),

		JA_MOD(     "@/d1031/" ),
		JA_MOD(     "@/d1031/aa_mov/" ),
		JA_MOD_MOV( "@/d1031/aa_mov/aa/",                                       "@/d1031/" ),
		JA_MOD_REN( "@/d1031/aa_mov/aa/bb_ren/",                                "bb" ),
		JA_MOD(     "@/d1031/aa_mov/aa/bb_ren/cc_mov/" ),
		JA_MOD_MOV( "@/d1031/aa_mov/aa/bb_ren/cc_mov/cc/",                      "@/d1031/aa_mov/aa/bb_ren/" ),
		JA_MOD_REN( "@/d1031/aa_mov/aa/bb_ren/cc_mov/cc/dd_ren/",               "dd" ),
		JA_MOD(     "@/d1031/aa_mov/aa/bb_ren/cc_mov/cc/dd_ren/ee/" ),
		JA_REN(     "@/d1031/aa_mov/aa/bb_ren/cc_mov/cc/dd_ren/ee/f1031_ren",   "f1031" ),

		JA_MOD(     "@/d1032/" ),
		JA_MOD(     "@/d1032/aa_mov/" ),
		JA_MOD_MOV( "@/d1032/aa_mov/aa/",                              "@/d1032/" ),
		JA_MOD_REN( "@/d1032/aa_mov/aa/bb_ren/",                       "bb" ),
		JA_MOD(     "@/d1032/aa_mov/aa/bb_ren/cc_mov/" ),
		JA_MOD_MOV( "@/d1032/aa_mov/aa/bb_ren/cc_mov/cc/",             "@/d1032/aa_mov/aa/bb_ren/" ),
		JA_MOD(     "@/d1032/aa_mov/aa/bb_ren/cc_mov/cc/dd/" ),
		JA_MOD(     "@/d1032/aa_mov/aa/bb_ren/cc_mov/cc/dd/ee/" ),
		JA_DEL(     "@/d1032/aa_mov/aa/bb_ren/cc_mov/cc/dd/ee/f1032" ),

		JA_MOD(     "@/d1033/" ),
		JA_MOD(     "@/d1033/aa_mov/" ),
		JA_MOD_MOV( "@/d1033/aa_mov/aa/",                            "@/d1033/" ),
		JA_MOD_REN( "@/d1033/aa_mov/aa/bb_ren/",                     "bb" ),
		JA_MOD(     "@/d1033/aa_mov/aa/bb_ren/cc_mov/" ),
		JA_MOD_MOV( "@/d1033/aa_mov/aa/bb_ren/cc_mov/cc/",           "@/d1033/aa_mov/aa/bb_ren/" ),
		JA_MOD(     "@/d1033/aa_mov/aa/bb_ren/cc_mov/cc/dd/" ),
		JA_MOD(     "@/d1033/aa_mov/aa/bb_ren/cc_mov/cc/dd/ee/" ),
		JA_DEL(     "@/d1033/aa_mov/aa/bb_ren/cc_mov/cc/dd/ee/f1033" ),


		JA_MOD( "@/d3000/" ),						// MOV(@/d3000/f3000 to @/d3000Mov/f3000) + MOV(@/d3000Mov/f3000 to @/d3000Mov2/f3000) ==> MOV(@/d3000/f3000 to @/d3000Mov2/f3000)
		JA_MOD( "@/d3000Mov/" ),					// TODO it is questionable whether this should be marked modifed after the join.
		JA_MOD( "@/d3000Mov2/" ),
		JA_MOV( "@/d3000Mov2/f3000", "@/d3000/" ),

		JA_MOD( "@/d3001/" ),                       // MOV(@/d3001/f3001 to @/d3001Mov/f3001) + MOV(@/d3001Mov/f3001 to @d3001/f3001) ==> undo
		JA_MOD( "@/d3001Mov/" ),					// TODO should this directory be marked modified in the join?
		JA(     "@/d3001/f3001", NULL, "@/d3001/", SG_DIFFSTATUS_FLAGS__UNDONE_MOVED, SG_FALSE, SG_FALSE ),

		JA_MOD( "@/d3002/" ),						// MOV(@/d3002/f3002 to @/d3002Mov/f3002) + D(@/d3002Mov/f3002) ==> D(@/d3002/f3002)
		JA_MOD( "@/d3002Mov/" ),
		JA_DEL( "@/d3002/f3002" ),

		JA_MOD(     "@/d3003/" ),
		JA_MOD(     "@/d3003Mov/" ),
		JA_MOD_MOV( "@/d3003Mov/f3003", "@/d3003/" ),

#if AA
		JA_MOD( "@/a4000/" ),
		JA(     "@/a4000/f4000", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),	// xattr should now be non-zero

		JA_MOD( "@/a4001/" ),
		JA(     "@/a4001/f4001", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		JA_MOD( "@/a4002/" ),
		JA(     "@/a4002/f4002", NULL, NULL, SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_XATTRS, SG_FALSE, SG_FALSE ),

		JA_MOD( "@/a4010/" ),
		JA(     "@/a4010/f4010", NULL, NULL, SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_ATTRBITS, SG_FALSE, SG_FALSE ),

		JA_MOD( "@/a4015/" ),
		JA(     "@/a4015/f4015Ren", "f4015", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),

		JA_MOD( "@/a4016/" ),
		JA_MOD( "@/a4016Mov/" ),
		JA(     "@/a4016Mov/f4016", NULL, "@/a4016/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),

		JA_MOD( "@/a4017/" ),
		JA(     "@/a4017/f4017", NULL, NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
#endif


		// these were added in Y on if AA.  they were NOT modified in Z.

#if AA
		JA_ADD( "@/aAdd23/" ),
		JA(     "@/aAdd23/fAdd23", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),
		JA_ADD( "@/aAdd24/" ),
		JA(     "@/aAdd24/fAdd24", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),

		JA_ADD( "@/aAdd26/" ),
		JA(     "@/aAdd26/fAdd26", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
		JA_ADD( "@/aAdd27/" ),
		JA(     "@/aAdd27/fAdd27", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
		JA_ADD( "@/aAdd28/" ),
		JA(     "@/aAdd28/fAdd28", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
		JA_ADD( "@/aAdd29/" ),
		JA(     "@/aAdd29/fAdd29", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),

		JA_ADD( "@/aAdd31/" ),
		JA(     "@/aAdd31/fAdd31", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
		JA_ADD( "@/aAdd32/" ),
		JA(     "@/aAdd32/fAdd32", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
		JA_ADD( "@/aAdd33/" ),
		JA(     "@/aAdd33/fAdd33", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
		JA_ADD( "@/aAdd34/" ),
		JA(     "@/aAdd34/fAdd34", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
#endif

		JA_ADD( "@/dAdd3/" ),
		JA_ADD( "@/dAdd4/" ),
		JA_ADD( "@/dAdd5/" ),
		JA_ADD( "@/dAdd6/" ),
		JA_ADD( "@/dAdd7/" ),
		JA_ADD( "@/dAdd8/" ),
		JA_ADD( "@/dAdd9/" ),

		JA_ADD( "@/dAdd19/" ),
		JA_ADD( "@/dAdd19/fAdd19" ),


		JA_MOD( "@/d2005/" ),
		JA_MOD( "@/d2005/f2005" ),
		JA_MOD( "@/d2006/" ),
		JA_MOD( "@/d2006/f2006" ),
		JA_MOD( "@/d2007/" ),
		JA_MOD( "@/d2007/f2007" ),
		JA_MOD( "@/d2008/" ),
		JA_MOD( "@/d2008/f2008" ),
		JA_MOD( "@/d2009/" ),
		JA_MOD( "@/d2009/f2009" ),
		JA_MOD( "@/d2010/" ),
		JA_MOD( "@/d2010/f2010" ),
		JA_MOD( "@/d2011/" ),
		JA_MOD( "@/d2011/f2011" ),
		JA_MOD( "@/d2012/" ),
		JA_MOD( "@/d2012/f2012" ),
		JA_MOD( "@/d2013/" ),
		JA_MOD( "@/d2013/f2013" ),
		JA_MOD( "@/d2014/" ),
		JA_MOD( "@/d2014/f2014" ),
		JA_MOD( "@/d2015/" ),
		JA_MOD( "@/d2015/f2015" ),
		JA_MOD( "@/d2016/" ),
		JA_MOD( "@/d2016/f2016" ),
		JA_MOD( "@/d2017/" ),
		JA_MOD( "@/d2017/f2017" ),
		JA_MOD( "@/d2018/" ),
		JA_MOD( "@/d2018/f2018" ),
		JA_MOD( "@/d2019/" ),
		JA_MOD( "@/d2019/f2019" ),
		JA_MOD( "@/d2020/" ),
		JA_MOD( "@/d2020/f2020" ),


		JA_REN( "@/d1003Ren/", "d1003" ),
		JA_REN( "@/d1004Ren/", "d1004" ),

		JA_MOD( "@/d1009/" ),
		JA_REN( "@/d1009/f1009Ren", "f1009" ),

		JA_MOD_REN( "@/d1010Ren/",         "d1010" ),		// dir is modified because stuff within it changed too.
		JA_REN(     "@/d1010Ren/f1010Ren", "f1010" ),
		JA_MOD_REN( "@/d1011Ren/",         "d1011" ),
		JA_REN(     "@/d1011Ren/f1011Ren", "f1011" ),
		JA_MOD_REN( "@/d1012Ren/",         "d1012" ),
		JA_REN(     "@/d1012Ren/f1012Ren", "f1012" ),
		JA_MOD_REN( "@/d1013Ren/",         "d1013" ),
		JA_REN(     "@/d1013Ren/f1013Ren", "f1013" ),
		JA_MOD_REN( "@/d1014Ren/",         "d1014" ),
		JA_REN(     "@/d1014Ren/f1014Ren", "f1014" ),

		JA_MOD_REN( "@/d1016Ren/",         "d1016" ),
		JA_REN(     "@/d1016Ren/f1016Ren", "f1016" ),
		JA_MOD_REN( "@/d1017Ren/",         "d1017" ),
		JA_REN(     "@/d1017Ren/f1017Ren", "f1017" ),
		JA_MOD_REN( "@/d1018Ren/",         "d1018" ),
		JA_REN(     "@/d1018Ren/f1018Ren", "f1018" ),
		JA_MOD_REN( "@/d1019Ren/",         "d1019" ),
		JA_REN(     "@/d1019Ren/f1019Ren", "f1019" ),
		JA_MOD_REN( "@/d1020Ren/",         "d1020" ),
		JA_REN(     "@/d1020Ren/f1020Ren", "f1020" ),


		JA_MOD( "@/d3004/" ),
		JA_MOD( "@/d3004Mov/" ),
		JA_MOV( "@/d3004Mov/f3004", "@/d3004/" ),
		JA_MOD( "@/d3005/" ),
		JA_MOD( "@/d3005Mov/" ),
		JA_MOV( "@/d3005Mov/f3005", "@/d3005/" ),
		JA_MOD( "@/d3006/" ),
		JA_MOD( "@/d3006Mov/" ),
		JA_MOV( "@/d3006Mov/f3006", "@/d3006/" ),
		JA_MOD( "@/d3007/" ),
		JA_MOD( "@/d3007Mov/" ),
		JA_MOV( "@/d3007Mov/f3007", "@/d3007/" ),
		JA_MOD( "@/d3008/" ),
		JA_MOD( "@/d3008Mov/" ),
		JA_MOV( "@/d3008Mov/f3008", "@/d3008/" ),
		JA_MOD( "@/d3009/" ),
		JA_MOD( "@/d3009Mov/" ),
		JA_MOV( "@/d3009Mov/f3009", "@/d3009/" ),

#if AA
		JA_MOD( "@/a4003/" ),
		JA(     "@/a4003/f4003", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		JA_MOD( "@/a4004/" ),
		JA(     "@/a4004/f4004", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		JA( "@/a4005/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		JA( "@/a4006/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		JA( "@/a4007/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		JA( "@/a4008/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		JA( "@/a4009/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		JA_MOD( "@/a4011/" ),
		JA(     "@/a4011/f4011", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),
		JA_MOD( "@/a4012/" ),
		JA(     "@/a4012/f4012", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),
		JA_MOD( "@/a4013/" ),
		JA(     "@/a4013/f4013", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),
		JA_MOD( "@/a4014/" ),
		JA(     "@/a4014/f4014", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),

		JA_MOD( "@/a4018/" ),
		JA(     "@/a4018/f4018", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		JA_MOD( "@/a4019/" ),
		JA(     "@/a4019/f4019", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),

		JA_MOD( "@/a5000/" ),
		JA(     "@/a5000/f5000Ren", "f5000", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		JA_MOD( "@/a5001/" ),
		JA(     "@/a5001/f5001Ren", "f5001", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		JA_MOD( "@/a5002/" ),
		JA(     "@/a5002/f5002Ren", "f5002", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		JA_MOD( "@/a5003/" ),
		JA(     "@/a5003/f5003Ren", "f5003", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,                                       SG_TRUE,  SG_FALSE ),
		JA_MOD( "@/a5004/" ),
		JA(     "@/a5004/f5004Ren", "f5004", NULL, SG_DIFFSTATUS_FLAGS__RENAMED                                         | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE  ),

		JA_MOD( "@/a5005/"    ),
		JA_MOD( "@/a5005Mov/" ),
		JA(     "@/a5005Mov/f5005", NULL, "@/a5005/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		JA_MOD( "@/a5006/"    ),
		JA_MOD( "@/a5006Mov/" ),
		JA(     "@/a5006Mov/f5006", NULL, "@/a5006/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		JA_MOD( "@/a5007/"    ),
		JA_MOD( "@/a5007Mov/" ),
		JA(     "@/a5007Mov/f5007", NULL, "@/a5007/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		JA_MOD( "@/a5008/"    ),
		JA_MOD( "@/a5008Mov/" ),
		JA(     "@/a5008Mov/f5008", NULL, "@/a5008/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,                                       SG_TRUE,  SG_FALSE ),
		JA_MOD( "@/a5009/"    ),
		JA_MOD( "@/a5009Mov/" ),
		JA(     "@/a5009Mov/f5009", NULL, "@/a5009/", SG_DIFFSTATUS_FLAGS__MOVED                                         | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE  ),

		JA_MOD( "@/a5010/"    ),
		JA_MOD( "@/a5010Mov/" ),
		JA(     "@/a5010Mov/f5010Ren", "f5010", "@/a5010/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		JA_MOD( "@/a5011/"    ),
		JA_MOD( "@/a5011Mov/" ),
		JA(     "@/a5011Mov/f5011Ren", "f5011", "@/a5011/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		JA_MOD( "@/a5012/"    ),
		JA_MOD( "@/a5012Mov/" ),
		JA(     "@/a5012Mov/f5012Ren", "f5012", "@/a5012/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		JA_MOD( "@/a5013/"    ),
		JA_MOD( "@/a5013Mov/" ),
		JA(     "@/a5013Mov/f5013Ren", "f5013", "@/a5013/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		JA_MOD( "@/a5014/"    ),
		JA_MOD( "@/a5014Mov/" ),
		JA(     "@/a5014Mov/f5014Ren", "f5014", "@/a5014/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
#endif

		JA_MOD( "@/d6000/"    ),
		JA_MOD( "@/d6000Mov/" ),
		JA(     "@/d6000Mov/f6000Ren", "f6000", "@/d6000/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		JA_MOD( "@/d6001/"    ),
		JA_MOD( "@/d6001Mov/" ),
		JA(     "@/d6001Mov/f6001Ren", "f6001", "@/d6001/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		JA_MOD( "@/d6002/"    ),
		JA_MOD( "@/d6002Mov/" ),
		JA(     "@/d6002Mov/f6002Ren", "f6002", "@/d6002/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		JA_MOD( "@/d6003/"    ),
		JA_MOD( "@/d6003Mov/" ),
		JA(     "@/d6003Mov/f6003Ren", "f6003", "@/d6003/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		JA_MOD( "@/d6004/"    ),
		JA_MOD( "@/d6004Mov/" ),
		JA(     "@/d6004Mov/f6004Ren", "f6004", "@/d6004/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),

#if AA
		JA_MOD( "@/a7000/" ),
		JA(     "@/a7000/f7000Ren", "f7000", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		JA_MOD( "@/a7001/" ),
		JA(     "@/a7001/f7001Ren", "f7001", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		JA_MOD( "@/a7002/" ),
		JA(     "@/a7002/f7002Ren", "f7002", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		JA_MOD( "@/a7003/" ),
		JA(     "@/a7003/f7003Ren", "f7003", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,                                       SG_TRUE, SG_FALSE ),
		JA_MOD( "@/a7004/" ),
		JA(     "@/a7004/f7004Ren", "f7004", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED                                         | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		JA_MOD( "@/a7005/" ),
		JA_MOD( "@/a7005Mov/" ),
		JA(     "@/a7005Mov/f7005", NULL, "@/a7005/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		JA_MOD( "@/a7006/" ),
		JA_MOD( "@/a7006Mov/" ),
		JA(     "@/a7006Mov/f7006", NULL, "@/a7006/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		JA_MOD( "@/a7007/" ),
		JA_MOD( "@/a7007Mov/" ),
		JA(     "@/a7007Mov/f7007", NULL, "@/a7007/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		JA_MOD( "@/a7008/" ),
		JA_MOD( "@/a7008Mov/" ),
		JA(     "@/a7008Mov/f7008", NULL, "@/a7008/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,                                       SG_TRUE, SG_FALSE ),
		JA_MOD( "@/a7009/" ),
		JA_MOD( "@/a7009Mov/" ),
		JA(     "@/a7009Mov/f7009", NULL, "@/a7009/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED                                         | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		JA_MOD( "@/a7010/" ),
		JA_MOD( "@/a7010Mov/" ),
		JA(     "@/a7010Mov/f7010Ren", "f7010", "@/a7010/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		JA_MOD( "@/a7011/" ),
		JA_MOD( "@/a7011Mov/" ),
		JA(     "@/a7011Mov/f7011Ren", "f7011", "@/a7011/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		JA_MOD( "@/a7012/" ),
		JA_MOD( "@/a7012Mov/" ),
		JA(     "@/a7012Mov/f7012Ren", "f7012", "@/a7012/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		JA_MOD( "@/a7013/" ),
		JA_MOD( "@/a7013Mov/" ),
		JA(     "@/a7013Mov/f7013Ren", "f7013", "@/a7013/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		JA_MOD( "@/a7014/" ),
		JA_MOD( "@/a7014Mov/" ),
		JA(     "@/a7014Mov/f7014Ren", "f7014", "@/a7014/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
#endif

		JA_MOD( "@/d8000/" ),
		JA_MOD( "@/d8000Mov/" ),
		JA(     "@/d8000Mov/f8000Ren", "f8000", "@/d8000/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		JA_MOD( "@/d8001/" ),
		JA_MOD( "@/d8001Mov/" ),
		JA(     "@/d8001Mov/f8001Ren", "f8001", "@/d8001/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		JA_MOD( "@/d8002/" ),
		JA_MOD( "@/d8002Mov/" ),
		JA(     "@/d8002Mov/f8002Ren", "f8002", "@/d8002/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		JA_MOD( "@/d8003/" ),
		JA_MOD( "@/d8003Mov/" ),
		JA(     "@/d8003Mov/f8003Ren", "f8003", "@/d8003/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		JA_MOD( "@/d8004/" ),
		JA_MOD( "@/d8004Mov/" ),
		JA(     "@/d8004Mov/f8004Ren", "f8004", "@/d8004/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),

	};

#undef JA
#undef JA_ADD
#undef JA_DEL
#undef JA_REN
#undef JA_MOV
#undef JA_MOD_REN
#undef JA_MOD_MOV	

	SG_uint32 nrRowsJA = SG_NrElements(aRowJA);

	//////////////////////////////////////////////////////////////////

	// create random GID <gid> to be the base of the working dir within the directory given.
	// <wd> := <top>/<gid>
	// mkdir <wd>

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	//////////////////////////////////////////////////////////////////
	// begin <cset0>
	// 
	// create a new repo.  let the pendingtree code set up the initial working directory
	// and do the initial commit.  fetch the baseline and call it <cset0>.

	INFO2("arbitrary cset vs working directory", SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_0,sizeof(bufCSet_0))  );

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );

	// end of <cset0>
	//////////////////////////////////////////////////////////////////
	// begin <cset1>
	//
	// begin modifying the working directory with the contents in aRowX.
	// this is a simple series of ADDs so that we have some folders/files
	// to play with.

	INFOP("begin_building_x",("begin_building_x"));
	for (kRow=0; kRow < nrRowsX; kRow++)
	{
		struct _row_x * pRowX = &aRowX[kRow];

		// mkdir <wd>/<dir_k>
		// create file <wd>/<dir_k>/<file_k>
		VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowX->pPathDir,pPathWorkingDir,pRowX->szDir)  );
		VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pRowX->pPathDir)  );
		if (pRowX->szFile)
			VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pRowX->pPathDir,pRowX->szFile,20)  );
	}

	// cause scan and update of pendingtree vfile.

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	// verify that the pendingtree contains everything that we think it should (and nothing that it shouldn't).

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStr_check)  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree, NULL,&pTreeDiff_check)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	for (kRow=0; kRow < nrRowsX; kRow++)
	{
		struct _row_x * pRowX = &aRowX[kRow];

		VERIFY_ERR_CHECK(  SG_string__sprintf(pCtx, pStr_check,"@/%s/",pRowX->szDir)  );
		VERIFY_ERR_CHECK(  SG_treediff2__find_by_repo_path(pCtx, pTreeDiff_check,SG_string__sz(pStr_check),&bFound,&szGidObject_check,&pOD_opaque_check)  );
		VERIFYP_COND("Row X Dir check",(bFound),("RowX[%d] [repo-path %s] not found in pendingtree as expected.",kRow,SG_string__sz(pStr_check)));
		if (bFound)
		{
			VERIFY_ERR_CHECK(  SG_treediff2__set_mark(pCtx, pTreeDiff_check,szGidObject_check,1,NULL)  );
			VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD_opaque_check,&dsFlags_check)  );
			VERIFYP_COND("Row X Dir check",(dsFlags_check == SG_DIFFSTATUS_FLAGS__ADDED),("RowX[%d] [repo-path %s] [dsFlags %x] not expected.",
																					 kRow,SG_string__sz(pStr_check),dsFlags_check));
		}
		if (pRowX->szFile)
		{
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pStr_check,pRowX->szFile)  );
			VERIFY_ERR_CHECK(  SG_treediff2__find_by_repo_path(pCtx, pTreeDiff_check,SG_string__sz(pStr_check),&bFound,&szGidObject_check,&pOD_opaque_check)  );
			VERIFYP_COND("Row X File check",(bFound),("RowX[%d] [repo-path %s] not found in pendingtree as expected.",kRow,SG_string__sz(pStr_check)));
			if (bFound)
			{
				VERIFY_ERR_CHECK(  SG_treediff2__set_mark(pCtx, pTreeDiff_check,szGidObject_check,1,NULL)  );
				VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD_opaque_check,&dsFlags_check)  );
				VERIFYP_COND("Row X File check",(dsFlags_check == SG_DIFFSTATUS_FLAGS__ADDED),("RowX[%d] [repo-path %s] [dsFlags %x] not expected.",
																						  kRow,SG_string__sz(pStr_check),dsFlags_check));
			}
		}
		SG_ERR_IGNORE(  SG_string__clear(pCtx, pStr_check)  );
	}
	VERIFY_ERR_CHECK(  SG_treediff2__find_by_repo_path(pCtx, pTreeDiff_check,"@/",&bFound,&szGidObject_check,NULL)  );
	VERIFYP_COND("Row X File check",(bFound),("[repo-path @/] not found in pendingtree as expected."));
	VERIFY_ERR_CHECK(  SG_treediff2__set_mark(pCtx, pTreeDiff_check,szGidObject_check,1,NULL)  );
	VERIFY_ERR_CHECK_DISCARD(  u0043_pendingtree_text__check_treediff_markers(pCtx, pTreeDiff_check,"before_commit_x")  );
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_check);
	SG_STRING_NULLFREE(pCtx, pStr_check);

	// Commit everything we have created in the file system.

	INFOP("commit_x",("commit_x"));
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	// Let this first commit be <cset1>.  This will serve as a starting point for
	// everything else (rather than starting from an empty repo).

	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_1,sizeof(bufCSet_1))  );

	// TODO load a tree-diff <cset0> vs <cset1>.  this should show a series of adds.
	// TODO verify the contents match our expectation from the aRowX table.

	// end of <cset1>
	//////////////////////////////////////////////////////////////////
	// begin <cset2>
	// 
	// create <cset2> by building upon <cset1>.  This contains a representative sample
	// of all of the operations and combinations of operations possible.

	INFOP("begin_building_y",("begin_building_y"));
	for (kRow=0; kRow < nrRowsY; kRow++)
	{
		struct _row_y * pRowY = &aRowY[kRow];

		// build <wd>/<dir>         := <wd>/<dir_k[1]>
		// and   <wd>/<dir>/<file>  := <wd>/<dir_k[1]>/<file_k[1]>

		VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowY->pPathDir,pPathWorkingDir,pRowY->szDir1)  );
		if (pRowY->szFile1)
			VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowY->pPathFile,pRowY->pPathDir,pRowY->szFile1)  );

		INFOP("arbitrary cset vs working directory",("Y kRow[%d]: %s %s",kRow,pRowY->szDir1,((pRowY->szFile1) ? pRowY->szFile1 : "(null)")));

		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__ADDED)
		{
			// mkdir <wd>/<dir>
			// optionally create file <wd>/<dir>/<file_k[1]>

			VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pRowY->pPathDir)  );
			if (pRowY->szFile1)
				VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pRowY->pPathDir,pRowY->szFile1,20)  );
		}

		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__DELETED)
		{
			if (pRowY->szFile1)
			{
				// delete <wd>/<dir>/<file_k[1]>

				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree,pRowY->pPathDir,1,&pRowY->szFile1, NULL, 0, NULL, 0, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
			}
			else
			{
				// rmdir <wd>/<dir>
				// TODO does this need to be empty?

				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree,pPathWorkingDir,1,&pRowY->szDir1, NULL, 0, NULL, 0, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
			}
		}

		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__MODIFIED)
		{
			// append data to <wd>/<dir>/<file_k[1]>

			VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pRowY->pPathDir,pRowY->szFile1,4)  );
		}

		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__RENAMED)
		{
			if (pRowY->szFile1)
			{
				// rename <wd>/<dir>/<file> --> <wd>/<dir>/<file_k[2]>
				// let    <wd>/<dir>/<file>  := <wd>/<dir>/<file_k[2]>

				SG_ASSERT( pRowY->szFile2 );
				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree,pRowY->pPathFile,pRowY->szFile2, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
				SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathFile);
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowY->pPathFile,pRowY->pPathDir,pRowY->szFile2)  );
			}
			else
			{
				// rename <wd>/<dir> --> <wd>/<dir_k[2]>
				// let    <wd>/<dir>  := <wd>/<dir_k[2]>

				SG_ASSERT( pRowY->szDir2 );
				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree,pRowY->pPathDir,pRowY->szDir2, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
				SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathDir);
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowY->pPathDir,pPathWorkingDir,pRowY->szDir2)  );
			}
		}

		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__MOVED)
		{
			if (pRowY->szFile1)
			{
				// move <wd>/<dir>/<file> --> <wd>/<dir_k[2]>
				// let  <wd>/<dir>         := <wd>/<dir_k[2]>
				// let  <wd>/<dir>/<file>  := <wd>/<dir_k[2]>/<file_k[?]>

				SG_ASSERT( pRowY->szDir2 );
				SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathDir);
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowY->pPathDir,pPathWorkingDir,pRowY->szDir2)  );

				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree,(const SG_pathname **)&(pRowY->pPathFile), 1,pRowY->pPathDir, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

				VERIFY_ERR_CHECK(  SG_pathname__get_last(pCtx, pRowY->pPathFile,&pStrEntryName)  );
				SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathFile);
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowY->pPathFile,pRowY->pPathDir,SG_string__sz(pStrEntryName))  );
				SG_STRING_NULLFREE(pCtx, pStrEntryName);
			}
			else
			{
				// move a directory.  we assume that both <dir_k[1]> and <dir_k[2]> are relative pathnames
				// and that the first is the directory to be moved and the second is the path of the parent
				// directory.  for example "a/b/c" and "d/e/f" will put "c" inside "f".

				SG_ASSERT( pRowY->szDir2 );
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathTemp,pPathWorkingDir,pRowY->szDir2)  );

				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree,(const SG_pathname **)&(pRowY->pPathDir), 1, pPathTemp, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

				VERIFY_ERR_CHECK(  SG_pathname__get_last(pCtx, pRowY->pPathDir,&pStrEntryName)  );
				SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathDir);
				VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPathTemp,SG_string__sz(pStrEntryName))  );
				SG_STRING_NULLFREE(pCtx, pStrEntryName);

				SG_pathname__free(pCtx, pRowY->pPathDir);
				pRowY->pPathDir = pPathTemp;
				pPathTemp = NULL;
			}
		}
		
#if AA
		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)
		{
			if (pRowY->szFile1)
				VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pRowY->pPathFile,"com.sourcegear.test1",2,(SG_byte *)"Y")  );
			else
				VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pRowY->pPathDir,"com.sourcegear.test1",2,(SG_byte *)"Y")  );
		}

		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)
		{
			if (pRowY->szFile1)
			{
				SG_fsobj_stat st;

				VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pRowY->pPathFile,&st)  );
				if (st.perms & S_IXUSR)
					st.perms &= ~S_IXUSR;
				else
					st.perms |= S_IXUSR;
				VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pRowY->pPathFile,st.perms)  );
			}
			else
			{
				// TODO chmod a directory.  we currently only define the +x bit on files.

				SG_ASSERT( 0 );
			}
		}
#endif
	}

	// cause scan and update of pendingtree vfile.

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	// verify that the pendingtree contains everything that we think it should (and nothing that it shouldn't).
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,NULL,&pTreeDiff_check)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	for (kRow=0; kRow < nrRowsYA; kRow++)
	{
		struct _row_y_answer * pRowYA = &aRowYA[kRow];

		VERIFY_ERR_CHECK(  SG_treediff2__find_by_repo_path(pCtx, pTreeDiff_check,pRowYA->szRepoPath,&bFound,&szGidObject_check,&pOD_opaque_check)  );
		VERIFYP_COND("Row Y check",(bFound),("RowYA[%d] [repo-path %s] not found in pendingtree as expected.",kRow,pRowYA->szRepoPath));
		if (bFound)
		{
			VERIFY_ERR_CHECK(  SG_treediff2__set_mark(pCtx, pTreeDiff_check,szGidObject_check,1,NULL)  );
			VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD_opaque_check,&dsFlags_check)  );
			VERIFYP_COND("Row Y check",(dsFlags_check == pRowYA->dsFlags),("RowYA[%d] [repo-path %s] [dsFlags %x] not expected [expected %x].",
																	  kRow,pRowYA->szRepoPath,dsFlags_check,pRowYA->dsFlags));

			VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_attrbits(pCtx, pOD_opaque_check,&attrbits_check)  );
			VERIFYP_COND("Row Y check",((attrbits_check != 0) == pRowYA->bAttrBitsNonZero),
					("RowYA[%d] [repo-path %s] [attrbits %d] expected [%s].",
					 kRow,pRowYA->szRepoPath,attrbits_check,((pRowYA->bAttrBitsNonZero) ? "non zero" : "zero")));

			VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_xattrs(pCtx, pOD_opaque_check,&szHidXAttrs_check)  );
			VERIFYP_COND("Row Y check",((szHidXAttrs_check != NULL) == pRowYA->bXAttrsNonZero),
					("RowYA[%d] [repo-path %s] [xattrs %s] expected [%s].",
					 kRow,pRowYA->szRepoPath,szHidXAttrs_check,((pRowYA->bXAttrsNonZero) ? "non zero" : "zero")));

			if (pRowYA->dsFlags & SG_DIFFSTATUS_FLAGS__RENAMED)
			{
				VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, pOD_opaque_check,&szOldName_check)  );
				VERIFYP_COND("Row Y check",(strcmp(szOldName_check,pRowYA->szOldName_when_rename)==0),
						("RowYA[%d] [repo-path %s] [oldname %s] expected [%s].",
						 kRow,pRowYA->szRepoPath,szOldName_check,pRowYA->szOldName_when_rename));
			}

			if (pRowYA->dsFlags & SG_DIFFSTATUS_FLAGS__MOVED)
			{
				VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_moved_from_repo_path(pCtx, pTreeDiff_check,pOD_opaque_check,&szOldParentRepoPath_check)  );
				VERIFYP_COND("Row Y check",(strcmp(szOldParentRepoPath_check,pRowYA->szOldParentRepoPath_when_moved)==0),
						("RowYA[%d] [repo-path %s] [old parent %s] expected [%s].",
						 kRow,pRowYA->szRepoPath,szOldParentRepoPath_check,pRowYA->szOldParentRepoPath_when_moved));
			}
		}
	}
	VERIFY_ERR_CHECK_DISCARD(  u0043_pendingtree_text__check_treediff_markers(pCtx, pTreeDiff_check,"before_commit_y")  );
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_check);

	// Commit everything we have created/modified in the file system.

	INFOP("commit_y",("commit_y"));
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	// Let this second commit be <cset2>.  This will become the baseline
	// and be used to do a treediff-status( <cset1> vs <cset2> ) and
	// then to do a pendingtree-status( <cset2> vs <working-directory> )
	// so that we can join them.

	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_2,sizeof(bufCSet_2))  );

	// TODO do a tree-diff of <cset1> vs <cset2>.  this should be a
	// series of lots of things.  and verify the contents match our
	// expectations from the aRowY table.

	// end of <cset2>
	//////////////////////////////////////////////////////////////////
	// begin <cset3>
	// 
	// populate the working directory with a bunch of changes (relative
	// to <cset2>) so that we can do a JOIN.

	for (kRow=0; kRow < nrRowsZ; kRow++)
	{
		struct _row_z * pRowZ = &aRowZ[kRow];

		// build <wd>/<dir>         := <wd>/<dir_k[1]>
		// and   <wd>/<dir>/<file>  := <wd>/<dir_k[1]>/<file_k[1]>

		VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowZ->pPathDir,pPathWorkingDir,pRowZ->szDir1)  );
		if (pRowZ->szFile1)
			VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowZ->pPathFile,pRowZ->pPathDir,pRowZ->szFile1)  );

		INFOP("arbitrary cset vs working directory",("Z kRow[%d]: %s %s",kRow,pRowZ->szDir1,((pRowZ->szFile1) ? pRowZ->szFile1 : "(null)")));

		if (pRowZ->dsFlags & SG_DIFFSTATUS_FLAGS__ADDED)
		{
			// mkdir <wd>/<dir>
			// optionally create file <wd>/<dir>/<file_k[1]>

			VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pRowZ->pPathDir)  );
			if (pRowZ->szFile1)
				VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pRowZ->pPathDir,pRowZ->szFile1,20)  );
		}

		if (pRowZ->dsFlags & SG_DIFFSTATUS_FLAGS__DELETED)
		{
			if (pRowZ->szFile1)
			{
				// delete <wd>/<dir>/<file_k[1]>

				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree,pRowZ->pPathDir,1,&pRowZ->szFile1, NULL, 0, NULL, 0, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
			}
			else
			{
				// rmdir <wd>/<dir>
				// TODO does this need to be empty?

				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree,pPathWorkingDir,1,&pRowZ->szDir1, NULL, 0, NULL, 0, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
			}
		}

		if (pRowZ->dsFlags & SG_DIFFSTATUS_FLAGS__MODIFIED)
		{
#if defined(DEBUG)
			if (!pRowZ->szContentParam)
				SG_ASSERT( 0 );
#endif

			if (pRowZ->szContentParam[0] == '+')
			{
				// append 4 lines of data to <wd>/<dir>/<file_k[1]>

				VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pRowZ->pPathDir,pRowZ->szFile1,4)  );
			}
			else if (pRowZ->szContentParam[0] == 'u')
			{
				// delete and re-create the file so that it looks like an UNDO

				VERIFY_ERR_CHECK(  _ut_pt__delete_file(pCtx, pRowZ->pPathDir,pRowZ->szFile1)  );
				VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pRowZ->pPathDir,pRowZ->szFile1,20)  );
			}
#if defined(DEBUG)
			else
				SG_ASSERT( 0 );
#endif
		}

		if (pRowZ->dsFlags & SG_DIFFSTATUS_FLAGS__RENAMED)
		{
			if (pRowZ->szFile1)
			{
				// rename <wd>/<dir>/<file> --> <wd>/<dir>/<file_k[2]>
				// let    <wd>/<dir>/<file>  := <wd>/<dir>/<file_k[2]>

				SG_ASSERT( pRowZ->szFile2 );
				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree,pRowZ->pPathFile,pRowZ->szFile2, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
				SG_PATHNAME_NULLFREE(pCtx, pRowZ->pPathFile);
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowZ->pPathFile,pRowZ->pPathDir,pRowZ->szFile2)  );
			}
			else
			{
				// rename <wd>/<dir> --> <wd>/<dir_k[2]>
				// let    <wd>/<dir>  := <wd>/<dir_k[2]>

				SG_ASSERT( pRowZ->szDir2 );
				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree,pRowZ->pPathDir,pRowZ->szDir2, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
				SG_PATHNAME_NULLFREE(pCtx, pRowZ->pPathDir);
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowZ->pPathDir,pPathWorkingDir,pRowZ->szDir2)  );
			}
		}

		if (pRowZ->dsFlags & SG_DIFFSTATUS_FLAGS__MOVED)
		{
			if (pRowZ->szFile1)
			{
				// move <wd>/<dir>/<file> --> <wd>/<dir_k[2]>
				// let  <wd>/<dir>         := <wd>/<dir_k[2]>
				// let  <wd>/<dir>/<file>  := <wd>/<dir_k[2]>/<file_k[?]>

				SG_ASSERT( pRowZ->szDir2 );
				SG_PATHNAME_NULLFREE(pCtx, pRowZ->pPathDir);
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowZ->pPathDir,pPathWorkingDir,pRowZ->szDir2)  );

				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&(pRowZ->pPathFile), 1,pRowZ->pPathDir, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

				VERIFY_ERR_CHECK(  SG_pathname__get_last(pCtx, pRowZ->pPathFile,&pStrEntryName)  );
				SG_PATHNAME_NULLFREE(pCtx, pRowZ->pPathFile);
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowZ->pPathFile,pRowZ->pPathDir,SG_string__sz(pStrEntryName))  );
				SG_STRING_NULLFREE(pCtx, pStrEntryName);
			}
			else
			{
				// move a directory.  we assume that both <dir_k[1]> and <dir_k[2]> are relative pathnames
				// and that the first is the directory to be moved and the second is the path of the parent
				// directory.  for example "a/b/c" and "d/e/f" will put "c" inside "f".

				SG_ASSERT( pRowZ->szDir2 );
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathTemp,pPathWorkingDir,pRowZ->szDir2)  );

				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree,(const SG_pathname **)&(pRowZ->pPathDir), 1, pPathTemp, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

				VERIFY_ERR_CHECK(  SG_pathname__get_last(pCtx, pRowZ->pPathDir,&pStrEntryName)  );
				SG_PATHNAME_NULLFREE(pCtx, pRowZ->pPathDir);
				VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPathTemp,SG_string__sz(pStrEntryName))  );
				SG_STRING_NULLFREE(pCtx, pStrEntryName);

				SG_pathname__free(pCtx, pRowZ->pPathDir);
				pRowZ->pPathDir = pPathTemp;
				pPathTemp = NULL;
			}
		}
		
#if AA
		if (pRowZ->dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)
		{
#if defined(DEBUG)
			if (!pRowZ->szXAttrParam)
				SG_ASSERT( 0 );
#endif
			
			if (pRowZ->szXAttrParam[0] == '2')
			{
				// add "com.sourcegear.test2" XAttr to the set (we don't know whether .test1 is set or not)

				if (pRowZ->szFile1)
					VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pRowZ->pPathFile,"com.sourcegear.test2",2,(SG_byte *)"Z")  );
				else
					VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pRowZ->pPathDir,"com.sourcegear.test2",2,(SG_byte *)"Z")  );
			}
			else if (pRowZ->szXAttrParam[0] == '1')
			{
				// set "com.sourcegear.test1" XAttr with new value (it may or may not already be present, so this
				// may add it or replace the value).

				if (pRowZ->szFile1)
					VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pRowZ->pPathFile,"com.sourcegear.test1",2,(SG_byte *)"Z")  );
				else
					VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pRowZ->pPathDir,"com.sourcegear.test1",2,(SG_byte *)"Z")  );
			}
			else if (pRowZ->szXAttrParam[0] == 'x')
			{
				// remove all XAttrs (without regard to who set them)

				const SG_pathname * pPath = ((pRowZ->szFile1) ? pRowZ->pPathFile : pRowZ->pPathDir);

				SG_ERR_IGNORE(  SG_fsobj__xattr__remove(pCtx, pPath,"com.sourcegear.test1")  );	// ignore error incase this xattr name:value pair wasn't set.
				SG_ERR_IGNORE(  SG_fsobj__xattr__remove(pCtx, pPath,"com.sourcegear.test2")  );	// ignore error incase this xattr name:value pair wasn't set.
			}
		}

		if (pRowZ->dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)
		{
			if (pRowZ->szFile1)
			{
				SG_fsobj_stat st;

				VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pRowZ->pPathFile,&st)  );
				if (st.perms & S_IXUSR)
					st.perms &= ~S_IXUSR;
				else
					st.perms |= S_IXUSR;
				VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pRowZ->pPathFile,st.perms)  );
			}
			else
			{
				// TODO chmod a directory.  we currently only define the +x bit on files.

				SG_ASSERT( 0 );
			}
		}
#endif
	}

	// let __addremove convert "found" items to "added", so that we can
	// compare the pending tree status with the treediff status and not
	// have to worry about the differences.

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	// NOTE: we don't bother computing a pTreeDiff_check on baseline-vs-pendingtree and
	// NOTE: using a simple z-answer sheet.  we assume that we covered all of the bases
	// NOTE: using the simple y-answer sheet.

	// Compute JOIN/Composite tree-diff using cset1 vs pending-tree (cset2 is
	// implied between them).

	INFOP("commit_z",("Before commit_z -- computing cset1-vs-pendingtree"));
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,bufCSet_1,&pTreeDiff_join_before_commit)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"JoinBefore_TreeDiffStatus at %s:%d\n",__FILE__,__LINE__)  );
	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx, pTreeDiff_join_before_commit,SG_TRUE)  );

	INFOP("join",("JoinBefore_TreeDiff_Dump [cset %s] vs pendingtree",bufCSet_1));
	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx, pTreeDiff_join_before_commit,SG_FALSE)  );

	// verify that the joined treediff contains everything that we think it should (and nothing that it shouldn't).
	// rather than creating pTreeDiff_check, we just use pTreeDiff_join_before_commit.

	for (kRow=0; kRow < nrRowsJA; kRow++)
	{
		struct _row_join_answer * pRowJA = &aRowJA[kRow];

		VERIFY_ERR_CHECK(  SG_treediff2__find_by_repo_path(pCtx, pTreeDiff_join_before_commit,pRowJA->szRepoPath,&bFound,&szGidObject_check,&pOD_opaque_check)  );
		VERIFYP_COND("Row Join check",(bFound),("RowJA[%d] [repo-path %s] not found in pendingtree as expected.",kRow,pRowJA->szRepoPath));
		if (bFound)
		{
			VERIFY_ERR_CHECK(  SG_treediff2__set_mark(pCtx, pTreeDiff_join_before_commit,szGidObject_check,1,NULL)  );
			VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD_opaque_check,&dsFlags_check)  );
			VERIFYP_COND("Row Join check",(dsFlags_check == pRowJA->dsFlags),("RowJA[%d] [repo-path %s] [dsFlags %x] not expected [expected %x].",
																	  kRow,pRowJA->szRepoPath,dsFlags_check,pRowJA->dsFlags));

			VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_attrbits(pCtx, pOD_opaque_check,&attrbits_check)  );
			VERIFYP_COND("Row Join check",((attrbits_check != 0) == pRowJA->bAttrBitsNonZero),
					("RowJA[%d] [repo-path %s] [attrbits %d] expected [%s].",
					 kRow,pRowJA->szRepoPath,(SG_uint32)attrbits_check,((pRowJA->bAttrBitsNonZero) ? "non zero" : "zero")));

			VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_xattrs(pCtx, pOD_opaque_check,&szHidXAttrs_check)  );
			VERIFYP_COND("Row Join check",((szHidXAttrs_check != NULL) == pRowJA->bXAttrsNonZero),
					("RowJA[%d] [repo-path %s] [xattrs %s] expected [%s].",
					 kRow,pRowJA->szRepoPath,szHidXAttrs_check,((pRowJA->bXAttrsNonZero) ? "non zero" : "zero")));

			if (pRowJA->dsFlags & SG_DIFFSTATUS_FLAGS__RENAMED)
			{
				VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, pOD_opaque_check,&szOldName_check)  );
				VERIFYP_COND("Row Join check",(strcmp(szOldName_check,pRowJA->szOldName_when_rename)==0),
						("RowJA[%d] [repo-path %s] [oldname %s] expected [%s].",
						 kRow,pRowJA->szRepoPath,szOldName_check,pRowJA->szOldName_when_rename));
			}

			if (pRowJA->dsFlags & SG_DIFFSTATUS_FLAGS__MOVED)
			{
				VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_moved_from_repo_path(pCtx, pTreeDiff_join_before_commit,pOD_opaque_check,&szOldParentRepoPath_check)  );
				VERIFYP_COND("Row Join check",(strcmp(szOldParentRepoPath_check,pRowJA->szOldParentRepoPath_when_moved)==0),
						("RowJA[%d] [repo-path %s] [old parent %s] expected [%s].",
						 kRow,pRowJA->szRepoPath,szOldParentRepoPath_check,pRowJA->szOldParentRepoPath_when_moved));
			}
		}
	}
	VERIFY_ERR_CHECK_DISCARD(  u0043_pendingtree_text__check_treediff_markers(pCtx, pTreeDiff_join_before_commit,"join_before_commit")  );

	
	// Commit everything we have created/modified in the file system.

	INFOP("commit_z",("Before commit_z -- committing pendingtree to make cset3"));
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	INFOP("commit_z",("z_committed"));

	// Let this commit be <cset3>.
	//
	// Compute a <cset1> vs <cset3> status.  This is the net effect
	// of the 2 changes.  In theory, it should match what we computed
	// in the JOIN (excluding any lost/found items).

	INFOP("join",("Preparing to compute cset1-vs-cset3"));
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_3,sizeof(bufCSet_3))  );
	VERIFY_ERR_CHECK(  SG_treediff2__alloc(pCtx, pRepo,&pTreeDiff_join_after_commit)  );
	VERIFY_ERR_CHECK(  SG_treediff2__compare_cset_vs_cset(pCtx, pTreeDiff_join_after_commit,bufCSet_1,bufCSet_3)  );

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"JoinAfter_TreeDiffStatus at %s:%d\n",__FILE__,__LINE__)  );
	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx, pTreeDiff_join_after_commit,SG_TRUE)  );

	INFOP("join",("JoinAfter_TreeDiff_Dump [cset %s] vs [cset %s]",bufCSet_1,bufCSet_3));
	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx, pTreeDiff_join_after_commit,SG_FALSE)  );	

	// TODO consider running the JA answers again on the join-after-commit data.

	// verify that the baseline-vs-pendingtree and the old-baseline-vs-new-baseline
	// tree-diffs are equivalent....

	VERIFY_ERR_CHECK_DISCARD(  _ut_pt__compare_2_treediffs_for_equivalence(pCtx,
																					  pTreeDiff_join_before_commit,
																					  pTreeDiff_join_after_commit,
																					  SG_FALSE)  );


	// end of <cset3>
	//////////////////////////////////////////////////////////////////
	
	// fall thru to common cleanup.

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathTemp);
	SG_STRING_NULLFREE(pCtx, pStrEntryName);
	SG_VHASH_NULLFREE(pCtx, pvhManifest_1);
	SG_VHASH_NULLFREE(pCtx, pvhManifest_2);
	SG_VHASH_NULLFREE(pCtx, pvhManifest_3);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRING_NULLFREE(pCtx, pStringStatus01);
	SG_STRING_NULLFREE(pCtx, pStringStatus12);
	SG_STRING_NULLFREE(pCtx, pStringStatus13);
	SG_STRING_NULLFREE(pCtx, pStringStatus1wd);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_join_before_commit);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_join_after_commit);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_check);
	SG_STRING_NULLFREE(pCtx, pStr_check);

	for (kRow=0; kRow < nrRowsX; kRow++)
	{
		struct _row_x * pRowX = &aRowX[kRow];
		SG_PATHNAME_NULLFREE(pCtx, pRowX->pPathDir);
	}

	for (kRow=0; kRow < nrRowsY; kRow++)
	{
		struct _row_y * pRowY = &aRowY[kRow];
		SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathDir);
		SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathFile);
	}

	for (kRow=0; kRow < nrRowsZ; kRow++)
	{
		struct _row_z * pRowZ = &aRowZ[kRow];
		SG_PATHNAME_NULLFREE(pCtx, pRowZ->pPathDir);
		SG_PATHNAME_NULLFREE(pCtx, pRowZ->pPathFile);
	}
}

void u0043_pendingtree_test__share_treediff_cache1(SG_context * pCtx, SG_pathname * pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	char bufCSet_0[SG_HID_MAX_BUFFER_LENGTH];
	char bufCSet_1[SG_HID_MAX_BUFFER_LENGTH];
	SG_pathname * pPathWorkingDir = NULL;
	SG_pathname * pPathDir_1 = NULL;
	SG_repo* pRepo = NULL;
	SG_treediff2 * pTreeDiff_owner = NULL;
	SG_treediff2 * pTreeDiff_borrower = NULL;

	// create a treediff in the normal manner and then create a treediff that
	// shares the treenode cache from the first.  try various combinations
	// of alloc/free and ensure that our crude reference counting works.

	// create random GID <gid> to be the base of the working dir within the directory given.
	// <wd> := <top>/<gid>
	// mkdir <wd>

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	//////////////////////////////////////////////////////////////////
	// begin <cset0>
	// 
	// create a new repo.  let the pendingtree code set up the initial working directory
	// and do the initial commit.  fetch the baseline and call it <cset0>.

	INFO2("shared_treediff_cache1: working directory", SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_0,sizeof(bufCSet_0))  );

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );

	// end of <cset0>
	//////////////////////////////////////////////////////////////////
	// begin <cset1>

	// mkdir <wd>/dir_1
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir_1,pPathWorkingDir,"dir_1")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir_1)  );

	// create file <wd>/dir_1/file_1
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir_1,"file_1",20)  );

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	INFOP("shared_1",("shared_1"));
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	// Let this first commit be <cset1>.
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_1,sizeof(bufCSet_1))  );

	//////////////////////////////////////////////////////////////////

	// create first treediff -- this will be the owner of the cache.
	VERIFY_ERR_CHECK(  SG_treediff2__alloc(pCtx, pRepo,&pTreeDiff_owner)  );
	VERIFY_ERR_CHECK(  SG_treediff2__compare_cset_vs_cset(pCtx, pTreeDiff_owner,bufCSet_0,bufCSet_1)  );

	// create second treediff -- this will borrow the cache.  (note that this doesn't actually
	// contain any diff-data, we would need to run a compare.)
	VERIFY_ERR_CHECK(  SG_treediff2__alloc__shared(pCtx, pRepo,pTreeDiff_owner,&pTreeDiff_borrower)  );

	//////////////////////////////////////////////////////////////////

	// try to delete the owner -- this should fail because we still have a borrower.

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_treediff2__free_with_error(pCtx,pTreeDiff_owner,SG_FALSE),
										  SG_ERR_ASSERT  );		// shared_1 -- freeing cache owner should fail

	// try to delete the borrower and then the owner

	VERIFY_ERR_CHECK_DISCARD(  SG_treediff2__free_with_error(pCtx, pTreeDiff_borrower,SG_FALSE)  );		// "shared_1 -- freeing cache borrower"
	pTreeDiff_borrower = NULL;

	VERIFY_ERR_CHECK_DISCARD(  SG_treediff2__free_with_error(pCtx, pTreeDiff_owner,SG_FALSE)  );		// "shared_1 -- freeing cache owner"
	pTreeDiff_owner = NULL;

	//////////////////////////////////////////////////////////////////

	// fall thru to common cleanup.

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir_1);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_borrower);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_owner);
	SG_REPO_NULLFREE(pCtx, pRepo);
}

void u0043_pendingtree_test__share_treediff_cache2(SG_context * pCtx, SG_pathname * pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	char bufCSet_0[SG_HID_MAX_BUFFER_LENGTH];
	char bufCSet_1[SG_HID_MAX_BUFFER_LENGTH];
	SG_pathname * pPathWorkingDir = NULL;
	SG_pathname * pPathDir_1 = NULL;
	SG_repo* pRepo = NULL;
	SG_treediff2 * pTreeDiff_owner = NULL;
	SG_treediff2 * pTreeDiff_borrower = NULL;
	SG_pendingtree* pPendingTree = NULL;
	// create a treediff in the normal manner and then create a treediff (via the
	// pendingtree code) that shares the treenode cache from the first.  try various combinations
	// of alloc/free and ensure that our crude reference counting works.

	// create random GID <gid> to be the base of the working dir within the directory given.
	// <wd> := <top>/<gid>
	// mkdir <wd>

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	//////////////////////////////////////////////////////////////////
	// begin <cset0>
	// 
	// create a new repo.  let the pendingtree code set up the initial working directory
	// and do the initial commit.  fetch the baseline and call it <cset0>.

	INFO2("share_treediff_cache2: working directory", SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_0,sizeof(bufCSet_0))  );

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );

	// end of <cset0>
	//////////////////////////////////////////////////////////////////
	// begin <cset1>

	// mkdir <wd>/dir_1
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir_1,pPathWorkingDir,"dir_1")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir_1)  );

	// create file <wd>/dir_1/file_1
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir_1,"file_1",20)  );

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	INFOP("shared_1",("shared_1"));
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );

	// Let this first commit be <cset1>.
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_1,sizeof(bufCSet_1))  );

	//////////////////////////////////////////////////////////////////

	// create first treediff -- this will be the owner of the cache.
	VERIFY_ERR_CHECK(  SG_treediff2__alloc(pCtx, pRepo,&pTreeDiff_owner)  );
	VERIFY_ERR_CHECK(  SG_treediff2__compare_cset_vs_cset(pCtx, pTreeDiff_owner,bufCSet_0,bufCSet_1)  );

	// create second treediff -- this will borrow the cache.
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff__shared(pCtx, pPendingTree,bufCSet_0,pTreeDiff_owner,&pTreeDiff_borrower)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	//////////////////////////////////////////////////////////////////

	// try to delete the owner -- this should fail because we still have a borrower.

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_treediff2__free_with_error(pCtx, pTreeDiff_owner,SG_FALSE),
										  SG_ERR_ASSERT  );			// shared_1 -- freeing cache owner should fail

	// try to delete the borrower and then the owner

	VERIFY_ERR_CHECK_DISCARD(  SG_treediff2__free_with_error(pCtx,pTreeDiff_borrower,SG_FALSE)  );		// shared_1 -- freeing cache borrower
	pTreeDiff_borrower = NULL;

	VERIFY_ERR_CHECK_DISCARD(  SG_treediff2__free_with_error(pCtx,pTreeDiff_owner,SG_FALSE)  );			// shared_1 -- freeing cache owner
	pTreeDiff_owner = NULL;

	//////////////////////////////////////////////////////////////////

	// fall thru to common cleanup.

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir_1);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_borrower);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_owner);
	SG_REPO_NULLFREE(pCtx, pRepo);
}

void u0043_pendingtree_test__revert__rename_needs_swap(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_bool bExists = SG_FALSE;
	SG_uint64 len_a_orig = 0;
	SG_uint64 len_b_orig = 0;
	SG_uint64 len_now = 0;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );

	// create @/a.txt
	// create @/b.txt

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "b.txt", 25)  );

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

	/* get the original lengths of a and b for comparison later */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_a_orig, NULL)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "b.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_b_orig, NULL)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	//////////////////////////////////////////////////////////////////

	// rename a.txt --> swap.txt
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "swap.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	// rename b.txt --> a.txt
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "b.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "a.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	// rename swap.txt --> b.txt
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "swap.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "b.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After entries RENAMED.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/a.txt",   _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/b.txt",   _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@",       SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/a.txt", SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/b.txt", SG_DIFFSTATUS_FLAGS__RENAMED)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////

	/* Now revert */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	//////////////////////////////////////////////////////////////////

	/* verify that a.txt is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_a_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that b.txt is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "b.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_b_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After REVERT of RENAMED entries.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/a.txt",   _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/b.txt",   _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@",       SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/a.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/b.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);


fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);
}

void u0043_pendingtree_test__revert__move_needs_swap(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname * pPathDir_1 = NULL;
	SG_pathname * pPathDir_2 = NULL;
	SG_pathname * pPathDir_Swap = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_bool bExists = SG_FALSE;
	SG_uint64 len_1_orig = 0;
	SG_uint64 len_2_orig = 0;
	SG_uint64 len_now = 0;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );

	// mkdir @/dir_1
	// mkdir @/dir_2
	// mkdir @/dir_swap

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pPathDir_1,   pPathWorkingDir,"dir_1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pPathDir_2,   pPathWorkingDir,"dir_2")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pPathDir_Swap,pPathWorkingDir,"dir_swap")  );	
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx,pPathDir_1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx,pPathDir_2)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx,pPathDir_Swap)  );

	// create @/dir_1/f.txt
	// create @/dir_2/f.txt

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir_1, "f.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir_2, "f.txt", 25)  );

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

	/* get the original lengths of dir_1/f.txt and dir_2/f.txt for comparison later */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_1, "f.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_1_orig, NULL)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "f.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_2_orig, NULL)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	//////////////////////////////////////////////////////////////////

	// mv dir_1/f.txt --> dir_swap/
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_1, "f.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPathDir_Swap, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	// mv dir_2/f.txt --> dir_1/
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "f.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPathDir_1, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	// mv dir_swap/f.txt --> dir_2/
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_Swap, "f.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPathDir_2, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After entries MOVED.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@",                      _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/dir_1",                _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_1/f.txt",          _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/dir_1/f.txt", sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/dir_2",                _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_2/f.txt",          _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F><phantom>@/dir_2/f.txt", sg_PTNODE_SAVED_FLAG_MOVED_AWAY,0)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@",             SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dir_1",       SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_1/f.txt", SG_DIFFSTATUS_FLAGS__MOVED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dir_2",       SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_2/f.txt", SG_DIFFSTATUS_FLAGS__MOVED)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////

	/* Now revert */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	//////////////////////////////////////////////////////////////////

	/* verify that dir_1/f.txt is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_1, "f.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_1_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that dir_2/f.txt is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "f.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_2_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After REVERT of MOVED entries.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@",             _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/dir_1",       _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_1/f.txt", _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/dir_2",       _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_2/f.txt", _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@",             SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dir_1",       SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_1/f.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dir_2",       SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_2/f.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	SG_PATHNAME_NULLFREE(pCtx, pPathDir_1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir_2);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir_Swap);

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);
}

void u0043_pendingtree_test__revert__rename_needs_multilevel_swap(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathDir_1 = NULL;
	SG_pathname* pPathDir_2 = NULL;
	SG_pathname* pPathDir_Swap = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_bool bExists = SG_FALSE;
	SG_uint64 len_a_orig = 0;
	SG_uint64 len_b_orig = 0;
	SG_uint64 len_w_orig = 0;
	SG_uint64 len_x_orig = 0;
	SG_uint64 len_y_orig = 0;
	SG_uint64 len_z_orig = 0;
	SG_uint64 len_now = 0;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );

	// mkdir @/dir_1
	// mkdir @/dir_2

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pPathDir_1,   pPathWorkingDir,"dir_1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pPathDir_2,   pPathWorkingDir,"dir_2")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx,pPathDir_1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx,pPathDir_2)  );

	// create @/dir_1/a.txt
	// create @/dir_1/b.txt
	// create @/dir_2/w.txt
	// create @/dir_2/x.txt
	// create @/dir_2/y.txt
	// create @/dir_2/z.txt

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir_1, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir_1, "b.txt", 25)  );

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir_2, "w.txt", 30)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir_2, "x.txt", 40)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir_2, "y.txt", 50)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathDir_2, "z.txt", 60)  );

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

	/* get the original lengths of the files for comparison later. */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_1, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_a_orig, NULL)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_1, "b.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_b_orig, NULL)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "w.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_w_orig, NULL)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "x.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_x_orig, NULL)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "y.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_y_orig, NULL)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "z.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_z_orig, NULL)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	//////////////////////////////////////////////////////////////////
	// swap @/dir_1/a.txt <--> @/dir_1/b.txt (using a temporary @/dir_1/swap.ab.txt)
	// 
	// rename @/dir_1/a.txt --> @/dir_1/swap.ab.txt
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_1, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "swap.ab.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	// rename @/dir_1/b.txt --> @/dir_1/a.txt
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_1, "b.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "a.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	// rename @/dir_1/swap.ab.txt --> @/dir_1/b.txt
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_1, "swap.ab.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "b.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	//////////////////////////////////////////////////////////////////
	// do a chain of renames in @/dir_2
	// 
	// rename @/dir_2/z.txt --> @/dir_2/swap.wxyz.txt
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "z.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "swap.wxyz.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	// rename @/dir_2/y.txt --> @/dir_2/z.txt
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "y.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "z.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	// rename @/dir_2/x.txt --> @/dir_2/y.txt
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "x.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "y.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	// rename @/dir_2/w.txt --> @/dir_2/x.txt
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "w.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "x.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	// rename @/dir_2/swap.wxyz.txt --> @/dir_2/w.txt
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "swap.wxyz.txt")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "w.txt", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	//////////////////////////////////////////////////////////////////
	// now get mean and swap @/dir_1 and @/dir_2 (using @/dir.swap.12
	//
	// rename @/dir_1 --> @/dir.swap.12
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathDir_1, "dir.swap.12", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	// rename @/dir_2 --> @/dir_1
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathDir_2, "dir_1", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	// rename @/dir.swap.12 --> @/dir_2
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir_Swap, pPathWorkingDir, "dir.swap.12")  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathDir_Swap, "dir_2", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir_Swap);

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After some wicked renames.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/dir_2",                           _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_2/a.txt",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_2/b.txt",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/dir_1",                           _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_1/w.txt",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_1/x.txt",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_1/y.txt",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_1/z.txt",                     _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dir_2",       SG_DIFFSTATUS_FLAGS__RENAMED|SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_2/a.txt", SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_2/b.txt", SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dir_1",       SG_DIFFSTATUS_FLAGS__RENAMED|SG_DIFFSTATUS_FLAGS__MODIFIED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_1/w.txt", SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_1/x.txt", SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_1/y.txt", SG_DIFFSTATUS_FLAGS__RENAMED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_1/z.txt", SG_DIFFSTATUS_FLAGS__RENAMED)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////
	// revert everything.  verify that all of the files are back in their original places. (that is, have their original lengths.)

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	/* verify that each file is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_1, "a.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_a_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that b.txt is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_1, "b.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_b_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that w.txt is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "w.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_w_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that x.txt is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "x.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_x_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that y.txt is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "y.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_y_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	/* verify that z.txt is back in its original location and is the same size as before */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathDir_2, "z.txt")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len_now, NULL)  );
	VERIFY_COND("match", (len_now == len_z_orig));
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	//////////////////////////////////////////////////////////////////
	// verify that the pending tree matches our expectation after the revert is finished.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After REVERT completed.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/dir_1",                           _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_1/a.txt",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_1/b.txt",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/dir_2",                           _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_2/w.txt",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_2/x.txt",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_2/y.txt",                     _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/dir_2/z.txt",                     _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dir_1",       SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_1/a.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_1/b.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dir_2",       SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_2/w.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_2/x.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_2/y.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/dir_2/z.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir_1);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir_2);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir_Swap);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);
	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
}


/**
 * Create some files and directories with ATTRBITS and XATTRS on them.
 * Commit.
 * Delete them.
 * Revert.
 * Verify that the ATTRBITS and XATTRS get restored too.
 */
void u0043_pendingtree_test__revert__deletes_with_attrbits_and_xattrs(SG_context * pCtx, SG_pathname * pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_dx = NULL;
	SG_pathname* pPath_d1 = NULL;
	SG_pathname* pPath_d1_d2 = NULL;
	SG_pathname* pPath_d1_d2_d3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f1 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f2 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_f4 = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_bool bExists;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)	 );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)	);

	// mkdir @/dx
	// mkdir @/d1
	// mkdir @/d1/d2
	// mkdir @/d1/d2/d3
	// mkdir @/d1/d2/d3/d4
	// create @/d1/d2/d3/d4/f1
	// create @/d1/d2/d3/d4/f2
	// create @/d1/d2/d3/d4/f3
	
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_dx, pPathWorkingDir, "dx")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_dx)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1, pPathWorkingDir, "d1")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2, pPath_d1, "d2")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2)	 );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3, pPath_d1_d2, "d3")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3)	);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4, pPath_d1_d2_d3, "d4")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3_d4)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f1, pPath_d1_d2_d3_d4, "f1")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f1", 20)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f2, pPath_d1_d2_d3_d4, "f2")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f2", 25)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f3, pPath_d1_d2_d3_d4, "f3")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f3", 30)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4_f4, pPath_d1_d2_d3_d4, "f4")  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPath_d1_d2_d3_d4, "f4", 35)  );

#if defined(MAC) || defined(LINUX)
	{
		SG_fsobj_stat st;

		// chmod +x @/d1/d2/d3/d4/f1
		VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx,pPath_d1_d2_d3_d4_f1,&st)  );
		st.perms |= S_IXUSR;
		VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx,pPath_d1_d2_d3_d4_f1,st.perms)  );
	
		// chmod -x @/d1/d2/d3/d4/f2
		VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx,pPath_d1_d2_d3_d4_f2,&st)  );
		st.perms &= ~S_IXUSR;
		VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx,pPath_d1_d2_d3_d4_f2,st.perms)  );
	}
#endif
	
#ifdef SG_BUILD_FLAG_TEST_XATTR
	// add some XATTRs to @/d1/d2/d3/d4/f3
	VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx,pPath_d1_d2_d3_d4_f3,"com.sourcegear.test1",strlen("test 1 value"),(SG_byte *)"test 1 value")  );
	VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx,pPath_d1_d2_d3_d4_f3,"com.sourcegear.test2",strlen("test 2 value"),(SG_byte *)"test 2 value")  );

	// add some XATTRs to @/dx
	VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx,pPath_dx,"com.sourcegear.test1",strlen("dx test 1 value"),(SG_byte *)"dx test 1 value")  );
	VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx,pPath_dx,"com.sourcegear.test2",strlen("dx test 2 value"),(SG_byte *)"dx test 2 value")  );
#endif

	// commit all of this

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)	);
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)	 );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

	// delete @/dx
	// delete @/d1/d2/d3/d4/f1
	// delete @/d1/d2/d3/d4/f2
	// delete @/d1/d2/d3/d4/f3

	{
		const char * apszName[] = { "dx",
									"d1/d2/d3/d4/f1",
									"d1/d2/d3/d4/f2",
									"d1/d2/d3/d4/f3" };
		SG_uint32 nrNames = SG_NrElements(apszName);

		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, nrNames, apszName, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

	// verify the stuff is gone

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_dx, &bExists, NULL, NULL)	 );
	VERIFY_COND("delete of dx", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f1, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3/d4/f1", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f2, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3/d4/f2", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_f3, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3/d4/f3", (!bExists));

	//////////////////////////////////////////////////////////////////

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After explicity delete of files and a directory with ATTRBITS and XATTRS.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<d>@/dx",                     _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/d1/d2/d3/d4/f1",         _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/d1/d2/d3/d4/f2",         _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/d1/d2/d3/d4/f3",         _ut_pt__PTNODE_DELETED)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<d>@/dx",                     SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/d1/d2/d3/d4/f1",         SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/d1/d2/d3/d4/f2",         SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/d1/d2/d3/d4/f3",         SG_DIFFSTATUS_FLAGS__DELETED)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////
	// revert everything.

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	// verify that the pending tree matches our expectation after the revert is finished.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After REVERT completed.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@/dx",            _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f1",_ut_pt__PTNODE_ZERO)  );	// unchanged
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f2",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f3",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/d1/d2/d3/d4/f4",_ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@/dx",            SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f1",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f2",SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/d1/d2/d3/d4/f3",SG_DIFFSTATUS_FLAGS__ZERO)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////
	// verify that the restored files/directories have the ATTRBITS and XATTRS.

#if defined(MAC) || defined(LINUX)
	{
		SG_fsobj_stat st;

		// verify +x on @/d1/d2/d3/d4/f1
		VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx,pPath_d1_d2_d3_d4_f1,&st)  );
		VERIFYP_COND("+x after revert", (st.perms & S_IXUSR), ("Restored f1 without +x [%04o]",st.perms));
	
		// verify -x on @/d1/d2/d3/d4/f2
		VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx,pPath_d1_d2_d3_d4_f2,&st)  );
		VERIFYP_COND("-x after revert", (!(st.perms & S_IXUSR)), ("Restored f2 without -x [%04o]",st.perms));
	}
#endif

#ifdef SG_BUILD_FLAG_TEST_XATTR
	{
		SG_rbtree * prbXAttrs = NULL;
		SG_bool bFound;

		// verify we have XATTRs on @/d1/d2/d3/d4/f3
		VERIFY_ERR_CHECK_DISCARD(  SG_fsobj__xattr__get_all(pCtx,pPath_d1_d2_d3_d4_f3,NULL,&prbXAttrs)  );
		VERIFYP_COND("xattr_get_all",(prbXAttrs),("Fetching restored XATTRs on f3"));
		if (prbXAttrs)
		{
			VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx,prbXAttrs,"com.sourcegear.test1",&bFound,NULL)  );
			VERIFYP_COND("xattr_get_all",(bFound),("could not find .test1 on f3"));
			VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx,prbXAttrs,"com.sourcegear.test2",&bFound,NULL)  );
			VERIFYP_COND("xattr_get_all",(bFound),("could not find .test2 on f3"));
			SG_RBTREE_NULLFREE(pCtx, prbXAttrs);
		}
		
		// verify we have XATTRs on @/dx
		VERIFY_ERR_CHECK_DISCARD(  SG_fsobj__xattr__get_all(pCtx,pPath_dx,NULL,&prbXAttrs)  );
		VERIFYP_COND("xattr_get_all",(prbXAttrs),("Fetching restored XATTRs on dx"));
		if (prbXAttrs)
		{
			VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx,prbXAttrs,"com.sourcegear.test1",&bFound,NULL)  );
			VERIFYP_COND("xattr_get_all",(bFound),("could not find .test1 on dx"));
			VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx,prbXAttrs,"com.sourcegear.test2",&bFound,NULL)  );
			VERIFYP_COND("xattr_get_all",(bFound),("could not find .test2 on dx"));
			SG_RBTREE_NULLFREE(pCtx, prbXAttrs);
		}
	}
#endif

	//////////////////////////////////////////////////////////////////	

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPath_dx);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_f4);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);
	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
}

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
/**
 * Create some SYMLINKS.
 * Commit.
 * Delete them.
 * Revert.
 * Verify that the ATTRBITS and XATTRS get restored too.
 */
void u0043_pendingtree_test__revert__deletes_with_symlinks(SG_context * pCtx, SG_pathname * pPathTopDir, SG_bool bIgnoreBackups)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_d1 = NULL;
	SG_pathname* pPath_d1_d2 = NULL;
	SG_pathname* pPath_d1_d2_d3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_s1 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_s2 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_s3 = NULL;
	SG_pathname* pPath_d1_d2_d3_d4_s4 = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_string * pStringLink = NULL;
    SG_fsobj_type t;
	SG_bool bExists;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)	 );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo3(pCtx, bufName, pPathWorkingDir, bIgnoreBackups)  );

	// mkdir @/d1
	// mkdir @/d1/d2
	// mkdir @/d1/d2/d3
	// mkdir @/d1/d2/d3/d4
	// create @/d1/d2/d3/d4/s1 --> bogus_1
	// create @/d1/d2/d3/d4/s2 --> bogus_2
	// create @/d1/d2/d3/d4/s3 --> bogus_3 (optionally with XATTRS)
	// create @/d1/d2/d3/d4/s4 --> bogus_4
	
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1, pPathWorkingDir, "d1")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2, pPath_d1, "d2")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2)	 );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3, pPath_d1_d2, "d3")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3)	);

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1_d2_d3_d4, pPath_d1_d2_d3, "d4")	);
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1_d2_d3_d4)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,  &pPath_d1_d2_d3_d4_s1, pPath_d1_d2_d3_d4, "s1")  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__create_symlink(pCtx,                       pPath_d1_d2_d3_d4, "s1", "bogus_1")  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,  &pPath_d1_d2_d3_d4_s2, pPath_d1_d2_d3_d4, "s2")  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__create_symlink(pCtx,                       pPath_d1_d2_d3_d4, "s2", "bogus_2")  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,  &pPath_d1_d2_d3_d4_s3, pPath_d1_d2_d3_d4, "s3")  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__create_symlink(pCtx,                       pPath_d1_d2_d3_d4, "s3", "bogus_3")  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,  &pPath_d1_d2_d3_d4_s4, pPath_d1_d2_d3_d4, "s4")  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__create_symlink(pCtx,                       pPath_d1_d2_d3_d4, "s4", "bogus_4")  );

#ifdef SG_BUILD_FLAG_TEST_XATTR
	// add some XATTRs to @/d1/d2/d3/d4/s3
	VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx,pPath_d1_d2_d3_d4_s3,"com.sourcegear.test1",strlen("test 1 value"),(SG_byte *)"test 1 value")  );
	VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx,pPath_d1_d2_d3_d4_s3,"com.sourcegear.test2",strlen("test 2 value"),(SG_byte *)"test 2 value")  );
#endif

	// commit all of this

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)	);
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)	 );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

	// delete @/d1/d2/d3/d4/s1
	// delete @/d1/d2/d3/d4/s2
	// delete @/d1/d2/d3/d4/s3

	{
		const char * apszName[] = { "d1/d2/d3/d4/s1",
									"d1/d2/d3/d4/s2",
									"d1/d2/d3/d4/s3" };
		SG_uint32 nrNames = SG_NrElements(apszName);

		VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, nrNames, apszName, NULL, 0, NULL, 0, SG_FALSE)  );
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	}

	// verify the stuff is gone

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_s1, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3/d4/s1", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_s2, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3/d4/s2", (!bExists));

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_s3, &bExists, NULL, NULL)  );
	VERIFY_COND("delete of d1/d2/d3/d4/s3", (!bExists));

	// change the symlink target on s4 --> changed_4

	VERIFY_ERR_CHECK(  u0043_pendingtree__replace_symlink(pCtx, pPath_d1_d2_d3_d4, "s4", "changed_4")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_s4, &bExists, &t, NULL)  );
	VERIFY_COND("s4 after re-target",(bExists));
	VERIFY_COND("s4 type after re-target", (t == SG_FSOBJ_TYPE__SYMLINK));
	VERIFY_ERR_CHECK(  SG_fsobj__readlink(pCtx, pPath_d1_d2_d3_d4_s4, &pStringLink)  );
	VERIFYP_COND("s4 link after re-target", (strcmp(SG_string__sz(pStringLink),"changed_4") == 0),("Expected[changed_4] Observed[%s]",SG_string__sz(pStringLink)));
	SG_STRING_NULLFREE(pCtx,pStringLink);

	//////////////////////////////////////////////////////////////////

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After re-target and explicit delete of SYMLINKS and XATTRS.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<l>@/d1/d2/d3/d4/s1",         _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<l>@/d1/d2/d3/d4/s2",         _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<l>@/d1/d2/d3/d4/s3",         _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<L>@/d1/d2/d3/d4/s4",         _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<l>@/d1/d2/d3/d4/s1",         SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<l>@/d1/d2/d3/d4/s2",         SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<l>@/d1/d2/d3/d4/s3",         SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<L>@/d1/d2/d3/d4/s4",         SG_DIFFSTATUS_FLAGS__MODIFIED)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////
	// revert everything.

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	// verify that the pending tree matches our expectation after the revert is finished.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After REVERT completed.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<L>@/d1/d2/d3/d4/s1",_ut_pt__PTNODE_ZERO)  );	// unchanged
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<L>@/d1/d2/d3/d4/s2",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<L>@/d1/d2/d3/d4/s3",_ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<L>@/d1/d2/d3/d4/s4",_ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<L>@/d1/d2/d3/d4/s1",      SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<L>@/d1/d2/d3/d4/s2",      SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<L>@/d1/d2/d3/d4/s3",      SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<L>@/d1/d2/d3/d4/s4",      SG_DIFFSTATUS_FLAGS__ZERO)  );
	if (!bIgnoreBackups) VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<L>@/d1/d2/d3/d4/s4~sg00~",SG_DIFFSTATUS_FLAGS__FOUND)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////
	// verify symlinks are restored properly.

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_s1, &bExists, &t, NULL)  );
	VERIFY_COND("s1 after revert",(bExists));
	VERIFY_COND("s1 type after revert", (t == SG_FSOBJ_TYPE__SYMLINK));
	VERIFY_ERR_CHECK(  SG_fsobj__readlink(pCtx, pPath_d1_d2_d3_d4_s1, &pStringLink)  );
	VERIFYP_COND("s1 link after revert", (strcmp(SG_string__sz(pStringLink),"bogus_1") == 0),("Expected[bogus_1] Observed[%s]",SG_string__sz(pStringLink)));
	SG_STRING_NULLFREE(pCtx,pStringLink);

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_s2, &bExists, &t, NULL)  );
	VERIFY_COND("s2 after revert",(bExists));
	VERIFY_COND("s2 type after revert", (t == SG_FSOBJ_TYPE__SYMLINK));
	VERIFY_ERR_CHECK(  SG_fsobj__readlink(pCtx, pPath_d1_d2_d3_d4_s2, &pStringLink)  );
	VERIFYP_COND("s2 link after revert", (strcmp(SG_string__sz(pStringLink),"bogus_2") == 0),("Expected[bogus_2] Observed[%s]",SG_string__sz(pStringLink)));
	SG_STRING_NULLFREE(pCtx,pStringLink);

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_s3, &bExists, &t, NULL)  );
	VERIFY_COND("s3 after revert",(bExists));
	VERIFY_COND("s3 type after revert", (t == SG_FSOBJ_TYPE__SYMLINK));
	VERIFY_ERR_CHECK(  SG_fsobj__readlink(pCtx, pPath_d1_d2_d3_d4_s3, &pStringLink)  );
	VERIFYP_COND("s3 link after revert", (strcmp(SG_string__sz(pStringLink),"bogus_3") == 0),("Expected[bogus_3] Observed[%s]",SG_string__sz(pStringLink)));
	SG_STRING_NULLFREE(pCtx,pStringLink);

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_d1_d2_d3_d4_s4, &bExists, &t, NULL)  );
	VERIFY_COND("s4 after revert",(bExists));
	VERIFY_COND("s4 type after revert", (t == SG_FSOBJ_TYPE__SYMLINK));
	VERIFY_ERR_CHECK(  SG_fsobj__readlink(pCtx, pPath_d1_d2_d3_d4_s4, &pStringLink)  );
	VERIFYP_COND("s4 link after revert", (strcmp(SG_string__sz(pStringLink),"bogus_4") == 0),("Expected[bogus_4] Observed[%s]",SG_string__sz(pStringLink)));
	SG_STRING_NULLFREE(pCtx,pStringLink);

	//////////////////////////////////////////////////////////////////
	// verify the restored XATTRS.

#ifdef SG_BUILD_FLAG_TEST_XATTR
	{
		SG_rbtree * prbXAttrs = NULL;
		SG_bool bFound;

		// verify we have XATTRs on @/d1/d2/d3/d4/s3
		VERIFY_ERR_CHECK_DISCARD(  SG_fsobj__xattr__get_all(pCtx,pPath_d1_d2_d3_d4_s3,NULL,&prbXAttrs)  );
		VERIFYP_COND("xattr_get_all",(prbXAttrs),("Fetching restored XATTRs on s3"));
		if (prbXAttrs)
		{
			VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx,prbXAttrs,"com.sourcegear.test1",&bFound,NULL)  );
			VERIFYP_COND("xattr_get_all",(bFound),("could not find .test1 on s3"));
			VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx,prbXAttrs,"com.sourcegear.test2",&bFound,NULL)  );
			VERIFYP_COND("xattr_get_all",(bFound),("could not find .test2 on s3"));
			SG_RBTREE_NULLFREE(pCtx, prbXAttrs);
		}
	}
#endif

	//////////////////////////////////////////////////////////////////	

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_s1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_s2);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_s3);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1_d2_d3_d4_s4);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);
	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_STRING_NULLFREE(pCtx,pStringLink);
}
#endif

//////////////////////////////////////////////////////////////////

/**
 * create a WD and try to do a REVERT when there is nothing dirty.
 */
void u0043_pendingtree_test__revert__on_clean_wd(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_uint32 nrChangesInTreeDiff;
	
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );

	// create @/a.txt
	// create @/b.txt

	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pPathWorkingDir, "b.txt", 25)  );

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0043_pendingtree__commit_all(pCtx, pPathWorkingDir,SG_TRUE)  );
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"With Clean Working Directory.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/a.txt",   _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/b.txt",   _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  SG_treediff2__count_changes(pCtx,pTreeDiff,&nrChangesInTreeDiff)  );
	VERIFYP_COND("nrChangesInTreeDiff",(nrChangesInTreeDiff == 0),("Expected[0] Observed[%d]",nrChangesInTreeDiff));

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@",       SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/a.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/b.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////

	/* Now revert */
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, NULL, 0, NULL, SG_TRUE, NULL, 0, NULL, 0)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	
	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After REVERT on Clean Working Directory.")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<D>@",         _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/a.txt",   _ut_pt__PTNODE_ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/b.txt",   _ut_pt__PTNODE_ZERO)  );

//	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
//	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  SG_treediff2__count_changes(pCtx,pTreeDiff,&nrChangesInTreeDiff)  );
	VERIFYP_COND("nrChangesInTreeDiff",(nrChangesInTreeDiff == 0),("Expected[0] Observed[%d]",nrChangesInTreeDiff));

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<D>@",       SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/a.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/b.txt", SG_DIFFSTATUS_FLAGS__ZERO)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);


fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);
}

//////////////////////////////////////////////////////////////////

/* TODO need a case that tries to reference a path that has been deleted (in the pending changeset) */

/* TODO put the basic scan in the commit code and then test to make sure it happens */

/* TODO test: a dir with 2 changes, a move plus something else.
 * commit the something else but not the move.  make sure the treenode
 * properly gets the old entry from the thing that moved away. */

TEST_MAIN(u0043_pendingtree)
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;
	
	TEMPLATE_MAIN_START;

	VERIFY_ERR_CHECK(  SG_tid__generate2__suffix(pCtx, bufTopDir, sizeof(bufTopDir), 32, "u0043")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathTopDir,bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathTopDir)  );

#if 1
	{
		// a cheap test to bang on the timestamp cache.
		SG_uint32 k, kLimit;

		kLimit = 20;
		for (k=0; k<kLimit; k++)
			BEGIN_TEST(  u0043_pendingtree_test__unsafe_remove_directory__modified(pCtx, pPathTopDir)  );
	}
#endif

#if 1	// this is here to let me temporarily turn off the bulk of the tests as i am writing/testing new tests at the bottom.
	BEGIN_TEST(  u0043_pendingtree_test__rename_file_add_dir_with_same_name(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert__rename_needs_backup(pCtx, pPathTopDir, SG_FALSE)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert__rename_needs_backup(pCtx, pPathTopDir, SG_TRUE)  );
	BEGIN_TEST(  u0043_pendingtree_test__remove_directory(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__unsafe_remove_directory__modified(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__unsafe_remove_directory__found(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__simple_1_file(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__simple_multiple_files(pCtx, pPathTopDir, 10)  );
	BEGIN_TEST(  u0043_pendingtree_test__simple_multiple_files(pCtx, pPathTopDir, 100)  );
	BEGIN_TEST(  u0043_pendingtree_test__simple_1_file_removed(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__simple_two_subdirs_move(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__2_files_commit_only_1(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__2_files_commit_change_but_not_remove(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__simple_rename(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__commit_with_no_actual_changes(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__move_file_add_dir_with_same_name(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__attempt_commit_split_move(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__rename_conflict(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__simple_get(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__file_has_not_really_changed(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__rename_to_same_name_conflict(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__move_to_same_dir(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__move_to_nonexistent_dir(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__simple_add(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__add_but_no_file(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__add_same_file_twice(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__status(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__deep_rename_move_delete(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__unsafe_remove_file(pCtx, pPathTopDir)  );

#if defined(MAC) || defined(LINUX)
	BEGIN_TEST(  u0043_pendingtree_test__symlink(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__chmod(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert__deletes_with_symlinks(pCtx, pPathTopDir, SG_FALSE)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert__deletes_with_symlinks(pCtx, pPathTopDir, SG_TRUE)  );
#else
	INFOP("u0043_pendingtree",("skipping SYMLINK and CHMOD tests"));
#endif

#ifdef SG_BUILD_FLAG_TEST_XATTR
	BEGIN_TEST(  u0043_pendingtree_test__xattrs(pCtx, pPathTopDir)  );
#else
	INFOP("u0043_pendingtree",("skipping XATTR tests"));
#endif

	BEGIN_TEST(  u0043_pendingtree_test__arbitrary_cset_vs_wd(pCtx, pPathTopDir)  );

	BEGIN_TEST(  u0043_pendingtree_test__share_treediff_cache1(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__share_treediff_cache2(pCtx, pPathTopDir)  );

	BEGIN_TEST(  u0043_pendingtree_test__revert__on_clean_wd(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert__rename_needs_swap(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert__move_needs_swap(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert__rename_needs_multilevel_swap(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert__lost_found(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert__sadistic_lost_found(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert(pCtx, pPathTopDir, SG_FALSE)  );	
	BEGIN_TEST(  u0043_pendingtree_test__revert(pCtx, pPathTopDir, SG_TRUE)  );	
	BEGIN_TEST(  u0043_pendingtree_test__partial_revert(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert__deletes_with_attrbits_and_xattrs(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert__explicit_delete_with_moves_to_another_dir(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__revert__implicit_nested_delete_with_moves_to_another_dir(pCtx, pPathTopDir)  );

	BEGIN_TEST(  u0043_pendingtree_test__delete_file_add_dir_with_same_name(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__delete_dir_add_new_dir_with_same_name(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__delete_dir_add_new_dir_with_same_name2(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__delete_dir_add_file_with_same_name(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0043_pendingtree_test__scan_should_ignore_some_stuff(pCtx, pPathTopDir, SG_FALSE)  );
	BEGIN_TEST(  u0043_pendingtree_test__scan_should_ignore_some_stuff(pCtx, pPathTopDir, SG_TRUE)  );

	BEGIN_TEST(  u0043_pendingtree_test__partial_revert_conflict(pCtx, pPathTopDir, SG_FALSE)  );
	BEGIN_TEST(  u0043_pendingtree_test__partial_revert_conflict(pCtx, pPathTopDir, SG_TRUE)  );

	BEGIN_TEST(  u0043_pendingtree_test__switch_baseline(pCtx, pPathTopDir)  );

	BEGIN_TEST(  u0043_pendingtree_test__revert__implicit_nested_delete_with_moves_to_root(pCtx, pPathTopDir)  );
#else
	INFOP("u0043_pendingtree",("skipping the bulk of the tests"));
#endif

	//////////////////////////////////////////////////////////////////
	// the following ones have outstanding issues.
	//////////////////////////////////////////////////////////////////



	//////////////////////////////////////////////////////////////////

	/* TODO rm -rf the top dir */

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);

	TEMPLATE_MAIN_END;
}

#undef AA
#undef MY_VERIFY_IN_SECTION
#undef MY_VERIFY_MOVED_FROM
#undef MY_VERIFY_NAMES
#undef MY_VERIFY_REPO_PATH
#undef MY_VERIFY__IN_SECTION__REPO_PATH
