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
 * @file u0066_logging.c
 *
 * @details Tests sg_logging.
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

#define MyMain()				TEST_MAIN(u0066_logging)
#define MyDcl(name)				u0066_logging__##name
#define MyFn(name)				u0066_logging__##name

//////////////////////////////////////////////////////////////////

void _u0066__call_sg_vlog(SG_context * pCtx, const char * szFormat, ...)
{
	va_list ap;
	va_start(ap, szFormat);
	SG_vlog(pCtx, szFormat, ap);
	va_end(ap);
}
void _u0066__call_sg_vlog_urgent(SG_context * pCtx, const char * szFormat, ...)
{
	va_list ap;
	va_start(ap, szFormat);
	SG_vlog_urgent(pCtx, szFormat, ap);
	va_end(ap);
}
void _u0066__call_sg_vlog_debug(SG_context * pCtx, const char * szFormat, ...)
{
	va_list ap;
	va_start(ap, szFormat);
	SG_vlog_debug(pCtx, szFormat, ap);
	va_end(ap);
}

void _u0066__logger_cb(SG_UNUSED_PARAM(SG_context * pCtx), const char * message, SG_log_message_level message_level, SG_UNUSED_PARAM(SG_int64 timestamp), void * pContext)
{
	SG_uint64 * pMessageCount = pContext;

	SG_UNUSED(pCtx);
	SG_UNUSED(timestamp);

	(*pMessageCount) += 1;
	VERIFY_COND("",
		( message_level==SG_LOG_MESSAGE_LEVEL_URGENT && message[0]=='u' )
		|| ( message_level==SG_LOG_MESSAGE_LEVEL_NORMAL && message[0]=='n' )
		|| ( message_level==SG_LOG_MESSAGE_LEVEL_DEBUG && message[0]=='d' )
	);
}

void _u0066__evil_logger_cb(SG_context * pCtx, SG_UNUSED_PARAM(const char * message), SG_UNUSED_PARAM(SG_log_message_level message_level), SG_UNUSED_PARAM(SG_int64 timestamp), SG_UNUSED_PARAM(void * pContext))
{
	SG_UNUSED(message);
	SG_UNUSED(message_level);
	SG_UNUSED(timestamp);
	SG_UNUSED(pContext);

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_log(pCtx,"n") , SG_ERR_LOGGING_MODULE_IN_USE  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_log_urgent(pCtx,"u") , SG_ERR_LOGGING_MODULE_IN_USE  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_log_debug(pCtx,"d") , SG_ERR_LOGGING_MODULE_IN_USE  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_logging__unregister_logger(pCtx,_u0066__evil_logger_cb, NULL) , SG_ERR_LOGGING_MODULE_IN_USE  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_logging__register_logger(pCtx,_u0066__logger_cb, NULL, SG_LOGGER_OUTPUT_LEVEL_VERBOSE) , SG_ERR_LOGGING_MODULE_IN_USE  );

	SG_ERR_THROW_RETURN(SG_ERR_UNSPECIFIED);
}

void MyFn(test__logging)(SG_context * pCtx)
{
	SG_uint64 verbose_logger_message_count = 0;
	SG_uint64 normal_logger_message_count = 0;
	SG_uint64 quiet_logger_message_count = 0;

	VERIFY_ERR_CHECK(  SG_logging__register_logger(pCtx, _u0066__logger_cb, &verbose_logger_message_count, SG_LOGGER_OUTPUT_LEVEL_VERBOSE)  );
	VERIFY_ERR_CHECK(  SG_logging__register_logger(pCtx, _u0066__logger_cb, &normal_logger_message_count, SG_LOGGER_OUTPUT_LEVEL_NORMAL)  );
	VERIFY_ERR_CHECK(  SG_logging__register_logger(pCtx, _u0066__logger_cb, &quiet_logger_message_count, SG_LOGGER_OUTPUT_LEVEL_QUIET)  );

	VERIFY_ERR_CHECK(  SG_log(pCtx, "n")  );
	VERIFY_ERR_CHECK(  SG_log_urgent(pCtx, "u")  );
	VERIFY_ERR_CHECK(  SG_log_debug(pCtx, "d")  );

	VERIFY_COND("", verbose_logger_message_count == 3 );
	VERIFY_COND("", normal_logger_message_count == 2 );
	VERIFY_COND("", quiet_logger_message_count == 1 );

	VERIFY_ERR_CHECK(  SG_log(pCtx, "%s", "n")  );
	VERIFY_ERR_CHECK(  SG_log_urgent(pCtx, "%s", "u")  );
	VERIFY_ERR_CHECK(  SG_log_debug(pCtx, "%s", "d")  );

	VERIFY_COND("", verbose_logger_message_count == 6 );
	VERIFY_COND("", normal_logger_message_count == 4 );
	VERIFY_COND("", quiet_logger_message_count == 2 );

	VERIFY_ERR_CHECK(  _u0066__call_sg_vlog(pCtx, "%s", "n")  );
	VERIFY_ERR_CHECK(  _u0066__call_sg_vlog_urgent(pCtx, "%s", "u")  );
	VERIFY_ERR_CHECK(  _u0066__call_sg_vlog_debug(pCtx, "%s", "d")  );

	VERIFY_COND("", verbose_logger_message_count == 9 );
	VERIFY_COND("", normal_logger_message_count == 6 );
	VERIFY_COND("", quiet_logger_message_count == 3 );

	VERIFY_ERR_CHECK(  SG_logging__register_logger(pCtx, _u0066__evil_logger_cb, NULL, SG_LOGGER_OUTPUT_LEVEL_VERBOSE)  );
	VERIFY_ERR_CHECK(  SG_log_urgent(pCtx, "u")  );

	VERIFY_COND("", verbose_logger_message_count == 10 );
	VERIFY_COND("", normal_logger_message_count == 7 );
	VERIFY_COND("", quiet_logger_message_count == 4 );

	VERIFY_ERR_CHECK(  SG_logging__unregister_logger(pCtx, _u0066__evil_logger_cb, NULL)  );
	VERIFY_ERR_CHECK(  SG_logging__unregister_logger(pCtx, _u0066__logger_cb, &verbose_logger_message_count)  );
	VERIFY_ERR_CHECK(  SG_logging__unregister_logger(pCtx, _u0066__logger_cb, &normal_logger_message_count)  );
	VERIFY_ERR_CHECK(  SG_logging__unregister_logger(pCtx, _u0066__logger_cb, &quiet_logger_message_count)  );

	VERIFY_ERR_CHECK(  SG_log_urgent(pCtx, "u")  );

	VERIFY_COND("", verbose_logger_message_count == 10 );
	VERIFY_COND("", normal_logger_message_count == 7 );
	VERIFY_COND("", quiet_logger_message_count == 4 );

fail:
	;
}

void MyFn(test__misc)(void)
{
	VERIFY_COND("", SG_logging__enumerate_log_level("quiet")==SG_LOGGER_OUTPUT_LEVEL_QUIET );
	VERIFY_COND("", SG_logging__enumerate_log_level("normal")==SG_LOGGER_OUTPUT_LEVEL_NORMAL );
	VERIFY_COND("", SG_logging__enumerate_log_level("verbose")==SG_LOGGER_OUTPUT_LEVEL_VERBOSE );
	VERIFY_COND("", SG_logging__enumerate_log_level("asdf")==-1 );
}

//////////////////////////////////////////////////////////////////

MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(test__logging)(pCtx)  );
	BEGIN_TEST(  MyFn(test__misc)()  );

	TEMPLATE_MAIN_END;
}

//////////////////////////////////////////////////////////////////

#undef MyMain
#undef MyDcl
#undef MyFn
