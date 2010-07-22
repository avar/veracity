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
 * @file sg_console.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

#if defined(WINDOWS)
#define OS_CHAR_T		wchar_t
#else
#define OS_CHAR_T		char
#endif

//this version will not do any sprintf style replacement.  It's best to use this one when you
//know that you don't need replacement.  Otherwise, you will get an error if your string
//contains a % sign.
void SG_console__raw(SG_context * pCtx,
				SG_console_stream cs, const char * szRawString)
{
	SG_ERR_CHECK(SG_console(pCtx, cs, "%s", szRawString)  );
fail:
	return;
}

//////////////////////////////////////////////////////////////////

void SG_console(SG_context * pCtx,
				SG_console_stream cs, const char * szFormat, ...)
{
	// we are given a UTF-8 string and want to format it and write it to
	// a stream using the appropriate character encoding for the platform.

	va_list ap;

	va_start(ap,szFormat);
	SG_console__va(pCtx,cs,szFormat,ap);
	va_end(ap);

	SG_ERR_CHECK_RETURN_CURRENT;
}

#if defined(WINDOWS)
static void _sg_console__write(SG_context * pCtx,
							   SG_console_stream cs, const SG_string * pString)
{
	//////////////////////////////////////////////////////////////////
	// On Windows, STDOUT and STDERR are opened in TEXT (translated)
	// mode (this causes LF <--> CRLF translation).  For historical
	// reasons this is said to be the "ANSI" encoding.  I think this
	// means the system-default ANSI code page.
	//
	// With VS2005, fopen() allows you to specify a Coded Character Set
	// CCS={UTF-8,UTF-16LE,UNICODE} with the mode args.  Adding one of
	// these CCS args changes the translation applied to the stream and
	// may cause a BOM to be read/written.
	//
	// I have yet to find a way to change the CCS setting for an already
	// opened stream (such as STDOUT and STDERR).  _setmode() can be
	// used to change the TEXT/BINARY setting, but it doesn't say anything
	// about changing the CCS.
	//
	// http://msdn.microsoft.com/en-us/library/yeby3zcb.aspx
	// http://msdn.microsoft.com/en-us/library/c4cy2b8e.aspx
	// http://msdn.microsoft.com/en-us/library/883tf19a.aspx
	// http://msdn.microsoft.com/en-us/library/x99tb11d.aspx
	//
	// Also, _wfopen() allows one to use a wide-char pathname to open
	// a file -- it does ***NOT*** imply anything about the translation
	// to be applied to the contents of the file; for that, you have to
	// use one of the CCS= args.
	//
	// fwprintf() writes the contents of a wide-char buffer to the stream.
	// they translate the buffer using the translation settings on the stream.
	// therefore, for STDOUT and STDERR, (which are ANSI TEXT), they
	// take the buffer and call wctomb() which uses the current CODE PAGE
	// set in the user's LOCALE to convert the buffer.
	//
	// http://msdn.microsoft.com/en-us/library/wabhak7d.aspx
	//
	// According to the following link, it is ***NOT*** possible to
	// set the user's locale to UTF-8 (because it requires more than
	// 2 bytes per character).
	//
	// http://msdn.microsoft.com/en-us/library/x99tb11d.aspx
	//
	// THEREFORE, for STDOUT and STDERR, our nice-n-neat UTF-8 output
	// will get converted into some CODE PAGE LOCALE (probably with
	// '?' substitutions for code points that are not in the code page).
	// I THINK THIS IS THE ONLY PLACE WHERE WE USE THE USER'S LOCALE
	// CODE PAGE ON WINDOWS.
	//
	//
	// TODO When we want to support reporting to a GUI window and/or logging
	// TODO to a custom logfile, we can do it CORRECTLY using either UTF-8.
	//
	//////////////////////////////////////////////////////////////////

	wchar_t * pBuf = NULL;
	const char * szInString;

	// allow null or blank string to silently succeed.

	if (!pString)
		return;
	szInString = SG_string__sz(pString);
	if (!szInString || !*szInString)
		return;

	// Convert UTF-8 input into UNICODE/UTF-16/UCS-2 wchar_t buffer.
	// Then let the OS mangle the wchar_t buffer as it needs to.

	SG_ERR_CHECK_RETURN(  SG_utf8__extern_to_os_buffer__wchar(pCtx, szInString,&pBuf,NULL)  );

	// WARNING: for GUI apps, stdout and stderr are closed before wmain()
	// WARNING: gets called.  so we need to do something else.

	switch (cs)
	{
	default:
	case SG_CS_STDERR:
		fwprintf(stderr,L"%s",pBuf);
		break;

	case SG_CS_STDOUT:
		fwprintf(stdout,L"%s",pBuf);
		break;
	}

#ifdef DEBUG
	OutputDebugString(pBuf); // Show console messages in VS debug output window.
#endif

	SG_NULLFREE(pCtx, pBuf);
}
#else
static void _sg_console__write(SG_context * pCtx,
							   SG_console_stream cs, const SG_string * pString)
{
	//////////////////////////////////////////////////////////////////
	// On Linux/Mac, we should write messages to STDOUT and STDERR using
	// the user's locale.  That is, I don't think we should blindly write
	// UTF-8, unless that is their locale setting.
	//////////////////////////////////////////////////////////////////

	char * pBuf = NULL;
	SG_uint32 lenBuf;
	const char * szInString;

	// allow null or blank string to silently succeed.

	if (!pString)
		return;
	szInString = SG_string__sz(pString);
	if (!szInString || !*szInString)
		return;

#if defined(MAC)
	SG_ERR_CHECK_RETURN(  SG_utf8__extern_to_os_buffer__sz(pCtx,szInString,&pBuf)  );
#endif
#if defined(LINUX)
	SG_utf8__extern_to_os_buffer__sz(pCtx,szInString,&pBuf);
	if (SG_context__has_err(pCtx))
	{
		if (SG_context__err_equals(pCtx,SG_ERR_UNMAPPABLE_UNICODE_CHAR))
		{
			// silently eat the error and try to use the lossy converter.
			SG_context__err_reset(pCtx);
			SG_ERR_CHECK_RETURN(  SG_utf8__extern_to_os_buffer_with_substitutions(pCtx,szInString,&pBuf)  );
		}
		else
		{
			SG_ERR_RETHROW_RETURN;
		}
	}
#endif

	lenBuf = (SG_uint32)strlen(pBuf);

	switch (cs)
	{
	default:
	case SG_CS_STDERR:
		fwrite(pBuf,sizeof(char),lenBuf,stderr);
		break;

	case SG_CS_STDOUT:
		fwrite(pBuf,sizeof(char),lenBuf,stdout);
		break;
	}

	SG_NULLFREE(pCtx, pBuf);
}
#endif

#if defined(WINDOWS)
static void _sg_console__grow_wchar_buf(SG_context* pCtx, wchar_t** ppThis, SG_uint32 iCurrSize, SG_uint32 iGrowBy)
{
	wchar_t* pwcsNew;
	SG_uint32 iSizeNew;

	SG_ARGCHECK_RETURN(ppThis, ppThis);

	iSizeNew = iCurrSize + iGrowBy;

	SG_ERR_CHECK_RETURN(  SG_allocN(pCtx,iSizeNew,pwcsNew)  );

	if (*ppThis)
	{
		if (iCurrSize > 0)
			wmemmove(pwcsNew,*ppThis,iCurrSize*sizeof(wchar_t));
		SG_NULLFREE(pCtx, *ppThis);
	}

	*ppThis = pwcsNew;
}
#else
static void _sg_console__grow_char_buf(SG_context* pCtx, char** ppThis, SG_uint32 iCurrSize, SG_uint32 iGrowBy)
{
	char* pszNew;
	SG_uint32 iSizeNew;

	SG_ARGCHECK_RETURN(ppThis, ppThis);

	iSizeNew = iCurrSize + iGrowBy;

	SG_ERR_CHECK_RETURN(  SG_allocN(pCtx,iSizeNew,pszNew)  );

	if (*ppThis)
	{
		if (iCurrSize > 0)
			memmove(pszNew,*ppThis,iCurrSize*sizeof(char));
		SG_NULLFREE(pCtx, *ppThis);
	}

	*ppThis = pszNew;
}
#endif

void SG_console__va(SG_context * pCtx,
					SG_console_stream cs, const char * szFormat, va_list ap)
{
	// we are given a UTF-8 string and want to format it and write it to
	// a stream using the appropriate character encoding for the platform.

	SG_string * pString = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx,&pString)  );
	SG_ERR_CHECK(  SG_string__vsprintf(pCtx,pString,szFormat,ap)  );

	SG_ERR_CHECK(  _sg_console__write(pCtx,cs,pString)  );

	SG_STRING_NULLFREE(pCtx, pString);
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pString);
}

void SG_console__flush(SG_context* pCtx, SG_console_stream cs)
{
	SG_UNUSED(pCtx);

	// TODO: propagate errors?
	switch (cs)
	{
		default:
		case SG_CS_STDERR:
			fflush(stderr);
			break;

		case SG_CS_STDOUT:
			fflush(stdout);
			break;
	}
}

void SG_console__read_stdin(SG_context* pCtx, SG_string** ppStrInput)
{
	SG_uint32 iCount = 0;
	SG_uint32 iMax = 128;

#if defined(WINDOWS)
	wint_t retval;
	wchar_t* pwcs;

	SG_ERR_CHECK(  SG_allocN(pCtx, iMax, pwcs)  );

	while((retval = fgetwc(stdin)) != WEOF)
	{
		pwcs[iCount++] = (wchar_t) retval;

		if (iCount == iMax)
		{
			SG_ERR_CHECK(  _sg_console__grow_wchar_buf(pCtx, &pwcs, iMax, iMax)  );
			iMax = iCount + iMax;
		}
	}
	pwcs[iCount] = 0;

	SG_ERR_CHECK(  SG_utf8__intern_from_os_buffer__wchar(pCtx, *ppStrInput, pwcs)  );

    // fall through
fail:
	SG_NULLFREE(pCtx, pwcs);

#else

	char* pszInput;
	int retval;

	SG_ERR_CHECK(  SG_allocN(pCtx, iMax, pszInput)  );

	while((retval = fgetc(stdin)) != EOF)
	{
		pszInput[iCount++] = (char) retval;

		if (iCount == iMax)
		{
			SG_ERR_CHECK(  _sg_console__grow_char_buf(pCtx, &pszInput, iMax, iMax)  );
			iMax = iCount + iMax;
		}
	}
	pszInput[iCount] = 0;

	SG_ERR_CHECK(  SG_utf8__intern_from_os_buffer__sz(pCtx, *ppStrInput, pszInput)  );

    // fall through
fail:
	SG_NULLFREE(pCtx, pszInput);

#endif
}

void SG_console__readline_stdin(SG_context* pCtx, SG_string** ppStrInput)
{
	SG_uint32 iCount = 0;
	SG_uint32 iMax = 128;

#if defined(WINDOWS)
	wint_t retval;
	wchar_t* pwcs;

	SG_ERR_CHECK(  SG_allocN(pCtx, iMax, pwcs)  );

	while((retval = fgetwc(stdin)) != '\n')
	{
		pwcs[iCount++] = (wchar_t) retval;

		if (iCount == iMax)
		{
			SG_ERR_CHECK(  _sg_console__grow_wchar_buf(pCtx, &pwcs, iMax, iMax)  );
			iMax = iCount + iMax;
		}
	}
	pwcs[iCount] = 0;

	SG_ERR_CHECK(  SG_utf8__intern_from_os_buffer__wchar(pCtx, *ppStrInput, pwcs)  );

    // fall through
fail:
	SG_NULLFREE(pCtx, pwcs);

#else

	char* pszInput;
	int retval;

	SG_ERR_CHECK(  SG_allocN(pCtx, iMax, pszInput)  );

	while((retval = fgetc(stdin)) != '\n')
	{
		pszInput[iCount++] = (char) retval;

		if (iCount == iMax)
		{
			SG_ERR_CHECK(  _sg_console__grow_char_buf(pCtx, &pszInput, iMax, iMax)  );
			iMax = iCount + iMax;
		}
	}
	pszInput[iCount] = 0;

	SG_ERR_CHECK(  SG_utf8__intern_from_os_buffer__sz(pCtx, *ppStrInput, pszInput)  );

    // fall through
fail:
	SG_NULLFREE(pCtx, pszInput);

#endif
}

//////////////////////////////////////////////////////////////////

void SG_console__ask_question__yn(SG_context * pCtx,
								  const char * pszQuestion,
								  SG_console_qa qaDefault,
								  SG_bool * pbAnswer)
{
	SG_string * pStrInput = NULL;
	const char * pszAnswer;

	SG_NONEMPTYCHECK_RETURN(pszQuestion);

	while (1)
	{
		switch (qaDefault)
		{
		case SG_QA_DEFAULT_YES:
			SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s [Yn]: ", pszQuestion)  );
			SG_ERR_CHECK(  SG_console__readline_stdin(pCtx, &pStrInput)  );
			pszAnswer = SG_string__sz(pStrInput);
			if ((pszAnswer[0] == 0) || (pszAnswer[0] == 'y'))
			{
				*pbAnswer = SG_TRUE;
				goto done;
			}
			else if (pszAnswer[0] == 'n')
			{
				*pbAnswer = SG_FALSE;
				goto done;
			}
			break;

		case SG_QA_DEFAULT_NO:
			SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s [yN]: ", pszQuestion)  );
			SG_ERR_CHECK(  SG_console__readline_stdin(pCtx, &pStrInput)  );
			pszAnswer = SG_string__sz(pStrInput);
			if ((pszAnswer[0] == 0) || (pszAnswer[0] == 'n'))
			{
				*pbAnswer = SG_FALSE;
				goto done;
			}
			else if (pszAnswer[0] == 'y')
			{
				*pbAnswer = SG_TRUE;
				goto done;
			}
			break;

		default:				// quiets compiler
		case SG_QA_NO_DEFAULT:
			SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "%s [yn]: ", pszQuestion)  );
			SG_ERR_CHECK(  SG_console__readline_stdin(pCtx, &pStrInput)  );
			pszAnswer = SG_string__sz(pStrInput);
			if (pszAnswer[0] == 'y')
			{
				*pbAnswer = SG_TRUE;
				goto done;
			}
			else if (pszAnswer[0] == 'n')
			{
				*pbAnswer = SG_FALSE;
				goto done;
			}
			break;
		}

		SG_STRING_NULLFREE(pCtx, pStrInput);
	}

done:
	;
fail:
	SG_STRING_NULLFREE(pCtx, pStrInput);
}
