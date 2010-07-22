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

#define MyMain()				TEST_MAIN(u0079_mutex)
#define MyDcl(name)				u0079_mutex__##name
#define MyFn(name)				u0079_mutex__##name


void MyFn(test__one_call_to_each_api)(SG_context *pCtx)
{
    SG_mutex m;

    SG_UNUSED(pCtx);

    SG_mutex__init(&m);
    SG_mutex__lock(&m);
    SG_mutex__unlock(&m);
}


MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(test__one_call_to_each_api)(pCtx)  );

	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn
