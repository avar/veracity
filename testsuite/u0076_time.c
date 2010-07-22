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

#define MyMain()				TEST_MAIN(u0076_time)
#define MyDcl(name)				u0076_time__##name
#define MyFn(name)				u0076_time__##name



void MyFn(test__parseRfc850)(SG_context *pCtx)
{
	const char *ims = "Tue, 16 Mar 2010 14:11:13 GMT";
	SG_int32 epoch = 1268748673;
	SG_int64 sgtime = (SG_int64)epoch * 1000;
	SG_string *msg = NULL;
	SG_int64 parsed = 0, localparsed;

	SG_ERR_CHECK(  SG_time__parseRFC850(pCtx, ims, &parsed, &localparsed)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &msg)  );
	SG_ERR_CHECK(  SG_string__sprintf(pCtx, msg, "parsing '%s': expected '%lli', got '%lli'", ims, (long long)sgtime, (long long)parsed)  );

	VERIFY_COND(SG_string__sz(msg), parsed == sgtime);

fail:
	SG_STRING_NULLFREE(pCtx, msg);
}


void MyFn(test__formatRfc850)(SG_context *pCtx)
{
	const char *expected = "Tue, 16 Mar 2010 14:11:13 GMT";
	SG_int32 epoch = 1268748673;
	SG_int64 sgtime = (SG_int64)epoch * 1000;
	SG_string *msg = NULL;
	char formatted[1024];

	SG_ERR_CHECK(  SG_time__formatRFC850(pCtx, sgtime, formatted, sizeof(formatted))  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &msg)  );
	SG_ERR_CHECK(  SG_string__sprintf(pCtx, msg, "formatting '%lli': expected '%s', got '%s'", sgtime, expected, formatted)  );

	VERIFY_COND(SG_string__sz(msg), strcmp(expected, formatted) == 0);

fail:
	SG_STRING_NULLFREE(pCtx, msg);
}


void MyFn(test__parseRfc850nonGmt)(SG_context *pCtx)
{
	const char *ims = "Tue, 16 Mar 2010 14:11:13 EDT";
	SG_int64 parsed = 0, localparsed;

	SG_time__parseRFC850(pCtx, ims, &parsed, &localparsed);
	VERIFY_CTX_ERR_EQUALS("out-of-range if not GMT", pCtx, SG_ERR_ARGUMENT_OUT_OF_RANGE);
	SG_context__err_reset(pCtx);
}




MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(test__parseRfc850)(pCtx)  );
	BEGIN_TEST(  MyFn(test__parseRfc850nonGmt)(pCtx)  );
	BEGIN_TEST(  MyFn(test__formatRfc850)(pCtx)  );

	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn
