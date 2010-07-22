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

void u0052_zip__test_1(SG_context * pCtx)
{
    SG_zip* pzip = NULL;
    SG_unzip* punzip = NULL;
    char buf_tid[SG_TID_MAX_BUFFER_LENGTH];
    SG_pathname* pPath = NULL;
    SG_uint32 count = 0;
    SG_byte buf[256];
    SG_uint32 iBytesRead;
    SG_bool b;
    const char* psz_name = NULL;
    SG_uint64 iLen = 0;

    VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_tid, sizeof(buf_tid), 32)  );
    VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPath, buf_tid)  );

    /* ---------------------------------------------------------------- */
    /* Create the zip file */
    /* ---------------------------------------------------------------- */
    VERIFY_ERR_CHECK(  SG_zip__open(pCtx, pPath, &pzip)  );

#define FILE1_CONTENTS "hello world"
    VERIFY_ERR_CHECK(  SG_zip__begin_file(pCtx, pzip, "f1")  );
    VERIFY_ERR_CHECK(  SG_zip__write(pCtx, pzip, (SG_byte*) FILE1_CONTENTS, (SG_uint32) (1 + strlen(FILE1_CONTENTS)))  );
    VERIFY_ERR_CHECK(  SG_zip__end_file(pCtx, pzip)  );

#define FILE2_CONTENTS "this is a test of the emergency broadcast system"
    VERIFY_ERR_CHECK(  SG_zip__begin_file(pCtx, pzip, "f2")  );
    VERIFY_ERR_CHECK(  SG_zip__write(pCtx, pzip, (SG_byte*) FILE2_CONTENTS, (SG_uint32) (1 + strlen(FILE2_CONTENTS)))  );
    VERIFY_ERR_CHECK(  SG_zip__end_file(pCtx, pzip)  );

#define FILE3_CONTENTS "x"
    VERIFY_ERR_CHECK(  SG_zip__begin_file(pCtx, pzip, "f3")  );
    VERIFY_ERR_CHECK(  SG_zip__write(pCtx, pzip, (SG_byte*) FILE3_CONTENTS, (SG_uint32) (1 + strlen(FILE3_CONTENTS)))  );
    VERIFY_ERR_CHECK(  SG_zip__end_file(pCtx, pzip)  );

#define FILE4_CONTENTS "spain"
    VERIFY_ERR_CHECK(  SG_zip__begin_file(pCtx, pzip, "f4")  );
    VERIFY_ERR_CHECK(  SG_zip__write(pCtx, pzip, (SG_byte*) FILE4_CONTENTS, (SG_uint32) (1 + strlen(FILE4_CONTENTS)))  );
    VERIFY_ERR_CHECK(  SG_zip__end_file(pCtx, pzip)  );

    VERIFY_ERR_CHECK(  SG_zip__nullclose(pCtx, &pzip)  );

    /* ---------------------------------------------------------------- */
    /* Read the zip file */
    /* ---------------------------------------------------------------- */
    VERIFY_ERR_CHECK(  SG_unzip__open(pCtx, pPath, &punzip)  );

    count = 0;
    b = SG_FALSE;

    VERIFY_ERR_CHECK(  SG_unzip__goto_first_file(pCtx, punzip, &b, NULL, NULL)  );
    while (b)
    {
        count++;
        VERIFY_ERR_CHECK(  SG_unzip__goto_next_file(pCtx, punzip, &b, NULL, NULL)  );
    }
    VERIFY_COND("count", (4 == count));

    b = SG_FALSE;
    VERIFY_ERR_CHECK(  SG_unzip__goto_first_file(pCtx, punzip, &b, &psz_name, &iLen)  );
    VERIFY_COND("b", b);
    VERIFY_COND("name", (0 == strcmp(psz_name, "f1")));
    VERIFY_COND("len", (iLen == (1 + strlen(FILE1_CONTENTS))));

    b = SG_FALSE;
    VERIFY_ERR_CHECK(  SG_unzip__goto_next_file(pCtx, punzip, &b, &psz_name, &iLen)  );
    VERIFY_COND("b", b);
    VERIFY_COND("name", (0 == strcmp(psz_name, "f2")));
    VERIFY_COND("len", (iLen == (1 + strlen(FILE2_CONTENTS))));

    b = SG_FALSE;
    VERIFY_ERR_CHECK(  SG_unzip__goto_next_file(pCtx, punzip, &b, &psz_name, &iLen)  );
    VERIFY_COND("b", b);
    VERIFY_COND("name", (0 == strcmp(psz_name, "f3")));
    VERIFY_COND("len", (iLen == (1 + strlen(FILE3_CONTENTS))));

    b = SG_FALSE;
    VERIFY_ERR_CHECK(  SG_unzip__goto_next_file(pCtx, punzip, &b, &psz_name, &iLen)  );
    VERIFY_COND("b", b);
    VERIFY_COND("name", (0 == strcmp(psz_name, "f4")));
    VERIFY_COND("len", (iLen == (1 + strlen(FILE4_CONTENTS))));

    /* verify that locate_file works */
    VERIFY_ERR_CHECK(  SG_unzip__locate_file(pCtx, punzip, "f2", &b, &iLen)  );
    VERIFY_COND("b", b);
    VERIFY_COND("len", (iLen == (1 + strlen(FILE2_CONTENTS))));
    VERIFY_ERR_CHECK(  SG_unzip__currentfile__open(pCtx, punzip)  );
    VERIFY_ERR_CHECK(  SG_unzip__currentfile__read(pCtx, punzip, buf, sizeof(buf), &iBytesRead)  );
    VERIFY_ERR_CHECK(  SG_unzip__currentfile__close(pCtx, punzip)  );
    VERIFY_COND("len", (iLen == iBytesRead));
    VERIFY_COND("match", (0 == strcmp((char*) buf, FILE2_CONTENTS)));

    /* verify that locate_file does not disturb things when it fails */
    VERIFY_ERR_CHECK(  SG_unzip__locate_file(pCtx, punzip, "not there", &b, &iLen)  );
    VERIFY_COND("b", !b);
    memset(buf, 0, 20);
    VERIFY_ERR_CHECK(  SG_unzip__currentfile__open(pCtx, punzip)  );
    VERIFY_ERR_CHECK(  SG_unzip__currentfile__read(pCtx, punzip, buf, sizeof(buf), &iBytesRead)  );
    VERIFY_ERR_CHECK(  SG_unzip__currentfile__close(pCtx, punzip)  );
    VERIFY_COND("len", ((1 + strlen(FILE2_CONTENTS)) == iBytesRead));
    VERIFY_COND("match", (0 == strcmp((char*) buf, FILE2_CONTENTS)));

    VERIFY_ERR_CHECK(  SG_unzip__nullclose(pCtx, &punzip)  );

    SG_PATHNAME_NULLFREE(pCtx, pPath);

    return;
fail:
    SG_ERR_IGNORE( SG_unzip__nullclose(pCtx, &punzip)  );
    SG_PATHNAME_NULLFREE(pCtx, pPath);
}

TEST_MAIN(u0052_zip)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0052_zip__test_1(pCtx)  );

	TEMPLATE_MAIN_END;
}
