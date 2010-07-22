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

// sg_web_utils.h
// web utilities.
//////////////////////////////////////////////////////////////////

#ifndef H_SG_WEB_UTILS_H
#define H_SG_WEB_UTILS_H

BEGIN_EXTERN_C

/**
 *	encode a string for safe display in HTML
 */
void SG_htmlencode(
	SG_context *pCtx,
	SG_string *raw,			/**< plain text, can contain <, >, &, etc */
	SG_string *encoded);	/**< HTML-escapged version of 'raw'. must be pre-allocated */

/**
 *	get a lookup table of URI query string parameters
 */
void SG_querystring_to_vhash(
	SG_context *pCtx,
	const char *pQueryString,	/**< query string to parse. Must not be null */
	SG_vhash   *params);		/**< each parameter and its associated, decoded value */

END_EXTERN_C

#endif//H_SG_WEB_UTILS_H
