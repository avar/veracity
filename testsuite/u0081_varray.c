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
 * @file u0081_varray.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"
#include "unittests_pendingtree.h"

//////////////////////////////////////////////////////////////////

static void _assertCount(
	 SG_context *pCtx,
	 const SG_varray *a,
	 SG_uint32 expected,
	 const char *desc)
{
	SG_uint32	count = 0;
	SG_string	*msg = NULL;

	VERIFY_ERR_CHECK(  SG_varray__count(pCtx, a, &count)  );

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &msg)  );
	VERIFY_ERR_CHECK(  SG_string__sprintf(pCtx, msg, "%s: expected %u, got %u", 
										  desc, (unsigned)expected, (unsigned)count
										  )  
					);

	VERIFY_COND(SG_string__sz(msg), (count == expected));

fail:
	SG_STRING_NULLFREE(pCtx, msg);
}


static void _assertStringAt(
	 SG_context *pCtx,
	 const SG_varray *a,
	 SG_uint32 pos,
	 const char *expected)
{
	SG_string	*msg = NULL;
	const char *got = NULL;

	VERIFY_ERR_CHECK(  SG_varray__get__sz(pCtx, a, pos, &got)  );
	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &msg)  );
	VERIFY_ERR_CHECK(  SG_string__sprintf(pCtx, msg, "string at %u: expected '%s', got '%s'", 
										  (unsigned)pos, expected, got
										  )  
					);

	VERIFY_COND(SG_string__sz(msg), strcmp(got, expected) == 0);

fail:
	SG_STRING_NULLFREE(pCtx, msg);
}

void u0081__remove_only(SG_context *pCtx)  
{
	SG_varray *a = NULL;

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &a)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_only")  );

	VERIFY_ERR_CHECK(  _assertCount(pCtx, a, 1, "size before delete")  );
	VERIFY_ERR_CHECK(  SG_varray__remove(pCtx, a, 0)  );
	VERIFY_ERR_CHECK(  _assertCount(pCtx, a, 0, "size after delete")  );

fail:
	SG_VARRAY_NULLFREE(pCtx, a);
}

void u0081__remove_first(SG_context *pCtx)  
{
	SG_varray *a = NULL;

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &a)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_first 0")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_first 1")  );

	VERIFY_ERR_CHECK(  _assertCount(pCtx, a, 2, "size before delete")  );
	VERIFY_ERR_CHECK(  SG_varray__remove(pCtx, a, 0)  );
	VERIFY_ERR_CHECK(  _assertCount(pCtx, a, 1, "size after delete")  );
	VERIFY_ERR_CHECK(  _assertStringAt(pCtx, a, 0, "u0081__remove_first 1")  );

fail:
	SG_VARRAY_NULLFREE(pCtx, a);
}

void u0081__remove_last(SG_context *pCtx)  
{
	SG_varray *a = NULL;

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &a)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_last 0")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_last 1")  );

	VERIFY_ERR_CHECK(  _assertCount(pCtx, a, 2, "size before delete")  );
	VERIFY_ERR_CHECK(  SG_varray__remove(pCtx, a, 1)  );
	VERIFY_ERR_CHECK(  _assertCount(pCtx, a, 1, "size after delete")  );
	VERIFY_ERR_CHECK(  _assertStringAt(pCtx, a, 0, "u0081__remove_last 0")  );

fail:
	SG_VARRAY_NULLFREE(pCtx, a);
}

void u0081__remove_middle(SG_context *pCtx)  
{
	SG_varray *a = NULL;

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &a)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_middle 0")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_middle 1")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_middle 2")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_middle 3")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_middle 4")  );

	VERIFY_ERR_CHECK(  _assertCount(pCtx, a, 5, "size before delete")  );
	VERIFY_ERR_CHECK(  SG_varray__remove(pCtx, a, 1)  );
	VERIFY_ERR_CHECK(  _assertCount(pCtx, a, 4, "size after delete")  );
	VERIFY_ERR_CHECK(  _assertStringAt(pCtx, a, 0, "u0081__remove_middle 0")  );
	VERIFY_ERR_CHECK(  _assertStringAt(pCtx, a, 1, "u0081__remove_middle 2")  );
	VERIFY_ERR_CHECK(  _assertStringAt(pCtx, a, 2, "u0081__remove_middle 3")  );
	VERIFY_ERR_CHECK(  _assertStringAt(pCtx, a, 3, "u0081__remove_middle 4")  );

fail:
	SG_VARRAY_NULLFREE(pCtx, a);
}

void u0081__remove_all(SG_context *pCtx)  
{
	SG_varray *a = NULL;

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &a)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_all 0")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_all 1")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_all 2")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_all 3")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_all 4")  );

	VERIFY_ERR_CHECK(  _assertCount(pCtx, a, 5, "size before delete")  );
	VERIFY_ERR_CHECK(  SG_varray__remove(pCtx, a, 1)  );
	VERIFY_ERR_CHECK(  SG_varray__remove(pCtx, a, 3)  );
	VERIFY_ERR_CHECK(  SG_varray__remove(pCtx, a, 0)  );
	VERIFY_ERR_CHECK(  SG_varray__remove(pCtx, a, 1)  );
	VERIFY_ERR_CHECK(  SG_varray__remove(pCtx, a, 0)  );
	VERIFY_ERR_CHECK(  _assertCount(pCtx, a, 0, "size after delete")  );

fail:
	SG_VARRAY_NULLFREE(pCtx, a);
}

void u0081__remove_past_end(SG_context *pCtx)  
{
	SG_varray *a = NULL;

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &a)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_past_end")  );

	SG_varray__remove(pCtx, a, 1);

	VERIFY_CTX_HAS_ERR("error expected", pCtx);
	VERIFY_CTX_ERR_EQUALS("out-of-range expected", pCtx, SG_ERR_VARRAY_INDEX_OUT_OF_RANGE);
	SG_context__err_reset(pCtx);
fail:
	SG_VARRAY_NULLFREE(pCtx, a);
}

void  u0081__remove_last_twice(SG_context * pCtx)
{
	SG_varray *a = NULL;

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &a)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, a, "u0081__remove_last_twice")  );

	VERIFY_ERR_CHECK(  SG_varray__remove(pCtx, a, 0)  );
	SG_varray__remove(pCtx, a, 0);

	VERIFY_CTX_HAS_ERR("error expected", pCtx);
	VERIFY_CTX_ERR_EQUALS("out-of-range expected", pCtx, SG_ERR_VARRAY_INDEX_OUT_OF_RANGE);
	SG_context__err_reset(pCtx);
fail:
	SG_VARRAY_NULLFREE(pCtx, a);
}  

TEST_MAIN(u0081_varray)
{
	TEMPLATE_MAIN_START;

        BEGIN_TEST(  u0081__remove_only(pCtx)  );
        BEGIN_TEST(  u0081__remove_first(pCtx)  );
        BEGIN_TEST(  u0081__remove_last(pCtx)  );
        BEGIN_TEST(  u0081__remove_middle(pCtx)  );
        BEGIN_TEST(  u0081__remove_all(pCtx)  );
        BEGIN_TEST(  u0081__remove_past_end(pCtx)  );
        BEGIN_TEST(  u0081__remove_last_twice(pCtx)  );


	TEMPLATE_MAIN_END;
}

#undef AA
#undef MY_VERIFY_IN_SECTION
#undef MY_VERIFY_MOVED_FROM
#undef MY_VERIFY_NAMES
#undef MY_VERIFY_REPO_PATH
#undef MY_VERIFY__IN_SECTION__REPO_PATH
