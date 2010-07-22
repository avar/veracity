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
 * @file u0054_repo_encodings.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"
#include "unittests_pendingtree.h"

#if 0
static void u0054_repo_encodings__verify_vhash_has_path(
	SG_context* pCtx,
	const SG_vhash* pvh,
	const char* pszPath
	)
{
	SG_uint32 count;
	SG_uint32 i;
	SG_bool bFound = SG_FALSE;

	SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );

	for (i=0; i<count; i++)
	{
		const char* pszgid = NULL;
		const SG_variant* pv = NULL;
		SG_vhash* pvhItem = NULL;
		const char* pszItemPath = NULL;

		SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh, i, &pszgid, &pv)  );
		SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &pvhItem)  );
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhItem, SG_STATUS_ENTRY_KEY__REPO_PATH, &pszItemPath)  );
		if (0 == strcmp(pszPath, pszItemPath))
		{
			bFound = SG_TRUE;
			break;
		}
	}

	if (!bFound)
	{
		SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
	}


fail:
	return;
}
#endif

#if 0
static void u0054_repo_encodings__verify_vhash_sz(
	SG_context* pCtx,
	const SG_vhash* pvh,
	const char* pszKey,
	const char* pszShouldBe
	)
{
	const char* pszIs;

	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, pszKey, &pszIs)  );
	if (0 != strcmp(pszIs, pszShouldBe))
	{
		SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
	}

fail:
	return;
}
#endif

static void u0054_repo_encodings__verify_vhash_count(
	SG_context* pCtx,
	const SG_vhash* pvh,
	SG_uint32 count_should_be
	)
{
	SG_uint32 count;

	SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );
	VERIFY_COND("u0054_repo_encodings__verify_vhash_count", (count == count_should_be));

fail:
	return;
}

static void u0054_repo_encodings__verifytree__dir_has_file(
	SG_context* pCtx,
	const SG_vhash* pvh,
	const char* pszName,
    char* buf_hid,
	SG_uint32 lenBuf
	)
{
	SG_vhash* pvhItem = NULL;
	const char* pszType = NULL;
    const char* psz_hid = NULL;

	VERIFY_COND("u0054_repo_encodings__verifytree__dir_has_file", (lenBuf >= SG_HID_MAX_BUFFER_LENGTH));

	SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, pszName, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhItem, "type", &pszType)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhItem, "hid", &psz_hid)  );

	VERIFY_COND("u0054_repo_encodings__verifytree__dir_has_file", (lenBuf > strlen(psz_hid)));
    SG_ERR_CHECK(  SG_strcpy(pCtx, buf_hid, lenBuf, psz_hid)  );

	VERIFY_COND("u0054_repo_encodings__verifytree__dir_has_file", (0 == strcmp(pszType, "file")));


fail:
	return;
}

static void u0054_repo_encodings__do_get_dir(SG_context* pCtx,SG_repo* pRepo, const char* pszidHidTreeNode, SG_vhash** ppResult)
{
	SG_uint32 count;
	SG_uint32 i;
	SG_treenode* pTreenode = NULL;
	SG_vhash* pvhDir = NULL;
	SG_vhash* pvhSub = NULL;
	SG_vhash* pvhSubDir = NULL;

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhDir)  );
	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszidHidTreeNode, &pTreenode)  );
	SG_ERR_CHECK(  SG_treenode__count(pCtx, pTreenode, &count)  );
	for (i=0; i<count; i++)
	{
		const SG_treenode_entry* pEntry = NULL;
		SG_treenode_entry_type type;
		const char* pszName = NULL;
		const char* pszidHid = NULL;
		const char* pszgid = NULL;

		SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx, pTreenode, i, &pszgid, &pEntry)  );
		SG_ERR_CHECK(  SG_treenode_entry__get_entry_type(pCtx, pEntry, &type)  );
		SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx, pEntry, &pszName)  );
		SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, pEntry, &pszidHid)  );

		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhSub)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhSub, "hid", pszidHid)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhSub, "gid", pszgid)  );

		if (SG_TREENODEENTRY_TYPE_DIRECTORY == type)
		{
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhSub, "type", "directory")  );

			SG_ERR_CHECK(  u0054_repo_encodings__do_get_dir(pCtx, pRepo, pszidHid, &pvhSubDir)  );
			SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhSub, "dir", &pvhSubDir)  );
		}
		else if (SG_TREENODEENTRY_TYPE_REGULAR_FILE == type)
		{
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhSub, "type", "file")  );

		}
		else if (SG_TREENODEENTRY_TYPE_SYMLINK == type)
		{
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhSub, "type", "symlink")  );

		}
		else
		{
			SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
		}

		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhDir, pszName, &pvhSub)  );
	}

	SG_TREENODE_NULLFREE(pCtx, pTreenode);
	pTreenode = NULL;

	*ppResult = pvhDir;

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhDir);
	SG_VHASH_NULLFREE(pCtx, pvhSub);

}

static void u0054_repo_encodings__get_entire_repo_manifest(
	SG_context* pCtx,
	const char* pszRidescName,
	SG_vhash** ppResult
	)
{
	SG_vhash* pvh = NULL;
	SG_repo* pRepo = NULL;
	SG_rbtree* pIdsetLeaves = NULL;
	SG_uint32 count_leaves = 0;
	SG_changeset* pcs = NULL;
	const char* pszidUserSuperRoot = NULL;
	SG_rbtree_iterator* pIterator = NULL;
	SG_bool b = SG_FALSE;
	const char* pszLeaf = NULL;
	SG_treenode* pTreenode = NULL;
	const SG_treenode_entry* pEntry = NULL;
	const char* pszidHidActualRoot = NULL;
	SG_uint32 count = 0;

	/*
	 * Fetch the descriptor by its given name and use it to connect to
	 * the repo.
	 */
	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, pszRidescName, &pRepo)  );

	/*
	 * This routine currently only works if there is exactly one leaf
	 * in the repo.  A smarter version of this (later) will allow the
	 * desired dagnode to be specified either by its hex id or by a
	 * named branch or a label.
	 */
	SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo,SG_DAGNUM__VERSION_CONTROL,&pIdsetLeaves)  );
	SG_ERR_CHECK(  SG_rbtree__count(pCtx, pIdsetLeaves, &count_leaves)  );

	SG_ARGCHECK_RETURN(count_leaves == 1, count_leaves);

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, pIdsetLeaves, &b, &pszLeaf, NULL)  );
	SG_rbtree__iterator__free(pCtx, pIterator);
	pIterator = NULL;

	/*
	 * Load the desired changeset from the repo so we can look up the
	 * id of its user root directory
	 */
	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, pszLeaf, &pcs)  );
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs, &pszidUserSuperRoot)  );

	/*
	 * What we really want here is not the super-root, but the actual root.
	 * So let's jump down one directory.
	 */
	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszidUserSuperRoot, &pTreenode)  );
	SG_ERR_CHECK(  SG_treenode__count(pCtx, pTreenode, &count)  );
	VERIFY_COND("count should be 1", (count == 1));

	SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx, pTreenode, 0, NULL, &pEntry)  );
	SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, pEntry, &pszidHidActualRoot)  );

	/*
	 * Retrieve everything.  A new directory will be created in cwd.
	 * Its name will be the name of the descriptor.
	 */
	SG_ERR_CHECK(  u0054_repo_encodings__do_get_dir(pCtx, pRepo, pszidHidActualRoot, &pvh)  );

	SG_TREENODE_NULLFREE(pCtx, pTreenode);
	pTreenode = NULL;

	SG_CHANGESET_NULLFREE(pCtx, pcs);
	pcs = NULL;
	SG_RBTREE_NULLFREE(pCtx, pIdsetLeaves);
	pIdsetLeaves = NULL;
	SG_REPO_NULLFREE(pCtx, pRepo);
	pRepo = NULL;

	*ppResult = pvh;

fail:
	return;
}


static void u0054_repo_encodings__commit_all(
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

static void u0054_repo_encodings__create_file__numbers(
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
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

static void u0054_repo_encodings__append_to_file__numbers(
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

fail:
	return;
}

static void u0054_repo_encodings__scan(
	SG_context* pCtx,
	const SG_pathname* pPathWorkingDir
	)
{
	SG_pendingtree* pPendingTree = NULL;

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );

	SG_ERR_CHECK(  SG_pendingtree__scan(pCtx, pPendingTree, SG_TRUE, NULL, 0, NULL, 0)  );

	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	return;

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

}

static int u0054_repo_encodings_test__1(SG_context* pCtx,SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	char bufOtherName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathGetDir = NULL;
	SG_pathname* pPathFile = NULL;
    SG_bool bExists = SG_FALSE;
	SG_vhash* pvh = NULL;
    SG_dagnode* pdn = NULL;
    const char* psz_hid_cs = NULL;
    SG_repo* pRepo = NULL;
	char buf_hid_1[SG_HID_MAX_BUFFER_LENGTH];
	char buf_hid_2[SG_HID_MAX_BUFFER_LENGTH];
    SG_blob_encoding blob_encoding;
    char* psz_hid_vcdiff_reference = NULL;
    SG_uint64 len_encoded = 0;
    SG_uint64 len_full = 0;
	SG_repo_tx_handle* pTx = NULL;
    SG_bool b_supports_change_blob_encoding = SG_FALSE;
    SG_bool b_supports_vacuum = SG_FALSE;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add stuff */
	VERIFY_ERR_CHECK(  u0054_repo_encodings__create_file__numbers(pCtx, pPathWorkingDir, "a", 2000)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0054_repo_encodings__commit_all(pCtx, pPathWorkingDir, &pdn)  );
    VERIFY_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pdn, &psz_hid_cs)  );
    SG_DAGNODE_NULLFREE(pCtx, pdn);

	/* Load a listing of everything in the repo so we can verify */
	VERIFY_ERR_CHECK(  u0054_repo_encodings__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  u0054_repo_encodings__verify_vhash_count(pCtx, pvh, 1)  );
	VERIFY_ERR_CHECK(  u0054_repo_encodings__verifytree__dir_has_file(pCtx, pvh, "a", buf_hid_1, sizeof(buf_hid_1))  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	VERIFY_ERR_CHECK(  u0054_repo_encodings__append_to_file__numbers(pCtx, pPathWorkingDir, "a", 10)  );
	VERIFY_ERR_CHECK(  u0054_repo_encodings__scan(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0054_repo_encodings__commit_all(pCtx, pPathWorkingDir, &pdn)  );
    VERIFY_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pdn, &psz_hid_cs)  );
    SG_DAGNODE_NULLFREE(pCtx, pdn);

	VERIFY_ERR_CHECK(  u0054_repo_encodings__get_entire_repo_manifest(pCtx, bufName, &pvh)  );
	VERIFY_ERR_CHECK(  u0054_repo_encodings__verify_vhash_count(pCtx, pvh, 1)  );
	VERIFY_ERR_CHECK(  u0054_repo_encodings__verifytree__dir_has_file(pCtx, pvh, "a", buf_hid_2, sizeof(buf_hid_2))  );
	SG_VHASH_NULLFREE(pCtx, pvh);

    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );

    VERIFY_ERR_CHECK(  SG_repo__query_implementation(pCtx, pRepo, SG_REPO__QUESTION__BOOL__SUPPORTS_CHANGE_BLOB_ENCODING, &b_supports_change_blob_encoding, NULL, NULL, 0, NULL)  );

    if (b_supports_change_blob_encoding)
    {
        // We want to make sure we test full->zlib and zlib->full regardless of the repo's compression policy...
        VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
        VERIFY_ERR_CHECK(  SG_repo__change_blob_encoding(pCtx, pRepo, pTx, buf_hid_1, SG_BLOBENCODING__ZLIB, NULL, &blob_encoding, NULL, &len_encoded, &len_full)  );
        VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );
        VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
        VERIFY_ERR_CHECK(  SG_repo__change_blob_encoding(pCtx, pRepo, pTx, buf_hid_1, SG_BLOBENCODING__FULL, NULL, &blob_encoding, NULL, &len_encoded, &len_full)  );
        VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );
        // ...so we run these tests twice.
        VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
        VERIFY_ERR_CHECK(  SG_repo__change_blob_encoding(pCtx, pRepo, pTx, buf_hid_1, SG_BLOBENCODING__ZLIB, NULL, &blob_encoding, NULL, &len_encoded, &len_full)  );
        VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );
        VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
        VERIFY_ERR_CHECK(  SG_repo__change_blob_encoding(pCtx, pRepo, pTx, buf_hid_1, SG_BLOBENCODING__FULL, NULL, &blob_encoding, NULL, &len_encoded, &len_full)  );
        VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );

        VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
        VERIFY_ERR_CHECK(  SG_repo__change_blob_encoding(pCtx, pRepo, pTx, buf_hid_2, SG_BLOBENCODING__VCDIFF, buf_hid_1, &blob_encoding, &psz_hid_vcdiff_reference, &len_encoded, &len_full)  );
        VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );
        SG_NULLFREE(pCtx, psz_hid_vcdiff_reference);

        VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
        VERIFY_ERR_CHECK(  SG_repo__change_blob_encoding(pCtx, pRepo, pTx, buf_hid_1, SG_BLOBENCODING__ZLIB, NULL, &blob_encoding, NULL, &len_encoded, &len_full)  );
        VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );
    }

    VERIFY_ERR_CHECK(  SG_repo__query_implementation(pCtx, pRepo, SG_REPO__QUESTION__BOOL__SUPPORTS_CHANGE_BLOB_ENCODING, &b_supports_vacuum, NULL, NULL, 0, NULL)  );

    if (b_supports_vacuum)
    {
        VERIFY_ERR_CHECK(  SG_repo__vacuum(pCtx, pRepo)  );
    }

    SG_REPO_NULLFREE(pCtx, pRepo);

	/* Now get it */
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufOtherName, sizeof(bufOtherName), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathGetDir, pPathTopDir, bufOtherName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathGetDir)  );
	VERIFY_ERR_CHECK(  unittests_workingdir__create_and_get(pCtx, bufName, pPathGetDir, SG_FALSE, NULL)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPathGetDir, "a")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathFile, &bExists, NULL, NULL)  );
	VERIFY_COND("here", (bExists));

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathGetDir);

	return 1;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathGetDir);

	return 0;
}

TEST_MAIN(u0054_repo_encodings)
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;

	TEMPLATE_MAIN_START;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathTopDir,bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathTopDir)  );

	BEGIN_TEST(  u0054_repo_encodings_test__1(pCtx, pPathTopDir)  );

	/* TODO rm -rf the top dir */

	// fall-thru to common cleanup

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);

	TEMPLATE_MAIN_END;
}

