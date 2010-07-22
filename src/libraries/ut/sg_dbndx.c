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

#include "sg_zing__private.h"

#define SG_DB_SORT_DESC     1
#define SG_DB_SORT_NUMERIC  2

struct _SG_dbndx
{
	SG_repo* pRepo;
    SG_uint32 iDagNum;

	SG_pathname* pPath_db;  /* the sqlite db file */

    sqlite3* psql;
	SG_bool			bInTransaction;

    SG_bool bQueryOnly;
    char* psz_cur_state_csid;
    SG_int32 i_cur_state_generation;
};

struct SG_dbndx_qresult
{
    SG_dbndx* pndx;

    SG_uint32 count_results;

    SG_vhash* pvh_fields;
    SG_bool b_want_rechid;
    SG_bool b_want_history;
    SG_bool b_flat_result;

	sqlite3_stmt* pStmt_pairs;

    SG_uint32 last_ordnum_p;
    const char* last_rechid_p;
    const char* last_name_p;
    const char* last_strvalue_p;
    SG_int64 last_ival_p;

	sqlite3_stmt* pStmt_history;

    const char* last_recid_h;
    const char* last_rechid_h;
    const char* last_csid_h;
    SG_int32 last_generation_h;

    SG_vhash* pvh_audits;
};

static void sg_dbndx__apply_one_delta(
        SG_context* pCtx,
        sqlite3* psql,
        char* psz_table_name,
        const char* psz_deltaid,
        SG_bool b_reverse
        );

static void sg_dbndx_qresult__alloc(
        SG_context* pCtx,
        SG_dbndx** ppndx,
        const char* psz_table_name,
        SG_stringarray* psa_slice_fields,
        SG_uint32 count,
        SG_dbndx_qresult** ppqr
        );

static void sg_dbndx__create_db(
        SG_context* pCtx,
        SG_dbndx* pdbc
        );

static void sg_dbndx__do_where(
        SG_context* pCtx,
        sqlite3* psql,
        const SG_varray* pcrit,
        char* pszTempTableName,
        SG_uint32* piCount
        );

static void sg_dbndx__do_state_filter(
        SG_context* pCtx,
        SG_dbndx* pndx,
        const char* pidState,
        char* pszResultsTableName,
        SG_uint32* piCount
        );

//////////////////////////////////////////////////////////////////
// All of the code in this file creates various temporary
// tables using buffers defined with the following size.
// Previously, this was a 't' + GID.
//
// We now have a real TID routine, so we use it instead.

#define sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH      SG_TID_MAX_BUFFER_LENGTH

// TODO By default, we get a full GID-sized TID.  Later,
// TODO consider using SG_tid__generate2() form and create
// TODO a shorter ID.

static void sg_dbndx__sql__get_temptable_name(SG_context* pCtx, char* pszTempTableName)
{
	SG_ERR_CHECK_RETURN(  SG_tid__generate(pCtx, pszTempTableName, sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH)  );
}

#define MY_SLEEP_MS      100
#define MY_TIMEOUT_MS  16000

static void sg_dbndx__read_cur_state(
	SG_context* pCtx,
    SG_dbndx* pdbc,
    char** ppsz,
    SG_int32* pigen
    )
{
	sqlite3_stmt* pStmt = NULL;
    const char* psz_val = NULL;
    char* psz_copy = NULL;
    int rc = 0;
    SG_int32 generation = -1;

    // this function is called when the sqlite db is opened, whether this
    // is for read or write.  this SELECT statement occurs in its own tx.

    SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pdbc->psql, &pStmt, "SELECT csid,generation FROM cur_state")  );
	if ((rc = sqlite3_step(pStmt)) == SQLITE_ROW)
    {
        psz_val = (const char*) sqlite3_column_text(pStmt,0);
        generation = (SG_int32) sqlite3_column_int(pStmt,1);

        SG_ERR_CHECK(  SG_strdup(pCtx, psz_val, &psz_copy)  );
    }
    else if (SQLITE_DONE != rc)
    {
		SG_ERR_THROW(SG_ERR_SQLITE(rc));
    }

    *ppsz = psz_copy;
    psz_copy = NULL;

    *pigen = generation;

fail:
    SG_NULLFREE(pCtx, psz_copy);
    if (pStmt)
    {
        SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );
    }
}

void SG_dbndx__open(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_uint32 iDagNum,
	SG_pathname* pPath,
	SG_bool bQueryOnly,
	SG_dbndx** ppNew
	)
{
	SG_dbndx* pdbc = NULL;
    SG_bool b_exists = SG_FALSE;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pdbc)  );

    pdbc->pRepo = pRepo;
    pdbc->pPath_db = pPath;
    pPath = NULL;
    pdbc->iDagNum = iDagNum;
    pdbc->bQueryOnly = bQueryOnly;
	pdbc->bInTransaction = SG_FALSE;

    if (SG_DAGNUM__IS_AUDIT(iDagNum))
    {
        SG_uint32 iBaseDagNum = 0;

        iBaseDagNum = iDagNum & ~(SG_DAGNUM__BIT__AUDIT);

        if (SG_DAGNUM__IS_DB(iBaseDagNum))
        {
            SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
        }
    }

    SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pdbc->pPath_db, &b_exists, NULL, NULL)  );

    if (b_exists)
    {
        SG_ERR_CHECK(  sg_sqlite__open__pathname(pCtx, pdbc->pPath_db, &pdbc->psql)  );
        // This is done in sg_sqlite.c now
        // SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "PRAGMA synchronous=OFF")  );
        SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "PRAGMA temp_store=2")  );
    }
    else
    {
        SG_ERR_CHECK(  sg_sqlite__create__pathname(pCtx, pdbc->pPath_db,&pdbc->psql)  );
        // This is done in sg_sqlite.c now
        // SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "PRAGMA synchronous=OFF")  );
        SG_ERR_CHECK(  sg_dbndx__create_db(pCtx, pdbc)  );
        SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "PRAGMA temp_store=2")  );
    }
    SG_ERR_CHECK(  sg_dbndx__read_cur_state(pCtx, pdbc, &pdbc->psz_cur_state_csid, &pdbc->i_cur_state_generation)  );

	*ppNew = pdbc;
    pdbc = NULL;

	return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_DBNDX_NULLFREE(pCtx, pdbc);
}

void SG_dbndx__free(SG_context* pCtx, SG_dbndx* pdbc)
{
	if (!pdbc)
		return;

	SG_ERR_IGNORE(  sg_sqlite__close(pCtx, pdbc->psql)  );

    SG_NULLFREE(pCtx, pdbc->psz_cur_state_csid);
	SG_PATHNAME_NULLFREE(pCtx, pdbc->pPath_db);

	SG_NULLFREE(pCtx, pdbc);
}

static void sg_dbndx__slurp_the_audits(
        SG_context* pCtx,
        SG_dbndx* pdbc,
        const char* psz_audits_table,
        SG_vhash** ppvh_audits
        )
{
    int rc;
    SG_vhash* pvh_one_audit = NULL;
    SG_vhash* pvh = NULL;
    SG_varray* pva_one_csid = NULL;
	sqlite3_stmt* pStmt = NULL;
    char buf_cur_csid[SG_HID_MAX_BUFFER_LENGTH];

    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );

    SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pdbc->psql, &pStmt, "SELECT csid, userid, timestamp FROM %s ORDER BY csid ASC", psz_audits_table)  );

    buf_cur_csid[0] = 0;
	while ((rc = sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char* psz_csid = (const char*) sqlite3_column_text(pStmt, 0);
		const char* psz_who = (const char*) sqlite3_column_text(pStmt, 1);
		SG_int64 i_when = sqlite3_column_int64(pStmt, 2);

        if (
                !(buf_cur_csid[0])
                || (0 != strcmp(buf_cur_csid, psz_csid))
           )
        {
            SG_ERR_CHECK(  SG_vhash__addnew__varray(pCtx, pvh, psz_csid, &pva_one_csid)  );
            SG_ERR_CHECK(  SG_strcpy(pCtx, buf_cur_csid, sizeof(buf_cur_csid), psz_csid)  );
        }

        SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva_one_csid, &pvh_one_audit)  );

        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_one_audit, "csid", psz_csid)  );
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_one_audit, "who", psz_who)  );
        SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_one_audit, "when", i_when)  );
	}

	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(SG_ERR_SQLITE(rc));
	}

	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );
    pStmt = NULL;

    *ppvh_audits = pvh;
    pvh = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_dbndx__lookup_audits(
        SG_context* pCtx, 
        SG_dbndx* pdbc, 
        const char* psz_csid,
        SG_varray** ppva
        )
{
    int rc;
    SG_varray* pva = NULL;
	sqlite3_stmt* pStmt = NULL;
    
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );

    SG_ERR_CHECK(  sg_sqlite__exec__retry(pCtx, pdbc->psql, "BEGIN TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );
	pdbc->bInTransaction = SG_TRUE;

    SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pdbc->psql, &pStmt, "SELECT userid, timestamp FROM db_audits WHERE csid = ? ORDER BY timestamp DESC")  );
    SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, psz_csid)  );

	while ((rc = sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char* psz_who = (const char*) sqlite3_column_text(pStmt, 0);
		SG_int64 i_when = sqlite3_column_int64(pStmt, 1);
        SG_vhash* pvh_audit = NULL;

        SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva, &pvh_audit)  );
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_audit, "who", psz_who)  );
        SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_audit, "when", i_when)  );
	}

	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(SG_ERR_SQLITE(rc));
	}
	
	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );
    pStmt = NULL;

    *ppva = pva;
    pva = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_ERR_IGNORE(  sg_sqlite__exec__retry(pCtx, pdbc->psql, "ROLLBACK TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );
	pdbc->bInTransaction = SG_FALSE;
}

void SG_dbndx__query_record_history(
        SG_context* pCtx,
        SG_dbndx* pdbc,
        const char* psz_recid,
        SG_varray** ppva
        )
{
    int rc;
    SG_vhash* pvh_history_entry = NULL;
    SG_varray* pva = NULL;
	sqlite3_stmt* pStmt = NULL;
    char sz_audits_table[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
    SG_vhash* pvh_audits = NULL;

    SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, sz_audits_table)  );

    SG_ERR_CHECK(  sg_sqlite__exec__retry(pCtx, pdbc->psql, "BEGIN TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );
	pdbc->bInTransaction = SG_TRUE;

    SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pdbc->psql, "CREATE TEMP TABLE %s (csid VARCHAR NOT NULL, userid VARCHAR NOT NULL, timestamp INTEGER NOT NULL)", sz_audits_table)  );

    // TODO SELECT DISTINCT?  probbly not
    SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pdbc->psql, "INSERT INTO %s SELECT DISTINCT a.csid, a.userid, a.timestamp FROM db_audits a,db_history h WHERE a.csid = h.csid AND h.recid = '%s'", sz_audits_table, psz_recid)  );

    SG_ERR_CHECK(  sg_dbndx__slurp_the_audits(pCtx, pdbc, sz_audits_table, &pvh_audits)  );

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );

    SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pdbc->psql, &pStmt, "SELECT hidrec, csid, generation FROM db_history WHERE recid = ? ORDER BY generation DESC")  );
    SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, psz_recid)  );

	while ((rc = sqlite3_step(pStmt)) == SQLITE_ROW)
	{
        SG_bool b_has_audits = SG_FALSE;
		const char* psz_hid_rec = (const char*) sqlite3_column_text(pStmt, 0);
		const char* psz_csid = (const char*) sqlite3_column_text(pStmt, 1);
		SG_int32 generation = sqlite3_column_int(pStmt, 2);

		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_history_entry)  );

		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_history_entry, "hid", psz_hid_rec)  );
		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_history_entry, "csid", psz_csid)  );
		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_history_entry, "generation", (SG_int64) generation)  );

        // add the audits here
        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_audits, psz_csid, &b_has_audits)  );
        if (b_has_audits)
        {
            SG_varray* pva_audits = NULL;
            SG_varray* pva_copy_audits = NULL;
            SG_uint32 count_audits = 0;
            SG_uint32 i = 0;

            SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_audits, psz_csid, &pva_audits)  );
            SG_ERR_CHECK(  SG_varray__count(pCtx, pva_audits, &count_audits)  );
            SG_ERR_CHECK(  SG_vhash__addnew__varray(pCtx, pvh_history_entry, "audits", &pva_copy_audits)  );
            for (i=0; i<count_audits; i++)
            {
                SG_vhash* pvh_one_audit = NULL;
                SG_vhash* pvh_copy_audit = NULL;
                const char* psz_who = NULL;
                SG_int64 i_when = 0;

                SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_audits, i, &pvh_one_audit)  );
                SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva_copy_audits, &pvh_copy_audit)  );
                SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_one_audit, "who", &psz_who)  );
                SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_one_audit, "when", &i_when)  );
                SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_copy_audit, "who", psz_who)  );
                SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_copy_audit, "when", i_when)  );
            }
        }

		SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pva, &pvh_history_entry)  );
	}

	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(SG_ERR_SQLITE(rc));
	}

	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );
    pStmt = NULL;

    *ppva = pva;
    pva = NULL;

fail:
    SG_ERR_IGNORE(  sg_sqlite__exec__retry(pCtx, pdbc->psql, "ROLLBACK TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );
	pdbc->bInTransaction = SG_FALSE;

    SG_VHASH_NULLFREE(pCtx, pvh_audits);
    SG_VHASH_NULLFREE(pCtx, pvh_history_entry);
    SG_VARRAY_NULLFREE(pCtx, pva);
}

static void sg_dbndx__add_record_to_history_table(
        SG_context* pCtx,
        SG_dbndx* pdbc,
        SG_dbrecord* prec,
        const char* psz_csid,
        SG_int32 generation
        )
{
	sqlite3_stmt* pStmt = NULL;
    const char* psz_recid = NULL;
    SG_bool b_recid = SG_FALSE;

    SG_ERR_CHECK(  SG_dbrecord__check_value(pCtx, prec, SG_ZING_FIELD__RECID, &b_recid, &psz_recid)  );
    if (b_recid)
    {
        const char* psz_hid = NULL;

        SG_ASSERT(psz_recid);

        SG_ERR_CHECK(  SG_dbrecord__get_hid__ref(pCtx, prec, &psz_hid)  );

        SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pdbc->psql, &pStmt, "INSERT OR IGNORE INTO db_history (recid, hidrec, csid, generation) VALUES (?, ?, ?, ?)")  );

		SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, psz_recid)  );
		SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, psz_hid)  );
		SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 3, psz_csid)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 4, generation)  );

		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
    }

fail:
	if (pStmt)
	{
		SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx,pStmt)  );
	}
}

static void sg_dbndx__add_record_to_audits(SG_context* pCtx, SG_dbrecord* prec, SG_varray* pva_audits_for_one_dag)
{
    SG_vhash* pvh_one_audit = NULL;
    const char* psz_csid = NULL;
    const char* psz_who = NULL;
    SG_int64 i_when = 0;

    SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, prec, "csid", &psz_csid)  );
    SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, prec, "who", &psz_who)  );
    SG_ERR_CHECK(  SG_dbrecord__get_value__int64(pCtx, prec, "when", &i_when)  );

    SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva_audits_for_one_dag, &pvh_one_audit)  );
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_one_audit, "csid", psz_csid)  );
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_one_audit, "who", psz_who)  );
    SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_one_audit, "when", i_when)  );

fail:
    ;
}

static void sg_dbndx__add_record_to_pairs_table(
        SG_context* pCtx,
        SG_dbrecord* prec,
        SG_zingtemplate* pzt,
        sqlite3_stmt* pStmt
        )
{
	SG_uint32 count;
	SG_uint32 i;
    const char* psz_hid = NULL;
    SG_bool b_has_rectype = SG_FALSE;
    const char* psz_rectype = NULL;

    SG_ERR_CHECK(  SG_dbrecord__get_hid__ref(pCtx, prec, &psz_hid)  );

    // not every dbrecord has a rectype.  links don't.
    SG_ERR_CHECK(  SG_dbrecord__check_value(pCtx, prec, SG_ZING_FIELD__RECTYPE, &b_has_rectype, &psz_rectype)  );

	SG_ERR_CHECK(  SG_dbrecord__count_pairs(pCtx, prec, &count)  );


	for (i=0; i<count; i++)
	{
		const char* psz_name;
		const char* psz_value;
        SG_zingfieldattributes* pzfa = NULL;

		SG_ERR_CHECK(  SG_dbrecord__get_nth_pair(pCtx, prec, i, &psz_name, &psz_value)  );

        if (pzt && psz_rectype)
        {
            SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, psz_rectype, psz_name, &pzfa)  );
        }

        SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
        SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );
        SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, psz_hid)  );
        SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, psz_name)  );
        SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 3, psz_value)  );

        if (
                pzfa
                && (
                    (pzfa->type == SG_ZING_TYPE__INT)
                    || (pzfa->type == SG_ZING_TYPE__BOOL)
                    || (pzfa->type == SG_ZING_TYPE__DATETIME)
                   )
           )
        {
            SG_int64 intvalue = 0;

            SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &intvalue, psz_value)  );
            SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 4, intvalue)  );
        }
        else if (
                pzfa
                && (pzfa->type == SG_ZING_TYPE__STRING)
                && (pzfa->v._string.allowed)
                )
        {
            SG_int64 ndx = 0;

            SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pzfa->v._string.allowed, psz_value, &ndx)  );
            SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 4, ndx)  );
        }
        else
        {
            SG_context__err_reset(pCtx);
            SG_ERR_CHECK(  sg_sqlite__bind_null(pCtx, pStmt, 4)  );
        }

        SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
	}


fail:
    ;
}

static void sg_dbndx__create_db(SG_context* pCtx, SG_dbndx* pdbc)
{
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE TABLE db_audits (csid VARCHAR NOT NULL UNIQUE, userid VARCHAR NOT NULL, timestamp INTEGER NOT NULL)")  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE TABLE db_history (recid VARCHAR NOT NULL, hidrec VARCHAR NOT NULL, csid VARCHAR NOT NULL, generation INTEGER NOT NULL, UNIQUE (recid, hidrec))")  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE INDEX IF NOT EXISTS db_history_recid ON db_history (recid)")  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE INDEX IF NOT EXISTS db_history_hidrec ON db_history (hidrec)")  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE INDEX IF NOT EXISTS db_history_generation ON db_history (generation)")  );

	/*
	 * The db_pairs table is used store all of the individual
	 * name-value pairs in all of the dbrecords.  If a dbrecord has 9
	 * pairs, then it will have 9 entries in this table.  The value is
	 * stored twice, once as a string, and once as an integer.  This
	 * allows numeric types to be properly sorted and queried.
	 *
	 * */

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE TABLE db_pairs (hidrec VARCHAR NOT NULL, name VARCHAR NOT NULL, strvalue VARCHAR NOT NULL, intvalue INTEGER NULL, UNIQUE (hidrec, name, strvalue))")  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE INDEX IF NOT EXISTS db_pairs_name ON db_pairs (name)")  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE INDEX IF NOT EXISTS db_pairs_strvalue ON db_pairs (strvalue)")  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE INDEX IF NOT EXISTS db_pairs_intvalue ON db_pairs (intvalue)")  );


	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE TABLE states (csid VARCHAR PRIMARY KEY NOT NULL, generation INTEGER NOT NULL, baseline VARCHAR NOT NULL)")  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE INDEX states_csid ON states (csid)")  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE INDEX states_generation ON states (generation)")  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE INDEX states_baseline ON states (baseline)")  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE TABLE delta_adds (csid VARCHAR NOT NULL, hidrec VARCHAR NOT NULL)")  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE INDEX delta_adds_csid ON delta_adds (csid)")  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE TABLE delta_removes (csid VARCHAR NOT NULL, hidrec VARCHAR NOT NULL)")  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE INDEX delta_removes_csid ON delta_removes (csid)")  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE TABLE cur_filter (hidrec VARCHAR UNIQUE NOT NULL)")  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pdbc->psql, "CREATE TABLE cur_state (csid VARCHAR NOT NULL, generation INTEGER NOT NULL)")  );

fail:
    ;
}

void SG_dbndx__inject_audits(
	SG_context* pCtx,
	SG_dbndx* pdbc,
    SG_varray* pva_audits
	)
{
    SG_uint32 count = 0;
    SG_uint32 i = 0;
	sqlite3_stmt* pStmt = NULL;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pdbc->psql, &pStmt, "INSERT OR IGNORE INTO db_audits (csid, userid, timestamp) VALUES (?, ?, ?)")  );

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_audits, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh = NULL;
        const char* psz_csid = NULL;
        const char* psz_who = NULL;
        SG_int64 i_when = 0;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_audits, i, &pvh)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "csid", &psz_csid)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "who", &psz_who)  );
        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, "when", &i_when)  );

        SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
        SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );

        SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, psz_csid)  );
        SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, psz_who)  );
        SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 3, i_when)  );

        SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
    }

    SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );
    pStmt = NULL;

fail:
    ;
}

void SG_dbndx__harvest_audits_for_injection(SG_context* pCtx, SG_repo* pRepo, const char* psz_csid, SG_varray* pva_audits)
{
    SG_changeset* pcs = NULL;
    SG_uint32 i;
    SG_uint32 count_blobs = 0;
	SG_dbrecord* pRec = NULL;
    SG_vhash* pvh_lbl = NULL;
    SG_vhash* pvh_dbrecords = NULL;
    const char* psz_key = NULL;
    SG_bool b_has_dbrecords = SG_FALSE;

	SG_NULLARGCHECK_RETURN(psz_csid);
	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pva_audits);

	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, psz_csid, &pcs)  );

    SG_ERR_CHECK(  SG_changeset__get_list_of_bloblists(pCtx, pcs, &pvh_lbl)  );
    SG_ERR_CHECK(  SG_changeset__get_bloblist_name(pCtx, SG_BLOB_REFTYPE__DBRECORD, &psz_key)  );
    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_lbl, psz_key, &b_has_dbrecords)  );
    if (b_has_dbrecords)
    {
        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_lbl, psz_key, &pvh_dbrecords)  );
        SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_dbrecords, &count_blobs)  );
        for (i=0; i<count_blobs; i++)
        {
            const char* psz_hid = NULL;

            SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_dbrecords, i, &psz_hid, NULL)  );
            SG_ERR_CHECK(  SG_dbrecord__load_from_repo(pCtx, pRepo, psz_hid, &pRec)  );
            SG_ERR_CHECK(  sg_dbndx__add_record_to_audits(pCtx, pRec, pva_audits)  );
            SG_DBRECORD_NULLFREE(pCtx, pRec);
        }
    }

fail:
    SG_DBRECORD_NULLFREE(pCtx, pRec);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
}

static void sg_dbndx__read_state_graph(
    SG_context* pCtx,
    SG_dbndx* pndx,
    SG_int32 i_generation_min,
    SG_int32 i_generation_max,
    SG_vhash** ppvh
    )
{
    SG_vhash* pvh = NULL;
	sqlite3_stmt* pStmt = NULL;
    int rc = 0;

    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );

    i_generation_min = 0;
    i_generation_max = 1000000;

    SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pndx->psql, &pStmt, "SELECT csid, baseline, generation FROM states WHERE generation >= %d AND generation <= %d", i_generation_min, i_generation_max)  );
	while ((rc = sqlite3_step(pStmt)) == SQLITE_ROW)
    {
        SG_vhash* pvh_entry = NULL;
        const char* psz_csid = (const char*) sqlite3_column_text(pStmt,0);
        const char* psz_baseline = (const char*) sqlite3_column_text(pStmt,1);
        SG_int32 gen = (SG_int32) sqlite3_column_int(pStmt,2);

        SG_ERR_CHECK(  SG_vhash__addnew__vhash(pCtx, pvh, psz_csid, &pvh_entry)  );
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_entry, "baseline", psz_baseline)  );
        SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_entry, "generation", (SG_int64) gen)  );
    }
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(SG_ERR_SQLITE(rc));
	}
	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

    *ppvh = pvh;
    pvh = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
}

static void sg_dbndx__march_back(
    SG_context* pCtx,
    SG_dbndx* pndx,
    const char* psz_csid,
    SG_int32 i_generation,
    SG_int32 how_far,
    SG_varray** ppva
    )
{
    SG_varray* pva = NULL;
    SG_vhash* pvh_graph = NULL;
    const char* psz_csid_cur = NULL;
    SG_int32 count = 0;

    if (how_far > i_generation)
    {
        how_far = i_generation;
    }

    SG_ERR_CHECK(  sg_dbndx__read_state_graph(pCtx, pndx, i_generation - how_far, i_generation, &pvh_graph)  );
    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva)  );

    psz_csid_cur = psz_csid;
    while (1)
    {
        SG_bool b = SG_FALSE;

        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_graph, psz_csid_cur, &b)  );
        if (b)
        {
            SG_vhash* pvh_entry = NULL;
            SG_vhash* pvh_new = NULL;
            const char* psz_baseline = NULL;
            SG_int64 gen = 0;

            SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_graph, psz_csid_cur, &pvh_entry)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_entry, "baseline", &psz_baseline)  );
            SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_entry, "generation", &gen)  );
            SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva, &pvh_new)  );
            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_new, "csid", psz_csid_cur)  );
            SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_new, "generation", gen)  );
            count++;

            if (count >= how_far)
            {
                break;
            }
            else
            {
                if (0 == strcmp(psz_baseline, "none"))
                {
                    break;
                }

                psz_csid_cur = psz_baseline;
            }
        }
        else
        {
            break;
        }
    }

    *ppva = pva;
    pva = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_VHASH_NULLFREE(pCtx, pvh_graph);
}

static void sg_dbndx__find_ca(
    SG_context* pCtx,
    SG_dbndx* pndx,
    const char* psz_csid_from,
    SG_int32 i_generation_from,
    const char* psz_csid_to,
    SG_int32 i_generation_to,
    SG_int32 how_far,
    char** ppsz_csid_ca,
    SG_int32* pi_generation_ca
    )
{
    SG_varray* pva_back_from = NULL;
    SG_varray* pva_back_to = NULL;
    SG_vhash* pvh_to = NULL;
    SG_uint32 count_from = 0;
    SG_uint32 count_to = 0;
    SG_uint32 i = 0;
    SG_int64 gen_winning = -1;
    const char* psz_csid_winning = NULL;

    SG_ERR_CHECK(  sg_dbndx__march_back(pCtx, pndx, psz_csid_from, i_generation_from, how_far, &pva_back_from)  );
    SG_ERR_CHECK(  sg_dbndx__march_back(pCtx, pndx, psz_csid_to, i_generation_to, how_far, &pva_back_to)  );

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_back_to, &count_to)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_back_from, &count_from)  );

    // now invert one of the lists so we can avoid n^2 loop
    // TODO check for count 0
    i = count_to - 1;

    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_to)  );
    while (1)
    {
        SG_vhash* pvh_entry = NULL;
        const char* psz_csid = NULL;
        SG_int64 gen = 0;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_back_to, i, &pvh_entry)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_entry, "csid", &psz_csid)  );
        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_entry, "generation", &gen)  );
        SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_to, psz_csid, gen)  );

        if (0 == i)
        {
            break;
        }
        i--;
    }

    // find an intersection
    for (i=0; i<count_from; i++)
    {
        SG_vhash* pvh_entry = NULL;
        const char* psz_csid = NULL;
        SG_bool b = SG_FALSE;
        SG_int64 gen = 0;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_back_from, i, &pvh_entry)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_entry, "csid", &psz_csid)  );

        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_to, psz_csid, &b)  );
        if (b)
        {
            SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_entry, "generation", &gen)  );
            if (
                    (gen_winning < 0)
                    || (gen > gen_winning)
               )
            {
                gen_winning = gen;
                psz_csid_winning = psz_csid;
            }
        }
    }

    if (psz_csid_winning)
    {
        SG_ERR_CHECK(  SG_strdup(pCtx, psz_csid_winning, ppsz_csid_ca)  );
        *pi_generation_ca = (SG_int32) gen_winning;
    }

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_to);
    SG_VARRAY_NULLFREE(pCtx, pva_back_from);
    SG_VARRAY_NULLFREE(pCtx, pva_back_to);
}

static void sg_dbndx__find_direct_path(
    SG_context* pCtx,
    SG_dbndx* pndx,
    const char* psz_csid_from,
    SG_int32 i_generation_from,
    const char* psz_csid_to,
    SG_int32 i_generation_to,
    SG_stringarray** ppsa
    )
{
    SG_stringarray* psa_tmp = NULL;
    SG_vhash* pvh_graph = NULL;
    const char* psz_csid_cur = NULL;
    SG_bool b_found = SG_FALSE;

    if (i_generation_to > i_generation_from)
    {
        if (!psz_csid_from)
        {
            psz_csid_from = "none";
        }

        SG_ERR_CHECK(  sg_dbndx__read_state_graph(pCtx, pndx, i_generation_from, i_generation_to, &pvh_graph)  );
        SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_tmp, 1)  );

        psz_csid_cur = psz_csid_to;
        while (1)
        {
            SG_bool b = SG_FALSE;

            SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_graph, psz_csid_cur, &b)  );
            if (b)
            {
                SG_vhash* pvh_entry = NULL;
                const char* psz_baseline = NULL;
                SG_int64 gen = 0;

                SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_graph, psz_csid_cur, &pvh_entry)  );
                SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_entry, "baseline", &psz_baseline)  );
                SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_entry, "generation", &gen)  );

                SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_tmp, psz_csid_cur)  );

                if (0 == strcmp(psz_baseline, psz_csid_from))
                {
                    b_found = SG_TRUE;
                    break;
                }
                else
                {
                    if (0 == strcmp(psz_baseline, "none"))
                    {
                        break;
                    }

                    psz_csid_cur = psz_baseline;
                }
            }
            else
            {
                break;
            }
        }
        }

    if (b_found)
    {
        *ppsa = psa_tmp;
        psa_tmp = NULL;
    }
    else
    {
    }

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_tmp);
    SG_VHASH_NULLFREE(pCtx, pvh_graph);
}

static void sg_dbndx__add_to_path__reverse(
    SG_context* pCtx,
    SG_varray* pva_path,
    SG_stringarray* psa_direct
    )
{
    SG_uint32 count = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_direct, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh_one_step = NULL;
        const char* psz_csid_cur = NULL;

        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa_direct, i, &psz_csid_cur)  );
        SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva_path, &pvh_one_step)  );
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_one_step, "csid", psz_csid_cur)  );
        SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, pvh_one_step, "reverse", SG_TRUE)  );
    }

fail:
    ;
}

static void sg_dbndx__add_to_path__normal(
    SG_context* pCtx,
    SG_varray* pva_path,
    SG_stringarray* psa_direct
    )
{
    SG_uint32 count = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_direct, &count)  );
    i = count-1;
    while (1)
    {
        SG_vhash* pvh_one_step = NULL;
        const char* psz_csid_cur = NULL;

        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa_direct, i, &psz_csid_cur)  );
        SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva_path, &pvh_one_step)  );
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_one_step, "csid", psz_csid_cur)  );
        SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, pvh_one_step, "reverse", SG_FALSE)  );
        if (0 == i)
        {
            break;
        }
        i--;
    }

fail:
    ;
}

static void sg_dbndx__find_ca_path(
    SG_context* pCtx,
    SG_dbndx* pndx,
    const char* psz_csid_from,
    SG_int32 i_generation_from,
    const char* psz_csid_to,
    SG_int32 i_generation_to,
    SG_varray** ppva
    )
{
    char* psz_csid_ca = NULL;
    SG_int32 i_generation_ca = 0;
    SG_stringarray* psa_from_ca = NULL;
    SG_stringarray* psa_to_ca = NULL;
    SG_varray* pva_path = NULL;

    SG_int32 how_far = 10;
    while (1)
    {
        SG_ERR_CHECK(  sg_dbndx__find_ca(
                    pCtx,
                    pndx,
                    psz_csid_from,
                    i_generation_from,
                    psz_csid_to,
                    i_generation_to,
                    how_far,
                    &psz_csid_ca,
                    &i_generation_ca
                    ) );
        if (psz_csid_ca)
        {
            break;
        }
        if (
                (how_far < i_generation_from)
                || (how_far < i_generation_to)
           )
        {
            how_far *= 4;
        }
        else
        {
            break;
        }
    }

    if (psz_csid_ca)
    {
        SG_ERR_CHECK(  sg_dbndx__find_direct_path(
                    pCtx,
                    pndx,
                    psz_csid_ca,
                    i_generation_ca,
                    psz_csid_from,
                    i_generation_from,
                    &psa_from_ca
                    )  );
        SG_ERR_CHECK(  sg_dbndx__find_direct_path(
                    pCtx,
                    pndx,
                    psz_csid_ca,
                    i_generation_ca,
                    psz_csid_to,
                    i_generation_to,
                    &psa_to_ca
                    )  );
        SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_path)  );
        SG_ERR_CHECK(  sg_dbndx__add_to_path__reverse(pCtx, pva_path, psa_from_ca)  );
        SG_ERR_CHECK(  sg_dbndx__add_to_path__normal(pCtx, pva_path, psa_to_ca)  );
    }
    else
    {
        SG_ERR_THROW2(  SG_ERR_ZING_NO_ANCESTOR,
                (pCtx, "from %s to %s", psz_csid_from, psz_csid_to)
                );
    }

    *ppva = pva_path;
    pva_path = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_path);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_from_ca);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_to_ca);
    SG_NULLFREE(pCtx, psz_csid_ca);
}

static void sg_dbndx__find_path(
    SG_context* pCtx,
    SG_dbndx* pndx,
    const char* psz_csid_from,
    SG_int32 i_generation_from,
    const char* psz_csid_to,
    SG_int32 i_generation_to,
    SG_varray** ppva
    )
{
    SG_varray* pva_path = NULL;
    SG_stringarray* psa_direct = NULL;
    SG_bool b_reverse = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pndx);
	SG_NULLARGCHECK_RETURN(psz_csid_to);
	SG_NULLARGCHECK_RETURN(ppva);

    if (i_generation_to > i_generation_from)
    {
        SG_ERR_CHECK(  sg_dbndx__find_direct_path(
                    pCtx,
                    pndx,
                    psz_csid_from,
                    i_generation_from,
                    psz_csid_to,
                    i_generation_to,
                    &psa_direct
                    )  );
    }
    else
    {
        // backward
        SG_ERR_CHECK(  sg_dbndx__find_direct_path(
                    pCtx,
                    pndx,
                    psz_csid_to,
                    i_generation_to,
                    psz_csid_from,
                    i_generation_from,
                    &psa_direct
                    )  );
        b_reverse = SG_TRUE;
    }

    if (psa_direct)
    {
        SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_path)  );
        if (b_reverse)
        {
            SG_ERR_CHECK(  sg_dbndx__add_to_path__reverse(pCtx, pva_path, psa_direct)  );
        }
        else
        {
            SG_ERR_CHECK(  sg_dbndx__add_to_path__normal(pCtx, pva_path, psa_direct)  );
        }

    }
    else
    {
        SG_ERR_CHECK(  sg_dbndx__find_ca_path(
                    pCtx,
                    pndx,
                    psz_csid_from,
                    i_generation_from,
                    psz_csid_to,
                    i_generation_to,
                    &pva_path
                    )  );
    }

    *ppva = pva_path;
    pva_path = NULL;

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_direct);
    SG_VARRAY_NULLFREE(pCtx, pva_path);
}

static void sg_dbndx__walk_path(
    SG_context* pCtx,
    SG_dbndx* pndx,
    SG_varray* pva,
    char* psz_table_name
    )
{
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_vhash* pvh_one_step = NULL;

	SG_NULLARGCHECK_RETURN(pndx);
	SG_NULLARGCHECK_RETURN(pva);
	SG_NULLARGCHECK_RETURN(psz_table_name);

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    for (i=0; i<count; i++)
    {
        const char* psz_csid = NULL;
        SG_bool b_reverse = SG_FALSE;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh_one_step)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_one_step, "csid", &psz_csid)  );
        SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvh_one_step, "reverse", &b_reverse)  );
        SG_ERR_CHECK(  sg_dbndx__apply_one_delta(pCtx, pndx->psql, psz_table_name, psz_csid, b_reverse)  );
    }

fail:
    ;
}

static void sg_dbndx__update_cur_state(SG_context* pCtx, SG_dbndx* pndx, const char* psz_csid, SG_int32 i_generation, const char* psz_baseline)
{
    SG_varray* pva_path = NULL;

	SG_NULLARGCHECK_RETURN(pndx);
	SG_NULLARGCHECK_RETURN(psz_csid);

    if (
            !(pndx->psz_cur_state_csid)
            || (
                psz_baseline
                && pndx->psz_cur_state_csid
                && (0 == strcmp(psz_baseline, pndx->psz_cur_state_csid))
               )
       )
    {
        SG_ERR_CHECK(  sg_dbndx__apply_one_delta(pCtx, pndx->psql, "cur_filter", psz_csid, SG_FALSE)  );
    }
    else
    {
        SG_ERR_CHECK(  sg_dbndx__find_path(
                    pCtx,
                    pndx,
                    pndx->psz_cur_state_csid,
                    pndx->i_cur_state_generation,
                    psz_csid,
                    i_generation,
                    &pva_path
                    ) );

        SG_ERR_CHECK(  sg_dbndx__walk_path(
                    pCtx,
                    pndx,
                    pva_path,
                    "cur_filter"
                    )  );
    }

    if (pndx->psz_cur_state_csid)
    {
        SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pndx->psql, "UPDATE cur_state SET csid='%s', generation=%d", psz_csid, i_generation)  );
    }
    else
    {
        SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pndx->psql, "INSERT INTO cur_state (csid, generation) VALUES ('%s', %d)", psz_csid, i_generation)  );
    }

    // TODO couldn't we just strcpy in, rather than free and re-strdup?
    SG_NULLFREE(pCtx, pndx->psz_cur_state_csid);
    SG_ERR_CHECK(  SG_strdup(pCtx, psz_csid, &pndx->psz_cur_state_csid)  );
    pndx->i_cur_state_generation = i_generation;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_path);
}

void SG_dbndx__update__multiple(SG_context* pCtx, SG_dbndx* pdbc, SG_stringarray* psa)
{
	const char* psz_hid_delta = NULL;
    SG_changeset* pChangeset = NULL;
    SG_uint32 i;
    SG_int32 generation = 0;
	SG_dbrecord* pRec = NULL;
    SG_zingtemplate* pzt = NULL;
    SG_vhash* pvh_delta = NULL;
    SG_varray* pva_record_adds = NULL;
    SG_varray* pva_record_removes = NULL;
    SG_uint32 count_adds = 0;
    SG_uint32 count_removes = 0;
    const char* psz_baseline = NULL;
    SG_uint32 count_changesets = 0;
    SG_uint32 ics = 0;
    const char* psz_csid = NULL;
    const char* psz_hid_template = NULL;
	sqlite3_stmt* pStmt_pairs = NULL;

	SG_NULLARGCHECK_RETURN(pdbc);
	SG_ARGCHECK_RETURN( pdbc->pRepo!=NULL , pdbc );
	SG_NULLARGCHECK_RETURN(psa);

    if (pdbc->bQueryOnly)
    {
		SG_ERR_THROW_RETURN(SG_ERR_DBNDX_ALLOWS_QUERY_ONLY);
    }

    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa, &count_changesets)  );

    SG_ERR_CHECK(  sg_sqlite__exec__retry(pCtx, pdbc->psql, "BEGIN EXCLUSIVE TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );
	pdbc->bInTransaction = SG_TRUE;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pdbc->psql, &pStmt_pairs, "INSERT OR IGNORE INTO db_pairs (hidrec, name, strvalue, intvalue) VALUES (?, ?, ?, ?)")  );
    for (ics=0; ics<count_changesets; ics++)
    {
        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa, ics, &psz_csid)  );
        SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pdbc->pRepo, psz_csid, &pChangeset)  );

        SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pChangeset,&psz_hid_delta)  );
        if (!psz_hid_delta)
        {
            SG_ERR_THROW(  SG_ERR_INVALIDARG  );
        }
        SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, pdbc->pRepo, psz_hid_delta, &pvh_delta)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_delta, "template", &psz_hid_template)  );
        SG_ERR_CHECK(  SG_zing__get_template__hid_template(pCtx, pdbc->pRepo, psz_hid_template, &pzt)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_delta, "baseline", &psz_baseline)  );

        SG_ERR_CHECK(  SG_changeset__get_generation(pCtx, pChangeset, &generation)  );

        SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_delta, "add", &pva_record_adds)  );
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva_record_adds, &count_adds)  );
        for (i=0; i<count_adds; i++)
        {
            const char* psz_hid_rec = NULL;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_record_adds, i, &psz_hid_rec)  );
            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pdbc->psql, "INSERT INTO delta_adds (csid, hidrec) VALUES ('%s', '%s')", psz_csid, psz_hid_rec)  );
            SG_ERR_CHECK(  SG_dbrecord__load_from_repo(pCtx, pdbc->pRepo, psz_hid_rec, &pRec)  );
            SG_ERR_CHECK(  sg_dbndx__add_record_to_pairs_table(pCtx, pRec, pzt, pStmt_pairs)  );
            SG_ERR_CHECK(  sg_dbndx__add_record_to_history_table(pCtx, pdbc, pRec, psz_csid, generation)  );
            SG_DBRECORD_NULLFREE(pCtx, pRec);
        }

        SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_delta, "remove", &pva_record_removes)  );
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva_record_removes, &count_removes)  );
        for (i=0; i<count_removes; i++)
        {
            const char* psz_hid_rec = NULL;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_record_removes, i, &psz_hid_rec)  );
            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pdbc->psql, "INSERT INTO delta_removes (csid, hidrec) VALUES ('%s', '%s')", psz_csid, psz_hid_rec)  );
        }

        SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pdbc->psql, "INSERT INTO states (csid, generation, baseline) VALUES ('%s', %d, '%s')", psz_csid, generation, psz_baseline ? psz_baseline : "none")  );

        // TODO on a clone, it is inefficient to update the cur_state filter every time
        SG_ERR_CHECK(  sg_dbndx__update_cur_state(pCtx, pdbc, psz_csid, generation, psz_baseline)  );

        SG_VHASH_NULLFREE(pCtx, pvh_delta);
        SG_CHANGESET_NULLFREE(pCtx, pChangeset);
    }

	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt_pairs)  );
    pStmt_pairs = NULL;

    SG_ERR_CHECK(  sg_sqlite__exec__retry(pCtx, pdbc->psql, "COMMIT TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );
	pdbc->bInTransaction = SG_FALSE;

fail:
    SG_DBRECORD_NULLFREE(pCtx, pRec);
    SG_VHASH_NULLFREE(pCtx, pvh_delta);
    SG_CHANGESET_NULLFREE(pCtx, pChangeset);

    if (pStmt_pairs)
    {
        SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt_pairs)  );
    }

	if (pdbc->bInTransaction)
    {
        SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pdbc->psql, "ROLLBACK TRANSACTION")  );
        pdbc->bInTransaction = SG_FALSE;
    }
}

static void sg_dbndx__apply_one_delta(
        SG_context* pCtx,
        sqlite3* psql,
        char* psz_table_name,
        const char* psz_deltaid,
        SG_bool b_reverse
        )
{
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_varray* pva = NULL;
	sqlite3_stmt* pStmt = NULL;
    int rc = 0;
    const char* psz_table_add = NULL;
    const char* psz_table_remove = NULL;

    if (b_reverse)
    {
        psz_table_add = "delta_removes";
        psz_table_remove = "delta_adds";
    }
    else
    {
        psz_table_add = "delta_adds";
        psz_table_remove = "delta_removes";
    }

	SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, psql, "INSERT OR IGNORE INTO %s SELECT hidrec FROM %s WHERE csid='%s'", psz_table_name, psz_table_add, psz_deltaid)  );

    // now the removes
    SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt, "SELECT hidrec FROM %s WHERE csid='%s'", psz_table_remove, psz_deltaid)  );
	while ((rc = sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char* psz_hidrec = (const char*) sqlite3_column_text(pStmt, 0);

        if (!pva)
        {
            SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva)  );
        }
        SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, psz_hidrec)  );
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(SG_ERR_SQLITE(rc));
	}
	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

    if (pva)
    {
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
        for (i=0; i<count; i++)
        {
            const char* psz_hidrec = NULL;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, i, &psz_hidrec)  );
            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, psql, "DELETE FROM %s WHERE hidrec='%s'", psz_table_name, psz_hidrec)  );
        }
    }

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
}

static void sg_dbndx__get_state_table(
        SG_context* pCtx,
        SG_dbndx* pndx,
        const char* psz_csid,
        char* buf_table,
        SG_uint32 bufsize
        )
{
    SG_string* pstr = NULL;
    SG_varray* pva_path = NULL;

    SG_NULLARGCHECK_RETURN(psz_csid);

    if (0 == strcmp(psz_csid, pndx->psz_cur_state_csid))
    {
        // no need to create the temp table
        // just return cur_state as the table name
        SG_ERR_CHECK(  SG_strcpy(pCtx, buf_table, bufsize, "cur_filter")  );
    }
    else
    {
        SG_int64 gen64 = -1;

        SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, buf_table)  );
        SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pndx->psql, "CREATE TEMP TABLE %s (hidrec VARCHAR UNIQUE NOT NULL)", buf_table)  );
        SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pndx->psql, "INSERT INTO %s SELECT hidrec FROM cur_filter", buf_table)  );
        SG_ERR_CHECK(  sg_sqlite__exec__va__int64(pCtx, pndx->psql, &gen64, "SELECT generation FROM states WHERE csid='%s'", psz_csid)  );

        SG_ERR_CHECK(  sg_dbndx__find_path(
                    pCtx,
                    pndx,
                    pndx->psz_cur_state_csid,
                    pndx->i_cur_state_generation,
                    psz_csid,
                    (SG_int32) gen64,
                    &pva_path
                    ) );

        SG_ERR_CHECK(  sg_dbndx__walk_path(
                    pCtx,
                    pndx,
                    pva_path,
                    buf_table
                    )  );

    }

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_path);
    SG_STRING_NULLFREE(pCtx, pstr);
    // TODO drop the temp table?
}

void SG_dbndx__query_across_states(SG_context* pCtx, SG_dbndx* pdbc, const SG_varray* pcrit, SG_int32 gMin, SG_int32 gMax, SG_vhash** ppResults)
{
	sqlite3_stmt* pStmt = NULL;
	char szMatchesTableName[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
	SG_uint32 count = 0;
	SG_vhash* pvhResults = NULL;
    int rc;
    SG_vhash* pvhOne = NULL;

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhResults)  );

    SG_ERR_CHECK(  sg_sqlite__exec__retry(pCtx, pdbc->psql, "BEGIN TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );
	pdbc->bInTransaction = SG_TRUE;

	/* First find all the records that match */

	SG_ERR_CHECK(  sg_dbndx__do_where(pCtx, pdbc->psql, pcrit, szMatchesTableName, &count)  );

	/* TODO what if count is 0 ?  shouldn't we just build  a list of states and set the count to 0 for all of them?  */

	/* Now find all the states for which we need to return results */

	if (gMin >= 0)
	{
		if (gMax >= 0)
		{
			SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pdbc->psql, &pStmt, "SELECT csid, generation FROM states WHERE generation >= %d AND generation <= %d ORDER BY generation ASC", gMin, gMax)  );
		}
		else
		{
			SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pdbc->psql, &pStmt, "SELECT csid, generation FROM states WHERE generation >= %d ORDER BY generation ASC", gMin)  );
		}
	}
	else if (gMax >= 0)
	{
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pdbc->psql, &pStmt, "SELECT csid, generation FROM states WHERE generation <= %d ORDER BY generation ASC", gMax)  );
	}
	else
	{
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pdbc->psql, &pStmt, "SELECT csid, generation FROM states ORDER BY generation ASC")  );
	}

	/* For each state, count the number of matching records which are members of that state */

	// TODO consider using sg_sqlite__step so that we get the SQL statement logging in the debug output.

	while ((rc = sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char* pszStateID = (const char*) sqlite3_column_text(pStmt, 0);
		SG_int32 generation = (SG_int32)sqlite3_column_int(pStmt,1);
		SG_uint32 count_results_this_state = 0;

		char szFilteredResultsTableName[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
		char szStateTable[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];

		SG_ERR_CHECK(  sg_dbndx__get_state_table(pCtx, pdbc, pszStateID, szStateTable, sizeof(szStateTable))  );

		SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, szFilteredResultsTableName)  );

        // TODO do we need to insert these into a temptable just to count them?

		SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pdbc->psql, "CREATE TEMP TABLE %s (hidrec VARCHAR UNIQUE)", szFilteredResultsTableName)  );
        /* TODO do we need an index on this WHERE? */

		SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pdbc->psql, "INSERT OR IGNORE INTO %s SELECT r.hidrec FROM %s r,%s s WHERE r.hidrec=s.hidrec", szFilteredResultsTableName, szMatchesTableName, szStateTable)  );

		SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, pdbc->psql, &count_results_this_state)  );

		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhOne)  );

		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvhOne, "generation", (SG_int64) generation)  );

		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvhOne, "count", (SG_int64) count_results_this_state)  );

		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhResults, pszStateID, &pvhOne)  );
	}

	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(SG_ERR_SQLITE(rc));
	}

	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );
    pStmt = NULL;

	*ppResults = pvhResults;
    pvhResults = NULL;

fail:
	if (pStmt)
	{
		SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	}
    SG_VHASH_NULLFREE(pCtx, pvhOne);
    SG_VHASH_NULLFREE(pCtx, pvhResults);

    SG_ERR_IGNORE(  sg_sqlite__exec__retry(pCtx, pdbc->psql, "ROLLBACK TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );
	pdbc->bInTransaction = SG_FALSE;
}

static void sg_dbndx__do_int_where(SG_context* pCtx, sqlite3* psql, const char* psz_op, const char* pszTempTableName, const char* psz_left, SG_int64 intvalue, SG_uint32* piCount)
{
	sqlite3_stmt* pStmt = NULL;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt, "INSERT OR IGNORE INTO %s (hidrec) SELECT hidrec FROM db_pairs WHERE ( (name = ?) AND (intvalue %s ?) )", pszTempTableName, psz_op)  );

	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, psz_left)  );

	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt, 2, intvalue)  );

	SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );

	SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, psql, piCount)  );

	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

	return;
fail:
	*piCount = 0;

	if (pStmt)
	{
		SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	}
}

static void sg_dbndx__do_state_filter(SG_context* pCtx, SG_dbndx* pndx, const char* pidState, char* szResultsTableName, SG_uint32* piCount)
{
    char szFilteredResultsTableName[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
    char szStartingStateTableName[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
    SG_uint32 count = 0;

    SG_ERR_CHECK(  sg_dbndx__get_state_table(pCtx, pndx, pidState, szStartingStateTableName, sizeof(szStartingStateTableName))  );
    SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, szFilteredResultsTableName)  );
    SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pndx->psql, "CREATE TEMP TABLE %s (ordnum INTEGER PRIMARY KEY AUTOINCREMENT, hidrec VARCHAR UNIQUE)", szFilteredResultsTableName)  );
    // TODO we do we want a sort here to serve as the default?
    SG_ERR_CHECK(  sg_sqlite__exec__count__va(pCtx, pndx->psql, &count, "INSERT INTO %s (hidrec) SELECT r.hidrec FROM %s r,%s s WHERE r.hidrec=s.hidrec", szFilteredResultsTableName, szResultsTableName, szStartingStateTableName)  );
	SG_ERR_CHECK(  SG_strcpy(pCtx, szResultsTableName, sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH, szFilteredResultsTableName)  );

    if (piCount)
    {
        *piCount = count;
    }

fail:
    ;
}

static void sg_dbndx__do_nostatefilter_copy(SG_context* pCtx, SG_dbndx* pndx, char* szResultsTableName, SG_uint32* piCount)
{
    char szFilteredResultsTableName[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
    SG_uint32 count = 0;

    SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, szFilteredResultsTableName)  );
    SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pndx->psql, "CREATE TEMP TABLE %s (ordnum INTEGER PRIMARY KEY AUTOINCREMENT, hidrec VARCHAR UNIQUE)", szFilteredResultsTableName)  );
    SG_ERR_CHECK(  sg_sqlite__exec__count__va(pCtx, pndx->psql, &count, "INSERT INTO %s (hidrec) SELECT r.hidrec FROM %s r", szFilteredResultsTableName, szResultsTableName)  );
	SG_ERR_CHECK(  SG_strcpy(pCtx, szResultsTableName, sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH, szFilteredResultsTableName)  );

    if (piCount)
    {
        *piCount = count;
    }

fail:
    ;
}

static void sg_dbndx__do_where(SG_context* pCtx, sqlite3* psql, const SG_varray* pcrit, char* pszTempTableName, SG_uint32* piCount)
{
	sqlite3_stmt* pStmt = NULL;

	SG_ERR_CHECK_RETURN(  sg_dbndx__sql__get_temptable_name(pCtx, pszTempTableName)  );

	SG_ERR_CHECK_RETURN(  sg_sqlite__exec__va(pCtx, psql, "CREATE TEMP TABLE %s (hidrec VARCHAR UNIQUE)", pszTempTableName)  );

	if (!pcrit)
	{
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt, "INSERT OR IGNORE INTO %s (hidrec) SELECT hidrec FROM db_pairs", pszTempTableName)  );

		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );

		SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, psql, piCount)  );

		SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

		return;
	}
	else
	{
		const char* psz_op = NULL;

		SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pcrit, SG_CRIT_NDX_OP, &psz_op)  );

		if (
			(0 == strcmp("<", psz_op))
			|| (0 == strcmp(">", psz_op))
			|| (0 == strcmp("<=", psz_op))
			|| (0 == strcmp(">=", psz_op))
			|| (0 == strcmp("!=", psz_op))
			)
		{
			SG_int64 intvalue;
			const char* psz_left = NULL;

			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pcrit, SG_CRIT_NDX_LEFT, &psz_left)  );

			SG_ERR_CHECK(  SG_varray__get__int64(pCtx, pcrit, SG_CRIT_NDX_RIGHT, &intvalue)  );

			SG_ERR_CHECK_RETURN(  sg_dbndx__do_int_where(pCtx, psql, psz_op, pszTempTableName, psz_left, intvalue, piCount)  );
			return;
		}
		else if (
			(0 == strcmp("==", psz_op))
			)
		{
			const char* psz_left = NULL;
			SG_uint16 t;

			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pcrit, SG_CRIT_NDX_LEFT, &psz_left)  );

			SG_ERR_CHECK(  SG_varray__typeof(pCtx, pcrit, SG_CRIT_NDX_RIGHT, &t)  );

			if (t == SG_VARIANT_TYPE_INT64)
			{
				SG_int64 intvalue;

				SG_ERR_CHECK(  SG_varray__get__int64(pCtx, pcrit, SG_CRIT_NDX_RIGHT, &intvalue)  );

				SG_ERR_CHECK_RETURN(  sg_dbndx__do_int_where(pCtx, psql, psz_op, pszTempTableName, psz_left, intvalue, piCount)  );
				return;
			}
			else if (t == SG_VARIANT_TYPE_SZ)
			{
				const char* psz_right = NULL;

				SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pcrit, SG_CRIT_NDX_RIGHT, &psz_right)  );

				SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt, "INSERT OR IGNORE INTO %s (hidrec) SELECT hidrec FROM db_pairs WHERE ( (name = ?) AND (strvalue = ?) )", pszTempTableName)  );

				SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, psz_left)  );
				SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, psz_right)  );
				SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );

				SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, psql, piCount)  );

				SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

				return;
			}
			else
			{
				SG_ERR_THROW_RETURN(SG_ERR_INVALIDARG); // TODO invalid crit
			}
		}
		else if (
			(0 == strcmp("in", psz_op))
			)
		{
			const char* psz_left = NULL;
            SG_varray* pva_right = NULL;
            SG_uint32 count = 0;
            SG_uint32 i = 0;
			char vlist[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];

			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pcrit, SG_CRIT_NDX_LEFT, &psz_left)  );
			SG_ERR_CHECK(  SG_varray__get__varray(pCtx, pcrit, SG_CRIT_NDX_RIGHT, &pva_right)  );
            SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, vlist)  );
            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, psql, "CREATE TEMP TABLE %s (strvalue VARCHAR UNIQUE)", vlist)  );
            SG_ERR_CHECK(  SG_varray__count(pCtx, pva_right, &count)  );
            SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt, "INSERT OR IGNORE INTO %s (strvalue) VALUES (?)", vlist)  );
            for (i=0; i<count; i++)
            {
                const char* psz_val = NULL;

                SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_right, i, &psz_val)  );
                SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
                SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );
                SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, psz_val)  );
                SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
            }

            SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

            SG_ERR_CHECK(  sg_sqlite__exec__count__va(pCtx, psql, &count, "INSERT INTO %s (hidrec) SELECT p.hidrec FROM db_pairs p,%s v WHERE p.strvalue=v.strvalue AND p.name='%s'", pszTempTableName, vlist, psz_left)  );

            *piCount = count;

            // TODO drop the vlist temp table

			return;
        }
		else if (
			(0 == strcmp("exists", psz_op))
			)
		{
			const char* psz_left = NULL;

			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pcrit, SG_CRIT_NDX_LEFT, &psz_left)  );
			SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt, "INSERT OR IGNORE INTO %s SELECT hidrec FROM db_pairs WHERE ( (name = ?) AND (strvalue IS NOT NULL) )", pszTempTableName)  );
			SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, psz_left)  );
			SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );

			SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, psql, piCount)  );

			SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

			return;
		}
		else if (
			(0 == strcmp("&&", psz_op))
			|| (0 == strcmp("||", psz_op))
			)
		{
			SG_varray* pcrit_left = NULL;
			SG_varray* pcrit_right = NULL;

			char left[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
			SG_uint32 my_count_left = 0;

			SG_ERR_CHECK(  SG_varray__get__varray(pCtx, pcrit, SG_CRIT_NDX_LEFT, &pcrit_left)  );

			SG_ERR_CHECK(  SG_varray__get__varray(pCtx, pcrit, SG_CRIT_NDX_RIGHT, &pcrit_right)  );

			SG_ERR_CHECK(  sg_dbndx__do_where(pCtx, psql, pcrit_left, left, &my_count_left)  );

			if (
				(my_count_left > 0)
				&& (left[0])
				)
			{
				char right[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
				SG_uint32 my_count_right = 0;

				SG_ERR_CHECK(  sg_dbndx__do_where(pCtx, psql, pcrit_right, right, &my_count_right)  );

				if (0 == strcmp("&&", psz_op))
				{
					if (
						(my_count_right > 0)
						&& (right[0])
						)
					{
						SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, psql, "INSERT OR IGNORE INTO %s (hidrec) SELECT %s.hidrec FROM %s,%s WHERE %s.hidrec=%s.hidrec", pszTempTableName,
												left,
												left, right,
															left, right)  );

						SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, psql, piCount)  );
					}
					else
					{
						/* result set is empty */
						pszTempTableName[0] = 0;
						*piCount = 0;

						return;
					}
				}
				else
				{
					/* OR */
					if (
						(my_count_right > 0)
						&& (right[0])
						)
					{
						SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, psql, "INSERT INTO %s (hidrec) SELECT hidrec FROM %s UNION SELECT hidrec FROM %s", pszTempTableName,
															left, right)  );
						SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, psql, piCount)  );
					}
					else
					{
						SG_ERR_CHECK(  SG_strcpy(pCtx, pszTempTableName, sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH, left)  );
						*piCount = my_count_left;

						return;
					}
				}

				return;
			}
			else
			{
				if (0 == strcmp("&&", psz_op))
				{
					/* result set will be empty.  no need to check the right side */
					pszTempTableName[0] = 0;
					*piCount = 0;

					return;
				}
				else
				{
					/* OR */

					/* nothing in the left, so the answer is whatever is on the right */
					SG_ERR_CHECK_RETURN(  sg_dbndx__do_where(pCtx, psql, pcrit_right, pszTempTableName, piCount)  );
					return;
				}
			}

			/* NOTREACHED: return SG_ERR_OK; */
		}
		else
		{
			SG_ERR_THROW_RETURN(SG_ERR_INVALIDARG); // TODO invalid crit
		}
	}

	// NOTREACHED SG_ASSERT(0);

fail:
	SG_ERR_IGNORE(  sg_sqlite__exec__va(pCtx, psql, "DROP TABLE %s", pszTempTableName)  );
	pszTempTableName[0] = 0;
	*piCount = 0;

	if (pStmt)
	{
		SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	}
}

static void sg_dbndx__sort(SG_context* pCtx, sqlite3* psql, char* szResultsTableName, const SG_varray* pSort)
{
	char szSortedTempTableName[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
	SG_uint32 i;
	SG_uint32 levels;
	sqlite3_stmt* pStmt = NULL;

	struct _keyinfo
	{
		char szTempTableName[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
		const char* psz_name;
		SG_uint8 flags;
	}* pki = NULL;

	SG_string* pstrSQL = NULL;

	SG_ERR_CHECK(  SG_varray__count(pCtx, pSort, &levels)  );

	SG_ERR_CHECK(  SG_allocN(pCtx, levels,pki)  );

	SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, szSortedTempTableName)  );

	SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, psql, "CREATE TEMP TABLE %s (ordnum INTEGER PRIMARY KEY AUTOINCREMENT, hidrec VARCHAR UNIQUE)", szSortedTempTableName)  );

	for (i=0; i<levels; i++)
	{
		struct _keyinfo* pi = &pki[i];
        SG_vhash* pvh_key = NULL;
        SG_bool b_has_direction = SG_FALSE;
        SG_bool b_has_sort_type = SG_FALSE;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pSort, i, &pvh_key)  );

        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_key, SG_DBNDX_SORT__DIRECTION, &b_has_direction)  );
        if (b_has_direction)
        {
            const char* psz_direction = NULL;
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_key, SG_DBNDX_SORT__DIRECTION, &psz_direction)  );
            if (0 == strcmp(psz_direction, "desc"))
            {
                pi->flags |= SG_DB_SORT_DESC;
            }
        }

        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_key, SG_DBNDX_SORT__TYPE, &b_has_sort_type)  );
        if (b_has_sort_type)
        {
            const char* psz_sort_type = NULL;
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_key, SG_DBNDX_SORT__TYPE, &psz_sort_type)  );
            if (0 == strcmp(psz_sort_type, "numeric"))
            {
                pi->flags |= SG_DB_SORT_NUMERIC;
            }
        }

		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_key, SG_DBNDX_SORT__NAME, &pi->psz_name)  );
		SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, pi->szTempTableName)  );

		if (
                (pi->flags & SG_DB_SORT_NUMERIC)
                || (0 == strcmp(pi->psz_name, "#WHEN"))
           )
		{
			SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, psql, "CREATE TEMP TABLE %s (hidrec VARCHAR UNIQUE, c%d INTEGER)", pi->szTempTableName, i)  );
		}
		else
		{
			SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, psql, "CREATE TEMP TABLE %s (hidrec VARCHAR UNIQUE, c%d VARCHAR)", pi->szTempTableName, i)  );
		}

        if (0 == strcmp(pi->psz_name, "#WHEN"))
        {
            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, psql, "INSERT INTO %s SELECT hidrec, timestamp FROM db_history h, db_audits a WHERE h.csid = a.csid", pi->szTempTableName)  );
        }
        else
        {
            SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql, &pStmt, "INSERT INTO %s SELECT hidrec, %s FROM db_pairs WHERE name=?", pi->szTempTableName, (pi->flags & SG_DB_SORT_NUMERIC) ? "intvalue" : "strvalue")  );

            SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, pi->psz_name)  );
            SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
            SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );
            pStmt = NULL;
        }

		//SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, psql, "CREATE INDEX %s_hidrec ON %s (hidrec)", pi->szTempTableName, pi->szTempTableName)  );
	}

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstrSQL)  );

	SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstrSQL, "INSERT INTO %s (hidrec) SELECT r.hidrec FROM %s r ", szSortedTempTableName, szResultsTableName)  );

	for (i=0; i<levels; i++)
	{
		char buf[256];

		SG_ERR_CHECK(  SG_sprintf(pCtx, buf, 255, "LEFT OUTER JOIN %s k%d ON r.hidrec=k%d.hidrec ", pki[i].szTempTableName, i, i)  );
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pstrSQL, buf)  );
	}

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pstrSQL, "ORDER BY ")  );

	for (i=0; i<levels; i++)
	{
		char buf[33];

		SG_ERR_CHECK(  SG_sprintf(pCtx, buf, 32, "%s c%d %s", (i != 0) ? ", " : "", i, (pki[i].flags & SG_DB_SORT_DESC) ? "DESC" : "ASC")  );
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pstrSQL, buf) );
	}

	SG_ERR_CHECK(  sg_sqlite__exec__string(pCtx, psql, NULL, &pstrSQL)  );

	SG_ERR_CHECK(  SG_strcpy(pCtx, szResultsTableName, sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH, szSortedTempTableName)  );

	SG_NULLFREE(pCtx, pki);
	pki = NULL;

	return;
fail:
	if (pStmt)
	{
		SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	}

    SG_NULLFREE(pCtx, pki);
    SG_STRING_NULLFREE(pCtx, pstrSQL);
}

void SG_dbndx__query(
	SG_context* pCtx,
	SG_dbndx** ppndx,
	const char* pidState,
	const SG_varray* pcrit,
	const SG_varray* pSort,
    SG_stringarray* psa_slice_fields,
    SG_dbndx_qresult** ppqr
	)
{
	char szResultsTableName[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
	SG_uint32 count_where = 0;
	SG_uint32 count_filtered = 0;
	SG_stringarray* pidset = NULL;
    SG_varray* pva_sliced = NULL;
    SG_varray* pva_rechid_sliced = NULL;
    SG_dbndx* pdbc = NULL;
    SG_dbndx_qresult* pqr = NULL;

	SG_NULLARGCHECK_RETURN(ppndx);
	SG_NULLARGCHECK_RETURN(*ppndx);
	SG_NULLARGCHECK_RETURN(ppqr);
	SG_NULLARGCHECK_RETURN(psa_slice_fields);
	//SG_NULLARGCHECK_RETURN(pidState);

    pdbc = *ppndx;

    SG_ERR_CHECK(  sg_sqlite__exec__retry(pCtx, pdbc->psql, "BEGIN TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );
	pdbc->bInTransaction = SG_TRUE;

	SG_ERR_CHECK(  sg_dbndx__do_where(pCtx, pdbc->psql, pcrit, szResultsTableName, &count_where)  );

	if (
		(0 == count_where)
		|| !(szResultsTableName[0])
		)
	{
        *ppqr = NULL;
		return;
	}

    // now filter for the given state
    if (pidState)
    {
        SG_ERR_CHECK(  sg_dbndx__do_state_filter(pCtx, pdbc, pidState, szResultsTableName, &count_filtered)  );
    }
    else
    {
        SG_ERR_CHECK(  sg_dbndx__do_nostatefilter_copy(pCtx, pdbc, szResultsTableName, &count_filtered)  );
    }

	if (0 == count_filtered)
	{
        // TODO free?
        *ppqr = NULL;
		return;
	}

    // now sort
    if (
            pSort
            && (count_filtered > 1)
            )
    {
        SG_ERR_CHECK(  sg_dbndx__sort(pCtx, pdbc->psql, szResultsTableName, pSort)  );
    }

    SG_ERR_CHECK(  sg_dbndx_qresult__alloc(pCtx, ppndx, szResultsTableName, psa_slice_fields, count_filtered, &pqr)  );

    *ppqr = pqr;
    pqr = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_rechid_sliced);
    SG_VARRAY_NULLFREE(pCtx, pva_sliced);
    SG_STRINGARRAY_NULLFREE(pCtx, pidset);
    SG_dbndx_qresult__free(pCtx, pqr);
}

static void sg_dbndx_qresult__alloc(SG_context* pCtx, SG_dbndx** ppndx, const char* szResultsTableName, SG_stringarray* psa_slice_fields, SG_uint32 count_results, SG_dbndx_qresult** ppqr)
{
	SG_dbndx_qresult* pqr = NULL;
	char szPairsTableName[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
    SG_uint32 i = 0;
    SG_uint32 count = 0;
    const char* psz_the_one_field = NULL;
    SG_uint32 count_wanted_fields = 0;
    SG_bool b_want_all_fields = SG_FALSE;

	SG_NULLARGCHECK_RETURN(ppndx);
	SG_NULLARGCHECK_RETURN(*ppndx);
	SG_NULLARGCHECK_RETURN(psa_slice_fields);
	SG_NULLARGCHECK_RETURN(ppqr);

	SG_ERR_CHECK(  SG_alloc1(pCtx, pqr)  );

    pqr->pndx = *ppndx;
    *ppndx = NULL;
    pqr->count_results = count_results;

    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_slice_fields, &count)  );
    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pqr->pvh_fields)  );
    for (i=0; i<count; i++)
    {
        const char* psz_name = NULL;

        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa_slice_fields, i, &psz_name)  );
        if (0 == strcmp(psz_name, "_rechid"))
        {
            pqr->b_want_rechid = SG_TRUE;
            count_wanted_fields++;
        }
        else if (0 == strcmp(psz_name, SG_ZING_FIELD__HISTORY))
        {
            pqr->b_want_history = SG_TRUE;
        }
        else if (0 == strcmp(psz_name, "*"))
        {
            b_want_all_fields = SG_TRUE;
        }
        else
        {
            SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pqr->pvh_fields, psz_name)  );
            count_wanted_fields++;
        }
    }

    if (b_want_all_fields)
    {
        SG_VHASH_NULLFREE(pCtx, pqr->pvh_fields);
    }
    else
    {
        if (1 == count_wanted_fields)
        {
            pqr->b_flat_result = SG_TRUE;
        }
    }

    if (pqr->b_flat_result)
    {
        if (pqr->b_want_rechid)
        {
            SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pqr->pndx->psql, &pqr->pStmt_pairs, "SELECT hidrec FROM %s", szResultsTableName)  );
        }
        else
        {
            SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pqr->pvh_fields, 0, &psz_the_one_field, NULL)  );
            SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, szPairsTableName)  );
            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pqr->pndx->psql, "CREATE TEMP TABLE %s (strvalue VARCHAR NOT NULL, intvalue INTEGER NULL)", szPairsTableName)  );
            // TODO the following line trusts a field name to not need escaping.  not good.
            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pqr->pndx->psql, "INSERT INTO %s SELECT p.strvalue, p.intvalue FROM db_pairs p,%s r WHERE p.hidrec=r.hidrec AND p.name = '%s' ORDER BY r.ordnum ASC", szPairsTableName, szResultsTableName, psz_the_one_field)  );

            SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pqr->pndx->psql, &pqr->pStmt_pairs, "SELECT strvalue, intvalue FROM %s", szPairsTableName)  );
        }
    }
    else
    {
        SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, szPairsTableName)  );
        SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pqr->pndx->psql, "CREATE TEMP TABLE %s (ordnum INTEGER, hidrec VARCHAR NOT NULL, name VARCHAR NOT NULL, strvalue VARCHAR NOT NULL, intvalue INTEGER NULL)", szPairsTableName)  );
        SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pqr->pndx->psql, "INSERT INTO %s SELECT r.ordnum, p.hidrec, p.name, p.strvalue, p.intvalue FROM db_pairs p,%s r WHERE p.hidrec=r.hidrec ORDER BY r.ordnum ASC", szPairsTableName, szResultsTableName)  );

        // TODO should we put the wanted fields into the SELECT, instead
        // of filtering them using the vhash as we retrieve the rows?  It
        // kind of depends on how many wanted fields there are.
        SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pqr->pndx->psql, &pqr->pStmt_pairs, "SELECT ordnum, hidrec, name, strvalue, intvalue FROM %s", szPairsTableName)  );

        if (pqr->b_want_history)
        {
            char sz_audits_table[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
            char sz_history_table[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];
            char sz_recid_table[sg_DBNDX_TEMPTABLE_NAME_BUF_LENGTH];

            SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, sz_recid_table)  );
            SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, sz_history_table)  );
            SG_ERR_CHECK(  sg_dbndx__sql__get_temptable_name(pCtx, sz_audits_table)  );

            // TODO see if we need to add indexes to the join columns even
            // though these are temp tables

            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pqr->pndx->psql, "CREATE TEMP TABLE %s (ordnum INTEGER NOT NULL, recid VARCHAR NOT NULL)", sz_recid_table)  );

            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pqr->pndx->psql, "INSERT INTO %s SELECT r.ordnum, p.strvalue FROM db_pairs p,%s r WHERE p.name = 'recid' AND r.hidrec=p.hidrec ORDER BY r.ordnum ASC", sz_recid_table, szResultsTableName)  );

            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pqr->pndx->psql, "CREATE TEMP TABLE %s (recid VARCHAR NOT NULL, hidrec VARCHAR NOT NULL, csid VARCHAR NOT NULL, generation INTEGER NOT NULL)", sz_history_table)  );

            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pqr->pndx->psql, "INSERT INTO %s SELECT h.recid, h.hidrec, h.csid, h.generation FROM db_history h, %s r WHERE h.recid=r.recid ORDER BY r.ordnum ASC, generation DESC", sz_history_table, sz_recid_table)  );

            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pqr->pndx->psql, "CREATE TEMP TABLE %s (csid VARCHAR NOT NULL, userid VARCHAR NOT NULL, timestamp INTEGER NOT NULL)", sz_audits_table)  );

            // TODO SELECT DISTINCT?  probbly not
            SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pqr->pndx->psql, "INSERT INTO %s SELECT DISTINCT a.csid, a.userid, a.timestamp FROM db_audits a,%s h WHERE a.csid = h.csid ORDER BY a.csid ASC", sz_audits_table, sz_history_table)  );

            SG_ERR_CHECK(  sg_dbndx__slurp_the_audits(pCtx, pqr->pndx, sz_audits_table, &pqr->pvh_audits)  );

            SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pqr->pndx->psql, &pqr->pStmt_history, "SELECT recid, hidrec, csid, generation FROM %s", sz_history_table)  );
        }
    }

    *ppqr = pqr;
    pqr = NULL;

fail:
    SG_dbndx_qresult__free(pCtx, pqr);
}

void SG_dbndx_qresult__count(
	SG_context* pCtx,
    SG_dbndx_qresult* pqr,
    SG_uint32* piCount
	)
{
    SG_NULLARGCHECK_RETURN(pqr);
    SG_NULLARGCHECK_RETURN(piCount);

    *piCount = pqr->count_results;
}

static void sg_dbndx_qresult__get_one(
	SG_context* pCtx,
    SG_dbndx_qresult* pqr,
    SG_varray* pva,
    SG_vhash** ppvh,
    SG_bool* pb_got_one,
    SG_bool* pb_done
	)
{
    int rc = 0;
    SG_vhash* pvh_allocated = NULL;

	SG_NULLARGCHECK_RETURN(pqr);

    if (pva)
    {
        SG_ARGCHECK_RETURN(!ppvh, ppvh);
    }
    if (ppvh)
    {
        SG_ARGCHECK_RETURN(!pva, pva);
        SG_ARGCHECK_RETURN(!pqr->b_flat_result, pva);
    }

    if (pqr->b_flat_result)
    {
        if (pqr->b_want_rechid)
        {
            rc = sqlite3_step(pqr->pStmt_pairs);

            if (SQLITE_ROW == rc)
            {
                if (pva)
                {
                    const char* psz_hidrec = (const char*) sqlite3_column_text(pqr->pStmt_pairs, 0);

                    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, psz_hidrec)  );
                }
                *pb_got_one = SG_TRUE;
            }
            else if (SQLITE_DONE == rc)
            {
                *pb_done = SG_TRUE;
            }
            else
            {
                SG_ERR_THROW(SG_ERR_SQLITE(rc));
            }
        }
        else if (pqr->b_want_history)
        {
            // you can't ask for history as the only thing
            SG_ERR_THROW( SG_ERR_NOTIMPLEMENTED  );
        }
        else
        {
            rc = sqlite3_step(pqr->pStmt_pairs);

            if (SQLITE_ROW == rc)
            {
                if (pva)
                {
                    const char* psz_strvalue = (const char*) sqlite3_column_text(pqr->pStmt_pairs, 0);
                    // SG_int64 ival = sqlite3_column_int64(pqr->pStmt_pairs, 1);

                    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, psz_strvalue)  );
                }
                *pb_got_one = SG_TRUE;
            }
            else if (SQLITE_DONE == rc)
            {
                *pb_done = SG_TRUE;
            }
            else
            {
                SG_ERR_THROW(SG_ERR_SQLITE(rc));
            }
        }
    }
    else
    {
        SG_uint32 cur_ordnum = 0;
        SG_vhash* cur_pvh = NULL;
        char sz_recid[SG_GID_BUFFER_LENGTH];

        sz_recid[0] = 0;
        while (1)
        {
            if (pqr->last_rechid_p)
            {

                if (0 == cur_ordnum)
                {
                    cur_ordnum = pqr->last_ordnum_p;
                    if (pva)
                    {
                        SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva, &cur_pvh)  );
                    }
                    if (ppvh)
                    {
                        SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_allocated)  );
                        cur_pvh = pvh_allocated;
                    }
                    if (cur_pvh)
                    {
                        if (pqr->b_want_rechid)
                        {
                            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, cur_pvh, "_rechid", pqr->last_rechid_p)  );
                        }
                    }
                    *pb_got_one = SG_TRUE;
                }

                if (cur_ordnum != pqr->last_ordnum_p)
                {
                    break;
                }

                if (cur_pvh)
                {
                    SG_bool b = SG_FALSE;

                    if (pqr->pvh_fields)
                    {
                        SG_ERR_CHECK(  SG_vhash__has(pCtx, pqr->pvh_fields, pqr->last_name_p, &b)  );
                    }
                    else
                    {
                        b = SG_TRUE;
                    }

                    if (b)
                    {
                        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, cur_pvh, pqr->last_name_p, pqr->last_strvalue_p)  );
                    }

                    if (0 == strcmp(pqr->last_name_p, "recid"))
                    {
                        SG_ERR_CHECK(  SG_strcpy(pCtx, sz_recid, sizeof(sz_recid), pqr->last_strvalue_p)  );
                    }
                }
            }

            rc = sqlite3_step(pqr->pStmt_pairs);

            if (SQLITE_ROW == rc)
            {
                pqr->last_ordnum_p = sqlite3_column_int(pqr->pStmt_pairs, 0);
                pqr->last_rechid_p = (const char*) sqlite3_column_text(pqr->pStmt_pairs, 1);
                pqr->last_name_p = (const char*) sqlite3_column_text(pqr->pStmt_pairs, 2);
                pqr->last_strvalue_p = (const char*) sqlite3_column_text(pqr->pStmt_pairs, 3);
                pqr->last_ival_p = sqlite3_column_int64(pqr->pStmt_pairs, 4);
            }
            else if (SQLITE_DONE == rc)
            {
                pqr->last_ordnum_p = 0;
                pqr->last_rechid_p = NULL;
                pqr->last_strvalue_p = NULL;
                pqr->last_name_p = NULL;
                pqr->last_ival_p = 0;

                *pb_done = SG_TRUE;
                break;
            }
            else
            {
                SG_ERR_THROW(SG_ERR_SQLITE(rc));
            }
        }

        if (pqr->b_want_history)
        {
            SG_varray* pva_history = NULL;

            if (!sz_recid[0])
            {
                SG_ERR_THROW(  SG_ERR_ZING_NO_RECID  );
            }

            SG_ERR_CHECK(  SG_vhash__addnew__varray(pCtx, cur_pvh, SG_ZING_FIELD__HISTORY, &pva_history)  );

            while (1)
            {
                if (!pqr->pStmt_history)
                {
                    break;
                }

                if (pqr->last_rechid_h)
                {
                    if (0 != strcmp(sz_recid, pqr->last_recid_h))
                    {
                        break;
                    }

                    if (pva_history)
                    {
                        SG_vhash* pvh_history_entry = NULL;
                        SG_bool b_has_audits = SG_FALSE;

                        SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva_history, &pvh_history_entry)  );
                        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_history_entry, "rechid", pqr->last_rechid_h)  );
                        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_history_entry, "csid", pqr->last_csid_h)  );
                        SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_history_entry, "generation", (SG_int64) pqr->last_generation_h)  );

                        SG_ERR_CHECK(  SG_vhash__has(pCtx, pqr->pvh_audits, pqr->last_csid_h, &b_has_audits)  );
                        if (b_has_audits)
                        {
                            SG_varray* pva_audits = NULL;
                            SG_varray* pva_copy_audits = NULL;
                            SG_uint32 count_audits = 0;
                            SG_uint32 i = 0;

                            SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pqr->pvh_audits, pqr->last_csid_h, &pva_audits)  );
                            SG_ERR_CHECK(  SG_varray__count(pCtx, pva_audits, &count_audits)  );
                            SG_ERR_CHECK(  SG_vhash__addnew__varray(pCtx, pvh_history_entry, "audits", &pva_copy_audits)  );
                            for (i=0; i<count_audits; i++)
                            {
                                SG_vhash* pvh_one_audit = NULL;
                                SG_vhash* pvh_copy_audit = NULL;
                                const char* psz_who = NULL;
                                SG_int64 i_when = 0;

                                SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_audits, i, &pvh_one_audit)  );
                                SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva_copy_audits, &pvh_copy_audit)  );
                                SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_one_audit, "who", &psz_who)  );
                                SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_one_audit, "when", &i_when)  );
                                SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_copy_audit, "who", psz_who)  );
                                SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_copy_audit, "when", i_when)  );
                            }
                        }
                    }
                }

                if (pqr->pStmt_history)
                {
                    rc = sqlite3_step(pqr->pStmt_history);

                    if (SQLITE_ROW == rc)
                    {
                        pqr->last_recid_h = (const char*) sqlite3_column_text(pqr->pStmt_history, 0);
                        pqr->last_rechid_h = (const char*) sqlite3_column_text(pqr->pStmt_history, 1);
                        pqr->last_csid_h = (const char*) sqlite3_column_text(pqr->pStmt_history, 2);
                        pqr->last_generation_h = sqlite3_column_int(pqr->pStmt_history, 3);
                    }
                    else if (SQLITE_DONE == rc)
                    {
                        pqr->last_recid_h = NULL;
                        pqr->last_rechid_h = NULL;
                        pqr->last_csid_h = NULL;
                        pqr->last_generation_h = 0;

                        SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pqr->pStmt_history)  );
                        pqr->pStmt_history = NULL;

                        break;
                    }
                    else
                    {
                        SG_ERR_THROW(SG_ERR_SQLITE(rc));
                    }
                }
            }
        }

        if (ppvh)
        {
            *ppvh = cur_pvh;
            pvh_allocated = NULL;
        }
    }

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_allocated);
}

void SG_dbndx_qresult__get__multiple(
	SG_context* pCtx,
    SG_dbndx_qresult* pqr,
    SG_int32 count_requested,
    SG_uint32* pi_count_retrieved,
    SG_varray* pva
	)
{
    SG_uint32 count = 0;

    while (1)
    {
        SG_bool b_done = SG_FALSE;
        SG_bool b_got_one = SG_FALSE;

        SG_ERR_CHECK_RETURN(  sg_dbndx_qresult__get_one(pCtx, pqr, pva, NULL, &b_got_one, &b_done)  );
        if (b_got_one)
        {
            count++;
        }
        else
        {
            SG_ASSERT(b_done);
        }
        if (b_done)
        {
            break;
        }
        if (
                (count_requested > 0)
                && (count >= (SG_uint32) count_requested)
           )
        {
            break;
        }
    }

    *pi_count_retrieved = count;
}

void SG_dbndx_qresult__get__one(
	SG_context* pCtx,
    SG_dbndx_qresult* pqr,
    SG_vhash** ppvh
	)
{
    SG_vhash* pvh = NULL;
    SG_bool b_got_one = SG_FALSE;
    SG_bool b_done = SG_FALSE;

    SG_ERR_CHECK(  sg_dbndx_qresult__get_one(pCtx, pqr, NULL, &pvh, &b_got_one, &b_done)  );
    if (b_got_one)
    {
        SG_ASSERT(pvh);
    }
    else
    {
        SG_ASSERT(b_done);
    }

    *ppvh = pvh;
    pvh = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_dbndx_qresult__done(
	SG_context* pCtx,
    SG_dbndx_qresult** ppqr
	)
{
    SG_dbndx_qresult__free(pCtx, *ppqr);
    *ppqr = NULL;
}

void SG_dbndx_qresult__free(SG_context* pCtx, SG_dbndx_qresult* pqr)
{
    if (!pqr)
    {
        return;
    }

    SG_VHASH_NULLFREE(pCtx, pqr->pvh_audits);
    SG_VHASH_NULLFREE(pCtx, pqr->pvh_fields);
	if (pqr->pStmt_pairs)
	{
		SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pqr->pStmt_pairs)  );
        pqr->pStmt_pairs = NULL;
	}
	if (pqr->pStmt_history)
	{
		SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pqr->pStmt_history)  );
        pqr->pStmt_history = NULL;
	}
    if (pqr->pndx->bInTransaction)
    {
        SG_ERR_IGNORE(  sg_sqlite__exec__retry(pCtx, pqr->pndx->psql, "ROLLBACK TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );
        pqr->pndx->bInTransaction = SG_FALSE;
    }

    SG_DBNDX_NULLFREE(pCtx, pqr->pndx);
    SG_NULLFREE(pCtx, pqr);
}

