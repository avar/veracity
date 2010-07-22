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

/**
 *
 * @file u0045_console.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"


//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0045_console)
#define MyDcl(name)				u0045_console__##name
#define MyFn(name)				u0045_console__##name
#define MyStruct_(name)			u0045_console__##name

#define MyStruct				MyStruct_(s)

//////////////////////////////////////////////////////////////////

typedef struct MyStruct
{
	SG_uint32					lineNr;
	const char *				szEntryName;
} MyStruct;

#define ROW(sz)					{ __LINE__, sz }

//////////////////////////////////////////////////////////////////

void MyFn(run_a_table)(SG_context * pCtx, const char * szLocaleCharSet, const MyStruct * aTestData, SG_uint32 nrRows)
{
	SG_uint32 kRow;
	char * pBufEscaped = NULL;

#if defined(LINUX) && defined(DEBUG)
	SG_utf8_debug__set_locale_env_charset(pCtx, szLocaleCharSet);
	VERIFYP_CTX_IS_OK("row", pCtx, ("Could not set locale to [%s]",szLocaleCharSet));
	SG_context__err_reset(pCtx);
#else
	szLocaleCharSet = NULL;
#endif

	for (kRow=0; kRow<nrRows; kRow++)
	{
		const MyStruct * pRow = &aTestData[kRow];

		SG_utf8__sprintf_utf8_with_escape_sequences(pCtx,pRow->szEntryName,&pBufEscaped);
		VERIFYP_CTX_IS_OK("row", pCtx, ("Could not escape row %d",pRow->lineNr));
		SG_context__err_reset(pCtx);

		SG_console(pCtx, SG_CS_STDOUT, "[%3d] [escaped %s] [%s]\n",
						 pRow->lineNr, pBufEscaped, pRow->szEntryName);
#if defined(LINUX)
		VERIFYP_CTX_IS_OK("row", pCtx, ("In locale [%s] could not send [row %d][escaped %s] to console.",
										  szLocaleCharSet,pRow->lineNr,pBufEscaped));
#else
		VERIFYP_CTX_IS_OK("row", pCtx, ("Could not send [row %d][escaped %s] to console.",
										  pRow->lineNr,pBufEscaped));
#endif
		SG_NULLFREE(pCtx, pBufEscaped);
	}
}

void MyFn(run)(SG_context * pCtx)
{
	static MyStruct aTestData[] =
		{
			ROW("aaaa"),
			ROW("bbbb"),
			ROW("CCCC"),

			ROW("inv001_>"),
			ROW("inv002_|"),

			ROW("res002_\x7f"),
			ROW("res002_%"),

			ROW("fds001_ "),
			ROW("fds002_."),
			ROW("fds003_    "),
			ROW("fds004_...."),

			ROW("com1"),
			ROW("aux.txt"),

			ROW("SN0001~1.txt"),
			ROW("SNzzzz~1.txt"),
			ROW("S0001~1.txt"),

			ROW("col001_per_%25.txt"),
			ROW("col001_per_%.txt"),

			ROW("col002_per_%%.txt"),
			ROW("col002_per_%%2525.txt"),

			ROW("col003_fds_"),
			ROW("col003_fds_ "),
			ROW("col003_fds_  "),
			ROW("col003_fds_ . "),

			ROW("col004_caseAAA"),
			ROW("col004_caseAaa"),

			ROW("col005_casebbb"),
			ROW("col005_caseBBB"),

			ROW("col006_caseCCC"),
			ROW("col006_caseccc"),

			ROW("col007_7bit_A%25."),
			ROW("col007_7bit_a%"),

			ROW("nonbmp001_\xf0\x90\x80\x80.txt"),
			ROW("nonbmp002_\xf0\x90\xa0\x80.txt"),

			ROW("ign001_\xe2\x80\x8c.txt"),
			ROW("ign002_\xe2\x81\xaa.txt"),

			ROW("sfm001_\xef\x80\xa3.txt"),
			ROW("sfm002_\xef\x80\xa4.txt"),
			ROW("sfm003_\xef\x80\xa8"),
			ROW("sfm004_\xef\x80\xa9"),

			ROW("nfd002_\xce\xb1\xcd\x85\xcd\x82.txt"),

			ROW("qqq001_\xef\xbf\xbd.txt"),
			ROW("qqq001_\xf0\x90\x80\x80.txt"),
			ROW("qqq001_\xf0\x90\xa0\x80.txt"),

			ROW("qqq002_\xce\xb2_per_%25.txt"),
			ROW("qqq002_\xce\xb2_per_%.txt"),

			ROW("qqq003_\xce\xb2_per_%%.txt"),
			ROW("qqq003_\xce\xb2_per_%%2525.txt"),

			ROW("qqq004_\xef\x80\xa3.txt"),
			ROW("qqq004_<.txt"),

			ROW("qqq005_\xef\x80\xa8"),
			ROW("qqq005_\xef\x80\xa9"),
			ROW("qqq005_....\xef\x80\xa9"),
			ROW("qqq005_ "),
			ROW("qqq005_."),
			ROW("qqq005_....."),
			ROW("qqq005_"),

			ROW("qqq006_\xe2\x80\x8c.txt"),
			ROW("qqq006_\xe2\x81\xaa.txt"),
			ROW("qqq006_.txt"),

			ROW("qqq008_\xce\xb1\xcd\x82\xcd\x85.txt"),
			ROW("qqq008_\xce\xb1\xcd\x85\xcd\x82.txt"),
			ROW("qqq008_\xe1\xbe\xb7.txt"),

			ROW("qqq009_\xe1\xbe\xb3\xcd\x82.txt"),
			ROW("qqq009_\xe1\xbe\xb6\xcd\x85.txt"),
			ROW("qqq009_\xe1\xbe\xb7.txt"),

			ROW("zzz001_\xc2\xa9_AAA"),
			ROW("zzz001_\xc2\xa9_Aaa"),

			ROW("zzz002_\xc2\xa9_AAA"),
			ROW("zzz002_\xc2\xa9_aaa"),

			ROW("zzz003_\xc2\xa9_aaa"),
			ROW("zzz003_\xc2\xa9_AAA"),

			ROW("zzz004_\xc2\xa9_aaa"),
			ROW("zzz004_\xc2\xa9_Aaa"),
			ROW("zzz004_\xc2\xa9_AAA"),

			ROW("zzz005_\xc2\xa9_\xc3\xa0\xc3\xa0.txt"),
			ROW("zzz005_\xc2\xa9_\xc3\x80\xc3\xa0.txt"),
			ROW("zzz005_\xc2\xa9_\xc3\x80\xc3\x80.txt"),

			ROW("zzz006_\xc2\xa9_\xc5\x91\xc5\x91.txt"),
			ROW("zzz006_\xc2\xa9_\xc5\x90\xc5\x90.txt"),
			ROW("zzz006_\xc2\xa9_\xc5\x90\xc5\x91.txt"),

			ROW("zzz007_\xc2\xa9_\xce\xb1\xce\xb1.txt"),
			ROW("zzz007_\xc2\xa9_\xce\x91\xce\x91.txt"),
			ROW("zzz007_\xc2\xa9_\xce\x91\xce\xb1.txt"),

			ROW("zzz008_\xc2\xa9_\xc5\x91\xc5\x91.txt"),
			ROW("zzz008_\xc2\xa9_\xc5\x90\xc5\x90.txt"),
			ROW("zzz008_\xc2\xa9_\xc5\x90\xc5\x91.txt"),
			ROW("zzz008_\xc2\xa9_o\xcc\x8bo\xcc\x8b.txt"),

			ROW("zzz009_\xc2\xa9_o\xcc\x8bo\xcc\x8b.txt"),
			ROW("zzz009_\xc2\xa9_\xc5\x91\xc5\x91.txt"),
			ROW("zzz009_\xc2\xa9_\xc5\x90\xc5\x90.txt"),
			ROW("zzz009_\xc2\xa9_\xc5\x90\xc5\x91.txt"),

			ROW("zzz010_\xc2\xa9_o\xcc\x8bo\xcc\x8b.txt"),
			ROW("zzz010_\xc2\xa9_\xc5\x91\xc5\x91.txt"),
			ROW("zzz010_\xc2\xa9_\xc5\x90\xc5\x90.txt"),
			ROW("zzz010_\xc2\xa9_\xc5\x90\xc5\x91.txt"),
			ROW("zzz010_\xc2\xa9_O\xcc\x8bO\xcc\x8b.txt"),
		};

	SG_uint32 nrRows = SG_NrElements(aTestData);

	VERIFY_ERR_CHECK_DISCARD(  MyFn(run_a_table)(pCtx,"utf-8",aTestData,nrRows)  );
	VERIFY_ERR_CHECK_DISCARD(  MyFn(run_a_table)(pCtx,"iso-8859-1",aTestData,nrRows)  );
	VERIFY_ERR_CHECK_DISCARD(  MyFn(run_a_table)(pCtx,"iso-8859-7",aTestData,nrRows)  );
	VERIFY_ERR_CHECK_DISCARD(  MyFn(run_a_table)(pCtx,"cp1252",aTestData,nrRows)  );
}

//////////////////////////////////////////////////////////////////

/**
 * The main test.
 */
MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(run)(pCtx)  );

	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn
#undef MyStruct_
#undef MyStruct
#undef ROW
