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
 * @file u0053_sync.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"
#include "unittests_pendingtree.h"

static void u0053_sync__verify_vhash_count(
	SG_context * pCtx,
	const SG_vhash* pvh,
	SG_uint32 count_should_be
	)
{
	SG_uint32 count;

	SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );
	VERIFY_COND("u0053_sync__verify_vhash_count", (count == count_should_be));

	return;

fail:
	return;
}

static void u0053_sync__verifytree__dir_has_file(
	SG_context * pCtx,
	const SG_vhash* pvh,
	const char* pszName
	)
{
	SG_vhash* pvhItem = NULL;
	const char* pszType = NULL;

	SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, pszName, &pvhItem)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhItem, "type", &pszType)  );
	VERIFY_COND("u0053_sync__verifytree__dir_has_file", (0 == strcmp(pszType, "file")));

	return;

fail:
	return;
}

static void u0053_sync__do_get_dir(SG_context * pCtx, SG_repo* pRepo, const char* pszidHidTreeNode, SG_vhash** ppResult)
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
		char* pszgid = NULL;

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

			SG_ERR_CHECK(  u0053_sync__do_get_dir(pCtx, pRepo, pszidHid, &pvhSubDir)  );
			SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhSub, "dir", pvhSubDir)  );
			pvhSubDir = NULL;
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

		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhDir, pszName, pvhSub)  );
	}

	SG_TREENODE_NULLFREE(pCtx, pTreenode);
	pTreenode = NULL;

	*ppResult = pvhDir;

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhDir);
	SG_VHASH_NULLFREE(pCtx, pvhSub);
	return;
}

void u0053_sync__get_entire_repo_manifest(
	SG_context * pCtx,
	const char* pszRidescName,
	SG_vhash** ppResult
	)
{
	SG_vhash* pvh = NULL;
	SG_repo* pRepo = NULL;
	SG_vhash* pvhRepoDescriptor = NULL;
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
	SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, pszRidescName, &pvhRepoDescriptor)  );
	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo, pvhRepoDescriptor)  );

	/*
	 * This routine currently only works if there is exactly one leaf
	 * in the repo.  A smarter version of this (later) will allow the
	 * desired dagnode to be specified either by its hex id or by a
	 * named branch or a label.
	 */
	SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo,SG_DAGNUM__VERSION_CONTROL,&pIdsetLeaves)  );
	SG_ERR_CHECK(  SG_rbtree__count(pCtx, pIdsetLeaves, &count_leaves)  );
	if (count_leaves != 1)
	{
		SG_ERR_THROW( SG_ERR_INVALIDARG ); /* TODO better err code here */
	}

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, pIdsetLeaves, &b, &pszLeaf, NULL)  );
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);

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
	 * Retrieve everything.
	 */
	SG_ERR_CHECK(  u0053_sync__do_get_dir(pCtx, pRepo, pszidHidActualRoot, &pvh)  );

	SG_TREENODE_NULLFREE(pCtx, pTreenode);
	SG_CHANGESET_NULLFREE(pCtx, pcs);
	SG_RBTREE_NULLFREE(pCtx, pIdsetLeaves);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_VHASH_NULLFREE(pCtx, pvhRepoDescriptor);

	*ppResult = pvh;

	return;

fail:
	return;
}

void u0053_sync__commit_all(
	SG_context * pCtx,
	const SG_pathname* pPathWorkingDir,
    SG_dagnode** ppdn
	)
{
	SG_pendingtree* pPendingTree = NULL;
    SG_dagnode* pdn = NULL;
    SG_whowhenwhere q;

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );

    SG_ERR_CHECK(  SG_whowhenwhere__init__now(pCtx, &q)  );
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

void u0053_sync__create_file__numbers(
	SG_context * pCtx,
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
	SG_ERR_CHECK(  SG_file__close(pCtx, pFile)  );
	pFile = NULL;

	SG_PATHNAME_NULLFREE(pCtx, pPathFile);

	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	return;
}

#if 0
static FN__sg_fetch_dagnode my_fetch_dagnodes_from_repo;

SG_error my_fetch_dagnodes_from_repo(void * pCtxFetchDagnode, const char * szHidDagnode, SG_uint32 iDagNum, SG_dagnode ** ppDagnode)
{
	return SG_repo__fetch_dagnode((SG_repo *)pCtxFetchDagnode,szHidDagnode,iDagNum,ppDagnode);
}

int u0053_sync_test__fragball(SG_pathname* pPathTopDir, SG_uint32 lines)
{
	SG_error err;
	char buf_name_1[SG_TID_MAX_BUFFER_LENGTH];
	char buf_dagnum[100];		// this is for a "%d" sprintf.
	char buf_filename[SG_TID_MAX_BUFFER_LENGTH + 20];	// "%s.zip"
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_fragball = NULL;
	SG_vhash* pvh = NULL;
    SG_repo* pRepo1 = NULL;
    SG_vhash* pvh_descriptor = NULL;
    SG_rbtree* prb_frags = NULL;
    SG_rbtree* prb_leaves = NULL;
    SG_dagnode* pdn = NULL;
    SG_dagfrag* pFrag = NULL;
    const char* psz_hid_cs = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_name_1, sizeof(buf_name_1), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(&pPathWorkingDir, pPathTopDir, buf_name_1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pPathWorkingDir)  );

    /* add stuff */
	VERIFY_ERR_CHECK(  u0053_sync__create_file__numbers(pPathWorkingDir, "aaa", lines)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(buf_name_1, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0053_sync__commit_all(pPathWorkingDir, &pdn)  );

    VERIFY_ERR_CHECK(  SG_closet__descriptors__get(buf_name_1, &pvh_descriptor)  );
    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(&pRepo1, pvh_descriptor)  );
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pRepo1, SG_DAGNUM__VERSION_CONTROL, &prb_leaves)  );

    SG_ERR_CHECK(  SG_dagfrag__alloc(&pFrag, SG_DAGNUM__VERSION_CONTROL, my_fetch_dagnodes_from_repo, pRepo1)  );
    SG_ERR_CHECK(  SG_dagfrag__add_leaves(pFrag, prb_leaves, 2)  );
    SG_RBTREE_NULLFREE(pCtx, prb_leaves);

    VERIFY_ERR_CHECK(  SG_dagnode__get_id_ref(pdn, &psz_hid_cs)  );

    VERIFY_ERR_CHECK(  SG_sprintf(buf_filename, sizeof(buf_filename), "%s.zip", psz_hid_cs)  );

    /* write the zip file */
    VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(&prb_frags)  );
    VERIFY_ERR_CHECK(  SG_dagnum__to_string(SG_DAGNUM__VERSION_CONTROL, buf_dagnum, sizeof(buf_dagnum))  );
    VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(prb_frags, buf_dagnum, pFrag)  );
    VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(&pPath_fragball, pPathTopDir, buf_filename)  );
    VERIFY_ERR_CHECK(  SG_fragball__write(pRepo1, pPath_fragball, prb_frags)  );
    SG_PATHNAME_NULLFREE(pCtx, pPath_fragball);
    SG_RBTREE_NULLFREE(pCtx, prb_frags);
    SG_RBTREE_NULLFREE(pCtx, prb_leaves);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
    SG_DAGFRAG_NULLFREE(pCtx, pFrag);

    SG_REPO_NULLFREE(pCtx, pRepo1);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return 1;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return 0;
}
#endif

int u0053_sync_test__stringarray_grow(SG_context * pCtx)
{
    SG_stringarray* psa = NULL;
    SG_uint32 i;
    SG_uint32 count;

    VERIFY_ERR_CHECK(  SG_stringarray__alloc(pCtx, &psa, 4)  );

    for (i=0; i<20; i++)
    {
        VERIFY_ERR_CHECK(  SG_stringarray__add(pCtx, psa, "hello")  );
    }

    VERIFY_ERR_CHECK(  SG_stringarray__count(pCtx, psa, &count)  );
    VERIFY_COND("count", (20 == count));

    SG_STRINGARRAY_NULLFREE(pCtx, psa);

    return 1;

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa);
    return 0;
}

int u0053_sync_test__simple_sync_maxfilesize(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char buf_name_1[SG_TID_MAX_BUFFER_LENGTH];
	char buf_name_2[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_file = NULL;
	SG_vhash* pvh = NULL;
    SG_repo* pRepo1 = NULL;
    SG_repo* pRepo2 = NULL;
    SG_vhash* pvh_descriptor = NULL;
	SG_vhash* pvhItem = NULL;
	const char* psz_hid_file = NULL;
    SG_uint64 len1 = 0;
    SG_uint64 len2 = 0;
    SG_sync_options opt;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_name_1, sizeof(buf_name_1), 32)  );
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_name_2, sizeof(buf_name_2), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, buf_name_1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add stuff */
	VERIFY_ERR_CHECK(  u0053_sync__create_file__numbers(pCtx, pPathWorkingDir, "aaa", 1000)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_file, pPathWorkingDir, "aaa")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath_file, &len1, NULL)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, buf_name_1, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0053_sync__commit_all(pCtx, pPathWorkingDir, NULL)  );

    /* open that repo */
    VERIFY_ERR_CHECK(  SG_closet__descriptors__get(pCtx, buf_name_1, &pvh_descriptor)  );
    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo1, pvh_descriptor)  );
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);

    /* create a clone and open it */
    VERIFY_ERR_CHECK(  SG_repo__create_empty_clone(pCtx, buf_name_1, buf_name_2)  );

    VERIFY_ERR_CHECK(  SG_closet__descriptors__get(pCtx, buf_name_2, &pvh_descriptor)  );
    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo2, pvh_descriptor)  );

    /* sync from the first instance to the second */
    VERIFY_ERR_CHECK(  SG_sync__init_options(pCtx, &opt)  );
    opt.max_blob_size = 1000;
    VERIFY_ERR_CHECK(  SG_sync__oneway__all_dags(pCtx, pRepo1, pRepo2, &opt)  );

    /* close both repos */
    SG_REPO_NULLFREE(pCtx, pRepo2);
    SG_REPO_NULLFREE(pCtx, pRepo1);

    /* retrieve the manifest from repo 1 and make sure it contains the file */
	VERIFY_ERR_CHECK(  u0053_sync__get_entire_repo_manifest(pCtx, buf_name_1, &pvh)  );
	VERIFY_ERR_CHECK(  u0053_sync__verify_vhash_count(pCtx, pvh, 1)  );
	VERIFY_ERR_CHECK(  u0053_sync__verifytree__dir_has_file(pCtx, pvh, "aaa")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

    /* retrieve the manifest from repo 2 and make sure it ALSO contains the file */
	VERIFY_ERR_CHECK(  u0053_sync__get_entire_repo_manifest(pCtx, buf_name_2, &pvh)  );
	VERIFY_ERR_CHECK(  u0053_sync__verify_vhash_count(pCtx, pvh, 1)  );
	VERIFY_ERR_CHECK(  u0053_sync__verifytree__dir_has_file(pCtx, pvh, "aaa")  );

    /* make sure the repo does NOT actually have the blob for this file, since
     * it was too large for the size limit option shown above */
	VERIFY_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, "aaa", &pvhItem)  );
	VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhItem, "hid", &psz_hid_file)  );

    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo2, pvh_descriptor)  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__fetch_blob_into_memory(pCtx, pRepo2, psz_hid_file, NULL, &len2),
										  SG_ERR_BLOB_NOT_FOUND  );
    SG_REPO_NULLFREE(pCtx, pRepo2);

	SG_VHASH_NULLFREE(pCtx, pvh);

    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPath_file);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return 1;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return 0;
}

void u0053_sync__query(SG_context * pCtx, SG_repo* pRepo)
{
    SG_varray* pcrit = NULL;
    SG_stringarray* psa_results = NULL;
    SG_uint32 count = 0;

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pcrit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "x")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "1")  );
    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pRepo, SG_DAGNUM__TESTING__DB, NULL, pcrit, &psa_results)  );
    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_results, &count)  );
    VERIFY_COND("count", (count==2));

    SG_STRINGARRAY_NULLFREE(pCtx, psa_results);
    SG_VARRAY_NULLFREE(pCtx, pcrit);

    return;

fail:
    return;
}

int u0053_sync_test__sync_db_then_query(SG_context * pCtx)
{
    SG_repo* pRepo1 = NULL;
    SG_repo* pRepo2 = NULL;
	char buf_descriptor_name_1[SG_TID_MAX_BUFFER_LENGTH];
	char buf_descriptor_name_2[SG_TID_MAX_BUFFER_LENGTH];
    SG_vhash* pvh_descriptor = NULL;
    SG_pendingdb* ptx = NULL;
    SG_dbrecord* prec = NULL;
    SG_changeset* pcs_tree = NULL;
    char* psz_gid_tree_root_directory = NULL;
    SG_changeset* pcs = NULL;
    SG_dagnode* pdn = NULL;
    SG_whowhenwhere q;
    char buf_admin_id[SG_GID_BUFFER_LENGTH];
	const char * pszHashMethod;

    SG_ERR_CHECK(  SG_gid__generate(pCtx, buf_admin_id, sizeof(buf_admin_id))  );

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_descriptor_name_1, sizeof(buf_descriptor_name_1), 32)  );
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_descriptor_name_2, sizeof(buf_descriptor_name_2), 32)  );

	VERIFY_ERR_CHECK(  SG_localsettings__get_string__sz(pCtx, "repo.hashmethod", &pszHashMethod, NULL)  );
    VERIFY_ERR_CHECK(  SG_repo__create__completely_new__closet(pCtx, buf_admin_id, pszHashMethod, buf_descriptor_name_1, &pcs_tree, &psz_gid_tree_root_directory)  );
    SG_CHANGESET_NULLFREE(pCtx, pcs_tree);
    SG_NULLFREE(pCtx, psz_gid_tree_root_directory);
    VERIFY_ERR_CHECK(  SG_closet__descriptors__get(pCtx, buf_descriptor_name_1, &pvh_descriptor)  );
    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo1, pvh_descriptor)  );
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);

    VERIFY_ERR_CHECK(  SG_pendingdb__alloc(pCtx, pRepo1, SG_DAGNUM__TESTING__DB, NULL, &ptx)  );

    VERIFY_ERR_CHECK(  SG_dbrecord__alloc(pCtx, &prec)  );
    VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "x", "1")  );
    VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "y", "2")  );
    VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "z", "3")  );
    VERIFY_ERR_CHECK(  SG_pendingdb__add(pCtx, ptx, prec)  );
    prec = NULL;

    VERIFY_ERR_CHECK(  SG_dbrecord__alloc(pCtx, &prec)  );
    VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "x", "1")  );
    VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "y", "4")  );
    VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "z", "3")  );
    VERIFY_ERR_CHECK(  SG_pendingdb__add(pCtx, ptx, prec)  );
    prec = NULL;

    VERIFY_ERR_CHECK(  SG_dbrecord__alloc(pCtx, &prec)  );
    VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "x", "2")  );
    VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "y", "4")  );
    VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "z", "3")  );
    VERIFY_ERR_CHECK(  SG_pendingdb__add(pCtx, ptx, prec)  );
    prec = NULL;

    VERIFY_ERR_CHECK(  SG_whowhenwhere__init__now(pCtx, &q)  );
    VERIFY_ERR_CHECK(  SG_pendingdb__commit(pCtx, ptx, &q, &pcs, &pdn)  );

    /* create a clone and open it */
    VERIFY_ERR_CHECK(  SG_repo__create_empty_clone(pCtx, buf_descriptor_name_1, buf_descriptor_name_2)  );

    VERIFY_ERR_CHECK(  SG_closet__descriptors__get(pCtx, buf_descriptor_name_2, &pvh_descriptor)  );
    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo2, pvh_descriptor)  );
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);

    /* sync from the first instance to the second */
    VERIFY_ERR_CHECK(  SG_sync__oneway__all_dags(pCtx, pRepo1, pRepo2, NULL)  );

    VERIFY_ERR_CHECK(  u0053_sync__query(pCtx, pRepo1)  );
    VERIFY_ERR_CHECK(  u0053_sync__query(pCtx, pRepo2)  );

    SG_PENDINGDB_NULLFREE(pCtx, ptx);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
    SG_REPO_NULLFREE(pCtx, pRepo2);
    SG_REPO_NULLFREE(pCtx, pRepo1);

    return 1;

fail:
    return 0;
}

#if 0
int u0053_sync_test__simple_sync_all__with_fragball(SG_pathname* pPathTopDir)
{
	SG_error err;
	char buf_name_1[SG_TID_MAX_BUFFER_LENGTH];
	char buf_name_2[SG_TID_MAX_BUFFER_LENGTH];
	char buf_fragball_name[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPath_fragball = NULL;
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_file = NULL;
	SG_vhash* pvh = NULL;
    SG_repo* pRepo1 = NULL;
    SG_repo* pRepo2 = NULL;
    SG_vhash* pvh_descriptor = NULL;
	SG_vhash* pvhItem = NULL;
	const char* psz_hid_file = NULL;
    SG_uint64 len1 = 0;
    SG_uint64 len2 = 0;
    SG_rbtree* prb_leaves = NULL;
    SG_dagfrag* pFrag = NULL;
    const char* psz_hid_cs = NULL;
    SG_rbtree* prb_frags = NULL;
    char buf_dagnum[100];				// enough for a "%d" sprintf
    SG_dagnode* pdn = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_name_1, sizeof(buf_name_1), 32)  );
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_name_2, sizeof(buf_name_2), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(&pPathWorkingDir, pPathTopDir, buf_name_1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pPathWorkingDir)  );

    /* add stuff */
	VERIFY_ERR_CHECK(  u0053_sync__create_file__numbers(pPathWorkingDir, "aaa", 1000)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(&pPath_file, pPathWorkingDir, "aaa")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pPath_file, &len1, NULL)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(buf_name_1, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0053_sync__commit_all(pPathWorkingDir, &pdn)  );

    /* open that repo */
    VERIFY_ERR_CHECK(  SG_closet__descriptors__get(buf_name_1, &pvh_descriptor)  );
    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(&pRepo1, pvh_descriptor)  );
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);

    /* create a clone and open it */
    VERIFY_ERR_CHECK(  SG_repo__create_empty_clone(buf_name_1, buf_name_2)  );

    VERIFY_ERR_CHECK(  SG_closet__descriptors__get(buf_name_2, &pvh_descriptor)  );
    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(&pRepo2, pvh_descriptor)  );

    /* sync from the first instance to the second */
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_fragball_name, sizeof(buf_fragball_name), 32)  );

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pRepo1, SG_DAGNUM__VERSION_CONTROL, &prb_leaves)  );

    SG_ERR_CHECK(  SG_dagfrag__alloc(&pFrag, SG_DAGNUM__VERSION_CONTROL, my_fetch_dagnodes_from_repo, pRepo1)  );
    SG_ERR_CHECK(  SG_dagfrag__add_leaves(pFrag, prb_leaves, 2)  );
    SG_RBTREE_NULLFREE(pCtx, prb_leaves);

    VERIFY_ERR_CHECK(  SG_dagnode__get_id_ref(pdn, &psz_hid_cs)  );

    /* write the fragball file */
    VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(&prb_frags)  );
    VERIFY_ERR_CHECK(  SG_dagnum__to_string(SG_DAGNUM__VERSION_CONTROL, buf_dagnum, sizeof(buf_dagnum))  );
    VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(prb_frags, buf_dagnum, pFrag)  );
    VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(&pPath_fragball, pPathTopDir, buf_fragball_name)  );
    VERIFY_ERR_CHECK(  SG_fragball__write(pRepo1, pPath_fragball, prb_frags)  );
    VERIFY_ERR_CHECK(  SG_fragball__slurp(pRepo2, pPath_fragball)  );

    SG_PATHNAME_NULLFREE(pCtx, pPath_fragball);
    SG_RBTREE_NULLFREE(pCtx, prb_frags);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
    SG_DAGFRAG_NULLFREE(pCtx, pFrag);

    /* close both repos */
    SG_REPO_NULLFREE(pCtx, pRepo2);
    SG_REPO_NULLFREE(pCtx, pRepo1);

    /* retrieve the manifest from repo 1 and make sure it contains the file */
	VERIFY_ERR_CHECK(  u0053_sync__get_entire_repo_manifest(buf_name_1, &pvh)  );
	VERIFY_ERR_CHECK(  u0053_sync__verify_vhash_count(pvh, 1)  );
	VERIFY_ERR_CHECK(  u0053_sync__verifytree__dir_has_file(pvh, "aaa")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

    /* retrieve the manifest from repo 2 and make sure it ALSO contains the file */
	VERIFY_ERR_CHECK(  u0053_sync__get_entire_repo_manifest(buf_name_2, &pvh)  );
	VERIFY_ERR_CHECK(  u0053_sync__verify_vhash_count(pvh, 1)  );
	VERIFY_ERR_CHECK(  u0053_sync__verifytree__dir_has_file(pvh, "aaa")  );

    /* and make sure it also contains the blob for that file */
	VERIFY_ERR_CHECK(  SG_vhash__get__vhash(pvh, "aaa", &pvhItem)  );
	VERIFY_ERR_CHECK(  SG_vhash__get__sz(pvhItem, "hid", &psz_hid_file)  );

    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(&pRepo2, pvh_descriptor)  );
    VERIFY_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pRepo2, psz_hid_file, NULL, &len2)  );
    SG_REPO_NULLFREE(pCtx, pRepo2);

    VERIFY_COND("len", (len1 == len2));

	SG_VHASH_NULLFREE(pCtx, pvh);

    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPath_file);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return 1;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return 0;
}
#endif

int u0053_sync_test__simple_sync_all(SG_context * pCtx, SG_pathname* pPathTopDir, SG_uint32 lines)
{
	char buf_name_1[SG_TID_MAX_BUFFER_LENGTH];
	char buf_name_2[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPath_file = NULL;
	SG_vhash* pvh = NULL;
    SG_repo* pRepo1 = NULL;
    SG_repo* pRepo2 = NULL;
    SG_vhash* pvh_descriptor = NULL;
	SG_vhash* pvhItem = NULL;
	const char* psz_hid_file = NULL;
    SG_uint64 len1 = 0;
    SG_uint64 len2 = 0;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_name_1, sizeof(buf_name_1), 32)  );
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_name_2, sizeof(buf_name_2), 32)  );

	/* create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, buf_name_1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* add stuff */
	VERIFY_ERR_CHECK(  u0053_sync__create_file__numbers(pCtx, pPathWorkingDir, "aaa", lines)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_file, pPathWorkingDir, "aaa")  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath_file, &len1, NULL)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, buf_name_1, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0053_sync__commit_all(pCtx, pPathWorkingDir, NULL)  );

    /* open that repo */
    VERIFY_ERR_CHECK(  SG_closet__descriptors__get(pCtx, buf_name_1, &pvh_descriptor)  );
    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo1, pvh_descriptor)  );
    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);

    /* create a clone and open it */
    VERIFY_ERR_CHECK(  SG_repo__create_empty_clone(pCtx, buf_name_1, buf_name_2)  );

    VERIFY_ERR_CHECK(  SG_closet__descriptors__get(pCtx, buf_name_2, &pvh_descriptor)  );
    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo2, pvh_descriptor)  );

    /* sync from the first instance to the second */
    VERIFY_ERR_CHECK(  SG_sync__oneway__all_dags(pCtx, pRepo1, pRepo2, NULL)  );

    /* close both repos */
    SG_REPO_NULLFREE(pCtx, pRepo2);
    SG_REPO_NULLFREE(pCtx, pRepo1);

    /* retrieve the manifest from repo 1 and make sure it contains the file */
	VERIFY_ERR_CHECK(  u0053_sync__get_entire_repo_manifest(pCtx, buf_name_1, &pvh)  );
	VERIFY_ERR_CHECK(  u0053_sync__verify_vhash_count(pCtx, pvh, 1)  );
	VERIFY_ERR_CHECK(  u0053_sync__verifytree__dir_has_file(pCtx, pvh, "aaa")  );
	SG_VHASH_NULLFREE(pCtx, pvh);

    /* retrieve the manifest from repo 2 and make sure it ALSO contains the file */
	VERIFY_ERR_CHECK(  u0053_sync__get_entire_repo_manifest(pCtx, buf_name_2, &pvh)  );
	VERIFY_ERR_CHECK(  u0053_sync__verify_vhash_count(pCtx, pvh, 1)  );
	VERIFY_ERR_CHECK(  u0053_sync__verifytree__dir_has_file(pCtx, pvh, "aaa")  );

    /* and make sure it also contains the blob for that file */
	VERIFY_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, "aaa", &pvhItem)  );
	VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhItem, "hid", &psz_hid_file)  );

    VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, &pRepo2, pvh_descriptor)  );
    VERIFY_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pRepo2, psz_hid_file, NULL, &len2)  );
    SG_REPO_NULLFREE(pCtx, pRepo2);

    VERIFY_COND("len", (len1 == len2));

	SG_VHASH_NULLFREE(pCtx, pvh);

    SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPath_file);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return 1;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return 0;
}

TEST_MAIN(u0053_sync)
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;

	TEMPLATE_MAIN_START;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathTopDir,bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathTopDir)  );

#if 0
    VERIFY_ERR_CHECK(  u0053_sync_test__simple_sync_all__with_fragball(pPathTopDir)  );
	VERIFY_ERR_CHECK(  u0053_sync_test__fragball(pPathTopDir, 10)  );
	VERIFY_ERR_CHECK(  u0053_sync_test__fragball(pPathTopDir, 1000)  );
	VERIFY_ERR_CHECK(  u0053_sync_test__fragball(pPathTopDir, 1000000)  );
#endif
	VERIFY_ERR_CHECK(  u0053_sync_test__simple_sync_all(pCtx, pPathTopDir, 1000)  );
	VERIFY_ERR_CHECK(  u0053_sync_test__simple_sync_all(pCtx, pPathTopDir, 1000000)  );
	VERIFY_ERR_CHECK(  u0053_sync_test__stringarray_grow(pCtx)  );
    VERIFY_ERR_CHECK(  u0053_sync_test__simple_sync_maxfilesize(pCtx, pPathTopDir)  );
    VERIFY_ERR_CHECK(  u0053_sync_test__sync_db_then_query(pCtx)  );

	/* TODO rm -rf the top dir */

	// fall-thru to common cleanup

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);

	TEMPLATE_MAIN_END;
}

