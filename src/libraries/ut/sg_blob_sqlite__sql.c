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
 * @file sg_blob_sqlite__sql.c
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_repo_sqlite__private.h"

#define SQL_BUSY_TIMEOUT_MS (30000)

//////////////////////////////////////////////////////////////////

// For cleaning up a sqlite3_stmt in a fail block.
static void _cleanup_stmt_in_fail(SG_context * pCtx, sqlite3_stmt* pStmt)
{
	if (pStmt)
		SG_ERR_IGNORE(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
}

void sg_repo_sqlite__create_id_table(SG_context* pCtx, sqlite3* pSql)
{
	int rc;

	rc = sqlite3_busy_timeout(pSql, SQL_BUSY_TIMEOUT_MS);
	if (rc)
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pSql, ("CREATE TABLE id"
		"   ("
		"     name VARCHAR PRIMARY KEY,"
		"     value VARCHAR NOT NULL"
		"   )"))  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pSql, ("CREATE INDEX id_name on id ( name )"))  );

	return;

fail:
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
}

void sg_repo_sqlite__add_id(SG_context* pCtx, sqlite3* psql, const char* pszName, const char* pszValue)
{
	sg_sqlite__exec__va(pCtx, psql, "INSERT INTO id (name, value) VALUES ('%s', '%s')", pszName, pszValue);
}

void sg_repo_sqlite_get_id(SG_context* pCtx, sqlite3* psql, const char* pszName, SG_string** ppstrValue)
{
	sg_sqlite__exec__va__string(pCtx, psql, ppstrValue, "SELECT value FROM id WHERE name='%s'", pszName);
}

void sg_blob_sqlite__create_blob_tables(SG_context* pCtx, sqlite3* pSql)
{
	// The blobinfo and blobs tables need an INTEGER PRIMARY KEY column to
	// assure that VACUUM won't alter rowids, which we use.  That's why they
	// have rowid_alias colums that aren't explicitly used anywhere.
	//
	// (See http://www.sqlite.org/lang_vacuum.html and
	// http://www.sqlite.org/lang_createtable.html#rowid).

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pSql, ("CREATE TABLE blobinfo"
		"   ("
		"     rowid_alias INTEGER PRIMARY KEY,"
		"     hid_blob VARCHAR NOT NULL,"
		"     encoding INTEGER NOT NULL,"
		"     len_full INTEGER NOT NULL,"
		"     len_encoded INTEGER NULL,"
		"     hid_vcdiff VARCHAR NULL"
		"   )"))  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pSql, ("CREATE UNIQUE INDEX blobinfo_hid_blob on blobinfo ( hid_blob )"))  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pSql, ("CREATE INDEX blobinfo_hid_vcdiff on blobinfo ( hid_vcdiff )"))  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pSql, ("CREATE INDEX blobinfo_encoding on blobinfo ( encoding )"))  );

	/*
	*	Blobs are in their very own table for 2 reasons:
	*
	*	-	You need to be able to write a blob before you know its hid, so if there were
	*		another field here to identify the associated blobinfo record, it would have
	*		to allow nulls, then we'd have to come back and update it after the blob was written.
	*		Two inserts should be faster than an insert and an update.
	*
	*	-	The maximum sqlite blob limit is also the maximum sqlite row limit.  (See
	*		http://www.sqlite.org/limits.html.)  If we want to use the absolute maximum
	*		possible sqlite blob size (and we still might not), this is the only way to do it.
	*/
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pSql, ("CREATE TABLE blobs"
		"   ("
		"     rowid_alias INTEGER PRIMARY KEY,"
		"     blob_data BLOB"
		"   )"))  );

	// This table maps 1 or more rows in the blobs table, representing a single sprawl blob,
	// to the single blobinfo row.
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pSql, ("CREATE TABLE blobmap"
		"   ("
		"     blobinfo_rowid INTEGER NOT NULL,"
		"     blobs_rowid INTEGER NOT NULL"
		"   )"))  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pSql, ("CREATE INDEX blobmap_blobinfo_rowid on blobmap ( blobinfo_rowid )"))  );

	return;

fail:
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
}

void sg_blob_sqlite__add_info(SG_context* pCtx, sqlite3* psql, sg_repo_tx_sqlite_blob_handle* pTxBlobHandle)
{
	sqlite3_stmt* pStmt = NULL;
	SG_int64 lBlobInfoRowID;
	SG_uint32 i;

	// Add stuff to blobinfo
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
		"INSERT INTO blobinfo (hid_blob, encoding, len_full, len_encoded, hid_vcdiff) "
		"VALUES (?, ?, ?, ?, ?)")  );

	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, pTxBlobHandle->psz_hid_storing)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 2, pTxBlobHandle->blob_encoding_storing)  );

	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 3, pTxBlobHandle->len_full)  );

	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 4, pTxBlobHandle->len_encoded)  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 5, pTxBlobHandle->psz_hid_vcdiff_reference)  );

	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	SG_ERR_CHECK(  sg_sqlite__last_insert_rowid(pCtx, psql, &lBlobInfoRowID)  );

	// Add stuff to blobmap
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
		"INSERT INTO blobmap (blobinfo_rowid, blobs_rowid) "
		"VALUES (?, ?)")  );

	for (i=0; i < pTxBlobHandle->iRowIDCount; i++ )
	{
		if (i)
		{
			SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
			SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );
		}
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, lBlobInfoRowID)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 2, pTxBlobHandle->paRowIDs[i])  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
	}
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	return;

fail:

	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_CONSTRAINT), SG_ERR_BLOBFILEALREADYEXISTS);
	SG_ERR_IGNORE(  _cleanup_stmt_in_fail(pCtx, pStmt)  );
}

void sg_blob_sqlite__update_info(SG_context* pCtx,
								 sqlite3* psql,
								 SG_int64* paRowIDs,
								 SG_uint32 iRowIDCount,
								 const char* pszHidBlob,
								 SG_blob_encoding blob_encoding,
								 SG_uint64 len_encoded,
								 SG_uint64 len_full,
								 const char* psz_hid_vcdiff_reference)
{
	sqlite3_stmt* pStmt = NULL;
	SG_int64 lBlobInfoRowID;
	SG_uint32 i;

	// Get the rowid
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
		"SELECT rowid FROM blobinfo WHERE hid_blob = ?")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, pszHidBlob)  );
	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_ROW)  );
	lBlobInfoRowID = sqlite3_column_int64(pStmt, 0);
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	// Update blobinfo
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
		"UPDATE blobinfo "
		"SET encoding = ?, len_full = ?, len_encoded = ?, hid_vcdiff = ? "
		"WHERE rowid = ?")  );

	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 1, blob_encoding)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 2, len_full)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 3, len_encoded)  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 4, psz_hid_vcdiff_reference)  );

	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 5, lBlobInfoRowID)  );

	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	if (iRowIDCount)
	{
		// Delete old blobs
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
			"DELETE FROM blobs WHERE rowid IN "
			"(SELECT blobs_rowid FROM blobmap WHERE blobinfo_rowid = ?)")  );
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, lBlobInfoRowID)  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

		// Delete old blobmap rows
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
			"DELETE FROM blobmap WHERE blobinfo_rowid = ?")  );
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, lBlobInfoRowID)  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

		// Write new blobmap rows
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
			"INSERT INTO blobmap (blobinfo_rowid, blobs_rowid) "
			"VALUES (?, ?)")  );

		for (i=0; i < iRowIDCount; i++ )
		{
			if (i)
			{
				SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
				SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );
			}
			SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, lBlobInfoRowID)  );
			SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 2, paRowIDs[i])  );
			SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
		}
		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
	}

	return;

fail:
	SG_ERR_IGNORE(  _cleanup_stmt_in_fail(pCtx, pStmt)  );
}


void sg_blob_sqlite__list_blobs(SG_context* pCtx,
								sqlite3* psql,
								SG_blob_encoding blob_encoding, /**< only return blobs of this encoding */
								SG_bool b_sort_descending,      /**< otherwise sort ascending */
								SG_bool b_sort_by_len_encoded,  /**< otherwise sort by len_full */
								SG_uint32 limit,                /**< only return this many entries */
								SG_uint32 offset,               /**< skip the first group of the sorted entries */
								SG_vhash** ppvh)                /**< Caller must free */
{
	int rc;
	sqlite3_stmt * pStmt = NULL;
	SG_vhash* pvh = NULL;
	SG_vhash* pvh_blob = NULL;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx,psql,&pStmt,
		"SELECT hid_blob, len_full, len_encoded, hid_vcdiff FROM blobinfo "
		"WHERE encoding=? ORDER BY %s %s LIMIT ? OFFSET ?", b_sort_by_len_encoded ?
		"len_encoded" : "len_full", b_sort_descending ? "DESC" : "ASC")  );

	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx,pStmt,1,blob_encoding)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx,pStmt,2,limit)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx,pStmt,3,offset)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx,&pvh)  );

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char * psz_hid = (const char *)sqlite3_column_text(pStmt,0);
		SG_uint64 len_full = sqlite3_column_int64(pStmt, 1);
		SG_uint64 len_encoded = sqlite3_column_int64(pStmt, 2);
		const char * psz_hid_vcdiff = (const char *)sqlite3_column_text(pStmt,3);

		SG_ERR_CHECK(  SG_VHASH__ALLOC__SHARED(pCtx, &pvh_blob, 3, pvh)  );
		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_blob, "len_full", len_full)  );
		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_blob, "len_encoded", len_encoded)  );
		if (psz_hid_vcdiff)
		{
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_blob, "hid_vcdiff", psz_hid_vcdiff)  );
		}
		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh, psz_hid, &pvh_blob)  );
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}

	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	*ppvh = pvh;

	return;

fail:
	SG_ERR_IGNORE(  _cleanup_stmt_in_fail(pCtx, pStmt)  );
	SG_VHASH_NULLFREE(pCtx, pvh_blob);
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void sg_blob_sqlite__find_by_hid_prefix(SG_context* pCtx, sqlite3* psql, const char* psz_hid_prefix, SG_rbtree** pprb)
{
	sqlite3_stmt * pStmt = NULL;
	SG_rbtree* prb = NULL;
	SG_int32 rc;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(psz_hid_prefix);
	SG_NULLARGCHECK_RETURN(pprb);

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx,psql,&pStmt,
		"SELECT hid_blob FROM blobinfo WHERE hid_blob LIKE '%s%%'", psz_hid_prefix)  );

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx,&prb)  );

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char * szHid = (const char *)sqlite3_column_text(pStmt,0);

		SG_ERR_CHECK(  SG_rbtree__add(pCtx,prb,szHid)  );
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}

	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx,&pStmt)  );

	*pprb = prb;

	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, prb);
	SG_ERR_IGNORE(  _cleanup_stmt_in_fail(pCtx, pStmt)  );
}

void sg_blob_sqlite__get_blob_stats(SG_context* pCtx,
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
									SG_uint64* p_total_blob_size_vcdiff_encoded)
{
	SG_int32 count_blobs_full = 0;
	SG_int32 count_blobs_alwaysfull = 0;
	SG_int32 count_blobs_zlib = 0;
	SG_int32 count_blobs_vcdiff = 0;
	SG_int64 total_blob_size_full = 0;
	SG_int64 total_blob_size_alwaysfull = 0;
	SG_int64 total_blob_size_zlib_full = 0;
	SG_int64 total_blob_size_zlib_encoded = 0;
	SG_int64 total_blob_size_vcdiff_full = 0;
	SG_int64 total_blob_size_vcdiff_encoded = 0;

	SG_ERR_CHECK(  sg_sqlite__exec__va__int32(pCtx, psql, &count_blobs_full, "SELECT count(*) FROM blobinfo WHERE encoding=%d", SG_BLOBENCODING__FULL)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va__int32(pCtx, psql, &count_blobs_alwaysfull, "SELECT count(*) FROM blobinfo WHERE encoding=%d", SG_BLOBENCODING__ALWAYSFULL)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va__int32(pCtx, psql, &count_blobs_zlib, "SELECT count(*) FROM blobinfo WHERE encoding=%d", SG_BLOBENCODING__ZLIB)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va__int32(pCtx, psql, &count_blobs_vcdiff, "SELECT count(*) FROM blobinfo WHERE encoding=%d", SG_BLOBENCODING__VCDIFF)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va__int64(pCtx, psql, &total_blob_size_full, "SELECT sum(len_full) FROM blobinfo WHERE encoding=%d", SG_BLOBENCODING__FULL)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va__int64(pCtx, psql, &total_blob_size_alwaysfull, "SELECT sum(len_full) FROM blobinfo WHERE encoding=%d", SG_BLOBENCODING__ALWAYSFULL)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va__int64(pCtx, psql, &total_blob_size_zlib_full, "SELECT sum(len_full) FROM blobinfo WHERE encoding=%d", SG_BLOBENCODING__ZLIB)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va__int64(pCtx, psql, &total_blob_size_zlib_encoded, "SELECT sum(len_encoded) FROM blobinfo WHERE encoding=%d", SG_BLOBENCODING__ZLIB)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va__int64(pCtx, psql, &total_blob_size_vcdiff_full, "SELECT sum(len_full) FROM blobinfo WHERE encoding=%d", SG_BLOBENCODING__VCDIFF)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va__int64(pCtx, psql, &total_blob_size_vcdiff_encoded, "SELECT sum(len_encoded) FROM blobinfo WHERE encoding=%d", SG_BLOBENCODING__VCDIFF)  );

	if (p_count_blobs_full)
		*p_count_blobs_full = (SG_uint32) count_blobs_full;
	if (p_count_blobs_alwaysfull)
		*p_count_blobs_alwaysfull = (SG_uint32) count_blobs_alwaysfull;
	if (p_count_blobs_zlib)
		*p_count_blobs_zlib = (SG_uint32) count_blobs_zlib;
	if (p_count_blobs_vcdiff)
		*p_count_blobs_vcdiff = (SG_uint32) count_blobs_vcdiff;

	if (p_total_blob_size_full)
		*p_total_blob_size_full = (SG_uint32) total_blob_size_full;
	if (p_total_blob_size_alwaysfull)
		*p_total_blob_size_alwaysfull = (SG_uint32) total_blob_size_alwaysfull;
	if (p_total_blob_size_zlib_full)
		*p_total_blob_size_zlib_full = (SG_uint32) total_blob_size_zlib_full;
	if (p_total_blob_size_zlib_encoded)
		*p_total_blob_size_zlib_encoded = (SG_uint32) total_blob_size_zlib_encoded;
	if (p_total_blob_size_vcdiff_full)
		*p_total_blob_size_vcdiff_full = (SG_uint32) total_blob_size_vcdiff_full;
	if (p_total_blob_size_vcdiff_encoded)
		*p_total_blob_size_vcdiff_encoded = (SG_uint32) total_blob_size_vcdiff_encoded;

	return;

fail:
	return;
}

void sg_blob_sqlite__check_for_vcdiff_reference(SG_context* pCtx, sqlite3* psql, const char* szHidBlob, SG_bool* pbResult)
{
	SG_int32 rc;
	sqlite3_stmt* pStmt = NULL;
	SG_bool bResult = SG_FALSE;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx,psql,&pStmt,
		"SELECT hid_blob FROM blobinfo WHERE hid_vcdiff = ? LIMIT 1")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx,pStmt,1,szHidBlob)  );

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		bResult = SG_TRUE;
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx,&pStmt)  );

	*pbResult = bResult;

	return;

fail:
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
	SG_ERR_IGNORE(  _cleanup_stmt_in_fail(pCtx, pStmt)  );
}

void sg_blob_sqlite__fetch_blobstream_info(SG_context* pCtx,
										   sqlite3* psql,
										   const char* pszHidBlob,
										   SG_int64 lBlobInfoRowID,
										   SG_int64* ppaRowIDs[],
										   SG_uint32* piRowIDCount)
{
	sqlite3_stmt* pStmt = NULL;
	SG_int32 rc;

	SG_int64* paRowIDs = NULL;
	SG_uint32 iRowIDCount;
	SG_uint32 i;

	if (!lBlobInfoRowID)
	{
		SG_NULLARGCHECK_RETURN(pszHidBlob);

		// Get blobinfo rowid if we weren't give one.
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
			"SELECT rowid FROM blobinfo WHERE hid_blob = ? LIMIT 1")  );
		SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, pszHidBlob)  );
		if ((rc=sqlite3_step(pStmt)) != SQLITE_ROW)
			SG_ERR_THROW(  SG_ERR_BLOB_NOT_FOUND  );
		lBlobInfoRowID = sqlite3_column_int64(pStmt, 0);
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
	}
	SG_ASSERT(lBlobInfoRowID);

	// Get blobs table rowids from blobmap table.
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql,&pStmt,
		"SELECT COUNT(blobs_rowid) FROM blobmap WHERE blobinfo_rowid = ?")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, lBlobInfoRowID)  );
	if ((rc=sqlite3_step(pStmt)) != SQLITE_ROW)
		SG_ERR_THROW(  SG_ERR_BLOB_NOT_FOUND  );

	iRowIDCount = sqlite3_column_int(pStmt, 0);
	if (0 == iRowIDCount)
		SG_ERR_THROW(  SG_ERR_BLOB_NOT_FOUND  );

	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	paRowIDs = SG_malloc(sizeof(SG_int64)*iRowIDCount);

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
		"SELECT blobs_rowid FROM blobmap WHERE blobinfo_rowid = ?")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, lBlobInfoRowID)  );

	for (i=0; (rc=sqlite3_step(pStmt)) == SQLITE_ROW; i++)
	{
		SG_int64 rowid = sqlite3_column_int64(pStmt, 0);
		paRowIDs[i] = rowid;
	}
	if (rc != SQLITE_DONE)
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
	SG_ASSERT(i==iRowIDCount);

	if (piRowIDCount)
		*piRowIDCount = iRowIDCount;
	if (ppaRowIDs)
		*ppaRowIDs = paRowIDs;

	return;

fail:
	SG_ERR_IGNORE(  _cleanup_stmt_in_fail(pCtx, pStmt)  );
	SG_NULLFREE(pCtx, paRowIDs);
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
}

void sg_blob_sqlite__fetch_info(SG_context* pCtx,
								sqlite3* psql,
								const char* szHidBlob,
								SG_int64* plBlobInfoRowID,
								SG_blob_encoding* p_blob_encoding,
								SG_uint64* p_len_encoded,
								SG_uint64* p_len_full,
								char** ppsz_hid_vcdiff_reference)
{
	sqlite3_stmt* pStmt = NULL;
	SG_bool b_found = SG_FALSE;
	SG_int64 lBlobInfoRowID = 0;
	SG_blob_encoding blob_encoding = 0;
	SG_uint64 len_encoded = 0;
	SG_uint64 len_full = 0;
	const char* psz_hid_vcdiff_reference = NULL;


	// Get stuff from blobinfo
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt,
		"SELECT rowid, encoding, len_full, len_encoded, hid_vcdiff FROM blobinfo WHERE hid_blob = ? LIMIT 1")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, szHidBlob)  );
	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_ROW)  );

	b_found = SG_TRUE;

	lBlobInfoRowID = sqlite3_column_int64(pStmt, 0);
	blob_encoding = (SG_blob_encoding)sqlite3_column_int(pStmt, 1);
	len_full = sqlite3_column_int64(pStmt, 2);
	len_encoded = sqlite3_column_int64(pStmt, 3);
	psz_hid_vcdiff_reference = (const char*)sqlite3_column_text(pStmt, 4);

	SG_ASSERT(lBlobInfoRowID);

	if (SG_IS_BLOBENCODING_FULL(blob_encoding))
	{
		SG_ASSERT(len_encoded == len_full);
	}

	if (ppsz_hid_vcdiff_reference)
	{
		if (psz_hid_vcdiff_reference)
			SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_hid_vcdiff_reference, ppsz_hid_vcdiff_reference)  );
		else
			*ppsz_hid_vcdiff_reference = NULL;
	}

	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	if (plBlobInfoRowID)
		*plBlobInfoRowID = lBlobInfoRowID;

	if (p_blob_encoding)
		*p_blob_encoding = blob_encoding;

	if (p_len_encoded)
		*p_len_encoded = len_encoded;

	if (p_len_full)
		*p_len_full = len_full;

	return;

fail:
	SG_ERR_IGNORE(  _cleanup_stmt_in_fail(pCtx, pStmt)  );

	if (!b_found)
		SG_ERR_RESET_THROW_RETURN(SG_ERR_BLOB_NOT_FOUND);

	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);

}

void sg_blob_sqlite__sql_blob__get_read_handle(SG_context* pCtx,
											   sqlite3* pSql,
											   SG_int64 lRowID,
											   sqlite3_blob** ppb)
{
	SG_ERR_CHECK_RETURN(  sg_sqlite__blob_open(pCtx, pSql, "main", "blobs", "blob_data", lRowID, 0, ppb)  );
}

void sg_blob_sqlite__sql_blob_create(SG_context* pCtx,
									 sqlite3* pSql,
									 SG_uint32 iLen,
									 SG_int64* plRowID,
									 sqlite3_blob** ppb)
{
	sqlite3_stmt* pStmt = NULL;
	SG_int64 lRowID;
	sqlite3_blob* pb = NULL;

	SG_NULLARGCHECK_RETURN(pSql);

	if (SQLITE_BLOB_MAX_BYTES < iLen)
		SG_ERR_THROW_RETURN(SG_ERR_BUFFERTOOSMALL);

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pSql, &pStmt,
		"INSERT INTO blobs (blob_data) VALUES (?)")  );

	SG_ERR_CHECK(  sg_sqlite__bind_blob__stream(pCtx, pStmt, 1, iLen)  );

	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	SG_ERR_CHECK(  sg_sqlite__last_insert_rowid(pCtx, pSql, &lRowID)  );

	if (0 == lRowID)
		SG_ERR_THROW(  SG_ERR_UNSPECIFIED  );

	// Get sqlite blob handle.
	if (ppb)
	{
		SG_ERR_CHECK(  sg_blob_sqlite__sql_blob__get_write_handle(pCtx, pSql, lRowID, &pb)  );
		*ppb = pb;
	}

	if (plRowID)
		*plRowID = lRowID;

	return;

fail:
	SG_ERR_IGNORE(  _cleanup_stmt_in_fail(pCtx, pStmt)  );
}

void sg_blob_sqlite__sql_blob__get_write_handle(SG_context* pCtx,
												sqlite3* pSql,
												SG_int64 lRowID,
												sqlite3_blob** ppb)
{
	sqlite3_blob * pb = NULL;

	SG_NULLARGCHECK_RETURN(ppb);
	SG_NULLARGCHECK_RETURN(pSql);
	SG_ARGCHECK_RETURN(lRowID != 0, lRowID);

	SG_ERR_CHECK(  sg_sqlite__blob_open(pCtx, pSql, "main", "blobs", "blob_data", lRowID, 1, &pb)  );

	*ppb = pb;
	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__blob_close(pCtx, &pb)  );
}

void sg_blob_sqlite__vacuum(SG_context* pCtx, sqlite3* pSql)
{
	SG_ERR_CHECK_RETURN(  sg_sqlite__exec(pCtx, pSql, "VACUUM")  );
}

void sg_blob_sqlite__delete(SG_context* pCtx, sqlite3* pSql, SG_int64* paRowIDs, SG_uint32 iRowCount)
{
	sqlite3_stmt* pStmt;
	SG_uint32 i;

	SG_NULLARGCHECK_RETURN(pSql);
	SG_NULLARGCHECK_RETURN(paRowIDs);
	SG_ARGCHECK_RETURN(iRowCount, iRowCount);

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pSql, &pStmt, "DELETE FROM blobs WHERE rowid = ?")  );

	for (i=0; i < iRowCount; i++)
	{
		if (i)
		{
			SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
			SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );
		}
		SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 1, paRowIDs[i])  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
	}

	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	return;

fail:
	SG_ERR_IGNORE(  _cleanup_stmt_in_fail(pCtx, pStmt)  );
}
