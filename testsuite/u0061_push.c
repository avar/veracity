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

#include <sg.h>
#include "unittests.h"
#include "unittests_pendingtree.h"

//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0061_push)
#define MyDcl(name)				u0061_push__##name
#define MyFn(name)				u0061_push__##name

void MyFn(create_file__numbers)(
	SG_context* pCtx,
	SG_pathname* pPath,
	const char* pszName,
	const SG_uint32 countLines
	)
{
	SG_pathname* pPathFile = NULL;
	SG_uint32 i;
	SG_file* pFile = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathFile, SG_FILE_CREATE_NEW | SG_FILE_RDWR, 0600, &pFile)  );
	for (i=0; i<countLines; i++)
	{
		char buf[64];

		SG_ERR_CHECK(  SG_sprintf(pCtx, buf, sizeof(buf), "%d\n", i)  );
		SG_ERR_CHECK(  SG_file__write(pCtx, pFile, (SG_uint32) strlen(buf), (SG_byte*) buf, NULL)  );
	}
	SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	return;
fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

void MyFn(commit_all)(
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

	if (ppdn)
	{
		*ppdn = pdn;
	}
	else
	{
		SG_DAGNODE_NULLFREE(pCtx, pdn);
	}

	return;
fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_DAGNODE_NULLFREE(pCtx, pdn);
}


void MyFn(test__simple)(SG_context* pCtx)
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;

	char buf_client_repo_name[SG_TID_MAX_BUFFER_LENGTH];
	char buf_server_repo_name[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	SG_repo* pClientRepo = NULL;
	SG_client* pClient = NULL;

	SG_repo* pServerRepo = NULL;
	SG_bool bMatch = SG_FALSE;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx,&pPathTopDir,bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx,pPathTopDir)  );

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_client_repo_name, sizeof(buf_client_repo_name), 32)  );
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_server_repo_name, sizeof(buf_server_repo_name), 32)  );

	INFOP("test__simple", ("client repo: %s", buf_client_repo_name));
	INFOP("test__simple", ("server repo: %s", buf_server_repo_name));

	/* create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, buf_client_repo_name)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo2(pCtx, buf_client_repo_name, pPathWorkingDir, NULL)  );

	/* open that repo */
	VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, buf_client_repo_name, &pClientRepo)  );

	/* create an empty clone to push into */
	VERIFY_ERR_CHECK(  SG_repo__create_empty_clone(pCtx, buf_client_repo_name, buf_server_repo_name)  );
	VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, buf_server_repo_name, &pServerRepo)  );

	/* add stuff to client repo */
	VERIFY_ERR_CHECK(  MyFn(create_file__numbers)(pCtx, pPathWorkingDir, "aaa", 10)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  MyFn(commit_all)(pCtx, pPathWorkingDir, NULL)  );

	VERIFY_ERR_CHECK(  MyFn(create_file__numbers)(pCtx, pPathWorkingDir, "bbb", 10)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  MyFn(commit_all)(pCtx, pPathWorkingDir, NULL)  );

	/* verify pre-push repos are different */
	VERIFY_ERR_CHECK(  SG_sync__compare_repo_dags(pCtx, pClientRepo, pServerRepo, &bMatch)  );
	VERIFY_COND_FAIL("pre-push repos differ", !bMatch);

	/* get a client and push from client repo to empty server repo */
	VERIFY_ERR_CHECK(  SG_client__open(pCtx, buf_server_repo_name, NULL_CREDENTIAL, &pClient)  ); // TODO Credentials
	VERIFY_ERR_CHECK(  SG_push__all(pCtx, pClientRepo, pClient, SG_TRUE)  );
	SG_CLIENT_NULLFREE(pCtx, pClient);

	/* verify post-push repos are identical */
	VERIFY_ERR_CHECK(  SG_sync__compare_repo_dags(pCtx, pClientRepo, pServerRepo, &bMatch)  );
	VERIFY_COND_FAIL("post-push repo DAGs differ", bMatch);
	VERIFY_ERR_CHECK(  SG_sync__compare_repo_blobs(pCtx, pClientRepo, pServerRepo, &bMatch)  );
	VERIFY_COND_FAIL("post-push repo blobs differ", bMatch);

	VERIFY_ERR_CHECK(  SG_repo__check_integrity(pCtx, pClientRepo, SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY, SG_DAGNUM__VERSION_CONTROL, NULL, NULL)  );

	/* TODO: verify more stuff? */

	/* Fall through to common cleanup */

fail:
	/* close client */
	SG_CLIENT_NULLFREE(pCtx, pClient);

	/* close both repos */
	SG_REPO_NULLFREE(pCtx, pServerRepo);
	SG_REPO_NULLFREE(pCtx, pClientRepo);

	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
}

void MyFn(test__big_changeset)(SG_context* pCtx)
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;

	char buf_client_repo_name[SG_TID_MAX_BUFFER_LENGTH];
	char buf_server_repo_name[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	SG_repo* pClientRepo = NULL;
	SG_client* pClient = NULL;

	SG_pathname* pPathCsDir = NULL;
	SG_uint32 lines;
	SG_uint32 i, j;

	SG_repo* pServerRepo = NULL;
	SG_bool bMatch = SG_FALSE;
	char buf_filename[7];

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx,&pPathTopDir,bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx,pPathTopDir)  );

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_client_repo_name, sizeof(buf_client_repo_name), 32)  );
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_server_repo_name, sizeof(buf_server_repo_name), 32)  );

	INFOP("test__big_changeset", ("client repo: %s", buf_client_repo_name));
	INFOP("test__big_changeset", ("server repo: %s", buf_server_repo_name));

	/* create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, buf_client_repo_name)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo2(pCtx, buf_client_repo_name, pPathWorkingDir, NULL)  );

	/* open that repo */
	VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, buf_client_repo_name, &pClientRepo)  );

	/* create an empty clone to push into */
	VERIFY_ERR_CHECK(  SG_repo__create_empty_clone(pCtx, buf_client_repo_name, buf_server_repo_name)  );
	VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, buf_server_repo_name, &pServerRepo)  );

	/* add stuff to client repo */
	for (i = 0; i < 1; i++) // number of changesets
	{
		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf_filename, sizeof(buf_filename), "%d", i)  );
		VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathCsDir, pPathWorkingDir, buf_filename)  );
		VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathCsDir)  );

		for (j = 0; j < 5000; j++) // number of files added per changeset
		{
			VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf_filename, sizeof(buf_filename), "%d", j)  );
			lines = j;

			VERIFY_ERR_CHECK(  MyFn(create_file__numbers)(pCtx, pPathCsDir, buf_filename, lines)  );
		}

		SG_PATHNAME_NULLFREE(pCtx, pPathCsDir);

		VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
		VERIFY_ERR_CHECK(  MyFn(commit_all)(pCtx, pPathWorkingDir, NULL)  );
	}


	/* verify pre-push repos are different */
	VERIFY_ERR_CHECK(  SG_sync__compare_repo_dags(pCtx, pClientRepo, pServerRepo, &bMatch)  );
	VERIFY_COND_FAIL("pre-push repos differ", !bMatch);

	/* get a client and push from client repo to empty server repo */
	VERIFY_ERR_CHECK(  SG_client__open(pCtx, buf_server_repo_name, NULL_CREDENTIAL, &pClient)  ); // TODO Credentials
	VERIFY_ERR_CHECK(  SG_push__all(pCtx, pClientRepo, pClient, SG_TRUE)  );
	SG_CLIENT_NULLFREE(pCtx, pClient);

	/* verify post-push repos are identical */
	VERIFY_ERR_CHECK(  SG_sync__compare_repo_dags(pCtx, pClientRepo, pServerRepo, &bMatch)  );
	VERIFY_COND_FAIL("post-push repo DAGs differ", bMatch);
	VERIFY_ERR_CHECK(  SG_sync__compare_repo_blobs(pCtx, pClientRepo, pServerRepo, &bMatch)  );
	VERIFY_COND_FAIL("post-push repo blobs differ", bMatch);

	VERIFY_ERR_CHECK(  SG_repo__check_integrity(pCtx, pClientRepo, SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY, SG_DAGNUM__VERSION_CONTROL, NULL, NULL)  );
	VERIFY_ERR_CHECK(  SG_repo__check_integrity(pCtx, pClientRepo, SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY, SG_DAGNUM__VERSION_CONTROL, NULL, NULL)  );

	/* TODO: verify more stuff? */

	/* Fall through to common cleanup */

fail:
	/* close client */
	SG_CLIENT_NULLFREE(pCtx, pClient);

	/* close both repos */
	SG_REPO_NULLFREE(pCtx, pServerRepo);
	SG_REPO_NULLFREE(pCtx, pClientRepo);

	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathCsDir);

	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
}

void MyFn(test__long_dag)(SG_context* pCtx)
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;

	char buf_client_repo_name[SG_TID_MAX_BUFFER_LENGTH];
	char buf_server_repo_name[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	SG_repo* pClientRepo = NULL;
	SG_client* pClient = NULL;

	SG_pathname* pPathCsDir = NULL;
	SG_uint32 lines;
	SG_uint32 i, j;

	SG_repo* pServerRepo = NULL;
	SG_bool bMatch = SG_FALSE;
	char buf_filename[7];

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx,&pPathTopDir,bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx,pPathTopDir)  );

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_client_repo_name, sizeof(buf_client_repo_name), 32)  );
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_server_repo_name, sizeof(buf_server_repo_name), 32)  );

	INFOP("test__long_dag", ("client repo: %s", buf_client_repo_name));
	INFOP("test__long_dag", ("server repo: %s", buf_server_repo_name));

	/* create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, buf_client_repo_name)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo2(pCtx, buf_client_repo_name, pPathWorkingDir, NULL)  );

	/* open that repo */
	VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, buf_client_repo_name, &pClientRepo)  );

	/* create an empty clone to push into */
	VERIFY_ERR_CHECK(  SG_repo__create_empty_clone(pCtx, buf_client_repo_name, buf_server_repo_name)  );
	VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, buf_server_repo_name, &pServerRepo)  );

	/* add stuff to client repo */
	for (i = 0; i < 20; i++) // number of changesets
	{
		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf_filename, sizeof(buf_filename), "%d", i)  );
		VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathCsDir, pPathWorkingDir, buf_filename)  );
		VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathCsDir)  );

		for (j = 0; j < 1; j++) // number of files added per changeset
		{
			VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf_filename, sizeof(buf_filename), "%d", j)  );
			lines = (int)(2500.0 * (rand() / (RAND_MAX + 1.0)));

			VERIFY_ERR_CHECK(  MyFn(create_file__numbers)(pCtx, pPathCsDir, buf_filename, lines)  );
		}

		SG_PATHNAME_NULLFREE(pCtx, pPathCsDir);

		VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
		VERIFY_ERR_CHECK(  MyFn(commit_all)(pCtx, pPathWorkingDir, NULL)  );
	}


	/* verify pre-push repos are different */
	VERIFY_ERR_CHECK(  SG_sync__compare_repo_dags(pCtx, pClientRepo, pServerRepo, &bMatch)  );
	VERIFY_COND_FAIL("pre-push repos differ", !bMatch);

	/* get a client and push from client repo to empty server repo */
	VERIFY_ERR_CHECK(  SG_client__open(pCtx, buf_server_repo_name, NULL_CREDENTIAL, &pClient)  ); // TODO Credentials
	VERIFY_ERR_CHECK(  SG_push__all(pCtx, pClientRepo, pClient, SG_TRUE)  );
	SG_CLIENT_NULLFREE(pCtx, pClient);

	/* verify post-push repos are identical */
	VERIFY_ERR_CHECK(  SG_sync__compare_repo_dags(pCtx, pClientRepo, pServerRepo, &bMatch)  );
	VERIFY_COND_FAIL("post-push repo DAGs differ", bMatch);
	VERIFY_ERR_CHECK(  SG_sync__compare_repo_blobs(pCtx, pClientRepo, pServerRepo, &bMatch)  );
	VERIFY_COND_FAIL("post-push repo blobs differ", bMatch);

	VERIFY_ERR_CHECK(  SG_repo__check_integrity(pCtx, pClientRepo, SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY, SG_DAGNUM__VERSION_CONTROL, NULL, NULL)  );
	VERIFY_ERR_CHECK(  SG_repo__check_integrity(pCtx, pClientRepo, SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY, SG_DAGNUM__VERSION_CONTROL, NULL, NULL)  );

	/* TODO: verify more stuff? */

	/* Fall through to common cleanup */

fail:
	/* close client */
	SG_CLIENT_NULLFREE(pCtx, pClient);

	/* close both repos */
	SG_REPO_NULLFREE(pCtx, pServerRepo);
	SG_REPO_NULLFREE(pCtx, pClientRepo);

	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathCsDir);

	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
}

void MyFn(test__wide_dag)(SG_context* pCtx)
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;

	char buf_client_repo_name[SG_TID_MAX_BUFFER_LENGTH];
	char buf_server_repo_name[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_vhash* pvh = NULL;
	SG_repo* pClientRepo = NULL;
	SG_client* pClient = NULL;
	const char* pszidFirstChangeset = NULL;

	SG_pathname* pPathCsDir = NULL;
	SG_uint32 lines;
	SG_uint32 i, j;

	SG_repo* pServerRepo = NULL;
	SG_bool bMatch = SG_FALSE;
	char buf_filename[7];

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx,&pPathTopDir,bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx,pPathTopDir)  );

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_client_repo_name, sizeof(buf_client_repo_name), 32)  );
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_server_repo_name, sizeof(buf_server_repo_name), 32)  );

	INFOP("test__wide_dag", ("client repo: %s", buf_client_repo_name));
	INFOP("test__wide_dag", ("server repo: %s", buf_server_repo_name));

	/* create the repo */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, buf_client_repo_name)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo2(pCtx, buf_client_repo_name, pPathWorkingDir, NULL)  );

	/* open that repo */
	VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, buf_client_repo_name, &pClientRepo)  );

	/* create an empty clone to push into */
	VERIFY_ERR_CHECK(  SG_repo__create_empty_clone(pCtx, buf_client_repo_name, buf_server_repo_name)  );
	VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, buf_server_repo_name, &pServerRepo)  );

	/* add stuff to client repo */
	for (i = 0; i < 20; i++) // number of changesets
	{
		VERIFY_ERR_CHECK(  _ut_pt__set_baseline(pCtx, pPathWorkingDir, pszidFirstChangeset)  );

		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf_filename, sizeof(buf_filename), "%d", i)  );
		VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathCsDir, pPathWorkingDir, buf_filename)  );
		VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathCsDir)  );

		for (j = 0; j < 1; j++) // number of files added per changeset
		{
			VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf_filename, sizeof(buf_filename), "%d", j)  );
			lines = (int)(2500.0 * (rand() / (RAND_MAX + 1.0)));

			VERIFY_ERR_CHECK(  MyFn(create_file__numbers)(pCtx, pPathCsDir, buf_filename, lines)  );
		}

		SG_PATHNAME_NULLFREE(pCtx, pPathCsDir);

		VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
		VERIFY_ERR_CHECK(  MyFn(commit_all)(pCtx, pPathWorkingDir, NULL)  );
	}

	/* verify pre-push repos are different */
	VERIFY_ERR_CHECK(  SG_sync__compare_repo_dags(pCtx, pClientRepo, pServerRepo, &bMatch)  );
	VERIFY_COND_FAIL("pre-push repos differ", !bMatch);

	/* get a client and push from client repo to empty server repo */
	VERIFY_ERR_CHECK(  SG_client__open(pCtx, buf_server_repo_name, NULL_CREDENTIAL, &pClient)  ); // TODO Credentials
	VERIFY_ERR_CHECK(  SG_push__all(pCtx, pClientRepo, pClient, SG_TRUE)  );
	SG_CLIENT_NULLFREE(pCtx, pClient);

	/* verify post-push repos are identical */
	VERIFY_ERR_CHECK(  SG_sync__compare_repo_dags(pCtx, pClientRepo, pServerRepo, &bMatch)  );
	VERIFY_COND_FAIL("post-push repo DAGs differ", bMatch);
	VERIFY_ERR_CHECK(  SG_sync__compare_repo_blobs(pCtx, pClientRepo, pServerRepo, &bMatch)  );
	VERIFY_COND_FAIL("post-push repo blobs differ", bMatch);

	VERIFY_ERR_CHECK(  SG_repo__check_integrity(pCtx, pClientRepo, SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY, SG_DAGNUM__VERSION_CONTROL, NULL, NULL)  );
	VERIFY_ERR_CHECK(  SG_repo__check_integrity(pCtx, pClientRepo, SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY, SG_DAGNUM__VERSION_CONTROL, NULL, NULL)  );

	/* TODO: verify more stuff? */

	/* Fall through to common cleanup */

fail:
	/* close client */
	SG_CLIENT_NULLFREE(pCtx, pClient);

	/* close both repos */
	SG_REPO_NULLFREE(pCtx, pServerRepo);
	SG_REPO_NULLFREE(pCtx, pClientRepo);

	SG_NULLFREE(pCtx, pszidFirstChangeset);
	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathCsDir);

	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
}

MyMain()
{
	TEMPLATE_MAIN_START;

#ifdef SG_NIGHTLY_BUILD
	VERIFY_ERR_CHECK(  MyFn(test__big_changeset)(pCtx)  );
#endif
	VERIFY_ERR_CHECK(  MyFn(test__simple)(pCtx)  );
	VERIFY_ERR_CHECK(  MyFn(test__long_dag)(pCtx)  );
	VERIFY_ERR_CHECK(  MyFn(test__wide_dag)(pCtx)  );

	// Fall through to common cleanup.

fail:

	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn
