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
 * @file u0044_xattr.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

#ifdef SG_BUILD_FLAG_TEST_XATTR

int u0044_xattr__test_0(SG_context* pCtx)
{
	SG_pathname* pPath = NULL;
	SG_rbtree* prb = NULL;
	SG_uint32 count = 0;
	SG_rbtree_iterator* pIterator = NULL;
	const char* pszKey = NULL;
	const SG_byte* pPacked = NULL;
	SG_bool b;
	SG_uint32 len;
	const SG_byte* pData = NULL;
    char bufFile[SG_TID_MAX_BUFFER_LENGTH];
    SG_string* pstr = NULL;
    SG_strpool* pPool = NULL;

    VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufFile, sizeof(bufFile), 32)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx,&pPath, bufFile)  );

    VERIFY_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx,&pstr, "nowhere")  );
    VERIFY_ERR_CHECK(  SG_fsobj__symlink(pCtx,pstr, pPath)  );
    SG_STRING_NULLFREE(pCtx, pstr);

	VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx,pPath, NULL, &prb)  );
	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx,prb, &count)  );
    VERIFY_COND("count", (0 == count));
	SG_RBTREE_NULLFREE(pCtx, prb);

    VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx,pPath, "foo", 4, (SG_byte*)"bar")  );
	VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx,pPath, NULL, &prb)  );
	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx,prb, &count)  );
    VERIFY_COND("count", (1 == count));

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,&pIterator, prb, &b, &pszKey, (void**) &pPacked)  );
	while (b)
	{
		SG_ERR_CHECK(  SG_memoryblob__unpack(pCtx,pPacked, &pData, &len)  );

        VERIFY_COND("len", (4 == len));
        VERIFY_COND("bar", (0 == strcmp((char*)pData, "bar")));

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIterator, &b, &pszKey, (void**) &pPacked)  );
	}
	SG_rbtree__iterator__free(pCtx, pIterator);
	pIterator = NULL;
	SG_RBTREE_NULLFREE(pCtx, prb);

    VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx,pPath, "go", 4, (SG_byte*)"illini")  );
	VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx,pPath, NULL, &prb)  );
	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx,prb, &count)  );
    VERIFY_COND("count", (2 == count));
	SG_RBTREE_NULLFREE(pCtx, prb);

    VERIFY_ERR_CHECK(  SG_STRPOOL__ALLOC(pCtx,&pPool, 1)  );
    len = 0;
    pData = NULL;
    VERIFY_ERR_CHECK(  SG_fsobj__xattr__get(pCtx,pPath, pPool, "foo", &len, &pData)  );
    VERIFY_COND("len", (4 == len));
    VERIFY_COND("bar", (0 == strcmp((char*)pData, "bar")));
    SG_STRPOOL_NULLFREE(pCtx, pPool);

    VERIFY_ERR_CHECK(  SG_fsobj__xattr__remove(pCtx,pPath, "foo")  );

	VERIFY_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx,pPath, NULL, &prb)  );
	VERIFY_ERR_CHECK(  SG_rbtree__count(pCtx,prb, &count)  );
    VERIFY_COND("count", (1 == count));
	SG_RBTREE_NULLFREE(pCtx, prb);

	SG_PATHNAME_NULLFREE(pCtx, pPath);

	return 0;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	return -1;
}
#endif

TEST_MAIN(u0044_xattr)
{
	TEMPLATE_MAIN_START;

#ifdef SG_BUILD_FLAG_TEST_XATTR
	BEGIN_TEST(  u0044_xattr__test_0(pCtx)  );
#else
    SG_UNUSED(pCtx);
	INFOP("u0044_xattr",("skipping XATTR tests"));
#endif

	TEMPLATE_MAIN_END;
}
