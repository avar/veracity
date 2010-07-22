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
 * @file u0048_multidag.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

void u0048_multidag__new_repo(
	SG_context * pCtx,
	const char* pszRidescName,
    SG_repo** ppResult
	)
{
	SG_vhash* pvhPartialDescriptor = NULL;
	SG_repo* pRepo = NULL;
	const SG_vhash* pvhActualDescriptor = NULL;
	SG_changeset* pcsFirst = NULL;
	char* pszidGidActualRoot = NULL;
	const char* pszidFirstChangeset = NULL;
    char buf_repo_id[SG_GID_BUFFER_LENGTH];
    char buf_admin_id[SG_GID_BUFFER_LENGTH];

	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_repo_id, sizeof(buf_repo_id))  );
	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_admin_id, sizeof(buf_admin_id))  );

	VERIFY_ERR_CHECK(  SG_closet__get_partial_repo_instance_descriptor_for_new_local_repo(pCtx, &pvhPartialDescriptor)  );

    /* This test case writes dag nodes which are not real.  They don't have a
     * changeset associated with them.  So, if we use a caching repo, the
     * caching code will fail because it tries to load a changeset
     * which doesn't exist.  So, we strip down to a raw repo here. */

	VERIFY_ERR_CHECK(  SG_repo__create_repo_instance(pCtx,pvhPartialDescriptor,SG_TRUE,NULL,buf_repo_id,buf_admin_id,&pRepo)  );
    VERIFY_ERR_CHECK(  sg_zing__init_new_repo(pCtx, pRepo)  );

	VERIFY_ERR_CHECK(  SG_repo__create_user_root_directory(pCtx, pRepo, "@", &pcsFirst, &pszidGidActualRoot)  );

	VERIFY_ERR_CHECK(  SG_changeset__get_id_ref(pCtx, pcsFirst, &pszidFirstChangeset)  );

	VERIFY_ERR_CHECK(  SG_repo__get_descriptor(pCtx, pRepo, &pvhActualDescriptor)  );

	/* TODO should this give an error if the ridesc name already exists? */

	VERIFY_ERR_CHECK(  SG_closet__descriptors__add(pCtx, pszRidescName, pvhActualDescriptor)  );

	SG_NULLFREE(pCtx, pszidGidActualRoot);
	SG_CHANGESET_NULLFREE(pCtx, pcsFirst);
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);

    *ppResult = pRepo;

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);
	SG_REPO_NULLFREE(pCtx, pRepo);

	return;
}

void u0048_multidag__add_dagnode(SG_context * pCtx,
								 char** ppszid, const char* pszidParent, SG_uint32 iDagNum, SG_repo* pRepo)
{
	char buf_tid[SG_TID_MAX_BUFFER_LENGTH];
    SG_dagnode* pdn = NULL;
	SG_repo_tx_handle* pTx;

	// create a TID just to get some random data.  use it to create a HID.
	// use the HID as the HID of a hypothetical changeset so that we can create the dagnode.

	VERIFY_ERR_CHECK(  SG_tid__generate(pCtx, buf_tid, sizeof(buf_tid))  );
	VERIFY_ERR_CHECK(  SG_repo__alloc_compute_hash__from_bytes(pCtx,
															   pRepo,
															   sizeof(buf_tid),
															   (SG_byte *)buf_tid,
															   ppszid)  );

    VERIFY_ERR_CHECK(  SG_dagnode__alloc(pCtx, &pdn,*ppszid, pszidParent ? 2: 1 )  );
    if (pszidParent)
    {
        VERIFY_ERR_CHECK(  SG_dagnode__add_parent(pCtx, pdn,pszidParent)  );
    }
    VERIFY_ERR_CHECK(  SG_dagnode__freeze(pCtx, pdn)  );
	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  SG_repo__store_dagnode(pCtx, pRepo, pTx, iDagNum, pdn)  );
    pdn = NULL;
    VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo,&pTx)  );

    return;

fail:
    return;
}

#define u0048_multidag__MY_LABEL_LENGTH		6
#define u0048_multidag__MY_LABEL			"u0048_"

void u0048_multidag_test__1(SG_context * pCtx)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH + u0048_multidag__MY_LABEL_LENGTH];
    SG_repo* pRepo = NULL;
    SG_rbtree* prb = NULL;
    SG_uint32 count;

    char* pid1 = NULL;
    char* pid1a = NULL;
    char* pid1b = NULL;
    char* pid1c = NULL;

    char* pid2 = NULL;
    char* pid2a = NULL;
    char* pid2b = NULL;

	VERIFY_ERR_CHECK(  SG_strcpy(pCtx, bufName, sizeof(bufName), u0048_multidag__MY_LABEL)  );
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, &bufName[u0048_multidag__MY_LABEL_LENGTH], (sizeof(bufName) - u0048_multidag__MY_LABEL_LENGTH), 32)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  u0048_multidag__new_repo(pCtx, bufName, &pRepo)  );

    VERIFY_ERR_CHECK(  u0048_multidag__add_dagnode(pCtx, &pid1, NULL, SG_DAGNUM__TESTING__NOTHING, pRepo)  );
    VERIFY_ERR_CHECK(  u0048_multidag__add_dagnode(pCtx, &pid1a, pid1, SG_DAGNUM__TESTING__NOTHING, pRepo)  );
    VERIFY_ERR_CHECK(  u0048_multidag__add_dagnode(pCtx, &pid1b, pid1, SG_DAGNUM__TESTING__NOTHING, pRepo)  );
    VERIFY_ERR_CHECK(  u0048_multidag__add_dagnode(pCtx, &pid1c, pid1, SG_DAGNUM__TESTING__NOTHING, pRepo)  );

    VERIFY_ERR_CHECK(  u0048_multidag__add_dagnode(pCtx, &pid2, NULL, SG_DAGNUM__TESTING2__NOTHING, pRepo)  );
    VERIFY_ERR_CHECK(  u0048_multidag__add_dagnode(pCtx, &pid2a, pid2, SG_DAGNUM__TESTING2__NOTHING, pRepo)  );
    VERIFY_ERR_CHECK(  u0048_multidag__add_dagnode(pCtx, &pid2b, pid2, SG_DAGNUM__TESTING2__NOTHING, pRepo)  );

    SG_NULLFREE(pCtx, pid1);
    SG_NULLFREE(pCtx, pid1a);
    SG_NULLFREE(pCtx, pid1b);
    SG_NULLFREE(pCtx, pid1c);

    SG_NULLFREE(pCtx, pid2);
    SG_NULLFREE(pCtx, pid2a);
    SG_NULLFREE(pCtx, pid2b);

    VERIFY_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, SG_DAGNUM__TESTING__NOTHING, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    SG_RBTREE_NULLFREE(pCtx, prb);
    VERIFY_COND("count", (3 == count));

    VERIFY_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, SG_DAGNUM__TESTING2__NOTHING, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    SG_RBTREE_NULLFREE(pCtx, prb);
    VERIFY_COND("count", (2 == count));

	SG_REPO_NULLFREE(pCtx, pRepo);

	return;

fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
}

TEST_MAIN(u0048_multidag)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0048_multidag_test__1(pCtx)  );

	TEMPLATE_MAIN_END;
}
