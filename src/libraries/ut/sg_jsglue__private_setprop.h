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
 * @file sg_jsglue__private_setprop.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_JSGLUE__PRIVATE_SETPROP_H
#define H_SG_JSGLUE__PRIVATE_SETPROP_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * define a string constant.
 */
static void sg_jsglue__define_property__sz(SG_context * pCtx,
										   JSContext * cx,
										   JSObject * pjsobj,
										   const char * pszKey,
										   const char * pszValue)
{
	JSString * pjsstr = NULL;
	jsval jv;

	SG_JS_BOOL_CHECK(  pjsstr = JS_NewStringCopyZ(cx, pszValue)  );
	jv     = STRING_TO_JSVAL(pjsstr);
	pjsstr = NULL;

	SG_JS_BOOL_CHECK(  JS_DefineProperty(cx, pjsobj,
										 pszKey, jv,
										 JS_PropertyStub, JS_PropertyStub,
										 JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT)  );
fail:
	return;
}

/**
 * define a boolean constant.
 */
static void sg_jsglue__define_property__bool(SG_context * pCtx,
											 JSContext * cx,
											 JSObject * pjsobj,
											 const char * pszKey,
											 SG_bool bValue)
{
	jsval jv;

	jv = BOOLEAN_TO_JSVAL(bValue);

	SG_JS_BOOL_CHECK(  JS_DefineProperty(cx, pjsobj,
										 pszKey, jv,
										 JS_PropertyStub, JS_PropertyStub,
										 JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT)  );
fail:
	return;
}

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_JSGLUE__PRIVATE_SETPROP_H
