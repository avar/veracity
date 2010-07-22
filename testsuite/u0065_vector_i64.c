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
 * @file u0065_vector_i64.c
 *
 * @details A simple test to exercise SG_vector_i64.
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

#define MyMain()				TEST_MAIN(u0065_vector_i64)
#define MyDcl(name)				u0065_vector_i64__##name
#define MyFn(name)				u0065_vector_i64__##name

//////////////////////////////////////////////////////////////////

void MyFn(test1)(SG_context * pCtx)
{
	SG_vector_i64 * pVec = NULL;
	SG_uint32 k, ndx, len;

	VERIFY_ERR_CHECK(  SG_vector_i64__alloc(pCtx,&pVec,20)  );

	VERIFY_ERR_CHECK(  SG_vector_i64__length(pCtx,pVec,&len)  );
	VERIFY_COND("test1",(len==0));

	for (k=0; k<100; k++)
	{
		SG_int64 kValue = (SG_int64)k;
		VERIFY_ERR_CHECK(  SG_vector_i64__append(pCtx,pVec,kValue,&ndx)  );
		VERIFY_COND("append",(ndx==k));
	}
	for (k=0; k<100; k++)
	{
		SG_int64 value;
		VERIFY_ERR_CHECK(  SG_vector_i64__get(pCtx,pVec,k,&value)  );
		VERIFY_COND("get1",(value == ((SG_int64)k)));
	}

	for (k=0; k<100; k++)
	{
		SG_int64 kValue = (SG_int64)k+10000;
		VERIFY_ERR_CHECK(  SG_vector_i64__set(pCtx,pVec,k,kValue)  );
	}
	for (k=0; k<100; k++)
	{
		SG_int64 value;
		VERIFY_ERR_CHECK(  SG_vector_i64__get(pCtx,pVec,k,&value)  );
		VERIFY_COND("get2",(value == ((SG_int64)(k+10000))));
	}

	VERIFY_ERR_CHECK(  SG_vector_i64__length(pCtx,pVec,&len)  );
	VERIFY_COND("test1",(len==100));

	VERIFY_ERR_CHECK(  SG_vector_i64__clear(pCtx,pVec)  );

	VERIFY_ERR_CHECK(  SG_vector_i64__length(pCtx,pVec,&len)  );
	VERIFY_COND("test1",(len==0));

	// fall thru to common cleanup

fail:
	SG_VECTOR_I64_NULLFREE(pCtx, pVec);
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
