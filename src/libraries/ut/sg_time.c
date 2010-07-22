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

#include <stdlib.h>
#include <time.h>

#define MILLISECONDS_PER_SECOND   (1000)

#define	TIME_T_MAX_BITS	32

static time_t utc_mktime(struct tm *tm);

static const char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char *weekdays[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};


//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)

#include <sys/time.h>

void SG_time__get_milliseconds_since_1970_utc(SG_context* pCtx, SG_int64* pResult)
{
	struct timeval tv;
	SG_int64 result;
	int rc;

	rc = gettimeofday(&tv, NULL);
	if (rc)
	{
		*pResult = 0;
		SG_ERR_THROW_RETURN( SG_ERR_ERRNO(errno) );
	}

	result = (SG_int64) tv.tv_sec;
	result *= MILLISECONDS_PER_SECOND;
	result += (tv.tv_usec / 1000);
	*pResult = result;

}

#endif /* MAC || LINUX */

//////////////////////////////////////////////////////////////////

#if defined(WINDOWS)

#define DELTA_EPOCH_IN_MILLISECS     11644473600000Ui64

void SG_time__get_milliseconds_since_1970_utc__ft(SG_context* pCtx, SG_int64* pResult, FILETIME* pft)
{
	SG_int64 result = 0;

	SG_NULLARGCHECK_RETURN(pResult);
	SG_NULLARGCHECK_RETURN(pft);

    result |= pft->dwHighDateTime;
    result <<= 32;
    result |= pft->dwLowDateTime;

	// a FILETIME is the number of 100-ns intervals since Jan 1, 1601.
	// convert to number milliseconds since unix epoch.
    result /= 10000;  /*convert to milliseconds*/
    result -= DELTA_EPOCH_IN_MILLISECS;

	*pResult = result;
}

void SG_time__get_milliseconds_since_1970_utc(SG_context* pCtx, SG_int64* pResult)
{
	FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
	SG_ERR_CHECK_RETURN(  SG_time__get_milliseconds_since_1970_utc__ft(pCtx, pResult, &ft)  );
}

#if defined(SG_BUILD_FLAG_FEATURE_NS)

#define DELTA_EPOCH_IN_100NANOSECS   (DELTA_EPOCH_IN_MILLISECS * 10000)

void SG_time__get_nanoseconds_since_1970_utc__ft(SG_context* pCtx, SG_int64* pResult, FILETIME* pft)
{
	SG_int64 result = 0;

	SG_NULLARGCHECK_RETURN(pResult);
	SG_NULLARGCHECK_RETURN(pft);

    result |= pft->dwHighDateTime;
    result <<= 32;
    result |= pft->dwLowDateTime;

	// a FILETIME is the number of 100-ns intervals since Jan 1, 1601.
	// convert to number of nanoseconds since unix epoch.
    result -= DELTA_EPOCH_IN_100NANOSECS;
    result *= 100;  /*convert to nanoseconds*/

	*pResult = result;
}
#endif

#endif /* WINDOWS */

//////////////////////////////////////////////////////////////////

void SG_time__decode__local(SG_context* pCtx, SG_int64 iTime, SG_time* pTime)
{
	// iTime contains time in milliseconds since epoch in UTC.
	// convert this to a SG_time expressed in localtime.

	time_t t1 = iTime / MILLISECONDS_PER_SECOND;

#if defined(MAC) || defined(LINUX)
	struct tm* ptm = localtime(&t1);
#endif
#if defined(WINDOWS)
	struct tm tBuf;
	struct tm* ptm = &tBuf;
	long tzBiasSec, dstBiasSec;
	(void)_localtime64_s(&tBuf,&t1);
	(void)_get_dstbias(&dstBiasSec);
	(void)_get_timezone(&tzBiasSec);
#endif

	SG_NULLARGCHECK_RETURN(pTime);

	pTime->year = ptm->tm_year + 1900;
	pTime->month = ptm->tm_mon + 1;
	pTime->mday = ptm->tm_mday;
	pTime->hour = ptm->tm_hour;
	pTime->min = ptm->tm_min;
	pTime->sec = ptm->tm_sec;
	pTime->msec = (SG_uint32)(iTime % MILLISECONDS_PER_SECOND);

	pTime->wday = ptm->tm_wday;
	pTime->yday = ptm->tm_yday;

#if defined(MAC) || defined(LINUX)
	pTime->offsetUTC = (SG_int32)ptm->tm_gmtoff;
#endif
#if defined(WINDOWS)
	pTime->offsetUTC = -(SG_int32)(tzBiasSec + ((ptm->tm_isdst > 0) ? dstBiasSec : 0));
#endif

}

void SG_time__decode(SG_context* pCtx,SG_int64 iTime, SG_time* pTime)
{
	// iTime contains time in milliseconds since epoch in UTC.
	// convert this to a SG_time expressed in UTC.

	time_t t1 = iTime / MILLISECONDS_PER_SECOND;
#if defined(MAC) || defined(LINUX)
	struct tm* ptm = gmtime(&t1);
#endif
#if defined(WINDOWS)
	struct tm tBuf;
	struct tm* ptm = &tBuf;
	(void)_gmtime64_s(&tBuf,&t1);
#endif

	SG_NULLARGCHECK_RETURN(pTime);

	pTime->year = ptm->tm_year + 1900;
	pTime->month = ptm->tm_mon + 1;
	pTime->mday = ptm->tm_mday;
	pTime->hour = ptm->tm_hour;
	pTime->min = ptm->tm_min;
	pTime->sec = ptm->tm_sec;
	pTime->msec = (SG_uint32)(iTime % MILLISECONDS_PER_SECOND);

	pTime->wday = ptm->tm_wday;
	pTime->yday = ptm->tm_yday;

	pTime->offsetUTC = 0;
}


void SG_time__parse(SG_context* pCtx,const char* pszInputString, SG_int64 * pTime, SG_bool bParseToMaximumValidValue)
{
	SG_string * pWorkingStr = NULL;
	char** splitOnSpaces = NULL;
	char** splitOnColons = NULL;
	char** splitOnDashes = NULL;
	SG_uint32 nCountAfterSplitOnSpaces = 0;
	SG_uint32 nCountAfterSplitOnDashes = 0;
	SG_uint32 nCountAfterSplitOnColons = 0;
	SG_string * pWorkingStringDate = NULL;
	SG_string * pWorkingStringTime = NULL;
	time_t resultTicks = 0;
	struct tm result;
	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pWorkingStr, pszInputString)  );
	//This understands exactly two formats
	//YYYY-MM-DD
	//
	//and
	//YYYY-MM-DD hh:mm:ss
	//The local time zone is always assumed.
	SG_ERR_CHECK(  SG_string__split__asciichar(pCtx, pWorkingStr, ' ', 2, &splitOnSpaces, &nCountAfterSplitOnSpaces) );
	if (nCountAfterSplitOnSpaces == 1)
	{
		//YYYY-M-DD

		SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pWorkingStringDate, splitOnSpaces[0])  );
		SG_ERR_CHECK(  SG_string__split__asciichar(pCtx, pWorkingStringDate, '-', 3, &splitOnDashes, &nCountAfterSplitOnDashes) );

		if(nCountAfterSplitOnDashes == 3)
		{
			SG_uint32 year = 0;
			SG_uint32 month = 0;
			SG_uint32 dayOfMonth = 0;

			SG_ERR_CHECK(  SG_uint32__parse(pCtx, &year, splitOnDashes[0])  );
			SG_ERR_CHECK(  SG_uint32__parse(pCtx, &month, splitOnDashes[1])  );
			SG_ERR_CHECK(  SG_uint32__parse(pCtx, &dayOfMonth, splitOnDashes[2])  );
			if (month<= 0 || month > 12)
				SG_ERR_THROW(SG_ERR_DATE_PARSING_ERROR);
			if (dayOfMonth<= 0 || dayOfMonth > 31)
				SG_ERR_THROW(SG_ERR_DATE_PARSING_ERROR);
			result.tm_year = year - 1900;
			result.tm_mon = month - 1;
			result.tm_mday = dayOfMonth;
			if (bParseToMaximumValidValue == SG_TRUE)
			{
				result.tm_hour = 23;
				result.tm_min = 59;
				result.tm_sec = 59;
			}
			else
			{
				result.tm_hour = 0;
				result.tm_min = 0;
				result.tm_sec = 0;
			}
		}
		else
			SG_ERR_THROW(SG_ERR_DATE_PARSING_ERROR);
	}
	else if (nCountAfterSplitOnSpaces == 2)
	{
		//YYYY-MM-DD hh:mm:ss
		//YYYY-M-DD

		SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pWorkingStringDate, splitOnSpaces[0])  );
		SG_ERR_CHECK(  SG_string__split__asciichar(pCtx, pWorkingStringDate, '-', 3, &splitOnDashes, &nCountAfterSplitOnDashes) );

		if(nCountAfterSplitOnDashes == 3)
		{
			SG_uint32 year = 0;
			SG_uint32 month = 0;
			SG_uint32 dayOfMonth = 0;

			SG_ERR_CHECK(  SG_uint32__parse(pCtx, &year, splitOnDashes[0])  );
			SG_ERR_CHECK(  SG_uint32__parse(pCtx, &month, splitOnDashes[1])  );
			SG_ERR_CHECK(  SG_uint32__parse(pCtx, &dayOfMonth, splitOnDashes[2])  );
			if (month<= 0 || month > 12)
				SG_ERR_THROW(SG_ERR_DATE_PARSING_ERROR);
			if (dayOfMonth<= 0 || dayOfMonth > 31)
				SG_ERR_THROW(SG_ERR_DATE_PARSING_ERROR);
			result.tm_year = year - 1900;
			result.tm_mon = month - 1;
			result.tm_mday = dayOfMonth;

		}
		else
			SG_ERR_THROW(SG_ERR_DATE_PARSING_ERROR);
		SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pWorkingStringTime, splitOnSpaces[1])  );
		SG_ERR_CHECK(  SG_string__split__asciichar(pCtx, pWorkingStringTime, ':', 3, &splitOnColons, &nCountAfterSplitOnColons) );

		if(nCountAfterSplitOnColons == 3)
		{
			SG_uint32 hour = 0;
			SG_uint32 minute = 0;
			SG_uint32 second = 0;

			SG_ERR_CHECK(  SG_uint32__parse(pCtx, &hour, splitOnColons[0])  );
			SG_ERR_CHECK(  SG_uint32__parse(pCtx, &minute, splitOnColons[1])  );
			SG_ERR_CHECK(  SG_uint32__parse(pCtx, &second, splitOnColons[2])  );
			if (hour > 23)
				SG_ERR_THROW(SG_ERR_DATE_PARSING_ERROR);
			if (minute > 59)
				SG_ERR_THROW(SG_ERR_DATE_PARSING_ERROR);
			if (second > 59)
				SG_ERR_THROW(SG_ERR_DATE_PARSING_ERROR);
			result.tm_hour = hour;
			result.tm_min = minute;
			result.tm_sec = second;
		}
		else
			SG_ERR_THROW(SG_ERR_DATE_PARSING_ERROR);
	}
	else
	{
		//Invalid date.
		SG_ERR_THROW(SG_ERR_DATE_PARSING_ERROR);
	}


	result.tm_isdst = -1;
	resultTicks = mktime(&result);
	if (resultTicks >= 0)
	{
		*pTime = resultTicks;
		*pTime = *pTime * MILLISECONDS_PER_SECOND;
		if (bParseToMaximumValidValue == SG_TRUE)
		{
			*pTime += 999;
		}
	}

	SG_STRING_NULLFREE(pCtx, pWorkingStringTime);
	SG_STRING_NULLFREE(pCtx, pWorkingStringDate);
	if (splitOnColons != NULL)
		SG_ERR_CHECK(  SG_freeStringList(pCtx, (const char ***)&splitOnColons, nCountAfterSplitOnColons)  );
	if (splitOnSpaces != NULL)
		SG_ERR_CHECK(  SG_freeStringList(pCtx, (const char ***)&splitOnSpaces, nCountAfterSplitOnSpaces)  );
	if (splitOnDashes != NULL)
		SG_ERR_CHECK(  SG_freeStringList(pCtx, (const char ***)&splitOnDashes, nCountAfterSplitOnDashes)  );
	SG_STRING_NULLFREE(pCtx, pWorkingStr);
	return;
fail:
	SG_STRING_NULLFREE(pCtx, pWorkingStringTime);
	SG_STRING_NULLFREE(pCtx, pWorkingStringDate);

	if (splitOnColons != NULL && nCountAfterSplitOnColons > 0)
		SG_ERR_IGNORE(  SG_freeStringList(pCtx, (const char ***)&splitOnColons, nCountAfterSplitOnColons)  );
	if (splitOnSpaces != NULL && nCountAfterSplitOnSpaces > 0)
		SG_ERR_IGNORE(  SG_freeStringList(pCtx, (const char ***)&splitOnSpaces, nCountAfterSplitOnSpaces)  );
	if (splitOnDashes != NULL)
		SG_ERR_IGNORE(  SG_freeStringList(pCtx, (const char ***)&splitOnDashes, nCountAfterSplitOnDashes)  );
	SG_STRING_NULLFREE(pCtx, pWorkingStr);
	return;
	}

static void _lookupMonth(SG_context* pCtx, const char *monthstr, int *month)
{
	int i;

	SG_NULLARGCHECK_RETURN(monthstr);

	for ( i = 0; i < 12; ++i )
	{
		if (strcmp(monthstr, months[i]) == 0)
		{
			*month = i;
			return;
		}
	}

	SG_ERR_THROW_RETURN(SG_ERR_ARGUMENT_OUT_OF_RANGE);
}

void SG_time__formatRFC850(SG_context* pCtx, SG_int64 utcms, char *formatted, SG_uint32 buflen)
{
	SG_time tm;

	SG_NULLARGCHECK_RETURN(formatted);

	// convert milliseconds-since-epoch into individual fields.

	SG_ERR_CHECK_RETURN(  SG_time__decode(pCtx, utcms, &tm)  );

	//Tue, 16 Mar 2010 14:11:13 EDT
	SG_ERR_CHECK_RETURN( SG_sprintf(pCtx, formatted, buflen,
		"%s, %02d %s %04d %02d:%02d:%02d GMT",
		weekdays[tm.wday], tm.mday, months[tm.month - 1], tm.year,
		tm.hour,tm.min,tm.sec) );

}

void SG_time__parseRFC850(SG_context* pCtx,const char* pszInputString, SG_int64 * pTime, SG_int64 * pReturnedLocalTime)
{
	int		scanOK = 0;
	char	trash[4];
	int		day = 0;
	int		month = 0;
	int		year = 0;
	int		hour = 0;
	int		minute = 0;
	int		second = 0;
	char	monthstr[4];
	char	tz[4];
	struct tm t;

	SG_UNUSED(pCtx);
	SG_UNUSED(pszInputString);

#if defined(WINDOWS)
	scanOK = sscanf_s(pszInputString, "%3s, %d %3s %d %d:%d:%d %3s",
		trash, sizeof(trash), &day, monthstr, sizeof(monthstr), &year, &hour, &minute, &second, tz, sizeof(tz));
#else
	scanOK = sscanf(pszInputString, "%3s, %d %3s %d %d:%d:%d %3s",
		trash, &day, monthstr, &year, &hour, &minute, &second, tz);
#endif

	if ((scanOK != 8) || (strcmp("GMT", tz) != 0))
	{
		SG_ERR_THROW_RETURN(SG_ERR_ARGUMENT_OUT_OF_RANGE);
	}

	SG_ERR_CHECK_RETURN(  _lookupMonth(pCtx, monthstr, &month)  );

	t.tm_min = minute;
	t.tm_hour = hour;
	t.tm_sec = second;
	t.tm_year = year - 1900;
	t.tm_mon = month;
	t.tm_mday = day;

	*pTime = (SG_int64)utc_mktime(&t) * 1000;
	*pReturnedLocalTime = (SG_int64)mktime(&t) * 1000;
}

//////////////////////////////////////////////////////////////////

void SG_time__format_utc__i64(SG_context* pCtx, SG_int64 iTime,
								  char * pBuf, SG_uint32 lenBuf)
{
	// do simple formatting of time value and generate a UTC-based, human-readable time.
	// this is like ctime(3), asctime(3), gmtime(3), localtime(3) only i don't currently
	// care about all the various formatting options.
	//
	// that is, we don't care about:
	// [] which fields we show,
	// [] localization (which order we show the fields and month names),
	// [] localtime (rather than UTC),
	// [] DST (rather than absolute UTC),
	// [] 12 vs 24 hour time.
	// [] UTF8.
	//
	// we are given a buffer that must be SG_TIME_FORMAT_LENGTH+1 characters or larger.

	SG_time tm;

	SG_NULLARGCHECK_RETURN( pBuf );
	SG_ARGCHECK_RETURN( (lenBuf >= SG_TIME_FORMAT_LENGTH+1), lenBuf);

	// convert milliseconds-since-epoch into individual fields.

	SG_ERR_CHECK_RETURN(  SG_time__decode(pCtx, iTime,&tm)  );

	SG_ERR_CHECK_RETURN( SG_sprintf(pCtx, pBuf,lenBuf,"%04d/%02d/%02d %02d:%02d:%02d.%03d +0000",
					 tm.year,tm.month,tm.mday,
					 tm.hour,tm.min,tm.sec,tm.msec) );

}

void SG_time__format_local__i64(SG_context* pCtx, SG_int64 iTime,
								  char * pBuf, SG_uint32 lenBuf)
{
	// do simple formatting of time value and generate a localtime-based, human-readable time.
	// this is like ctime(3), asctime(3), gmtime(3), localtime(3) only i don't currently
	// care about all the various formatting options.
	//
	// that is, we don't care about:
	// [] which fields we show,
	// [] localization (which order we show the fields and month names),
	// [] 12 vs 24 hour time.
	// [] UTF8.
	//
	// we are given a buffer that must be SG_TIME_FORMAT_LENGTH+1 characters or larger.

	SG_time tm;

	SG_NULLARGCHECK_RETURN( pBuf );
	SG_ARGCHECK_RETURN( (lenBuf >= SG_TIME_FORMAT_LENGTH+1), lenBuf );

	// convert milliseconds-since-epoch into individual fields.

	SG_ERR_CHECK_RETURN(  SG_time__decode__local(pCtx, iTime,&tm)  );

	SG_ERR_CHECK_RETURN( SG_sprintf(pCtx, pBuf,lenBuf,"%04d/%02d/%02d %02d:%02d:%02d.%03d %+03d%02d",
					 tm.year,tm.month,tm.mday,
					 tm.hour,tm.min,tm.sec,tm.msec,
					 tm.offsetUTC/3600,abs((tm.offsetUTC/60)%60)) );

}

#if defined(SG_BUILD_FLAG_FEATURE_NS)
void SG_time__format_local__i64_ns(SG_context* pCtx, SG_int64 iTime_ns,
								   char * pBuf, SG_uint32 lenBuf)
{
	SG_int64 iTime_ms  = iTime_ns / 1000000;
	SG_int32 remainder = (SG_int32)(iTime_ns % 1000000);
	SG_time tm;

	SG_NULLARGCHECK_RETURN( pBuf );
	SG_ARGCHECK_RETURN( (lenBuf >= SG_TIME_FORMAT_LENGTH+1), lenBuf );

	// convert milliseconds-since-epoch into individual fields.

	SG_ERR_CHECK_RETURN(  SG_time__decode__local(pCtx, iTime_ms, &tm)  );

	SG_ERR_CHECK_RETURN( SG_sprintf(pCtx, pBuf,lenBuf,"%04d/%02d/%02d %02d:%02d:%02d.%03d%06d %+03d%02d",
									tm.year,tm.month,tm.mday,
									tm.hour,tm.min,tm.sec,tm.msec,remainder,
									tm.offsetUTC/3600,abs((tm.offsetUTC/60)%60)) );
}
#endif

void SG_time__local_time(SG_context* pCtx, SG_time * pResult)
{
	SG_int64 lMillisecondsSince1970;

	SG_ERR_CHECK(  SG_time__get_milliseconds_since_1970_utc(pCtx, &lMillisecondsSince1970)  );
	SG_ERR_CHECK(  SG_time__decode__local(pCtx, lMillisecondsSince1970, pResult)  );

fail:
	return;
}

/*
 * Copyright (c) 1998-2000 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 */
/*
 * Copyright (c) 1987, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Arthur David Olson of the National Cancer Institute.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
** Adapted from code provided by Robert Elz, who writes:
**	The "best" way to do mktime I think is based on an idea of Bob
**	Kridle's (so its said...) from a long time ago. (mtxinu!kridle now).
**	It does a binary search of the time_t space.  Since time_t's are
**	just 32 bits, its a max of 32 iterations (even at 64 bits it
**	would still be very reasonable).
*/

//#include "lib.h"
//#include "utc-mktime.h"

static int tmcomp(register const struct tm * const atmp,
		  register const struct tm * const btmp)
{
	register int	result;

	if ((result = (atmp->tm_year - btmp->tm_year)) == 0 &&
		(result = (atmp->tm_mon - btmp->tm_mon)) == 0 &&
		(result = (atmp->tm_mday - btmp->tm_mday)) == 0 &&
		(result = (atmp->tm_hour - btmp->tm_hour)) == 0 &&
		(result = (atmp->tm_min - btmp->tm_min)) == 0)
			result = atmp->tm_sec - btmp->tm_sec;
	return result;
}

static time_t utc_mktime(struct tm *tm)
{
	register int			dir;
	register int			bits;
	register int			saved_seconds;
	time_t				t;
	struct tm			yourtm, *mytm;
#if defined(WINDOWS)
	struct tm	_mytm;

	mytm = &_mytm;
#endif

	yourtm = *tm;
	saved_seconds = yourtm.tm_sec;
	yourtm.tm_sec = 0;
	/*
	** Calculate the number of magnitude bits in a time_t
	** (this works regardless of whether time_t is
	** signed or unsigned, though lint complains if unsigned).
	**
	** We check TIME_T_MAX_BITS beforehand since gmtime() may fail
	** with large 64bit values in some systems.
	*/
	for (bits = 0, t = 1; t > 0 && bits < TIME_T_MAX_BITS-1; bits++)
		t <<= 1;

	/*
	** If time_t is signed, then 0 is the median value,
	** if time_t is unsigned, then 1 << bits is median.
	*/
	t = (t < 0) ? 0 : ((time_t) 1 << bits);
	for ( ; ; ) {
#if defined(WINDOWS)
		if (gmtime_s(mytm, &t) != 0)
		{
			return((time_t)-1);
		}
#else
		mytm = gmtime(&t);
#endif
		dir = tmcomp(mytm, &yourtm);
		if (dir != 0) {
			if (bits-- < 0)
				return (time_t)-1;
			if (bits < 0)
				--t;
			else if (dir > 0)
				t -= (time_t) 1 << bits;
			else	t += (time_t) 1 << bits;
			continue;
		}
		break;
	}
	t += saved_seconds;
	return t;
}
