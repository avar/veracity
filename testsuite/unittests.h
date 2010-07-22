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

// testsuite/unittests/unittests.h
// Commonly used declations for unit tests.
//////////////////////////////////////////////////////////////////

#ifndef H_UNITTESTS_H
#define H_UNITTESTS_H

//////////////////////////////////////////////////////////////////

#if defined(WINDOWS)	// disable compiler warnings for non _s versions of things like sprintf.
#pragma warning ( disable : 4996 )
#endif

//////////////////////////////////////////////////////////////////

static int g_cPass = 0;
static int g_cFail = 0;
static int g_cSumPass = 0;
static int g_cSumFail = 0;

//////////////////////////////////////////////////////////////////

const char * _my_sprintf_a(const char * szFormat, ...);
const char * _my_sprintf_b(const char * szFormat, ...);

/**
 * print a nice separator label to mark the start of a test case within a test.
 *
 * this CANNOT use a SG_context.
 */
void _begin_test_label(const char * szFile, int lineNr,
					   const char * szTestName)
{
	fprintf(stderr,
			"\n"
			"\n"
			"//////////////////////////////////////////////////////////////////////////////////\n"
			"BeginTest  %s:%d %s\n",
			szFile,lineNr,
			szTestName);
}

/**
 * print an info message.
 *
 * this CANNOT use a SG_context.
 */
void _report_info(const char * szTestName,
				  const char * szFile, int lineNr,
				  const char * szInfo)
{
	fprintf(stderr,
			"Info       %s:%d [%s][%s]\n",
			szFile,lineNr,
			szTestName,
			szInfo);
}

/**
 * print "TestFailed..." message.
 *
 * this CANNOT use a SG_context.
 */
int _report_testfail(const char * szTestName,
					 const char * szFile, int lineNr,
					 const char * szReason)
{
	fprintf(stderr,
			"TestFailed %s:%d [%s][%s]\n",
			szFile,lineNr,
			szTestName,
			szReason);
	g_cFail++;
	g_cSumFail++;

	return 0;
}

/**
 * print "TestFailed..." message.
 *
 * this CANNOT use a SG_context.
 */
int _report_testfail2(const char * szTestName,
					  const char * szFile, int lineNr,
					  const char * szReason,
					  const char * szReason2)
{
	fprintf(stderr,
			"TestFailed %s:%d [%s][%s][%s]\n",
			szFile,lineNr,
			szTestName,
			szReason,
			szReason2);
	g_cFail++;
	g_cSumFail++;

	return 0;
}

/**
 * print "TestFailed..." message.
 *
 * this CANNOT use a SG_context.
 */
int _report_testfail3(const char * szTestName,
					  const char * szFile, int lineNr,
					  const char * szReason,
					  const char * szReason2,
					  const char * szReason3)
{
	fprintf(stderr,
			"TestFailed %s:%d [%s][%s][%s][%s]\n",
			szFile,lineNr,
			szTestName,
			szReason,
			szReason2,
			szReason3);
	g_cFail++;
	g_cSumFail++;

	return 0;
}

//////////////////////////////////////////////////////////////////

/**
 * print "TestFailed..." message when we also have an error number.
 *
 * this CANNOT use a SG_context.
 */
int _report_testfail_err(const char * szTestName,
						 const char * szFile, int lineNr,
						 SG_error err,
						 const char * szReason)
{
	char bufError[SG_ERROR_BUFFER_SIZE+1];

	SG_error__get_message(err,bufError,SG_ERROR_BUFFER_SIZE);

	fprintf(stderr,
			"TestFailed %s:%d [%s][%s] [error: %s]\n",
			szFile,lineNr,
			szTestName,
			szReason,
			bufError);
	g_cFail++;
	g_cSumFail++;

	return 0;
}

/**
 * print "TestFailed..." message when we also have an error number.
 *
 * this CANNOT use a SG_context.
 */
int _report_testfail2_err(const char * szTestName,
						  const char * szFile, int lineNr,
						  SG_error err,
						  const char * szReason,
						  const char * szReason2)
{
	char bufError[SG_ERROR_BUFFER_SIZE+1];

	SG_error__get_message(err,bufError,SG_ERROR_BUFFER_SIZE);

	fprintf(stderr,
			"TestFailed %s:%d [%s][%s][%s] [error: %s]\n",
			szFile,lineNr,
			szTestName,
			szReason,
			szReason2,
			bufError);
	g_cFail++;
	g_cSumFail++;

	return 0;
}

//////////////////////////////////////////////////////////////////

/**
 * print "TestFailed..." message and detailed info from pCtx.
 */
int _report_testfail_ctx(SG_context * pCtx,
						 const char * szTestName,
						 const char * szFile, int lineNr,
						 const char * szReason)
{
	fprintf(stderr,
			"TestFailed %s:%d [%s][%s]\n",
			szFile,lineNr,
			szTestName,
			szReason);

	// we assume that this never changes pCtx.
	(void) SG_context__err_to_console(pCtx, SG_CS_STDERR);

	g_cFail++;
	g_cSumFail++;

	return 0;
}

/**
 * print "TestFailed..." message and detailed info from pCtx.
 */
int _report_testfail2_ctx(SG_context * pCtx,
						  const char * szTestName,
						  const char * szFile, int lineNr,
						  const char * szReason,
						  const char * szReason2)
{
	fprintf(stderr,
			"TestFailed %s:%d [%s][%s][%s]\n",
			szFile,lineNr,
			szTestName,
			szReason,
			szReason2);

	// we assume that this never changes pCtx.
	(void) SG_context__err_to_console(pCtx, SG_CS_STDERR);

	g_cFail++;
	g_cSumFail++;

	return 0;
}

//////////////////////////////////////////////////////////////////

void _incr_pass(void)
{
	g_cPass++;
	g_cSumPass++;
}

/**
 * Verify that pCtx contains SG_ERR_OK.
 * If not, dump the current error information.
 *
 * Use this when you expect to have an OK result.
 */
int _verify_ctx_is_ok(const char * szName, SG_context * pCtx, const char * szFile, int lineNr, const char * szReason2)
{
	if (!SG_context__has_err(pCtx))
	{
		_incr_pass();
		return 1;
	}
	else
	{
		SG_context__err_stackframe_add(pCtx,szFile,lineNr);
		if (szReason2 && *szReason2)
			_report_testfail2_ctx(pCtx,szName,szFile,lineNr,"VERIFY_CTX_IS_OK",szReason2);
		else
			_report_testfail_ctx(pCtx,szName,szFile,lineNr,"VERIFY_CTX_IS_OK");

		return 0;
	}
}

/**
 * Verify that pCtx contains some (any) error.
 * If not, we do not dump the pCtx because it doesn't contain anything.
 */
int _verify_ctx_has_err(const char * szName, SG_context * pCtx, const char * szFile, int lineNr, const char * szReason2)
{
	if (SG_context__has_err(pCtx))
	{
		_incr_pass();
		return 1;
	}
	else
	{
		SG_context__err_stackframe_add(pCtx,szFile,lineNr);
		if (szReason2 && *szReason2)
			_report_testfail2(szName,szFile,lineNr,"VERIFY_CTX_HAS_ERR expected an error but was OK",szReason2);
		else
			_report_testfail(szName,szFile,lineNr,"VERIFY_CTX_HAS_ERR expected an error but was OK");

		return 0;
	}
}

/**
 * Verify that pCtx contains *exactly* the given error value.
 * If now, we print the expected value and the observed value
 * (and if the observed value is an error, any error info we have).
 */
int _verify_ctx_err_equals(const char * szName,
						   SG_context * pCtx, SG_error errExpected,
						   const char * szFile, int lineNr,
						   const char * szReason2)
{
	SG_error errObserved;

	SG_context__get_err(pCtx,&errObserved);
	if (errObserved == errExpected)
	{
		_incr_pass();
		return 1;
	}
	else
	{
		char bufError_expected[SG_ERROR_BUFFER_SIZE+1];
		char bufError_observed[SG_ERROR_BUFFER_SIZE+1];
		const char * szReason1;

		SG_error__get_message(errExpected,bufError_expected,SG_ERROR_BUFFER_SIZE);
		if (SG_IS_ERROR(errObserved))
			SG_error__get_message(errObserved,bufError_observed,SG_ERROR_BUFFER_SIZE);
		else
			SG_ERR_IGNORE(  SG_strcpy(pCtx, bufError_observed, SG_ERROR_BUFFER_SIZE, "No Error")  );

		szReason1 = _my_sprintf_b("VERIFY_CTX_ERR_EQUALS [expected: %s][observed: %s]",
								  bufError_expected,
								  bufError_observed);

		if (SG_IS_ERROR(errObserved))
			SG_context__err_stackframe_add(pCtx,szFile,lineNr);
		else
			SG_context__err__generic(pCtx, SG_ERR_UNSPECIFIED, szFile, lineNr);

		if (szReason2 && *szReason2)
			_report_testfail2_ctx(pCtx,szName,szFile,lineNr,szReason1,szReason2);
		else
			_report_testfail_ctx(pCtx,szName,szFile,lineNr,szReason1);

		return 0;
	}
}

//////////////////////////////////////////////////////////////////

#define MAX_MESSAGE		20480

/**
 * inline version of sprintf that can be used inside VERIFY* macros.
 *
 * YOU CANNOT USE THIS TWICE INSIDE ONE MACRO (because of the static buffer).
 *
 * this CANNOT use a SG_context.
 */
const char * _my_sprintf_a(const char * szFormat, ...)
{
	// not thread-safe

	static char buf[MAX_MESSAGE];
	va_list ap;

	va_start(ap,szFormat);

	memset(buf,0,MAX_MESSAGE);

#if defined(WINDOWS)
	vsnprintf_s(buf,MAX_MESSAGE-1,_TRUNCATE,szFormat,ap);
#else
	vsnprintf(buf,MAX_MESSAGE-1,szFormat,ap);
#endif

	va_end(ap);

	return buf;
}

/**
 * inline version of sprintf that can be used inside VERIFY* macros.
 *
 * YOU CANNOT USE THIS TWICE INSIDE ONE MACRO (because of the static buffer).
 *
 * this CANNOT use a SG_context.
 */
const char * _my_sprintf_b(const char * szFormat, ...)
{
	// not thread-safe

	static char buf[MAX_MESSAGE];
	va_list ap;

	va_start(ap,szFormat);

	memset(buf,0,MAX_MESSAGE);

#if defined(WINDOWS)
	vsnprintf_s(buf,MAX_MESSAGE-1,_TRUNCATE,szFormat,ap);
#else
	vsnprintf(buf,MAX_MESSAGE-1,szFormat,ap);
#endif

	va_end(ap);

	return buf;
}

/**
 * write results to STDOUT and STDERR and prepare for EXIT.
 *
 * this CANNOT use a SG_context.
 */
int _report_and_exit(const char * szProgramName)
{
	// we write results to both output streams because
	// the makefile diverts one to a file and one to the
	// console.
	// TODO this ends up with two copies of everything in the cmake logfile

	int bFailed = (g_cSumFail > 0);			// true if there were *any* errors
	int len = (int)strlen(szProgramName);
	int i;

	fprintf(stdout, "%s", szProgramName);
	fprintf(stderr, "%s", szProgramName);
	for (i=0; i<(40-len); i++)
	{
		fputc(' ', stdout);
		fputc(' ', stderr);
	}
	fprintf(stdout, "%6d passed", g_cSumPass);
	fprintf(stderr, "%6d passed", g_cSumPass);
	if (g_cSumFail > 0)
	{
		fprintf(stdout, "     %6d FAIL", g_cSumFail);	// overall error count (whole program)
		fprintf(stderr, "     %6d FAIL", g_cSumFail);
	}
	fputc('\n', stdout);
	fputc('\n', stderr);

	g_cPass = 0;
	g_cFail = 0;

	// we do not reset g_cSumPass -- it is intented to be a cummulative success count.
	// we do not reset g_cSumFail -- it is intented to be a cummulative error count.

#ifdef DEBUG
	if (SG_mem__check_for_leaks())
	{
		bFailed = 1;
	}
#endif

	return bFailed;
}

/**
 * Write results for 1 of u1000_repo_script's scripts to STDOUT and STDERR.
 * We also include info for any error recorded in pCtx.
 *
 * We DO NOT throw/report any errors, so pCtx is READONLY.
 */
void _report_and_exit_script(const char * szProgramName, const char * szScriptName, const SG_context * pCtx)
{
	// we write results to both output streams because
	// the makefile diverts one to a file and one to the
	// console.
	// TODO this ends up with two copies of everything in the cmake logfile
	//
	// here we write the error count for a single script.

	int len = (int)strlen(szProgramName) + 1 + (int)strlen(szScriptName);
	int i;

	fprintf(stdout, "%s:%s", szProgramName,szScriptName);
	fprintf(stderr, "%s:%s", szProgramName,szScriptName);
	for (i=0; i<(50-len); i++)
	{
		fputc(' ', stdout);
		fputc(' ', stderr);
	}
	fprintf(stdout, "%6d passed", g_cPass);
	fprintf(stderr, "%6d passed", g_cPass);
	if (g_cFail > 0)
	{
		fprintf(stdout, "     %6d FAIL", g_cFail);		// error count for this script
		fprintf(stderr, "     %6d FAIL", g_cFail);
	}
	if (SG_context__has_err(pCtx))
	{
		SG_error err;
		char bufError[SG_ERROR_BUFFER_SIZE+1];

		SG_context__get_err(pCtx,&err);
		SG_error__get_message(err,bufError,SG_ERROR_BUFFER_SIZE);
		fprintf(stdout, " %s",bufError);
		fprintf(stderr, " %s",bufError);
	}
	fputc('\n', stdout);
	fputc('\n', stderr);

	g_cPass = 0;
	g_cFail = 0;

	// we do not reset g_cSumPass -- it is intented to be a cummulative success count.
	// we do not reset g_cSumFail -- it is intented to be a cummulative error count.

	// TODO do we want to check for leaks here or not?
	// TODO Jeff: no because the script driver may have memory allocated.
}

//////////////////////////////////////////////////////////////////

#ifndef SG_STATEMENT
#define SG_STATEMENT( s )			do { s } while (0)
#endif

//////////////////////////////////////////////////////////////////
// INFO() -- output generic info into report that's unassociated
// with a pass/fail.

#define INFO2(name,info)				SG_STATEMENT( _report_info((name),__FILE__,__LINE__, (info)); )
#define INFOP(name,va)					SG_STATEMENT( _report_info((name),__FILE__,__LINE__, _my_sprintf_a va); )

//////////////////////////////////////////////////////////////////

// VERIFY_CTX_{OK,ERR,ERR_IS}() verify that pCtx is OK or ERROR or a specific ERROR.
// VERIFYP_CTX_{OK,ERR,ERR_IS}() do the same, but also allow a custom sprintf-message to be recorded.
//
// The _OK version is used when we think the test should succeed; we print the info from the pCtx.
// The _ERR and _ERR_IS versions only print the message
// Bad results write the "TestFailed..." messages seen in stderr.
// Execution continues afterward.
//
// You probably want to do a SG_context__reset(pCtx) after the {ERR,ERR_IS} forms.

#define VERIFY_CTX_IS_OK(name, pCtx)                           _verify_ctx_is_ok((name),(pCtx),__FILE__,__LINE__,NULL)
#define VERIFY_CTX_HAS_ERR(name, pCtx)                         _verify_ctx_has_err((name),(pCtx),__FILE__,__LINE__,NULL)
#define VERIFY_CTX_ERR_EQUALS(name, pCtx, errExpected)         _verify_ctx_err_equals((name),(pCtx),(errExpected),__FILE__,__LINE__,NULL)

#define VERIFYP_CTX_IS_OK(name, pCtx, va)                      _verify_ctx_is_ok((name),(pCtx),__FILE__,__LINE__, _my_sprintf_a va)
#define VERIFYP_CTX_HAS_ERR(name, pCtx, va)                    _verify_ctx_has_err((name),(pCtx),__FILE__,__LINE__, _my_sprintf_a va)
#define VERIFYP_CTX_ERR_EQUALS(name, pCtx, errExpected, va)    _verify_ctx_err_equals((name),(pCtx),(errExpected),__FILE__,__LINE__, _my_sprintf_a va)


// VERIFY_ERR_CHECK*() -- like SG_ERR_CHECK() and SG_ERR_CHECK_RETURN() but with logging.
// _CHECK    logs and jumps to FAIL
// _DISCARD  logs and RESETS the ctx and allows the code to CONTINUE
// _RETURN   logs and RETURNS

#define VERIFY_ERR_CHECK(expr)			SG_STATEMENT( SG_ASSERT(!SG_context__has_err(pCtx)); expr; if (0 == _verify_ctx_is_ok("VERIFY_ERR_CHECK",        pCtx,__FILE__,__LINE__,#expr)) { goto fail;                   } )
#define VERIFY_ERR_CHECK_DISCARD(expr)  SG_STATEMENT( SG_ASSERT(!SG_context__has_err(pCtx)); expr; if (0 == _verify_ctx_is_ok("VERIFY_ERR_CHECK_DISCARD",pCtx,__FILE__,__LINE__,#expr)) { SG_context__err_reset(pCtx); } )
#define VERIFY_ERR_CHECK_RETURN(expr)	SG_STATEMENT( SG_ASSERT(!SG_context__has_err(pCtx)); expr; if (0 == _verify_ctx_is_ok("VERIFY_ERR_CHECK_RETURN", pCtx,__FILE__,__LINE__,#expr)) { return 0;                    } )

// assert that pCtx has an error and reset the context.
#define VERIFY_ERR_CHECK_HAS_ERR_DISCARD(expr)	SG_STATEMENT( SG_ASSERT(!SG_context__has_err(pCtx)); expr; _verify_ctx_has_err("VERIFY_ERR_CHECK_HAS_ERR_DISCARD",pCtx,__FILE__,__LINE__,#expr); SG_context__err_reset(pCtx); )

// assert that pCtx has the named error and reset the context.
#define VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(expr,errExpected)	SG_STATEMENT( SG_ASSERT(!SG_context__has_err(pCtx)); expr; _verify_ctx_err_equals("VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD",pCtx,(errExpected),__FILE__,__LINE__,#expr); SG_context__err_reset(pCtx); )

// VERIFY*_COND()        -- verify the condition, log the error and continue
// VERIFY*_COND_RETURN() -- verify the condition, log the error and RETURN if it's false.

#define VERIFY_COND(name,cond)                  SG_STATEMENT( if ((cond)) { g_cPass++; g_cSumPass++; } else        _report_testfail ((name),__FILE__,__LINE__, #cond); )
#define VERIFY_COND_FAIL(name,cond)             SG_STATEMENT( if ((cond)) { g_cPass++; g_cSumPass++; } else {      _report_testfail ((name),__FILE__,__LINE__, #cond); goto fail; } )
#define VERIFY_COND_RETURN(name,cond)			SG_STATEMENT( if ((cond)) { g_cPass++; g_cSumPass++; } else return _report_testfail ((name),__FILE__,__LINE__, #cond); )

#define VERIFYP_COND(name,cond,va)              SG_STATEMENT( if ((cond)) { g_cPass++; g_cSumPass++; } else        _report_testfail2((name),__FILE__,__LINE__, #cond, _my_sprintf_a va); )
#define VERIFYP_COND_FAIL(name,cond,va)         SG_STATEMENT( if ((cond)) { g_cPass++; g_cSumPass++; } else {      _report_testfail2((name),__FILE__,__LINE__, #cond, _my_sprintf_a va); goto fail; } )
#define VERIFYP_COND_RETURN(name,cond,va)       SG_STATEMENT( if ((cond)) { g_cPass++; g_cSumPass++; } else return _report_testfail2((name),__FILE__,__LINE__, #cond, _my_sprintf_a va); )


// If you don't use this function, #define the following before including this header
// (To surpress gcc warning "warning: ‘_sg_unittests__extract_data_dir_from_args’ defined but not used")
#ifndef OMIT_EXTRACT_DATA_DIR_FROM_ARGS_FUNCTION

#if defined(WINDOWS)
static void _sg_unittests__extract_data_dir_from_args(SG_context * pCtx, int argc, wchar_t ** argv, SG_pathname ** ppDataDir)
#else
static void _sg_unittests__extract_data_dir_from_args(SG_context * pCtx, int argc, char ** argv, SG_pathname ** ppDataDir)
#endif
{
    SG_getopt * pGetopt = NULL;
    SG_pathname * pDataDir = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_ARGCHECK_RETURN(argc==2, argc);
    SG_NULLARGCHECK_RETURN(argv);
    SG_NULLARGCHECK_RETURN(ppDataDir);

    SG_ERR_CHECK(  SG_getopt__alloc(pCtx, argc, argv, &pGetopt)  );
    SG_ARGCHECK(pGetopt->count_args==2, argv);
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pDataDir, pGetopt->paszArgs[1])  );
    SG_GETOPT_NULLFREE(pCtx, pGetopt);

    *ppDataDir = pDataDir;

    return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pDataDir);
    SG_GETOPT_NULLFREE(pCtx, pGetopt);
}
#endif


//////////////////////////////////////////////////////////////////
// BEGIN_TEST -- log that we are starting the test and then run it.

#define BEGIN_TEST(s)			SG_STATEMENT(  _begin_test_label(__FILE__,__LINE__, #s);  SG_context__err_reset(pCtx); s; VERIFY_COND("test failed with a dirty context", !SG_context__has_err(pCtx)); SG_context__err_reset(pCtx);  )

//////////////////////////////////////////////////////////////////
// TEST_MAIN() -- define main() to allow us to do some tricks --
// run test as standalone test or as part of a group (for code
// coverage purposes).

#ifdef GCOV_TEST
#define TEST_MAIN(fn) int fn(SG_context * pCtx, const SG_pathname * pDataDir)
#else
#if defined(WINDOWS)
#define TEST_MAIN(fn) int wmain(int argc, wchar_t ** argv)
#else
#define TEST_MAIN(fn) int main(int argc, char ** argv)
#endif
#endif

//////////////////////////////////////////////////////////////////
// TEST_MAIN_WITH_ARGS() -- define main() to allow us to do some tricks --
// run test as standalone test or as part of a group (for code
// coverage purposes).

#ifdef GCOV_TEST
#define TEST_MAIN_WITH_ARGS(fn)		int fn(int argc, char**argv)
#else
#if defined(WINDOWS)
#define TEST_MAIN_WITH_ARGS(fn) int wmain(int argc, wchar_t ** argv)
#else
#define TEST_MAIN_WITH_ARGS(fn) int main(int argc, char ** argv)
#endif
#endif

//////////////////////////////////////////////////////////////////
// The following templates should be at the top and bottom of the
// body of the TEST_MAIN() function in each unit test (possibly with
// the exception of u0063_error_context (which has special stuff to
// test the SG_context code)).

#ifdef GCOV_TEST
#define TEMPLATE_MAIN_START SG_UNUSED(pDataDir); // Not necessarily unused...
#else
#define TEMPLATE_MAIN_START									\
	SG_context * pCtx = NULL;								\
	SG_pathname * pDataDir = NULL;							\
	SG_vhash* pSettings = NULL;								\
	SG_context__alloc(&pCtx);								\
	VERIFY_ERR_CHECK_RETURN(  SG_lib__global_initialize(pCtx)  );	\
	VERIFY_ERR_CHECK_RETURN(  SG_localsettings__list__vhash(pCtx, &pSettings)  );	\
	SG_ERR_IGNORE(  _sg_unittests__extract_data_dir_from_args(pCtx, argc, argv, &pDataDir)  );
#endif

#ifdef GCOV_TEST
#define TEMPLATE_MAIN_END return (_report_and_exit(__FILE__));
#else
#define TEMPLATE_MAIN_END						\
	SG_localsettings__import(pCtx, pSettings);	\
	SG_VHASH_NULLFREE(pCtx, pSettings);			\
	SG_PATHNAME_NULLFREE(pCtx, pDataDir);		\
	SG_lib__global_cleanup(pCtx);				\
	SG_CONTEXT_NULLFREE(pCtx);					\
	return (_report_and_exit(__FILE__));
#endif

//////////////////////////////////////////////////////////////////

int unittest__alloc_unique_pathname_in_cwd(SG_context * pCtx,
										   SG_pathname ** ppPathname)
{
	// allocate a pathname containing a non-existent string.
	// caller can use the result to generate a temp file or directory.
	//
	// caller must free returned pathname.

	char buf[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname * p = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx,&p)  );
	VERIFY_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx,p)  );

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx,buf,sizeof(buf), 32)  );

	VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,p,buf)  );

	*ppPathname = p;
	return 1;

fail:
	SG_PATHNAME_NULLFREE(pCtx, p);
	return 0;
}

int unittest__alloc_unique_pathname(SG_context * pCtx,
									const char * utf8Path,
									SG_pathname ** ppPathname)
{
	// allocate a pathname containing a non-existent string.
	// caller can use the result to generate a temp file or directory.
	//
	// we assume, but do not verify, that the given path refers to something
	// that resembles a directory.  we do not verify that it exists on disk
	// either.
	//
	// caller must free returned pathname.

	char bufTid[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname * p = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx,&p)  );
	VERIFY_ERR_CHECK(  SG_pathname__set__from_sz(pCtx,p,utf8Path)  );

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx,bufTid,sizeof(bufTid), 32)  );

	VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,p,bufTid)  );

	*ppPathname = p;
	return 1;

fail:
	SG_PATHNAME_NULLFREE(pCtx, p);
	return 0;
}

int unittest__alloc_unique_pathname_in_dir(SG_context * pCtx,
										   const char * szParentDirectory,
										   SG_pathname ** ppPathnameUnique)
{
	// allocate a pathname containing a non-existent string.
	// this is something of the form <parent_directory>/<tid>
	//
	// caller can use the result to generate a temp file or directory.
	//
	// caller must free returned pathname.

	SG_pathname * p = NULL;
	char bufTid[SG_TID_MAX_BUFFER_LENGTH];

	VERIFY_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__SZ(pCtx,&p,szParentDirectory)  );

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx,bufTid,sizeof(bufTid), 32)  );

	VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,p,bufTid)  );

	*ppPathnameUnique = p;
	return 1;

fail:
	SG_PATHNAME_NULLFREE(pCtx, p);
	return 0;
}

void unittest__get_nonexistent_pathname(SG_context * pCtx, SG_pathname ** ppPathname)
{
	// TODO we now have 4 routines abstracted from the various u00xx tests
	// TODO that all do pretty much the same thing.  (and a bunch of ones
	// TODO in the individual tests that were cloned when a new test was
	// TODO created.)
	// TODO
	// TODO at some point, we should consolidate these....

	unittest__alloc_unique_pathname_in_cwd(pCtx,ppPathname);
}

//////////////////////////////////////////////////////////////////

/**
 * Are we ignoring our backup files?  That is, is there a value in
 * "ignores" in "localsettings" that would cause add/remove/status
 * commands to ignore (not report) our "~sg00~" suffixed files?
 */
void unittest__get__ignore_backup_files(SG_context * pCtx, const SG_pathname * pPathWorkingDir, SG_bool * pbIgnore)
{
    SG_varray* pva = NULL;
	SG_pathname * pPathOriginalCWD = NULL;
	SG_uint32 k, nr;
	const char * psz_k;
	SG_bool bFoundMatch = SG_FALSE;

	// cd into the WD and set a drawer-local localsetting.
	// we don't want to mess up our global settings if the
	// test crashes.

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathOriginalCWD)  );
	VERIFY_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathOriginalCWD)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  SG_localsettings__get__varray(pCtx, "ignores", NULL, &pva, NULL)  );
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathOriginalCWD)  );

	if (pva)
	{
		VERIFY_ERR_CHECK(  SG_varray__count(pCtx, pva, &nr)  );
		for (k=0; k<nr; k++)
		{
			VERIFY_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, k, &psz_k)  );

			// TODO later put in whatever match function is required to guarantee
			// TODO that we properly compute the result.  For now, I know that the
			// TODO default settings have "~" which implicitly matches.

			if (strcmp(psz_k,"~") == 0)
			{
				bFoundMatch = SG_TRUE;
				break;
			}
		}
	}

	*pbIgnore = bFoundMatch;

fail:
	if (pPathOriginalCWD)
		SG_ERR_IGNORE(  SG_fsobj__cd__pathname(pCtx, pPathOriginalCWD)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathOriginalCWD);
	SG_VARRAY_NULLFREE(pCtx, pva);
}

/**
 * Turn on/off the ignoring of our "~sg00~" backup files in add/remove/status.
 */
void unittests__set__ignore_backup_files(SG_context * pCtx, const SG_pathname * pPathWorkingDir, SG_bool bIgnore)
{
	SG_bool bCurrent;

	// do a RESET and then do "ignores+=~" or "ignores-=~"

    // TODO put these in a special /testing path
	VERIFY_ERR_CHECK(  SG_localsettings__reset(pCtx, "ignores")  );
	if (bIgnore)
    {
		SG_ERR_CHECK_RETURN(  SG_localsettings__varray__append(pCtx, "ignores", "~")  );
    }
	else
    {
		SG_ERR_CHECK_RETURN(  SG_localsettings__varray__remove_first_match(pCtx, "ignores", "~")  );
    }

	SG_ERR_CHECK_RETURN(  unittest__get__ignore_backup_files(pCtx, pPathWorkingDir, &bCurrent)  );
	SG_ASSERT(  (bIgnore == bCurrent)  );

fail:
    ;
}

//////////////////////////////////////////////////////////////////

/**
 * A thin crunchy wrapper around SG_pendingtree__commit() to introduce
 * a little delay after the commit so that the timestamps on subsequently
 * modified items will differ.  This is necessary on filesystems that
 * don't have sub-second mtime resolution so that the timestamp cache
 * doesn't get fooled.
 *
 * 2010/06/06 Update. This isn't really necessary with the last round
 *            of timestamp cache fixes, but I'll leave this here because
 *            it might still be useful for testing later.
 */
void unittests_pendingtree__commit(SG_context* pCtx,
								   SG_pendingtree* pPendingTree,
								   const SG_audit* pq,
								   const SG_pathname* pPathRelativeTo,
								   SG_uint32 count_items, const char** paszItems,
								   const char* const* paszIncludes, SG_uint32 count_includes,
								   const char* const* paszExcludes, SG_uint32 count_excludes,
								   const char* const* paszAssocs, SG_uint32 count_assocs,
								   SG_dagnode** ppdn)
{
	// TODO 2010/06/06 For now, I'm going to just say that all C tests assume that
	// TODO            commits are fully recursive.  Fix this later.

	SG_bool bRecursive = SG_TRUE;

	SG_ERR_CHECK_RETURN(  SG_pendingtree__commit(pCtx, pPendingTree, pq, pPathRelativeTo,
												 count_items, paszItems,
												 bRecursive,
												 paszIncludes, count_includes,
												 paszExcludes, count_excludes,
												 paszAssocs, count_assocs,
												 ppdn)  );

	// 2010/05/29 With the {mtime,clock} change, I'm hoping that we don't need this anymore.
	// sleep(1);
}

/**
 * Another wrapper for the same reason.
 */
void unittests_workingdir__create_and_get(
	SG_context* pCtx,
	const char* pszDescriptorName,
	const SG_pathname* pPathDirPutTopLevelDirInHere,
	SG_bool bCreateDrawer,
    const char* psz_spec_hid_cs_baseline
	)
{
	SG_ERR_CHECK_RETURN(  SG_workingdir__create_and_get(pCtx,
														pszDescriptorName,
														pPathDirPutTopLevelDirInHere,
														bCreateDrawer,
														psz_spec_hid_cs_baseline)  );

	// 2010/05/29 With the {mtime,clock} change, I'm hoping that we don't need this anymore.
	// sleep(1);
}

//////////////////////////////////////////////////////////////////

#endif//H_UNITTESTS_H
