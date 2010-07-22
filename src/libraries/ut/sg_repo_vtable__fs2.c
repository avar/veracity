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

/*
 *
 * @file sg_repo_vtable__fs2.c
 *
 */

#include <sg.h>
#include <zlib.h>
#include "sg_repo__private.h"
#include "sg_repo__private_utils.h"

#include "sg_repo_vtable__fs2.h"

// TODO temporary hack:
#ifdef WINDOWS
//#pragma warning(disable:4100)
#pragma warning(disable:4706)
#endif

// TODO rework dbndx.  don't open/close everytime?  move it into the main sqlite file?
//
// TODO unify the _BUSY error codes.  too many.

// TODO fix dbndx to be conc safe

// TODO
//
// put timestamps and/or pids in lock files.  find a good place
// to delete stale locks.
//
// fix mem leaks on abort of concurrency test.
//


#define MY_SLEEP_MS      100
#define MY_TIMEOUT_MS  16000

#define SG_RETRY_THINGIE(expr) \
SG_STATEMENT( \
  SG_uint32 slept = 0; \
  while(1) \
  { \
      SG_ASSERT(!pData->b_in_sqlite_transaction); \
      SG_ERR_CHECK(  sg_sqlite__exec__retry(pCtx, pData->psql, "BEGIN TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  ); \
      pData->b_in_sqlite_transaction = SG_TRUE; \
      expr; \
      if ( \
          SG_context__err_equals(pCtx, SG_ERR_DB_BUSY) \
          || SG_context__err_equals(pCtx, SG_ERR_SQLITE(SQLITE_BUSY)) \
          ) \
      { \
          SG_context__err_reset(pCtx); \
          SG_ERR_CHECK(  sg_sqlite__exec__retry(pCtx, pData->psql, "ROLLBACK TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  ); \
          pData->b_in_sqlite_transaction = SG_FALSE; \
          if (slept > MY_TIMEOUT_MS) \
          { \
              SG_ERR_THROW(  SG_ERR_DB_BUSY  ); \
          } \
          SG_sleep_ms(MY_SLEEP_MS); \
          slept += MY_SLEEP_MS; \
      } \
      else if (SG_context__has_err(pCtx)) \
      { \
          SG_ERR_IGNORE(  sg_sqlite__exec__retry(pCtx, pData->psql, "ROLLBACK TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );\
          pData->b_in_sqlite_transaction = SG_FALSE; \
          SG_ERR_CHECK_CURRENT; \
      } \
      else \
      { \
          SG_ERR_CHECK(  sg_sqlite__exec__retry(pCtx, pData->psql, "COMMIT TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );\
          pData->b_in_sqlite_transaction = SG_FALSE; \
          break; \
      } \
  } \
)

//////////////////////////////////////////////////////////////////

typedef struct _sg_blob_fs2_handle_store sg_blob_fs2_handle_store;
typedef struct _sg_blob_fs2_handle_fetch sg_blob_fs2_handle_fetch;

/**
 * we get one pointer in the SG_repo for our instance data.  in SG_repo
 * this is an opaque "sg_repo__vtable__instance_data *".  we cast it
 * to the following definition.
 */
struct _my_instance_data
{
    SG_repo*					pRepo;				// we DO NOT own this (but rather we are bound to it)
	SG_pathname *				pPathParentDir;
	SG_pathname *				pPathMyDir;

    // TODO b_indexes

    char						buf_hash_method[SG_REPO_HASH_METHOD__MAX_BUFFER_LENGTH];
    char						buf_repo_id[SG_GID_BUFFER_LENGTH];
    char						buf_admin_id[SG_GID_BUFFER_LENGTH];

    sqlite3*					psql;

    SG_bool                     b_in_sqlite_transaction;
	sg_blob_fs2_handle_store*	pBlobStoreHandle;

	SG_uint32					strlen_hashes;
};
typedef struct _my_instance_data my_instance_data;

struct _my_tx_data
{
    my_instance_data* pData;
    SG_vector* pvec_blobs;
    SG_rbtree* prb_append_locks;
    SG_rbtree* prb_drill_locks;

    SG_rbtree* prb_frags;
};
typedef struct _my_tx_data my_tx_data;

#define MY_CHUNK_SIZE			(16*1024)

struct _sg_blob_fs2_handle_fetch
{
	my_instance_data *			pData;

    char sz_hid_requested[SG_HID_MAX_BUFFER_LENGTH];
	SG_uint64					len_encoded_stored;		// we need to make sure whatever we read matches this
	SG_uint64					len_encoded_observed;	// value calculated as we read/uncompress/expand blob content into Raw Data
	SG_uint64					len_full_observed;	// value calculated as we read/uncompress/expand blob content into Raw Data
	SG_uint64					len_full_stored;
    SG_blob_encoding            blob_encoding_stored;
    SG_blob_encoding            blob_encoding_returning;
    char*                       psz_hid_vcdiff_reference_stored;
    char*                       psz_hid_vcdiff_reference_returning;
	SG_file *					m_pFileBlob;
	SG_repo_hash_handle *		pRHH_VerifyOnFetch;
    SG_uint32                   filenumber;
    SG_uint64                   offset;

    SG_pathname*                pPath_readlock;

    /* vcdiff stuff */
    SG_bool                     b_undeltifying;
    SG_vcdiff_undeltify_state* pst;
    sg_blob_fs2_handle_fetch* pbh_delta;
    SG_readstream* pstrm_delta;
    SG_seekreader* psr_reference;
    SG_pathname* pPath_tempfile;
    SG_byte* p_buf;
    SG_uint32 count;
    SG_uint32 next;

    /* zlib stuff */
    SG_bool                     b_uncompressing;
	z_stream zStream;
	SG_byte bufCompressed[MY_CHUNK_SIZE];
};

struct _sg_blob_fs2_handle_store
{
    SG_uint32 filenumber;
    SG_uint64 offset;
    SG_file* pFileBlob;

    my_tx_data* ptx;
    SG_blob_encoding blob_encoding_given;
    SG_blob_encoding blob_encoding_storing;
    const char* psz_hid_vcdiff_reference;
    SG_uint64 len_full_given;
    SG_uint64 len_full_observed;
    SG_uint64 len_encoded_observed;
    const char* psz_hid_known;
    const char* psz_hid_blob_final;
	SG_repo_hash_handle * pRHH_ComputeOnStore;
    SG_uint32 old_filenumber;

    /* zlib stuff */
    SG_bool b_compressing;
	z_stream zStream;
	SG_byte bufOut[MY_CHUNK_SIZE];
};

struct pending_blob_info
{
    SG_uint32 old_filenumber;
    SG_uint32 filenumber;
    SG_uint64 offset;
    char sz_hid_blob_final[SG_HID_MAX_BUFFER_LENGTH];
    SG_blob_encoding blob_encoding_storing;
    SG_uint64 len_full_given;
    SG_uint64 len_encoded_observed;
    const char* psz_hid_vcdiff_reference;
};

void sg_blob_fs2__fetch_blob__begin(
    SG_context * pCtx,
    my_instance_data* pData,
    const char* psz_hid_blob,
    SG_uint16 lock,
    SG_bool b_convert_to_full,
    char** ppsz_objectid,
    SG_blob_encoding* pBlobFormat,
    char** ppsz_hid_vcdiff_reference,
    SG_uint64* pLenRawData,
    SG_uint64* pLenFull,
    sg_blob_fs2_handle_fetch** ppHandle
    );

void sg_blob_fs2__fetch_blob__chunk(
    SG_context * pCtx,
    sg_blob_fs2_handle_fetch* ppHandle,
    SG_uint32 len_buf,
    SG_byte* p_buf,
    SG_uint32* p_len_got,
    SG_bool* pb_done
    );

void sg_blob_fs2__fetch_blob__end(
    SG_context * pCtx,
    sg_blob_fs2_handle_fetch** ppbh
    );

void sg_blob_fs2__fetch_blob__abort(
    SG_context * pCtx,
    sg_blob_fs2_handle_fetch** ppbh
    );

void sg_fs2__begin_tx(
    SG_context * pCtx,
    my_instance_data* pData,
    my_tx_data** pptx
    );

static void sg_fs2__get_dbndx_path(
    SG_context * pCtx,
    my_instance_data* pData,
    SG_uint32 iDagNum,
    SG_pathname** ppResult
    )
{
    char buf[SG_DAGNUM__BUF_MAX__NAME + 10];
    SG_pathname* pPath = NULL;

    SG_ERR_CHECK(  SG_dagnum__to_name(pCtx, iDagNum, buf, sizeof(buf))  );
    SG_ERR_CHECK(  SG_strcat(pCtx, buf, sizeof(buf), ".dbndx")  );
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pData->pPathMyDir, buf)  );

    *ppResult = pPath;

fail:
    ;
}

static void sg_fs2__get_treendx_path(
    SG_context * pCtx,
    my_instance_data* pData,
    SG_uint32 iDagNum,
    SG_pathname** ppResult
    )
{
    char buf[SG_DAGNUM__BUF_MAX__NAME + 10];
    SG_pathname* pPath = NULL;

    SG_ERR_CHECK(  SG_dagnum__to_name(pCtx, iDagNum, buf, sizeof(buf))  );
    SG_ERR_CHECK(  SG_strcat(pCtx, buf, sizeof(buf), ".treendx")  );
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pData->pPathMyDir, buf)  );

    *ppResult = pPath;

fail:
    ;
}

void sg_fs2__commit_tx(
	SG_context * pCtx,
	my_instance_data* pData,
	my_tx_data** pptx
	);


//////////////////////////////////////////////////////////////////

// In FS2, the blob data is stored in a series of "Number Files".
// Each can independently grow to ~1 Gb.  Each is named using the
// following sprintf format spec: "%06d".
//
// Note: this could probably be 7 or 8, but I pulled this value from
// Note: various stack variables.

#define sg_FILENUMBER_BUFFER_LENGTH				32

static void sg_fs2__filenumber_to_filename(SG_context * pCtx, char* buf, SG_uint32 bufsize, SG_uint32 filenumber)
{
	SG_ASSERT(  (bufsize >= sg_FILENUMBER_BUFFER_LENGTH)  );

    SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx, buf, bufsize, "%06d", filenumber)  );
}

// In FS2, various lock files are used on each of the "Number Files".
// For most locks, we just have one, such as "%06d_append".  But for
// read locks, we have one for each reader: "%06d_read_<tid>".
// Where <tid> is a sequence of hex digits; this doesn't have to be 64
// as in a full TID or GID.

#define sg_MAX_LOCKFILE_TYPE_LENGTH				10			// { "read_", "append", "gate", "hole", "drill", "vac" }
#define sg_MAX_LOCKFILE_LENGTH_RANDOM			32			// only needed for "read_<tid>".  this gives us one UUID's worth of random digits.
#define sg_MAX_LOCKFILE_SUFFIX_LENGTH			(sg_MAX_LOCKFILE_TYPE_LENGTH + sg_MAX_LOCKFILE_LENGTH_RANDOM)
#define sg_MAX_LOCKFILE_BUFFER_LENGTH			(sg_FILENUMBER_BUFFER_LENGTH + 1 + sg_MAX_LOCKFILE_SUFFIX_LENGTH)

static void sg_fs2__lock(
        SG_context * pCtx,
        my_instance_data* pData,
        const char* psz_filenumber,
        SG_uint16 lock_type,
        SG_pathname** ppLockFile
        );

static void my_fetch_info(
        SG_context * pCtx,
        my_instance_data* pData,
        const char * szHidBlob,
        SG_uint16 lock,
        SG_pathname** pp_readlock,
        SG_uint32* p_filenumber,
        SG_uint64* p_offset,
        SG_blob_encoding* p_blob_encoding,
        SG_uint64* p_len_encoded,
        SG_uint64* p_len_full,
        char** ppsz_hid_vcdiff_reference
        )
{
	sqlite3_stmt * pStmt = NULL;
    int rc;
    SG_blob_encoding blob_encoding = 0;
    SG_uint64 len_encoded = 0;
    SG_uint64 len_full = 0;
    const char * psz_hid_vcdiff_reference = NULL;
    SG_uint32 filenumber = 0;
    SG_uint64 offset = 0;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pData->psql,&pStmt,
									  "SELECT encoding, len_encoded, len_full, hid_vcdiff, filenumber, offset FROM directory WHERE hid = ?")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt,1,szHidBlob)  );

    rc = sqlite3_step(pStmt);
    if (SQLITE_ROW == rc)
    {
        blob_encoding = (SG_blob_encoding) sqlite3_column_int(pStmt, 0);

        len_encoded = sqlite3_column_int64(pStmt, 1);
        len_full = sqlite3_column_int64(pStmt, 2);

        if (SG_IS_BLOBENCODING_FULL(blob_encoding))
        {
            SG_ASSERT(len_encoded == len_full);
        }

        psz_hid_vcdiff_reference = (const char *)sqlite3_column_text(pStmt,3);

        if (ppsz_hid_vcdiff_reference)
        {
            if (psz_hid_vcdiff_reference)
            {
                SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_hid_vcdiff_reference, ppsz_hid_vcdiff_reference)  );
            }
            else
            {
                *ppsz_hid_vcdiff_reference = NULL;
            }
        }

        filenumber = sqlite3_column_int(pStmt,4);
        offset = sqlite3_column_int64(pStmt, 5);

        SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );

        SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

        if (lock)
        {
            char buf_filenumber[sg_FILENUMBER_BUFFER_LENGTH];

            SG_ERR_CHECK(  sg_fs2__filenumber_to_filename(pCtx, buf_filenumber, sizeof(buf_filenumber), filenumber)  );
            SG_ERR_CHECK(  sg_fs2__lock(pCtx, pData, buf_filenumber, lock, pp_readlock)  );
        }

        if (p_blob_encoding)
        {
            *p_blob_encoding = blob_encoding;
        }
        if (p_len_encoded)
        {
            *p_len_encoded = len_encoded;
        }
        if (p_len_full)
        {
            *p_len_full = len_full;
        }
        if (p_filenumber)
        {
            *p_filenumber = filenumber;
        }
        if (p_offset)
        {
            *p_offset = offset;
        }
    }
    else if (SQLITE_DONE == rc)
    {
		SG_context__err__generic(pCtx, SG_ERR_BLOB_NOT_FOUND, __FILE__, __LINE__);
        SG_ERR_IGNORE(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
    }

	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
}

void sg_blob_fs2__sql__fetch_info(
        SG_context * pCtx,
        my_instance_data * pData,
        const char * szHidBlob,
        SG_blob_encoding* p_blob_encoding,
        SG_uint64* p_len_encoded,
        SG_uint64* p_len_full,
        char** ppsz_hid_vcdiff_reference
        )
{
    SG_RETRY_THINGIE(
            my_fetch_info(pCtx, pData, szHidBlob, 0, NULL, NULL, NULL, p_blob_encoding, p_len_encoded, p_len_full, ppsz_hid_vcdiff_reference)
            );

fail:
    return;
}

void sg_blob_fs2_handle_store__free(SG_context* pCtx, sg_blob_fs2_handle_store* pbh)
{
    if (!pbh)
    {
        return;
    }

    /* TODO don't we have to free psz_hid_vcdiff_reference? */

	deflateEnd(&pbh->zStream);

    if (pbh->pRHH_ComputeOnStore)
	{
		// if this handle is being freed with an active repo_hash_handle, we destroy it too.
		// if everything went as expected, our caller should have finalized the hash and
		// gotten the computed HID and already freed the repo_hash_handle.
		SG_ERR_IGNORE(  sg_repo__fs2__hash__abort(pCtx, pbh->ptx->pData->pRepo, &pbh->pRHH_ComputeOnStore)  );
	}

	SG_FILE_NULLCLOSE(pCtx, pbh->pFileBlob);
    SG_NULLFREE(pCtx, pbh->psz_hid_blob_final);
    SG_NULLFREE(pCtx, pbh);
}

#define sg_FS2_MAX_FILE_LENGTH (1024*1024*1024)

static void sg_fs2__filenumber_to_path(SG_context * pCtx, const SG_pathname* pPathnameRepoDir, SG_uint32 filenumber, SG_pathname** ppPath)
{
    char buf[sg_FILENUMBER_BUFFER_LENGTH];
    SG_pathname* pPath = NULL;

    SG_ERR_CHECK(  sg_fs2__filenumber_to_filename(pCtx, buf, sizeof(buf), filenumber)  );
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pPathnameRepoDir, buf)  );

    *ppPath = pPath;

    return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath);
}

static void sg_fs2__find_a_place__already_locked(
    SG_context * pCtx,
    my_tx_data* ptx,
    SG_uint64 space,
    SG_uint32* p_filenumber
    )
{
    SG_pathname* pPath_file = NULL;
    SG_file* pFile = NULL;
    SG_uint64 len = 0;
    SG_uint32 result_filenumber = 0;
    const char* psz_filenumber = NULL;
    SG_bool b = SG_FALSE;
    SG_rbtree_iterator* pit = NULL;

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, ptx->prb_append_locks, &b, &psz_filenumber, NULL)  );
    while (b)
    {
        SG_uint32 filenumber = atoi(psz_filenumber);

        SG_PATHNAME_NULLFREE(pCtx, pPath_file);
        SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_file, ptx->pData->pPathMyDir, psz_filenumber)  );

        /* Now see if there is room to append this blob.  Note that
         * this check only really applies if the file exists and
         * has non-zero length.  Every blob has to go somewhere,
         * regardless of its length, so if a file is empty or
         * does not exist yet, we allow the blob to go in there. */

        SG_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath_file, &len, NULL)  );
        if (
                len
                && ((len + space) > sg_FS2_MAX_FILE_LENGTH)
           )
        {
            goto nope;
        }

        /* this file is okay. */
        result_filenumber = filenumber;

        break;

nope:
        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_filenumber, NULL)  );
    }

    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    SG_PATHNAME_NULLFREE(pCtx, pPath_file);

    *p_filenumber = result_filenumber;

    return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath_file);
    SG_FILE_NULLCLOSE(pCtx, pFile);
}

static void sg_fs2__get_lock_pathname(
        SG_context * pCtx,
        my_instance_data* pData,
        const char* psz_filenumber,
        const char* psz_suffix,
        SG_pathname** ppPath
        )
{
    SG_pathname* pPath_lock = NULL;
    char buf_lock[sg_MAX_LOCKFILE_BUFFER_LENGTH];

	SG_NULLARGCHECK_RETURN(psz_filenumber);
	SG_NULLARGCHECK_RETURN(psz_suffix);
	SG_NULLARGCHECK_RETURN(ppPath);

    SG_ERR_CHECK(  SG_strcpy(pCtx, buf_lock, sizeof(buf_lock), psz_filenumber)  );
    SG_ERR_CHECK(  SG_strcat(pCtx, buf_lock, sizeof(buf_lock), "_")  );
    SG_ERR_CHECK(  SG_strcat(pCtx, buf_lock, sizeof(buf_lock), psz_suffix)  );
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_lock, pData->pPathMyDir, buf_lock)  );

    *ppPath = pPath_lock;
    pPath_lock = NULL;

    return;

fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath_lock);
}

static void sg_fs2__create_lockish_file(
        SG_context * pCtx,
        SG_pathname* pPath_lock,
        SG_bool* pb
        )
{
    SG_file* pFile = NULL;

    SG_file__open__pathname(pCtx,pPath_lock,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,0644,&pFile);
    SG_FILE_NULLCLOSE(pCtx, pFile);

    /* TODO should we check specifically for the error wherein the lock
     * file already exists?  Or can we just continue the loop on any error
     * at all?  If we're going to check for the error specifically, we
     * probably need SG_file to return a special error code for this
     * case, rather than returning something errno/win32 wrapped. */
    if (SG_context__has_err(pCtx))
    {
        *pb = SG_FALSE;
		SG_context__err_reset(pCtx);
    }
    else
    {
        *pb = SG_TRUE;
    }
}

#define MAX_WAIT_FOR_GATE_MS 1000
#define RETRY_SLEEP_MS 100

#define SG_FS2__LOCK__APPEND        1
#define SG_FS2__LOCK__READ          2
#define SG_FS2__LOCK__HOLE          3
#define SG_FS2__LOCK__DRILL         4
#define SG_FS2__LOCK__VAC           5

static void sg_fs2__lock(
        SG_context * pCtx,
        my_instance_data* pData,
        const char* psz_filenumber,
        SG_uint16 lock_type,
        SG_pathname** ppLockFile
        )
{
    SG_pathname* pPath_gate = NULL;
    SG_pathname* pPath_append = NULL;
    SG_pathname* pPath_hole = NULL;
    SG_pathname* pPath_drill = NULL;
    SG_pathname* pPath_vac = NULL;
    SG_pathname* pPath_read = NULL;

    SG_pathname* pPath_lock_result = NULL;
    SG_uint32 ms_waited = 0;
    SG_bool b = SG_FALSE;
    SG_bool b_exists_append = SG_FALSE;
    SG_bool b_exists_hole = SG_FALSE;
    SG_bool b_exists_drill = SG_FALSE;
    SG_bool b_exists_vac = SG_FALSE;

	SG_NULLARGCHECK_RETURN(psz_filenumber);

    // for a plain readlock, we don't need the gate
    if (SG_FS2__LOCK__READ == lock_type)
    {
        char buf_suffix[sg_MAX_LOCKFILE_SUFFIX_LENGTH];
		char buf_tid[SG_TID_MAX_BUFFER_LENGTH];

		SG_ERR_CHECK(  SG_tid__generate2(pCtx, buf_tid, sizeof(buf_tid), sg_MAX_LOCKFILE_LENGTH_RANDOM)  );

        SG_ERR_CHECK(  SG_strcpy(pCtx, buf_suffix, sizeof(buf_suffix), "read_")  );
		SG_ERR_CHECK(  SG_strcat(pCtx, buf_suffix, sizeof(buf_suffix), &buf_tid[SG_TID_LENGTH_PREFIX])  );

        SG_ERR_CHECK(  sg_fs2__get_lock_pathname(pCtx, pData, psz_filenumber, buf_suffix, &pPath_read)  );

        b = SG_FALSE;
        SG_ERR_CHECK(  sg_fs2__create_lockish_file(pCtx, pPath_read, &b)  );
        if (b)
        {
            pPath_lock_result = pPath_read;
            pPath_read = NULL;
        }

        goto done;
    }

    // First grab the gate, which gives us access to the locks
    SG_ERR_CHECK(  sg_fs2__get_lock_pathname(pCtx, pData, psz_filenumber, "gate", &pPath_gate)  );
    while (1)
    {
        b = SG_FALSE;
        SG_ERR_CHECK(  sg_fs2__create_lockish_file(pCtx, pPath_gate, &b)  );
        if (b)
        {
            break;
        }

        if (ms_waited > MAX_WAIT_FOR_GATE_MS)
        {
            break;
        }

        // if we're after an append lock, we don't even bother
        // waiting for the gate.  just quit and allow the
        // caller to try the next file.
        if (SG_FS2__LOCK__APPEND == lock_type)
        {
            break;
        }

        SG_sleep_ms(RETRY_SLEEP_MS);
        ms_waited += RETRY_SLEEP_MS;
    }

    if (!b)
    {
        SG_PATHNAME_NULLFREE(pCtx, pPath_gate);
        goto done;
    }

    // we got the gate file.  we MUST NOT exit this function without
    // deleting it.  and we MUST exit this function as quickly as
    // possible.  no sleeping or waiting around.

    // create path objects for use later in the function
    SG_ERR_CHECK(  sg_fs2__get_lock_pathname(pCtx, pData, psz_filenumber, "hole", &pPath_hole)  );

    SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_hole, &b_exists_hole, NULL, NULL)  );

    if (SG_FS2__LOCK__HOLE == lock_type)
    {
        if (!b_exists_hole)
        {
            b = SG_FALSE;
            SG_ERR_CHECK(  sg_fs2__create_lockish_file(pCtx, pPath_hole, &b)  );
        }

        pPath_lock_result = pPath_hole;
        pPath_hole = NULL;

        goto done;
    }

    SG_ERR_CHECK(  sg_fs2__get_lock_pathname(pCtx, pData, psz_filenumber, "drill", &pPath_drill)  );
    SG_ERR_CHECK(  sg_fs2__get_lock_pathname(pCtx, pData, psz_filenumber, "vac", &pPath_vac)  );
    SG_ERR_CHECK(  sg_fs2__get_lock_pathname(pCtx, pData, psz_filenumber, "append", &pPath_append)  );

    SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_drill, &b_exists_drill, NULL, NULL)  );
    SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_vac, &b_exists_vac, NULL, NULL)  );
    SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_append, &b_exists_append, NULL, NULL)  );

    if (SG_FS2__LOCK__VAC == lock_type)
    {
        if (
                b_exists_drill
                || b_exists_vac
                || b_exists_append
                || !b_exists_hole
                )
        {
            goto done;
        }

        b = SG_FALSE;
        SG_ERR_CHECK(  sg_fs2__create_lockish_file(pCtx, pPath_vac, &b)  );
        if (b)
        {
            pPath_lock_result = pPath_vac;
            pPath_vac = NULL;
        }

        goto done;
    }

    if (SG_FS2__LOCK__DRILL == lock_type)
    {
        if (
                b_exists_drill
                || b_exists_vac
                )
        {
            goto done;
        }

        b = SG_FALSE;
        SG_ERR_CHECK(  sg_fs2__create_lockish_file(pCtx, pPath_drill, &b)  );
        if (b)
        {
            pPath_lock_result = pPath_drill;
            pPath_drill = NULL;
        }

        goto done;
    }

    if (SG_FS2__LOCK__APPEND == lock_type)
    {
        if (
                b_exists_hole
                || b_exists_append
                || b_exists_drill
                || b_exists_vac
                )
        {
            goto done;
        }

        b = SG_FALSE;
        SG_ERR_CHECK(  sg_fs2__create_lockish_file(pCtx, pPath_append, &b)  );
        if (b)
        {
            pPath_lock_result = pPath_append;
            pPath_append = NULL;
        }

        goto done;
    }

    SG_ERR_THROW(  SG_ERR_ASSERT  ); // this should never happen

done:

    if (ppLockFile)
    {
        *ppLockFile = pPath_lock_result;
        pPath_lock_result = NULL;
    }

    // fall thru

fail:
    if (pPath_gate)
    {
        SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_gate)  );
        SG_PATHNAME_NULLFREE(pCtx, pPath_gate);
    }
    SG_PATHNAME_NULLFREE(pCtx, pPath_lock_result);
    SG_PATHNAME_NULLFREE(pCtx, pPath_vac);
    SG_PATHNAME_NULLFREE(pCtx, pPath_drill);
    SG_PATHNAME_NULLFREE(pCtx, pPath_hole);
    SG_PATHNAME_NULLFREE(pCtx, pPath_append);
    SG_PATHNAME_NULLFREE(pCtx, pPath_read);

    return;
}

static void sg_fs2__find_a_place__new_lock(
    SG_context * pCtx,
    my_tx_data* ptx,
    SG_uint64 space,
    SG_uint32* p_filenumber
    )
{
    SG_pathname* pPath_file = NULL;
    char buf_file[sg_FILENUMBER_BUFFER_LENGTH];
    SG_file* pFile = NULL;
    SG_uint64 len = 0;
    SG_bool bExists;
    SG_pathname* pPath_lock = NULL;
    SG_uint32 filenumber = 0;

    /* Look through all the files and find one which has enough space for the proposed blob. */
    filenumber = 0;
    while (1)
    {
        SG_bool bAlreadyLocked = SG_FALSE;

        filenumber++;
        SG_ERR_CHECK(  sg_fs2__filenumber_to_filename(pCtx, buf_file, sizeof(buf_file), filenumber)  );

        /* skip anything where we already have the lock */
        SG_ERR_CHECK(  SG_rbtree__find(pCtx, ptx->prb_append_locks, buf_file, &bAlreadyLocked, NULL)  );
        if (bAlreadyLocked)
        {
            continue;
        }

        SG_PATHNAME_NULLFREE(pCtx, pPath_file);
        SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_file, ptx->pData->pPathMyDir, buf_file)  );

        bExists = SG_FALSE;
        SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_file, &bExists, NULL, NULL)  );
        if (bExists)
        {
            /* Now see if there is room to append this blob.  Note that
             * this check only really applies if the file exists and
             * has non-zero length.  Every blob has to go somewhere,
             * regardless of its length, so if a file is empty or
             * does not exist yet, we allow the blob to go in there. */

            SG_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath_file, &len, NULL)  );
            if (
                    len
                    && ((len + space) > sg_FS2_MAX_FILE_LENGTH)
               )
            {
                continue;
            }
        }

        /* OK, this is our file as long as we can get a lock on it */

        pPath_lock = NULL;
        SG_ERR_CHECK(  sg_fs2__lock(pCtx, ptx->pData, buf_file, SG_FS2__LOCK__APPEND, &pPath_lock)  );

        if (!pPath_lock)
        {
            continue;
        }

        /* add the lock to our list */
        SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, ptx->prb_append_locks, buf_file, pPath_lock)  );
        pPath_lock = NULL;

        /* OK, we got the lock, so the file is ours.  Now we have to
         * deal with the case where the file doesn't exist yet.  Just
         * create it empty. */
        SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_file, &bExists, NULL, NULL)  );
        if (!bExists)
        {
            SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath_file,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,0644,&pFile)  );
            SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );
        }

        /* If we get here, then all is okay.  We've got our file
         * and the lock on it, so we can exit the loop. */
        break;
    }
    SG_PATHNAME_NULLFREE(pCtx, pPath_file);

    *p_filenumber = filenumber;

    return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath_file);
    SG_FILE_NULLCLOSE(pCtx, pFile);
}

static void sg_fs2__find_a_place(
    SG_context * pCtx,
    my_tx_data* ptx,
    SG_uint64 space,
    SG_uint32* p_filenumber
    )
{
    SG_uint32 filenumber = 0;

    SG_ERR_CHECK(  sg_fs2__find_a_place__already_locked(pCtx, ptx, space, &filenumber)  );

    if (!filenumber)
    {
        SG_ERR_CHECK(  sg_fs2__find_a_place__new_lock(pCtx, ptx, space, &filenumber)  );
    }

    SG_ASSERT(filenumber);

    *p_filenumber = filenumber;

    return;
fail:
    return;
}

static void my_sg_blob_fs2__store_blob__begin(
    SG_context * pCtx,
    my_tx_data* ptx,
    SG_blob_encoding blob_encoding_given,
    const char* psz_hid_vcdiff_reference,
    SG_bool b_zlib,
    SG_uint64 len_encoded,
    SG_uint64 len_full,
    const char* psz_hid_known,
    sg_blob_fs2_handle_store** ppHandle
    )
{
    sg_blob_fs2_handle_store* pbh = NULL;
	int zError;
    SG_pathname* pPathnameFile = NULL;
    SG_uint64 space_needed = 0;

	SG_NULLARGCHECK_RETURN(ptx);

    /* TODO should we check here to see if the blob is already there?
     * the check is potentially expensive, since it requires us to
     * hit the sqlite db.  also, we can only check when psz_hid_known
     * is provided.  also, doing the check here doesn't spare us the
     * need to handle collisions during the tx commit, since if we
     * check here and the blob is not present, it might be in progress
     * on another thread/process. */

    SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(sg_blob_fs2_handle_store), &pbh)  );

    if (SG_IS_BLOBENCODING_FULL(blob_encoding_given))
    {
		SG_ERR_CHECK(  sg_repo__fs2__hash__begin(pCtx, ptx->pData->pRepo, &pbh->pRHH_ComputeOnStore)  );
    }
    else
    {
        SG_NULLARGCHECK(  psz_hid_known  );
    }

    pbh->ptx = ptx;
    pbh->blob_encoding_given = blob_encoding_given;
    pbh->psz_hid_vcdiff_reference = psz_hid_vcdiff_reference;
    pbh->len_full_given = len_full;
    pbh->psz_hid_known = psz_hid_known;

	memset(&pbh->zStream,0,sizeof(pbh->zStream));

    if (
            b_zlib
            && (SG_BLOBENCODING__FULL == blob_encoding_given)
       )
    {
        zError = deflateInit(&pbh->zStream,Z_DEFAULT_COMPRESSION);
        if (zError != Z_OK)
        {
            SG_ERR_THROW(  SG_ERR_ZLIB(zError)  );
        }
        pbh->b_compressing = SG_TRUE;
        pbh->blob_encoding_storing = SG_BLOBENCODING__ZLIB;

        space_needed = len_full;
    }
    else
    {
        pbh->blob_encoding_storing = pbh->blob_encoding_given;
        pbh->b_compressing = SG_FALSE;

        if (len_encoded > 0)
        {
            space_needed = len_encoded;
        }
        else
        {
            space_needed = len_full;
        }
    }

    SG_ERR_CHECK(  sg_fs2__find_a_place(pCtx, ptx, space_needed, &pbh->filenumber)  );

    SG_ERR_CHECK(  sg_fs2__filenumber_to_path(pCtx, ptx->pData->pPathMyDir, pbh->filenumber, &pPathnameFile)  );

    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathnameFile,SG_FILE_WRONLY|SG_FILE_OPEN_EXISTING,0644,&pbh->pFileBlob)  );
    SG_PATHNAME_NULLFREE(pCtx, pPathnameFile);
    SG_ERR_CHECK(  SG_file__seek_end(pCtx, pbh->pFileBlob, &pbh->offset)  );

    *ppHandle = pbh;
    pbh = NULL;

    return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPathnameFile);
    SG_ERR_IGNORE(  sg_blob_fs2_handle_store__free(pCtx, pbh)  );
}

void sg_blob_fs2__store_blob__begin(
    SG_context * pCtx,
    my_tx_data* ptx,
    SG_blob_encoding blob_encoding_given,
    const char* psz_hid_vcdiff_reference,
    SG_uint64 lenFull,
	SG_uint64 lenEncoded,
    const char* psz_hid_known,
    sg_blob_fs2_handle_store** ppHandle
    )
{
    SG_bool b_zlib = SG_FALSE;
    sg_blob_fs2_handle_store* pbh = NULL;

	SG_NULLARGCHECK_RETURN(ptx);

	if (ptx->pData->pBlobStoreHandle)
    {
		SG_ERR_THROW_RETURN(SG_ERR_INCOMPLETE_BLOB_IN_REPO_TX);
    }

    if (SG_BLOBENCODING__FULL == blob_encoding_given)
    {
        /* This is the place where our policy of always compressing new blobs
         * when we can is coded. To change that policy and have it triggered
         * from an option of some kind, change it here. */

        b_zlib = SG_TRUE;
    }

    SG_ERR_CHECK(  my_sg_blob_fs2__store_blob__begin(pCtx,
            ptx,
            blob_encoding_given,
            psz_hid_vcdiff_reference,
            b_zlib,
            lenEncoded,
            lenFull,
            psz_hid_known,
            &pbh)  );

	ptx->pData->pBlobStoreHandle = pbh;
    *ppHandle = pbh;

    return;
fail:
    return;
}

void sg_blob_fs2__store_blob__chunk(
    SG_context * pCtx,
    sg_blob_fs2_handle_store* pbh,
    SG_uint32 len_chunk,
    const SG_byte* p_chunk,
    SG_uint32* piBytesWritten
    )
{
	int zError;

    if (pbh->pRHH_ComputeOnStore)
    {
		SG_ERR_CHECK(  sg_repo__fs2__hash__chunk(pCtx, pbh->ptx->pData->pRepo, pbh->pRHH_ComputeOnStore, len_chunk, p_chunk)  );
    }

    if (pbh->b_compressing)
    {
        // give this chunk to compressor (it will update next_in and avail_in as
        // it consumes the input).

        pbh->zStream.next_in = (SG_byte*) p_chunk;
        pbh->zStream.avail_in = len_chunk;
        pbh->len_full_observed += len_chunk;

        while (1)
        {
            // give the compressor the complete output buffer

            pbh->zStream.next_out = pbh->bufOut;
            pbh->zStream.avail_out = MY_CHUNK_SIZE;

            // let compressor compress what it can of our input.  it may or
            // may not take all of it.  this will update next_in, avail_in,
            // next_out, and avail_out.

            zError = deflate(&pbh->zStream,Z_NO_FLUSH);
            if (zError != Z_OK)
            {
                SG_ERR_THROW(  SG_ERR_ZLIB(zError)  );
            }

            // if there was enough input to generate a compression block,
            // we can write it to our output file.  the amount generated
            // is the delta of avail_out (or equally of next_out).

            if (pbh->zStream.avail_out < MY_CHUNK_SIZE)
            {
                SG_uint32 nOut = MY_CHUNK_SIZE - pbh->zStream.avail_out;
                SG_ASSERT ( (nOut == (SG_uint32)(pbh->zStream.next_out - pbh->bufOut)) );

                SG_ERR_CHECK(  SG_file__write(pCtx, pbh->pFileBlob,nOut,pbh->bufOut,NULL)  );
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
        SG_ERR_CHECK(  SG_file__write(pCtx, pbh->pFileBlob,len_chunk,p_chunk,NULL)  );
        if (SG_IS_BLOBENCODING_FULL(pbh->blob_encoding_given))
        {
            pbh->len_full_observed += len_chunk;
        }
        pbh->len_encoded_observed += len_chunk;
    }

    if (piBytesWritten)
    {
        *piBytesWritten = len_chunk;
    }

    return;
fail:
    return;
}

void sg_blob_fs2__store_blob__end(
    SG_context * pCtx,
    sg_blob_fs2_handle_store** ppbh,
    char** ppsz_hid_returned
    )
{
    char* szHidBlob = NULL;
    sg_blob_fs2_handle_store* pbh = *ppbh;
    struct pending_blob_info* pbi = NULL;

    if (pbh->b_compressing)
    {
        int zError;

        // we reached end of the input file.  now we need to tell the
        // compressor that we have no more input and that it needs to
        // compress and flush any partial buffers that it may have.

        SG_ASSERT( (pbh->zStream.avail_in == 0) );
        while (1)
        {
            pbh->zStream.next_out = pbh->bufOut;
            pbh->zStream.avail_out = MY_CHUNK_SIZE;

            zError = deflate(&pbh->zStream,Z_FINISH);
            if ((zError != Z_OK) && (zError != Z_STREAM_END))
            {
                SG_ERR_THROW(  SG_ERR_ZLIB(zError)  );
            }

            if (pbh->zStream.avail_out < MY_CHUNK_SIZE)
            {
                SG_uint32 nOut = MY_CHUNK_SIZE - pbh->zStream.avail_out;
                SG_ASSERT ( (nOut == (SG_uint32)(pbh->zStream.next_out - pbh->bufOut)) );

                SG_ERR_CHECK(  SG_file__write(pCtx, pbh->pFileBlob,nOut,pbh->bufOut,NULL)  );

                pbh->len_encoded_observed += nOut;
            }

            if (zError == Z_STREAM_END)
                break;
        }
    }

    if (pbh->pRHH_ComputeOnStore)
    {
		// all of the data has been hashed now.  let the hash algorithm finalize stuff
		// and spit out an HID for us.  this allocates an HID and frees the repo_hash_handle.
		SG_ERR_CHECK(  sg_repo__fs2__hash__end(pCtx, pbh->ptx->pData->pRepo, &pbh->pRHH_ComputeOnStore, &szHidBlob)  );

        if (pbh->psz_hid_known)
        {
            if (0 != strcmp(szHidBlob, pbh->psz_hid_known))
            {
                SG_ERR_THROW(  SG_ERR_BLOB_NOT_VERIFIED_MISMATCH  );
            }
        }
    }
    else
    {
        /* we are trusting psz_hid_known */
        SG_ERR_CHECK(  SG_STRDUP(pCtx, pbh->psz_hid_known, &szHidBlob)  );
    }

    SG_ASSERT(szHidBlob);
    pbh->psz_hid_blob_final = szHidBlob;
    szHidBlob = NULL;

    if (SG_IS_BLOBENCODING_FULL(pbh->blob_encoding_given))
    {
        if (pbh->len_full_observed != pbh->len_full_given)
        {
            SG_ERR_THROW(  SG_ERR_BLOB_NOT_VERIFIED_INCOMPLETE  );
        }
    }

    SG_ERR_CHECK(  SG_file__close(pCtx, &pbh->pFileBlob)  );

    SG_ERR_CHECK(  SG_alloc(pCtx, sizeof(struct pending_blob_info), 1, &pbi)  );
    pbi->filenumber = pbh->filenumber;
    pbi->old_filenumber = pbh->old_filenumber;
    pbi->offset = pbh->offset;
    pbi->blob_encoding_storing = pbh->blob_encoding_storing;
    pbi->len_full_given = pbh->len_full_given;
    pbi->len_encoded_observed = pbh->len_encoded_observed;
    SG_ERR_CHECK(  SG_strcpy(pCtx, pbi->sz_hid_blob_final, sizeof(pbi->sz_hid_blob_final), pbh->psz_hid_blob_final)  );
	if (ppsz_hid_returned)
    {
        SG_ERR_CHECK(  SG_STRDUP(pCtx, pbh->psz_hid_blob_final, ppsz_hid_returned)  );
    }

    if (pbh->psz_hid_vcdiff_reference)
    {
        SG_ERR_CHECK(  SG_STRDUP(pCtx, pbh->psz_hid_vcdiff_reference, (char**) &pbi->psz_hid_vcdiff_reference)  );
    }
    SG_ERR_CHECK(  SG_vector__append(pCtx, pbh->ptx->pvec_blobs, pbi, NULL)  );

	// fall-thru to common cleanup

fail:
	SG_FILE_NULLCLOSE(pCtx, pbh->pFileBlob);
	SG_NULLFREE(pCtx, szHidBlob);

    SG_ERR_IGNORE(  sg_blob_fs2_handle_store__free(pCtx, pbh)  );

    /* The blob handle gets freed whether this function succeeds or fails,
     * so we null the reference */
    *ppbh = NULL;
}

SG_free_callback sg_f2__free_pbi;

void sg_fs2__free_pbi(SG_context* pCtx, void * pVoid_pbi)
{
	struct pending_blob_info* pbi = (struct pending_blob_info*)pVoid_pbi;

    if (!pbi)
    {
        return;
    }

    if (pbi->psz_hid_vcdiff_reference)
    {
        SG_NULLFREE(pCtx, pbi->psz_hid_vcdiff_reference);
    }
    SG_NULLFREE(pCtx, pbi);
}

void sg_blob_fs2__store_blob__abort(
    SG_context * pCtx,
    sg_blob_fs2_handle_store** ppbh
    )
{
    SG_ERR_IGNORE(  sg_blob_fs2_handle_store__free(pCtx, *ppbh)  );
    *ppbh = NULL;
}

static void _blob_handle__init_digest(SG_context * pCtx, sg_blob_fs2_handle_fetch * pbh)
{
	// If they want us to verify the HID using the actual contents of the Raw
	// Data that we stored in the BLOBFILE (after we uncompress/undeltafy/etc it),
	// we start a digest now.

	SG_ERR_CHECK_RETURN(  sg_repo__fs2__hash__begin(pCtx, pbh->pData->pRepo, &pbh->pRHH_VerifyOnFetch)  );
}

static void _sg_blob_handle__check_digest(SG_context * pCtx, sg_blob_fs2_handle_fetch * pbh)
{
	// verify that the digest that we computed as we read the contents
	// of the blob matches the HID that we think it should have.

	if (pbh->len_full_stored != 0 && pbh->len_encoded_stored != pbh->len_encoded_observed)
		SG_ERR_THROW_RETURN(SG_ERR_BLOB_NOT_VERIFIED_INCOMPLETE);

	if (pbh->pRHH_VerifyOnFetch)
	{
		char * pszHid_Computed = NULL;
		SG_bool bNoMatch;

		SG_ERR_CHECK_RETURN(  sg_repo__fs2__hash__end(pCtx, pbh->pData->pRepo, &pbh->pRHH_VerifyOnFetch, &pszHid_Computed)  );
		bNoMatch = (0 != strcmp(pszHid_Computed, pbh->sz_hid_requested));
		SG_NULLFREE(pCtx, pszHid_Computed);

		if (bNoMatch)
        {
			SG_ERR_THROW_RETURN(SG_ERR_BLOB_NOT_VERIFIED_MISMATCH);
        }
	}
}

static void _sg_blob_handle__close(SG_context * pCtx, sg_blob_fs2_handle_fetch * pbh);

void sg_fs2__fetch_blob_into_tempfile(
	SG_context * pCtx,
    my_instance_data* pData,
	const char* psz_hid_blob,
	SG_pathname** ppResult
	)
{
    sg_blob_fs2_handle_fetch* pbh = NULL;
    SG_uint64 len = 0;
    SG_uint32 got = 0;
    SG_byte* p_buf = NULL;
    SG_uint64 left = 0;
	SG_pathname* pTempDirPath = NULL;
	char buf_filename[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pTempFilePath = NULL;
	SG_file* pFile = NULL;
    SG_bool b_done = SG_FALSE;

	SG_NULLARGCHECK_RETURN(psz_hid_blob);

	SG_ERR_CHECK( SG_PATHNAME__ALLOC__USER_TEMP_DIRECTORY(pCtx, &pTempDirPath)  );

	SG_ERR_CHECK(  SG_tid__generate(pCtx, buf_filename, sizeof(buf_filename))  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pTempFilePath, pTempDirPath, buf_filename)  );
	SG_PATHNAME_NULLFREE(pCtx, pTempDirPath);

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pTempFilePath, SG_FILE_RDWR | SG_FILE_CREATE_NEW, SG_FSOBJ_PERMS__MASK, &pFile) );

    SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__begin(pCtx,
                pData,
                psz_hid_blob,
                SG_FS2__LOCK__READ,
                SG_TRUE,
                NULL,
                NULL,
                NULL,
                NULL,
                &len,
                &pbh)  );
    left = len;
    SG_ERR_CHECK(  SG_alloc(pCtx, SG_STREAMING_BUFFER_SIZE, 1, &p_buf)  );
    while (!b_done)
    {
        SG_uint32 want = SG_STREAMING_BUFFER_SIZE;
        if (want > left)
        {
            want = (SG_uint32) left;
        }
        SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__chunk(pCtx, pbh, want, p_buf, &got, &b_done)  );
        SG_ERR_CHECK(  SG_file__write(pCtx, pFile, got, p_buf, NULL)  );

        left -= got;
    }
    SG_ASSERT(0 == left);
    SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__end(pCtx, &pbh)  );
    SG_NULLFREE(pCtx, p_buf);
    SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

    *ppResult = pTempFilePath;

    return;
fail:
    SG_ERR_IGNORE(  sg_blob_fs2__fetch_blob__abort(pCtx, &pbh)  );
    SG_FILE_NULLCLOSE(pCtx, pFile);
    SG_NULLFREE(pCtx, p_buf);
}

static void sg_blob_fs2__open_seekreader(
	SG_context * pCtx,
    my_instance_data* pData,
	const char* szHidBlob, // HID of BLOBFILE that we want
	SG_seekreader** ppsr,
	SG_pathname** ppPath_tempfile
	)
{
    SG_seekreader* psr = NULL;
    SG_pathname* pPath_tempfile = NULL;

	SG_NULLARGCHECK_RETURN(pData);
	SG_NULLARGCHECK_RETURN(szHidBlob);
	SG_NULLARGCHECK_RETURN(ppsr);

    SG_ERR_CHECK(  sg_fs2__fetch_blob_into_tempfile(pCtx, pData, szHidBlob, &pPath_tempfile)  );
    SG_ERR_CHECK(  SG_seekreader__alloc__for_file(pCtx, pPath_tempfile, 0, &psr)  );

    *ppPath_tempfile = pPath_tempfile;
    pPath_tempfile = NULL;

    *ppsr = psr;
    psr = NULL;

	return;
fail:
    // TODO remove the tempfile?
    SG_PATHNAME_NULLFREE(pCtx, pPath_tempfile);
}

static void sg_fs2__open_blob_for_reading(
	SG_context * pCtx,
    my_instance_data* pData,
	const char* szHidBlob, // HID of BLOBFILE that we want
    SG_uint16 lock,
	SG_bool b_convert_to_full,
	sg_blob_fs2_handle_fetch ** ppbh // returned handle for BLOB -- must call our close
	)
{
	sg_blob_fs2_handle_fetch * pbh = NULL;
    SG_pathname* pPathnameFile = NULL;
    SG_pathname* pPath_readlock = NULL;
    SG_uint32 filenumber = 0;
    SG_uint64 offset = 0;
    SG_blob_encoding blob_encoding_stored = 0;
    SG_uint64 len_encoded_stored = 0;
    SG_uint64 len_full_stored = 0;
    char* psz_hid_vcdiff_reference_stored = NULL;

	SG_NULLARGCHECK_RETURN(pData);
	SG_NULLARGCHECK_RETURN(szHidBlob);
	SG_NULLARGCHECK_RETURN(ppbh);

	if (!pData->b_in_sqlite_transaction)
	{
		SG_RETRY_THINGIE(
			my_fetch_info(pCtx, pData, szHidBlob, lock, &pPath_readlock, &filenumber, &offset, &blob_encoding_stored, &len_encoded_stored, &len_full_stored, &psz_hid_vcdiff_reference_stored)
			);
	}
	else
	{
		SG_ERR_CHECK(  my_fetch_info(pCtx, pData, szHidBlob, lock, &pPath_readlock, &filenumber, &offset, &blob_encoding_stored, &len_encoded_stored, &len_full_stored, &psz_hid_vcdiff_reference_stored)  );
	}

    if (pPath_readlock)
    {
        SG_ERR_CHECK(  SG_alloc1(pCtx, pbh)  );
		pbh->pData = pData;

        SG_ERR_CHECK(  SG_strcpy(pCtx, pbh->sz_hid_requested, sizeof(pbh->sz_hid_requested), szHidBlob)  );
        pbh->pPath_readlock = pPath_readlock;
        pbh->filenumber = filenumber;
        pbh->offset = offset;
        pbh->blob_encoding_stored = blob_encoding_stored;
        pbh->len_encoded_stored = len_encoded_stored;
        pbh->len_full_stored = len_full_stored;
        pbh->psz_hid_vcdiff_reference_stored = psz_hid_vcdiff_reference_stored;
        psz_hid_vcdiff_reference_stored = NULL;

        SG_ERR_CHECK(  sg_fs2__filenumber_to_path(pCtx, pData->pPathMyDir, pbh->filenumber, &pPathnameFile)  );

        SG_file__open__pathname(pCtx,pPathnameFile,SG_FILE_RDONLY|SG_FILE_OPEN_EXISTING,
                                      SG_FSOBJ_PERMS__UNUSED,&pbh->m_pFileBlob);
        SG_PATHNAME_NULLFREE(pCtx, pPathnameFile);
        if (SG_context__has_err(pCtx))
        {
            SG_error err = SG_ERR_UNSPECIFIED;
            SG_context__get_err(pCtx, &err);

            switch (err)
            {
            default:
                SG_ERR_RETHROW;

#if defined(MAC) || defined(LINUX)
            case SG_ERR_ERRNO(ENOENT):
#endif
#if defined(WINDOWS)
            case SG_ERR_GETLASTERROR(ERROR_FILE_NOT_FOUND):
            case SG_ERR_GETLASTERROR(ERROR_PATH_NOT_FOUND):
#endif
                SG_ERR_RESET_THROW(SG_ERR_BLOB_NOT_FOUND);
            }
        }

        if (pbh->offset)
        {
            SG_ERR_CHECK(  SG_file__seek(pCtx, pbh->m_pFileBlob, pbh->offset)  );
        }

        if (
                SG_IS_BLOBENCODING_FULL(pbh->blob_encoding_stored)
                || b_convert_to_full
           )
        {
            SG_ERR_CHECK(  _blob_handle__init_digest(pCtx, pbh)  );
        }

        *ppbh = pbh;	// caller must call our close_handle routine to free this
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_REPO_BUSY  );
    }

	return;

fail:
	SG_ERR_IGNORE(  _sg_blob_handle__close(pCtx, pbh)  );
}

static void _sg_blob_handle__close(SG_context * pCtx, sg_blob_fs2_handle_fetch * pbh)
{
	// close and free blob handle.
	//
	// this can happen when either they have completely read the contents
	// of the BLOB -or- they are aborting because of an error (or lack of
	// interest in the rest of the blob).

	if (!pbh)
		return;

	inflateEnd(&pbh->zStream);

	if (pbh->pRHH_VerifyOnFetch)
	{
		// if this fetch-handle is being freed with an active repo_hash_handle, we destory it too.
		// if everything went as expected, our caller should have finalized the hash (called __end)
		// and gotten the computed HID (and already freed the repo_hash_handle).  so if we still have
		// one, we must assume that there was a problem and call __abort on it.

		SG_ERR_IGNORE(  sg_repo__fs2__hash__abort(pCtx, pbh->pData->pRepo, &pbh->pRHH_VerifyOnFetch)  );
	}

	SG_FILE_NULLCLOSE(pCtx, pbh->m_pFileBlob);
    SG_NULLFREE(pCtx, pbh->psz_hid_vcdiff_reference_stored);

	SG_NULLFREE(pCtx, pbh);
}

void sg_blob_fs2__setup_undeltify(
    SG_context * pCtx,
    my_instance_data* pData,
    const char* psz_hid_blob,
    SG_uint16 lock,
    sg_blob_fs2_handle_fetch* pbh,
    const char* psz_hid_vcdiff_reference
    )
{
    /* The reference blob needs to get into a seekreader. */
    SG_ERR_CHECK(  sg_blob_fs2__open_seekreader(pCtx,
                pData,
                psz_hid_vcdiff_reference,
                &pbh->psr_reference,
                &pbh->pPath_tempfile
                )  );

    /* The delta blob needs to be a readstream */
    SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__begin(pCtx,
                pData,
                psz_hid_blob,
                lock,
                SG_FALSE,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                &pbh->pbh_delta)  );
    SG_ERR_CHECK(  SG_readstream__alloc(pCtx,
                pbh->pbh_delta,
                (SG_stream__func__read*) sg_blob_fs2__fetch_blob__chunk,
                NULL,
                &pbh->pstrm_delta)  );

    /* Now begin the undeltify operation */
    SG_ERR_CHECK(  SG_vcdiff__undeltify__begin(pCtx, pbh->psr_reference, pbh->pstrm_delta, &pbh->pst)  );
    pbh->p_buf = NULL;
    pbh->count = 0;
    pbh->next = 0;

    return;
fail:
    return;
}

void sg_blob_fs2__fetch_blob__begin(
    SG_context * pCtx,
    my_instance_data* pData,
    const char* psz_hid_blob,
    SG_uint16 lock,
    SG_bool b_convert_to_full,
    char** ppsz_objectid,
    SG_blob_encoding* p_blob_encoding,
    char** ppsz_hid_vcdiff_reference,
    SG_uint64* pLenEncoded,
    SG_uint64* pLenFull,
    sg_blob_fs2_handle_fetch** ppHandle
    )
{
	sg_blob_fs2_handle_fetch * pbh = NULL;

    SG_UNUSED(ppsz_objectid); // TODO

	SG_ERR_CHECK_RETURN(  sg_fs2__open_blob_for_reading(pCtx,
                pData,
                psz_hid_blob,
                lock,
                b_convert_to_full,
                &pbh
                )  );

    if (
            b_convert_to_full
            && (!SG_IS_BLOBENCODING_FULL(pbh->blob_encoding_stored))
            )
    {
        pbh->blob_encoding_returning = SG_BLOBENCODING__FULL;
        pbh->psz_hid_vcdiff_reference_returning = NULL;

        if (SG_BLOBENCODING__ZLIB == pbh->blob_encoding_stored)
        {
            int zError;

            pbh->b_uncompressing = SG_TRUE;

            memset(&pbh->zStream,0,sizeof(pbh->zStream));
            zError = inflateInit(&pbh->zStream);
            if (zError != Z_OK)
            {
                SG_ERR_THROW(  SG_ERR_ZLIB(zError)  );
            }
        }
        else if (SG_BLOBENCODING__VCDIFF == pbh->blob_encoding_stored)
        {
            pbh->b_undeltifying = SG_TRUE;
            SG_ASSERT(pbh->psz_hid_vcdiff_reference_stored);
            SG_ERR_CHECK(  sg_blob_fs2__setup_undeltify(pCtx, pData, psz_hid_blob, lock, pbh, pbh->psz_hid_vcdiff_reference_stored)  );
        }
        else
        {
            /* should never happen */
            SG_ERR_THROW(  SG_ERR_ASSERT  );
        }
    }
    else
    {
        pbh->b_undeltifying = SG_FALSE;
        pbh->b_uncompressing = SG_FALSE;

        pbh->blob_encoding_returning = pbh->blob_encoding_stored;
        pbh->psz_hid_vcdiff_reference_returning = pbh->psz_hid_vcdiff_reference_stored;
    }

    if (p_blob_encoding)
    {
        *p_blob_encoding = pbh->blob_encoding_returning;
    }
    if (ppsz_hid_vcdiff_reference)
    {
        *ppsz_hid_vcdiff_reference = pbh->psz_hid_vcdiff_reference_returning;
    }
    if (pLenEncoded)
    {
        *pLenEncoded = pbh->len_encoded_stored;
    }
    if (pLenFull)
    {
        *pLenFull = pbh->len_full_stored;
    }

    if (ppHandle)
    {
        *ppHandle = pbh;
    }
    else
    {
        SG_ERR_IGNORE(  _sg_blob_handle__close(pCtx, pbh)  );
    }

    return;
fail:
    return;
}

void sg_blob_fs2__fetch_blob__chunk(
    SG_context * pCtx,
    sg_blob_fs2_handle_fetch* pbh,
    SG_uint32 len_buf,
    SG_byte* p_buf,
    SG_uint32* p_len_got,
    SG_bool* pb_done
    )
{
    SG_uint32 nbr = 0;
    SG_bool b_done = SG_FALSE;

    if (pbh->b_uncompressing)
    {
        int zError;

        if (0 == pbh->zStream.avail_in)
        {
            SG_uint32 want = sizeof(pbh->bufCompressed);
            if (want > (pbh->len_encoded_stored - pbh->len_encoded_observed))
            {
                want = (SG_uint32)(pbh->len_encoded_stored - pbh->len_encoded_observed);
            }

            SG_file__read(pCtx,pbh->m_pFileBlob,want,pbh->bufCompressed,&nbr);
			if(SG_context__has_err(pCtx) && !SG_context__err_equals(pCtx, SG_ERR_EOF))
				SG_ERR_RETHROW_RETURN;

            pbh->len_encoded_observed += nbr;

            pbh->zStream.next_in = pbh->bufCompressed;
            pbh->zStream.avail_in = nbr;
        }

        pbh->zStream.next_out = p_buf;
        pbh->zStream.avail_out = len_buf;

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

			zError = inflate(&pbh->zStream,Z_NO_FLUSH);
			if ((zError != Z_OK)  &&  (zError != Z_STREAM_END))
			{
				SG_ERR_THROW(  SG_ERR_ZLIB(zError)  );
			}

			if (zError == Z_STREAM_END)
            {
                b_done = SG_TRUE;
                break;
            }

			// if decompressor didn't take all of our input, let it continue
			// with the existing buffer (next_in was advanced).

			if (pbh->zStream.avail_in == 0)
            {
				break;
            }

			if (pbh->zStream.avail_out == 0)
            {
                /* no room left */
				break;
            }
		}

        if (pbh->zStream.avail_out < len_buf)
        {
            nbr = len_buf - pbh->zStream.avail_out;
        }
        else
        {
            nbr = 0;
        }
    }
    else if (pbh->b_undeltifying)
    {
        if (!pbh->p_buf || !pbh->count || (pbh->next >= pbh->count))
        {
            SG_ASSERT(pbh->pst);
            SG_ERR_CHECK(  SG_vcdiff__undeltify__chunk(pCtx, pbh->pst, &pbh->p_buf, &pbh->count)  );
            if (0 == pbh->count)
            {
                b_done = SG_TRUE;
            }
        }

        nbr = pbh->count - pbh->next;
        if (nbr > len_buf)
        {
            nbr = len_buf;
        }

        memcpy(p_buf, pbh->p_buf + pbh->next, nbr);
        pbh->next += nbr;

        /* We don't have a way of knowing how many bytes the vcdiff code has read
         * from our readstream.  But the readstream keeps track of this, so we just
         * ask it.
         */
        SG_ERR_CHECK(  SG_readstream__get_count(pCtx, pbh->pstrm_delta, &pbh->len_encoded_observed)  );
    }
    else
    {
        SG_uint32 want;

        want = len_buf;
        if (want > (pbh->len_encoded_stored - pbh->len_encoded_observed))
        {
            want = (SG_uint32)(pbh->len_encoded_stored - pbh->len_encoded_observed);
        }

        SG_file__read(pCtx, pbh->m_pFileBlob, want, p_buf, &nbr);
        if(SG_context__has_err(pCtx) && !SG_context__err_equals(pCtx, SG_ERR_EOF))
        {
			SG_ERR_RETHROW_RETURN;
        }

        pbh->len_encoded_observed += nbr;

        if (pbh->len_encoded_observed == pbh->len_encoded_stored)
        {
            b_done = SG_TRUE;
        }
    }

    if (nbr)
    {
        if (pbh->pRHH_VerifyOnFetch)
        {
            SG_ERR_CHECK_RETURN(  sg_repo__fs2__hash__chunk(pCtx, pbh->pData->pRepo, pbh->pRHH_VerifyOnFetch, nbr, p_buf)  );
        }
        if (SG_IS_BLOBENCODING_FULL(pbh->blob_encoding_returning))
        {
            pbh->len_full_observed += nbr;
        }
    }

    *p_len_got = nbr;
    *pb_done = b_done;

    return;
fail:
    return;
}

void sg_blob_fs2__fetch_blob__end(
    SG_context * pCtx,
    sg_blob_fs2_handle_fetch** ppbh
    )
{
    sg_blob_fs2_handle_fetch* pbh = NULL;

	SG_NULLARGCHECK_RETURN(ppbh);
    pbh = *ppbh;

    if (pbh->b_uncompressing)
    {
        SG_ASSERT( (pbh->zStream.avail_in == 0) );

        inflateEnd(&pbh->zStream);
    }
    else if (pbh->b_undeltifying)
    {
        SG_ERR_CHECK(  SG_vcdiff__undeltify__end(pCtx, pbh->pst)  );
        pbh->pst = NULL;

        SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__end(pCtx, &pbh->pbh_delta)  );

        SG_ERR_CHECK(  SG_readstream__close(pCtx, pbh->pstrm_delta)  );
        pbh->pstrm_delta = NULL;

        SG_ERR_CHECK(  SG_seekreader__close(pCtx, pbh->psr_reference)  );
        pbh->psr_reference = NULL;

        if (pbh->pPath_tempfile)
        {
            SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pbh->pPath_tempfile)  );
            SG_PATHNAME_NULLFREE(pCtx, pbh->pPath_tempfile);
        }

        pbh->p_buf = NULL;
        pbh->count = 0;
        pbh->next = 0;
    }

    SG_ERR_CHECK(  SG_file__close(pCtx, &pbh->m_pFileBlob)  );

    if (pbh->pPath_readlock)
    {
        SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pbh->pPath_readlock)  );
        SG_PATHNAME_NULLFREE(pCtx, pbh->pPath_readlock);
    }

	if (pbh->len_full_stored != 0 && pbh->len_encoded_observed != pbh->len_encoded_stored)
    {
        SG_ERR_THROW_RETURN(SG_ERR_BLOB_NOT_VERIFIED_INCOMPLETE);
    }

    if (SG_IS_BLOBENCODING_FULL(pbh->blob_encoding_returning))
    {
        if (pbh->len_full_observed != pbh->len_full_stored)
        {
            SG_ERR_THROW_RETURN(SG_ERR_BLOB_NOT_VERIFIED_INCOMPLETE);
        }
    }

	if (pbh->pRHH_VerifyOnFetch)
	{
		SG_ERR_CHECK_RETURN(  _sg_blob_handle__check_digest(pCtx, pbh)  );
	}

	SG_ERR_IGNORE(  _sg_blob_handle__close(pCtx, pbh)  );

    *ppbh = NULL;

    return;
fail:
    return;
}

void sg_blob_fs2__fetch_blob__abort(
    SG_context * pCtx,
    sg_blob_fs2_handle_fetch** ppbh
    )
{
    sg_blob_fs2_handle_fetch* pbh = NULL;

	SG_NULLARGCHECK_RETURN(ppbh);
    pbh = *ppbh;

    if (pbh && pbh->pPath_readlock)
    {
        SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pbh->pPath_readlock)  );
        SG_PATHNAME_NULLFREE(pCtx, pbh->pPath_readlock);
    }

	SG_ERR_IGNORE(  _sg_blob_handle__close(pCtx, *ppbh)  );
    *ppbh = NULL;
}

static void sg_blob_fs2__copy(
    SG_context * pCtx,
    my_tx_data* ptx,
    SG_byte* p_buf,
    SG_uint32 buf_size,
    const char* psz_hid_blob
    )
{
    sg_blob_fs2_handle_fetch* pbh_orig = NULL;
    sg_blob_fs2_handle_store* pbh_new = NULL;
    SG_uint64 len_full = 0;
    SG_uint64 len_encoded = 0;
    SG_uint64 left = 0;
    SG_blob_encoding encoding_stored;
    char* psz_hid_vcdiff_reference = NULL;
    SG_bool b_done = SG_FALSE;

    /*
     * Since this is [only] called from vacuum, we don't
     * need a drill lock.
     */

    SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__begin(pCtx,
                ptx->pData,
                psz_hid_blob,
                SG_FS2__LOCK__READ,
                SG_FALSE,
                NULL,
                &encoding_stored,
                &psz_hid_vcdiff_reference,
                &len_encoded,
                &len_full,
                &pbh_orig)  );

    SG_ERR_CHECK(  my_sg_blob_fs2__store_blob__begin(pCtx,
                ptx,
                encoding_stored,
                psz_hid_vcdiff_reference,
                SG_FALSE,
                len_encoded,
                len_full,
                psz_hid_blob,
                &pbh_new)  );
    pbh_new->old_filenumber = pbh_orig->filenumber;

    left = len_encoded;
    while (!b_done)
    {
        SG_uint32 want = buf_size;
        SG_uint32 got = 0;

        if (want > left)
        {
            want = (SG_uint32) left;
        }
        SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__chunk(pCtx, pbh_orig, want, p_buf, &got, &b_done)  );
        SG_ERR_CHECK(  sg_blob_fs2__store_blob__chunk(pCtx, pbh_new, got, p_buf, NULL)  );

        left -= got;
    }

    SG_ERR_CHECK(  sg_blob_fs2__store_blob__end(pCtx, &pbh_new, NULL)  );
    SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__end(pCtx, &pbh_orig)  );

    SG_ASSERT(0 == left);

    return;

fail:
    SG_ERR_IGNORE(  sg_blob_fs2__store_blob__abort(pCtx, &pbh_new)  );
    SG_ERR_IGNORE(  sg_blob_fs2__fetch_blob__abort(pCtx, &pbh_orig)  );
}

void sg_blob_fs2__change_encoding_to_full_or_zlib(
    SG_context * pCtx,
    my_tx_data* ptx,
    const char* psz_hid_blob,
    SG_bool b_zlib
    )
{
    sg_blob_fs2_handle_fetch* pbh_orig = NULL;
    sg_blob_fs2_handle_store* pbh_new = NULL;
    SG_uint64 len_full = 0;
    SG_uint64 left = 0;
    SG_byte* p_buf = NULL;
    SG_uint32 buf_size = 0;
    SG_bool b_done = SG_FALSE;

	SG_NULLARGCHECK_RETURN(ptx);

    buf_size = SG_STREAMING_BUFFER_SIZE;
    SG_ERR_CHECK(  SG_alloc(pCtx, buf_size, 1, &p_buf)  );

    SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__begin(pCtx,
                ptx->pData,
                psz_hid_blob,
                SG_FS2__LOCK__DRILL,
                SG_TRUE,
                NULL,
                NULL,
                NULL,
                NULL,
                &len_full,
                &pbh_orig)  );

    // move the drill lock into the tx
    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, ptx->prb_drill_locks, psz_hid_blob, pbh_orig->pPath_readlock)  );
    pbh_orig->pPath_readlock = NULL;

    SG_ERR_CHECK(  my_sg_blob_fs2__store_blob__begin(pCtx,
                ptx,
                SG_BLOBENCODING__FULL,
                NULL,
                b_zlib,
                0,
                len_full,
                psz_hid_blob,
                &pbh_new)  );
    pbh_new->old_filenumber = pbh_orig->filenumber;

    left = len_full;
    while (!b_done)
    {
        SG_uint32 want = buf_size;
        SG_uint32 got = 0;

        if (want > left)
        {
            want = (SG_uint32) left;
        }
        SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__chunk(pCtx, pbh_orig, want, p_buf, &got, &b_done)  );
        SG_ERR_CHECK(  sg_blob_fs2__store_blob__chunk(pCtx, pbh_new, got, p_buf, NULL)  );

        left -= got;
    }

    SG_ERR_CHECK(  sg_blob_fs2__store_blob__end(pCtx, &pbh_new, NULL)  );
    SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__end(pCtx, &pbh_orig)  );

    SG_ASSERT(0 == left);

    SG_NULLFREE(pCtx, p_buf);

    return;

fail:
    SG_NULLFREE(pCtx, p_buf);
    SG_ERR_IGNORE(  sg_blob_fs2__store_blob__abort(pCtx, &pbh_new)  );
    SG_ERR_IGNORE(  sg_blob_fs2__fetch_blob__abort(pCtx, &pbh_orig)  );
}

void sg_blob_fs2__change_encoding_to_vcdiff(
    SG_context * pCtx,
    my_tx_data* ptx,
    const char* psz_hid_blob,
    const char* psz_hid_vcdiff_reference
    )
{
    sg_blob_fs2_handle_fetch* pbh_target = NULL;
    sg_blob_fs2_handle_store* pbh_delta = NULL;
    SG_seekreader* psr_reference = NULL;
    SG_readstream* pstrmTarget = NULL;
    SG_writestream* pstrmDelta = NULL;
    SG_uint64 len_full = 0;
    SG_pathname* pPath_tempfile = NULL;

	SG_NULLARGCHECK_RETURN(ptx);

    /* The reference blob needs to get into a seekreader. */
	SG_ERR_CHECK(  sg_blob_fs2__open_seekreader(pCtx,
                ptx->pData,
                psz_hid_vcdiff_reference,
                &psr_reference,
                &pPath_tempfile
                )  );

    /* The blob we're deltifying needs to be a readstream */
    SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__begin(pCtx,
                ptx->pData,
                psz_hid_blob,
                SG_FS2__LOCK__DRILL,
                SG_TRUE,
                NULL,
                NULL,
                NULL,
                NULL,
                &len_full,
                &pbh_target)  );

    // move the drill lock into the tx
    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, ptx->prb_drill_locks, psz_hid_blob, pbh_target->pPath_readlock)  );
    pbh_target->pPath_readlock = NULL;

    SG_ERR_CHECK(  SG_readstream__alloc(pCtx,
                pbh_target,
                (SG_stream__func__read*) sg_blob_fs2__fetch_blob__chunk,
                NULL,
                &pstrmTarget)  );

    /* The new delta needs to be a writestream. */

    SG_ERR_CHECK(  my_sg_blob_fs2__store_blob__begin(pCtx,
                ptx,
                SG_BLOBENCODING__VCDIFF,
                psz_hid_vcdiff_reference,
                SG_FALSE,
                0,
                len_full,
                psz_hid_blob,
                &pbh_delta)  );
    pbh_delta->old_filenumber = pbh_target->filenumber;
    SG_ERR_CHECK(  SG_writestream__alloc(pCtx,
                pbh_delta,
                (SG_stream__func__write*) sg_blob_fs2__store_blob__chunk,
                NULL,
                &pstrmDelta)  );

    SG_ERR_CHECK(  SG_vcdiff__deltify__streams(pCtx, psr_reference, pstrmTarget, pstrmDelta)  );

    SG_ERR_CHECK(  SG_readstream__close(pCtx, pstrmTarget)  );
    pstrmTarget = NULL;
    SG_ERR_CHECK(  SG_writestream__close(pCtx, pstrmDelta)  );
    pstrmDelta = NULL;
    SG_ERR_CHECK(  SG_seekreader__close(pCtx, psr_reference)  );
    psr_reference = NULL;

    if (pPath_tempfile)
    {
        SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath_tempfile)  );
        SG_PATHNAME_NULLFREE(pCtx, pPath_tempfile);
    }

    SG_ERR_CHECK(  sg_blob_fs2__store_blob__end(pCtx, &pbh_delta, NULL)  );
    SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__end(pCtx, &pbh_target)  );

    return;

fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath_tempfile);

    SG_ERR_IGNORE(  sg_blob_fs2__store_blob__abort(pCtx, &pbh_delta)  );
    SG_ERR_IGNORE(  sg_blob_fs2__fetch_blob__abort(pCtx, &pbh_target)  );
    if (psr_reference)
    {
        SG_ERR_IGNORE(  SG_seekreader__close(pCtx, psr_reference)  );
    }
}

void sg_blob_fs2__change_blob_encoding(
    SG_context * pCtx,
    my_tx_data* ptx,
    const char* psz_hid_blob,
    SG_blob_encoding blob_encoding_desired,
    const char* psz_hid_vcdiff_reference_desired,
    SG_blob_encoding* p_blob_encoding_new,
    char** ppsz_hid_vcdiff_reference,
    SG_uint64* p_len_encoded,
    SG_uint64* p_len_full
    )
{
    SG_blob_encoding blob_encoding_stored = 0;
    SG_uint64 len_encoded_stored = 0;
    SG_uint64 len_full_stored = 0;
    char* psz_hid_vcdiff_reference_stored = NULL;

	SG_NULLARGCHECK_RETURN(ptx);
	SG_NULLARGCHECK_RETURN(psz_hid_blob);

	SG_ERR_CHECK(  sg_blob_fs2__sql__fetch_info(pCtx,
                ptx->pData,
                psz_hid_blob,
                &blob_encoding_stored,
                &len_encoded_stored,
                &len_full_stored,
                &psz_hid_vcdiff_reference_stored
                )  );


    switch (blob_encoding_desired)
    {
        case SG_BLOBENCODING__ALWAYSFULL:
            {
                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  ); // TODO
            }
            break;

        case SG_BLOBENCODING__FULL:
            {
                if (!SG_IS_BLOBENCODING_FULL(blob_encoding_stored))
                {
                    SG_ERR_CHECK(  sg_blob_fs2__change_encoding_to_full_or_zlib(pCtx,
                                ptx,
                                psz_hid_blob,
                                SG_FALSE
                                )  );
                }
                else if (SG_BLOBENCODING__ALWAYSFULL == blob_encoding_stored)
                {
                    SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  ); // TODO
                }
            }
            break;

        case SG_BLOBENCODING__ZLIB:
            {
                if (
                        (SG_BLOBENCODING__ZLIB != blob_encoding_stored)
                        && (SG_BLOBENCODING__ALWAYSFULL != blob_encoding_stored)
                   )
                {
                    SG_ERR_CHECK(  sg_blob_fs2__change_encoding_to_full_or_zlib(pCtx,
                                ptx,
                                psz_hid_blob,
                                SG_TRUE
                                )  );
                }
            }
            break;

        case SG_BLOBENCODING__VCDIFF:
            {
                SG_NULLARGCHECK_RETURN(psz_hid_vcdiff_reference_desired);

                /* Note that we check to see if the blob is already in
                 * vcdiff format, but we do not check to see if the
                 * reference is the one that was requested.  TODO should we? */

                if (
                        (SG_BLOBENCODING__VCDIFF != blob_encoding_stored)
                        && (SG_BLOBENCODING__ALWAYSFULL != blob_encoding_stored)
                   )
                {
                    SG_ERR_CHECK(  sg_blob_fs2__change_encoding_to_vcdiff(pCtx,
                                ptx,
                                psz_hid_blob,
                                psz_hid_vcdiff_reference_desired
                                )  );
                }

            }
            break;

        default:
            SG_ARGCHECK_RETURN(  SG_FALSE  ,  blob_encoding_desired  );
    }

    SG_NULLFREE(pCtx, psz_hid_vcdiff_reference_stored);

    // TODO is this fetch_info call needed here?  why?  why can't the info we
    // need be passed up from the funcs we call above?

	SG_ERR_CHECK(  sg_blob_fs2__sql__fetch_info(pCtx,
                ptx->pData,
                psz_hid_blob,
                &blob_encoding_stored,
                &len_encoded_stored,
                &len_full_stored,
                &psz_hid_vcdiff_reference_stored
                )  );

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

    return;
fail:
    return;
}

static void sg_blob_fs2__sql__find_by_hid_prefix(SG_context * pCtx, sqlite3 * psql, const char * psz_hid_prefix, SG_rbtree** pprb)
{
	sqlite3_stmt * pStmt = NULL;
    SG_rbtree* prb = NULL;
    int rc;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULLARGCHECK_RETURN(psz_hid_prefix);
	SG_NULLARGCHECK_RETURN(pprb);

    // TODO sqlite cannot use an index for LIKE queries.
    // consider replacing the line below with
    // WHERE hid >= %s AND hid < OTHER
    // with OTHER calculated by adding one to
    // the given hex string.
    // http://web.utk.edu/~jplyon/sqlite/SQLite_optimization_FAQ.html
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql,&pStmt,
									  "SELECT hid FROM directory WHERE hid LIKE '%s%%'", psz_hid_prefix)  );

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );

    while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
    {
        const char * szHid = (const char *)sqlite3_column_text(pStmt,0);

        SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb,szHid)  );
    }
    if (rc != SQLITE_DONE)
    {
        SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
    }

    SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

    *pprb = prb;

	return;
fail:
    SG_RBTREE_NULLFREE(pCtx, prb);
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
}

void sg_blob_fs2__get_blob_stats(
    SG_context * pCtx,
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
    )
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
	sqlite3_stmt * pStmt = NULL;
    int rc;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql,&pStmt,
									  "SELECT encoding, len_encoded, len_full FROM directory")  );

    while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
    {
        SG_blob_encoding encoding = (SG_blob_encoding) sqlite3_column_int(pStmt, 0);
        SG_uint64 len_encoded = sqlite3_column_int64(pStmt, 1);
        SG_uint64 len_full = sqlite3_column_int64(pStmt, 2);

        if (SG_BLOBENCODING__FULL == encoding)
        {
            count_blobs_full++;
            total_blob_size_full += len_full;
        }
        else if (SG_BLOBENCODING__ALWAYSFULL == encoding)
        {
            count_blobs_alwaysfull++;
            total_blob_size_alwaysfull += len_full;
        }
        else if (SG_BLOBENCODING__ZLIB == encoding)
        {
            count_blobs_zlib++;
            total_blob_size_zlib_full += len_full;
            total_blob_size_zlib_encoded += len_encoded;
        }
        else if (SG_BLOBENCODING__VCDIFF == encoding)
        {
            count_blobs_vcdiff++;
            total_blob_size_vcdiff_full += len_full;
            total_blob_size_vcdiff_encoded += len_encoded;
        }
        else
        {
            // should never happen
            SG_ERR_THROW(  SG_ERR_ASSERT  );
        }
    }
    if (rc != SQLITE_DONE)
    {
        SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
    }

    SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

    if (p_count_blobs_full)
    {
        *p_count_blobs_full = (SG_uint32) count_blobs_full;
    }
    if (p_count_blobs_alwaysfull)
    {
        *p_count_blobs_alwaysfull = (SG_uint32) count_blobs_alwaysfull;
    }
    if (p_count_blobs_zlib)
    {
        *p_count_blobs_zlib = (SG_uint32) count_blobs_zlib;
    }
    if (p_count_blobs_vcdiff)
    {
        *p_count_blobs_vcdiff = (SG_uint32) count_blobs_vcdiff;
    }

    if (p_total_blob_size_full)
    {
        *p_total_blob_size_full = (SG_uint32) total_blob_size_full;
    }
    if (p_total_blob_size_alwaysfull)
    {
        *p_total_blob_size_alwaysfull = (SG_uint32) total_blob_size_alwaysfull;
    }
    if (p_total_blob_size_zlib_full)
    {
        *p_total_blob_size_zlib_full = (SG_uint32) total_blob_size_zlib_full;
    }
    if (p_total_blob_size_zlib_encoded)
    {
        *p_total_blob_size_zlib_encoded = (SG_uint32) total_blob_size_zlib_encoded;
    }
    if (p_total_blob_size_vcdiff_full)
    {
        *p_total_blob_size_vcdiff_full = (SG_uint32) total_blob_size_vcdiff_full;
    }
    if (p_total_blob_size_vcdiff_encoded)
    {
        *p_total_blob_size_vcdiff_encoded = (SG_uint32) total_blob_size_vcdiff_encoded;
    }

    return;

fail:
    SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
    return;
}

void sg_blob_fs2__list_blobs(
    SG_context * pCtx,
        sqlite3* psql,
        SG_blob_encoding blob_encoding, /**< only return blobs of this encoding */
        SG_bool b_sort_descending,      /**< otherwise sort ascending */
        SG_bool b_sort_by_len_encoded,  /**< otherwise sort by len_full */
        SG_uint32 limit,                /**< only return this many entries */
        SG_uint32 offset,               /**< skip the first group of the sorted entries */
        SG_vhash** ppvh                 /**< Caller must free */
        )
{
    int rc;
	sqlite3_stmt * pStmt = NULL;
    SG_vhash* pvh = NULL;
    SG_vhash* pvh_blob = NULL;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql,&pStmt,
									  "SELECT hid,len_full,len_encoded,hid_vcdiff FROM directory WHERE encoding=? ORDER BY %s %s LIMIT ? OFFSET ?", b_sort_by_len_encoded ? "len_encoded" : "len_full", b_sort_descending ? "DESC" : "ASC")  );

	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt,1,blob_encoding)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt,2,limit)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt,3,offset)  );

    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );

    while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
    {
        const char * psz_hid = (const char *)sqlite3_column_text(pStmt,0);
        SG_uint64 len_encoded = sqlite3_column_int64(pStmt, 1);
        SG_uint64 len_full = sqlite3_column_int64(pStmt, 2);
        const char * psz_hid_vcdiff = (const char *)sqlite3_column_text(pStmt,3);

        SG_ERR_CHECK(  SG_VHASH__ALLOC__SHARED(pCtx, &pvh_blob, 3, pvh)  );
        SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_blob, "len_encoded", len_encoded)  );
        SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_blob, "len_full", len_full)  );
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
    SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
    SG_VHASH_NULLFREE(pCtx, pvh_blob);
    SG_VHASH_NULLFREE(pCtx, pvh);
}


static void _my_make_db_pathname(SG_context * pCtx, my_instance_data* pData, SG_pathname ** ppResult)
{
	// create the pathname for the SQL DB file.

	SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, ppResult, pData->pPathMyDir, "fs2.sqlite3")  );
}

static void sg_fs2__vacuum__list_blobs_to_move(
    SG_context * pCtx,
    sqlite3 * psql,
    SG_uint32 filenumber,
    SG_rbtree* prb,
    SG_uint64* pi_trunc_point,
    SG_uint64* pi_total_bytes_to_move
    )
{
    int rc;
	sqlite3_stmt * pStmt = NULL;
    SG_uint64 cur_pos = 0;
    SG_bool b_saw_a_hole = SG_FALSE;
    SG_uint64 total_bytes_to_move = 0;
    SG_uint64 first_hole = 0;

    SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql,&pStmt, "SELECT hid, offset, len_encoded FROM directory WHERE filenumber=? ORDER BY offset ASC")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt,1,filenumber)  );

    while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
    {
        const char * psz_hid = (const char *)sqlite3_column_text(pStmt,0);
        SG_uint64 offset = sqlite3_column_int64(pStmt, 1);
        SG_uint64 len_encoded = sqlite3_column_int64(pStmt, 2);

        if (offset != cur_pos)
        {
            if (!b_saw_a_hole)
            {
                first_hole = cur_pos;
            }
            b_saw_a_hole = SG_TRUE;
            cur_pos = offset;
        }

        cur_pos += len_encoded;

        if (b_saw_a_hole)
        {
            SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, psz_hid)  );
            total_bytes_to_move += len_encoded;
        }
    }
    if (rc != SQLITE_DONE)
    {
        SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
    }

    SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

    if (!b_saw_a_hole)
    {
        first_hole = cur_pos;
    }

    *pi_trunc_point = first_hole;
    *pi_total_bytes_to_move = total_bytes_to_move;

    return;
fail:
    SG_ERR_IGNORE(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
    return;
}

#define MAX_WAIT_FOR_VAC_MS 1000
#define VAC_SLEEP_MS 100

static void sg_fs2__wait_until_the_coast_is_clear(
        SG_context * pCtx,
        my_instance_data* pData,
        SG_uint32 filenumber,
        SG_bool* pb
        )
{
    char buf_filename[sg_FILENUMBER_BUFFER_LENGTH];
    SG_uint32 slept = 0;

    SG_ERR_CHECK_RETURN(  sg_fs2__filenumber_to_filename(pCtx, buf_filename, sizeof(buf_filename), filenumber)  );

    while (1)
    {
        SG_uint32 count = 0;

        SG_ERR_CHECK_RETURN(  SG_dir__count(pCtx, pData->pPathMyDir, buf_filename, "_read", NULL, &count)  );
        if (count)
        {
            goto nope;
        }

        *pb = SG_TRUE;
        return;

nope:
        if (slept >= MAX_WAIT_FOR_VAC_MS)
        {
            break;
        }

        SG_sleep_ms(VAC_SLEEP_MS);
        slept += VAC_SLEEP_MS;
    }

    *pb = SG_FALSE;
    return;
}

static void sg_fs2__vacuum__do_one_file(
    SG_context * pCtx,
    my_instance_data* pData,
    SG_uint32 filenumber
    )
{
    SG_rbtree* prb_blobs_to_move = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_bool b;
    const char* psz_hid = NULL;
    SG_byte* p_buf = NULL;
    SG_uint32 buf_size = 0;
    SG_pathname* pPath = NULL;
    my_tx_data* ptx = NULL;
    SG_bool b_ok = SG_FALSE;
    char buf_filename[sg_FILENUMBER_BUFFER_LENGTH];
    SG_pathname* pPath_lock_vac = NULL;
    SG_uint64 trunc_point = 0;
    SG_uint64 file_length_before_vacuum = 0;
    SG_file* pFile = NULL;
    SG_uint32 count_to_move = 0;
    SG_uint64 total_bytes_to_move = 0;
    SG_uint64 bytes_saved = 0;
    double ratio = 0;

    SG_ERR_CHECK(  sg_fs2__filenumber_to_filename(pCtx, buf_filename, sizeof(buf_filename), filenumber)  );

    SG_ERR_CHECK(  sg_fs2__lock(pCtx, pData, buf_filename, SG_FS2__LOCK__VAC, &pPath_lock_vac)  );

    if (!pPath_lock_vac)
    {
        goto done;
    }

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_blobs_to_move)  );

    SG_RETRY_THINGIE(
        sg_fs2__vacuum__list_blobs_to_move(pCtx, pData->psql, filenumber, prb_blobs_to_move, &trunc_point, &total_bytes_to_move)
        );

    SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_blobs_to_move, &count_to_move)  );
    SG_ERR_CHECK(  sg_fs2__filenumber_to_path(pCtx, pData->pPathMyDir, filenumber, &pPath)  );
    SG_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath, &file_length_before_vacuum, NULL)  );

    bytes_saved = (file_length_before_vacuum - trunc_point) - total_bytes_to_move;

    ratio = bytes_saved / (double) total_bytes_to_move;

    SG_ASSERT(file_length_before_vacuum >= trunc_point);
    SG_ASSERT((file_length_before_vacuum - trunc_point) >= total_bytes_to_move);

    // TODO here we have a policy decision which should probably
    // be made somewhere outside this code.  Perhaps the caller
    // of vacuum should provide something instructing us how
    // hard we should work.

    if (ratio < 0.5)
    {
        goto done;
    }

    if (count_to_move)
    {
        /* begin our repo tx */
        SG_ERR_CHECK(  sg_fs2__begin_tx(pCtx, pData, &ptx)  );

        /* now move all those blobs */

        buf_size = SG_STREAMING_BUFFER_SIZE;
        SG_ERR_CHECK(  SG_alloc(pCtx, buf_size, 1, &p_buf)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_blobs_to_move, &b, &psz_hid, NULL)  );
        while (b)
        {
            /* sg_blob_fs2__copy has the effect of moving the blob from one file to
             * another.  because we never append a blob to a file which already has
             * a hole, we know we are not making our problem worse here. */

            SG_ERR_CHECK(  sg_blob_fs2__copy(pCtx, ptx, p_buf, buf_size, psz_hid)  );

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid, NULL)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
        SG_RBTREE_NULLFREE(pCtx, prb_blobs_to_move);
        SG_NULLFREE(pCtx, p_buf);

        /* now we end the repo API tx.  but all the locks we grabbed before are
         * still in effect.  those locks are being held on the files which
         * need to be truncated. */

        SG_ERR_CHECK(  sg_fs2__commit_tx(pCtx, pData, &ptx)  );
    }

    // now there should be no directory entries for this filenumber

    SG_ERR_CHECK(  sg_fs2__wait_until_the_coast_is_clear(pCtx, pData, filenumber, &b_ok)  );

    if (b_ok)
    {
        // then truncate or remove the file

        if (trunc_point > 0)
        {
            if (trunc_point < file_length_before_vacuum)
            {
                SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath,SG_FILE_WRONLY|SG_FILE_OPEN_EXISTING,0644,&pFile)  );
                SG_ERR_CHECK(  SG_file__seek(pCtx, pFile, trunc_point)  );
                SG_ERR_CHECK(  SG_file__truncate(pCtx, pFile)  );
                SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );
            }
        }
        else
        {
            SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath)  );
        }
        SG_PATHNAME_NULLFREE(pCtx, pPath);

        // and remove its _hole file

        SG_ERR_CHECK(  sg_fs2__get_lock_pathname(pCtx, pData, buf_filename, "hole", &pPath)  );
        SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPath)  );
        SG_PATHNAME_NULLFREE(pCtx, pPath);
    }
    else
    {
        // we consider this a non-error.  everything has been moved out
        // of the file.  we just can't delete it yet.  so we leave
        // the file AND its _hole file and move on.
    }

done:


fail:
    SG_FILE_NULLCLOSE(pCtx, pFile);

    if (pPath_lock_vac)
    {
        SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_lock_vac)  );
        SG_PATHNAME_NULLFREE(pCtx, pPath_lock_vac);
    }

    if (ptx)
    {
        // TODO abort the tx
    }

    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_RBTREE_NULLFREE(pCtx, prb_blobs_to_move);
    SG_NULLFREE(pCtx, p_buf);
}

static void sg_fs2__vacuum(
    SG_context * pCtx,
    my_instance_data* pData
    )
{
    SG_rbtree* prb_holes = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_filename = NULL;

    SG_ERR_CHECK(  SG_dir__list(pCtx, pData->pPathMyDir, NULL, NULL, "_hole", &prb_holes)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_holes, &b, &psz_filename, NULL)  );
    while (b)
    {
        // atoi works here because the file is dddddd_hole and
        // atoi stops on the first non-digit.  Cheesy.

        SG_uint32 filenumber = (SG_uint32) atoi(psz_filename);

        SG_ERR_CHECK(  sg_fs2__vacuum__do_one_file(pCtx, pData, filenumber)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_filename, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_holes);

    return;
}

void sg_repo__fs2__open_repo_instance(SG_context * pCtx, SG_repo * pRepo)
{
	my_instance_data * pData = NULL;
	SG_pathname * pPathnameSqlDb = NULL;
    sqlite3* psql = NULL;
    const char* pszParentDir = NULL;
    const char* psz_subdir_name = NULL;
    SG_string* pstr_prop = NULL;
	char * pszTrivialHash_Computed = NULL;
	SG_bool bEqual_TrivialHash;

	SG_NULLARGCHECK_RETURN(pRepo);

	// Only open/create the db once.
	if (pRepo->p_vtable_instance_data)
		SG_ERR_THROW2_RETURN(SG_ERR_INVALIDARG, (pCtx, "instance data should be empty"));

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pData)  );
    pData->pRepo = pRepo;

	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pRepo->pvh_descriptor, SG_RIDESC_FSLOCAL__PATH_PARENT_DIR, &pszParentDir)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pData->pPathParentDir, pszParentDir)  );

	// make sure that <repo_parent_dir> directory exists on disk.

	SG_ERR_CHECK(  SG_fsobj__verify_directory_exists_on_disk__pathname(pCtx, pData->pPathParentDir)  );

	// for the FS implementation, we have a <repo_parent_dir>/<subdir>
	// directory that actually contains the contents of the REPO.
	// construct the pathname to the subdir and verify that it exists on disk.

	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pRepo->pvh_descriptor, SG_RIDESC_FSLOCAL__DIR_NAME, &psz_subdir_name)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pData->pPathMyDir, pData->pPathParentDir, psz_subdir_name)  );
	SG_ERR_CHECK(  SG_fsobj__verify_directory_exists_on_disk__pathname(pCtx, pData->pPathMyDir)  );

	pRepo->p_vtable_instance_data = (sg_repo__vtable__instance_data *)pData;

	SG_ERR_CHECK_RETURN(  _my_make_db_pathname(pCtx, pData,&pPathnameSqlDb)  );

    SG_ERR_CHECK(  SG_dag_sqlite3__open(pCtx, pPathnameSqlDb, &psql)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathnameSqlDb);

    // TODO fetch b_indexes

	// Get repo and admin ids
	SG_ERR_CHECK(  sg_sqlite__exec__va__string(pCtx, psql, &pstr_prop, "SELECT value FROM props WHERE name='hashmethod'")  );
	SG_ERR_CHECK( SG_strcpy(pCtx, pData->buf_hash_method, sizeof(pData->buf_hash_method), SG_string__sz(pstr_prop))  );
	SG_STRING_NULLFREE(pCtx, pstr_prop);

	SG_ERR_CHECK(  sg_sqlite__exec__va__string(pCtx, psql, &pstr_prop, "SELECT value FROM props WHERE name='repoid'")  );
	SG_ERR_CHECK( SG_strcpy(pCtx, pData->buf_repo_id, sizeof(pData->buf_repo_id), SG_string__sz(pstr_prop))  );
	SG_STRING_NULLFREE(pCtx, pstr_prop);

	SG_ERR_CHECK(  sg_sqlite__exec__va__string(pCtx, psql, &pstr_prop, "SELECT value FROM props WHERE name='adminid'")  );
	SG_ERR_CHECK( SG_strcpy(pCtx, pData->buf_admin_id, sizeof(pData->buf_admin_id), SG_string__sz(pstr_prop))  );
	SG_STRING_NULLFREE(pCtx, pstr_prop);

	// Verify the TRIVIAL-HASH.  This is one last sanity check before we return
	// the pRepo and let them start playing with it.
	//
	// When the first instance of this REPO was originally created on disk
	// (see __create_repo_instance()), a HASH-METHOD was selected.  This CANNOT
	// EVER be changed; not for that instance on disk NOR for any clone instance;
	// ALL HASHES within a REPO must be computed using the SAME hash-method.
	//
	// At this point, we must verify that the currently running implementation
	// also supports the hash-method that was registered when the REPO was
	// created on disk.
	//
	// First, we use the registered hash-method and compute a trivial hash; this
	// ensures that the hash-method is installed and available.   (I call it "trivial"
	// because we are computing the hash of a zero-length buffer.)  This throws
	// SG_ERR_UNKNOWN_HASH_METHOD.
	//
	// Second, verify that the answer matches; this is really obscure but might
	// catch a configuration error or something.

	SG_ERR_CHECK(  sg_repo_utils__one_step_hash__from_sghash(pCtx, pData->buf_hash_method, 0, NULL, &pszTrivialHash_Computed)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va__string(pCtx, psql, &pstr_prop, "SELECT value FROM props WHERE name='trivialhash'")  );
	bEqual_TrivialHash = (strcmp(pszTrivialHash_Computed,SG_string__sz(pstr_prop)) == 0);
	if (!bEqual_TrivialHash)
		SG_ERR_THROW(  SG_ERR_TRIVIAL_HASH_DIFFERENT  );
	pData->strlen_hashes = SG_string__length_in_bytes(pstr_prop);
	SG_STRING_NULLFREE(pCtx, pstr_prop);
	SG_NULLFREE(pCtx, pszTrivialHash_Computed);

	// all is well.

    pData->psql = psql;
	return;

fail:
    SG_ERR_IGNORE(  SG_dag_sqlite3__nullclose(pCtx, &psql)  );
	SG_STRING_NULLFREE(pCtx, pstr_prop);
	SG_NULLFREE(pCtx, pszTrivialHash_Computed);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameSqlDb);
    if(pData!=NULL)
    {
        SG_PATHNAME_NULLFREE(pCtx, pData->pPathParentDir);
        SG_PATHNAME_NULLFREE(pCtx, pData->pPathMyDir);
    	SG_NULLFREE(pCtx, pData);
    }
	pRepo->p_vtable_instance_data = NULL;
}

//////////////////////////////////////////////////////////////////

// For FS2-based REPOs stored in the CLOSET, we create a unique
// directory and store everything within it.

// TODO review these names and see if we want to define/register
// TODO more meaningful suffixes.  For example, on the Mac we can
// TODO tell Finder to hide the contents of a directory (like it
// TODO does for .app directories).

#define sg_MY_CLOSET_DIR_PREFIX				"fs2_"
#define sg_MY_CLOSET_DIR_LENGTH_RANDOM		32											// how many hex digits we want in the name
#define sg_MY_CLOSET_DIR_SUFFIX				".fs2"
#define sg_MY_CLOSET_DIR_BUFFER_LENGTH		(4 + sg_MY_CLOSET_DIR_LENGTH_RANDOM + 4 + 1)	// "fs2_" + <hex_digits> + ".fs2" + NULL

/**
 * Create a new, unique directory name for this instance of a REPO.
 * This is a local-only name for the DB.  The repo-instance maps to this name.
 *
 * TODO This method was only chosen because we need some unique name (that
 * TODO fits within the platform pathname limits and) that is usable.  We
 * TODO don't really care what it is.  Feel free to offer suggestions/improvements.
 *
 * TODO Revisit this when we think about live-in repos.
 */
static void _my_create_name_dir(SG_context * pCtx, char * pBuf, SG_uint32 lenBuf)
{
	char bufTid[SG_TID_MAX_BUFFER_LENGTH];

	SG_ASSERT(  (lenBuf >= sg_MY_CLOSET_DIR_BUFFER_LENGTH)  );

	SG_ERR_CHECK_RETURN(  SG_tid__generate2(pCtx, bufTid, sizeof(bufTid), sg_MY_CLOSET_DIR_LENGTH_RANDOM)  );

	SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx, pBuf, lenBuf, sg_MY_CLOSET_DIR_PREFIX)  );
	SG_ERR_CHECK_RETURN(  SG_strcat(pCtx, pBuf, lenBuf, &bufTid[SG_TID_LENGTH_PREFIX])  );	// skip leading 't' in TID
	SG_ERR_CHECK_RETURN(  SG_strcat(pCtx, pBuf, lenBuf, sg_MY_CLOSET_DIR_SUFFIX)  );
}

void sg_repo__fs2__create_repo_instance(
        SG_context * pCtx,
        SG_repo * pRepo,
        SG_bool b_indexes,
        const char* psz_hash_method,
        const char* psz_repo_id,
        const char* psz_admin_id
        )
{
	my_instance_data * pData = NULL;
	SG_pathname * pPathnameSqlDb = NULL;
    const char* pszParentDir = NULL;
	char * pszTrivialHash = NULL;
	char buf_subdir_name[sg_MY_CLOSET_DIR_BUFFER_LENGTH];

	SG_UNUSED(b_indexes);

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(psz_repo_id);
	SG_NULLARGCHECK_RETURN(psz_admin_id);

	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, psz_repo_id)  );
	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, psz_admin_id)  );

	SG_ARGCHECK( pRepo->p_vtable_instance_data == NULL , pRepo ); // only open/create db once

	// Validate the HASH-METHOD.  that is, does this repo implementation support that hash-method.
	// We use the given hash-method to compute a TRIVIAL-HASH.  This does 2 things.  First, it
	// verifies that the hash-method is not bogus.  Second, it gives us a "known answer" that we
	// can put in the "props" (aka "id") table for later consistency checks when pushing/pulling.
	// Note that we cannot just use the REPO API hash routines because we haven't set pRepo->pData
	// and we haven't set pRepo->pData->buf_hash_method, so we do it directly.

	SG_ERR_CHECK(  sg_repo_utils__one_step_hash__from_sghash(pCtx, psz_hash_method, 0, NULL, &pszTrivialHash)  );

	// allocate and install pData
	SG_ERR_CHECK(  SG_alloc1(pCtx, pData)  );
    pData->pRepo = pRepo;

    /* We were given a parent dir.  Make sure it exists. */
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pRepo->pvh_descriptor, SG_RIDESC_FSLOCAL__PATH_PARENT_DIR, &pszParentDir)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pData->pPathParentDir, pszParentDir)  );
	SG_ERR_CHECK(  SG_fsobj__verify_directory_exists_on_disk__pathname(pCtx, pData->pPathParentDir)  );

	/* Create the subdir name and make sure it exists on disk. */
	SG_ERR_CHECK(  _my_create_name_dir(pCtx, buf_subdir_name, sizeof(buf_subdir_name))  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pData->pPathMyDir, pData->pPathParentDir, buf_subdir_name)  );
	SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pData->pPathMyDir)  );

    /* store the subdir name in the descriptor */
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pRepo->pvh_descriptor, SG_RIDESC_FSLOCAL__DIR_NAME, buf_subdir_name)  );

    /* Tell the dag code to create its sql db */
	SG_ERR_CHECK(  _my_make_db_pathname(pCtx, pData,&pPathnameSqlDb)  );
    SG_ERR_CHECK(  SG_dag_sqlite3__create(pCtx, pPathnameSqlDb, &pData->psql)  );

    // Now we add our stuff into that same db
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pData->psql, "BEGIN EXCLUSIVE TRANSACTION")  );
    pData->b_in_sqlite_transaction = SG_TRUE;

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pData->psql, ("CREATE TABLE directory"
										  "   ("
										  "     filenumber INTEGER NOT NULL,"
										  "     offset INTEGER NOT NULL,"
										  "     len_encoded INTEGER NOT NULL,"
										  "     encoding INTEGER NOT NULL,"
										  "     len_full INTEGER NOT NULL,"
										  "     hid_vcdiff VARCHAR NULL,"
										  "     hid VARCHAR UNIQUE NOT NULL"
										  "   )"))  );

	// We could put a UNIQUE constraint on (filenumber, offset, len_encoded)
    // but it slows things down a lot.
    //
    // We don't need to explicitly ask for an INDEX on hid because the UNIQUE
    // constraint implies one.
    //
    // Any other indexes on this table are not necessary and would slow
    // things down.

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pData->psql, ("CREATE TABLE props"
		"   ("
		"     name VARCHAR PRIMARY KEY,"
		"     value VARCHAR NOT NULL"
		"   )"))  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pData->psql, ("CREATE INDEX props_name on props ( name )"))  );

	/* Store repoid and adminid in the DB and in memory. */
	SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pData->psql, "INSERT INTO props (name, value) VALUES ('%s', '%s')", "hashmethod", psz_hash_method)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pData->psql, "INSERT INTO props (name, value) VALUES ('%s', '%s')", "repoid", psz_repo_id)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pData->psql, "INSERT INTO props (name, value) VALUES ('%s', '%s')", "adminid", psz_admin_id)  );
	SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pData->psql, "INSERT INTO props (name, value) VALUES ('%s', '%s')", "trivialhash", pszTrivialHash)  );

	SG_ERR_CHECK(  SG_strcpy(pCtx, pData->buf_hash_method, sizeof(pData->buf_hash_method), psz_hash_method)  );
	SG_ERR_CHECK(  SG_strcpy(pCtx, pData->buf_repo_id, sizeof(pData->buf_repo_id), psz_repo_id)  );
	SG_ERR_CHECK(  SG_strcpy(pCtx, pData->buf_admin_id, sizeof(pData->buf_admin_id), psz_admin_id)  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pData->psql, "COMMIT TRANSACTION")  );
    pData->b_in_sqlite_transaction = SG_FALSE;

	pData->strlen_hashes = strlen(pszTrivialHash);

	SG_PATHNAME_NULLFREE(pCtx, pPathnameSqlDb);
	SG_NULLFREE(pCtx, pszTrivialHash);

	pRepo->p_vtable_instance_data = (sg_repo__vtable__instance_data *)pData;

	return;

fail:
    /* TODO close psql */
    if (pData->b_in_sqlite_transaction)
    {
        SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );
        pData->b_in_sqlite_transaction = SG_FALSE;
    }
	SG_NULLFREE(pCtx, pData);
	SG_NULLFREE(pCtx, pszTrivialHash);
}

void sg_repo__fs2__delete_repo_instance(
    SG_context * pCtx,
    SG_repo * pRepo
    )
{
	my_instance_data * pData = NULL;
    SG_pathname * pPath = NULL;

    SG_ASSERT(pCtx!=NULL);
	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPath, pData->pPathMyDir)  );

    SG_ERR_CHECK(  sg_repo__fs2__close_repo_instance(pCtx, pRepo)  );

    SG_ERR_CHECK(  SG_fsobj__rmdir_recursive__pathname(pCtx, pPath)  );

    SG_PATHNAME_NULLFREE(pCtx, pPath);

    return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath);
}

void sg_repo__fs2__close_repo_instance(SG_context * pCtx, SG_repo * pRepo)
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;
	pRepo->p_vtable_instance_data = NULL;

	if (!pData)					// no instance data implies that the sql db is not open
		return;					// just ignore close.

	// TODO should we assert that we are not in a transaction?  YES, or error code.

    SG_ERR_IGNORE(  SG_dag_sqlite3__nullclose(pCtx, &pData->psql)  );

	SG_PATHNAME_NULLFREE(pCtx, pData->pPathParentDir);
	SG_PATHNAME_NULLFREE(pCtx, pData->pPathMyDir);

    SG_ERR_IGNORE(  sg_blob_fs2_handle_store__free(pCtx, pData->pBlobStoreHandle)  );
	SG_NULLFREE(pCtx, pData);

	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
}

void sg_repo__fs2__fetch_dagnode(SG_context * pCtx, SG_repo * pRepo, const char* pszidHidChangeset, SG_dagnode ** ppNewDagnode)	// must match FN_
{
	my_instance_data * pData = NULL;

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_RETRY_THINGIE(
            SG_dag_sqlite3__fetch_dagnode(pCtx,pData->psql, pszidHidChangeset, ppNewDagnode)
            );

fail:
    return;
}

void sg_repo__fs2__list_dags(SG_context * pCtx, SG_repo* pRepo, SG_uint32* piCount, SG_uint32** paDagNums)
{
	my_instance_data * pData = NULL;
	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_RETRY_THINGIE(
            SG_dag_sqlite3__list_dags(pCtx, pData->psql, piCount, paDagNums)
            );

fail:
    return;
}

void sg_repo__fs2__fetch_dag_leaves(SG_context * pCtx, SG_repo * pRepo, SG_uint32 iDagNum, SG_rbtree ** ppIdsetReturned)	// must match FN_
{
	my_instance_data * pData = NULL;
	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_RETRY_THINGIE(
            SG_dag_sqlite3__fetch_leaves(pCtx, pData->psql, iDagNum, ppIdsetReturned)
            );

fail:
    return;
}

void sg_repo__fs2__fetch_dagnode_children(SG_context * pCtx, SG_repo * pRepo, SG_uint32 iDagNum, const char* pszDagnodeID, SG_rbtree ** ppIdsetReturned)	// must match FN_
{
	my_instance_data * pData = NULL;
	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_RETRY_THINGIE(
            SG_dag_sqlite3__fetch_dagnode_children(pCtx, pData->psql, iDagNum, pszDagnodeID, ppIdsetReturned)
            );

fail:
    return;
}

void sg_repo__fs2__get_dag_lca(
    SG_context * pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
    const SG_rbtree* prbNodes,
    SG_daglca ** ppDagLca
    )
{
	my_instance_data * pData = NULL;
	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_RETRY_THINGIE(
        SG_dag_sqlite3__get_lca(pCtx, pData->psql, iDagNum, prbNodes, ppDagLca)
        );

fail:
    return;
}

void sg_repo__fs2__check_dag_consistency(SG_context * pCtx, SG_repo * pRepo, SG_uint32 iDagNum)	// must match FN_
{
	// I consider this to be a DEBUG routine.  But I'm putting it
	// in the VTABLE, so I'm not #ifdef'ing it because I want
	// the size of the VTABLE to be constant.
	//
	// Run a series of consistency checks on the DAG DB.
	// We do this in our own transaction so that all queries are
	// consistent.
	//
	// we do a rollback when we're done because we don't want to
	// actually make any changes.

	my_instance_data * pData = NULL;
	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_RETRY_THINGIE(
        SG_dag_sqlite3__check_consistency(pCtx, pData->psql, iDagNum)
        );

fail:
    return;
}

void _fs2_my_get_dbndx(SG_context * pCtx, my_instance_data* pData, SG_uint32 iDagNum, SG_bool bQueryOnly, SG_dbndx** ppResult)
{
    SG_pathname* pPath = NULL;
    SG_dbndx* pndx = NULL;

    SG_ERR_CHECK(  sg_fs2__get_dbndx_path(pCtx, pData, iDagNum, &pPath)  );
    SG_ERR_CHECK(  SG_dbndx__open(pCtx, pData->pRepo, iDagNum, pPath, bQueryOnly, &pndx)  );
    *ppResult = pndx;

    return;

fail:
    ;
    /* don't free pPath here because the dbndx should own it */
}

void _fs2_my_get_treendx(SG_context * pCtx, my_instance_data* pData, SG_uint32 iDagNum, SG_bool bQueryOnly, SG_treendx** ppResult)
{
    SG_pathname* pPath = NULL;
    SG_treendx* pndx = NULL;

    SG_ERR_CHECK(  sg_fs2__get_treendx_path(pCtx, pData, iDagNum, &pPath)  );
    SG_ERR_CHECK(  SG_treendx__open(pCtx, pData->pRepo, iDagNum, pPath, bQueryOnly, &pndx)  );
    *ppResult = pndx;

    return;

fail:
    ;
    /* don't free pPath here because the treendx should own it */
}

void sg_repo__fs2__find_dagnodes_by_prefix(SG_context * pCtx, SG_repo * pRepo, SG_uint32 iDagNum, const char* psz_hid_prefix, SG_rbtree** pprb)
{
	my_instance_data * pData = NULL;
	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_RETRY_THINGIE(
        SG_dag_sqlite3__find_by_prefix(pCtx, pData->psql, iDagNum, psz_hid_prefix, pprb)
        );

fail:
    return;
}

void sg_repo__fs2__list_blobs(
    SG_context * pCtx,
    SG_repo * pRepo,
    SG_blob_encoding blob_encoding, /**< only return blobs of this encoding */
    SG_bool b_sort_descending,      /**< otherwise sort ascending */
    SG_bool b_sort_by_len_encoded,  /**< otherwise sort by len_full */
    SG_uint32 limit,                /**< only return this many entries */
    SG_uint32 offset,               /**< skip the first group of the sorted entries */
    SG_vhash** ppvh                 /**< Caller must free */
    )
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(ppvh);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_RETRY_THINGIE(
        sg_blob_fs2__list_blobs(pCtx, pData->psql, blob_encoding, b_sort_descending, b_sort_by_len_encoded, limit, offset, ppvh)
        );

fail:
    return;
}

void sg_repo__fs2__find_blobs_by_prefix(SG_context * pCtx, SG_repo * pRepo, const char* psz_hid_prefix, SG_rbtree** pprb)
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(psz_hid_prefix);
	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pprb);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_RETRY_THINGIE(
        sg_blob_fs2__sql__find_by_hid_prefix(pCtx, pData->psql, psz_hid_prefix, pprb)
        );

fail:
    return;
}

void sg_repo__fs2__get_repo_id(SG_context * pCtx, SG_repo * pRepo, char** ppsz_id)
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;
#if defined(DEBUG)
	// buf_repo_id is a GID.  it should have been checked when it was stored.
	// checking now for sanity during GID/HID conversion.
	SG_ERR_CHECK(  SG_gid__argcheck(pCtx, pData->buf_repo_id)  );
#endif
    SG_ERR_CHECK(  SG_STRDUP(pCtx, pData->buf_repo_id, ppsz_id)  );

    return;
fail:
    return;
}

void sg_repo__fs2__get_admin_id(SG_context * pCtx, SG_repo * pRepo, char** ppsz_id)
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;
#if defined(DEBUG)
	// buf_admin_id is a GID.  it should have been checked when it was stored.
	// checking now for sanity during GID/HID conversion.
	SG_ERR_CHECK(  SG_gid__argcheck(pCtx, pData->buf_admin_id)  );
#endif
    SG_ERR_CHECK(  SG_STRDUP(pCtx, pData->buf_admin_id, ppsz_id)  );

    return;
fail:
    return;
}

void sg_repo__fs2__store_blob__begin(
    SG_context * pCtx,
    SG_repo * pRepo,
	SG_repo_tx_handle* pTx,
    const char* psz_objectid,
    SG_blob_encoding blob_encoding,
    const char* psz_hid_vcdiff_reference,
    SG_uint64 lenFull,
	SG_uint64 lenEncoded,
    const char* psz_hid_known,
    SG_repo_store_blob_handle** ppHandle
    )
{
    sg_blob_fs2_handle_store* pbh = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pTx);

    SG_UNUSED(psz_objectid);

    SG_ERR_CHECK(  sg_blob_fs2__store_blob__begin(pCtx, (my_tx_data*)pTx, blob_encoding, psz_hid_vcdiff_reference, lenFull, lenEncoded, psz_hid_known, &pbh)  );

    *ppHandle = (SG_repo_store_blob_handle*) pbh;

    return;
fail:
    return;
}

void sg_repo__fs2__store_blob__chunk(
    SG_context * pCtx,
    SG_repo * pRepo,
    SG_repo_store_blob_handle* pHandle,
    SG_uint32 len_chunk,
    const SG_byte* p_chunk,
    SG_uint32* piWritten
    )
{
	SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK(  sg_blob_fs2__store_blob__chunk(pCtx, (sg_blob_fs2_handle_store*) pHandle, len_chunk, p_chunk, piWritten)  );

    return;
fail:
    return;
}

void sg_repo__fs2__store_blob__end(
    SG_context * pCtx,
    SG_repo * pRepo,
	SG_repo_tx_handle* pTx,
    SG_repo_store_blob_handle** ppHandle,
    char** ppsz_hid_returned
    )
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pTx);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;
	pData->pBlobStoreHandle = NULL;

    SG_ERR_CHECK(  sg_blob_fs2__store_blob__end(pCtx, (sg_blob_fs2_handle_store**) ppHandle, ppsz_hid_returned)  );

    return;
fail:
    return;
}

void sg_repo__fs2__store_blob__abort(
    SG_context * pCtx,
    SG_repo * pRepo,
	SG_repo_tx_handle* pTx,
    SG_repo_store_blob_handle** ppHandle
    )
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pTx);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;
	pData->pBlobStoreHandle = NULL;

    SG_ERR_CHECK(  sg_blob_fs2__store_blob__abort(pCtx, (sg_blob_fs2_handle_store**) ppHandle)  );

    return;
fail:
    return;
}

void sg_repo__fs2__fetch_blob__begin(
    SG_context * pCtx,
    SG_repo * pRepo,
    const char* psz_hid_blob,
    SG_bool b_convert_to_full,
    char** ppsz_objectid,
    SG_blob_encoding* pBlobFormat,
    char** ppsz_hid_vcdiff_reference,
    SG_uint64* pLenRawData,
    SG_uint64* pLenFull,
    SG_repo_fetch_blob_handle** ppHandle
    )
{
	my_instance_data * pData = NULL;
    sg_blob_fs2_handle_fetch* pbh = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__begin(
                pCtx,
                pData,
                psz_hid_blob,
                SG_FS2__LOCK__READ,
                b_convert_to_full,
                ppsz_objectid,
                pBlobFormat,
                ppsz_hid_vcdiff_reference,
                pLenRawData,
                pLenFull,
                &pbh
                )  );

    *ppHandle = (SG_repo_fetch_blob_handle*) pbh;

    return;
fail:
    return;
}

void sg_repo__fs2__fetch_blob__chunk(
    SG_context * pCtx,
    SG_repo * pRepo,
    SG_repo_fetch_blob_handle* pHandle,
    SG_uint32 len_buf,
    SG_byte* p_buf,
    SG_uint32* p_len_got,
    SG_bool* pb_done
    )
{
	SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__chunk(pCtx, (sg_blob_fs2_handle_fetch*) pHandle, len_buf, p_buf, p_len_got, pb_done)  );

    return;
fail:
    return;
}

void sg_repo__fs2__fetch_blob__end(
    SG_context * pCtx,
    SG_repo * pRepo,
    SG_repo_fetch_blob_handle** ppHandle
    )
{
	SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__end(pCtx, (sg_blob_fs2_handle_fetch**) ppHandle)  );

    return;
fail:
    return;
}

void sg_repo__fs2__fetch_blob__abort(
    SG_context * pCtx,
    SG_repo * pRepo,
    SG_repo_fetch_blob_handle** ppHandle
    )
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__abort(pCtx, (sg_blob_fs2_handle_fetch**) ppHandle)  );

    // TODO I don't think the following is needed
    if (pData->b_in_sqlite_transaction)
    {
        SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pData->psql, ("ROLLBACK TRANSACTION"))  );
        pData->b_in_sqlite_transaction = SG_FALSE;
    }

    return;
fail:
    return;
}

static void _build_repo_fragball(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_pathname* pFragballPath)
{
	my_instance_data* pData = NULL;
	SG_uint32* paDagNums = NULL;
	SG_dagnode* pDagnode = NULL;
	sqlite3_stmt* pStmt = NULL;
	int rc;
	SG_dagfrag* pFrag = NULL;
	const char* pszHid;
	sg_blob_fs2_handle_fetch* pbh = NULL;
	char* pszHidVcdiffRef = NULL;
	char* psz_objectid = NULL;
	SG_fragball_writer* pFragballWriter = NULL;

	// Caller should ensure we're inside a sqlite tx.

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	// Add all dagnodes to fragball
	{
		SG_uint32 count_dagnums;
		SG_uint32 i;

		SG_ERR_CHECK(  SG_dag_sqlite3__list_dags(pCtx, pData->psql, &count_dagnums, &paDagNums)  );
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pData->psql, &pStmt,
			"SELECT child_id from dag_info WHERE dagnum = ?")  );

		for (i = 0; i < count_dagnums; i++)
		{
			SG_ERR_CHECK(  SG_dagfrag__alloc(pCtx, &pFrag, pData->buf_repo_id, pData->buf_admin_id, paDagNums[i])  );
			SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 1, paDagNums[i])  );

			while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
			{
				pszHid = (const char*)sqlite3_column_text(pStmt, 0);
				SG_ERR_CHECK(  SG_dag_sqlite3__fetch_dagnode(pCtx, pData->psql, pszHid, &pDagnode)  );
				SG_ERR_CHECK(  SG_dagfrag__add_dagnode(pCtx, pFrag, &pDagnode)  );
			}
			SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
			SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );

			SG_ERR_CHECK(  SG_fragball__append__frag(pCtx, pFragballPath, pFrag)  );
			SG_DAGFRAG_NULLFREE(pCtx, pFrag);
		}

		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
		SG_NULLFREE(pCtx, paDagNums);
	}

	// Add all blobs to fragball
	{
		SG_uint64 lenEncoded;
		SG_uint64 lenFull;
		SG_blob_encoding encoding;

		SG_ERR_CHECK(  SG_fragball_writer__alloc(pCtx, pRepo, pFragballPath, SG_FALSE, &pFragballWriter)  );

		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pData->psql, &pStmt,
			"SELECT hid from directory ORDER BY filenumber, offset")  );

		while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
		{
			pszHid = (const char*)sqlite3_column_text(pStmt, 0);
			SG_ERR_CHECK(  sg_blob_fs2__fetch_blob__begin(pCtx, pData, pszHid, SG_FS2__LOCK__READ, SG_FALSE, &psz_objectid, &encoding, &pszHidVcdiffRef, &lenEncoded, &lenFull, &pbh)  );
			SG_ERR_CHECK(  SG_fragball__append_blob__from_handle(pCtx, pFragballWriter,
				(SG_repo_fetch_blob_handle**)&pbh, psz_objectid, pszHid, encoding, pszHidVcdiffRef, lenEncoded, lenFull)  );
			SG_NULLFREE(pCtx, pszHidVcdiffRef);
			SG_NULLFREE(pCtx, psz_objectid);
		}

		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
		SG_ERR_CHECK(  SG_fragball_writer__free(pCtx, pFragballWriter)  );
	}

	return;

fail:
	SG_ERR_IGNORE(  SG_fragball_writer__free(pCtx, pFragballWriter)  );
	SG_NULLFREE(pCtx, paDagNums);
	SG_DAGFRAG_NULLFREE(pCtx, pFrag);
	SG_DAGNODE_NULLFREE(pCtx, pDagnode);
	SG_NULLFREE(pCtx, pszHidVcdiffRef);
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	if (pbh)
		SG_ERR_IGNORE(  sg_blob_fs2__fetch_blob__abort(pCtx, &pbh)  );
}

void sg_repo__fs2__fetch_repo__fragball(
	SG_context* pCtx,
	SG_repo* pRepo,
	const SG_pathname* pFragballDirPathname,
	char** ppszFragballName)
{
	my_instance_data* pData = NULL;
	SG_pathname* pFragballPathname = NULL;
	SG_uint32* paDagNums = NULL;
	SG_rbtree* prbDagnodes = NULL;
	SG_string* pstrFragballName = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pFragballDirPathname);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK(  SG_fragball__create(pCtx, pFragballDirPathname, &pFragballPathname)  );

	SG_ERR_CHECK(  SG_log_debug(pCtx, "Started building fragball for clone.")  );
	SG_RETRY_THINGIE(  _build_repo_fragball(pCtx, pRepo, pFragballPathname)  );
	SG_ERR_CHECK(  SG_log_debug(pCtx, "Done building fragball for clone.")  );

	SG_ERR_CHECK(  SG_pathname__get_last(pCtx, pFragballPathname, &pstrFragballName)  );
	SG_ERR_CHECK(  SG_STRDUP(pCtx, SG_string__sz(pstrFragballName), ppszFragballName)  );

	/* fallthru */

fail:
	// If we had an error, delete the half-baked fragball.
	if (pFragballPathname && SG_context__has_err(pCtx))
		SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pFragballPathname)  );

	SG_PATHNAME_NULLFREE(pCtx, pFragballPathname);
	SG_NULLFREE(pCtx, paDagNums);
	SG_RBTREE_NULLFREE(pCtx, prbDagnodes);
	SG_STRING_NULLFREE(pCtx, pstrFragballName);
}

void sg_repo__fs2__get_blob_stats(
    SG_context * pCtx,
    SG_repo * pRepo,
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
    )
{
	my_instance_data * pData = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_RETRY_THINGIE(
        sg_blob_fs2__get_blob_stats(pCtx, pData->psql,
            p_count_blobs_full,
            p_count_blobs_alwaysfull,
            p_count_blobs_zlib,
            p_count_blobs_vcdiff,
            p_total_blob_size_full,
            p_total_blob_size_alwaysfull,
            p_total_blob_size_zlib_full,
            p_total_blob_size_zlib_encoded,
            p_total_blob_size_vcdiff_full,
            p_total_blob_size_vcdiff_encoded
        )
        );

fail:
    return;
}

void sg_repo__fs2__vacuum(
    SG_context * pCtx,
    SG_repo * pRepo
    )
{
	my_instance_data * pData = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_ERR_CHECK_RETURN(  sg_fs2__vacuum(pCtx, pData)  );
}

void sg_repo__fs2__change_blob_encoding(
    SG_context * pCtx,
    SG_repo * pRepo,
	SG_repo_tx_handle* ptx,
    const char* psz_hid_blob,
    SG_blob_encoding blob_encoding_desired,
    const char* psz_hid_vcdiff_reference_desired,
    SG_blob_encoding* p_blob_encoding_new,
    char** ppsz_hid_vcdiff_reference,
    SG_uint64* p_len_encoded,
    SG_uint64* p_len_full
    )
{
    SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK_RETURN(  sg_blob_fs2__change_blob_encoding(pCtx, (my_tx_data*) ptx, psz_hid_blob, blob_encoding_desired, psz_hid_vcdiff_reference_desired, p_blob_encoding_new, ppsz_hid_vcdiff_reference, p_len_encoded, p_len_full)  );
}

void sg_fs2__remove_lock_files(
	SG_context * pCtx,
	my_tx_data* ptx
    );

static void sg_fs2__nullfree_tx_data(SG_context* pCtx, my_tx_data** pptx)
{
    if (!pptx || !*pptx)
    {
        return;
    }

	SG_ERR_IGNORE(  sg_fs2__remove_lock_files(pCtx, *pptx)  );
    SG_RBTREE_NULLFREE(pCtx, (*pptx)->prb_append_locks);
    SG_RBTREE_NULLFREE(pCtx, (*pptx)->prb_drill_locks);
	SG_VECTOR_NULLFREE_WITH_ASSOC(pCtx, (*pptx)->pvec_blobs, sg_fs2__free_pbi);
    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, (*pptx)->prb_frags, (SG_free_callback *)SG_dagfrag__free);

    SG_NULLFREE(pCtx, *pptx);
    *pptx = NULL;
}

void sg_fs2__begin_tx(
    SG_context * pCtx,
    my_instance_data* pData,
    my_tx_data** pptx
    )
{
    my_tx_data* ptx = NULL;

    SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(my_tx_data), &ptx)  );
    ptx->pData = pData;

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &ptx->prb_append_locks)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &ptx->prb_drill_locks)  );
    SG_ERR_CHECK(  SG_vector__alloc(pCtx, &ptx->pvec_blobs, 10)  );

    *pptx = ptx;

    return;
fail:
    SG_ERR_IGNORE(  sg_fs2__nullfree_tx_data(pCtx, &ptx)  );
}

void sg_repo__fs2__begin_tx(
	SG_context * pCtx,
	SG_repo* pRepo,
	SG_repo_tx_handle** ppTx
	)
{
	my_instance_data * pData = NULL;
    my_tx_data* ptx = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(ppTx);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_ERR_CHECK(  sg_fs2__begin_tx(pCtx, pData, &ptx)  );

    *ppTx = (SG_repo_tx_handle*) ptx;

    return;
fail:
    SG_ERR_IGNORE(  sg_fs2__nullfree_tx_data(pCtx, &ptx)  );
}

// this is a helper function called only from sg_fs2__commit_tx
static void sg_fs2__insert__prepare(SG_context * pCtx, sqlite3* psql, sqlite3_stmt** pp)
{
    sqlite3_stmt* pStmt = NULL;

    SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, psql,&pStmt,
                                      "INSERT INTO directory (hid, filenumber, offset, encoding, len_encoded, len_full, hid_vcdiff) VALUES (?, ?, ?, ?, ?, ?, ?)")  );

    *pp = pStmt;

    return;
fail:
    return;
}

void sg_fs2__remove_lock_files(
	SG_context * pCtx,
	my_tx_data* ptx
    )
{
    SG_bool b = SG_FALSE;
    SG_rbtree_iterator* pit = NULL;
    const char* psz_filenumber = NULL;
    SG_pathname* pPath_lock = NULL;

    if (ptx->prb_append_locks)
    {
        /* remove the lock files here.  */
        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, ptx->prb_append_locks, &b, &psz_filenumber, (void**) &pPath_lock)  );
        while (b)
        {
            SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_lock)  );
            SG_PATHNAME_NULLFREE(pCtx, pPath_lock);

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_filenumber, (void**) &pPath_lock)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
        SG_RBTREE_NULLFREE(pCtx, ptx->prb_append_locks);
    }

    if (ptx->prb_drill_locks)
    {
        /* remove the lock files here.  */
        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, ptx->prb_drill_locks, &b, &psz_filenumber, (void**) &pPath_lock)  );
        while (b)
        {
            SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_lock)  );
            SG_PATHNAME_NULLFREE(pCtx, pPath_lock);

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_filenumber, (void**) &pPath_lock)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
        SG_RBTREE_NULLFREE(pCtx, ptx->prb_drill_locks);
    }

    return;

fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_PATHNAME_NULLFREE(pCtx, pPath_lock);

    return;
}

static void sg_fs2__get_dbndx__cb(
        SG_context* pCtx,
        void * pVoidData,
        SG_uint32 iDagNum,
        SG_dbndx** ppndx
        )
{
	my_instance_data* pData = (my_instance_data*) pVoidData;

    SG_ERR_CHECK_RETURN(  _fs2_my_get_dbndx(pCtx, pData, iDagNum, SG_FALSE, ppndx)  );
}

static void sg_fs2__get_treendx__cb(
        SG_context* pCtx,
        void * pVoidData,
        SG_uint32 iDagNum,
        SG_treendx** ppndx
        )
{
	my_instance_data* pData = (my_instance_data*) pVoidData;

    SG_ERR_CHECK_RETURN(  _fs2_my_get_treendx(pCtx, pData, iDagNum, SG_FALSE, ppndx)  );
}

void sg_fs2__commit_tx(
	SG_context * pCtx,
	my_instance_data* pData,
	my_tx_data** pptx
	)
{
    SG_uint32 i;
    SG_uint32 count;
    my_tx_data* ptx = NULL;
    struct pending_blob_info* pbi = NULL;
    SG_bool b = SG_FALSE;
    SG_rbtree_iterator* pit = NULL;
    SG_rbtree* prb_new_dagnodes = NULL;
    SG_rbtree* prb_missing = NULL;
    SG_stringarray* psa_new = NULL;
    SG_dagnode* pdn = NULL;
	sqlite3_stmt * pStmt = NULL;

    ptx = (my_tx_data*) *pptx;

	SG_ERR_CHECK(  sg_sqlite__exec__retry(pCtx, pData->psql, "BEGIN EXCLUSIVE TRANSACTION", MY_SLEEP_MS, MY_TIMEOUT_MS)  );
    ptx->pData->b_in_sqlite_transaction = SG_TRUE;

    SG_ERR_CHECK(  SG_vector__length(pCtx, ptx->pvec_blobs, &count)  );

    /* First create holes for anything that is being updated */
	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pData->psql,&pStmt,
									  "DELETE FROM directory WHERE hid=? AND filenumber=?")  );
    for (i=0; i<count; i++)
    {
        SG_ERR_CHECK(  SG_vector__get(pCtx, ptx->pvec_blobs, i, (void**) &pbi)  );

        if (pbi->old_filenumber)
        {
            char buf[sg_FILENUMBER_BUFFER_LENGTH];
            SG_uint32 num_changes;

            SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
            SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );

            SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt,1,pbi->sz_hid_blob_final)  );
            SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt,2,pbi->old_filenumber)  );
            SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt,SQLITE_DONE)  );
            SG_ERR_CHECK(  sg_sqlite__num_changes(pCtx, pData->psql, &num_changes)  );
            if (1 == num_changes)
            {
                // pbi->old_filenumber should be added to the holes
                SG_ERR_CHECK(  sg_fs2__filenumber_to_filename(pCtx, buf, sizeof(buf), pbi->old_filenumber)  );
                SG_ERR_CHECK(  sg_fs2__lock(pCtx, ptx->pData, buf, SG_FS2__LOCK__HOLE, NULL)  );
            }
            else
            {
                // this happens in concurrent scenarios when the blob has
                // already been moved.  because of the locking, this
                // should never happen

                SG_ERR_THROW(  SG_ERR_ASSERT  );
            }
        }
    }
    SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

    /* Now process all the inserts */
    SG_ERR_CHECK(  sg_fs2__insert__prepare(pCtx, pData->psql, &pStmt)  );
    for (i=0; i<count; i++)
    {
        SG_ERR_CHECK(  SG_vector__get(pCtx, ptx->pvec_blobs, i, (void**) &pbi)  );

        SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
        SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );

        if (SG_BLOBENCODING__VCDIFF == pbi->blob_encoding_storing)
        {
            SG_ASSERT(pbi->psz_hid_vcdiff_reference);
        }

        SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt,1,pbi->sz_hid_blob_final)  );
        SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt,2,pbi->filenumber)  );
        SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt,3,pbi->offset)  );
        SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt,4,pbi->blob_encoding_storing)  );
        SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt,5,pbi->len_encoded_observed)  );
        SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx, pStmt,6,pbi->len_full_given)  );
        SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt,7,pbi->psz_hid_vcdiff_reference)  );
        sg_sqlite__step(pCtx,pStmt,SQLITE_DONE);

        // the only constraint on this table is UNIQUE on the hid column
        // AND the NOT NULL on the hid column
        if (SG_context__err_equals(pCtx, SG_ERR_SQLITE(SQLITE_CONSTRAINT)))
        {
            char buf[sg_FILENUMBER_BUFFER_LENGTH];

			SG_context__err_reset(pCtx);

            // clean up pStmt because we still want to use it
            SG_ERR_IGNORE(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );
            SG_ERR_CHECK(  sg_fs2__insert__prepare(pCtx, pData->psql, &pStmt)  );

            /* If the blob was already present, then we wrote junk to
             * the blob data file.  Since we still hold the append
             * lock on this file, we COULD just truncate it, but
             * only if this blob was the last one we wrote to
             * this file, and we can't know that without some rather
             * more complicated checking.  So we just mark the hole. */

            SG_ERR_CHECK(  sg_fs2__filenumber_to_filename(pCtx, buf, sizeof(buf), pbi->filenumber)  );
            SG_ERR_CHECK(  sg_fs2__lock(pCtx, ptx->pData, buf, SG_FS2__LOCK__HOLE, NULL)  );
        }
		else
		{
            // This code used to do SG_context__err_reset, to ignore other
            // errors from sqlite_step.  We *think* that's wrong, so I'm
            // changing it to SG_ERR_CHECK_CURRENT. -Eric

            SG_ERR_CHECK_CURRENT;
		}
    }
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

    /* store the frags */
    if (ptx->prb_frags)
    {
        SG_dagfrag* pFrag = NULL;
        const char* psz_dagnum = NULL;

        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_new_dagnodes)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, ptx->prb_frags, &b, &psz_dagnum, (void**) &pFrag)  );
        while (b)
        {
            SG_uint32 iDagNum = 0;

            SG_ERR_CHECK(  SG_dagnum__from_sz__decimal(pCtx, psz_dagnum, &iDagNum)  );

            SG_ERR_CHECK(  SG_dag_sqlite3__insert_frag(pCtx, pData->psql, iDagNum, pFrag, &prb_missing, &psa_new)  );
            SG_RBTREE_NULLFREE(pCtx, prb_missing);

            if (psa_new)
            {
                SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_new_dagnodes, psz_dagnum, psa_new)  );
            }
            psa_new = NULL;

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_dagnum, (void**) &pFrag)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    }

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pData->psql, ("COMMIT TRANSACTION"))  );
    pData->b_in_sqlite_transaction = SG_FALSE;

    /* remove the lock files here.  */
	SG_ERR_CHECK(  sg_fs2__remove_lock_files(pCtx, ptx)  );

    /* the commit is complete.  next we want to just update any dbndxes.
     * but if this update fails, we cannot and should not try to rollback
     * the commit itself, since it has already succeeded. */

    if (prb_new_dagnodes)
    {
        SG_ERR_CHECK(  sg_repo__update_ndx(pCtx, pData->pRepo, prb_new_dagnodes,
                    sg_fs2__get_dbndx__cb, pData,
                    sg_fs2__get_treendx__cb, pData)  );
    }

fail:
    SG_ERR_IGNORE(  sg_fs2__nullfree_tx_data(pCtx, (my_tx_data**) pptx)  );
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_RBTREE_NULLFREE(pCtx, prb_missing);
    SG_RBTREE_NULLFREE(pCtx, prb_new_dagnodes);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
    if (pData->b_in_sqlite_transaction)
    {
        SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, ("ROLLBACK TRANSACTION"))  );
        pData->b_in_sqlite_transaction = SG_FALSE;
    }
}

void sg_repo__fs2__check_dagfrag(
	SG_context * pCtx,
	SG_repo * pRepo,
	SG_dagfrag * pFrag,
	SG_bool * pbConnected,
	SG_rbtree ** ppIdsetMissing,
	SG_stringarray ** ppsa_new,
	SG_rbtree** pprbLeaves
    )
{
	my_instance_data * pData = NULL;
	pData = (my_instance_data *)pRepo->p_vtable_instance_data;
    SG_ERR_CHECK_RETURN(  SG_dag_sqlite3__check_frag(pCtx, pData->psql, pFrag, pbConnected, ppIdsetMissing, ppsa_new, pprbLeaves)  );
}

void sg_repo__fs2__store_dagfrag(
	SG_context * pCtx,
	SG_repo * pRepo,
	SG_repo_tx_handle* pTx,
	SG_dagfrag * pFrag
	)
{
    SG_uint32 iDagNum = 0;

    my_tx_data* ptx = NULL;
    char buf[SG_DAGNUM__BUF_MAX__DEC];

    SG_UNUSED(pRepo);

    ptx = (my_tx_data*) pTx;

    /* We don't actually store the frag now.  We just tuck it away
     * so it can be stored at the end when commit_tx gets called. */

    SG_ERR_CHECK(  SG_dagfrag__get_dagnum(pCtx, pFrag, &iDagNum)  );
    SG_ERR_CHECK(  SG_dagnum__to_sz__decimal(pCtx, iDagNum, buf, sizeof(buf))  );
    if (!ptx->prb_frags)
    {
        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &ptx->prb_frags)  );
    }
    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, ptx->prb_frags, buf, pFrag)  );

    return;
fail:
    return;
}

void sg_repo__fs2__commit_tx(
	SG_context * pCtx,
	SG_repo* pRepo,
	SG_repo_tx_handle** ppTx
	)
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULL_PP_CHECK_RETURN(ppTx);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	if (pData->pBlobStoreHandle)
	{
		SG_ERR_IGNORE(  sg_repo__fs2__abort_tx(pCtx, pRepo, ppTx)  );
		SG_ERR_THROW_RETURN(SG_ERR_INCOMPLETE_BLOB_IN_REPO_TX);
	}

    SG_ERR_CHECK(  sg_fs2__commit_tx(pCtx, pData, (my_tx_data**) ppTx)  );

    return;
fail:
    if (pData && (pData->b_in_sqlite_transaction))
    {
        SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, ("ROLLBACK TRANSACTION"))  );
        pData->b_in_sqlite_transaction = SG_FALSE;
    }

    if (ppTx)
    {
        SG_ERR_IGNORE(  sg_fs2__nullfree_tx_data(pCtx, (my_tx_data**) ppTx)  );
    }
}

void sg_repo__fs2__abort_tx(
	SG_context * pCtx,
	SG_repo* pRepo,
	SG_repo_tx_handle** ppTx
	)
{
	my_instance_data * pData = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULL_PP_CHECK_RETURN(ppTx);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	if (pData->pBlobStoreHandle)
	{
        SG_ERR_IGNORE(  sg_blob_fs2__store_blob__abort(pCtx, &pData->pBlobStoreHandle)  );
	}

    if (pData->b_in_sqlite_transaction)
    {
        SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pData->psql, ("ROLLBACK TRANSACTION"))  );
        pData->b_in_sqlite_transaction = SG_FALSE;
    }

    /* TODO rollback truncate the data files */

    SG_ERR_IGNORE(  sg_fs2__nullfree_tx_data(pCtx, (my_tx_data**) ppTx)  );

    return;
fail:
    return;
}

void sg_repo__fs2__dbndx__query(
	SG_context* pCtx,
	SG_repo* pRepo,
    SG_uint32 iDagNum,
	const char* pidState,
	const SG_varray* pcrit,
	const SG_varray* pSort,
    SG_stringarray* psa_slice_fields,
    SG_repo_qresult** ppqr
	)
{
    SG_dbndx* pndx = NULL;
	my_instance_data * pData = NULL;
    SG_dbndx_qresult* pqr = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_ERR_CHECK(  _fs2_my_get_dbndx(pCtx, pData, iDagNum, SG_TRUE, &pndx)  );
    SG_ERR_CHECK(  SG_dbndx__query(pCtx, &pndx, pidState, pcrit, pSort, psa_slice_fields, &pqr)  );
    *ppqr = (SG_repo_qresult*) pqr;
    pqr = NULL;

fail:
    SG_DBNDX_NULLFREE(pCtx, pndx);
    SG_dbndx_qresult__free(pCtx, pqr);
}

void sg_repo__fs2__qresult__count(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_repo_qresult* pqr,
    SG_uint32* piCount
    )
{
    SG_dbndx_qresult* pndx_qr = NULL;

    SG_NULLARGCHECK_RETURN(pqr);
    SG_NULLARGCHECK_RETURN(piCount);
	SG_NULLARGCHECK_RETURN(pRepo);

    pndx_qr = (SG_dbndx_qresult*) pqr;

    SG_ERR_CHECK_RETURN(  SG_dbndx_qresult__count(pCtx, pndx_qr, piCount)  );
}

void sg_repo__fs2__qresult__get__multiple(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_repo_qresult* pqr,
    SG_int32 count_wanted,
    SG_uint32* pi_count_received,
    SG_varray* pva
    )
{
    SG_dbndx_qresult* pndx_qr = NULL;

    SG_NULLARGCHECK_RETURN(pqr);
	SG_NULLARGCHECK_RETURN(pRepo);

    pndx_qr = (SG_dbndx_qresult*) pqr;

    SG_ERR_CHECK_RETURN(  SG_dbndx_qresult__get__multiple(pCtx, pndx_qr, count_wanted, pi_count_received, pva)  );
}

void sg_repo__fs2__qresult__get__one(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_repo_qresult* pqr,
    SG_vhash** ppvh
    )
{
    SG_dbndx_qresult* pndx_qr = NULL;

    SG_NULLARGCHECK_RETURN(pqr);
	SG_NULLARGCHECK_RETURN(pRepo);

    pndx_qr = (SG_dbndx_qresult*) pqr;

    SG_ERR_CHECK_RETURN(  SG_dbndx_qresult__get__one(pCtx, pndx_qr, ppvh)  );
}

void sg_repo__fs2__qresult__done(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_repo_qresult** ppqr
    )
{
    SG_NULLARGCHECK_RETURN(ppqr);
	SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK_RETURN(  SG_dbndx_qresult__done(pCtx, (SG_dbndx_qresult**) ppqr)  );
}

void sg_repo__fs2__dbndx__lookup_audits(
	SG_context* pCtx,
    SG_repo* pRepo, 
    SG_uint32 iDagNum,
    const char* psz_csid,
    SG_varray** ppva
	)
{
    SG_dbndx* pndx = NULL;
	my_instance_data * pData = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_ERR_CHECK(  _fs2_my_get_dbndx(pCtx, pData, iDagNum, SG_TRUE, &pndx)  );
    SG_ERR_CHECK(  SG_dbndx__lookup_audits(pCtx, pndx, psz_csid, ppva)  );
    SG_DBNDX_NULLFREE(pCtx, pndx);

    return;

fail:
    SG_DBNDX_NULLFREE(pCtx, pndx);
    return;
}

void sg_repo__fs2__dbndx__query_record_history(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
    const char* psz_recid,  // this is NOT the hid of a record.  it is the zing recid
    SG_varray** ppva
	)
{
    SG_dbndx* pndx = NULL;
	my_instance_data * pData = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_ERR_CHECK(  _fs2_my_get_dbndx(pCtx, pData, iDagNum, SG_TRUE, &pndx)  );
    SG_ERR_CHECK(  SG_dbndx__query_record_history(pCtx, pndx, psz_recid, ppva)  );
    SG_DBNDX_NULLFREE(pCtx, pndx);

    return;

fail:
    SG_DBNDX_NULLFREE(pCtx, pndx);
    return;
}

void sg_repo__fs2__hash__begin(
	SG_context* pCtx,
    SG_repo * pRepo,
    SG_repo_hash_handle** ppHandle
    )
{
	my_instance_data * pData;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pRepo->p_vtable_instance_data);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_repo_utils__hash_begin__from_sghash(pCtx, pData->buf_hash_method, ppHandle)  );
}

void sg_repo__fs2__hash__chunk(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_hash_handle* pHandle,
    SG_uint32 len_chunk,
    const SG_byte* p_chunk
    )
{
	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pRepo->p_vtable_instance_data);

	SG_ERR_CHECK_RETURN(  sg_repo_utils__hash_chunk__from_sghash(pCtx, pHandle, len_chunk, p_chunk)  );
}

void sg_repo__fs2__hash__end(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_hash_handle** ppHandle,
    char** ppsz_hid_returned
    )
{
	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pRepo->p_vtable_instance_data);

	SG_ERR_CHECK_RETURN(  sg_repo_utils__hash_end__from_sghash(pCtx, ppHandle, ppsz_hid_returned)  );
}

void sg_repo__fs2__hash__abort(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_hash_handle** ppHandle
    )
{
	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pRepo->p_vtable_instance_data);

	SG_ERR_CHECK_RETURN(  sg_repo_utils__hash_abort__from_sghash(pCtx, ppHandle)  );
}

//////////////////////////////////////////////////////////////////

void sg_repo__fs2__query_implementation(
    SG_context* pCtx,
    SG_uint16 question,
    SG_bool* p_bool,
    SG_int64* p_int,
    char* p_buf_string,
    SG_uint32 len_buf_string,
    SG_vhash** pp_vhash
    )
{
  SG_UNUSED(p_int);
  SG_UNUSED(p_buf_string);
  SG_UNUSED(len_buf_string);

    switch (question)
    {
	case SG_REPO__QUESTION__VHASH__LIST_HASH_METHODS:
		{
			SG_NULLARGCHECK_RETURN(pp_vhash);
			SG_ERR_CHECK_RETURN(  sg_repo_utils__list_hash_methods__from_sghash(pCtx, pp_vhash)  );
			return;
		}

	case SG_REPO__QUESTION__BOOL__SUPPORTS_LIVE_WITH_WORKING_DIRECTORY:
		{
			SG_NULLARGCHECK_RETURN(p_bool);
			*p_bool = SG_TRUE;
			return;
		}

	case SG_REPO__QUESTION__BOOL__SUPPORTS_CHANGE_BLOB_ENCODING:
		{
			SG_NULLARGCHECK_RETURN(p_bool);
			*p_bool = SG_TRUE;
			return;
		}

	case SG_REPO__QUESTION__BOOL__SUPPORTS_VACUUM:
		{
			SG_NULLARGCHECK_RETURN(p_bool);
			*p_bool = SG_TRUE;
			return;
		}

	case SG_REPO__QUESTION__BOOL__SUPPORTS_OBLITERATE_BLOB:
		{
			SG_NULLARGCHECK_RETURN(p_bool);
			*p_bool = SG_FALSE;
			return;
		}

	case SG_REPO__QUESTION__BOOL__SUPPORTS_ZLIB:
		{
			SG_NULLARGCHECK_RETURN(p_bool);
			*p_bool = SG_TRUE;
			return;
		}

	case SG_REPO__QUESTION__BOOL__SUPPORTS_VCDIFF:
		{
			SG_NULLARGCHECK_RETURN(p_bool);
			*p_bool = SG_TRUE;
			return;
		}

	default:
		SG_ERR_THROW_RETURN(  SG_ERR_NOTIMPLEMENTED  );
	}
}

void sg_repo__fs2__fetch_blob__info(
    SG_context* pCtx,
	SG_repo * pRepo,
    const char* psz_hid_blob,
    SG_bool* pb_blob_exists,
    char** ppsz_objectid,
    SG_blob_encoding* pBlobFormat,
    char** ppsz_hid_vcdiff_reference,
    SG_uint64* pLenRawData,
    SG_uint64* pLenFull
    )
{
  SG_UNUSED(pRepo);
  SG_UNUSED(psz_hid_blob);
  SG_UNUSED(pb_blob_exists);
  SG_UNUSED(pBlobFormat);
  SG_UNUSED(ppsz_objectid);
  SG_UNUSED(ppsz_hid_vcdiff_reference);
  SG_UNUSED(pLenRawData);
  SG_UNUSED(pLenFull);

    SG_ERR_THROW_RETURN(  SG_ERR_NOTIMPLEMENTED  );
}

void sg_repo__fs2__query_blob_existence(
	SG_context* pCtx,
	SG_repo * pRepo,
	SG_stringarray* psaQueryBlobHids,
	SG_stringarray** ppsaNonexistentBlobs
	)
{
	my_instance_data * pData = NULL;
	SG_stringarray* psaNonexistentBlobs = NULL;
	sqlite3_stmt* pStmt = NULL;
	const char* pszHid = NULL;
	SG_uint32 count, i;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(psaQueryBlobHids);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK(  SG_stringarray__count(pCtx, psaQueryBlobHids, &count)  );
	SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psaNonexistentBlobs, count)  );

	SG_RETRY_THINGIE
	(
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pData->psql, &pStmt,
			"SELECT EXISTS(SELECT filenumber from directory where hid = ?)")  );

		for	(i = 0; i < count; i++)
		{
			SG_bool exists;

			SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
			SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );

			SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psaQueryBlobHids, i, &pszHid)  );

			SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 1, pszHid)  );
			SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_ROW)  );
			exists = sqlite3_column_int(pStmt, 0);
			if (!exists)
				SG_ERR_CHECK(  SG_stringarray__add(pCtx, psaNonexistentBlobs, pszHid)  );
		}

		SG_RETURN_AND_NULL(psaNonexistentBlobs, ppsaNonexistentBlobs);

		SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );
	);

	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	SG_STRINGARRAY_NULLFREE(pCtx, psaNonexistentBlobs);
}

void sg_repo__fs2__obliterate_blob(
    SG_context* pCtx,
	SG_repo * pRepo,
    const char* psz_hid_blob
    )
{
  SG_UNUSED(pRepo);
  SG_UNUSED(psz_hid_blob);

  SG_ERR_THROW_RETURN(  SG_ERR_NOTIMPLEMENTED  );
}

void sg_repo__fs2__treendx__get_path_in_dagnode(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_search_item_gid,
        const char* psz_changeset,
        SG_treenode_entry ** ppTreeNodeEntry
        )
{
    SG_treendx* pTreeNdx = NULL;
	my_instance_data * pData = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_ERR_CHECK(  _fs2_my_get_treendx(pCtx, pData, iDagNum, SG_TRUE, &pTreeNdx)  );

    SG_ERR_CHECK(  SG_treendx__get_path_in_dagnode(pCtx, pTreeNdx, psz_search_item_gid, psz_changeset, ppTreeNodeEntry)  );
    SG_TREENDX_NULLFREE(pCtx, pTreeNdx);

    return;

fail:
    SG_TREENDX_NULLFREE(pCtx, pTreeNdx);
    return;
}

void sg_repo__fs2__treendx__get_all_paths(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_gid,
        SG_stringarray ** ppResults
        )
{
    SG_treendx* pTreeNdx = NULL;
	my_instance_data * pData = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_ERR_CHECK(  _fs2_my_get_treendx(pCtx, pData, iDagNum, SG_TRUE, &pTreeNdx)  );

    SG_ERR_CHECK(  SG_treendx__get_all_paths(pCtx, pTreeNdx, psz_gid, ppResults)  );
    SG_TREENDX_NULLFREE(pCtx, pTreeNdx);

    return;

fail:
    SG_TREENDX_NULLFREE(pCtx, pTreeNdx);
    return;
}

void sg_repo__fs2__get_hash_method(
        SG_context* pCtx,
        SG_repo* pRepo,
        char** ppsz_result  /* caller must free */
        )
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;
    SG_ERR_CHECK(  SG_STRDUP(pCtx, pData->buf_hash_method, ppsz_result)  );

    return;
fail:
    return;
}

void sg_repo__fs2__dbndx__query_across_states(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
	const SG_varray* pcrit,
	SG_int32 gMin,
	SG_int32 gMax,
	SG_vhash** ppResults
	)
{
    SG_dbndx* pndx = NULL;
	my_instance_data * pData = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    SG_ERR_CHECK(  _fs2_my_get_dbndx(pCtx, pData, iDagNum, SG_TRUE, &pndx)  );
    SG_ERR_CHECK(  SG_dbndx__query_across_states(pCtx, pndx, pcrit, gMin, gMax, ppResults)  );
    SG_DBNDX_NULLFREE(pCtx, pndx);

    return;

fail:
    SG_DBNDX_NULLFREE(pCtx, pndx);
    return;
}

void sg_repo__fs2__check_integrity(
    SG_context* pCtx,
    SG_repo * pRepo,
    SG_uint16 cmd,
    SG_uint32 iDagNum,
    SG_bool* p_bool,
    SG_vhash** pp_vhash
    )
{
	my_instance_data * pData = NULL;

	SG_UNUSED(cmd);
	SG_UNUSED(p_bool);
	SG_UNUSED(pp_vhash);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

    // TODO handle cmd
    // TODO handle pp_vhash

    SG_RETRY_THINGIE(
        SG_dag_sqlite3__check_consistency(pCtx, pData->psql, iDagNum)
        );

    return;

fail:
    return;
}

