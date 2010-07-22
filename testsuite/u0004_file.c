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

// testsuite/unittests/u0004_file.c
// test file io operations
//////////////////////////////////////////////////////////////////

#include <sg.h>

#include "unittests.h"

//////////////////////////////////////////////////////////////////

int u0004_file__test_file(SG_context * pCtx, SG_uint64 len)
{
	SG_file* pf;
	SG_byte firstbyte = 0;
	SG_uint32 nbw;
	SG_pathname * pPathname = NULL;
	SG_bool b = SG_FALSE;
	SG_fsobj_stat st;

	VERIFY_ERR_CHECK_RETURN(  unittest__get_nonexistent_pathname(pCtx, &pPathname)  );

	SG_file__open__pathname(pCtx, pPathname, SG_FILE_CREATE_NEW | SG_FILE_RDWR, 0777, &pf);
	VERIFYP_CTX_IS_OK("test_file_creat", pCtx, ("create should succeed because the file does not exist"));
	SG_context__err_reset(pCtx);

	// TODO append

	if (len > 0)
	{
		SG_byte* b;

		if (len < 32768)
		{
			SG_uint64 pos = 0;

			b = SG_malloc((SG_uint32)len);
			memset(b, 42, (SG_uint32)len);

			firstbyte = b[0];

			SG_file__write(pCtx, pf, (SG_uint32)len, b, &nbw);
			VERIFY_CTX_IS_OK("test_file_creat", pCtx);
			VERIFY_COND("test_file_creat", (nbw == (SG_uint32)len));
			SG_context__err_reset(pCtx);

			SG_file__tell(pCtx, pf, &pos);
			VERIFY_CTX_IS_OK("test_file_creat", pCtx);
			VERIFY_COND("test_file_creat", (pos == len));
			SG_context__err_reset(pCtx);

			SG_NULLFREE(pCtx, b);
		}
		else
		{
			SG_uint64 count = len;

			b = SG_malloc(32768);
			memset(b, 42, 32768);
			firstbyte = b[0];

			while (count > 0)
			{
				SG_uint32 amt = (count < 32768) ? (SG_uint32)count : 32768;

				SG_file__write(pCtx, pf, amt, b, NULL);
				VERIFY_CTX_IS_OK("test_file_creat", pCtx);
				SG_context__err_reset(pCtx);

				count -= amt;
			}
			SG_NULLFREE(pCtx, b);
		}
	}

	SG_file__close(pCtx, &pf);
	VERIFY_COND("test_file_creat", !SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	{
		SG_uint64 mylen = 0;
		SG_fsobj_type ft;

		SG_fsobj__exists__pathname(pCtx, pPathname, &b, &ft, NULL);
		VERIFY_CTX_IS_OK("test_file_creat", pCtx);
		VERIFYP_COND("file exists", (b), ("file should exist"));
		VERIFY_COND("file exists", (ft == SG_FSOBJ_TYPE__REGULAR));
		SG_context__err_reset(pCtx);

		SG_fsobj__length__pathname(pCtx, pPathname, &mylen, &ft);
		VERIFY_CTX_IS_OK("test_file_creat", pCtx);
		VERIFYP_COND("test_file_length", (mylen == len), ("file has wrong length"));
		VERIFY_COND("file exists", (ft == SG_FSOBJ_TYPE__REGULAR));
		SG_context__err_reset(pCtx);

		SG_file__open__pathname(pCtx, pPathname, SG_FILE_CREATE_NEW, 0777, &pf);
		VERIFYP_CTX_HAS_ERR("test_file_creat_excl", pCtx, ("create/excl must fail here because the file already exists"));
		SG_context__err_reset(pCtx);

		if (len > 0)
		{
			SG_uint32 got = 0;
			SG_byte b;

			SG_file__open__pathname(pCtx, pPathname, SG_FILE_OPEN_EXISTING | SG_FILE_RDONLY, SG_FSOBJ_PERMS__UNUSED, &pf);
			VERIFY_CTX_IS_OK("test_file_creat", pCtx);
			SG_context__err_reset(pCtx);

			SG_file__read(pCtx, pf, 1, &b, &got);
			VERIFY_CTX_IS_OK("test_file_creat", pCtx);
			VERIFY_COND("test_file_creat", (got == 1));
			VERIFY_COND("test_file_creat", (b == firstbyte));
			SG_context__err_reset(pCtx);

			SG_file__close(pCtx, &pf);
			VERIFY_CTX_IS_OK("test_file_creat", pCtx);
			SG_context__err_reset(pCtx);
		}
	}

	if (len > 2)
	{
		SG_file__open__pathname(pCtx, pPathname, SG_FILE_OPEN_EXISTING | SG_FILE_RDWR, SG_FSOBJ_PERMS__UNUSED, &pf);
		VERIFY_CTX_IS_OK("test_file_creat", pCtx);
		SG_context__err_reset(pCtx);

		SG_file__seek(pCtx, pf, 2);
		VERIFY_CTX_IS_OK("test_file_creat", pCtx);
		SG_context__err_reset(pCtx);

		SG_file__truncate(pCtx, pf);
		VERIFY_CTX_IS_OK("test_file_creat", pCtx);
		SG_context__err_reset(pCtx);

		SG_file__close(pCtx, &pf);
		VERIFY_CTX_IS_OK("test_file_creat", pCtx);
		SG_context__err_reset(pCtx);

		SG_fsobj__stat__pathname(pCtx, pPathname, &st);
		VERIFY_CTX_IS_OK("ctx_ok", pCtx);
		SG_context__err_reset(pCtx);

		VERIFY_COND("len is 2 now", (st.size == 2));
	}

	SG_fsobj__remove__pathname(pCtx, pPathname);
	VERIFY_CTX_IS_OK("test_file_creat", pCtx);
	SG_context__err_reset(pCtx);

	SG_fsobj__exists__pathname(pCtx, pPathname, &b, NULL, NULL);
	VERIFY_CTX_IS_OK("test_file_creat", pCtx);
	VERIFY_COND("file exists", !b);
	SG_context__err_reset(pCtx);

	SG_PATHNAME_NULLFREE(pCtx, pPathname);

	return 1;
}

TEST_MAIN(u0004_file)
{
	TEMPLATE_MAIN_START;

    BEGIN_TEST(  u0004_file__test_file(pCtx, 1)  );
    BEGIN_TEST(  u0004_file__test_file(pCtx, 5)  );
    BEGIN_TEST(  u0004_file__test_file(pCtx, 0)  );
    BEGIN_TEST(  u0004_file__test_file(pCtx, 10)  );
    BEGIN_TEST(  u0004_file__test_file(pCtx, 1000)  );
    BEGIN_TEST(  u0004_file__test_file(pCtx, 32768)  );
    BEGIN_TEST(  u0004_file__test_file(pCtx, 100000)  );

	TEMPLATE_MAIN_END;
}

