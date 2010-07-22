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
#include <math.h>
#include "unittests.h"

//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0077_jsondb)
#define MyDcl(name)				u0077_jsondb__##name
#define MyFn(name)				u0077_jsondb__##name

static SG_bool MyFn(dblsEqual)(double x, double y)
{
	return fabs(x - y) < 0.0000000001;
}

static void MyFn(_create_db)(
	SG_context* pCtx,
	const SG_pathname* pPathTop,
	SG_jsondb** ppJsondb)
{
	SG_jsondb* pJsonDb = NULL;
	SG_pathname* pPathDbFile = NULL;
	char bufDbName[SG_TID_MAX_BUFFER_LENGTH];

	SG_ASSERT(SG_TID_MAX_BUFFER_LENGTH > 18);
	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufDbName, sizeof(bufDbName), 10)  );
	VERIFY_ERR_CHECK(  SG_strcat(pCtx, bufDbName, sizeof(bufDbName), ".jsondb")  );
	VERIFY_ERR_CHECK(  SG_pathname__alloc__pathname_sz(pCtx, &pPathDbFile, pPathTop, bufDbName)  );

	INFO2("Jsondb file", SG_pathname__sz(pPathDbFile));
	VERIFY_ERR_CHECK(  SG_console__flush(pCtx, SG_CS_STDERR)  );

	VERIFY_ERR_CHECK(  SG_jsondb__create_or_open(pCtx, pPathDbFile, "newobj1", &pJsonDb)  );
	SG_JSONDB_NULLFREE(pCtx, pJsonDb);

	VERIFY_ERR_CHECK(  SG_jsondb__create_or_open(pCtx, pPathDbFile, "newobj2", &pJsonDb)  );

	SG_RETURN_AND_NULL(pJsonDb, ppJsondb);

	/* fall through */
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathDbFile);
	SG_JSONDB_NULLFREE(pCtx, pJsonDb);
}

static void MyFn(int64_or_double)(SG_context* pCtx, SG_jsondb* pJsonDb)
{
	SG_int64 i64;
	double dbl;

	VERIFY_ERR_CHECK(  SG_jsondb__add__object(pCtx, pJsonDb, "int64_or_double", NULL)  );

	VERIFY_ERR_CHECK(  SG_jsondb__add__int64(pCtx, pJsonDb, "/max_i64", SG_TRUE, SG_INT64_MAX)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__int64_or_double(pCtx, pJsonDb, "/max_i64", &i64)  );
	VERIFY_COND("", i64 == SG_INT64_MAX);
	i64=5;
	VERIFY_ERR_CHECK(  SG_jsondb__get__int64(pCtx, pJsonDb, "/max_i64", &i64)  );
	VERIFY_COND("", i64 == SG_INT64_MAX);

	VERIFY_ERR_CHECK(  SG_jsondb__add__int64(pCtx, pJsonDb, "/0_i64", SG_FALSE, 0)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__int64_or_double(pCtx, pJsonDb, "/0_i64", &i64)  );
	VERIFY_COND("", i64 == 0);
	i64=5;
	VERIFY_ERR_CHECK(  SG_jsondb__get__int64(pCtx, pJsonDb, "/0_i64", &i64)  );
	VERIFY_COND("", i64 == 0);

	// Round-tripping an actual floating point number always exceeds the int64 limit.
	VERIFY_ERR_CHECK(  SG_jsondb__add__double(pCtx, pJsonDb, "/easy_pi", SG_FALSE, 3.14159)  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__get__int64_or_double(pCtx, pJsonDb, "/easy_pi", &i64),
		SG_ERR_VARIANT_INVALIDTYPE);
	VERIFY_ERR_CHECK(  SG_jsondb__get__double(pCtx, pJsonDb, "/easy_pi", &dbl)  );
	VERIFY_COND("", MyFn(dblsEqual)(dbl, 3.14159));

	VERIFY_ERR_CHECK(  SG_jsondb__add__double(pCtx, pJsonDb, "/dbl1", SG_FALSE, 12345)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__int64_or_double(pCtx, pJsonDb, "/dbl1", &i64)  );
	VERIFY_COND("", MyFn(dblsEqual)((double)i64, 12345));

	VERIFY_ERR_CHECK(  SG_jsondb__add__double(pCtx, pJsonDb, "/dbl0", SG_FALSE, 0)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__int64_or_double(pCtx, pJsonDb, "/dbl0", &i64)  );
	VERIFY_COND("", MyFn(dblsEqual)((double)i64, 0));

fail:
	;
}

static void MyFn(object_and_path_basics)(SG_context* pCtx, SG_jsondb* pJsonDb)
{
	SG_bool b;
	const char* psz = NULL;
	char* psz2 = NULL;
	SG_variant* pv = NULL;
	SG_int64 i64;

	VERIFY_ERR_CHECK(  SG_jsondb__set_current_object(pCtx, pJsonDb, NULL)  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(SG_jsondb__has(pCtx, pJsonDb, "/", &b),
		SG_ERR_JSONDB_NO_CURRENT_OBJECT);

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(SG_jsondb__set_current_object(pCtx, pJsonDb, "lalala"),
		SG_ERR_NOT_FOUND);

	VERIFY_ERR_CHECK(  SG_alloc1(pCtx, pv)  );
	pv->type = SG_VARIANT_TYPE_INT64;
	pv->v.val_int64 = 7;
	VERIFY_ERR_CHECK(  SG_jsondb__add__object(pCtx, pJsonDb, "add-obj-with-variant", pv)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__int64(pCtx, pJsonDb, "/", &i64)  );
	VERIFY_COND("", i64==7);
	SG_NULLFREE(pCtx, pv);

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__get__sz(pCtx, pJsonDb, "/", &psz2),
		SG_ERR_VARIANT_INVALIDTYPE);

	VERIFY_ERR_CHECK(  SG_jsondb__add__object(pCtx, pJsonDb, "basics", NULL)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get_current_object_name(pCtx, pJsonDb, &psz)  );
	VERIFY_COND("", strcmp(psz, "basics") == 0);

	VERIFY_ERR_CHECK(  SG_jsondb__set_current_object(pCtx, pJsonDb, NULL)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get_current_object_name(pCtx, pJsonDb, &psz)  );
	VERIFY_COND("", !psz);

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(SG_jsondb__add__object(pCtx, pJsonDb, "basics", NULL),
		SG_ERR_JSONDB_OBJECT_ALREADY_EXISTS);
	VERIFY_ERR_CHECK(  SG_jsondb__get_current_object_name(pCtx, pJsonDb, &psz)  );
	VERIFY_COND("", !psz);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(SG_jsondb__has(pCtx, pJsonDb, "/", &b),
		SG_ERR_JSONDB_NO_CURRENT_OBJECT);

	VERIFY_ERR_CHECK(  SG_jsondb__set_current_object(pCtx, pJsonDb, "basics")  );
	VERIFY_ERR_CHECK(  SG_jsondb__get_current_object_name(pCtx, pJsonDb, &psz)  );
	VERIFY_COND("", strcmp(psz, "basics") == 0);
	psz = NULL;

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(SG_jsondb__has(pCtx, pJsonDb, "blah", &b), // not rooted
		SG_ERR_JSONDB_INVALID_PATH);
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/", &b)  );
	VERIFY_COND("", !b);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(SG_jsondb__get__object(pCtx, pJsonDb, &pv),
		SG_ERR_NOT_FOUND);

	VERIFY_ERR_CHECK(  SG_jsondb__add__int64(pCtx, pJsonDb, "/", SG_FALSE, 7)  );
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/", &b)  );
	VERIFY_COND("", b);
	VERIFY_ERR_CHECK(  SG_jsondb__get__object(pCtx, pJsonDb, &pv)  );
	VERIFY_COND("", pv->type == SG_VARIANT_TYPE_INT64);
	VERIFY_COND("", pv->v.val_int64 == 7);

	VERIFY_ERR_CHECK(  SG_jsondb__update__vhash(pCtx, pJsonDb, "/", SG_FALSE, NULL)  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__add__null(pCtx, pJsonDb, "//test", SG_TRUE),
		SG_ERR_JSONDB_INVALID_PATH);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__add__null(pCtx, pJsonDb, "/test//x", SG_TRUE),
		SG_ERR_JSONDB_INVALID_PATH);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__add__null(pCtx, pJsonDb, "/test/x/y//", SG_TRUE),
		SG_ERR_JSONDB_INVALID_PATH);

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__sz(pCtx, pJsonDb, "/trailingslash/", SG_FALSE, "x")  );
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/trailingslash", &b)  );
	VERIFY_COND("", b);
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/trailingslash/", &b)  );
	VERIFY_COND("", b);
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/trailingslash\\/", &b)  );
	VERIFY_COND("", !b);

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__sz(pCtx, pJsonDb, "/trailingslash\\/", SG_FALSE, "y")  );
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/trailingslash\\/", &b)  );
	VERIFY_COND("", b);

	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/trailingslash", &psz2)  );
	VERIFY_COND("", strcmp("x", psz2)==0);
	SG_NULLFREE(pCtx, psz2);
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/trailingslash/", &psz2)  );
	VERIFY_COND("", strcmp("x", psz2)==0);
	SG_NULLFREE(pCtx, psz2);
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/trailingslash\\/", &psz2)  );
	VERIFY_COND("", strcmp("y", psz2)==0);
	SG_NULLFREE(pCtx, psz2);

	VERIFY_ERR_CHECK(  SG_jsondb__remove__object(pCtx, pJsonDb)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get_current_object_name(pCtx, pJsonDb, &psz)  );
	VERIFY_COND("", !psz);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(SG_jsondb__has(pCtx, pJsonDb, "/", &b),
		SG_ERR_JSONDB_NO_CURRENT_OBJECT);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(SG_jsondb__set_current_object(pCtx, pJsonDb, "basics"),
		SG_ERR_NOT_FOUND);

	/* fall through */
fail:
	SG_NULLFREE(pCtx, psz);
	SG_NULLFREE(pCtx, psz2);
	SG_NULLFREE(pCtx, pv);
}

static void MyFn(simple_types)(SG_context* pCtx, SG_jsondb* pJsonDb)
{
	char* psz = NULL;
	SG_uint32 i32;
	SG_int64 i64;
	double dbl;
	SG_bool b;
	SG_uint16 type;
	SG_variant* pv = NULL;

	VERIFY_ERR_CHECK(  SG_jsondb__add__object(pCtx, pJsonDb, "simple_types", NULL)  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(SG_jsondb__count(pCtx, pJsonDb, "/", &i32), SG_ERR_NOT_FOUND);

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__sz(pCtx, pJsonDb, "/sz", SG_TRUE, "test")  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/sz", &psz)  );
	VERIFY_COND("", strcmp(psz, "test") == 0);
	SG_NULLFREE(pCtx, psz);
	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/sz", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_SZ);
	VERIFY_ERR_CHECK(  SG_jsondb__get__variant(pCtx, pJsonDb, "/sz", &pv)  );
	VERIFY_COND("", pv->type == SG_VARIANT_TYPE_SZ);
	VERIFY_COND("", strcmp(pv->v.val_sz, "test") == 0);
	SG_NULLFREE(pCtx, pv->v.val_sz);
	SG_NULLFREE(pCtx, pv);

	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/", &i32)  );
	VERIFY_COND("", i32 == 1);

	psz = NULL;
	VERIFY_ERR_CHECK(  SG_jsondb__check__sz(pCtx, pJsonDb, "/sz1", &b, &psz)  );
	VERIFY_COND("", !b);
	VERIFY_COND("", psz==NULL);
	VERIFY_ERR_CHECK(  SG_jsondb__check__sz(pCtx, pJsonDb, "/sz", &b, &psz)  );
	VERIFY_COND("", b);
	VERIFY_COND("", strcmp(psz, "test") == 0);
	SG_NULLFREE(pCtx, psz);

	VERIFY_ERR_CHECK(  SG_jsondb__update__string__sz(pCtx, pJsonDb, "/sz", SG_FALSE, "test-update")  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/sz", &psz)  );
	VERIFY_COND("", strcmp(psz, "test-update") == 0);
	SG_NULLFREE(pCtx, psz);

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__buflen(pCtx, pJsonDb, "/sz_buflen", SG_FALSE, "1234567890", 5)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/sz_buflen", &psz)  );
	VERIFY_COND("", strcmp(psz, "12345") == 0);
	SG_NULLFREE(pCtx, psz);
	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/sz_buflen", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_SZ);
	VERIFY_ERR_CHECK(  SG_jsondb__get__variant(pCtx, pJsonDb, "/sz_buflen", &pv)  );
	VERIFY_COND("", pv->type == SG_VARIANT_TYPE_SZ);
	VERIFY_COND("", strcmp(pv->v.val_sz, "12345") == 0);
	SG_NULLFREE(pCtx, pv->v.val_sz);
	SG_NULLFREE(pCtx, pv);

	VERIFY_ERR_CHECK(  SG_jsondb__update__string__buflen(pCtx, pJsonDb, "/sz_buflen", SG_FALSE, "test-update", 5)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/sz_buflen", &psz)  );
	VERIFY_COND("", strcmp(psz, "test-") == 0);
	SG_NULLFREE(pCtx, psz);

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__buflen(pCtx, pJsonDb, "/sz_buflen2", SG_FALSE, "1234567890", 15)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/sz_buflen2", &psz)  );
	VERIFY_COND("", strcmp(psz, "1234567890") == 0);
	SG_NULLFREE(pCtx, psz);
	VERIFY_ERR_CHECK(  SG_jsondb__get__variant(pCtx, pJsonDb, "/sz_buflen2", &pv)  );
	VERIFY_COND("", pv->type == SG_VARIANT_TYPE_SZ);
	VERIFY_COND("", strcmp(pv->v.val_sz, "1234567890") == 0);
	SG_NULLFREE(pCtx, pv->v.val_sz);
	SG_NULLFREE(pCtx, pv);
	VERIFY_ERR_CHECK(  SG_jsondb__update__string__buflen(pCtx, pJsonDb, "/sz_buflen2", SG_FALSE, "test-update2", 99)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/sz_buflen2", &psz)  );
	VERIFY_COND("", strcmp(psz, "test-update2") == 0);
	SG_NULLFREE(pCtx, psz);

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__buflen(pCtx, pJsonDb, "/sz_buflen3", SG_FALSE, "1234567890", 0)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/sz_buflen3", &psz)  );
	VERIFY_COND("", strcmp(psz, "1234567890") == 0);
	SG_NULLFREE(pCtx, psz);
	VERIFY_ERR_CHECK(  SG_jsondb__update__string__buflen(pCtx, pJsonDb, "/sz_buflen3", SG_FALSE, "test-update3", 0)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/sz_buflen3", &psz)  );
	VERIFY_COND("", strcmp(psz, "test-update3") == 0);
	SG_NULLFREE(pCtx, psz);

	VERIFY_ERR_CHECK(  SG_jsondb__add__int64(pCtx, pJsonDb, "/max_i64", SG_FALSE, SG_INT64_MAX)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__int64(pCtx, pJsonDb, "/max_i64", &i64)  );
	VERIFY_COND("", i64 == SG_INT64_MAX);
	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/max_i64", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_INT64);
	VERIFY_ERR_CHECK(  SG_jsondb__get__variant(pCtx, pJsonDb, "/max_i64", &pv)  );
	VERIFY_COND("", pv->type == SG_VARIANT_TYPE_INT64);
	VERIFY_COND("", pv->v.val_int64 == SG_INT64_MAX);
	SG_NULLFREE(pCtx, pv);
	VERIFY_ERR_CHECK(  SG_jsondb__update__int64(pCtx, pJsonDb, "/max_i64", SG_FALSE, SG_INT64_MAX - 5)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__int64(pCtx, pJsonDb, "/max_i64", &i64)  );
	VERIFY_COND("", i64 == (SG_INT64_MAX-5));

	VERIFY_ERR_CHECK(  SG_jsondb__add__double(pCtx, pJsonDb, "/easy_pi", SG_FALSE, 3.14159)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__double(pCtx, pJsonDb, "/easy_pi", &dbl)  );
	VERIFY_COND("", MyFn(dblsEqual)(dbl, 3.14159));
	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/easy_pi", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_DOUBLE);
	VERIFY_ERR_CHECK(  SG_jsondb__get__variant(pCtx, pJsonDb, "/easy_pi", &pv)  );
	VERIFY_COND("", pv->type == SG_VARIANT_TYPE_DOUBLE);
	VERIFY_COND("", MyFn(dblsEqual)(pv->v.val_double, 3.14159));
	SG_NULLFREE(pCtx, pv);
	VERIFY_ERR_CHECK(  SG_jsondb__update__double(pCtx, pJsonDb, "/easy_pi", SG_FALSE, 6.022141)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__double(pCtx, pJsonDb, "/easy_pi", &dbl)  );
	VERIFY_COND("", MyFn(dblsEqual)(dbl, 6.022141));

	VERIFY_ERR_CHECK(  SG_jsondb__update__null(pCtx, pJsonDb, "/easy_pi", SG_FALSE)  );
	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/easy_pi", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_NULL);

	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/null", &b)  );
	VERIFY_COND("", !b);
	VERIFY_ERR_CHECK(  SG_jsondb__add__null(pCtx, pJsonDb, "/null", SG_FALSE)  );
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/null", &b)  );
	VERIFY_COND("", b);
	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/null", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_NULL);
	VERIFY_ERR_CHECK(  SG_jsondb__get__variant(pCtx, pJsonDb, "/null", &pv)  );
	VERIFY_COND("", pv->type == SG_VARIANT_TYPE_NULL);
	VERIFY_COND("", pv->v.val_sz == NULL);
	SG_NULLFREE(pCtx, pv);

	VERIFY_ERR_CHECK(  SG_jsondb__add__bool(pCtx, pJsonDb, "/b0", SG_FALSE, SG_FALSE)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__bool(pCtx, pJsonDb, "/b0", &b)  );
	VERIFY_COND("", !b);
	VERIFY_ERR_CHECK(  SG_jsondb__update__bool(pCtx, pJsonDb, "/b0", SG_FALSE, SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__bool(pCtx, pJsonDb, "/b0", &b)  );
	VERIFY_COND("", b);
	VERIFY_ERR_CHECK(  SG_jsondb__add__bool(pCtx, pJsonDb, "/b1", SG_FALSE, SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__bool(pCtx, pJsonDb, "/b1", &b)  );
	VERIFY_COND("", b);
	VERIFY_ERR_CHECK(  SG_jsondb__update__bool(pCtx, pJsonDb, "/b1", SG_FALSE, SG_FALSE)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__bool(pCtx, pJsonDb, "/b1", &b)  );
	VERIFY_COND("", !b);
	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/b0", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_BOOL);
	VERIFY_ERR_CHECK(  SG_jsondb__get__variant(pCtx, pJsonDb, "/b0", &pv)  );
	VERIFY_COND("", pv->type == SG_VARIANT_TYPE_BOOL);
	VERIFY_COND("", pv->v.val_bool == SG_TRUE);
	SG_NULLFREE(pCtx, pv);

	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );
	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/", &i32)  );
	VERIFY_COND("", i32 == 9);


	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/sz", &b)  );
	VERIFY_COND("", b);
	VERIFY_ERR_CHECK(  SG_jsondb__remove(pCtx, pJsonDb, "/sz")  );
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/sz", &b)  );
	VERIFY_COND("", !b);

	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );

	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/easy_pi", &b)  );
	VERIFY_COND("", b);
	VERIFY_ERR_CHECK(  SG_jsondb__remove(pCtx, pJsonDb, "/easy_pi")  );
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/easy_pi", &b)  );
	VERIFY_COND("", !b);

	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );

	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/b1", &b)  );
	VERIFY_COND("", b);
	VERIFY_ERR_CHECK(  SG_jsondb__remove(pCtx, pJsonDb, "/b1")  );
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/b1", &b)  );
	VERIFY_COND("", !b);

	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );
	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/", &i32)  );
	VERIFY_COND("", i32 == 6);

	VERIFY_ERR_CHECK(  SG_jsondb__remove(pCtx, pJsonDb, "/")  );
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/", &b)  );
	VERIFY_COND("", !b);

	/* fall through */
fail:
	SG_NULLFREE(pCtx, psz);
	SG_NULLFREE(pCtx, pv);
}

static void MyFn(recursive_add)(SG_context* pCtx, SG_jsondb* pJsonDb)
{
	char* pszTest = NULL;
	SG_bool b;
	SG_uint16 type;
	SG_uint32 count;

	VERIFY_ERR_CHECK(  SG_jsondb__add__object(pCtx, pJsonDb, "recursive add", NULL)  );

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__sz(pCtx, pJsonDb, "/test_sz_key/1/2/3/4/5/6", SG_TRUE, "test_sz_val")  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/test_sz_key/1/2/3/4/5/6", &pszTest)  );
	VERIFY_COND("x", strcmp(pszTest, "test_sz_val") == 0);
	SG_NULLFREE(pCtx, pszTest);

	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/", &count)  );
	VERIFY_COND("", count == 1);
	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/test_sz_key", &count)  );
	VERIFY_COND("", count == 1);
	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/test_sz_key/1", &count)  );
	VERIFY_COND("", count == 1);
	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/test_sz_key/1/2", &count)  );
	VERIFY_COND("", count == 1);
	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/test_sz_key/1/2/3", &count)  );
	VERIFY_COND("", count == 1);
	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/test_sz_key/1/2/3/4", &count)  );
	VERIFY_COND("", count == 1);
	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/test_sz_key/1/2/3/4/5", &count)  );
	VERIFY_COND("", count == 1);
	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/test_sz_key/1/2/3/4/5/6", &count)  );
	VERIFY_COND("", count == 0);

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(SG_jsondb__add__string__sz(pCtx, pJsonDb, "/", SG_FALSE, "test_sz_val"),
		SG_ERR_JSONDB_OBJECT_ALREADY_EXISTS);

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__add__string__sz(pCtx, pJsonDb, "/test_sz_key/1/2/3/4/5/6/7", SG_FALSE, "test_sz_val2"),
		SG_ERR_JSONDB_NON_CONTAINER_ANCESTOR_EXISTS);
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/test_sz_key/1/2/3/4/5/6/7", &b)  );
	VERIFY_COND("", !b);

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(SG_jsondb__add__string__sz(pCtx, pJsonDb, "/test_sz_key/1/2/3", SG_TRUE, "test_sz_val"),
		SG_ERR_JSONDB_OBJECT_ALREADY_EXISTS);

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__add__string__sz(pCtx, pJsonDb, "/test_sz_key/1/2/3/4/5/6/7/8/9/10", SG_TRUE, "test_sz_val2"),
		SG_ERR_JSONDB_NON_CONTAINER_ANCESTOR_EXISTS);
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/test_sz_key/1/2/3/4/5/6/7/8/9/10", &b)  );
	VERIFY_COND("", !b);

	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/test_sz_key/1/2/3", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_VHASH);

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__sz(pCtx, pJsonDb, "/test_sz_key/1/2/3/4/5a/6a/7a/8a/9a/10a", SG_TRUE, "test_sz_val3")  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/test_sz_key/1/2/3/4/5a/6a/7a/8a/9a/10a", &pszTest)  );
	VERIFY_COND("x", strcmp(pszTest, "test_sz_val3") == 0);

	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );

	/* fall through */
fail:
	SG_NULLFREE(pCtx, pszTest);
}

void MyFn(vhash)(SG_context* pCtx, SG_jsondb* pJsonDb)
{
	SG_vhash* pvh = NULL;
	SG_vhash* pvh2 = NULL;
	SG_uint16 type;
 	const char* psz = NULL;
	SG_varray* pva = NULL;
	char* psz2 = NULL;

	VERIFY_ERR_CHECK(  SG_jsondb__add__object(pCtx, pJsonDb, "vhash", NULL)  );

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	VERIFY_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "test1key", "test1val")  );
	VERIFY_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "test2key", "test2val")  );
	VERIFY_ERR_CHECK(  SG_jsondb__add__vhash(pCtx, pJsonDb, "/", SG_FALSE, pvh)  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	VERIFY_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "blah_key", "blah_val")  );

	VERIFY_ERR_CHECK(  SG_jsondb__add__vhash(pCtx, pJsonDb, "/vhash1", SG_FALSE, pvh)  );
 	SG_VHASH_NULLFREE(pCtx, pvh);

	type = 0;
	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_VHASH);
	type = 0;
	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/vhash1", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_VHASH);

	VERIFY_ERR_CHECK(  SG_jsondb__get__vhash(pCtx, pJsonDb, "/vhash1", &pvh)  );
	VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "blah_key", &psz)  );
	VERIFY_COND("", strcmp(psz, "blah_val") == 0);
	SG_VHASH_NULLFREE(pCtx, pvh);

	VERIFY_ERR_CHECK(  SG_varray__alloc(pCtx, &pva)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, "test1")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, "test2")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, "test3")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, "test4")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, "test5")  );

	VERIFY_ERR_CHECK(  SG_jsondb__add__varray(pCtx, pJsonDb, "/pvh1/pvh2/pva", SG_TRUE, pva) );
	SG_VARRAY_NULLFREE(pCtx, pva);
	VERIFY_ERR_CHECK(  SG_jsondb__get__vhash(pCtx, pJsonDb, "/pvh1", &pvh)  );
	VERIFY_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, "pvh2", &pvh2)  );
	VERIFY_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh2, "pva", &pva)  );

	VERIFY_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, 0, &psz)  );
	VERIFY_COND("", 0==strcmp("test1",psz));
	VERIFY_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, 1, &psz)  );
	VERIFY_COND("", 0==strcmp("test2",psz));
	VERIFY_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, 2, &psz)  );
	VERIFY_COND("", 0==strcmp("test3",psz));
	VERIFY_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, 3, &psz)  );
	VERIFY_COND("", 0==strcmp("test4",psz));
	VERIFY_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, 4, &psz)  );
	VERIFY_COND("", 0==strcmp("test5",psz));

	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );

	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_VHASH__ALLOC(pCtx, &pvh);
	VERIFY_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "testkey", "testval")  );

	VERIFY_ERR_CHECK(  SG_jsondb__update__vhash(pCtx, pJsonDb, "/", SG_FALSE, pvh)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/testkey", &psz2)  );
	VERIFY_COND("", 0 == strcmp("testval", psz2));
	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );

	/* fall through */
fail:
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_NULLFREE(pCtx, psz2);
}

void MyFn(varray)(SG_context* pCtx, SG_jsondb* pJsonDb)
{
	SG_varray* pva = NULL;
	SG_varray* pva2 = NULL;
	SG_string* pStr = NULL;
	SG_string* pStr2 = NULL;
	SG_bool b;
	char* psz = NULL;
	SG_vhash* pvh = NULL;
	SG_uint16 type;
	SG_uint32 count;

	VERIFY_ERR_CHECK(  SG_jsondb__add__object(pCtx, pJsonDb, "varray", NULL)  );

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );
	VERIFY_ERR_CHECK(  SG_jsondb__add__varray(pCtx, pJsonDb, "/", SG_FALSE, pva)  );
	SG_VARRAY_NULLFREE(pCtx, pva);

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__add__string__sz(pCtx, pJsonDb, "/test_sz_key", SG_FALSE, "val"),
		SG_ERR_JSONDB_INVALID_PATH);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__add__string__sz(pCtx, pJsonDb, "/0", SG_FALSE, "val"),
		SG_ERR_JSONDB_INVALID_PATH);
	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/", &count)  );
	VERIFY_COND("", count == 0);

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__sz(pCtx, pJsonDb, "/#", SG_FALSE, "val")  );
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/0", &b)  );
	VERIFY_COND("", b);
	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/", &count)  );
	VERIFY_COND("", count == 1);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__add__string__sz(pCtx, pJsonDb, "/0/0", SG_FALSE, "val"),
		SG_ERR_JSONDB_NON_CONTAINER_ANCESTOR_EXISTS);

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__sz(pCtx, pJsonDb, "/#/#", SG_TRUE, "val")  );
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/1/0", &b)  );
	VERIFY_COND("", b);

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__sz(pCtx, pJsonDb, "/#/#", SG_TRUE, "val")  );
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/2/0", &b)  );
	VERIFY_COND("", b);

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__sz(pCtx, pJsonDb, "/#/#/#/#/#/#/#", SG_TRUE, "val")  );
	VERIFY_ERR_CHECK(  SG_jsondb__has(pCtx, pJsonDb, "/3/0/0/0/0/0/0", &b)  );
	VERIFY_COND("", b);

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__add__string__sz(pCtx, pJsonDb, "/3/0/1", SG_FALSE, "val"),
		SG_ERR_JSONDB_INVALID_PATH);
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__add__string__sz(pCtx, pJsonDb, "/3/1/#", SG_FALSE, "val"),
		SG_ERR_JSONDB_PARENT_DOESNT_EXIST);

	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );

	VERIFY_ERR_CHECK(  SG_jsondb__remove(pCtx, pJsonDb, "/2")  );

	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );

	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/2/0/0/0/0/0/0", &psz)  );
	VERIFY_COND("", strcmp(psz, "val")==0);
	SG_NULLFREE(pCtx, psz);

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );
	VERIFY_ERR_CHECK(  SG_varray__append__bool(pCtx, pva, SG_TRUE)  );
	VERIFY_ERR_CHECK(  SG_varray__append__double(pCtx, pva, 3.14159)  );
	VERIFY_ERR_CHECK(  SG_varray__append__int64(pCtx, pva, 42)  );
	VERIFY_ERR_CHECK(  SG_varray__append__null(pCtx, pva)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, "The Victors")  );

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	VERIFY_ERR_CHECK(  SG_vhash__add__bool(pCtx, pvh, "a", SG_FALSE)  );
	VERIFY_ERR_CHECK(  SG_vhash__add__double(pCtx, pvh, "b", 9.8)  );
	VERIFY_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, "c", 7)  );
	VERIFY_ERR_CHECK(  SG_vhash__add__null(pCtx, pvh, "d")  );
	VERIFY_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "e", "Champions of the West")  );
	VERIFY_ERR_CHECK(  SG_varray__append__vhash(pCtx, pva, &pvh)  );

	VERIFY_ERR_CHECK(  SG_jsondb__add__varray(pCtx, pJsonDb, "/#", SG_FALSE, pva)  );

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStr)  );
	VERIFY_ERR_CHECK(  SG_varray__to_json(pCtx, pva, pStr)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__varray(pCtx, pJsonDb, "/3", &pva2)  );
	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStr2)  );
	VERIFY_ERR_CHECK(  SG_varray__to_json(pCtx, pva2, pStr2)  );
	VERIFY_COND("", strcmp(SG_string__sz(pStr), SG_string__sz(pStr2)) == 0 );
	SG_VARRAY_NULLFREE(pCtx, pva);
	SG_VARRAY_NULLFREE(pCtx, pva2);

	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/3/4", &psz)  );
	VERIFY_COND("", strcmp(psz, "The Victors") == 0);
	SG_NULLFREE(pCtx, psz);

	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/3/5/e", &psz)  );
	VERIFY_COND("", strcmp(psz, "Champions of the West") == 0);
	SG_NULLFREE(pCtx, psz);

	type = 0;
	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_VARRAY);
	type = 0;
	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/3", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_VARRAY);
	type = 0;
	VERIFY_ERR_CHECK(  SG_jsondb__typeof(pCtx, pJsonDb, "/3/5", &type)  );
	VERIFY_COND("", type == SG_VARIANT_TYPE_VHASH);

	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/", &count)  );
	VERIFY_COND("", count == 4);

	VERIFY_ERR_CHECK(  SG_jsondb__remove(pCtx, pJsonDb, "/0")  );

	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/", &count)  );
	VERIFY_COND("", count == 3);

	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/2/5/e", &psz)  );
	VERIFY_COND("", strcmp(psz, "Champions of the West") == 0);
	SG_NULLFREE(pCtx, psz);

	{
		SG_uint32 i, j;

		VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );
		for (i = 0; i < 5; i++)
		{
			VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva2)  );
			for (j = 0; j < 5; j++)
			{
				VERIFY_ERR_CHECK(  SG_varray__append__int64(pCtx, pva2, j)  );
			}
			VERIFY_ERR_CHECK(  SG_varray__append__varray(pCtx, pva, &pva2)  );
		}
		VERIFY_ERR_CHECK(  SG_jsondb__add__varray(pCtx, pJsonDb, "/#", SG_FALSE, pva)  );

		SG_VARRAY_NULLFREE(pCtx, pva);

		VERIFY_ERR_CHECK(  SG_jsondb__get__varray(pCtx, pJsonDb, "/3", &pva)  );
		for (i = 0; i < 5; i++)
		{
			VERIFY_ERR_CHECK(  SG_varray__get__varray(pCtx, pva, i, &pva2)  );
			for (j = 0; j < 5; j++)
			{
				SG_int64 i64;
				VERIFY_ERR_CHECK(  SG_varray__get__int64(pCtx, pva2, j, &i64)  );
				VERIFY_COND("", i64 == j);
			}
		}
		pva2 = NULL;

	}

	VERIFY_ERR_CHECK(  SG_jsondb__remove(pCtx, pJsonDb, "/3/2")  );
	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );

	SG_NULLFREE(pCtx, psz);
	SG_VARRAY_NULLFREE(pCtx, pva);
	SG_VARRAY__ALLOC(pCtx, &pva);
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, "testval")  );
	VERIFY_ERR_CHECK(  SG_jsondb__update__varray(pCtx, pJsonDb, "/", SG_FALSE, pva)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/0", &psz)  );
	VERIFY_COND("", 0 == strcmp("testval", psz));
	SG_NULLFREE(pCtx, psz);
	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );


	SG_VARRAY_NULLFREE(pCtx, pva);
	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, "val1")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, "val2")  );
	VERIFY_ERR_CHECK(  SG_jsondb__add__object(pCtx, pJsonDb, "varray2", NULL)  );
	VERIFY_ERR_CHECK(  SG_jsondb__add__varray(pCtx, pJsonDb, "/", SG_FALSE, pva)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/1", &psz)  );
	VERIFY_COND("", 0==strcmp(psz, "val2"));

	/* fall through */
fail:
	SG_VARRAY_NULLFREE(pCtx, pva);
	SG_VARRAY_NULLFREE(pCtx, pva2);
	SG_STRING_NULLFREE(pCtx, pStr);
	SG_STRING_NULLFREE(pCtx, pStr2);
	SG_NULLFREE(pCtx, psz);
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void MyFn(container_updates)(SG_context* pCtx, SG_jsondb* pJsonDb)
{
	SG_uint32 i32;
	char* psz = NULL;
	SG_uint32 i, j;
	SG_vhash* pvh = NULL;
	SG_vhash* pvh2 = NULL;
	char buf[12];

	VERIFY_ERR_CHECK(  SG_jsondb__add__object(pCtx, pJsonDb, "container_updates", NULL)  );

	VERIFY_ERR_CHECK(  SG_jsondb__add__int64(pCtx, pJsonDb, "/42", SG_TRUE, 42)  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__uint32(pCtx, pJsonDb, "/42", &i32)  );
	VERIFY_COND("", i32 == 42);

	VERIFY_ERR_CHECK(  SG_jsondb__update__string__sz(pCtx, pJsonDb, "/42", SG_FALSE, "42")  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/42", &psz)  );
	VERIFY_COND("", strcmp(psz, "42") == 0);
	SG_NULLFREE(pCtx, psz);

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	for (i = 0; i < 5; i++)
	{
		VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh2)  );
		for (j = 0; j < 5; j++)
		{
			VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, 12, "%d", j)  );
			VERIFY_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh2, buf, j)  );
		}
		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, 12, "%d", i)  );
		VERIFY_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh, buf, &pvh2)  );
	}
	VERIFY_ERR_CHECK(  SG_jsondb__update__vhash(pCtx, pJsonDb, "/42", SG_FALSE, pvh)  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	VERIFY_ERR_CHECK(SG_jsondb__get__uint32(pCtx, pJsonDb, "/42/0/0", &i32)  );
	VERIFY_COND("", i32 == 0);
	VERIFY_ERR_CHECK(SG_jsondb__get__uint32(pCtx, pJsonDb, "/42/4/4", &i32)  );
	VERIFY_COND("", i32 == 4);

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	for (i = 0; i < 5; i++)
	{
		VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh2)  );
		for (j = 0; j < 5; j++)
		{
			VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, 12, "%d", j)  );
			VERIFY_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh2, buf, j)  );
		}
		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, 12, "%d", i)  );
		VERIFY_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh, buf, &pvh2)  );
	}
	VERIFY_ERR_CHECK(  SG_jsondb__update__vhash(pCtx, pJsonDb, "/7", SG_FALSE, pvh)  );
	VERIFY_ERR_CHECK(SG_jsondb__get__uint32(pCtx, pJsonDb, "/7/0/0", &i32)  );
	VERIFY_COND("", i32 == 0);
	VERIFY_ERR_CHECK(SG_jsondb__get__uint32(pCtx, pJsonDb, "/7/4/4", &i32)  );
	VERIFY_COND("", i32 == 4);

	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );

	VERIFY_ERR_CHECK(  SG_jsondb__update__string__sz(pCtx, pJsonDb, "/42", SG_FALSE, "42")  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/42", &psz)  );
	VERIFY_COND("", strcmp(psz, "42") == 0);

	VERIFY_ERR_CHECK(SG_jsondb__get__uint32(pCtx, pJsonDb, "/7/0/0", &i32)  );
	VERIFY_COND("", i32 == 0);
	VERIFY_ERR_CHECK(SG_jsondb__get__uint32(pCtx, pJsonDb, "/7/4/4", &i32)  );
	VERIFY_COND("", i32 == 4);

	VERIFY_ERR_CHECK(  SG_jsondb__update__null(pCtx, pJsonDb, "/", SG_FALSE)  );
	VERIFY_ERR_CHECK(  SG_jsondb__count(pCtx, pJsonDb, "/", &i32)  );
	VERIFY_COND("", i32 == 0);

	/* fall through */
fail:
	SG_NULLFREE(pCtx, psz);
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_VHASH_NULLFREE(pCtx, pvh2);
}

void MyFn(lft_collision)(SG_context* pCtx, SG_jsondb* pJsonDb)
{
	VERIFY_ERR_CHECK(  SG_jsondb__add__object(pCtx, pJsonDb, "lft_collision", NULL)  );

	VERIFY_ERR_CHECK(  SG_jsondb__add__vhash(pCtx, pJsonDb, "/x", SG_TRUE, NULL)  );
	VERIFY_ERR_CHECK(  SG_jsondb__add__vhash(pCtx, pJsonDb, "/y/y1", SG_TRUE, NULL)  );
	VERIFY_ERR_CHECK(  SG_jsondb__add__vhash(pCtx, pJsonDb, "/y/y2", SG_TRUE, NULL)  );

	VERIFY_ERR_CHECK(  SG_jsondb__add__vhash(pCtx, pJsonDb, "/x/x1", SG_TRUE, NULL)  );

	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );

	/* fall through */
fail:
	;
}

void MyFn(escape_chars)(SG_context* pCtx, SG_jsondb* pJsonDb)
{
	char* psz = NULL;
	char* upsz = NULL;
	char* psz2 = NULL;
	SG_uint32 i,j;
	SG_vhash* pvh = NULL;
	SG_vhash* pvh2 = NULL;
	char buf[100];

	VERIFY_ERR_CHECK(  SG_jsondb__add__object(pCtx, pJsonDb, "escape_chars", NULL)  );

	VERIFY_ERR_CHECK(  SG_jsondb__add__string__sz(pCtx, pJsonDb, "/\\/", SG_TRUE, "slash")  );
	VERIFY_ERR_CHECK(  SG_jsondb__get__sz(pCtx, pJsonDb, "/\\/", &psz)  );
	VERIFY_COND("", strcmp(psz, "slash") == 0);
	SG_NULLFREE(pCtx, psz);

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_jsondb__add__string__sz(pCtx, pJsonDb, "/\\/", SG_TRUE, "slash_dup"),
		SG_ERR_JSONDB_OBJECT_ALREADY_EXISTS);

	VERIFY_ERR_CHECK(  SG_jsondb__escape_keyname(pCtx, "test-a-path", &psz)  );
	VERIFY_COND("", strcmp(psz, "test-a-path") == 0);
	SG_NULLFREE(pCtx, psz);
	VERIFY_ERR_CHECK(  SG_jsondb__unescape_keyname(pCtx, "test-a-path", &psz)  );
	VERIFY_COND("", strcmp(psz, "test-a-path") == 0);
	SG_NULLFREE(pCtx, psz);

	VERIFY_ERR_CHECK(  SG_jsondb__escape_keyname(pCtx, "/test/a/path/", &psz)  );
	VERIFY_COND("", strcmp(psz, "\\/test\\/a\\/path\\/") == 0);
	VERIFY_ERR_CHECK(  SG_jsondb__unescape_keyname(pCtx, psz, &upsz)  );
	VERIFY_COND("", strcmp(upsz, "/test/a/path/") == 0);
	SG_NULLFREE(pCtx, psz);
	SG_NULLFREE(pCtx, upsz);

	VERIFY_ERR_CHECK(  SG_jsondb__escape_keyname(pCtx, "\\test\\a\\path\\", &psz)  );
	VERIFY_COND("", strcmp(psz, "\\\\test\\\\a\\\\path\\\\") == 0);
	VERIFY_ERR_CHECK(  SG_jsondb__unescape_keyname(pCtx, psz, &upsz)  );
	VERIFY_COND("", strcmp(upsz, "\\test\\a\\path\\") == 0);
	SG_NULLFREE(pCtx, psz);
	SG_NULLFREE(pCtx, upsz);

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	for (i = 0; i < 5; i++)
	{
		VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh2)  );
		for (j = 0; j < 5; j++)
		{
			VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, 100, "%d/%d\\%d%d", i, j, i, j)  );
			VERIFY_ERR_CHECK(  SG_jsondb__escape_keyname(pCtx, buf, &psz)  );
			VERIFY_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh2, psz, j)  );
			VERIFY_ERR_CHECK(  SG_NULLFREE(pCtx, psz)  );
		}
		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, 100, "%d/%d\\%d", i, i, i)  );
		VERIFY_ERR_CHECK(  SG_jsondb__escape_keyname(pCtx, buf, &psz)  );
		VERIFY_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh, psz, &pvh2)  );
		VERIFY_ERR_CHECK(  SG_NULLFREE(pCtx, psz)  );
	}
	VERIFY_ERR_CHECK(  SG_jsondb__add__vhash(pCtx, pJsonDb, "/vhash-keys", SG_TRUE, pvh)  );
	VERIFY_ERR_CHECK(  SG_VHASH_NULLFREE(pCtx, pvh)  );

	VERIFY_ERR_CHECK(  SG_jsondb__get__vhash(pCtx, pJsonDb, "/vhash-keys", &pvh)  );
	for (i = 0; i < 5; i++)
	{
		SG_vhash* pvh_ref;
		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, 100, "%d/%d\\%d", i, i, i)  );
		VERIFY_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, buf, &pvh_ref)  );
		VERIFY_ERR_CHECK(  SG_NULLFREE(pCtx, psz)  );
		for (j = 0; j < 5; j++)
		{
			SG_int64 j2;
			VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, 100, "%d/%d\\%d%d", i, j, i, j)  );
			VERIFY_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_ref, buf, &j2)  );
			VERIFY_COND("", j == j2);

			VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, 100, "%d/%d\\%d", i, i, i)  );
			VERIFY_ERR_CHECK(  SG_jsondb__escape_keyname(pCtx, buf, &psz)  );
			VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, 100, "%d/%d\\%d%d", i, j, i, j)  );
			VERIFY_ERR_CHECK(  SG_jsondb__escape_keyname(pCtx, buf, &psz2)  );
			VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, 100, "/vhash-keys/%s/%s", psz, psz2)  );
			j2 = 9;
			VERIFY_ERR_CHECK(  SG_jsondb__get__int64(pCtx, pJsonDb, buf, &j2)  );
			VERIFY_COND("", j == j2);
			SG_NULLFREE(pCtx, psz);
			SG_NULLFREE(pCtx, psz2);
		}
	}

	VERIFY_ERR_CHECK(  SG_jsondb_debug__verify_tree(pCtx, pJsonDb)  );

	/* fall through */
fail:
	SG_NULLFREE(pCtx, psz);
	SG_NULLFREE(pCtx, upsz);
	SG_NULLFREE(pCtx, psz2);
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_VHASH_NULLFREE(pCtx, pvh2);
}

MyMain()
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTop = NULL;
	SG_jsondb* pJsonDb = NULL;

	TEMPLATE_MAIN_START;

	//VERIFY_ERR_CHECK(  MyFn(json_sandbox(pCtx))  );

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir), 10)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathTop, bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathTop)  );

 	VERIFY_ERR_CHECK(  MyFn(_create_db)(pCtx, pPathTop, &pJsonDb)  );

	VERIFY_ERR_CHECK(  MyFn(object_and_path_basics)(pCtx, pJsonDb)  );

	VERIFY_ERR_CHECK(  MyFn(simple_types)(pCtx, pJsonDb)  );

	VERIFY_ERR_CHECK(  MyFn(int64_or_double)(pCtx, pJsonDb)  );

	VERIFY_ERR_CHECK(  MyFn(recursive_add)(pCtx, pJsonDb)  );

	VERIFY_ERR_CHECK(  MyFn(vhash)(pCtx, pJsonDb)  );

	VERIFY_ERR_CHECK(  MyFn(varray)(pCtx, pJsonDb)  );

	VERIFY_ERR_CHECK(  MyFn(container_updates)(pCtx, pJsonDb)  );

 	VERIFY_ERR_CHECK(  MyFn(lft_collision)(pCtx, pJsonDb)  );

	VERIFY_ERR_CHECK(  MyFn(escape_chars)(pCtx, pJsonDb)  );

	// Fall through to common cleanup.
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTop);
	SG_JSONDB_NULLFREE(pCtx, pJsonDb);

	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn
