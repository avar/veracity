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
* @file sg_repo_sqlite_tx.c
*
* @details SQLite repo transaction management.
*/

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_repo_sqlite__private.h"

#define INITIAL_VECTOR_SIZE 5

//////////////////////////////////////////////////////////////////

static void _sg_repo_tx_sqlite_blob_handle_free(SG_context * pCtx, void * pVoidData)
{
	sg_repo_tx_sqlite_blob_handle* ptxbh = (sg_repo_tx_sqlite_blob_handle*)pVoidData;

	if(!ptxbh)
		return;

	SG_NULLFREE(pCtx, ptxbh->paRowIDs);
	SG_NULLFREE(pCtx, ptxbh->psz_hid_storing);
	SG_NULLFREE(pCtx, ptxbh->psz_hid_vcdiff_reference);
	SG_NULLFREE(pCtx, ptxbh);
}

static void _sg_repo_sqlite__tx_handle_nullfree(SG_context * pCtx, sg_repo_tx_sqlite_handle** ppTx)
{
	if (!ppTx || !(*ppTx))
		return;

    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, (*ppTx)->prb_frags, (SG_free_callback *)SG_dagfrag__free);
	SG_VECTOR_NULLFREE_WITH_ASSOC(pCtx, (*ppTx)->pv_tx_bh, _sg_repo_tx_sqlite_blob_handle_free);
	SG_NULLFREE(pCtx, *ppTx);
}

static void _delete_blobs_for_rollback(SG_context* pCtx, sqlite3* psql, SG_vector* pv)
{
	SG_uint32 iNumStoreHandles;
	SG_uint32 i;
	sg_repo_tx_sqlite_blob_handle* ptxbh = NULL;

	if (!pv)
		return;

	SG_ERR_CHECK(  SG_vector__length(pCtx, pv, &iNumStoreHandles)  );
	for (i=0; i < iNumStoreHandles; i++)
	{
		SG_ERR_CHECK(  SG_vector__get(pCtx, pv, i, (void**) &ptxbh)  );
		if (ptxbh)
		{
			if (ptxbh->iRowIDCount)
				SG_ERR_CHECK(  sg_blob_sqlite__delete(pCtx, psql, ptxbh->paRowIDs, ptxbh->iRowIDCount)  );
		}
	}

	return;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

void sg_repo_tx__sqlite__begin(SG_context* pCtx,
							   my_instance_data * pData,
							   sg_repo_tx_sqlite_handle** ppTx)
{
	sg_repo_tx_sqlite_handle* pTx;

	SG_NULLARGCHECK_RETURN(pData);

	SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(sg_repo_tx_sqlite_handle), &pTx)  );
	pTx->pData = pData;
	SG_ERR_CHECK(  SG_vector__alloc(pCtx, &pTx->pv_tx_bh, INITIAL_VECTOR_SIZE)  );

	*ppTx = pTx;

	return;

fail:
	SG_ERR_IGNORE(  _sg_repo_sqlite__tx_handle_nullfree(pCtx, &pTx)  );
	ppTx = NULL;
}

static void sg_sqlite__get_dbndx__cb(
        SG_context* pCtx,
        void * pVoidData,
        SG_uint32 iDagNum,
        SG_dbndx** ppndx
        )
{
    SG_repo* pRepo = (SG_repo*) pVoidData;

    SG_ERR_CHECK_RETURN(  _sqlite_my_get_dbndx(pCtx, pRepo, iDagNum, SG_FALSE, ppndx)  );
}

static void sg_sqlite__get_treendx__cb(
        SG_context* pCtx,
        void * pVoidData,
        SG_uint32 iDagNum,
        SG_treendx** ppndx
        )
{
    SG_repo* pRepo = (SG_repo*) pVoidData;

    SG_ERR_CHECK_RETURN(  _sqlite_my_get_treendx(pCtx, pRepo, iDagNum, SG_FALSE, ppndx)  );
}

void sg_repo_tx__sqlite__commit(SG_context* pCtx,
								SG_repo* pRepo,
								sqlite3* psql,
								sg_repo_tx_sqlite_handle** ppTx)
{
	SG_vector* pvStoreHandles = NULL;
	sg_repo_tx_sqlite_handle* pTx = NULL;
	sg_repo_tx_sqlite_blob_handle* ptxbh = NULL;
	SG_uint32 iNumStoreHandles;
	SG_uint32 i;
	SG_rbtree* prb_missing = NULL;
	SG_stringarray* psa_new = NULL;
	SG_bool bRepoTxDone = SG_FALSE;
    SG_rbtree* prb_new_dagnodes = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_dagnode* pdn = NULL;
    SG_vhash* pvh_audits = NULL;

	SG_NULLARGCHECK_RETURN(psql);
	SG_NULL_PP_CHECK_RETURN(ppTx);

	pTx = *ppTx;
	pvStoreHandles = pTx->pv_tx_bh;

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, "BEGIN IMMEDIATE TRANSACTION")  );

	// Write data to make this transaction's blobs visible.
	SG_ERR_CHECK(  SG_vector__length(pCtx, pvStoreHandles, &iNumStoreHandles)  );
	if (iNumStoreHandles && pTx->bLastBlobRemoved)
		iNumStoreHandles--;

	for (i=0; i < iNumStoreHandles; i++)
	{
		SG_ERR_CHECK(  SG_vector__get(pCtx, pvStoreHandles, i, (void**) &ptxbh)  );
		if (ptxbh->psz_hid_storing)
		{
			if (ptxbh->b_changing_encoding)
			{
				SG_ERR_CHECK(  sg_blob_sqlite__update_info(pCtx, psql, ptxbh->paRowIDs, ptxbh->iRowIDCount,
					ptxbh->psz_hid_storing, ptxbh->blob_encoding_storing, ptxbh->len_encoded,
					ptxbh->len_full, ptxbh->psz_hid_vcdiff_reference)  );
			}
			else
			{
				sg_blob_sqlite__add_info(pCtx, psql, ptxbh);
				if (SG_context__err_equals(pCtx, SG_ERR_BLOBFILEALREADYEXISTS))
					SG_context__err_reset(pCtx);
				else
					SG_ERR_CHECK_CURRENT;

				// Ian TODO: delete blob if it already exists.
			}
		}
		else
		{
			// Incomplete blob.
			SG_ERR_THROW(SG_ERR_INCOMPLETE_BLOB_IN_REPO_TX);
		}
	}

    /* store the frags */
    if (pTx->prb_frags)
    {
        SG_dagfrag* pFrag = NULL;
        const char* psz_dagnum = NULL;
        SG_bool b = SG_FALSE;

        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_new_dagnodes)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pTx->prb_frags, &b, &psz_dagnum, (void**) &pFrag)  );
        while (b)
        {
            SG_uint32 iDagNum = 0;

            SG_ERR_CHECK(  SG_dagnum__from_sz__decimal(pCtx, psz_dagnum, &iDagNum)  );

            SG_ERR_CHECK(  SG_dag_sqlite3__insert_frag(pCtx, psql, iDagNum, pFrag, &prb_missing, &psa_new)  );
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

	SG_ERR_CHECK(  sg_sqlite__exec(pCtx, psql, "COMMIT TRANSACTION")  );
	bRepoTxDone = SG_TRUE;

    /* the commit is complete.  next we want to just update any dbndxes.
     * but if this update fails, we cannot and should not try to rollback
     * the commit itself, since it has already succeeded. */

    if (prb_new_dagnodes)
    {
        SG_ERR_CHECK(  sg_repo__update_ndx(pCtx, pRepo, prb_new_dagnodes,
                    sg_sqlite__get_dbndx__cb, pRepo,
                    sg_sqlite__get_treendx__cb, pRepo)  );
    }

	SG_ERR_IGNORE(  _sg_repo_sqlite__tx_handle_nullfree(pCtx, ppTx)  );

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_audits);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_RBTREE_NULLFREE(pCtx, prb_missing);
    SG_RBTREE_NULLFREE(pCtx, prb_new_dagnodes);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
	if (!bRepoTxDone)
	{
		// Delete blobs.
		SG_ERR_IGNORE(  _delete_blobs_for_rollback(pCtx, psql, (*ppTx)->pv_tx_bh)  );

		SG_ERR_IGNORE(  _sg_repo_sqlite__tx_handle_nullfree(pCtx, ppTx)  );
		SG_ERR_IGNORE(  sg_sqlite__exec(pCtx, psql, "ROLLBACK TRANSACTION")  );
	}

	SG_STRINGARRAY_NULLFREE(pCtx, psa_new);
}

void sg_repo_tx__sqlite__abort(SG_context* pCtx,
							   sqlite3* psql,
							   sg_repo_tx_sqlite_handle** ppTx,
							   sg_blob_sqlite_handle_store** pbh)
{
	SG_NULLARGCHECK_RETURN(psql);
	SG_NULL_PP_CHECK_RETURN(ppTx);

	if (pbh && *pbh)
		sg_blob_sqlite__store_blob__abort(pCtx, *ppTx, pbh);

	(void) _delete_blobs_for_rollback(pCtx, psql, (*ppTx)->pv_tx_bh);

	SG_ERR_IGNORE(  _sg_repo_sqlite__tx_handle_nullfree(pCtx, ppTx)  );
}

void sg_repo_tx__sqlite__add_blob(SG_context* pCtx,
								  sg_repo_tx_sqlite_handle* pTx,
								  sg_blob_sqlite_handle_store* pbh)
{
	sg_repo_tx_sqlite_blob_handle* ptxbh = NULL;
	SG_uint32 iLen;

	SG_NULLARGCHECK_RETURN(pTx);
	SG_NULLARGCHECK_RETURN(pbh);

	SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(sg_repo_tx_sqlite_blob_handle), &ptxbh)  );
	pbh->ptxbh = ptxbh;

	if (pTx->bLastBlobRemoved)
	{
		// The last blob was removed, so put this one in its place.
		SG_ERR_CHECK(  SG_vector__length(pCtx, pTx->pv_tx_bh, &iLen)  );
		SG_ERR_CHECK(  SG_vector__set(pCtx, pTx->pv_tx_bh, iLen-1, ptxbh)  );
		pTx->bLastBlobRemoved = SG_FALSE;
	}
	else
		SG_ERR_CHECK(  SG_vector__append(pCtx, pTx->pv_tx_bh, ptxbh, NULL)  );

	return;

fail:
	return;
}

void sg_repo_tx__sqlite__remove_last_blob(SG_context* pCtx, sg_repo_tx_sqlite_handle* pTx)
{
	SG_uint32 iLen;
	sg_repo_tx_sqlite_blob_handle* ptxbh = NULL;

	SG_NULLARGCHECK_RETURN(pTx);

	pTx->bLastBlobRemoved = SG_TRUE;

	SG_ERR_CHECK(  SG_vector__length(pCtx, pTx->pv_tx_bh, &iLen)  );
	SG_ERR_CHECK(  SG_vector__get(pCtx, pTx->pv_tx_bh, iLen-1, (void**) &ptxbh)  );
	_sg_repo_tx_sqlite_blob_handle_free(pCtx, ptxbh);
	SG_ERR_CHECK(  SG_vector__set(pCtx, pTx->pv_tx_bh, iLen-1, NULL)  );

	return;

fail:
	return;
}

void sg_repo_tx__sqlite_blob_handle__alloc(SG_context* pCtx,
										   sg_repo_tx_sqlite_handle* pTx,
										   sg_repo_tx_sqlite_blob_handle** ppTxbh)
{
	SG_NULLARGCHECK_RETURN(pTx);

	SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(sg_repo_tx_sqlite_blob_handle), ppTxbh)  );
//	(*ppTxbh)->pData = pTx->pData;
	SG_ERR_CHECK(  SG_vector__append(pCtx, pTx->pv_tx_bh, *ppTxbh, NULL)  );

	return;

fail:
	return;
}
