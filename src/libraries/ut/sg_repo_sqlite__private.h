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
 * @file sg_repo_sqlite__private.h
 *
 * @details Declarations for private routines for the SQLite repo implementation.
 *
 */

//////////////////////////////////////////////////////////////////
#include <zlib.h>

#ifndef H_SG_REPO_SQLITE__PRIVATE_H
#define H_SG_REPO_SQLITE__PRIVATE_H

// http://www.sqlite.org/limits.html says the limit is 1 billion,
// but I got crashes when a blob actually reached that size.
// Perhaps the true limit is 1 billion minus the 8 byte rowid?
// For now, 500 million works.
#define SQLITE_BLOB_MAX_BYTES	500000000

#define MY_CHUNK_SIZE			SG_STREAMING_BUFFER_SIZE

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

typedef struct _my_instance_data my_instance_data;


struct _blob_seekreader
{
	SG_tempfile* pTempfile;
	SG_seekreader* pSeekreader;
};
typedef struct _blob_seekreader blob_seekreader;

// This is the struct used to store blob data prior to repo tx commit.
struct _sg_repo_tx_sqlite_blob_handle
{
	char*							psz_hid_storing;
	SG_blob_encoding				blob_encoding_storing;
	SG_bool							b_changing_encoding;
	SG_uint64						len_full;
	SG_uint64						len_encoded;
	char*							psz_hid_vcdiff_reference;
	SG_int64*						paRowIDs;				// Array of rowids holding the blob.
	SG_uint32						iRowIDCount;			// The number of rowids in paRowIDs.
	SG_tempfile*					pTempFile;
};
typedef struct _sg_repo_tx_sqlite_blob_handle sg_repo_tx_sqlite_blob_handle;

struct _sg_repo_tx_sqlite_handle
{
	my_instance_data *				pData;				// we DO NOT own this

	SG_vector*						pv_tx_bh;			// pointer to a vector containing repo transaction blob handles
	SG_bool							bLastBlobRemoved;	// If a blob is aborted, the last slot in the vector becomes available and this is set true;
    SG_rbtree*                      prb_frags;
};
typedef struct _sg_repo_tx_sqlite_handle sg_repo_tx_sqlite_handle;

struct _sg_blob_sqlite_blobstream
{
	sqlite3*			psql;

	SG_int64*			paRowIDs;		// Array of rowids holding the blob.
	SG_uint32			iCount;			// The number of rowids in paRowIDs.
	sqlite3_blob*		pBlob;			// Pointer to the currently open blob.
	SG_uint32			iOffset;		// Position in the currently open blob.
	SG_uint32			iIdx;			// The element in paRowIDs that pBlob currently points to.

	SG_uint32			iLastLen;		// The length of the last blob in the stream.  (All
										// but the last blob have length SQLITE_BLOB_MAX_BYTES.)
};
typedef struct _sg_blob_sqlite_blobstream sg_blob_sqlite_blobstream;

struct _sg_blob_sqlite_handle_store
{
	my_instance_data *				pData;					// we DO NOT own this

	SG_blob_encoding				blob_encoding_given;
	SG_blob_encoding				blob_encoding_storing;
	const char*						psz_hid_vcdiff_reference;
	SG_uint64						len_full_given;
	SG_uint64						len_encoded_given;
	SG_uint64						len_full_observed;
	SG_uint64						len_encoded_observed;
	const char*						psz_hid_known;
	SG_repo_hash_handle *			pRHH_ComputeOnStore;	// was SG_digest* pDigestRaw;

	SG_bool							b_changing_encoding;

	SG_tempfile*					pTempFile;

	sg_blob_sqlite_blobstream*		pBlobStream;

	/* zlib stuff */
	SG_bool							b_compressing;
	z_stream						zStream;
	SG_byte							bufOut[MY_CHUNK_SIZE];

	SG_byte*						pBufTemp;				// If the blob is small enough, we compress to this buffer, not a file.
	SG_uint32						iBufTempLen;
	SG_byte*						pBufTempPosition;

	sg_repo_tx_sqlite_blob_handle* ptxbh;
};
typedef struct _sg_blob_sqlite_handle_store sg_blob_sqlite_handle_store;

struct _sg_blob_sqlite_handle_fetch
{
	my_instance_data *				pData;					// we DO NOT own this

	char							sz_hid_requested[SG_HID_MAX_BUFFER_LENGTH];
	SG_uint64						len_encoded_stored;		// value taken from blobinfo -- we need to make sure whatever we read matches this
	SG_uint64						len_encoded_observed;	// value calculated as we read/uncompress/expand blob content into Raw Data
	SG_uint64						len_full_observed;		// value calculated as we read/uncompress/expand blob content into Raw Data
	SG_uint64						len_full_stored;
	SG_blob_encoding				blob_encoding_stored;
	SG_blob_encoding				blob_encoding_returning;
	SG_repo_hash_handle *			pRHH_VerifyOnFetch;		// was SG_digest* pDigest;

	sg_blob_sqlite_blobstream*		pBlobStream;

	/* zlib stuff */
	SG_bool							b_uncompressing;
	z_stream						zStream;
	SG_byte							bufCompressed[MY_CHUNK_SIZE]; // TODO malloc this?  It's a lot of memory that's not used often.

	/* vcdiff stuff */
 	char*							psz_hid_vcdiff_reference_stored;
 	SG_bool							b_undeltifying;
 	SG_vcdiff_undeltify_state*		pst;
 	SG_readstream*					pstrm_delta;
 	blob_seekreader*				psr_reference;
 	SG_byte*						p_undeltify_buf;
	SG_uint32						count;
	SG_uint32						next;
};
typedef struct _sg_blob_sqlite_handle_fetch sg_blob_sqlite_handle_fetch;

/**
 * we get one pointer in the SG_repo for our instance data.  in SG_repo
 * this is an opaque "sg_repo__vtable__instance_data *".  we cast it
 * to the following definition.
 */
struct _my_instance_data
{
    SG_repo*						pRepo;			// we DO NOT own this (but rather we are bound to it)

	SG_pathname*					pParentDirPathname;
	SG_pathname*					pMyDirPathname;
	SG_pathname*					pSqlDbPathname;

	char							buf_hash_method[SG_REPO_HASH_METHOD__MAX_BUFFER_LENGTH];
	char							buf_repo_id[SG_GID_BUFFER_LENGTH];
	char							buf_admin_id[SG_GID_BUFFER_LENGTH];

	sqlite3*						psql;

	sg_blob_sqlite_handle_store*	pBlobStoreHandle;

	SG_uint32						strlen_hashes;
};

//////////////////////////////////////////////////////////////////

void sg_blob_sqlite_blobstream_free(SG_context * pCtx,
									sg_blob_sqlite_blobstream* pbs);
void sg_blob_sqlite_blobstream_free_ex(SG_context * pCtx,
									   sg_blob_sqlite_blobstream* pbs,
									   SG_int64** ppaRowIDs,
									   SG_uint32* piCount);

void sg_blob_sqlite_handle_store__free(SG_context * pCtx, sg_blob_sqlite_handle_store* pbh);
void sg_blob_sqlite_handle_store__free_ex(SG_context * pCtx,
										  sg_blob_sqlite_handle_store* pbh,
										  SG_int64** ppaRowIDs,
										  SG_uint32* piCount);

void sg_blob_sqlite_handle_fetch__free(SG_context * pCtx, sg_blob_sqlite_handle_fetch* pbh);

void sg_repo_sqlite__create_id_table(SG_context* pCtx, sqlite3* pSql);

void sg_repo_sqlite__add_id(SG_context* pCtx, sqlite3* psql, const char* pszName, const char* pszValue);

void sg_repo_sqlite_get_id(SG_context* pCtx, sqlite3* psql, const char* pszName, SG_string** ppstrValue);

void sg_blob_sqlite__create_blob_tables(SG_context* pCtx, sqlite3* pSql);

void sg_blob_sqlite__add_info(SG_context* pCtx, sqlite3* psql, sg_repo_tx_sqlite_blob_handle* pTxBlobHandle);

void sg_blob_sqlite__check_for_vcdiff_reference(SG_context* pCtx, sqlite3* psql, const char* szHidBlob, SG_bool* pbResult);

void sg_blob_sqlite_blobstream_alloc(SG_context* pCtx, sqlite3* pSql, sg_blob_sqlite_blobstream** ppbs);

void sg_blob_sqlite_blobstream_alloc_for_read(SG_context* pCtx,
											  sqlite3* pSql,
											  SG_int64 lBlobInfoRowID,
											  SG_uint64 lTotalLen,
											  sg_blob_sqlite_blobstream** ppbs);

void sg_blob_sqlite_blobstream_open_for_write(SG_context* pCtx,
											  sg_blob_sqlite_blobstream* pbs,
											  SG_uint64 lTotalLen);

void sg_blob_sqlite_blobstream_alloc_for_write(SG_context* pCtx,
											   sqlite3* pSql,
											   SG_uint64 lTotalLen,
											   sg_blob_sqlite_blobstream** ppbs);

void sg_blob_sqlite__fetch_blobstream_info(SG_context* pCtx,
										   sqlite3* psql,
										   const char* pszHidBlob,
										   SG_int64 lBlobInfoRowID,
										   SG_int64* ppaRowIDs[],
										   SG_uint32* piRowIDCount);

void sg_blob_sqlite__fetch_info(SG_context* pCtx,
								sqlite3* psql,
								const char* szHidBlob,
								SG_int64* plBlobInfoRowID,
								SG_blob_encoding* p_blob_encoding,
								SG_uint64* p_len_encoded,
								SG_uint64* p_len_full,
								char** ppsz_hid_vcdiff_reference);

void sg_blob_sqlite__update_info(SG_context* pCtx,
								 sqlite3* psql,
								 SG_int64* paRowIDs,
								 SG_uint32 iRowIDCount,
								 const char* pszHidBlob,
								 SG_blob_encoding blob_encoding,
								 SG_uint64 len_encoded,
								 SG_uint64 len_full,
								 const char* psz_hid_vcdiff_reference);

void sg_blob_sqlite__store_blob__begin(SG_context* pCtx,
									   sg_repo_tx_sqlite_handle* pTx,
									   sqlite3* pSql,
									   SG_blob_encoding blob_encoding_given,
									   const char* psz_hid_vcdiff_reference,
									   SG_uint64 lenFull,
									   SG_uint64 lenEncoded,
									   const char* psz_hid_known,
									   SG_bool b_compress,
									   sg_blob_sqlite_handle_store** ppHandle);

void sg_blob_sqlite__store_blob__chunk(SG_context* pCtx,
									   sg_blob_sqlite_handle_store* pbh,
									   SG_uint32 len_chunk,
									   const SG_byte* p_chunk,
									   SG_uint32* piBytesWritten);

void sg_blob_sqlite__store_blob__end(SG_context* pCtx,
									 sg_repo_tx_sqlite_handle* pTx,
									 sg_blob_sqlite_handle_store** ppbh,
									 char** ppsz_hid_returned);

void sg_blob_sqlite__store_blob__abort(SG_context * pCtx,
									   sg_repo_tx_sqlite_handle* pTx,
									   sg_blob_sqlite_handle_store** ppHandle);

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
									   sg_blob_sqlite_handle_fetch** ppHandle);

void sg_blob_sqlite__fetch_blob__chunk(SG_context* pCtx,
									   sg_blob_sqlite_handle_fetch* pFetchHandle,
									   SG_uint32 len_buf,
									   SG_byte* p_buf,
									   SG_uint32* p_len_got,
                                       SG_bool* pb_done);

void sg_blob_sqlite__fetch_blob__end(SG_context* pCtx,
									 sg_blob_sqlite_handle_fetch** ppHandle);

void sg_blob_sqlite__fetch_blob__abort(SG_context * pCtx,
									   sg_blob_sqlite_handle_fetch** ppHandle);

void sg_blob_sqlite__list_blobs(SG_context* pCtx,
								sqlite3* psql,
								SG_blob_encoding blob_encoding, /**< only return blobs of this encoding */
								SG_bool b_sort_descending,      /**< otherwise sort ascending */
								SG_bool b_sort_by_len_encoded,  /**< otherwise sort by len_full */
								SG_uint32 limit,                /**< only return this many entries */
								SG_uint32 offset,               /**< skip the first group of the sorted entries */
								SG_vhash** ppvh);               /**< Caller must free */

void sg_blob_sqlite__find_by_hid_prefix(SG_context* pCtx, sqlite3* psql, const char* psz_hid_prefix, SG_rbtree** pprb);

void sg_blob_sqlite__get_blob_stats(
	SG_context* pCtx,
	sqlite3* psql,
	SG_uint32* p_count_blobs_full,
	SG_uint32* p_count_blobs_alwaysfull,
	SG_uint32* p_count_blobs_zlib,
	SG_uint32* p_count_blobs_vcdiff,
	SG_uint64* p_total_blob_size_full,
	SG_uint64* p_total_blob_size_alwaysfull,
	SG_uint64* p_total_blob_size_zlib_full,
	SG_uint64* p_total_blob_size_zlib_encoded,
	SG_uint64* p_total_blob_size_vcdiff_full,
	SG_uint64* p_total_blob_size_vcdiff_encoded
	);

void sg_blob_sqlite__change_blob_encoding(
	SG_context* pCtx,
	sg_repo_tx_sqlite_handle* pTx,
	sqlite3 * psql,
	const char* psz_hid_blob,
	SG_blob_encoding blob_encoding_desired,
	const char* psz_hid_vcdiff_reference_desired,
	SG_blob_encoding* p_blob_encoding_new,
	char** ppsz_hid_vcdiff_reference,
	SG_uint64* p_len_encoded,
	SG_uint64* p_len_full
	);

void sg_blob_sqlite__sql_blob__get_read_handle(SG_context* pCtx,
											   sqlite3* pSql,
											   SG_int64 lRowID,
											   sqlite3_blob** ppb);

void sg_blob_sqlite__sql_blob_create(SG_context* pCtx,
									 sqlite3* pSql,
									 SG_uint32 iLen,
									 SG_int64* plRowID,
									 sqlite3_blob** ppb);

void sg_blob_sqlite__sql_blob__get_write_handle(SG_context* pCtx,
												sqlite3* pSql,
												SG_int64 lRowID,
												sqlite3_blob** ppb);


void sg_blob_sqlite_blobstream_read(SG_context* pCtx,
									sg_blob_sqlite_blobstream* pbs,
									SG_uint32 len_chunk,
									SG_byte* p_chunk,
									SG_uint32* p_len_read);

void sg_blob_sqlite_blobstream_write(SG_context* pCtx,
									 sg_blob_sqlite_blobstream* pbs,
									 SG_uint32 len_chunk,
									 const SG_byte* p_chunk,
									 SG_uint32* p_len_written);

void sg_blob_sqlite_blobstream_close(SG_context* pCtx,
									 sg_blob_sqlite_blobstream* pbs);

void sg_blob_sqlite_seekreader_seek(SG_context* pCtx,
									sg_blob_sqlite_blobstream* pbs,
									SG_uint64 iPos);

void sg_blob_sqlite__seekreader_open(SG_context* pCtx,
									 my_instance_data * pData,
									 sqlite3* psql,
									 const char* szHid,
									 blob_seekreader** ppsr);

void sg_blob_sqlite__readstream_open(SG_context* pCtx,
									 sqlite3* psql,
									 const char* pszHidBlobRequested,
									 SG_readstream** pprs);

void sg_blob_sqlite__vacuum(SG_context* pCtx, sqlite3* pSql);

void sg_blob_sqlite__delete(SG_context* pCtx, sqlite3* pSql, SG_int64* paRowIDs, SG_uint32 iRowCount);

void sg_repo_tx__sqlite__begin(SG_context* pCtx,
							   my_instance_data * pData,
							   sg_repo_tx_sqlite_handle** ppTx);

void sg_repo_tx__sqlite__commit(SG_context* pCtx,
								SG_repo* pRepo,
								sqlite3* psql,
								sg_repo_tx_sqlite_handle** ppTx);

void sg_repo_tx__sqlite__abort(SG_context* pCtx,
							   sqlite3* psql,
							   sg_repo_tx_sqlite_handle** ppTx,
							   sg_blob_sqlite_handle_store** pbh);


void sg_repo_tx__sqlite__add_blob(SG_context* pCtx,
								  sg_repo_tx_sqlite_handle* pTx,
								  sg_blob_sqlite_handle_store* pbh);

void sg_repo_tx__sqlite__remove_last_blob(SG_context* pCtx, sg_repo_tx_sqlite_handle* pTx);

void sg_repo_tx__sqlite_blob_handle__alloc(SG_context* pCtx,
										   sg_repo_tx_sqlite_handle* pTx,
										   sg_repo_tx_sqlite_blob_handle** ppTxbh);

void _sqlite_my_get_dbndx(SG_context* pCtx,
							  SG_repo* pRepo,
							  SG_uint32 iDagNum,
							  SG_bool bQueryOnly,
							  SG_dbndx** ppNdx);

void _sqlite_my_get_treendx(SG_context* pCtx,
							  SG_repo* pRepo,
							  SG_uint32 iDagNum,
							  SG_bool bQueryOnly,
							  SG_treendx** ppNdx);

//////////////////////////////////////////////////////////////////

void sg_repo__sqlite__hash__begin(
	SG_context* pCtx,
    SG_repo * pRepo,
    SG_repo_hash_handle** ppHandle);

void sg_repo__sqlite__hash__chunk(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_hash_handle* pHandle,
    SG_uint32 len_chunk,
    const SG_byte* p_chunk);

void sg_repo__sqlite__hash__end(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_hash_handle** ppHandle,
    char** ppsz_hid_returned);

void sg_repo__sqlite__hash__abort(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_hash_handle** ppHandle);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_REPO_SQLITE__PRIVATE_H
