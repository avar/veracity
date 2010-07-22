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
 * @file sg_logging.c
 */

#include <sg.h>

//////////////////////////////////////////////////////////////////

typedef struct
{
	SG_logger_callback * pCallback;
	void * pContext;
} _sg_logger;

#define MAX_NUM_LOGGERS 16
typedef struct
{
	unsigned char count;
	_sg_logger a[MAX_NUM_LOGGERS];
} _sg_logger_list;
#define LOGGER_LIST_0 {0,{{NULL,NULL},{NULL,NULL},{NULL,NULL},{NULL,NULL},{NULL,NULL},{NULL,NULL},{NULL,NULL},{NULL,NULL}}}

static struct
{
	SG_bool allowNewMessages;
	_sg_logger_list loggingDebugMessages;
	_sg_logger_list loggingNormalMessages; // Redundantly includes previous list
	_sg_logger_list loggingUrgentMessages; // Redundantly includes previous list
} _sg_logging__instance = {SG_TRUE, LOGGER_LIST_0, LOGGER_LIST_0, LOGGER_LIST_0};

#define mAllowNewMessages _sg_logging__instance.allowNewMessages
#define mpLoggingDebugMessages &_sg_logging__instance.loggingDebugMessages
#define mpLoggingNormalMessages &_sg_logging__instance.loggingNormalMessages
#define mpLoggingUrgentMessages &_sg_logging__instance.loggingUrgentMessages

static SG_bool _sg_logging__add(_sg_logger_list * pList, SG_logger_callback pCallback, void * pContext)
{
	if( pList->count == MAX_NUM_LOGGERS )
		return SG_FALSE;
	pList->a[pList->count].pCallback = pCallback;
	pList->a[pList->count].pContext = pContext;
	++pList->count;
	return SG_TRUE;
}
static void _sg_logging__rm(_sg_logger_list * pList, SG_logger_callback pCallback, void * pContext)
{
	int i = 0;
	while( i<pList->count)
	{
		if( pList->a[i].pCallback==pCallback && pList->a[i].pContext==pContext )
		{
			--pList->count;
			pList->a[i] = pList->a[pList->count];
			return;
		}
		++i;
	}
}
void SG_logging__register_logger(SG_context * pCtx, SG_logger_callback pCallback, void * pContext, SG_logger_output_level level)
{
	if( !mAllowNewMessages )
		SG_ERR_THROW2_RETURN( SG_ERR_LOGGING_MODULE_IN_USE , (pCtx, "Can't register a new logger while handling a log message.") );

	if( !_sg_logging__add(mpLoggingUrgentMessages, pCallback, pContext) )
		SG_ERR_THROW2_RETURN( SG_ERR_ALREADY_INITIALIZED , (pCtx, "The max number of loggers (%d) has already been initialized.", MAX_NUM_LOGGERS) );

	if( level >= SG_LOGGER_OUTPUT_LEVEL_NORMAL )
		_sg_logging__add(mpLoggingNormalMessages, pCallback, pContext);
	else
		_sg_logging__rm(mpLoggingNormalMessages, pCallback, pContext);

	if( level == SG_LOGGER_OUTPUT_LEVEL_VERBOSE )
		_sg_logging__add(mpLoggingDebugMessages, pCallback, pContext);
	else
		_sg_logging__rm(mpLoggingDebugMessages, pCallback, pContext);
}
void SG_logging__unregister_logger(SG_context * pCtx, SG_logger_callback pCallback, void * pContext)
{
	if( !mAllowNewMessages )
		SG_ERR_THROW2_RETURN( SG_ERR_LOGGING_MODULE_IN_USE , (pCtx, "Can't unregister a logger while handling a log message.") );

	_sg_logging__rm(mpLoggingUrgentMessages, pCallback, pContext);
	_sg_logging__rm(mpLoggingNormalMessages, pCallback, pContext);
	_sg_logging__rm(mpLoggingDebugMessages, pCallback, pContext);
}

SG_enumeration SG_logging__enumerate_log_level(const char * szValue)
{
	if( SG_stricmp(szValue, "quiet")==0 )
		return SG_LOGGER_OUTPUT_LEVEL_QUIET;
	if( SG_stricmp(szValue, "normal")==0 )
		return SG_LOGGER_OUTPUT_LEVEL_NORMAL;
	if( SG_stricmp(szValue, "verbose")==0 )
		return SG_LOGGER_OUTPUT_LEVEL_VERBOSE;
	return -1;
}

void _sg_log(SG_context * pCtx, _sg_logger_list * pLoggers, SG_log_message_level level, const char * szFormat, va_list ap)
{
	char sz[1024];
	SG_int64 timestamp = 0;
	int i = 0;

	if( !mAllowNewMessages )
		SG_ERR_THROW2_RETURN( SG_ERR_LOGGING_MODULE_IN_USE , (pCtx, "Can't log another message while handling a log message.") );

	if( pLoggers->count==0 )
		return;

	SG_ERR_CHECK_RETURN(  SG_vsprintf_truncate(pCtx, sz, 1024, szFormat, ap)  );
	SG_ERR_CHECK_RETURN(  SG_time__get_milliseconds_since_1970_utc(pCtx, &timestamp)  );

	mAllowNewMessages = SG_FALSE;
	for(; i<pLoggers->count; ++i)
	{
		pLoggers->a[i].pCallback(pCtx, sz, level, timestamp, pLoggers->a[i].pContext);
		SG_context__err_reset(pCtx); // Ignore errors returned by the callback function. We really only pass in a pCtx so they don't have to alloc one themself.
	}
	mAllowNewMessages = SG_TRUE;
}

void SG_log_urgent(SG_context * pCtx, const char * szFormat, ...)
{
	va_list ap;
	va_start(ap,szFormat);
	_sg_log(pCtx, mpLoggingUrgentMessages, SG_LOG_MESSAGE_LEVEL_URGENT, szFormat, ap);
	va_end(ap);
}
void SG_vlog_urgent(SG_context * pCtx, const char * szFormat, va_list ap)
{
	_sg_log(pCtx, mpLoggingUrgentMessages, SG_LOG_MESSAGE_LEVEL_URGENT, szFormat, ap);
}

void SG_log(SG_context * pCtx, const char * szFormat, ...)
{
	va_list ap;
	va_start(ap,szFormat);
	_sg_log(pCtx, mpLoggingNormalMessages, SG_LOG_MESSAGE_LEVEL_NORMAL, szFormat, ap);
	va_end(ap);
}
void SG_vlog(SG_context * pCtx, const char * szFormat, va_list ap)
{
	_sg_log(pCtx, mpLoggingNormalMessages, SG_LOG_MESSAGE_LEVEL_NORMAL, szFormat, ap);
}

void SG_log_debug(SG_context * pCtx, const char * szFormat, ...)
{
	va_list ap;
	va_start(ap,szFormat);
	_sg_log(pCtx, mpLoggingDebugMessages, SG_LOG_MESSAGE_LEVEL_DEBUG, szFormat, ap);
	va_end(ap);
}
void SG_vlog_debug(SG_context * pCtx, const char * szFormat, va_list ap)
{
	_sg_log(pCtx, mpLoggingDebugMessages, SG_LOG_MESSAGE_LEVEL_DEBUG, szFormat, ap);
}

void SG_log_current_error(SG_context *pCtx)
{
	SG_string * pErrString = NULL;
	if( SG_IS_OK(SG_context__err_to_string(pCtx, &pErrString)) )
		SG_ERR_IGNORE(  SG_log(pCtx,SG_string__sz(pErrString))  );
	SG_STRING_NULLFREE(pCtx, pErrString);
}
void SG_log_current_error_urgent(SG_context *pCtx)
{
	SG_string * pErrString = NULL;
	if( SG_IS_OK(SG_context__err_to_string(pCtx, &pErrString)) )
		SG_ERR_IGNORE(  SG_log_urgent(pCtx,SG_string__sz(pErrString))  );
	SG_STRING_NULLFREE(pCtx, pErrString);
}
void SG_log_current_error_debug(SG_context *pCtx)
{
	SG_string * pErrString = NULL;
	if( SG_IS_OK(SG_context__err_to_string(pCtx, &pErrString)) )
		SG_ERR_IGNORE(  SG_log_debug(pCtx,SG_string__sz(pErrString))  );
	SG_STRING_NULLFREE(pCtx, pErrString);
}
