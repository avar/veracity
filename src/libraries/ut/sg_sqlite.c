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

#if defined(DEBUG)
#	define TRACE_SQLITE				0
#	define TRACE_SQLITE_ERRORS		0
#else
#	define TRACE_SQLITE				0
#	define TRACE_SQLITE_ERRORS		0
#endif

#define SG_SQLITE_ERR_CHECK(expr)			SG_STATEMENT(	rc=(expr);															\
															if (rc)																\
																SG_ERR_THROW2(SG_ERR_SQLITE(rc), (pCtx, sqlite3_errmsg(psql))); )

#define SG_SQLITE_ERR_CHECK_RETURN(expr)	SG_STATEMENT(	rc=(expr);																	\
															if (rc)																		\
																SG_ERR_THROW2_RETURN(SG_ERR_SQLITE(rc), (pCtx, sqlite3_errmsg(psql)));	)

void sg_sqlite__create__pathname(
	SG_context * pCtx,
	const SG_pathname * pPathnameDb,
	sqlite3 **ppDb         /* OUT: SQLite db handle */
)
{
	SG_fsobj_type t;
	SG_fsobj_perms p;
	SG_bool b;
	int rc;

	SG_NULLARGCHECK_RETURN(pPathnameDb);
	SG_NULLARGCHECK_RETURN(ppDb);

	/* First, check to see if this file already exists.  If so, cough up a hairball */

	SG_ERR_CHECK_RETURN(  SG_fsobj__exists__pathname(pCtx, pPathnameDb, &b, &t, &p)  );
	if (b)
	{
		// TODO this is a questionable error code to be throwing;
		// TODO maybe something like EEXIST...
		SG_ERR_THROW_RETURN(  SG_ERR_INVALIDARG  );
	}

	rc = sqlite3_open(SG_pathname__sz(pPathnameDb), ppDb);
	if (rc)
    {
		SG_ERR_THROW_RETURN(SG_ERR_SQLITE(rc));
    }
    SG_ERR_CHECK_RETURN(  sg_sqlite__exec__retry(pCtx, *ppDb, "PRAGMA synchronous=OFF", 100, 5000)  );
}

void sg_sqlite__open__pathname(
	SG_context * pCtx,
	const SG_pathname * pPathnameDb,
	sqlite3 **ppDb         /* OUT: SQLite db handle */
)
{
	int rc;

	SG_NULLARGCHECK_RETURN(pPathnameDb);
	SG_NULLARGCHECK_RETURN(ppDb);

#if TRACE_SQLITE
    fprintf(stderr, "OPEN %s\n", SG_pathname__sz(pPathnameDb));
#endif

	rc = sqlite3_open(SG_pathname__sz(pPathnameDb), ppDb);
	if (rc)
    {
		SG_ERR_THROW_RETURN(  SG_ERR_SQLITE(rc)  );
    }
    SG_ERR_CHECK_RETURN(  sg_sqlite__exec__retry(pCtx, *ppDb, "PRAGMA synchronous=OFF", 100, 5000)  );
}

void sg_sqlite__close(SG_context * pCtx, sqlite3* psql)
{
	int rc;

	if (!psql)
		return;

	rc = sqlite3_close(psql);
	if (rc)
	{
#if TRACE_SQLITE_ERRORS
		// GRIPE it'd be really nice if SQLITE3 had a way to get a sqlite3
		// GRIPE error message given an error code.  as it is, you have to
		// GRIPE get the message immediately after an error -- kind of like
		// GRIPE errno and perror() instead of strerror()...
		fprintf(stderr,"SQLITE3_CLOSE failed: [result %d][%s]\n",
			rc,sqlite3_errmsg(psql));
#endif
		SG_ERR_THROW2_RETURN(SG_ERR_SQLITE(rc), (pCtx, sqlite3_errmsg(psql)));
	}
}

void sg_sqlite__exec__retry(SG_context * pCtx, sqlite3* psql, const char* psz, SG_uint32 sleep_ms, SG_uint32 timeout_ms)
{
	int rc;
    SG_uint32 slept = 0;

#if TRACE_SQLITE
	fprintf(stderr,"Attempting sqlite3_exec(%s)\n",psz);
#endif

    while (1)
    {
        rc = sqlite3_exec(psql, psz, NULL, NULL, NULL);
        if (!rc)
        {
            break;
        }

        if (SQLITE_BUSY == rc)
        {
            if (slept > timeout_ms)
            {
                //SG_ERR_IGNORE(  SG_log(pCtx, "sg_sqlite__exec__retry can't wait any longer\n")  );
                SG_ERR_THROW2_RETURN(  SG_ERR_SQLITE(rc), (pCtx, sqlite3_errmsg(psql))  );
            }

            SG_sleep_ms(sleep_ms);
            slept += sleep_ms;
        }
        else
        {
            //SG_ERR_IGNORE(  SG_log(pCtx, "SQLITE3_EXEC failed: [result %d][%s] for [%s]\n", rc,sqlite3_errmsg(psql),psz)  );
#if TRACE_SQLITE_ERRORS
			// GRIPE it'd be really nice if SQLITE3 had a way to get a sqlite3
			// GRIPE error message given an error code.  as it is, you have to
			// GRIPE get the message immediately after an error -- kind of like
			// GRIPE errno and perror() instead of strerror()...
			fprintf(stderr,"SQLITE3_EXEC failed: [result %d][%s] for [%s]\n",
				rc,sqlite3_errmsg(psql),psz);
#endif
			SG_ERR_THROW2_RETURN(  SG_ERR_SQLITE(rc), (pCtx, sqlite3_errmsg(psql))  );
        }
    }
}

void sg_sqlite__exec(SG_context * pCtx, sqlite3* psql, const char* psz)
{
	int rc;
    SG_int64 t1 = 0;
    SG_int64 t2 = 0;

#if TRACE_SQLITE
	fprintf(stderr,"Attempting sqlite3_exec(%s)\n",psz);
#endif

    SG_time__get_milliseconds_since_1970_utc(pCtx, &t1);
	rc = sqlite3_exec(psql, psz, NULL, NULL, NULL);
    SG_time__get_milliseconds_since_1970_utc(pCtx, &t2);

#if TRACE_SQLITE
	fprintf(stderr,"  query took %d ms\n", (SG_uint32) (t2 - t1));
    {
        SG_uint32 count = 0;

        sg_sqlite__num_changes(pCtx, psql, &count);
        fprintf(stderr,"  num_changes: %d\n", count);
    }
#endif

	if (rc)
	{
		//SG_ERR_IGNORE(  SG_log(pCtx, "SQLITE3_EXEC failed: [result %d][%s] for [%s]\n", rc,sqlite3_errmsg(psql),psz)  );
#if TRACE_SQLITE_ERRORS
		// GRIPE it'd be really nice if SQLITE3 had a way to get a sqlite3
		// GRIPE error message given an error code.  as it is, you have to
		// GRIPE get the message immediately after an error -- kind of like
		// GRIPE errno and perror() instead of strerror()...
		fprintf(stderr,"SQLITE3_EXEC failed: [result %d][%s] for [%s]\n",
			rc,sqlite3_errmsg(psql),psz);
#endif
		SG_ERR_THROW2_RETURN(  SG_ERR_SQLITE(rc), (pCtx, sqlite3_errmsg(psql))  );
	}
}

void sg_sqlite__exec__string(SG_context * pCtx, sqlite3* psql, SG_uint32* piCount, SG_string** pstrSQL)
{
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, SG_string__sz(*pstrSQL))  );
    if (piCount)
    {
        SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, psql, piCount)  );
    }

fail:
	SG_STRING_NULLFREE(pCtx, *pstrSQL);
	*pstrSQL = NULL;
}

void sg_sqlite__exec__count__va(SG_context * pCtx, sqlite3* psql, SG_uint32* piCount, const char * szFormat, ...)
{
	va_list ap;
	SG_string* pstrSQL = NULL;

	SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC(pCtx, &pstrSQL)  );

	va_start(ap,szFormat);
	SG_string__vsprintf(pCtx, pstrSQL, szFormat, ap);
	va_end(ap);

	SG_ERR_CHECK_CURRENT;

	SG_ERR_CHECK(  sg_sqlite__exec__string(pCtx, psql, piCount, &pstrSQL)  );	// this will free pstrSQL.
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pstrSQL);
}

void sg_sqlite__exec__va(SG_context * pCtx, sqlite3* psql, const char * szFormat, ...)
{
	va_list ap;
	SG_string* pstrSQL = NULL;

	SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC(pCtx, &pstrSQL)  );

	va_start(ap,szFormat);
	SG_string__vsprintf(pCtx, pstrSQL, szFormat, ap);
	va_end(ap);

	SG_ERR_CHECK_CURRENT;

	SG_ERR_CHECK(  sg_sqlite__exec__string(pCtx, psql, NULL, &pstrSQL)  );	// this will free pstrSQL.
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pstrSQL);
}

void sg_sqlite__prepare(SG_context * pCtx, sqlite3* psql, sqlite3_stmt** ppStmt, const char * szFormat, ...)
{
	va_list ap;
	SG_string* pstrSQL = NULL;
	int rc;

	SG_ERR_CHECK_RETURN(  SG_STRING__ALLOC(pCtx, &pstrSQL)  );

	va_start(ap,szFormat);
	SG_string__vsprintf(pCtx, pstrSQL, szFormat, ap);
	va_end(ap);

	SG_ERR_CHECK_CURRENT;

#if TRACE_SQLITE
	fprintf(stderr,"Preparing Statement(%s)\n",SG_string__sz(pstrSQL));
#endif
	SG_SQLITE_ERR_CHECK(  sqlite3_prepare_v2(psql, SG_string__sz(pstrSQL), -1, ppStmt, 0)  );

	/* fall through */
fail:
	SG_STRING_NULLFREE(pCtx, pstrSQL);
}

void sg_sqlite__finalize(SG_context * pCtx, sqlite3_stmt* pStmt)
{
	int rc;

	if (!pStmt)
		return;

	rc = sqlite3_finalize(pStmt);
	if (rc)
		SG_ERR_THROW_RETURN(  SG_ERR_SQLITE(rc)  );
}

void sg_sqlite__nullfinalize(SG_context * pCtx, sqlite3_stmt ** ppStmt)
{
	sg_sqlite__finalize(pCtx, *ppStmt);
	*ppStmt = NULL;

	SG_ERR_CHECK_RETURN_CURRENT;
}

void sg_sqlite__reset(SG_context * pCtx, sqlite3_stmt* pStmt)
{
	int rc = sqlite3_reset(pStmt);
	if (rc)
		SG_ERR_THROW_RETURN(  SG_ERR_SQLITE(rc)  );
}

void sg_sqlite__step(SG_context * pCtx, sqlite3_stmt* pStmt, int rcshould)
{
	int rc;

#if TRACE_SQLITE && (SQLITE_VERSION_NUMBER >= 3006000)
	fprintf(stderr,"Attempting sqlite3_step(%s)\n",sqlite3_sql(pStmt));
#endif

	rc = sqlite3_step(pStmt);

#if TRACE_SQLITE_ERRORS && (SQLITE_VERSION_NUMBER >= 3006000)
	if (rc == SQLITE_BUSY)
	{
		fprintf(stderr,"SQLITE3_STEP failed: [result %d][BUSY] for [%s]\n",
				rc,sqlite3_sql(pStmt));
	}
#endif

	if (rc != rcshould)
		SG_ERR_THROW_RETURN(  SG_ERR_SQLITE(rc)  );
}

void sg_sqlite__bind_blob__string(SG_context * pCtx, sqlite3_stmt* pStmt, SG_uint32 ndx, SG_string* pStr)
{
	int rc;
	const char* psz = NULL;
	SG_uint32 len;

	psz = SG_string__sz(pStr);
	len = SG_string__length_in_bytes(pStr);

#if TRACE_SQLITE
	fprintf(stderr,"Binding[%d] blob...\n",ndx);
#endif

	rc = sqlite3_bind_blob(pStmt, ndx, psz, len, SQLITE_STATIC);
	if (rc)
		SG_ERR_THROW_RETURN(  SG_ERR_SQLITE(rc)  );
}

void sg_sqlite__bind_blob__stream(SG_context * pCtx, sqlite3_stmt* pStmt, SG_uint32 ndx, SG_uint32 lenFull)
{
	int rc;

#if TRACE_SQLITE
	fprintf(stderr,"Binding[%d] blob of length %d...\n",ndx, lenFull);
#endif

	rc = sqlite3_bind_zeroblob(pStmt, ndx, lenFull);
	if (rc)
		SG_ERR_THROW_RETURN(  SG_ERR_SQLITE(rc)  );
}

void sg_sqlite__bind_int(SG_context * pCtx, sqlite3_stmt* pStmt, SG_uint32 ndx, SG_int32 v)
{
	int rc;

#if TRACE_SQLITE
    fprintf(stderr,"Binding[%d] int[%d]...\n",ndx,v);
#endif

	rc = sqlite3_bind_int(pStmt, ndx, v);
	if (rc)
		SG_ERR_THROW_RETURN(  SG_ERR_SQLITE(rc)  );
}

void sg_sqlite__bind_int64(SG_context * pCtx, sqlite3_stmt* pStmt, SG_uint32 ndx, SG_int64 v)
{
	int rc;

#if TRACE_SQLITE
	{
        SG_int_to_string_buffer buf_int64;

        SG_int64_to_sz(v, buf_int64);
		SG_ERR_DISCARD;
		fprintf(stderr,"Binding[%d] int64[%s]...\n",ndx,buf_int64);
	}
#endif

	rc = sqlite3_bind_int64(pStmt, ndx, v);
	if (rc)
		SG_ERR_THROW_RETURN(  SG_ERR_SQLITE(rc)  );
}

void sg_sqlite__bind_null(SG_context * pCtx, sqlite3_stmt* pStmt, SG_uint32 ndx)
{
	int rc;

#if TRACE_SQLITE
	fprintf(stderr,"Binding[%d] null\n",ndx);
#endif

	rc = sqlite3_bind_null(pStmt, ndx);
	if (rc)
		SG_ERR_THROW_RETURN(  SG_ERR_SQLITE(rc)  );
}

void sg_sqlite__bind_text(SG_context * pCtx, sqlite3_stmt* pStmt, SG_uint32 ndx, const char* psz)
{
	int rc;

#if TRACE_SQLITE
	fprintf(stderr,"Binding[%d] text(%s)\n",ndx,psz);
#endif

	rc = sqlite3_bind_text(pStmt, ndx, psz, -1, SQLITE_STATIC);
	if (rc)
		SG_ERR_THROW_RETURN(  SG_ERR_SQLITE(rc)  );
}

void sg_sqlite__clear_bindings(SG_context * pCtx, sqlite3_stmt* pStmt)
{
	int rc = sqlite3_clear_bindings(pStmt);
	if (rc)
		SG_ERR_THROW_RETURN(  SG_ERR_SQLITE(rc)  );
}

void sg_sqlite__exec__va__int64(SG_context * pCtx, sqlite3* psql, SG_int64* piResult, const char * szFormat, ...)
{
	sqlite3_stmt* pStmt = NULL;
	va_list ap;
	SG_string* pstrSQL = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstrSQL)  );

	va_start(ap,szFormat);
	SG_string__vsprintf(pCtx, pstrSQL, szFormat, ap);
	va_end(ap);

	SG_ERR_CHECK_CURRENT;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt, SG_string__sz(pstrSQL))  );
	SG_STRING_NULLFREE(pCtx, pstrSQL);

	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_ROW)  );

	*piResult = (SG_int64)sqlite3_column_int64(pStmt,0);

	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pstrSQL);
}

void sg_sqlite__exec__va__int32(SG_context * pCtx, sqlite3* psql, SG_int32* piResult, const char * szFormat, ...)
{
	sqlite3_stmt* pStmt = NULL;
	va_list ap;
	SG_string* pstrSQL = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstrSQL)  );

	va_start(ap,szFormat);
	SG_string__vsprintf(pCtx, pstrSQL, szFormat, ap);
	va_end(ap);

	SG_ERR_CHECK_CURRENT;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt, SG_string__sz(pstrSQL))  );
	SG_STRING_NULLFREE(pCtx, pstrSQL);

	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_ROW)  );

	*piResult = (SG_int32)sqlite3_column_int(pStmt,0);

	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pstrSQL);
}

void sg_sqlite__exec__va__string(SG_context * pCtx, sqlite3* psql, SG_string** ppstrResult, const char * szFormat, ...)
{
	sqlite3_stmt* pStmt = NULL;
	va_list ap;
	SG_string* pstrSQL = NULL;
	const char* pszResult = NULL;
	SG_string* pstrResult = NULL;

	va_start(ap,szFormat);
	SG_STRING__ALLOC__VFORMAT(pCtx, &pstrSQL, szFormat, ap);
	va_end(ap);

	SG_ERR_CHECK_CURRENT;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt, SG_string__sz(pstrSQL))  );
	SG_STRING_NULLFREE(pCtx, pstrSQL);

	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_ROW)  );

	pszResult = (const char*) sqlite3_column_text(pStmt, 0);

	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pstrResult, pszResult)  );

	*ppstrResult = pstrResult;

	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pstrSQL);
}

void sg_sqlite__num_changes(SG_context * pCtx, sqlite3* psql, SG_uint32* piResult)
{
	SG_NULLARGCHECK_RETURN(piResult);

	*piResult = (SG_uint32) sqlite3_changes(psql);
}

void sg_sqlite__last_insert_rowid(SG_context * pCtx, sqlite3* psql, SG_int64* piResult)
{
	SG_NULLARGCHECK_RETURN(piResult);

	*piResult = sqlite3_last_insert_rowid(psql);
}

void sg_sqlite__blob_open(SG_context * pCtx, sqlite3* psql, const char* pszDB, const char* pszTable, const char* pszColumn, SG_int64 iRow, int writeable, sqlite3_blob** ppBlob)
{
	int rc;
	SG_SQLITE_ERR_CHECK_RETURN(  sqlite3_blob_open(psql, pszDB, pszTable, pszColumn, iRow, writeable, ppBlob)  );
}

void sg_sqlite__blob_write(SG_context * pCtx, sqlite3* psql, sqlite3_blob* pBlob, const void* pvData, SG_uint32 iLen, SG_uint32 iOffset)
{
	int rc;

	SG_NULLARGCHECK_RETURN(psql);

#if TRACE_SQLITE
	fprintf(stderr,"Attempting to write %d bytes to blob at offset %d...\n",iLen,iOffset);
#endif

	SG_SQLITE_ERR_CHECK_RETURN(  sqlite3_blob_write(pBlob, pvData, iLen, iOffset)  );

#if TRACE_SQLITE
	fprintf(stderr,"Successfully wrote %d bytes to blob at offset %d.\n",iLen,iOffset);
#endif
}

void sg_sqlite__blob_read(SG_context * pCtx, sqlite3* psql, sqlite3_blob* pBlob, void* pvData, SG_uint32 iLen, SG_uint32 iOffset)
{
	int rc;

	SG_NULLARGCHECK_RETURN(psql);

#if TRACE_SQLITE
	fprintf(stderr,"Attempting to read %d bytes from blob at offset %d...\n",iLen,iOffset);
#else
	psql = NULL;	// quiets compiler when trace is off
#endif

	rc = sqlite3_blob_read(pBlob, pvData, iLen, iOffset);
	if (SQLITE_OK == rc)
	{
#if TRACE_SQLITE
		fprintf(stderr,"Successfully read %d bytes from blob at offset %d.\n",iLen,iOffset);
#endif
		return;
	}
	else
	{
#if TRACE_SQLITE_ERRORS
		fprintf(stderr,"Failed to read from blob: [result %d][%s]\n",
			rc,sqlite3_errmsg(psql));
#endif
		SG_ERR_THROW2_RETURN(  SG_ERR_SQLITE(rc), (pCtx,sqlite3_errmsg(psql))  );
	}
}

void sg_sqlite__blob_close(SG_UNUSED_PARAM(SG_context * pCtx), sqlite3_blob** pBlob)
{
	SG_UNUSED(pCtx);

	// Per http://www.sqlite.org/c3ref/blob_close.html, the blob is closed unconditionally.  There's no action to take on an error.
	// As a result, this routine need not return an error code or take an SG_context*.  This makes many several fail blocks quite
	// a bit simpler in the sqlite repo.

	if (pBlob && *pBlob)
	{
		sqlite3_blob_close(*pBlob);
		*pBlob = NULL;
	}
}

void sg_sqlite__blob_bytes(SG_context * pCtx, sqlite3_blob* pBlob, SG_uint32* piLen)
{
	SG_NULLARGCHECK_RETURN(pBlob);
	SG_NULLARGCHECK_RETURN(piLen);

	*piLen = sqlite3_blob_bytes(pBlob);
}
