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
 * @file u0042_portability.c
 *
 * TODO fix me to run on Linux when my locale is set to something besides UTF-8.
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"


//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0042_portability)
#define MyDcl(name)				u0042_portability__##name
#define MyFn(name)				u0042_portability__##name
#define MyStruct_(name)			u0042_portability__##name

#define MyStruct				MyStruct_(s)

//////////////////////////////////////////////////////////////////

typedef struct MyStruct
{
	SG_uint32					lineNr;
	const char *				szEntryName;
	SG_bool						bCheckMyName;
	SG_bool						bTrivialDuplicate;
	SG_portability_flags		portWarningsExpected;
	SG_bool						bFirstInSet;
} MyStruct;

#define ROW(sz,bCheck,bDup,flags,bFirstInSet)	{ __LINE__, sz, bCheck, bDup, flags, bFirstInSet }
#define ROW_NEW_OK(sz)							ROW(sz,SG_TRUE,SG_FALSE,SG_PORT_FLAGS__NONE,SG_TRUE)
#define ROW_NEW(sz,flags)						ROW(sz,SG_TRUE,SG_FALSE,flags,SG_TRUE)
#define ROW_NEW1(sz,flags)						ROW(sz,SG_TRUE,SG_FALSE,flags,SG_TRUE)
#define ROW_NEW2(sz,flags)						ROW(sz,SG_TRUE,SG_FALSE,flags,SG_FALSE)
#define ROW_DUP(sz)								ROW(sz,SG_TRUE,SG_TRUE,SG_PORT_FLAGS__NONE,SG_TRUE)

//////////////////////////////////////////////////////////////////

void MyFn(run_a_table)(SG_context * pCtx,
					   const char * szLocaleCharSet, SG_portability_flags portMask,
					   const MyStruct * aTestData, SG_uint32 nrRows)
{
	SG_varray * pvaLog = NULL;
	SG_portability_dir * pDir = NULL;
	SG_portability_flags portWarnings, portWarningsAll, portWarningsChecked, portWarningsLogged;
	SG_uint32 kRow;
	SG_pathname * pPathnameTempDir = NULL;
	SG_pathname * pPathnameFile = NULL;
	SG_file * pFile = NULL;
	SG_utf8_converter * pConverter = NULL;
	char buf1[100], buf2[100], buf3[100];
	SG_bool bHaveLogContent, bShouldHaveLogContent;
    SG_uint32 count_warnings = 0;
	SG_string * pStringLog = NULL;

	VERIFY_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pvaLog)  );
	VERIFY_ERR_CHECK(  SG_utf8_converter__alloc(pCtx, szLocaleCharSet,&pConverter)  );
	VERIFY_ERR_CHECK(  SG_portability_dir__alloc(pCtx, portMask,
												 "/abc/def.dir",
												 pConverter,
												 pvaLog,
												 &pDir)  );

	// add all items to container.  this computes __INDIV__ flags immediately,
	// but __COLLISION__ flags are not ready until after we have added everything.

	for (kRow=0; kRow<nrRows; kRow++)
	{
		const MyStruct * pRow = &aTestData[kRow];
		SG_bool bIsDuplicate = SG_FALSE;

		VERIFY_ERR_CHECK_DISCARD(  SG_portability_dir__add_item(pCtx, pDir, NULL, pRow->szEntryName,pRow->bCheckMyName,pRow->bCheckMyName,&bIsDuplicate)  );
		VERIFYP_COND("duplicate_test",(bIsDuplicate == pRow->bTrivialDuplicate),
					 ("row[%d,%d] Item[%s] [bIsDuplicate %d] [expected %d]",
					  kRow,pRow->lineNr,pRow->szEntryName,bIsDuplicate,pRow->bTrivialDuplicate));
	}

	// scan each item that we added and make sure that the flags computed
	// match what we expected.  both __INDIV__ and __COLLISION__ data should
	// be valid.

	for (kRow=0; kRow<nrRows; kRow++)
	{
		const MyStruct * pRow = &aTestData[kRow];

		if (pRow->bTrivialDuplicate)
			continue;

		SG_portability_dir__get_item_result_flags(pCtx,pDir,pRow->szEntryName,&portWarnings,&portWarningsLogged);
		VERIFYP_CTX_IS_OK("row", pCtx, ("Item[%s]",pRow->szEntryName));

		if (SG_context__has_err(pCtx))
		{
			SG_context__err_reset(pCtx);
			continue;
		}

		if (portWarnings != pRow->portWarningsExpected)
		{
			SG_hex__format_uint64(buf1,portWarnings);
			SG_hex__format_uint64(buf2,pRow->portWarningsExpected);

			VERIFYP_COND("run",(portWarnings==pRow->portWarningsExpected),
					("Row[%d,%d] Item[%s] flags [computed %s][expected %s]",
					 kRow,pRow->lineNr,pRow->szEntryName,
					 buf1,buf2));
		}
	}

	// get the overall flags from the collider.  these are the union over all
	// the individual entries.

	VERIFY_ERR_CHECK_DISCARD(  SG_portability_dir__get_result_flags(pCtx, pDir,&portWarningsAll,&portWarningsChecked,&portWarningsLogged)  );
	SG_hex__format_uint64(buf1,portWarningsAll);
	SG_hex__format_uint64(buf2,portWarningsChecked);
	SG_hex__format_uint64(buf3,portWarningsLogged);
	INFOP("get_results",("Overall Flags [all %s][checked %s][logged %s]",buf1,buf2,buf3));

	// the portability collider always computes all potential problem flag bits.
	// it should only generate log messages for issues in the MASK for NEW items.

	SG_hex__format_uint64(buf1,portMask);
    SG_ERR_IGNORE(  SG_varray__count(pCtx, pvaLog, &count_warnings)  );
	bHaveLogContent = (count_warnings > 0);
	bShouldHaveLogContent = (portWarningsLogged != SG_PORT_FLAGS__NONE);
	VERIFYP_COND("log_results",(bHaveLogContent==bShouldHaveLogContent),
			("[locale %s][mask %s][bHaveLogContent %d][bShouldHaveLogContent %d][numWarnings %d]",
			 ((szLocaleCharSet)?szLocaleCharSet:"utf8"),buf1,bHaveLogContent,bShouldHaveLogContent,
			 count_warnings));

	// create a temporary directory on disk and try to create a simple file
	// using each entryname.  some items will/may generate a filesystem error
	// (items with invalid names for this platform or that cause a collision
	// on this platform with a previous name in the directory).  for each
	// filesystem error, we should have predicted one.  this is not an exact
	// 1-to-1 match because this platform may allow things that other platforms
	// do not ---- so you need to run this test on a variety of platforms and
	// with local and remote filesystems and make sure that each error that we
	// predict actually happens on one combination of OS and filesystem.  i
	// can't automate that here.

	VERIFY_ERR_CHECK(  unittest__alloc_unique_pathname_in_cwd(pCtx,&pPathnameTempDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathnameTempDir)  );
	for (kRow=0; kRow<nrRows; kRow++)
	{
		const MyStruct * pRow = &aTestData[kRow];
		char bufError[SG_ERROR_BUFFER_SIZE+1];

		if (pRow->bTrivialDuplicate)
			continue;

		VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathnameFile,
														   pPathnameTempDir,pRow->szEntryName)  );

		SG_file__open__pathname(pCtx, pPathnameFile,
									  SG_FILE_WRONLY|SG_FILE_CREATE_NEW,0777,&pFile);
		SG_ERR_IGNORE(  SG_PATHNAME_NULLFREE(pCtx, pPathnameFile)  );

		if (!SG_context__has_err(pCtx))
		{
			SG_FILE_NULLCLOSE(pCtx, pFile);
			if ((pRow->portWarningsExpected == SG_PORT_FLAGS__NONE)
				|| ((pRow->portWarningsExpected & SG_PORT_FLAGS__MASK_COLLISION)
					&& pRow->bFirstInSet))
			{
			}
			else
			{
				INFOP("open(false positive for this platform)",
					  ("Row[%d,%d], Item[%s]",
					   kRow,pRow->lineNr,pRow->szEntryName));
			}
		}
		else
		{
			SG_error err = SG_ERR_UNSPECIFIED;
			SG_context__get_err(pCtx, &err);
			SG_context__err_reset(pCtx);
			switch (err)
			{
#if defined(WINDOWS)
			case SG_ERR_GETLASTERROR(ERROR_ALREADY_EXISTS):
			case SG_ERR_GETLASTERROR(ERROR_FILE_EXISTS):
				VERIFYP_COND("open",(pRow->portWarningsExpected & SG_PORT_FLAGS__MASK_COLLISION),
						("Collision when not expected: Row[%d,%d], Item[%s]",
						 kRow,pRow->lineNr,pRow->szEntryName));
				break;

			case SG_ERR_GETLASTERROR(ERROR_INVALID_NAME):
			case SG_ERR_GETLASTERROR(ERROR_BAD_NET_NAME):		// we sometimes (but not always) get this for "aux" and "com1" ???
			case SG_ERR_GETLASTERROR(ERROR_BAD_NETPATH):		// we sometimes (but not always) get this for "aux" and "com1" ???
				VERIFYP_COND("open",(pRow->portWarningsExpected & SG_PORT_FLAGS__MASK_INDIV),
						("Invalid entryname when not expected: Row[%d,%d], Item[%s]",
						 kRow,pRow->lineNr,pRow->szEntryName));
				break;
#endif

#if defined(LINUX)
			case SG_ERR_ERRNO(EEXIST):
				VERIFYP_COND("open",(pRow->portWarningsExpected & SG_PORT_FLAGS__MASK_COLLISION),
						("Collision when not expected: Row[%d,%d], Item[%s]",
						 kRow,pRow->lineNr,pRow->szEntryName));
				break;

			case SG_ERR_ERRNO(ENAMETOOLONG):
				VERIFYP_COND("open",(pRow->portWarningsExpected & SG_PORT_FLAGS__INDIV__ENTRYNAME_LENGTH),
						("Name too long when not expected: Row[%d,%d], Item[%s]",
						 kRow,pRow->lineNr,pRow->szEntryName));
				break;

				// TODO fill in other errors that we might get....

#endif

#if defined(MAC)
			case SG_ERR_ERRNO(EEXIST):
				VERIFYP_COND("open",(pRow->portWarningsExpected & SG_PORT_FLAGS__MASK_COLLISION),
						("Collision when not expected: Row[%d,%d], Item[%s]",
						 kRow,pRow->lineNr,pRow->szEntryName));
				break;

			case SG_ERR_ERRNO(ENAMETOOLONG):
				VERIFYP_COND("open",(pRow->portWarningsExpected & SG_PORT_FLAGS__INDIV__ENTRYNAME_LENGTH),
						("Name too long when not expected: Row[%d,%d], Item[%s]",
						 kRow,pRow->lineNr,pRow->szEntryName));
				break;

				// TODO fill in other errors that we might get....

#endif

			default:
				SG_error__get_message(err,bufError,SG_ERROR_BUFFER_SIZE);
				VERIFYP_COND("open",(0),
						("Unexpected error: Row[%d,%d], Item[%s] [err %d:%d %s]",
						 kRow,pRow->lineNr,pRow->szEntryName,
						 (SG_uint32)(SG_ERR_TYPE(err) >> 32), (SG_uint32)SG_ERR_GENERIC_VALUE(err),
						 bufError));
				break;
			}
		}
	}

	SG_context__err_reset(pCtx);

	if (pvaLog)
	{
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStringLog)  );
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pStringLog, "\n")  );
		SG_ERR_CHECK(  SG_varray__to_json(pCtx, pvaLog, pStringLog)  );
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pStringLog, "\n\n")  );
		INFO2("pvaLog", SG_string__sz(pStringLog));
	}

	// fall-thru to common cleanup

fail:
	SG_STRING_NULLFREE(pCtx, pStringLog);
	SG_VARRAY_NULLFREE(pCtx, pvaLog);
	SG_portability_dir__free(pCtx, pDir);
	SG_UTF8_CONVERTER_NULLFREE(pCtx, pConverter);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameTempDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameFile);
	SG_FILE_NULLCLOSE(pCtx, pFile);
}

void MyFn(run_utf8)(SG_context * pCtx, SG_portability_flags portMask)
{
	// add a series of entrynames to a portability_dir/collider and
	// verify that we get the expected problems and/or collisions.
	// in this test, we assume that the Linux users will be using
	// UTF-8 for their LOCALE setting.  (this lets us bypass the
	// LOCALE conversions and so we will NOT see either __INDIV__LOCALE
	// nor __COLLISION__LOCALE in our results.)

	static MyStruct aTestData[] =
		{
			// problematic characters with 7-bit clean entrynames

			ROW_NEW_OK("aaaa"),
			ROW_NEW_OK("bbbb"),
			ROW_NEW_OK("CCCC"),

			ROW_NEW("-abc",				SG_PORT_FLAGS__INDIV__LEADING_DASH),

			ROW_NEW("inv001_>",			SG_PORT_FLAGS__INDIV__INVALID_00_7F),
			ROW_NEW("inv002_|",			SG_PORT_FLAGS__INDIV__INVALID_00_7F),

			ROW_NEW("res002_\x7f",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F),
			ROW_NEW("res002_%",			SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F),

			ROW_NEW("fds001_ ",			SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE),
			ROW_NEW("fds002_.",			SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE),
			ROW_NEW("fds003_    ",		SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE),
			ROW_NEW("fds004_....",		SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE),

			ROW_NEW("com1",				SG_PORT_FLAGS__INDIV__MSDOS_DEVICE),
			ROW_NEW("aux.txt",			SG_PORT_FLAGS__INDIV__MSDOS_DEVICE),

			ROW_NEW("SN0001~1.txt",		SG_PORT_FLAGS__INDIV__SHORTNAMES),
			ROW_NEW("SNzzzz~1.txt",		SG_PORT_FLAGS__INDIV__SHORTNAMES),
			ROW_NEW("S0001~1.txt",		SG_PORT_FLAGS__INDIV__SHORTNAMES),

			// some simple collisions with 7-bit clean entrynames.

			ROW_NEW1("col001_per_%25.txt",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__PERCENT25),
			ROW_NEW2("col001_per_%.txt",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("col002_per_%%.txt",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__ORIGINAL),
			ROW_NEW2("col002_per_%%2525.txt",	SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__PERCENT25),

			ROW_NEW1("col003_fds_",				SG_PORT_FLAGS__COLLISION__ORIGINAL),
			ROW_NEW2("col003_fds_ ",			SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),
			ROW_NEW2("col003_fds_  ",			SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),
			ROW_NEW2("col003_fds_ . ",			SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),

			ROW_NEW1("col004_caseAAA",			SG_PORT_FLAGS__COLLISION__CASE),
			ROW_NEW2("col004_caseAaa",			SG_PORT_FLAGS__COLLISION__CASE),

			ROW_NEW1("col005_casebbb",			SG_PORT_FLAGS__COLLISION__ORIGINAL),
			ROW_NEW2("col005_caseBBB",			SG_PORT_FLAGS__COLLISION__CASE),

			ROW_NEW1("col006_caseCCC",			SG_PORT_FLAGS__COLLISION__CASE),
			ROW_NEW2("col006_caseccc",			SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("col007_7bit_A%25.",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__PERCENT25|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__CASE),
			ROW_NEW2("col007_7bit_a%",			SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__ORIGINAL),

			// entryname and pathname length problems

			ROW_NEW(("len001_"
					 "01234567890123456789012345678901234567890123456789"
					 "01234567890123456789012345678901234567890123456789"
					 "01234567890123456789012345678901234567890123456789"
					 "01234567890123456789012345678901234567890123456789"
					 "01234567890123456789012345678901234567890123456789"), SG_PORT_FLAGS__INDIV__ENTRYNAME_LENGTH|SG_PORT_FLAGS__INDIV__PATHNAME_LENGTH),

			// some problematic non-7-bit characters in UTF-8

			ROW_NEW("nonbmp001_\xf0\x90\x80\x80.txt",	SG_PORT_FLAGS__INDIV__NONBMP),		// u+10000 Linear B Syllable B008 A
			ROW_NEW("nonbmp002_\xf0\x90\xa0\x80.txt",	SG_PORT_FLAGS__INDIV__NONBMP),		// u+10800 Cypriot Syllable A

			ROW_NEW("ign001_\xe2\x80\x8c.txt",			SG_PORT_FLAGS__INDIV__IGNORABLE),	// u+200c Zero width non-joiner
			ROW_NEW("ign002_\xe2\x81\xaa.txt",			SG_PORT_FLAGS__INDIV__IGNORABLE),	// u+206a Inhibit symmetric swapping

			ROW_NEW("sfm001_\xef\x80\xa3.txt",			SG_PORT_FLAGS__INDIV__SFM_NTFS),			// u+f023 Private Use (mac sfm '<')
			ROW_NEW("sfm002_\xef\x80\xa4.txt",			SG_PORT_FLAGS__INDIV__SFM_NTFS),			// u+f023 Private Use (mac sfm '>')
			ROW_NEW("sfm003_\xef\x80\xa8",				SG_PORT_FLAGS__INDIV__SFM_NTFS),			// u+f023 Private Use (mac sfm final space)
			ROW_NEW("sfm004_\xef\x80\xa9",				SG_PORT_FLAGS__INDIV__SFM_NTFS),			// u+f023 Private Use (mac sfm final dot)

			// 03B1;GREEK SMALL LETTER ALPHA;Ll;0;L;;;;;N;;;0391;;0391
			// 0345;COMBINING GREEK YPOGEGRAMMENI;Mn;240;NSM;;;;;N;GREEK NON-SPACING IOTA BELOW;;0399;;0399
			// 0342;COMBINING GREEK PERISPOMENI;Mn;230;NSM;;;;;N;;;;;
			//
			// 0342 is class 230; 0345 is class 240 ---> so 0342 comes before 0345
			//
			// 1FB7;1FB7;03B1 0342 0345;1FB7;03B1 0342 0345;
			//
			// 1FB3;GREEK SMALL LETTER ALPHA WITH YPOGEGRAMMENI;Ll;0;L;03B1 0345;;;;N;;;1FBC;;1FBC
			// 1FB6;GREEK SMALL LETTER ALPHA WITH PERISPOMENI;Ll;0;L;03B1 0342;;;;N;;;;;
			// 1FB7;GREEK SMALL LETTER ALPHA WITH PERISPOMENI AND YPOGEGRAMMENI;Ll;0;L;1FB6 0345;;;;N;;;;;

			ROW_NEW("nfd002_\xce\xb1\xcd\x85\xcd\x82.txt",	SG_PORT_FLAGS__INDIV__NON_CANONICAL_NFD),	// u03b1 u0345 u0342

			// potential collisions due to non-7-bit problematic characters in UTF-8

			ROW_NEW1("qqq001_\xef\xbf\xbd.txt",			SG_PORT_FLAGS__COLLISION__ORIGINAL),								// u+fffd replacement character
			ROW_NEW2("qqq001_\xf0\x90\x80\x80.txt",		SG_PORT_FLAGS__INDIV__NONBMP|SG_PORT_FLAGS__COLLISION__NONBMP),		// u+10000 non-bmp char folded to replacement character
			ROW_NEW2("qqq001_\xf0\x90\xa0\x80.txt",		SG_PORT_FLAGS__INDIV__NONBMP|SG_PORT_FLAGS__COLLISION__NONBMP),		// u+10800 non-bmp char folded to replacement character

			ROW_NEW1("qqq002_\xce\xb2_per_%25.txt",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__PERCENT25),	// "%25" <--> "%" with U+03b2 greek small letter beta
			ROW_NEW2("qqq002_\xce\xb2_per_%.txt",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("qqq003_\xce\xb2_per_%%.txt",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__ORIGINAL),		// "%25" <--> "%" with U+03b2 greek small letter beta
			ROW_NEW2("qqq003_\xce\xb2_per_%%2525.txt",	SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__PERCENT25),

			ROW_NEW1("qqq004_\xef\x80\xa3.txt",			SG_PORT_FLAGS__INDIV__SFM_NTFS|SG_PORT_FLAGS__COLLISION__SFM_NTFS),			// u+f023 Private Use (mac sfm '<')
			ROW_NEW2("qqq004_<.txt",					SG_PORT_FLAGS__INDIV__INVALID_00_7F|SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("qqq005_\xef\x80\xa8",				SG_PORT_FLAGS__INDIV__SFM_NTFS|SG_PORT_FLAGS__COLLISION__SFM_NTFS|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),			// u+f023 Private Use (mac sfm final space)
			ROW_NEW2("qqq005_\xef\x80\xa9",				SG_PORT_FLAGS__INDIV__SFM_NTFS|SG_PORT_FLAGS__COLLISION__SFM_NTFS|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),			// u+f023 Private Use (mac sfm final dot)
			ROW_NEW2("qqq005_....\xef\x80\xa9",			SG_PORT_FLAGS__INDIV__SFM_NTFS|SG_PORT_FLAGS__COLLISION__SFM_NTFS|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),			// u+f023 Private Use (mac sfm final dot)
			ROW_NEW2("qqq005_ ",						SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),
			ROW_NEW2("qqq005_.",						SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),
			ROW_NEW2("qqq005_.....",					SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),
			ROW_NEW2("qqq005_",							SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("qqq006_\xe2\x80\x8c.txt",			SG_PORT_FLAGS__INDIV__IGNORABLE|SG_PORT_FLAGS__COLLISION__IGNORABLE),	// u+200c Zero width non-joiner
			ROW_NEW2("qqq006_\xe2\x81\xaa.txt",			SG_PORT_FLAGS__INDIV__IGNORABLE|SG_PORT_FLAGS__COLLISION__IGNORABLE),	// u+200c Zero width non-joiner
			ROW_NEW2("qqq006_.txt",						SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("qqq008_\xce\xb1\xcd\x82\xcd\x85.txt",	SG_PORT_FLAGS__COLLISION__NFC),												// 03b1 0342 0345
			ROW_NEW2("qqq008_\xce\xb1\xcd\x85\xcd\x82.txt",	SG_PORT_FLAGS__INDIV__NON_CANONICAL_NFD|SG_PORT_FLAGS__COLLISION__NFC),		// 03b1 0345 0342
			ROW_NEW2("qqq008_\xe1\xbe\xb7.txt",				SG_PORT_FLAGS__COLLISION__ORIGINAL),										// 1fb7

			ROW_NEW1("qqq009_\xe1\xbe\xb3\xcd\x82.txt",	SG_PORT_FLAGS__INDIV__NON_CANONICAL_NFD|SG_PORT_FLAGS__COLLISION__NFC),		// 1fb3 (03b1 0345) 0342
			ROW_NEW2("qqq009_\xe1\xbe\xb6\xcd\x85.txt",	SG_PORT_FLAGS__INDIV__NON_CANONICAL_NFD|SG_PORT_FLAGS__COLLISION__NFC),		// 1fb6 (03b1 0342) 0345
			ROW_NEW2("qqq009_\xe1\xbe\xb7.txt",			SG_PORT_FLAGS__COLLISION__ORIGINAL),										// 1fb7 (03b1 0345 0342)

			// case folding for non-us-ascii cases....
			// we stick a u00A9 COPYRIGHT SIGN \xc2\xa9 in the middle of each name to force us to go thru the 32bit code path.

			ROW_NEW1("zzz001_\xc2\xa9_AAA",		SG_PORT_FLAGS__COLLISION__CASE),
			ROW_NEW2("zzz001_\xc2\xa9_Aaa",		SG_PORT_FLAGS__COLLISION__CASE),

			ROW_NEW1("zzz002_\xc2\xa9_AAA",		SG_PORT_FLAGS__COLLISION__CASE),
			ROW_NEW2("zzz002_\xc2\xa9_aaa",		SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("zzz003_\xc2\xa9_aaa",		SG_PORT_FLAGS__COLLISION__ORIGINAL),
			ROW_NEW2("zzz003_\xc2\xa9_AAA",		SG_PORT_FLAGS__COLLISION__CASE),

			ROW_NEW1("zzz004_\xc2\xa9_aaa",		SG_PORT_FLAGS__COLLISION__ORIGINAL),
			ROW_NEW2("zzz004_\xc2\xa9_Aaa",		SG_PORT_FLAGS__COLLISION__CASE),
			ROW_NEW2("zzz004_\xc2\xa9_AAA",		SG_PORT_FLAGS__COLLISION__CASE),

			ROW_NEW1("zzz005_\xc2\xa9_\xc3\xa0\xc3\xa0.txt",		SG_PORT_FLAGS__COLLISION__ORIGINAL),	// 00e0 latin small letter a with grave + 00e0
			ROW_NEW2("zzz005_\xc2\xa9_\xc3\x80\xc3\xa0.txt",		SG_PORT_FLAGS__COLLISION__CASE),		// 00c0 latin capital letter a with grave + 00e0
			ROW_NEW2("zzz005_\xc2\xa9_\xc3\x80\xc3\x80.txt",		SG_PORT_FLAGS__COLLISION__CASE),		// 00c0 + 00c0

			ROW_NEW1("zzz006_\xc2\xa9_\xc5\x91\xc5\x91.txt",		SG_PORT_FLAGS__COLLISION__ORIGINAL),	// 0151 latin small letter o with double acute + 0151
			ROW_NEW2("zzz006_\xc2\xa9_\xc5\x90\xc5\x90.txt",		SG_PORT_FLAGS__COLLISION__CASE),		// 0150 latin capital letter o with double acute + 0150
			ROW_NEW2("zzz006_\xc2\xa9_\xc5\x90\xc5\x91.txt",		SG_PORT_FLAGS__COLLISION__CASE),		// 0150 latin capital letter o with double acute + 0151

			ROW_NEW1("zzz007_\xc2\xa9_\xce\xb1\xce\xb1.txt",		SG_PORT_FLAGS__COLLISION__ORIGINAL),	// 03b1 greek small letter alpha + 03b1
			ROW_NEW2("zzz007_\xc2\xa9_\xce\x91\xce\x91.txt",		SG_PORT_FLAGS__COLLISION__CASE),		// 0391 greek capital letter alpha + 0391
			ROW_NEW2("zzz007_\xc2\xa9_\xce\x91\xce\xb1.txt",		SG_PORT_FLAGS__COLLISION__CASE),		// 0391 greek capital letter alpha + 03b1

			ROW_NEW1("zzz008_\xc2\xa9_\xc5\x91\xc5\x91.txt",		SG_PORT_FLAGS__COLLISION__ORIGINAL),	// 0151 latin small letter o with double acute + 0151
			ROW_NEW2("zzz008_\xc2\xa9_\xc5\x90\xc5\x90.txt",		SG_PORT_FLAGS__COLLISION__CASE),		// 0150 latin capital letter o with double acute + 0150
			ROW_NEW2("zzz008_\xc2\xa9_\xc5\x90\xc5\x91.txt",		SG_PORT_FLAGS__COLLISION__CASE),		// 0150 latin capital letter o with double acute + 0151
			ROW_NEW2("zzz008_\xc2\xa9_o\xcc\x8bo\xcc\x8b.txt",		SG_PORT_FLAGS__COLLISION__NFC),			// 0151 ('o' 030b double acute) 0151 ('o' 030b)

			ROW_NEW1("zzz009_\xc2\xa9_o\xcc\x8bo\xcc\x8b.txt",		SG_PORT_FLAGS__COLLISION__NFC),			// 0151 ('o' 030b) 0151 ('o' 030b)
			ROW_NEW2("zzz009_\xc2\xa9_\xc5\x91\xc5\x91.txt",		SG_PORT_FLAGS__COLLISION__ORIGINAL),	// 0151 latin small letter o with double acute + 0151
			ROW_NEW2("zzz009_\xc2\xa9_\xc5\x90\xc5\x90.txt",		SG_PORT_FLAGS__COLLISION__CASE),		// 0150 latin capital letter o with double acute + 0150
			ROW_NEW2("zzz009_\xc2\xa9_\xc5\x90\xc5\x91.txt",		SG_PORT_FLAGS__COLLISION__CASE),		// 0150 latin capital letter o with double acute + 0151

			ROW_NEW1("zzz010_\xc2\xa9_o\xcc\x8bo\xcc\x8b.txt",		SG_PORT_FLAGS__COLLISION__NFC),									// 0151 ('o' 030b) 0151 ('o' 030b)
			ROW_NEW2("zzz010_\xc2\xa9_\xc5\x91\xc5\x91.txt",		SG_PORT_FLAGS__COLLISION__ORIGINAL),							// 0151 latin small letter o with double acute + 0151
			ROW_NEW2("zzz010_\xc2\xa9_\xc5\x90\xc5\x90.txt",		SG_PORT_FLAGS__COLLISION__CASE),								// 0150 latin capital letter o with double acute + 0150
			ROW_NEW2("zzz010_\xc2\xa9_\xc5\x90\xc5\x91.txt",		SG_PORT_FLAGS__COLLISION__CASE),								// 0150 latin capital letter o with double acute + 0151
			ROW_NEW2("zzz010_\xc2\xa9_O\xcc\x8bO\xcc\x8b.txt",		SG_PORT_FLAGS__COLLISION__CASE|SG_PORT_FLAGS__COLLISION__NFC),	// 0150 ('O' 030b) 0150 ('O' 030b)

			// TODO do locale-based names...
		};

	SG_uint32 nrRows = SG_NrElements(aTestData);
	char * szLocaleCharSet = NULL;

	SG_ERR_CHECK_RETURN(  MyFn(run_a_table)(pCtx, szLocaleCharSet,portMask,aTestData,nrRows)  );
}

void MyFn(run_latin1)(SG_context * pCtx, SG_portability_flags portMask)
{
	// add a series of entrynames to a portability_dir/collider and
	// verify that we get the expected problems and/or collisions.
	// in this test, we assume that the Linux users will be using
	// Latin1 for their LOCALE setting.

	static MyStruct aTestData[] =
		{
			// problematic characters with 7-bit clean entrynames

			ROW_NEW_OK("aaaa"),
			ROW_NEW_OK("bbbb"),
			ROW_NEW_OK("CCCC"),

			ROW_NEW("inv001_>",			SG_PORT_FLAGS__INDIV__INVALID_00_7F),
			ROW_NEW("inv002_|",			SG_PORT_FLAGS__INDIV__INVALID_00_7F),

			ROW_NEW("res002_\x7f",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F),
			ROW_NEW("res002_%",			SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F),

			ROW_NEW("fds001_ ",			SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE),
			ROW_NEW("fds002_.",			SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE),
			ROW_NEW("fds003_    ",		SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE),
			ROW_NEW("fds004_....",		SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE),

			ROW_NEW("com1",				SG_PORT_FLAGS__INDIV__MSDOS_DEVICE),
			ROW_NEW("aux.txt",			SG_PORT_FLAGS__INDIV__MSDOS_DEVICE),

			ROW_NEW("SN0001~1.txt",		SG_PORT_FLAGS__INDIV__SHORTNAMES),
			ROW_NEW("SNzzzz~1.txt",		SG_PORT_FLAGS__INDIV__SHORTNAMES),
			ROW_NEW("S0001~1.txt",		SG_PORT_FLAGS__INDIV__SHORTNAMES),

			// some simple collisions with 7-bit clean entrynames.

			ROW_NEW1("col001_per_%25.txt",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__PERCENT25),
			ROW_NEW2("col001_per_%.txt",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("col002_per_%%.txt",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__ORIGINAL),
			ROW_NEW2("col002_per_%%2525.txt",	SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__PERCENT25),

			ROW_NEW1("col003_fds_",				SG_PORT_FLAGS__COLLISION__ORIGINAL),
			ROW_NEW2("col003_fds_ ",			SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),
			ROW_NEW2("col003_fds_  ",			SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),
			ROW_NEW2("col003_fds_ . ",			SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),

			ROW_NEW1("col004_caseAAA",			SG_PORT_FLAGS__COLLISION__CASE),
			ROW_NEW2("col004_caseAaa",			SG_PORT_FLAGS__COLLISION__CASE),

			ROW_NEW1("col005_casebbb",			SG_PORT_FLAGS__COLLISION__ORIGINAL),
			ROW_NEW2("col005_caseBBB",			SG_PORT_FLAGS__COLLISION__CASE),

			ROW_NEW1("col006_caseCCC",			SG_PORT_FLAGS__COLLISION__CASE),
			ROW_NEW2("col006_caseccc",			SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("col007_7bit_A%25.",		SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__PERCENT25|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__CASE),
			ROW_NEW2("col007_7bit_a%",			SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__ORIGINAL),

			// entryname and pathname length problems

			ROW_NEW(("len001_"
					 "01234567890123456789012345678901234567890123456789"
					 "01234567890123456789012345678901234567890123456789"
					 "01234567890123456789012345678901234567890123456789"
					 "01234567890123456789012345678901234567890123456789"
					 "01234567890123456789012345678901234567890123456789"), SG_PORT_FLAGS__INDIV__ENTRYNAME_LENGTH|SG_PORT_FLAGS__INDIV__PATHNAME_LENGTH),

			// some problematic non-7-bit characters in UTF-8

			ROW_NEW("nonbmp001_\xf0\x90\x80\x80.txt",	SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__NONBMP),		// u+10000 Linear B Syllable B008 A
			ROW_NEW("nonbmp002_\xf0\x90\xa0\x80.txt",	SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__NONBMP),		// u+10800 Cypriot Syllable A

			ROW_NEW("ign001_\xe2\x80\x8c.txt",			SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__IGNORABLE),	// u+200c Zero width non-joiner
			ROW_NEW("ign002_\xe2\x81\xaa.txt",			SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__IGNORABLE),	// u+206a Inhibit symmetric swapping

			ROW_NEW("sfm001_\xef\x80\xa3.txt",			SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__SFM_NTFS),			// u+f023 Private Use (mac sfm '<')
			ROW_NEW("sfm002_\xef\x80\xa4.txt",			SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__SFM_NTFS),			// u+f023 Private Use (mac sfm '>')
			ROW_NEW("sfm003_\xef\x80\xa8",				SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__SFM_NTFS),			// u+f023 Private Use (mac sfm final space)
			ROW_NEW("sfm004_\xef\x80\xa9",				SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__SFM_NTFS),			// u+f023 Private Use (mac sfm final dot)

			// 03B1;GREEK SMALL LETTER ALPHA;Ll;0;L;;;;;N;;;0391;;0391
			// 0345;COMBINING GREEK YPOGEGRAMMENI;Mn;240;NSM;;;;;N;GREEK NON-SPACING IOTA BELOW;;0399;;0399
			// 0342;COMBINING GREEK PERISPOMENI;Mn;230;NSM;;;;;N;;;;;
			//
			// 0342 is class 230; 0345 is class 240 ---> so 0342 comes before 0345
			//
			// 1FB7;1FB7;03B1 0342 0345;1FB7;03B1 0342 0345;
			//
			// 1FB3;GREEK SMALL LETTER ALPHA WITH YPOGEGRAMMENI;Ll;0;L;03B1 0345;;;;N;;;1FBC;;1FBC
			// 1FB6;GREEK SMALL LETTER ALPHA WITH PERISPOMENI;Ll;0;L;03B1 0342;;;;N;;;;;
			// 1FB7;GREEK SMALL LETTER ALPHA WITH PERISPOMENI AND YPOGEGRAMMENI;Ll;0;L;1FB6 0345;;;;N;;;;;

			ROW_NEW("nfd002_\xce\xb1\xcd\x85\xcd\x82.txt",	SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__NON_CANONICAL_NFD),	// u03b1 u0345 u0342

			// potential collisions due to non-7-bit problematic characters in UTF-8

			ROW_NEW1("qqq001_\xef\xbf\xbd.txt",			SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__ORIGINAL),								// u+fffd replacement character
			ROW_NEW2("qqq001_\xf0\x90\x80\x80.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__NONBMP|SG_PORT_FLAGS__COLLISION__NONBMP),		// u+10000 non-bmp char folded to replacement character
			ROW_NEW2("qqq001_\xf0\x90\xa0\x80.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__NONBMP|SG_PORT_FLAGS__COLLISION__NONBMP),		// u+10800 non-bmp char folded to replacement character

			ROW_NEW1("qqq002_\xce\xb2_per_%25.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__PERCENT25),	// "%25" <--> "%" with U+03b2 greek small letter beta
			ROW_NEW2("qqq002_\xce\xb2_per_%.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("qqq003_\xce\xb2_per_%%.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__ORIGINAL),		// "%25" <--> "%" with U+03b2 greek small letter beta
			ROW_NEW2("qqq003_\xce\xb2_per_%%2525.txt",	SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__RESTRICTED_00_7F|SG_PORT_FLAGS__COLLISION__PERCENT25),

			ROW_NEW1("qqq004_\xef\x80\xa3.txt",			SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__SFM_NTFS|SG_PORT_FLAGS__COLLISION__SFM_NTFS),			// u+f023 Private Use (mac sfm '<')
			ROW_NEW2("qqq004_<.txt",					SG_PORT_FLAGS__INDIV__INVALID_00_7F|SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("qqq005_\xef\x80\xa8",				SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__SFM_NTFS|SG_PORT_FLAGS__COLLISION__SFM_NTFS|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),			// u+f023 Private Use (mac sfm final space)
			ROW_NEW2("qqq005_\xef\x80\xa9",				SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__SFM_NTFS|SG_PORT_FLAGS__COLLISION__SFM_NTFS|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),			// u+f023 Private Use (mac sfm final dot)
			ROW_NEW2("qqq005_....\xef\x80\xa9",			SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__SFM_NTFS|SG_PORT_FLAGS__COLLISION__SFM_NTFS|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),			// u+f023 Private Use (mac sfm final dot)
			ROW_NEW2("qqq005_ ",							SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),
			ROW_NEW2("qqq005_.",							SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),
			ROW_NEW2("qqq005_.....",						SG_PORT_FLAGS__INDIV__FINAL_DOT_SPACE|SG_PORT_FLAGS__COLLISION__FINAL_DOT_SPACE),
			ROW_NEW2("qqq005_",							SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("qqq006_\xe2\x80\x8c.txt",			SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__IGNORABLE|SG_PORT_FLAGS__COLLISION__IGNORABLE),	// u+200c Zero width non-joiner
			ROW_NEW2("qqq006_\xe2\x81\xaa.txt",			SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__IGNORABLE|SG_PORT_FLAGS__COLLISION__IGNORABLE),	// u+200c Zero width non-joiner
			ROW_NEW2("qqq006_.txt",						SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("qqq008_\xce\xb1\xcd\x82\xcd\x85.txt",	SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__NFC),												// 03b1 0342 0345
			ROW_NEW2("qqq008_\xce\xb1\xcd\x85\xcd\x82.txt",	SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__NON_CANONICAL_NFD|SG_PORT_FLAGS__COLLISION__NFC),		// 03b1 0345 0342
			ROW_NEW2("qqq008_\xe1\xbe\xb7.txt",				SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__ORIGINAL),										// 1fb7

			ROW_NEW1("qqq009_\xe1\xbe\xb3\xcd\x82.txt",	SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__NON_CANONICAL_NFD|SG_PORT_FLAGS__COLLISION__NFC),		// 1fb3 (03b1 0345) 0342
			ROW_NEW2("qqq009_\xe1\xbe\xb6\xcd\x85.txt",	SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__INDIV__NON_CANONICAL_NFD|SG_PORT_FLAGS__COLLISION__NFC),		// 1fb6 (03b1 0342) 0345
			ROW_NEW2("qqq009_\xe1\xbe\xb7.txt",			SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__ORIGINAL),										// 1fb7 (03b1 0345 0342)

			// case folding for non-us-ascii cases....
			// we stick a u00A9 COPYRIGHT SIGN \xc2\xa9 in the middle of each name to force us to go thru the 32bit code path.
			// u00a9 copyright sign is present in Latin-1

			ROW_NEW1("zzz001_\xc2\xa9_AAA",		SG_PORT_FLAGS__COLLISION__CASE),
			ROW_NEW2("zzz001_\xc2\xa9_Aaa",		SG_PORT_FLAGS__COLLISION__CASE),

			ROW_NEW1("zzz002_\xc2\xa9_AAA",		SG_PORT_FLAGS__COLLISION__CASE),
			ROW_NEW2("zzz002_\xc2\xa9_aaa",		SG_PORT_FLAGS__COLLISION__ORIGINAL),

			ROW_NEW1("zzz003_\xc2\xa9_aaa",		SG_PORT_FLAGS__COLLISION__ORIGINAL),
			ROW_NEW2("zzz003_\xc2\xa9_AAA",		SG_PORT_FLAGS__COLLISION__CASE),

			ROW_NEW1("zzz004_\xc2\xa9_aaa",		SG_PORT_FLAGS__COLLISION__ORIGINAL),
			ROW_NEW2("zzz004_\xc2\xa9_Aaa",		SG_PORT_FLAGS__COLLISION__CASE),
			ROW_NEW2("zzz004_\xc2\xa9_AAA",		SG_PORT_FLAGS__COLLISION__CASE),

			ROW_NEW1("zzz005_\xc2\xa9_\xc3\xa0\xc3\xa0.txt",		SG_PORT_FLAGS__COLLISION__ORIGINAL),	// 00e0 latin small letter a with grave + 00e0
			ROW_NEW2("zzz005_\xc2\xa9_\xc3\x80\xc3\xa0.txt",		SG_PORT_FLAGS__COLLISION__CASE),		// 00c0 latin capital letter a with grave + 00e0
			ROW_NEW2("zzz005_\xc2\xa9_\xc3\x80\xc3\x80.txt",		SG_PORT_FLAGS__COLLISION__CASE),		// 00c0 + 00c0

			ROW_NEW1("zzz006_\xc2\xa9_\xc5\x91\xc5\x91.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__ORIGINAL),	// 0151 latin small letter o with double acute + 0151
			ROW_NEW2("zzz006_\xc2\xa9_\xc5\x90\xc5\x90.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__CASE),		// 0150 latin capital letter o with double acute + 0150
			ROW_NEW2("zzz006_\xc2\xa9_\xc5\x90\xc5\x91.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__CASE),		// 0150 latin capital letter o with double acute + 0151

			ROW_NEW1("zzz007_\xc2\xa9_\xce\xb1\xce\xb1.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__ORIGINAL),	// 03b1 greek small letter alpha + 03b1
			ROW_NEW2("zzz007_\xc2\xa9_\xce\x91\xce\x91.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__CASE),		// 0391 greek capital letter alpha + 0391
			ROW_NEW2("zzz007_\xc2\xa9_\xce\x91\xce\xb1.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__CASE),		// 0391 greek capital letter alpha + 03b1

			ROW_NEW1("zzz008_\xc2\xa9_\xc5\x91\xc5\x91.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__ORIGINAL),	// 0151 latin small letter o with double acute + 0151
			ROW_NEW2("zzz008_\xc2\xa9_\xc5\x90\xc5\x90.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__CASE),		// 0150 latin capital letter o with double acute + 0150
			ROW_NEW2("zzz008_\xc2\xa9_\xc5\x90\xc5\x91.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__CASE),		// 0150 latin capital letter o with double acute + 0151
			ROW_NEW2("zzz008_\xc2\xa9_o\xcc\x8bo\xcc\x8b.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__NFC),			// 0151 ('o' 030b double acute) 0151 ('o' 030b)

			ROW_NEW1("zzz009_\xc2\xa9_o\xcc\x8bo\xcc\x8b.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__NFC),			// 0151 ('o' 030b) 0151 ('o' 030b)
			ROW_NEW2("zzz009_\xc2\xa9_\xc5\x91\xc5\x91.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__ORIGINAL),	// 0151 latin small letter o with double acute + 0151
			ROW_NEW2("zzz009_\xc2\xa9_\xc5\x90\xc5\x90.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__CASE),		// 0150 latin capital letter o with double acute + 0150
			ROW_NEW2("zzz009_\xc2\xa9_\xc5\x90\xc5\x91.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__CASE),		// 0150 latin capital letter o with double acute + 0151

			ROW_NEW1("zzz010_\xc2\xa9_o\xcc\x8bo\xcc\x8b.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__NFC),									// 0151 ('o' 030b) 0151 ('o' 030b)
			ROW_NEW2("zzz010_\xc2\xa9_\xc5\x91\xc5\x91.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__ORIGINAL),							// 0151 latin small letter o with double acute + 0151
			ROW_NEW2("zzz010_\xc2\xa9_\xc5\x90\xc5\x90.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__CASE),								// 0150 latin capital letter o with double acute + 0150
			ROW_NEW2("zzz010_\xc2\xa9_\xc5\x90\xc5\x91.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__CASE),								// 0150 latin capital letter o with double acute + 0151
			ROW_NEW2("zzz010_\xc2\xa9_O\xcc\x8bO\xcc\x8b.txt",		SG_PORT_FLAGS__INDIV__CHARSET|SG_PORT_FLAGS__COLLISION__CASE|SG_PORT_FLAGS__COLLISION__NFC),	// 0150 ('O' 030b) 0150 ('O' 030b)

		};

	SG_uint32 nrRows = SG_NrElements(aTestData);
	char * szLocaleCharSet = "ISO-8859-1";

	SG_ERR_CHECK_RETURN(  MyFn(run_a_table)(pCtx, szLocaleCharSet,portMask,aTestData,nrRows)  );
}

//////////////////////////////////////////////////////////////////

void MyFn(run_trivial_collisions)(SG_context * pCtx, SG_portability_flags portMask)
{
	// add a series of entrynames to a portability_dir/collider and
	// test for trivial collisions.
	//
	// we still assume a UTF-8 LOCALE for Linux users (because I haven't
	// written the locale<-->utf-8 code yet.)

	static MyStruct aTestData[] =
		{
			ROW_NEW_OK("aaaa"),
			ROW_NEW_OK("bbbb"),
			ROW_NEW_OK("CCCC"),

			ROW_DUP("aaaa"),
			ROW_DUP("bbbb"),
			ROW_DUP("CCCC"),
		};

	SG_uint32 nrRows = SG_NrElements(aTestData);
	char * szLocaleCharSet = NULL;

	SG_ERR_CHECK_RETURN(  MyFn(run_a_table)(pCtx, szLocaleCharSet,portMask,aTestData,nrRows)  );
}

void MyFn(run_debug_bad_name_i)(SG_context * pCtx, SG_portability_flags portMask)
{
	// add a series of entrynames to a portability_dir/collider and
	// test for trivial collisions.

	static MyStruct aTestData[] =
		{
			ROW_NEW(SG_PORT_DEBUG_BAD_NAME, SG_PORT_FLAGS__INDIV__DEBUG_BAD_NAME),
		};

	SG_uint32 nrRows = SG_NrElements(aTestData);
	char * szLocaleCharSet = NULL;

	SG_ERR_CHECK_RETURN(  MyFn(run_a_table)(pCtx, szLocaleCharSet,portMask,aTestData,nrRows)  );
}

void MyFn(run_debug_bad_name_c)(SG_context * pCtx, SG_portability_flags portMask)
{
	// add a series of entrynames to a portability_dir/collider and
	// test for trivial collisions.

	static MyStruct aTestData[] =
		{
			ROW_NEW1(SG_PORT_DEBUG_BAD_NAME       , SG_PORT_FLAGS__INDIV__DEBUG_BAD_NAME | SG_PORT_FLAGS__COLLISION__ORIGINAL),
			ROW_NEW2(SG_PORT_DEBUG_BAD_NAME ".foo", SG_PORT_FLAGS__INDIV__DEBUG_BAD_NAME | SG_PORT_FLAGS__COLLISION__DEBUG_BAD_NAME),
			ROW_NEW2(SG_PORT_DEBUG_BAD_NAME ".bar", SG_PORT_FLAGS__INDIV__DEBUG_BAD_NAME | SG_PORT_FLAGS__COLLISION__DEBUG_BAD_NAME),
		};

	SG_uint32 nrRows = SG_NrElements(aTestData);
	char * szLocaleCharSet = NULL;

	SG_ERR_CHECK_RETURN(  MyFn(run_a_table)(pCtx, szLocaleCharSet,portMask,aTestData,nrRows)  );
}

//////////////////////////////////////////////////////////////////

/**
 * The main test.
 */
MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(run_utf8)(pCtx, SG_PORT_FLAGS__ALL)  );
	BEGIN_TEST(  MyFn(run_latin1)(pCtx, SG_PORT_FLAGS__ALL)  );
	BEGIN_TEST(  MyFn(run_trivial_collisions)(pCtx, SG_PORT_FLAGS__ALL)  );

	BEGIN_TEST(  MyFn(run_utf8)(pCtx, SG_PORT_FLAGS__MASK_INDIV)  );
	BEGIN_TEST(  MyFn(run_utf8)(pCtx, SG_PORT_FLAGS__MASK_COLLISION)  );

	BEGIN_TEST(  MyFn(run_debug_bad_name_i)(pCtx, SG_PORT_FLAGS__ALL)  );
	BEGIN_TEST(  MyFn(run_debug_bad_name_c)(pCtx, SG_PORT_FLAGS__ALL)  );

	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn
#undef MyStruct_
#undef MyStruct
#undef ROW
#undef ROW_NEW_OK
#undef ROW_NEW
#undef ROW_NEW1
#undef ROW_NEW2
#undef ROW_DUP
