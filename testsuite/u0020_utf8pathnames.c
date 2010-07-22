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

// testsuite/unittests/u0020_utf8pathnames.c
// test/explore various utf8 and pathname issues.
//////////////////////////////////////////////////////////////////

#include <sg.h>

#include "unittests.h"

//////////////////////////////////////////////////////////////////

void u0020_utf8pathnames__mkdir_tmp_dir(SG_context * pCtx, SG_pathname ** ppPathnameTmpDir)
{
	// create a temporary directory using a random name in the
	// current directory or in TMP.
	//
	// return the name of the pathname.

	char bufTid[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname * pPathnameTmpDir = NULL;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTid, sizeof(bufTid), 32)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__USER_TEMP_DIRECTORY(pCtx, &pPathnameTmpDir)  );
	VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPathnameTmpDir,bufTid)  );

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir_recursive__pathname(pCtx, pPathnameTmpDir)  );

	*ppPathnameTmpDir = pPathnameTmpDir;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathnameTmpDir);
}

// here we have some sample file names (written in utf32).
// some are in NFC and some are not.

SG_int32 aA_32 []= { 0x0041, 0x0041, 0x0041, 0x0000 };				// AAA (nfc)
SG_byte  aA_8  []= { 0x41,   0x41,   0x41,   0x00 };
SG_byte  aA_nfc[]= { 0x41,   0x41,   0x41,   0x00 };
SG_byte  aA_nfd[]= { 0x41,   0x41,   0x41,   0x00 };				// nfd and nfc same when no special squences (right???)

SG_int32 aB_32 []= { 0x0042, 0x03a9,     0x0042, 0x0000 };			// B [Greek Capital Letter Omega] B (nfc)
SG_byte  aB_8  []= { 0x42,   0xce, 0xa9, 0x42,   0x00 };
SG_byte  aB_nfc[]= { 0x42,   0xce, 0xa9, 0x42,   0x00 };
SG_byte  aB_nfd[]= { 0x42,   0xce, 0xa9, 0x42,   0x00 };

SG_int32 aC_32 []= { 0x0043, 0x0041, 0x0300,     0x0043, 0x0000 };	// C A [Combining Grave Accent] C (nfd)
SG_byte  aC_8  []= { 0x43,   0x41,   0xcc, 0x80, 0x43,   0x00 };
SG_byte  aC_nfc[]= { 0x43,   0xc3, 0x80,         0x43,   0x00 };
SG_byte  aC_nfd[]= { 0x43,   0x41,   0xcc, 0x80, 0x43,   0x00 };

SG_int32 aD_32 []= { 0x0044, 0x00c0,           0x0044, 0x0000 };	// D [Latin Capital Letter A with Grave] D (nfc)
SG_byte  aD_8  []= { 0x44,   0xc3, 0x80,       0x44,     0x00 };
SG_byte  aD_nfc[]= { 0x44,   0xc3, 0x80,       0x44,     0x00 };
SG_byte  aD_nfd[]= { 0x44,   0x41, 0xcc, 0x80, 0x44,     0x00 };

// U+1f67 decomposes into U+03c9 U+0314 U+0342 or (U+1f61 U+0342)
SG_int32 aE_32 []= { 0x0045, 0x1f67,                             0x0045, 0x0000 };	// E [Greek Small Letter Omega with Dasia and Perispomeni] E (nfc)
SG_byte  aE_8  []= { 0x45,   0xe1, 0xbd, 0xa7,                   0x45,   0x00 };
SG_byte  aE_nfc[]= { 0x45,   0xe1, 0xbd, 0xa7,                   0x45,   0x00 };
SG_byte  aE_nfd[]= { 0x45,   0xcf, 0x89, 0xcc, 0x94, 0xcd, 0x82, 0x45,   0x00 };

SG_int32 aF_32 []= { 0x0046, 0x03c9,     0x0314,     0x0342,     0x0046, 0x0000 };	// F [Greek Small Letter Omega] [Combining Reversed Comma Above] [Combininb Greek Perispomeni] F (nfd)
SG_byte  aF_8  []= { 0x46,   0xcf, 0x89, 0xcc, 0x94, 0xcd, 0x82, 0x46,   0x00 };
SG_byte  aF_nfc[]= { 0x46,   0xe1, 0xbd, 0xa7,                   0x46,   0x00 };
SG_byte  aF_nfd[]= { 0x46,   0xcf, 0x89, 0xcc, 0x94, 0xcd, 0x82, 0x46,   0x00 };

typedef struct _tableitem
{
	SG_int32 * pa32;
	SG_byte * pa8;
	SG_byte * paNfc;
	SG_byte * paNfd;

} _tableitem;

static _tableitem table[] =
{
	{ aA_32, aA_8, aA_nfc, aA_nfd },
	{ aB_32, aB_8, aB_nfc, aB_nfd },
	{ aC_32, aC_8, aC_nfc, aC_nfd },
	{ aD_32, aD_8, aD_nfc, aD_nfd },
	{ aE_32, aE_8, aE_nfc, aE_nfd },
	{ aF_32, aF_8, aF_nfc, aF_nfd },
};

int u0020_utf8pathnames__testfilename(SG_string * pStringFilename)
{
	// verify that file name matches what we expect.
	//
	// WE RELY ON THE FACT THAT EACH FILENAME IN THE ARRAY STARTS
	// WITH A DIFFERENT LETTER.

	const char * szFilename = SG_string__sz(pStringFilename);
	char c = szFilename[0];

	if (c == '.')		// "." and ".."
		return 1;

	if ((c >= 'A') && (c <= 'Z'))
	{
		_tableitem * pti;
		size_t tableindex = (c - 'A');
		SG_bool bTestIsNFC, bTestIsNFD;
		SG_bool bMatchGiven, bMatchNFC, bMatchNFD;

		VERIFYP_COND_RETURN("u0020_utf8pathnames",(tableindex < SG_NrElements(table)),("unexpected file [%s]",szFilename));

		pti = &table[tableindex ];

		// verify that we didn't mess up when creating the test case.

		bTestIsNFC = (strcmp((char *)pti->pa8,(char *)pti->paNfc) == 0);
		bTestIsNFD = (strcmp((char *)pti->pa8,(char *)pti->paNfd) == 0);
		VERIFYP_COND("u0020_utf8pathnames",(bTestIsNFC || bTestIsNFD),("Is tableitem[%c] NFC or NFD?",c));

		// see if the filename we got from the system matches what we gave.
		// if not, see if it matches the NFC() version or the NFD() version.

		bMatchGiven = (strcmp(szFilename,(char *)pti->pa8) == 0);
		bMatchNFC   = (strcmp(szFilename,(char *)pti->paNfc) == 0);
		bMatchNFD   = (strcmp(szFilename,(char *)pti->paNfd) == 0);

		INFOP("u0020_utf8pathnames",("tableitem[%c] NFC[%d] [NFC %s NFD] : MatchGiven[%d] MatchNFC[%d] MatchNFD[%d]",
									 c,
									 bTestIsNFC,((bTestIsNFC == bTestIsNFD) ? "==" : "!="),
									 bMatchGiven,bMatchNFC,bMatchNFD));

//		// TODO fix this test based upon what we know about the platform.
//		VERIFYP_COND("u0020_utf8pathnames",(strcmp(szFilename,(char *)pti->pa8)==0),("Received [%s] expected [%s]",szFilename,pti->pa8));
		return 1;
	}

	VERIFYP_COND_RETURN("u0020_utf8pathnames",(0),("unexpected file [%s]",szFilename));
}

int u0020_utf8pathnames__readdir(SG_context * pCtx, const SG_pathname * pPathnameTmpDir)
{
	// open the tmp dir for reading and read the filename of each file in it.
	// compare these with the version of the filename that we used to create
	// the file.
	//
	// WE RELY ON THE FACT THAT EACH FILENAME IN THE ARRAY STARTS
	// WITH A DIFFERENT LETTER.

	SG_error errRead;
	SG_dir * pDir;
	SG_string * pStringFilename = NULL;

	VERIFY_ERR_CHECK_DISCARD(  SG_STRING__ALLOC(pCtx, &pStringFilename)  );

	// opendir gives us the first file automatically.

	VERIFY_ERR_CHECK_DISCARD(  SG_dir__open(pCtx,pPathnameTmpDir,&pDir,&errRead,pStringFilename,NULL)  );
	VERIFYP_COND("u0020_utf8pathnames",(SG_IS_OK(errRead)),("Reading first file in directory."));

	do
	{
		u0020_utf8pathnames__testfilename(pStringFilename);

		SG_dir__read(pCtx,pDir,pStringFilename,NULL);
		SG_context__get_err(pCtx,&errRead);

	} while (SG_IS_OK(errRead));

	VERIFY_CTX_ERR_EQUALS("u0020_utf8pathnames",pCtx,SG_ERR_NOMOREFILES);
	SG_context__err_reset(pCtx);

	SG_DIR_NULLCLOSE(pCtx, pDir);
	SG_STRING_NULLFREE(pCtx, pStringFilename);
	return 1;
}

int u0020_utf8pathnames__create_file(SG_context * pCtx, const SG_pathname * pPathnameTmpDir, _tableitem * pti)
{
	// create a file in the given tmp dir using the given filename.

	SG_pathname * pPathnameNewFile;
	char * pBufUtf8;
	SG_uint32 lenUtf8;
	SG_file * pFile;
	int iResult;
	SG_bool bTest;

	// convert the utf32 string into utf8.

	VERIFY_ERR_CHECK_DISCARD(  SG_utf8__from_utf32(pCtx, pti->pa32,&pBufUtf8,&lenUtf8)  );	// we have to free pBufUtf8

	// verify that the computed utf8 string matches what we thought it should be.
	// (this hopefully guards against the conversion layer playing NFC/NFD tricks.)

	iResult = SG_utf8__compare(pBufUtf8,(char *)pti->pa8);
	VERIFYP_COND("u0020_utf8pathnames",(iResult==0),("Compare failed [%s][%s]",pBufUtf8,pti->pa8));

	// create full pathname to the file to create.

	VERIFY_ERR_CHECK_DISCARD(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,
															   &pPathnameNewFile,
															   pPathnameTmpDir,pBufUtf8)  );

	// create the file and close it.
	// on Linux when our locale is set to something other than UTF-8, we may
	// get an ICU(10) == U_INVALID_CHAR_FOUND error because the test data is
	// not necessarily friendly to any one locale and there are some NFD
	// cases too.   we map ICU(10) to SG_ERR_UNMAPPABLE_UNICODE_CHAR

	SG_file__open__pathname(pCtx,pPathnameNewFile,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,0755,&pFile);
#if defined(LINUX)
	bTest = (  (!SG_context__has_err(pCtx))  ||  (SG_context__err_equals(pCtx,SG_ERR_UNMAPPABLE_UNICODE_CHAR))  );
#else
	bTest = (  (!SG_context__has_err(pCtx))  );
#endif
	SG_context__err_reset(pCtx);

	VERIFYP_COND("u0020_utf8pathnames",bTest,
			 ("Error Creating file [%s]",SG_pathname__sz(pPathnameNewFile)));

	VERIFY_ERR_CHECK_DISCARD(  SG_file__close(pCtx, &pFile)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathnameNewFile);
	SG_NULLFREE(pCtx, pBufUtf8);

	return 1;
}

int u0020_utf8pathnames__test(SG_context * pCtx)
{
	SG_pathname * pPathnameTmpDir;
	int k, kLimit;

	// create a temporary directory.

	VERIFY_ERR_CHECK_RETURN(  u0020_utf8pathnames__mkdir_tmp_dir(pCtx, &pPathnameTmpDir)  );

	INFOP("u0020_utf8pathnames",("Creating directory [%s]",SG_pathname__sz(pPathnameTmpDir)));

	// create a series of files in the temporary directory that have
	// various unicode characters in their names.

	kLimit = SG_NrElements(table);
	for (k=0; k<kLimit; k++)
	{
		_tableitem * pti = &table[k];

		INFOP("u0020_utf8pathnames",("[%d] starts with [%c]",k,pti->pa8[0]));
		VERIFY_ERR_CHECK_DISCARD(  u0020_utf8pathnames__create_file(pCtx,pPathnameTmpDir,pti)  );
	}

	// open the tmp dir for reading and read the filename of each file in it.
	// compare these with the version of the filename that we used to create
	// the file.

	VERIFY_ERR_CHECK_DISCARD(  u0020_utf8pathnames__readdir(pCtx, pPathnameTmpDir)  );

	// clean up our mess

	VERIFY_ERR_CHECK_DISCARD(  SG_fsobj__rmdir_recursive__pathname(pCtx, pPathnameTmpDir)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathnameTmpDir);
	return 1;
}


TEST_MAIN(u0020_utf8pathnames)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0020_utf8pathnames__test(pCtx)  );

	TEMPLATE_MAIN_END;
}
