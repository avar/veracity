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
 * @file sg_console_prototypes.h
 *
 * @details Routines for writing to the console.  This includes
 * stdout and stderr for command line applications.
 *
 * These routines take care of converting UTF-8 strings into
 * whatever locale/wchar_t versions are required for the platform.
 *
 * TODO Later we may want to expand it to include support for
 * a console window in a gui app.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_CONSOLE_PROTOTYPES_H
#define H_SG_CONSOLE_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_console__raw(SG_context * pCtx,
				SG_console_stream cs, const char * szRawString);
void SG_console(SG_context * pCtx,
				SG_console_stream cs, const char * szFormat, ...);
void SG_console__va(SG_context * pCtx,
					SG_console_stream cs, const char * szFormat, va_list ap);
void SG_console__flush(SG_context* pCtx, SG_console_stream cs);
void SG_console__read_stdin(SG_context* pCtx, SG_string** ppStrInput);
void SG_console__readline_stdin(SG_context* pCtx, SG_string** ppStrInput);

/**
 * Ask a Y/N question on the console.
 */
void SG_console__ask_question__yn(SG_context * pCtx,
								  const char * pszQuestion,
								  SG_console_qa qaDefault,
								  SG_bool * pbAnswer);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_CONSOLE_PROTOTYPES_H
