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
 * @file u0069_treendx.c
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"
#include "unittests_pendingtree.h"

//////////////////////////////////////////////////////////////////

void u0069_treendx__commit_all(SG_context * pCtx,
	const SG_pathname* pPathWorkingDir,
	char** ppszChangesetGID
	)
{
	SG_pendingtree* pPendingTree = NULL;
    SG_repo* pRepo = NULL;
	SG_dagnode * pNewDagNode = NULL;
    SG_audit q;

	// actually do the commit
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
    VERIFY_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );
    VERIFY_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
    //VERIFY_ERR_CHECK(  SG_pendingtree__scan(pCtx, pPendingTree, SG_TRUE, NULL, NULL)  );
	VERIFY_ERR_CHECK(  unittests_pendingtree__commit(pCtx, pPendingTree, &q, NULL, 0, NULL, NULL, 0, NULL, 0, NULL, 0, &pNewDagNode)  );
	VERIFY_ERR_CHECK(  SG_dagnode__get_id(pCtx, pNewDagNode, ppszChangesetGID)  );

fail:
	SG_DAGNODE_NULLFREE(pCtx, pNewDagNode);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

void u0069_treendx__append_to_file__numbers(SG_context * pCtx,
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
	return;
}

void u0069_treendx__create_file__numbers(SG_context * pCtx,
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
	return;
}

void u0069_treendx__move(SG_context * pCtx,
		const SG_pathname* pPathWorkingDir,
		const char* relPath1,
		const char* relPath2
		)
{
	SG_pendingtree* pPendingTree = NULL;
	SG_pathname * pObjectToMove = NULL;
	SG_pathname * pDestinationDir = NULL;
	SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pObjectToMove, pPathWorkingDir, relPath1)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pDestinationDir, pPathWorkingDir, relPath2)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)&pObjectToMove, 1, pDestinationDir, SG_FALSE)  );

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pObjectToMove);
	SG_PATHNAME_NULLFREE(pCtx, pDestinationDir);
}

void u0069_treendx__delete(SG_context * pCtx,
		const SG_pathname* pPathWorkingDir,
		const char* relPath1
		)
{
	SG_pendingtree* pPendingTree = NULL;
	SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree, pPathWorkingDir, 1, &relPath1, NULL, 0, NULL, 0, SG_FALSE)  );

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

//This will return the count of paths in the tree index for the given path or gid.
//If path is not null and gid is null, it will look up the item at that path in
//the current dagnode,
void u0069_treendx__get_path_count(SG_context * pCtx, SG_repo * pRepo, const char* gid, SG_uint32 * pReturnCount)
{
	SG_treendx * pTreeNdx = NULL;
	SG_stringarray * pResultPaths = NULL;
	VERIFY_ERR_CHECK(  SG_repo__treendx__get_all_paths(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, gid, &pResultPaths)  );
	VERIFY_ERR_CHECK(  SG_stringarray__count(pCtx, pResultPaths, pReturnCount)  );
fail:
	SG_TREENDX_NULLFREE(pCtx, pTreeNdx);
	SG_STRINGARRAY_NULLFREE(pCtx, pResultPaths);
	return;
}

//This will return the treenodeentry of an item in a particular changeset.
void u0069_treendx__get_treenode_in_changeset(SG_context * pCtx, SG_repo * pRepo, const char* gid, const char * pszChangesetGID, SG_treenode_entry ** ppTreeNode)
{
	VERIFY_ERR_CHECK(  SG_repo__treendx__get_path_in_dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, gid, pszChangesetGID, ppTreeNode)  );
fail:
	return;
}


void u0069_treendx_test__empty_repo(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_uint32 count = 0;
	char* cset_id = NULL;
	SG_changeset * pcs = NULL;
	SG_repo* pRepo = NULL;
	const char* pszHidTreeNode = NULL;
	char* pszTreeNodeEntryGID = NULL;
	SG_treenode * pTreenodeRoot = NULL;
	SG_treenode_entry * pTreeNodeEntry = NULL;
	SG_treenode_entry * treenode = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo2(pCtx, bufName, pPathWorkingDir, &cset_id)  );

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );
	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, cset_id, &pcs)  );

	//Find the root node in the current changeset.
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs, &pszHidTreeNode) );
	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszHidTreeNode, &pTreenodeRoot)  );
	SG_ERR_CHECK(  SG_treenode__find_treenodeentry_by_path(pCtx, pRepo, pTreenodeRoot, "@", &pszTreeNodeEntryGID, &pTreeNodeEntry)  );
	VERIFY_COND("pszTreeNodeEntryGID should not be NULL", (NULL != pszTreeNodeEntryGID));
	VERIFY_COND("pTreeNodeEntry should not be NULL", (NULL != pTreeNodeEntry));

	VERIFY_ERR_CHECK(  u0069_treendx__get_path_count(pCtx, pRepo, pszTreeNodeEntryGID, &count));
	VERIFY_COND("count should now be 1", (1 == count));

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id, &treenode));
	VERIFY_COND("Couldn't find @ in the initial changeset.", (treenode != NULL));

	SG_NULLFREE(pCtx, pszTreeNodeEntryGID);
	SG_TREENODE_ENTRY_NULLFREE(pCtx, pTreeNodeEntry);
	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);


	SG_TREENODE_NULLFREE(pCtx, pTreenodeRoot);
	SG_CHANGESET_NULLFREE(pCtx, pcs);
	SG_NULLFREE(pCtx, cset_id);
	//SG_NULLFREE(pCtx, pszHidTreeNode);
	SG_REPO_NULLFREE(pCtx, pRepo);

	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return;

fail:
	return;
}

void u0069_treendx_test__added_only(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_uint32 count = 0;
	char* cset_id_initial_changeset = NULL;
	char* cset_id_after_add = NULL;
	SG_changeset * pcs = NULL;
	SG_repo* pRepo = NULL;
	const char* pszHidTreeNode = NULL;
	char* pszTreeNodeEntryGID = NULL;
	SG_treenode * pTreenodeRoot = NULL;
	SG_treenode_entry * pTreeNodeEntry = NULL;
	SG_treenode_entry * treenode = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathDir2 = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo2(pCtx, bufName, pPathWorkingDir, &cset_id_initial_changeset)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir2, pPathDir1, "d2")  );

	/* add stuff */
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir2)  );
	VERIFY_ERR_CHECK(  u0069_treendx__create_file__numbers(pCtx, pPathDir1, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  u0069_treendx__create_file__numbers(pCtx, pPathDir2, "b.txt", 20)  );

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0069_treendx__commit_all(pCtx, pPathWorkingDir, &cset_id_after_add)  );

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );
	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, cset_id_after_add, &pcs)  );

	//Find the root node in the current changeset.
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs, &pszHidTreeNode) );
	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszHidTreeNode, &pTreenodeRoot)  );
	SG_ERR_CHECK(  SG_treenode__find_treenodeentry_by_path(pCtx, pRepo, pTreenodeRoot, "@/d1/a.txt", &pszTreeNodeEntryGID, &pTreeNodeEntry)  );
	VERIFY_COND("pszTreeNodeEntryGID should not be NULL", (NULL != pszTreeNodeEntryGID));
	VERIFY_COND("pTreeNodeEntry should not be NULL", (NULL != pTreeNodeEntry));

	VERIFY_ERR_CHECK(  u0069_treendx__get_path_count(pCtx, pRepo, pszTreeNodeEntryGID, &count));
	VERIFY_COND("count should now be 1", (1 == count));

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id_after_add, &treenode));
	VERIFY_COND("Couldn't find @/d1/a.txt in the second changeset.", (treenode != NULL));

	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);
	treenode = NULL;

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id_initial_changeset, &treenode));
	VERIFY_COND("@/d1/a.txt should not be in the initial changeset.", (treenode == NULL));

	SG_NULLFREE(pCtx, pszTreeNodeEntryGID);
	SG_TREENODE_ENTRY_NULLFREE(pCtx, pTreeNodeEntry);

	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);


	SG_TREENODE_NULLFREE(pCtx, pTreenodeRoot);
	SG_CHANGESET_NULLFREE(pCtx, pcs);
	SG_NULLFREE(pCtx, cset_id_after_add);
	SG_NULLFREE(pCtx, cset_id_initial_changeset);
	//SG_NULLFREE(pCtx, pszHidTreeNode);
	SG_REPO_NULLFREE(pCtx, pRepo);

	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return;

fail:
	return;
}

void u0069_treendx_test__moved_only(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_uint32 count = 0;
	char* cset_id_initial_changeset = NULL;
	char* cset_id_after_add = NULL;
	char* cset_id_after_move = NULL;
	SG_changeset * pcs = NULL;
	SG_repo* pRepo = NULL;
	const char* pszHidTreeNode = NULL;
	char* pszTreeNodeEntryGID = NULL;
	SG_treenode * pTreenodeRoot = NULL;
	SG_treenode_entry * pTreeNodeEntry = NULL;
	SG_treenode_entry * treenode = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathDir2 = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo2(pCtx, bufName, pPathWorkingDir, &cset_id_initial_changeset)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir2, pPathWorkingDir, "d2")  );

	/* add stuff */
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir2)  );
	VERIFY_ERR_CHECK(  u0069_treendx__create_file__numbers(pCtx, pPathDir1, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  u0069_treendx__create_file__numbers(pCtx, pPathDir2, "b.txt", 20)  );

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0069_treendx__commit_all(pCtx, pPathWorkingDir, &cset_id_after_add)  );

	VERIFY_ERR_CHECK(  u0069_treendx__move(pCtx, pPathWorkingDir, "d1", "d2")  );

	VERIFY_ERR_CHECK(  u0069_treendx__commit_all(pCtx, pPathWorkingDir, &cset_id_after_move)  );

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );
	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, cset_id_after_move, &pcs)  );

	//Find the root node in the current changeset.
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs, &pszHidTreeNode) );
	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszHidTreeNode, &pTreenodeRoot)  );
	SG_ERR_CHECK(  SG_treenode__find_treenodeentry_by_path(pCtx, pRepo, pTreenodeRoot, "@/d2/d1/a.txt", &pszTreeNodeEntryGID, &pTreeNodeEntry)  );
	VERIFY_COND("pszTreeNodeEntryGID should not be NULL", (NULL != pszTreeNodeEntryGID));
	VERIFY_COND("pTreeNodeEntry should not be NULL", (NULL != pTreeNodeEntry));

	VERIFY_ERR_CHECK(  u0069_treendx__get_path_count(pCtx, pRepo, pszTreeNodeEntryGID, &count));
	VERIFY_COND("count should now be 2", (2 == count));

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id_after_move, &treenode));
	VERIFY_COND("Couldn't find @/d2/d1/a.txt in the third changeset.", (treenode != NULL));

	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);
	treenode = NULL;

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id_after_add, &treenode));
	VERIFY_COND("Couldn't find @/d1/a.txt in the second changeset.", (treenode != NULL));

	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);
	treenode = NULL;

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id_initial_changeset, &treenode));
	VERIFY_COND("@/d1/a.txt should not be in the initial changeset.", (treenode == NULL));

	SG_NULLFREE(pCtx, pszTreeNodeEntryGID);
	SG_TREENODE_ENTRY_NULLFREE(pCtx, pTreeNodeEntry);

	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);


	SG_TREENODE_NULLFREE(pCtx, pTreenodeRoot);
	SG_CHANGESET_NULLFREE(pCtx, pcs);
	SG_NULLFREE(pCtx, cset_id_after_add);
	SG_NULLFREE(pCtx, cset_id_after_move);
	SG_NULLFREE(pCtx, cset_id_initial_changeset);
	//SG_NULLFREE(pCtx, pszHidTreeNode);
	SG_REPO_NULLFREE(pCtx, pRepo);

	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

	return;

fail:
	return;
}

void u0069_treendx_test__deleted_only(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_uint32 count = 0;
	char* cset_id_initial_changeset = NULL;
	char* cset_id_after_add = NULL;
	char* cset_id_after_delete = NULL;
	SG_changeset * pcs = NULL;
	SG_repo* pRepo = NULL;
	const char* pszHidTreeNode = NULL;
	char* pszTreeNodeEntryGID = NULL;
	SG_treenode * pTreenodeRoot = NULL;
	SG_treenode_entry * pTreeNodeEntry = NULL;
	SG_treenode_entry * treenode = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathDir2 = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo2(pCtx, bufName, pPathWorkingDir, &cset_id_initial_changeset)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir2, pPathWorkingDir, "d2")  );

	/* add stuff */
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir2)  );
	VERIFY_ERR_CHECK(  u0069_treendx__create_file__numbers(pCtx, pPathDir1, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  u0069_treendx__create_file__numbers(pCtx, pPathDir2, "b.txt", 20)  );

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0069_treendx__commit_all(pCtx, pPathWorkingDir, &cset_id_after_add)  );

	VERIFY_ERR_CHECK(  u0069_treendx__delete(pCtx, pPathWorkingDir, "d1")  );

	VERIFY_ERR_CHECK(  u0069_treendx__commit_all(pCtx, pPathWorkingDir, &cset_id_after_delete)  );

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );
	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, cset_id_after_add, &pcs)  );  //Load the middle one.

	//Find the root node in the current changeset.
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs, &pszHidTreeNode) );
	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszHidTreeNode, &pTreenodeRoot)  );
	SG_ERR_CHECK(  SG_treenode__find_treenodeentry_by_path(pCtx, pRepo, pTreenodeRoot, "@/d1/a.txt", &pszTreeNodeEntryGID, &pTreeNodeEntry)  );
	VERIFY_COND("pszTreeNodeEntryGID should not be NULL", (NULL != pszTreeNodeEntryGID));
	VERIFY_COND("pTreeNodeEntry should not be NULL", (NULL != pTreeNodeEntry));

	VERIFY_ERR_CHECK(  u0069_treendx__get_path_count(pCtx, pRepo, pszTreeNodeEntryGID, &count));
	VERIFY_COND("count should now be 1", (1 == count));

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id_after_delete, &treenode));
	VERIFY_COND("@/d1/a.txt should not be in the third changeset.", (treenode == NULL));

	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);
	treenode = NULL;

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id_after_add, &treenode));
	VERIFY_COND("Couldn't find @/d1/a.txt in the second changeset.", (treenode != NULL));

	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);
	treenode = NULL;

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id_initial_changeset, &treenode));
	VERIFY_COND("@/d1/a.txt should not be in the initial changeset.", (treenode == NULL));

fail:
	SG_NULLFREE(pCtx, pszTreeNodeEntryGID);
	SG_TREENODE_ENTRY_NULLFREE(pCtx, pTreeNodeEntry);
	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);
	SG_TREENODE_NULLFREE(pCtx, pTreenodeRoot);
	SG_CHANGESET_NULLFREE(pCtx, pcs);
	SG_NULLFREE(pCtx, cset_id_after_add);
	SG_NULLFREE(pCtx, cset_id_after_delete);
	SG_NULLFREE(pCtx, cset_id_initial_changeset);
	//SG_NULLFREE(pCtx, pszHidTreeNode);
	SG_REPO_NULLFREE(pCtx, pRepo);

	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
}

void u0069_treendx_test__not_the_right_gid(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathFile = NULL;
	SG_uint32 count = 0;
	char* cset_id_initial_changeset = NULL;
	char* cset_id_after_add = NULL;
	char* cset_id_after_delete = NULL;
	char* cset_id_after_second_add = NULL;
	SG_changeset * pcs = NULL;
	SG_repo* pRepo = NULL;
	const char* pszHidTreeNode = NULL;
	char* pszTreeNodeEntryGID = NULL;
	SG_treenode * pTreenodeRoot = NULL;
	SG_treenode_entry * pTreeNodeEntry = NULL;
	SG_treenode_entry * treenode = NULL;
	SG_pathname* pPathDir1 = NULL;
	SG_pathname* pPathDir2 = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );

	/* First create the working dir */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );

    /* create the repo */
	VERIFY_ERR_CHECK(  _ut_pt__new_repo2(pCtx, bufName, pPathWorkingDir, &cset_id_initial_changeset)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir1, pPathWorkingDir, "d1")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathDir2, pPathWorkingDir, "d2")  );

	/* add stuff */
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir1)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathDir2)  );
	VERIFY_ERR_CHECK(  u0069_treendx__create_file__numbers(pCtx, pPathDir1, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  u0069_treendx__create_file__numbers(pCtx, pPathDir2, "b.txt", 20)  );

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0069_treendx__commit_all(pCtx, pPathWorkingDir, &cset_id_after_add)  );

	VERIFY_ERR_CHECK(  u0069_treendx__delete(pCtx, pPathWorkingDir, "d1/a.txt")  );

	VERIFY_ERR_CHECK(  u0069_treendx__commit_all(pCtx, pPathWorkingDir, &cset_id_after_delete)  );

	VERIFY_ERR_CHECK(  u0069_treendx__create_file__numbers(pCtx, pPathDir1, "a.txt", 20)  );
	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  u0069_treendx__commit_all(pCtx, pPathWorkingDir, &cset_id_after_second_add)  );

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );
	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, cset_id_after_second_add, &pcs)  );  //Load the middle one.

	//Find the root node in the current changeset.
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs, &pszHidTreeNode) );
	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszHidTreeNode, &pTreenodeRoot)  );
	SG_ERR_CHECK(  SG_treenode__find_treenodeentry_by_path(pCtx, pRepo, pTreenodeRoot, "@/d1/a.txt", &pszTreeNodeEntryGID, &pTreeNodeEntry)  );
	VERIFY_COND("pszTreeNodeEntryGID should not be NULL", (NULL != pszTreeNodeEntryGID));
	VERIFY_COND("pTreeNodeEntry should not be NULL", (NULL != pTreeNodeEntry));

	VERIFY_ERR_CHECK(  u0069_treendx__get_path_count(pCtx, pRepo, pszTreeNodeEntryGID, &count));
	VERIFY_COND("count should now be 1", (1 == count));

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id_after_delete, &treenode));
	VERIFY_COND("@/d1/a.txt should not be in the third changeset.", (treenode == NULL));

	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);
	treenode = NULL;

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id_after_add, &treenode));
	VERIFY_COND("@/d1/a.txt should not be in the second changeset.", (treenode == NULL));

	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);
	treenode = NULL;

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id_after_second_add, &treenode));
	VERIFY_COND("@/d1/a.txt should  be in the fourth changeset.", (treenode != NULL));

	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);
	treenode = NULL;

	VERIFY_ERR_CHECK(  u0069_treendx__get_treenode_in_changeset(pCtx, pRepo, pszTreeNodeEntryGID, cset_id_initial_changeset, &treenode));
	VERIFY_COND("@/d1/a.txt should not be in the initial changeset.", (treenode == NULL));

fail:
	SG_NULLFREE(pCtx, pszTreeNodeEntryGID);
	SG_TREENODE_ENTRY_NULLFREE(pCtx, pTreeNodeEntry);
	SG_TREENODE_ENTRY_NULLFREE(pCtx, treenode);
	SG_TREENODE_NULLFREE(pCtx, pTreenodeRoot);
	SG_CHANGESET_NULLFREE(pCtx, pcs);
	SG_NULLFREE(pCtx, cset_id_after_add);
	SG_NULLFREE(pCtx, cset_id_after_second_add);
	SG_NULLFREE(pCtx, cset_id_after_delete);
	SG_NULLFREE(pCtx, cset_id_initial_changeset);
	//SG_NULLFREE(pCtx, pszHidTreeNode);
	SG_REPO_NULLFREE(pCtx, pRepo);

	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir2);
	SG_PATHNAME_NULLFREE(pCtx, pPathDir1);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
}

TEST_MAIN(u0069_treendx)
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;

	TEMPLATE_MAIN_START;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir),  32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathTopDir,bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathTopDir)  );

	BEGIN_TEST(  u0069_treendx_test__moved_only(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0069_treendx_test__empty_repo(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0069_treendx_test__added_only(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0069_treendx_test__deleted_only(pCtx, pPathTopDir)  );
	BEGIN_TEST(  u0069_treendx_test__not_the_right_gid(pCtx, pPathTopDir)  );
//	BEGIN_TEST(  u0069_treendx_test__bogus_gid(pCtx, pPathTopDir)  );
//	BEGIN_TEST(  u0069_treendx_test__single_add(pCtx, pPathTopDir)  );
//	BEGIN_TEST(  u0069_treendx_test__multiple_adds(pCtx, pPathTopDir)  );
//	BEGIN_TEST(  u0069_treendx_test__file_rename(pCtx, pPathTopDir)  );
//	BEGIN_TEST(  u0069_treendx_test__folder_rename(pCtx, pPathTopDir)  );
//	BEGIN_TEST(  u0069_treendx_test__file_rename_then_rename_back(pCtx, pPathTopDir)  );
//	BEGIN_TEST(  u0069_treendx_test__folder_rename_then_rename_back(pCtx, pPathTopDir)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);

	TEMPLATE_MAIN_END;
}
