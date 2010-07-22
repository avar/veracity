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
 * @file u0031_repopath.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

void u0031_test_repopath_1(SG_context * pCtx)
{
	SG_strpool* pPool = NULL;
	const char** apszParts = NULL;
	SG_uint32 count = 0;

	VERIFY_ERR_CHECK(  SG_STRPOOL__ALLOC(pCtx, &pPool, 4)  );

	VERIFY_ERR_CHECK(  SG_repopath__split(pCtx, "@/foo/bar/hello/world", pPool, &apszParts, &count)  );
	VERIFY_COND("ok", (apszParts != NULL));
	VERIFY_COND("ok", (count == 5));

	VERIFY_COND("part", (0 == strcmp(apszParts[0], "@")));
	VERIFY_COND("part", (0 == strcmp(apszParts[1], "foo")));
	VERIFY_COND("part", (0 == strcmp(apszParts[2], "bar")));
	VERIFY_COND("part", (0 == strcmp(apszParts[3], "hello")));
	VERIFY_COND("part", (0 == strcmp(apszParts[4], "world")));

	SG_NULLFREE(pCtx, apszParts);

	VERIFY_ERR_CHECK(  SG_repopath__split(pCtx, "@/foo/bar/hello/world/", pPool, &apszParts, &count)  );
	VERIFY_COND("ok", (apszParts != NULL));
	VERIFY_COND("ok", (count == 5));

	VERIFY_COND("part", (0 == strcmp(apszParts[0], "@")));
	VERIFY_COND("part", (0 == strcmp(apszParts[1], "foo")));
	VERIFY_COND("part", (0 == strcmp(apszParts[2], "bar")));
	VERIFY_COND("part", (0 == strcmp(apszParts[3], "hello")));
	VERIFY_COND("part", (0 == strcmp(apszParts[4], "world")));

fail:
	SG_NULLFREE(pCtx, apszParts);
	SG_STRPOOL_NULLFREE(pCtx, pPool);
}

void u0031_test_repopath_removeFinalSlash(SG_context * pCtx)
{
	SG_string * testString = NULL;

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &testString)  );

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, testString, "@/"));
	VERIFY_ERR_CHECK(  SG_repopath__remove_final_slash(pCtx, testString)  );
	VERIFY_COND("@/", (0 == strcmp(SG_string__sz(testString), "@")));

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, testString, "@"));
	VERIFY_ERR_CHECK(  SG_repopath__remove_final_slash(pCtx, testString)  );
	VERIFY_COND("@", (0 == strcmp(SG_string__sz(testString), "@")));

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, testString, "relative/path//")  );
	VERIFY_ERR_CHECK(  SG_repopath__remove_final_slash(pCtx, testString)  );
	VERIFY_COND("relative/path//", (0 == strcmp(SG_string__sz(testString), "relative/path")));

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, testString, "relative/path/")  );
	VERIFY_ERR_CHECK(  SG_repopath__remove_final_slash(pCtx, testString)  );
	VERIFY_COND("relative/path/", (0 == strcmp(SG_string__sz(testString), "relative/path")));

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, testString, "///////")  );
	VERIFY_ERR_CHECK(  SG_repopath__remove_final_slash(pCtx, testString)  );
	VERIFY_COND("///////", (0 == strcmp(SG_string__sz(testString), "")));
fail:
		SG_STRING_NULLFREE(pCtx, testString);
}

void u0031_test_repopath_normalize(SG_context * pCtx)
{
	SG_string * testString = NULL;

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &testString)  );

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, testString, "@/path/./with/"));
	VERIFY_ERR_CHECK(  SG_repopath__normalize(pCtx, testString, SG_FALSE)  );
	VERIFY_COND("@/path/./with/", (0 == strcmp(SG_string__sz(testString), "@/path/with")));

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, testString, "@/path/with"));
	VERIFY_ERR_CHECK(  SG_repopath__normalize(pCtx, testString, SG_FALSE)  );
	VERIFY_COND("@/path/\\\\with\\", (0 == strcmp(SG_string__sz(testString), "@/path/with")));

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, testString, "./test\\path")  );
	VERIFY_ERR_CHECK(  SG_repopath__normalize(pCtx, testString, SG_FALSE)  );
	VERIFY_COND("./test\\path", (0 == strcmp(SG_string__sz(testString), "test/path")));

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, testString, "relative\\path\\\\")  );
	VERIFY_ERR_CHECK(  SG_repopath__normalize(pCtx, testString, SG_FALSE)  );
	VERIFY_COND("relative\\path\\\\", (0 == strcmp(SG_string__sz(testString), "relative/path")));

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, testString, "./././relative/path")  );
	VERIFY_ERR_CHECK(  SG_repopath__normalize(pCtx, testString, SG_FALSE)  );
	VERIFY_COND("./././relative/path", (0 == strcmp(SG_string__sz(testString), "relative/path")));

	VERIFY_ERR_CHECK(  SG_string__set__sz(pCtx, testString, "./././")  );
	VERIFY_ERR_CHECK(  SG_repopath__normalize(pCtx, testString, SG_FALSE)  );
	VERIFY_COND("./././", (0 == strcmp(SG_string__sz(testString), "")));
fail:
		SG_STRING_NULLFREE(pCtx, testString);
}

TEST_MAIN(u0031_repopath)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0031_test_repopath_1(pCtx)  );
	BEGIN_TEST(  u0031_test_repopath_removeFinalSlash(pCtx)  );
	BEGIN_TEST(  u0031_test_repopath_normalize(pCtx)  );

	TEMPLATE_MAIN_END;
}
