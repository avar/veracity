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

int u0027_pool__test(SG_context* pCtx)
{
	SG_strpool* pp;
	SG_uint32 i;

	SG_STRPOOL__ALLOC(pCtx, &pp, 32768);

	for (i=0; i<100000; i++)
	{
		const char* psz = NULL;

		VERIFY_ERR_CHECK_RETURN(  SG_strpool__add__sz(pCtx, pp,
			"this string is going to get added over and over and over and over and over", &psz)  );

		VERIFY_COND("ok", psz);
	}

	SG_STRPOOL_NULLFREE(pCtx, pp);

	return 1;
}

TEST_MAIN(u0027_pool)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0027_pool__test(pCtx)  );

	TEMPLATE_MAIN_END;
}

