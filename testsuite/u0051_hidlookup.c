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
 * @file u0051_hidlookup.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"
#include "unittests_pendingtree.h"

void u0051_hidlookup__commit_all(
	SG_context* pCtx,
	const SG_pathname* pPathWorkingDir,
    SG_dagnode** ppdn
	)
{
	SG_pendingtree* pPendingTree = NULL;
    SG_repo* pRepo = NULL;
    SG_dagnode* pdn = NULL;
    SG_audit q;

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );

    SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );
    SG_ERR_CHECK(  SG_audit__init(pCtx, &q, pRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
	SG_ERR_CHECK(  unittests_pendingtree__commit(pCtx, pPendingTree, &q, NULL, 0, NULL, NULL, 0, NULL, 0, NULL, 0, &pdn)  );

	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

    *ppdn = pdn;

	return;
fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

void u0051_hidlookup__create_file__numbers(
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

void u0051_hidlookup_test__1(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
    SG_dagnode* pdn = NULL;
    const char* psz_hid_cs = NULL;
    SG_repo* pRepo = NULL;
    char buf_partial[256];
    char* psz_result = NULL;
    SG_audit q;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add stuff */
	VERIFY_ERR_CHECK(  u0051_hidlookup__create_file__numbers(pCtx, pPathWorkingDir, "aaa", 20)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0051_hidlookup__commit_all(pCtx, pPathWorkingDir, &pdn)  );

    VERIFY_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pdn, &psz_hid_cs)  );

	VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );

    VERIFY_ERR_CHECK(  SG_audit__init(pCtx, &q, pRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
    VERIFY_ERR_CHECK(  SG_vc_tags__add(pCtx, pRepo, psz_hid_cs, "remember", &q)  );

    VERIFY_ERR_CHECK(  SG_strcpy(pCtx, buf_partial, sizeof(buf_partial), psz_hid_cs)  );
    buf_partial[10] = 0;

	VERIFY_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, buf_partial, &psz_result)  );
    VERIFY_COND("found", (0 == strcmp(psz_result, psz_hid_cs)));
    SG_NULLFREE(pCtx, psz_result);

    VERIFY_ERR_CHECK(  SG_repo__hidlookup__blob(pCtx, pRepo, buf_partial, &psz_result)  );
    VERIFY_COND("found", (0 == strcmp(psz_result, psz_hid_cs)));
    SG_NULLFREE(pCtx, psz_result);

	VERIFY_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, "remember", &psz_result)  );
    VERIFY_COND("found", (0 == strcmp(psz_result, psz_hid_cs)));
    SG_NULLFREE(pCtx, psz_result);

fail:
    SG_NULLFREE(pCtx, psz_result);
    SG_REPO_NULLFREE(pCtx, pRepo);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

TEST_MAIN(u0051_hidlookup)
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;

	TEMPLATE_MAIN_START;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathTopDir,bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathTopDir)  );

	BEGIN_TEST(  u0051_hidlookup_test__1(pCtx, pPathTopDir)  );

	/* TODO rm -rf the top dir */

	// fall-thru to common cleanup

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);

	TEMPLATE_MAIN_END;
}

