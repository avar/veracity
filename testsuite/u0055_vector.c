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
 * @file u0055_vector.c
 *
 * @details A simple test to exercise SG_vector.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0055_vector)
#define MyDcl(name)				u0055_vector__##name
#define MyFn(name)				u0055_vector__##name

//////////////////////////////////////////////////////////////////

void MyFn(test1)(SG_context * pCtx)
{
	SG_vector * pVec = NULL;
	SG_uint32 k, ndx, len;
	void * pValue;
	SG_uint32 variable_1 = 0;
	SG_uint32 variable_2 = 0;
#define ADDR_1(k)		((void *)((&variable_1)+k))
#define ADDR_2(k)		((void *)((&variable_2)+k))

	VERIFY_ERR_CHECK(  SG_vector__alloc(pCtx,&pVec,20)  );

	VERIFY_ERR_CHECK(  SG_vector__length(pCtx,pVec,&len)  );
	VERIFY_COND("test1",(len==0));

	for (k=0; k<100; k++)
	{
		// fabricate a bogus pointer from a constant and stuff it into the vector.

		VERIFY_ERR_CHECK(  SG_vector__append(pCtx,pVec,ADDR_1(k),&ndx)  );
		VERIFY_COND("append",(ndx==k));
	}
	for (k=0; k<100; k++)
	{
		VERIFY_ERR_CHECK(  SG_vector__get(pCtx,pVec,k,&pValue)  );
		VERIFY_COND("get1",(pValue == ADDR_1(k)));
	}

	for (k=0; k<100; k++)
	{
		VERIFY_ERR_CHECK(  SG_vector__set(pCtx,pVec,k,ADDR_2(k))  );
	}
	for (k=0; k<100; k++)
	{
		VERIFY_ERR_CHECK(  SG_vector__get(pCtx,pVec,k,&pValue)  );
		VERIFY_COND("get2",(pValue == ADDR_2(k)));
	}

	VERIFY_ERR_CHECK(  SG_vector__length(pCtx,pVec,&len)  );
	VERIFY_COND("test1",(len==100));

	VERIFY_ERR_CHECK(  SG_vector__clear(pCtx,pVec)  );

	VERIFY_ERR_CHECK(  SG_vector__length(pCtx,pVec,&len)  );
	VERIFY_COND("test1",(len==0));

	// fall thru to common cleanup

fail:
	SG_VECTOR_NULLFREE(pCtx, pVec);
}

//////////////////////////////////////////////////////////////////

MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(test1)(pCtx)  );

	TEMPLATE_MAIN_END;
}

//////////////////////////////////////////////////////////////////

#undef MyMain
#undef MyDcl
#undef MyFn
