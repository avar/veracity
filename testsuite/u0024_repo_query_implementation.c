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
 * @file u0024_repo_query_implementation.c
 *
 * @details Test the various query-interface questions.
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

//////////////////////////////////////////////////////////////////

// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0024_repo_query_implementation)
#define MyDcl(name)				u0024_repo_query_implementation__##name
#define MyFn(name)				u0024_repo_query_implementation__##name

//////////////////////////////////////////////////////////////////

/**
 * Verify that we can specify various hash-methods for
 * each repo implementation.  Verify that we can create a repo
 * with a specific hash-method.  Verify that hashes created in
 * that repo are of the proper size.
 */
// TODO

//////////////////////////////////////////////////////////////////

/**
 * List all of the installed repo implementations.
 *
 * NOTE: I don't have a way to verify that the list is complete or verify
 * NOTE: what it should contain, so I just print the list.
 */
void MyFn(list_vtables)(SG_context * pCtx)
{
	SG_vhash * pvh_vtables = NULL;
	SG_uint32 count_vtables;
	SG_uint32 k;

	VERIFY_ERR_CHECK(  SG_repo__query_implementation(pCtx,NULL,
													 SG_REPO__QUESTION__VHASH__LIST_REPO_IMPLEMENTATIONS,
													 NULL,NULL,NULL,0,
													 &pvh_vtables)  );
	VERIFY_ERR_CHECK(  SG_vhash__count(pCtx,pvh_vtables,&count_vtables)  );
	for (k=0; k<count_vtables; k++)
	{
		const char * pszKey_vtable_k;

		VERIFY_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx,pvh_vtables,k,&pszKey_vtable_k,NULL)  );
		INFOP("vtable",("Repo Implementation[%d]: [%s]",k,pszKey_vtable_k));
	}

fail:
	SG_VHASH_NULLFREE(pCtx, pvh_vtables);
}

//////////////////////////////////////////////////////////////////

/**
 * List the hashes supported by each repo implementation.
 *
 * NOTE: I don't have a way to verify that the list is complete or verify
 * NOTE: what it should contain, so I just print the list.
 */
void MyFn(list_hashes)(SG_context * pCtx)
{
	SG_repo * pRepo = NULL;
	SG_vhash * pvh_vtables = NULL;
	SG_vhash * pvh_HashMethods = NULL;
	SG_uint32 k, count_vtables;

	VERIFY_ERR_CHECK(  SG_repo__query_implementation(pCtx,NULL,
													 SG_REPO__QUESTION__VHASH__LIST_REPO_IMPLEMENTATIONS,
													 NULL,NULL,NULL,0,
													 &pvh_vtables)  );
	VERIFY_ERR_CHECK(  SG_vhash__count(pCtx,pvh_vtables,&count_vtables)  );
	for (k=0; k<count_vtables; k++)
	{
		const char * pszKey_vtable_k;
		SG_uint32 j, count_HashMethods;

		VERIFY_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx,pvh_vtables,k,&pszKey_vtable_k,NULL)  );
		INFOP("vtable",("Repo Implementation[%d]: [%s]",k,pszKey_vtable_k));

		VERIFY_ERR_CHECK(  SG_repo__alloc(pCtx,&pRepo,pszKey_vtable_k)  );
		VERIFY_ERR_CHECK(  SG_repo__query_implementation(pCtx,pRepo,
														 SG_REPO__QUESTION__VHASH__LIST_HASH_METHODS,
														 NULL,NULL,NULL,0,
														 &pvh_HashMethods)  );
		VERIFY_ERR_CHECK(  SG_vhash__count(pCtx,pvh_HashMethods,&count_HashMethods)  );
		for (j=0; j<count_HashMethods; j++)
		{
			const char * pszKey_HashMethod_j;
			const SG_variant * pVariant;
			SG_int64 i64;
			SG_uint32 strlen_Hash_j;

			VERIFY_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx,pvh_HashMethods,j,&pszKey_HashMethod_j,&pVariant)  );
			VERIFY_ERR_CHECK(  SG_variant__get__int64(pCtx,pVariant,&i64)  );
			strlen_Hash_j = (SG_uint32)i64;
			INFOP("vtable.hash_method",("Repo [%s] Hash [%s] Length [%d]",pszKey_vtable_k,pszKey_HashMethod_j,strlen_Hash_j));
		}

		SG_VHASH_NULLFREE(pCtx, pvh_HashMethods);
		SG_REPO_NULLFREE(pCtx, pRepo);
	}

fail:
	SG_VHASH_NULLFREE(pCtx, pvh_HashMethods);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_VHASH_NULLFREE(pCtx, pvh_vtables);
}

//////////////////////////////////////////////////////////////////

MyMain()
{
//	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
//	SG_pathname* pPathTopDir = NULL;

	TEMPLATE_MAIN_START;

//	BEGIN_TEST(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir), 32)  );
//	BEGIN_TEST(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathTopDir,bufTopDir)  );
//	BEGIN_TEST(  SG_fsobj__mkdir__pathname(pCtx, pPathTopDir)  );

	BEGIN_TEST(  MyFn(list_vtables)(pCtx)  );
	BEGIN_TEST(  MyFn(list_hashes)(pCtx)  );

	TEMPLATE_MAIN_END;
}

//////////////////////////////////////////////////////////////////

#undef MyMain
#undef MyDcl
#undef MyFn
