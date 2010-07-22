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
 * @file sg_repo_vtable__sqlite.c
 *
 * @details SQLite implementation of the repo vtable.
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_repo__private.h"
#include "sg_repo__private_utils.h"

#include "sg_dag_sqlite3_typedefs.h"
#include "sg_dag_sqlite3_prototypes.h"
#include "sg_repo_vtable__sqlite.h"
#include "sg_repo_sqlite__private.h"

// TODO temporary hack:
#ifdef WINDOWS
//#pragma warning(disable:4100)
#pragma warning(disable:4706)
#endif

//////////////////////////////////////////////////////////////////

static void _my_nullfree_instance_data(SG_context* pCtx, my_instance_data** ppData)
{
	if ( ppData && (*ppData))
	{
		SG_dag_sqlite3__nullclose(pCtx, &(*ppData)->psql);
		SG_PATHNAME_NULLFREE(pCtx, (*ppData)->pParentDirPathname);
		SG_PATHNAME_NULLFREE(pCtx, (*ppData)->pMyDirPathname);
		SG_PATHNAME_NULLFREE(pCtx, (*ppData)->pSqlDbPathname);
		(void) sg_blob_sqlite_handle_store__free(pCtx, (*ppData)->pBlobStoreHandle);
		SG_NULLFREE(pCtx, (*ppData));
	}
}

//////////////////////////////////////////////////////////////////

// For SQLITE-based REPOs stored in the CLOSET, we create a unique
// directory and a unique SQLITE file.  Both have a variation of an
// ID in their names.  In both cases these are random names; the
// REPO-DESCRIPTOR contains links to them and is the only thing that
// knows/cares where they are on disk.
//
// TODO review these names and see if we want to define/register
// TODO more meaningful suffixes.  For example, on the Mac we can
// TODO tell Finder to hide the contents of a directory (like it
// TODO does for .app directories).

#define sg_MY_CLOSET_DIR_PREFIX				"sqlite_"
#define sg_MY_CLOSET_DIR_LENGTH_RANDOM		32											// how many hex digits we want in the name
#define sg_MY_CLOSET_DIR_SUFFIX				".dir"
#define sg_MY_CLOSET_DIR_BUFFER_LENGTH		(7 + sg_MY_CLOSET_DIR_LENGTH_RANDOM + 4 + 1)	// "sqlite_" + <hex_digits> + ".dir" + NULL

#define sg_MY_CLOSET_REPO_PREFIX			"sg_"
#define sg_MY_CLOSET_REPO_LENGTH_RANDOM		32											// how many hex digits we want in the name
#define sg_MY_CLOSET_REPO_SUFFIX			".repo"
#define sg_MY_CLOSET_REPO_BUFFER_LENGTH		(3 + sg_MY_CLOSET_REPO_LENGTH_RANDOM + 5 + 1)	// "sg_" + <hex_digits> + ".repo" + NULL

/**
 * Create new, unique names (directory and file) for this instance of a REPO.
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

static void _my_create_name_repo(SG_context * pCtx, char * pBuf, SG_uint32 lenBuf)
{
	char bufTid[SG_TID_MAX_BUFFER_LENGTH];

	SG_ASSERT(  (lenBuf >= sg_MY_CLOSET_REPO_BUFFER_LENGTH)  );

	SG_ERR_CHECK_RETURN(  SG_tid__generate2(pCtx, bufTid, sizeof(bufTid), sg_MY_CLOSET_REPO_LENGTH_RANDOM)  );

	SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx, pBuf, lenBuf, sg_MY_CLOSET_REPO_PREFIX)  );
	SG_ERR_CHECK_RETURN(  SG_strcat(pCtx, pBuf, lenBuf, &bufTid[SG_TID_LENGTH_PREFIX])  );	// skip leading 't' in TID
	SG_ERR_CHECK_RETURN(  SG_strcat(pCtx, pBuf, lenBuf, sg_MY_CLOSET_REPO_SUFFIX)  );
}

void sg_repo__sqlite__create_repo_instance(SG_context* pCtx, SG_repo* pRepo, SG_bool b_indexes, const char* psz_hash_method, const char* psz_repo_id, const char* psz_admin_id)
{
	my_instance_data* pData = NULL;
	const char* pszParentDir = NULL;
	char buf_subdir_name[sg_MY_CLOSET_DIR_BUFFER_LENGTH];
	char buf_db_filename[sg_MY_CLOSET_REPO_BUFFER_LENGTH];
	SG_bool bTxOpen = SG_FALSE;
	sqlite3* psql = NULL;
	char * pszTrivialHash = NULL;

	SG_UNUSED(b_indexes);

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(psz_repo_id);
	SG_NULLARGCHECK_RETURN(psz_admin_id);

	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, psz_repo_id)  );
	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, psz_admin_id)  );

	SG_ARGCHECK_RETURN( pRepo->p_vtable_instance_data==NULL , pRepo ); // Only open/create the db once.

	// Validate the HASH-METHOD.  that is, does this repo implementation support that hash-method.
	// We use the given hash-method to compute a TRIVIAL-HASH.  This does 2 things.  First, it
	// verifies that the hash-method is not bogus.  Second, it gives us a "known answer" that we
	// can put in the "props" (aka "id") table for later consistency checks when pushing/pulling.
	// Note that we cannot just use the REPO API hash routines because we haven't set pRepo->pData
	// and we haven't set pRepo->pData->buf_hash_method, so we do it directly.

	SG_ERR_CHECK(  sg_repo_utils__one_step_hash__from_sghash(pCtx, psz_hash_method, 0, NULL, &pszTrivialHash)  );

	SG_ERR_CHECK(  SG_alloc1(pCtx, pData)  );
	pData->pRepo = pRepo;

	/* We were given a parent dir.  Make sure it exists. */
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pRepo->pvh_descriptor, SG_RIDESC_FSLOCAL__PATH_PARENT_DIR, &pszParentDir)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pData->pParentDirPathname, pszParentDir)  );
	SG_ERR_CHECK(  SG_fsobj__verify_directory_exists_on_disk__pathname(pCtx, pData->pParentDirPathname)  );

	/* Create the subdir name and make sure it exists on disk. */
	SG_ERR_CHECK(  _my_create_name_dir(pCtx, buf_subdir_name, sizeof(buf_subdir_name))  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pData->pMyDirPathname, pData->pParentDirPathname, buf_subdir_name)  );
	SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pData->pMyDirPathname)  );

	/* store the subdir name in the descriptor */
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pRepo->pvh_descriptor, SG_RIDESC_FSLOCAL__DIR_NAME, buf_subdir_name)  );

	/* Build path to new sqlite3 file. */
	SG_ERR_CHECK(  _my_create_name_repo(pCtx, buf_db_filename, sizeof(buf_db_filename))  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pData->pSqlDbPathname, pData->pMyDirPathname, buf_db_filename)  );

	/* store the sqlite db file name in the descriptor */
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pRepo->pvh_descriptor, SG_RIDESC_SQLITE__FILENAME, buf_db_filename)  );

	// Let the DAG code create the sqlite database.
	SG_ERR_CHECK(  SG_dag_sqlite3__create(pCtx, pData->pSqlDbPathname, &psql)  );

	// Add our stuff to it.
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, "BEGIN TRANSACTION")  );
	bTxOpen = SG_TRUE;
	SG_ERR_CHECK(  sg_repo_sqlite__create_id_table(pCtx, psql)  );
	SG_ERR_CHECK(  sg_blob_sqlite__create_blob_tables(pCtx, psql)  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, "COMMIT TRANSACTION")  );
	bTxOpen = SG_FALSE;

	/* Store repoid and adminid in the DB and in memory. */
	SG_ERR_CHECK(  sg_repo_sqlite__add_id(pCtx, psql, "hashmethod", psz_hash_method)  );
	SG_ERR_CHECK(  sg_repo_sqlite__add_id(pCtx, psql, "repoid", psz_repo_id)  );
	SG_ERR_CHECK(  sg_repo_sqlite__add_id(pCtx, psql, "adminid", psz_admin_id)  );
	SG_ERR_CHECK(  sg_repo_sqlite__add_id(pCtx, psql, "trivialhash", pszTrivialHash)  );

	SG_ERR_CHECK(  SG_strcpy(pCtx, pData->buf_hash_method, sizeof(pData->buf_hash_method), psz_hash_method)  );
	SG_ERR_CHECK(  SG_strcpy(pCtx, pData->buf_repo_id, sizeof(pData->buf_repo_id), psz_repo_id)  );
	SG_ERR_CHECK(  SG_strcpy(pCtx, pData->buf_admin_id, sizeof(pData->buf_admin_id), psz_admin_id)  );

	pData->strlen_hashes = strlen(pszTrivialHash);
	SG_NULLFREE(pCtx, pszTrivialHash);

	pData->psql = psql;

	pRepo->p_vtable_instance_data = (sg_repo__vtable__instance_data*)pData;

	return;

fail:
	if (psql)
	{
		if (bTxOpen)
			SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, psql, "ROLLBACK TRANSACTION")  );

		SG_ERR_IGNORE(  SG_dag_sqlite3__nullclose(pCtx, &psql)  );
	}
	SG_ERR_IGNORE(  _my_nullfree_instance_data(pCtx, &pData)  );
	SG_NULLFREE(pCtx, pszTrivialHash);
}

void sg_repo__sqlite__delete_repo_instance(
    SG_context * pCtx,
    SG_repo * pRepo
    )
{
	my_instance_data * pData = NULL;
    SG_pathname * pPath = NULL;

    SG_ASSERT(pCtx!=NULL);
	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPath, pData->pMyDirPathname)  );

    SG_ERR_CHECK(  sg_repo__sqlite__close_repo_instance(pCtx, pRepo)  );

    SG_ERR_CHECK(  SG_fsobj__rmdir_recursive__pathname(pCtx, pPath)  );

    SG_PATHNAME_NULLFREE(pCtx, pPath);

    return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath);
}

void sg_repo__sqlite__open_repo_instance(SG_context* pCtx, SG_repo* pRepo)
{
	my_instance_data* pData = NULL;
	const char* pszParentDir = NULL;
	const char* pszSubDirName = NULL;
	const char* pszDbFileName = NULL;
	SG_string* pstrId = NULL;
	char * pszTrivialHash_Computed = NULL;
	SG_bool bEqual_TrivialHash;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_ARGCHECK_RETURN( pRepo->p_vtable_instance_data==NULL , pRepo ); // Only open/create the db once.

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pData)  );
	pData->pRepo = pRepo;

	// Get parent directory and make sure it exists on disk.
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pRepo->pvh_descriptor, SG_RIDESC_FSLOCAL__PATH_PARENT_DIR, &pszParentDir)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pData->pParentDirPathname, pszParentDir)  );
	SG_ERR_CHECK(  SG_fsobj__verify_directory_exists_on_disk__pathname(pCtx, pData->pParentDirPathname)  );

	// Get our subdir and verify that it exists on disk.
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pRepo->pvh_descriptor, SG_RIDESC_FSLOCAL__DIR_NAME, &pszSubDirName)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pData->pMyDirPathname, pData->pParentDirPathname, pszSubDirName)  );
	SG_ERR_CHECK(  SG_fsobj__verify_directory_exists_on_disk__pathname(pCtx, pData->pMyDirPathname)  );

	// Get path to sqlite file and open it.
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pRepo->pvh_descriptor, SG_RIDESC_SQLITE__FILENAME, &pszDbFileName)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pData->pSqlDbPathname, pData->pMyDirPathname, pszDbFileName)  );
	SG_ERR_CHECK(  SG_dag_sqlite3__open(pCtx, pData->pSqlDbPathname, &pData->psql)  );

	// Get repo and admin ids
	SG_ERR_CHECK( sg_repo_sqlite_get_id(pCtx, pData->psql, "hashmethod", &pstrId) );
	SG_ERR_CHECK( SG_strcpy(pCtx, pData->buf_hash_method, sizeof(pData->buf_hash_method), SG_string__sz(pstrId))  );
	SG_STRING_NULLFREE(pCtx, pstrId);

	SG_ERR_CHECK( sg_repo_sqlite_get_id(pCtx, pData->psql, "repoid", &pstrId) );
	SG_ERR_CHECK( SG_strcpy(pCtx, pData->buf_repo_id, sizeof(pData->buf_repo_id), SG_string__sz(pstrId))  );
	SG_STRING_NULLFREE(pCtx, pstrId);

	SG_ERR_CHECK( sg_repo_sqlite_get_id(pCtx, pData->psql, "adminid", &pstrId) );
	SG_ERR_CHECK( SG_strcpy(pCtx, pData->buf_admin_id, sizeof(pData->buf_admin_id), SG_string__sz(pstrId))  );
	SG_STRING_NULLFREE(pCtx, pstrId);

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
	SG_ERR_CHECK( sg_repo_sqlite_get_id(pCtx, pData->psql, "trivialhash", &pstrId) );
	bEqual_TrivialHash = (strcmp(pszTrivialHash_Computed,SG_string__sz(pstrId)) == 0);
	if (!bEqual_TrivialHash)
		SG_ERR_THROW(  SG_ERR_TRIVIAL_HASH_DIFFERENT  );
	pData->strlen_hashes = SG_string__length_in_bytes(pstrId);
	SG_STRING_NULLFREE(pCtx, pstrId);
	SG_NULLFREE(pCtx, pszTrivialHash_Computed);

	pRepo->p_vtable_instance_data = (sg_repo__vtable__instance_data*)pData;
	return;

fail:
	SG_ERR_IGNORE(  _my_nullfree_instance_data(pCtx, &pData)  );
	SG_STRING_NULLFREE(pCtx, pstrId);
	SG_NULLFREE(pCtx, pszTrivialHash_Computed);
}

void sg_repo__sqlite__close_repo_instance(SG_context* pCtx, SG_repo* pRepo)
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data*)pRepo->p_vtable_instance_data;
	pRepo->p_vtable_instance_data = NULL;

	if (!pData)		// no instance data implies that the sql db is not open
		return;		// just ignore close.

	SG_ERR_CHECK(  SG_dag_sqlite3__nullclose(pCtx, &pData->psql)  );
	_my_nullfree_instance_data(pCtx, &pData);

	return;

fail:
	SG_ERR_REPLACE(SG_ERR_SQLITE(SQLITE_BUSY), SG_ERR_DB_BUSY);
}

void sg_repo__sqlite__begin_tx(SG_context* pCtx,
							   SG_repo* pRepo,
							   SG_repo_tx_handle** ppTx)
{
	my_instance_data* pData = NULL;

	pData = (my_instance_data*)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_repo_tx__sqlite__begin(pCtx, pData, (sg_repo_tx_sqlite_handle**)ppTx)  );
}

void sg_repo__sqlite__commit_tx(SG_context* pCtx,
								SG_repo* pRepo,
								SG_repo_tx_handle** ppTx)
{
	my_instance_data* pData = NULL;

	pData = (my_instance_data*)pRepo->p_vtable_instance_data;

	if (pData->pBlobStoreHandle)
	{
		sg_repo__sqlite__abort_tx(pCtx, pRepo, ppTx);
		SG_ERR_DISCARD;
		SG_ERR_THROW_RETURN(SG_ERR_INCOMPLETE_BLOB_IN_REPO_TX);
	}

	SG_ERR_CHECK_RETURN(  sg_repo_tx__sqlite__commit(pCtx, pRepo, pData->psql, (sg_repo_tx_sqlite_handle**)ppTx)  );
}

void sg_repo__sqlite__abort_tx(SG_context* pCtx,
							   SG_repo* pRepo,
							   SG_repo_tx_handle** ppTx)
{
	my_instance_data* pData = NULL;

	pData = (my_instance_data*)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_repo_tx__sqlite__abort(pCtx, pData->psql, (sg_repo_tx_sqlite_handle**)ppTx, &pData->pBlobStoreHandle)  );
}

void sg_repo__sqlite__fetch_dagnode(SG_context* pCtx,
									SG_repo* pRepo,
									const char* pszidHidChangeset,
									SG_dagnode** ppNewDagnode)	// must match FN_
{
	my_instance_data * pData = NULL;

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_sqlite__exec(pCtx, pData->psql, "BEGIN TRANSACTION")  );
	SG_dag_sqlite3__fetch_dagnode(pCtx, pData->psql, pszidHidChangeset, ppNewDagnode);
	SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );
	SG_ERR_CHECK_RETURN_CURRENT;
}

void sg_repo__sqlite__list_dags(SG_context* pCtx,
								SG_repo* pRepo,
								SG_uint32* piCount,
								SG_uint32** paDagNums)
{
	my_instance_data * pData = NULL;

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_sqlite__exec(pCtx, pData->psql, "BEGIN TRANSACTION")  );
	SG_dag_sqlite3__list_dags(pCtx, pData->psql, piCount, paDagNums);
	SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );
	SG_ERR_CHECK_RETURN_CURRENT;
}

void sg_repo__sqlite__fetch_dag_leaves(SG_context* pCtx,
									   SG_repo* pRepo,
									   SG_uint32 iDagNum,
									   SG_rbtree** ppIdsetReturned)	// must match FN_
{
	my_instance_data * pData = NULL;

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_sqlite__exec(pCtx, pData->psql, "BEGIN TRANSACTION")  );
	SG_dag_sqlite3__fetch_leaves(pCtx, pData->psql, iDagNum, ppIdsetReturned);
	SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );
	SG_ERR_CHECK_RETURN_CURRENT;
}

void sg_repo__sqlite__fetch_dagnode_children(SG_context * pCtx, SG_repo * pRepo, SG_uint32 iDagNum, const char* pszDagnodeID, SG_rbtree ** ppIdsetReturned)	// must match FN_
{
	my_instance_data * pData = NULL;
	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_sqlite__exec(pCtx, pData->psql, "BEGIN TRANSACTION")  );
	SG_dag_sqlite3__fetch_dagnode_children(pCtx, pData->psql, iDagNum, pszDagnodeID, ppIdsetReturned);
	SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );
	SG_ERR_CHECK_RETURN_CURRENT;
}

void sg_repo__sqlite__get_dag_lca(SG_context* pCtx,
								  SG_repo* pRepo,
								  SG_uint32 iDagNum,
								  const SG_rbtree* prbNodes,
								  SG_daglca** ppDagLca)
{
	my_instance_data * pData = NULL;

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_sqlite__exec(pCtx, pData->psql, "BEGIN TRANSACTION")  );
	SG_dag_sqlite3__get_lca(pCtx, pData->psql, iDagNum, prbNodes, ppDagLca);
	SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );
	SG_ERR_CHECK_RETURN_CURRENT;
}

void sg_repo__sqlite__check_dagfrag(SG_context* pCtx,
									SG_repo* pRepo,
									SG_dagfrag* pFrag,
									SG_bool* pbConnected,
									SG_rbtree** ppIdsetMissing,
									SG_stringarray** ppsa_new,
									SG_rbtree** pprbLeaves)
{
	my_instance_data * pData = NULL;
	pData = (my_instance_data *)pRepo->p_vtable_instance_data;
	SG_ERR_CHECK_RETURN(  SG_dag_sqlite3__check_frag(pCtx, pData->psql, pFrag, pbConnected, ppIdsetMissing, ppsa_new, pprbLeaves)  );
}

void sg_repo__sqlite__store_dagfrag(SG_context* pCtx,
									SG_UNUSED_PARAM(SG_repo* pRepo),
									SG_repo_tx_handle* pTx,
									SG_dagfrag* pFrag)
{
    SG_uint32 iDagNum = 0;

	sg_repo_tx_sqlite_handle* ptx = (sg_repo_tx_sqlite_handle*)pTx;
    char buf[SG_DAGNUM__BUF_MAX__DEC];

    SG_UNUSED(pRepo);


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

void sg_repo__sqlite__find_dagnodes_by_prefix(SG_context* pCtx,
											  SG_repo* pRepo,
											  SG_uint32 iDagNum,
											  const char* psz_hid_prefix,
											  SG_rbtree** pprb)
{
	my_instance_data * pData = NULL;

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_sqlite__exec(pCtx, pData->psql, "BEGIN TRANSACTION")  );
	SG_dag_sqlite3__find_by_prefix(pCtx, pData->psql, iDagNum, psz_hid_prefix, pprb);
	SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );
	SG_ERR_CHECK_RETURN_CURRENT;
}

void sg_repo__sqlite__list_blobs(
        SG_context* pCtx,
		SG_repo* pRepo,
        SG_blob_encoding blob_encoding, /**< only return blobs of this encoding */
        SG_bool b_sort_descending,      /**< otherwise sort ascending */
        SG_bool b_sort_by_len_encoded,  /**< otherwise sort by len_full */
        SG_uint32 limit,                /**< only return this many entries */
        SG_uint32 offset,               /**< skip the first group of the sorted entries */
        SG_vhash** ppvh                 /**< Caller must free */
        )
{
	my_instance_data* pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(ppvh);

	pData = (my_instance_data*)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_sqlite__exec(pCtx, pData->psql, "BEGIN TRANSACTION")  );
	sg_blob_sqlite__list_blobs(pCtx, pData->psql, blob_encoding, b_sort_descending, b_sort_by_len_encoded, limit, offset, ppvh);
	SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );
	SG_ERR_CHECK_RETURN_CURRENT;
}

void sg_repo__sqlite__find_blobs_by_prefix(SG_context* pCtx,
										   SG_repo* pRepo,
										   const char* psz_hid_prefix,
										   SG_rbtree** pprb)
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(psz_hid_prefix);
	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pprb);

	pData = (my_instance_data*)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_sqlite__exec(pCtx, pData->psql, "BEGIN TRANSACTION")  );
	sg_blob_sqlite__find_by_hid_prefix(pCtx, pData->psql, psz_hid_prefix, pprb);
	SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );
	SG_ERR_CHECK_RETURN_CURRENT;
}

void sg_repo__sqlite__get_repo_id(SG_context* pCtx, SG_repo* pRepo, char** ppsz_id)
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;
#if defined(DEBUG)
	// buf_repo_id is a GID.  it should have been checked when it was stored.
	// checking now for sanity during GID/HID conversion.
	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, pData->buf_repo_id)  );
#endif
	SG_ERR_CHECK_RETURN(  SG_STRDUP(pCtx, pData->buf_repo_id, ppsz_id)  );
}

void sg_repo__sqlite__get_admin_id(SG_context* pCtx, SG_repo* pRepo, char** ppsz_id)
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;
#if defined(DEBUG)
	// buf_admin_id is a GID.  it should have been checked when it was stored.
	// checking now for sanity during GID/HID conversion.
	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, pData->buf_admin_id)  );
#endif
	SG_ERR_CHECK_RETURN(  SG_STRDUP(pCtx, pData->buf_admin_id, ppsz_id)  );
}

static void sg_sqlite__get_dbndx_path(
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
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pData->pMyDirPathname, buf)  );

    *ppResult = pPath;

fail:
    ;
}

static void sg_sqlite__get_treendx_path(
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
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pData->pMyDirPathname, buf)  );

    *ppResult = pPath;

fail:
    ;
}

void _sqlite_my_get_dbndx(SG_context* pCtx,
							  SG_repo* pRepo,
							  SG_uint32 iDagNum,
							  SG_bool bQueryOnly,
							  SG_dbndx** ppNdx)
{
	my_instance_data* pData = (my_instance_data*)pRepo->p_vtable_instance_data;
	SG_pathname* pPath = NULL;

	SG_ERR_CHECK(  sg_sqlite__get_dbndx_path(pCtx, pData, iDagNum, &pPath)  );

	SG_ERR_CHECK_RETURN(  SG_dbndx__open(pCtx, pRepo, iDagNum, pPath, bQueryOnly, ppNdx)  );
fail:
	;
	    /* don't free pPath here because the treendx should own it */
	return;
}

void _sqlite_my_get_treendx(SG_context* pCtx,
							  SG_repo* pRepo,
							  SG_uint32 iDagNum,
							  SG_bool bQueryOnly,
							  SG_treendx** ppNdx)
{
	my_instance_data* pData = (my_instance_data*)pRepo->p_vtable_instance_data;
	SG_pathname* pPath = NULL;

	SG_ERR_CHECK(  sg_sqlite__get_treendx_path(pCtx, pData, iDagNum, &pPath)  );

	SG_ERR_CHECK_RETURN(  SG_treendx__open(pCtx, pRepo, iDagNum, pPath, bQueryOnly, ppNdx)  );
fail:
	;
	    /* don't free pPath here because the treendx should own it */
	return;
}


void sg_repo__sqlite__store_blob__begin(
    SG_context* pCtx,
	SG_repo* pRepo,
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
	my_instance_data * pData = NULL;
	sg_blob_sqlite_handle_store* pbh = NULL;
	SG_bool bCompress;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pTx);

    SG_UNUSED(psz_objectid);

	pData = (my_instance_data*)pRepo->p_vtable_instance_data;

	if (pData->pBlobStoreHandle)
		SG_ERR_THROW_RETURN(SG_ERR_INCOMPLETE_BLOB_IN_REPO_TX);

	// Tweak the compression policy here.

	// Compress everything unless we're explictly told otherwise.
	bCompress = (SG_BLOBENCODING__FULL == blob_encoding);

	// Compress nothing.
	//bCompress = SG_FALSE;

	SG_ERR_CHECK_RETURN(  sg_blob_sqlite__store_blob__begin(
		pCtx,
		(sg_repo_tx_sqlite_handle*)pTx,
		pData->psql,
		blob_encoding,
		psz_hid_vcdiff_reference,
		lenFull,
		lenEncoded,
		psz_hid_known,
		bCompress,
		&pbh)  );

	pData->pBlobStoreHandle = pbh;

	*ppHandle = (SG_repo_store_blob_handle*)pbh;
}

void sg_repo__sqlite__store_blob__chunk(SG_context* pCtx,
										SG_repo* pRepo,
										SG_repo_store_blob_handle* pHandle,
										SG_uint32 len_chunk,
										const SG_byte* p_chunk,
										SG_uint32* piWritten)
{
	SG_NULLARGCHECK_RETURN(pRepo);

	SG_ERR_CHECK_RETURN(  sg_blob_sqlite__store_blob__chunk(pCtx, (sg_blob_sqlite_handle_store*)pHandle, len_chunk, p_chunk, piWritten)  );
}

void sg_repo__sqlite__store_blob__end(SG_context* pCtx,
									  SG_repo* pRepo,
									  SG_repo_tx_handle* pTx,
									  SG_repo_store_blob_handle** ppHandle,
									  char** ppsz_hid_returned)
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pTx);

	pData = (my_instance_data*)pRepo->p_vtable_instance_data;
	pData->pBlobStoreHandle = NULL;

	SG_ERR_CHECK_RETURN(
		sg_blob_sqlite__store_blob__end(pCtx, (sg_repo_tx_sqlite_handle*)pTx, (sg_blob_sqlite_handle_store**)ppHandle, ppsz_hid_returned)  );
}

void sg_repo__sqlite__store_blob__abort(SG_context* pCtx,
										SG_repo* pRepo,
										SG_repo_tx_handle* pTx,
										SG_repo_store_blob_handle** ppHandle)
{
	my_instance_data * pData = (my_instance_data*)pRepo->p_vtable_instance_data;

	SG_NULLARGCHECK_RETURN(pTx);

	pData->pBlobStoreHandle = NULL;

	sg_blob_sqlite__store_blob__abort(pCtx, (sg_repo_tx_sqlite_handle*)pTx, (sg_blob_sqlite_handle_store**)ppHandle);
}

void sg_repo__sqlite__fetch_blob__begin(SG_context* pCtx,
										SG_repo* pRepo,
										const char* psz_hid_blob,
										SG_bool b_convert_to_full,
										char** ppsz_objectid,
										SG_blob_encoding* pBlobFormat,
										char** ppsz_hid_vcdiff_reference,
										SG_uint64* pLenRawData,
										SG_uint64* pLenFull,
										SG_repo_fetch_blob_handle** ppHandle)
{
	my_instance_data * pData = NULL;
	sg_blob_sqlite_handle_fetch* pbh = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_blob_sqlite__fetch_blob__begin(pCtx, pData, pData->psql, psz_hid_blob,
		b_convert_to_full, ppsz_objectid, pBlobFormat, ppsz_hid_vcdiff_reference, pLenRawData, pLenFull, &pbh)  );

	if (ppHandle)
		*ppHandle = (SG_repo_fetch_blob_handle*) pbh;
}

void sg_repo__sqlite__fetch_blob__chunk(SG_context* pCtx,
											SG_repo* pRepo,
											SG_repo_fetch_blob_handle* pHandle,
											SG_uint32 len_buf,
											SG_byte* p_buf,
											SG_uint32* p_len_got,
                                            SG_bool* pb_done)
{
	SG_NULLARGCHECK_RETURN(pRepo);

	SG_ERR_CHECK_RETURN(  sg_blob_sqlite__fetch_blob__chunk(pCtx, (sg_blob_sqlite_handle_fetch*)pHandle, len_buf, p_buf, p_len_got, pb_done)  );
}

void sg_repo__sqlite__fetch_blob__end(SG_context* pCtx,
									  SG_repo* pRepo,
									  SG_repo_fetch_blob_handle** ppHandle)
{
	SG_NULLARGCHECK_RETURN(pRepo);

	SG_ERR_CHECK_RETURN(  sg_blob_sqlite__fetch_blob__end(pCtx, (sg_blob_sqlite_handle_fetch**) ppHandle)  );
}

void sg_repo__sqlite__fetch_blob__abort(SG_context* pCtx,
											SG_UNUSED_PARAM(SG_repo* pRepo),
											SG_repo_fetch_blob_handle** ppHandle)
{
	SG_UNUSED(pRepo);
	SG_ERR_CHECK_RETURN(  sg_blob_sqlite__fetch_blob__abort(pCtx, (sg_blob_sqlite_handle_fetch**)ppHandle)  );
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
	sg_blob_sqlite_handle_fetch* pbh = NULL;
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
			"SELECT hid_blob from blobinfo")  );

		while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
		{
			pszHid = (const char*)sqlite3_column_text(pStmt, 0);
			SG_ERR_CHECK(  sg_blob_sqlite__fetch_blob__begin(pCtx, pData, pData->psql, pszHid, SG_FALSE, &psz_objectid, &encoding, &pszHidVcdiffRef, &lenEncoded, &lenFull, &pbh)  );
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
		SG_ERR_IGNORE(  sg_blob_sqlite__fetch_blob__abort(pCtx, &pbh)  );
}

void sg_repo__sqlite__fetch_repo__fragball(
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
	SG_ERR_CHECK(  _build_repo_fragball(pCtx, pRepo, pFragballPathname)  );
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

void sg_repo__sqlite__get_blob_stats(SG_context* pCtx,
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
									 SG_uint64* p_total_blob_size_vcdiff_encoded)
{
	my_instance_data* pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data*)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_sqlite__exec(pCtx, pData->psql, "BEGIN TRANSACTION")  );
	SG_ERR_CHECK(  sg_blob_sqlite__get_blob_stats(pCtx, pData->psql,
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
		)  );

	/* fall through */
fail:
	SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );
}

void sg_repo__sqlite__change_blob_encoding(SG_context* pCtx,
										   SG_repo* pRepo,
										   SG_repo_tx_handle* pTx,
										   const char* psz_hid_blob,
										   SG_blob_encoding blob_encoding_desired,
										   const char* psz_hid_vcdiff_reference_desired,
										   SG_blob_encoding* p_blob_encoding_new,
										   char** ppsz_hid_vcdiff_reference,
										   SG_uint64* p_len_encoded,
										   SG_uint64* p_len_full)
{
	my_instance_data* pData;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data*)pRepo->p_vtable_instance_data;

	SG_ERR_CHECK_RETURN(  sg_blob_sqlite__change_blob_encoding(pCtx, (sg_repo_tx_sqlite_handle*)pTx, pData->psql, psz_hid_blob, blob_encoding_desired,
		psz_hid_vcdiff_reference_desired, p_blob_encoding_new, ppsz_hid_vcdiff_reference, p_len_encoded, p_len_full)  );
}

void sg_repo__sqlite__vacuum(SG_context* pCtx,
							 SG_repo * pRepo)
{
	my_instance_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);

	pData = (my_instance_data *)pRepo->p_vtable_instance_data;

	// Ian TODO: All this does now is a sqlite vacuum.
	//
	// It should probably also look for "orphaned" blobs (which are blobs that exist in the
	// blobs table but not in blobinfo or blobmap) and delete them.  This would account for
	// blobs that were written, then there's a power loss before commit_tx is called.  So
	// abort_tx is never called to clean them up.  And we're writing blobs outside sqlite
	// transactions, so there's no journal rollback that's going to clean things up.
	//
	// Obviously this means some kind of lock is necessary.  Running vacuum during a repo tx
	// would be disastrous, as blobs are written first and not encapsulated in a sqlite tx.
	// I'm not sure explicitly requesting an EXCLUSIVE sqlite lock will do the trick, so this
	// needs some thought.
	//
	// PRAGMA locking_mode = EXCLUSIVE might help.

	SG_ERR_CHECK_RETURN(  sg_blob_sqlite__vacuum(pCtx, pData->psql)  );
}

//////////////////////////////////////////////////////////////////

void sg_repo__sqlite__hash__begin(
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

void sg_repo__sqlite__hash__chunk(
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

void sg_repo__sqlite__hash__end(
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

void sg_repo__sqlite__hash__abort(
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

void sg_repo__sqlite__query_implementation(
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
			*p_bool = SG_FALSE;		// TODO fix this
			return;
		}

	case SG_REPO__QUESTION__BOOL__SUPPORTS_CHANGE_BLOB_ENCODING:
		{
			SG_NULLARGCHECK_RETURN(p_bool);
			*p_bool = SG_TRUE;
			return;
		}

	default:
		SG_ERR_THROW_RETURN(  SG_ERR_NOTIMPLEMENTED  );
	}
}

void sg_repo__sqlite__fetch_blob__info(
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

void sg_repo__sqlite__query_blob_existence(
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

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pData->psql, "BEGIN TRANSACTION")  );

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pData->psql, &pStmt,
		"SELECT EXISTS(SELECT rowid_alias from blobinfo where hid_blob = ?)")  );

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
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );

	return;

fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );
	SG_STRINGARRAY_NULLFREE(pCtx, psaNonexistentBlobs);
}

void sg_repo__sqlite__obliterate_blob(
    SG_context* pCtx,
	SG_repo * pRepo,
    const char* psz_hid_blob
    )
{
  SG_UNUSED(pRepo);
  SG_UNUSED(psz_hid_blob);

    SG_ERR_THROW_RETURN(  SG_ERR_NOTIMPLEMENTED  );
}

void sg_repo__sqlite__dbndx__lookup_audits(
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

    SG_ERR_CHECK(  _sqlite_my_get_dbndx(pCtx, pRepo, iDagNum, SG_TRUE, &pndx)  );
    SG_ERR_CHECK(  SG_dbndx__lookup_audits(pCtx, pndx, psz_csid, ppva)  );
    SG_DBNDX_NULLFREE(pCtx, pndx);

    return;

fail:
    SG_DBNDX_NULLFREE(pCtx, pndx);
    return;
}

void sg_repo__sqlite__dbndx__query_record_history(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
    const char* psz_recid,  // this is NOT the hid of a record.  it is the zing recid
    SG_varray** ppva
	)
{
    SG_dbndx* pndx = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK(  _sqlite_my_get_dbndx(pCtx, pRepo, iDagNum, SG_TRUE, &pndx)  );
    SG_ERR_CHECK(  SG_dbndx__query_record_history(pCtx, pndx, psz_recid, ppva)  );
    SG_DBNDX_NULLFREE(pCtx, pndx);

    return;

fail:
    SG_DBNDX_NULLFREE(pCtx, pndx);
    return;
}

void sg_repo__sqlite__dbndx__query(
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
    SG_dbndx_qresult* pqr = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK(  _sqlite_my_get_dbndx(pCtx, pRepo, iDagNum, SG_TRUE, &pndx)  );
    SG_ERR_CHECK(  SG_dbndx__query(pCtx, &pndx, pidState, pcrit, pSort, psa_slice_fields, &pqr)  );
    *ppqr = (SG_repo_qresult*) pqr;
    pqr = NULL;

fail:
    SG_DBNDX_NULLFREE(pCtx, pndx);
    SG_ERR_IGNORE(  SG_dbndx_qresult__free(pCtx, pqr)  );
}

void sg_repo__sqlite__qresult__count(
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

void sg_repo__sqlite__qresult__get__multiple(
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

void sg_repo__sqlite__qresult__get__one(
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

void sg_repo__sqlite__qresult__done(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_repo_qresult** ppqr
    )
{
    SG_NULLARGCHECK_RETURN(ppqr);
	SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK_RETURN(  SG_dbndx_qresult__done(pCtx, (SG_dbndx_qresult**) ppqr)  );
}

void sg_repo__sqlite__treendx__get_path_in_dagnode(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_search_item_gid,
        const char* psz_changeset,
        SG_treenode_entry ** ppTreeNodeEntry
        )
{
    SG_treendx* pTreeNdx = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK(  _sqlite_my_get_treendx(pCtx, pRepo, iDagNum, SG_TRUE, &pTreeNdx)  );

    SG_ERR_CHECK(  SG_treendx__get_path_in_dagnode(pCtx, pTreeNdx, psz_search_item_gid, psz_changeset, ppTreeNodeEntry)  );
    SG_TREENDX_NULLFREE(pCtx, pTreeNdx);

    return;

fail:
    SG_TREENDX_NULLFREE(pCtx, pTreeNdx);
    return;
}

void sg_repo__sqlite__treendx__get_all_paths(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_gid,
        SG_stringarray ** ppResults
        )
{
    SG_treendx* pTreeNdx = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK(  _sqlite_my_get_treendx(pCtx, pRepo, iDagNum, SG_TRUE, &pTreeNdx)  );

    SG_ERR_CHECK(  SG_treendx__get_all_paths(pCtx, pTreeNdx, psz_gid, ppResults)  );
    SG_TREENDX_NULLFREE(pCtx, pTreeNdx);

    return;

fail:
    SG_TREENDX_NULLFREE(pCtx, pTreeNdx);
    return;
}

void sg_repo__sqlite__get_hash_method(
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

void sg_repo__sqlite__dbndx__query_across_states(
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

    SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK(  _sqlite_my_get_dbndx(pCtx, pRepo, iDagNum, SG_TRUE, &pndx)  );
    SG_ERR_CHECK(  SG_dbndx__query_across_states(pCtx, pndx, pcrit, gMin, gMax, ppResults)  );
    SG_DBNDX_NULLFREE(pCtx, pndx);

    return;

fail:
    SG_DBNDX_NULLFREE(pCtx, pndx);
    return;
}

void sg_repo__sqlite__check_integrity(
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

	SG_ERR_CHECK_RETURN(  sg_sqlite__exec(pCtx, pData->psql, "BEGIN TRANSACTION")  );
	SG_ERR_CHECK(  SG_dag_sqlite3__check_consistency(pCtx, pData->psql, iDagNum)  );

	/* fall through */
fail:
	SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pData->psql, "ROLLBACK TRANSACTION")  );
}

