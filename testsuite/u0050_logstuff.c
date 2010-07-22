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
 * @file u0050_logstuff.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"
#include "unittests_pendingtree.h"

void u0050_logstuff__commit_all(
	SG_context * pCtx,
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

void u0050_logstuff__create_file__numbers(
	SG_context * pCtx,
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

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

int u0050_logstuff_test__1(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_vhash* pvh = NULL;
    SG_dagnode* pdn = NULL;
    const char* psz_hid_cs = NULL;
    SG_repo* pRepo = NULL;
    SG_uint32 count;
    SG_rbtree* prb = NULL;
    SG_varray* pva = NULL;
    SG_rbtree* prb_reversed = NULL;
    const char* psz_val = NULL;
    SG_audit q;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add stuff */
	VERIFY_ERR_CHECK(  u0050_logstuff__create_file__numbers(pCtx, pPathWorkingDir, "aaa", 20)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0050_logstuff__commit_all(pCtx, pPathWorkingDir, &pdn)  );

    VERIFY_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pdn, &psz_hid_cs)  );

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );

#define MY_COMMENT "The name of this new file sucks!  What kind of a name is 'aaa'?"

    VERIFY_ERR_CHECK(  SG_audit__init(pCtx, &q, pRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
    VERIFY_ERR_CHECK(  SG_vc_comments__add(pCtx, pRepo, psz_hid_cs, MY_COMMENT, &q)  );
    VERIFY_ERR_CHECK(  SG_vc_stamps__add(pCtx, pRepo, psz_hid_cs, "crap", &q)  );
    VERIFY_ERR_CHECK(  SG_vc_tags__add(pCtx, pRepo, psz_hid_cs, "tcrap", &q)  );

    VERIFY_ERR_CHECK(  SG_vc_comments__lookup(pCtx, pRepo, psz_hid_cs, &pva)  );
    VERIFY_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    VERIFY_COND("count", (1 == count));
    VERIFY_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, 0, &pvh)  );
    VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "text", &psz_val)  );
    VERIFY_COND("match", (0 == strcmp(psz_val, MY_COMMENT))  );
    SG_VARRAY_NULLFREE(pCtx, pva);

    VERIFY_ERR_CHECK(  SG_vc_stamps__lookup(pCtx, pRepo, psz_hid_cs, &pva)  );
    VERIFY_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    VERIFY_COND("count", (1 == count));
    VERIFY_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, 0, &pvh)  );
    VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "stamp", &psz_val)  );
    VERIFY_COND("match", (0 == strcmp(psz_val, "crap"))  );
    SG_VARRAY_NULLFREE(pCtx, pva);

    VERIFY_ERR_CHECK(  SG_vc_tags__lookup(pCtx, pRepo, psz_hid_cs, &pva)  );
    VERIFY_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    VERIFY_COND("count", (1 == count));
    VERIFY_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, 0, &pvh)  );
    VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "tag", &psz_val)  );
    VERIFY_COND("match", (0 == strcmp(psz_val, "tcrap"))  );
    SG_VARRAY_NULLFREE(pCtx, pva);

    VERIFY_ERR_CHECK(  SG_vc_tags__list(pCtx, pRepo, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFY_COND("count", (1 == count));
    VERIFY_ERR_CHECK(  SG_vc_tags__build_reverse_lookup(pCtx, prb, &prb_reversed)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb_reversed, &count)  );
    VERIFY_COND("count", (1 == count));

    {
        const char* psz_my_key = NULL;
        const char* psz_my_val = NULL;
        SG_bool b;

        VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, NULL, prb_reversed, &b, &psz_my_key, (void**) &psz_my_val)  );
        VERIFY_COND("ok", (0 == strcmp(psz_my_val, "tcrap"))  );
        VERIFY_COND("ok", (0 == strcmp(psz_my_key, psz_hid_cs))  );
    }
    SG_RBTREE_NULLFREE(pCtx, prb_reversed);

    SG_RBTREE_NULLFREE(pCtx, prb);

    VERIFY_ERR_CHECK(  SG_vc_tags__add(pCtx, pRepo, psz_hid_cs, "whatever", &q)  );

    VERIFY_ERR_CHECK(  SG_vc_tags__lookup(pCtx, pRepo, psz_hid_cs, &pva)  );
    VERIFY_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    VERIFY_COND("count", (2 == count));
    SG_VARRAY_NULLFREE(pCtx, pva);

    VERIFY_ERR_CHECK(  SG_vc_tags__list(pCtx, pRepo, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFY_COND("count", (2 == count));

    VERIFY_ERR_CHECK(  SG_vc_tags__build_reverse_lookup(pCtx, prb, &prb_reversed)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb_reversed, &count)  );
    VERIFY_COND("count", (1 == count));

    {
        const char* psz_my_key = NULL;
        const char* psz_my_val = NULL;
        SG_bool b;

        VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, NULL, prb_reversed, &b, &psz_my_key, (void**) &psz_my_val)  );
        VERIFY_COND("ok", (0 == strcmp(psz_my_key, psz_hid_cs))  );
        /* we don't know whether psz_my_val is tcrap or whatever. */
        // VERIFY_COND("ok", (0 == strcmp(psz_my_val, "tcrap"))  );
    }
    SG_RBTREE_NULLFREE(pCtx, prb_reversed);

    SG_RBTREE_NULLFREE(pCtx, prb);

    {
        const char* psz_remove = "whatever";

        VERIFY_ERR_CHECK(  SG_vc_tags__remove(pCtx, pRepo, &q, 1, &psz_remove)  );
        /* Note that by removing whatever, we are bringing the tags list back
         * to a state where it has been before (just tcrap).  This changeset in
         * the tags table will have its own csid, because the parentage is
         * different, but it's root idtrie HID will be the same as a previous
         * node. */
    }

    VERIFY_ERR_CHECK(  SG_vc_tags__lookup(pCtx, pRepo, psz_hid_cs, &pva)  );
    VERIFY_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    VERIFY_COND("count", (1 == count));
    SG_VARRAY_NULLFREE(pCtx, pva);

    VERIFY_ERR_CHECK(  SG_vc_tags__list(pCtx, pRepo, &prb)  );
    VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
    VERIFY_COND("count", (1 == count));
    SG_RBTREE_NULLFREE(pCtx, prb);

    SG_REPO_NULLFREE(pCtx, pRepo);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	return 1;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	return 0;
}

TEST_MAIN(u0050_logstuff)
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;

	TEMPLATE_MAIN_START;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx,&pPathTopDir,bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx,pPathTopDir)  );

	BEGIN_TEST(  u0050_logstuff_test__1(pCtx,pPathTopDir)  );

	/* TODO rm -rf the top dir */

	// fall-thru to common cleanup

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);
	TEMPLATE_MAIN_END;
}

