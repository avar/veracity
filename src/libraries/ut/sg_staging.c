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
#include <zlib.h>

#define MY_BUSY_TIMEOUT_MS      (30000)
#define REPO_FILENAME "__REPO__"

struct _sg_staging_blob_handle
{
	SG_repo* pRepo;
	SG_pathname* pFragballPathname;
	SG_uint64 offset;
	SG_blob_encoding encoding;
	SG_uint64 len_encoded;
	SG_uint64 len_full;
};
typedef struct _sg_staging_blob_handle sg_staging_blob_handle;

struct _sg_staging
{
	sqlite3* psql;
	SG_pathname* pPath;
	SG_repo* pRepo;
};
typedef struct _sg_staging sg_staging;

static void _nullfree_staging_blob_handle(SG_context* pCtx, sg_staging_blob_handle** ppbh)
{
	if (ppbh)
	{
		sg_staging_blob_handle* pbh = *ppbh;
		if (pbh)
		{
			SG_PATHNAME_NULLFREE(pCtx, pbh->pFragballPathname);
			SG_NULLFREE(pCtx, pbh);
		}
		*ppbh = NULL;
	}
}

void SG_staging__free(SG_context* pCtx, SG_staging* pStaging)
{
	if (pStaging)
	{
		sg_staging* ps = (sg_staging*)pStaging;
		SG_PATHNAME_NULLFREE(pCtx, ps->pPath);
		SG_REPO_NULLFREE(pCtx, ps->pRepo);
		if (ps->psql)
			SG_ERR_CHECK_RETURN(  sg_sqlite__close(pCtx, ps->psql)  );
		SG_NULLFREE(pCtx, ps);
	}
}

static void _add_new_nodes_to_status(
	SG_context* pCtx,
	const char* const* aszHids,
	SG_uint32 countHids,
	SG_vhash* pvh_status,
	const char* pszDagnum)
{
	SG_bool b = SG_FALSE;
	SG_uint32 i;
	SG_vhash* pvh_dag_new_nodes_container = NULL;
	SG_vhash* pvh_dag_new_nodes_container_ref = NULL;
	SG_vhash* pvh_dag_new_nodes = NULL;

	if (!countHids)
		return;

	SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_status, SG_SYNC_STATUS_KEY__NEW_NODES, &b)  );

	if (b)
	{
		SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_status, SG_SYNC_STATUS_KEY__NEW_NODES, &pvh_dag_new_nodes_container_ref)  );
	}
	else
	{
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_dag_new_nodes_container)  );
		pvh_dag_new_nodes_container_ref = pvh_dag_new_nodes_container;
		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh_status, SG_SYNC_STATUS_KEY__NEW_NODES, &pvh_dag_new_nodes_container)  );
	}

	SG_ERR_CHECK(  SG_VHASH__ALLOC__PARAMS(pCtx, &pvh_dag_new_nodes, countHids, NULL, NULL)  );

	for (i = 0; i < countHids; i++)
		SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pvh_dag_new_nodes, aszHids[i])  );

	SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh_dag_new_nodes_container_ref, pszDagnum, &pvh_dag_new_nodes)  );

	// fall through
fail:
	SG_VHASH_NULLFREE(pCtx, pvh_dag_new_nodes_container);
	SG_VHASH_NULLFREE(pCtx, pvh_dag_new_nodes);

}

static void _add_disconnected_dag_to_status(
	SG_context* pCtx,
	SG_rbtree* prb_missing,
	SG_vhash* pvh_status,
	const char* pszDagnum)
{
	SG_uint32 count;
	SG_bool b = SG_FALSE;
	SG_rbtree_iterator* pit = NULL;
	SG_vhash* pvh_dag_fringe_container = NULL;
	SG_vhash* pvh_dag_fringe_container_ref = NULL;
	SG_vhash* pvh_fringe = NULL;
	const char* psz_hid = NULL;

	SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_missing, &count)  );
	SG_ASSERT(count);

	SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_status, SG_SYNC_STATUS_KEY__DAGS, &b)  );

	if (b)
	{
		SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_status, SG_SYNC_STATUS_KEY__DAGS, &pvh_dag_fringe_container_ref)  );
	}
	else
	{
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_dag_fringe_container)  );
        pvh_dag_fringe_container_ref = pvh_dag_fringe_container;
		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh_status, SG_SYNC_STATUS_KEY__DAGS, &pvh_dag_fringe_container)  );
	}

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_fringe)  );

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_missing, &b, &psz_hid, NULL)  );
	while (b)
	{
		SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pvh_fringe, psz_hid)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid, NULL)  );
	}

	SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh_dag_fringe_container_ref, pszDagnum, &pvh_fringe)  );

	// fall through

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_VHASH_NULLFREE(pCtx, pvh_dag_fringe_container);
    SG_VHASH_NULLFREE(pCtx, pvh_fringe);
}

static void _add_leafdelta_to_status(
	SG_context* pCtx,
	SG_vhash* pvh_status,
	const char* pszDagnum,
	SG_int32 leafDelta)
{
	SG_bool b = SG_FALSE;
	SG_vhash* pvh_leafd_container = NULL;
	SG_vhash* pvh_leafd_container_ref = NULL;

	SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_status, SG_SYNC_STATUS_KEY__LEAFD, &b)  );

	if (b)
	{
		SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_status, SG_SYNC_STATUS_KEY__LEAFD, &pvh_leafd_container_ref)  );
	}
	else
	{
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_leafd_container)  );
		pvh_leafd_container_ref = pvh_leafd_container;
		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh_status, SG_SYNC_STATUS_KEY__LEAFD, &pvh_leafd_container)  );
	}
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_leafd_container_ref, pszDagnum, leafDelta)  );

	// fall through

fail:
	SG_VHASH_NULLFREE(pCtx, pvh_leafd_container);
}

static void _get_blob_from_staging(SG_context* pCtx,
								sg_staging* pMe,
								const char* pszHid,
								sg_staging_blob_handle** ppBlobHandleReturned)
{
	sqlite3_stmt* pStmt = NULL;
	int rc;
	sg_staging_blob_handle* pBlobHandleReturned = NULL;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
		"SELECT filename, offset, encoding, len_encoded, len_full FROM blobs_present WHERE hid = ? LIMIT 1")  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt,1,pszHid)  );
	rc = sqlite3_step(pStmt);

	if (rc == SQLITE_ROW)
	{
		if (ppBlobHandleReturned)
		{
			SG_alloc1(pCtx, pBlobHandleReturned);

			pBlobHandleReturned->pRepo = pMe->pRepo;
			SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pBlobHandleReturned->pFragballPathname,
				pMe->pPath, (const char*)sqlite3_column_text(pStmt, 0))  );
			pBlobHandleReturned->offset = sqlite3_column_int64(pStmt, 1);
			pBlobHandleReturned->encoding = (SG_blob_encoding)sqlite3_column_int(pStmt, 2);
			pBlobHandleReturned->len_encoded = sqlite3_column_int64(pStmt, 3);
			pBlobHandleReturned->len_full = sqlite3_column_int64(pStmt, 4);

			*ppBlobHandleReturned = pBlobHandleReturned;
		}
	}
	else if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}
	else
	{
		// blob doesn't exist
		SG_ERR_THROW(SG_ERR_BLOB_NOT_FOUND);
	}

	SG_ERR_CHECK(  sg_sqlite__finalize(pCtx, pStmt)  );

	return;

fail:
	_nullfree_staging_blob_handle(pCtx, &pBlobHandleReturned);
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx,pStmt)  );
}

static void _add_missing_blobs_to_status(
	SG_context* pCtx,
	SG_vhash* pvh_status,
	SG_stringarray* psaMissingBlobHids)
{
	SG_bool b = SG_FALSE;
	SG_vhash* pvh_container = NULL;
	SG_vhash* pvh_container_ref = NULL;
	SG_uint32 i, count;

	SG_ERR_CHECK(  SG_stringarray__count(pCtx, psaMissingBlobHids, &count)  );
	if (count)
	{
		SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_status, SG_SYNC_STATUS_KEY__BLOBS, &b)  );
		if (b)
		{
			SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_status, SG_SYNC_STATUS_KEY__BLOBS, &pvh_container_ref)  );
		}
		else
		{
			SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_container)  );
			pvh_container_ref = pvh_container;
			SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh_status, SG_SYNC_STATUS_KEY__BLOBS, &pvh_container)  );
		}

		for (i = 0; i < count; i++)
		{
			const char* pszHid;
			SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psaMissingBlobHids, i, &pszHid)  );
			SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pvh_container_ref, pszHid)  );
		}
	}

    return;
fail:
    SG_VHASH_NULLFREE(pCtx, pvh_container);
}

static void _add_referenced_blobs__from_bloblist(
	SG_context* pCtx,
	sg_staging* pMe,
    SG_vhash* pvh
	)
{
	SG_uint32 i;
	sqlite3_stmt* pStmt = NULL;
    SG_uint32 count = 0;

    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx,pMe->psql,&pStmt,
		(
		"INSERT OR IGNORE INTO blobs_referenced "
		"(hid, is_changeset, data_blobs_known) "
		"VALUES (?, ?, 0)"
		) )  );

	for (i = 0; i < count; i++)
	{
        const char* psz_hid = NULL;

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh, i, &psz_hid, NULL)  );

		SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
		SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );
		SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx,pStmt,1,psz_hid)  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx,pStmt,2,SG_FALSE)  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx,pStmt,SQLITE_DONE)  );
	}

	/* fall through */
fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx,pStmt)  );
}

static void _add_referenced_blobs__from_list_of_bloblists(
	SG_context* pCtx,
	sg_staging* pMe,
    const SG_vhash* pvh_lbl
	)
{
    SG_uint32 count_lists = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_lbl, &count_lists)  );
    for (i=0; i<count_lists; i++)
    {
        const char* psz_key = NULL;
        const SG_variant* pv = NULL;
        SG_vhash* pvh_bl = NULL;

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_lbl, i, &psz_key, &pv)  );
        SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &pvh_bl)  );
        SG_ERR_CHECK(  _add_referenced_blobs__from_bloblist(pCtx, pMe, pvh_bl)  );
    }

fail:
    ;
}

static void _add_referenced_blobs(
	SG_context* pCtx,
	sg_staging* pMe,
	const char* const* aszHids,
	SG_uint32 countHids
	)
{
	SG_uint32 i;
	sqlite3_stmt* pStmt = NULL;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx,pMe->psql,&pStmt,
		(
		"INSERT OR IGNORE INTO blobs_referenced "
		"(hid, is_changeset, data_blobs_known) "
		"VALUES (?, ?, 0)"
		) )  );

	for (i = 0; i < countHids; i++)
	{
		SG_ERR_CHECK(  sg_sqlite__reset(pCtx, pStmt)  );
		SG_ERR_CHECK(  sg_sqlite__clear_bindings(pCtx, pStmt)  );
		SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx,pStmt,1,aszHids[i])  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx,pStmt,2,SG_TRUE)  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx,pStmt,SQLITE_DONE)  );
	}

	/* fall through */
fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx,pStmt)  );
}

/**
 * Updates pvh_status to reflect blobs in the current push that are
 * not yet present in the staging area or the repo itself.
 */
static void _check_blobs(
	SG_context* pCtx,
	sg_staging* pMe,
	SG_vhash* pvh_status,
	SG_bool bCheckChangesetBlobs,
	SG_bool bCheckDataBlobs)
{
	sqlite3_stmt* pStmt = NULL;
	int rc;
	SG_stringarray* psaQueryRepoForBlobs = NULL;
	SG_stringarray* psaBlobsNotInRepo = NULL;
	SG_changeset* pChangeset = NULL;
	sg_staging_blob_handle* pBlobHandle = NULL;

	/* Ian TODO: missing vcdiff reference blobs probably get handled in here */

	// If the caller cares about non-changeset blobs, we crack open
	// all the changeset blobs that are present in the staging area and haven't
	// already been explored.  We'll add the data blobs to our blobs_referenced
	// table.
	if (bCheckDataBlobs)
	{
		const char* pszHid;

		// Get a list of all the changesets whose blobs haven't yet been explored.
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
			("SELECT hid FROM blobs_referenced "
			 "WHERE is_changeset = 1 AND data_blobs_known = 0 "
			 " AND hid IN (SELECT hid from blobs_present)"))  );
		while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
		{
            SG_vhash* pvh_lbl = NULL;

			pszHid = (const char*)sqlite3_column_text(pStmt, 0);

			// Read changeset blob from staging to get its list of blobs.
			SG_ERR_CHECK(  _get_blob_from_staging(pCtx, pMe, pszHid, &pBlobHandle)  );
			SG_ERR_CHECK(  SG_changeset__load_from_staging(pCtx, pszHid, (SG_staging_blob_handle*)pBlobHandle, &pChangeset)  );
			SG_ERR_CHECK(  SG_changeset__get_list_of_bloblists(pCtx, pChangeset, &pvh_lbl)  );
			SG_ERR_CHECK(  _nullfree_staging_blob_handle(pCtx, &pBlobHandle)  );

			// Add the newly discovered non-changeset blobs to the staging area's list of referenced blobs.
			SG_ERR_CHECK(  _add_referenced_blobs__from_list_of_bloblists(pCtx, pMe, pvh_lbl)  );

			SG_CHANGESET_NULLFREE(pCtx, pChangeset);
		}
		if (rc != SQLITE_DONE)
			SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
		SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

		// Regardless of the sqlite transaction status, this is safe because
		// staging areas are unique to each invocation of pull_begin/push_begin.
		SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql,
			("UPDATE blobs_referenced SET data_blobs_known = 1 "
			 "WHERE is_changeset = 1 AND data_blobs_known = 0 "
			 "  AND hid IN (SELECT hid from blobs_present)"))  );

		// At this point the blobs_referenced table is up-to-date given the
		// frags and changeset blobs that are present.
	}

	// Build the list of blobs to ask the repo which ones it needs.
	if (bCheckChangesetBlobs)
	{
		if (bCheckDataBlobs)
		{
			SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
				("SELECT hid FROM blobs_referenced "
				 "WHERE hid NOT IN (SELECT hid FROM blobs_present)"))  );
		}
		else
		{
			SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
				("SELECT hid FROM blobs_referenced "
				 "WHERE is_changeset = 1 "
				 " AND hid NOT IN (SELECT hid FROM blobs_present)"))  );
		}
	}
	else
	{
		if (bCheckDataBlobs)
		{
			SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
				("SELECT hid FROM blobs_referenced "
				"WHERE is_changeset = 0 "
				" AND hid NOT IN (SELECT hid FROM blobs_present)"))  );
		}
	}
	if (pStmt)
	{
		SG_int32 count;
		SG_ERR_CHECK(  sg_sqlite__exec__va__int32(pCtx, pMe->psql, &count,
			("SELECT (SELECT count(*) FROM blobs_referenced) "
			 " - (SELECT count(*) FROM blobs_present)"))  );

		if (count)
		{
			SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psaQueryRepoForBlobs, count)  );

			while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
			{
				SG_ERR_CHECK(  SG_stringarray__add(pCtx, psaQueryRepoForBlobs,
					(const char*)sqlite3_column_text(pStmt, 0))  );
			}
			if (rc != SQLITE_DONE)
				SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
			SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

			SG_ERR_CHECK(  SG_repo__query_blob_existence(pCtx, pMe->pRepo, psaQueryRepoForBlobs, &psaBlobsNotInRepo)  );

			SG_ERR_CHECK(  _add_missing_blobs_to_status(pCtx, pvh_status, psaBlobsNotInRepo)  );
		}
	}

	/* fall through*/
fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx, pStmt)  );
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	SG_ERR_IGNORE(  _nullfree_staging_blob_handle(pCtx, &pBlobHandle)  );
	SG_STRINGARRAY_NULLFREE(pCtx, psaQueryRepoForBlobs);
	SG_STRINGARRAY_NULLFREE(pCtx, psaBlobsNotInRepo);

}

static void _store_one_frag(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_repo_tx_handle* ptx,
	const char* psz_frag)
{
	SG_vhash* pvh_frag = NULL;
	SG_dagfrag* pFrag = NULL;

	SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &pvh_frag, psz_frag)  );
	SG_ERR_CHECK(  SG_dagfrag__alloc__from_vhash(pCtx, &pFrag, pvh_frag)  );
	SG_VHASH_NULLFREE(pCtx, pvh_frag);

	SG_ERR_CHECK(  SG_repo__store_dagfrag(pCtx, pRepo, ptx, pFrag)  );

	/* fall through */

fail:
	return;
}

static void _check_one_frag(
	SG_context* pCtx,
	sg_staging* pMe,
	SG_bool bCheckConnectedness,
	SG_bool bCheckLeafDelta,
	SG_bool bListNewNodes,
	SG_vhash* pvh_status,
	const char* psz_frag)
{
	SG_vhash* pvh_frag = NULL;
	SG_dagfrag* pFrag = NULL;
	SG_uint32 iDagNum = 0;
	char buf_dagnum[SG_DAGNUM__BUF_MAX__DEC];				// this just has to be big enough for a "%d"; it is not a GID/HID/TID.
	SG_bool bConnected = SG_FALSE;
	SG_rbtree* prb_missing = NULL;
	SG_stringarray* psa_new_nodes = NULL;
	SG_rbtree* prb_new_leaves = NULL;
	SG_rbtree* prbExistingLeaves = NULL;

	SG_NULLARGCHECK_RETURN(pvh_status);
	SG_NULLARGCHECK_RETURN(psz_frag);

	/* TODO if we come up with a truly useful way of passing a hint back,
	 * do it here.  For example, would it be helpful to pass back the
	 * generation number of all the leaves in this dag?  For now, we
	 * just pass back the fringe. */

	SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &pvh_frag, psz_frag)  );
	SG_ERR_CHECK(  SG_dagfrag__alloc__from_vhash(pCtx, &pFrag, pvh_frag)  );
	SG_VHASH_NULLFREE(pCtx, pvh_frag);

	SG_ERR_CHECK(  SG_dagfrag__get_dagnum(pCtx, pFrag, &iDagNum)  );
	SG_ERR_CHECK(  SG_dagnum__to_sz__decimal(pCtx, iDagNum, buf_dagnum, sizeof(buf_dagnum))  );

	if (bCheckConnectedness || bCheckLeafDelta || bListNewNodes)
	{
		SG_ERR_CHECK(  SG_repo__check_dagfrag(pCtx, pMe->pRepo, pFrag, &bConnected, &prb_missing, &psa_new_nodes,
			(bCheckLeafDelta ? &prb_new_leaves : NULL))  );

		// Add the missing parent nodes that prevent the DAGs connecting (the fringe) to the status vhash.
		if (!bConnected)
			SG_ERR_CHECK(  _add_disconnected_dag_to_status(pCtx, prb_missing, pvh_status, buf_dagnum)  );
		else
		{
			SG_ERR_CHECK(  sg_sqlite__exec__va(pCtx, pMe->psql,
				"UPDATE frags SET connected = 1 WHERE dagnum = %d", iDagNum)  );
		}

		// Add the blobs that are new to us to the staging area's list of referenced blobs.
		if (psa_new_nodes)
		{
			const char* const* asz = NULL;
			SG_uint32 asz_count = 0;

			SG_ERR_CHECK(  SG_stringarray__sz_array_and_count(pCtx, psa_new_nodes, &asz, &asz_count)  );
			SG_ERR_CHECK(  _add_referenced_blobs(pCtx, pMe, asz, asz_count)  );


			/* If a list of new nodes was requested, add it here. */
			// TODO: when incoming/outgoing do something useful with non-version-control dags, this should change.
			if (bListNewNodes && (SG_DAGNUM__VERSION_CONTROL == iDagNum))
				SG_ERR_CHECK(  _add_new_nodes_to_status(pCtx, asz, asz_count, pvh_status, buf_dagnum)  );
		}
	}

	if (prb_new_leaves)
	{
		SG_uint32 existingCount, newCount;
		// Ian TODO: how much does this race condition matter?
		SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pMe->pRepo, iDagNum, &prbExistingLeaves)  );
		SG_ERR_CHECK(  SG_rbtree__count(pCtx, prbExistingLeaves, &existingCount)  );
		if (existingCount == 0)
			existingCount = 1; // The first commit into an empty dag is technically a new head, but shouldn't be treated that way.
		SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_new_leaves, &newCount)  );
		SG_ERR_CHECK(  _add_leafdelta_to_status(pCtx, pvh_status, buf_dagnum, newCount-existingCount)  );
	}

	/* fall through */

fail:
	SG_DAGFRAG_NULLFREE(pCtx, pFrag);
	SG_RBTREE_NULLFREE(pCtx, prb_missing);
	SG_STRINGARRAY_NULLFREE(pCtx, psa_new_nodes);
	SG_RBTREE_NULLFREE(pCtx, prb_new_leaves);
	SG_RBTREE_NULLFREE(pCtx, prbExistingLeaves);
}

static void _store_one_blob(
	SG_context* pCtx,
	sg_staging* pMe,
	SG_file* pFile,
    const char* psz_objectid,
	SG_byte* buf,
	SG_uint32 buf_size,
	SG_repo_tx_handle* ptx,
	const char* psz_hid,
	SG_blob_encoding encoding,
	const char* psz_hid_vcdiff_reference,
	SG_uint64 len_full,
	SG_uint64 len_encoded
	)
{
	SG_uint32 sofar = 0;
	SG_uint32 got = 0;
	SG_repo_store_blob_handle* pbh = NULL;

	SG_NULLARGCHECK_RETURN(ptx);

	SG_ERR_CHECK(  SG_repo__store_blob__begin(pCtx, pMe->pRepo, ptx, psz_objectid, encoding, psz_hid_vcdiff_reference, len_full, len_encoded, psz_hid, &pbh)  );

	while (sofar < (SG_uint32) len_encoded)
	{
		SG_uint32 want = 0;
		if ((len_encoded - sofar) > buf_size)
		{
			want = buf_size;
		}
		else
		{
			want = (SG_uint32) (len_encoded - sofar);
		}
		SG_ERR_CHECK(  SG_file__read(pCtx, pFile, want, buf, &got)  );
		SG_ERR_CHECK(  SG_repo__store_blob__chunk(pCtx, pMe->pRepo, pbh, got, buf, NULL)  );

		sofar += got;
	}

	SG_ERR_CHECK(  SG_repo__store_blob__end(pCtx, pMe->pRepo, ptx, &pbh, NULL)  );

	return;

fail:
	if (pbh)
		SG_ERR_IGNORE(  SG_repo__store_blob__abort(pCtx, pMe->pRepo, ptx, &pbh)  );
}

static void _open_db(
	SG_context* pCtx,
	const SG_pathname* pPath_staging_dir,
	sqlite3** ppsql
	)
{
	SG_pathname* pPath = NULL;
	SG_bool bExists = SG_FALSE;
	sqlite3* psql = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pPath_staging_dir, "pending.db")  );

	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath, &bExists, NULL, NULL)  );

	if (bExists)
	{
		SG_ERR_CHECK(  sg_sqlite__open__pathname(pCtx, pPath,&psql)  );
		sqlite3_busy_timeout(psql,MY_BUSY_TIMEOUT_MS);
	}
	else
	{
		SG_ERR_CHECK(  sg_sqlite__create__pathname(pCtx, pPath,&psql)  );
		sqlite3_busy_timeout(psql,MY_BUSY_TIMEOUT_MS);

		SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, "BEGIN TRANSACTION")  );

		SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, ("CREATE TABLE frags"
		                                        "   ("
		                                        "     dagnum INTEGER UNIQUE NOT NULL,"
												"     connected INTEGER NOT NULL,"
		                                        "     frag TEXT NOT NULL"
		                                        "   )"))  );

		SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, ("CREATE TABLE blobs_present"
		                                        "   ("
		                                        "     filename VARCHAR NOT NULL,"
		                                        "     offset INTEGER NOT NULL,"
		                                        "     encoding INTEGER NOT NULL,"
		                                        "     len_encoded INTEGER NOT NULL,"
		                                        "     len_full INTEGER NOT NULL,"
		                                        "     hid_vcdiff VARCHAR NULL,"
		                                        "     hid VARCHAR UNIQUE NOT NULL,"
		                                        "     objectid VARCHAR NULL"
		                                        "   )"))  );

		SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, ("CREATE TABLE blobs_referenced"
												"    ("
												"      hid VARCHAR UNIQUE NOT NULL,"
												"      is_changeset INTEGER NOT NULL, "
												"      data_blobs_known INTEGER NOT NULL "
												"    )"))  );

		// We don't need an indexes on hids because the unique constraints imply them.

		SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, "COMMIT TRANSACTION")  );
	}

	SG_PATHNAME_NULLFREE(pCtx, pPath);

	*ppsql = psql;

	return;
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

static void _store_all_blobs(
	SG_context* pCtx,
	sg_staging* pMe,
	SG_repo_tx_handle* ptx
	)
{
	sqlite3_stmt* pStmt = NULL;
	int rc;
	SG_byte* buf = NULL;
	SG_uint32 buf_size;

	SG_file* pFile = NULL;
	char* pszLastFile = NULL;
	SG_pathname* pPath_file = NULL;

	buf_size = SG_STREAMING_BUFFER_SIZE;
	SG_ERR_CHECK(  SG_alloc(pCtx, buf_size, 1, &buf)  );

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql,&pStmt,
		"SELECT hid, filename, offset, encoding, len_encoded, len_full, hid_vcdiff, objectid FROM blobs_present ORDER BY filename ASC, offset ASC")  );

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char* psz_hid = (const char*) sqlite3_column_text(pStmt, 0);
		const char* psz_filename = (const char*) sqlite3_column_text(pStmt, 1);
		SG_uint64 offset = sqlite3_column_int64(pStmt,2);
		SG_blob_encoding encoding = (SG_blob_encoding)sqlite3_column_int(pStmt,3);
		SG_uint64 len_encoded = sqlite3_column_int64(pStmt, 4);
		SG_uint64 len_full = sqlite3_column_int64(pStmt, 5);
		const char * psz_hid_vcdiff = (const char *)sqlite3_column_text(pStmt,6);
		const char * psz_objectid = (const char *)sqlite3_column_text(pStmt,7);

		if ( (!pFile) || (strcmp(pszLastFile, psz_filename) != 0) )
		{
			if (pFile)
			{
				SG_FILE_NULLCLOSE(pCtx, pFile);
				SG_NULLFREE(pCtx, pszLastFile);
			}

			SG_STRDUP(pCtx, psz_filename, &pszLastFile);
			SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_file, pMe->pPath, psz_filename)  );
			SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath_file, SG_FILE_RDONLY|SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile)  );
			SG_PATHNAME_NULLFREE(pCtx, pPath_file);
		}

		// We have to seek past the object header each time.
		SG_ERR_CHECK(  SG_file__seek(pCtx, pFile, offset)  );

		SG_ERR_CHECK(  _store_one_blob(
			pCtx,
			pMe,
			pFile,
            psz_objectid,
			buf,
			buf_size,
			ptx,
			psz_hid,
			encoding,
			psz_hid_vcdiff,
			len_full,
			len_encoded
			)  );

	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}

	/* fall thru */

fail:
	SG_NULLFREE(pCtx, buf);
	if (pStmt)
		SG_ERR_IGNORE(  sg_sqlite__nullfinalize(pCtx,&pStmt)  );
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_NULLFREE(pCtx, pszLastFile);
	SG_PATHNAME_NULLFREE(pCtx, pPath_file);
}

static void _check_all_frags(
	SG_context* pCtx,
	sg_staging* pMe,
	SG_bool bCheckConnectedness,
	SG_bool bCheckLeafDelta,
	SG_bool bListNewNodes,
	SG_vhash* pvh_status
	)
{
	sqlite3_stmt* pStmt = NULL;
	int rc;

	if (!bCheckLeafDelta && !bListNewNodes)
	{
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
			"SELECT frag FROM frags WHERE connected = 0")  );
	}
	else
	{
		// If the caller wants leaf delta counts or new nodes, we need to check all
		// dags, not just the disconnected ones.
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
			"SELECT frag FROM frags")  );
	}

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char* psz_frag = (const char*) sqlite3_column_text(pStmt, 0);

		SG_ERR_CHECK(  _check_one_frag(
			pCtx,
			pMe,
			bCheckConnectedness,
			bCheckLeafDelta,
			bListNewNodes,
			pvh_status,
			psz_frag
			)  );
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}

	/* fall thru */

fail:
	if (pStmt)
		SG_ERR_IGNORE(  sg_sqlite__nullfinalize(pCtx,&pStmt)  );
}

static void _store_all_frags(
	SG_context* pCtx,
	sg_staging* pMe,
	SG_repo_tx_handle* ptx
	)
{
	sqlite3_stmt* pStmt = NULL;
	int rc;


	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql,&pStmt, "SELECT frag FROM frags")  );

	while ((rc=sqlite3_step(pStmt)) == SQLITE_ROW)
	{
		const char* psz_frag = (const char*) sqlite3_column_text(pStmt, 0);

		SG_ERR_CHECK(  _store_one_frag(
			pCtx,
			pMe->pRepo,
			ptx,
			psz_frag
			)  );
	}
	if (rc != SQLITE_DONE)
	{
		SG_ERR_THROW(  SG_ERR_SQLITE(rc)  );
	}

	/* fall thru */

fail:
	if (pStmt)
		SG_ERR_IGNORE(  sg_sqlite__nullfinalize(pCtx,&pStmt)  );
}

static void _save_blob_info(
	SG_context* pCtx,
	sg_staging* pMe,
	const char* psz_filename,
	SG_uint64 offset,
	SG_uint64 len_encoded,
	const char* psz_hid,
	SG_blob_encoding encoding,
	const char* psz_hid_vcdiff_reference,
	const char* psz_objectid,
	SG_uint64 len_full)
{
	sqlite3_stmt* pStmt = NULL;

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx,pMe->psql,&pStmt,
		(
		"INSERT OR IGNORE INTO blobs_present "
		"(hid, filename, offset, encoding, len_encoded, len_full, hid_vcdiff, objectid) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
		) )  );

	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx,pStmt,1,psz_hid)  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx,pStmt,2,psz_filename)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx,pStmt,3,offset)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx,pStmt,4,encoding)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx,pStmt,5,len_encoded)  );
	SG_ERR_CHECK(  sg_sqlite__bind_int64(pCtx,pStmt,6,len_full)  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx,pStmt,7,psz_hid_vcdiff_reference)  );
	SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx,pStmt,8,psz_objectid)  );
	SG_ERR_CHECK(  sg_sqlite__step(pCtx,pStmt,SQLITE_DONE)  );

	/* fall through */
fail:
	SG_ERR_IGNORE(  sg_sqlite__finalize(pCtx,pStmt)  );
}

static void _save_frag(
	SG_context* pCtx,
	sg_staging* pMe,
	SG_vhash* pvh_frag
	)
{
	SG_uint32 iDagNum;
	sqlite3_stmt* pStmt = NULL;
	SG_vhash* pvh_prev_frag = NULL;
	SG_dagfrag* pFrag = NULL;
	SG_dagfrag* pNewFrag = NULL;
	SG_string* pstr = NULL;
	SG_vhash* pvh_combined_frag = NULL;

	SG_ERR_CHECK(  SG_dagfrag__alloc__from_vhash(pCtx, &pFrag, pvh_frag)  );
	SG_ERR_CHECK(  SG_dagfrag__get_dagnum(pCtx, pFrag, &iDagNum)  );

	SG_ERR_CHECK(  sg_sqlite__prepare(pCtx,pMe->psql,&pStmt, "SELECT frag FROM frags WHERE dagnum = ?")  );
	SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx,pStmt,1,iDagNum)  );

	if (sqlite3_step(pStmt) == SQLITE_ROW)
	{
		const char* psz_frag = (const char*) sqlite3_column_text(pStmt, 0);

		SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &pvh_prev_frag, psz_frag)  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
	}
	SG_ERR_CHECK(  sg_sqlite__nullfinalize(pCtx, &pStmt)  );

	if (pvh_prev_frag)
	{
		pNewFrag = pFrag;
		SG_ERR_CHECK(  SG_dagfrag__alloc__from_vhash(pCtx, &pFrag, pvh_prev_frag)  );
		SG_VHASH_NULLFREE(pCtx, pvh_prev_frag);
		SG_ERR_CHECK(  SG_dagfrag__eat_other_frag(pCtx, pNewFrag, &pFrag)  );

		SG_ERR_CHECK(  SG_dagfrag__to_vhash__shared(pCtx, pNewFrag, NULL, &pvh_combined_frag)  );
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
		SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh_combined_frag, pstr)  );
		SG_VHASH_NULLFREE(pCtx, pvh_combined_frag);
		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
										  "UPDATE frags SET frag = ? WHERE dagnum = ?")  );
		SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx,pStmt,1,SG_string__sz(pstr))  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx,pStmt,2,iDagNum)  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
	}
	else
	{
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
		SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh_frag, pstr)  );

		SG_ERR_CHECK(  sg_sqlite__prepare(pCtx, pMe->psql, &pStmt,
			"INSERT INTO frags (dagnum, connected, frag) VALUES (?, 0, ?)")  );
		SG_ERR_CHECK(  sg_sqlite__bind_int(pCtx, pStmt, 1, iDagNum)  );
		SG_ERR_CHECK(  sg_sqlite__bind_text(pCtx, pStmt, 2, SG_string__sz(pstr))  );
		SG_ERR_CHECK(  sg_sqlite__step(pCtx, pStmt, SQLITE_DONE)  );
	}

	/* fall through */
fail:
	SG_DAGFRAG_NULLFREE(pCtx, pFrag);
	SG_DAGFRAG_NULLFREE(pCtx, pNewFrag);
	SG_STRING_NULLFREE(pCtx, pstr);
	SG_ERR_IGNORE(  sg_sqlite__nullfinalize(pCtx,&pStmt)  );
}

static void _slurp__version_1(
	SG_context* pCtx,
	sg_staging* pMe,
	const char* psz_filename,
	SG_file* pfb
	)
{
	SG_vhash* pvh = NULL;
	const char* psz_op = NULL;
	SG_uint64 iPos = 0;

	SG_NULLARGCHECK_RETURN(psz_filename);
	SG_NULLARGCHECK_RETURN(pfb);

	while (1)
	{
		SG_vhash* pvh_frag = NULL;
		SG_uint64 offset_begin_record = 0;

		SG_ERR_CHECK(  SG_file__tell(pCtx, pfb, &offset_begin_record)  );
		SG_ERR_CHECK(  SG_fragball__read_object_header(pCtx, pfb, &pvh)  );
		if (!pvh)
		{
			break;
		}
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "op", &psz_op)  );
		if (0 == strcmp(psz_op, "frag"))
		{
			SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, "frag", &pvh_frag)  );
			SG_ERR_CHECK(  _save_frag(pCtx, pMe, pvh_frag)  );
		}
		else if (0 == strcmp(psz_op, "blob"))
		{
			SG_int64 i64;
			const char* psz_hid = NULL;
			SG_blob_encoding encoding = 0;
			const char* psz_hid_vcdiff_reference = NULL;
			const char* psz_objectid = NULL;
			SG_uint64 len_full  = 0;
			SG_uint64 len_encoded  = 0;

			// TODO: Should we check the blobs_referenced table to ensure this is a blob we actually want?
			//       The assumption is that a blob would never arrive here unless it was first returned by
			//       get_status, which means it's already in blobs_referenced.

			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "hid", &psz_hid)  );
			SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, "encoding", &i64)  );
			encoding = (SG_blob_encoding) i64;
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "vcdiff_ref", &psz_hid_vcdiff_reference)  );
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "objectid", &psz_objectid)  );
			SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, "len_full", &i64)  );
			len_full = (SG_uint64) i64;
			SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, "len_encoded", &i64)  );
			len_encoded = (SG_uint64) i64;
			SG_ERR_CHECK(  SG_file__tell(pCtx, pfb, &iPos)  );

			SG_ERR_CHECK(  _save_blob_info(pCtx, pMe, psz_filename, iPos, len_encoded, psz_hid, encoding, psz_hid_vcdiff_reference, psz_objectid, len_full)  );

			/* seek ahead to skip the blob */
			SG_ERR_CHECK(  SG_file__seek(pCtx, pfb, iPos + len_encoded)  );
		}
		else
		{
			SG_ERR_THROW(  SG_ERR_FRAGBALL_INVALID_OP  );
		}
		SG_VHASH_NULLFREE(pCtx, pvh);
	}

	/* fall through */

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);
}

static void _commit__version_1(
	SG_context* pCtx,
	sg_staging* pMe,
	SG_repo_tx_handle* pTx,
	SG_file* pfb
	)
{
	SG_vhash* pvh = NULL;
	const char* psz_op = NULL;
	SG_dagfrag* pFrag = NULL;
	SG_byte* buf = NULL;
	SG_uint32 buf_size = SG_STREAMING_BUFFER_SIZE;

	SG_ERR_CHECK(  SG_alloc(pCtx, buf_size, 1, &buf)  );

	while (1)
	{
		SG_vhash* pvh_frag = NULL;
		SG_uint64 offset_begin_record = 0;

		SG_ERR_CHECK(  SG_file__tell(pCtx, pfb, &offset_begin_record)  );
		SG_ERR_CHECK(  SG_fragball__read_object_header(pCtx, pfb, &pvh)  );
		if (!pvh)
		{
			break;
		}
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "op", &psz_op)  );
		if (0 == strcmp(psz_op, "frag"))
		{
			SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, "frag", &pvh_frag)  );
			SG_ERR_CHECK(  SG_dagfrag__alloc__from_vhash(pCtx, &pFrag, pvh_frag)  );
			SG_ERR_CHECK(  SG_repo__store_dagfrag(pCtx, pMe->pRepo, pTx, pFrag)  );
			pFrag = NULL;
		}
		else if (0 == strcmp(psz_op, "blob"))
		{
			SG_int64 i64;
			const char* psz_hid = NULL;
			const char* psz_objectid = NULL;
			SG_blob_encoding encoding = 0;
			const char* psz_hid_vcdiff_reference = NULL;
			SG_uint64 len_full  = 0;
			SG_uint64 len_encoded  = 0;

			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "hid", &psz_hid)  );
			SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, "encoding", &i64)  );
			encoding = (SG_blob_encoding) i64;
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "vcdiff_ref", &psz_hid_vcdiff_reference)  );
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "objectid", &psz_objectid)  );
			SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, "len_full", &i64)  );
			len_full = (SG_uint64) i64;
			SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, "len_encoded", &i64)  );
			len_encoded = (SG_uint64) i64;

			SG_ERR_CHECK(  _store_one_blob(
                        pCtx,
                        pMe,
                        pfb,
                        psz_objectid,
                        buf,
                        buf_size,
                        pTx,
                        psz_hid,
                        encoding,
                        psz_hid_vcdiff_reference,
                        len_full,
                        len_encoded)  );
		}
		else
		{
			SG_ERR_THROW(  SG_ERR_FRAGBALL_INVALID_OP  );
		}
		SG_VHASH_NULLFREE(pCtx, pvh);
	}

	/* fall through */

fail:
	SG_NULLFREE(pCtx, buf);
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_DAGFRAG_NULLFREE(pCtx, pFrag);
}

static void _zlib_inflate(SG_context* pCtx, SG_byte* pbSource, SG_uint32 len_source, SG_byte* pbDest, SG_uint32 len_dest)
{
	int zErr;
	SG_uint32 have;
	z_stream strm;

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	zErr = inflateInit(&strm);
	if (zErr != Z_OK)
		SG_ERR_THROW_RETURN(SG_ERR_ZLIB(zErr));

	strm.avail_in = len_source;
	strm.next_in = pbSource;

	/* run inflate() on input until output buffer not full */
	do {
		strm.avail_out = len_dest;
		strm.next_out = pbDest;
		zErr = inflate(&strm, Z_NO_FLUSH);
		SG_ASSERT(zErr != Z_STREAM_ERROR);  /* state not clobbered */
		switch (zErr) {
			case Z_NEED_DICT:
				zErr = Z_DATA_ERROR;     /* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				SG_ERR_THROW_RETURN(SG_ERR_ZLIB(zErr));
		}
		have = len_dest - strm.avail_out;
	} while (strm.avail_out == 0);

	/* clean up and return */
	(void)inflateEnd(&strm);
	if (zErr != Z_STREAM_END)
		SG_ERR_THROW_RETURN(SG_ERR_ZLIB(zErr));
}

static void _write_repo_descriptor_name(SG_context* pCtx, const SG_pathname* pPath_staging, const char* psz_repo_descriptor_name)
{
	SG_file * pFile = NULL;
	SG_pathname* pPath = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pPath_staging, REPO_FILENAME)  );
	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_WRONLY|SG_FILE_CREATE_NEW, 0644, &pFile)  );
	SG_ERR_CHECK(  SG_file__write(pCtx, pFile, 1+strlen(psz_repo_descriptor_name), (SG_byte*) psz_repo_descriptor_name, NULL)  );
	SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

	/* fallthru */

fail:
    SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

static void _read_repo_descriptor_name(SG_context* pCtx, const SG_pathname* pPath_staging, char** ppsz_repo_descriptor_name)
{
	SG_byte buf[1000];
	SG_uint32 got;
	SG_file * pFile = NULL;
	SG_pathname* pPath = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pPath_staging, REPO_FILENAME)  );
	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_RDONLY|SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile)  );
	SG_ERR_CHECK(  SG_file__read(pCtx, pFile, sizeof(buf), buf, &got)  );
	if (got == sizeof(buf))
		SG_ERR_THROW(SG_ERR_LIMIT_EXCEEDED);
	else
		SG_ERR_CHECK(  SG_strdup(pCtx, (const char*)buf, ppsz_repo_descriptor_name)  );

	SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

	/* fallthru */

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

static void _make_staging_area_path(SG_context* pCtx, const char* psz_name, SG_pathname** ppPath)
{
	SG_pathname* pPath_tempdir = NULL;
	SG_pathname* pPath_staging = NULL;

	/* TODO: Do we want staging areas to go in user temp?  Probably not.
	   Keep in mind this method is called for both push (by SG_server) and pull (by SG_pull). */
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__USER_TEMP_DIRECTORY(pCtx, &pPath_tempdir)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath_staging, pPath_tempdir, psz_name)  );

	*ppPath = pPath_staging;
	pPath_staging = NULL;

	/* fallthru */

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath_tempdir);
	SG_PATHNAME_NULLFREE(pCtx, pPath_staging);
}

///////////////////////////////////////////////////////////////////////////////

void SG_staging__create(SG_context* pCtx,
						const char* psz_repo_descriptor_name,
						char** ppsz_tid_staging, // An ID that uniquely identifies the staging area
						SG_staging** ppStaging)
{
	char* psz_staging_area_name = NULL;
	SG_pathname* pPath_staging = NULL;
	sg_staging* pMe = NULL;
	SG_staging* pStaging = NULL;

	SG_ERR_CHECK(  SG_tid__alloc(pCtx, &psz_staging_area_name)  );		// TODO consider SG_tid__alloc2(...)
	SG_ERR_CHECK(  _make_staging_area_path(pCtx, (const char*)psz_staging_area_name, &pPath_staging)  );

	SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath_staging)  );
	SG_ERR_CHECK(  _write_repo_descriptor_name(pCtx, pPath_staging, psz_repo_descriptor_name)  );

	/* TODO do we want to write anything else into this staging area directory?  Like a readme? */

	if (ppStaging)
	{
		SG_ERR_CHECK(  SG_alloc1(pCtx, pMe)  );
		pStaging = (SG_staging*)pMe;
		pMe->pPath = pPath_staging;
		pPath_staging = NULL;
		SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, psz_repo_descriptor_name, &pMe->pRepo)  );
		SG_ERR_CHECK(  _open_db(pCtx, pMe->pPath, &pMe->psql)  );
		*ppStaging = pStaging;
		pStaging = NULL;
	}

	SG_RETURN_AND_NULL(psz_staging_area_name, ppsz_tid_staging);

	/* fallthru */

fail:
	SG_STAGING_NULLFREE(pCtx, pStaging);
	SG_NULLFREE(pCtx, psz_staging_area_name);
	SG_PATHNAME_NULLFREE(pCtx, pPath_staging);
}

void SG_staging__open(SG_context* pCtx,
					  const char* psz_tid_staging,
					  SG_staging** ppStaging)
{
	SG_pathname* pPath_staging = NULL;
	sg_staging* pMe = NULL;
	SG_staging* pStaging = NULL;
	char* pszDescriptorName = NULL;

	SG_ERR_CHECK(  _make_staging_area_path(pCtx, psz_tid_staging, &pPath_staging)  );
#if defined(DEBUG)
	{
		// Ensure the staging path exists and that it's not a file.
		SG_bool staging_path_exists = SG_FALSE;
		SG_fsobj_type fsobj_type;
		SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath_staging, &staging_path_exists, &fsobj_type, NULL)  );
		SG_ASSERT(staging_path_exists);
		SG_ASSERT(fsobj_type != SG_FSOBJ_TYPE__REGULAR);
	}
#endif

	SG_ERR_CHECK(  SG_alloc1(pCtx, pMe)  );
	pStaging = (SG_staging*)pMe;

	pMe->pPath = pPath_staging;
	pPath_staging = NULL;

	SG_ERR_CHECK(  _read_repo_descriptor_name(pCtx, pMe->pPath, &pszDescriptorName)  );
	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, pszDescriptorName, &pMe->pRepo)  );

	SG_ERR_CHECK(  _open_db(pCtx, pMe->pPath, &pMe->psql)  );

	SG_RETURN_AND_NULL(pStaging, ppStaging);

	/* fallthru */

fail:
	SG_STAGING_NULLFREE(pCtx, pStaging);
	SG_NULLFREE(pCtx, pszDescriptorName);
	SG_PATHNAME_NULLFREE(pCtx, pPath_staging);
}

void SG_staging__cleanup(SG_context* pCtx,
						 SG_staging** ppStaging)
{
	sg_staging* pMe;

	SG_NULL_PP_CHECK_RETURN(ppStaging);
	pMe = (sg_staging*)*ppStaging;

	SG_ERR_CHECK(  sg_sqlite__close(pCtx, pMe->psql)  );
	pMe->psql = NULL;
	SG_ERR_CHECK(  SG_fsobj__rmdir_recursive__pathname(pCtx, pMe->pPath) );

	/* fall through */
fail:
	SG_STAGING_NULLFREE(pCtx, *ppStaging);
}

void SG_staging__cleanup__by_id(
	SG_context* pCtx,
	const char* pszPushId)
{
	SG_pathname* pPath = NULL;

	SG_ERR_CHECK(  _make_staging_area_path(pCtx, pszPushId, &pPath)  );
	SG_ERR_CHECK(  SG_fsobj__rmdir_recursive__pathname(pCtx, pPath) );

	/* fall through */
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

void SG_staging__get_pathname__from_id(SG_context* pCtx, const char* pszPushId, SG_pathname** ppPathname)
{
	SG_pathname* pPath = NULL;

	SG_ERR_CHECK(  _make_staging_area_path(pCtx, pszPushId, &pPath)  );
	SG_RETURN_AND_NULL(pPath, ppPathname);

	/* fall through */
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

void SG_staging__slurp_fragball(
	SG_context* pCtx,
	SG_staging* pStaging,
	const char* psz_filename
	)
{
	SG_vhash* pvh_version = NULL;
	const char* psz_version = NULL;
	SG_uint32 version = 0;
	SG_file* pfb = NULL;
	SG_pathname* pPath = NULL;
	sg_staging* pMe = (sg_staging*)pStaging;
	SG_bool bInSqlTx = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pStaging);
	SG_NULLARGCHECK_RETURN(psz_filename);

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pMe->pPath, psz_filename)  );

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pfb)  );
	SG_PATHNAME_NULLFREE(pCtx, pPath);

	SG_ERR_CHECK(  SG_fragball__read_object_header(pCtx, pfb, &pvh_version)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_version, "version", &psz_version)  );
	version = atoi(psz_version);
	SG_VHASH_NULLFREE(pCtx, pvh_version);

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "BEGIN TRANSACTION")  );
	bInSqlTx = SG_TRUE;
	switch (version)
	{
		case 1:
			SG_ERR_CHECK(  _slurp__version_1(pCtx, pMe, psz_filename, pfb)  );
			break;

		default:
			SG_ERR_THROW(  SG_ERR_FRAGBALL_INVALID_VERSION  );
			break;
	}
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "COMMIT TRANSACTION")  );
	bInSqlTx = SG_FALSE;

	SG_ERR_CHECK(  SG_file__close(pCtx, &pfb)  );

	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_FILE_NULLCLOSE(pCtx, pfb);
	SG_VHASH_NULLFREE(pCtx, pvh_version);
	if (bInSqlTx)
		SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pMe->psql, "ROLLBACK TRANSACTION")  );
}

void SG_staging__commit(
	SG_context* pCtx,
	SG_staging* pStaging
	)
{
	SG_repo_tx_handle* ptx = NULL;
	sg_staging* pMe = (sg_staging*)pStaging;
	SG_bool bInRepoTx = SG_FALSE;
	SG_bool bInSqlTx = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pStaging);

	SG_ERR_CHECK(  SG_repo__begin_tx(pCtx, pMe->pRepo, &ptx)  );
	bInRepoTx = SG_TRUE;

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "BEGIN TRANSACTION")  );
	bInSqlTx = SG_TRUE;

	SG_ERR_CHECK(  SG_log_debug(pCtx, "Storing blobs from staging to repo")  );
	SG_ERR_CHECK(  _store_all_blobs(pCtx, pMe, ptx)  );

	SG_ERR_CHECK(  SG_log_debug(pCtx, "Storing dagnodes from staging to repo")  );
	SG_ERR_CHECK(  _store_all_frags(pCtx, pMe, ptx)  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "COMMIT TRANSACTION")  );
	bInSqlTx = SG_FALSE;

	SG_ERR_CHECK(  SG_log_debug(pCtx, "Staging repo__commit_tx begin")  );
	SG_ERR_CHECK(  SG_repo__commit_tx(pCtx, pMe->pRepo, &ptx)  );
	bInRepoTx = SG_FALSE;
	SG_ERR_CHECK(  SG_log_debug(pCtx, "Staging repo__commit_tx end")  );

	return;

fail:
	if (bInSqlTx)
		SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, pMe->psql, "ROLLBACK TRANSACTION")  );
	if (bInRepoTx)
		SG_ERR_IGNORE(  SG_repo__abort_tx(pCtx, pMe->pRepo, &ptx)  );
}

void SG_staging__commit_fragball(SG_context* pCtx, SG_staging* pStaging, const char* pszFragballName)
{
	SG_repo_tx_handle* ptx = NULL;
	sg_staging* pMe = (sg_staging*)pStaging;
	SG_bool bInRepoTx = SG_FALSE;
	SG_vhash* pvh_version = NULL;
	const char* psz_version = NULL;
	SG_uint32 version = 0;
	SG_pathname* pPathFragball = NULL;
	SG_file* pfb = NULL;

	SG_NULLARGCHECK_RETURN(pStaging);
	SG_NULLARGCHECK_RETURN(pszFragballName);

	SG_ERR_CHECK(  SG_repo__begin_tx(pCtx, pMe->pRepo, &ptx)  );
	bInRepoTx = SG_TRUE;

	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "Storing data to local repository...")  );
	SG_ERR_CHECK(  SG_log_debug(pCtx, "Storing fragball from staging to repo")  );

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFragball, pMe->pPath, pszFragballName)  );

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathFragball, SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pfb)  );
	SG_PATHNAME_NULLFREE(pCtx, pPathFragball);

	SG_ERR_CHECK(  SG_fragball__read_object_header(pCtx, pfb, &pvh_version)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_version, "version", &psz_version)  );
	version = atoi(psz_version);
	SG_VHASH_NULLFREE(pCtx, pvh_version);

	switch (version)
	{
	case 1:
		SG_ERR_CHECK(  _commit__version_1(pCtx, pMe, ptx, pfb)  );
		break;

	default:
		SG_ERR_THROW(  SG_ERR_FRAGBALL_INVALID_VERSION  );
		break;
	}
	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "done\n")  );

	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "Committing changes...")  );
	SG_ERR_CHECK(  SG_log_debug(pCtx, "Staging repo__commit_tx begin")  );
	SG_ERR_CHECK(  SG_repo__commit_tx(pCtx, pMe->pRepo, &ptx)  );
	bInRepoTx = SG_FALSE;
	SG_ERR_CHECK(  SG_log_debug(pCtx, "Staging repo__commit_tx end")  );
	SG_ERR_CHECK(  SG_context__msg__emit(pCtx, "done")  );

	/* fall through */
fail:
	if (bInRepoTx)
		SG_ERR_IGNORE(  SG_repo__abort_tx(pCtx, pMe->pRepo, &ptx)  );
	SG_VHASH_NULLFREE(pCtx, pvh_version);
	SG_PATHNAME_NULLFREE(pCtx, pPathFragball);
	SG_FILE_NULLCLOSE(pCtx, pfb);
	SG_ERR_IGNORE(  SG_context__msg__emit(pCtx, "\n")  );
}

void SG_staging__check_status(
	SG_context* pCtx,
	SG_staging* pStaging,
	SG_bool bCheckConnectedness,
	SG_bool bCheckLeafDelta,
	SG_bool bListNewNodes,
	SG_bool bCheckChangesetBlobs,
	SG_bool bCheckDataBlobs,
	SG_vhash** ppResult)
{
	SG_vhash* pvh_status = NULL;
	sg_staging* pMe = (sg_staging*)pStaging;

	SG_NULLARGCHECK_RETURN(pStaging);

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_status)  );

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "BEGIN TRANSACTION")  );
	SG_ERR_CHECK(  _check_all_frags(pCtx, pMe, bCheckConnectedness, bCheckLeafDelta, bListNewNodes, pvh_status)  );
	if (bCheckChangesetBlobs || bCheckDataBlobs)
		SG_ERR_CHECK(  _check_blobs(pCtx, pMe, pvh_status, bCheckChangesetBlobs, bCheckDataBlobs)  );
	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, pMe->psql, "COMMIT TRANSACTION")  );

	/* TODO lots more checks here */

	*ppResult = pvh_status;
	pvh_status = NULL;

	/* fall through */

fail:
	SG_VHASH_NULLFREE(pCtx, pvh_status);
}

void SG_staging__fetch_blob_into_memory(SG_context* pCtx,
										SG_staging_blob_handle* pStagingBlobHandle,
										SG_byte** ppBuf,
										SG_uint32* pLenFull)
{
	sg_staging_blob_handle* pbh = (sg_staging_blob_handle*)pStagingBlobHandle;
	SG_byte* pBufFull = NULL;
	SG_byte* pBufCompressed = NULL;
	SG_file* pFile = NULL;
	SG_uint32 len_got;

	SG_NULLARGCHECK_RETURN(pStagingBlobHandle);
	SG_NULLARGCHECK_RETURN(ppBuf);

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pbh->pFragballPathname, SG_FILE_RDONLY|SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile)  );
	SG_ERR_CHECK(  SG_file__seek(pCtx, pFile, pbh->offset)  );

	switch(pbh->encoding)
	{
		case SG_BLOBENCODING__ALWAYSFULL:
		case SG_BLOBENCODING__FULL:
		{
			SG_allocN(pCtx, (SG_uint32)pbh->len_full + 1, pBufFull);

			SG_ERR_CHECK(  SG_file__read(pCtx, pFile, (SG_uint32)pbh->len_full, pBufFull, &len_got)  );
			if (len_got != pbh->len_full)
				SG_ERR_THROW(SG_ERR_INCOMPLETEREAD);

			break;
		}

		case SG_BLOBENCODING__ZLIB:
		{

			SG_allocN(pCtx, (SG_uint32)pbh->len_full + 1, pBufFull);
			SG_allocN(pCtx, (SG_uint32)pbh->len_encoded + 1, pBufCompressed);

			SG_ERR_CHECK(  SG_file__read(pCtx, pFile, (SG_uint32)pbh->len_encoded, pBufCompressed, &len_got)  );
			if (len_got != pbh->len_encoded)
				SG_ERR_THROW(SG_ERR_INCOMPLETEREAD);

			SG_ERR_CHECK(  _zlib_inflate(pCtx, pBufCompressed, (SG_uint32)pbh->len_encoded, pBufFull, (SG_uint32)pbh->len_full)  );
			SG_NULLFREE(pCtx, pBufCompressed);

			break;
		}

		case SG_BLOBENCODING__VCDIFF:
			/* Ian TODO: handle vcdiff encoding in staging */
			SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED);
			break;

		default:
			SG_ERR_THROW2(SG_ERR_INVALIDARG, (pCtx, "Unknown blob encoding: %s", pbh->encoding));
	}

	SG_FILE_NULLCLOSE(pCtx, pFile);

	if (pLenFull)
		*pLenFull = (SG_uint32)pbh->len_full;
	*ppBuf = pBufFull;

	return;

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_NULLFREE(pCtx, pBufFull);
	SG_NULLFREE(pCtx, pBufCompressed);
}

void SG_staging__get_pathname(SG_context* pCtx, SG_staging* pStaging, const SG_pathname** ppPathname)
{
	SG_NULLARGCHECK_RETURN(pStaging);
	SG_NULLARGCHECK_RETURN(ppPathname);

	*ppPathname = ((sg_staging*)pStaging)->pPath;
}
