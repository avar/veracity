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
 * @file sg_time.h
 *
 * @details Routines for handling time values.
 *
 * TODO Verify that iTime is ***ALWAYS*** in UTC.  (I believe that this is correct.)
 * TODO
 * TODO Note that SG_time may be in UTC or LOCALTIME (depending on which version
 * TODO of __decode that you called).
 * TODO
 * TODO Please make sure that all calls to do time-formatting use an iTime rather
 * TODO than a SG_time so that we don't have to worry about the UTC->LOCALTIME
 * TODO delta being applied twice.
 * TODO
 * TODO Consider adding code to verify that time comparisons don't use SG_time's
 * TODO of different types.
 */

#ifndef H_SG_TIME_H
#define H_SG_TIME_H

BEGIN_EXTERN_C;

typedef struct
{
	SG_uint32 year;   /* year */
	SG_uint32 month;  /* month of year (1 - 12) */
	SG_uint32 mday;   /* day of month (1 - 31) */
	SG_uint32 hour;   /* hour (0 - 23) */
	SG_uint32 min;    /* minute (0 - 59) */
	SG_uint32 sec;    /* seconds (0 - 60) */
	SG_uint32 msec;   /* milliseconds */

	SG_uint32 wday;   /* day of week (Sunday = 0) */
	SG_uint32 yday;   /* day of year (0 - 365) */

	SG_int32 offsetUTC;  /* offset from UTC (TZONE and DST) in SECONDS (+ for east of PM and - for west) */
} SG_time;

#define SG_TIME_ZERO_INITIALIZER		{0,0,0, 0,0,0,0, 0,0, 0}

#if 0
void SG_time__get_microseconds_since_1970_utc(SG_context* pCtx, SG_int64* pResult);
#endif
void SG_time__get_milliseconds_since_1970_utc(SG_context* pCtx, SG_int64* pResult);
void SG_time__decode(SG_context* pCtx, SG_int64 iTime, SG_time* pTime);
void SG_time__decode__local(SG_context* pCtx, SG_int64 iTime, SG_time* pTime);
void SG_time__parse(SG_context* pCtx, const char * pszInput, SG_int64 * pReturnedTime, SG_bool bParseToMaximumValidValue);
void SG_time__parseRFC850(SG_context* pCtx, const char * pszInput, SG_int64 * pReturnedTime, SG_int64 * pReturnedLocalTime);
void SG_time__formatRFC850(SG_context* pCtx, SG_int64 utcms, char *formatted, SG_uint32 lenbuf);
void SG_time__local_time(SG_context* pCtx, SG_time * pResult);

#if defined(WINDOWS)
void SG_time__get_milliseconds_since_1970_utc__ft(SG_context* pCtx, SG_int64* pResult, FILETIME* pft);
void SG_time__get_nanoseconds_since_1970_utc__ft(SG_context* pCtx, SG_int64* pResult, FILETIME* pft);
#endif

/**
 *	"Tue, 16 Mar 2010 14:11:13 GMT" + 1
 */
#define SG_MAX_RFC850_LENGTH	(30)

/**
 * "yyyy/mm/dd hh:mm:ss.mmm +0000"
 * "yyyy/mm/dd hh:mm:ss.nnnnnnnnn +0000"
 * "yyyy/mm/dd hh:mm:ss.nnnnnnnnn -0500"
 *
 * Add 1 for the null byte.
 */
#define SG_TIME_FORMAT_LENGTH			35

/**
 * Do simple formatting of time value and generate a UTC-based, human-readable time.
 * This is quite crude.
 *
 * "yyyy/mm/dd hh:mm:ss.mmm +0000"
 *
 * You must provide a buffer containing at least SG_TIME_FORMAT_LENGTH+1 bytes to
 * receive the formatted message.
 */
void SG_time__format_utc__i64(
    SG_context* pCtx,
	SG_int64 iTime,
    char * pBuf,
    SG_uint32 lenBuf
    );

/**
 * Do simple formatting of time value and generate a localtime-based, human-readable time.
 * This is quite crude.
 *
 * "yyyy/mm/dd hh:mm:ss.mmm -0500"
 *
 * We CANNOT use TZNAMEs (like EDT or CST) because Windows doesn't store them like
 * that.  The various Windows functions (_get_tzname(), GetTimeZoneInformation(), and
 * etc. all use the values in the registry (see TIME_ZONE_INFORMATION structure) which
 * have values like "Eastern Standard Time" rather than "EST".  If the user has set
 * the TZ env variable to an old-school value (like "PST8PDT"), it will use that and
 * return "PST" or "PDT".  But we cannot rely on that.
 *
 * You must provide a buffer containing at least SG_TIME_FORMAT_LENGTH+1 bytes to
 * receive the formatted message.
 */
void SG_time__format_local__i64(
    SG_context* pCtx,
	SG_int64 iTime,
    char * pBuf,
    SG_uint32 lenBuf
    );

#if defined(SG_BUILD_FLAG_FEATURE_NS)
void SG_time__format_local__i64_ns(SG_context* pCtx, SG_int64 iTime_ns,
								   char * pBuf, SG_uint32 lenBuf);
#endif

//////////////////////////////////////////////////////////////////

END_EXTERN_C;


#endif
