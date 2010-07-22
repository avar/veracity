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

SG_bool compare_files_are_identical(const SG_pathname* pPath1, const SG_pathname* pPath2)
{
	SG_bool bResult;
	FILE* fp1;
	FILE* fp2;

	fp1 = fopen(SG_pathname__sz(pPath1), "r");
	if (!fp1)
    {
		return SG_FALSE;
    }
	fp2 = fopen(SG_pathname__sz(pPath2), "r");
	if (!fp2)
    {
		fclose(fp1);
		return SG_FALSE;
    }

	bResult = SG_TRUE;
	while (SG_TRUE)
    {
		int c1 = fgetc(fp1);
		int c2 = fgetc(fp2);
		if (c1 != c2)
		{
			bResult = SG_FALSE;
			break;
		}
		if (c1 == EOF)
		{
			break;
		}
    }
	fclose(fp1);
	fclose(fp2);
	return bResult;
}


void u0023_vcdiff__do_test_deltify(SG_context * pCtx,
								   SG_pathname* pPath_version1, SG_pathname* pPath_version2)
{
	SG_bool b;
	SG_pathname* pPathDelta = NULL;
	SG_pathname* pPathReconstructed = NULL;

	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx,&pPathDelta)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx,&pPathReconstructed)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vcdiff__deltify__files(pCtx,pPath_version1, pPath_version2, pPathDelta)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vcdiff__undeltify__files(pCtx,pPath_version1, pPathReconstructed, pPathDelta)  );

	b = compare_files_are_identical(pPath_version2, pPathReconstructed);
	VERIFY_COND("match after deltify/undeltify", (b));

	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx,pPathDelta)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx,pPathReconstructed)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathDelta);
	SG_PATHNAME_NULLFREE(pCtx, pPathReconstructed);
}

void u0023_vcdiff__test_deltify(SG_context * pCtx)
{
	FILE* fp;
	SG_pathname* pPath_version1 = NULL;
	SG_pathname* pPath_version2 = NULL;
	int i;

	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx,&pPath_version1)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx,&pPath_version2)  );

	fp = fopen(SG_pathname__sz(pPath_version1), "w");
	for (i=0; i<500; i++)
	{
		fprintf(fp, "Ah, I should have known it from the very start This girl will leave me with a broken heart Now listen people what I'm telling you A-keep away from-a Runaround Sue\n");
	}
	fclose(fp);

	fp = fopen(SG_pathname__sz(pPath_version2), "w");
	for (i=0; i<100; i++)
	{
		fprintf(fp, "He rocks in the tree-top all a day long Hoppin' and a-boppin' and a-singin' the song All the little birds on J-Bird St. Love to hear the robin goin' tweet tweet tweet\n");
	}
	fclose(fp);

	VERIFY_ERR_CHECK_DISCARD(  u0023_vcdiff__do_test_deltify(pCtx, pPath_version1, pPath_version2)  );

	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version1)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version2)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath_version1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_version2);
}

void u0023_vcdiff__test_deltify_small_files(SG_context * pCtx)
{
	FILE* fp;
	SG_pathname* pPath_version1;
	SG_pathname* pPath_version2;

	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version1)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version2)  );

	fp = fopen(SG_pathname__sz(pPath_version1), "w");
	fprintf(fp, "e");
	fclose(fp);

	fp = fopen(SG_pathname__sz(pPath_version2), "w");
	fprintf(fp, "a");
	fclose(fp);

	VERIFY_ERR_CHECK_DISCARD(  u0023_vcdiff__do_test_deltify(pCtx, pPath_version1, pPath_version2)  );

	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version1)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version2)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath_version1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_version2);
}

void u0023_vcdiff__test_deltify_binary(SG_context * pCtx)
{
	FILE* fp;
	SG_pathname* pPath_version1 = NULL;
	SG_pathname* pPath_version2 = NULL;
	int i;

	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version1)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version2)  );

	fp = fopen(SG_pathname__sz(pPath_version1), "w");
	for (i=0; i<50000; i++)
	{
		fputc(i % 256, fp);
	}
	fclose(fp);

	fp = fopen(SG_pathname__sz(pPath_version2), "w");
	for (i=0; i<50000; i++)
	{
		fputc((50000 - i) % 256, fp);
	}
	fclose(fp);

	VERIFY_ERR_CHECK_DISCARD(  u0023_vcdiff__do_test_deltify(pCtx, pPath_version1, pPath_version2)  );

	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version1)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version2)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath_version1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_version2);
}

void u0023_vcdiff__test_deltify_identical(SG_context * pCtx)
{
	FILE* fp;
	SG_pathname* pPath_version1 = NULL;
	SG_pathname* pPath_version2 = NULL;
	int i;

	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version1)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version2)  );

	fp = fopen(SG_pathname__sz(pPath_version1), "w");
	for (i=0; i<50000; i++)
	{
		fputc(i % 256, fp);
	}
	fclose(fp);

	fp = fopen(SG_pathname__sz(pPath_version2), "w");
	for (i=0; i<50000; i++)
	{
		fputc(i % 256, fp);
	}
	fclose(fp);

	VERIFY_ERR_CHECK_DISCARD(  u0023_vcdiff__do_test_deltify(pCtx, pPath_version1, pPath_version2)  );

	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version1)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version2)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath_version1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_version2);
}

void u0023_vcdiff__test_deltify_shrinkalot(SG_context * pCtx)
{
	FILE* fp;
	SG_pathname* pPath_version1 = NULL;
	SG_pathname* pPath_version2 = NULL;
	int i;

	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version1)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version2)  );

	fp = fopen(SG_pathname__sz(pPath_version1), "w");
	for (i=0; i<500000; i++)
	{
		fputc(i % 256, fp);
	}
	fclose(fp);

	fp = fopen(SG_pathname__sz(pPath_version2), "w");
	fprintf(fp, "e");
	fclose(fp);

	VERIFY_ERR_CHECK_DISCARD(  u0023_vcdiff__do_test_deltify(pCtx, pPath_version1, pPath_version2)  );

	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version1)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version2)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath_version1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_version2);
}

void u0023_vcdiff__test_deltify_to_zerolength(SG_context * pCtx)
{
	FILE* fp;
	SG_pathname* pPath_version1 = NULL;
	SG_pathname* pPath_version2 = NULL;
	int i;

	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version1)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version2)  );

	fp = fopen(SG_pathname__sz(pPath_version1), "w");
	for (i=0; i<500000; i++)
	{
		fputc(i % 256, fp);
	}
	fclose(fp);

	fp = fopen(SG_pathname__sz(pPath_version2), "w");
	fclose(fp);

	VERIFY_ERR_CHECK_DISCARD(  u0023_vcdiff__do_test_deltify(pCtx, pPath_version1, pPath_version2)  );

	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version1)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version2)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath_version1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_version2);
}

void u0023_vcdiff__test_deltify_growalot(SG_context * pCtx)
{
	FILE* fp;
	SG_pathname* pPath_version1 = NULL;
	SG_pathname* pPath_version2 = NULL;
	int i;

	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version1)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version2)  );

	fp = fopen(SG_pathname__sz(pPath_version1), "w");
	fprintf(fp, "e");
	fclose(fp);

	fp = fopen(SG_pathname__sz(pPath_version2), "w");
	for (i=0; i<500000; i++)
	{
		fputc(i % 256, fp);
	}
	fclose(fp);

	VERIFY_ERR_CHECK_DISCARD(  u0023_vcdiff__do_test_deltify(pCtx, pPath_version1, pPath_version2)  );

	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version1)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version2)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath_version1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_version2);
}

#if 0
void u0023_vcdiff__test_deltify_from_zerolength(SG_context * pCtx)
{
	FILE* fp;
	SG_pathname* pPath_version1 = NULL;
	SG_pathname* pPath_version2 = NULL;
	int i;

	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version1)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version2)  );

	fp = fopen(SG_pathname__sz(pPath_version1), "w");
	fclose(fp);

	fp = fopen(SG_pathname__sz(pPath_version2), "w");
	for (i=0; i<500000; i++)
	{
		fputc(i % 256, fp);
	}
	fclose(fp);

	VERIFY_ERR_CHECK_DISCARD(  u0023_vcdiff__do_test_deltify(pCtx, pPath_version1, pPath_version2)  );

	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version1)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version2)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath_version1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_version2);
}
#endif

void u0023_vcdiff__test_deltify_add(SG_context * pCtx)
{
	FILE* fp;
	SG_pathname* pPath_version1 = NULL;
	SG_pathname* pPath_version2 = NULL;
	int i;

	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version1)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version2)  );

	fp = fopen(SG_pathname__sz(pPath_version1), "w");
	for (i=0; i<500000; i++)
	{
		fputc('e', fp);
	}
	fclose(fp);

	fp = fopen(SG_pathname__sz(pPath_version2), "w");
	fprintf(fp, "Ah, I should havq known it from thq vqry start This girl will lqavq mq with a brokqn hqart Now listqn pqoplq what I'm tqlling you A-kqqp away from-a Runaround Suq\n");
	fclose(fp);

	VERIFY_ERR_CHECK_DISCARD(  u0023_vcdiff__do_test_deltify(pCtx, pPath_version1, pPath_version2)  );

	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version1)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version2)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath_version1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_version2);
}

void u0023_vcdiff__test_deltify_run(SG_context * pCtx)
{
	FILE* fp;
	SG_pathname* pPath_version1 = NULL;
	SG_pathname* pPath_version2 = NULL;
	int i;

	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version1)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPath_version2)  );

	fp = fopen(SG_pathname__sz(pPath_version1), "w");
	fprintf(fp, "Ah, I should havq known it from thq vqry start This girl will lqavq mq with a brokqn hqart Now listqn pqoplq what I'm tqlling you A-kqqp away from-a Runaround Suq\n");
	fclose(fp);

	fp = fopen(SG_pathname__sz(pPath_version2), "w");
	for (i=0; i<500000; i++)
	{
		fputc('e', fp);
	}
	fclose(fp);

	VERIFY_ERR_CHECK_DISCARD(  u0023_vcdiff__do_test_deltify(pCtx, pPath_version1, pPath_version2)  );

	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version1)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_version2)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath_version1);
	SG_PATHNAME_NULLFREE(pCtx, pPath_version2);
}

TEST_MAIN(u0023_vcdiff)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0023_vcdiff__test_deltify(pCtx)  );
	BEGIN_TEST(  u0023_vcdiff__test_deltify_small_files(pCtx)  );
	BEGIN_TEST(  u0023_vcdiff__test_deltify_binary(pCtx)  );
	BEGIN_TEST(  u0023_vcdiff__test_deltify_identical(pCtx)  );
	BEGIN_TEST(  u0023_vcdiff__test_deltify_shrinkalot(pCtx)  );
	BEGIN_TEST(  u0023_vcdiff__test_deltify_growalot(pCtx)  );
	BEGIN_TEST(  u0023_vcdiff__test_deltify_add(pCtx)  );
	BEGIN_TEST(  u0023_vcdiff__test_deltify_run(pCtx)  );
	BEGIN_TEST(  u0023_vcdiff__test_deltify_to_zerolength(pCtx)  );
    /* we don't support deltifying from a zero length file */
	//BEGIN_TEST(  u0023_vcdiff__test_deltify_from_zerolength(pCtx)  );

	TEMPLATE_MAIN_END;
}

