/**
 * @copyright
 * Copyright (C) 2008, SourceGear LLC
 * @endcopyright
 *
 * @file sg_getopt.h
 *
 * Portions of this code is based upon the following Apache Portable Runtime source code:
 * http://www.apache.org/dist/apr/apr-1.3.8-win32-src.zip (apr_getopt.h)
 * and is covered by the Apache license given below.
 *
 * Other portions of this file are copyright SourceGear.
 */

/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_GETOPT_H
#define H_SG_GETOPT_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

#if defined(WINDOWS)
#define ARGV_CHAR_T			wchar_t
#define ARGV_LITERAL(sz)	(L##sz)
#else
#define ARGV_CHAR_T			char
#define ARGV_LITERAL(sz)	(sz)
#endif

//////////////////////////////////////////////////////////////////

/**
 * We convert the (argc,argv) given to us in {main,wmain} and
 * internalize them from whatever the platform gave us into nice
 * and neat and normalized UTF-8.
 *
 */
struct sg_getopt
{
	SG_string *			pStringAppName;
	SG_string *			pStringCommandName;

	SG_uint32			count_args;

	char **				paszArgs;

    /** index into parent argv vector */
    SG_uint32 ind;
    /** character checked for validity */
    SG_uint32 opt;
    /** reset getopt */
    SG_uint32 reset;

	/** argument associated with option */
    char const* place;
    /** set to nonzero to support interleaving options with regular args */
    SG_int32 interleave;
    /** start of non-option arguments skipped for interleaving */
    SG_uint32 skip_start;
    /** end of non-option arguments skipped for interleaving */
    SG_uint32 skip_end;
};

typedef struct sg_getopt SG_getopt;

struct sg_getopt_option
{
	/** long option name, or NULL if option has no long name */
    const char * pStringName;
    /** option letter, or a value greater than 255 if option has no letter */
    SG_int32 optch;
    /** nonzero if option takes an argument */
    SG_int32 has_arg;
    /** a description of the option */
    const char * pStringDescription;
};

typedef struct sg_getopt_option SG_getopt_option;

#define SG_GETOPT_NULLFREE(pCtx, pGetopt)             SG_STATEMENT( SG_context__push_level(pCtx); SG_getopt__free(pCtx, pGetopt); assert(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); pGetopt=NULL; )

//////////////////////////////////////////////////////////////////

void SG_getopt__alloc(SG_context * pCtx, int argc, ARGV_CHAR_T ** argv, SG_getopt ** ppGetopt);
void SG_getopt__free(SG_context * pCtx, SG_getopt * pGetopt);

//////////////////////////////////////////////////////////////////


void SG_getopt__set_app_name(SG_context * pCtx, SG_getopt * pGetopt, ARGV_CHAR_T * pBuf);
void SG_getopt__set_command_name(SG_context * pCtx, SG_getopt * pGetopt, ARGV_CHAR_T * pBuf);

const char * SG_getopt__get_app_name(const SG_getopt * pGetopt);
const char * SG_getopt__get_command_name(const SG_getopt * pGetopt);

//////////////////////////////////////////////////////////////////

void SG_getopt__long(SG_context* pCtx,
					SG_getopt* os,
                    const SG_getopt_option* opts,
                    SG_uint32* optch, const char** optarg,
					SG_bool* bNoMoreOptions);

void SG_getopt__parse_all_args(SG_context* pCtx,
							   SG_getopt* os,
							   const char*** paszArgs,
							   SG_uint32* pCount_args);

void SG_getopt__print_option(SG_context* pCtx, SG_console_stream cs, SG_getopt_option* opt, const char* overrideDesc);

void SG_getopt__print_invalid_option(SG_context* pCtx, SG_int32 code, const SG_getopt_option* option_table);

void SG_getopt__get_option_from_code2(SG_context* pCtx,
									 SG_int32 code,
									 const SG_getopt_option *option_table,
									 char* pszdesc_override,
									 const SG_getopt_option** ppOpt);

void SG_getopt__get_option_from_code(SG_int32 code,
									const SG_getopt_option* option_table,
									const SG_getopt_option** ppOpt);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_GETOPT_H