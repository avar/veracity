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

int u0010_dbrecord__test_dbrecord(SG_context * pCtx)
{
	SG_dbrecord* prec;
	const char* pszName;
	const char* pszValue;
	SG_uint32 count = 0;

	VERIFY_ERR_CHECK_DISCARD(  SG_dbrecord__alloc(pCtx,&prec)  );
	VERIFY_COND("prec valid", (prec != NULL));
	SG_dbrecord__count_pairs(pCtx, prec, &count);
	VERIFY_COND("count_pairs", (!SG_context__has_err(pCtx) && (count == 0)));
	SG_context__err_reset(pCtx);

	SG_dbrecord__add_pair(pCtx, prec, "x", "5");
	VERIFY_COND("ok", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);
	SG_dbrecord__count_pairs(pCtx, prec, &count);
	VERIFY_COND("count_pairs", (!SG_context__has_err(pCtx) && (count == 1)));
	SG_context__err_reset(pCtx);

	/* the following should fail because x already exists in this record */
	SG_dbrecord__add_pair(pCtx, prec, "x", "33");
	VERIFY_COND("ok", SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);
	SG_dbrecord__count_pairs(pCtx, prec, &count);
	VERIFY_COND("count_pairs", (!SG_context__has_err(pCtx) && (count == 1)));
	SG_context__err_reset(pCtx);

	SG_dbrecord__add_pair(pCtx, prec, "y", "5");
	VERIFY_COND("ok", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);
	SG_dbrecord__count_pairs(pCtx, prec, &count);
	VERIFY_COND("count_pairs", (!SG_context__has_err(pCtx) && (count == 2)));
	SG_context__err_reset(pCtx);

	SG_dbrecord__add_pair(pCtx, prec, "z", "5");
	VERIFY_COND("ok", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);
	SG_dbrecord__count_pairs(pCtx, prec, &count);
	VERIFY_COND("count_pairs", (!SG_context__has_err(pCtx) && (count == 3)));
	SG_context__err_reset(pCtx);

	SG_dbrecord__add_pair(pCtx, prec, "w", "5");
	VERIFY_COND("ok", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);
	SG_dbrecord__count_pairs(pCtx, prec, &count);
	VERIFY_COND("count_pairs", (!SG_context__has_err(pCtx) && (count == 4)));
	SG_context__err_reset(pCtx);

	SG_dbrecord__get_nth_pair(pCtx, prec, 2, &pszName, &pszValue);
	VERIFY_COND("SG_dbrecord__get_nth_pair", !SG_context__has_err(pCtx));
	VERIFY_COND("SG_dbrecord__get_nth_pair", (pszName));
	VERIFY_COND("SG_dbrecord__get_nth_pair", (pszValue));
	SG_context__err_reset(pCtx);

	SG_dbrecord__get_nth_pair(pCtx, prec, 7, &pszName, &pszValue);
	VERIFY_COND("SG_dbrecord__get_nth_pair", SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	SG_DBRECORD_NULLFREE(pCtx, prec);

	return 1;
}

int u0010_dbrecord__test2(SG_context * pCtx)
{
	SG_dbrecord* prec1 = NULL;
	SG_string* pstr1 = NULL;
	SG_dbrecord* prec2 = NULL;
	SG_string* pstr2 = NULL;

	VERIFY_ERR_CHECK_DISCARD(  SG_dbrecord__alloc(pCtx, &prec1)  );
	VERIFY_COND("prec valid", (prec1 != NULL));

	SG_dbrecord__add_pair(pCtx, prec1, "a", "5");
	VERIFY_COND("ok", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	SG_dbrecord__add_pair(pCtx, prec1, "b", "5");
	VERIFY_COND("ok", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	SG_dbrecord__add_pair(pCtx, prec1, "c", "5");
	VERIFY_COND("ok", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	SG_STRING__ALLOC(pCtx, &pstr1);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_dbrecord__to_json(pCtx, prec1, pstr1);
	VERIFY_COND("ok", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	VERIFY_ERR_CHECK_DISCARD(  SG_dbrecord__alloc(pCtx, &prec2)  );
	VERIFY_COND("prec valid", (prec2 != NULL));

	SG_dbrecord__add_pair(pCtx, prec2, "b", "5");
	VERIFY_COND("ok", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	SG_dbrecord__add_pair(pCtx, prec2, "c", "5");
	VERIFY_COND("ok", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	SG_dbrecord__add_pair(pCtx, prec2, "a", "5");
	VERIFY_COND("ok", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	SG_STRING__ALLOC(pCtx, &pstr2);
	VERIFY_CTX_IS_OK("ctx_ok", pCtx);
	SG_context__err_reset(pCtx);

	SG_dbrecord__to_json(pCtx, prec2, pstr2);
	VERIFY_COND("ok", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	VERIFY_COND("match", (0 == strcmp(SG_string__sz(pstr1), SG_string__sz(pstr2))));

	SG_STRING_NULLFREE(pCtx, pstr1);

	SG_DBRECORD_NULLFREE(pCtx, prec1);

	SG_STRING_NULLFREE(pCtx, pstr2);

	SG_DBRECORD_NULLFREE(pCtx, prec2);

	return 1;
}

TEST_MAIN(u0010_dbrecord)
{
	TEMPLATE_MAIN_START;

    BEGIN_TEST(  u0010_dbrecord__test_dbrecord(pCtx)  );
	BEGIN_TEST(  u0010_dbrecord__test2(pCtx)  );

	TEMPLATE_MAIN_END;
}

