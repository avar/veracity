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

void u0028_vhash__varray_1(SG_context * pCtx)
{
	SG_varray* pa = NULL;
	SG_varray* pa2 = NULL;
    SG_bool b = SG_FALSE;
    const SG_variant* pv = NULL;
    const char* psz = NULL;
    SG_int64 num;
    double d;
    SG_vhash* pvh = NULL;
    SG_varray* pva = NULL;
    SG_uint16 typ;
    SG_vhash* pvh_null = NULL;
    SG_varray* pva_null = NULL;

    VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pa)  );

	SG_ERR_CHECK(  SG_varray__append__string__buflen(pCtx, pa, NULL, 0)  );
	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pa, &pvh_null)  );
	SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pa, &pva_null)  );

    VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pa2)  );

    VERIFY_ERR_CHECK(  SG_varray__equal(pCtx, pa, pa2, &b)  );
    VERIFY_COND("f", (!b));

	SG_ERR_CHECK(  SG_varray__append__int64(pCtx, pa2, 71)  );
	SG_ERR_CHECK(  SG_varray__append__int64(pCtx, pa2, 31)  );
	SG_ERR_CHECK(  SG_varray__append__int64(pCtx, pa2, 51)  );

    VERIFY_ERR_CHECK(  SG_varray__sort(pCtx, pa2, SG_variant_sort_callback__increasing)  );

    VERIFY_ERR_CHECK(  SG_varray__equal(pCtx, pa, pa2, &b)  );
    VERIFY_COND("f", (!b));

    VERIFY_ERR_CHECK(  SG_varray__get__variant(pCtx, pa2, 0, &pv)  );

    /* try to fetch with an index out of range */
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__variant(pCtx, pa2, 17, &pv),
										  SG_ERR_VARRAY_INDEX_OUT_OF_RANGE  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__sz(pCtx, pa2, 17, &psz),
										  SG_ERR_VARRAY_INDEX_OUT_OF_RANGE  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__int64(pCtx, pa2, 17, &num),
										  SG_ERR_VARRAY_INDEX_OUT_OF_RANGE  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__double(pCtx, pa2, 17, &d),
										  SG_ERR_VARRAY_INDEX_OUT_OF_RANGE  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__bool(pCtx, pa2, 17, &b),
										  SG_ERR_VARRAY_INDEX_OUT_OF_RANGE  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__varray(pCtx, pa2, 17, &pva),
										  SG_ERR_VARRAY_INDEX_OUT_OF_RANGE  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__vhash(pCtx, pa2, 17, &pvh),
										  SG_ERR_VARRAY_INDEX_OUT_OF_RANGE  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__typeof(pCtx, pa2, 17, &typ),
										  SG_ERR_VARRAY_INDEX_OUT_OF_RANGE  );

    /* now the wrong types */
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__sz(pCtx, pa2, 1, &psz),
										  SG_ERR_VARIANT_INVALIDTYPE  );

    //VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__int64(pCtx, pa2, 1, &num),
    //SG_ERR_VARIANT_INVALIDTYPE  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__double(pCtx, pa2, 1, &d),
										  SG_ERR_VARIANT_INVALIDTYPE  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__bool(pCtx, pa2, 1, &b),
										  SG_ERR_VARIANT_INVALIDTYPE  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__varray(pCtx, pa2, 1, &pva),
										  SG_ERR_VARIANT_INVALIDTYPE  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_varray__get__vhash(pCtx, pa2, 1, &pvh),
										  SG_ERR_VARIANT_INVALIDTYPE  );

    VERIFY_ERR_CHECK(  SG_varray__typeof(pCtx, pa2, 1, &typ)  );
    VERIFY_COND("typ", (SG_VARIANT_TYPE_INT64 == typ));

    /* values */
    VERIFY_ERR_CHECK(  SG_varray__get__int64(pCtx, pa2, 0, &num)  );
    VERIFY_COND("val", (31 == num));

    VERIFY_ERR_CHECK(  SG_varray__get__int64(pCtx, pa2, 1, &num)  );
    VERIFY_COND("val", (51 == num));

    VERIFY_ERR_CHECK(  SG_varray__get__int64(pCtx, pa2, 2, &num)  );
    VERIFY_COND("val", (71 == num));

    /* can be nulls */
    VERIFY_ERR_CHECK(  SG_varray__get__sz(pCtx, pa, 1, &psz)  );
    VERIFY_COND("null", (!psz));

    VERIFY_ERR_CHECK(  SG_varray__get__varray(pCtx, pa, 1, &pva)  );
    VERIFY_COND("null", (!pva));

    VERIFY_ERR_CHECK(  SG_varray__get__vhash(pCtx, pa, 1, &pvh)  );
    VERIFY_COND("null", (!pva));

    SG_VARRAY_NULLFREE(pCtx, pa);
    SG_VARRAY_NULLFREE(pCtx, pa2);

    return;

fail:
    return;
}

void u0028_vhash__create(SG_context * pCtx, SG_vhash** pph)
{
	SG_vhash* ph = NULL;
	SG_varray* pa = NULL;

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &ph)  );

	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, ph, "hello", "world")  );

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pa)  );

	SG_ERR_CHECK(  SG_varray__append__int64(pCtx, pa, 31)  );
	SG_ERR_CHECK(  SG_varray__append__int64(pCtx, pa, 51)  );
	SG_ERR_CHECK(  SG_varray__append__int64(pCtx, pa, 71)  );
	SG_ERR_CHECK(  SG_varray__append__int64(pCtx, pa, 91)  );

	SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, ph, "a", &pa)  );

	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, ph, "b", "fiddle")  );

	*pph = ph;

	return;

fail:
	// TODO free
	return;
}

void u0028_vhash__test_5(SG_context * pCtx)
{
	SG_vhash* pvh = NULL;
	SG_varray* pva = NULL;
	SG_uint16 type;
	SG_bool b;
    double d;

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );

    VERIFY_ERR_CHECK(  SG_vhash__update__double(pCtx, pvh, "d", 3.14)  );
    VERIFY_ERR_CHECK(  SG_vhash__update__double(pCtx, pvh, "d", 1.414)  );
    VERIFY_ERR_CHECK(  SG_vhash__update__vhash(pCtx, pvh, "pvh", NULL)  );
    VERIFY_ERR_CHECK(  SG_vhash__update__varray(pCtx, pvh, "pva", NULL)  );
    VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );
    VERIFY_ERR_CHECK(  SG_vhash__update__varray(pCtx, pvh, "pva", &pva)  );
    VERIFY_ERR_CHECK(  SG_vhash__update__varray(pCtx, pvh, "pva", NULL)  );
    VERIFY_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh, "adslkfjlasdj", NULL)  );
    VERIFY_ERR_CHECK(  SG_vhash__get__double(pCtx, pvh, "d", &d)  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_vhash__typeof(pCtx, pvh, "not_there", &type),
										  SG_ERR_VHASH_KEYNOTFOUND  );

    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_vhash__equal(pCtx, pvh, NULL, &b),
										  SG_ERR_INVALIDARG  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_vhash__equal(pCtx, NULL, pvh, &b),
										  SG_ERR_INVALIDARG  );
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_vhash__equal(pCtx, NULL, NULL, &b),
										  SG_ERR_INVALIDARG  );

    VERIFY_ERR_CHECK(  SG_vhash__equal(pCtx, pvh, pvh, &b)  );
    VERIFY_COND("b", b);

	SG_VARRAY_NULLFREE(pCtx, pva);
	SG_VHASH_NULLFREE(pCtx, pvh);

	return;

fail:
    return;
}

void u0028_vhash__test_null(SG_context * pCtx)
{
	SG_vhash* pvh = NULL;

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );

	// NULL or empty-string keys should be disallowed
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_vhash__add__string__sz(pCtx, pvh, NULL, "test"),
		SG_ERR_INVALIDARG);

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(
		SG_vhash__add__string__sz(pCtx, pvh, "", "test"),
		SG_ERR_INVALIDARG);

	/* fall through */
fail:
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void u0028_vhash__test_1(SG_context * pCtx)
{
	SG_vhash* ph = NULL;
	SG_varray* pva = NULL;
	SG_uint32 count;
	SG_uint16 type;
	SG_bool b;
	const char* psz;

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &ph)  );

	b = SG_FALSE;
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__has(pCtx, ph, "hello", &b)  );
	VERIFY_COND("not there", !b);

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, ph, "hello", "world")  );

	b = SG_FALSE;
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__has(pCtx, ph, "hello", &b)  );
	VERIFY_COND("there", b);

	type = 0;
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__typeof(pCtx, ph, "hello", &type)  );
	VERIFY_COND("type", type == SG_VARIANT_TYPE_SZ);

	psz = NULL;
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__get__sz(pCtx, ph, "hello", &psz)  );
	VERIFY_COND("match", 0 == strcmp(psz, "world"));

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_vhash__add__string__sz(pCtx, ph, "hello", "again"),
		SG_ERR_VHASH_DUPLICATEKEY  );

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, ph, "hola", "world")  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__get_keys(pCtx, ph, &pva)  );

	VERIFY_ERR_CHECK_DISCARD(  SG_varray__count(pCtx, pva, &count)  );
	VERIFY_COND("count is 2", (2 == count));

	SG_VARRAY_NULLFREE(pCtx, pva);
	SG_VHASH_NULLFREE(pCtx, ph);

	return;

fail:
	return;
}

void u0028_vhash__test_2(SG_context * pCtx)
{
	SG_vhash* ph = NULL;
	SG_uint32 count;
	SG_uint32 i;

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &ph)  );

	for (i=0; i<10000; i++)
	{
		char buf[SG_TID_MAX_BUFFER_LENGTH];

		VERIFY_ERR_CHECK_DISCARD(  SG_tid__generate(pCtx, buf, sizeof(buf))  );

		VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, ph, buf, "never mind this")  );
	}

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__count(pCtx, ph, &count)  );
	VERIFY_COND("count is ok", (count == 10000));

	SG_VHASH_NULLFREE(pCtx, ph);

	return;

fail:
	return;
}

void u0028_vhash__test_4(SG_context * pCtx)
{
	SG_vhash* pvh1;
	SG_vhash* pvh2;
	SG_vhash* pvh3;
	SG_vhash* pvh4;
	SG_bool b;

	VERIFY_ERR_CHECK_DISCARD(  u0028_vhash__create(pCtx, &pvh1)  );
	VERIFY_ERR_CHECK_DISCARD(  u0028_vhash__create(pCtx, &pvh2)  );

	b = SG_FALSE;
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__equal(pCtx, pvh1, pvh2, &b)  );
	VERIFY_COND("same", b);

	VERIFY_ERR_CHECK_DISCARD(  u0028_vhash__create(pCtx, &pvh3)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__vhash(pCtx, pvh1, "sub", &pvh3)  );
	b = SG_FALSE;
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__equal(pCtx, pvh1, pvh2, &b)  );
	VERIFY_COND("not same", !b);

	VERIFY_ERR_CHECK_DISCARD(  u0028_vhash__create(pCtx, &pvh4)  );

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__vhash(pCtx, pvh2, "sub", &pvh4)  );
	b = SG_FALSE;
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__equal(pCtx, pvh1, pvh2, &b)  );
	VERIFY_COND("same again", b);

	SG_VHASH_NULLFREE(pCtx, pvh1);
	SG_VHASH_NULLFREE(pCtx, pvh2);
}

void u0028_vhash__update(SG_context * pCtx)
{
	SG_vhash * pvh;
	SG_string * pStrOriginal;
	SG_string * pStrSorted;
	const char * szOriginalExpected = "{\n\t\"z\" : 0,\n\t\"y\" : 1,\n\t\"x\" : 2,\n\t\"w\" : 3\n}\n";
	const char * szOriginalModifiedExpected = "{\n\t\"z\" : 0,\n\t\"y\" : 34,\n\t\"x\" : 2,\n\t\"w\" : 3\n}\n";
	const char * szSortedExpected   = "{\n\t\"w\" : 3,\n\t\"x\" : 2,\n\t\"y\" : 1,\n\t\"z\" : 0\n}\n";

	VERIFY_ERR_CHECK_DISCARD(  SG_VHASH__ALLOC(pCtx, &pvh)  );

	// add 4 keys in a known order

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvh,"z",0)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvh,"y",1)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvh,"x",2)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvh,"w",3)  );

	// verify json output is in known order

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStrOriginal)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__to_json(pCtx, pvh,pStrOriginal)  );

	VERIFYP_COND("sort",(strcmp(SG_string__sz(pStrOriginal),szOriginalExpected)==0),
			("Original [received %s] [expected %s]",
			 SG_string__sz(pStrOriginal),szOriginalExpected));
	SG_STRING_NULLFREE(pCtx, pStrOriginal);

	// modify y
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__update__int64(pCtx, pvh,"y",34)  );

	// verify json output is in known order

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStrOriginal)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__to_json(pCtx, pvh,pStrOriginal)  );

	VERIFYP_COND("sort",(strcmp(SG_string__sz(pStrOriginal),szOriginalModifiedExpected)==0),
			("Original [received %s] [expected %s]",
			 SG_string__sz(pStrOriginal),szOriginalModifiedExpected));
	SG_STRING_NULLFREE(pCtx, pStrOriginal);

	// put y back the way it was
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__update__int64(pCtx, pvh,"y",1)  );

	// sort vhash in ascending order

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__sort(pCtx, pvh,SG_FALSE,SG_vhash_sort_callback__increasing)  );

	// verify json output is in sorted order

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStrSorted)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__to_json(pCtx, pvh,pStrSorted)  );

	VERIFYP_COND("sort",(strcmp(SG_string__sz(pStrSorted),szSortedExpected)==0),
			("Sorted [received %s] [expected %s]",
			 SG_string__sz(pStrSorted),szSortedExpected));
	SG_STRING_NULLFREE(pCtx, pStrSorted);

	// clean up

	SG_VHASH_NULLFREE(pCtx, pvh);
}

void u0028_vhash__sort(SG_context * pCtx)
{
	SG_vhash * pvh;
	SG_string * pStrOriginal;
	SG_string * pStrSorted;
	const char * szOriginalExpected = "{\n\t\"z\" : 0,\n\t\"y\" : 1,\n\t\"x\" : 2,\n\t\"w\" : 3\n}\n";
	const char * szSortedExpected   = "{\n\t\"w\" : 3,\n\t\"x\" : 2,\n\t\"y\" : 1,\n\t\"z\" : 0\n}\n";

	VERIFY_ERR_CHECK_DISCARD(  SG_VHASH__ALLOC(pCtx, &pvh)  );

	// add 4 keys in a known order

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvh,"z",0)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvh,"y",1)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvh,"x",2)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvh,"w",3)  );

	// verify json output is in known order

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStrOriginal)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__to_json(pCtx, pvh,pStrOriginal)  );

	VERIFYP_COND("sort",(strcmp(SG_string__sz(pStrOriginal),szOriginalExpected)==0),
			("Original [received %s] [expected %s]",
			 SG_string__sz(pStrOriginal),szOriginalExpected));
	SG_STRING_NULLFREE(pCtx, pStrOriginal);

	// sort vhash in ascending order
	// we only created a single level vhash, so we don't bother with recursive sorting.

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__sort(pCtx, pvh,SG_FALSE,SG_vhash_sort_callback__increasing)  );

	// verify json output is in sorted order

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStrSorted)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__to_json(pCtx, pvh,pStrSorted)  );

	VERIFYP_COND("sort",(strcmp(SG_string__sz(pStrSorted),szSortedExpected)==0),
			("Sorted [received %s] [expected %s]",
			 SG_string__sz(pStrSorted),szSortedExpected));
	SG_STRING_NULLFREE(pCtx, pStrSorted);

	// clean up

	SG_VHASH_NULLFREE(pCtx, pvh);
}

void u0028_vhash__remove(SG_context * pCtx)
{
	SG_vhash * pvh;
	SG_string * pStrOriginal;
	SG_string * pStrSorted;
	const char * szOriginalExpected = "{\n\t\"z\" : 0,\n\t\"y\" : 1,\n\t\"x\" : 2,\n\t\"w\" : 3\n}\n";
	const char * szRemovedExpected = "{\n\t\"z\" : 0,\n\t\"x\" : 2,\n\t\"w\" : 3\n}\n";
	const char * szReturnedExpected = "{\n\t\"z\" : 0,\n\t\"x\" : 2,\n\t\"w\" : 3,\n\t\"y\" : 1\n}\n";
	const char * szSortedExpected   = "{\n\t\"w\" : 3,\n\t\"x\" : 2,\n\t\"y\" : 1,\n\t\"z\" : 0\n}\n";

	VERIFY_ERR_CHECK_DISCARD(  SG_VHASH__ALLOC(pCtx, &pvh)  );

	// add 4 keys in a known order

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvh,"z",0)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvh,"y",1)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvh,"x",2)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvh,"w",3)  );

	// verify json output is in known order

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStrOriginal)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__to_json(pCtx, pvh,pStrOriginal)  );

	VERIFYP_COND("sort",(strcmp(SG_string__sz(pStrOriginal),szOriginalExpected)==0),
			("Original [received %s] [expected %s]",
			 SG_string__sz(pStrOriginal),szOriginalExpected));
	SG_STRING_NULLFREE(pCtx, pStrOriginal);

	// remove y
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__remove(pCtx, pvh, "y")  );

	// verify json output no longer has y

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStrOriginal)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__to_json(pCtx, pvh,pStrOriginal)  );

	VERIFYP_COND("sort",(strcmp(SG_string__sz(pStrOriginal),szRemovedExpected)==0),
			("Original [received %s] [expected %s]",
			 SG_string__sz(pStrOriginal),szRemovedExpected));
	SG_STRING_NULLFREE(pCtx, pStrOriginal);

	// put y back
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__update__int64(pCtx, pvh,"y",1)  );

	// verify json output has y at the end

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStrOriginal)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__to_json(pCtx, pvh,pStrOriginal)  );

	VERIFYP_COND("sort",(strcmp(SG_string__sz(pStrOriginal),szReturnedExpected)==0),
			("Original [received %s] [expected %s]",
			 SG_string__sz(pStrOriginal),szReturnedExpected));
	SG_STRING_NULLFREE(pCtx, pStrOriginal);

	// sort vhash in ascending order

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__sort(pCtx,pvh,SG_FALSE,SG_vhash_sort_callback__increasing)  );

	// verify json output is in sorted order

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStrSorted)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__to_json(pCtx, pvh,pStrSorted)  );

	VERIFYP_COND("sort",(strcmp(SG_string__sz(pStrSorted),szSortedExpected)==0),
			("Sorted [received %s] [expected %s]",
			 SG_string__sz(pStrSorted),szSortedExpected));
	SG_STRING_NULLFREE(pCtx, pStrSorted);

	// clean up

	SG_VHASH_NULLFREE(pCtx, pvh);
}

void u0028_vhash__recursive_sort(SG_context * pCtx)
{
	SG_vhash * pvhtop;
	SG_vhash * pvhsub;
	SG_string * pStrOriginal;
	SG_string * pStrSorted;
	const char * szOriginalExpected        = "{\n\t\"d\" : 0,\n\t\"c\" : 1,\n\t\"b\" : 2,\n\t\"a\" : 3,\n\t\"h\" : \n\t{\n\t\t\"z\" : 0,\n\t\t\"y\" : 1,\n\t\t\"x\" : 2,\n\t\t\"w\" : 3\n\t}\n}\n";
	const char * szSortedSimpleExpected    = "{\n\t\"a\" : 3,\n\t\"b\" : 2,\n\t\"c\" : 1,\n\t\"d\" : 0,\n\t\"h\" : \n\t{\n\t\t\"z\" : 0,\n\t\t\"y\" : 1,\n\t\t\"x\" : 2,\n\t\t\"w\" : 3\n\t}\n}\n";
	const char * szSortedRecursiveExpected = "{\n\t\"a\" : 3,\n\t\"b\" : 2,\n\t\"c\" : 1,\n\t\"d\" : 0,\n\t\"h\" : \n\t{\n\t\t\"w\" : 3,\n\t\t\"x\" : 2,\n\t\t\"y\" : 1,\n\t\t\"z\" : 0\n\t}\n}\n";

	VERIFY_ERR_CHECK_DISCARD(  SG_VHASH__ALLOC(pCtx, &pvhsub)  );

	// add 4 keys in a known order

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvhsub,"z",0)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvhsub,"y",1)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvhsub,"x",2)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvhsub,"w",3)  );

	// create a second vhash and put in 4 simple values and the first vhash

	VERIFY_ERR_CHECK_DISCARD(  SG_VHASH__ALLOC(pCtx, &pvhtop)  );

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvhtop,"d",0)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvhtop,"c",1)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvhtop,"b",2)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__int64(pCtx, pvhtop,"a",3)  );

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__vhash(pCtx, pvhtop,"h",&pvhsub)  );

	// verify json output is in known order (the order that we added the keys)

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStrOriginal)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__to_json(pCtx, pvhtop,pStrOriginal)  );

	VERIFYP_COND("sort",(strcmp(SG_string__sz(pStrOriginal),szOriginalExpected)==0),
			("Original [received %s] [expected %s]",
			 SG_string__sz(pStrOriginal),szOriginalExpected));
	SG_STRING_NULLFREE(pCtx, pStrOriginal);

	// sort non-recursively vhash in ascending order

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__sort(pCtx,pvhtop,SG_FALSE,SG_vhash_sort_callback__increasing)  );

	// verify json output is in sorted order

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStrSorted)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__to_json(pCtx, pvhtop,pStrSorted)  );

	VERIFYP_COND("sort",(strcmp(SG_string__sz(pStrSorted),szSortedSimpleExpected)==0),
			("Sorted [received %s] [expected %s]",
			 SG_string__sz(pStrSorted),szSortedSimpleExpected));
	SG_STRING_NULLFREE(pCtx, pStrSorted);

	// sort recursively vhash in ascending order

	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__sort(pCtx,pvhtop,SG_TRUE,SG_vhash_sort_callback__increasing)  );

	// verify json output is in sorted order

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStrSorted)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__to_json(pCtx, pvhtop,pStrSorted)  );

	VERIFYP_COND("sort",(strcmp(SG_string__sz(pStrSorted),szSortedRecursiveExpected)==0),
			("Sorted [received %s] [expected %s]",
			 SG_string__sz(pStrSorted),szSortedRecursiveExpected));
	SG_STRING_NULLFREE(pCtx, pStrSorted);

	// clean up

	SG_VHASH_NULLFREE(pCtx, pvhtop);
}

void u0028_vhash__vfile(SG_context * pCtx)
{
	SG_pathname* pPath = NULL;
	SG_vhash* pvh = NULL;
	SG_vfile* pvf = NULL;
	SG_uint64 len = 0;
	SG_bool bExists = SG_FALSE;
	const char* psz = NULL;

	VERIFY_ERR_CHECK(  unittest__get_nonexistent_pathname(pCtx,&pPath)  );

	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(  SG_vfile__slurp(pCtx, pPath, &pvh)  );	// should fail

	VERIFY_ERR_CHECK(  SG_vfile__update__string__sz(pCtx, pPath, "foo", "bar")  );

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath,&bExists,NULL,NULL)  );
	VERIFY_COND("exists", bExists);

	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath, &len, NULL)  );
	VERIFY_COND("len is > zero", (len > 0));

	VERIFY_ERR_CHECK(  SG_vfile__slurp(pCtx, pPath, &pvh)  );

	VERIFY_COND("pvh", (pvh != NULL));
	VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "foo", &psz)  );
	VERIFY_COND("foo", (0 == strcmp(psz, "bar")));

	SG_VHASH_NULLFREE(pCtx, pvh);

	VERIFY_ERR_CHECK(  SG_vfile__begin(pCtx, pPath, SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, &pvh, &pvf)  );
	VERIFY_COND("pvh", (pvh != NULL));
	VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "foo", &psz)  );
	VERIFY_COND("foo", (0 == strcmp(psz, "bar")));
	VERIFY_COND("pvf", (pvf != NULL));
	VERIFY_ERR_CHECK(  SG_vfile__end(pCtx, &pvf, NULL)  );
	VERIFY_COND("pvf", (pvf == NULL));
	SG_VHASH_NULLFREE(pCtx, pvh);

	VERIFY_ERR_CHECK(  SG_vfile__begin(pCtx, pPath, SG_FILE_RDWR | SG_FILE_OPEN_EXISTING, &pvh, &pvf)  );
	VERIFY_COND("pvh", (pvh != NULL));
	VERIFY_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "foo", &psz)  );
	VERIFY_COND("foo", (0 == strcmp(psz, "bar")));
	VERIFY_COND("pvf", (pvf != NULL));
	VERIFY_ERR_CHECK(  SG_vfile__end(pCtx, &pvf, NULL)  );
	VERIFY_COND("pvf", (pvf == NULL));
	SG_VHASH_NULLFREE(pCtx, pvh);

	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath, &len, NULL)  );

	VERIFY_COND("len is now zero", (len == 0));

	VERIFY_ERR_CHECK(  SG_vfile__slurp(pCtx, pPath, &pvh)  );
	VERIFY_COND("pvh", (pvh == NULL));

	VERIFY_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath);

	return;

fail:
	return;
}

TEST_MAIN(u0028_vhash)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0028_vhash__test_null(pCtx)  );
//	BEGIN_TEST(  u0028_vhash__test_1(pCtx)  );
//	BEGIN_TEST(  u0028_vhash__test_2(pCtx)  );
//	BEGIN_TEST(  u0028_vhash__test_4(pCtx)  );
//	BEGIN_TEST(  u0028_vhash__sort(pCtx)  );
//	BEGIN_TEST(  u0028_vhash__update(pCtx)  );
//	BEGIN_TEST(  u0028_vhash__remove(pCtx)  );
//	BEGIN_TEST(  u0028_vhash__recursive_sort(pCtx)  );
//	BEGIN_TEST(  u0028_vhash__vfile(pCtx)  );
//	BEGIN_TEST(  u0028_vhash__varray_1(pCtx)  );
//	BEGIN_TEST(  u0028_vhash__test_5(pCtx)  );

	TEMPLATE_MAIN_END;
}

