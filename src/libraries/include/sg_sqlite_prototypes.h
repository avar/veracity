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
 * @file sg_sqlite_prototypes.h
 *
 * @details These routines are intended to make it easier to
 * use sqlite in libsg.  Several of them do little more than
 * wrap sqlite calls in a way that returns SG_error.
 *
 * This is not a complete wrapper.  sqlite3.h still needs
 * to be included (and is, in sg.h) because lots of sqlite
 * typedefs are still used (like sqlite3 and sqlite3_stmt).
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_SQLITE_PROTOTYPES_H
#define H_SG_SQLITE_PROTOTYPES_H

BEGIN_EXTERN_C;

void sg_sqlite__create__pathname(
	SG_context * pCtx,
	const SG_pathname * pPathnameDb,
	sqlite3 **ppDb         /* OUT: SQLite db handle */
	);

void sg_sqlite__open__pathname(
	SG_context * pCtx,
	const SG_pathname * pPathnameDb,
	sqlite3 **ppDb         /* OUT: SQLite db handle */
	);

void sg_sqlite__close(SG_context * pCtx, sqlite3* psql);

void sg_sqlite__exec(SG_context * pCtx, sqlite3* psql, const char* psz);
void sg_sqlite__exec__retry(SG_context * pCtx, sqlite3* psql, const char* psz, SG_uint32 sleep_ms, SG_uint32 timeout_ms);

void sg_sqlite__exec__string(SG_context * pCtx, sqlite3* psql, SG_uint32* piCount, SG_string** pstrSQL);

void sg_sqlite__exec__count__va(SG_context * pCtx, sqlite3* psql, SG_uint32* picount, const char * szFormat, ...);
void sg_sqlite__exec__va(SG_context * pCtx, sqlite3* psql, const char * szFormat, ...);

void sg_sqlite__prepare(SG_context * pCtx, sqlite3* psql, sqlite3_stmt** ppStmt, const char * szFormat, ...);

void sg_sqlite__finalize(SG_context * pCtx, sqlite3_stmt* pStmt);

void sg_sqlite__nullfinalize(SG_context * pCtx, sqlite3_stmt ** ppStmt);

void sg_sqlite__reset(SG_context * pCtx, sqlite3_stmt* pStmt);

void sg_sqlite__step(
	SG_context * pCtx,
	sqlite3_stmt* pStmt,
	int rcshould  /**< Return an error if the rc code returned is not equal to rcshould */
	);

void sg_sqlite__bind_int64(SG_context * pCtx, sqlite3_stmt* pStmt, SG_uint32 ndx, SG_int64 v);
void sg_sqlite__bind_int(SG_context * pCtx, sqlite3_stmt* pStmt, SG_uint32 ndx, SG_int32 v);

void sg_sqlite__bind_null(SG_context * pCtx, sqlite3_stmt* pStmt, SG_uint32 ndx);

void sg_sqlite__bind_text(SG_context * pCtx, sqlite3_stmt* pStmt, SG_uint32 ndx, const char* psz);

void sg_sqlite__bind_blob__string(SG_context * pCtx, sqlite3_stmt* pStmt, SG_uint32 ndx, SG_string* pStr);
void sg_sqlite__bind_blob__stream(SG_context * pCtx, sqlite3_stmt* pStmt, SG_uint32 ndx, SG_uint32 lenFull);

void sg_sqlite__clear_bindings(SG_context * pCtx, sqlite3_stmt* pStmt);

void sg_sqlite__exec__va__int32(SG_context * pCtx, sqlite3* psql, SG_int32* piResult, const char * szFormat, ...);
void sg_sqlite__exec__va__int64(SG_context * pCtx, sqlite3* psql, SG_int64* piResult, const char * szFormat, ...);

void sg_sqlite__exec__va__string(SG_context * pCtx, sqlite3* psql, SG_string** ppstrResult, const char * szFormat, ...);

void sg_sqlite__num_changes(SG_context * pCtx, sqlite3* psql, SG_uint32* piResult);
void sg_sqlite__last_insert_rowid(SG_context * pCtx, sqlite3* psql, SG_int64* plResult);

void sg_sqlite__blob_open(SG_context * pCtx, sqlite3* psql, const char* pszDB, const char* pszTable, const char* pszColumn, SG_int64 iRow, int writeable, sqlite3_blob** ppBlob);
void sg_sqlite__blob_write(SG_context * pCtx, sqlite3* psql, sqlite3_blob* pBlob, const void* pvData, SG_uint32 iLen, SG_uint32 iOffset);
void sg_sqlite__blob_read(SG_context * pCtx, sqlite3* psql, sqlite3_blob* pBlob, void* pvData, SG_uint32 iLen, SG_uint32 iOffset);
void sg_sqlite__blob_bytes(SG_context * pCtx, sqlite3_blob* pBlob, SG_uint32* piLen);
void sg_sqlite__blob_close(SG_context * pCtx, sqlite3_blob** pBlob);

END_EXTERN_C;

#endif//H_SG_SQLITE_PROTOTYPES_H
