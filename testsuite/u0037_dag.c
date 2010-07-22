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
 * @file u0037_dag.c
 *
 * @details Simple test to create DAGNODEs and update the on-disk DAG.
 * We bypass all of the CHANGESET and SG_committing stuff.  We create
 * fake DAGNODE HIDs using GIDs.
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

//////////////////////////////////////////////////////////////////

void u0037_dag__create_new_repo(SG_context * pCtx, SG_repo ** ppRepo)
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

	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);

	{
		const SG_vhash * pvhRepoDescriptor = NULL;
		VERIFY_ERR_CHECK(  SG_repo__get_descriptor(pCtx, pRepo,&pvhRepoDescriptor)  );

		//INFOP("open_repo",("Repo is [%s]",SG_string__sz(pstrRepoDescriptor)));
	}

	SG_PATHNAME_NULLFREE(pCtx, pPathnameRepoDir);

    SG_NULLFREE(pCtx, pszRepoImpl);

	*ppRepo = pRepo;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameRepoDir);

    SG_NULLFREE(pCtx, pszRepoImpl);
}

void u0037_dag__create_graph(SG_context * pCtx,
							SG_repo * pRepo,
							SG_uint32 cGenerations,
							SG_uint32 cBranches,
							SG_uint32 cParents)
{
	// create an arbitrarily complex dag.
	//
	// cGenerations is the number of steps between the leaves and the root.
	// cBranches is how many branches we have at EACH generation
	//
	// we then create a bunch of HIDs as placeholders for actual changesets
	// (and the computed HIDs).  (We create a TID to get some random data
	// and use it to compute a HID that we need.  Previously, we used a GID
	// and just called it an HID because they were interchangeable, but that
	// is no longer true.
	//
	// we will then start creating dagnodes for each generated ID and
	// add them to the DAG in the REPO.  For each dagnode, we will give
	// it a random number of parents (between 1 and cParents).  the
	// parents will be from any previous generation (to avoid loops in
	// the dag).
	//
	// TODO i added a somewhat random value for the dagnode.generation
	// TODO field.  this causes the dagfrag output to look rather stupid.
	// TODO i don't want to fix this right now.  what we should really
	// TODO is generate some less fake data....

	SG_rbtree * pIdsetAncestors = NULL;
	SG_uint32 kGeneration, kBranch, kParent;
	SG_uint32 sumAncestors = 0;
	SG_string * pString;

	srand(1);

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );
	VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pString,"create_graph\n")  );

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pIdsetAncestors)  );

	for (kGeneration=0; kGeneration<cGenerations; kGeneration++)
	{
		SG_rbtree * pIdsetLeaves = NULL;
		const char** paIds = NULL;
		SG_uint32 acount;
		SG_rbtree_iterator *pIterator = NULL;
		SG_bool b;
		const char* pszKey = NULL;
		SG_uint32 i;
		SG_repo_tx_handle* pTx;

		VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, pIdsetAncestors, &acount)  );

		paIds = SG_calloc(acount, sizeof(const char*));
		VERIFY_COND("alloc", (paIds != NULL));

		VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, pIdsetAncestors, &b, &pszKey, NULL)  );
			i = 0;
		while (b)
		{
			paIds[i++] = pszKey;

			VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, NULL)  );
				}
		SG_rbtree__iterator__free(pCtx, pIterator);
		pIterator = NULL;

		// create one generation-worth of leaf IDs.
		// we remember these IDs in a temporary IDSET.
		//
		// and add a DAGNODE for each ID to the DAG on disk.

		VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pIdsetLeaves)  );

		for (kBranch=0; kBranch<cBranches; kBranch++)
		{
			char bufTidRandom[SG_TID_MAX_BUFFER_LENGTH];
			char* pHid = NULL;
			SG_dagnode * pDagnode = NULL;

			VERIFY_ERR_CHECK(  SG_tid__generate(pCtx, bufTidRandom, sizeof(bufTidRandom))  );
			VERIFY_ERR_CHECK(  SG_repo__alloc_compute_hash__from_bytes(pCtx,
																	   pRepo,
																	   strlen(bufTidRandom),
																	   (SG_byte *)bufTidRandom,
																	   &pHid)  );

			VERIFY_ERR_CHECK(  SG_rbtree__update(pCtx, pIdsetLeaves,pHid)  );

			VERIFY_ERR_CHECK(  SG_dagnode__alloc(pCtx, &pDagnode, pHid, kGeneration+1 )  );

			if (kGeneration > 0)
			{
				SG_uint32 rParents = (SG_uint32)(rand() % cParents) + 1;	// how many parents this dagnode has.

				for (kParent=0; kParent<rParents; kParent++)
				{
					SG_uint32 x = (SG_uint32)(rand() % sumAncestors);		// pick a random ancestor

					VERIFY_ERR_CHECK(  SG_dagnode__add_parent(pCtx, pDagnode,paIds[x])  );
				}
			}
			VERIFY_ERR_CHECK(  SG_dagnode__freeze(pCtx, pDagnode)  );

			VERIFY_ERR_CHECK_DISCARD(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );

			SG_repo__store_dagnode(pCtx, pRepo,pTx,SG_DAGNUM__TESTING__NOTHING,pDagnode);
            pDagnode = NULL;
			SG_context__err_reset(pCtx);

			VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo,&pTx)  );

			SG_NULLFREE(pCtx, pHid);
		}

		SG_NULLFREE(pCtx, paIds);
		paIds = NULL;

		// after we've done everything for this generation, fold
		// the IDs in this generation's IDSET into the ancestor IDSET
		// and delete the temporary/generation IDSET.

		VERIFY_ERR_CHECK(  SG_rbtree__add__from_other_rbtree(pCtx, pIdsetAncestors,pIdsetLeaves)  );

		SG_RBTREE_NULLFREE(pCtx, pIdsetLeaves);
		VERIFY_CTX_IS_OK("create_graph", pCtx);

		sumAncestors += cBranches;
	}

	SG_RBTREE_NULLFREE(pCtx, pIdsetAncestors);

	INFOP("create_graph",(("\n%s"),SG_string__sz(pString)));
	SG_STRING_NULLFREE(pCtx, pString);

	return;
fail:
	// Leaks, blah.
	return;
}

void u0037_dag__test_sparse(SG_context * pCtx, SG_repo * pRepo)
{
	char bufTid[SG_TID_MAX_BUFFER_LENGTH];
	char* pIdRandomChild = NULL;
	char* pIdRandomParent1 = NULL;
	char* pIdRandomParent2 = NULL;
	SG_dagnode * pDagnodeRandom = NULL;
	SG_repo_tx_handle* pTx = NULL;

	// since we created a connected graph, we should not be sparse.

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__check_integrity(pCtx,pRepo,SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY,SG_DAGNUM__TESTING__NOTHING,NULL,NULL)  );

	// try to add a random un-related node -- this should now fail.
	//
	// dagnodes are HIDs.  we compute some random TIDs to use as hash input and compute some HIDs.

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTid, sizeof(bufTid), 32)  );
	VERIFY_ERR_CHECK(  SG_repo__alloc_compute_hash__from_bytes(pCtx, pRepo, strlen(bufTid), (SG_byte *)bufTid, &pIdRandomChild)  );

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTid, sizeof(bufTid), 48)  );
	VERIFY_ERR_CHECK(  SG_repo__alloc_compute_hash__from_bytes(pCtx, pRepo, strlen(bufTid), (SG_byte *)bufTid, &pIdRandomParent1)  );

	VERIFY_ERR_CHECK(  SG_tid__generate(pCtx, bufTid, sizeof(bufTid))  );
	VERIFY_ERR_CHECK(  SG_repo__alloc_compute_hash__from_bytes(pCtx, pRepo, strlen(bufTid), (SG_byte *)bufTid, &pIdRandomParent2)  );

	VERIFY_ERR_CHECK(  SG_dagnode__alloc(pCtx, &pDagnodeRandom,pIdRandomChild, 1000000)  );
	VERIFY_ERR_CHECK(  SG_dagnode__add_parent(pCtx, pDagnodeRandom,pIdRandomParent1)  );
	VERIFY_ERR_CHECK(  SG_dagnode__add_parent(pCtx, pDagnodeRandom,pIdRandomParent2)  );
	VERIFY_ERR_CHECK(  SG_dagnode__freeze(pCtx, pDagnodeRandom)  );

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	SG_repo__store_dagnode(pCtx, pRepo, pTx, SG_DAGNUM__TESTING__NOTHING, pDagnodeRandom);
    pDagnodeRandom = NULL;
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_repo__commit_tx(pCtx, pRepo, &pTx);
	VERIFY_CTX_ERR_EQUALS("add random dagnode", pCtx,  SG_ERR_CANNOT_CREATE_SPARSE_DAG);
	SG_context__err_reset(pCtx);

	SG_NULLFREE(pCtx, pIdRandomChild);
	SG_NULLFREE(pCtx, pIdRandomParent1);
	SG_NULLFREE(pCtx, pIdRandomParent2);

	// verify that the dag is still consistent (not sparse)

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__check_integrity(pCtx,pRepo,SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY,SG_DAGNUM__TESTING__NOTHING,NULL,NULL)  );

	return;
fail:
	SG_NULLFREE(pCtx, pIdRandomChild);
	SG_NULLFREE(pCtx, pIdRandomParent1);
	SG_NULLFREE(pCtx, pIdRandomParent2);
}

//////////////////////////////////////////////////////////////////

struct _u0037_dag__do_single_piece_frag_cb_ctx
{
	SG_repo * pRepo;
	SG_int32 nrGenerations;
};

void u0037_dag__do_single_piece_frag_cb(SG_context * pCtx, const char * pszidHid, SG_UNUSED_PARAM(void* assoc), void * pCtxVoid)
{
	// create a dagfrag and put one piece in it.
	// that is, start with a single leaf and build
	// a small fragment from it.

	SG_dagfrag * pFrag = NULL;
	SG_string * pString = NULL;
	SG_dagfrag_state qs;
	SG_int32 gen;
	SG_bool present;
	const SG_dagnode * pDagnode = NULL;
	struct _u0037_dag__do_single_piece_frag_cb_ctx * context = (struct _u0037_dag__do_single_piece_frag_cb_ctx *)pCtxVoid;
    char* psz_repo_id = NULL;
    char* psz_admin_id = NULL;
	SG_bool bValidFormat;

	SG_UNUSED(assoc);

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );

    VERIFY_ERR_CHECK(  SG_repo__get_repo_id(pCtx, context->pRepo, &psz_repo_id)  );
	VERIFY_ERR_CHECK(  SG_gid__verify_format(pCtx, psz_repo_id, &bValidFormat)  );
	VERIFY_COND("repo_id is GID",(bValidFormat));

    VERIFY_ERR_CHECK(  SG_repo__get_admin_id(pCtx, context->pRepo, &psz_admin_id)  );
	VERIFY_ERR_CHECK(  SG_gid__verify_format(pCtx, psz_admin_id, &bValidFormat)  );
	VERIFY_COND("admin_id is GID",(bValidFormat));

	VERIFY_ERR_CHECK(  SG_dagfrag__alloc(pCtx,&pFrag,psz_repo_id,psz_admin_id,SG_DAGNUM__TESTING__NOTHING)  );

    SG_NULLFREE(pCtx, psz_repo_id);
    SG_NULLFREE(pCtx, psz_admin_id);

	VERIFY_ERR_CHECK(  SG_dagfrag__load_from_repo__one(pCtx, pFrag,context->pRepo,pszidHid,context->nrGenerations)  );

#ifdef DEBUG
	SG_ERR_IGNORE(  SG_dagfrag_debug__dump(pCtx,pFrag,"Single Piece Frag",10,pString)  );
	INFOP("single piece frag",("\n%s",SG_string__sz(pString)));
#endif

	VERIFY_ERR_CHECK(  SG_dagfrag__query(pCtx, pFrag,pszidHid,&qs,&gen, &present, &pDagnode)  );
	VERIFY_COND("single piece frag",(qs == SG_DFS_START_MEMBER));
	VERIFY_COND("single piece frag",(pDagnode));

	SG_DAGFRAG_NULLFREE(pCtx, pFrag);
	SG_STRING_NULLFREE(pCtx, pString);

	return;
fail:
    SG_NULLFREE(pCtx, psz_repo_id);
    SG_NULLFREE(pCtx, psz_admin_id);
	SG_DAGFRAG_NULLFREE(pCtx, pFrag);
	SG_STRING_NULLFREE(pCtx, pString);
}

void u0037_dag__do_single_piece_frag(SG_context * pCtx,
									 SG_repo * pRepo,
									 SG_rbtree * pIdsetLeaves)
{
	struct _u0037_dag__do_single_piece_frag_cb_ctx ctx;

	ctx.pRepo = pRepo;
	ctx.nrGenerations = 10;

	VERIFY_ERR_CHECK(  SG_rbtree__foreach(pCtx, pIdsetLeaves,u0037_dag__do_single_piece_frag_cb,&ctx)  );

	return;
fail:
	return;
}

void u0037_dag__do_frags(SG_context * pCtx, SG_repo * pRepo)
{
	SG_rbtree * pIdsetLeaves;

	// get the complete set of leaves.

	VERIFY_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, SG_DAGNUM__TESTING__NOTHING,&pIdsetLeaves)  );

	// build various (semi-random) fragments.
	//
	// for now, we'll have to eyeball the output to know if this
	// test works.  later, we can write tests that know what the
	// output should be.

	VERIFY_ERR_CHECK(  u0037_dag__do_single_piece_frag(pCtx, pRepo,pIdsetLeaves)  );

	SG_RBTREE_NULLFREE(pCtx, pIdsetLeaves);

	return;
fail:
	SG_RBTREE_NULLFREE(pCtx, pIdsetLeaves);
}


void u0037_dag__run(SG_context * pCtx)
{
	// run the main test.

	SG_repo * pRepo = NULL;

	// create a new repo.

	VERIFY_ERR_CHECK(  u0037_dag__create_new_repo(pCtx, &pRepo)  );

	// create a random dag.

	VERIFY_ERR_CHECK(  u0037_dag__create_graph(pCtx, pRepo,100,3,2)  );

	// verify that the dag on disk is self-consistent.

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__check_integrity(pCtx,pRepo,SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY,SG_DAGNUM__TESTING__NOTHING,NULL,NULL)  );

	// test sparseness by adding random dagnodes to make dag sparse

	VERIFY_ERR_CHECK(  u0037_dag__test_sparse(pCtx, pRepo)  );

	// re-verify that the dag on disk is self-consistent.

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__check_integrity(pCtx,pRepo,SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY,SG_DAGNUM__TESTING__NOTHING,NULL,NULL)  );

	// try to create various fragments of the dag for push/pull
	// type operations.

	VERIFY_ERR_CHECK(  u0037_dag__do_frags(pCtx, pRepo)  );

fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
}

/**
 * Simple test to stress the dag on disk.
 */
TEST_MAIN(u0037_dag)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0037_dag__run(pCtx)  );

	TEMPLATE_MAIN_END;
}
