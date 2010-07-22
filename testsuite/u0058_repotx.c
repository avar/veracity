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

//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0058_repotx)
#define MyDcl(name)				u0058_repotx__##name
#define MyFn(name)				u0058_repotx__##name

void MyFn(create_repo)(SG_context * pCtx, SG_repo** ppRepo)
{
	SG_repo* pRepo = NULL;
	SG_pathname* pPath_repo = NULL;
	char buf_repo_id[SG_GID_BUFFER_LENGTH];
	char buf_admin_id[SG_GID_BUFFER_LENGTH];
	SG_vhash* pvhPartialDescriptor = NULL;
	char* pszRepoImpl = NULL;

	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_repo_id, sizeof(buf_repo_id))  );
	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_admin_id, sizeof(buf_admin_id))  );

	/* Get our paths fixed up */
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath_repo)  );
	VERIFY_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath_repo)  );
	VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath_repo, "repo")  );

	SG_fsobj__mkdir__pathname(pCtx, pPath_repo);
	SG_context__err_reset(pCtx);

	// Create the repo
	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhPartialDescriptor)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__NEWREPO_DRIVER, NULL, &pszRepoImpl, NULL)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_KEY__STORAGE, pszRepoImpl)  );

	VERIFY_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_FSLOCAL__PATH_PARENT_DIR, SG_pathname__sz(pPath_repo))  );
	VERIFY_ERR_CHECK(  SG_repo__create_repo_instance(pCtx,pvhPartialDescriptor,SG_TRUE,NULL,buf_repo_id,buf_admin_id,&pRepo)  );

	*ppRepo = pRepo;

	// Fall through to common cleanup

fail:
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPath_repo);

    SG_NULLFREE(pCtx, pszRepoImpl);
}

void MyFn(alloc_random_buffer)(SG_context * pCtx, SG_byte** ppBuf, SG_uint32* lenBuf)
{
	char  bufGidRandom[SG_GID_BUFFER_LENGTH];
	SG_byte* pBuf = NULL;
	const char* pszTest =
		"Hail! to the victors valiant\n"
		"Hail! to the conqu'ring heroes\n"
		"Hail! Hail! to Michigan\n"
		"The leaders and best!\n"
		"Hail! to the victors valiant\n"
		"Hail! to the conqu'ring heroes\n"
		"Hail! Hail! to Michigan,\n"
		"The champions of the West!\n";
	SG_uint32 lenTest = strlen(pszTest);
	SG_uint32 lenTotal = lenTest + SG_GID_BUFFER_LENGTH;

	// Add some random data so we don't accidentally add duplicate blobs.
	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, bufGidRandom, sizeof(bufGidRandom))  );
	VERIFY_ERR_CHECK(  SG_alloc(pCtx, 1, lenTotal, &pBuf)  );
	memcpy(pBuf, pszTest, lenTest);
	memcpy(pBuf + lenTest, bufGidRandom, sizeof(bufGidRandom));

	*ppBuf = pBuf;
	*lenBuf = lenTotal;

	return;

fail:
	SG_NULLFREE(pCtx, pBuf);
}

void MyFn(no_repo_tx)(SG_context * pCtx, SG_repo* pRepo)
{
	SG_repo_tx_handle* pTx = NULL;
	SG_repo_store_blob_handle* pbh;
    SG_bool b_supports_change_blob_encoding = SG_FALSE;

	SG_byte* pBufIn = NULL;
	SG_uint32 lenBufIn = 0;
	char* pszHidReturned = NULL;

	char* pId = NULL;
	SG_dagnode* pdnCreated = NULL;

	char buf_tid[SG_TID_MAX_BUFFER_LENGTH];

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__commit_tx(pCtx, pRepo, NULL),
										  SG_ERR_INVALIDARG  );		// commit_tx without repo tx didn't fail with INVALIDARG.

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__abort_tx(pCtx, pRepo, NULL),
										  SG_ERR_INVALIDARG  );		// abort_tx without repo tx didn't fail with INVALIDARG.

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__store_blob__begin(pCtx, pRepo, NULL, NULL, SG_BLOBENCODING__FULL, NULL, 10, 0, NULL, &pbh),
										  SG_ERR_INVALIDARG  );		// store_blob__begin without repo tx didn't fail with INVALIDARG.

	// Create a repo tx so we can test the other blob functions.
	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  MyFn(alloc_random_buffer)(pCtx, &pBufIn, &lenBufIn)  );

	VERIFY_ERR_CHECK(  SG_repo__store_blob__begin(pCtx, pRepo, pTx, NULL, SG_BLOBENCODING__FULL, NULL, lenBufIn, 0, NULL, &pbh)  );
	VERIFY_ERR_CHECK(  SG_repo__store_blob__chunk(pCtx, pRepo, pbh, lenBufIn, pBufIn, NULL)  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__store_blob__end(pCtx, pRepo, NULL, &pbh, &pszHidReturned),
										  SG_ERR_INVALIDARG  );		// store_blob__end without repo tx didn't fail with INVALIDARG.

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__store_blob__abort(pCtx, pRepo, NULL, &pbh),
										  SG_ERR_INVALIDARG  );		// store_blob__abort without repo tx didn't fail with INVALIDARG.

	VERIFY_ERR_CHECK(  SG_repo__store_blob__end(pCtx, pRepo, pTx, &pbh, &pszHidReturned)  );
	VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );

    VERIFY_ERR_CHECK(  SG_repo__query_implementation(pCtx, pRepo, SG_REPO__QUESTION__BOOL__SUPPORTS_CHANGE_BLOB_ENCODING, &b_supports_change_blob_encoding, NULL, NULL, 0, NULL)  );

    if (b_supports_change_blob_encoding)
    {
        VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__change_blob_encoding(pCtx, pRepo, NULL, pszHidReturned, SG_BLOBENCODING__FULL, NULL,
                                                                            NULL, NULL, NULL, NULL),
                                              SG_ERR_INVALIDARG  );		// change_blob_encoding without repo tx didn't fail with INVALIDARG.
    }

	// create a TID just to get some random data.  use it to create a HID.
	// use the HID as the HID of a hypothetical changeset so that we can create the dagnode.

	VERIFY_ERR_CHECK(  SG_tid__generate(pCtx, buf_tid, sizeof(buf_tid))  );
	VERIFY_ERR_CHECK(  SG_repo__alloc_compute_hash__from_bytes(pCtx,
															   pRepo,
															   sizeof(buf_tid),
															   (SG_byte *)buf_tid,
															   &pId)  );

	VERIFY_ERR_CHECK(  SG_dagnode__alloc(pCtx, &pdnCreated, pId, 1)  );
	VERIFY_ERR_CHECK(  SG_dagnode__freeze(pCtx, pdnCreated)  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__store_dagnode(pCtx, pRepo, NULL, SG_DAGNUM__TESTING__NOTHING, pdnCreated),
										  SG_ERR_INVALIDARG  );		// store_dagnode without repo tx didn't fail with INVALIDARG.

	// We're intentionally not testing the higher-level store_blob_from_memory and store_blob_from_file
	// routines here because they're just wrappers for the begin/chunk/end routines we do test.

	// Fall through to common cleanup.

fail:
	SG_NULLFREE(pCtx, pszHidReturned);
	SG_NULLFREE(pCtx, pBufIn);
	SG_NULLFREE(pCtx, pId);
	SG_DAGNODE_NULLFREE(pCtx, pdnCreated);
}

// Store a blob.  Make sure it doesn't show up until/unless the repo tx is committed.
void MyFn(one_blob)(SG_context * pCtx, SG_repo* pRepo)
{
	SG_byte* pBufIn = NULL;
	SG_uint32 lenBufIn = 0;

	SG_byte* pBufOut = NULL;
	SG_uint64 lenBufOut = 0;

	SG_repo_tx_handle* pTx;
	char* pszHidReturned = NULL;

	VERIFY_ERR_CHECK(  MyFn(alloc_random_buffer)(pCtx, &pBufIn, &lenBufIn)  );

	// Start writing blob.
	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  SG_repo__store_blob_from_memory(pCtx, pRepo, pTx, NULL, SG_FALSE, pBufIn, lenBufIn, &pszHidReturned)  );

	// Should fail: tx not committed.
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__fetch_blob_into_memory(pCtx, pRepo, pszHidReturned, &pBufOut, &lenBufOut),
										  SG_ERR_BLOB_NOT_FOUND  );			// Blob visible before repo tx committed
	SG_NULLFREE(pCtx, pBufOut);

	// Abort repo tx.
	VERIFY_ERR_CHECK(  SG_repo__abort_tx(pCtx, pRepo, &pTx)  );
	VERIFY_COND("SG_repo__abort_tx should null/free the repo transaction.", !pTx);

	// Should fail: tx aborted.
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__fetch_blob_into_memory(pCtx, pRepo, pszHidReturned, &pBufOut, &lenBufOut),
										  SG_ERR_BLOB_NOT_FOUND  );			// Blob exists after repo tx abort
	SG_NULLFREE(pCtx, pBufOut);

	SG_NULLFREE(pCtx, pszHidReturned);

	// Write blob, commit tx.
	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  SG_repo__store_blob_from_memory(pCtx, pRepo, pTx, NULL, SG_FALSE, pBufIn, lenBufIn, &pszHidReturned)  );
	VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );
	VERIFY_COND("SG_repo__commit_tx should null/free the repo transaction.", !pTx);

	// Read back the blob.  It should exist now.
	VERIFY_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pRepo, pszHidReturned, &pBufOut, &lenBufOut)  );
	// Just verify the length.  It's another test's job to roundtrip blobs and verify data.
	VERIFY_COND(  "blob length mismatch", lenBufOut == lenBufIn  );

	 // Fall through to common cleanup.

fail:
	SG_NULLFREE(pCtx, pszHidReturned);
	SG_NULLFREE(pCtx, pBufIn);
	SG_NULLFREE(pCtx, pBufOut);
}

// Abort a repo tx while a blob is being stored.
//
// We jump through some awkward hoops to clean up memory in this case.  If it becomes more trouble than
// it's worth, this test might not be worth running.
void MyFn(abort_mid_blob)(SG_context * pCtx, SG_repo* pRepo)
{
	SG_uint32 iCountFullBlobsBefore;
	SG_uint32 iCountZlibBlobsBefore;
	SG_uint32 iCountVcdBlobsBefore;
	SG_uint32 iCountFullBlobsAfter;
	SG_uint32 iCountZlibBlobsAfter;
	SG_uint32 iCountVcdBlobsAfter;

	SG_byte* pRandomBuf = NULL;
	SG_uint32 lenRandomBuf;
	SG_uint64 lenTotal = 0;
	SG_repo_tx_handle* pTx = NULL;
	SG_repo_store_blob_handle* pbh;
	SG_uint32 i;
	SG_uint32 iLenWritten = 0;

	VERIFY_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo, &iCountFullBlobsBefore, NULL, &iCountZlibBlobsBefore, &iCountVcdBlobsBefore,
		NULL, NULL, NULL, NULL, NULL, NULL)  );

	VERIFY_ERR_CHECK(  MyFn(alloc_random_buffer)(pCtx, &pRandomBuf, &lenRandomBuf)  );
	lenTotal = lenRandomBuf * 5;

	// Start writing blob.
	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  SG_repo__store_blob__begin(pCtx, pRepo, pTx, NULL, SG_BLOBENCODING__FULL, NULL, lenTotal, 0, NULL, &pbh)  );

	for (i=0; i < 3; i++)
	{
		VERIFY_ERR_CHECK(  SG_repo__store_blob__chunk(pCtx, pRepo, pbh, lenRandomBuf, pRandomBuf, &iLenWritten)  );
		// This chunk is much smaller than SG_STREAMING_BUFFER_SIZE, so the whole thing should be written.
		VERIFY_COND("SG_repo__store_blob__chunk length mismatch.", iLenWritten == lenRandomBuf);
	}

	VERIFY_ERR_CHECK(  SG_repo__abort_tx(pCtx, pRepo, &pTx)  );
	SG_NULLFREE(pCtx, pRandomBuf);

	VERIFY_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo, &iCountFullBlobsAfter, NULL, &iCountZlibBlobsAfter, &iCountVcdBlobsAfter,
		NULL, NULL, NULL, NULL, NULL, NULL)  );

	VERIFY_COND("Full blob count mismatch", iCountFullBlobsBefore == iCountFullBlobsAfter);
	VERIFY_COND("Zlib blob count mismatch", iCountZlibBlobsBefore == iCountZlibBlobsAfter);
	VERIFY_COND("VCDiff blob count mismatch", iCountVcdBlobsBefore == iCountVcdBlobsAfter);

	return;

fail:
	SG_NULLFREE(pCtx, pRandomBuf);
}

// Commit a repo tx while a blob is being stored.
//
// We jump through some awkward hoops to clean up memory in this case.  If it becomes more trouble than
// it's worth, this test might not be worth running.
void MyFn(commit_mid_blob)(SG_context * pCtx, SG_repo* pRepo)
{
	SG_uint32 iCountFullBlobsBefore;
	SG_uint32 iCountZlibBlobsBefore;
	SG_uint32 iCountVcdBlobsBefore;
	SG_uint32 iCountFullBlobsAfter;
	SG_uint32 iCountZlibBlobsAfter;
	SG_uint32 iCountVcdBlobsAfter;

	SG_byte* pRandomBuf = NULL;
	SG_uint32 lenRandomBuf;
	SG_uint64 lenTotal = 0;
	SG_repo_tx_handle* pTx = NULL;
	SG_repo_store_blob_handle* pbh;
	SG_uint32 i;
	SG_uint32 iLenWritten = 0;

	VERIFY_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo, &iCountFullBlobsBefore, NULL, &iCountZlibBlobsBefore, &iCountVcdBlobsBefore,
		NULL, NULL, NULL, NULL, NULL, NULL)  );

	VERIFY_ERR_CHECK(  MyFn(alloc_random_buffer)(pCtx, &pRandomBuf, &lenRandomBuf)  );
	lenTotal = lenRandomBuf * 5;

	// Start writing blob.
	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  SG_repo__store_blob__begin(pCtx, pRepo, pTx, NULL, SG_BLOBENCODING__FULL, NULL, lenTotal, 0, NULL, &pbh)  );

	for (i=0; i < 3; i++)
	{
		VERIFY_ERR_CHECK(  SG_repo__store_blob__chunk(pCtx, pRepo, pbh, lenRandomBuf, pRandomBuf, &iLenWritten)  );
		// This chunk is much smaller than SG_STREAMING_BUFFER_SIZE, so the whole thing should be written.
		VERIFY_COND("SG_repo__store_blob__chunk length mismatch.", iLenWritten == lenRandomBuf);
	}

	// We're not done yet, so this should fail.
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__commit_tx(pCtx, pRepo, &pTx),
										  SG_ERR_INCOMPLETE_BLOB_IN_REPO_TX  );	// SG_repo__commit_tx should return SG_ERR_INCOMPLETE_BLOB_IN_REPO_TX
	VERIFY_COND("SG_repo__commit_tx should free the repo transaction.", !pTx);

	SG_NULLFREE(pCtx, pRandomBuf);

	VERIFY_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo, &iCountFullBlobsAfter, NULL, &iCountZlibBlobsAfter, &iCountVcdBlobsAfter,
		NULL, NULL, NULL, NULL, NULL, NULL)  );

	VERIFY_COND("Full blob count mismatch", iCountFullBlobsBefore == iCountFullBlobsAfter);
	VERIFY_COND("Zlib blob count mismatch", iCountZlibBlobsBefore == iCountZlibBlobsAfter);
	VERIFY_COND("VCDiff blob count mismatch", iCountVcdBlobsBefore == iCountVcdBlobsAfter);

	return;

fail:
	SG_NULLFREE(pCtx, pRandomBuf);
}

// Create 8 blobs, commit those whose bit is set in blobMask, abort the rest.  Verify results.
void MyFn(eight_blobs_commit_masked)(SG_context * pCtx, SG_repo* pRepo, SG_uint8 blobMask)
{
	SG_uint32 iCountFullBlobsBefore;
	SG_uint32 iCountZlibBlobsBefore;
	SG_uint32 iCountVcdBlobsBefore;
	SG_uint32 iCountFullBlobsAfter;
	SG_uint32 iCountZlibBlobsAfter;
	SG_uint32 iCountVcdBlobsAfter;

	SG_byte* pRandomBuf = NULL;
	SG_uint32 lenRandomBuf;
	SG_uint64 lenTotal = 0;
	SG_repo_tx_handle* pTx = NULL;
	SG_repo_store_blob_handle* pbh;
	SG_uint32 i,j;
	SG_uint32 iLenWritten = 0;
	char* apszHids[8];

	SG_uint8 countBlobsToAdd;
	SG_uint8 mask = blobMask;

	// Count the number of bits set in blobMask.
	for (countBlobsToAdd = 0; mask; mask >>= 1)
		countBlobsToAdd += mask & 1;

	VERIFY_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo, &iCountFullBlobsBefore, NULL, &iCountZlibBlobsBefore, &iCountVcdBlobsBefore,
		NULL, NULL, NULL, NULL, NULL, NULL)  );

	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );

	for (i=0; i < 8; i++)
	{
		VERIFY_ERR_CHECK(  MyFn(alloc_random_buffer)(pCtx, &pRandomBuf, &lenRandomBuf)  );
		lenTotal = lenRandomBuf * 3;

		VERIFY_ERR_CHECK(  SG_repo__store_blob__begin(pCtx, pRepo, pTx, NULL, SG_BLOBENCODING__FULL, NULL, lenTotal, 0, NULL, &pbh)  );
		for (j=0; j < 3; j++)
		{
			VERIFY_ERR_CHECK(  SG_repo__store_blob__chunk(pCtx, pRepo, pbh, lenRandomBuf, pRandomBuf, &iLenWritten)  );
			// This chunk is much smaller than SG_STREAMING_BUFFER_SIZE, so the whole thing should be written.
			VERIFY_COND("SG_repo__store_blob__chunk length mismatch.", iLenWritten == lenRandomBuf);

			if ((1 == j) && ((1 << i & blobMask) == 0)) // we're mid-blob and blob is to be aborted (bit is unset)
				break;
		}
		if ((1 << i & blobMask) != 0) // blob is to be commit (bit is set)
		{
			// Finish the blob.
			VERIFY_ERR_CHECK(  SG_repo__store_blob__end(pCtx, pRepo, pTx, &pbh, &(apszHids[i]))  );
		}
		else
		{
			// Abort the blob.
			VERIFY_ERR_CHECK(  SG_repo__store_blob__abort(pCtx, pRepo, pTx, &pbh)  );
			apszHids[i] = NULL;
		}
		SG_NULLFREE(pCtx, pRandomBuf);
	}

	VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );

	VERIFY_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo, &iCountFullBlobsAfter, NULL, &iCountZlibBlobsAfter, &iCountVcdBlobsAfter,
		NULL, NULL, NULL, NULL, NULL, NULL)  );

	VERIFY_COND("blob count mismatch", iCountFullBlobsBefore + iCountZlibBlobsBefore + iCountVcdBlobsBefore + countBlobsToAdd
		== iCountFullBlobsAfter + iCountZlibBlobsAfter + iCountVcdBlobsAfter);

	// Verify HIDs we think we added, were.
	for (i=0; i < 8; i++)
	{
		if (apszHids[i])
		{
			VERIFY_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pRepo, apszHids[i], &pRandomBuf, NULL)  );
			SG_NULLFREE(pCtx, pRandomBuf);
		}
	}

	// Fall through to common cleanup.

fail:
	SG_NULLFREE(pCtx, pRandomBuf);
	for (i=0; i < 8; i++)
		SG_NULLFREE(pCtx, apszHids[i]);
}

// You can only store one blob at a time.
void MyFn(multiple_stores_fail)(SG_context * pCtx, SG_repo* pRepo)
{
	SG_uint32 iCountFullBlobsBefore;
	SG_uint32 iCountZlibBlobsBefore;
	SG_uint32 iCountVcdBlobsBefore;
	SG_uint32 iCountFullBlobsAfter;
	SG_uint32 iCountZlibBlobsAfter;
	SG_uint32 iCountVcdBlobsAfter;

	SG_repo_tx_handle* pTx = NULL;
	SG_repo_store_blob_handle* pbh;
	SG_repo_store_blob_handle* pbh2;

	VERIFY_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo, &iCountFullBlobsBefore, NULL, &iCountZlibBlobsBefore, &iCountVcdBlobsBefore,
		NULL, NULL, NULL, NULL, NULL, NULL)  );

	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );

	// Start writing a blob.
	VERIFY_ERR_CHECK(  SG_repo__store_blob__begin(pCtx, pRepo, pTx, NULL, SG_BLOBENCODING__FULL, NULL, 500, 0, NULL, &pbh)  );

	// Start another blob.  Should fail.
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__store_blob__begin(pCtx, pRepo, pTx, NULL, SG_BLOBENCODING__FULL, NULL, 500, 0, NULL, &pbh2),
										  SG_ERR_INCOMPLETE_BLOB_IN_REPO_TX  ); // SG_repo__store_blob__begin didn't fail when previous blob wasn't done.

	VERIFY_ERR_CHECK(  SG_repo__abort_tx(pCtx, pRepo, &pTx)  );

	VERIFY_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo, &iCountFullBlobsAfter, NULL, &iCountZlibBlobsAfter, &iCountVcdBlobsAfter,
		NULL, NULL, NULL, NULL, NULL, NULL)  );

	VERIFY_COND("Full blob count mismatch", iCountFullBlobsBefore == iCountFullBlobsAfter);
	VERIFY_COND("Zlib blob count mismatch", iCountZlibBlobsBefore == iCountZlibBlobsAfter);
	VERIFY_COND("VCDiff blob count mismatch", iCountVcdBlobsBefore == iCountVcdBlobsAfter);

	return;

fail:
	return;
}

void MyFn(empty_tx)(SG_context * pCtx, SG_repo* pRepo)
{
	SG_uint32 iCountFullBlobsBefore;
	SG_uint32 iCountZlibBlobsBefore;
	SG_uint32 iCountVcdBlobsBefore;
	SG_uint32 iCountFullBlobsAfter;
	SG_uint32 iCountZlibBlobsAfter;
	SG_uint32 iCountVcdBlobsAfter;

	SG_repo_tx_handle* pTx = NULL;

	VERIFY_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo, &iCountFullBlobsBefore, NULL, &iCountZlibBlobsBefore, &iCountVcdBlobsBefore,
		NULL, NULL, NULL, NULL, NULL, NULL)  );

	// Commit empty tx.
	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );

	VERIFY_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo, &iCountFullBlobsAfter, NULL, &iCountZlibBlobsAfter, &iCountVcdBlobsAfter,
		NULL, NULL, NULL, NULL, NULL, NULL)  );

	VERIFY_COND("Full blob count mismatch", iCountFullBlobsBefore == iCountFullBlobsAfter);
	VERIFY_COND("Zlib blob count mismatch", iCountZlibBlobsBefore == iCountZlibBlobsAfter);
	VERIFY_COND("VCDiff blob count mismatch", iCountVcdBlobsBefore == iCountVcdBlobsAfter);

	// Abort empty tx.
	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  SG_repo__abort_tx(pCtx, pRepo, &pTx)  );

	VERIFY_ERR_CHECK(  SG_repo__get_blob_stats(pCtx, pRepo, &iCountFullBlobsAfter, NULL, &iCountZlibBlobsAfter, &iCountVcdBlobsAfter,
		NULL, NULL, NULL, NULL, NULL, NULL)  );

	VERIFY_COND("Full blob count mismatch", iCountFullBlobsBefore == iCountFullBlobsAfter);
	VERIFY_COND("Zlib blob count mismatch", iCountZlibBlobsBefore == iCountZlibBlobsAfter);
	VERIFY_COND("VCDiff blob count mismatch", iCountVcdBlobsBefore == iCountVcdBlobsAfter);

	return;

fail:
	return;
}

void MyFn(one_dagnode)(SG_context * pCtx, SG_repo* pRepo)
{
	char* pId = NULL;
	SG_dagnode* pdnCreated = NULL;
	SG_dagnode* pdnFetched = NULL;
	SG_repo_tx_handle* pTx = NULL;
	char buf_tid[SG_TID_MAX_BUFFER_LENGTH];

	VERIFY_ERR_CHECK(  SG_tid__generate(pCtx, buf_tid, sizeof(buf_tid))  );
	VERIFY_ERR_CHECK(  SG_repo__alloc_compute_hash__from_bytes(pCtx,
															   pRepo,
															   sizeof(buf_tid),
															   (SG_byte *)buf_tid,
															   &pId)  );

	VERIFY_ERR_CHECK(  SG_dagnode__alloc(pCtx, &pdnCreated, pId, 1)  );
	VERIFY_ERR_CHECK(  SG_dagnode__freeze(pCtx, pdnCreated)  );

	// Add dagnode.
	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  SG_repo__store_dagnode(pCtx, pRepo, pTx, SG_DAGNUM__TESTING__NOTHING, pdnCreated)  );
    pdnCreated = NULL;

	// Should fail: tx not committed.
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__fetch_dagnode(pCtx, pRepo, pId, &pdnFetched),
										  SG_ERR_NOT_FOUND  );	// Dag node visible before repo tx committed.

	// Abort repo tx.
	VERIFY_ERR_CHECK(  SG_repo__abort_tx(pCtx, pRepo, &pTx)  );
	VERIFY_COND("SG_repo__abort_tx should null/free the repo transaction.", !pTx);

	// Should fail: tx aborted.
	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_repo__fetch_dagnode(pCtx, pRepo, pId, &pdnFetched),
										  SG_ERR_NOT_FOUND  ); // Dag node exists after repo tx abort

	// Write dagnode, commit tx.
	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  SG_dagnode__alloc(pCtx, &pdnCreated, pId, 1)  );
	VERIFY_ERR_CHECK(  SG_dagnode__freeze(pCtx, pdnCreated)  );
	VERIFY_ERR_CHECK(  SG_repo__store_dagnode(pCtx, pRepo, pTx, SG_DAGNUM__TESTING__NOTHING, pdnCreated)  );
    pdnCreated = NULL;
	VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );
	VERIFY_COND("SG_repo__commit_tx should null/free the repo transaction.", !pTx);

	// Read back the dagnode.  It should exist now.
	VERIFY_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, pId, &pdnFetched)  );

	// Fall through to common cleanup.

fail:
	SG_NULLFREE(pCtx, pId);
	SG_DAGNODE_NULLFREE(pCtx, pdnCreated);
	SG_DAGNODE_NULLFREE(pCtx, pdnFetched);
}

void MyFn(do_tests)(SG_context * pCtx)
{
	SG_repo* pRepo = NULL;
	SG_uint8 blobMask;

	VERIFY_ERR_CHECK(  MyFn(create_repo)(pCtx, &pRepo)  );
	VERIFY_ERR_CHECK(  MyFn(no_repo_tx)(pCtx, pRepo)  );
	VERIFY_ERR_CHECK(  MyFn(one_blob)(pCtx, pRepo)  );
	VERIFY_ERR_CHECK(  MyFn(abort_mid_blob)(pCtx, pRepo)  );
	VERIFY_ERR_CHECK(  MyFn(commit_mid_blob)(pCtx, pRepo)  );

	// Abort nothing.
	blobMask = 0xFF;
	VERIFY_ERR_CHECK(  MyFn(eight_blobs_commit_masked)(pCtx, pRepo, blobMask)  );
	// Abort the odd blobs.
	blobMask = 0xAA;
	VERIFY_ERR_CHECK(  MyFn(eight_blobs_commit_masked)(pCtx, pRepo, blobMask)  );
	// Abort the even blobs.
	blobMask = (~blobMask);
	VERIFY_ERR_CHECK(  MyFn(eight_blobs_commit_masked)(pCtx, pRepo, blobMask)  );
	// Abort the first 4 blobs.
	blobMask = 0x0F;
	VERIFY_ERR_CHECK(  MyFn(eight_blobs_commit_masked)(pCtx, pRepo, blobMask)  );
	// Abort the last 4 blobs.
	blobMask = 0xF0;
	VERIFY_ERR_CHECK(  MyFn(eight_blobs_commit_masked)(pCtx, pRepo, blobMask)  );
	// Abort all the blobs.
	blobMask = 0x0;
	VERIFY_ERR_CHECK(  MyFn(eight_blobs_commit_masked)(pCtx, pRepo, blobMask)  );

	VERIFY_ERR_CHECK(  MyFn(multiple_stores_fail)(pCtx, pRepo)  );
	VERIFY_ERR_CHECK(  MyFn(empty_tx)(pCtx, pRepo)  );

	VERIFY_ERR_CHECK(  MyFn(one_dagnode)(pCtx, pRepo)  );

	// fall thru

fail:
	SG_repo__free(pCtx, pRepo);
}

MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(do_tests)(pCtx)  );

	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn
