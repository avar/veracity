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

// TODO consider removing this test.  All it does is test
// SG_dbcriteria__check_one_record, which is used only by
// this test.

void u0015_dbcriteria__test_dbcriteria(SG_context * pCtx)
{
	SG_dbrecord *prec = NULL;
	SG_uint32 count = SG_UINT32_MAX;

	SG_varray* pcrit = NULL;
	SG_bool b;

	VERIFY_ERR_CHECK(  SG_dbrecord__alloc(pCtx, &prec)  );
	VERIFY_COND("prec valid", (prec != NULL));

	VERIFY_ERR_CHECK(  SG_dbrecord__count_pairs(pCtx, prec, &count)  );
	VERIFY_COND("count_pairs", count == 0);

	VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "x", "17")  );
	VERIFY_ERR_CHECK(  SG_dbrecord__count_pairs(pCtx, prec, &count)  );
	VERIFY_COND("count_pairs", count == 1);

	VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "y", "33")  );
	VERIFY_ERR_CHECK(  SG_dbrecord__count_pairs(pCtx, prec, &count)  );
	VERIFY_COND("count_pairs", count == 2);

	VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "z", "40")  );
	VERIFY_ERR_CHECK(  SG_dbrecord__count_pairs(pCtx, prec, &count)  );
	VERIFY_COND("count_pairs", count == 3);

	VERIFY_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, prec, "w", "-22")  );
	VERIFY_ERR_CHECK(  SG_dbrecord__count_pairs(pCtx, prec, &count)  );
	VERIFY_COND("count_pairs", count == 4);


	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pcrit)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "x")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "exists")  );

	VERIFY_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit, prec, &b)  );
	VERIFY_COND("crit matched", (b));
	SG_VARRAY_NULLFREE(pCtx, pcrit);

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pcrit)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "x")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "==")  );
	VERIFY_ERR_CHECK(  SG_varray__append__int64(pCtx, pcrit, 17)  );

	VERIFY_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit, prec, &b)  );
	VERIFY_COND("crit matched", (b));
	SG_VARRAY_NULLFREE(pCtx, pcrit);

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pcrit)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "x")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "==")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "17")  );

	VERIFY_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit, prec, &b)  );
	VERIFY_COND("crit matched", (b));
	SG_VARRAY_NULLFREE(pCtx, pcrit);

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pcrit)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "x")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "<=")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "17")  );

	VERIFY_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit, prec, &b)  );
	VERIFY_COND("crit matched", (b));
	SG_VARRAY_NULLFREE(pCtx, pcrit);

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pcrit)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "x")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "<")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "18")  );

	VERIFY_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit, prec, &b)  );
	VERIFY_COND("crit matched", (b));
	SG_VARRAY_NULLFREE(pCtx, pcrit);

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pcrit)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "x")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, ">")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "16")  );

	VERIFY_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit, prec, &b)  );
	VERIFY_COND("crit matched", (b));
	SG_VARRAY_NULLFREE(pCtx, pcrit);

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pcrit)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "x")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, ">=")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "17")  );

	VERIFY_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit, prec, &b)  );
	VERIFY_COND("crit matched", (b));
	SG_VARRAY_NULLFREE(pCtx, pcrit);

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pcrit)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "x")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, ">")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "17")  );

	VERIFY_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit, prec, &b)  );
	VERIFY_COND("crit should not match", (!b));
	SG_VARRAY_NULLFREE(pCtx, pcrit);

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pcrit)  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "x")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "<")  );
	VERIFY_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pcrit, "17")  );

	VERIFY_ERR_CHECK(  SG_dbcriteria__check_one_record(pCtx, pcrit, prec, &b)  );
	VERIFY_COND("crit should not match", (!b));
	SG_VARRAY_NULLFREE(pCtx, pcrit);

	/* TODO more tests here */


fail:
	SG_VARRAY_NULLFREE(pCtx, pcrit);
	SG_DBRECORD_NULLFREE(pCtx, prec);
}

TEST_MAIN(u0015_dbcriteria)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0015_dbcriteria__test_dbcriteria(pCtx)  );

	TEMPLATE_MAIN_END;
}
