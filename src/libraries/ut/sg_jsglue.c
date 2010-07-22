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
#include <sgbrings/js/jsapi.h>
#include <sgbrings/js/jsdate.h>

#include "sg_js_safeptr__private.h"

// TODO temporary hack:
#ifdef WINDOWS
//#pragma warning(disable:4100)
#pragma warning(disable:4706)
#endif

#define BUFFER_SIZE					4096

/* TODO make sure we tell JS that strings are utf8 */

/*
 * Any object that requires a pointer (like an SG_repo)
 * is handled as a special class with private data
 * for storing the pointer (as an SG_safeptr).  See
 * sg_jsglue__get_object_private().
 *
 * Any object that can be serialized to a vhash is
 * simply handled as a plain JS object.
 */

/* Whenever we put a 64 bit integer into JSON, or into
 * a JSObject, if it won't fit into a 32 bit signed
 * int, we actually store it as a string instead of
 * as an int. */

//////////////////////////////////////////////////////////////////

/**
 * SG_JS_BOOL_CHECK() is used to wrap JS_ statements that fail by returning JS_FALSE.
 */
#define SG_JS_BOOL_CHECK(expr)	SG_STATEMENT(							\
	if (!(expr))														\
	{																	\
		SG_context__err(pCtx,SG_ERR_JS,__FILE__,__LINE__,#expr);		\
		goto fail;														\
	}																	)

/**
 * SG_JS_NULL_CHECK() is used to wrap JS_ statements that fail by returning NULL.
 */
#define SG_JS_NULL_CHECK(expr)	SG_STATEMENT(							\
	if (!(expr))														\
	{																	\
		SG_context__err(pCtx,SG_ERR_JS,__FILE__,__LINE__,#expr);		\
		goto fail;														\
	}																	)

//////////////////////////////////////////////////////////////////


SG_context * SG_jsglue__get_clean_sg_context(JSContext * cx)
{
	// TODO Do we want to use a SG_safeptr to wrap this?
	// TODO We have a bit of a chicken-n-egg problem.
	// TODO
	// TODO I don't think it is as much of an issue because
	// TODO we only have one JSContext.  Which is not like
	// TODO a JSObject where we have many.
	//
	// I'm going to say no for now.
	SG_context * ptr = NULL;
	ptr = (SG_context *)JS_GetContextPrivate(cx);
	SG_context__err_reset(ptr);
	return ptr;
}

SG_context * SG_jsglue__get_sg_context(JSContext * cx)
{
	// TODO Do we want to use a SG_safeptr to wrap this?
	// TODO We have a bit of a chicken-n-egg problem.
	// TODO
	// TODO I don't think it is as much of an issue because
	// TODO we only have one JSContext.  Which is not like
	// TODO a JSObject where we have many.
	//
	// I'm going to say no for now.
	SG_context * ptr = NULL;
	ptr = (SG_context *)JS_GetContextPrivate(cx);
	return ptr;
}
void SG_jsglue__set_sg_context(SG_context * pCtx, JSContext * cx)
{
	// remember pCtx in the cx so that it can be referenced
	// by methods that we export to JavaScript.
	JS_SetContextPrivate(cx, pCtx);
}

//////////////////////////////////////////////////////////////////

/**
 * This is a JSContextCallback.
 *
 * This gets called whenever a CX is created or destroyed.
 *
 * https://developer.mozilla.org/en/SpiderMonkey/JSAPI_Reference/JS_SetContextCallback
 */
JSBool SG_jsglue__context_callback(JSContext * cx, uintN contextOp)
{
	switch (contextOp)
	{
	case JSCONTEXT_NEW:
		JS_SetErrorReporter(cx,SG_jsglue__error_reporter);
		JS_SetVersion(cx, JSVERSION_LATEST);
		// TODO see SetContextOptions() in vscript.c for other goodies to set.
		return JS_TRUE;

	default:
	//case JSCONTEXT_DESTROY:
		return JS_TRUE;
	}
}

/**
 * This is a JSErrorReporter.
 *
 * This gets called whenever an error happens on this CX (either inside JavaScript proper
 * or within our glue code when SG_jsglue__report_sg_error() is called).
 */
void SG_jsglue__error_reporter(JSContext * cx, const char * pszMessage, JSErrorReport * pJSErrorReport)
{
	SG_context * pCtx = SG_jsglue__get_sg_context(cx);

	if (SG_context__has_err(pCtx))
	{
		// we had an error within the jsglue code in sglib (which has pushed
		// one or more sglib-stackframes) before calling SG_jsglue__report_sg_error()
		// at the end of the METHOD/PROPERY.
		//
		// So, the information in the JSErrorReport may or may not be of that
		// much value.
		//
		// We ignore the supplied pszMessage because it is just a copy of
		// the description that was provided when the original sglib-error
		// was thrown.
		//
		// TODO experiment with this and see if the JSErrorReport gives us any
		// TODO new information (like the location in the script) that we'll want.
		// TODO
		// TODO for now, just append a stackframe with the info and see if we
		// TODO get a duplicate entry....

		SG_context__err_stackframe_add(pCtx,
									   ((pJSErrorReport->filename) ? pJSErrorReport->filename : "<no_filename>"),
									   pJSErrorReport->lineno);
	}
	else
	{
		// something within JavaScript threw an error (and sglib was not involved).
		// start a new error in pCtx using all of the information in JSErrorReport.

		SG_context__err(pCtx, SG_ERR_JS,
						((pJSErrorReport->filename) ? pJSErrorReport->filename : "<no_filename>"),
						pJSErrorReport->lineno,
						pszMessage);
	}
}

/**
 * This should be called as the last step in the fail-block of ***all***
 * METHOD/PROPERTY functions.
 *
 * This is necessary so that JavaScript try/exception handling works
 * as expected.
 *
 * IT IS IMPORTANT THAT ALL CALLS TO SG_jsglue__report_sg_error() ***NOT*** BE
 * WRAPPED IN SG_ERR_IGNORE() (because it would hide the error from us).
 */
void SG_jsglue__report_sg_error(/*const*/ SG_context * pCtxConst, JSContext * cx)
{
	const char * pszDescription = NULL;
	SG_string * pStrContextDump = NULL;

	SG_ASSERT(  SG_context__has_err(pCtxConst)  );

	(void)SG_context__err_get_description(pCtxConst,&pszDescription);

	SG_context__err_to_string(pCtxConst,&pStrContextDump);

	JS_ReportError(cx, (  (pStrContextDump)
			? SG_string__sz(pStrContextDump)
			: pszDescription));
	SG_STRING_NULLFREE(pCtxConst, pStrContextDump);
}

//////////////////////////////////////////////////////////////////

/**
 * We cannot control the prototypes of methods and properties;
 * these are defined by SpiderMonkey.
 *
 * Note that they do not take a SG_Context.  Methods and properties
 * will have to extract it from the js-context-private data.
 */
#define SG_JSGLUE_METHOD_NAME(class, name) sg_jsglue__method__##class##__##name
#define SG_JSGLUE_METHOD_PROTOTYPE(class, name) static JSBool SG_JSGLUE_METHOD_NAME(class, name)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)

#define SG_JSGLUE_PROPERTY_NAME(class, name) sg_jsglue__property__##class##__##name
#define SG_JSGLUE_PROPERTY_PROTOTYPE(class, name) static JSBool SG_JSGLUE_PROPERTY_NAME(class, name)(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)

void sg_jsglue__variant_to_jsval(SG_context * pCtx, const SG_variant* pv, JSContext* cx, jsval* pjv);
/* ---------------------------------------------------------------- */
/* JSClass structures.  The InitClass calls are at the bottom of
 * the file */
/* ---------------------------------------------------------------- */

static JSClass sg_repo_class = {
    "sg_repo",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass sg_class = {
    "sg",
    0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass fs_class = {
    "sg_fs",
    0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass file_class = {
    "sg_file",
    0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass fsobj_properties_class = {
    "fsobj_properties",
    0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};
/* TODO need a class for sg_file */

static JSClass pending_tree_class = {
    "sg_pending_tree",
    0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/* ---------------------------------------------------------------- */
/* Utility functions */
/* ---------------------------------------------------------------- */


SG_safeptr* sg_jsglue__get_object_private(JSContext *cx, JSObject *obj)
{
    SG_safeptr* psp = (SG_safeptr*) JS_GetPrivate(cx, obj);
    return psp;
}

void sg_jsglue__set_hid_property(SG_context * pCtx, JSContext* cx, JSObject* jso, const char* psz_hid)
{
    jsval jv;
    JSString* pjstr = NULL;
    JSBool ok;
    uintN attrs = 0;;

    SG_JS_NULL_CHECK(  (pjstr = JS_NewStringCopyZ(cx, psz_hid))  );
    jv = STRING_TO_JSVAL(pjstr);
    pjstr = NULL;
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, jso, "__hid", &jv)  );
    SG_JS_BOOL_CHECK(  JS_GetPropertyAttributes(cx, jso, "__hid", &attrs, &ok)  );
    attrs |= JSPROP_READONLY;
    attrs &= (~JSPROP_ENUMERATE);
    SG_JS_BOOL_CHECK(  JS_SetPropertyAttributes(cx, jso, "__hid", attrs, &ok)  );

    return;

fail:
    return;
}

void sg_jsglue__stringarray_to_jsobject(SG_context * pCtx, JSContext* cx, SG_stringarray* psa, JSObject** pp)
{
    JSObject* obj = JS_NewArrayObject(cx, 0, NULL);
    SG_uint32 count = 0;
    SG_uint32 i;

    /* TODO do we need to JS_AddRoot on the obj? */

    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa, &count)  );

    for (i=0; i<count; i++)
    {
        const char * pszv = NULL;
        jsval jv;

        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa, i, &pszv)  );
        jv = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, pszv));

        SG_JS_BOOL_CHECK(  JS_SetElement(cx, obj, i, &jv)  );
    }

    *pp = obj;
    obj = NULL;

	return;

fail:
    /* TODO free obj */
    return;
}

void sg_jsglue__varray_to_jsobject(SG_context * pCtx, JSContext* cx, SG_varray* pva, JSObject** pp)
{
    JSObject* obj = JS_NewArrayObject(cx, 0, NULL);
    SG_uint32 count = 0;
    SG_uint32 i;

    /* TODO do we need to JS_AddRoot on the obj? */

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );

    for (i=0; i<count; i++)
    {
        const SG_variant* pv = NULL;
        jsval jv;

        SG_ERR_CHECK(  SG_varray__get__variant(pCtx, pva, i, &pv)  );
        SG_ERR_CHECK(  sg_jsglue__variant_to_jsval(pCtx, pv, cx, &jv)  );

        SG_JS_BOOL_CHECK(  JS_SetElement(cx, obj, i, &jv)  );
    }

    *pp = obj;
    obj = NULL;

	return;

fail:
    /* TODO free obj */
    return;
}

void sg_jsglue__int64_to_jsval(SG_context * pCtx, JSContext* cx, SG_int64 i, jsval* pjv)
{
    jsval jv = JSVAL_VOID;

    if (
            SG_int64__fits_in_int32(i)
            && INT_FITS_IN_JSVAL((SG_int32) i)
            )
    {
        jv = INT_TO_JSVAL((SG_int32) i);
    }
    else if (SG_int64__fits_in_double(i))
    {
        double d = (double) i;

        SG_JS_BOOL_CHECK(  JS_NewNumberValue(cx, d, &jv)  );
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_INTEGER_OVERFLOW  );
    }

    *pjv = jv;

    return;

fail:
    return;
}

void sg_jsglue__variant_to_jsval(SG_context * pCtx, const SG_variant* pv, JSContext* cx, jsval* pjv)
{
    jsval jv = JSVAL_VOID;
    JSString* pjs = NULL;
    JSObject* jso = NULL;

	switch (pv->type)
	{
	case SG_VARIANT_TYPE_DOUBLE:
        {
            double d;

            SG_ERR_CHECK(  SG_variant__get__double(pCtx, pv, &d)  );
            SG_JS_BOOL_CHECK(  JS_NewNumberValue(cx, d, &jv)  );
            break;
        }

	case SG_VARIANT_TYPE_INT64:
        {
            SG_int64 i = 0;

            SG_ERR_CHECK(  SG_variant__get__int64(pCtx, pv, &i)  );

            SG_ERR_CHECK(  sg_jsglue__int64_to_jsval(pCtx, cx, i, &jv)  );

            break;
        }

	case SG_VARIANT_TYPE_BOOL:
        {
            SG_bool b;
            int v;

            SG_ERR_CHECK(  SG_variant__get__bool(pCtx, pv, &b)  );

            /* BOOLEAN_TO_JSVAL is documented as accepting only 0 or 1, so
             * we make sure */
            v = b ? 1 : 0;
            jv = BOOLEAN_TO_JSVAL(v);
            break;
        }

	case SG_VARIANT_TYPE_NULL:
        {
            jv = JSVAL_NULL;
            break;
        }

	case SG_VARIANT_TYPE_SZ:
        {
            const char* psz = NULL;

            SG_ERR_CHECK(  SG_variant__get__sz(pCtx, pv, &psz)  );
            SG_JS_NULL_CHECK(  (pjs = JS_NewStringCopyZ(cx, psz))  );
            jv = STRING_TO_JSVAL(pjs);
            pjs = NULL;
            break;
        }

	case SG_VARIANT_TYPE_VHASH:
        {
            SG_vhash* pvh = NULL;

            SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &pvh)  );
            SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pvh, &jso)  );
            jv = OBJECT_TO_JSVAL(jso);
            jso = NULL;
            break;
        }

	case SG_VARIANT_TYPE_VARRAY:
        {
            SG_varray* pva = NULL;

            SG_ERR_CHECK(  SG_variant__get__varray(pCtx, pv, &pva)  );
            SG_ERR_CHECK(  sg_jsglue__varray_to_jsobject(pCtx, cx, pva, &jso)  );
            jv = OBJECT_TO_JSVAL(jso);
            jso = NULL;
            break;
        }
	}

    *pjv = jv;

    return;

fail:
    /* TODO free pjs */
    /* TODO free jso */
    return;
}

void sg_jsglue__vhash_to_jsobject(SG_context * pCtx, JSContext* cx, SG_vhash* pvh, JSObject** pp)
{
    JSObject* obj = NULL;
    SG_uint32 count = 0;
    SG_uint32 i;

	SG_JS_NULL_CHECK(  (obj = JS_NewObject(cx, NULL, NULL, NULL))  );
    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );

    for (i=0; i<count; i++)
    {
        const char* psz_name = NULL;
        const SG_variant* pv = NULL;
        jsval jv;

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh, i, &psz_name, &pv)  );
        SG_ERR_CHECK(  sg_jsglue__variant_to_jsval(pCtx, pv, cx, &jv)  );

        SG_JS_BOOL_CHECK(  JS_SetProperty(cx, obj, psz_name, &jv)  );
    }

    *pp = obj;
    obj = NULL;

	return;

fail:
    /* free obj */
    return;
}

void sg_jsglue__jsobject_to_varray(SG_context * pCtx, JSContext* cx, JSObject* obj, SG_varray** ppva)
{
    jsuint len = 0;
    SG_uint32 i = 0;
    SG_varray* pva = NULL;
    SG_varray* pva_sub = NULL;
    SG_vhash* pvh_sub = NULL;

    SG_JS_BOOL_CHECK(  JS_GetArrayLength(cx, obj, &len)  );

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );

    for (i=0; i<len; i++)
    {
        jsval jv_value;

        SG_JS_BOOL_CHECK(  JS_GetElement(cx, obj, i, &jv_value)  );

        if (JSVAL_IS_STRING(jv_value))
        {
            SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, JS_GetStringBytes(JSVAL_TO_STRING(jv_value)))  );
        }
        else if (JSVAL_IS_INT(jv_value))
        {
            SG_ERR_CHECK(  SG_varray__append__int64(pCtx, pva, (SG_int64) JSVAL_TO_INT(jv_value))  );
        }
        else if (
                JSVAL_IS_NUMBER(jv_value)
                || JSVAL_IS_DOUBLE(jv_value)
                )
        {
            jsdouble d = 0;
            SG_JS_BOOL_CHECK(  JS_ValueToNumber(cx, jv_value, &d)  );
            SG_ERR_CHECK(  SG_varray__append__double(pCtx, pva, (double) d)  );
        }
        else if (JSVAL_IS_BOOLEAN(jv_value))
        {
            SG_ERR_CHECK(  SG_varray__append__bool(pCtx, pva, JSVAL_TO_BOOLEAN(jv_value))  );
        }
        else if (JSVAL_IS_NULL(jv_value))
        {
            SG_ERR_CHECK(  SG_varray__append__null(pCtx, pva)  );
        }
        else if (JSVAL_IS_OBJECT(jv_value))
        {
            JSObject* po = JSVAL_TO_OBJECT(jv_value);

            if (JS_IsArrayObject(cx, po))
            {
                SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, po, &pva_sub)  );

                SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva, &pva_sub)  );
            }
            else
            {
                SG_ERR_CHECK(  sg_jsglue__jsobject_to_vhash(pCtx, cx, po, &pvh_sub)  );

                SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pva, &pvh_sub)  );
            }
        }
        else if (JSVAL_IS_VOID(jv_value))
        {
			SG_ERR_THROW( SG_ERR_JS );
        }
        else
        {
            SG_ERR_THROW( SG_ERR_JS );
        }
    }

    *ppva = pva;
    pva = NULL;

	return;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_sub);
    SG_VARRAY_NULLFREE(pCtx, pva_sub);
}

void sg_jsglue__jsobject_to_vhash(SG_context * pCtx, JSContext* cx, JSObject* obj, SG_vhash** ppvh)
{
    JSIdArray* properties = NULL;
    SG_vhash* pvh = NULL;
    SG_int32 i;
    SG_vhash* pvh_sub = NULL;
    SG_varray* pva_sub = NULL;

    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );

    SG_JS_BOOL_CHECK(  properties = JS_Enumerate(cx, obj)  );

    for (i=0; i<properties->length; i++)
    {
        jsval jv_name;
        jsval jv_value;
        JSString* str_name = NULL;

        SG_JS_BOOL_CHECK(  JS_IdToValue(cx, properties->vector[i], &jv_name)  );
        SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(jv_name)  );
        str_name = JSVAL_TO_STRING(jv_name);

        SG_JS_BOOL_CHECK(  JS_GetProperty(cx, obj, JS_GetStringBytes(str_name), &jv_value)  );

        if (JSVAL_IS_STRING(jv_value))
        {
            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, JS_GetStringBytes(str_name), JS_GetStringBytes(JSVAL_TO_STRING(jv_value)))  );
        }
        else if (JSVAL_IS_INT(jv_value))
        {
            SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, JS_GetStringBytes(str_name), (SG_int64) JSVAL_TO_INT(jv_value))  );
        }
        else if (
                JSVAL_IS_NUMBER(jv_value)
                || JSVAL_IS_DOUBLE(jv_value)
                )
        {
            jsdouble d = 0;
            SG_JS_BOOL_CHECK(  JS_ValueToNumber(cx, jv_value, &d)  );
            SG_ERR_CHECK(  SG_vhash__add__double(pCtx, pvh, JS_GetStringBytes(str_name), (double) d)  );
        }
        else if (JSVAL_IS_BOOLEAN(jv_value))
        {
            SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, pvh, JS_GetStringBytes(str_name), JSVAL_TO_BOOLEAN(jv_value))  );
        }
        else if (JSVAL_IS_NULL(jv_value))
        {
            SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pvh, JS_GetStringBytes(str_name))  );
        }
        else if (JSVAL_IS_OBJECT(jv_value))
        {
            JSObject* po = JSVAL_TO_OBJECT(jv_value);

            if (JS_IsArrayObject(cx, po))
            {
                SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, po, &pva_sub)  );
                SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh, JS_GetStringBytes(str_name), &pva_sub)  );

            }
            else
            {
                SG_ERR_CHECK(  sg_jsglue__jsobject_to_vhash(pCtx, cx, po, &pvh_sub)  );

                SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh, JS_GetStringBytes(str_name), &pvh_sub)  );
            }
        }
        else if (JSVAL_IS_VOID(jv_value))
        {
			SG_ERR_THROW( SG_ERR_JS );
        }
        else
        {
            SG_ERR_THROW( SG_ERR_JS );
        }
    }

    JS_DestroyIdArray(cx, properties);

    *ppvh = pvh;
    pvh = NULL;

	return;

fail:
    JS_DestroyIdArray(cx, properties);
    SG_VHASH_NULLFREE(pCtx, pvh);
    SG_VHASH_NULLFREE(pCtx, pvh_sub);
    SG_VARRAY_NULLFREE(pCtx, pva_sub);
}

SG_bool sg_jsglue_get_equals_flag(const char* psz_flag, const char* psz_match, const char** psz_value)
{
    int len = (int) strlen(psz_match);
    if (
            (0 == memcmp(psz_flag, psz_match, len))
            && ('=' == psz_flag[len])
        )
    {
        *psz_value = psz_flag + len + 1;
        return SG_TRUE;
    }

    return SG_FALSE;
}


void sg_jsglue_get_pattern_array(SG_context * pCtx, const char* psz_patterns, SG_stringarray** psaPatterns)
{
	char** pppResults = NULL;
	SG_uint32 i;
	SG_uint32 ncount = 0;


	// TODO 2010/06/16 From what I can tell by skimming, this code is intended to
	// TODO            allow the caller to receive "exclude=a,b,c" and we cut these
	// TODO            apart (breaking on the comma) into individual strings and
	// TODO            append them to the array (which we allocate if necessary).
	// TODO
	// TODO            Do we REALLY want to do this?
	// TODO
	// TODO            The callers are set up to allow an arg array containing
	// TODO            something like [ "exclude=a,b,c", "exclude=*.h", ...],
	// TODO            so they could get the same effect with the multiple n/v pairs.
	// TODO
	// TODO            Commas are valid characters in filenames and patterns, so
	// TODO            this prohibits us from, for example, excluding **,v" files.

	SG_ERR_CHECK (  SG_string__split__sz_asciichar(pCtx, psz_patterns, ',', strlen(psz_patterns) + 1, &pppResults, &ncount) );
	if (!(*psaPatterns))
	{
		SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, psaPatterns, ncount) );
	}
	for(i = 0; i < ncount; i++)
	{
		SG_ERR_CHECK (  SG_stringarray__add(pCtx, *psaPatterns, pppResults[i])  );
	}


fail:
	if (pppResults)
	{
		for(i = 0; i < ncount; i++)
			SG_NULLFREE(pCtx, pppResults[i]);
		SG_NULLFREE(pCtx, pppResults);
	}
}

static void sg_jsglue__get_repo_from_jsval(SG_context* pCtx, JSContext* cx, jsval arg, SG_repo** ppRepo)
{
	SG_vhash* pvh_descriptor = NULL;
	SG_repo* pRepo = NULL;

	if (JSVAL_IS_STRING(arg))
	{
		SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, JS_GetStringBytes(JSVAL_TO_STRING(arg)), &pRepo)  );
	}
	else if (JSVAL_IS_OBJECT(arg))
	{
		JSObject* po = JSVAL_TO_OBJECT(arg);
		SG_ERR_CHECK(  sg_jsglue__jsobject_to_vhash(pCtx, cx, po, &pvh_descriptor)  );
		SG_ERR_CHECK(  SG_repo__open_repo_instance__unnamed(pCtx, &pvh_descriptor, &pRepo)  );
	}
	else
	{
		// TODO what should the error code be?  all i found here was a goto fail.
		SG_ERR_THROW( SG_ERR_JS );
	}

	SG_RETURN_AND_NULL(pRepo, ppRepo);

	/* fall through */
fail:
	SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
	SG_REPO_NULLFREE(pCtx, pRepo);
}

static void sg_jsglue__get_repo_from_cwd(SG_context* pCtx, SG_repo** ppRepo, SG_pathname** ppPathCwd, SG_pathname** ppPathWorkingDirectoryTop)
{
	SG_pathname * pPathCwd = NULL;
	SG_repo * pRepo = NULL;
	SG_pathname * pPathWorkingDirectoryTop = NULL;
	SG_string * pstrRepoDescriptorName = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );
	SG_ERR_CHECK(  SG_workingdir__find_mapping(pCtx, pPathCwd, &pPathWorkingDirectoryTop, &pstrRepoDescriptorName, NULL)  );
	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, SG_string__sz(pstrRepoDescriptorName), &pRepo)  );

	SG_RETURN_AND_NULL(pPathCwd, ppPathCwd);
	SG_RETURN_AND_NULL(pPathWorkingDirectoryTop, ppPathWorkingDirectoryTop);
	SG_RETURN_AND_NULL(pRepo, ppRepo);

	/* fall through */
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDirectoryTop);
	SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
}

/* ---------------------------------------------------------------- */
/* JSNative method definitions */
/* ---------------------------------------------------------------- */

SG_JSGLUE_METHOD_PROTOTYPE(fs, length)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    const char* psz_arg = NULL;
    SG_pathname* pPath = NULL;
    SG_uint64 i;
    SG_fsobj_type t;
    jsval jv;

    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  1 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
    psz_arg = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg)  );

    SG_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath, &i, &t)  );

    SG_ERR_CHECK(  sg_jsglue__int64_to_jsval(pCtx, cx, i, &jv)  );

    /* TODO verify t */

    SG_PATHNAME_NULLFREE(pCtx, pPath);

    *rval = jv;

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(fs, mkdir)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    const char* psz_arg = NULL;
    SG_pathname* pPath = NULL;
    SG_bool b = SG_FALSE;

    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  1 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
    psz_arg = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg)  );

	//TODO:  This currently throws an error (stopping javascript execution)
    //if there's something there.  Should it just return false?

    SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath)  );
	b = SG_TRUE;
    SG_PATHNAME_NULLFREE(pCtx, pPath);

    *rval = BOOLEAN_TO_JSVAL(b);

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(fs, rmdir)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    const char* psz_arg = NULL;
    SG_pathname* pPath = NULL;
    SG_bool b = SG_FALSE;

    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  1 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
    psz_arg = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg)  );

	//TODO:  This currently throws an error (stopping javascript execution)
    //if there is not something there.  Should it just return false?

    SG_ERR_CHECK(  SG_fsobj__rmdir__pathname(pCtx, pPath)  );

	b = SG_TRUE;
    SG_PATHNAME_NULLFREE(pCtx, pPath);

    *rval = BOOLEAN_TO_JSVAL(b);

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(fs, mkdir_recursive)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    const char* psz_arg = NULL;
    SG_pathname* pPath = NULL;
    SG_bool b = SG_FALSE;

    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  1 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
    psz_arg = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg)  );

    //TODO:  This currently throws an error (stopping javascript execution)
    //if there's something there.  Should it just return false?

    SG_ERR_CHECK(  SG_fsobj__mkdir_recursive__pathname(pCtx, pPath)  );

    b = SG_TRUE;
    SG_PATHNAME_NULLFREE(pCtx, pPath);

    *rval = BOOLEAN_TO_JSVAL(b);

    return JS_TRUE;

fail:
    SG_jsglue__report_sg_error(pCtx, cx);
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(fs, rmdir_recursive)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    const char* psz_arg = NULL;
    SG_pathname* pPath = NULL;
    SG_bool b = SG_FALSE;

    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  1 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
    psz_arg = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg)  );
    //TODO:  This currently throws an error (stopping javascript execution)
    //if there is not something there.  Should it just return false?
    SG_ERR_CHECK(  SG_fsobj__rmdir_recursive__pathname(pCtx, pPath)  );
    b = SG_TRUE;
    SG_PATHNAME_NULLFREE(pCtx, pPath);

    *rval = BOOLEAN_TO_JSVAL(b);

    return JS_TRUE;

fail:
    SG_jsglue__report_sg_error(pCtx, cx);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(fs, exists)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    const char* psz_arg = NULL;
    SG_pathname* pPath = NULL;
    SG_bool b = SG_FALSE;

    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  1 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
    psz_arg = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    if (strlen(psz_arg) != 0)
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg)  );

    SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath, &b, NULL, NULL)  );

    SG_PATHNAME_NULLFREE(pCtx, pPath);

    *rval = BOOLEAN_TO_JSVAL(b);

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(fs, remove)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    const char* psz_arg = NULL;
    SG_pathname* pPath = NULL;

    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  1 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
    psz_arg = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg)  );

    SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath)  );

    SG_PATHNAME_NULLFREE(pCtx, pPath);

    *rval = JSVAL_VOID;

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(fs, move)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    const char* psz_arg1 = NULL;
    const char* psz_arg2 = NULL;
    SG_pathname* pPath = NULL;
    SG_pathname* pNewPath = NULL;

    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  2 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[1])  );
    psz_arg1 = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    psz_arg2 = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));

    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg1)  );
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pNewPath, psz_arg2)  );

    SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx, pPath, pNewPath)  );

    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_PATHNAME_NULLFREE(pCtx, pNewPath);

    *rval = JSVAL_TRUE;

    return JS_TRUE;

fail:
    *rval = JSVAL_FALSE;
    SG_jsglue__report_sg_error(pCtx, cx);
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_PATHNAME_NULLFREE(pCtx, pNewPath);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(fs, getcwd)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_string* pPath = NULL;
    JSString* pjs = NULL;

    SG_UNUSED(argv);
    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  0 == argc  );

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pPath)  );
    SG_ERR_CHECK(  SG_fsobj__getcwd(pCtx, pPath)  );

    SG_JS_NULL_CHECK(  (pjs = JS_NewStringCopyZ(cx, SG_string__sz(pPath)))  );
    SG_STRING_NULLFREE(pCtx, pPath);

    *rval = STRING_TO_JSVAL(pjs);

    return JS_TRUE;

fail:
    SG_jsglue__report_sg_error(pCtx, cx);
    SG_STRING_NULLFREE(pCtx, pPath);
    *rval = JSVAL_VOID;
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(fs, cd)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_pathname* pPath = NULL;
    JSString* pjs = NULL;
    const char* psz_arg = NULL;

    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  1 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );

	pjs = JSVAL_TO_STRING(argv[0]);

    psz_arg = JS_GetStringBytes(pjs);

    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg)  );
    SG_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPath)   );
    SG_PATHNAME_NULLFREE(pCtx, pPath);

    *rval = STRING_TO_JSVAL(pjs);

    return JS_TRUE;

fail:
    SG_jsglue__report_sg_error(pCtx, cx);
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    *rval = JSVAL_VOID;
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(fs, chmod)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    const char* psz_arg = NULL;
    SG_pathname* pPath = NULL;
    SG_bool b = SG_FALSE;
    SG_fsobj_perms perms = 0;

    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  2 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_INT(argv[1])  );
    psz_arg = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    perms = JSVAL_TO_INT(argv[1]);

    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg)  );

    //TODO:  This currently throws an error (stopping javascript execution)
    //if there's something there.  Should it just return false?

    SG_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pPath, perms )  );

    b = SG_TRUE;
    SG_PATHNAME_NULLFREE(pCtx, pPath);

    *rval = BOOLEAN_TO_JSVAL(b);

    return JS_TRUE;

fail:
    SG_jsglue__report_sg_error(pCtx, cx);
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(fs, stat)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    const char* psz_arg = NULL;
    SG_pathname* pPath = NULL;
    SG_fsobj_stat FsObjStat;
    int iIsFile;
    int iIsDir;
    int iIsSymlink;
    jsval perms;
    jsval modtime;
    jsval size;
    jsval dateval;
    jsval isFile;
    jsval isDirectory;
    jsval isSymlink;
    JSObject* object = NULL;
    JSObject* dateObject = NULL;
    jsval privaterval;

    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  1 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
    psz_arg = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg)  );

    SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath, &FsObjStat)  );

    object = JS_NewObject(cx, &fsobj_properties_class, NULL, NULL);

    //Permissions are in an int.
    perms = INT_TO_JSVAL(FsObjStat.perms);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, object, "permissions", &perms)  );

    //Create a Javascript Date object for the modtime.
    dateObject = JS_NewObject(cx, &js_DateClass, NULL, NULL);
    SG_ERR_CHECK(  sg_jsglue__int64_to_jsval(pCtx, cx, FsObjStat.mtime_ms, &modtime)   );
    SG_JS_BOOL_CHECK( JS_CallFunctionName(cx, dateObject, "setTime", 1, &modtime, &privaterval) );
    dateval = OBJECT_TO_JSVAL(dateObject);
	SG_JS_BOOL_CHECK(  JS_SetProperty(cx, object, "modtime", &dateval )  );

	SG_ERR_CHECK(  sg_jsglue__int64_to_jsval(pCtx, cx, FsObjStat.size, &size)   );
	SG_JS_BOOL_CHECK(  JS_SetProperty(cx, object, "size", &size)  );

	iIsFile = FsObjStat.type == SG_FSOBJ_TYPE__REGULAR;
	iIsDir = FsObjStat.type == SG_FSOBJ_TYPE__DIRECTORY;
	iIsSymlink = FsObjStat.type == SG_FSOBJ_TYPE__SYMLINK;
	isFile = BOOLEAN_TO_JSVAL(iIsFile);
	isDirectory = BOOLEAN_TO_JSVAL(iIsDir);
	isSymlink = BOOLEAN_TO_JSVAL(iIsSymlink);

	SG_JS_BOOL_CHECK(  JS_SetProperty(cx, object, "isFile", &isFile)  );
	SG_JS_BOOL_CHECK(  JS_SetProperty(cx, object, "isDirectory", &isDirectory)  );
	SG_JS_BOOL_CHECK(  JS_SetProperty(cx, object, "isSymlink", &isSymlink)  );

    SG_PATHNAME_NULLFREE(pCtx, pPath);

    *rval = OBJECT_TO_JSVAL(object);

    return JS_TRUE;

fail:
    SG_jsglue__report_sg_error(pCtx, cx);
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, to_json)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_vhash* pvh = NULL;
    SG_varray* pva = NULL;
    SG_string* pstr = NULL;
    JSString* pjs = NULL;
    JSObject* jso = NULL;

    SG_UNUSED(obj);

    SG_JS_BOOL_CHECK(  1 == argc  );

    SG_JS_BOOL_CHECK(  JSVAL_IS_OBJECT(argv[0])  );

    jso = JSVAL_TO_OBJECT(argv[0]);

    if (JS_IsArrayObject(cx, jso))
    {
        SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva)  );
        SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
        SG_ERR_CHECK(  SG_varray__to_json(pCtx, pva, pstr)  );
        SG_VARRAY_NULLFREE(pCtx, pva);
    }
    else
    {
        SG_ERR_CHECK(  sg_jsglue__jsobject_to_vhash(pCtx, cx, jso, &pvh)  );
        SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
        SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh, pstr)  );
        SG_VHASH_NULLFREE(pCtx, pvh);
    }

    SG_JS_NULL_CHECK(  (pjs = JS_NewStringCopyZ(cx, SG_string__sz(pstr)))  );
    SG_STRING_NULLFREE(pCtx, pstr);

    *rval = STRING_TO_JSVAL(pjs);
    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRING_NULLFREE(pCtx, pstr);
    return JS_FALSE;
}

//used by both exec() and read()
void _read_file_into_string(SG_context * pCtx, SG_file * pFile,
      SG_string * pString)
{
	SG_uint32 numRead;
	SG_byte buf[BUFFER_SIZE];

	while (1)
	{
		SG_file__read(pCtx, pFile, BUFFER_SIZE, buf, &numRead);
		if (SG_context__err_equals(pCtx, SG_ERR_EOF))
		{
			SG_context__err_reset(pCtx);
			break;
		}

		SG_ERR_CHECK_CURRENT;
		SG_ERR_CHECK( SG_string__append__buf_len(pCtx, pString, buf, numRead) );
	}
	return;
fail:
	SG_ERR_IGNORE(  SG_string__clear(pCtx, pString) );
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, exec)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	const char * pPathToExec = NULL;
	SG_exec_argvec * pArgVec = NULL;
	SG_exit_status pChildExitStatus = 0;
	SG_tempfile * pTempStdOut;
	SG_tempfile * pTempStdErr;
	SG_string * pStringErr = NULL;
	SG_string * pStringOut = NULL;
	uint i = 0;
	jsval exitstatus = JSVAL_VOID;
	jsval jsstderr = JSVAL_VOID;
	jsval jsstdout = JSVAL_VOID;
	JSObject* object = NULL;

    SG_UNUSED(obj);

	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
	pPathToExec = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

	SG_ERR_CHECK(  SG_exec_argvec__alloc(pCtx, &pArgVec)   );
	//allocate the new string array that we will send down.
	for (i = 1; i < argc; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, JS_GetStringBytes(JSVAL_TO_STRING(argv[i]))) );
	}

	//We need to create temp files to handle the stdout and stderr output.
	SG_ERR_CHECK( SG_tempfile__open_new(pCtx, &pTempStdOut)  );
	SG_ERR_CHECK( SG_tempfile__open_new(pCtx, &pTempStdErr)  );

	//Perform the exec, ignore any errors, so we can report the return status.
	SG_ERR_CHECK(  SG_exec__exec_sync__files(pCtx, pPathToExec, pArgVec, NULL, pTempStdOut->pFile, pTempStdErr->pFile, &pChildExitStatus)  );

	SG_ERR_CHECK( SG_file__fsync(pCtx, pTempStdOut->pFile)   );
	SG_ERR_CHECK( SG_file__fsync(pCtx, pTempStdErr->pFile)   );

	SG_ERR_CHECK( SG_file__seek(pCtx, pTempStdOut->pFile, 0)   );
	SG_ERR_CHECK( SG_file__seek(pCtx, pTempStdErr->pFile, 0)   );

	//Read the contents into some strings.
	SG_ERR_CHECK( SG_STRING__ALLOC(pCtx, &pStringErr) );
	SG_ERR_CHECK( SG_STRING__ALLOC(pCtx, &pStringOut) );
	_read_file_into_string(pCtx, pTempStdErr->pFile, pStringErr);
	_read_file_into_string(pCtx, pTempStdOut->pFile, pStringOut);

	SG_ERR_CHECK( SG_tempfile__close_and_delete(pCtx, &pTempStdErr) );
	SG_ERR_CHECK( SG_tempfile__close_and_delete(pCtx, &pTempStdOut) );


	//Bundle a Javascript object to return.
	//There are three properties.
	//exit_status [number]
	//stdout [string]
	//stderr [string]
	SG_JS_NULL_CHECK(  (object = JS_NewObject(cx, &fsobj_properties_class, NULL, NULL))  );

	//Permissions are in an int.
	exitstatus = INT_TO_JSVAL(pChildExitStatus);
	SG_JS_BOOL_CHECK( JS_SetProperty(cx, object, "exit_status", &exitstatus) );

	jsstderr = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, SG_string__sz(pStringErr)));
	SG_JS_BOOL_CHECK( JS_SetProperty(cx, object, "stderr", &jsstderr) );
	jsstdout = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, SG_string__sz(pStringOut)));
	SG_JS_BOOL_CHECK( JS_SetProperty(cx, object, "stdout", &jsstdout) );


	SG_STRING_NULLFREE(pCtx, pStringOut);
	SG_STRING_NULLFREE(pCtx, pStringErr);
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);
	*rval = OBJECT_TO_JSVAL(object);
	return JS_TRUE;
fail:
	SG_ERR_IGNORE( SG_tempfile__close_and_delete(pCtx, &pTempStdErr) );
	SG_ERR_IGNORE( SG_tempfile__close_and_delete(pCtx, &pTempStdOut) );
	SG_STRING_NULLFREE(pCtx, pStringOut);
	SG_STRING_NULLFREE(pCtx, pStringErr);
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, exec_nowait)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	const char * pPathToExec = NULL;
	SG_exec_argvec * pArgVec = NULL;
	SG_process_id processID = 0;
	SG_pathname * pPathNameForOutput = NULL;
	SG_pathname * pPathNameForError = NULL;
	SG_file * pFileForOutput = NULL;
	SG_file * pFileForError = NULL;
	uint i = 0;
	SG_uint32 firstArg = 0;
	JSObject* jso = NULL;
	SG_varray* pva_options = NULL;
	const char * pszVal = NULL;

    SG_UNUSED(obj);
    SG_UNUSED(rval);

	if (argc > 1)
	{
		jsval arg0 = argv[0];

		jso = JSVAL_TO_OBJECT(arg0);

		if (JS_IsArrayObject(cx, jso))
		{
			 SG_uint32 count = 0;

			 SG_uint32 j = 0;
			 const char* pszOption;

			 SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

			 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

			 for (j = 0; j < count; j++)
			 {

				 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

				 if ( sg_jsglue_get_equals_flag (pszOption, "output", &pszVal) )
				 {
					SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathNameForOutput, pszVal)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "error", &pszVal) )
				 {
					 SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathNameForError, pszVal)  );
				 }
			 }
			 firstArg++;
		}
	}

	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[firstArg])  );
	pPathToExec = JS_GetStringBytes(JSVAL_TO_STRING(argv[firstArg]));

	SG_ERR_CHECK(  SG_exec_argvec__alloc(pCtx, &pArgVec)   );
	//allocate the new string array that we will send down.
	for (i = firstArg + 1; i < argc; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, JS_GetStringBytes(JSVAL_TO_STRING(argv[i]))) );
	}

	//We need to create temp files to handle the stdout and stderr output.
	if (pPathNameForOutput != NULL)
	{
		SG_ERR_CHECK( SG_file__open__pathname(pCtx, pPathNameForOutput, SG_FILE_OPEN_OR_CREATE|SG_FILE_WRONLY,  0700, &pFileForOutput)   );
	}
	if (pPathNameForError != NULL)
	{
		SG_ERR_CHECK( SG_file__open__pathname(pCtx, pPathNameForError, SG_FILE_OPEN_OR_CREATE|SG_FILE_WRONLY,  0700, &pFileForError)   );
	}
	SG_ERR_CHECK(   SG_exec__exec_async__files(pCtx, pPathToExec, pArgVec, NULL, pFileForOutput, pFileForError, &processID)  );
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PATHNAME_NULLFREE(pCtx, pPathNameForOutput);
	SG_PATHNAME_NULLFREE(pCtx, pPathNameForError);
	//It's ok to close these, because the child process has its own open file descriptor
	SG_FILE_NULLCLOSE(pCtx, pFileForError);
	SG_FILE_NULLCLOSE(pCtx, pFileForOutput);
    *rval = INT_TO_JSVAL(processID);
	return JS_TRUE;
fail:
	//It's ok to close these, because the child process has its own open file descriptor
	SG_FILE_NULLCLOSE(pCtx, pFileForError);
	SG_FILE_NULLCLOSE(pCtx, pFileForOutput);
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PATHNAME_NULLFREE(pCtx, pPathNameForOutput);
	SG_PATHNAME_NULLFREE(pCtx, pPathNameForError);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, exec_pipeinput)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	const char * pPathToExec = NULL;
	SG_exec_argvec * pArgVec = NULL;
	SG_exit_status pChildExitStatus = 0;
	SG_tempfile * pTempStdOut;
	SG_tempfile * pTempStdErr;
	SG_file* pFileStdIn;
	SG_string * pStringErr = NULL;
	SG_string * pStringOut = NULL;
	uint i = 0;
	jsval exitstatus = JSVAL_VOID;
	jsval jsstderr = JSVAL_VOID;
	jsval jsstdout = JSVAL_VOID;
	JSObject* object = NULL;
	SG_pathname* pStdInPathname;

    SG_UNUSED(obj);

	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
	pPathToExec = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

	SG_ERR_CHECK(  SG_exec_argvec__alloc(pCtx, &pArgVec)   );
	//allocate the new string array that we will send down.
	for (i = 1; i < argc - 1; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec, JS_GetStringBytes(JSVAL_TO_STRING(argv[i]))) );
	}

	// assume the last param is the file to pipe
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[argc - 1])  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pStdInPathname, JS_GetStringBytes(JSVAL_TO_STRING(argv[argc -1])))  );

	//We need to create temp files to handle the stdout and stderr output.
	SG_ERR_CHECK(  SG_tempfile__open_new(pCtx, &pTempStdOut)  );
	SG_ERR_CHECK(  SG_tempfile__open_new(pCtx, &pTempStdErr)  );

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pStdInPathname, SG_FILE_RDONLY|SG_FILE_OPEN_EXISTING,0644, &pFileStdIn)  );

	//Perform the exec
	SG_ERR_CHECK( SG_exec__exec_sync__files(pCtx, pPathToExec, pArgVec, pFileStdIn, pTempStdOut->pFile, pTempStdErr->pFile, &pChildExitStatus) );

	SG_FILE_NULLCLOSE(pCtx, pFileStdIn);
	SG_PATHNAME_NULLFREE(pCtx, pStdInPathname);

	SG_ERR_CHECK( SG_file__fsync(pCtx, pTempStdOut->pFile)   );
	SG_ERR_CHECK( SG_file__fsync(pCtx, pTempStdErr->pFile)   );

	SG_ERR_CHECK( SG_file__seek(pCtx, pTempStdOut->pFile, 0)   );
	SG_ERR_CHECK( SG_file__seek(pCtx, pTempStdErr->pFile, 0)   );

	//Read the contents into some strings.
	SG_ERR_CHECK( SG_STRING__ALLOC(pCtx, &pStringErr) );
	SG_ERR_CHECK( SG_STRING__ALLOC(pCtx, &pStringOut) );
	_read_file_into_string(pCtx, pTempStdErr->pFile, pStringErr);
	_read_file_into_string(pCtx, pTempStdOut->pFile, pStringOut);

	SG_ERR_CHECK( SG_tempfile__close_and_delete(pCtx, &pTempStdErr) );
	SG_ERR_CHECK( SG_tempfile__close_and_delete(pCtx, &pTempStdOut) );


	//Bundle a Javascript object to return.
	//There are three properties.
	//exit_status [number]
	//stdout [string]
	//stderr [string]
	SG_JS_NULL_CHECK(  (object = JS_NewObject(cx, &fsobj_properties_class, NULL, NULL))  );

	//Permissions are in an int.
	exitstatus = INT_TO_JSVAL(pChildExitStatus);
	SG_JS_BOOL_CHECK( JS_SetProperty(cx, object, "exit_status", &exitstatus) );

	jsstderr = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, SG_string__sz(pStringErr)));
	SG_JS_BOOL_CHECK( JS_SetProperty(cx, object, "stderr", &jsstderr) );
	jsstdout = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, SG_string__sz(pStringOut)));
	SG_JS_BOOL_CHECK( JS_SetProperty(cx, object, "stdout", &jsstdout) );


	SG_STRING_NULLFREE(pCtx, pStringOut);
	SG_STRING_NULLFREE(pCtx, pStringErr);
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);
	*rval = OBJECT_TO_JSVAL(object);
	return JS_TRUE;
fail:
	SG_STRING_NULLFREE(pCtx, pStringOut);
	SG_STRING_NULLFREE(pCtx, pStringErr);
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, time)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_int64 t;

    SG_UNUSED(argc);
    SG_UNUSED(obj);
    SG_UNUSED(argv);

    // This function is here partly because JS new Date().getTime() is
    // supposed to do the exact same thing, but on Windows, it kind
    // of doesn't.  See
    //
    // https://bugzilla.mozilla.org/show_bug.cgi?id=363258

    SG_time__get_milliseconds_since_1970_utc(pCtx, &t);

    if (SG_int64__fits_in_double(t))
    {
        double d = (double) t;
        jsval jv;

        SG_JS_BOOL_CHECK(  JS_NewNumberValue(cx, d, &jv)  );
        *rval = jv;
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_INTEGER_OVERFLOW  );
    }

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, platform)
{
	SG_UNUSED(argc);
	SG_UNUSED(argv);
	SG_UNUSED(obj);

#if defined(LINUX)
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "LINUX"));
	return JS_TRUE;
#elif defined(MAC)
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "MAC"));
	return JS_TRUE;
#elif defined(WINDOWS)
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "WINDOWS"));
	return JS_TRUE;
#else
	return JS_FALSE;
#endif
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, gid)
{
	SG_context * pCtx = SG_jsglue__get_sg_context(cx);
    char buf[SG_GID_BUFFER_LENGTH];

    SG_UNUSED(obj);
    SG_UNUSED(argc);
    SG_UNUSED(argv);

    SG_ERR_CHECK(  SG_gid__generate(pCtx, buf, sizeof(buf))  );

    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buf));

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, set_local_setting)
{
	SG_context * pCtx = SG_jsglue__get_sg_context(cx);

	SG_UNUSED(obj);

	SG_JS_BOOL_CHECK(  2 == argc  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[1])  );

    SG_ERR_CHECK(  SG_localsettings__update__sz(
                pCtx,
                JS_GetStringBytes(JSVAL_TO_STRING(argv[0])),
                JS_GetStringBytes(JSVAL_TO_STRING(argv[1]))
                ) );

    // TODO do we want to return something here?
	*rval = JSVAL_NULL;

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, local_settings)
{
	SG_context * pCtx = SG_jsglue__get_sg_context(cx);
	SG_vhash * pvh = NULL;
	JSObject* jso = NULL;
	const char * argOne = NULL;
	const char * argTwo = NULL;
	SG_uint32 firstArg = 0;
	SG_bool bReset = SG_FALSE;
	SG_varray* pva_options = NULL;

	SG_UNUSED(obj);

    // TODO rework this.  it's a very un-javascript-like API
    // see set_local_setting above for a partial replacement

	if (argc > 0)
	{
		jsval arg0 = argv[0];

		jso = JSVAL_TO_OBJECT(arg0);

		if (JS_IsArrayObject(cx, jso))
		{
			 SG_uint32 count = 0;

			 SG_uint32 j = 0;
			 const char* pszOption;

			 SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

			 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

			 for (j = 0; j < count; j++)
			 {

				 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

				 if ( strcmp(pszOption, "reset")==0)
				 {
					bReset = SG_TRUE;
				 }
			 }
			 firstArg++;
		}
		if (argc >= 2)
		{
			argOne = JS_GetStringBytes(JSVAL_TO_STRING(argv[firstArg]));
			if (argc > (firstArg + 1))
			{
				if (JSVAL_IS_STRING(argv[firstArg + 1]))
				{
					argTwo = JS_GetStringBytes(JSVAL_TO_STRING(argv[firstArg + 1]));
					SG_ERR_CHECK(  SG_localsettings__update__sz(pCtx, argOne, argTwo) );
				}
				else if (JSVAL_IS_INT(argv[firstArg + 1]))
                {
                    SG_int_to_string_buffer sz_i;

                    SG_int64_to_sz(JSVAL_TO_INT(argv[firstArg + 1]), sz_i);
					SG_ERR_CHECK(  SG_localsettings__update__sz(pCtx, argOne, sz_i)  );
                }
			}
			else if (bReset)
			{
				SG_ERR_CHECK(  SG_localsettings__reset(pCtx, argOne)  );
			}
		}
	}

    SG_ERR_CHECK(  SG_localsettings__list__vhash(pCtx, &pvh)  );
    SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pvh, &jso)   );
    SG_VARRAY_NULLFREE(pCtx, pva_options);

	*rval = OBJECT_TO_JSVAL(jso);
	SG_VHASH_NULLFREE(pCtx, pvh);
    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, list_descriptors)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_vhash* pvh = NULL;
    JSObject* jso = NULL;

    SG_UNUSED(obj);
    SG_UNUSED(argc);
    SG_UNUSED(argv);

    SG_ERR_CHECK(  SG_closet__descriptors__list(pCtx, &pvh)  );
    SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pvh, &jso)  );
    SG_VHASH_NULLFREE(pCtx, pvh);
    *rval = OBJECT_TO_JSVAL(jso);
    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    SG_VHASH_NULLFREE(pCtx, pvh);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, new_repo)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pathname* pPathCwd = NULL;
	SG_changeset* pcsFirst = NULL;
	char* pszidGidActualRoot = NULL;
	const char* pszidFirstChangeset;
	const char * pszDescriptorName;
    char buf_admin_id[SG_GID_BUFFER_LENGTH];

    SG_UNUSED(obj);

    // TODO we don't know how to deal with admin ids yet.  but we need
    // one to create a repo.  so for now we just create a gid here.
    SG_ERR_CHECK(  SG_gid__generate(pCtx, buf_admin_id, sizeof(buf_admin_id))  );

	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
	pszDescriptorName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    SG_ERR_CHECK(  SG_repo__create__completely_new__closet(pCtx, buf_admin_id, NULL, pszDescriptorName, &pcsFirst, &pszidGidActualRoot)  );

	if (argc == 1 || (JSVAL_IS_BOOLEAN(argv[1]) && JSVAL_TO_BOOLEAN(argv[1]) == JS_TRUE))
	{
		SG_ERR_CHECK(  SG_changeset__get_id_ref(pCtx, pcsFirst, &pszidFirstChangeset)  );

		SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
		SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

		SG_ERR_CHECK(  SG_workingdir__create_and_get(pCtx, pszDescriptorName, pPathCwd, SG_TRUE, pszidFirstChangeset)  );
		SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	}
	SG_NULLFREE(pCtx, pszidGidActualRoot);
	SG_CHANGESET_NULLFREE(pCtx, pcsFirst);

    *rval = JSVAL_VOID;
    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_NULLFREE(pCtx, pszidGidActualRoot);
	SG_CHANGESET_NULLFREE(pCtx, pcsFirst);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, clone)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	const char* psz_existing = NULL;
	const char* psz_new = NULL;
	SG_client* pClient = NULL;

    SG_UNUSED(obj);
    SG_UNUSED(argc);
    
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
	psz_existing = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[1])  );
	psz_new = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));

    // Implementation copied from do_cmd_server in cli's sg.c.

    SG_ERR_CHECK(  SG_client__open(pCtx, psz_existing, NULL_CREDENTIAL, &pClient)  );

	SG_ERR_CHECK(  SG_repo__create_empty_clone_from_remote(pCtx, pClient, psz_new)  );

	SG_ERR_CHECK(  SG_pull__clone(pCtx, psz_new, pClient)  );

	// Save default push/pull location for the new repository instance.
	SG_ERR_CHECK(  SG_localsettings__descriptor__update__sz(pCtx, psz_new, "paths/default", psz_existing)  );

	SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "Use 'sg.checkout(\"%s\", <path>);' to get a working copy.\n", psz_new)  );

    SG_CLIENT_NULLFREE(pCtx, pClient);

    *rval = JSVAL_VOID;
    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	SG_CLIENT_NULLFREE(pCtx, pClient);
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, open_repo)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    JSObject* jso = NULL;
    SG_repo* pRepo = NULL;
    SG_safeptr* psp = NULL;

    SG_UNUSED(obj);
    SG_UNUSED(argc);

    jso = JS_NewObject(cx, &sg_repo_class, NULL, NULL);

	SG_ERR_CHECK(  sg_jsglue__get_repo_from_jsval(pCtx, cx, argv[0], &pRepo)  );

    SG_ERR_CHECK(  SG_safeptr__wrap__repo(pCtx, pRepo, &psp)  );
    JS_SetPrivate(cx, jso, psp);
    psp = NULL;

    *rval = OBJECT_TO_JSVAL(jso);
    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	// TODO do we need to free anything?
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(sg, repo_diag_blobs)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    JSObject* jso = NULL;
    SG_repo* pRepo = NULL;
    SG_vhash* pvh = NULL;

    SG_UNUSED(obj);
    SG_UNUSED(argc);

    jso = JS_NewObject(cx, &sg_repo_class, NULL, NULL);

	SG_ERR_CHECK(  sg_jsglue__get_repo_from_jsval(pCtx, cx, argv[0], &pRepo)  );

    SG_ERR_CHECK(  SG_repo__diag_blobs(pCtx, pRepo, &pvh)  );

    SG_REPO_NULLFREE(pCtx, pRepo);

    SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pvh, &jso)   );
    SG_VHASH_NULLFREE(pCtx, pvh);
    *rval = OBJECT_TO_JSVAL(jso);

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_VHASH_NULLFREE(pCtx, pvh);
    return JS_FALSE;
}

//SG_JSGLUE_METHOD_PROTOTYPE(sg, open_pending_tree)
//{
//	SG_context * pCtx = SG_jsglue__get_sg_context(cx);
//    JSObject* jso = NULL;
//    SG_safeptr* psp = NULL;
//    SG_pendingtree* pPendingTree = NULL;
//    SG_pathname* pPath = NULL;
//    SG_bool bIgnoreWarnings = SG_FALSE;
//
//    jso = JS_NewObject(cx, &pending_tree_class, NULL, NULL);
//
//    SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
//	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );
//    SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );
//
//    SG_PATHNAME_NULLFREE(pCtx, pPath);
//    SG_ERR_CHECK(  SG_safeptr__wrap__pending_tree(pCtx, pPendingTree, &psp)  );
//    JS_SetPrivate(cx, jso, psp);
//    psp = NULL;
//
//    *rval = OBJECT_TO_JSVAL(jso);
//    return JS_TRUE;
//
//fail:
//	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
//	// TODO do we need to free anything?
//    return JS_FALSE;
//}

//SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, close)
//{
//	SG_context * pCtx = SG_jsglue__get_sg_context(cx);
//    SG_pendingtree* pPendingTree = NULL;
//    SG_safeptr* psp = sg_jsglue__get_object_private(cx, obj);
//
//    SG_ERR_CHECK(  SG_safeptr__unwrap__pending_tree(pCtx, psp, &pPendingTree)  );
//    SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
//    SG_SAFEPTR_NULLFREE(pCtx, psp);
//    JS_SetPrivate(cx, obj, NULL);
//
//    *rval = JSVAL_VOID;
//    return JS_TRUE;
//
//fail:
//	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
//    /* TODO */
//    return JS_FALSE;
//}

/*  usage: sg.add([options],[files/folders])
 *
 *  possible options: nonrecursive, include=pattern, exclude=pattern, test
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, add)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pendingtree* pPendingTree = NULL;
	SG_pathname* pPath = NULL;
	const char** newArgv = NULL;
	uint i;
	uint n = 0;
	JSObject* jso = NULL;
	jsval arg0 = argv[0];
	SG_bool bIgnoreWarnings = SG_FALSE;
	SG_bool bRecursive = SG_TRUE;
	SG_bool bTestOnly = SG_FALSE;
	SG_uint32 nCountItems = 0;
	SG_varray* pva_options = NULL;
	SG_stringarray* includes = NULL;
	SG_stringarray* excludes = NULL;

	SG_UNUSED(obj);

	jso = JSVAL_TO_OBJECT(arg0);

	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &includes, 1) );
	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &excludes, 1) );

    if (JS_IsArrayObject(cx, jso))
    {
		 SG_uint32 count = 0;

		 SG_uint32 j = 0;
		 const char* pszOption;

         SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

		 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

		 for (j = 0; j < count; j++)
		 {
			 const char* pszVal = NULL;

			 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

			 if ( strcmp(pszOption, "nonrecursive")==0)
			 {
				bRecursive = SG_FALSE;
			 }
			 else if ( strcmp(pszOption, "test")==0)
			 {
				bTestOnly = SG_TRUE;
			 }
			 else if ( sg_jsglue_get_equals_flag (pszOption, "include", &pszVal) )
			 {
				 SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &includes)  );
			 }
			 else if ( sg_jsglue_get_equals_flag (pszOption, "exclude", &pszVal) )
			 {
				 SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &excludes)  );
			 }
			 else if ( sg_jsglue_get_equals_flag (pszOption, "include-from", &pszVal) )
			 {
				 SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &includes)  );
			 }
			 else if ( sg_jsglue_get_equals_flag (pszOption, "exclude-from", &pszVal) )
			 {
				 SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &excludes)  );
			 }
		 }

		 n++;
	}
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );

	//allocate the new string array that we will send down.
	SG_ERR_CHECK(  SG_alloc(pCtx, argc, sizeof(char*), &newArgv)  );

	for (i = n; i < argc; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		newArgv[nCountItems] = JS_GetStringBytes(JSVAL_TO_STRING(argv[i]));
		nCountItems++;
	}

    {
        const char * const * ppIncludes;
        SG_uint32 count_includes;
        const char * const * ppExcludes;
        SG_uint32 count_excludes;
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, includes, &ppIncludes, &count_includes)  );
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, excludes, &ppExcludes, &count_excludes)  );

	    SG_ERR_CHECK(  SG_pendingtree__add(
		    pCtx, pPendingTree,
		    pPath,
		    nCountItems,
		    newArgv,
		    bRecursive,
		    ppIncludes,
		    count_includes,
		    ppExcludes,
		    count_excludes,
		    bTestOnly)  );
    }

	SG_NULLFREE(pCtx, newArgv);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	*rval = JSVAL_VOID;
	return JS_TRUE;

fail:
	SG_NULLFREE(pCtx, newArgv);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}


/*  usage: sg.remove([options],[files/folders])
 *
 *  possible options: include=pattern, exclude=pattern, test
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, remove)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pendingtree* pPendingTree = NULL;
	SG_pathname* pPath = NULL;
	const char** newArgv = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	uint i;
	uint n = 0;
	JSObject* jso = NULL;
	jsval arg0 = argv[0];
	SG_bool bTestOnly = SG_FALSE;
	SG_uint32 nCountItems = 0;
	SG_varray* pva_options = NULL;
	SG_stringarray* includes = NULL;
	SG_stringarray* excludes = NULL;

	SG_UNUSED(obj);

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );
	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &includes, 1) );
	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &excludes, 1) );

	jso = JSVAL_TO_OBJECT(arg0);

    if (JS_IsArrayObject(cx, jso))
    {
		 SG_uint32 j = 0;
		 const char* pszOption;
		 SG_uint32 count;

         SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

		 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

		 for (j = 0; j < count; j++)
		 {
			 const char* pszVal = NULL;

			 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

			 if ( strcmp(pszOption, "test")==0)
			 {
				bTestOnly = SG_TRUE;
			 }

		 	 else if ( sg_jsglue_get_equals_flag (pszOption, "include", &pszVal) )
			 {
				 SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &includes)  );
			 }
			 else if ( sg_jsglue_get_equals_flag (pszOption, "exclude", &pszVal) )
			 {
				 SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &excludes)  );
			 }
			 else if ( sg_jsglue_get_equals_flag (pszOption, "include-from", &pszVal) )
			 {
				 SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &includes)  );
			 }
			 else if ( sg_jsglue_get_equals_flag (pszOption, "exclude-from", &pszVal) )
			 {
				 SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &excludes)  );
			 }

		 }
		 n++;

	}

	//allocate the new string array that we will send down.
	SG_ERR_CHECK(  SG_alloc(pCtx, argc, sizeof(char*), &newArgv)  );
	for (i = n; i < argc; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		newArgv[nCountItems] = JS_GetStringBytes(JSVAL_TO_STRING(argv[i]));
		nCountItems++;
	}

    {
        const char * const * ppIncludes;
        SG_uint32 count_includes;
        const char * const * ppExcludes;
        SG_uint32 count_excludes;
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, includes, &ppIncludes, &count_includes)  );
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, excludes, &ppExcludes, &count_excludes)  );

	    SG_ERR_CHECK(  SG_pendingtree__remove(pCtx,
											  pPendingTree,
											  pPath,
											  nCountItems, newArgv,
											  ppIncludes, count_includes,
											  ppExcludes, count_excludes,
											  SG_FALSE)  );
    }

	SG_NULLFREE(pCtx, newArgv);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	*rval = JSVAL_VOID;
	return JS_TRUE;

fail:
	SG_NULLFREE(pCtx, newArgv);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

/*  usage: sg.rename([options], file_or_folder, new_name)
 *
 *  possible options: force
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, rename)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pendingtree* pPendingTree = NULL;
	SG_pathname* pPath = NULL;
	SG_pathname* pPathToRename = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	const char* psz_arg1 = NULL;
	const char* psz_arg2 = NULL;
	uint n = 0;
	JSObject* jso = NULL;
	jsval arg0 = argv[0];
	SG_varray* pva_options = NULL;

	SG_UNUSED(obj);

	jso = JSVAL_TO_OBJECT(arg0);

    if (JS_IsArrayObject(cx, jso))
    {
		 SG_uint32 count = 0;
		 SG_uint32 j = 0;
		 const char* pszOption;

         SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

		 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

		 for (j = 0; j < count; j++)
		 {
			 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

			 if ( strcmp(pszOption, "force")==0)
			 {
				//TODO
			 }

		 }
		 n++;
	}

	SG_JS_BOOL_CHECK(  2 == (argc - n)  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[n])  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[n+1])  );
	psz_arg1 = JS_GetStringBytes(JSVAL_TO_STRING(argv[n]));
	psz_arg2 = JS_GetStringBytes(JSVAL_TO_STRING(argv[n+1]));

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathToRename, psz_arg1)  );

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );

	SG_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree, pPathToRename, psz_arg2, SG_FALSE)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_PATHNAME_NULLFREE(pCtx, pPathToRename);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	*rval = JSVAL_VOID;
	return JS_TRUE;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_PATHNAME_NULLFREE(pCtx, pPathToRename);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

/*  usage: sg.move([options], file_or_folder, new_destination)
 *
 *  possible options: force, include=pattern, exclude=pattern
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, move)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pendingtree* pPendingTree = NULL;
	SG_pathname* pPath = NULL;
	SG_pathname* pPathToMove = NULL;
	SG_pathname* pPathMoveToLocation = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	SG_pathname ** newArgv = NULL;
	SG_pathname * pPathnameTemp = NULL;
	const char* psz_arg2 = NULL;
	uint n = 0;
	uint i = 0;
	SG_uint32 nCountItems = 0;
	JSObject* jso = NULL;
	jsval arg0 = argv[0];
	SG_varray* pva_options = NULL;

	SG_UNUSED(obj);

	jso = JSVAL_TO_OBJECT(arg0);

    if (JS_IsArrayObject(cx, jso))
    {
		 SG_uint32 count = 0;
		 SG_uint32 j = 0;
		 const char* pszOption;

         SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

		 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

		 for (j = 0; j < count; j++)
		 {
			 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

			 if ( strcmp(pszOption, "force")==0)
			 {
				//TODO
			 }
		}
		n++;
	}

	SG_JS_BOOL_CHECK( (argc - n) >= 2  );


	//allocate the new string array that we will send down.
	SG_ERR_CHECK(  SG_alloc(pCtx, argc, sizeof(SG_pathname*), &newArgv)  );

	for (i = n; i < argc - 1; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		SG_ERR_CHECK( SG_PATHNAME__ALLOC__SZ(pCtx, &pPathnameTemp, JS_GetStringBytes(JSVAL_TO_STRING(argv[i])))  );
		newArgv[i] = pPathnameTemp;
		nCountItems++;
	}

	psz_arg2 = JS_GetStringBytes(JSVAL_TO_STRING(argv[argc - 1]));
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathMoveToLocation, psz_arg2)  );

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );

    SG_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree, (const SG_pathname **)newArgv, nCountItems, pPathMoveToLocation, SG_FALSE)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_PATHNAME_NULLFREE(pCtx, pPathToMove);
	SG_PATHNAME_NULLFREE(pCtx, pPathMoveToLocation);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	for (i = n; i < nCountItems; i++)
	{
		SG_PATHNAME_NULLFREE(pCtx, newArgv[i]);
	}
	SG_NULLFREE(pCtx, newArgv);
	*rval = JSVAL_VOID;
	return JS_TRUE;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_PATHNAME_NULLFREE(pCtx, pPathToMove);
	SG_PATHNAME_NULLFREE(pCtx, pPathMoveToLocation);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	for (i = n; i < nCountItems; i++)
	{
		SG_PATHNAME_NULLFREE(pCtx, newArgv[i]);
	}
	SG_NULLFREE(pCtx, newArgv);

	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

/*  usage: ???
 *
 *  possible options: ???
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, get)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pathname* pPath = NULL;
	const char* psz_arg1 = NULL;
	const char* psz_arg2 = NULL;
	uint n = 0;
	JSObject* jso = NULL;
	jsval arg0 = argv[0];
	SG_varray* pva_options = NULL;

	SG_UNUSED(obj);

	jso = JSVAL_TO_OBJECT(arg0);

    if (JS_IsArrayObject(cx, jso))
    {
		 SG_uint32 count = 0;
		 SG_uint32 j = 0;
		 const char* pszOption;

         SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

		 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

		 for (j = 0; j < count; j++)
		 {
			 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );
			 //TODO get options
		 }
		 n++;
	}
	SG_JS_BOOL_CHECK(  2 == (argc - n)  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[n+1])  );
	psz_arg1 = JS_GetStringBytes(JSVAL_TO_STRING(argv[n]));
	psz_arg2 = JS_GetStringBytes(JSVAL_TO_STRING(argv[n+1]));

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );

	SG_ERR_CHECK(  SG_workingdir__create_and_get(pCtx, psz_arg1, pPath, SG_TRUE, psz_arg2)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	*rval = JSVAL_VOID;
	return JS_TRUE;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}


/*  usage: ???
 *
 *  possible options: ????
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, scan)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pendingtree* pPendingTree = NULL;
	SG_pathname* pPath = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	JSObject* jso = NULL;
	SG_varray* pva_options = NULL;
	SG_stringarray* includes = NULL;
	SG_stringarray* excludes = NULL;

	SG_UNUSED(obj);

	if (argc > 0)
	{
		jsval arg0 = argv[0];

		jso = JSVAL_TO_OBJECT(arg0);

		if (JS_IsArrayObject(cx, jso))
		{
			 SG_uint32 count = 0;
			 SG_uint32 j = 0;
			 const char* pszOption;
			 const char* pszVal = NULL;

			 SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

			 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

			 for (j = 0; j < count; j++)
			 {
				 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );
				//TODO scan options
				if ( sg_jsglue_get_equals_flag (pszOption, "include", &pszVal) )
				{
					 SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &includes)  );
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "exclude", &pszVal) )
				{
					SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &excludes)  );
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "include-from", &pszVal) )
				{
					SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &includes)  );
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "exclude-from", &pszVal) )
				{
					SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &excludes)  );
				}
			 }
		}
	}
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );

    {
        const char * const * ppIncludes = NULL;
        SG_uint32 count_includes = 0;
        const char * const * ppExcludes = NULL;
        SG_uint32 count_excludes = 0;
        if (includes != NULL)
        	SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, includes, &ppIncludes, &count_includes)  );
        if (excludes != NULL)
        	SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, excludes, &ppExcludes, &count_excludes)  );

        SG_ERR_CHECK(  SG_pendingtree__clear_wd_timestamp_cache(pCtx, pPendingTree)  );		// flush the timestamp cache we loaded with the rest of wd.json

	    SG_ERR_CHECK(  SG_pendingtree__scan(pCtx, pPendingTree, SG_TRUE, ppIncludes,
		    count_includes,
		    ppExcludes,
		    count_excludes)  );
    }

	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_VARRAY_NULLFREE(pCtx, pva_options);

	*rval = JSVAL_VOID;
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);


	return JS_TRUE;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);

    return JS_FALSE;
}

/*  usage: sg.revert([options], [files,folders])
 *
 *  possible options: include=pattern, exclude=pattern, test, verbose
 *  TODO force, nobackup, ignore-portability-warnings
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, revert)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pendingtree* pPendingTree = NULL;
	SG_pathname* pPath = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	const char** newArgv = NULL;
	uint i;
	uint n = 0;
	JSObject* jso = NULL;
	JSObject* jsoReturn = NULL;
	SG_bool bTestOnly = SG_FALSE;
	SG_bool bVerbose = SG_FALSE;
	SG_uint32 nCountItems = 0;
	SG_varray* pva_options = NULL;
	SG_stringarray* includes = NULL;
	SG_stringarray* excludes = NULL;
	SG_pendingtree_action_log_enum eActionLog;
	SG_varray * pvaActionLog;

	SG_UNUSED(obj);

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );
	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &includes, 1) );
	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &excludes, 1) );

	if (argc > 0)
	{
		jsval arg0 = argv[0];
		jso = JSVAL_TO_OBJECT(arg0);

		if (JS_IsArrayObject(cx, jso))
		{
			 SG_uint32 count = 0;
			 SG_uint32 j = 0;
			 const char* pszOption;

			 SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

			 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

			 for (j = 0; j < count; j++)
			 {
				 const char* pszVal = NULL;
				 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

				 if ( strcmp(pszOption, "test")==0)
				 {
					bTestOnly = SG_TRUE;
				 }
				 else if ( strcmp(pszOption, "verbose") == 0 )
				 {
					 bVerbose = SG_TRUE;
				 }

				 // TODO else if "force"
				 // TODO else if "nobackup"
				 // TODO else if "ignore-portability-warnings"   -- TODO find shorter option name

				 else if ( sg_jsglue_get_equals_flag (pszOption, "include", &pszVal) )
				 {
					SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &includes)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "exclude", &pszVal) )
				 {
					SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &excludes)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "include", &pszVal) )
				 {
					SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &includes)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "exclude", &pszVal) )
				 {
					SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &excludes)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "include-from", &pszVal) )
				 {
					 SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &includes)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "exclude-from", &pszVal) )
				 {
					 SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &excludes)  );
				 }

			 }
			 n++;
		}
	}
	//allocate the new string array that we will send down.
	SG_ERR_CHECK(  SG_alloc(pCtx, argc, sizeof(char*), &newArgv)  );
	for (i = n; i < argc; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		newArgv[nCountItems] = JS_GetStringBytes(JSVAL_TO_STRING(argv[i]));
		nCountItems++;
	}

	if (bTestOnly)
	{
		eActionLog = SG_PT_ACTION__LOG_IT;
		// ignore --test and --verbose combination
	}
	else
	{
		eActionLog = SG_PT_ACTION__DO_IT;
		if (bVerbose)
			eActionLog |= SG_PT_ACTION__LOG_IT;
	}

    {
        const char * const * ppIncludes;
        SG_uint32 count_includes;
        const char * const * ppExcludes;
        SG_uint32 count_excludes;
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, includes, &ppIncludes, &count_includes)  );
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, excludes, &ppExcludes, &count_excludes)  );

	    SG_ERR_CHECK(  SG_pendingtree__revert(pCtx,
										      pPendingTree,
										      eActionLog,
										      pPath,
										      nCountItems, newArgv,
										      SG_TRUE,
										      ppIncludes, count_includes,
										      ppExcludes, count_excludes)  );
    }

	SG_ERR_CHECK(  SG_pendingtree__get_action_log(pCtx, pPendingTree, &pvaActionLog)  );
	if (pvaActionLog)
	{
		SG_ERR_CHECK(  sg_jsglue__varray_to_jsobject(pCtx, cx, pvaActionLog, &jsoReturn)  );
		*rval = OBJECT_TO_JSVAL(jsoReturn);
	}
	else
	{
		*rval = JSVAL_VOID;
	}

	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);

	SG_NULLFREE(pCtx, newArgv);
	return JS_TRUE;

fail:
	// TODO deal with portability warnings
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_NULLFREE(pCtx, newArgv);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}


/*  usage: sg.update([options])
 *
 *  possible options: test, verbose, rev, tag
 *  TODO force or clean, ignore-portability-warnings, nobackup
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, update)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pendingtree* pPendingTree = NULL;
	SG_pathname* pPath = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	uint n = 0;
	JSObject* jso = NULL;
	JSObject* jsoReturn = NULL;
	SG_bool bTestOnly = SG_FALSE;
	SG_bool bVerbose = SG_FALSE;
	SG_varray* pva_options = NULL;
	SG_pendingtree_action_log_enum eActionLog;
	SG_varray * pvaActionLog;
	SG_bool bForce = SG_FALSE;
	char * pszTargetChangeset = NULL;

	SG_UNUSED(obj);

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );

	if (argc > 0)
	{
		jsval arg0 = argv[0];
		jso = JSVAL_TO_OBJECT(arg0);

		if (JS_IsArrayObject(cx, jso))
		{
			 SG_uint32 count = 0;
			 SG_uint32 j = 0;
			 const char* pszOption;

			 SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

			 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

			 for (j = 0; j < count; j++)
			 {
				 const char* pszVal = NULL;
				 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

				 if ( strcmp(pszOption, "test")==0)
				 {
					bTestOnly = SG_TRUE;
				 }
				 else if ( strcmp(pszOption, "verbose") == 0 )
				 {
					 bVerbose = SG_TRUE;
				 }
				 else if ( strcmp(pszOption, "force") == 0 )
				 {
					 bForce = SG_TRUE;
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "rev", &pszVal) )
				{
					// TODO should this call the SG_repo__hidlookup code and allow
					// TODO HID prefixes to be used like on the command line?
					SG_ERR_CHECK(  SG_STRDUP(pCtx, pszVal, &pszTargetChangeset)  );
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "tag", &pszVal) )
				{
					char * psz_rev = NULL;
					SG_repo * pRepo = NULL;
					SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );
					SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, pszVal, &psz_rev)  );
					// TODO 4/1/10 what does __lookup__tag return if there is not EXACTLY 1 match?
					// TODO 4/1/10 this code would imply that it returns null rather than throwing.
					// TODO 4/1/10 shouldn't we throw here rather than leaving pszTargetChangeset
					// TODO 4/1/10 set to null and thus implying an UPDATE-to-HEAD?
					if (psz_rev != NULL)
						SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_rev, &pszTargetChangeset)  );
					SG_NULLFREE(pCtx, psz_rev);
				}
				 // TODO else if "force"
				 // TODO else if "nobackup"
				 // TODO else if "ignore-portability-warnings"   -- TODO find shorter option name

			 }
			 n++;
		}
	}

	if (bTestOnly)
	{
		eActionLog = SG_PT_ACTION__LOG_IT;
		// ignore --test and --verbose combination
	}
	else
	{
		eActionLog = SG_PT_ACTION__DO_IT;
		if (bVerbose)
			eActionLog |= SG_PT_ACTION__LOG_IT;
	}

	SG_ERR_CHECK(  SG_pendingtree__update_baseline(pCtx, pPendingTree, pszTargetChangeset,
													   bForce,
													   eActionLog)  );
	SG_ERR_CHECK(  SG_pendingtree__get_action_log(pCtx, pPendingTree, &pvaActionLog)  );
	if (pvaActionLog)
	{
		SG_ERR_CHECK(  sg_jsglue__varray_to_jsobject(pCtx, cx, pvaActionLog, &jsoReturn)  );
		*rval = OBJECT_TO_JSVAL(jsoReturn);
	}
	else
	{
		*rval = JSVAL_VOID;
	}

	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_NULLFREE(pCtx, pszTargetChangeset);

	return JS_TRUE;

fail:
	// TODO deal with portability warnings
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_NULLFREE(pCtx, pszTargetChangeset);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

/*  usage: sg.stamp([options])
 *
 *  possible options: stamp=STAMPTOAPPLY, rev=CSID, tag=TAG, remove
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, stamp)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_stringarray * pasz_stamps = NULL;
	SG_stringarray * pasz_revisions = NULL;
	SG_bool bRemove = SG_FALSE;
	JSObject* jso = NULL;
	SG_varray* pva_options = NULL;

	SG_repo * pRepo = NULL;

	SG_uint32 nRevisionCount = 0;
	SG_uint32 nStampsCount = 0;
	SG_uint32 nStampsIndex = 0;
	SG_uint32 nRevisionIndex = 0;
	SG_audit q;
	const char * psz_revision = NULL;
	const char * pszCurrentStamp = NULL;

	SG_UNUSED(obj);
	SG_UNUSED(rval);

	SG_ERR_CHECK(  sg_jsglue__get_repo_from_cwd(pCtx, &pRepo, NULL, NULL)  );

	SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &pasz_revisions, 1)  );
	SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &pasz_stamps, 1)  );
	if (argc > 0)
	{
		jsval arg0 = argv[0];
		jso = JSVAL_TO_OBJECT(arg0);

		if (JS_IsArrayObject(cx, jso))
		{
			 SG_uint32 count = 0;
			 SG_uint32 j = 0;
			 const char* pszOption;

			 SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

			 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

			 for (j = 0; j < count; j++)
			 {
				 const char* pszVal = NULL;
				 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

				 if ( strcmp(pszOption, "remove")==0)
				 {
					 bRemove = SG_TRUE;
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "stamp", &pszVal)  )
				 {
					 SG_ERR_CHECK(  SG_stringarray__add(pCtx, pasz_stamps, pszVal)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "rev", &pszVal) )
				{
					 char * psz_rev = NULL;
					 SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, pszVal, &psz_rev)  );
					if (psz_rev != NULL)
					{

						SG_ERR_CHECK(  SG_stringarray__add(pCtx, pasz_revisions, psz_rev)  );
					}
					SG_NULLFREE(pCtx, psz_rev);
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "tag", &pszVal) )
				{
					char * psz_rev = NULL;
					SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, pszVal, &psz_rev)  );
					if (psz_rev != NULL)
						SG_ERR_CHECK(  SG_stringarray__add(pCtx, pasz_revisions, psz_rev)  );
					else
					{
						SG_ERR_THROW2(  SG_ERR_TAG_NOT_FOUND,
														   (pCtx, "%s", pszVal )  );
					}
					SG_NULLFREE(pCtx, psz_rev);
				}
			 }
		}
	}

	SG_ERR_CHECK(  SG_stringarray__count(pCtx, pasz_revisions, &nRevisionCount)  );
	SG_ERR_CHECK(  SG_stringarray__count(pCtx, pasz_stamps, &nStampsCount)  );
	if (nStampsCount == 0 && !bRemove)
				SG_ERR_THROW2(  SG_ERR_INVALIDARG,
				                           (pCtx, "Must include at least one stamp.")  );

	if (nRevisionCount == 0)
		SG_ERR_THROW2(  SG_ERR_INVALIDARG,
								   (pCtx, "Must include at least one changeset using either rev or tag")  );

	SG_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW,SG_AUDIT__WHO__FROM_SETTINGS)  );

	for (nRevisionIndex = 0; nRevisionIndex < nRevisionCount; nRevisionIndex++)
	{
		SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pasz_revisions, nRevisionIndex, &psz_revision)  );

		if (psz_revision != NULL && !bRemove)
		{
			for (nStampsIndex = 0; nStampsIndex < nStampsCount; nStampsIndex++)
			{
				SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pasz_stamps, nStampsIndex, &pszCurrentStamp)  );
				SG_ERR_CHECK(  SG_vc_stamps__add(pCtx, pRepo, psz_revision, pszCurrentStamp, &q)  );
			}
		}
		else if (psz_revision != NULL && bRemove)
		{
			const char * const * pasz_char_stamps = NULL;
			if (nStampsCount > 0)
				SG_ERR_CHECK(  SG_stringarray__sz_array(pCtx, pasz_stamps, &pasz_char_stamps)  );
			//Remove all stamps from this revision.
			SG_ERR_CHECK(  SG_vc_stamps__remove(pCtx, pRepo, &q, (const char *)psz_revision, nStampsCount, pasz_char_stamps)  );
		}
	}

	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_STRINGARRAY_NULLFREE(pCtx, pasz_revisions);
	SG_STRINGARRAY_NULLFREE(pCtx, pasz_stamps);
	SG_REPO_NULLFREE(pCtx, pRepo);
	return JS_TRUE;

fail:
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_STRINGARRAY_NULLFREE(pCtx, pasz_revisions);
	SG_STRINGARRAY_NULLFREE(pCtx, pasz_stamps);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

/*  usage: sg.stamps()
 *
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, stamps)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_repo * pRepo = NULL;
	SG_varray* pVArray = NULL;
	JSObject* jso = NULL;

	SG_UNUSED(obj);
	SG_UNUSED(argc);
	SG_UNUSED(argv);

	SG_ERR_CHECK(  sg_jsglue__get_repo_from_cwd(pCtx, &pRepo, NULL, NULL)  );

	SG_ERR_CHECK(  SG_vc_stamps__list_all_stamps(pCtx, pRepo, &pVArray)  );
	SG_ERR_CHECK(  sg_jsglue__varray_to_jsobject(pCtx, cx, pVArray, &jso)   );
	*rval = OBJECT_TO_JSVAL(jso);

	SG_VARRAY_NULLFREE(pCtx, pVArray);
	SG_REPO_NULLFREE(pCtx, pRepo);
	return JS_TRUE;

fail:
	SG_VARRAY_NULLFREE(pCtx, pVArray);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	/* TODO */
	return JS_FALSE;
}

/*  usage: sg.tag([options])
 *
 *  possible options: tag=EXISTINGTAG, rev=CSID, tag=TAG, remove, force
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, tag)
{
        SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
        SG_bool bRemove = SG_FALSE;
        SG_bool bForce = SG_FALSE;
        JSObject* jso = NULL;
        SG_varray* pva_options = NULL;
        SG_pendingtree * pPendingTree = NULL;
		SG_repo * pRepo = NULL;
        SG_bool bRev = SG_FALSE;
        uint n = 0;
        char * pszSelectedCSID = NULL;
        uint i = 0;
        SG_uint32 nCountItems = 0;
        const char** newArgv = NULL;


        SG_UNUSED(obj);
        SG_UNUSED(rval);

		SG_ERR_CHECK(  sg_jsglue__get_repo_from_cwd(pCtx, &pRepo, NULL, NULL)  );

        if (argc > 0)
        {
                jsval arg0 = argv[0];
                jso = JSVAL_TO_OBJECT(arg0);

                if (JS_IsArrayObject(cx, jso))
                {
                         SG_uint32 count = 0;
                         SG_uint32 j = 0;
                         const char* pszOption;

                         SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

                         SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );


                         for (j = 0; j < count; j++)
                         {
                                 const char* pszVal = NULL;
                                 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

                                 if ( strcmp(pszOption, "remove")==0)
                                 {
                                         bRemove = SG_TRUE;
                                 }
                                 else if ( strcmp(pszOption, "force")==0)
                                 {
                                         bForce = SG_TRUE;
                                 }
                                 else if ( sg_jsglue_get_equals_flag (pszOption, "rev", &pszVal) )
                                {
                                	 bRev = SG_TRUE;
                                	 SG_ERR_CHECK(  SG_STRDUP(pCtx, pszVal, &pszSelectedCSID)  );
                                }
                                else if ( sg_jsglue_get_equals_flag (pszOption, "tag", &pszVal) )
                                {
                                	bRev = SG_FALSE;
                                    if (pszSelectedCSID != NULL)
                                    	 SG_ERR_THROW2(SG_ERR_USAGE, (pCtx, "A tag can only be applied to one changeset.")  );
                                   	SG_ERR_CHECK(  SG_STRDUP(pCtx, pszVal, &pszSelectedCSID)  );
                                }
                         }
                         n++;
                }
        }

        //allocate the new string array that we will send down.
		SG_ERR_CHECK(  SG_alloc(pCtx, argc, sizeof(char*), &newArgv)  );

		for (i = n; i < argc; i++)
		{
			SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
			newArgv[nCountItems] = JS_GetStringBytes(JSVAL_TO_STRING(argv[i]));
			nCountItems++;
		}

		if (bRemove)
		{
			SG_ERR_CHECK(  SG_tag__remove(pCtx, pRepo, pszSelectedCSID, bRev, nCountItems, newArgv)  );
		}
		else
		{
			SG_ERR_CHECK(  SG_tag__add_tags(pCtx, pRepo, NULL, pszSelectedCSID, bRev, bForce, newArgv, nCountItems)  );
		}

        SG_NULLFREE(pCtx, newArgv);
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
        SG_VARRAY_NULLFREE(pCtx, pva_options);
        SG_REPO_NULLFREE(pCtx, pRepo);
        SG_NULLFREE(pCtx, pszSelectedCSID);
        return JS_TRUE;

fail:
        SG_NULLFREE(pCtx, newArgv);
        SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
        SG_VARRAY_NULLFREE(pCtx, pva_options);
        SG_REPO_NULLFREE(pCtx, pRepo);
        SG_NULLFREE(pCtx, pszSelectedCSID);
        SG_jsglue__report_sg_error(pCtx,cx);    // DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}


/*  usage: sg.tags()
 *
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, tags)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_repo * pRepo = NULL;
	SG_varray* pVArray = NULL;
	JSObject* jso = NULL;
	const char * psz_tag = NULL;
	const char * psz_csid = NULL;
	SG_rbtree * prbtree = NULL;
	SG_rbtree_iterator * pIter = NULL;
	SG_bool b = SG_FALSE;

	SG_UNUSED(obj);
	SG_UNUSED(argc);
	SG_UNUSED(argv);

	SG_ERR_CHECK(  sg_jsglue__get_repo_from_cwd(pCtx, &pRepo, NULL, NULL)  );

	SG_ERR_CHECK(  SG_vc_tags__list(pCtx, pRepo, &prbtree)  );
	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pVArray)  );

	if (prbtree)
	{
		SG_vhash * pvh_current = NULL;
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter, prbtree, &b, &psz_tag, (void**)&psz_csid)  );

		while (b)
		{
			SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_current)  );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_current, "tag", psz_tag) );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_current, "csid", psz_csid) );
			SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pVArray, &pvh_current)  );
			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter, &b, &psz_tag, (void**)&psz_csid)  );
		}
		SG_rbtree__iterator__free(pCtx, pIter);
		pIter = NULL;
		SG_RBTREE_NULLFREE(pCtx, prbtree);
	}
	SG_ERR_CHECK(  sg_jsglue__varray_to_jsobject(pCtx, cx, pVArray, &jso)   );
	*rval = OBJECT_TO_JSVAL(jso);

	SG_RBTREE_NULLFREE(pCtx, prbtree);
	SG_VARRAY_NULLFREE(pCtx, pVArray);
	SG_REPO_NULLFREE(pCtx, pRepo);
	return JS_TRUE;

fail:
	SG_RBTREE_NULLFREE(pCtx, prbtree);
	SG_VARRAY_NULLFREE(pCtx, pVArray);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	/* TODO */
	return JS_FALSE;
}

/*  usage: sg.status([options], [files,folders])
 *
 *  possible options: nonrecursive, include=pattern, exclude=pattern
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, status)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pendingtree* pPendingTree = NULL;
	SG_pathname* pPath = NULL;
	SG_treediff2* pTreeDiff = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	SG_vhash* pVHash = NULL;
	const char** newArgv = NULL;
	uint i;
	JSObject* jso = NULL;
	uint n = 0;

	SG_bool bRecursive = SG_TRUE;
	SG_uint32 nCountItems = 0;
	SG_varray* pva_options = NULL;
	SG_stringarray* includes = NULL;
	SG_stringarray* excludes = NULL;
	char * psz_cset_1 = NULL;
	char * psz_cset_2 = NULL;
	SG_bool bIgnoreIgnores = SG_FALSE;

	SG_UNUSED(obj);

	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &includes, 1) );
	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &excludes, 1) );

	if (argc > 0)
	{
		jsval arg0 = argv[0];
		jso = JSVAL_TO_OBJECT(arg0);

		if (JS_IsArrayObject(cx, jso))
		{
			SG_uint32 count = 0;
			SG_uint32 j = 0;
			const char* pszOption;

			SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

			SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

			 for (j = 0; j < count; j++)
			 {
				const char* pszVal = NULL;

				SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

				if ( strcmp(pszOption, "nonrecursive")==0)
				{
					bRecursive = SG_FALSE;
				}
				else if ( strcmp(pszOption, "ignore-ignores")==0 )
				{
					bIgnoreIgnores = SG_TRUE;
				}
			    else if ( sg_jsglue_get_equals_flag (pszOption, "include", &pszVal) )
			    {
				   SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &includes)  );
			    }
			    else if ( sg_jsglue_get_equals_flag (pszOption, "exclude", &pszVal) )
			    {
				  SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &excludes)  );
			    }
				else if ( sg_jsglue_get_equals_flag (pszOption, "include-from", &pszVal) )
				{
					SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &includes)  );
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "exclude-from", &pszVal) )
				{
					SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &excludes)  );
				}
				else if ( sg_jsglue_get_equals_flag(pszOption, "rev", &pszVal) )
				{
					// TODO 2010/06/03 Should we allow rev prefixes like we do in the command line client?
					if (psz_cset_1 == NULL)
						SG_ERR_CHECK(  SG_STRDUP(pCtx, pszVal, &psz_cset_1)  );
					else if (psz_cset_2 == NULL)
						SG_ERR_CHECK(  SG_STRDUP(pCtx, pszVal, &psz_cset_2)  );
					else
					{
						// ignore 3rd and beyond...
					}
				}

				// TODO 2010/06/03 Should we allow "tag" like we do in the command line client?
			}
			n++;
		}
	}
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );
	SG_ERR_CHECK(  SG_pendingtree__diff_or_status(pCtx, pPendingTree, psz_cset_1, psz_cset_2, &pTreeDiff)  );

	//allocate the new string array that we will send down.
	SG_ERR_CHECK(  SG_alloc(pCtx, argc, sizeof(char*), &newArgv)  );
	for (i = n; i < argc; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		newArgv[nCountItems] = JS_GetStringBytes(JSVAL_TO_STRING(argv[i]));
		nCountItems++;
	}

    {
        const char * const * ppIncludes;
        SG_uint32 count_includes;
        const char * const * ppExcludes;
        SG_uint32 count_excludes;
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, includes, &ppIncludes, &count_includes)  );
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, excludes, &ppExcludes, &count_excludes)  );

      //  if ((nCountItems > 0) || (count_includes > 0) || (count_excludes > 0))
		    SG_ERR_CHECK(  SG_treediff2__file_spec_filter(pCtx, pTreeDiff,
													      nCountItems, newArgv, bRecursive,
													      ppIncludes, count_includes,
													      ppExcludes, count_excludes,
														bIgnoreIgnores)  );
    }

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pVHash)   );
	SG_ERR_CHECK(  SG_treediff2__report_status_to_vhash(pCtx, pTreeDiff, pVHash, SG_TRUE )   );

	SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pVHash, &jso)   );
	*rval = OBJECT_TO_JSVAL(jso);
	SG_VHASH_NULLFREE(pCtx, pVHash);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	SG_NULLFREE(pCtx, psz_cset_1);
	SG_NULLFREE(pCtx, psz_cset_2);
	SG_NULLFREE(pCtx, newArgv);
	return JS_TRUE;

fail:
	SG_VHASH_NULLFREE(pCtx, pVHash);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_NULLFREE(pCtx, newArgv);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_NULLFREE(pCtx, psz_cset_1);
	SG_NULLFREE(pCtx, psz_cset_2);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

/*  usage: sg.status([options], [files,folders])
 *
 *  possible options: nonrecursive, include=pattern, exclude=pattern
 *
 *  TODO 2010/06/03 this usage list is out of date.
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, status_string)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pendingtree* pPendingTree = NULL;
	SG_pathname* pPath = NULL;
	SG_treediff2* pTreeDiff = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	SG_string* pStrReport = NULL;
	const char** newArgv = NULL;
	uint i;
	uint n = 0;
	JSObject* jso = NULL;

	SG_bool bRecursive = SG_TRUE;
	SG_uint32 nCountItems = 0;
	SG_varray* pva_options = NULL;
	SG_stringarray* includes = NULL;
	SG_stringarray* excludes = NULL;
	char * psz_cset_1 = NULL;
	char * psz_cset_2 = NULL;
	SG_bool bIgnoreIgnores = SG_FALSE;

	SG_bool bHaveIssuesIn = SG_FALSE;
	const SG_varray * pvaIssuesIn = NULL;
	SG_vhash * pVhash = NULL;

	SG_UNUSED(obj);

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );

	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &includes, 1) );
	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &excludes, 1) );

	if (argc > 0)
	{
		jsval arg0 = argv[0];
		jso = JSVAL_TO_OBJECT(arg0);

		if (JS_IsArrayObject(cx, jso))
		{
			SG_uint32 count = 0;
			SG_uint32 j = 0;
			const char* pszOption;

			SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

			SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

			for (j = 0; j < count; j++)
			{
				const char* pszVal = NULL;
				SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

				if ( strcmp(pszOption, "nonrecursive")==0)
				{
					bRecursive = SG_FALSE;
				}
				else if ( strcmp(pszOption, "ignore-ignores")==0 )
				{
					bIgnoreIgnores = SG_TRUE;
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "include", &pszVal) )
				{
					SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &includes)  );
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "exclude", &pszVal) )
				{
					SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &excludes)  );
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "include-from", &pszVal) )
				{
					SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &includes)  );
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "exclude-from", &pszVal) )
				{
					SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &excludes)  );
				}
				else if ( sg_jsglue_get_equals_flag(pszOption, "rev", &pszVal) )
				{
					// TODO 2010/06/03 Should these be prefix-lookups like we do for the command line client?
					if (psz_cset_1 == NULL)
						SG_ERR_CHECK(  SG_STRDUP(pCtx, pszVal, &psz_cset_1)  );
					else if (psz_cset_2 == NULL)
						SG_ERR_CHECK(  SG_STRDUP(pCtx, pszVal, &psz_cset_2)  );
					else
					{
						// ignore 3rd and beyond...
					}
				}

				// TODO 2010/06/03 Should we allow "tag" options like we do for the command line client?
			}
			n++;
		}
	}

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );
	SG_ERR_CHECK(  SG_pendingtree__diff_or_status(pCtx, pPendingTree, psz_cset_1, psz_cset_2, &pTreeDiff)  );

	//allocate the new string array that we will send down.
	SG_ERR_CHECK(  SG_alloc(pCtx, argc, sizeof(char*), &newArgv)  );
	for (i = n; i < argc; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		newArgv[i] = JS_GetStringBytes(JSVAL_TO_STRING(argv[i]));
		nCountItems++;
	}

    {
        const char * const * ppIncludes;
        SG_uint32 count_includes;
        const char * const * ppExcludes;
        SG_uint32 count_excludes;
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, includes, &ppIncludes, &count_includes)  );
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, excludes, &ppExcludes, &count_excludes)  );

		SG_ERR_CHECK(  SG_treediff2__file_spec_filter(pCtx, pTreeDiff,
													  nCountItems, newArgv, bRecursive,
													  ppIncludes, count_includes,
													  ppExcludes, count_excludes,
													  bIgnoreIgnores)  );
    }

	if (psz_cset_1 == NULL && psz_cset_2 == NULL)
		SG_ERR_CHECK(  SG_pendingtree__get_wd_issues__ref(pCtx, pPendingTree, &bHaveIssuesIn, &pvaIssuesIn)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pVhash)  );
	SG_ERR_CHECK(  SG_treediff2__report_status_to_vhash(pCtx, pTreeDiff, pVhash, SG_TRUE)  );
	SG_ERR_CHECK(  SG_status_formatter__vhash_to_string(pCtx, pVhash, (SG_varray *)pvaIssuesIn, &pStrReport)  );
	//SG_ERR_CHECK(  SG_treediff2__report_status_to_string(pCtx, pTreeDiff, pStrReport, SG_TRUE )   );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, SG_string__sz(pStrReport)));

	SG_VHASH_NULLFREE(pCtx, pVhash);
	SG_STRING_NULLFREE(pCtx, pStrReport);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_NULLFREE(pCtx, psz_cset_1);
	SG_NULLFREE(pCtx, psz_cset_2);
	SG_NULLFREE(pCtx, newArgv);
	return JS_TRUE;

fail:
	SG_VHASH_NULLFREE(pCtx, pVhash);
	SG_STRING_NULLFREE(pCtx, pStrReport);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_NULLFREE(pCtx, newArgv);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_NULLFREE(pCtx, psz_cset_1);
	SG_NULLFREE(pCtx, psz_cset_2);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}
/*  usage: sg.history([options], [files,folders])
 *
 *  possible options: leaves, rev=CHANGESET, user=USERNAME, max=MAXRESULTS, from=MINIMUMDATE, to=MAXIMUMDATE
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, history)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_treediff2* pTreeDiff = NULL;
	SG_varray* pVArrayResults = NULL;
	const char** newArgv = NULL;
	uint i;
	JSObject* jso = NULL;
	JSObject* jsoReturn = NULL;
	uint n = 0;

	SG_bool bLeaves = SG_FALSE;
	SG_uint32 nCountItems = 0;
	SG_varray* pva_options = NULL;

	SG_int64 maxDate = SG_INT64_MAX;
	SG_int64 minDate = 0;
	SG_uint32 maxResults = SG_UINT32_MAX;
	SG_uint32 nChangesetCount = 0;
	const char * pszUser = NULL;
	SG_stringarray * psa_revs = NULL;
	const char * const* pasz_changesets = NULL;
	SG_repo * pRepo = NULL;

	SG_UNUSED(obj);

	SG_ERR_CHECK(  sg_jsglue__get_repo_from_cwd(pCtx, &pRepo, NULL, NULL)  );

	if (argc > 0)
	{
		jsval arg0 = argv[0];
		jso = JSVAL_TO_OBJECT(arg0);

		if (JS_IsArrayObject(cx, jso))
		{
			SG_uint32 count = 0;
			SG_uint32 j = 0;
			const char* pszOption;
			const char* pszVal;

			SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

			SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

			 for (j = 0; j < count; j++)
			 {
				SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

				if ( strcmp(pszOption, "leaves")==0)
				{
					bLeaves = SG_TRUE;
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "from", &pszVal) )
				{
					SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &minDate, pszVal)  );
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "to", &pszVal) )
				{
					SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &maxDate, pszVal)  );
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "rev", &pszVal) )
				{
					char * psz_rev = NULL;
					if (psa_revs == NULL)
						SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_revs, 1)  );
					SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, pszVal, &psz_rev)  );
					if (!psz_rev)
					{
						SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Could not locate changeset:  %s\n", pszVal)  );
					}
					else
						SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_revs, psz_rev)  );
					SG_NULLFREE(pCtx, psz_rev);
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "tag", &pszVal) )
				{
					char * psz_rev = NULL;
					if (psa_revs == NULL)
						SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_revs, 1)  );
					SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, pszVal, &psz_rev)  );
					if (psz_rev != NULL)
						SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_revs, psz_rev)  );
					SG_NULLFREE(pCtx, psz_rev);
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "max", &pszVal) )
				{
					SG_ERR_CHECK(  SG_uint32__parse(pCtx, &maxResults, pszVal)  );
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "user", &pszVal) )
				{
					pszUser = pszVal;
				}
			}
			n++;
		}
	}

	SG_ERR_CHECK(  SG_alloc(pCtx, argc, sizeof(char*), &newArgv)  );
	//allocate the new string array that we will send down.
	for (i = n; i < argc; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		newArgv[nCountItems] = JS_GetStringBytes(JSVAL_TO_STRING(argv[i]));
		nCountItems++;
	}
	if (psa_revs != NULL)
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, psa_revs, &pasz_changesets, &nChangesetCount)  );
	SG_ERR_CHECK(  SG_history__query(pCtx, NULL, pRepo, nCountItems, newArgv,
			pasz_changesets, nChangesetCount, pszUser, NULL,
			maxResults, minDate, maxDate, bLeaves, SG_FALSE, &pVArrayResults)  );

	if (pVArrayResults != NULL)
	{
#if 1 && defined(DEBUG)
		SG_ERR_IGNORE(  SG_varray_debug__dump_varray_of_vhashes_to_console(pCtx, pVArrayResults, "My History")  );
#endif
		SG_ERR_CHECK(  sg_jsglue__varray_to_jsobject(pCtx, cx, pVArrayResults, &jsoReturn)   );
		*rval = OBJECT_TO_JSVAL(jsoReturn);
	}
	else
		*rval = JSVAL_VOID;
	SG_VARRAY_NULLFREE(pCtx, pVArrayResults);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_STRINGARRAY_NULLFREE(pCtx, psa_revs);
	SG_NULLFREE(pCtx, newArgv);
	return JS_TRUE;

fail:
	SG_VARRAY_NULLFREE(pCtx, pVArrayResults);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_NULLFREE(pCtx, newArgv);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_STRINGARRAY_NULLFREE(pCtx, psa_revs);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, manually_set_parents)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pendingtree* pPendingTree = NULL;
	SG_varray * pVArrayParents = NULL;
	SG_uint32 i = 0;

	SG_UNUSED(obj);
	SG_UNUSED(rval);

	SG_JS_BOOL_CHECK( argc >= 1);
	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pVArrayParents)  );
	for (i = 0; i < argc; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pVArrayParents, JS_GetStringBytes(JSVAL_TO_STRING(argv[i])))  );
	}
	SG_ERR_CHECK(  SG_pendingtree__alloc_from_cwd(pCtx, SG_FALSE, &pPendingTree)  );
	SG_ERR_CHECK(  SG_pendingtree__set_wd_parents(pCtx,  pPendingTree, &pVArrayParents)  );
	pVArrayParents = NULL;
	SG_ERR_CHECK(  SG_pendingtree__save(pCtx, pPendingTree)  );
	//SG_VARRAY_NULLFREE(pCtx, pVArrayParents);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	return JS_TRUE;
fail:
	//SG_VARRAY_NULLFREE(pCtx, pVArrayParents);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, parents)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_bool bRev = SG_FALSE;
	char * psz_rev_or_tag = NULL;
	JSObject* jso = NULL;
	SG_varray * pva_options = NULL;
	SG_uint32 n = 0;
	SG_stringarray * pStringArrayResults = NULL;
	const char * pszFilePath = NULL;
	JSObject* jsoReturn = NULL;
	SG_repo * pRepo = NULL;

	SG_UNUSED(obj);

	SG_ERR_CHECK(  sg_jsglue__get_repo_from_cwd(pCtx, &pRepo, NULL, NULL)  );

	if (argc > 0)
	{
		jsval arg0 = argv[0];
		jso = JSVAL_TO_OBJECT(arg0);

		if (JS_IsArrayObject(cx, jso))
		{
			SG_uint32 count = 0;
			SG_uint32 j = 0;
			const char* pszOption;
			const char* pszVal;

			SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

			SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

			SG_JS_BOOL_CHECK(  count <= 1  );

			 for (j = 0; j < count; j++)
			 {
				SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

				if ( sg_jsglue_get_equals_flag (pszOption, "rev", &pszVal) )
				{
					bRev = SG_TRUE;
					SG_ERR_CHECK(  SG_STRDUP(pCtx, pszVal, &psz_rev_or_tag)  );
				}
				else if ( sg_jsglue_get_equals_flag (pszOption, "tag", &pszVal) )
				{
					bRev = SG_FALSE;
					SG_ERR_CHECK(  SG_STRDUP(pCtx, pszVal, &psz_rev_or_tag)  );
				}
			}
			n++;
		}
		if (argc > n)
		{
			SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[n])  );
			pszFilePath = JS_GetStringBytes(JSVAL_TO_STRING(argv[n]));
		}
	}

	SG_ERR_CHECK( SG_parents__list(pCtx, pRepo, NULL, psz_rev_or_tag, bRev, pszFilePath, &pStringArrayResults)  );

	if (pStringArrayResults != NULL)
	{
		SG_ERR_CHECK(  sg_jsglue__stringarray_to_jsobject(pCtx, cx, pStringArrayResults, &jsoReturn)  );
		*rval = OBJECT_TO_JSVAL(jsoReturn);
	}
	SG_STRINGARRAY_NULLFREE(pCtx, pStringArrayResults);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_NULLFREE(pCtx, psz_rev_or_tag);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	return JS_TRUE;
fail:
	SG_NULLFREE(pCtx, psz_rev_or_tag);
	SG_STRINGARRAY_NULLFREE(pCtx, pStringArrayResults);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, heads)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_varray* pVArrayResults = NULL;
	JSObject* jsoReturn = NULL;

	SG_rbtree * prbLeaves = NULL;
	SG_repo * pRepo = NULL;

	SG_UNUSED(obj);
	SG_UNUSED(argc);
	SG_UNUSED(argv);

	SG_ERR_CHECK(  sg_jsglue__get_repo_from_cwd(pCtx, &pRepo, NULL, NULL)  );

	SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx,pRepo,SG_DAGNUM__VERSION_CONTROL,&prbLeaves)  );

	SG_ERR_CHECK(  SG_rbtree__to_varray__keys_only(pCtx, prbLeaves, &pVArrayResults)  );

	if (pVArrayResults != NULL)
	{
		SG_ERR_CHECK(  sg_jsglue__varray_to_jsobject(pCtx, cx, pVArrayResults, &jsoReturn)   );
		*rval = OBJECT_TO_JSVAL(jsoReturn);
	}
	else
		*rval = JSVAL_VOID;
	SG_VARRAY_NULLFREE(pCtx, pVArrayResults);
	SG_RBTREE_NULLFREE(pCtx, prbLeaves);
	SG_REPO_NULLFREE(pCtx, pRepo);
	return JS_TRUE;

fail:
	SG_VARRAY_NULLFREE(pCtx, pVArrayResults);
	SG_RBTREE_NULLFREE(pCtx, prbLeaves);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

/*  usage: sg.addremove([options], path)
 *
 *  possible options: nonrecursive, test, include=pattern, exclude=pattern
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, addremove)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pendingtree* pPendingTree = NULL;
	SG_pathname* pPath = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	uint n = 0;
	JSObject* jso = NULL;
	const char** newArgv = NULL;
	SG_bool bRecursive = SG_TRUE;
	SG_bool bTestOnly = SG_FALSE;
	SG_varray* pva_options = NULL;
	SG_stringarray* includes = NULL;
	SG_stringarray* excludes = NULL;
	SG_uint32 i;
	SG_uint32 nCountItems = 0;

	SG_UNUSED(obj);

	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &includes, 1) );
	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &excludes, 1) );

	if (argc > 0)
	{
		jsval arg0 = argv[0];
		jso = JSVAL_TO_OBJECT(arg0);

		if (JS_IsArrayObject(cx, jso))
		{
			 SG_uint32 count = 0;
			 SG_uint32 j = 0;
			 const char* pszOption;

			 SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

			 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

			 for (j = 0; j < count; j++)
			 {
				 const char* pszVal = NULL;

				 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

				 if ( strcmp(pszOption, "nonrecursive")==0)
				 {
					bRecursive = SG_FALSE;
				 }
				 else if ( strcmp(pszOption, "test")==0)
				 {
					bTestOnly = SG_TRUE;
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "include", &pszVal) )
				 {
					 SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &includes)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "exclude", &pszVal) )
				 {
					 SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &excludes)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "include-from", &pszVal) )
				 {
					 SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &includes)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "exclude-from", &pszVal) )
				 {
					 SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &excludes)  );
				 }
			}
			n++;
		}
	}

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );

	//allocate the new string array that we will send down.
	SG_ERR_CHECK(  SG_alloc(pCtx, argc, sizeof(char*), &newArgv)  );

	for (i = n; i < argc; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		newArgv[nCountItems] = JS_GetStringBytes(JSVAL_TO_STRING(argv[i]));
		nCountItems++;
	}

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );

    {
        const char * const * ppIncludes;
        SG_uint32 count_includes;
        const char * const * ppExcludes;
        SG_uint32 count_excludes;
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, includes, &ppIncludes, &count_includes)  );
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, excludes, &ppExcludes, &count_excludes)  );

	    SG_ERR_CHECK(  SG_pendingtree__addremove(pCtx, pPendingTree, nCountItems,  newArgv, bRecursive,
		    ppIncludes,
		    count_includes,
		    ppExcludes,
		    count_excludes,
		    bTestOnly)  );
    }

	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	*rval = JSVAL_VOID;
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_NULLFREE(pCtx, newArgv);

	return JS_TRUE;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	SG_NULLFREE(pCtx, newArgv);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

/*  usage: sg.commit([options], [files/folders])
 *
 *  possible options: nonrecursive, test, include=pattern, exclude=pattern, message=comment, stamp=stamptext
 */
SG_JSGLUE_METHOD_PROTOTYPE(pending_tree, commit)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pendingtree* pPendingTree = NULL;
    SG_repo* pRepo = NULL;
	SG_pathname* pPath = NULL;
	const char** newArgv = NULL;
	uint i;
	SG_dagnode* pdn = NULL;
	SG_audit q;
	const char* psz_hid_cs = NULL;
	char* psz_comment = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	uint n = 0;
	JSObject* jso = NULL;
	SG_bool bRecursive = SG_TRUE;
	SG_bool bTestOnly = SG_FALSE;
	SG_uint32 nCountItems = 0;
	SG_varray* pva_options = NULL;
	SG_stringarray* includes = NULL;
	SG_stringarray* excludes = NULL;
	SG_stringarray* psa_stamps = NULL;
	SG_int64 nCommitDate = SG_AUDIT__WHEN__NOW;

	SG_UNUSED(obj);

	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &includes, 1) );
	SG_ERR_CHECK (  SG_STRINGARRAY__ALLOC(pCtx, &excludes, 1) );

	if (argc > 0)
	{
		jsval arg0 = argv[0];

		jso = JSVAL_TO_OBJECT(arg0);

		if (JS_IsArrayObject(cx, jso))
		{
			 SG_uint32 count = 0;
			 SG_uint32 j = 0;
			 const char* pszOption;

			 SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso, &pva_options)  );

			 SG_ERR_CHECK(  SG_varray__count(pCtx, pva_options, &count)  );

			 for (j = 0; j < count; j++)
			 {
				 const char* pszVal = NULL;

				 SG_ERR_CHECK( SG_varray__get__sz(pCtx, pva_options, j, &pszOption) );

				 if ( strcmp(pszOption, "nonrecursive")==0)
				 {
					bRecursive = SG_FALSE;
				 }
				 else if ( strcmp(pszOption, "test")==0)
				 {
					bTestOnly = SG_TRUE;
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "include", &pszVal) )
				 {
					 SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &includes)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "exclude", &pszVal) )
				 {
					 SG_ERR_CHECK (  sg_jsglue_get_pattern_array(pCtx, pszVal, &excludes)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "include-from", &pszVal) )
				 {
					 SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &includes)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "exclude-from", &pszVal) )
				 {
					 SG_ERR_CHECK (  SG_file_spec__read_patterns_from_file(pCtx, pszVal, &excludes)  );
				 }

				else if (  sg_jsglue_get_equals_flag (pszOption, "message", &pszVal) )
				 {
					SG_ERR_CHECK(  SG_STRDUP(pCtx, pszVal, &psz_comment)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "stamp", &pszVal) )
				 {
					if (psa_stamps == NULL)
						SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_stamps, 1)  );
					SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_stamps, pszVal)  );
				 }
				 else if ( sg_jsglue_get_equals_flag (pszOption, "date", &pszVal) )
				 {
					SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &nCommitDate, pszVal)  );
				 }
			 }

			 n++;

		}
	}

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );
    SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );
	SG_ERR_CHECK(  SG_audit__init(pCtx, &q, pRepo, nCommitDate, SG_AUDIT__WHO__FROM_SETTINGS)  );

    {
		// TODO 2010/06/05 BUG see sprawl-809.  we should have SG_pendingtree__commit()
		// TODO            do the pre-scan so that we get consistent behavior with vv.
		// TODO
		// TODO            We should be passing NULLs for includes/excludes.

        const char * const * ppIncludes;
        SG_uint32 count_includes;
        const char * const * ppExcludes;
        SG_uint32 count_excludes;
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, includes, &ppIncludes, &count_includes)  );
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, excludes, &ppExcludes, &count_excludes)  );

        SG_ERR_CHECK(  SG_pendingtree__scan(pCtx, pPendingTree, SG_TRUE,
		    ppIncludes,
		    count_includes,
		    ppExcludes,
		    count_excludes)  );
    }

	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	//allocate the new string array that we will send down.
	SG_ERR_CHECK(  SG_alloc(pCtx, argc, sizeof(char*), &newArgv)  );
	for (i = n; i < argc; i++)
	{
		SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[i])  );
		newArgv[nCountItems] = JS_GetStringBytes(JSVAL_TO_STRING(argv[i]));
		nCountItems++;
	}

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPath, bIgnoreWarnings, &pPendingTree)  );

    {
        const char * const * ppIncludes;
        SG_uint32 count_includes;
        const char * const * ppExcludes;
        SG_uint32 count_excludes;
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, includes, &ppIncludes, &count_includes)  );
        SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, excludes, &ppExcludes, &count_excludes)  );

	    SG_ERR_CHECK(  SG_pendingtree__commit(pCtx,
											  pPendingTree,
											  &q,
											  pPath,
											  nCountItems, newArgv,
											  bRecursive,
											  ppIncludes, count_includes,
											  ppExcludes, count_excludes,
											  NULL, 0, // TODO associations ?
											  &pdn)  );
    }

	if (pdn == NULL)
	{
		//The commit did not actually commit anything, but there wasn't an error.
		*rval = JSVAL_VOID;
	}
	else
	{
		SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pdn, &psz_hid_cs)  );
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, psz_hid_cs));
		if (psz_comment)
		{
			SG_repo* pRepo = NULL;
			SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );
			SG_ERR_CHECK(  SG_vc_comments__add(pCtx, pRepo, psz_hid_cs, psz_comment, &q)  );
		}
		if (psa_stamps)
		{
			SG_uint32 iCount = 0;
			SG_uint32 iCountStamps = 0;
			const char* pszstamp;
			SG_repo* pRepo = NULL;
			SG_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );
			SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_stamps, &iCountStamps )  );
			while (iCount < iCountStamps)
			{
				SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa_stamps, iCount, &pszstamp)  );
				SG_ERR_CHECK(  SG_vc_stamps__add(pCtx, pRepo, psz_hid_cs, pszstamp, &q)  );
				iCount++;
			}

		}
	}

	SG_NULLFREE(pCtx, psz_comment);
	SG_NULLFREE(pCtx, newArgv);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_STRINGARRAY_NULLFREE(pCtx, psa_stamps);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	SG_DAGNODE_NULLFREE(pCtx, pdn);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	return JS_TRUE;

fail:
	SG_NULLFREE(pCtx, psz_comment);
	SG_NULLFREE(pCtx, newArgv);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_DAGNODE_NULLFREE(pCtx, pdn);
	SG_VARRAY_NULLFREE(pCtx, pva_options);
	SG_STRINGARRAY_NULLFREE(pCtx, includes);
	SG_STRINGARRAY_NULLFREE(pCtx, psa_stamps);
	SG_STRINGARRAY_NULLFREE(pCtx, excludes);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(file, write)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pathname* pPath = NULL;
	SG_file* pFile = NULL;
	const char* psz_arg1 = NULL;
	const char* psz_arg2 = NULL;

	SG_UNUSED(obj);

	SG_JS_BOOL_CHECK(  2 == argc  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[1])  );
	psz_arg1 = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	psz_arg2 = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg1)  );

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_OPEN_OR_CREATE|SG_FILE_WRONLY, 0600, &pFile)   );

	SG_ERR_CHECK(  SG_file__truncate(pCtx, pFile)   );
	SG_ERR_CHECK(  SG_file__write__sz(pCtx, pFile, psz_arg2)  );
	SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)   );

	SG_PATHNAME_NULLFREE(pCtx, pPath);

	*rval = JSVAL_VOID;
	return JS_TRUE;

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(file, append)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pathname* pPath = NULL;
	SG_file* pFile = NULL;
	const char* psz_arg1 = NULL;
	const char* psz_arg2 = NULL;

	SG_UNUSED(obj);

	SG_JS_BOOL_CHECK(  2 == argc  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[1])  );
	psz_arg1 = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	psz_arg2 = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg1)  );

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_OPEN_OR_CREATE|SG_FILE_WRONLY,  0700, &pFile)   );
	SG_ERR_CHECK(  SG_file__seek_end(pCtx, pFile, NULL)   );
	SG_ERR_CHECK(  SG_file__write__sz(pCtx, pFile, psz_arg2)   );
	SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)   );

	SG_PATHNAME_NULLFREE(pCtx, pPath);

	*rval = JSVAL_VOID;
	return JS_TRUE;

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	/* TODO */
	return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(file, read)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_pathname* pPath = NULL;
	SG_file* pFile = NULL;
	const char* psz_arg1 = NULL;
	SG_uint32 numRead;
	SG_byte buf[BUFFER_SIZE];
	SG_string * pString = NULL;

	SG_UNUSED(obj);

	SG_JS_BOOL_CHECK(  1 == argc  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
	psz_arg1 = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, psz_arg1)  );

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_OPEN_OR_CREATE|SG_FILE_RDONLY,  0700, &pFile)   );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)   );
	while(1)
	{
		SG_file__read(pCtx, pFile, BUFFER_SIZE, buf, &numRead);
		if (SG_context__err_equals(pCtx, SG_ERR_EOF))
		{
			SG_context__err_reset(pCtx);
			break;
		}

		SG_ERR_CHECK_CURRENT;
		SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pString, buf, numRead)   );
	}
	SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)   );

	SG_PATHNAME_NULLFREE(pCtx, pPath);

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, SG_string__sz(pString)));
	SG_STRING_NULLFREE(pCtx, pString);
	return JS_TRUE;

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_STRING_NULLFREE(pCtx, pString);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	/* TODO */
	return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(repo, close)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_repo* pPendingTree = NULL;
    SG_safeptr* psp = sg_jsglue__get_object_private(cx, obj);

    SG_UNUSED(argc);
    SG_UNUSED(argv);

    SG_ERR_CHECK(  SG_safeptr__unwrap__repo(pCtx, psp, &pPendingTree)  );
    SG_REPO_NULLFREE(pCtx, pPendingTree);
    SG_SAFEPTR_NULLFREE(pCtx, psp);
    JS_SetPrivate(cx, obj, NULL);

    *rval = JSVAL_VOID;
    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(repo, fetch_json)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_repo* pRepo = NULL;
    SG_safeptr* psp = NULL;
    SG_vhash* pvh = NULL;
    JSObject* jso = NULL;

    SG_UNUSED(argc);

    psp = sg_jsglue__get_object_private(cx, obj);
    SG_ERR_CHECK(  SG_safeptr__unwrap__repo(pCtx, psp, &pRepo)  );

    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );

    SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, pRepo, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &pvh)  );

    SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pvh, &jso)  );

    SG_VHASH_NULLFREE(pCtx, pvh);

    *rval = OBJECT_TO_JSVAL(jso);
    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(repo, fetch_dagnode)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_repo* pRepo = NULL;
    SG_safeptr* psp = NULL;
    SG_dagnode* pdn = NULL;
    SG_vhash* pvh = NULL;
    JSObject* jso = NULL;

    SG_UNUSED(argc);

    psp = sg_jsglue__get_object_private(cx, obj);
    SG_ERR_CHECK(  SG_safeptr__unwrap__repo(pCtx, psp, &pRepo)  );

    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );

    SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &pdn)  );

    SG_ERR_CHECK(  SG_dagnode__to_vhash__shared(pCtx, pdn, NULL, &pvh)  );
    SG_DAGNODE_NULLFREE(pCtx, pdn);

    SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pvh, &jso)  );

    SG_VHASH_NULLFREE(pCtx, pvh);

    *rval = OBJECT_TO_JSVAL(jso);
    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    /* TODO */
    return JS_FALSE;
}

SG_JSGLUE_METHOD_PROTOTYPE(repo, fetch_dag_leaves)
{
   SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_repo* pRepo = NULL;
    SG_safeptr* psp = sg_jsglue__get_object_private(cx, obj);
    SG_rbtree* prb_leaves = NULL;
    SG_uint32 iDagNum;
    int32 jsi;
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_id = NULL;
    SG_uint32 i;
    JSObject* jso;

    SG_UNUSED(argc);

    SG_ERR_CHECK(  SG_safeptr__unwrap__repo(pCtx, psp, &pRepo)  );

    JS_ValueToInt32(cx, argv[0], &jsi);
    iDagNum = (SG_uint32) jsi;

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, iDagNum, &prb_leaves)  );

    jso = JS_NewArrayObject(cx, 0, NULL);

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_leaves, &b, &psz_id, NULL)  );
    i = 0;
    while (b)
    {
        jsval jv;
        JSString* pjs = NULL;

        pjs = JS_NewStringCopyZ(cx, psz_id);
        jv = STRING_TO_JSVAL(pjs);
        SG_JS_BOOL_CHECK(  JS_SetElement(cx, jso, i++, &jv)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_id, NULL)  );
    }
    SG_rbtree__iterator__free(pCtx, pit);
    SG_RBTREE_NULLFREE(pCtx, prb_leaves);

    *rval = OBJECT_TO_JSVAL(jso);

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	// TODO do we need to free anything?
    return JS_FALSE;
}

SG_JSGLUE_PROPERTY_PROTOTYPE(repo, admin_id)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_safeptr* psp = NULL;
    SG_repo* pRepo = NULL;
    char* psz_id = NULL;

    SG_UNUSED(idval);

    psp = sg_jsglue__get_object_private(cx, obj);
    SG_ERR_CHECK(  SG_safeptr__unwrap__repo(pCtx, psp, &pRepo)  );

    SG_ERR_CHECK(  SG_repo__get_admin_id(pCtx, pRepo, &psz_id)  );

    *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, psz_id));

    SG_NULLFREE(pCtx, psz_id);

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    return JS_FALSE;
}

SG_JSGLUE_PROPERTY_PROTOTYPE(repo, repo_id)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_safeptr* psp = NULL;
    SG_repo* pRepo = NULL;
    char* psz_id = NULL;

    SG_UNUSED(idval);

    psp = sg_jsglue__get_object_private(cx, obj);
    SG_ERR_CHECK(  SG_safeptr__unwrap__repo(pCtx, psp, &pRepo)  );

    SG_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pRepo, &psz_id)  );

    *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, psz_id));

    SG_NULLFREE(pCtx, psz_id);

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    return JS_FALSE;
}

/* ---------------------------------------------------------------- */
/* Now the method tables */
/* ---------------------------------------------------------------- */

/*
 * These properties are available on a repo class.  To create a repo
 * use the javascript "var repo = sg.open_repo("repo name")
 */
static JSPropertySpec sg_repo_properties[] = {
    {"repo_id",           0,      JSPROP_READONLY,       SG_JSGLUE_PROPERTY_NAME(repo, repo_id), NULL},
    {"admin_id",          0,     JSPROP_READONLY,       SG_JSGLUE_PROPERTY_NAME(repo, admin_id), NULL},
    {NULL,0,0,NULL,NULL}
};
/*
 * These methods are available on a repo class.  To create a repo
 * use the javascript "var repo = sg.open_repo("repo name")
 */
static JSFunctionSpec sg_repo_methods[] = {
    {"fetch_dag_leaves", SG_JSGLUE_METHOD_NAME(repo,fetch_dag_leaves),1,0,0},
    {"close", SG_JSGLUE_METHOD_NAME(repo,close),0,0,0},

    {"fetch_dagnode", SG_JSGLUE_METHOD_NAME(repo,fetch_dagnode),1,0,0},
    {"fetch_json", SG_JSGLUE_METHOD_NAME(repo,fetch_json),1,0,0},

    {NULL,NULL,0,0,0}
};

/*
 * These methods are available on the global static "sg" object.
 * example of usage: sg.new_repo()
 * sg.time()
 * sg.gid()
 */
static JSFunctionSpec sg_methods[] = {
    {"open_repo",            SG_JSGLUE_METHOD_NAME(sg,open_repo),       1,0,0},
    {"repo_diag_blobs",            SG_JSGLUE_METHOD_NAME(sg,repo_diag_blobs),       1,0,0},
//    {"open_pending_tree",            SG_JSGLUE_METHOD_NAME(sg,open_pending_tree),       1,0,0},

    {"new_repo",            SG_JSGLUE_METHOD_NAME(sg,new_repo),       1,0,0},
    {"clone",               SG_JSGLUE_METHOD_NAME(sg,clone), 2,0,0},

    {"time",            SG_JSGLUE_METHOD_NAME(sg,time),     0,0,0},
    {"platform",        SG_JSGLUE_METHOD_NAME(sg,platform), 0,0,0},
    {"gid",             SG_JSGLUE_METHOD_NAME(sg,gid),      0,0,0},
    {"set_local_setting",   SG_JSGLUE_METHOD_NAME(sg,set_local_setting),      2,0,0},
    {"local_settings",   SG_JSGLUE_METHOD_NAME(sg,local_settings),      0,0,0},

// TODO Now that we allow REPOs to select a Hash Algorithm, HIDs are
// TODO essentially variable length.  We need to have a REPO variable
// TODO on hand before we can request a HID be computed.  I'm commenting
// TODO out this method because I don't think it is being used.  I think
// TODO it was only added for debugging when we first got started.
//    {"hid",             SG_JSGLUE_METHOD_NAME(sg,hid),      1,0,0},

    {"exec",            SG_JSGLUE_METHOD_NAME(sg,exec),       1,0,0},
    {"exec_nowait",            SG_JSGLUE_METHOD_NAME(sg,exec_nowait),       1,0,0},
	{"exec_pipeinput",				SG_JSGLUE_METHOD_NAME(sg,exec_pipeinput),		1,0,0},
    {"list_descriptors",            SG_JSGLUE_METHOD_NAME(sg,list_descriptors),       0,0,0},
    {"to_json",            SG_JSGLUE_METHOD_NAME(sg,to_json),       1,0,0},

    {NULL,NULL,0,0,0}
};
/*
 * These methods are available on the global static "sg.fs" object.
 * examples of usage: sg.fs.length("/etc/hosts")
 * sg.fs.exists("/etc/hotss")
 * sg.fs.getcwd()
 */
static JSFunctionSpec fs_methods[] = {
    {"length",            SG_JSGLUE_METHOD_NAME(fs,length),       1,0,0},
    {"remove",            SG_JSGLUE_METHOD_NAME(fs,remove),       1,0,0},
    {"exists",            SG_JSGLUE_METHOD_NAME(fs,exists),       1,0,0},
    {"mkdir",            SG_JSGLUE_METHOD_NAME(fs,mkdir),       1,0,0},
    {"rmdir",            SG_JSGLUE_METHOD_NAME(fs,rmdir),       1,0,0},
    {"mkdir_recursive",   SG_JSGLUE_METHOD_NAME(fs,mkdir_recursive),       1,0,0},
    {"rmdir_recursive",   SG_JSGLUE_METHOD_NAME(fs,rmdir_recursive),       1,0,0},
    {"move",  	      SG_JSGLUE_METHOD_NAME(fs,move),       2,0,0},
    //Gets the Current Working Directory
    {"getcwd",  	  	      SG_JSGLUE_METHOD_NAME(fs,getcwd),       0,0,0},
    {"chmod",  	  	      SG_JSGLUE_METHOD_NAME(fs,chmod),       2,0,0},
	//This returns a javascript object that contains information about the timestamp and permissions, and other stuff
    {"stat",  	  	      SG_JSGLUE_METHOD_NAME(fs,stat),       1,0,0},
    //Change the Current Working Directory
    {"cd",  	  	      SG_JSGLUE_METHOD_NAME(fs,cd),       1,0,0},

    // fsobj_type?
    // perms?
    // time?
    {NULL,NULL,0,0,0}
};
/*
 * These methods are available on the global static "sg.pending_tree" object.
 * examples of usage: sg.pending_tree.add("relative/path/to/object")
 * sg.pending_tree.status()
 */
static JSFunctionSpec pending_tree_methods[] = {
    //{"close",            SG_JSGLUE_METHOD_NAME(pending_tree,close),       0,0,0},
    {"add",            SG_JSGLUE_METHOD_NAME(pending_tree,add),       1,0,0},
    {"commit",            SG_JSGLUE_METHOD_NAME(pending_tree,commit),       0,0,0},
    {"remove",            SG_JSGLUE_METHOD_NAME(pending_tree,remove),       1,0,0},
    {"rename",            SG_JSGLUE_METHOD_NAME(pending_tree,rename),       2,0,0},
    {"move",            SG_JSGLUE_METHOD_NAME(pending_tree,move),       2,0,0},
    {"get",            SG_JSGLUE_METHOD_NAME(pending_tree,get),       2,0,0},
    {"scan",            SG_JSGLUE_METHOD_NAME(pending_tree,scan),       0,0,0},
    {"addremove",            SG_JSGLUE_METHOD_NAME(pending_tree,addremove),       0,0,0},
    {"revert",            SG_JSGLUE_METHOD_NAME(pending_tree,revert),       0,0,0},
    //This returns the history as an array.
    {"history",            SG_JSGLUE_METHOD_NAME(pending_tree,history),       0,0,0},
    //This returns the updates as an array.
	{"update",            SG_JSGLUE_METHOD_NAME(pending_tree,update),       0,0,0},
	{"stamp",            SG_JSGLUE_METHOD_NAME(pending_tree,stamp),       0,0,0},
	{"stamps",            SG_JSGLUE_METHOD_NAME(pending_tree,stamps),       0,0,0},
    //This returns the status as an array.
    {"status",            SG_JSGLUE_METHOD_NAME(pending_tree,status),       0,0,0},
    //This returns the status as a string, formatted exactly like the CLC does it.
    {"status_string",            SG_JSGLUE_METHOD_NAME(pending_tree,status_string),       0,0,0},
    {"tag",            SG_JSGLUE_METHOD_NAME(pending_tree,tag),       0,0,0},
    {"tags",            SG_JSGLUE_METHOD_NAME(pending_tree,tags),       0,0,0},
    {"parents",            SG_JSGLUE_METHOD_NAME(pending_tree,parents),       0,0,0},
    {"heads",            SG_JSGLUE_METHOD_NAME(pending_tree,heads),       0,0,0},
    {"manually_set_parents",            SG_JSGLUE_METHOD_NAME(pending_tree,manually_set_parents),       0,0,0},
    {NULL,NULL,0,0,0}
};
/*
 * These methods are available on the global static "sg.file" object.
 * examples of usage: sg.file.write("relative/path/to/object", "file contents")
 * sg.file.append("file.txt", "new line to append")
 */
static JSFunctionSpec file_methods[] = {
    {"write",            SG_JSGLUE_METHOD_NAME(file,write),       2,0,0},
    {"append",            SG_JSGLUE_METHOD_NAME(file,append),       2,0,0},
    {"read",            SG_JSGLUE_METHOD_NAME(file,read),       1,0,0},
    {NULL,NULL,0,0,0}
};

// TODO need class/methods to do SG_pathname operations

//////////////////////////////////////////////////////////////////

#include "sg_jsglue__private_setprop.h"
#include "sg_jsglue__private_sg_config.h"

//////////////////////////////////////////////////////////////////

/* ---------------------------------------------------------------- */
/* This is the function which sets everything up */
/* ---------------------------------------------------------------- */

void SG_jsglue__install_scripting_api(SG_context * pCtx,
									  JSContext *cx, JSObject *glob)
{
    JSObject* pjsobj_sg = NULL;
    JSObject* pjsobj_fs = NULL;
    JSObject* pjsobj_pending_tree = NULL;
    JSObject* pjsobj_file = NULL;
    JSObject* pjsobj_dagnums = NULL;
    jsval jv;

    SG_JS_BOOL_CHECK(  pjsobj_sg = JS_DefineObject(cx, glob, "sg", &sg_class, NULL, 0)  );

    SG_JS_BOOL_CHECK(  JS_DefineFunctions(cx, pjsobj_sg, sg_methods)  );

    SG_JS_BOOL_CHECK(  pjsobj_fs = JS_DefineObject(cx, pjsobj_sg, "fs", &fs_class, NULL, 0)  );

    SG_JS_BOOL_CHECK(  JS_DefineFunctions(cx, pjsobj_fs, fs_methods)  );

    SG_JS_BOOL_CHECK(  pjsobj_pending_tree = JS_DefineObject(cx, pjsobj_sg, "pending_tree", &pending_tree_class, NULL, 0)  );

    SG_JS_BOOL_CHECK(  JS_DefineFunctions(cx, pjsobj_pending_tree, pending_tree_methods)  );

    SG_JS_BOOL_CHECK(  pjsobj_file = JS_DefineObject(cx, pjsobj_sg, "file", &file_class, NULL, 0)  );

	SG_JS_BOOL_CHECK(  JS_DefineFunctions(cx, pjsobj_file, file_methods)  );

	// Define global sg.config. array with a list of known values.
	SG_ERR_CHECK(  sg_jsglue__install_sg_config(pCtx, cx, pjsobj_sg)  );

    /* TODO put ALL the dagnums in here */

    SG_JS_BOOL_CHECK(  pjsobj_dagnums = JS_DefineObject(cx, pjsobj_sg, "dagnum", NULL, NULL, 0)  );

    jv = INT_TO_JSVAL(SG_DAGNUM__SUP);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, pjsobj_dagnums, "SUP", &jv)  );

    jv = INT_TO_JSVAL(SG_DAGNUM__VERSION_CONTROL);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, pjsobj_dagnums, "VERSION_CONTROL", &jv)  );

    jv = INT_TO_JSVAL(SG_DAGNUM__WORK_ITEMS);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, pjsobj_dagnums, "WORK_ITEMS", &jv)  );

    jv = INT_TO_JSVAL(SG_DAGNUM__USERS);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, pjsobj_dagnums, "USERS", &jv)  );

    jv = INT_TO_JSVAL(SG_DAGNUM__WIKI);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, pjsobj_dagnums, "WIKI", &jv)  );

    jv = INT_TO_JSVAL(SG_DAGNUM__VC_COMMENTS);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, pjsobj_dagnums, "VC_COMMENTS", &jv)  );

    jv = INT_TO_JSVAL(SG_DAGNUM__BRANCHES);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, pjsobj_dagnums, "BRANCHES", &jv)  );

    jv = INT_TO_JSVAL(SG_DAGNUM__WIKI);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, pjsobj_dagnums, "WIKI", &jv)  );

    jv = INT_TO_JSVAL(SG_DAGNUM__FORUM);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, pjsobj_dagnums, "FORUM", &jv)  );

    jv = INT_TO_JSVAL(SG_DAGNUM__VC_TAGS);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, pjsobj_dagnums, "VC_TAGS", &jv)  );

    jv = INT_TO_JSVAL(SG_DAGNUM__VC_STAMPS);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, pjsobj_dagnums, "VC_STAMPS", &jv)  );

    jv = INT_TO_JSVAL(SG_DAGNUM__TESTING__DB);
    SG_JS_BOOL_CHECK(  JS_SetProperty(cx, pjsobj_dagnums, "TESTING_DB", &jv)  );

    SG_JS_BOOL_CHECK(  JS_InitClass(
            cx,
            glob,
            NULL, /* parent proto */
            &sg_repo_class,
            NULL, /* no constructor */
            0, /* nargs */
            sg_repo_properties,
            sg_repo_methods,
            NULL,  /* static properties */
            NULL   /* static methods */
            )  );

    return;

fail:
    // TODO we had an error installing stuff.
    // TODO do we need to tear down anything that we just created?
    return;
}

#if 0
/*
TODO 3/31/10 Things Jeff wants in the JS glue:
[] Need way to tell which platform we are on so that we can know
   about AttrBits and XAttrs.
[] Need interfaces to AttrBits and XAttrs as appropriate.
[] Need fsobj.symlink.
[] Need way to do readdir() and iterate over contents of directory.
[] Need way to read treenode (maybe?)
[] Need to do SG_wdrepo changes.
[] When pendingtree operation throw a portability warning, we need
   to get the varray/vhash of problems back to inspect/print.
[] pendingtree.rename(path,entryname) and pendingtree.move(path,directory), right?
[] the "_" in "pending_tree" class is hard to type after typing SG_pendingtree.
[] several routines (like pending_tree.move) allocate the includes/excludes
   arrays before they know whether they need them.  we should defer those allocs
   until we actually see a --include/--exclude since the SG_pendingtree__ code
   properly handles null pointers for them.
*/
#endif
