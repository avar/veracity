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
 * @file sg_context.c
 *
 */

// IMPORTANT NOTE:
//
// All routines in this file need to use the nested context pDummyCtx when calling another
// sglib routines.  This prevents an error within the error-handler from overwriting the
// error being initially handled.

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_lib__private.h"

//////////////////////////////////////////////////////////////////

static SG_error _context__alloc__stacksize(SG_context** ppCtx)
{
	SG_error err;
	SG_context* pCtx = NULL;

	if (!ppCtx)
		return SG_ERR_INVALIDARG;

	err = SG_alloc_err(1, sizeof(SG_context), &pCtx);
	if ( SG_IS_ERROR(err) )
		return err;

	pCtx->level = 0;
	pCtx->errValues[pCtx->level] = SG_ERR_OK;
	pCtx->lenStackTrace = 0;
	pCtx->szStackTrace[0] = 0;

	pCtx->numMsgListeners = 0;

	*ppCtx = pCtx;
	return SG_ERR_OK;
}

SG_error SG_context__alloc(SG_context** ppCtx)
{
	// WARNING: We assume that it is possible for the us to create
	// WARNING: a properly-populated SG_context before the library is initialized.
	// WARNING: (Before SG_lib__global_initialize() has been called.)
	// WARNING:
	// WARNING: That is, that the creation of the SG_context MUST NOT require any
	// WARNING: character encoding, locale, and/or UTF-8 stuff.
	// WARNING:
	// WARNING: Also, there the SG_context creation MUST NOT require us to read
	// WARNING: any local settings.

	SG_error err;
	SG_context* pCtx = NULL;

	if (!ppCtx)
		return SG_ERR_INVALIDARG;

	err = _context__alloc__stacksize(&pCtx);
	if ( SG_IS_ERROR(err) )
		return err;

	*ppCtx = pCtx;

	return SG_ERR_OK;
}


/**
 * To keep the "foo__free(pCtx, pFoo)" form, we have to accept 2 args.
 * The 2nd is the one we are being asked to free.  The first should
 * probably be NULL.
 */
void SG_context__free(SG_context * pCtxToFree)
{
	if (!pCtxToFree)
		return;

	(void)SG_free__no_ctx(pCtxToFree);
}

//////////////////////////////////////////////////////////////////

SG_error SG_context__err__generic(SG_context* pCtx, SG_error new_err, const char * szFileName, int iLine)
{
	SG_ASSERT( pCtx );

//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );		// asserted in __push

	SG_ASSERT( (SG_IS_ERROR(new_err)) );						// require an error; formerly asserted in SG_ERR_ macro

	SG_ASSERT( (SG_IS_OK(pCtx->errValues[pCtx->level])) );		// assume no previous unhandled error

	pCtx->errValues[pCtx->level] = new_err;

	return SG_context__err_stackframe_add(pCtx, szFileName, iLine);
}

SG_error SG_context__err(SG_context* pCtx, SG_error new_err, const char * szFileName, int iLine, const char* szDescription)
{
	SG_error err;

// These are checked in SG_context__err__generic().
//	SG_ASSERT( pCtx );
//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	err = SG_context__err__generic(pCtx, new_err, szFileName, iLine);
	if (SG_IS_ERROR(err))
		return err;

	return SG_context__err_set_description(pCtx, szDescription);
}

SG_error SG_context__err_replace(SG_context* pCtx, SG_error new_err, const char * szFileName, int iLine)
{
	SG_ASSERT( pCtx );
//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	if (SG_IS_OK(new_err))
		return SG_ERR_INVALIDARG;

	if (SG_IS_OK(pCtx->errValues[pCtx->level]))
		return SG_ERR_CTX_NEEDS_ERR;

	pCtx->errValues[pCtx->level] = new_err;

	return SG_context__err_stackframe_add(pCtx, szFileName, iLine);
}

SG_error SG_context__err_set_description(SG_context* pCtx, const char* szFormat, ...)
{
	SG_error err;
	va_list ap;

	SG_ASSERT( pCtx );
//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	if (pCtx->level > 0)		// when in an error-on-error (level > 0),
		return SG_ERR_OK;		// we don't alter the description.

	SG_ASSERT( (SG_IS_ERROR(pCtx->errValues[pCtx->level])) );	// assume __err or __err__generic just set error

	SG_context__push_level(pCtx);		// format extra info on new level so that we don't trash error info in current level
	{
		va_start(ap,szFormat);
		SG_vsprintf_truncate(pCtx, pCtx->szDescription,SG_CONTEXT_LEN_DESCRIPTION + 1,szFormat,ap);
		va_end(ap);

		err = pCtx->errValues[pCtx->level];		// the error result of the sprintf
	}
	SG_context__pop_level(pCtx);

	return err;
}

SG_error SG_context__err_get_description(const SG_context* pCtx, const char** ppszInfo)
{
	SG_ASSERT( pCtx );
//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	if (!ppszInfo)
		return SG_ERR_INVALIDARG;

	if (pCtx->level > 0)
	{
		// when in an error-on-error (level > 0), the description DOES NOT belong to the
		// error context/level that we are in (that of the error-on-error).  The description
		// *always* belongs to the original error that triggered things.

		*ppszInfo = NULL;
	}
	else
	{
		*ppszInfo = pCtx->szDescription;
	}

	return SG_ERR_OK;
}

SG_error SG_context__get_err(const SG_context* pCtx, SG_error* pErr)
{
	SG_ASSERT( pCtx );
//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	if (!pErr)
		return SG_ERR_INVALIDARG;

	*pErr = pCtx->errValues[pCtx->level];

	return SG_ERR_OK;
}

SG_bool SG_context__err_equals(const SG_context* pCtx, SG_error err)
{
	SG_ASSERT( pCtx );		// I don't know what else to do here.
//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	return (pCtx->errValues[pCtx->level] == err);
}

SG_bool SG_context__has_err(const SG_context* pCtx)
{
	SG_ASSERT( pCtx );

	return ( SG_CONTEXT__HAS_ERR(pCtx) );
}

SG_bool SG_context__stack_is_full(const SG_context* pCtx)
{
	SG_ASSERT( pCtx );
//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	if (pCtx->level > 0)			// when in an error-on-error situation, we don't push
		return SG_FALSE;			// stackframes, so it doesn't matter.
	else
		return pCtx->bStackTraceAtLimit;
}

void SG_context__err_reset(SG_context* pCtx)
{
	SG_ASSERT( pCtx );
//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	pCtx->errValues[pCtx->level] = SG_ERR_OK;

	if (pCtx->level > 0) // we only clear the error value at the current error-on-error level.
		return;          // this allows the fail-block code to do whatever it needs without affecting the original error.

	pCtx->szDescription[0] = 0;

	pCtx->lenStackTrace = 0;
	pCtx->szStackTrace[0] = 0;

	pCtx->bStackTraceAtLimit = SG_FALSE;
}

SG_error SG_context__err_stackframe_add(SG_context* pCtx, const char* szFilename, SG_uint32 linenum)
{
	SG_error err = SG_ERR_OK;
	SG_uint32 len_needed;
	SG_uint32 len_avail;
	char buf[20];

	SG_ASSERT( pCtx );
//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	if (pCtx->level > 0)		// when in an error-on-error (level > 0),
		return SG_ERR_OK;		// we don't add to the stack trace.

	SG_ASSERT( (szFilename && *szFilename) );
	SG_ASSERT( (linenum) );
	SG_ASSERT( (SG_IS_ERROR(pCtx->errValues[pCtx->level])) );
	SG_ASSERT( (!pCtx->bStackTraceAtLimit) );

	// do all of the formatting in an error-on-error level so that none of
	// the string manipulation trashes the current error context.
	//
	// ***DO NOT JUMP OUT OF THIS PUSH..POP BLOCK.***

	SG_context__push_level(pCtx);
	{
		SG_uint32 lenFilename;
		SG_uint32 lenLineNr;

		SG_sprintf(pCtx, buf, sizeof(buf), "%d", linenum);

		lenLineNr = strlen(buf);
		lenFilename = strlen(szFilename);
		len_needed = lenFilename + lenLineNr + 3; 	// 3 == \t, :, and \n.

		// TODO should 5 be 6 to account for the NULL byte?
		// 5 == "\t...\n"
		len_avail = SG_NrElements(pCtx->szStackTrace) - pCtx->lenStackTrace - 5;

		if (len_needed > len_avail)
		{
			pCtx->bStackTraceAtLimit = SG_TRUE;
			memmove(&pCtx->szStackTrace[pCtx->lenStackTrace],"\t...\n",5);
			pCtx->lenStackTrace += 5;
			pCtx->szStackTrace[pCtx->lenStackTrace] = 0;
		}
		else
		{
			pCtx->szStackTrace[pCtx->lenStackTrace] = '\t';                             pCtx->lenStackTrace++;
			memmove(&pCtx->szStackTrace[pCtx->lenStackTrace],szFilename,lenFilename);   pCtx->lenStackTrace+=lenFilename;
			pCtx->szStackTrace[pCtx->lenStackTrace] = ':';                              pCtx->lenStackTrace++;
			memmove(&pCtx->szStackTrace[pCtx->lenStackTrace],buf,lenLineNr);            pCtx->lenStackTrace+=lenLineNr;
			pCtx->szStackTrace[pCtx->lenStackTrace] = '\n';                             pCtx->lenStackTrace++;
			pCtx->szStackTrace[pCtx->lenStackTrace] = 0;
		}

		err = pCtx->errValues[pCtx->level];		// we return the error value from the formatting.
	}
	SG_context__pop_level(pCtx);

	return err;
}

SG_error SG_context__err_to_string(SG_context* pCtx, SG_string** ppErrString)
{
	SG_error err = SG_ERR_OK;
	char szErr[SG_ERROR_BUFFER_SIZE];
	SG_string* pTempStr = NULL;
	SG_uint32 lenRequired;

	SG_ASSERT( pCtx );
//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	if (!ppErrString)
		return SG_ERR_INVALIDARG;

	if (pCtx->level > 0)
	{
		// when in an error-on-error (level > 0), the error string DOES NOT belong to the
		// error context/level that we are in.  The full message is only defined for the
		// original error that triggered things.

		*ppErrString = NULL;
		return SG_ERR_OK;
	}

	SG_error__get_message(pCtx->errValues[pCtx->level], szErr, sizeof(szErr));

	lenRequired = (  strlen(szErr)
					 + strlen(pCtx->szDescription)
					 + pCtx->lenStackTrace
					 + 100);

	// do all of the formatting in an error-on-error context/level
	// so that none of the string allocation/manipulation trashes
	// the current error context.
	//
	// ***DO NOT JUMP OUT OF THIS PUSH..POP BLOCK.***

	SG_context__push_level(pCtx);
	{
		SG_STRING__ALLOC__RESERVE(pCtx,&pTempStr,lenRequired);

		if (SG_IS_OK(pCtx->errValues[pCtx->level]))
		{
			// since we pre-reserved all of the space we require, we
			// don't expect any of the appends to fail.  So we can
			// ignore intermediate errors after each step.  if one of
			// them does have a problem, the subsequent statements will
			// fail because we already have an error at this level.
			// either way, we don't care because we're going to pop the
			// level in a minute anyway.

			if (szErr[0] != 0)
			{
				SG_string__append__sz(pCtx, pTempStr, szErr);
				if (pCtx->szDescription[0] != 0)
					SG_string__append__sz(pCtx, pTempStr, ": ");
				else
					SG_string__append__sz(pCtx, pTempStr, ".");
			}
			if (pCtx->szDescription[0] != 0)
				SG_string__append__sz(pCtx, pTempStr, pCtx->szDescription);
			if (szErr[0] != 0 || pCtx->szDescription[0] != 0)
				SG_string__append__sz(pCtx, pTempStr, "\n");

			SG_string__append__sz(pCtx, pTempStr, pCtx->szStackTrace);

			// if all of the formating works, we return the allocated string.
			// if not, we delete it and return the formatting error.

			if (SG_IS_OK(pCtx->errValues[pCtx->level]))
				*ppErrString = pTempStr;
			else
				SG_STRING_NULLFREE(pCtx, pTempStr);
		}

		err = pCtx->errValues[pCtx->level];		// we return the error value of the allocation/formatting
	}
	SG_context__pop_level(pCtx);

	return err;
}

SG_error SG_context__err_to_console(SG_context* pCtx, SG_console_stream cs)
{
	SG_error err;
	SG_string* pErrStr = NULL;

	SG_ASSERT( pCtx );
//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	if (pCtx->level > 0)		// when in an error-on-error (level > 0), there is NO error string.
		return SG_ERR_OK;		// so we don't need to do anything.

	// get the full error string about the error at the current level.

	err = SG_context__err_to_string(pCtx, &pErrStr);
	if (SG_IS_ERROR(err))		// an allocation/formatting error, just give up.
		return err;

	// write the error string/message to the console.  we push a new level
	// so that any problems converting the message to the user's locale or
	// writing to the console device don't trash the current error context.
	//
	// ***DO NOT JUMP OUT OF THIS PUSH..POP BLOCK.***

	SG_context__push_level(pCtx);
	{
		SG_console(pCtx, cs, SG_string__sz(pErrStr));

		err = pCtx->errValues[pCtx->level];		// we return the error value of the conversion/writing.
	}
	SG_context__pop_level(pCtx);

	SG_STRING_NULLFREE(pCtx, pErrStr);

	return err;
}

//////////////////////////////////////////////////////////////////

void SG_context__push_level(SG_context * pCtx)
{
	SG_ASSERT( pCtx );

	// I'm going to assume that whatever kind of clean up
	// that we need to do in a fail-block cannot cause
	// us to push 100 levels....

	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	pCtx->level++;
	pCtx->errValues[pCtx->level] = SG_ERR_OK;
}

void SG_context__pop_level(SG_context * pCtx)
{
	SG_ASSERT( pCtx );
	SG_ASSERT( pCtx->level > 0 );
//	SG_ASSERT( pCtx->level < SG_CONTEXT_MAX_ERROR_LEVELS );

	pCtx->level--;
}

//////////////////////////////////////////////////////////////////

// The msg routines aren't thread safe, but that's okay because
// an SG_context* is only ever accessed by its creator's thread.

void SG_context__msg__add_listener(SG_context* pCtx, SG_context__msg_callback* cb)
{
	SG_NULLARGCHECK_RETURN(pCtx);
	SG_NULLARGCHECK_RETURN(cb);

	if (pCtx->numMsgListeners >= SG_CONTEXT_MAX_MSG_LISTENERS)
		SG_ERR_THROW_RETURN(SG_ERR_LIMIT_EXCEEDED);

	pCtx->msg_callbacks[pCtx->numMsgListeners++] = cb;
}

void SG_context__msg__rm_listener(SG_context* pCtx, SG_context__msg_callback* cb)
{
	SG_uint32 i = 0;

	SG_NULLARGCHECK_RETURN(pCtx);
	SG_NULLARGCHECK_RETURN(cb);

	while(i < pCtx->numMsgListeners)
	{
		if( pCtx->msg_callbacks[i] == cb )
		{
			pCtx->numMsgListeners--;
			pCtx->msg_callbacks[i] = pCtx->msg_callbacks[pCtx->numMsgListeners];
			return;
		}
		++i;
	}
}

void SG_context__msg__emit(SG_context* pCtx, const char* pszMsg)
{
	SG_uint32 i;

	SG_NULLARGCHECK_RETURN(pCtx);

	if (pszMsg)
	{
		for (i = 0; i < pCtx->numMsgListeners; i++)
		{
			SG_ERR_CHECK_RETURN(  pCtx->msg_callbacks[i](pCtx, pszMsg)  );
		}
	}
}