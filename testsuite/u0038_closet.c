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
 * @file u0038_closet.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

void u0038_test_ridesc(SG_context * pCtx)
{
	SG_vhash* pvh = NULL;
	SG_vhash* pvh2 = NULL;
	SG_vhash* pvh_all = NULL;
	SG_uint32 count = 0;

	VERIFY_ERR_CHECK_DISCARD(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, pvh, "hello", "world")  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, pvh, "hola", "mundo")  );
	SG_ERR_IGNORE(  SG_closet__descriptors__remove(pCtx, "1")  );
	VERIFY_ERR_CHECK_DISCARD(  SG_closet__descriptors__add(pCtx, "1", pvh)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_closet__descriptors__remove(pCtx, "1")  );

	SG_ERR_IGNORE(  SG_closet__descriptors__remove(pCtx, "2")  );		/* This may or may not be an error */

	/* delete one that is not there should be an error */
	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_closet__descriptors__remove(pCtx, "2")  );

	/* fetch one that is not there should be an error */
	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_closet__descriptors__get(pCtx, "2", &pvh2)  );

	SG_ERR_IGNORE(  SG_closet__descriptors__remove(pCtx, "3")  );
	SG_ERR_IGNORE(  SG_closet__descriptors__remove(pCtx, "4")  );
	VERIFY_ERR_CHECK_DISCARD(  SG_closet__descriptors__add(pCtx, "3", pvh)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_closet__descriptors__add(pCtx, "4", pvh)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_closet__descriptors__get(pCtx, "3", &pvh2)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_closet__descriptors__list(pCtx, &pvh_all)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__count(pCtx, pvh_all, &count)  );
	VERIFY_COND("count", (count >= 2));
	SG_ERR_IGNORE(  SG_closet__descriptors__remove(pCtx, "3")  );
	SG_ERR_IGNORE(  SG_closet__descriptors__remove(pCtx, "4")  );

	SG_VHASH_NULLFREE(pCtx, pvh_all);
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_VHASH_NULLFREE(pCtx, pvh2);
}

void u0038_test_wdmapping(SG_context * pCtx)
{
	SG_vhash* pvh = NULL;
	SG_pathname* pPath = NULL;
	SG_pathname* pMappedPath = NULL;
	SG_string* pstrRepoDescriptorName = NULL;
	char* pszidGid = NULL;

	VERIFY_ERR_CHECK_DISCARD(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );

	VERIFY_ERR_CHECK_DISCARD(  SG_pathname__set__from_cwd(pCtx, pPath)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, pvh, "hello", "world")  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, pvh, "hola", "mundo")  );
	SG_ERR_IGNORE(  SG_closet__descriptors__remove(pCtx, "r1")  );
	VERIFY_ERR_CHECK_DISCARD(  SG_closet__descriptors__add(pCtx, "r1", pvh)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_workingdir__set_mapping(pCtx, pPath, "r1", NULL)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_workingdir__find_mapping(pCtx, pPath, &pMappedPath, &pstrRepoDescriptorName, &pszidGid)  );
	VERIFY_COND("ridesc match", (0 == strcmp("r1", SG_string__sz(pstrRepoDescriptorName))));
	VERIFY_ERR_CHECK_DISCARD(  SG_pathname__append__from_sz(pCtx, pPath, "foo")  );
	VERIFY_ERR_CHECK_DISCARD(  SG_pathname__append__from_sz(pCtx, pPath, "bar")  );
	VERIFY_ERR_CHECK_DISCARD(  SG_pathname__append__from_sz(pCtx, pPath, "plok")  );

	SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
	SG_PATHNAME_NULLFREE(pCtx, pMappedPath);

	VERIFY_ERR_CHECK_DISCARD(  SG_workingdir__find_mapping(pCtx, pPath, &pMappedPath, &pstrRepoDescriptorName, &pszidGid)  );

	SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
	SG_PATHNAME_NULLFREE(pCtx, pMappedPath);
	SG_PATHNAME_NULLFREE(pCtx, pPath);

	SG_VHASH_NULLFREE(pCtx, pvh);
}

TEST_MAIN(u0038_closet)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0038_test_ridesc(pCtx)  );
	BEGIN_TEST(  u0038_test_wdmapping(pCtx)  );

	TEMPLATE_MAIN_END;
}
