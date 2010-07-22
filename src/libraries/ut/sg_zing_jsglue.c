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

#ifdef WINDOWS
//#pragma warning(disable:4100)
#pragma warning(disable:4706)
#endif

#include "sg_zing__private.h"

void sg_zingdb__get_leaf(
    SG_context* pCtx,
	sg_zingdb* pz,
    char** pp
    );

void sg_zingdb__free(
        SG_context* pCtx,
        sg_zingdb* pzs
        );

void sg_zingdb__alloc(
    SG_context* pCtx,
	SG_repo* pRepo,
    SG_uint32 iDagNum,
	sg_zingdb** pp
    );

struct sg_zinglinkcollection
{
    SG_rbtree* prb;
    SG_rbtree* prb_recid_to_link;
    SG_zingrecord* pzr;
    SG_bool b_from;
    char* psz_link_name_mine;
    char* psz_primary_link_name;
};

struct sg_zingdb
{
    SG_repo* pRepo;
    SG_uint32 iDagNum;
};

struct sg_statetxcombo
{
    sg_zingdb* pzstate;
    SG_zingtx* pztx;
};

void sg_zingdb__free(
        SG_context* pCtx,
        sg_zingdb* pzs
        )
{
    if (pzs)
    {
        SG_NULLFREE(pCtx, pzs);
    }
}

void sg_statetxcombo__alloc(
    SG_context* pCtx,
    sg_zingdb* pzstate,
    SG_zingtx* pztx,
	sg_statetxcombo** pp
    )
{
    sg_statetxcombo* pThis = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pThis)  );

    pThis->pzstate = pzstate;
    pThis->pztx = pztx;

    *pp = pThis;
    pThis = NULL;

    return;

fail:
    SG_NULLFREE(pCtx, pThis);
}

void sg_zingdb__alloc(
    SG_context* pCtx,
	SG_repo* pRepo,
    SG_uint32 iDagNum,
	sg_zingdb** pp
    )
{
    sg_zingdb* pThis = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);
    SG_NULLARGCHECK_RETURN(pp);

	SG_ERR_CHECK(  SG_alloc1(pCtx, pThis)  );

    pThis->pRepo = pRepo;
    pThis->iDagNum = iDagNum;

    *pp = pThis;
    pThis = NULL;

    return;

fail:
    sg_zingdb__free(pCtx, pThis);
}

/* TODO make sure we tell JS that strings are utf8 */

/*
 * Any object that requires a pointer (like an SG_zingtx)
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

//////////////////////////////////////////////////////////////////

/**
 * We cannot control the prototypes of methods and properties;
 * these are defined by SpiderMonkey.
 *
 * Note that they do not take a SG_Context.  Methods and properties
 * will have to extract it from the js-context-private data.
 */
#define SG_ZING_JSGLUE_METHOD_NAME(class, name) sg_zing_jsglue__method__##class##__##name
#define SG_ZING_JSGLUE_METHOD_PROTOTYPE(class, name) static JSBool SG_ZING_JSGLUE_METHOD_NAME(class, name)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)

#define SG_ZING_JSGLUE_PROPERTY_NAME(class, name) sg_zing_jsglue__property__##class##__##name
#define SG_ZING_JSGLUE_PROPERTY_PROTOTYPE(class, name) static JSBool SG_ZING_JSGLUE_PROPERTY_NAME(class, name)(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)

/* ---------------------------------------------------------------- */
/* Utility functions */
/* ---------------------------------------------------------------- */

SG_safeptr* sg_zing_jsglue__get_object_private(JSContext *cx, JSObject *obj)
{
    SG_safeptr* psp = (SG_safeptr*) JS_GetPrivate(cx, obj);
    return psp;
}

JSBool zingrec_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
JSBool zingrec_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
void zingrec_finalize(JSContext *cx, JSObject *obj);
JSBool zinglinkcollection_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
JSBool zinglinkcollection_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
void zinglinkcollection_finalize(JSContext *cx, JSObject *obj);
void zingdb_finalize(JSContext *cx, JSObject *obj);

/* ---------------------------------------------------------------- */
/* JSClass structures.  The InitClass calls are at the bottom of
 * the file */
/* ---------------------------------------------------------------- */
static JSClass sg_zingdb_class = {
    "zingdb",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, zingdb_finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass sg_zingtx_class = {
    "sg_zingtx",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass sg_zingrecord_class = {
    "sg_zingrecord",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, zingrec_getter, zingrec_setter,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, zingrec_finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass sg_zinglinkcollection_class = {
    "sg_zinglinkcollection",
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_ENUMERATE,
    JS_PropertyStub, JS_PropertyStub, zinglinkcollection_getter, zinglinkcollection_setter,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, zinglinkcollection_finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};


/* ---------------------------------------------------------------- */
/* JSNative method definitions */
/* ---------------------------------------------------------------- */
JSBool zingrec_getter(JSContext *cx, JSObject *obj, jsval id,
                                 jsval *vp)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_zingrecord* pZingRec = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
	const char* psz_recid = NULL;
	char* psz_name = NULL;
	JSString* pjstr = NULL;
    SG_zingfieldattributes* pzfa = NULL;
    SG_zinglinkattributes* pzla = NULL;
    SG_zinglinksideattributes* pzlsa_mine = NULL;
    SG_zinglinksideattributes* pzlsa_other = NULL;
    JSString* pjs = NULL;
    SG_rbtree* prb = NULL;
    JSObject* jso = NULL;
    sg_zinglinkcollection* pzl = NULL;
    SG_safeptr* psp_pzl = NULL;
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    SG_string* pstr_val = NULL;

	SG_NULLARGCHECK(psp);

	SG_ERR_CHECK(  SG_safeptr__unwrap__zingrecord(pCtx, psp, &pZingRec)  );
	psz_name = JS_GetStringBytes(JSVAL_TO_STRING(id));

    if (strcmp(psz_name, SG_ZING_FIELD__RECID) == 0)
    {
        SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pZingRec, &psz_recid) );
        pjstr = JS_NewStringCopyZ(cx, psz_recid);
        *vp = STRING_TO_JSVAL(pjstr);
    }
    else if (strcmp(psz_name, SG_ZING_FIELD__RECTYPE) == 0)
    {
        const char* psz_rectype = NULL;

        SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pZingRec, &psz_rectype) );
        pjstr = JS_NewStringCopyZ(cx, psz_rectype);
        *vp = STRING_TO_JSVAL(pjstr);
    }
    else if (strcmp(psz_name, SG_ZING_FIELD__HISTORY) == 0)
    {
        SG_varray* pva = NULL;
        SG_ERR_CHECK(  SG_zingrecord__get_history(pCtx, pZingRec, &pva) );
        SG_ERR_CHECK(  sg_jsglue__varray_to_jsobject(pCtx, cx, pva, &jso)   );
        *vp = OBJECT_TO_JSVAL(jso);
    }
    else
    {
        SG_ERR_CHECK(  SG_zingrecord__lookup_name(pCtx, pZingRec, psz_name, &pzfa, &pzla, &pzlsa_mine, &pzlsa_other)  );

        if (pzfa)
        {
            switch (pzfa->type)
            {
                case SG_ZING_TYPE__BOOL:
                    {
                        SG_bool b = SG_FALSE;

                        SG_ERR_CHECK(  SG_zingrecord__get_field__bool(pCtx, pZingRec, pzfa, &b) );
                        *vp = BOOLEAN_TO_JSVAL(b);
                        break;
                    }

                case SG_ZING_TYPE__DATETIME:
                    {
                        SG_int64 t;

                        // TODO should we perhaps convert to a Date object?

                        SG_ERR_CHECK(  SG_zingrecord__get_field__datetime(pCtx, pZingRec, pzfa, &t) );
                        if (SG_int64__fits_in_double(t))
                        {
                            double d = (double) t;
                            jsval jv;

                            SG_JS_BOOL_CHECK(  JS_NewNumberValue(cx, d, &jv)  );
                            *vp = jv;
                        }
                        else
                        {
                            SG_ERR_THROW(  SG_ERR_INTEGER_OVERFLOW  );
                        }
                        break;
                    }

                case SG_ZING_TYPE__INT:
                    {
                        SG_int64 intVal;

                        SG_ERR_CHECK(  SG_zingrecord__get_field__int(pCtx, pZingRec, pzfa, &intVal) );
                        *vp = INT_TO_JSVAL(intVal);
                        break;
                    }

                case SG_ZING_TYPE__USERID:
                    {
                        const char* psz_val = NULL;

                        SG_ERR_CHECK(  SG_zingrecord__get_field__userid(pCtx, pZingRec, pzfa, &psz_val) );
                        // TODO do we really want to just return the user id?
                        pjs = JS_NewStringCopyZ(cx, psz_val);
                        *vp = STRING_TO_JSVAL(pjs);
                        break;
                    }

                case SG_ZING_TYPE__STRING:
                    {
                        const char* psz_val = NULL;

                        SG_ERR_CHECK(  SG_zingrecord__get_field__string(pCtx, pZingRec, pzfa, &psz_val) );
                        pjs = JS_NewStringCopyZ(cx, psz_val);
                        *vp = STRING_TO_JSVAL(pjs);
                        break;
                    }

                case SG_ZING_TYPE__DAGNODE:
                    {
                        const char* psz_val = NULL;

                        SG_ERR_CHECK(  SG_zingrecord__get_field__dagnode(pCtx, pZingRec, pzfa, &psz_val) );
                        pjs = JS_NewStringCopyZ(cx, psz_val);
                        *vp = STRING_TO_JSVAL(pjs);
                        break;
                    }

                case SG_ZING_TYPE__ATTACHMENT:
                    {
                        const char* psz_val = NULL;

                        // JS get field on an attachment returns the HID of the blob
                        SG_ERR_CHECK(  SG_zingrecord__get_field__attachment(pCtx, pZingRec, pzfa, &psz_val) );
                        pjs = JS_NewStringCopyZ(cx, psz_val);
                        *vp = STRING_TO_JSVAL(pjs);
                        break;
                    }

                default:
                    SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                    break;
            }
        }
        else if (pzla)
        {
            SG_zingtx* pztx = NULL;
            SG_uint32 count  = 0;

            SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pZingRec, &psz_recid) );
            SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pZingRec, &pztx)  );
            SG_ERR_CHECK(  SG_zingtx___find_links(pCtx, pztx, pzla, psz_recid, pzlsa_mine->bFrom, &prb)  );
            if (prb)
            {
                SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
            }

            if (pzlsa_mine->bSingular)
            {
                if (count > 1)
                {
                    SG_ERR_THROW(  SG_ERR_UNSPECIFIED  );
                }
                else if (1 == count)
                {
                    const char* psz_link = NULL;
                    const char* psz_hid_rec = NULL;
                    char buf_link_name[SG_GID_BUFFER_LENGTH];
                    char buf_recid_from[SG_GID_BUFFER_LENGTH];
                    char buf_recid_to[SG_GID_BUFFER_LENGTH];

                    SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, prb, &psz_link, (void**) &psz_hid_rec)  );
                    SG_ERR_CHECK(  SG_zinglink__unpack(pCtx, psz_link, buf_recid_from, buf_recid_to, buf_link_name)  );

                    if (pzlsa_mine->bFrom)
                    {
                        pjs = JS_NewStringCopyZ(cx, buf_recid_to);
                    }
                    else
                    {
                        pjs = JS_NewStringCopyZ(cx, buf_recid_from);
                    }
                    *vp = STRING_TO_JSVAL(pjs);
                }
                else
                {
                    *vp = JSVAL_NULL;
                }
            }
            else
            {
                const char* psz_link = NULL;

                SG_ERR_CHECK(  SG_alloc1(pCtx, pzl)  );
                pzl->prb = prb;
                prb = NULL;
                pzl->pzr = pZingRec;
                SG_ERR_CHECK(  SG_strdup(pCtx, psz_name, &pzl->psz_link_name_mine)  );
                SG_ERR_CHECK(  SG_strdup(pCtx, pzla->psz_link_name, &pzl->psz_primary_link_name)  );

                // build the reverse lookup tree
                SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pzl->prb_recid_to_link)  );
                SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pzl->prb, &b, &psz_link, NULL)  );
                while (b)
                {
                    char buf_recid_from[SG_GID_BUFFER_LENGTH];
                    char buf_recid_to[SG_GID_BUFFER_LENGTH];

                    SG_ERR_CHECK(  SG_zinglink__unpack(pCtx, psz_link, buf_recid_from, buf_recid_to, NULL)  );

                    if (pzlsa_mine->bFrom)
                    {
                        SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, pzl->prb_recid_to_link, buf_recid_to, psz_link)  );
                    }
                    else
                    {
                        SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, pzl->prb_recid_to_link, buf_recid_from, psz_link)  );
                    }

                    SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link, NULL)  );
                }
                SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

                jso = JS_NewObject(cx, &sg_zinglinkcollection_class, NULL, NULL);
                SG_ERR_CHECK(  SG_safeptr__wrap__zinglinkcollection(pCtx, pzl, &psp_pzl)  );
                JS_SetPrivate(cx, jso, psp_pzl);
                *vp = OBJECT_TO_JSVAL(jso);
            }
            SG_RBTREE_NULLFREE(pCtx, prb);
        }
        else
        {
            return JS_PropertyStub(cx, obj, id, vp);
        }
    }

    SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_mine);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_other);

	return JS_TRUE;

fail:
    SG_STRING_NULLFREE(pCtx, pstr_val);
    SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_mine);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_other);

	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

void sg_zing_jsglue__set_field(
        JSContext* cx,
        SG_context* pCtx,
        SG_zingrecord* pZingRec,
        SG_zingfieldattributes* pzfa,
        const char* psz_name,
        jsval* vp
        )
{
    SG_pathname* pPath = NULL;

    // assigning a field to null is equivalent to removing it from the record
    if (JSVAL_IS_NULL(*vp))
    {
        SG_ERR_CHECK(  SG_zingrecord__remove_field(pCtx, pZingRec, pzfa)  );
    }
    else
    {
        switch (pzfa->type)
        {
            case SG_ZING_TYPE__ATTACHMENT:
                {
                    if (JSVAL_IS_STRING(*vp))
                    {
                        SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pPath, JS_GetStringBytes(JSVAL_TO_STRING((*vp)))) );
                        SG_ERR_CHECK(  SG_zingrecord__set_field__attachment__pathname(pCtx, pZingRec, pzfa, &pPath) );
                    }
                    else
                    {
                        // TODO what other ways could an attachment be set?
                        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
                    }
                    break;
                }

            case SG_ZING_TYPE__BOOL:
                {
                    if (JSVAL_IS_BOOLEAN(*vp))
                    {
                        SG_ERR_CHECK(  SG_zingrecord__set_field__bool(pCtx, pZingRec, pzfa, JSVAL_TO_BOOLEAN(*vp)) );
                    }
                    else
                    {
                        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
                    }
                    break;
                }

            case SG_ZING_TYPE__DATETIME:
                {
                    if (JSVAL_IS_INT(*vp))
                    {
                        SG_ERR_CHECK(  SG_zingrecord__set_field__datetime(pCtx, pZingRec, pzfa, JSVAL_TO_INT(*vp)) );
                    }
                    else if (JSVAL_IS_NUMBER(*vp))
                    {
                        jsdouble d;
                        SG_int64 i;

                        d = *JSVAL_TO_DOUBLE(*vp);
                        i = (SG_int64)d;

                        SG_ERR_CHECK(  SG_zingrecord__set_field__datetime(pCtx, pZingRec, pzfa, i) );
                    }
                    else if (JSVAL_IS_OBJECT(*vp))
                    {
                        JSObject* jsotmp = NULL;

                        jsotmp = JSVAL_TO_OBJECT(*vp);
                        if (JS_InstanceOf(cx, jsotmp, &js_DateClass, NULL))
                        {
                            jsval tmpval;
                            jsdouble d;
                            SG_int64 i;

                            SG_JS_BOOL_CHECK( JS_CallFunctionName(cx, jsotmp, "getTime", 0, NULL, &tmpval) );

                            d = *JSVAL_TO_DOUBLE(*vp);
                            i = (SG_int64)d;

                            SG_ERR_CHECK(  SG_zingrecord__set_field__datetime(pCtx, pZingRec, pzfa, i) );
                        }
                        else
                        {
                            SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
                        }
                    }
                    else
                    {
                        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
                    }
                    break;
                }

            case SG_ZING_TYPE__INT:
                {
                    if (JSVAL_IS_INT(*vp))
                    {
                        SG_ERR_CHECK(  SG_zingrecord__set_field__int(pCtx, pZingRec, pzfa, JSVAL_TO_INT(*vp)) );
                    }
                    else
                    {
                        SG_ERR_THROW2(  SG_ERR_ZING_TYPE_MISMATCH,
                            (pCtx, "Field '%s'", psz_name)
                            );
                    }
                    break;
                }

            case SG_ZING_TYPE__STRING:
                {
                    if (JSVAL_IS_STRING(*vp))
                    {
                        SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, pZingRec, pzfa, JS_GetStringBytes(JSVAL_TO_STRING((*vp)))) );
                    }
                    else
                    {
                        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
                    }
                    break;
                }

            case SG_ZING_TYPE__USERID:
                {
                    if (JSVAL_IS_STRING(*vp))
                    {
                        SG_ERR_CHECK(  SG_zingrecord__set_field__userid(pCtx, pZingRec, pzfa, JS_GetStringBytes(JSVAL_TO_STRING((*vp)))) );
                    }
                    else
                    {
                        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
                    }
                    break;
                }

            case SG_ZING_TYPE__DAGNODE:
                {
                    if (JSVAL_IS_STRING(*vp))
                    {
                        SG_ERR_CHECK(  SG_zingrecord__set_field__dagnode(pCtx, pZingRec, pzfa, JS_GetStringBytes(JSVAL_TO_STRING((*vp)))) );
                    }
                    else
                    {
                        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
                    }
                    break;
                }

            default:
                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                break;
        }
    }

fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath);
}

JSBool zingrec_setter(JSContext *cx, JSObject *obj, jsval id,
                                 jsval *vp)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_zingrecord* pZingRec = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
	char* psz_name = NULL;
    SG_zingfieldattributes* pzfa = NULL;
    SG_zinglinkattributes* pzla = NULL;
    SG_zinglinksideattributes* pzlsa_mine = NULL;
    SG_zinglinksideattributes* pzlsa_other = NULL;

	SG_NULLARGCHECK(psp);

	SG_ERR_CHECK(  SG_safeptr__unwrap__zingrecord(pCtx, psp, &pZingRec)  );
	psz_name = JS_GetStringBytes(JSVAL_TO_STRING(id));

    SG_ERR_CHECK(  SG_zingrecord__lookup_name(pCtx, pZingRec, psz_name, &pzfa, &pzla, &pzlsa_mine, &pzlsa_other)  );

    if (pzfa)
    {
        SG_ERR_CHECK(  sg_zing_jsglue__set_field(cx, pCtx, pZingRec, pzfa, psz_name, vp)  );
    }
    else if (pzla)
    {
        if (pzlsa_mine->bSingular)
        {
            if (JSVAL_IS_NULL(*vp))
            {
                const char* psz_recid_mine = NULL;
                SG_zingtx* pztx = NULL;

                SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pZingRec, &pztx)  );

                SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pZingRec, &psz_recid_mine)  );

                // If a link for "mine" already exists, delete it.
                if (pzlsa_mine->bFrom)
                {
                    SG_ERR_CHECK(  SG_zingtx__delete_links_from(pCtx, pztx, pzla, psz_recid_mine)  );
                }
                else
                {
                    SG_ERR_CHECK(  SG_zingtx__delete_links_to(pCtx, pztx, pzla, psz_recid_mine)  );
                }
            }
            else if (JSVAL_IS_STRING(*vp))
            {
                // TODO this should be a recid.  validate it?
                SG_ERR_CHECK(  SG_zingrecord__set_singular_link_and_overwrite_other_if_singular(pCtx, pZingRec, psz_name, JS_GetStringBytes(JSVAL_TO_STRING((*vp))))   );
            }
            else if (JSVAL_IS_OBJECT(*vp))
            {
                SG_safeptr* psp2 = NULL;
                SG_zingrecord* pzr2 = NULL;
                const char* pszOtherRecID = NULL;

                psp2 = sg_zing_jsglue__get_object_private(cx, JSVAL_TO_OBJECT(*vp));
                // TODO what if this object isn't a zingrecord.  is this the
                // way we want fo fail?
                SG_ERR_CHECK(  SG_safeptr__unwrap__zingrecord(pCtx, psp2, &pzr2)  );
                SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzr2, &pszOtherRecID)  );
                SG_ERR_CHECK(  SG_zingrecord__set_singular_link_and_overwrite_other_if_singular(pCtx, pZingRec, psz_name, pszOtherRecID)   );
            }
        }
        else
        {
            // rec.children = foo is not allowed
            SG_ERR_THROW(  SG_ERR_UNSPECIFIED  );
        }
    }
    else
    {
        // rec.foo is not allowed if foo is not a defined field in the template
        SG_ERR_THROW(  SG_ERR_UNSPECIFIED  );
    }

    SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_mine);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_other);

	return JS_TRUE;

fail:
    SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_mine);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_other);

	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;

}

void zingdb_finalize(JSContext *cx, JSObject *obj)
{
	SG_context * pCtx = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
	sg_zingdb* pzstate = NULL;

	SG_context__alloc(&pCtx);
	SG_NULLARGCHECK(psp);

	SG_ERR_CHECK(  SG_safeptr__unwrap__zingdb(pCtx, psp, &pzstate)  );
	sg_zingdb__free(pCtx, pzstate);
	SG_SAFEPTR_NULLFREE(pCtx, psp);
	SG_CONTEXT_NULLFREE(pCtx);
	return;

fail:
	sg_zingdb__free(pCtx, pzstate);
	SG_SAFEPTR_NULLFREE(pCtx, psp);
	SG_CONTEXT_NULLFREE(pCtx);

}

void zingrec_finalize(JSContext *cx, JSObject *obj)
{
	SG_context * pCtx = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
	SG_zingrecord* pZingRec = NULL;

	SG_context__alloc(&pCtx);
	SG_NULLARGCHECK(psp);

	SG_ERR_CHECK(  SG_safeptr__unwrap__zingrecord(pCtx, psp, &pZingRec)  );
	//SG_ZINGRECORD_NULLFREE(pCtx, pZingRec);
	SG_SAFEPTR_NULLFREE(pCtx, psp);
	SG_CONTEXT_NULLFREE(pCtx);
	return;
fail:
	//SG_ZINGRECORD_NULLFREE(pCtx, pZingRec);
	SG_SAFEPTR_NULLFREE(pCtx, psp);
	SG_CONTEXT_NULLFREE(pCtx);

}

JSBool zinglinkcollection_getter(JSContext *cx, JSObject *obj, jsval id,
                                 jsval *vp)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    sg_zinglinkcollection* pzl = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
    SG_uint32 ndx = 0;

	SG_NULLARGCHECK(psp);
    SG_ERR_CHECK(  SG_safeptr__unwrap__zinglinkcollection(pCtx, psp, &pzl)  );

    if (JSVAL_IS_STRING(id))
    {
        // TODO just define a property for length
	    const char* psz_name = JS_GetStringBytes(JSVAL_TO_STRING(id));
        if (strcmp(psz_name, "length") == 0)
        {
            SG_ERR_CHECK(  SG_rbtree__count(pCtx, pzl->prb, &ndx)  );
            *vp = INT_TO_JSVAL(ndx);
        }
        else
        {
            return JS_PropertyStub(cx, obj, id, vp);
        }
    }
    else
    {
        return JS_PropertyStub(cx, obj, id, vp);
    }

	return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

JSBool zinglinkcollection_setter(JSContext *cx, JSObject *obj, jsval id,
                                 jsval *vp)
{
  SG_UNUSED(cx);
  SG_UNUSED(obj);
  SG_UNUSED(id);
  SG_UNUSED(vp);

	return JS_FALSE;
}

void zinglinkcollection_finalize(JSContext *cx, JSObject *obj)
{
	SG_context * pCtx = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
	sg_zinglinkcollection* pzl = NULL;

	SG_context__alloc(&pCtx);
	SG_NULLARGCHECK(psp);

	SG_ERR_CHECK(  SG_safeptr__unwrap__zinglinkcollection(pCtx, psp, &pzl)  );
    SG_RBTREE_NULLFREE(pCtx, pzl->prb);
    SG_RBTREE_NULLFREE(pCtx, pzl->prb_recid_to_link);
    SG_NULLFREE(pCtx, pzl->psz_primary_link_name);
    SG_NULLFREE(pCtx, pzl->psz_link_name_mine);
    SG_NULLFREE(pCtx, pzl);
	SG_SAFEPTR_NULLFREE(pCtx, psp);
	SG_CONTEXT_NULLFREE(pCtx);
	return;
fail:
    SG_NULLFREE(pCtx, pzl);
	SG_SAFEPTR_NULLFREE(pCtx, psp);
	SG_CONTEXT_NULLFREE(pCtx);

}

JSBool zingdb_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	JSObject* jso = NULL;
    sg_zingdb* pzstate = NULL;
	SG_safeptr* psp = NULL;
	SG_repo* pRepo1 = NULL;
	SG_safeptr * pspRepo = NULL;
    char* psz_hid_cs0 = NULL;
    SG_uint32 iDagNum = 0;

    SG_UNUSED(obj);

	SG_JS_BOOL_CHECK(  2 == argc  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_OBJECT(argv[0])  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_INT(argv[1])  );

	pspRepo = sg_zing_jsglue__get_object_private(cx, JSVAL_TO_OBJECT(argv[0]));
	SG_ERR_CHECK(  SG_safeptr__unwrap__repo(pCtx, pspRepo, &pRepo1)  );
	if (pRepo1 == NULL)
    {
		goto fail;
    }

    iDagNum = (SG_uint32) JSVAL_TO_INT(argv[1]);

	jso = JS_NewObject(cx, &sg_zingdb_class, NULL, NULL);

	SG_ERR_CHECK(  sg_zingdb__alloc(pCtx, pRepo1, iDagNum, &pzstate)  );
	SG_ERR_CHECK(  SG_safeptr__wrap__zingdb(pCtx, pzstate, &psp)  );
    JS_SetPrivate(cx, jso, psp);

	*rval = OBJECT_TO_JSVAL(jso);
	SG_NULLFREE(pCtx, psz_hid_cs0);
	return JS_TRUE;

fail:
    sg_zingdb__free(pCtx, pzstate);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingtx, get_template)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	sg_statetxcombo* pcombo = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
	SG_zingtemplate * pZingTemplate = NULL;
	SG_vhash * pvh = NULL;
	JSObject * jso = NULL;
	SG_NULLARGCHECK(psp);

	SG_UNUSED(argc);
	SG_UNUSED(argv);

	SG_ERR_CHECK(  SG_safeptr__unwrap__statetxcombo(pCtx, psp, &pcombo)  );
	SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pcombo->pztx, &pZingTemplate)  );
	if (pZingTemplate != NULL)
	{
		SG_ERR_CHECK(  SG_zingtemplate__get_vhash(pCtx, pZingTemplate, &pvh)  );
		SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pvh, &jso)  );
		*rval = OBJECT_TO_JSVAL(jso);
	}
	else
	{
		*rval = JSVAL_NULL;
	}
	return JS_TRUE;
fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingtx, set_template)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	sg_statetxcombo* pcombo = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
	SG_vhash * pvh = NULL;
	SG_NULLARGCHECK(psp);
	SG_JS_BOOL_CHECK(  1 == argc  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_OBJECT(argv[0])  );

	SG_ERR_CHECK(  SG_safeptr__unwrap__statetxcombo(pCtx, psp, &pcombo)  );
	SG_ERR_CHECK(  sg_jsglue__jsobject_to_vhash(pCtx, cx, JSVAL_TO_OBJECT(argv[0]), &pvh)  );
	SG_ERR_CHECK(  SG_zingtx__store_template(pCtx, pcombo->pztx, &pvh)  );
	*rval = JSVAL_VOID;
	return JS_TRUE;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

#if 0
static void sg_zing_jsglue__dbrecord_to_jsobject(SG_context * pCtx, JSContext* cx, SG_zingtemplate* pzt, SG_dbrecord* prec, JSObject** pp)
{
    JSObject* jso = NULL;
    jsval jv;
    JSString* pjstr = NULL;
    SG_uint32 count = 0;
    SG_uint32 i;
    const char* psz_rectype = NULL;
    SG_zingfieldattributes* pzfa = NULL;

    jso = JS_NewObject(cx, NULL, NULL, NULL);

    SG_ERR_CHECK(  SG_dbrecord__count_pairs(pCtx, prec, &count)  );
    SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, prec, SG_ZING_FIELD__RECTYPE, &psz_rectype)  );

    for (i=0; i<count; i++)
    {
        const char* psz_name = NULL;
        const char* psz_val = NULL;
        uintN attrs = JSPROP_READONLY;

        SG_ERR_CHECK(  SG_dbrecord__get_nth_pair(pCtx, prec, i, &psz_name, &psz_val)  );

        if (
                (0 == strcmp(psz_name, SG_ZING_FIELD__RECTYPE))
                || (0 == strcmp(psz_name, SG_ZING_FIELD__RECID))
           )
        {
            pjstr = JS_NewStringCopyZ(cx, psz_val);
            jv = STRING_TO_JSVAL(pjstr);
        }
        else
        {
            attrs |= JSPROP_ENUMERATE;

            SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, psz_rectype, psz_name, &pzfa)  );

            switch (pzfa->type)
            {
                case SG_ZING_TYPE__BOOL:
                    {
                        SG_int64 ival;
                        SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &ival, psz_val)  );
                        jv = BOOLEAN_TO_JSVAL(ival != 0);
                        break;
                    }

                case SG_ZING_TYPE__DATETIME:
                    {
                        SG_int64 t;
                        SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &t, psz_val)  );

                        // TODO should we perhaps convert to a Date object?

                        if (SG_int64__fits_in_double(t))
                        {
                            double d = (double) t;

                            SG_JS_BOOL_CHECK(  JS_NewNumberValue(cx, d, &jv)  );
                        }
                        else
                        {
                            SG_ERR_THROW(  SG_ERR_INTEGER_OVERFLOW  );
                        }
                        break;
                    }

                case SG_ZING_TYPE__INT:
                    {
                        SG_int64 ival;
                        SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &ival, psz_val)  );
                        jv = INT_TO_JSVAL(ival);
                        break;
                    }

                case SG_ZING_TYPE__USERID:
                    {
                        // TODO do we really want to just return the user id?
                        pjstr = JS_NewStringCopyZ(cx, psz_val);
                        jv = STRING_TO_JSVAL(pjstr);
                        break;
                    }

                case SG_ZING_TYPE__STRING:
                    {
                        pjstr = JS_NewStringCopyZ(cx, psz_val);
                        jv = STRING_TO_JSVAL(pjstr);
                        break;
                    }

                case SG_ZING_TYPE__DAGNODE:
                    {
                        pjstr = JS_NewStringCopyZ(cx, psz_val);
                        jv = STRING_TO_JSVAL(pjstr);
                        break;
                    }

                case SG_ZING_TYPE__ATTACHMENT:
                    {
                        pjstr = JS_NewStringCopyZ(cx, psz_val);
                        jv = STRING_TO_JSVAL(pjstr);
                        break;
                    }

                default:
                    SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                    break;
            }
        }

        SG_JS_BOOL_CHECK(  JS_DefineProperty(cx, jso, psz_name, jv, NULL, NULL, attrs)  );
    }

    // TODO what about links?

    *pp = jso;

    return;

fail:
    return;
}
#endif

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingdb, get_history)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_safeptr* psp = NULL;
    const char* psz_recid = NULL;
    JSObject* jso = NULL;
    sg_zingdb* pz = NULL;
    SG_varray* pva = NULL;

    psp = sg_zing_jsglue__get_object_private(cx, obj);
    SG_ERR_CHECK(  SG_safeptr__unwrap__zingdb(pCtx, psp, &pz)  );

	SG_JS_BOOL_CHECK(  1 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
    psz_recid = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    SG_ERR_CHECK(  SG_repo__dbndx__query_record_history(pCtx, pz->pRepo, pz->iDagNum, psz_recid, &pva)  );

    SG_ERR_CHECK(  sg_jsglue__varray_to_jsobject(pCtx, cx, pva, &jso)   );
    SG_VARRAY_NULLFREE(pCtx, pva);
	*rval = OBJECT_TO_JSVAL(jso);

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingdb, get_record)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_safeptr* psp = NULL;
    SG_vhash* pvh_rec = NULL;
    const char* psz_recid = NULL;
    JSObject* jso = NULL;
    sg_zingdb* pz = NULL;
    char* psz_csid = NULL;

    psp = sg_zing_jsglue__get_object_private(cx, obj);
    SG_ERR_CHECK(  SG_safeptr__unwrap__zingdb(pCtx, psp, &pz)  );

	SG_JS_BOOL_CHECK(  1 == argc  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
    psz_recid = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    SG_ERR_CHECK(  sg_zingdb__get_leaf(pCtx, pz, &psz_csid)  );

    SG_ERR_CHECK(  SG_zing__get_record__vhash(pCtx, pz->pRepo, pz->iDagNum, psz_csid, psz_recid, &pvh_rec)  );
    SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pvh_rec, &jso)  );
    SG_VHASH_NULLFREE(pCtx, pvh_rec);

    SG_NULLFREE(pCtx, psz_csid);

	*rval = OBJECT_TO_JSVAL(jso);

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingdb, get_template)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	sg_zingdb* pz = NULL;
    SG_safeptr* psp = NULL;
    JSObject* jso = NULL;
    SG_zingtemplate* pzt = NULL;
    SG_vhash* pvh = NULL;
    char* psz_csid = NULL;

    SG_UNUSED(argc);
    SG_UNUSED(argv);

    SG_JS_BOOL_CHECK(  0 == argc  );

    // TODO this func always returns the template from the leaf.
    // it might want to optionally accept a csid parameter

    psp = sg_zing_jsglue__get_object_private(cx, obj);
    SG_ERR_CHECK(  SG_safeptr__unwrap__zingdb(pCtx, psp, &pz)  );

    SG_ERR_CHECK(  sg_zingdb__get_leaf(pCtx, pz, &psz_csid)  );
    SG_ERR_CHECK(  sg_zing__load_template__csid(pCtx, pz->pRepo, psz_csid, &pzt)  );
    SG_NULLFREE(pCtx, psz_csid);
    SG_ERR_CHECK(  SG_zingtemplate__get_vhash(pCtx, pzt, &pvh)  );
    SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pvh, &jso)  );
    SG_ZINGTEMPLATE_NULLFREE(pCtx, pzt);
    *rval = OBJECT_TO_JSVAL(jso);

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingdb, get_leaves)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    SG_safeptr* psp = NULL;
    JSObject* jsa = NULL;
    SG_uint32 i = 0;
    JSString* pjstr = NULL;
    sg_zingdb* pz = NULL;
    SG_rbtree* prb_leaves = NULL;
    const char* psz_hid = NULL;
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;

    SG_UNUSED(argv);

    SG_JS_BOOL_CHECK(  0 == argc  );

    psp = sg_zing_jsglue__get_object_private(cx, obj);
    SG_ERR_CHECK(  SG_safeptr__unwrap__zingdb(pCtx, psp, &pz)  );

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pz->pRepo, pz->iDagNum, &prb_leaves)  );

    jsa = JS_NewArrayObject(cx, 0, NULL);

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_leaves, &b, &psz_hid, NULL)  );
    while (b)
    {
        jsval jv;

        pjstr = JS_NewStringCopyZ(cx, psz_hid);
        jv = STRING_TO_JSVAL(pjstr);
        SG_JS_BOOL_CHECK(  JS_SetElement(cx, jsa, i++, &jv)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_RBTREE_NULLFREE(pCtx, prb_leaves);

    *rval = OBJECT_TO_JSVAL(jsa);
    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingdb, query_across_states)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	sg_zingdb* pz = NULL;
    SG_safeptr* psp = NULL;
    JSObject* jso = NULL;
    char* psz_csid = NULL;
    const char* psz_rectype = NULL;
    const char* psz_where = NULL;
    SG_vhash* pvh = NULL;
    SG_int32 gmin = -1;
    SG_int32 gmax = -1;

    SG_JS_BOOL_CHECK(  argc >= 1  );
    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  ); // rectype
    psz_rectype = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    if (argc > 1)
    {
        if (JSVAL_IS_NULL(argv[1]))
        {
            psz_where = NULL;
        }
        else
        {
            SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[1])  ); // where
            psz_where = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
        }
    }

    if (argc > 2)
    {
        SG_JS_BOOL_CHECK(  JSVAL_IS_INT(argv[2])  ); // gmin
        gmin = JSVAL_TO_INT(argv[2]);
    }

    if (argc > 3)
    {
        SG_JS_BOOL_CHECK(  JSVAL_IS_INT(argv[3])  ); // gmin
        gmax = JSVAL_TO_INT(argv[3]);
    }

    psp = sg_zing_jsglue__get_object_private(cx, obj);
    SG_ERR_CHECK(  SG_safeptr__unwrap__zingdb(pCtx, psp, &pz)  );

    SG_ERR_CHECK(  sg_zingdb__get_leaf(pCtx, pz, &psz_csid)  );

    SG_ERR_CHECK(  SG_zing__query_across_states(
                pCtx,
                pz->pRepo,
                pz->iDagNum,
                psz_csid,
                psz_rectype,
                psz_where,
                gmin,
                gmax,
                &pvh
                )  );

    SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pvh, &jso)   );
    SG_VHASH_NULLFREE(pCtx, pvh);
    *rval = OBJECT_TO_JSVAL(jso);

    SG_NULLFREE(pCtx, psz_csid);

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingdb, query)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	sg_zingdb* pz = NULL;
    SG_safeptr* psp = NULL;
    JSObject* jso = NULL;
    char* psz_leaf = NULL;
    const char* psz_csid = NULL;
    const char* psz_specified_state = NULL;
    const char* psz_rectype = NULL;
    const char* psz_where = NULL;
    const char* psz_sort = NULL;
    SG_uint32 limit = 0;
    SG_uint32 skip = 0;
    SG_bool b_specified_state = SG_FALSE;
    SG_varray* pva_fields = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva_sliced = NULL;
    JSObject* jso_fields = NULL;

    SG_JS_BOOL_CHECK(  argc >= 2  );

    SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  ); // rectype
    psz_rectype = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

    SG_JS_BOOL_CHECK(  JSVAL_IS_OBJECT(argv[1])  );
    jso_fields = JSVAL_TO_OBJECT(argv[1]);
    SG_JS_BOOL_CHECK(  JS_IsArrayObject(cx, jso_fields)  );
    SG_ERR_CHECK(  sg_jsglue__jsobject_to_varray(pCtx, cx, jso_fields, &pva_fields)  );
    SG_ERR_CHECK(  SG_varray__to_stringarray(pCtx, pva_fields, &psa_fields)  );
    SG_VARRAY_NULLFREE(pCtx, pva_fields);

    if (argc > 2)
    {
        if (JSVAL_IS_NULL(argv[2]))
        {
            psz_where = NULL;
        }
        else
        {
            SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[2])  ); // where
            psz_where = JS_GetStringBytes(JSVAL_TO_STRING(argv[2]));
        }
    }

    if (argc > 3)
    {
        if (JSVAL_IS_NULL(argv[3]))
        {
            psz_sort = NULL;
        }
        else
        {
            SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[3])  ); // sort
            psz_sort = JS_GetStringBytes(JSVAL_TO_STRING(argv[3]));
        }
    }

    if (argc > 4)
    {
        SG_JS_BOOL_CHECK(  JSVAL_IS_INT(argv[4])  ); // limit
        limit = JSVAL_TO_INT(argv[4]);
    }

    if (argc > 5)
    {
        SG_JS_BOOL_CHECK(  JSVAL_IS_INT(argv[5])  ); // skip
        skip = JSVAL_TO_INT(argv[5]);
    }

    if (argc > 6)
    {
        SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[6])  ); // state
        psz_specified_state = JS_GetStringBytes(JSVAL_TO_STRING(argv[6]));
        b_specified_state = SG_TRUE;
    }

    psp = sg_zing_jsglue__get_object_private(cx, obj);
    SG_ERR_CHECK(  SG_safeptr__unwrap__zingdb(pCtx, psp, &pz)  );

    if (b_specified_state)
    {
        psz_csid = psz_specified_state;
    }
    else
    {
        SG_ERR_CHECK(  sg_zingdb__get_leaf(pCtx, pz, &psz_leaf)  );
        psz_csid = psz_leaf;
    }

    SG_ERR_CHECK(  SG_zing__query(
                pCtx,
                pz->pRepo,
                pz->iDagNum,
                psz_csid,
                psz_rectype,
                psz_where,
                psz_sort,
                limit,
                skip,
                psa_fields,
                &pva_sliced
                )  );
    if (pva_sliced)
    {
        SG_ERR_CHECK(  sg_jsglue__varray_to_jsobject(pCtx, cx, pva_sliced, &jso)   );
        SG_VARRAY_NULLFREE(pCtx, pva_sliced);
    }
    else
    {
        jso = JS_NewArrayObject(cx, 0, NULL);
    }
    *rval = OBJECT_TO_JSVAL(jso);

    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_leaf);

    return JS_TRUE;

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
    return JS_FALSE;
}

void sg_zingdb__get_leaf(
    SG_context* pCtx,
	sg_zingdb* pz,
    char** pp
    )
{
    SG_rbtree* prb_leaves = NULL;
    SG_uint32 count_leaves = 0;
    const char* psz_hid_cs_leaf = NULL;

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pz->pRepo, pz->iDagNum, &prb_leaves)  );
    SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_leaves, &count_leaves)  );
    if (count_leaves > 1)
    {
        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
    }

    if (1 == count_leaves)
    {
        SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, prb_leaves, &psz_hid_cs_leaf, NULL)  );
    }
    else
    {
        psz_hid_cs_leaf = NULL;
    }

    if (psz_hid_cs_leaf)
    {
        SG_ERR_CHECK(  SG_strdup(pCtx, psz_hid_cs_leaf, pp)  );
    }
    else
    {
        *pp = NULL;
    }

    // fall thru

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_leaves);
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingdb, begin_tx)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	sg_zingdb* pz = NULL;
    sg_statetxcombo* pcombo = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
	JSObject* jso = NULL;
	SG_zingtx* pZingTx = NULL;
    char* psz_hid_cs0 = NULL;
    char* psz_db_state_id = NULL;
    const char* psz_specified_userid = NULL;
    const char* psz_userid = NULL;
    SG_audit q;

	SG_NULLARGCHECK(psp);

	SG_ERR_CHECK(  SG_safeptr__unwrap__zingdb(pCtx, psp, &pz)  );

	SG_JS_BOOL_CHECK(  (0 == argc) || (1 == argc) || (2 == argc) );

	jso = JS_NewObject(cx, &sg_zingtx_class, NULL, NULL);

    if (0 == argc)
    {
        SG_ERR_CHECK(  sg_zingdb__get_leaf(pCtx, pz, &psz_db_state_id)  );
    }
    else
    {
        if (JSVAL_IS_NULL(argv[0]))
        {
            SG_ERR_CHECK(  sg_zingdb__get_leaf(pCtx, pz, &psz_db_state_id)  );
        }
        else
        {
            SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );
            SG_ERR_CHECK(  SG_strdup(pCtx, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &psz_db_state_id)  );
        }

        if (argc > 1)
        {
            SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[1])  );
            psz_specified_userid = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
        }
    }

    if (psz_specified_userid)
    {
        psz_userid = psz_specified_userid;
    }
    else
    {
        SG_ERR_CHECK(  SG_audit__init(pCtx, &q, pz->pRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
        psz_userid = q.who_szUserId;
    }

	SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pz->pRepo, pz->iDagNum, psz_userid, psz_db_state_id, &pZingTx)  );
    if (psz_db_state_id)
    {
        SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pZingTx, psz_db_state_id)  );
    }
    SG_NULLFREE(pCtx, psz_db_state_id);
    SG_ERR_CHECK(  sg_statetxcombo__alloc(pCtx, pz, pZingTx, &pcombo)  );
	SG_ERR_CHECK(  SG_safeptr__wrap__statetxcombo(pCtx, pcombo, &psp)  );
    JS_SetPrivate(cx, jso, psp);

	*rval = OBJECT_TO_JSVAL(jso);
	SG_NULLFREE(pCtx, psz_hid_cs0);
	return JS_TRUE;

fail:
	SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pZingTx)  );
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingdb, merge)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	sg_zingdb* pzstate = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
    char* psz_hid_cs0 = NULL;
    SG_varray* pva_violations = NULL;
    SG_varray* pva_log = NULL;
    SG_vhash* pvh = NULL;
    JSObject* jso = NULL;
    SG_audit q;
    const char* psz_leaf_0 = NULL;
    const char* psz_leaf_1 = NULL;
    SG_dagnode* pdn_merged = NULL;

	SG_NULLARGCHECK(psp);

	SG_ERR_CHECK(  SG_safeptr__unwrap__zingdb(pCtx, psp, &pzstate)  );

	SG_JS_BOOL_CHECK(  (0 == argc) || (2 == argc)  );

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh)  );
	SG_ERR_CHECK(  SG_audit__init(pCtx, &q, pzstate->pRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );

    if (2 == argc)
    {
        psz_leaf_0 = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
        psz_leaf_1 = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));

        SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_log)  );
        SG_ERR_CHECK(  SG_zingmerge__attempt_automatic_merge(pCtx,
                    pzstate->pRepo,
                    pzstate->iDagNum,
                    &q,
                    psz_leaf_0,
                    psz_leaf_1,
                    &pdn_merged,
                    &pva_violations,
                    pva_log
                    )  );
        if (pdn_merged)
        {
            const char* psz_hid = NULL;

            SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pdn_merged, &psz_hid)  );
            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "csid", psz_hid)  );
            SG_DAGNODE_NULLFREE(pCtx, pdn_merged);
        }
        SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh, "log", &pva_log)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_zing__get_leaf__merge_if_necessary(pCtx, pzstate->pRepo, &q, pzstate->iDagNum, &psz_hid_cs0, &pva_violations, &pva_log)  );
        if (psz_hid_cs0)
        {
            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "csid", psz_hid_cs0)  );
            SG_NULLFREE(pCtx, psz_hid_cs0);
        }

        if (pva_log)
        {
            SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh, "log", &pva_log)  );
        }
    }

    if (pva_violations)
    {
        SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh, "errors", &pva_violations)  );
    }

    SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pvh, &jso)   );
    SG_VHASH_NULLFREE(pCtx, pvh);
    *rval = OBJECT_TO_JSVAL(jso);

	return JS_TRUE;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
    SG_DAGNODE_NULLFREE(pCtx, pdn_merged);
    SG_NULLFREE(pCtx, psz_hid_cs0);
    SG_VARRAY_NULLFREE(pCtx, pva_violations);
    SG_VARRAY_NULLFREE(pCtx, pva_log);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingtx, abort)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    sg_statetxcombo* pcombo = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
	SG_NULLARGCHECK(psp);

	SG_UNUSED(argv);
	SG_UNUSED(argc);
	SG_UNUSED(rval);

	SG_ERR_CHECK(  SG_safeptr__unwrap__statetxcombo(pCtx, psp, &pcombo)  );
	SG_ERR_CHECK(  SG_zing__abort_tx(pCtx, &pcombo->pztx)  );
    SG_NULLFREE(pCtx, pcombo);
	SG_SAFEPTR_NULLFREE(pCtx, psp);

	return JS_TRUE;

fail:
	SG_SAFEPTR_NULLFREE(pCtx, psp);
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingtx, commit)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    sg_statetxcombo* pcombo = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
	SG_changeset* pcs = NULL;
	SG_dagnode* pdn = NULL;
	const char* psz_hid_cs0 = NULL;
    SG_varray* pva_violations = NULL;
    SG_vhash* pvh = NULL;
    JSObject* jso = NULL;
    SG_int64 when = -1;

    SG_UNUSED(argc);
    SG_UNUSED(argv);

	SG_NULLARGCHECK(psp);

	SG_ERR_CHECK(  SG_safeptr__unwrap__statetxcombo(pCtx, psp, &pcombo)  );

	if (argc == 1)
	{
		JSString *jss = JSVAL_TO_STRING(argv[0]);
		SG_ERR_CHECK(  SG_int64__parse(pCtx, &when, JS_GetStringBytes(jss))  );
	}
	else
	{
		// TODO it would probably be nifty to allow this timestamp to be set in JS code
		SG_time__get_milliseconds_since_1970_utc(pCtx, &when);
	}

	SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, when, &pcombo->pztx, &pcs, &pdn, &pva_violations)  );

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh)  );

    SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, "when", when)  );

    if (pva_violations)
        SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh, "errors", &pva_violations)  );

    if (pdn)
	{
        SG_int32 generation = -1;

		SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pdn, &psz_hid_cs0)  );
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "csid", psz_hid_cs0)  );

		SG_ERR_CHECK(  SG_dagnode__get_generation(pCtx, pdn, &generation)  );
        SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, "generation", (SG_int64) generation)  );
	}

    SG_ERR_CHECK(  sg_jsglue__vhash_to_jsobject(pCtx, cx, pvh, &jso)   );
    SG_VHASH_NULLFREE(pCtx, pvh);
    *rval = OBJECT_TO_JSVAL(jso);

    if (pdn)
    {
        SG_NULLFREE(pCtx, pcombo);
        SG_SAFEPTR_NULLFREE(pCtx, psp);
    }

	SG_DAGNODE_NULLFREE(pCtx, pdn);
	SG_CHANGESET_NULLFREE(pCtx, pcs);

	return JS_TRUE;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_violations);
	SG_DAGNODE_NULLFREE(pCtx, pdn);
	SG_CHANGESET_NULLFREE(pCtx, pcs);

	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingtx, new_record)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    sg_statetxcombo* pcombo = NULL;
	JSObject* jso = NULL;
	JSObject* jso_init = NULL;
	SG_zingrecord* pRec = NULL;
	SG_safeptr* psrec = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
    JSIdArray* properties = NULL;
    SG_zingfieldattributes* pzfa = NULL;
    SG_zinglinkattributes* pzla = NULL;
    SG_zinglinksideattributes* pzlsa_mine = NULL;
    SG_zinglinksideattributes* pzlsa_other = NULL;

	SG_NULLARGCHECK(psp);

	SG_ERR_CHECK(  SG_safeptr__unwrap__statetxcombo(pCtx, psp, &pcombo)  );

	SG_JS_BOOL_CHECK(  (1 == argc) || (2 == argc)  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );

	jso = JS_NewObject(cx, &sg_zingrecord_class, NULL, NULL);

	SG_ERR_CHECK(  SG_zingtx__create_new_record(pCtx, pcombo->pztx, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &pRec)  );

    if (2 == argc)
    {
        SG_int32 i = 0;

        SG_JS_BOOL_CHECK(  JSVAL_IS_OBJECT(argv[1])  );
        jso_init = JSVAL_TO_OBJECT(argv[1]);

        SG_JS_BOOL_CHECK(  properties = JS_Enumerate(cx, jso_init)  );

        for (i=0; i<properties->length; i++)
        {
            jsval jv_name;
            jsval jv_value;
            JSString* str_name = NULL;
            const char* psz_name = NULL;

            SG_JS_BOOL_CHECK(  JS_IdToValue(cx, properties->vector[i], &jv_name)  );
            SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(jv_name)  );

            str_name = JSVAL_TO_STRING(jv_name);
            psz_name = JS_GetStringBytes(str_name);

            SG_JS_BOOL_CHECK(  JS_GetProperty(cx, jso_init, psz_name, &jv_value)  );

            SG_ERR_CHECK(  SG_zingrecord__lookup_name(pCtx, pRec, psz_name, &pzfa, &pzla, &pzlsa_mine, &pzlsa_other)  );

            if (pzfa)
            {
                SG_ERR_CHECK(  sg_zing_jsglue__set_field(cx, pCtx, pRec, pzfa, psz_name, &jv_value)  );
            }
            else if (pzla)
            {
                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
            }
            else
            {
                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
            }

            SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
            SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_mine);
            SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_other);
        }

        JS_DestroyIdArray(cx, properties);
    }

	SG_ERR_CHECK(  SG_safeptr__wrap__zingrecord(pCtx, pRec, &psrec)  );
	JS_SetPrivate(cx, jso, psrec);

	*rval = OBJECT_TO_JSVAL(jso);
	return JS_TRUE;

fail:
    SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx, pzla);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_mine);
    SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx, pzlsa_other);

	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;

}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingtx, open_record)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    sg_statetxcombo* pcombo = NULL;
	JSObject* jso = NULL;
	SG_zingrecord* pRec = NULL;
	SG_safeptr* psrec = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);

	SG_NULLARGCHECK(psp);
	SG_ERR_CHECK(  SG_safeptr__unwrap__statetxcombo(pCtx, psp, &pcombo)  );

    if (SG_DAGNUM__IS_NO_RECID(pcombo->pzstate->iDagNum))
    {
        SG_ERR_THROW(  SG_ERR_ZING_NO_RECID  );
    }

	SG_JS_BOOL_CHECK(  1 == argc  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );

	jso = JS_NewObject(cx, &sg_zingrecord_class, NULL, NULL);

	SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pcombo->pztx, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &pRec)  );
	SG_ERR_CHECK(  SG_safeptr__wrap__zingrecord(pCtx, pRec, &psrec)  );
	JS_SetPrivate(cx, jso, psrec);
	*rval = OBJECT_TO_JSVAL(jso);
	return JS_TRUE;
fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;

}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zingtx, delete_record)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
    sg_statetxcombo* pcombo = NULL;
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
	SG_NULLARGCHECK(psp);
	SG_ERR_CHECK(  SG_safeptr__unwrap__statetxcombo(pCtx, psp, &pcombo)  );

	SG_UNUSED(rval);

	SG_JS_BOOL_CHECK(  1 == argc  );
	SG_JS_BOOL_CHECK(  JSVAL_IS_STRING(argv[0])  );

    if (SG_DAGNUM__IS_NO_RECID(pcombo->pzstate->iDagNum))
    {
        SG_ERR_THROW(  SG_ERR_ZING_NO_RECID  );
    }

	SG_ERR_CHECK(  SG_zingtx__delete_record(pCtx, pcombo->pztx, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])))  );
	return JS_TRUE;
fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;

}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zinglinkcollection, add)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
    sg_zinglinkcollection* pzl = NULL;

    SG_UNUSED(rval);

	SG_NULLARGCHECK(psp);

	SG_JS_BOOL_CHECK(  1 == argc  );

	SG_ERR_CHECK(  SG_safeptr__unwrap__zinglinkcollection(pCtx, psp, &pzl)  );

    if (JSVAL_IS_STRING(argv[0]))
    {
        // TODO this should be a recid.  validate it?
        SG_ERR_CHECK(  SG_zingrecord__add_link_and_overwrite_other_if_singular(pCtx, pzl->pzr, pzl->psz_link_name_mine, JS_GetStringBytes(JSVAL_TO_STRING((argv[0]))))   );
    }
    else if (JSVAL_IS_OBJECT(argv[0]))
    {
        SG_safeptr* psp2 = NULL;
        SG_zingrecord* pzr2 = NULL;
        const char* pszOtherRecID = NULL;

        psp2 = sg_zing_jsglue__get_object_private(cx, JSVAL_TO_OBJECT(argv[0]));
        // TODO what if this object isn't a zingrecord.  is this the
        // way we want fo fail?
        SG_ERR_CHECK(  SG_safeptr__unwrap__zingrecord(pCtx, psp2, &pzr2)  );
        SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzr2, &pszOtherRecID)  );
        SG_ERR_CHECK(  SG_zingrecord__add_link_and_overwrite_other_if_singular(pCtx, pzl->pzr, pzl->psz_link_name_mine, pszOtherRecID)   );
    }

	return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zinglinkcollection, remove)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
    sg_zinglinkcollection* pzl = NULL;
    const char* psz_link = NULL;
    const char* psz_hid_rec = NULL;
    SG_zingtx* pztx = NULL;

    SG_UNUSED(rval);

	SG_NULLARGCHECK(psp);

	SG_JS_BOOL_CHECK(  1 == argc  );

	SG_ERR_CHECK(  SG_safeptr__unwrap__zinglinkcollection(pCtx, psp, &pzl)  );

    if (JSVAL_IS_STRING(argv[0]))
    {
        // TODO this should be a recid.  validate it?
        SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzl->prb_recid_to_link, JS_GetStringBytes(JSVAL_TO_STRING((argv[0]))), NULL, (void**) &psz_link)  );
    }
    else if (JSVAL_IS_OBJECT(argv[0]))
    {
        SG_safeptr* psp2 = NULL;
        SG_zingrecord* pzr2 = NULL;
        const char* pszOtherRecID = NULL;

        psp2 = sg_zing_jsglue__get_object_private(cx, JSVAL_TO_OBJECT(argv[0]));
        // TODO what if this object isn't a zingrecord.  is this the
        // way we want fo fail?
        SG_ERR_CHECK(  SG_safeptr__unwrap__zingrecord(pCtx, psp2, &pzr2)  );
        SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzr2, &pszOtherRecID)  );
        SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzl->prb_recid_to_link, pszOtherRecID, NULL, (void**) &psz_link)  );
    }

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzl->prb, psz_link, NULL, (void**) &psz_hid_rec)  );

    SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pzl->pzr, &pztx)  );
    SG_ERR_CHECK(  sg_zingtx__delete_link__packed(pCtx, pztx, psz_link, psz_hid_rec)   );

	return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

SG_ZING_JSGLUE_METHOD_PROTOTYPE(zinglinkcollection, to_array)
{
	SG_context * pCtx = SG_jsglue__get_clean_sg_context(cx);
	SG_safeptr* psp = sg_zing_jsglue__get_object_private(cx, obj);
    sg_zinglinkcollection* pzl = NULL;
    JSObject* jso = NULL;
    JSString* pjs = NULL;
    SG_uint32 cur = 0;
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_link = NULL;

    SG_UNUSED(argv);

	SG_NULLARGCHECK(psp);

	SG_JS_BOOL_CHECK(  0 == argc  );

	SG_ERR_CHECK(  SG_safeptr__unwrap__zinglinkcollection(pCtx, psp, &pzl)  );

    jso = JS_NewArrayObject(cx, 0, NULL);

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pzl->prb, &b, &psz_link, NULL)  );
    cur = 0;
    while (b)
    {
        char buf_recid_from[SG_GID_BUFFER_LENGTH];
        char buf_recid_to[SG_GID_BUFFER_LENGTH];
        jsval jv;

        SG_ERR_CHECK(  SG_zinglink__unpack(pCtx, psz_link, buf_recid_from, buf_recid_to, NULL)  );

        if (pzl->b_from)
        {
            pjs = JS_NewStringCopyZ(cx, buf_recid_to);
        }
        else
        {
            pjs = JS_NewStringCopyZ(cx, buf_recid_from);
        }

        jv = STRING_TO_JSVAL(pjs);
        SG_JS_BOOL_CHECK(  JS_SetElement(cx, jso, cur++, &jv)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    *rval = OBJECT_TO_JSVAL(jso);

    return JS_TRUE;

fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return JS_FALSE;
}

/* ---------------------------------------------------------------- */
/* Now the method tables */
/* ---------------------------------------------------------------- */

static JSPropertySpec sg_zingtx_properties[] =
{
    {NULL,0,0,NULL,NULL}
};

static JSFunctionSpec sg_zingdb_methods[] =
{
    {"begin_tx", 			SG_ZING_JSGLUE_METHOD_NAME(zingdb,begin_tx),0,0,0},
    {"get_template", 		SG_ZING_JSGLUE_METHOD_NAME(zingdb,get_template),0,0,0},
    {"get_leaves", 		    SG_ZING_JSGLUE_METHOD_NAME(zingdb,get_leaves),0,0,0},
    {"get_record", 			SG_ZING_JSGLUE_METHOD_NAME(zingdb,get_record),1,0,0},
    {"query", 			    SG_ZING_JSGLUE_METHOD_NAME(zingdb,query),1,0,0},
    {"query_across_states", 			    SG_ZING_JSGLUE_METHOD_NAME(zingdb,query_across_states),1,0,0},
    {"get_history", 	    SG_ZING_JSGLUE_METHOD_NAME(zingdb,get_history),1,0,0},
    {"merge", 			    SG_ZING_JSGLUE_METHOD_NAME(zingdb,merge),0,0,0},
    {NULL,NULL,0,0,0}
};

static JSFunctionSpec sg_zingtx_methods[] =
{
    {"abort", 			  	SG_ZING_JSGLUE_METHOD_NAME(zingtx,abort),0,0,0},
    {"commit", 			  	SG_ZING_JSGLUE_METHOD_NAME(zingtx,commit),0,0,0},
    {"new_record", 	        SG_ZING_JSGLUE_METHOD_NAME(zingtx,new_record),1,0,0},
    {"open_record", 		SG_ZING_JSGLUE_METHOD_NAME(zingtx,open_record),1,0,0},
    {"delete_record", 		SG_ZING_JSGLUE_METHOD_NAME(zingtx,delete_record),1,0,0},
    {"get_template", 		SG_ZING_JSGLUE_METHOD_NAME(zingtx,get_template),0,0,0},
    {"set_template", 		SG_ZING_JSGLUE_METHOD_NAME(zingtx,set_template),1,0,0},
    {NULL,NULL,0,0,0}
};

static JSFunctionSpec sg_zinglinkcollection_methods[] =
{
    {"add", 			  	SG_ZING_JSGLUE_METHOD_NAME(zinglinkcollection,add),1,0,0},
    {"remove", 			  	SG_ZING_JSGLUE_METHOD_NAME(zinglinkcollection,remove),1,0,0},
    {"to_array", 			  	SG_ZING_JSGLUE_METHOD_NAME(zinglinkcollection,to_array),0,0,0},
    {NULL,NULL,0,0,0}
};

/* ---------------------------------------------------------------- */
/* This is the function which sets everything up */
/* ---------------------------------------------------------------- */

void SG_zing_jsglue__install_scripting_api(SG_context * pCtx,
									  JSContext *cx, JSObject *glob)
{
	SG_JS_BOOL_CHECK(  JS_InitClass(
            cx,
            glob,
            NULL, /* parent proto */
            &sg_zingtx_class,
            NULL,
            0, /* nargs */
            sg_zingtx_properties,
            sg_zingtx_methods,
            NULL,  /* static properties */
            NULL   /* static methods */
            )  );
	SG_JS_BOOL_CHECK(  JS_InitClass(
            cx,
            glob,
            NULL, /* parent proto */
            &sg_zingrecord_class,
	    	NULL,
            0, /* nargs */
            NULL,
            NULL,
            NULL,  /* static properties */
            NULL   /* static methods */
            )  );
	SG_JS_BOOL_CHECK(  JS_InitClass(
            cx,
            glob,
            NULL, /* parent proto */
            &sg_zinglinkcollection_class,
	    	NULL,
            0, /* nargs */
            NULL,
            sg_zinglinkcollection_methods,
            NULL,  /* static properties */
            NULL   /* static methods */
            )  );
	SG_JS_BOOL_CHECK(  JS_InitClass(
            cx,
            glob,
            NULL, /* parent proto */
            &sg_zingdb_class,
	    	zingdb_constructor,
            0, /* nargs */
            NULL,
            sg_zingdb_methods,
            NULL,  /* static properties */
            NULL   /* static methods */
            )  );
	return;
fail:
	SG_jsglue__report_sg_error(pCtx,cx);	// DO NOT SG_ERR_IGNORE() THIS
	return;
}

