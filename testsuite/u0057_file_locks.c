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

// testsuite/unittests/u0057_file_locks.c
// test file io operations
//////////////////////////////////////////////////////////////////

#include <sg.h>

#include "unittests.h"

//////////////////////////////////////////////////////////////////

/**
 * TODO what is this supposed to be testing?
 *
 *
 */
void u0057_file_locks__test_no_locks(SG_context * pCtx)
{
	SG_file* pFile1;
	SG_file* pFile2;
	SG_uint32 nbw;
	SG_pathname * pPathname = NULL;
	char buf[SG_TID_MAX_BUFFER_LENGTH];
	SG_uint32 i;
	SG_uint32 lenTid;

	SG_ERR_IGNORE(  unittest__get_nonexistent_pathname(pCtx, &pPathname)  );

	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathname, SG_FILE_CREATE_NEW | SG_FILE_RDWR, 0777, &pFile1)  );

	for (i=0; i<100; i++)
	{
		VERIFY_ERR_CHECK(  SG_tid__generate(pCtx, buf, sizeof(buf))  );
		lenTid = strlen(buf);

		VERIFY_ERR_CHECK(  SG_file__write(pCtx, pFile1, lenTid, (SG_byte*) buf, &nbw)  );
	}

	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathname, SG_FILE_OPEN_EXISTING | SG_FILE_RDONLY, 0777, &pFile2)  );

	for (i=0; i<100; i++)
	{
		VERIFY_ERR_CHECK(  SG_tid__generate(pCtx, buf, sizeof(buf))  );
		lenTid = strlen(buf);

		VERIFY_ERR_CHECK(  SG_file__write(pCtx, pFile1, lenTid, (SG_byte*) buf, &nbw)  );
		VERIFY_ERR_CHECK(  SG_file__read(pCtx, pFile2, lenTid, (SG_byte*) buf, &nbw)  );
	}

	VERIFY_ERR_CHECK(  SG_file__close(pCtx, &pFile2)  );
	VERIFY_ERR_CHECK(  SG_file__close(pCtx, &pFile1)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathname);

	return;
fail:
	return;
}

TEST_MAIN(u0057_file_locks)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0057_file_locks__test_no_locks(pCtx)  );

	TEMPLATE_MAIN_END;
}
