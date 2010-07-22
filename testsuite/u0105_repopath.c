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

void  __testComparison(SG_context * pCtx, const char * pszPath1, const char * pszPath2,  SG_int32 expectedResult)
{
	SG_int32 nResult = 0;
	SG_ERR_CHECK(  SG_repopath__compare(pCtx, pszPath1, pszPath2, &nResult)  );
	if (expectedResult < 0)
		VERIFY_COND("result", nResult < 0);
	else if (expectedResult > 0)
		VERIFY_COND("result", nResult > 0);
	else
		VERIFY_COND("result", nResult == 0);
	expectedResult *= -1;
	SG_ERR_CHECK(  SG_repopath__compare(pCtx, pszPath2, pszPath1, &nResult)  );
	if (expectedResult < 0)
		VERIFY_COND("result", nResult < 0);
	else if (expectedResult > 0)
		VERIFY_COND("result", nResult > 0);
	else
		VERIFY_COND("result", nResult == 0);
fail:
	return;
}
void u0105_repopath__compare_test(SG_context * pCtx)
{
	__testComparison(pCtx, "", "", 0);
	__testComparison(pCtx, "", "blah", -1);
	__testComparison(pCtx, "box", "car", -1);
	__testComparison(pCtx, "box", "boxcar", -1);
	__testComparison(pCtx, "@", "@", 0);
	__testComparison(pCtx, "@", "@/", 0);

	__testComparison(pCtx, "@/subfolder/subitem", "@/subfolder with spaces/subitem", -1);
	__testComparison(pCtx, "@/subfolder/", "@/subfolder////", 0);

    return;
}

TEST_MAIN(u0105_repopath)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0105_repopath__compare_test(pCtx)  );

	TEMPLATE_MAIN_END;
}

