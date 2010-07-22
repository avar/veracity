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

struct _SG_pendingdb
{
    SG_uint32 iDagNum;
    SG_repo* pRepo;
    SG_rbtree* prbParents;
    SG_rbtree* prb_records_remove;
    SG_rbtree* prb_records_add;
    SG_rbtree* prbAttachments_buffers;
    SG_rbtree* prbAttachments_files;
    SG_vhash* pvh_template;
    char buf_hid_template[SG_HID_MAX_BUFFER_LENGTH];
    char buf_hid_baseline[SG_HID_MAX_BUFFER_LENGTH];
};

void SG_pendingdb__alloc(
    SG_context* pCtx,
	SG_repo* pRepo,
    SG_uint32 iDagNum,
    const char* psz_hid_baseline,
    const char* psz_hid_template,
	SG_pendingdb** ppThis
	)
{
    SG_pendingdb* pThis = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pThis)  );

    if (psz_hid_baseline)
    {
        SG_ERR_CHECK(  SG_strcpy(pCtx, pThis->buf_hid_baseline, sizeof(pThis->buf_hid_baseline), psz_hid_baseline)  );
    }

    pThis->iDagNum = iDagNum;
    if (psz_hid_template)
    {
        SG_ERR_CHECK(  SG_strcpy(pCtx, pThis->buf_hid_template, sizeof(pThis->buf_hid_template), psz_hid_template)  );
    }

    /* TODO why does this code open the repo again? */
	SG_ERR_CHECK(  SG_repo__open_repo_instance__copy(pCtx, pRepo, &pThis->pRepo)  );

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pThis->prbParents)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pThis->prb_records_add)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pThis->prb_records_remove)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pThis->prbAttachments_buffers)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pThis->prbAttachments_files)  );

    *ppThis = pThis;
    pThis = NULL;

fail:
    SG_PENDINGDB_NULLFREE(pCtx, pThis);
}

void SG_pendingdb__free(SG_context* pCtx, SG_pendingdb* pThis)
{
	if (!pThis)
		return;

    SG_REPO_NULLFREE(pCtx, pThis->pRepo);
    SG_RBTREE_NULLFREE(pCtx, pThis->prbAttachments_buffers);
    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pThis->prbAttachments_files, (SG_free_callback*) SG_pathname__free);
    SG_RBTREE_NULLFREE(pCtx, pThis->prb_records_add);
    SG_RBTREE_NULLFREE(pCtx, pThis->prb_records_remove);
    SG_RBTREE_NULLFREE(pCtx, pThis->prbParents);
    SG_NULLFREE(pCtx, pThis);
}

void SG_pendingdb__add(
    SG_context* pCtx,
	SG_pendingdb* pPendingDb,
    SG_dbrecord** ppRecord
    )
{
    const char* psz_hid_rec = NULL;

    SG_ERR_CHECK(  SG_dbrecord__freeze(pCtx, *ppRecord, pPendingDb->pRepo, &psz_hid_rec)  );
    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pPendingDb->prb_records_add, psz_hid_rec, *ppRecord)  );
    *ppRecord = NULL;

fail:
    return;
}

void SG_pendingdb__add_attachment__from_pathname(
    SG_context* pCtx,
	SG_pendingdb * pPendingDb,
    SG_pathname* pPath,
    char** ppsz_hid
    )
{
    char* psz_hid = NULL;
    SG_pathname* pOldPath = NULL;
    SG_file* pFile = NULL;
    SG_pathname* pMyPath = NULL;

	SG_NULLARGCHECK_RETURN(pPath);
	SG_NULLARGCHECK_RETURN(pPendingDb);
	SG_NULLARGCHECK_RETURN(ppsz_hid);

    // calc the hid
    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile)  );
    SG_ERR_CHECK(  SG_repo__alloc_compute_hash__from_file(pCtx, pPendingDb->pRepo, pFile, &psz_hid)  );
    SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

    SG_ERR_CHECK(  SG_pathname__alloc__copy(pCtx, &pMyPath, pPath)  );
    SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, pPendingDb->prbAttachments_files, psz_hid, pMyPath, (void**) &pOldPath)  );
    pMyPath = NULL;

    SG_PATHNAME_NULLFREE(pCtx, pOldPath);

    *ppsz_hid = psz_hid;
    psz_hid = NULL;

    // fall thru

fail:
    SG_PATHNAME_NULLFREE(pCtx, pMyPath);
    SG_NULLFREE(pCtx, psz_hid);
    SG_FILE_NULLCLOSE(pCtx, pFile);
}

void SG_pendingdb__add_attachment__from_memory(
    SG_context* pCtx,
	SG_pendingdb * pPendingDb,
    void* pBytes,
    SG_uint32 len,
    const char** ppsz_hid
    )
{
    SG_ERR_CHECK_RETURN(  SG_rbtree__memoryblob__add__hid(pCtx, pPendingDb->prbAttachments_buffers, pPendingDb->pRepo, pBytes, len, ppsz_hid)  );
}

void SG_pendingdb__remove(
    SG_context* pCtx,
	SG_pendingdb * pPendingDb,
    const char* psz_hid_record
    )
{
    SG_ERR_CHECK_RETURN(  SG_rbtree__add(pCtx, pPendingDb->prb_records_remove, psz_hid_record)  );
}

void SG_pendingdb__add_parent(
    SG_context* pCtx,
	SG_pendingdb* pPendingDb,
    const char* psz_hid_cs_parent
    )
{
    if (psz_hid_cs_parent)
    {
        SG_ERR_CHECK_RETURN(  SG_rbtree__add(pCtx, pPendingDb->prbParents, psz_hid_cs_parent)  );
    }
}

void SG_pendingdb__set_template(
	SG_context* pCtx,
	SG_pendingdb* pPendingDb,
    SG_vhash* pvh
    )
{
	SG_NULLARGCHECK_RETURN(pPendingDb);
	SG_NULLARGCHECK_RETURN(pvh);

    pPendingDb->pvh_template = pvh;
}

void SG_pendingdb__commit(
	SG_context* pCtx,
	SG_pendingdb* pPendingDb,
    const SG_audit* pq,
    SG_changeset** ppcs,
    SG_dagnode** ppdn
    )
{
    SG_committing* ptx = NULL;
    SG_rbtree* prb_both_add_and_remove = NULL;
    SG_rbtree_iterator* pit = NULL;
    const char* psz_hid_cs_parent = NULL;
    const char* psz_hid_att = NULL;
    SG_pathname* pPath_att = NULL;
    SG_bool b = SG_FALSE;
    SG_changeset* pcs = NULL;
    SG_dagnode* pdn = NULL;
    char* psz_hid_delta = NULL;
    SG_dbrecord* prec = NULL;
    const char* psz_hid_record = NULL;
    SG_byte* pPacked = NULL;
    char* pszActualHid = NULL;
    SG_file* pFile_att = NULL;
    SG_vhash* pvh_delta = NULL;
    SG_varray* pva_records_add = NULL;
    SG_varray* pva_records_remove = NULL;
    SG_string* pstr = NULL;
    char* psz_hid_template = NULL;

	SG_NULLARGCHECK_RETURN(pPendingDb);

    SG_ERR_CHECK(  SG_committing__alloc(pCtx, &ptx, pPendingDb->pRepo, pPendingDb->iDagNum, pq, CSET_VERSION_1)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pPendingDb->prbParents, &b, &psz_hid_cs_parent, NULL)  );
    while (b)
    {
        SG_ERR_CHECK(  SG_committing__add_parent(pCtx, ptx, psz_hid_cs_parent)  );
        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_cs_parent, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh_delta)  );
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_delta, "ver", "1")  ); // TODO
    if (pPendingDb->buf_hid_baseline[0])
    {
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_delta, "baseline", pPendingDb->buf_hid_baseline)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pvh_delta, "baseline")  );
    }
    SG_ERR_CHECK(  SG_vhash__addnew__varray(pCtx, pvh_delta, "add", &pva_records_add)  );
    SG_ERR_CHECK(  SG_vhash__addnew__varray(pCtx, pvh_delta, "remove", &pva_records_remove)  );

    SG_ERR_CHECK(  SG_rbtree__alloc(pCtx, &prb_both_add_and_remove)  );
	SG_ERR_CHECK(  SG_rbtree__compare__keys_only(
                pCtx,
                pPendingDb->prb_records_add,
                pPendingDb->prb_records_remove,
                NULL,
                NULL,
                NULL,
                prb_both_add_and_remove
                )  );

    // store the records
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pPendingDb->prb_records_add, &b, NULL, (void**) &prec)  );
    while (b)
    {
        SG_bool b_both = SG_FALSE;

        SG_ERR_CHECK(  SG_committing__add_dbrecord(pCtx, ptx, prec)  );
        SG_ERR_CHECK(  SG_dbrecord__get_hid__ref(pCtx, prec, &psz_hid_record)  );
        /* TODO we COULD verify that the new hid matches the rbtree's key */
        SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_both_add_and_remove, psz_hid_record, &b_both, NULL)  );

        if (!b_both)
        {
            SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_records_add, psz_hid_record)  );
        }

        SG_DBRECORD_NULLFREE(pCtx, prec);
        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, NULL, (void**) &prec)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // now the removes
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pPendingDb->prb_records_remove, &b, &psz_hid_record, NULL)  );
    while (b)
    {
        SG_bool b_both = SG_FALSE;

        SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_both_add_and_remove, psz_hid_record, &b_both, NULL)  );

        if (!b_both)
        {
            SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_records_remove, psz_hid_record)  );
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_record, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // now the attachments
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pPendingDb->prbAttachments_files, &b, &psz_hid_att, (void**) &pPath_att)  );
    while (b)
    {
        SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath_att, SG_FILE_LOCK | SG_FILE_RDONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile_att)  );

        /* TODO fix the b_dont_bother flag below according to the filename of this item? */
        SG_ERR_CHECK(  SG_committing__add_file(
                    pCtx,
                    ptx,
                    NULL, // TODO we don't really have an id to use here
                    SG_FALSE,
                    pFile_att,
                    SG_BLOB_REFTYPE__DBUSERFILE,
                    &pszActualHid)  );

        SG_FILE_NULLCLOSE(pCtx, pFile_att);
        /* TODO should we check to make sure actual hid is what it should be ? */
        SG_NULLFREE(pCtx, pszActualHid);

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_att, (void**) &pPath_att)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pPendingDb->prbAttachments_buffers, &b, NULL, (void**) &pPacked)  );
    while (b)
    {
        SG_uint32 len;
        const SG_byte* pData = NULL;

        SG_ERR_CHECK(  SG_memoryblob__unpack(pCtx, pPacked, &pData, &len)  );
        SG_ERR_CHECK(  SG_committing__add_bytes__buflen(
                    pCtx,
                    ptx,
                    NULL, // TODO we don't really have an id to use here
                    pData,
                    len,
                    SG_BLOB_REFTYPE__DBUSERFILE,
                    &pszActualHid)  );
        /* TODO should we check to make sure actual hid is what it should be ? */
        SG_NULLFREE(pCtx, pszActualHid);

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, NULL, (void**) &prec)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    if (pPendingDb->pvh_template)
    {
        SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
        SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pPendingDb->pvh_template, pstr)  );
        SG_ERR_CHECK(  SG_committing__add_bytes__buflen(
                    pCtx,
                    ptx,
                    NULL, // TODO we don't really have an id to use here
                    (SG_byte*) SG_string__sz(pstr),
                    SG_string__length_in_bytes(pstr),
                    SG_BLOB_REFTYPE__DBTEMPLATE,
                    &psz_hid_template)  );
        SG_STRING_NULLFREE(pCtx, pstr);
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_delta, SG_ZING_DBDELTA__TEMPLATE, psz_hid_template)  );
        SG_NULLFREE(pCtx, psz_hid_template);
    }
    else
    {
        SG_ASSERT(pPendingDb->buf_hid_template[0]);
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_delta, SG_ZING_DBDELTA__TEMPLATE, pPendingDb->buf_hid_template)  );
    }

	SG_ERR_CHECK(  SG_vhash__sort(pCtx, pvh_delta, SG_TRUE, SG_vhash_sort_callback__increasing)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh_delta, pstr)  );
    SG_ERR_CHECK(  SG_committing__add_bytes__buflen(
                pCtx,
                ptx,
                NULL,
                (SG_byte*) SG_string__sz(pstr),
                SG_string__length_in_bytes(pstr),
                SG_BLOB_REFTYPE__DBINFO,
                &psz_hid_delta)  );

    SG_ERR_CHECK(  SG_committing__set_root(pCtx, ptx, psz_hid_delta)  );

    SG_NULLFREE(pCtx, psz_hid_delta);
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_VHASH_NULLFREE(pCtx, pvh_delta);

    SG_ERR_CHECK(  SG_committing__end(pCtx, ptx, &pcs, &pdn)  );
    ptx = NULL;

    SG_RETURN_AND_NULL(pcs, ppcs);
    SG_RETURN_AND_NULL(pdn, ppdn);

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_both_add_and_remove);
    SG_NULLFREE(pCtx, psz_hid_template);
    SG_VHASH_NULLFREE(pCtx, pvh_delta);
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    if (ptx)
    {
        SG_ERR_IGNORE(  SG_committing__abort(pCtx, ptx)  );
    }
}


