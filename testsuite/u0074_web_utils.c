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

//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0074_web_utils)
#define MyDcl(name)				u0074_web_utils__##name
#define MyFn(name)				u0074_web_utils__##name


// Note u008_misc_utils also contains some tests of functions in sg_str_utils, since those functions used to be in sg_misc_utils.


static void checkEncode(SG_context *pCtx, const char *instr, const char *expected)
{
	SG_string *sinstr = NULL;
	SG_string *sresult = NULL;
	SG_string *msg = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &msg)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &sinstr, instr)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &sresult, "unset")  );

	VERIFY_ERR_CHECK(  SG_htmlencode(pCtx, sinstr, sresult)  );

	SG_ERR_CHECK(  SG_string__sprintf(pCtx, msg, "encoding '%s': expected '%s', got '%s'", instr, expected, SG_string__sz(sresult))  );

	VERIFY_COND(SG_string__sz(msg), strcmp(SG_string__sz(sresult), expected) == 0);

fail:
	SG_STRING_NULLFREE(pCtx, msg);
	SG_STRING_NULLFREE(pCtx, sinstr);
	SG_STRING_NULLFREE(pCtx, sresult);
}

void MyFn(test__encodeTrivial)(SG_context *pCtx)
{
	SG_ERR_CHECK_RETURN( checkEncode(pCtx, "", "")  );
	SG_ERR_CHECK_RETURN( checkEncode(pCtx, "abc123", "abc123")  );

}

void MyFn(test__encodeSpecials)(SG_context *pCtx)
{
	SG_ERR_CHECK_RETURN( checkEncode(pCtx, "<", "&lt;")  );
	SG_ERR_CHECK_RETURN( checkEncode(pCtx, "< and <", "&lt; and &lt;")  );
	SG_ERR_CHECK_RETURN(  checkEncode(pCtx, ">", "&gt;")  );
	SG_ERR_CHECK_RETURN(  checkEncode(pCtx, ">> and >", "&gt;&gt; and &gt;")  );
	SG_ERR_CHECK_RETURN(  checkEncode(pCtx, "&", "&amp;")  );
	SG_ERR_CHECK_RETURN(  checkEncode(pCtx, "\"", "&quot;")  );
	SG_ERR_CHECK_RETURN(  checkEncode(pCtx, "\" > & < \"", "&quot; &gt; &amp; &lt; &quot;")  );
}

static void checkQsParse(SG_context *pCtx, const char *qs, const char **expected)
{
	SG_uint32	vhcount = 0;
	SG_uint32	expectedCount = 0;
	SG_vhash   *params = NULL;
	SG_bool	    found = SG_FALSE;
	SG_string  *msg = NULL;

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &params)  );

	VERIFY_ERR_CHECK(  SG_querystring_to_vhash(pCtx, qs, params)  );
	SG_ERR_CHECK(  SG_vhash__count(pCtx, params, &vhcount)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &msg)  );

	while (*expected != NULL)
	{
		const char *vari = *expected++;
		const char *value = *expected++;
		const char *got = NULL;

		++expectedCount;

		SG_ERR_CHECK(  SG_vhash__has(pCtx, params, vari, &found)  );
		VERIFY_COND(vari, found);

		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, params, vari, &got)  );

		SG_ERR_CHECK(  SG_string__sprintf(pCtx, msg, "value of %s: expected '%s', got '%s'",
			vari, value, got)  );
		VERIFY_COND(SG_string__sz(msg), strcmp(value, got) == 0);
	}

	VERIFY_COND("count matches expected", expectedCount == vhcount);

fail:
	SG_VHASH_NULLFREE(pCtx, params);
	SG_STRING_NULLFREE(pCtx, msg);
}

void MyFn(test__queryStringSimple)(SG_context *pCtx)
{
	const char *qsempty = "";
	const char * expectedEmpty[] = {
		NULL, NULL
	};
	const char *qsone= "one=uno";
	const char * expectedOne[] = {
		"one", "uno",
		NULL, NULL
	};

	SG_ERR_CHECK_RETURN(  checkQsParse(pCtx, qsempty, expectedEmpty)  );
	SG_ERR_CHECK_RETURN(  checkQsParse(pCtx, qsone, expectedOne)  );
}

void MyFn(test__queryStringMultiple)(SG_context *pCtx)
{
	const char *qs = "one=uno&two=dos&three=3";
	const char * expected[] = {
		"one", "uno",
		"two", "dos",
		"three", "3",
		NULL, NULL
	};

	SG_ERR_CHECK_RETURN(  checkQsParse(pCtx, qs, expected)  );
}

void MyFn(test__queryStringDecode)(SG_context *pCtx)
{
	const char *qs = "one=have%20space&two=have+plus&three=two%20%20spaces";
	const char * expected[] = {
		"one", "have space",
		"two", "have plus",
		"three", "two  spaces",
		NULL, NULL
	};

	SG_ERR_CHECK_RETURN(  checkQsParse(pCtx, qs, expected)  );
}



MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(test__encodeTrivial)(pCtx)  );
	BEGIN_TEST(  MyFn(test__encodeSpecials)(pCtx)  );
	BEGIN_TEST(  MyFn(test__queryStringSimple)(pCtx)  );
	BEGIN_TEST(  MyFn(test__queryStringMultiple)(pCtx)  );
	BEGIN_TEST(  MyFn(test__queryStringDecode)(pCtx)  );

	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn
