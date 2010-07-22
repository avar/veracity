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

//////////////////////////////////////////////////////////////////
// WARNING: This is a non-standard test used to test SG_context.
// WARNING: As such, it does not use the standard VERIFY* macros
// WARNING: and does not have the same sub-test form as the other
// WARNING: tests.
// WARNING:
// WARNING: DO NOT USE THIS AS A TEMPLATE FOR STARTING A NEW TEST.
//////////////////////////////////////////////////////////////////

#define OMIT_EXTRACT_DATA_DIR_FROM_ARGS_FUNCTION

#include <sg.h>
#include "unittests.h"

//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0063_error_context)
#define MyDcl(name)				u0063_error_context__##name
#define MyFn(name)				u0063_error_context__##name

//////////////////////////////////////////////////////////////////
// You can't use the common VERIFY_CTX_* or VERIFY_ERR_CHECK_* unit
// testing macros here, because they test the value contained within
// pCtx.
//
// WE ONLY WANT TO TEST THE err RETURN VALUE FROM THE SG_context ROUTINES.
// If we have an error value, we goto fail.

#define MY_VERIFY_RESULT_FAIL(expr)			SG_STATEMENT( err=(expr); if (SG_IS_OK(err)) { _incr_pass(); } else { _report_testfail_err("MY_VERIFY_RESULT_FAIL",__FILE__,__LINE__,err,#expr); goto fail; } )

//////////////////////////////////////////////////////////////////

SG_error MyFn(test__err_reset)(SG_context* pCtx)
{
	const char* pszInfo = NULL;
	SG_bool bHasInfo = SG_FALSE;

	VERIFY_COND_RETURN("Not initially in error state", SG_context__has_err(pCtx));

	SG_context__err_get_description(pCtx, &pszInfo);
	bHasInfo = (0 != pszInfo[0]);

	SG_context__err_reset(pCtx);
	VERIFY_COND("has_err reports error after reset", !SG_context__has_err(pCtx));

	if (bHasInfo)
		VERIFY_COND("info string not cleared", 0 == pszInfo[0]);

	return SG_ERR_OK;
}

SG_error MyFn(test__generic_err)(SG_context* pCtx)
{
	SG_error err;

	SG_context__err_reset(pCtx);

	MY_VERIFY_RESULT_FAIL(  SG_context__err__generic(pCtx, SG_ERR_INVALIDARG, __FILE__, __LINE__)  );
	VERIFY_COND("error not reported", SG_context__has_err(pCtx)  );

	MY_VERIFY_RESULT_FAIL(  MyFn(test__err_reset)(pCtx)  );

	return SG_ERR_OK;

fail:
	return err;
}

SG_error MyFn(test__long_err_message)(SG_context* pCtx)
{
	SG_error err;
	const char* pszInfo;

	SG_context__err_reset(pCtx);

	MY_VERIFY_RESULT_FAIL
	(
		SG_context__err(pCtx, SG_ERR_INVALIDARG, __FILE__, __LINE__,
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! ")
	);

	VERIFY_COND("error not reported", SG_context__has_err(pCtx)  );
	MY_VERIFY_RESULT_FAIL(  SG_context__err_get_description(pCtx, &pszInfo)  );

	VERIFY_COND("truncated message not 1023 bytes", 1023 == strlen(pszInfo));
	VERIFY_COND("incorrect truncated message contents", 0 == strcmp(pszInfo,
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"This is a long error message.  It needs to be longer than 1023 characters. Longer?  Longer! Longer! "
		"12345678901234567890123"));

	MY_VERIFY_RESULT_FAIL(  MyFn(test__err_reset)(pCtx)  );

	return SG_ERR_OK;

fail:
	return err;
}

SG_error MyFn(test__stack_overflow)(SG_context* pCtx)
{
	SG_error err;
	SG_string* pErrString = NULL;
	SG_bool overflow = SG_FALSE;

	SG_context__err_reset(pCtx);

	MY_VERIFY_RESULT_FAIL(  SG_context__err__generic(pCtx, SG_ERR_INVALIDARG, __FILE__, __LINE__)  );

	while (!overflow)
	{
		MY_VERIFY_RESULT_FAIL(  SG_context__err_stackframe_add(pCtx, __FILE__, __LINE__)  );
		overflow = SG_context__stack_is_full(pCtx);
	}

	MY_VERIFY_RESULT_FAIL(  SG_context__err_to_string(pCtx, &pErrString)  );

	VERIFY_COND("overflowed stack trace doesn't end with ...",
				strcmp("...\n", SG_string__sz(pErrString) + SG_string__length_in_bytes(pErrString) - 4) == 0);

	MY_VERIFY_RESULT_FAIL(  MyFn(test__err_reset)(pCtx)  );

	SG_STRING_NULLFREE(pCtx, pErrString);
	return SG_ERR_OK;

fail:
	SG_STRING_NULLFREE(pCtx, pErrString);
	return err;
}

static void _null_arg_check(SG_context* pCtx, void* pVoid)
{
	SG_context__err_reset(pCtx);

	SG_NULLARGCHECK(pVoid);

fail:
	return;
}

SG_error MyFn(test__null_arg_check)(SG_context* pCtx)
{
	SG_error err;
	SG_error ctxErr;
	const char* pszDesc;

	SG_context__err_reset(pCtx);

	_null_arg_check(pCtx, NULL);

	VERIFY_COND("no null argument error reported", SG_context__has_err(pCtx));

	MY_VERIFY_RESULT_FAIL(SG_context__get_err(pCtx, &ctxErr));
	VERIFY_COND("incorrect error reported", ctxErr == SG_ERR_INVALIDARG);

	VERIFY_COND("equals should be true", SG_context__err_equals(pCtx, ctxErr));
	VERIFY_COND("equals should be false", !SG_context__err_equals(pCtx, SG_ERR_REPO_DB_NOT_OPEN));

	VERIFY_COND("equals should be false for null pCtx", !SG_context__err_equals(pCtx, SG_ERR_REPO_DB_NOT_OPEN));

	MY_VERIFY_RESULT_FAIL(SG_context__err_get_description(pCtx, &pszDesc));
	VERIFY_COND("error description mismatch", 0 == strcmp(pszDesc, "pVoid is null"));

	MY_VERIFY_RESULT_FAIL(  MyFn(test__err_reset)(pCtx)  );

	return SG_ERR_OK;

fail:
	return err;
}

#if defined(WINDOWS)
// #pragma warning(disable: 4100) // unreferenced formal parameter
#endif

MyMain()
{
	SG_error err;
	SG_context* pCtx2 = NULL;

#ifdef GCOV_TEST
	SG_UNUSED(pCtx);
	SG_UNUSED(pDataDir);
#else
	SG_UNUSED(argc);
	SG_UNUSED(argv);
#endif

	MY_VERIFY_RESULT_FAIL(  SG_context__alloc(&pCtx2)  );

	VERIFY_COND("initial error state incorrect", !SG_context__has_err(pCtx2)  );
	MY_VERIFY_RESULT_FAIL(  SG_context__get_err(pCtx2, &err)  );
	VERIFY_COND("initial error state incorrect", SG_ERR_OK == err);

	MY_VERIFY_RESULT_FAIL(  MyFn(test__generic_err)(pCtx2)  );
	MY_VERIFY_RESULT_FAIL(  MyFn(test__long_err_message)(pCtx2)  );
	MY_VERIFY_RESULT_FAIL(  MyFn(test__stack_overflow)(pCtx2)  );
	MY_VERIFY_RESULT_FAIL(  MyFn(test__null_arg_check)(pCtx2)  );

	// Fall through to common cleanup.

fail:
	SG_CONTEXT_NULLFREE(pCtx2);

    return (_report_and_exit(__FILE__));
}

#if defined(WINDOWS)
#pragma warning(default: 4100)
#endif

#undef MyMain
#undef MyDcl
#undef MyFn
