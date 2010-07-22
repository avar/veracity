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

// testsuite/unittests/u0008_misc_utils.c
// test sg_misc_utils
//////////////////////////////////////////////////////////////////

#include <sg.h>

#include "unittests.h"

void u0008_misc_utils__int64_fits(void)
{
    VERIFY_COND("fit",  SG_int64__fits_in_int32( (SG_int64) (0)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_int32((SG_int64) (-1)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_int32((SG_int64) (1)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_int32((SG_int64) (-128)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_int32((SG_int64) (-32768)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_int32((SG_int64) (-32769)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_int32((SG_int64) (-2147483647-1)  ));
    VERIFY_COND("fit",  !SG_int64__fits_in_int32((SG_int64) (-3147483647LL)  ));

    VERIFY_COND("fit",  SG_int64__fits_in_int32( (SG_int64) (SG_UINT8_MAX)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_int32( (SG_int64) (SG_UINT16_MAX)  ));
    VERIFY_COND("fit",  !SG_int64__fits_in_int32( (SG_int64) (SG_UINT32_MAX)  ));
    // VERIFY_COND("fit",  !SG_int64__fits_in_int32( (SG_int64) (SG_UINT64_MAX)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_int32( (SG_int64) (SG_INT8_MAX)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_int32( (SG_int64) (SG_INT16_MAX)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_int32( (SG_int64) (SG_INT32_MAX)  ));
    VERIFY_COND("fit",  !SG_int64__fits_in_int32( (SG_int64) (SG_INT64_MAX)  ));

    VERIFY_COND("fit",  SG_uint64__fits_in_int32( (SG_uint64) (SG_UINT8_MAX)  ));
    VERIFY_COND("fit",  SG_uint64__fits_in_int32( (SG_uint64) (SG_UINT16_MAX)  ));
    VERIFY_COND("fit",  !SG_uint64__fits_in_int32( (SG_uint64) (SG_UINT32_MAX)  ));
    VERIFY_COND("fit",  !SG_uint64__fits_in_int32( (SG_uint64) (SG_UINT64_MAX)  ));
    VERIFY_COND("fit",  SG_uint64__fits_in_int32( (SG_uint64) (SG_INT8_MAX)  ));
    VERIFY_COND("fit",  SG_uint64__fits_in_int32( (SG_uint64) (SG_INT16_MAX)  ));
    VERIFY_COND("fit",  SG_uint64__fits_in_int32( (SG_uint64) (SG_INT32_MAX)  ));
    VERIFY_COND("fit",  !SG_uint64__fits_in_int32( (SG_uint64) (SG_INT64_MAX)  ));

    VERIFY_COND("fit",  SG_int64__fits_in_uint32( (SG_int64) (0)  ));
    VERIFY_COND("fit",  !SG_int64__fits_in_uint32((SG_int64) (-1)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_uint32((SG_int64) (1)  ));
    VERIFY_COND("fit",  !SG_int64__fits_in_uint32((SG_int64) (-128)  ));
    VERIFY_COND("fit",  !SG_int64__fits_in_uint32((SG_int64) (-32768)  ));
    VERIFY_COND("fit",  !SG_int64__fits_in_uint32((SG_int64) (-32769)  ));
    VERIFY_COND("fit",  !SG_int64__fits_in_uint32((SG_int64) (-2147483647-1)  ));
    VERIFY_COND("fit",  !SG_int64__fits_in_uint32((SG_int64) (-3147483647LL)  ));

    VERIFY_COND("fit",  SG_int64__fits_in_uint32( (SG_int64) (SG_UINT8_MAX)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_uint32( (SG_int64) (SG_UINT16_MAX)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_uint32( (SG_int64) (SG_UINT32_MAX)  ));
    // VERIFY_COND("fit",  !SG_int64__fits_in_uint32( (SG_int64) (SG_UINT64_MAX)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_uint32( (SG_int64) (SG_INT8_MAX)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_uint32( (SG_int64) (SG_INT16_MAX)  ));
    VERIFY_COND("fit",  SG_int64__fits_in_uint32( (SG_int64) (SG_INT32_MAX)  ));
    VERIFY_COND("fit",  !SG_int64__fits_in_uint32( (SG_int64) (SG_INT64_MAX)  ));

    VERIFY_COND("fit",  SG_uint64__fits_in_uint32( (SG_uint64) (SG_UINT8_MAX)  ));
    VERIFY_COND("fit",  SG_uint64__fits_in_uint32( (SG_uint64) (SG_UINT16_MAX)  ));
    VERIFY_COND("fit",  SG_uint64__fits_in_uint32( (SG_uint64) (SG_UINT32_MAX)  ));
    VERIFY_COND("fit",  !SG_uint64__fits_in_uint32( (SG_uint64) (SG_UINT64_MAX)  ));
    VERIFY_COND("fit",  SG_uint64__fits_in_uint32( (SG_uint64) (SG_INT8_MAX)  ));
    VERIFY_COND("fit",  SG_uint64__fits_in_uint32( (SG_uint64) (SG_INT16_MAX)  ));
    VERIFY_COND("fit",  SG_uint64__fits_in_uint32( (SG_uint64) (SG_INT32_MAX)  ));
    VERIFY_COND("fit",  !SG_uint64__fits_in_uint32( (SG_uint64) (SG_INT64_MAX)  ));
}

void u0008_misc_utils__test_strdup_ok(SG_context * pCtx)
{
	char * sz = NULL;

	VERIFY_ERR_CHECK(  SG_strdup(pCtx,"abcdef",&sz)  );
	VERIFY_COND("test_strdup", (strcmp(sz,"abcdef")==0));

fail:
	SG_NULLFREE(pCtx, sz);
}

void u0008_misc_utils__test_strdup_errors(SG_context * pCtx)
{
	char * sz = NULL;

	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(SG_strdup(pCtx,NULL,&sz));
	VERIFY_ERR_CHECK_HAS_ERR_DISCARD(SG_strdup(pCtx,"abcdef",NULL));
}

void u0008_misc_utils__test_strcpy_exact(SG_context * pCtx)
{
	char buf[21];

	memset(buf, 'q', 21);

	SG_strcpy(pCtx, buf, 20, "0123456789012345678");
	VERIFY_CTX_IS_OK("err", pCtx);

	VERIFY_COND("u0008_misc_utils__test_strcpy_exact", buf[19] == 0);
	VERIFY_COND("u0008_misc_utils__test_strcpy_exact", buf[20] == 'q');

	memset(buf, 'q', 21);

	SG_context__err_reset(pCtx);
	SG_strcpy(pCtx, buf, 20, "0123456thisstringisnowwaytoolong7890123456789");
	VERIFYP_CTX_ERR_EQUALS("err", pCtx,  SG_ERR_BUFFERTOOSMALL, ("0123456thisstringisnowwaytoolong7890123456789"));

	VERIFY_COND("u0008_misc_utils__test_strcpy_exact", buf[19] == 0);
	VERIFY_COND("u0008_misc_utils__test_strcpy_exact", buf[20] == 'q');

	memset(buf, 'q', 21);

	SG_context__err_reset(pCtx);
	SG_strcpy(pCtx,buf, 10, "m");
	VERIFY_CTX_IS_OK("err", pCtx);

	VERIFY_COND("u0008_misc_utils__test_strcpy_exact", buf[1] == 0);
	VERIFY_COND("u0008_misc_utils__test_strcpy_exact", buf[9] == 0);
	VERIFY_COND("u0008_misc_utils__test_strcpy_exact", buf[10] == 'q');
}

void u0008_misc_utils__test_strcat(SG_context * pCtx)
{
	char buf[21];

	memset(buf, 'q', 21);

	SG_strcpy(pCtx, buf, 10, "hello");
	VERIFY_CTX_IS_OK("err", pCtx);
	VERIFY_COND("u0008_misc_utils__test_strcpy_exact", buf[9] == 0);
	VERIFY_COND("u0008_misc_utils__test_strcpy_exact", buf[10] == 'q');

	VERIFY_COND("u0008_misc_utils__test_strcat", (0 == strcmp(buf, "hello")));

	SG_context__err_reset(pCtx);
	SG_strcat(pCtx, buf, 10, "m");
	VERIFY_CTX_IS_OK("err", pCtx);
	VERIFY_COND("u0008_misc_utils__test_strcat", (0 == strcmp(buf, "hellom")));

	SG_context__err_reset(pCtx);
	SG_strcat(pCtx, buf, 10, "fiddlesticks");
	VERIFYP_CTX_ERR_EQUALS("err", pCtx,  SG_ERR_BUFFERTOOSMALL, ("fiddlesticks"));
	VERIFY_COND("u0008_misc_utils__test_strcpy_exact", buf[9] == 0);
	VERIFY_COND("u0008_misc_utils__test_strcpy_exact", buf[10] == 'q');
	VERIFY_COND("u0008_misc_utils__test_strcat", (0 == strcmp(buf, "hellomfid")));
	SG_context__err_reset(pCtx);
}

void u0008_misc_utils__test_strcpy(SG_context * pCtx)
{
	char bufSrc[20];
	char bufAnswer[20];
	char bufWork[20];

	// fill work with all x's (no terminator) and copy whole input string.
	// verify no truncation & proper termination.  remainder of bufWork
	// past NULL is also null.

	memset(bufSrc,'a',20);		// src is 10 a's
	bufSrc[10] = 0;

	memset(bufAnswer,'a',10);
	memset(bufAnswer+10,0,10);

	memset(bufWork,'x',sizeof(bufWork));	// pre-fill destintation with known garbage

	SG_strcpy(pCtx,bufWork,20,bufSrc);			// copy first strlen() bytes of src -- null the rest.

	VERIFY_COND("test_strcpy", (strcmp(bufWork,bufSrc) == 0));
	VERIFY_COND("test_strcpy", (memcmp(bufWork,bufAnswer,20) == 0));

	// copy src with truncation.
	// verify no buffer over-run.

	memset(bufSrc,'a',20);		// src is 10 a's
	bufSrc[10] = 0;

	memset(bufAnswer,'a',4);
	memset(bufAnswer+4,0,1);
	memset(bufAnswer+5,'x',15);

	memset(bufWork,'x',sizeof(bufWork));

	SG_context__err_reset(pCtx);
	SG_strcpy(pCtx,bufWork,5,bufSrc);
	VERIFY_COND("test_strcpy", (memcmp(bufWork,bufAnswer,20) == 0));
	SG_context__err_reset(pCtx);
}

void u0008_misc_utils__test_sprintf(SG_context * pCtx)
{
	char buf[1024];

	SG_sprintf(pCtx, buf,100,"abcd");
	VERIFY_CTX_IS_OK("test_sprintf", pCtx);
	VERIFY_COND("test_sprintf",(strcmp(buf,"abcd")==0));

	SG_context__err_reset(pCtx);
	SG_sprintf(pCtx, buf,5,"abcd");
	VERIFY_CTX_IS_OK("test_sprintf", pCtx);
	VERIFY_COND("test_sprintf",(strcmp(buf,"abcd")==0));

	SG_context__err_reset(pCtx);
	SG_sprintf(pCtx,buf,4,"abcd");
	VERIFYP_CTX_ERR_EQUALS("test_sprintf", pCtx, SG_ERR_BUFFERTOOSMALL,("abcd"));
	VERIFY_COND("test_sprintf",(buf[0] == 0));
	SG_context__err_reset(pCtx);
}

TEST_MAIN(u0008_misc_utils)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0008_misc_utils__int64_fits(/*pCtx*/)  );
	BEGIN_TEST(  u0008_misc_utils__test_strdup_ok(pCtx)  );
	BEGIN_TEST(  u0008_misc_utils__test_strdup_errors(pCtx)  );
	BEGIN_TEST(  u0008_misc_utils__test_strcpy_exact(pCtx)  );
	BEGIN_TEST(  u0008_misc_utils__test_strcat(pCtx)  );
	BEGIN_TEST(  u0008_misc_utils__test_strcpy(pCtx)  );
	BEGIN_TEST(  u0008_misc_utils__test_sprintf(pCtx)  );

	TEMPLATE_MAIN_END;
}
