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
 * @file u0059_js.c
 *
 * @details A simple test to exercise SG_exec.
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

#define MyMain()				TEST_MAIN(u0059_js)
#define MyDcl(name)				u0059_js__##name
#define MyFn(name)				u0059_js__##name

//////////////////////////////////////////////////////////////////

/* The class of the global object. */
static JSClass global_class = {
        "global", JSCLASS_GLOBAL_FLAGS,
            JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
                JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
                    JSCLASS_NO_OPTIONAL_MEMBERS
};

/* The error reporter callback. */
void reportError(SG_UNUSED_PARAM(JSContext *cx), const char *message, JSErrorReport *report)
{
    SG_UNUSED(cx);

    fprintf(stderr, "%s:%u:%s\n",
        report->filename ? report->filename : "<no filename>",
        (unsigned int) report->lineno,
        message);
}


void MyFn(test1)(void)
{
    /* JS variables. */
    JSRuntime *rt;
    JSContext *cx;
    JSObject  *global;
    /* These should indicate source location for diagnostics. */
    char *filename = NULL;
    uintN lineno = 0;
    jsval rval;
    JSBool ok = 0;
    char *source = "17 * 6";

    /* Create a JS runtime. */
    rt = JS_NewRuntime(8L * 1024L * 1024L);
    if (!rt)
    {
        goto fail;
    }

    /* Create a context. */
    cx = JS_NewContext(rt, 8192);
    if (!cx)
    {
        goto fail;
    }

    JS_SetOptions(cx, JSOPTION_VAROBJFIX);
    JS_SetVersion(cx, JSVERSION_LATEST);
    JS_SetErrorReporter(cx, reportError);

    /* Create the global object. */
    global = JS_NewObject(cx, &global_class, NULL, NULL);
    if (!global)
    {
        goto fail;
    }

    /* Populate the global object with the standard globals,
    *        like Object and Array. */
    if (!JS_InitStandardClasses(cx, global))
    {
        goto fail;
    }

    ok = JS_EvaluateScript(cx, global, source, strlen(source), filename, lineno, &rval);

    if (ok)
    {
        /* Should get a number back from the example source. */
        int32 d;

        ok = JS_ValueToInt32(cx, rval, &d);

        VERIFY_COND("ok", ok);
        VERIFY_COND("script vaqlue is correct", (102 == d));
    }

    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    JS_ShutDown();
    return;

fail:
    return;
}

//////////////////////////////////////////////////////////////////

MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(test1)()  );

	TEMPLATE_MAIN_END;
}

//////////////////////////////////////////////////////////////////

#undef MyMain
#undef MyDcl
#undef MyFn

