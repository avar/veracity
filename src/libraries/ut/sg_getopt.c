/**
 * @copyright
 * Copyright (C) 2008, SourceGear LLC
 * @endcopyright
 *
 * @file sg_getopt.c
 *
 * Portions of this code is based upon the following Apache Portable Runtime source code:
 * http://www.apache.org/dist/apr/apr-1.3.8-win32-src.zip (getopt.c)
 * and is covered by the Apache license given below.
 *
 * Other portions of this file are copyright SourceGear.
 */

/*
 * Copyright (c) 1987, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_getopt.h"

//////////////////////////////////////////////////////////////////

void SG_getopt__alloc(SG_context * pCtx, int argc, ARGV_CHAR_T ** argv, SG_getopt ** ppGetopt)
{
	SG_getopt * pGetopt = NULL;
	SG_string * pStr = NULL;
	int k;

	SG_ERR_CHECK(  SG_alloc1(pCtx,pGetopt)  );

	pGetopt->count_args = 0;

	// make sure we alloc for at least app and command, since we fake a help if they left the command off.
	if (argc > 2)
		SG_ERR_CHECK(  SG_alloc(pCtx,argc,sizeof(char *),&pGetopt->paszArgs)  );
	else
		SG_ERR_CHECK(  SG_alloc(pCtx, 2, sizeof(char*), &pGetopt->paszArgs)  );

	// intern our app name.

	SG_ERR_CHECK(  SG_getopt__set_app_name(pCtx, pGetopt,argv[0])  );

	SG_ERR_CHECK(  SG_strdup(pCtx, SG_string__sz(pGetopt->pStringAppName), &pGetopt->paszArgs[pGetopt->count_args++])  );

	// intern the command name.  if no command given, fake a "sg help".

	SG_ERR_CHECK(  SG_getopt__set_command_name(pCtx, pGetopt, ((argc < 2) ? ARGV_LITERAL("help") : argv[1]))  );

	SG_ERR_CHECK(  SG_strdup(pCtx, SG_string__sz(pGetopt->pStringCommandName), &pGetopt->paszArgs[pGetopt->count_args++])  );

	// intern the rest of the command line arguments into 1 of 2
	// utf-8 argv subsets.

	if (argc > 2)
	{	
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStr)  );

		for (k=2; k<argc; k++)
		{
			char * psz;

			SG_ERR_CHECK(  SG_UTF8__INTERN_FROM_OS_BUFFER(pCtx, pStr,argv[k])  );
			SG_ERR_CHECK(  SG_strdup(pCtx, SG_string__sz(pStr),&psz)  );

			pGetopt->paszArgs[pGetopt->count_args++] = psz;
		}
	}
	
	SG_STRING_NULLFREE(pCtx, pStr);
	
	pGetopt->place = "";

    pGetopt->interleave = 0;
    pGetopt->ind = 1;
    pGetopt->skip_start = 1;
    pGetopt->skip_end = 1;

	*ppGetopt = pGetopt;
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pStr);
	SG_GETOPT_NULLFREE(pCtx, pGetopt);
}

void SG_getopt__free(SG_context * pCtx, SG_getopt * pGetopt)
{
	if (!pGetopt)
		return;

	SG_STRING_NULLFREE(pCtx, pGetopt->pStringAppName);
	SG_STRING_NULLFREE(pCtx, pGetopt->pStringCommandName);

	if (pGetopt->paszArgs)
	{
		SG_ERR_CHECK_RETURN(  SG_freeStringList(pCtx, (const char ***)&pGetopt->paszArgs, pGetopt->count_args)  );
	}

	SG_NULLFREE(pCtx, pGetopt);
}

//////////////////////////////////////////////////////////////////

void SG_getopt__set_app_name(SG_context * pCtx, SG_getopt * pGetopt, ARGV_CHAR_T * pBuf)
{
	SG_pathname *tp = NULL;
	SG_string *st = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &st)  );
	SG_ERR_CHECK(  SG_UTF8__INTERN_FROM_OS_BUFFER(pCtx,st,pBuf)  );

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &tp, SG_string__sz(st))  );
	SG_STRING_NULLFREE(pCtx, st);

	SG_STRING_NULLFREE(pCtx, pGetopt->pStringAppName);

	SG_ERR_CHECK(  SG_pathname__get_last(pCtx, tp, &pGetopt->pStringAppName)  );

fail:
	SG_STRING_NULLFREE(pCtx, st);
	SG_PATHNAME_NULLFREE(pCtx, tp);
}

void SG_getopt__set_command_name(SG_context * pCtx, SG_getopt * pGetopt, ARGV_CHAR_T * pBuf)
{
	if (!pGetopt->pStringCommandName)
		SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC(pCtx, &pGetopt->pStringCommandName)  );

	SG_ERR_CHECK_RETURN(  SG_UTF8__INTERN_FROM_OS_BUFFER(pCtx,pGetopt->pStringCommandName,pBuf)  );
}

//////////////////////////////////////////////////////////////////

const char * SG_getopt__get_app_name(const SG_getopt * pGetopt)
{
	return SG_string__sz(pGetopt->pStringAppName);
}

const char * SG_getopt__get_command_name(const SG_getopt * pGetopt)
{
	return SG_string__sz(pGetopt->pStringCommandName);
}

/////////////////////////////////////////////////////////////////

/* Reverse the sequence argv[start..start+len-1]. */
static void _sg_reverse(char **argv, SG_uint32 start, SG_uint32 len)
{
    char *temp;

    for (; len >= 2; start++, len -= 2) {
        temp = argv[start];
        argv[start] = argv[start + len - 1];
        argv[start + len - 1] = temp;
    }
}

/*
 * Permute os->argv with the goal that non-option arguments will all
 * appear at the end.  os->skip_start is where we started skipping
 * non-option arguments, os->skip_end is where we stopped, and os->ind
 * is where we are now.
 */
static void _sg_permute(SG_getopt *os)
{
    int len1 = os->skip_end - os->skip_start;
    int len2 = os->ind - os->skip_end;

    if (os->interleave) {
        /*
         * Exchange the sequences argv[os->skip_start..os->skip_end-1] and
         * argv[os->skip_end..os->ind-1].  The easiest way to do that is
         * to reverse the entire range and then reverse the two
         * sub-ranges.
         */
        _sg_reverse(os->paszArgs, os->skip_start, len1 + len2);
        _sg_reverse(os->paszArgs, os->skip_start, len2);
        _sg_reverse(os->paszArgs, os->skip_start + len2, len1);
    }

    /* Reset skip range to the new location of the non-option sequence. */
    os->skip_start += len2;
    os->skip_end += len2;
}

void SG_getopt__long(SG_context* pCtx,
					SG_getopt* os,
                    const SG_getopt_option* opts,
                    SG_uint32* optch, const char** optarg,
					SG_bool* bNoMoreOptions)
{
    const char *p;
    SG_int32 i;

    /* Let the calling program reset option processing. */
    if (os->reset) {
        os->place = "";
        os->ind = 1;
        os->reset = 0;
    }

    /*
     * We can be in one of two states: in the middle of processing a
     * run of short options, or about to process a new argument.
     * Since the second case can lead to the first one, handle that
     * one first.  */
    p = os->place;
    if (*p == '\0') {
        /* If we are interleaving, skip non-option arguments. */
        if (os->interleave) {
            while (os->ind < os->count_args && *os->paszArgs[os->ind] != '-')
                os->ind++;
            os->skip_end = os->ind;
        }
        if (os->ind >= os->count_args || *os->paszArgs[os->ind] != '-') {
            os->ind = os->skip_start;
            //return APR_EOF;
			*bNoMoreOptions = SG_TRUE;
			SG_ERR_THROW_RETURN(  SG_ERR_GETOPT_NO_MORE_OPTIONS  );
        }

        p = os->paszArgs[os->ind++] + 1;
        if (*p == '-' && p[1] != '\0') {        /* Long option */
            /* Search for the long option name in the caller's table. */
            SG_uint32 len = 0;

            p++;
            for (i = 0; ; i++) {
                if (opts[i].optch == 0)             /* No match */
					SG_ERR_THROW2_RETURN(  SG_ERR_GETOPT_BAD_ARG, (pCtx, "Invalid option: %s", p - 2)  );
                    //return serr(os, "invalid option", p - 2, APR_BADCH);

                if (opts[i].pStringName) {
                    len = strlen(opts[i].pStringName);
                    if (strncmp(p, opts[i].pStringName, len) == 0
                        && (p[len] == '\0' || p[len] == '='))
                        break;
                }
            }
            *optch = opts[i].optch;

            if (opts[i].has_arg) {
                if (p[len] == '=')             /* Argument inline */
                    *optarg = p + len + 1;
                else { 
                    if (os->ind >= os->count_args)   /* Argument missing */
						SG_ERR_THROW2_RETURN(  SG_ERR_GETOPT_BAD_ARG, (pCtx, "Missing argument:  %s", p - 2)  );
                        //return serr(os, "missing argument", p - 2, APR_BADARG);
                    else                       /* Argument in next arg */
                        *optarg = os->paszArgs[os->ind++];
                }
            } else {
                *optarg = NULL;
                if (p[len] == '=')
					SG_ERR_THROW2_RETURN(  SG_ERR_GETOPT_BAD_ARG, (pCtx, "Erroneous argument:  %s", p - 2)  );
                    //return serr(os, "erroneous argument", p - 2, APR_BADARG);
            }
            _sg_permute(os);
            //return APR_SUCCESS;
			*bNoMoreOptions = SG_FALSE;
			return;
        } else {
            if (*p == '-') {                 /* Bare "--"; we're done */
                _sg_permute(os);
                os->ind = os->skip_start;
                //return APR_EOF;
				*bNoMoreOptions = SG_TRUE;
				SG_ERR_THROW_RETURN(SG_ERR_GETOPT_NO_MORE_OPTIONS);
            }
            else 
                if (*p == '\0')                    /* Bare "-" is illegal */
					SG_ERR_THROW2_RETURN(  SG_ERR_GETOPT_BAD_ARG, (pCtx, "Invalid option:  %s", p)  );
                    //return serr(os, "invalid option", p, APR_BADCH);
        }
    }

    /*
     * Now we're in a run of short options, and *p is the next one.
     * Look for it in the caller's table.
     */
    for (i = 0; ; i++) {
        if (opts[i].optch == 0)                     /* No match */
			SG_ERR_THROW2_RETURN(  SG_ERR_GETOPT_BAD_ARG, (pCtx, "Invalid option character:  %c", *p)  );
            //return cerr(os, "invalid option character", *p, APR_BADCH);

        if (*p == opts[i].optch)
            break;
    }
    *optch = *p++;

    if (opts[i].has_arg) {
        if (*p != '\0')                         /* Argument inline */
            *optarg = p;
        else { 
            if (os->ind >= os->count_args)           /* Argument missing */
				SG_ERR_THROW2_RETURN(  SG_ERR_GETOPT_BAD_ARG, (pCtx, "Missing argument:  %c", *optch)  );
                //return cerr(os, "missing argument", *optch, APR_BADARG);
            else                               /* Argument in next arg */
                *optarg = os->paszArgs[os->ind++];
        }
        os->place = "";
    } else {
        *optarg = NULL;
        os->place = p;
    }

    _sg_permute(os);
	*bNoMoreOptions = SG_FALSE;
    return;
}
// all the options should be parsed before this is called
void SG_getopt__parse_all_args(SG_context* pCtx, SG_getopt* os, const char*** ppaszArgs, SG_uint32* pCount_args)
{
	const char** psz = NULL;
	SG_uint32 i = 0;


	// skip over the first arg, it's the command name
	os->ind++;

	if (os->ind >= os->count_args)
    {
		*pCount_args = 0;			// no remaining args, don't bother
		*ppaszArgs = NULL;			// returning the array.
	}
	else
	{
		SG_ERR_CHECK(  SG_alloc(pCtx, os->count_args - os->ind, sizeof(char*), &psz)  );

		while (os->ind < os->count_args)
		{
			psz[i++] = os->paszArgs[os->ind++];
		}

		*pCount_args = i;
		*ppaszArgs = psz;
	}

	return;

fail:
	SG_NULLFREE(pCtx, psz);
}

void SG_getopt__print_option(SG_context* pCtx, SG_console_stream cs, SG_getopt_option* opt, const char * overrideDesc)
{
	SG_string *opts;

	if (opt == NULL)
    {
		SG_ERR_IGNORE(  SG_console(pCtx, cs, "?")  );
		return;
    }

	SG_ERR_CHECK(  SG_string__alloc(pCtx, &opts)  );

	/* We have a valid option which may or may not have a "short
		name" (a single-character alias for the long option). */
	if (opt->optch <= 255)
		SG_ERR_CHECK(  SG_string__sprintf(pCtx, opts, "-%c [--%s]", opt->optch, opt->pStringName)  );
	else
		SG_ERR_CHECK(  SG_string__sprintf(pCtx, opts, "--%s", opt->pStringName)  );

	if (opt->has_arg)
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, opts, " ARG")  );

	if (overrideDesc)
		SG_ERR_IGNORE(  SG_console(pCtx, cs, "%-20s :  %s\n", SG_string__sz(opts), overrideDesc)  );
	else
		SG_ERR_IGNORE(  SG_console(pCtx, cs, "%-20s :  %s\n", SG_string__sz(opts), opt->pStringDescription)  );


fail:
	SG_STRING_NULLFREE(pCtx, opts);
}

void SG_getopt__print_invalid_option(SG_context* pCtx, SG_int32 code, const SG_getopt_option* option_table)
{
	const SG_getopt_option* pOpt;

	SG_getopt__get_option_from_code(code, option_table, &pOpt);

	if (pOpt)
	{
		if (pOpt->optch <= 255)
			SG_console(pCtx, SG_CS_STDERR, "-%c [--%s]\n", pOpt->optch, pOpt->pStringName);
		else
			SG_console(pCtx, SG_CS_STDERR, "--%s\n", pOpt->pStringName);
	} 
	else
		SG_console(pCtx, SG_CS_STDERR, "-%c\n", code);
}

void SG_getopt__get_option_from_code2(SG_context* pCtx,
									 SG_int32 code,
									 const SG_getopt_option* option_table,
									 char* pszdesc_override,
									 const SG_getopt_option** ppOpt)
{
	SG_int32 i;

	for (i = 0; option_table[i].optch; i++)
		if (option_table[i].optch == code)
		{
			if (pszdesc_override)
			{
				SG_getopt_option *tmpopt;
				SG_ERR_CHECK(  SG_alloc1(pCtx, tmpopt)  );
				*tmpopt = option_table[i];
				tmpopt->pStringDescription = pszdesc_override;
				*ppOpt = tmpopt;
			}
			*ppOpt = &(option_table[i]);
		}

fail:
	return;
}



void SG_getopt__get_option_from_code(SG_int32 code,
									const SG_getopt_option *option_table,
									const SG_getopt_option ** ppOpt)
{
  SG_int32 i;

  for (i = 0; option_table[i].optch; i++)
  {
    if (option_table[i].optch == code)
	{
      *ppOpt = &(option_table[i]);
	  return;
	}
  }

  *ppOpt = NULL;
}
