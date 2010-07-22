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

#include "unittests.h"

#define U0026_MESSY "\"welcome \"to the\r\nmid'dle of the film'\"\t"

void u0026_jsonparser__create_1(SG_context* pCtx, SG_string* pStr)
{
	SG_jsonwriter* pjson = NULL;
	char* pid = NULL;

	SG_ERR_CHECK(  SG_jsonwriter__alloc(pCtx, &pjson, pStr)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_start_object(pCtx, pjson)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__string__sz(pCtx, pjson, "hello", "world")  );

	SG_ERR_CHECK(  SG_tid__alloc(pCtx, &pid)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__string__sz(pCtx, pjson, "eyedee", pid)  );
	SG_NULLFREE(pCtx, pid);

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__int64(pCtx, pjson, "x", 5)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__string__sz(pCtx, pjson, "messy", U0026_MESSY)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_end_object(pCtx, pjson)  );

	SG_JSONWRITER_NULLFREE(pCtx, pjson);
	pjson = NULL;

	return;
fail:
	return;
}

void u0026_jsonparser__create_2(SG_context* pCtx, SG_string* pStr)
{
	SG_jsonwriter* pjson = NULL;
	SG_vhash* pvh = NULL;
	SG_varray* pva = NULL;
	SG_uint32 i;
	char* pid = NULL;

	SG_ERR_CHECK(  SG_jsonwriter__alloc(pCtx, &pjson, pStr)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_start_object(pCtx, pjson)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__string__sz(pCtx, pjson, "hello", "world")  );

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__int64(pCtx, pjson, "x", 5)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__double(pCtx, pjson, "pi", 3.14159)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__bool(pCtx, pjson, "b1", SG_TRUE)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__bool(pCtx, pjson, "b2", SG_FALSE)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_begin_pair(pCtx, pjson, "furball")  );

	SG_ERR_CHECK(  SG_jsonwriter__write_start_array(pCtx, pjson)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_element__string__sz(pCtx, pjson, "plok")  );

	SG_ERR_CHECK(  SG_jsonwriter__write_element__double(pCtx, pjson, 47.567)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_element__int64(pCtx, pjson, 22222)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_element__null(pCtx, pjson)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_element__bool(pCtx, pjson, SG_TRUE)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_element__bool(pCtx, pjson, SG_FALSE)  );

	SG_ERR_CHECK(  SG_gid__alloc(pCtx, &pid)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_element__string__sz(pCtx, pjson, pid)  );
	SG_NULLFREE(pCtx, pid);

	SG_ERR_CHECK(  SG_jsonwriter__write_end_array(pCtx, pjson)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__null(pCtx, pjson, "nope")  );

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__string__sz(pCtx, pjson, "messy", U0026_MESSY)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "fried", "tomatoes")  );
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, "q", 333)  );

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__vhash(pCtx, pjson, "sub", pvh)  );

	SG_VHASH_NULLFREE(pCtx, pvh);

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );

	for (i=0; i<1000; i++)
	{
		SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, "plok")  );

		SG_ERR_CHECK(  SG_varray__append__int64(pCtx, pva, 22)  );

		SG_ERR_CHECK(  SG_varray__append__double(pCtx, pva, 1.414)  );

		SG_ERR_CHECK(  SG_varray__append__bool(pCtx, pva, SG_TRUE)  );

		SG_ERR_CHECK(  SG_varray__append__bool(pCtx, pva, SG_FALSE)  );

		SG_ERR_CHECK(  SG_varray__append__null(pCtx, pva)  );
	}

	SG_ERR_CHECK(  SG_jsonwriter__write_pair__varray(pCtx, pjson, "a", pva)  );

	SG_VARRAY_NULLFREE(pCtx, pva);

	SG_ERR_CHECK(  SG_jsonwriter__write_end_object(pCtx, pjson)  );

	SG_JSONWRITER_NULLFREE(pCtx, pjson);

	return;
fail:
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_VARRAY_NULLFREE(pCtx, pva);
	SG_JSONWRITER_NULLFREE(pCtx, pjson);
}

struct u0026_ctx
{
	char key[256];
};

void u0026_jcb(SG_UNUSED_PARAM(SG_context* pCtx), void* ctx, int type, const SG_jsonparser_value* value)
{
	struct u0026_ctx* p = (struct u0026_ctx*) ctx;

	SG_UNUSED(pCtx);

    switch(type) {
    case SG_JSONPARSER_TYPE_ARRAY_BEGIN:
		p->key[0] = 0;
        break;
    case SG_JSONPARSER_TYPE_ARRAY_END:
		p->key[0] = 0;
        break;
	case SG_JSONPARSER_TYPE_OBJECT_BEGIN:
		p->key[0] = 0;
        break;
    case SG_JSONPARSER_TYPE_OBJECT_END:
		p->key[0] = 0;
        break;
    case SG_JSONPARSER_TYPE_INTEGER:
		if (0 == strcmp(p->key, "x"))
		{
			VERIFY_COND("x", 5 == value->vu.integer_value);
		}
		p->key[0] = 0;
        break;
    case SG_JSONPARSER_TYPE_FLOAT:
		p->key[0] = 0;
        break;
    case SG_JSONPARSER_TYPE_NULL:
		p->key[0] = 0;
        break;
    case SG_JSONPARSER_TYPE_TRUE:
		p->key[0] = 0;
        break;
    case SG_JSONPARSER_TYPE_FALSE:
		p->key[0] = 0;
        break;
    case SG_JSONPARSER_TYPE_KEY:
		strcpy(p->key, value->vu.str.value);
        break;
    case SG_JSONPARSER_TYPE_STRING:
		if (0 == strcmp(p->key, "hello"))
		{
			VERIFY_COND("hello:world", 0 == strcmp("world", value->vu.str.value));
		}
		else if (0 == strcmp(p->key, "messy"))
		{
			VERIFY_COND("messy", 0 == strcmp(U0026_MESSY, value->vu.str.value));
		}
		p->key[0] = 0;
        break;
    default:
        SG_ASSERT(0);
        break;
    }
}

void u0026_jsonparser__verify(SG_context * pCtx, SG_string* pstr)
{
	const char *psz = SG_string__sz(pstr);
	SG_jsonparser* jc = NULL;
	struct u0026_ctx ctx;

	ctx.key[0] = 0;

	SG_ERR_CHECK(  SG_jsonparser__alloc(pCtx, &jc, u0026_jcb, &ctx)  );

    SG_ERR_CHECK(  SG_jsonparser__chars(pCtx, jc, psz)  );
	if (!SG_context__has_err(pCtx))
	{
		SG_jsonparser__done(pCtx, jc);
		VERIFY_COND("JSON_checker_done", !SG_context__has_err(pCtx));
	}

fail:
	SG_JSONPARSER_NULLFREE(pCtx, jc);
}

void u0026_jsonparser__test_jsonparser(SG_context * pCtx)
{
	SG_string* pstr = NULL;

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );

	u0026_jsonparser__create_1(pCtx, pstr);
	VERIFY_ERR_CHECK_DISCARD(  u0026_jsonparser__verify(pCtx, pstr)  );

fail:
	SG_STRING_NULLFREE(pCtx, pstr);
}

void u0026_jsonparser__test_jsonparser_vhash_1(SG_context * pCtx)
{
	SG_string* pstr1 = NULL;
	SG_string* pstr2 = NULL;
	SG_vhash* pvh = NULL;

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr1)  );
	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr2)  );

	VERIFY_ERR_CHECK(  u0026_jsonparser__create_1(pCtx, pstr1)  );

	SG_VHASH__ALLOC__FROM_JSON(pCtx, &pvh, SG_string__sz(pstr1));
	VERIFY_COND("from_json", !SG_context__has_err(pCtx));
	VERIFY_COND("from_json", pvh);

	SG_context__err_reset(pCtx);
	SG_vhash__to_json(pCtx, pvh, pstr2);
	VERIFY_COND("from_json", !SG_context__has_err(pCtx));

	// TODO do some checks

fail:
	SG_STRING_NULLFREE(pCtx, pstr1);
	SG_STRING_NULLFREE(pCtx, pstr2);
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void u0026_jsonparser__test_jsonparser_vhash_2(SG_context * pCtx)
{
	SG_string* pstr1 = NULL;
	SG_string* pstr2 = NULL;
	SG_vhash* pvh = NULL;

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr1)  );
	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr2)  );

	VERIFY_ERR_CHECK(  u0026_jsonparser__create_2(pCtx, pstr1)  );

	SG_VHASH__ALLOC__FROM_JSON(pCtx, &pvh, SG_string__sz(pstr1));
	VERIFY_COND("from_json", !SG_context__has_err(pCtx));
	VERIFY_COND("from_json", pvh);

	SG_context__err_reset(pCtx);
	SG_vhash__to_json(pCtx, pvh, pstr2);
	VERIFY_COND("from_json", !SG_context__has_err(pCtx));

	// TODO do some checks

fail:
	SG_STRING_NULLFREE(pCtx, pstr1);
	SG_STRING_NULLFREE(pCtx, pstr2);
	SG_VHASH_NULLFREE(pCtx, pvh);
}

TEST_MAIN(u0026_jsonparser)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0026_jsonparser__test_jsonparser(pCtx)  );
	BEGIN_TEST(  u0026_jsonparser__test_jsonparser_vhash_1(pCtx)  );
	BEGIN_TEST(  u0026_jsonparser__test_jsonparser_vhash_2(pCtx)  );

	TEMPLATE_MAIN_END;
}

