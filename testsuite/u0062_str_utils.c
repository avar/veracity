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

#define MyMain()				TEST_MAIN(u0062_str_utils)
#define MyDcl(name)				u0062_str_utils__##name
#define MyFn(name)				u0062_str_utils__##name


// Note u008_misc_utils also contains some tests of functions in sg_str_utils, since those functions used to be in sg_misc_utils.


void MyFn(test__stricmp)(void)
{
	VERIFY_COND(  "stricmp"  ,  SG_stricmp("ABC","abc")==0  );
	VERIFY_COND(  "stricmp"  ,  SG_stricmp("abc","ABC")==0  );

	VERIFY_COND(  "stricmp"  ,  SG_stricmp("ABC","abz")<0  );
	VERIFY_COND(  "stricmp"  ,  SG_stricmp("abc","ABZ")<0  );

	VERIFY_COND(  "stricmp"  ,  SG_stricmp("ABZ","abc")>0  );
	VERIFY_COND(  "stricmp"  ,  SG_stricmp("abz","ABC")>0  );

	VERIFY_COND(  "stricmp"  ,  SG_stricmp("ABC","abcd")<0  );
	VERIFY_COND(  "stricmp"  ,  SG_stricmp("abc","ABCD")<0  );

	VERIFY_COND(  "stricmp"  ,  SG_stricmp("ABCD","abc")>0  );
	VERIFY_COND(  "stricmp"  ,  SG_stricmp("abcd","ABC")>0  );
}

void MyFn(test__substring)(SG_context * pCtx)
{
	char * sz = NULL;

	VERIFY_ERR_CHECK(  SG_ascii__substring(pCtx, "abcd", 1, 2, &sz)  );
	VERIFY_COND(  "substring"  ,  strcmp(sz,"bc")==0  );

fail:
	SG_NULLFREE(pCtx, sz );
}

void MyFn(test__substring_to_end)(SG_context * pCtx)
{
	char * sz = NULL;

	VERIFY_ERR_CHECK(  SG_ascii__substring__to_end(pCtx, "abcd", 2, &sz)  );
	VERIFY_COND(  "substring_to_end"  ,  strcmp(sz,"cd")==0  );

fail:
	SG_NULLFREE(pCtx, sz );
}

void MyFn(test__int64_parse)(SG_context * pCtx)
{
	SG_int64 i = 0;

	VERIFY_ERR_CHECK_DISCARD(  SG_int64__parse(pCtx, &i, "1234")  );
	VERIFY_COND( "int64_parse"  ,  i==1234 );

	VERIFY_ERR_CHECK_DISCARD(  SG_int64__parse(pCtx, &i, "-1234")  );
	VERIFY_COND( "int64_parse"  ,  i==-1234 );

	//todo: test how invalid input is handled...
}

void MyFn(test__double_parse)(SG_context * pCtx)
{
	double x = 0;

	VERIFY_ERR_CHECK_DISCARD(  SG_double__parse(pCtx, &x, "1234")  );
	VERIFY_COND( "double_parse"  ,  x==1234 );

	VERIFY_ERR_CHECK_DISCARD(  SG_double__parse(pCtx, &x, "-1234")  );
	VERIFY_COND( "double_parse"  ,  x==-1234 );

	VERIFY_ERR_CHECK_DISCARD(  SG_double__parse(pCtx, &x, "0.0")  );
	VERIFY_COND( "double_parse"  ,  x==0 );

	VERIFY_ERR_CHECK_DISCARD(  SG_double__parse(pCtx, &x, "-1.5e27")  );
	VERIFY_COND( "double_parse"  ,  x==-1.5e27 );

	VERIFY_ERR_CHECK_DISCARD(  SG_double__parse(pCtx, &x, "1.5e-27")  );
	VERIFY_COND( "double_parse"  ,  x==1.5e-27 );

	//todo: test how invalid input is handled...
}

void MyFn(test__int64_to_string)(SG_UNUSED_PARAM(SG_context * pCtx))
{
	SG_int_to_string_buffer s;
	SG_UNUSED(pCtx);

	VERIFY_COND("", strcmp("1234",SG_int64_to_sz(1234,s))==0 );
	VERIFY_COND("", strcmp("-1234",SG_int64_to_sz(-1234,s))==0 );
	VERIFY_COND("", strcmp("0",SG_int64_to_sz(0,s))==0 );
	VERIFY_COND("", strcmp("9223372036854775807",SG_int64_to_sz(SG_INT64_MAX,s))==0 );
	VERIFY_COND("", strcmp("-9223372036854775808",SG_int64_to_sz(SG_INT64_MIN,s))==0 );
	VERIFY_COND( "" , SG_int64_to_sz(-1,NULL)==NULL );

	VERIFY_COND("", strcmp("1234",SG_uint64_to_sz(1234,s))==0 );
	VERIFY_COND("", strcmp("0",SG_uint64_to_sz(0,s))==0 );
	VERIFY_COND("", strcmp("18446744073709551615",SG_uint64_to_sz(SG_UINT64_MAX,s))==0 );
	VERIFY_COND("", SG_uint64_to_sz(12345678,NULL)==NULL );

	VERIFY_COND( "" , strcmp("0000000000001234",SG_uint64_to_sz__hex(0x1234,s))==0 );
	VERIFY_COND( "" , strcmp("0000000000000000",SG_uint64_to_sz__hex(0,s))==0 );
	VERIFY_COND( "" , strcmp("ffffffffffffffff",SG_uint64_to_sz__hex(SG_UINT64_MAX,s))==0 );
}

void MyFn(test__sprintf_truncate)(SG_context * pCtx)
{
	char buf[25];

	VERIFY_ERR_CHECK(  SG_sprintf_truncate(pCtx, buf, sizeof(buf),
		"1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890")  );
	VERIFY_COND("simple truncated buffer contents incorrect", 0==strcmp(buf, "123456789012345678901234"));

	VERIFY_ERR_CHECK(  SG_sprintf_truncate(pCtx, buf, sizeof(buf),
		"%d %d %d", 2147483647,2147483647,2147483647)  );
	VERIFY_COND("formatted, truncated buffer contents incorrect", 0==strcmp(buf, "2147483647 2147483647 21"));

fail:
	return;
}

MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(test__stricmp)(/*pCtx*/)  );
	BEGIN_TEST(  MyFn(test__substring)(pCtx)  );
	BEGIN_TEST(  MyFn(test__substring_to_end)(pCtx)  );
	BEGIN_TEST(  MyFn(test__int64_parse)(pCtx)  );
	BEGIN_TEST(  MyFn(test__double_parse)(pCtx)  );
	BEGIN_TEST(  MyFn(test__int64_to_string)(pCtx)  );
	BEGIN_TEST(  MyFn(test__sprintf_truncate)(pCtx)  );

	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn
