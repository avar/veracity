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
 * @file u0046_pendport.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"
#include "unittests_pendingtree.h"

//////////////////////////////////////////////////////////////////

void u0046_pendport__commit_all(
	SG_context* pCtx,
	const SG_pathname* pPathWorkingDir
	)
{
	SG_pendingtree* pPendingTree = NULL;
    SG_repo* pRepo = NULL;
    SG_audit q;

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

    SG_ERR_CHECK(  SG_audit__init(pCtx, &q, pRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
	SG_ERR_CHECK(  unittests_pendingtree__commit(pCtx, pPendingTree, &q, NULL, 0, NULL, NULL, 0, NULL, 0, NULL, 0, NULL)  );

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

void u0046_pendport__commit(
	SG_context* pCtx,
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

	paszItems = (const char**) SG_calloc(count, sizeof(char*));
	if (!paszItems)
	{
		SG_ERR_THROW(  SG_ERR_MALLOCFAILED  );
	}

	va_start( marker, pPathWorkingDir );
	for (i=0; i<count; i++)
	{
		paszItems[i++] = va_arg( marker, char*);
	}
	va_end( marker );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );

    SG_ERR_CHECK(  SG_audit__init(pCtx, &q, pRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
	SG_ERR_CHECK(  unittests_pendingtree__commit(pCtx, pPendingTree, &q, pPathWorkingDir, count, paszItems,  NULL, 0, NULL, 0, NULL, 0, NULL)  );

fail:
	SG_NULLFREE(pCtx, paszItems);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

void u0046_pendport__delete_file(
	SG_context* pCtx,
	SG_pathname* pPath,
	const char* pszName
	)
{
	SG_pathname* pPathFile = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPathFile)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

void u0046_pendport__delete_dir(
	SG_context* pCtx,
	SG_pathname* pPath,
	const char* pszName
	)
{
	SG_pathname* pPathFile = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__rmdir__pathname(pCtx, pPathFile)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

void u0046_pendport__append_to_file__numbers(
	SG_context* pCtx,
	SG_pathname* pPath,
	const char* pszName,
	const SG_uint32 countLines
	)
{
	SG_pathname* pPathFile = NULL;
	SG_uint32 i;
	SG_file* pFile = NULL;
	SG_uint64 len = 0;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len, NULL)  );
	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathFile, SG_FILE_OPEN_EXISTING | SG_FILE_RDWR, 0600, &pFile)  );
	VERIFY_ERR_CHECK(  SG_file__seek(pCtx, pFile, len)  );
	for (i=0; i<countLines; i++)
	{
		char buf[64];

		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, sizeof(buf), "%d\n", i)  );
		VERIFY_ERR_CHECK(  SG_file__write(pCtx, pFile, (SG_uint32) strlen(buf), (SG_byte*) buf, NULL)  );
	}
	VERIFY_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	return;
fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

void u0046_pendport__create_symlink(
	SG_context* pCtx,
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

void u0046_pendport__create_file__numbers(
	SG_context* pCtx,
	SG_pathname* pPath,
	const char* pszName,
	const SG_uint32 countLines
	)
{
	SG_pathname* pPathFile = NULL;
	SG_uint32 i;
	SG_file* pFile = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathFile, SG_FILE_CREATE_NEW | SG_FILE_RDWR, 0600, &pFile)  );
	for (i=0; i<countLines; i++)
	{
		char buf[64];

		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, sizeof(buf), "%d\n", i)  );
		VERIFY_ERR_CHECK(  SG_file__write(pCtx, pFile, (SG_uint32) strlen(buf), (SG_byte*) buf, NULL)  );
	}
	VERIFY_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	return;

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

//////////////////////////////////////////////////////////////////

/**
 * Create @/aaa
 * Commit
 * Create @/<bogus_file_name> and verify we get portability warning for a potentially invalid file name.
 * Verify nothing pending.
 */
void u0046_pendport_test__add(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pendingtree* pPendingTree = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add stuff */
	VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, "aaa", 20)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove_param(pCtx, pPathWorkingDir, SG_FALSE)  );
	VERIFY_ERR_CHECK(  u0046_pendport__commit_all(pCtx, pPathWorkingDir)  );

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

    {
        const char* pszName = SG_PORT_DEBUG_BAD_NAME;

        VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, pszName, 44)  );
        VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
		VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_pendingtree__add(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_FALSE, NULL, 0, NULL, 0, SG_FALSE)  ,  SG_ERR_PORTABILITY_WARNINGS  );
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
    }

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////
	// TODO so if we called __addremove, would we see the bogus file as a __FOUND entry?

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

void u0046_pendport_test__rename(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pathname* pPathNewName = NULL;
	SG_pendingtree* pPendingTree = NULL;
    SG_bool bExists = SG_FALSE;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add files */
	VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, "aaa", 20)  );
	VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, "bbb", 20)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove_param(pCtx, pPathWorkingDir, SG_FALSE)  );
	VERIFY_ERR_CHECK(  u0046_pendport__commit_all(pCtx, pPathWorkingDir)  );

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now rename a file */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "bbb")  );
	VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "AAA", SG_FALSE)  ,  SG_ERR_PORTABILITY_WARNINGS  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

    /* make sure the rename didn't actually happen */
    VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
    VERIFY_COND("file should not have been renamed", (bExists));

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewName);
}

void u0046_pendport_test__move(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pathname* pPathNewName = NULL;
    SG_pathname* pPath_d1 = NULL;
	SG_pendingtree* pPendingTree = NULL;
    SG_bool bExists = SG_FALSE;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add stuff */
	VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, "aaa", 20)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1)  );
	VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPath_d1, "AAA", 20)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove_param(pCtx, pPathWorkingDir, SG_FALSE)  );
	VERIFY_ERR_CHECK(  u0046_pendport__commit_all(pCtx, pPathWorkingDir)  );

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now move */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "aaa")  );
	VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPath_d1, SG_FALSE)  ,  SG_ERR_PORTABILITY_WARNINGS  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

    /* make sure the move didn't actually happen */
    VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
    VERIFY_COND("file should not have been moved", (bExists));

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewName);
}

//////////////////////////////////////////////////////////////////

/**
 * When Portability-Warnings are enabled, we try to prevent putting stuff into a
 * directory that would cause problems on other systems (such as adding "README"
 * and "readme" to the same directory on a Windows or MAC system).   The trick is,
 * this is not an error on a Linux system.  So we have to make some guesses.
 *
 * Create @/aaa
 * Commit
 *
 * Delete @/aaa
 * Create @/AAA  (which would clash on case-insensitive filesystems)
 *
 * Revert only the delete of @/aaa and verify that we get a portability warning.
 *
 * On Linux, this is a synthetic error -- we have to detect it.
 * On Windows/Mac, we *want* to detect it *before* we start reverting the WD;
 * that is, if we do nothing, we'll start rebuilding the WD and then get an OS
 * error when the duplicate is detected (or worse, overwrite the other file).
 */
void u0046_pendport_test__revert__delete(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile_aaa = NULL;
	SG_pathname* pPathFile_AAA = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_vhash * pvhPending = NULL;
	SG_rbtree * prbPending = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_bool bExists;
	SG_uint64 len_aaa, len_AAA, len_observed;
	SG_uint32 nrLines_aaa = 20;
	SG_uint32 nrLines_AAA = 40;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile_aaa, pPathWorkingDir,"aaa")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile_AAA, pPathWorkingDir,"AAA")  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add stuff */
	VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, "aaa", nrLines_aaa)  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx,pPathFile_aaa,&len_aaa,NULL)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove_param(pCtx, pPathWorkingDir, SG_FALSE)  );
	VERIFY_ERR_CHECK(  u0046_pendport__commit_all(pCtx, pPathWorkingDir)  );

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	//////////////////////////////////////////////////////////////////

	/* Now delete a file */
    {
        const char* pszName = "aaa";

        VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
        VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, NULL, 0, NULL, 0, SG_FALSE)  );
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
    }

    /* Now add a file which would clash with the old name */
    {
        const char* pszName = "AAA";

        VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, pszName, nrLines_AAA)  );
		VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx,pPathFile_AAA,&len_AAA,NULL)  );

        VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
        VERIFY_ERR_CHECK(  SG_pendingtree__add(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_FALSE, NULL, 0, NULL, 0, SG_FALSE)  );
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
    }

	//////////////////////////////////////////////////////////////////
	// verify that the pendingtree matches our expectation after the manipulation of the WD.

	VERIFY_ERR_CHECK(  SG_pendingtree_debug__export(pCtx,pPathWorkingDir,SG_TRUE,&pTreeDiff,&pvhPending)  );
	VERIFY_ERR_CHECK(  _ut_pt__convert_pendingtree_pvh_2_repo_path_rbtree(pCtx,pvhPending,&prbPending)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree_dump(pCtx,prbPending,"After delete(aaa), create(AAA).")  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<f>@/aaa",         _ut_pt__PTNODE_DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_flags(pCtx,prbPending,"<F>@/AAA",         _ut_pt__PTNODE_ZERO)  );

	VERIFY_ERR_CHECK(  SG_treediff2__report_status_to_console(pCtx,pTreeDiff,SG_FALSE)  );
	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff,SG_FALSE)  );

	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<f>@/aaa",       SG_DIFFSTATUS_FLAGS__DELETED)  );
	VERIFY_ERR_CHECK(  _ut_pt__pending_rbtree__verify_treediff_flags(pCtx,prbPending,pTreeDiff,"<F>@/AAA",       SG_DIFFSTATUS_FLAGS__ADDED)  );

	SG_TREEDIFF2_NULLFREE(pCtx,pTreeDiff);
	SG_RBTREE_NULLFREE(pCtx,prbPending);
	SG_VHASH_NULLFREE(pCtx,pvhPending);

	//////////////////////////////////////////////////////////////////

    /* Now try to revert just the delete -- this should fail. */
    {
        const char* pszName = "aaa";

        VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
        VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_TRUE, NULL, 0, NULL, 0),
											  SG_ERR_PORTABILITY_WARNINGS  );
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
    }

	//////////////////////////////////////////////////////////////////
	// verify the directory is as expected
	//
	// we cannot reliably check for "aaa" not existing and "AAA" existing on Windows/MAC
	// because they both look the same.  It is also not guaranteed that that test would
	// work on Linux -- because we may be running on a remote directory that is mounted
	// from one of these other systems.
	//
	// BUT we can check the file lengths.

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathFile_aaa,&bExists,NULL,NULL)  );
	if (bExists)
	{
		// the OS told us about "aaa", but it is really?
		VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx,pPathFile_aaa,&len_observed,NULL)  );
		VERIFYP_COND("len",(len_observed != len_aaa),("Length indicates 'aaa' or 'AAA' is actually 'aaa'; this should not be."));
	}

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathFile_AAA,&bExists,NULL,NULL)  );
	VERIFY_COND("AAA should exist",(bExists));
	if (bExists)
	{
		// the OS told us about "AAA", but it is really?
		VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx,pPathFile_AAA,&len_observed,NULL)  );
		VERIFYP_COND("len_observed",(len_observed == len_AAA),("Length indicates 'aaa' or 'AAA' is not actually 'AAA'; this should not be."));
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile_aaa);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile_AAA);
}

void u0046_pendport_test__revert__move(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
    SG_pathname* pPath_d1 = NULL;
	SG_pathname* pPath_newhome = NULL;
	SG_pendingtree* pPendingTree = NULL;
    SG_bool bExists = SG_FALSE;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add stuff */
	VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, "aaa", 20)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_d1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_d1)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove_param(pCtx, pPathWorkingDir, SG_FALSE)  );
	VERIFY_ERR_CHECK(  u0046_pendport__commit_all(pCtx, pPathWorkingDir)  );

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

    /* now move */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "aaa")  );
	VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pPathFile, 1, pPath_d1, SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

    /* Now add a file which would clash with the old name */
    {
        const char* pszName = "AAA";

        VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, pszName, 44)  );
        VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
        VERIFY_ERR_CHECK(  SG_pendingtree__add(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_FALSE, NULL, 0, NULL, 0, SG_FALSE)  );
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
    }

    /* Now revert just the move */
    {
        const char* pszName = "d1/aaa";

        VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
        VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_TRUE, NULL, 0, NULL, 0)  ,  SG_ERR_PORTABILITY_WARNINGS  );
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
    }

    /* make sure the file wasn't renamed back */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_newhome, pPath_d1, "aaa")  );
    VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_newhome, &bExists, NULL, NULL)  );
    VERIFY_COND("file should still be there", (bExists));

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPath_d1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_newhome);
}

void u0046_pendport_test__revert__rename(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_pathname* pPathNewName = NULL;
	SG_pendingtree* pPendingTree = NULL;
    SG_bool bExists = SG_FALSE;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add files */
	VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, "aaa", 20)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove_param(pCtx, pPathWorkingDir, SG_FALSE)  );
	VERIFY_ERR_CHECK(  u0046_pendport__commit_all(pCtx, pPathWorkingDir)  );

    /* pending changeset should be empty */
	VERIFY_WD_JSON_PENDING_CLEAN(pCtx, pPathWorkingDir);

	/* Now rename a file */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathWorkingDir, "aaa")  );
	VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathFile, "renamed", SG_FALSE)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathNewName, pPathWorkingDir, "renamed")  );

    /* Now add a file which would clash with the old name */
    {
        const char* pszName = "AAA";

        VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, pszName, 44)  );
        VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
        VERIFY_ERR_CHECK(  SG_pendingtree__add(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_FALSE, NULL, 0, NULL, 0, SG_FALSE)  );
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
    }

    /* Now revert just the rename */
    {
        const char* pszName = "renamed";

        VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
        VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  _ut_pt__pendingtree__revert(pCtx, pPendingTree, pPathWorkingDir, 1, &pszName, SG_TRUE, NULL, 0, NULL, 0)  ,  SG_ERR_PORTABILITY_WARNINGS  );
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
    }

    /* make sure the file wasn't renamed back */
    VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathNewName, &bExists, NULL, NULL)  );
    VERIFY_COND("file should not have been renamed", (bExists));

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathNewName);
}

//////////////////////////////////////////////////////////////////

void u0046_pendport_test__ignore_warnings(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pendingtree* pPendingTree = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add files */
	VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, SG_PORT_DEBUG_BAD_NAME, 20)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );

    /* try addremove */
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  _ut_pt__addremove_param(pCtx, pPathWorkingDir, SG_FALSE),
										  SG_ERR_PORTABILITY_WARNINGS  );

    /* try again, with ignore warnings turned on */
	VERIFY_ERR_CHECK(  _ut_pt__addremove_param(pCtx, pPathWorkingDir, SG_TRUE)  );

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
}

//////////////////////////////////////////////////////////////////

void u0046_pendport_test__addremove(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add files */
	VERIFY_ERR_CHECK(  u0046_pendport__create_file__numbers(pCtx, pPathWorkingDir, SG_PORT_DEBUG_BAD_NAME, 20)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  _ut_pt__addremove_param(pCtx, pPathWorkingDir, SG_FALSE),
										  SG_ERR_PORTABILITY_WARNINGS  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
}

//////////////////////////////////////////////////////////////////

TEST_MAIN(u0046_pendport)
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;

	TEMPLATE_MAIN_START;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathTopDir, bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathTopDir)  );

	//////////////////////////////////////////////////////////////////

	BEGIN_TEST(  u0046_pendport_test__add(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0046_pendport_test__rename(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0046_pendport_test__move(pCtx, pPathTopDir)  );
	// TODO need _test__delete()

	//////////////////////////////////////////////////////////////////

	// TODO need _test__revert__add()
	BEGIN_TEST(  u0046_pendport_test__revert__delete(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0046_pendport_test__revert__move(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0046_pendport_test__revert__rename(pCtx, pPathTopDir)  );

	// TODO need complete (rather than partial) revert tests for each of the above.

	//////////////////////////////////////////////////////////////////

	BEGIN_TEST(  u0046_pendport_test__ignore_warnings(pCtx, pPathTopDir)  );

	//////////////////////////////////////////////////////////////////

	BEGIN_TEST(  u0046_pendport_test__addremove(pCtx, pPathTopDir)  );

	//////////////////////////////////////////////////////////////////

	/* TODO rm -rf the top dir */

	// fall-thru to common cleanup

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);

	TEMPLATE_MAIN_END;
}
