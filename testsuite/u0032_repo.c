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

void u0032_repo__test_ids(SG_context* pCtx)
{
	SG_repo* pRepo = NULL;
	SG_pathname* pPath_repo = NULL;
	SG_vhash* pvhPartialDescriptor = NULL;
    char buf_repo_id[SG_GID_BUFFER_LENGTH];
    char buf_admin_id[SG_GID_BUFFER_LENGTH];
	char* pszRepoImpl = NULL;
	char* pszStoredAdminId = NULL;
	char* pszStoredRepoId = NULL;

	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_repo_id, sizeof(buf_repo_id))  );
	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_admin_id, sizeof(buf_admin_id))  );

	/* Get our paths fixed up */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath_repo)  );
	VERIFY_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath_repo)  );
	VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath_repo, "repo")  );

	SG_ERR_IGNORE(  SG_fsobj__mkdir__pathname(pCtx, pPath_repo)  );

	/* Create the repo */

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx,&pvhPartialDescriptor)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__NEWREPO_DRIVER, NULL, &pszRepoImpl, NULL)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_KEY__STORAGE, pszRepoImpl)  );
	VERIFY_ERR_CHECK(  SG_vhash__add__string__sz(pCtx,pvhPartialDescriptor, SG_RIDESC_FSLOCAL__PATH_PARENT_DIR, SG_pathname__sz(pPath_repo))  );
	VERIFY_ERR_CHECK(  SG_repo__create_repo_instance(pCtx,pvhPartialDescriptor,SG_TRUE,NULL,buf_repo_id,buf_admin_id,&pRepo)  );

	VERIFY_ERR_CHECK(  SG_repo__get_admin_id(pCtx, pRepo, &pszStoredAdminId)  );

	VERIFY_COND("intital adminid == stored adminid", 0 == strcmp(pszStoredAdminId, buf_admin_id));

	VERIFY_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pRepo, &pszStoredRepoId)  );

	VERIFY_COND("initial repoid == stored repoid", 0 == strcmp(pszStoredRepoId, buf_repo_id));

	SG_NULLFREE(pCtx, pszStoredAdminId);
	SG_NULLFREE(pCtx, pszStoredRepoId);
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPath_repo);
	SG_REPO_NULLFREE(pCtx, pRepo);
    SG_NULLFREE(pCtx, pszRepoImpl);

	return;
fail:
	SG_NULLFREE(pCtx, pszStoredAdminId);
	SG_NULLFREE(pCtx, pszStoredRepoId);
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPath_repo);
	SG_REPO_NULLFREE(pCtx, pRepo);
    SG_NULLFREE(pCtx, pszRepoImpl);
}

void u0032_repo__test_delete(SG_context* pCtx)
{
	SG_repo* pRepo = NULL;
	SG_pathname* pPath_repo = NULL;
	SG_vhash* pvhPartialDescriptor = NULL;
    const SG_vhash * pvhRepoDescriptor_ref = NULL;
    SG_vhash* pvhRepoDescriptor = NULL;
    char buf_repo_id[SG_GID_BUFFER_LENGTH];
    char buf_admin_id[SG_GID_BUFFER_LENGTH];
	char* pszRepoImpl = NULL;

    SG_rbtree * pBefore = NULL;
    SG_uint32 count_before = 0;
    SG_rbtree * pDuring = NULL;
    SG_uint32 count_during = 0;
    SG_rbtree * pAfter = NULL;
    SG_uint32 count_after = 0;

    SG_rbtree_iterator * pItBefore = NULL;
    SG_rbtree_iterator * pItAfter = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath_repo)  );
	VERIFY_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath_repo)  );
	VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath_repo, "repo")  );
	SG_ERR_IGNORE(  SG_fsobj__mkdir__pathname(pCtx, pPath_repo)  );

    VERIFY_ERR_CHECK(  SG_dir__list(pCtx, pPath_repo, NULL, NULL, NULL, &pBefore)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, pBefore, &count_before)  );

	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_repo_id, sizeof(buf_repo_id))  );
	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_admin_id, sizeof(buf_admin_id))  );

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx,&pvhPartialDescriptor)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__NEWREPO_DRIVER, NULL, &pszRepoImpl, NULL)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_KEY__STORAGE, pszRepoImpl)  );

	VERIFY_ERR_CHECK(  SG_vhash__add__string__sz(pCtx,pvhPartialDescriptor, SG_RIDESC_FSLOCAL__PATH_PARENT_DIR, SG_pathname__sz(pPath_repo))  );
	VERIFY_ERR_CHECK(  SG_repo__create_repo_instance(pCtx,pvhPartialDescriptor,SG_TRUE,NULL,buf_repo_id,buf_admin_id,&pRepo)  );

    VERIFY_ERR_CHECK(  SG_repo__get_descriptor(pCtx, pRepo, &pvhRepoDescriptor_ref)  );
    VERIFY_ERR_CHECK(  SG_vhash__alloc__copy(pCtx, &pvhRepoDescriptor, pvhRepoDescriptor_ref)  );

    SG_REPO_NULLFREE(pCtx, pRepo);

    VERIFY_ERR_CHECK(  SG_dir__list(pCtx, pPath_repo, NULL, NULL, NULL, &pDuring)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, pDuring, &count_during)  );

    INFOP("", ("]Fwiw, creating the repo seems to have created %d new folder(s) on disk.[][", count_during-count_before));

    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance__unnamed(pCtx, &pvhRepoDescriptor, &pRepo)  );
    VERIFY_ERR_CHECK(  SG_repo__delete_repo_instance(pCtx, &pRepo)  );
    VERIFY_COND("", pRepo==NULL);

    VERIFY_ERR_CHECK(  SG_dir__list(pCtx, pPath_repo, NULL, NULL, NULL, &pAfter)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, pAfter, &count_after)  );

    VERIFY_COND("", count_before==count_after);
    if(count_before==count_after)
    {
        SG_bool ok1;
        SG_bool ok2;
        const char * szBefore;
        const char * szAfter;
        VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pItBefore, pBefore, &ok1, &szBefore, NULL)  );
        VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pItAfter, pAfter, &ok2, &szAfter, NULL)  );
        while(ok1&&ok2)
        {
            VERIFY_COND("", strcmp(szBefore, szAfter)==0);
            VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pItBefore, &ok1, &szBefore, NULL)  );
            VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pItAfter, &ok2, &szAfter, NULL)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pItBefore);
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pItAfter);
    }

    SG_RBTREE_NULLFREE(pCtx, pBefore);
    SG_RBTREE_NULLFREE(pCtx, pDuring);
    SG_RBTREE_NULLFREE(pCtx, pAfter);

	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPath_repo);
    SG_NULLFREE(pCtx, pszRepoImpl);

    return;
fail:
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);
    SG_VHASH_NULLFREE(pCtx, pvhRepoDescriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPath_repo);
	SG_REPO_NULLFREE(pCtx, pRepo);
    SG_NULLFREE(pCtx, pszRepoImpl);
    SG_RBTREE_NULLFREE(pCtx, pBefore);
    SG_RBTREE_NULLFREE(pCtx, pDuring);
    SG_RBTREE_NULLFREE(pCtx, pAfter);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pItBefore);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pItAfter);
}

TEST_MAIN(u0032_repo)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0032_repo__test_ids(pCtx)  );
    BEGIN_TEST(  u0032_repo__test_delete(pCtx)  );

	TEMPLATE_MAIN_END;
}

