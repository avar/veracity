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
 * @file u0072_repo_hash_method.c
 *
 * @details Some tests to exercise the creation of blobs via the repo api
 *          using various hash-methods.
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0072_repo_hash_method)
#define MyDcl(name)				u0072_repo_hash_method__##name
#define MyFn(name)				u0072_repo_hash_method__##name

#define MyMaxFile				(16*1024)
#define MyStepFile				(3*1024)

//////////////////////////////////////////////////////////////////

/**
 * lookup the hash-method associated with this REPO and
 * compute how long the HIDs should be.
 */
void MyFn(get_hash_length)(SG_context * pCtx, SG_repo * pRepo, SG_uint32 * p_strlen_hashes)
{
	char * pszHashMethod = NULL;
	char * p;
	SG_uint32 bitLength;

	VERIFY_ERR_CHECK(  SG_repo__get_hash_method(pCtx, pRepo, &pszHashMethod)  );
	// parse "SHA1/160"
	SG_ASSERT( (pszHashMethod) );
	SG_ASSERT( (pszHashMethod[0]) );
	for (p=pszHashMethod; (*p && *p!='/'); p++)
		;
	bitLength = (SG_uint32)atoi(++p);

	*p_strlen_hashes = ((bitLength / 8) * 2);

fail:
	SG_NULLFREE(pCtx, pszHashMethod);
}

//////////////////////////////////////////////////////////////////

void MyFn(create_repo)(SG_context * pCtx, const char * pszHashMethod, SG_repo ** ppRepo)
{
	// caller must free returned value.

	SG_repo * pRepo;
	SG_pathname * pPathnameRepoDir = NULL;
	SG_vhash* pvhPartialDescriptor = NULL;
    char buf_repo_id[SG_GID_BUFFER_LENGTH];
    char buf_admin_id[SG_GID_BUFFER_LENGTH];
	char* pszRepoImpl = NULL;

	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_repo_id, sizeof(buf_repo_id))  );
	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_admin_id, sizeof(buf_admin_id))  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathnameRepoDir)  );
	VERIFY_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathnameRepoDir)  );

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhPartialDescriptor)  );

	VERIFY_ERR_CHECK_DISCARD(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__NEWREPO_DRIVER, NULL, &pszRepoImpl, NULL)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_KEY__STORAGE, pszRepoImpl)  );

	VERIFY_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_FSLOCAL__PATH_PARENT_DIR, SG_pathname__sz(pPathnameRepoDir))  );

	VERIFY_ERR_CHECK(  SG_repo__create_repo_instance(pCtx,pvhPartialDescriptor,SG_TRUE,pszHashMethod,buf_repo_id,buf_admin_id,&pRepo)  );

	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);

	*ppRepo = pRepo;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameRepoDir);

    SG_NULLFREE(pCtx, pszRepoImpl);
}

void MyFn(create_tmp_src_dir)(SG_context * pCtx, SG_pathname ** ppPathnameTempDir)
{
	// create a temp directory in the current directory to be the
	// home of some userfiles.
	// caller must free returned value.

	SG_pathname * pPathnameTempDir = NULL;

	VERIFY_ERR_CHECK(  unittest__alloc_unique_pathname_in_cwd(pCtx,&pPathnameTempDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir_recursive__pathname(pCtx, pPathnameTempDir)  );

	INFOP("mktmpdir",("Temp Src Dir is [%s]",SG_pathname__sz(pPathnameTempDir)));

	*ppPathnameTempDir = pPathnameTempDir;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathnameTempDir);
}

//////////////////////////////////////////////////////////////////

void MyFn(create_blob_from_bytes)(SG_context * pCtx,
								  SG_uint32 strlen_hashes,
								  SG_repo * pRepo,
								  SG_uint32 lenBuf1,
								  const char * szSrc)
{
	// create a large buffer containing some known data.
	// use it to create a blob directly from the buffer.
	// read it back from the repo and verify it.

	char* pszidTidRandom1 = NULL;
	char* pszidTidRandom2 = NULL;
	SG_uint32 lenSrc;
	char* pszidHidBlob1 = NULL;
	char* pszidHidBlob1Dup = NULL;
	char* pszidHidBlob2 = NULL;
	char* pszidHidVerify1 = NULL;
	char* pszidHidVerify2 = NULL;
	SG_bool bEqual;
	char * pbuf1 = NULL;
	char * pbuf2 = NULL;
	char * pbuf1End;
	char * p1;
	SG_uint64 lenBuf2;
	SG_repo_tx_handle* pTx = NULL;
	SG_repo_fetch_blob_handle* pFetchHandle = NULL;

	SG_uint64 lenAbortedBlob;
	SG_uint32 lenGotAbortedBlob;
    SG_bool b_done = SG_FALSE;

	//////////////////////////////////////////////////////////////////

	SG_ERR_IGNORE(  SG_tid__alloc2(pCtx, &pszidTidRandom1, 32)  );
	SG_ERR_IGNORE(  SG_tid__alloc2(pCtx, &pszidTidRandom2, 32)  );

	if (lenBuf1 < 100)
		lenBuf1 += 100;

	pbuf1 = (char *)SG_calloc(1,lenBuf1+1);
	pbuf1End = pbuf1+lenBuf1;
	p1 = pbuf1;

	// write random gid at the beginning of the buffer
	// so that we won't get collisions if we are called
	// multiple times.

	memcpy(p1,pszidTidRandom1,strlen(pszidTidRandom1));
	p1 += strlen(pszidTidRandom1);
	*p1++ = '\n';

	// generate lots of data in the file so that we'll cause the
	// blob routines to exercise the chunking stuff.

	lenSrc = (SG_uint32)strlen(szSrc);
	while (p1+lenSrc < pbuf1End)
	{
		memcpy(p1,szSrc,lenSrc);
		p1 += lenSrc;
	}

	// use the buffer to create a blob in the repo.  we use lenBuf1 as the
	// length rather than (p1-pbuf1) so we may have some nulls at the end.
	// hope this is ok???  i guess we'll find out...

	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  SG_repo__store_blob_from_memory(pCtx, pRepo,pTx,NULL,SG_FALSE,(SG_byte *)pbuf1,lenBuf1,&pszidHidBlob1)  );
	VERIFYP_COND("hash length",(strlen(pszidHidBlob1)==strlen_hashes),
				 ("HID[%s] Length[%d] ExpectedLength[%d]",pszidHidBlob1,strlen(pszidHidBlob1),strlen_hashes));
	VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );

	INFOP("create_blob_from_bytes",("Created blob [%s]",(pszidHidBlob1)));

	//////////////////////////////////////////////////////////////////
	// abort a fetch, ensure it doesn't interfere with subsequent complete fetch

	VERIFY_ERR_CHECK(  SG_repo__fetch_blob__begin(pCtx, pRepo, pszidHidBlob1, SG_TRUE, NULL, NULL, NULL, NULL, &lenAbortedBlob, &pFetchHandle)  );

	// We want to abort mid-blob, so we set an arbitrary (but small) chunk size, and verify that the blob is
	// bigger than it.
	VERIFY_ERR_CHECK(  SG_repo__fetch_blob__chunk(pCtx, pRepo, pFetchHandle, 64, (SG_byte*)pbuf1, &lenGotAbortedBlob, &b_done)  );
	VERIFY_COND("create_blob_from_bytes(fetch abort sufficient length)", lenAbortedBlob > lenGotAbortedBlob);

	VERIFY_ERR_CHECK(  SG_repo__fetch_blob__abort(pCtx, pRepo, &pFetchHandle)  );
	VERIFY_COND("create_blob_from_bytes(fetch abort freed handle)", !pFetchHandle);

	//////////////////////////////////////////////////////////////////
	// fetch blob into a new buffer and verify that it matches.

	VERIFY_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pRepo,pszidHidBlob1,(SG_byte **)&pbuf2,&lenBuf2)  );
	VERIFY_COND("create_blob_from_bytes(fetch blob)",(lenBuf2 == (SG_uint64)lenBuf1));

	//////////////////////////////////////////////////////////////////
	// verify that the contents of buf-2 is identical to the contents of buf-1.
	// (we already know that the HIDs match and was verified during the fetch,
	// but there are times when the HID is just being used as a key -- it
	// doesn't mean that what we actually restored is correct.

	VERIFY_ERR_CHECK(  SG_repo__alloc_compute_hash__from_bytes(pCtx, pRepo, lenBuf1, (SG_byte *)pbuf1, &pszidHidVerify1)  );

	VERIFY_ERR_CHECK(  SG_repo__alloc_compute_hash__from_bytes(pCtx, pRepo, (SG_uint32)lenBuf2, (SG_byte *)pbuf2, &pszidHidVerify2)  );

	bEqual = (0 == (strcmp(pszidHidVerify1,pszidHidVerify2)));
	VERIFY_COND("create_blob_from_bytes(verify v1==v2)",bEqual);

	bEqual = (0 == (strcmp(pszidHidVerify1,pszidHidBlob1)));
	VERIFY_COND("create_blob_from_bytes(verify v1==id)",bEqual);

	VERIFY_COND("creata_blob_from_bytes(memcmp)",(memcmp(pbuf1,pbuf2,lenBuf1)==0));

	//////////////////////////////////////////////////////////////////
	// cleanup

fail:
	SG_NULLFREE(pCtx, pbuf1);
	SG_NULLFREE(pCtx, pbuf2);
	SG_NULLFREE(pCtx, pszidTidRandom1);
	SG_NULLFREE(pCtx, pszidTidRandom2);
	SG_NULLFREE(pCtx, pszidHidBlob1);
	SG_NULLFREE(pCtx, pszidHidBlob1Dup);
	SG_NULLFREE(pCtx, pszidHidBlob2);
	SG_NULLFREE(pCtx, pszidHidVerify1);
	SG_NULLFREE(pCtx, pszidHidVerify2);
}

void MyFn(create_some_blobs_from_bytes)(SG_context * pCtx, SG_repo * pRepo)
{
	SG_uint64 k;
	char * pszHashMethod = NULL;
	char * pszRepoId = NULL;
	char * pszAdminId = NULL;
	SG_uint32 strlen_hashes;

    VERIFY_ERR_CHECK_DISCARD(  SG_repo__get_hash_method(pCtx, pRepo, &pszHashMethod)  );
    VERIFY_ERR_CHECK_DISCARD(  SG_repo__get_repo_id(pCtx, pRepo, &pszRepoId)  );
    VERIFY_ERR_CHECK_DISCARD(  SG_repo__get_admin_id(pCtx, pRepo, &pszAdminId)  );

	VERIFY_ERR_CHECK(  MyFn(get_hash_length)(pCtx, pRepo, &strlen_hashes)  );

	INFOP("create_some_blobs_from_bytes",("HashMethod[%s] RepoID[%s] AdminId[%s] HashLength[%d]",
										  pszHashMethod, pszRepoId, pszAdminId, strlen_hashes));

    SG_NULLFREE(pCtx, pszHashMethod);
    SG_NULLFREE(pCtx, pszRepoId);
    SG_NULLFREE(pCtx, pszAdminId);

	//////////////////////////////////////////////////////////////////
	// create a series of blobs of various known lengths and contents.

	for (k=1; k <= MyMaxFile; k+= MyStepFile)
	{
		SG_ERR_IGNORE(  MyFn(create_blob_from_bytes)(pCtx, strlen_hashes, pRepo,(SG_uint32)k,"Hello World!\nThis is line 2.\n")  );
		SG_ERR_IGNORE(  MyFn(create_blob_from_bytes)(pCtx, strlen_hashes, pRepo,(SG_uint32)k,"Welcome to the middle of the film!\n")  );
	}

	return;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

void MyFn(create_blob_from_file)(SG_context * pCtx,
								 SG_uint32 strlen_hashes,
								 SG_repo * pRepo,
								 const SG_pathname * pPathnameTempDir,
								 SG_uint64 lenFile,
								 const char * szSrc)
{
	// create a file of length "lenFile" in the temp directory.
	// use it to create a blob.
	// try to create it a second time and verify that we get an duplicate-hid error.

	char* pszidGidRandom1 = NULL;
	char* pszidGidRandom2 = NULL;
	SG_pathname * pPathnameTempFile1 = NULL;
	SG_pathname * pPathnameTempFile2 = NULL;
	SG_file * pFileTempFile1 = NULL;
	SG_file * pFileTempFile2 = NULL;
	SG_uint32 lenSrc;
	SG_uint64 lenWritten;
	char* pszidHidBlob1 = NULL;
	char* pszidHidBlob1Dup = NULL;
	char* pszidHidBlob2 = NULL;
	char* pszidHidVerify1 = NULL;
	char* pszidHidVerify2 = NULL;
	SG_bool bEqual;
	SG_repo_tx_handle* pTx = NULL;
	SG_uint64 iBlobFullLength = 0;

	//////////////////////////////////////////////////////////////////
	// create temp-file-1 of length "lenFile" in the temp directory.

	VERIFY_ERR_CHECK_DISCARD(  SG_gid__alloc(pCtx, &pszidGidRandom1)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_gid__alloc(pCtx, &pszidGidRandom2)  );

	VERIFY_ERR_CHECK_DISCARD(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathnameTempFile1,pPathnameTempDir,(pszidGidRandom1))  );

	VERIFY_ERR_CHECK_DISCARD(  SG_file__open__pathname(pCtx, pPathnameTempFile1,SG_FILE_RDWR|SG_FILE_CREATE_NEW,0644,&pFileTempFile1)  );

	// write random gid at the beginning of the file
	// so that we won't get collisions if we are called
	// multiple times.

	VERIFY_ERR_CHECK_DISCARD(  SG_file__write(pCtx, pFileTempFile1,(SG_uint32)strlen(pszidGidRandom1),(SG_byte *)pszidGidRandom1,NULL)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_file__write(pCtx, pFileTempFile1,1,(SG_byte *)"\n",NULL)  );

	// generate lots of data in the file so that we'll cause the
	// blob routines to exercise the chunking stuff.

	lenSrc = (SG_uint32)strlen(szSrc);
	lenWritten = 0;
	while (lenWritten < lenFile)
	{
		VERIFY_ERR_CHECK_DISCARD(  SG_file__write(pCtx, pFileTempFile1,lenSrc,(SG_byte *)szSrc,NULL)  );

		lenWritten += lenSrc;
	}
	// the test file does NOT have a final LF.  i'm not sure it matters one way or the
	// other, but i'm just saying that we're not putting on a final LF.

	SG_ERR_IGNORE(  SG_file__seek(pCtx, pFileTempFile1,0)  );

	//////////////////////////////////////////////////////////////////
	// use currently open temp file to create a blob.
	// we get the HID back.  (we need to free it later.)

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_repo__store_blob_from_file(pCtx, pRepo,pTx,NULL,SG_FALSE,pFileTempFile1,&pszidHidBlob1,&iBlobFullLength)  );
	VERIFYP_COND("hash length",(strlen(pszidHidBlob1)==strlen_hashes),
				 ("HID[%s] Length[%d] ExpectedLength[%d]",pszidHidBlob1,strlen(pszidHidBlob1),strlen_hashes));
	VERIFY_ERR_CHECK_DISCARD(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );

	INFOP("create_blob_from_file",("Created blob [%s]",(pszidHidBlob1)));

	//////////////////////////////////////////////////////////////////
	// create empty temp-file-2 and try to read the blob from the repo.

	VERIFY_ERR_CHECK_DISCARD(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathnameTempFile2,pPathnameTempDir,(pszidGidRandom2))  );

	VERIFY_ERR_CHECK_DISCARD(  SG_file__open__pathname(pCtx, pPathnameTempFile2,SG_FILE_RDWR|SG_FILE_CREATE_NEW,0644,&pFileTempFile2)  );

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__fetch_blob_into_file(pCtx, pRepo,pszidHidBlob1,pFileTempFile2,NULL)  );

	//////////////////////////////////////////////////////////////////
	// verify that the contents of temp-file-2 is identical to the
	// contents of temp-file-1.  (we already know that the HIDs match
	// and was verified during the fetch, but there are times when the
	// HID is just being used as a key -- it doesn't mean that what we
	// actually restored is correct.

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__alloc_compute_hash__from_file(pCtx, pRepo, pFileTempFile1, &pszidHidVerify1)  );

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__alloc_compute_hash__from_file(pCtx, pRepo, pFileTempFile2, &pszidHidVerify2)  );

	bEqual = (0 == (strcmp(pszidHidVerify1,pszidHidVerify2)));
	VERIFY_COND("create_blob_from_file(verify v1==v2)",bEqual);

	bEqual = (0 == (strcmp(pszidHidVerify1,pszidHidBlob1)));
	VERIFY_COND("create_blob_from_file(verify v1==id)",bEqual);

	//////////////////////////////////////////////////////////////////
	// TODO delete temp source file

	SG_ERR_IGNORE(  SG_file__close(pCtx, &pFileTempFile1)  );
	SG_ERR_IGNORE(  SG_file__close(pCtx, &pFileTempFile2)  );

	//////////////////////////////////////////////////////////////////
	// cleanup

	SG_NULLFREE(pCtx, pszidGidRandom1);
	SG_NULLFREE(pCtx, pszidGidRandom2);
	SG_NULLFREE(pCtx, pszidHidBlob1);
	SG_NULLFREE(pCtx, pszidHidBlob1Dup);
	SG_NULLFREE(pCtx, pszidHidBlob2);
	SG_NULLFREE(pCtx, pszidHidVerify1);
	SG_NULLFREE(pCtx, pszidHidVerify2);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameTempFile1);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameTempFile2);
}

void MyFn(create_some_blobs_from_files)(SG_context * pCtx,
										SG_repo * pRepo,
										const SG_pathname * pPathnameTempDir)
{
	SG_uint64 k;
	char * pszHashMethod = NULL;
	char * pszRepoId = NULL;
	char * pszAdminId = NULL;
	SG_uint32 strlen_hashes;

    VERIFY_ERR_CHECK_DISCARD(  SG_repo__get_hash_method(pCtx, pRepo, &pszHashMethod)  );
    VERIFY_ERR_CHECK_DISCARD(  SG_repo__get_repo_id(pCtx, pRepo, &pszRepoId)  );
    VERIFY_ERR_CHECK_DISCARD(  SG_repo__get_admin_id(pCtx, pRepo, &pszAdminId)  );

	VERIFY_ERR_CHECK(  MyFn(get_hash_length)(pCtx, pRepo, &strlen_hashes)  );

	INFOP("create_some_blobs_from_files",("HashMethod[%s] RepoID[%s] AdminId[%s] HashLength[%d] TempDir[%s]",
										  pszHashMethod, pszRepoId, pszAdminId, strlen_hashes,
										  SG_pathname__sz(pPathnameTempDir)));

    SG_NULLFREE(pCtx, pszHashMethod);
    SG_NULLFREE(pCtx, pszRepoId);
    SG_NULLFREE(pCtx, pszAdminId);

	//////////////////////////////////////////////////////////////////
	// create a series of blobs of various known lengths and contents.

	for (k=1; k <= MyMaxFile; k+= MyStepFile)
	{
		SG_ERR_IGNORE(  MyFn(create_blob_from_file)(pCtx, strlen_hashes, pRepo,pPathnameTempDir,k,"Hello World!\nThis is line 2.\n")  );
		SG_ERR_IGNORE(  MyFn(create_blob_from_file)(pCtx, strlen_hashes, pRepo,pPathnameTempDir,k,"Welcome to the middle of the film!\n")  );
	}

	return;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

void MyFn(create_zero_byte_blob)(SG_context* pCtx,
								 SG_repo* pRepo,
								 const char * pszHid_KnownValueForZeroByteBlob)
{

	char* pszidHidBlob1 = NULL;
	char * pbuf1 = NULL;
	char * pbuf2 = NULL;
	SG_uint64 lenBuf2;
	SG_repo_tx_handle* pTx = NULL;
	SG_uint32 lenBuf1 = 0;

	pbuf1 = (char *)SG_calloc(1,lenBuf1+1);

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_repo__store_blob_from_memory(pCtx, pRepo,pTx,NULL,SG_FALSE,(SG_byte *)pbuf1,lenBuf1,&pszidHidBlob1)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );

	INFOP("create_zero_byte_blob",("Created blob [%s]",(pszidHidBlob1)));

	//////////////////////////////////////////////////////////////////
	// fetch blob into a new buffer and verify that it matches.

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__fetch_blob_into_memory(pCtx, pRepo,pszidHidBlob1,(SG_byte **)&pbuf2,&lenBuf2)  );
	VERIFY_COND("create_zero_byte_blob(fetch blob)",(lenBuf2 == (SG_uint64)lenBuf1));

	VERIFY_COND("create_zero_byte_blob(memcmp)",(memcmp(pbuf1,pbuf2,lenBuf1)==0));

	// The empty blob should always have this hid
	VERIFY_COND("zero byte blob hid mismatch",
				strcmp(pszidHidBlob1, pszHid_KnownValueForZeroByteBlob) == 0);

	//////////////////////////////////////////////////////////////////
	// cleanup

	SG_NULLFREE(pCtx, pbuf1);
	SG_NULLFREE(pCtx, pbuf2);
	SG_NULLFREE(pCtx, pszidHidBlob1);

}

//////////////////////////////////////////////////////////////////

void MyFn(test_repo_with_hash_method)(SG_context * pCtx, const char * pszHashMethod, const char * pszHid_KnownValueForZeroByteBlob)
{
	SG_repo * pRepo = NULL;
	SG_pathname * pPathnameTempDir = NULL;

	VERIFY_ERR_CHECK(  MyFn(create_repo)(pCtx,pszHashMethod,&pRepo)  );
	VERIFY_ERR_CHECK(  MyFn(create_tmp_src_dir)(pCtx,&pPathnameTempDir)  );

	VERIFY_ERR_CHECK(  MyFn(create_some_blobs_from_bytes)(pCtx, pRepo)  );
	VERIFY_ERR_CHECK(  MyFn(create_some_blobs_from_files)(pCtx, pRepo,pPathnameTempDir)  );
	VERIFY_ERR_CHECK(  MyFn(create_zero_byte_blob)(pCtx, pRepo, pszHid_KnownValueForZeroByteBlob)  );

	//////////////////////////////////////////////////////////////////
	// TODO delete repo directory and everything we created under it.
	// TODO delete temp directory and everything we created under it.

fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameTempDir);
}

//////////////////////////////////////////////////////////////////

MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(test_repo_with_hash_method)(pCtx, "SHA1/160", "da39a3ee5e6b4b0d3255bfef95601890afd80709")  );

	BEGIN_TEST(  MyFn(test_repo_with_hash_method)(pCtx, "SHA2/256", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")  );
	BEGIN_TEST(  MyFn(test_repo_with_hash_method)(pCtx, "SHA2/384", "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b")  );
	BEGIN_TEST(  MyFn(test_repo_with_hash_method)(pCtx, "SHA2/512", "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e")  );

	BEGIN_TEST(  MyFn(test_repo_with_hash_method)(pCtx, "SKEIN/256",  "0b04103b828cddaebcf592ac845ecafd5887f61230a755406d38d85376e1ae08")  );
	BEGIN_TEST(  MyFn(test_repo_with_hash_method)(pCtx, "SKEIN/512",  "5af68a4912e0a6187a004947a9d2a37d7a1f0873f0bdd9dc64838ece60da5535c2a55d039bd58e178948996b7a8336486ed969c894be658e47d595a5a9b86a8b")  );
	BEGIN_TEST(  MyFn(test_repo_with_hash_method)(pCtx, "SKEIN/1024", "5c88c7faeed294c36d955dd01ece99ec852bb8738a499743d8d93e64dc00ed3e0b3ee42774172e10ba2109634771e5c4443b651d6755e73937c0f57f6259dc0a2fd66048718c2cc65e789ea7fdf32baf0fc63509cf448faebe7aea765b51ee5f2852e5d244d7c0211fd1a17f5ca50f94d500e6eeec6f88d93637484420a2d1fe")  );

	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn

#undef MyMaxFile
#undef MyStepFile
