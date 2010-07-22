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
 * @file sg_jsglue_prototypes.h
 *
 */

#ifndef H_SG_JSGLUE_PROTOTYPES_H
#define H_SG_JSGLUE_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

#define SG_OPT_NONRECURSIVE "nonrecursive";
#define SG_OPT_TEST "test";
#define SG_OPT_FORCE "force";
#define SG_OPT_STAMP "stamp";
#define SG_OPT_MESSGE "message";
#define SG_OPT_INCLUDE "include";
#define SG_OPT_EXCLUDE "exclude"

SG_context * SG_jsglue__get_clean_sg_context(JSContext * cx);

void SG_jsglue__set_sg_context(SG_context * pCtx, JSContext * cx);

JSBool SG_jsglue__context_callback(JSContext * cx, uintN contextOp);

void SG_jsglue__error_reporter(JSContext * cx, const char * pszMessage, JSErrorReport * pJSErrorReport);

void SG_jsglue__report_sg_error(SG_context * pCtx, JSContext * cx);

void SG_jsglue__install_scripting_api(SG_context * pCtx, JSContext *cx, JSObject *obj);

//////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////
// Utility functions used by methods and properties.
//These are also used by sg_zing_jsglue.
void sg_jsglue__jsobject_to_vhash(SG_context * pCtx, JSContext* cx, JSObject* obj, SG_vhash** ppvh);
void sg_jsglue__jsobject_to_varray(SG_context * pCtx, JSContext* cx, JSObject* obj, SG_varray** ppva);
void sg_jsglue__vhash_to_jsobject(SG_context * pCtx, JSContext* cx, SG_vhash* pvh, JSObject** pp);
void sg_jsglue__varray_to_jsobject(SG_context * pCtx, JSContext* cx, SG_varray* pva, JSObject** pp);
SG_bool sg_jsglue_get_equals_flag(const char* psz_flag, const char* psz_match, const char** psz_value);
void sg_jsglue_get_pattern_array(SG_context * pCtx, const char* psz_patterns, SG_stringarray** psaPatterns);


END_EXTERN_C;

#endif//H_SG_JSGLUE_PROTOTYPES_H

