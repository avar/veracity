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
* @file sg_blob_sqlite__io.c
*
*/

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include <zlib.h>
#include "sg_repo_sqlite__private.h"

//////////////////////////////////////////////////////////////////
static void _blob_seekreader_nullfree(SG_context * pCtx, blob_seekreader** ppsr)
{
	if (!ppsr || !(*ppsr))
		return;

	SG_ERR_IGNORE(  SG_seekreader__close(pCtx, (*ppsr)->pSeekreader)  );
	SG_ERR_IGNORE(  SG_tempfile__delete(pCtx, &((*ppsr)->pTempfile))  );

	SG_NULLFREE(pCtx, *ppsr);
}

void sg_blob_sqlite_handle_fetch__free(SG_context * pCtx, sg_blob_sqlite_handle_fetch* pbh)
{
	if (!pbh)
		return;

	inflateEnd(&pbh->zStream);

	sg_blob_sqlite_blobstream_free(pCtx, pbh->pBlobStream);

	SG_NULLFREE(pCtx, pbh->psz_hid_vcdiff_reference_stored);

	if (pbh->pRHH_VerifyOnFetch)
	{
		// if this fetch-handle is being freed with an active repo_hash_handle, we destory it too.
		// if everything went as expected, our caller should have finalized the hash (called __end)
		// and gotten the computed HID (and already freed the repo_hash_handle).  so if we still have
		// one, we must assume that there was a problem and call __abort on it.

		SG_ERR_IGNORE(  sg_repo__sqlite__hash__abort(pCtx, pbh->pData->pRepo, &pbh->pRHH_VerifyOnFetch)  );
	}

	SG_NULLFREE(pCtx, pbh);
}

void sg_blob_sqlite_handle_store__free_ex(SG_context * pCtx,
										  sg_blob_sqlite_handle_store* pbh,
										  SG_int64** ppaRowIDs,
										  SG_uint32* piCount)
{
	if (!pbh)
		return;

	sg_blob_sqlite_blobstream_free_ex(pCtx, pbh->pBlobStream, ppaRowIDs, piCount);
	deflateEnd(&pbh->zStream);

	if (pbh->pBufTemp)
		SG_ERR_IGNORE(  SG_NULLFREE(pCtx, pbh->pBufTemp)  );

	if (pbh->pTempFile)
	{
		SG_ERR_IGNORE(  SG_tempfile__close_and_delete(pCtx, &pbh->pTempFile)  );
	}

	if (pbh->pRHH_ComputeOnStore)
	{
		// if this handle is being freed with an active repo_hash_handle, we destroy it too.
		// if everything went as expected, our caller should have finalized the hash and
		// gotten the computed HID and already freed the repo_hash_handle.
		SG_ERR_IGNORE(  sg_repo__sqlite__hash__abort(pCtx, pbh->pData->pRepo, &pbh->pRHH_ComputeOnStore)  );
	}

	SG_NULLFREE(pCtx, pbh);
}
void sg_blob_sqlite_handle_store__free(SG_context * pCtx, sg_blob_sqlite_handle_store* pbh)
{
	sg_blob_sqlite_handle_store__free_ex(pCtx, pbh, NULL, NULL);
}

static void _blob_handle__init_digest(SG_context* pCtx, sg_blob_sqlite_handle_fetch* pBlobHandle)
{
	// If they want us to verify the HID using the actual contents of the Raw
	// Data that we stored in the BLOBFILE (after we uncompress/undeltafy/etc it),
	// we start a digest now.

	SG_ERR_CHECK_RETURN(  sg_repo__sqlite__hash__begin(pCtx, pBlobHandle->pData->pRepo, &pBlobHandle->pRHH_VerifyOnFetch)  );
}

static void _blob_handle__verify_lengths_and_digest(SG_context* pCtx, sg_blob_sqlite_handle_fetch* pfh)
{
	// verify that the digest that we computed as we read the contents
	// of the blob matches the HID that we think it should have.

	SG_NULLARGCHECK_RETURN(pfh);

	if (pfh->len_full_stored != 0 && pfh->len_encoded_observed != pfh->len_encoded_stored)
		SG_ERR_THROW_RETURN(SG_ERR_BLOB_NOT_VERIFIED_INCOMPLETE);

	if (SG_IS_BLOBENCODING_FULL(pfh->blob_encoding_returning))
	{
		if (pfh->len_full_observed != pfh->len_full_stored)
		{
			SG_ERR_THROW_RETURN(SG_ERR_BLOB_NOT_VERIFIED_INCOMPLETE);
		}
	}

	if (pfh->pRHH_VerifyOnFetch)
	{
		char * pszHid_Computed = NULL;
		SG_bool bNoMatch;

		SG_ERR_CHECK_RETURN(  sg_repo__sqlite__hash__end(pCtx, pfh->pData->pRepo, &pfh->pRHH_VerifyOnFetch, &pszHid_Computed)  );
		bNoMatch = (0 != strcmp(pszHid_Computed, pfh->sz_hid_requested));
		SG_NULLFREE(pCtx, pszHid_Computed);

		if (bNoMatch)
		{
			SG_ERR_THROW_RETURN(SG_ERR_BLOB_NOT_VERIFIED_MISMATCH);
		}
	}
}

static void _setup_undeltify(SG_context* pCtx, sg_blob_sqlite_handle_fetch* pfh)
{
	pfh->b_undeltifying = SG_TRUE;

	// Get seekreader on reference blob.
	SG_ERR_CHECK_RETURN(  sg_blob_sqlite__seekreader_open(pCtx, pfh->pData, pfh->pBlobStream->psql,
		pfh->psz_hid_vcdiff_reference_stored, &pfh->psr_reference)  );

	// Get readstream on delta blob.
	SG_ERR_CHECK_RETURN(  sg_blob_sqlite__readstream_open(pCtx, pfh->pBlobStream->psql, pfh->sz_hid_requested,
		&pfh->pstrm_delta)  );

	SG_ERR_CHECK_RETURN(  SG_vcdiff__undeltify__begin(pCtx, pfh->psr_reference->pSeekreader, pfh->pstrm_delta, &pfh->pst)  );

	pfh->p_undeltify_buf = NULL;
	pfh->count = 0;
	pfh->next = 0;
}

static void _sg_blob_sqlite__open_blob_for_fetch(
	SG_context* pCtx,
	my_instance_data * pData,
	sqlite3* pSql,
	const char* pszHidRequested,
	sg_blob_sqlite_handle_fetch** ppFetchHandle)
{
	SG_int64 lBlobInfoRowID;
	SG_blob_encoding blob_encoding_stored;
	SG_uint64 len_encoded_stored;
	SG_uint64 len_full_stored;
	char* psz_hid_vcdiff_ref = NULL;
	sg_blob_sqlite_handle_fetch* pfh = NULL;

	SG_NULLARGCHECK_RETURN(pData);
	SG_NULLARGCHECK_RETURN(pSql);
	SG_NULLARGCHECK_RETURN(pszHidRequested);
	SG_NULLARGCHECK_RETURN(ppFetchHandle);

	// Get blob info.
	SG_ERR_CHECK(  sg_blob_sqlite__fetch_info(pCtx, pSql, pszHidRequested, &lBlobInfoRowID,
		&blob_encoding_stored, &len_encoded_stored,
		&len_full_stored, &psz_hid_vcdiff_ref)  );

	SG_ERR_CHECK(  SG_alloc(pCtx,1,sizeof(sg_blob_sqlite_handle_fetch), &pfh)  );
	pfh->pData = pData;
	SG_ERR_CHECK(  SG_strcpy(pCtx,pfh->sz_hid_requested, sizeof(pfh->sz_hid_requested), pszHidRequested)  );
	pfh->blob_encoding_stored = blob_encoding_stored;
	pfh->len_full_stored = len_full_stored;
	pfh->len_encoded_stored = len_encoded_stored;
	pfh->psz_hid_vcdiff_reference_stored = psz_hid_vcdiff_ref;

	SG_ERR_CHECK(  sg_blob_sqlite_blobstream_alloc_for_read(pCtx, pSql, lBlobInfoRowID,
		len_encoded_stored, &pfh->pBlobStream)  );

	*ppFetchHandle = pfh;

	return;

fail:

	SG_ERR_IGNORE(  sg_blob_sqlite_handle_fetch__free(pCtx, pfh)  );
}


void sg_blob_sqlite__store_blob__begin(SG_context* pCtx,
									   sg_repo_tx_sqlite_handle* pTx,
									   sqlite3* pSql,
									   SG_blob_encoding blob_encoding_given,
									   const char* psz_hid_vcdiff_reference,
									   SG_uint64 len_full_given,
									   SG_uint64 len_encoded_given,
									   const char* psz_hid_known,
									   SG_bool b_compress,
									   sg_blob_sqlite_handle_store** ppHandle)
{
	sg_blob_sqlite_handle_store* pbh = NULL;
	int zErr;
	SG_uint64 lTotalLen;

	SG_NULLARGCHECK_RETURN(pTx);

	if (!SG_IS_BLOBENCODING_FULL(blob_encoding_given))
	{
		// If we're not getting the full blob, a hid has to be provided.
		SG_NULLARGCHECK_RETURN(psz_hid_known);

		// If the blob's encoded, len_encoded_given must be valid.
		SG_NULLARGCHECK_RETURN(len_encoded_given);
	}
	else
	{
		// If we got a full blob, we need a full length.
		// BUT the length can be zero, so we can't do this:  SG_ARGCHECK_RETURN(len_full_given, len_full_given);
	}

	if (SG_BLOBENCODING__VCDIFF == blob_encoding_given)
	{
		// If this is a vcdiff'd blob, a reference has to be provided.
		SG_NULLARGCHECK_RETURN(psz_hid_vcdiff_reference);
	}

	// Build the store handle.
	SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(sg_blob_sqlite_handle_store), &pbh)  );
	pbh->pData = pTx->pData;

	// Add the handle to the repository transaction.
	SG_ERR_CHECK(  sg_repo_tx__sqlite__add_blob(pCtx, pTx, pbh)  );

	if (SG_IS_BLOBENCODING_FULL(blob_encoding_given))
		SG_ERR_CHECK(  sg_repo__sqlite__hash__begin(pCtx, pTx->pData->pRepo, &pbh->pRHH_ComputeOnStore)  );

	pbh->blob_encoding_given = blob_encoding_given;
	pbh->psz_hid_vcdiff_reference = psz_hid_vcdiff_reference;
	pbh->len_full_given = len_full_given;
	pbh->len_encoded_given = len_encoded_given;
	pbh->psz_hid_known = psz_hid_known;
	pbh->b_compressing = b_compress;
	pbh->b_changing_encoding = SG_FALSE;


	if (b_compress)
	{
		// We'll compress to a temporary file or buffer so we know the sprawl
		// blob's compressed length before inserting the sqlite blob.
		pbh->blob_encoding_storing = SG_BLOBENCODING__ZLIB;

		memset(&pbh->zStream, 0, sizeof(pbh->zStream));

		zErr = deflateInit(&pbh->zStream,Z_DEFAULT_COMPRESSION);
		if (zErr != Z_OK)
			SG_ERR_THROW(  SG_ERR_ZLIB(zErr)  );

		if (len_full_given < MY_CHUNK_SIZE)
		{
			// The blob's full length is smaller than our chunk size.  We'll compress in memory.
			pbh->iBufTempLen = (SG_uint32)(len_full_given + 50);  // +50 prevents enlarging the buffer on the common tiny blobs that compress badly.
			SG_allocN(pCtx, pbh->iBufTempLen, pbh->pBufTemp);
			pbh->pBufTempPosition = pbh->pBufTemp;
		}
		else
		{
			// Open the temp file and add its handle to our sg_blob_sqlite_handle_store.
			SG_ERR_CHECK(  SG_tempfile__open_new(pCtx, &pbh->pTempFile)  );
		}

		// Add the temp file handle to the repo tx blob handle
		pbh->ptxbh->pTempFile = pbh->pTempFile;

		SG_ERR_CHECK(  sg_blob_sqlite_blobstream_alloc(pCtx, pSql, &pbh->pBlobStream)  );
	}
	else
	{
		pbh->blob_encoding_storing = blob_encoding_given;

		// Set up sqlite blob(s) to receive the data.

		if (SG_IS_BLOBENCODING_FULL(blob_encoding_given))
			lTotalLen = len_full_given;
		else
			lTotalLen = len_encoded_given;

		SG_ERR_CHECK(  sg_blob_sqlite_blobstream_alloc_for_write(pCtx, pSql, lTotalLen, &pbh->pBlobStream)  );
	}

	// All's well, return the store handle.
	SG_RETURN_AND_NULL(pbh, ppHandle);

	return;

fail:
	SG_ERR_IGNORE(  sg_blob_sqlite_handle_store__free(pCtx, pbh)  );
}

static void _write_zlib_block_to_temp_buffer(SG_context* pCtx,
											 sg_blob_sqlite_handle_store* pbh,
											 SG_uint32 iSrcBufLen)
{
	SG_byte* pNewBuf = NULL;

	if (pbh->pBufTempPosition + iSrcBufLen > pbh->pBufTemp + pbh->iBufTempLen)
	{
		// We've outgrown the buffer.  Move to a bigger one.
		SG_uint32 iLenCurrentBuf = pbh->pBufTempPosition - pbh->pBufTemp;
		SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "Growing blob buffer!\r\n")  );
		SG_ERR_CHECK(  SG_allocN(pCtx, pbh->iBufTempLen * 2 + iSrcBufLen, pNewBuf)  );
		memcpy(pNewBuf, pbh->pBufTemp, iLenCurrentBuf);
		SG_NULLFREE(pCtx, pbh->pBufTemp);
		pbh->pBufTemp = pNewBuf;
		pbh->pBufTempPosition = pNewBuf + iLenCurrentBuf;
	}

	memcpy(pbh->pBufTempPosition, pbh->bufOut, iSrcBufLen);
	pbh->pBufTempPosition += iSrcBufLen;

	return;

fail:
	SG_NULLFREE(pCtx, pNewBuf);
}

void sg_blob_sqlite__store_blob__chunk(SG_context * pCtx,
									   sg_blob_sqlite_handle_store* pbh,
									   SG_uint32 len_chunk,
									   const SG_byte* p_chunk,
									   SG_uint32* piBytesWritten)
{
	int zErr;

	if (pbh->pRHH_ComputeOnStore)
		SG_ERR_CHECK(  sg_repo__sqlite__hash__chunk(pCtx, pbh->pData->pRepo, pbh->pRHH_ComputeOnStore, len_chunk, p_chunk)  );

	if (SG_IS_BLOBENCODING_FULL(pbh->blob_encoding_given))
		pbh->len_full_observed += len_chunk;

	if (pbh->blob_encoding_storing == pbh->blob_encoding_given)
		pbh->len_encoded_observed += len_chunk;

	if (pbh->b_compressing)
	{
		// give this chunk to compressor (it will update next_in and avail_in as
		// it consumes the input).

		pbh->zStream.next_in = (SG_byte*) p_chunk;
		pbh->zStream.avail_in = len_chunk;

		while (1)
		{
			// give the compressor the complete output buffer

			pbh->zStream.next_out = pbh->bufOut;
			pbh->zStream.avail_out = MY_CHUNK_SIZE;

			// let compressor compress what it can of our input.  it may or
			// may not take all of it.  this will update next_in, avail_in,
			// next_out, and avail_out.

			zErr = deflate(&pbh->zStream,Z_NO_FLUSH);
			if (zErr != Z_OK)
				SG_ERR_THROW(  SG_ERR_ZLIB(zErr)  );

			// if there was enough input to generate a compression block,
			// we can write it to our output file.  the amount generated
			// is the delta of avail_out (or equally of next_out).

			if (pbh->zStream.avail_out < MY_CHUNK_SIZE)
			{
				SG_uint32 nOut = MY_CHUNK_SIZE - pbh->zStream.avail_out;
				SG_ASSERT ( (nOut == (SG_uint32)(pbh->zStream.next_out - pbh->bufOut)) );

				if (pbh->pBufTemp)
				{
					SG_ERR_CHECK(  _write_zlib_block_to_temp_buffer(pCtx, pbh, nOut)  );
				}
				else
					SG_ERR_CHECK(  SG_file__write(pCtx, pbh->pTempFile->pFile, nOut, pbh->bufOut, NULL)  );

				pbh->len_encoded_observed += nOut;
			}

			// if compressor didn't take all of our input, let it
			// continue with the existing buffer (next_in was advanced).

			if (pbh->zStream.avail_in == 0)
				break;
		}
	}
	else
	{
		SG_ERR_CHECK(  sg_blob_sqlite_blobstream_write(pCtx, pbh->pBlobStream, len_chunk, p_chunk, NULL)  );
	}

	if (piBytesWritten)
		*piBytesWritten = len_chunk;

	return;

fail:
	return;
}

static void _store_blob_from_file(SG_context* pCtx,
								  SG_tempfile** ppTempFile,
								  sg_blob_sqlite_blobstream* pbs,
								  SG_uint64 lFileLen)
{
	SG_byte* buf = NULL;
	SG_uint32 bytesRead;
	SG_file* pFile = (*ppTempFile)->pFile;

	SG_ERR_CHECK(  SG_file__seek(pCtx, pFile, 0)  );

	SG_ERR_CHECK(  sg_blob_sqlite_blobstream_open_for_write(pCtx, pbs, lFileLen)  );

	buf = SG_malloc(MY_CHUNK_SIZE);
	if (!buf)
		SG_ERR_THROW_RETURN(SG_ERR_MALLOCFAILED);

	while (1)
	{
		SG_file__read(pCtx, pFile, MY_CHUNK_SIZE, buf, &bytesRead);
		if ( SG_context__err_equals(pCtx, SG_ERR_EOF) || 0 == bytesRead)
		{
			SG_context__err_reset(pCtx);
			break;
		}

		SG_ERR_CHECK_CURRENT;

		SG_ERR_CHECK(  sg_blob_sqlite_blobstream_write(pCtx, pbs, bytesRead, buf, NULL)  );
	}

	SG_ERR_CHECK(  SG_tempfile__close_and_delete(pCtx, ppTempFile)  );

fail:
	SG_NULLFREE(pCtx, buf);
}

void sg_blob_sqlite__store_blob__end(SG_context* pCtx,
									 SG_UNUSED_PARAM(sg_repo_tx_sqlite_handle* pTx),
									 sg_blob_sqlite_handle_store** ppbh,
									 char** ppsz_hid_returned)
{
	sg_blob_sqlite_handle_store* pbh = *ppbh;
	sg_repo_tx_sqlite_blob_handle* ptxbh = pbh->ptxbh;

	SG_UNUSED(pTx);

	if (pbh->pRHH_ComputeOnStore)
	{
		SG_ERR_CHECK(  sg_repo__sqlite__hash__end(pCtx, pbh->pData->pRepo, &pbh->pRHH_ComputeOnStore, &ptxbh->psz_hid_storing)  );

		if (pbh->psz_hid_known && 0 != strcmp(ptxbh->psz_hid_storing, pbh->psz_hid_known))
			SG_ERR_THROW(  SG_ERR_BLOB_NOT_VERIFIED_MISMATCH  );
	}
	else
	{
		/* we are trusting psz_hid_known */
		SG_ERR_CHECK(  SG_STRDUP(pCtx, pbh->psz_hid_known, &ptxbh->psz_hid_storing)  );
	}

	if (pbh->b_compressing)
	{
		int zErr;

		// we reached end of the input file.  now we need to tell the
		// compressor that we have no more input and that it needs to
		// compress and flush any partial buffers that it may have.

		SG_ASSERT( (pbh->zStream.avail_in == 0) );
		while (1)
		{
			pbh->zStream.next_out = pbh->bufOut;
			pbh->zStream.avail_out = MY_CHUNK_SIZE;

			zErr = deflate(&pbh->zStream, Z_FINISH);
			if ((zErr != Z_OK) && (zErr != Z_STREAM_END))
			{
				SG_ERR_THROW(  SG_ERR_ZLIB(zErr)  );
			}

			if (pbh->zStream.avail_out < MY_CHUNK_SIZE)
			{
				SG_uint32 nOut = MY_CHUNK_SIZE - pbh->zStream.avail_out;
				SG_ASSERT ( (nOut == (SG_uint32)(pbh->zStream.next_out - pbh->bufOut)) );

				if (pbh->pBufTemp)
				{
					SG_ERR_CHECK(  _write_zlib_block_to_temp_buffer(pCtx, pbh, nOut)  );
				}
				else
					SG_ERR_CHECK(  SG_file__write(pCtx, pbh->pTempFile->pFile, nOut, pbh->bufOut, NULL)  );

				pbh->len_encoded_observed += nOut;
			}

			if (zErr == Z_STREAM_END)
				break;
		}

		if (pbh->pBufTemp)
		{
			SG_uint32 len_written = 0;
			SG_ERR_CHECK(  sg_blob_sqlite_blobstream_open_for_write(pCtx, pbh->pBlobStream, pbh->len_encoded_observed)  );
			SG_ERR_CHECK(  sg_blob_sqlite_blobstream_write(pCtx, pbh->pBlobStream, (SG_uint32)pbh->len_encoded_observed, pbh->pBufTemp, &len_written)  );
			SG_ASSERT(len_written == pbh->len_encoded_observed);
		}
		else
		{
			// The temp file now has the complete compressed Sprawl blob.  Stream it to a sqlite blob.
			SG_ERR_CHECK(  _store_blob_from_file(pCtx, &pbh->pTempFile, pbh->pBlobStream,
				pbh->len_encoded_observed) );
		}

		// Clear repo tx's reference to the temp file.
		pbh->ptxbh->pTempFile = NULL;
	}

	if (0 != pbh->len_full_given
		&& SG_IS_BLOBENCODING_FULL(pbh->blob_encoding_given)
		&& pbh->len_full_given != pbh->len_full_observed)
	{
		SG_ERR_THROW(  SG_ERR_BLOB_NOT_VERIFIED_INCOMPLETE  );
	}

	if (0 != pbh->len_encoded_given && pbh->len_encoded_given != pbh->len_encoded_observed)
		SG_ERR_THROW(  SG_ERR_BLOB_NOT_VERIFIED_INCOMPLETE  );

	if (SG_IS_BLOBENCODING_FULL(pbh->blob_encoding_given) &&
		SG_IS_BLOBENCODING_FULL(pbh->blob_encoding_storing) &&
		pbh->len_encoded_observed != pbh->len_full_observed)
	{
		SG_ERR_THROW(  SG_ERR_BLOB_NOT_VERIFIED_INCOMPLETE  );
	}

	// Mark blob transfer complete and populate the tx data.
	ptxbh->b_changing_encoding = pbh->b_changing_encoding;
	ptxbh->blob_encoding_storing = pbh->blob_encoding_storing;

	if (pbh->psz_hid_vcdiff_reference)
		SG_ERR_CHECK(  SG_STRDUP(pCtx, pbh->psz_hid_vcdiff_reference, &ptxbh->psz_hid_vcdiff_reference)  );

	// We always have a len_encoded_observed, but when we're handed a pre-encoded
	// blob , we have to trust len_full_given.  If it's wrong, it will be discovered
	// on fetch with b_convert_to_full.  When we can observe the full length,
	// sg_blob_sqlite__store_blob__end() verifies that observed == given.
	ptxbh->len_encoded = pbh->len_encoded_observed;
	ptxbh->len_full = pbh->len_full_given;

	sg_blob_sqlite_handle_store__free_ex(pCtx, pbh, &ptxbh->paRowIDs, &ptxbh->iRowIDCount);

	if (ppsz_hid_returned)
		SG_ERR_CHECK(  SG_STRDUP(pCtx, ptxbh->psz_hid_storing, ppsz_hid_returned)  );

	// fall-thru to common cleanup

fail:

	// This handle is freed, as far as the caller is concerned,
	// but we don't actually free it until commit_tx or abort_tx are done with it.
	*ppbh = NULL;
}

void sg_blob_sqlite__fetch_blob__begin(SG_context* pCtx,
									   my_instance_data * pData,
									   sqlite3 * pSql,
									   const char* psz_hid_blob,
									   SG_bool b_convert_to_full,
									   char** ppsz_objectid,
									   SG_blob_encoding* p_blob_encoding,
									   char** ppsz_hid_vcdiff_reference,
									   SG_uint64* pLenEncoded,
									   SG_uint64* pLenFull,
									   sg_blob_sqlite_handle_fetch** ppHandle)
{
	sg_blob_sqlite_handle_fetch* pfh = NULL;
	int zErr;

	SG_NULLARGCHECK_RETURN(psz_hid_blob);
	SG_NULLARGCHECK_RETURN(ppHandle);
	SG_NULLARGCHECK_RETURN(pSql);

    SG_UNUSED(ppsz_objectid); // TODO

	SG_ERR_CHECK(  _sg_blob_sqlite__open_blob_for_fetch(pCtx, pData, pSql, psz_hid_blob, &pfh)  );

	if (b_convert_to_full || SG_IS_BLOBENCODING_FULL(pfh->blob_encoding_stored))
		SG_ERR_CHECK(  _blob_handle__init_digest(pCtx, pfh)  );

	if (b_convert_to_full && !SG_IS_BLOBENCODING_FULL(pfh->blob_encoding_stored))
	{
		pfh->blob_encoding_returning = SG_BLOBENCODING__FULL;

		if (SG_BLOBENCODING__VCDIFF == pfh->blob_encoding_stored)
		{
			SG_ERR_CHECK(  _setup_undeltify(pCtx, pfh)  );
		}
		else if (SG_BLOBENCODING__ZLIB == pfh->blob_encoding_stored)
		{
			pfh->b_uncompressing = SG_TRUE;

			memset(&pfh->zStream, 0, sizeof(pfh->zStream));
			zErr = inflateInit(&pfh->zStream);
			if (zErr != Z_OK)
				SG_ERR_THROW(  SG_ERR_ZLIB(zErr)  );
		}
		else
		{
			// Should never happen.
			SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
		}
	}
	else
	{
		pfh->blob_encoding_returning = pfh->blob_encoding_stored;
		pfh->b_undeltifying = SG_FALSE;
		pfh->b_uncompressing = SG_FALSE;
	}

	if (p_blob_encoding)
		*p_blob_encoding = pfh->blob_encoding_stored;
	if (ppsz_hid_vcdiff_reference)
		*ppsz_hid_vcdiff_reference = pfh->psz_hid_vcdiff_reference_stored;
	if (pLenEncoded)
		*pLenEncoded = pfh->len_encoded_stored;
	if (pLenFull)
		*pLenFull = pfh->len_full_stored;

	*ppHandle = pfh;

	return;

fail:
	SG_ERR_IGNORE(  sg_blob_sqlite_handle_fetch__free(pCtx, pfh)  );

}

void sg_blob_sqlite__fetch_blob__chunk(SG_context* pCtx,
									   sg_blob_sqlite_handle_fetch* pfh,
									   SG_uint32 len_buf,
									   SG_byte* p_buf,
									   SG_uint32* p_len_got,
                                       SG_bool* pb_done)
{
	SG_uint32 len_read;
	SG_uint64 len_remaining;
	SG_uint64 len_read_total;
	sg_blob_sqlite_blobstream* pbs = pfh->pBlobStream;
    SG_bool b_done = SG_FALSE;

	if (pfh->b_uncompressing)
	{
		int zErr;

		if (0 == pfh->zStream.avail_in)
		{
			len_read_total = pbs->iIdx * SQLITE_BLOB_MAX_BYTES + pbs->iOffset;
			len_remaining = (pfh->len_encoded_stored - len_read_total);
			if (len_remaining < sizeof(pfh->bufCompressed))
				len_read = (SG_uint32)len_remaining;
			else
				len_read = sizeof(pfh->bufCompressed);

			SG_ERR_CHECK(  sg_blob_sqlite_blobstream_read(pCtx, pbs, len_read, pfh->bufCompressed, NULL)  );

			pfh->len_encoded_observed += len_read;

			pfh->zStream.next_in = pfh->bufCompressed;
			pfh->zStream.avail_in = len_read;
		}

		pfh->zStream.next_out = p_buf;
		pfh->zStream.avail_out = len_buf;

		while (1)
		{
			// let decompressor decompress what it can of our input.  it may or
			// may not take all of it.  this will update next_in, avail_in,
			// next_out, and avail_out.
			//
			// WARNING: we get Z_STREAM_END when the decompressor is finished
			// WARNING: processing all of the input.  it gives this immediately
			// WARNING: (with the last partial buffer) rather than returning OK
			// WARNING: for the last partial buffer and then returning END with
			// WARNING: zero bytes transferred.

			zErr = inflate(&pfh->zStream, Z_NO_FLUSH);
			if ((zErr != Z_OK)  &&  (zErr != Z_STREAM_END))
				SG_ERR_THROW(  SG_ERR_ZLIB(zErr)  );

			if (zErr == Z_STREAM_END)
			{
                b_done = SG_TRUE;
				break;
			}

			// if decompressor didn't take all of our input, let it continue
			// with the existing buffer (next_in was advanced).

			if (pfh->zStream.avail_in == 0)
				break;

			if (pfh->zStream.avail_out == 0)
			{
				/* no room left */
				break;
			}
		}

		// avail_out is the remaining free space in the buffer, so here
		// we're getting the full/unencoded number of bytes we actually read.
		if (pfh->zStream.avail_out < len_buf)
			len_read = len_buf - pfh->zStream.avail_out;
		else
			len_read = 0;
	}
	else if (pfh->b_undeltifying)
	{
		if (!pfh->p_undeltify_buf || !pfh->count || (pfh->next >= pfh->count))
		{
			SG_ASSERT(pfh->pst);

			SG_ERR_CHECK(  SG_vcdiff__undeltify__chunk(pCtx, pfh->pst, &pfh->p_undeltify_buf, &pfh->count)  );

            if (0 == pfh->count)
            {
                b_done = SG_TRUE;
            }
		}

		len_read = pfh->count - pfh->next;
		if (len_read > len_buf)
		{
			len_read = len_buf;
		}

		memcpy(p_buf, pfh->p_undeltify_buf + pfh->next, len_read);
		pfh->next += len_read;

		/* We don't have a way of knowing how many bytes the vcdiff code has read
		* from our readstream.  But the readstream keeps track of this, so we just
		* ask it.
		*/
		SG_ERR_CHECK(  SG_readstream__get_count(pCtx, pfh->pstrm_delta, &pfh->len_encoded_observed)  );
	}
	else
	{
		//    Stored full => returning full
		//                or
		// Stored encoded => returning encoded

		if (SG_IS_BLOBENCODING_FULL(pfh->blob_encoding_stored))
			len_remaining = (pfh->len_full_stored - pbs->iOffset);
		else
			len_remaining = (pfh->len_encoded_stored - pbs->iOffset);

		// If the buffer's bigger than the remaining size of the blob,
		// read only to the end of the blob.
		if (len_remaining < len_buf)
			len_read = (SG_uint32)len_remaining;
		else
			len_read = len_buf;

		if (len_read)
		{
			SG_ERR_CHECK(  sg_blob_sqlite_blobstream_read(pCtx, pbs, len_read, p_buf, NULL)  );
			pfh->len_encoded_observed += len_read;

            if (pfh->len_encoded_observed == pfh->len_encoded_stored)
            {
                b_done = SG_TRUE;
            }
		}
	}

	if (len_read && pfh->pRHH_VerifyOnFetch)
		SG_ERR_CHECK_RETURN(  sg_repo__sqlite__hash__chunk(pCtx, pfh->pData->pRepo, pfh->pRHH_VerifyOnFetch, len_read, p_buf)  );

	if (SG_IS_BLOBENCODING_FULL(pfh->blob_encoding_returning))
		pfh->len_full_observed += len_read;

	if ( p_len_got )
		*p_len_got = len_read;

    *pb_done = b_done;

	return;

fail:
	return;
}

void sg_blob_sqlite__fetch_blob__end(SG_context* pCtx, sg_blob_sqlite_handle_fetch** ppHandle)
{
	sg_blob_sqlite_handle_fetch* pfh = *ppHandle;

	if (pfh->b_uncompressing)
	{
		SG_ASSERT( (pfh->zStream.avail_in == 0) );
		inflateEnd(&pfh->zStream);
	}
	else if (pfh->b_undeltifying)
	{
		SG_ERR_CHECK(  SG_vcdiff__undeltify__end(pCtx, pfh->pst)  );
		pfh->pst = NULL;

		SG_ERR_CHECK(  SG_readstream__close(pCtx, pfh->pstrm_delta)  );
		pfh->pstrm_delta = NULL;

		_blob_seekreader_nullfree(pCtx, &pfh->psr_reference);

		pfh->p_undeltify_buf = NULL;
		pfh->count = 0;
		pfh->next = 0;
	}

	SG_ERR_CHECK(  _blob_handle__verify_lengths_and_digest(pCtx, pfh)  );

	// Fall through to common cleanup.

fail:
	SG_ERR_IGNORE(  sg_blob_sqlite_handle_fetch__free(pCtx, pfh)  );
	*ppHandle = NULL;
}

void sg_blob_sqlite__fetch_blob__abort(SG_context * pCtx, sg_blob_sqlite_handle_fetch** ppbh)
{
	if (!ppbh || !*ppbh)
		return;

	sg_blob_sqlite_handle_fetch__free(pCtx, *ppbh);
	*ppbh = NULL;
}

void sg_blob_sqlite__store_blob__abort(SG_context * pCtx,
									   sg_repo_tx_sqlite_handle* pTx,
									   sg_blob_sqlite_handle_store** ppbh)
{
	sg_blob_sqlite_handle_store* pbh;
	SG_uint32 i;

	if (!ppbh || !*ppbh)
		return;

	SG_NULLARGCHECK_RETURN(pTx);

	pbh = *ppbh;

	if (pbh->pBlobStream)
	{
		sg_sqlite__blob_close(pCtx, &(pbh->pBlobStream->pBlob));
		// TODO can the above fail?
	}

	// Delete the empty/partially written blob row(s)

	if (pbh->pBlobStream)
	{
		if (pbh->pBlobStream->paRowIDs && pbh->pBlobStream->iCount && pbh->pBlobStream->psql)
		{
			for (i=0; i < pbh->pBlobStream->iCount; i++)
			{
				SG_ERR_IGNORE(  sg_blob_sqlite__delete(pCtx, pbh->pBlobStream->psql, pbh->pBlobStream->paRowIDs, pbh->pBlobStream->iCount)  );
			}
		}
	}

	// Remove this blob from the repository transaction.
	SG_ERR_IGNORE(  sg_repo_tx__sqlite__remove_last_blob(pCtx, pTx)  );

	SG_ERR_IGNORE(  sg_blob_sqlite_handle_store__free(pCtx, *ppbh)  );
	*ppbh = NULL;
}

static void _change_encoding_to_full_or_zlib(SG_context* pCtx,
											 sg_repo_tx_sqlite_handle* pTx,
											 sqlite3* psql,
											 const char* psz_hid_blob,
											 SG_bool bCompressing,
											 SG_uint64 len_full_stored)
{
	sg_blob_sqlite_handle_fetch* pfh = NULL;
	sg_blob_sqlite_handle_store* psh = NULL;
	SG_uint64 left = 0;
	SG_byte* p_buf = NULL;
    SG_bool b_done = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pTx);

	SG_ERR_CHECK(  sg_blob_sqlite__store_blob__begin(pCtx, pTx, psql, SG_BLOBENCODING__FULL,
		NULL, len_full_stored, 0, psz_hid_blob, bCompressing, &psh)  );
	psh->b_changing_encoding = SG_TRUE;

	SG_ERR_CHECK(  sg_blob_sqlite__fetch_blob__begin(pCtx, pTx->pData, psql, psz_hid_blob, SG_TRUE, NULL, NULL, NULL,
		NULL, NULL, &pfh)  );

	SG_ERR_CHECK(  SG_alloc(pCtx, 1, MY_CHUNK_SIZE, &p_buf)  );

	left = len_full_stored;
	while (!b_done)
	{
		SG_uint32 want = MY_CHUNK_SIZE;
		SG_uint32 got = 0;

		if (want > left)
			want = (SG_uint32)left;

		SG_ERR_CHECK(  sg_blob_sqlite__fetch_blob__chunk(pCtx, pfh, want, p_buf, &got, &b_done)  );
		SG_ERR_CHECK(  sg_blob_sqlite__store_blob__chunk(pCtx, psh,  got, p_buf, NULL)  );

		left -= got;
	}

	SG_ERR_CHECK(  sg_blob_sqlite__fetch_blob__end(pCtx, &pfh)  );

	SG_ERR_CHECK(  sg_blob_sqlite__store_blob__end(pCtx, pTx, &psh, NULL)  );

    SG_ASSERT(0 == left);

	SG_NULLFREE(pCtx, p_buf);

	return;

fail:
	if (pfh)
		SG_ERR_IGNORE(  sg_blob_sqlite__fetch_blob__abort(pCtx, &pfh)  );
	if (psh)
		SG_ERR_IGNORE(  sg_blob_sqlite__store_blob__abort(pCtx, pTx, &psh)  );

	SG_NULLFREE(pCtx, p_buf);
}

static void _change_encoding_to_vcdiff(SG_context* pCtx,
									   sg_repo_tx_sqlite_handle* pTx,
									   sqlite3* psql,
									   SG_uint64 len_full,
									   const char* psz_hid_blob,
									   const char* psz_hid_vcdiff_reference)
{
	sg_blob_sqlite_handle_fetch* pfh;
	blob_seekreader* psr_reference = NULL;
	SG_readstream* pstrmTarget = NULL;
	SG_writestream* pstrmDelta = NULL;
	SG_uint64 len_encoded;
	SG_tempfile* pTempfile;
	sg_blob_sqlite_blobstream* pbs = NULL;
	sg_repo_tx_sqlite_blob_handle* ptxbh = NULL;

	// The reference blob needs to get into a seekreader.
	SG_ERR_CHECK(  sg_blob_sqlite__seekreader_open(pCtx, pTx->pData, psql, psz_hid_vcdiff_reference, &psr_reference)  );

	// The blob we're deltifying needs to be a readstream.  We use the blob__fetch routines
	// rather the the sqlite blobstream reader so we can decompress from zlib if necessary.
	SG_ERR_CHECK(  sg_blob_sqlite__fetch_blob__begin(pCtx, pTx->pData, psql, psz_hid_blob, SG_TRUE, NULL, NULL,
		NULL, NULL, NULL, &pfh)  );
	SG_ERR_CHECK(  SG_readstream__alloc(pCtx, pfh,
		(SG_stream__func__read*)sg_blob_sqlite__fetch_blob__chunk, NULL, &pstrmTarget)  );

	/* The new delta is a writestream backed by a temp file
	   because sqlite requires knowing a blob's length writing it. */
	SG_ERR_CHECK(  SG_tempfile__open_new(pCtx, &pTempfile)  );
	SG_ERR_CHECK(  SG_writestream__alloc(pCtx, pTempfile->pFile,
		(SG_stream__func__write*)SG_file__write, NULL, &pstrmDelta)  );

	SG_ERR_CHECK(  SG_vcdiff__deltify__streams(pCtx, psr_reference->pSeekreader, pstrmTarget, pstrmDelta)  );

	SG_ERR_CHECK(  SG_writestream__get_count(pCtx, pstrmDelta, &len_encoded)  );

	SG_ERR_CHECK(  SG_readstream__close(pCtx, pstrmTarget)  );
	pstrmTarget = NULL;
	SG_ERR_CHECK(  SG_writestream__close(pCtx, pstrmDelta)  );
	pstrmDelta = NULL;

	SG_ERR_IGNORE(  _blob_seekreader_nullfree(pCtx, &psr_reference)  );

	SG_ERR_CHECK(  sg_blob_sqlite__fetch_blob__end(pCtx, &pfh)  );

	// temp file -> sqlite blob(s)
	SG_ERR_CHECK(  sg_blob_sqlite_blobstream_alloc(pCtx, psql, &pbs)  );

	SG_ERR_CHECK(  _store_blob_from_file(pCtx, &pTempfile, pbs, len_encoded)  );


	// Mark blob transfer complete and populate the tx data.
	SG_ERR_CHECK(  sg_repo_tx__sqlite_blob_handle__alloc(pCtx, pTx, &ptxbh)  );

	sg_blob_sqlite_blobstream_free_ex(pCtx, pbs, &ptxbh->paRowIDs, &ptxbh->iRowIDCount);

	SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_hid_blob, &ptxbh->psz_hid_storing)  );
	if (psz_hid_vcdiff_reference)
		SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_hid_vcdiff_reference, &ptxbh->psz_hid_vcdiff_reference)  );

	ptxbh->b_changing_encoding = SG_TRUE;
	ptxbh->blob_encoding_storing = SG_BLOBENCODING__VCDIFF;

	// We always have a len_encoded_observed, but when we're handed a pre-encoded
	// blob , we have to trust len_full_given.  If it's wrong, it will be discovered
	// on fetch with b_convert_to_full.  When we can observe the full length,
	// sg_blob_sqlite__store_blob__end() verifies that observed == given.
	ptxbh->len_encoded = len_encoded;
	ptxbh->len_full = len_full;

	return;

fail:
	if (pbs)
		SG_ERR_IGNORE(  sg_blob_sqlite_blobstream_free(pCtx, pbs)  );
	if (psr_reference)
		SG_ERR_IGNORE(  _blob_seekreader_nullfree(pCtx, &psr_reference)  );

	if (pstrmDelta)
	{
		SG_ERR_IGNORE(  SG_writestream__close(pCtx, pstrmDelta)  );
	}
}


void sg_blob_sqlite__change_blob_encoding(SG_context* pCtx,
										  sg_repo_tx_sqlite_handle* pTx,
										  sqlite3* psql,
										  const char* psz_hid_blob,
										  SG_blob_encoding blob_encoding_desired,
										  const char* psz_hid_vcdiff_reference_desired,
										  SG_blob_encoding* p_blob_encoding_new,
										  char** ppsz_hid_vcdiff_reference,
										  SG_uint64* p_len_encoded,
										  SG_uint64* p_len_full)
{
	//SG_bool b_is_a_vcdiff_reference = SG_FALSE;
	SG_blob_encoding blob_encoding_stored = 0;
	char* psz_hid_vcdiff_reference_stored = NULL;
	SG_uint64 len_encoded_stored;
	SG_uint64 len_full_stored;

	SG_NULLARGCHECK_RETURN(pTx);
	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(psz_hid_blob);

	SG_ERR_CHECK(  sg_blob_sqlite__fetch_info(pCtx, psql, psz_hid_blob, NULL, &blob_encoding_stored,
		NULL, &len_full_stored, &psz_hid_vcdiff_reference_stored)  );

    // TODO handle conversions to/from ALWAYSFULL

	switch (blob_encoding_desired)
	{
		case SG_BLOBENCODING__FULL:

			if (!SG_IS_BLOBENCODING_FULL(blob_encoding_stored))
			{
				SG_ERR_CHECK(  _change_encoding_to_full_or_zlib(pCtx, pTx, psql, psz_hid_blob,
					SG_FALSE, len_full_stored)  );
			}
			break;

		case SG_BLOBENCODING__ZLIB:

			if (
                    (SG_BLOBENCODING__ZLIB != blob_encoding_stored)
                    && (SG_BLOBENCODING__ALWAYSFULL != blob_encoding_stored)
               )
			{
				SG_ERR_CHECK(  _change_encoding_to_full_or_zlib(pCtx, pTx, psql, psz_hid_blob,
					SG_TRUE, len_full_stored)  );
			}
			break;

		case SG_BLOBENCODING__VCDIFF:

			if (!psz_hid_vcdiff_reference_desired)
				SG_ERR_THROW2_RETURN(SG_ERR_INVALIDARG, (pCtx, "A reference blob is required when converting to vcdiff."));

			/* Note that we check to see if the blob is already in
			* vcdiff format, but we do not check to see if the
			* reference is the one that was requested.  TODO should we? */

			if (SG_BLOBENCODING__VCDIFF != blob_encoding_stored)
			{
				SG_ERR_CHECK(  _change_encoding_to_vcdiff(pCtx, pTx, psql, len_full_stored,
					psz_hid_blob, (const char*)psz_hid_vcdiff_reference_desired)  );
			}
			break;

		default:
			SG_ERR_THROW2_RETURN(SG_ERR_INVALIDARG, (pCtx, "Unknown desired blob encoding: %s", blob_encoding_desired));
	}

	SG_NULLFREE(pCtx, psz_hid_vcdiff_reference_stored);

	SG_ERR_CHECK(  sg_blob_sqlite__fetch_info(pCtx, psql, psz_hid_blob, NULL,
		&blob_encoding_stored, &len_encoded_stored, &len_full_stored,
		&psz_hid_vcdiff_reference_stored)  );

	/* TODO we could verify that the retrieved values match the requested ones */

	if (p_blob_encoding_new)
	{
		*p_blob_encoding_new = blob_encoding_stored;
	}
	if (p_len_encoded)
	{
		*p_len_encoded = len_encoded_stored;
	}
	if (p_len_full)
	{
		*p_len_full = len_full_stored;
	}
	if (ppsz_hid_vcdiff_reference)
	{
		*ppsz_hid_vcdiff_reference = psz_hid_vcdiff_reference_stored;
	}
	else
	{
		SG_NULLFREE(pCtx, psz_hid_vcdiff_reference_stored);
	}

fail:
	return;
}
