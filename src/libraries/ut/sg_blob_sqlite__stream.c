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
* @file sg_blob_sqlite__stream.c
*
*/

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_repo_sqlite__private.h"

//////////////////////////////////////////////////////////////////

void sg_blob_sqlite_blobstream_free_ex(SG_context * pCtx,
									   sg_blob_sqlite_blobstream* pbs,
									   SG_int64** ppaRowIDs,
									   SG_uint32* piCount)
{
	if (pbs)
	{
		sg_sqlite__blob_close(pCtx, &pbs->pBlob);

		if (piCount)
			*piCount = pbs->iCount;

		SG_RETURN_AND_NULL(pbs->paRowIDs, ppaRowIDs);
		SG_NULLFREE(pCtx, pbs->paRowIDs);

		SG_NULLFREE(pCtx, pbs);
	}
}

void sg_blob_sqlite_blobstream_free(SG_context * pCtx, sg_blob_sqlite_blobstream* pbs)
{
	sg_blob_sqlite_blobstream_free_ex(pCtx, pbs, NULL, NULL);
}

static void _sg_blob_sqlite_blobstream_init_length(SG_context * pCtx,
												   sg_blob_sqlite_blobstream* pbs,
												   SG_uint64 lTotalLen)
{
	SG_NULLARGCHECK_RETURN(pbs);

	pbs->iCount = (SG_uint32)(lTotalLen / SQLITE_BLOB_MAX_BYTES);
	pbs->iLastLen = (SG_uint32)(lTotalLen % SQLITE_BLOB_MAX_BYTES);
	if (pbs->iLastLen)
		(pbs->iCount)++;
	else
		pbs->iLastLen = SQLITE_BLOB_MAX_BYTES;

	if (lTotalLen == 0)
	{
		pbs->iCount = 1;
		pbs->iLastLen = 0;
	}
}

void sg_blob_sqlite_blobstream_alloc(SG_context* pCtx,
									 sqlite3* pSql,
									 sg_blob_sqlite_blobstream** ppbs)
{
	sg_blob_sqlite_blobstream* pbs;

	SG_ASSERT(pSql && ppbs);

	SG_ERR_CHECK_RETURN(  SG_alloc(pCtx, 1, sizeof(sg_blob_sqlite_blobstream), &pbs)  );

	pbs->psql = pSql;

	*ppbs = pbs;
}

void sg_blob_sqlite_blobstream_alloc_for_read(SG_context* pCtx,
											  sqlite3* pSql,
											  SG_int64 lBlobInfoRowID,
											  SG_uint64 lTotalLen,
											  sg_blob_sqlite_blobstream** ppbs)
{
	sg_blob_sqlite_blobstream* pbs = NULL;
	SG_int64* paRowIDs;
	SG_uint32 iRowCount;
	sqlite3_blob* pBlob;

	SG_ASSERT(ppbs);

	SG_ERR_CHECK(  sg_blob_sqlite_blobstream_alloc(pCtx, pSql, &pbs)  );
	SG_ERR_CHECK(  _sg_blob_sqlite_blobstream_init_length(pCtx, pbs, lTotalLen)  );

	SG_ERR_CHECK(  sg_blob_sqlite__fetch_blobstream_info(pCtx, pSql, NULL, lBlobInfoRowID,
		&paRowIDs, &iRowCount)  );

	// Open handle for first sqlite blob.
	SG_ERR_CHECK(  sg_blob_sqlite__sql_blob__get_read_handle(pCtx, pSql, paRowIDs[0], &pBlob)  );

	pbs->paRowIDs = paRowIDs;
	pbs->iCount = iRowCount;
	pbs->pBlob = pBlob;

	*ppbs = pbs;

	return;

fail:
    SG_ERR_IGNORE(  sg_blob_sqlite_blobstream_free(pCtx, pbs)  );
}

void sg_blob_sqlite_blobstream_open_for_write(SG_context* pCtx,
											  sg_blob_sqlite_blobstream* pbs,
											  SG_uint64 lTotalLen)
{
	SG_int64 lRowID;
	SG_uint32 iSqlBlobLen;
	SG_uint32 i;
	SG_bool bTxOpen = SG_FALSE;

	SG_ASSERT(pbs);

	SG_ERR_CHECK(  _sg_blob_sqlite_blobstream_init_length(pCtx, pbs, lTotalLen)  );

	SG_ERR_CHECK(  SG_alloc(pCtx, pbs->iCount, sizeof(SG_int64), &pbs->paRowIDs)  );

	// Create placeholder blobs and add their rowids to the store handle.
	if (1 == pbs->iCount)
		iSqlBlobLen = pbs->iLastLen;
	else
		iSqlBlobLen = SQLITE_BLOB_MAX_BYTES;

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pbs->psql, "BEGIN TRANSACTION")  );
	bTxOpen = SG_TRUE;

	for (i=0; i < pbs->iCount; i++)
	{
		SG_ERR_CHECK(  sg_blob_sqlite__sql_blob_create(pCtx, pbs->psql, iSqlBlobLen, &lRowID, NULL)  );
		(pbs->paRowIDs)[i] = lRowID;
		if ( i == pbs->iCount - 1)
			iSqlBlobLen = pbs->iLastLen;
	}

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pbs->psql, "COMMIT TRANSACTION")  );
	bTxOpen = SG_FALSE;

	// Open the first blob for writing.
	SG_ERR_CHECK(  sg_blob_sqlite__sql_blob__get_write_handle(pCtx, pbs->psql,
		(pbs->paRowIDs)[0], &pbs->pBlob)  );

	return;

fail:
	if (pbs && pbs->psql && bTxOpen)
		SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pbs->psql, "ROLLBACK TRANSACTION")  );
}

void sg_blob_sqlite_blobstream_alloc_for_write(SG_context* pCtx,
											   sqlite3* pSql,
											   SG_uint64 lTotalLen,
											   sg_blob_sqlite_blobstream** ppbs)
{
	sg_blob_sqlite_blobstream* pbs = NULL;

	SG_NULLARGCHECK_RETURN(ppbs);

	SG_ERR_CHECK_RETURN(  sg_blob_sqlite_blobstream_alloc(pCtx, pSql, &pbs)  );

	SG_ERR_CHECK(  sg_blob_sqlite_blobstream_open_for_write(pCtx, pbs, lTotalLen)  );

	*ppbs = pbs;

    return;
fail:
    SG_ERR_IGNORE(  sg_blob_sqlite_blobstream_free(pCtx, pbs)  );
}

void sg_blob_sqlite_blobstream_read(SG_context* pCtx,
									sg_blob_sqlite_blobstream* pbs,
									SG_uint32 len_chunk,
									SG_byte* p_chunk,
									SG_uint32* p_len_read)
{
	SG_uint32 len_read_total = 0;
	SG_uint32 len_remaining_in_current_sqlite_blob;
	SG_uint32 len_to_read_from_current_sqlite_blob = 0;
	SG_uint32 len_remaining_in_sprawl_chunk = len_chunk;

	while (1)
	{
		if (pbs->iIdx == pbs->iCount - 1)
			len_remaining_in_current_sqlite_blob = pbs->iLastLen - pbs->iOffset;
		else
			len_remaining_in_current_sqlite_blob = SQLITE_BLOB_MAX_BYTES - pbs->iOffset;

		len_to_read_from_current_sqlite_blob = SG_MIN(len_remaining_in_current_sqlite_blob,
			len_remaining_in_sprawl_chunk);

		// vcdiff will keep calling this until we return EOF.
		if (0 == len_to_read_from_current_sqlite_blob)
			SG_ERR_THROW_RETURN( SG_ERR_EOF );			// TODO with the new SG_context stuff, is this right? or should we have a pbEOF arg?

		SG_ERR_CHECK(  sg_sqlite__blob_read(pCtx, pbs->psql, pbs->pBlob, p_chunk+len_read_total,
			len_to_read_from_current_sqlite_blob, pbs->iOffset)  );

		pbs->iOffset += len_to_read_from_current_sqlite_blob;
		len_remaining_in_sprawl_chunk -= len_to_read_from_current_sqlite_blob;
		len_remaining_in_current_sqlite_blob -= len_to_read_from_current_sqlite_blob;
		len_read_total += len_to_read_from_current_sqlite_blob;

		if (0 == len_remaining_in_current_sqlite_blob)
		{
			// We've read the entire sqlite blob.

			// Is this the last sqlite blob?
			if (pbs->iIdx == pbs->iCount- 1)
			{
				// When called from the vcdiff seekreader, len_chunk is just an upper limit,
				// so we need to stop when we've hit the end of the blobstream.
				break;
			}

			// There are more sqlite blobs.  Move on to the next one.
			sg_sqlite__blob_close(pCtx, &pbs->pBlob);
			SG_ERR_CHECK(  sg_blob_sqlite__sql_blob__get_read_handle(pCtx, pbs->psql,
				pbs->paRowIDs[++pbs->iIdx], &pbs->pBlob)  );
			pbs->iOffset = 0;
		}

		// When called from fetch_blob__chunk, len_chunk is always the right number of
		// bytes to return, so we keep reading until we get there.
		if (len_read_total == len_chunk)
			break;
	}

	if (p_len_read)
		*p_len_read = len_read_total;

	return;

fail:
	return;
}

void sg_blob_sqlite_blobstream_write(SG_context* pCtx,
									 sg_blob_sqlite_blobstream* pbs,
									 SG_uint32 len_chunk,
									 const SG_byte* p_chunk,
									 SG_uint32* p_len_written)
{
	SG_uint32 len_written_total = 0;
	SG_uint32 len_remaining_in_current_sqlite_blob;
	SG_uint32 len_to_write_to_current_sqlite_blob = 0;
	SG_uint32 len_remaining_in_sprawl_chunk = len_chunk;

	SG_NULLARGCHECK_RETURN(pbs);

	while (1)
	{
		if (pbs->iIdx == pbs->iCount - 1)
			len_remaining_in_current_sqlite_blob = pbs->iLastLen - pbs->iOffset;
		else
			len_remaining_in_current_sqlite_blob = SQLITE_BLOB_MAX_BYTES - pbs->iOffset;

		len_to_write_to_current_sqlite_blob = SG_MIN(len_remaining_in_current_sqlite_blob,
			len_remaining_in_sprawl_chunk);

		SG_ERR_CHECK(  sg_sqlite__blob_write(pCtx, pbs->psql, pbs->pBlob, p_chunk+len_written_total,
			len_to_write_to_current_sqlite_blob, pbs->iOffset)  );

		pbs->iOffset += len_to_write_to_current_sqlite_blob;
		len_remaining_in_sprawl_chunk -= len_to_write_to_current_sqlite_blob;
		len_remaining_in_current_sqlite_blob -= len_to_write_to_current_sqlite_blob;
		len_written_total += len_to_write_to_current_sqlite_blob;

		if (0 == len_remaining_in_current_sqlite_blob)
		{
			// We've filled the entire sqlite blob.

			// Is this the last sqlite blob?
			if (pbs->iIdx == pbs->iCount- 1)
			{
				// Stop when we've hit the end of the blobstream.
				break;
			}

			// There are more sqlite blobs.  Move on to the next one.
			sg_sqlite__blob_close(pCtx, &pbs->pBlob);
			SG_ERR_CHECK(  sg_blob_sqlite__sql_blob__get_write_handle(pCtx, pbs->psql,
				pbs->paRowIDs[++pbs->iIdx], &pbs->pBlob)  );
			pbs->iOffset = 0;
		}

		// Stop when we've written everything we were given.
		if (len_written_total == len_chunk)
			break;
	}

	if (p_len_written)
		*p_len_written = len_written_total;

	return;

fail:
	return;
}

void sg_blob_sqlite_seekreader_seek(SG_context* pCtx,
									sg_blob_sqlite_blobstream* pbs,
									SG_uint64 iPos)
{
	SG_uint32 quotient;
	SG_uint32 remainder;

	SG_NULLARGCHECK_RETURN(pbs);

	if (iPos < SQLITE_BLOB_MAX_BYTES)
		pbs->iOffset = (SG_uint32)iPos;
	else
	{
		quotient = (SG_uint32)(iPos / SQLITE_BLOB_MAX_BYTES);
		remainder = (SG_uint32)(iPos % SQLITE_BLOB_MAX_BYTES);

		sg_sqlite__blob_close(pCtx, &pbs->pBlob);

		SG_ASSERT(quotient <= pbs->iCount);

		SG_ERR_CHECK(  sg_blob_sqlite__sql_blob__get_read_handle(pCtx, pbs->psql,
			pbs->paRowIDs[pbs->iIdx], &pbs->pBlob)  );

		pbs->iIdx = quotient - 1;
		pbs->iOffset = remainder;
	}

	return;

fail:
	return;
}

void sg_blob_sqlite_blobstream_close(SG_context * pCtx, sg_blob_sqlite_blobstream* pbs)
{
	sg_blob_sqlite_blobstream_free(pCtx, pbs);
}

/* Blob seekreaders now just write the full blob to a file, then use a file
   seekreader (which allows a blob of any encoding to be a vcdiff reference.
   This routine was used when blobs had their own seekreaders.

static SG_error _blob_sqlite_seekreader_alloc(sg_blob_sqlite_blobstream* pBlobSpan,
											  SG_uint64 lEncodedLen,
											  SG_seekreader** ppsr)
{
	SG_error err;
	SG_seekreader* psr = NULL;

	if (!pBlobSpan || !lEncodedLen || !ppsr)
		return SG_ERR_INVALIDARG;

	SG_ERR_CHECK(  SG_seekreader__alloc(pBlobSpan,  0, lEncodedLen,
		(SG_stream__func__read*)sg_blob_sqlite_blobstream_read,
		(SG_stream__func__seek*)sg_blob_sqlite_seekreader_seek,
		(SG_stream__func__close*)sg_blob_sqlite_blobstream_close,
		&psr)  );

	*ppsr = psr;

	return SG_ERR_OK;

fail:
	SG_NULLFREE(pCtx, psr);
	return err;
}
*/

static void _write_full_blob_to_file(SG_context* pCtx,
									 my_instance_data * pData,
									 sqlite3* psql,
									 const char* szBlobHid,
									 SG_tempfile** ppTempFile)
{
	sg_blob_sqlite_handle_fetch* pfh;
	SG_tempfile* pTempFile;
	SG_byte* buf = NULL;
	SG_uint64 lLenFull;
	SG_uint64 left;
    SG_bool b_done = SG_FALSE;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(szBlobHid);
	SG_NULLARGCHECK_RETURN(ppTempFile);

	SG_ERR_CHECK(  sg_blob_sqlite__fetch_blob__begin(pCtx, pData, psql, szBlobHid, SG_TRUE, NULL, NULL, NULL,
		NULL, &lLenFull, &pfh)  );

	SG_ERR_CHECK(  SG_tempfile__open_new(pCtx, &pTempFile)  );

	SG_ERR_CHECK(  SG_allocN(pCtx, MY_CHUNK_SIZE, buf)  );

	left = lLenFull;
	while (!b_done)
	{
		SG_uint32 want = MY_CHUNK_SIZE;
		SG_uint32 got = 0;

		if (want > left)
			want = (SG_uint32)left;

		SG_ERR_CHECK(  sg_blob_sqlite__fetch_blob__chunk(pCtx, pfh, want, buf, &got, &b_done)  );
		SG_ERR_CHECK(  SG_file__write(pCtx, pTempFile->pFile, got, buf, NULL)  );

		left -= got;
	}

    SG_ASSERT(0 == left);

	SG_ERR_CHECK(  sg_blob_sqlite__fetch_blob__end(pCtx, &pfh)  );
	SG_NULLFREE(pCtx, buf);

	*ppTempFile = pTempFile;

	return;

fail:
	SG_NULLFREE(pCtx, buf);
	SG_ERR_IGNORE(  SG_tempfile__close_and_delete(pCtx, &pTempFile)  );
	SG_ERR_IGNORE(  sg_blob_sqlite_handle_fetch__free(pCtx, pfh)  );
}

void sg_blob_sqlite__seekreader_open(SG_context* pCtx,
									 my_instance_data * pData,
									 sqlite3* psql,
									 const char* szHid,
									 blob_seekreader** ppsr)
{
	blob_seekreader* psr = NULL;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(szHid);
	SG_NULLARGCHECK_RETURN(ppsr);

	SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(blob_seekreader), &psr)  );

	SG_ERR_CHECK(  _write_full_blob_to_file(pCtx, pData, psql, szHid, &psr->pTempfile)  );
	SG_ERR_CHECK(  SG_tempfile__close(pCtx, psr->pTempfile)  );
	SG_ERR_CHECK(  SG_seekreader__alloc__for_file(pCtx, psr->pTempfile->pPathname, 0, &psr->pSeekreader)  );

	*ppsr = psr;

	return;

fail:
	SG_NULLFREE(pCtx, psr);
}

static void _blob_sqlite_readstream_alloc(SG_context* pCtx,
										  sg_blob_sqlite_blobstream* pBlobSpan,
										  SG_readstream** pprs)
{
	SG_readstream* prs = NULL;

	SG_NULLARGCHECK_RETURN(pBlobSpan);
	SG_NULLARGCHECK_RETURN(pprs);

	SG_ERR_CHECK(  SG_readstream__alloc(pCtx, pBlobSpan,
		(SG_stream__func__read*)sg_blob_sqlite_blobstream_read,
		(SG_stream__func__close*)sg_blob_sqlite_blobstream_close,
		&prs)  );

	*pprs = prs;

	return;

fail:
	SG_NULLFREE(pCtx, prs);
}


void sg_blob_sqlite__readstream_open(SG_context* pCtx,
									 sqlite3* psql,
									 const char* pszHidBlobRequested,
									 SG_readstream** pprs)
{
	SG_uint64 len_encoded;
	SG_int64 lBlobInfoRowID;
	sg_blob_sqlite_blobstream* pbs = NULL;
	SG_readstream* prs = NULL;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(pszHidBlobRequested);
	SG_NULLARGCHECK_RETURN(pprs);

	SG_ERR_CHECK(  sg_blob_sqlite__fetch_info(pCtx, psql, pszHidBlobRequested, &lBlobInfoRowID,
		NULL, &len_encoded, NULL, NULL)  );

	SG_ERR_CHECK(  sg_blob_sqlite_blobstream_alloc_for_read(pCtx, psql, lBlobInfoRowID, len_encoded, &pbs)  );

	SG_ERR_CHECK(  _blob_sqlite_readstream_alloc(pCtx, pbs, &prs)  );

	*pprs = prs;
	prs = NULL;

	return;

fail:
	SG_ERR_IGNORE(  sg_blob_sqlite_blobstream_free(pCtx, pbs)  );
}
