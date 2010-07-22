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
 * @file u0041_rbtree.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

void u0041_rbtree__cb_printf(SG_UNUSED_PARAM(SG_context* pCtx), const char* pszKey, SG_UNUSED_PARAM(void* assocData), SG_UNUSED_PARAM(void* ctx))
{
	SG_UNUSED(pCtx);
	SG_UNUSED(ctx);
	SG_UNUSED(assocData);

	printf("%s\n", pszKey);
}

void u0041_rbtree__cb_insert(SG_context* pCtx, const char* pszKey, void* assocData, void* ctx)
{
	SG_rbtree* prb2 = (SG_rbtree*) ctx;

	//printf("%s\n", pszKey);

	SG_rbtree__add__with_assoc(pCtx, prb2, pszKey, assocData);
}

void u0041_test_rbtree_0(SG_context * pCtx)
{
	SG_rbtree* prb = NULL;
	SG_uint32 count = 0;
	void* pAssocData = NULL;
	SG_bool bFound = SG_FALSE;

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );

	VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx, prb, "hola", &bFound, NULL)  );
	VERIFY_COND("b", !bFound);

	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_rbtree__remove(pCtx, prb, "hola")  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
	VERIFY_COND("count", (count == 0));

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb, "hello", NULL)  );

	VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx, prb, "hello", &bFound, NULL)  );
	VERIFY_COND("b", bFound);

	VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx, prb, "Hello", &bFound, NULL)  );
	VERIFY_COND("b", !bFound);

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
	VERIFY_COND("count", (count == 1));

	VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx, prb, "hola", &bFound, NULL)  );
	VERIFY_COND("b", !bFound);

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb, "hola", NULL)  );

	VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx, prb, "hola", &bFound, NULL)  );
	VERIFY_COND("b", bFound);

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
	VERIFY_COND("count", (count == 2));

	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_rbtree__add__with_assoc(pCtx, prb, "hello", NULL)  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
	VERIFY_COND("count", (count == 2));

	VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb, "hola")  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
	VERIFY_COND("count", (count == 1));

	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_rbtree__remove(pCtx, prb, "not there")  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
	VERIFY_COND("count", (count == 1));

	VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb, "hello")  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
	VERIFY_COND("count", (count == 0));

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb, "howdy", "partner")  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
	VERIFY_COND("count", (count == 1));

	VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx, prb, "howdy", NULL, &pAssocData)  );
	VERIFY_COND("assoc data", (0 == strcmp("partner", (char*) pAssocData)));

	VERIFY_ERR_CHECK(  SG_rbtree__find(pCtx, prb, "howdy", NULL, NULL)  );

	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_rbtree__find(pCtx, prb, "hello", NULL, &pAssocData)  );

	VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb, "howdy")  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
	VERIFY_COND("count", (count == 0));

	pAssocData = NULL;
	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_rbtree__find(pCtx, prb, "howdy", NULL, &pAssocData)  );
	VERIFY_COND("not found", (pAssocData == NULL));

fail:
	SG_RBTREE_NULLFREE(pCtx, prb);
}

void u0041_test_rbtree_1(SG_context * pCtx)
{
	char szHex[SG_GID_BUFFER_LENGTH];
	SG_uint32 i;
	SG_strpool* pPool = NULL;
	SG_rbtree* prb1 = NULL;
	SG_rbtree* prb2 = NULL;
	SG_uint16 depth1;
	SG_uint16 depth2;
	SG_uint32 count1;
	SG_uint32 count2;

	VERIFY_ERR_CHECK(  SG_STRPOOL__ALLOC(pCtx, &pPool, 128*1024)  );

	/*
	 * Insert a bunch of GIDs into a rbtree tree.
	 * This should naturally end up with a reasonble
	 * depth, since the data is random.
	 *
	 * Update: 2010/01/28 Previously, GIDs were just a sequence of hex digits.
	 *                    Now they have a 'g' prefix followed by the sequence
	 *                    of hex digits.  In theory, both forms have the same
	 *                    random distribution, so we could use the new forms
	 *                    and assume that these balancing tests are still valid
	 *                    (albeit, a little slower with the new forms).  But,
	 *                    we also can skip the 'g' so that the test is identical.
	 */

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb1)  );

	for (i=0; i<100000; i++)
	{
		const char* psz;

		VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, szHex, sizeof(szHex))  );

		VERIFY_ERR_CHECK(  SG_strpool__add__sz(pCtx, pPool, szHex, &psz)  );

		VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, psz, NULL)  );

		if ((i % 100) == 0)
		{
			VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb1, psz)  );
		}
	}

	/*
	 * Now take all the GIDs from the first rbtree
	 * tree and insert them all, IN ORDER, into a second
	 * one.  This is the worst case scenario for a self
	 * balancing binary tree, so it exercises the balancing
	 * code rather nicely.
	 */

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb2)  );

	VERIFY_ERR_CHECK(  SG_rbtree__foreach(pCtx, prb1, u0041_rbtree__cb_insert, prb2)  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb1, &count1)  );
	//printf("count of tree 1: %d\n", count1);

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb2, &count2)  );
	//printf("count of tree 2: %d\n", count2);

	VERIFY_COND("counts should match", (count1 == count2));

	VERIFY_ERR_CHECK(  SG_rbtree__depth(pCtx, prb1, &depth1)  );
	//printf("depth of tree 1: %d\n", depth1);

	VERIFY_ERR_CHECK(  SG_rbtree__depth(pCtx, prb2, &depth2)  );
	//printf("depth of tree 2: %d\n", depth2);

	/*
	 * It's hard to make any specific comparisons between depth1 and depth2,
	 * but the two numbers shouldn't be terribly far apart.
	 */

fail:
	SG_STRPOOL_NULLFREE(pCtx, pPool);
	SG_RBTREE_NULLFREE(pCtx, prb1);
	SG_RBTREE_NULLFREE(pCtx, prb2);
}

void u0041_test_rbtree_2(SG_context * pCtx)
{
	SG_rbtree* prb1 = NULL;
	SG_rbtree* prb2 = NULL;
	SG_rbtree* prbOnly1 = NULL;
	SG_rbtree* prbOnly2 = NULL;
	SG_rbtree* prbBoth = NULL;
	SG_uint32 count1;
	SG_uint32 count2;
	SG_uint32 countBoth;
	SG_bool bIdentical;

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb1)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "a", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "b", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "c", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "d", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "e", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "f", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "g", NULL)  );

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb2)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "a", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "b", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "c", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "d", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "e", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "f", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "g", NULL)  );

	bIdentical = SG_FALSE;
	VERIFY_ERR_CHECK(  SG_rbtree__compare__keys_only(pCtx, prb1, prb2, &bIdentical, NULL, NULL, NULL)  );
	VERIFY_COND("identical", bIdentical);

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prbOnly1)  );

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prbOnly2)  );

	VERIFY_ERR_CHECK(  SG_rbtree__compare__keys_only(pCtx, prb1, prb2, &bIdentical, prbOnly1, prbOnly2, NULL)  );
	VERIFY_COND("identical", bIdentical);

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prbOnly1, &count1)  );
	VERIFY_COND("count", (count1 == 0));

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prbOnly2, &count2)  );
	VERIFY_COND("count", (count2 == 0));

	VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb1, "b")  );

	VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb1, "d")  );

	VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb2, "c")  );

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prbBoth)  );

	VERIFY_ERR_CHECK(  SG_rbtree__compare__keys_only(pCtx, prb1, prb2, &bIdentical, NULL, NULL, NULL)  );
	VERIFY_COND("not identical", !bIdentical);

	VERIFY_ERR_CHECK(  SG_rbtree__compare__keys_only(pCtx, prb1, prb2, &bIdentical, prbOnly1, prbOnly2, prbBoth)  );
	VERIFY_COND("not identical", !bIdentical);

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prbOnly1, &count1)  );
	VERIFY_COND("count", (count1 == 1));

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prbOnly2, &count2)  );
	VERIFY_COND("count", (count2 == 2));

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prbBoth, &countBoth)  );
	VERIFY_COND("count", (countBoth == 4));

#if 0	// less noise
	printf("Only in 1:\n");
	VERIFY_ERR_CHECK(  SG_rbtree__foreach(pCtx, prbOnly1, u0041_rbtree__cb_printf, NULL)  );

	printf("Only in 2:\n");
	VERIFY_ERR_CHECK(  SG_rbtree__foreach(pCtx, prbOnly2, u0041_rbtree__cb_printf, NULL)  );

	printf("Both:\n");
	VERIFY_ERR_CHECK(  SG_rbtree__foreach(pCtx, prbBoth, u0041_rbtree__cb_printf, NULL)  );
#endif

fail:
	SG_RBTREE_NULLFREE(pCtx, prb1);
	SG_RBTREE_NULLFREE(pCtx, prb2);
	SG_RBTREE_NULLFREE(pCtx, prbOnly1);
	SG_RBTREE_NULLFREE(pCtx, prbOnly2);
	SG_RBTREE_NULLFREE(pCtx, prbBoth);
}

void u0041_test_rbtree_3(SG_context * pCtx)
{
	SG_rbtree* prb1 = NULL;
	SG_rbtree* prb2 = NULL;
	SG_uint32 count1;

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb1)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "a", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "b", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "c", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "d", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "e", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "f", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "g", NULL)  );

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb2)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "h", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "i", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "j", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "k", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "l", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "m", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "n", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__from_other_rbtree(pCtx, prb1, prb2)  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb1, &count1)  );
	VERIFY_COND("count", (count1 == 14));

fail:
	SG_RBTREE_NULLFREE(pCtx, prb1);
	SG_RBTREE_NULLFREE(pCtx, prb2);
}

void u0041_test_rbtree_4(SG_context * pCtx)
{
	SG_rbtree* prb1 = NULL;
	SG_uint32 count1;
	SG_uint32 countv;
	SG_varray* pva = NULL;

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb1)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "a", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "b", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "c", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "d", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "e", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "f", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "g", NULL)  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb1, &count1)  );
	VERIFY_COND("count", (count1 == 7));

	VERIFY_ERR_CHECK(  SG_rbtree__to_varray__keys_only(pCtx, prb1, &pva)  );

	VERIFY_ERR_CHECK(  SG_varray__count(pCtx, pva, &countv)  );
	VERIFY_COND("count", (countv == 7));

fail:
	SG_RBTREE_NULLFREE(pCtx, prb1);
	SG_VARRAY_NULLFREE(pCtx, pva);
}

#if 0
void u0041_test_rbtree_5(SG_context * pCtx)
{
	char szHex[SG_GID_BUFFER_LENGTH];
	SG_uint32 i;
	SG_strpool* pPool = NULL;
	SG_rbtree* prb1 = NULL;
	SG_int64 t1;
	SG_int64 t2;
	SG_int32 elapsed_ms;
	SG_rbtree* pids = NULL;

	VERIFY_ERR_CHECK(  SG_STRPOOL__ALLOC(pCtx, &pPool, 128*1024)  );

	/*
	 * Insert a bunch of GIDs into a rbtree tree.
	 * This should naturally end up with a reasonble
	 * depth, since the data is random.
	 */

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb1)  );

	SG_time__get_microseconds_since_1970_utc(&t1);

	for (i=0; i<10000; i++)
	{
		const char* psz;

		VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, szHex, sizeof(szHex))  );

		VERIFY_ERR_CHECK(  SG_strpool__add__with_assoc(pCtx, pPool, szHex, &psz)  );

		VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, psz, NULL)  );

		if ((i % 100) == 0)
		{
			VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb1, psz)  );
		}
	}

	SG_time__get_microseconds_since_1970_utc(&t2);

	elapsed_ms = (SG_int32) (t2 - t1) / 1000;

	printf("rbtree: %d\n", elapsed_ms);

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pids)  );

	SG_time__get_microseconds_since_1970_utc(&t1);

	for (i=0; i<10000; i++)
	{
		VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, szHex, sizeof(szHex))  );

		VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, pids, szHex)  );

		if ((i % 100) == 0)
		{
			VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, pids, szHex)  );
		}
	}

	SG_time__get_microseconds_since_1970_utc(&t2);

	elapsed_ms = (SG_int32) (t2 - t1) / 1000;

	printf("idset: %d\n", elapsed_ms);

fail:
	SG_RBTREE_NULLFREE(pCtx, pids);
	SG_STRPOOL_NULLFREE(pCtx, pPool);
	SG_RBTREE_NULLFREE(pCtx, prb1);
}
#endif

void u0041_test_rbtree_6(SG_context * pCtx)
{
	SG_rbtree* prb1 = NULL;
	SG_rbtree* prb2 = NULL;
	SG_uint32 count1;
	const char** paKeys = NULL;
	SG_rbtree_iterator *pIterator = NULL;
	SG_bool b;
	const char* pszKey = NULL;
	SG_uint32 i;

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb1)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "a", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "c", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "e", NULL)  );

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb2)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "b", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "d", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "f", NULL)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add__from_other_rbtree(pCtx, prb1, prb2)  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb1, &count1)  );
	VERIFY_COND("count", (count1 == 6));

	paKeys = SG_calloc(count1, sizeof(const char*));
	VERIFY_COND("alloc", (paKeys != NULL));

	VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, prb1, &b, &pszKey, NULL)  );
	i = 0;
	while (b)
	{
		paKeys[i++] = pszKey;

		VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, NULL)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);

	VERIFY_COND("check", (0 == strcmp(paKeys[0], "a")));
	VERIFY_COND("check", (0 == strcmp(paKeys[1], "b")));
	VERIFY_COND("check", (0 == strcmp(paKeys[2], "c")));
	VERIFY_COND("check", (0 == strcmp(paKeys[3], "d")));
	VERIFY_COND("check", (0 == strcmp(paKeys[4], "e")));
	VERIFY_COND("check", (0 == strcmp(paKeys[5], "f")));

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
	SG_RBTREE_NULLFREE(pCtx, prb1);
	SG_RBTREE_NULLFREE(pCtx, prb2);
	SG_NULLFREE(pCtx, paKeys);
}

//////////////////////////////////////////////////////////////////

SG_rbtree_compare_function_callback u0041_test_rbtree__reverse_order;

int u0041_test_rbtree__reverse_order(const char * psz1, const char * psz2)
{
	return (strcmp(psz2,psz1));
}

void u0041_test_rbtree_6_reverse(SG_context * pCtx)
{
	SG_rbtree* prb1 = NULL;
	SG_rbtree* prb2 = NULL;
	SG_uint32 count1;
	const char** paKeys = NULL;
	SG_rbtree_iterator *pIterator = NULL;
	SG_bool b;
	const char* pszKey = NULL;
	SG_uint32 i;

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS2(pCtx, &prb1, 128, NULL, u0041_test_rbtree__reverse_order)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "a", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "c", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "e", NULL)  );

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS2(pCtx, &prb2, 128, NULL, u0041_test_rbtree__reverse_order)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "b", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "d", NULL)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb2, "f", NULL)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add__from_other_rbtree(pCtx, prb1, prb2)  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb1, &count1)  );
	VERIFY_COND("count", (count1 == 6));

	paKeys = SG_calloc(count1, sizeof(const char*));
	VERIFY_COND("alloc", (paKeys != NULL));

	VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, prb1, &b, &pszKey, NULL)  );
	i = 0;
	while (b)
	{
		paKeys[i++] = pszKey;

		VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, NULL)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);

	VERIFY_COND("check", (0 == strcmp(paKeys[0], "f")));
	VERIFY_COND("check", (0 == strcmp(paKeys[1], "e")));
	VERIFY_COND("check", (0 == strcmp(paKeys[2], "d")));
	VERIFY_COND("check", (0 == strcmp(paKeys[3], "c")));
	VERIFY_COND("check", (0 == strcmp(paKeys[4], "b")));
	VERIFY_COND("check", (0 == strcmp(paKeys[5], "a")));

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
	SG_RBTREE_NULLFREE(pCtx, prb1);
	SG_RBTREE_NULLFREE(pCtx, prb2);
	SG_NULLFREE(pCtx, paKeys);
}

void u0041_test_rbtree_7(SG_context * pCtx)
{
	SG_rbtree* prb1 = NULL;
	void* q;

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb1)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, prb1, "a")  );
	VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, prb1, "c")  );
	VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, prb1, "e")  );

	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_rbtree__add(pCtx, prb1, "e")  );

	VERIFY_ERR_CHECK(  SG_rbtree__update(pCtx, prb1, "e")  );

	q = "never";
	VERIFY_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, prb1, "e", "now", &q)  );
	VERIFY_COND("should be null", (!q));

	VERIFY_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, prb1, "e", "later", &q)  );
	VERIFY_COND("should be", (0 == strcmp(q, "now")));

	VERIFY_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, prb1, "e", &q)  );
	VERIFY_COND("should be", (0 == strcmp(q, "later")));

	VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb1, "c")  );

	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_rbtree__remove(pCtx, prb1, "e")  );

fail:
	SG_RBTREE_NULLFREE(pCtx, prb1);
}

void u0041_test_rbtree_8(SG_context * pCtx)
{
	SG_rbtree* prb1 = NULL;
	SG_uint32 count;
	void* q = NULL;

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb1)  );

	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "a", "1")  );
	VERIFY_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb1, "c", "2")  );

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb1, &count)  );
	VERIFY_COND("count", (count == 2));

	VERIFY_ERR_CHECK(  SG_rbtree__remove__with_assoc(pCtx, prb1, "a", &q)  );
	VERIFY_COND("q", (0 == strcmp(q, "1")));

	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb1, &count)  );
	VERIFY_COND("count", (count == 1));

fail:
	SG_RBTREE_NULLFREE(pCtx, prb1);
}

#if 0 // SG_rbtree__memoryblob__add__hid() now requires a pRepo to compute the hashes.  ignore this test for now.
void u0041_test_rbtree_9(SG_context* pCtx)
{
	SG_rbtree* prb1 = NULL;
    SG_byte buf[1024];
    SG_uint32 len;
    SG_uint32 i;
    const char* pszHid = NULL;
    SG_byte* pBytes = NULL;
    const SG_byte* pb = NULL;

    for (i=0; i<1024; i++)
    {
        buf[i] = (SG_byte) ((i) & 0xff);
    }

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb1)  );

    VERIFY_ERR_CHECK(  SG_rbtree__memoryblob__add__name(pCtx, prb1, "b1", buf, 400)  );
    VERIFY_ERR_CHECK(  SG_rbtree__memoryblob__add__hid(pCtx, prb1, buf, 400, &pszHid)  );
    VERIFY_ERR_CHECK(  SG_rbtree__memoryblob__alloc(pCtx, prb1, "b2", 300, &pBytes)  );
    pBytes[197] = buf[197];

    VERIFY_ERR_CHECK(  SG_rbtree__memoryblob__get(pCtx, prb1, "b1", &pb, &len)  );
    VERIFY_COND("len", (400 == len));
    VERIFY_COND("b", (pb[197] == buf[197]));

    VERIFY_ERR_CHECK(  SG_rbtree__memoryblob__get(pCtx, prb1, "b2", &pb, &len)  );
    VERIFY_COND("len", (300 == len));
    VERIFY_COND("b", (pb[197] == buf[197]));

	SG_RBTREE_NULLFREE(pCtx, prb1);

	return;
fail:
	SG_RBTREE_NULLFREE(pCtx, prb1);
}
#endif

//////////////////////////////////////////////////////////////////
// the __many_gids_in_rbtree_ tests were in u0002_id.  they were
// originally written when IDs were packed (binary) and HID/GIDs
// were interchangable, and when we had IDSETs as a container.
// so they no longer really test what they were originally intended
// to test.  however, after all of the updates, they still do stress
// the gid generation and the rbtree storage.  so i moved them from
// u0002 to here and renamed them.

int u0041_rbtree__many_gids_in_rbtree_1(SG_context * pCtx)
{
	int count=100000;
	SG_rbtree* pids = NULL;
	char* pid1 = NULL;
	int i;

	VERIFY_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pids)  );

	for (i=0; i<count; i++)
	{
		SG_bool b = SG_FALSE;

		VERIFY_ERR_CHECK(  SG_gid__alloc(pCtx,&pid1)  );

		VERIFY_ERR_CHECK_DISCARD(  SG_rbtree__find(pCtx, pids, pid1, &b, NULL)  );
		VERIFYP_COND("not there", !b, (pid1));

		VERIFY_ERR_CHECK_DISCARD(  SG_rbtree__update(pCtx, pids, pid1)  );

		VERIFY_ERR_CHECK_DISCARD(  SG_rbtree__find(pCtx, pids, pid1, &b, NULL)  );
		VERIFY_COND("now it's there", b);

		SG_NULLFREE(pCtx, pid1);
	}

	SG_RBTREE_NULLFREE(pCtx, pids);
	return 1;

fail:
	SG_NULLFREE(pCtx, pid1);
	SG_RBTREE_NULLFREE(pCtx, pids);
	return 0;
}

int u0041_rbtree__many_gids_in_rbtree_2(SG_context * pCtx)
{
	SG_uint32 count=100;
	SG_uint32 c;
	SG_rbtree* pids = NULL;
	char* pid = NULL;
	SG_uint32 i;
	SG_rbtree_iterator* pIterator = NULL;
	SG_bool b = SG_FALSE;
	const char* pszKey = NULL;

	VERIFY_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pids)  );

	for (i=0; i<count; i++)
	{
		VERIFY_ERR_CHECK(  SG_gid__alloc(pCtx, &pid)  );

		VERIFY_ERR_CHECK_DISCARD(  SG_rbtree__update(pCtx, pids, pid)  );

		SG_NULLFREE(pCtx, pid);
	}

	VERIFY_ERR_CHECK_DISCARD(  SG_rbtree__count(pCtx,pids,&c)  );
	VERIFY_COND("test_idset2::count", (c == count));

	VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, pids, &b, &pszKey, NULL)  );
	while (b)
	{
		VERIFY_COND("idset__fetch_all", (pszKey != NULL));

		VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, NULL)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);

	SG_RBTREE_NULLFREE(pCtx, pids);
	return 1;

fail:
	SG_NULLFREE(pCtx, pid);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
	SG_RBTREE_NULLFREE(pCtx, pids);
	return 0;
}

int u0041_rbtree__many_gids_in_rbtree_3(SG_context * pCtx)
{
	char* pid = NULL;
	SG_rbtree* pids = NULL;
	SG_bool b;
	SG_uint32 count;

	VERIFY_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC(pCtx,&pids)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_rbtree__count(pCtx,pids,&count)  );
	VERIFY_COND("test_idset3::count", (count==0));

	VERIFY_ERR_CHECK(  SG_gid__alloc(pCtx,&pid)  );

	b = SG_FALSE;
	VERIFY_ERR_CHECK_DISCARD(  SG_rbtree__find(pCtx, pids, pid, &b, NULL)  );
	VERIFY_COND("not there", !b);

	VERIFY_ERR_CHECK_DISCARD(  SG_rbtree__update(pCtx, pids, pid)  );

	b = SG_FALSE;
	VERIFY_ERR_CHECK_DISCARD(  SG_rbtree__find(pCtx, pids, pid, &b, NULL)  );
	VERIFY_COND("now it's there", b);

	VERIFY_ERR_CHECK_DISCARD(  SG_rbtree__update(pCtx, pids, pid)  );

	b = SG_FALSE;
	VERIFY_ERR_CHECK_DISCARD(  SG_rbtree__find(pCtx, pids, pid, &b, NULL)  );
	VERIFY_COND("still there", b);

	SG_NULLFREE(pCtx, pid);
	SG_RBTREE_NULLFREE(pCtx, pids);
	return 1;

fail:
	SG_NULLFREE(pCtx, pid);
	SG_RBTREE_NULLFREE(pCtx, pids);
	return 1;
}

int u0041_rbtree__rm_nonexistent(SG_context * pCtx)
{
	// See SPRAWL-786

	SG_rbtree* prb = NULL;
	SG_rbtree_iterator* pit = NULL;
	SG_bool ok;
	const char* pszKey;
	SG_uint32 i;

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, prb, "p")  );
	VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, prb, "k")  );
	VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, prb, "g")  );
	VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, prb, "d")  );
	VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, prb, "b")  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_rbtree__remove(pCtx, prb, "a"), SG_ERR_NOT_FOUND);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_rbtree__remove(pCtx, prb, "c"), SG_ERR_NOT_FOUND);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_rbtree__remove(pCtx, prb, "e"), SG_ERR_NOT_FOUND);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_rbtree__remove(pCtx, prb, "f"), SG_ERR_NOT_FOUND);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_rbtree__remove(pCtx, prb, "h"), SG_ERR_NOT_FOUND);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_rbtree__remove(pCtx, prb, "i"), SG_ERR_NOT_FOUND);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_rbtree__remove(pCtx, prb, "j"), SG_ERR_NOT_FOUND);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_rbtree__remove(pCtx, prb, "l"), SG_ERR_NOT_FOUND);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_rbtree__remove(pCtx, prb, "m"), SG_ERR_NOT_FOUND);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_rbtree__remove(pCtx, prb, "n"), SG_ERR_NOT_FOUND);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_rbtree__remove(pCtx, prb, "o"), SG_ERR_NOT_FOUND);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_rbtree__remove(pCtx, prb, "q"), SG_ERR_NOT_FOUND);
	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &i)  );
	VERIFY_COND("", 5 == i);
	VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb, &ok, &pszKey, NULL)  );
	for (i = 0; ok; i++)
	{
		if (0 == i)
			VERIFY_COND("", 0 == strcmp("b", pszKey));
		else if (1 == i)
			VERIFY_COND("", 0 == strcmp("d", pszKey));
		else if (2 == i)
			VERIFY_COND("", 0 == strcmp("g", pszKey));
		else if (3 == i)
			VERIFY_COND("", 0 == strcmp("k", pszKey));
		else if (4 == i)
			VERIFY_COND("", 0 == strcmp("p", pszKey));
		VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &ok, &pszKey, NULL)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
	VERIFY_COND("", 5 == i);

	VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb, "g")  );
	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &i)  );
	VERIFY_COND("", 4 == i);
	VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb, &ok, &pszKey, NULL)  );
	for (i = 0; ok; i++)
	{
		if (0 == i)
			VERIFY_COND("", 0 == strcmp("b", pszKey));
		else if (1 == i)
			VERIFY_COND("", 0 == strcmp("d", pszKey));
		else if (2 == i)
			VERIFY_COND("", 0 == strcmp("k", pszKey));
		else if (3 == i)
			VERIFY_COND("", 0 == strcmp("p", pszKey));
		VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &ok, &pszKey, NULL)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
	VERIFY_COND("", 4 == i);

	VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb, "b")  );
	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &i)  );
	VERIFY_COND("", 3 == i);
	VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb, &ok, &pszKey, NULL)  );
	for (i = 0; ok; i++)
	{
		if (0 == i)
			VERIFY_COND("", 0 == strcmp("d", pszKey));
		else if (1 == i)
			VERIFY_COND("", 0 == strcmp("k", pszKey));
		else if (2 == i)
			VERIFY_COND("", 0 == strcmp("p", pszKey));
		VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &ok, &pszKey, NULL)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
	VERIFY_COND("", 3 == i);

	VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb, "p")  );
	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &i)  );
	VERIFY_COND("", 2 == i);
	VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb, &ok, &pszKey, NULL)  );
	for (i = 0; ok; i++)
	{
		if (0 == i)
			VERIFY_COND("", 0 == strcmp("d", pszKey));
		else if (1 == i)
			VERIFY_COND("", 0 == strcmp("k", pszKey));
		VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &ok, &pszKey, NULL)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
	VERIFY_COND("", 2 == i);

	VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb, "d")  );
	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &i)  );
	VERIFY_COND("", 1 == i);
	VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb, &ok, &pszKey, NULL)  );
	for (i = 0; ok; i++)
	{
		if (0 == i)
			VERIFY_COND("", 0 == strcmp("k", pszKey));
		VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &ok, &pszKey, NULL)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
	VERIFY_COND("", 1 == i);

	VERIFY_ERR_CHECK(  SG_rbtree__remove(pCtx, prb, "k")  );
	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &i)  );
	VERIFY_COND("", 0 == i);
	pszKey = NULL;
	VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb, &ok, &pszKey, NULL)  );
	VERIFY_COND("", !ok);
	VERIFY_COND("", !pszKey);
	VERIFY_COND("", !pit);

	/* fall through */
fail:
	SG_RBTREE_NULLFREE(pCtx, prb);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
	return 1;
}

//////////////////////////////////////////////////////////////////

TEST_MAIN(u0041_rbtree)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0041_test_rbtree_0(pCtx)  );
	BEGIN_TEST(  u0041_test_rbtree_1(pCtx)  );
	BEGIN_TEST(  u0041_test_rbtree_2(pCtx)  );
	BEGIN_TEST(  u0041_test_rbtree_3(pCtx)  );
	BEGIN_TEST(  u0041_test_rbtree_4(pCtx)  );
	//BEGIN_TEST(   u0041_test_rbtree_5(pCtx)  );
	BEGIN_TEST(  u0041_test_rbtree_6(pCtx)  );
	BEGIN_TEST(  u0041_test_rbtree_6_reverse(pCtx)  );
	BEGIN_TEST(  u0041_test_rbtree_7(pCtx)  );
	BEGIN_TEST(  u0041_test_rbtree_8(pCtx)  );
	//BEGIN_TEST(  u0041_test_rbtree_9(pCtx)  );

	BEGIN_TEST(  u0041_rbtree__many_gids_in_rbtree_1(pCtx)  );
	BEGIN_TEST(  u0041_rbtree__many_gids_in_rbtree_2(pCtx)  );
	BEGIN_TEST(  u0041_rbtree__many_gids_in_rbtree_3(pCtx)  );

	//BEGIN_TEST(  u0041_rbtree__rm_nonexistent(pCtx)  );

	TEMPLATE_MAIN_END;
}
