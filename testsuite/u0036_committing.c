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
 * @file u0036_committing.c
 *
 * @details Simple test to create a changeset using SG_committing.
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

#define TRACE_CHANGESETS		1

//////////////////////////////////////////////////////////////////

void u0036_committing__open_repo(SG_context * pCtx, SG_repo ** ppRepo)
{
	// repo's are created on disk as <RepoContainerDirectory>/<RepoGID>/{blobs,...}
	// we pass the current directory as RepoContainerDirectory.
	// caller must free returned value.

	SG_repo * pRepo = NULL;
	SG_pathname * pPathnameRepoDir = NULL;
	SG_vhash* pvhPartialDescriptor = NULL;
    char buf_repo_id[SG_GID_BUFFER_LENGTH];
    char buf_admin_id[SG_GID_BUFFER_LENGTH];
	char* pszRepoImpl = NULL;

	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_repo_id, sizeof(buf_repo_id))  );
	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_admin_id, sizeof(buf_admin_id))  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathnameRepoDir)  );
	VERIFY_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathnameRepoDir)  );

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhPartialDescriptor)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__NEWREPO_DRIVER, NULL, &pszRepoImpl, NULL)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_KEY__STORAGE, pszRepoImpl)  );
	VERIFY_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_FSLOCAL__PATH_PARENT_DIR, SG_pathname__sz(pPathnameRepoDir))  );
	VERIFY_ERR_CHECK(  SG_repo__create_repo_instance(pCtx,pvhPartialDescriptor,SG_TRUE,NULL,buf_repo_id,buf_admin_id,&pRepo)  );
    VERIFY_ERR_CHECK(  sg_zing__init_new_repo(pCtx, pRepo)  );
    VERIFY_ERR_CHECK(  SG_user__create(pCtx, pRepo, "debug@sourcegear.com")  );
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);

	{
		const SG_vhash * pvhRepoDescriptor = NULL;
		SG_repo__get_descriptor(pCtx,pRepo,&pvhRepoDescriptor);
		VERIFY_CTX_IS_OK("open_repo", pCtx);

		//INFOP("open_repo",("Repo is [%s]",SG_string__sz(pstrRepoDescriptor)));
	}

	SG_PATHNAME_NULLFREE(pCtx, pPathnameRepoDir);

    SG_NULLFREE(pCtx, pszRepoImpl);

	*ppRepo = pRepo;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathnameRepoDir);

    SG_NULLFREE(pCtx, pszRepoImpl);
}

void u0036_committing__fetch_dagnode(SG_context * pCtx,
									 SG_repo * pRepo,
									 const char* pszidHidChangeset,
									 SG_dagnode ** ppDagnodeReturned)	// caller must free result
{
	// TODO decide if a transaction is necessary to fetch DAG info.

	SG_dagnode * pNewDagnode = NULL;

	VERIFY_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx,pRepo,pszidHidChangeset,&pNewDagnode)  );

	*ppDagnodeReturned = pNewDagnode;

fail:
	return;
}

void u0036_committing__create_changeset_p(SG_context * pCtx,
										  SG_repo * pRepo,
										  const char* pszidHidPrimaryParent,
										  char** ppszidHidReturned)	// caller must free result
{
	SG_committing * pCommitting = NULL;
	SG_changeset * pChangesetOriginal = NULL;
	SG_dagnode * pDagnodeOriginal = NULL;
    SG_audit q;

	SG_changeset * pChangesetTest = NULL;
	SG_dagnode * pDagnodeTest = NULL;
	SG_bool bEqual;
	char* pszidGidRandom = NULL;
    const char* psz_csid = NULL;

    VERIFY_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW,SG_AUDIT__WHO__FROM_SETTINGS)  );

	// this test was written at version 1.

	SG_ASSERT( CSET_VERSION__CURRENT == CSET_VERSION_1 );

	// allocate and populate a minimal changeset using the SG_committing interface.

	VERIFY_ERR_CHECK(  SG_committing__alloc(pCtx,&pCommitting,pRepo,SG_DAGNUM__VERSION_CONTROL,&q,CSET_VERSION_1)  );
    VERIFY_ERR_CHECK(  SG_committing__add_parent(pCtx, pCommitting, pszidHidPrimaryParent)  );

	// for this test, set the treenode root to NULL

	VERIFY_ERR_CHECK(  SG_committing__set_root(pCtx,pCommitting,NULL)  );

	// add a random blob filled with random data -- to test bloblists and bloblength stuff.
	VERIFY_ERR_CHECK(  SG_gid__alloc(pCtx,&pszidGidRandom)  );
	VERIFY_ERR_CHECK(  SG_committing__add_bytes__buflen(pCtx,pCommitting,NULL,
														(const SG_byte *)pszidGidRandom,strlen(pszidGidRandom)+1,
														SG_BLOB_REFTYPE__TREEUSERFILE,NULL)  );
	SG_NULLFREE(pCtx, pszidGidRandom);

	// store changeset into repository and return HID to the caller.
	// we also get back the original Changeset structure so we can inspect it.

	VERIFY_ERR_CHECK(  SG_committing__end(pCtx,pCommitting,&pChangesetOriginal,&pDagnodeOriginal)  );
	pCommitting = NULL;

	VERIFY_ERR_CHECK(  SG_changeset__get_id_ref(pCtx,pChangesetOriginal,&psz_csid)  );
    VERIFY_ERR_CHECK(  SG_strdup(pCtx,psz_csid,ppszidHidReturned)  );

	// if we successfully created it, fetch it back in and make sure we
	// didn't lose anything on the translation.

	if ((*ppszidHidReturned) && pChangesetOriginal)
	{
		VERIFY_ERR_CHECK(  SG_changeset__load_from_repo(pCtx,pRepo,*ppszidHidReturned,&pChangesetTest)  );

		if (pChangesetTest)
		{
			// TODO VERIFY_ERR_CHECK(  SG_changeset__equal(pCtx,pChangesetTest,pChangesetOriginal,&bEqual)  );
			// TODO VERIFY_COND("verify_p",(bEqual));

			VERIFY_ERR_CHECK(  SG_gid__alloc(pCtx,&pszidGidRandom)  );

			// try to insert something into the changeset we read from the disk.
			// this should fail because the changeset should be frozen.

			VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_changeset__add_blob_to_list(pCtx,pChangesetTest,pszidGidRandom,SG_BLOB_REFTYPE__TREEUSERFILE,0)  );

			SG_CHANGESET_NULLFREE(pCtx, pChangesetTest);
		}

		// also try to fetch the DAGNODE for this changeset from the REPO DAG.
		// see if it round-trips well also.

		VERIFY_ERR_CHECK(  u0036_committing__fetch_dagnode(pCtx, pRepo,*ppszidHidReturned,&pDagnodeTest)  );
		VERIFY_ERR_CHECK(  SG_dagnode__equal(pCtx,pDagnodeOriginal,pDagnodeTest,&bEqual)  );
		VERIFY_COND("dagnode_equal",(bEqual));

		SG_DAGNODE_NULLFREE(pCtx, pDagnodeTest);
	}

fail:
	SG_NULLFREE(pCtx, pszidGidRandom);
	SG_CHANGESET_NULLFREE(pCtx, pChangesetOriginal);
	SG_DAGNODE_NULLFREE(pCtx, pDagnodeOriginal);
	SG_DAGNODE_NULLFREE(pCtx, pDagnodeTest);
}

void u0036_committing__create_changeset_pss(SG_context * pCtx,
											SG_repo * pRepo,
											const char* pszidHidPrimaryParent,
											const char* pszidHidSecondaryParent1,
											const char* pszidHidSecondaryParent2,
											char** ppszidHidReturned)	// caller must free result
{
	SG_committing * pCommitting = NULL;
	SG_changeset * pChangesetOriginal = NULL;
	SG_dagnode * pDagnodeOriginal = NULL;
    SG_audit q;

	SG_changeset * pChangesetTest = NULL;
	SG_dagnode * pDagnodeTest = NULL;
	SG_bool bEqual;
    const char* psz_csid = NULL;

    VERIFY_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW,SG_AUDIT__WHO__FROM_SETTINGS)  );

	// this test was written at version 1.

	SG_ASSERT( CSET_VERSION__CURRENT == CSET_VERSION_1 );

	// allocate and populate a minimal changeset using the SG_committing interface.

	VERIFY_ERR_CHECK(  SG_committing__alloc(pCtx,&pCommitting,pRepo,SG_DAGNUM__VERSION_CONTROL,&q,CSET_VERSION_1)  );

    VERIFY_ERR_CHECK(  SG_committing__add_parent(pCtx,pCommitting, pszidHidPrimaryParent)  );

	// set some secondary parents

	VERIFY_ERR_CHECK(  SG_committing__add_parent(pCtx,pCommitting,pszidHidSecondaryParent1)  );

	VERIFY_ERR_CHECK(  SG_committing__add_parent(pCtx,pCommitting,pszidHidSecondaryParent2)  );

	// for this test, set the root treenode to NULL.

	VERIFY_ERR_CHECK(  SG_committing__set_root(pCtx,pCommitting,NULL)  );

	// store changeset into repository and return HID to the caller.
	// we also get back the original Changeset structure so we can inspect it.

	VERIFY_ERR_CHECK(  SG_committing__end(pCtx,pCommitting,&pChangesetOriginal,&pDagnodeOriginal)  );
	pCommitting = NULL;

	VERIFY_ERR_CHECK(  SG_changeset__get_id_ref(pCtx,pChangesetOriginal,&psz_csid)  );
    VERIFY_ERR_CHECK(  SG_strdup(pCtx,psz_csid,ppszidHidReturned)  );

	// if we successfully created it, fetch it back in and make sure we
	// didn't lose anything on the translation.

	if ((*ppszidHidReturned) && pChangesetOriginal)
	{
		VERIFY_ERR_CHECK(  SG_changeset__load_from_repo(pCtx,pRepo,*ppszidHidReturned,&pChangesetTest)  );

		if (pChangesetTest)
		{
			// TODO VERIFY_ERR_CHECK(  SG_changeset__equal(pCtx,pChangesetTest,pChangesetOriginal,&bEqual)  );
			// TODO VERIFY_COND("verify_pss",(bEqual));

			SG_CHANGESET_NULLFREE(pCtx, pChangesetTest);
		}

		// also try to fetch the DAGNODE for this changeset from the REPO DAG.
		// see if it round-trips well also.

		VERIFY_ERR_CHECK(  u0036_committing__fetch_dagnode(pCtx, pRepo,*ppszidHidReturned,&pDagnodeTest)  );
		VERIFY_ERR_CHECK(  SG_dagnode__equal(pCtx,pDagnodeOriginal,pDagnodeTest,&bEqual)  );
		VERIFY_COND("dagnode_equal",(bEqual));

		SG_DAGNODE_NULLFREE(pCtx, pDagnodeTest);
	}

fail:
	SG_CHANGESET_NULLFREE(pCtx, pChangesetTest);
	SG_DAGNODE_NULLFREE(pCtx, pDagnodeTest);
	SG_CHANGESET_NULLFREE(pCtx, pChangesetOriginal);
	SG_DAGNODE_NULLFREE(pCtx, pDagnodeOriginal);
}

void u0036_committing__run(SG_context * pCtx)
{
	// run the main test.

	SG_repo * pRepo;
	char* pszidHidChangeset0 = NULL;
	char* pszidHidChangeset1a = NULL;
	char* pszidHidChangeset1b = NULL;
	char* pszidHidChangeset1c = NULL;
	char* pszidHidChangeset2 = NULL;

	// create a new repo.

	VERIFY_ERR_CHECK(  u0036_committing__open_repo(pCtx,&pRepo)  );

	// create a simple changeset "0" without secondary parents and with NULL primary parent.

	VERIFY_ERR_CHECK(  u0036_committing__create_changeset_p(pCtx, pRepo,NULL,&pszidHidChangeset0)  );
	INFOP("changeset0",("Changeset HID [%s]",(pszidHidChangeset0)));

	// create 3 simple changesets "1a", "1b", and "1c" that are children of "0".

	VERIFY_ERR_CHECK_DISCARD(  u0036_committing__create_changeset_p(pCtx, pRepo,pszidHidChangeset0,&pszidHidChangeset1a)  );
	INFOP("changeset1a",("Changeset HID [%s]",(pszidHidChangeset1a)));

	VERIFY_ERR_CHECK_DISCARD(  u0036_committing__create_changeset_p(pCtx, pRepo,pszidHidChangeset0,&pszidHidChangeset1b)  );
	INFOP("changeset1b",("Changeset HID [%s]",(pszidHidChangeset1b)));

	VERIFY_ERR_CHECK_DISCARD(  u0036_committing__create_changeset_p(pCtx, pRepo,pszidHidChangeset0,&pszidHidChangeset1c)  );
	INFOP("changeset1c",("Changeset HID [%s]",(pszidHidChangeset1c)));

	// create a simple changeset "2" that has primary parent "1a" and secondaries "1b" and "1c".

	VERIFY_ERR_CHECK_DISCARD(  u0036_committing__create_changeset_pss(pCtx, pRepo,pszidHidChangeset1a,pszidHidChangeset1b,pszidHidChangeset1c,&pszidHidChangeset2)  );
	INFOP("changeset2",("Changeset HID [%s]",(pszidHidChangeset2)));

	SG_NULLFREE(pCtx, pszidHidChangeset0);
	SG_NULLFREE(pCtx, pszidHidChangeset1a);
	SG_NULLFREE(pCtx, pszidHidChangeset1b);
	SG_NULLFREE(pCtx, pszidHidChangeset1c);
	SG_NULLFREE(pCtx, pszidHidChangeset2);

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__check_integrity(pCtx,pRepo,SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY,SG_DAGNUM__VERSION_CONTROL,NULL,NULL)  );

fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
}

/**
 * Simple test to create, store, fetch, verify a simple changeset.
 */
TEST_MAIN(u0036_committing)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0036_committing__run(pCtx)  );

	TEMPLATE_MAIN_END;
}
