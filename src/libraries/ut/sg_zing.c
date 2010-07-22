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

// TODO temporary hack:
#ifdef WINDOWS
//#pragma warning(disable:4100)
#pragma warning(disable:4706)
#endif

struct SG_zingrecord
{
    SG_zingtx* pztx;
    SG_rbtree* prb_field_values;
    SG_rbtree* prb_attachments;
    SG_bool b_dirty_fields;
    SG_bool b_dirty_links;
    SG_bool bDeleteMe;
    SG_varray* pva_history;
    char* psz_original_hid;
};

struct sg_link_dep_info
{
    SG_zingtx* pztx;
    char buf_link_name[1 + SG_ZING_MAX_FIELD_NAME_LENGTH];

    SG_rbtree* prb_actions;

    SG_rbtree* prb_to;
};

struct sg_froms
{
    SG_rbtree* prb_froms;
    SG_bool bDirty;
};

static void sg_zing__query__template(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        SG_zingtemplate* pzt,
        const char* psz_csid,
        const char* psz_rectype,
        const char* psz_where,
        const char* psz_sort,
        SG_int32 limit,
        SG_int32 skip,
        SG_stringarray* psa_slice_fields,
        SG_varray** ppva_sliced
        );

static void sg_zingtx__change_record(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zingrecord* pzrec,
        const char* psz_name,
        const char* psz_val
        );

static void SG_zingtx__find_all_records_linking_to(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid_to,
        const char* psz_link_name,
        SG_rbtree** pprb
        );

static void sg_zingtx__find_all_links_to_or_from(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid,
        SG_rbtree* prb_recidlist,
        const char* psz_link_name,
        SG_bool b_from,
        SG_rbtree** pprb
        );

void SG_zingtx__get_template(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zingtemplate** ppResult
        )
{
  SG_UNUSED(pCtx);

    *ppResult = pztx->ptemplate;
}

static void bc__actions__free(
        SG_UNUSED_PARAM(SG_context *pCtx),
        void * pVoidData
        )
{
	struct sg_actions* pa = (struct sg_actions*) pVoidData;

	SG_UNUSED(pCtx);

	if (!pa)
    {
		return;
    }

    SG_NULLFREE(pCtx, pa);
}

static void bc__froms__free(
        SG_UNUSED_PARAM(SG_context *pCtx),
        void * pVoidData
        )
{
	struct sg_froms* pf = (struct sg_froms*) pVoidData;

	SG_UNUSED(pCtx);

	if (!pf)
    {
		return;
    }

    SG_RBTREE_NULLFREE(pCtx, pf->prb_froms);
    SG_NULLFREE(pCtx, pf);
}

static void bc__link_dep_info__free(
        SG_UNUSED_PARAM(SG_context *pCtx),
        void * pVoidData
        )
{
	struct sg_link_dep_info* pldi = (struct sg_link_dep_info*) pVoidData;

	SG_UNUSED(pCtx);

	if (!pldi)
    {
		return;
    }

    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pldi->prb_actions, bc__actions__free);
    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pldi->prb_to, bc__froms__free);

	SG_NULLFREE(pCtx, pldi);
}

void _sg_zingtx__free(
        SG_context* pCtx,
        SG_zingtx* pztx
        )
{
    if (!pztx)
    {
        return;
    }

    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pztx->prb_dependencies, bc__link_dep_info__free);
    SG_ZINGTEMPLATE_NULLFREE(pCtx, pztx->ptemplate);
    SG_RBTREE_NULLFREE(pCtx, pztx->prb_rechecks);
    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pztx->prb_records, (SG_free_callback*) SG_zingrecord__free);
    SG_RBTREE_NULLFREE(pCtx, pztx->prb_parents);
    SG_RBTREE_NULLFREE(pCtx, pztx->prb_links_delete);
    SG_RBTREE_NULLFREE(pCtx, pztx->prb_links_new);
    SG_NULLFREE(pCtx, pztx->psz_csid);
    SG_NULLFREE(pCtx, pztx->psz_hid_template);
    SG_NULLFREE(pCtx, pztx->psz_hid_delta);
    SG_NULLFREE(pCtx, pztx);
}

void SG_zingtx__get_repo(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_repo** ppRepo
        )
{
  SG_UNUSED(pCtx);

    *ppRepo = pztx->pRepo;
}

void SG_zingtx__get_dagnum(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_uint32* pdagnum
        )
{
  SG_UNUSED(pCtx);

    *pdagnum = pztx->iDagNum;
}

void SG_zingtx__store_template(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_vhash** ppvh_template
        )
{
	SG_NULL_PP_CHECK_RETURN(ppvh_template);

    SG_ZINGTEMPLATE_NULLFREE(pCtx, pztx->ptemplate);
    SG_ERR_CHECK(  SG_zingtemplate__alloc(pCtx, ppvh_template, &pztx->ptemplate)  );
    pztx->b_template_dirty = SG_TRUE;

    // fall thru

fail:
    return;
}

void sg_zing__get_dbtop(
        SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_hid_cs,
        char** ppsz_hid_template,
        char** ppsz_hid_root
        )
{
    SG_changeset* pcs = NULL;
    const char* psz_hid_delta = NULL;
    SG_vhash* pvh_delta = NULL;
    const char* psz_hid_template = NULL;
    const char* psz_hid_root = NULL;

    if (psz_hid_cs)
    {
        SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, psz_hid_cs, &pcs)  );

        SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs,&psz_hid_delta)  );
        if (!psz_hid_delta)
        {
            SG_ERR_THROW(  SG_ERR_UNSPECIFIED  );
        }
        SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, pRepo, psz_hid_delta, &pvh_delta)  );
        if (ppsz_hid_template)
        {
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_delta, "template", &psz_hid_template)  );
            SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_hid_template, ppsz_hid_template)  );
        }
        if (ppsz_hid_root)
        {
            if (psz_hid_root)
            {
                SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_hid_delta, ppsz_hid_root)  );
            }
            else
            {
                *ppsz_hid_root = NULL;
            }
        }
    }

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_delta);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
}

void sg_zing__load_template__csid(
        SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_csid,
        SG_zingtemplate** ppzt
        )
{
    SG_vhash* pvh_template = NULL;
    SG_zingtemplate* pzt = NULL;
    char* psz_hid_template = NULL;

    if (psz_csid)
    {
        SG_ERR_CHECK(  sg_zing__get_dbtop(pCtx, pRepo, psz_csid, &psz_hid_template, NULL)  );
        SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, pRepo, psz_hid_template, &pvh_template)  );
        SG_ERR_CHECK(  SG_zingtemplate__alloc(pCtx, &pvh_template, &pzt)  );
    }

    *ppzt = pzt;
    pzt = NULL;

    // fall thru

fail:
    SG_NULLFREE(pCtx, psz_hid_template);
    SG_ZINGTEMPLATE_NULLFREE(pCtx, pzt);
    SG_VHASH_NULLFREE(pCtx, pvh_template);
}

void sg_zing__load_template__hid_template(
        SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_hid_template,
        SG_zingtemplate** ppzt
        )
{
    SG_vhash* pvh_template = NULL;
    SG_zingtemplate* pzt = NULL;

    if (psz_hid_template)
    {
        SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, pRepo, psz_hid_template, &pvh_template)  );
        SG_ERR_CHECK(  SG_zingtemplate__alloc(pCtx, &pvh_template, &pzt)  );
    }

    *ppzt = pzt;
    pzt = NULL;

    // fall thru

fail:
    SG_ZINGTEMPLATE_NULLFREE(pCtx, pzt);
    SG_VHASH_NULLFREE(pCtx, pvh_template);
}

void _sg_zingtx__load_template(
        SG_context* pCtx,
        SG_zingtx* pztx
        )
{
    SG_ERR_CHECK_RETURN(  sg_zing__load_template__hid_template(pCtx, pztx->pRepo, pztx->psz_hid_template, &pztx->ptemplate)  );
}

void SG_zing__begin_tx(
    SG_context* pCtx,
	SG_repo* pRepo,
    SG_uint32 iDagNum,
    const char* who,
    const char* psz_hid_baseline,
	SG_zingtx** ppTx
	)
{
    SG_zingtx* pTx = NULL;

    SG_NULLARGCHECK_RETURN(pRepo);
    SG_NULLARGCHECK_RETURN(ppTx);

	SG_ERR_CHECK(  SG_alloc1(pCtx, pTx)  );

    pTx->pRepo = pRepo;
    pTx->iDagNum = iDagNum;
    pTx->b_in_commit = SG_FALSE;

    if (!SG_DAGNUM__IS_AUDIT(pTx->iDagNum))
    {
        SG_NULLARGCHECK_RETURN(who);
        SG_ERR_CHECK(  SG_strcpy(pCtx, pTx->buf_who, sizeof(pTx->buf_who), who)  );
    }

    if (psz_hid_baseline)
    {
        SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_hid_baseline, &pTx->psz_csid)  );
        SG_ERR_CHECK(  sg_zing__get_dbtop(pCtx, pRepo, pTx->psz_csid, &pTx->psz_hid_template, &pTx->psz_hid_delta)  );
    }

    SG_ERR_CHECK(  _sg_zingtx__load_template(pCtx, pTx)  );

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pTx->prb_records)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pTx->prb_parents)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pTx->prb_dependencies)  );

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pTx->prb_links_delete)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pTx->prb_links_new)  );

    *ppTx = pTx;
    pTx = NULL;

    return;

fail:
    SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pTx)  );
}

void SG_zingrecord__alloc(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_original_hid,
        const char* psz_recid,			// This is a GID
        const char* psz_rectype,
        SG_zingrecord** pprec
        )
{
    SG_zingrecord* pzrec = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pzrec)  );
    pzrec->pztx = pztx;
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pzrec->prb_field_values)  );

    SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, pzrec->prb_field_values, SG_ZING_FIELD__RECTYPE, psz_rectype)  );
    SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, pzrec->prb_field_values, SG_ZING_FIELD__RECID, psz_recid)  );

    if (psz_original_hid)
    {
        SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_original_hid, &pzrec->psz_original_hid)  );
    }

    *pprec = pzrec;
    pzrec = NULL;

    return;

fail:
    SG_ZINGRECORD_NULLFREE(pCtx, pzrec);
}

void SG_zingrecord__alloc__from_dbrecord(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_dbrecord* pdbrec,
        SG_bool b_use_hid,
        SG_zingrecord** ppzrec
        )
{
    SG_zingrecord* pzrec = NULL;
    const char* psz_recid = NULL;		// This is a GID
    const char* psz_rectype = NULL;
    const char* psz_hid = NULL;
    SG_uint32 count = 0;
    SG_uint32 i;

    SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, pdbrec, SG_ZING_FIELD__RECID, &psz_recid)  );
    SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, pdbrec, SG_ZING_FIELD__RECTYPE, &psz_rectype)  );
    if (b_use_hid)
    {
        SG_ERR_CHECK(  SG_dbrecord__get_hid__ref(pCtx, pdbrec, &psz_hid)  );
    }

    SG_ERR_CHECK(  SG_zingrecord__alloc(pCtx, pztx, psz_hid, psz_recid, psz_rectype, &pzrec)  );

    SG_ERR_CHECK(  SG_dbrecord__count_pairs(pCtx, pdbrec, &count)  );
    for (i=0; i<count; i++)
    {
		const char* psz_Name;
		const char* psz_Value;

		SG_ERR_CHECK(  SG_dbrecord__get_nth_pair(pCtx, pdbrec, i, &psz_Name, &psz_Value)  );

        if (0 == strcmp(psz_Name, SG_ZING_FIELD__RECID))
        {
        }
        else if (0 == strcmp(psz_Name, SG_ZING_FIELD__RECTYPE))
        {
        }
        else
        {
            SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, pzrec->prb_field_values, psz_Name, psz_Value)  );
        }
    }

    *ppzrec = pzrec;
    pzrec = NULL;

    return;

fail:
    return;
}

void SG_zingtx__add_parent(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_hid_cs_parent
        )
{
    SG_NULLARGCHECK_RETURN(psz_hid_cs_parent);
    SG_ASSERT(psz_hid_cs_parent);

    SG_ERR_CHECK_RETURN(  SG_rbtree__add(pCtx, pztx->prb_parents, psz_hid_cs_parent)  );
}

void sg_zingtx__add__from_dbrecord(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_dbrecord* pdbrec
        )
{
    SG_zingrecord* pzrec = NULL;
    const char* psz_recid = NULL;		// This is a GID

    SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, pdbrec, SG_ZING_FIELD__RECID, &psz_recid)  );
    SG_ERR_CHECK(  SG_zingrecord__alloc__from_dbrecord(pCtx, pztx, pdbrec, SG_FALSE, &pzrec)  );
    pzrec->b_dirty_fields = SG_TRUE;
    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pztx->prb_records, psz_recid, pzrec)  );
    pzrec = NULL;

    // fall thru

fail:
    SG_ZINGRECORD_NULLFREE(pCtx, pzrec);
}

void sg_zingtx__delete__from_dbrecord(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_dbrecord* pdbrec
        )
{
    SG_zingrecord* pzrec = NULL;
    const char* psz_recid = NULL;		// This is a GID

    SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, pdbrec, SG_ZING_FIELD__RECID, &psz_recid)  );
    SG_ERR_CHECK(  SG_zingrecord__alloc__from_dbrecord(pCtx, pztx, pdbrec, SG_TRUE, &pzrec)  );
    pzrec->bDeleteMe = SG_TRUE;
    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pztx->prb_records, psz_recid, pzrec)  );
    pzrec = NULL;

    // fall thru

fail:
    SG_ZINGRECORD_NULLFREE(pCtx, pzrec);
}

void sg_zingtx__mod__from_dbrecord(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_original_hid,
        SG_dbrecord* pdbrec_add
        )
{
    SG_zingrecord* pzrec = NULL;
    const char* psz_recid = NULL;		// This is a GID

    // TODO disallow mod if no_recid

    SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, pdbrec_add, SG_ZING_FIELD__RECID, &psz_recid)  );
    SG_ERR_CHECK(  SG_zingrecord__alloc__from_dbrecord(pCtx, pztx, pdbrec_add, SG_FALSE, &pzrec)  );
    SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_original_hid, &pzrec->psz_original_hid)  );
    pzrec->b_dirty_fields = SG_TRUE;
    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pztx->prb_records, psz_recid, pzrec)  );
    pzrec = NULL;

    // fall thru

fail:
    SG_ZINGRECORD_NULLFREE(pCtx, pzrec);
}

void SG_zingtx__get_record(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid,			// This is a GID.
        SG_zingrecord** ppzrec
        )
{
    SG_bool b;
    SG_zingrecord* pzrec = NULL;
    SG_dbrecord* pdbrec = NULL;
    const char* psz_hid_rec = NULL;
    SG_varray* pva_crit = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    SG_uint32 count_results = 0;

    b = SG_FALSE;
    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pztx->prb_records, psz_recid, &b, (void**) &pzrec)  );
    if (b)
    {
        if (pzrec->bDeleteMe)
        {
            SG_ERR_THROW(  SG_ERR_ZING_RECORD_NOT_FOUND  );
        }

        *ppzrec = pzrec;
        return;
    }

    // look up the recid (a gid) to find the current hid
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, SG_ZING_FIELD__RECID)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, psz_recid)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "_rechid")  );
    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pztx->pRepo, pztx->iDagNum, pztx->psz_csid, pva_crit, psa_fields, &pva)  );
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    if (!pva)
    {
        SG_ERR_THROW(  SG_ERR_ZING_RECORD_NOT_FOUND  );
    }

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count_results)  );
    if(count_results==0)
    {
        SG_ERR_THROW(SG_ERR_ZING_RECORD_NOT_FOUND);
    }
    SG_ASSERT(count_results==1);

    // load the dbrecord from the repo
    SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, 0, &psz_hid_rec)  );
    SG_ERR_CHECK(  SG_dbrecord__load_from_repo(pCtx, pztx->pRepo, psz_hid_rec, &pdbrec)  );
    SG_VARRAY_NULLFREE(pCtx, pva);

    SG_ERR_CHECK(  SG_zingrecord__alloc__from_dbrecord(pCtx, pztx, pdbrec, SG_TRUE, &pzrec)  );
    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pztx->prb_records, psz_recid, pzrec)  );

    *ppzrec = pzrec;
    pzrec = NULL;

fail:
    SG_DBRECORD_NULLFREE(pCtx, pdbrec);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva);
}

void SG_zingrecord__get_rectype(
        SG_context* pCtx,
        SG_zingrecord* pzrec,
        const char** ppsz_rectype
        )
{
	SG_NULLARGCHECK_RETURN(pzrec);

    SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx, pzrec->prb_field_values, SG_ZING_FIELD__RECTYPE, NULL, (void**) ppsz_rectype)  );
}

void SG_zingrecord__get_history(
        SG_context* pCtx,
        SG_zingrecord* pzrec,
        SG_varray** ppva
        )
{
	SG_NULLARGCHECK_RETURN(pzrec);
	SG_NULLARGCHECK_RETURN(ppva);

    if (!pzrec->pva_history)
    {
        const char* psz_recid = NULL;
        SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzrec, &psz_recid)  );
        SG_ERR_CHECK(  SG_repo__dbndx__query_record_history(pCtx, pzrec->pztx->pRepo, pzrec->pztx->iDagNum, psz_recid, &pzrec->pva_history)  );
    }

    *ppva = pzrec->pva_history;

fail:
    ;
}

void SG_zingrecord__get_recid(
        SG_context* pCtx,
        SG_zingrecord* pzrec,
        const char** ppsz_recid			// This is a GID.
        )
{
	SG_NULLARGCHECK_RETURN(pzrec);

    SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx, pzrec->prb_field_values, SG_ZING_FIELD__RECID, NULL, (void**) ppsz_recid)  );
}

void SG_zingrecord__get_tx(
        SG_context* pCtx,
        SG_zingrecord* pzrec,
        SG_zingtx** pptx
        )
{
  SG_UNUSED(pCtx);

    *pptx = pzrec->pztx;
}

void SG_zinglink__unpack(
        SG_context* pCtx,
        const char* psz_link,
        char* psz_recid_from,
        char* psz_recid_to,
        char* psz_name
        )
{
    if (psz_recid_from)
    {
        memcpy(psz_recid_from, psz_link, SG_GID_BUFFER_LENGTH);
        SG_ASSERT(':' == psz_recid_from[SG_GID_ACTUAL_LENGTH]);
        psz_recid_from[SG_GID_ACTUAL_LENGTH] = 0;
    }

    if (psz_recid_to)
    {
        memcpy(psz_recid_to, psz_link + SG_GID_BUFFER_LENGTH, SG_GID_BUFFER_LENGTH);
        SG_ASSERT(':' == psz_recid_to[SG_GID_ACTUAL_LENGTH]);
        psz_recid_to[SG_GID_ACTUAL_LENGTH] = 0;
    }

    if (psz_name)
    {
        SG_ERR_CHECK(  SG_strcpy(pCtx, psz_name, SG_GID_BUFFER_LENGTH, psz_link + (SG_GID_BUFFER_LENGTH*2))  );
    }

    return;

fail:
    return;
}

void sg_zinglink__pack(
        SG_context* pCtx,
        const char* psz_recid_from,
        const char* psz_recid_to,
        const char* psz_link_name,
        struct sg_zinglink* pzlink
        )
{
    SG_ERR_CHECK(  SG_strcpy(pCtx, pzlink->buf, SG_GID_BUFFER_LENGTH, psz_recid_from)  );
    pzlink->buf[SG_GID_ACTUAL_LENGTH] = ':';
    SG_ERR_CHECK(  SG_strcpy(pCtx, pzlink->buf + SG_GID_BUFFER_LENGTH, SG_GID_BUFFER_LENGTH, psz_recid_to)  );
    pzlink->buf[SG_GID_BUFFER_LENGTH + SG_GID_ACTUAL_LENGTH] = ':';
    SG_ERR_CHECK(  SG_strcpy(pCtx, pzlink->buf + 2 * SG_GID_BUFFER_LENGTH, 1 + SG_ZING_MAX_FIELD_NAME_LENGTH, psz_link_name)  );

    return;

fail:
    return;
}

void sg_zinglink__from_vhash(
        SG_context* pCtx,
        const SG_vhash* pvh,
        struct sg_zinglink* pzlink
        )
{
    const char* psz_from = NULL;
    const char* psz_to = NULL;
    const char* psz_name = NULL;

    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, SG_ZING_FIELD__LINK__FROM, &psz_from)  );
    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, SG_ZING_FIELD__LINK__TO, &psz_to)  );
    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, SG_ZING_FIELD__LINK__NAME, &psz_name)  );

    SG_ERR_CHECK(  sg_zinglink__pack(pCtx, psz_from, psz_to, psz_name, pzlink)  );

    return;

fail:
    return;
}

static void sg_zinglink__to_dbrecord(
        SG_context* pCtx,
        const char* psz_link,
        SG_dbrecord** ppResult
        )
{
    SG_dbrecord* pdbrec = NULL;
    char buf_link_name[SG_GID_BUFFER_LENGTH];
    char buf_recid_from[SG_GID_BUFFER_LENGTH];
    char buf_recid_to[SG_GID_BUFFER_LENGTH];

    SG_ERR_CHECK(  SG_zinglink__unpack(pCtx, psz_link, buf_recid_from, buf_recid_to, buf_link_name)  );

    SG_ERR_CHECK(  SG_DBRECORD__ALLOC(pCtx, &pdbrec)  );
    // we do not put a rectype in the link record.  it's not needed.  the
    // field names for links are special and unique, so they can't be
    // used by any other kind of dbrecord
    SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec, SG_ZING_FIELD__LINK__NAME, buf_link_name)  );
    SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec, SG_ZING_FIELD__LINK__FROM, buf_recid_from)  );
    SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec, SG_ZING_FIELD__LINK__TO, buf_recid_to)  );

    *ppResult = pdbrec;
    pdbrec = NULL;

    return;

fail:
    SG_DBRECORD_NULLFREE(pCtx, pdbrec);
}

void SG_zingrecord__to_dbrecord(
        SG_context* pCtx,
        SG_zingrecord* pzrec,
        SG_dbrecord** ppResult
        )
{
    SG_dbrecord* pdbrec = NULL;
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_field_name = NULL;
    const char* psz_field_val = NULL;

    SG_ERR_CHECK(  SG_DBRECORD__ALLOC(pCtx, &pdbrec)  );

    // copy all the fields into the dbrecord
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pzrec->prb_field_values, &b, &psz_field_name, (void**) &psz_field_val)  );
    while (b)
    {
        SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec, psz_field_name, psz_field_val)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_field_name, (void**) &psz_field_val)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    *ppResult = pdbrec;
    pdbrec = NULL;

    return;

fail:
    SG_DBRECORD_NULLFREE(pCtx, pdbrec);
}

void sg_zingfieldattributes__get_type(
	SG_context *pCtx,
	SG_zingfieldattributes *pzfa,
	SG_uint16 *pType)
{
	SG_NULLARGCHECK_RETURN(pzfa);
	SG_NULLARGCHECK_RETURN(pType);

	*pType = pzfa->type;
}

void SG_zinglinkattributes__free(
        SG_context* pCtx,
        SG_zinglinkattributes* pzla
        )
{
    SG_NULLFREE(pCtx, pzla);
}

void SG_zinglinksideattributes__free(
        SG_context* pCtx,
        SG_zinglinksideattributes* pzlsa
        )
{
    if (pzlsa)
    {
        SG_RBTREE_NULLFREE(pCtx, pzlsa->prb_rectypes);
        SG_NULLFREE(pCtx, pzlsa);
    }
}

void SG_zingrecord__remove_field(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa
        )
{
    SG_zingtx* pztx = NULL;

    if (pzfa->required)
    {
        SG_ERR_THROW2(  SG_ERR_ZING_CONSTRAINT,
                (pCtx, "Field '%s' is required", pzfa->name)
                );
    }

    // ask the record for its tx
    SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pzr, &pztx)  );

    SG_ERR_CHECK(  sg_zingtx__change_record(pCtx, pztx, pzr, pzfa->name, NULL)  );

    // fall thru

fail:
    return;
}

void SG_zinglinksideattributes__check_valid_rectype(
        SG_context* pCtx,
        SG_zinglinksideattributes* pzla,
        const char* psz_rectype,
        SG_bool* pb
        )
{
    SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx, pzla->prb_rectypes, psz_rectype, pb, NULL)  );
}

void SG_zinglinkattributes__get_name(
        SG_context* pCtx,
        SG_zinglinkattributes* pzla,
        const char** ppsz_result
        )
{
  SG_UNUSED(pCtx);
    *ppsz_result = pzla->psz_link_name;
}

void SG_zingtx__create_link_crit(
        SG_context* pCtx,
        const char* psz_link_name,
        const char* psz_recid,
        SG_bool bFrom,
        SG_varray** ppva
        )
{
    SG_varray* pva_crit = NULL;
    SG_varray* pva_crit_name = NULL;
    SG_varray* pva_crit_recid = NULL;

    /* construct the crit */
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit_name)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_name, SG_ZING_FIELD__LINK__NAME)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_name, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_name, psz_link_name)  );

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit_recid)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_recid, bFrom ? SG_ZING_FIELD__LINK__FROM : SG_ZING_FIELD__LINK__TO)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_recid, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_recid, psz_recid)  );

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, &pva_crit_name)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "&&")  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, &pva_crit_recid)  );

    *ppva = pva_crit;

    return;

fail:
    return;
}

static void sg_zingtx__get_link_dep_info(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_link_name,
        struct sg_link_dep_info** ppldi
        )
{
    SG_bool b = SG_FALSE;
    struct sg_link_dep_info* pldi = NULL;
    SG_zingtemplate* pztemplate = NULL;

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pztx->prb_dependencies, psz_link_name, &b, (void**) &pldi)  );
    if (!b)
    {
        SG_ERR_CHECK(  SG_alloc1(pCtx, pldi)  );
        pldi->pztx = pztx;
        SG_ERR_CHECK(  SG_strcpy(pCtx, pldi->buf_link_name, sizeof(pldi->buf_link_name), psz_link_name)  );

        // ask the tx for its template
        SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pztemplate)  );

        // ask the template for the link actions
        SG_ERR_CHECK(  sg_zingtemplate__get_link_actions(pCtx, pztemplate, pldi->buf_link_name, &pldi->prb_actions)  );

        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pldi->prb_to)  );
        SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pztx->prb_dependencies, psz_link_name, pldi)  );
    }

    *ppldi = pldi;

    return;

fail:
    return;
}

static void sg_ldi__get_froms(
        SG_context* pCtx,
        struct sg_link_dep_info* pldi,
        const char* psz_recid_to,
        struct sg_froms** ppf
        )
{
    SG_bool b = SG_FALSE;
    struct sg_froms* pf = NULL;

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pldi->prb_to, psz_recid_to, &b, (void**) &pf)  );
    if (!b)
    {
        SG_ERR_CHECK(  SG_alloc1(pCtx, pf)  );

        pf->bDirty = SG_FALSE;
        SG_ERR_CHECK(  SG_zingtx__find_all_records_linking_to(pCtx, pldi->pztx, psz_recid_to, pldi->buf_link_name, &pf->prb_froms)  );
        SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pldi->prb_to, psz_recid_to, pf)  );
    }

    *ppf = pf;

    return;

fail:
    return;
}

static void sg_zingtx__get_froms(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_link_name,
        const char* psz_recid_to,
        struct sg_froms** ppf
        )
{
    struct sg_link_dep_info* pldi = NULL;

    SG_ERR_CHECK(  sg_zingtx__get_link_dep_info(pCtx, pztx, psz_link_name, &pldi)  );

    SG_ERR_CHECK(  sg_ldi__get_froms(pCtx, pldi, psz_recid_to, ppf)  );

    return;

fail:
    return;
}

static void sg_zingtx__change_record(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zingrecord* pzrec,
        const char* psz_name,
        const char* psz_val
        )
{
    SG_rbtree* prb_links = NULL;
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_link = NULL;
    SG_bool b_has_links = SG_FALSE;

    if (psz_val)
    {
        SG_ERR_CHECK(  SG_rbtree__update__with_pooled_sz(pCtx, pzrec->prb_field_values, psz_name, psz_val)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_rbtree__remove(pCtx, pzrec->prb_field_values, psz_name)  );
    }

    pzrec->b_dirty_fields = SG_TRUE;

    SG_ERR_CHECK(  SG_zingtemplate__has_any_links(pCtx, pztx->ptemplate, &b_has_links)  );
    if (b_has_links)
    {
        const char* psz_recid = NULL;

        SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzrec, &psz_recid)  );
        SG_ERR_CHECK(  sg_zingtx__find_all_links_to_or_from(pCtx, pztx, psz_recid, NULL, NULL, SG_TRUE, &prb_links)  );

        if (prb_links)
        {
            SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_links, &b, &psz_link, NULL)  );
            while (b)
            {
                char buf_link_name[SG_GID_BUFFER_LENGTH];
                char buf_recid_to[SG_GID_BUFFER_LENGTH];
                struct sg_froms* pf = NULL;

                SG_ERR_CHECK(  SG_zinglink__unpack(pCtx, psz_link, NULL, buf_recid_to, buf_link_name)  );
                SG_ERR_CHECK(  sg_zingtx__get_froms(pCtx, pztx, buf_link_name, buf_recid_to, &pf)  );

                pf->bDirty = SG_TRUE;

                SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link, NULL)  );
            }
            SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
            SG_RBTREE_NULLFREE(pCtx, prb_links);
        }
    }

fail:
    return;
}

void sg_zingtx__delete_link__packed(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_link,
        const char* psz_hid_rec
        )
{
    char buf_link_name[SG_GID_BUFFER_LENGTH];
    char buf_recid_from[SG_GID_BUFFER_LENGTH];
    char buf_recid_to[SG_GID_BUFFER_LENGTH];
    struct sg_froms* pf = NULL;

    SG_ERR_CHECK(  SG_zinglink__unpack(pCtx, psz_link, buf_recid_from, buf_recid_to, buf_link_name)  );
    SG_ERR_CHECK(  sg_zingtx__get_froms(pCtx, pztx, buf_link_name, buf_recid_to, &pf)  );

    SG_ERR_CHECK(  SG_rbtree__remove(pCtx, pf->prb_froms, buf_recid_from)  );
    pf->bDirty = SG_TRUE;

    if (psz_hid_rec)
    {
        SG_ERR_CHECK(  SG_rbtree__update__with_pooled_sz(pCtx, pztx->prb_links_delete, psz_hid_rec, psz_link)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_rbtree__remove(pCtx, pztx->prb_links_new, psz_link)  );
    }

    {
        SG_zingrecord* pzrec_from = NULL;
        SG_zingrecord* pzrec_to = NULL;

        // We mark both records dirty so that commit-time constraint checks
        // will be performed.  Not sure this is good.  We need some of those
        // commit-time constraints, but not all of them.  TODO

        SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, buf_recid_from, &pzrec_from)  );
        SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, buf_recid_to, &pzrec_to)  );
        pzrec_from->b_dirty_links = SG_TRUE;
        pzrec_to->b_dirty_links = SG_TRUE;
    }

fail:
    return;
}

void SG_zingtx__add_links_to_be_deleted(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_rbtree* prb_records
        )
{
    const char* psz_hid_rec = NULL;
    const char* psz_link = NULL;
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_records, &b, &psz_link, (void**) &psz_hid_rec)  );
    while (b)
    {
        SG_ERR_CHECK(  sg_zingtx__delete_link__packed(pCtx, pztx, psz_link, psz_hid_rec)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link, (void**) &psz_hid_rec)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    return;

fail:
    return;
}

static void sg_zingtx__delete_all_links_from(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid_from
        )
{
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva_crit = NULL;
    SG_rbtree* prb_delete_from_new = NULL;
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_link = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_varray* pva = NULL;

    // find all links that already existed before this tx
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, SG_ZING_FIELD__LINK__FROM)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, psz_recid_from)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 4)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "_rechid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__LINK__FROM)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__LINK__TO)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__LINK__NAME)  );
    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pztx->pRepo, pztx->iDagNum, pztx->psz_csid, pva_crit, psa_fields, &pva)  );

    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva_crit);

    if (pva)
    {
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
        for (i=0; i<count; i++)
        {
            struct sg_zinglink zlink;
            SG_vhash* pvh_link = NULL;
            const char* psz_hid_rec = NULL;

            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh_link)  );

            SG_ERR_CHECK(  sg_zinglink__from_vhash(pCtx, pvh_link, &zlink)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_link, "_rechid", &psz_hid_rec)  );

            SG_ERR_CHECK(  sg_zingtx__delete_link__packed(pCtx, pztx, zlink.buf, psz_hid_rec)  );
        }
        SG_VARRAY_NULLFREE(pCtx, pva);
    }

    // find all new links
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_delete_from_new)  );
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_links_new, &b, &psz_link, NULL)  );
    while (b)
    {
        char buf_recid_from[SG_GID_BUFFER_LENGTH];

        SG_ERR_CHECK(  SG_zinglink__unpack(pCtx, psz_link, buf_recid_from, NULL, NULL)  );

        if (0 == strcmp(buf_recid_from, psz_recid_from))
        {
            SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb_delete_from_new, psz_link)  );
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // delete the new links we found
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_delete_from_new, &b, &psz_link, NULL)  );
    while (b)
    {
        SG_ERR_CHECK(  SG_rbtree__remove(pCtx, pztx->prb_links_new, psz_link)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_RBTREE_NULLFREE(pCtx, prb_delete_from_new);

fail:
    return;
}

static void sg_zingtx__does_link_already_exist(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid_from,
        const char* psz_recid_to,
        const char* psz_link_name,
        SG_bool* pb
        )
{
    SG_bool b = SG_FALSE;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    SG_uint32 count = 0;
    SG_varray* pva_crit = NULL;
    SG_varray* pva_crit_name = NULL;
    SG_varray* pva_crit_from = NULL;
    SG_varray* pva_crit_to = NULL;
    SG_varray* pva_crit_from_and_to = NULL;

    SG_NULLARGCHECK_RETURN(pztx);
    SG_NULLARGCHECK_RETURN(psz_recid_from);
    SG_NULLARGCHECK_RETURN(psz_recid_to);
    SG_NULLARGCHECK_RETURN(psz_link_name);
    SG_NULLARGCHECK_RETURN(pb);

    // name
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit_name)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_name, SG_ZING_FIELD__LINK__NAME)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_name, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_name, psz_link_name)  );

    // from
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit_from)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_from, SG_ZING_FIELD__LINK__FROM)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_from, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_from, psz_recid_from)  );

    // to
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit_to)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_to, SG_ZING_FIELD__LINK__TO)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_to, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_to, psz_recid_to)  );

    // from and to
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit_from_and_to)  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit_from_and_to, &pva_crit_from)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_from_and_to, "&&")  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit_from_and_to, &pva_crit_to)  );

    // all
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, &pva_crit_from_and_to)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "&&")  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, &pva_crit_name)  );

    // query
    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 4)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "_rechid")  );
    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pztx->pRepo, pztx->iDagNum, pztx->psz_csid, pva_crit, psa_fields, &pva)  );
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva_crit);

    if (pva)
    {
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
        if (count > 0)
        {
            SG_ASSERT(1 == count);
            b = SG_TRUE;
        }
    }

    *pb = b;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
}

static void _recidMatch(
	SG_context *pCtx,
	const char *candidate,
	const SG_rbtree *recidlist,
	const char *recid,
	SG_bool *has
    )
{
	if (recidlist != NULL)
	{
        SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx, recidlist, candidate, has, NULL)  );
	}
	else
	{
		*has = (strcmp(recid, candidate) == 0);
	}
}

static void sg_zingtx__find_all_links_to_or_from(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid,			// either
        SG_rbtree* prb_recidlist,       // or
        const char* psz_link_name,
        SG_bool b_from,
        SG_rbtree** pprb
        )
{
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_varray* pva_crit = NULL;
    SG_varray* pva_crit_name = NULL;
    SG_varray* pva_crit_recid = NULL;
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_hid_rec = NULL;
    SG_rbtree* prb_result = NULL;
    const char* psz_link = NULL;
	SG_varray *rlcopy = NULL;

    SG_ASSERT((prb_recidlist && !psz_recid) || (!prb_recidlist && psz_recid));

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_result)  );

    // find all links that already existed before this tx

	if (prb_recidlist)
	{
        SG_ERR_CHECK(  SG_rbtree__to_varray__keys_only(pCtx, prb_recidlist, &rlcopy)  );

		if (psz_link_name)
		{
			/* construct the crit */
			SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit_name)  );
			SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_name, SG_ZING_FIELD__LINK__NAME)  );
			SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_name, "==")  );
			SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_name, psz_link_name)  );

			SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit_recid)  );
			SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_recid, b_from ? SG_ZING_FIELD__LINK__FROM : SG_ZING_FIELD__LINK__TO)  );
			SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_recid, "in")  );
			SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit_recid, &rlcopy  )  );

			SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
			SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, &pva_crit_name)  );
			SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "&&")  );
			SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, &pva_crit_recid)  );
		}
		else
		{
			SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
			SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, b_from ? SG_ZING_FIELD__LINK__FROM : SG_ZING_FIELD__LINK__TO)  );
			SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "in")  );
			SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, &rlcopy)  );
		}
	}
	else
	{
		if (psz_link_name)
		{
			SG_ERR_CHECK(  SG_zingtx__create_link_crit(pCtx, psz_link_name, psz_recid, b_from, &pva_crit)  );
		}
		else
		{
			SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
			SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, b_from ? SG_ZING_FIELD__LINK__FROM : SG_ZING_FIELD__LINK__TO)  );
			SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "==")  );
			SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, psz_recid)  );
		}
	}

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 4)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "_rechid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__LINK__FROM)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__LINK__TO)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__LINK__NAME)  );
    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pztx->pRepo, pztx->iDagNum, pztx->psz_csid, pva_crit, psa_fields, &pva)  );

    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva_crit);

    if (pva)
    {
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
        for (i=0; i<count; i++)
        {
            struct sg_zinglink zlink;
            SG_vhash* pvh_link = NULL;
            const char* psz_hid_rec = NULL;

            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh_link)  );

            SG_ERR_CHECK(  sg_zinglink__from_vhash(pCtx, pvh_link, &zlink)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_link, "_rechid", &psz_hid_rec)  );

            SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, prb_result, zlink.buf, psz_hid_rec)  );
        }
        SG_VARRAY_NULLFREE(pCtx, pva);
    }

    // find all new links
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_links_new, &b, &psz_link, NULL)  );
    while (b)
    {
        char buf_link_name[SG_GID_BUFFER_LENGTH];
        char buf_recid_from[SG_GID_BUFFER_LENGTH];
        char buf_recid_to[SG_GID_BUFFER_LENGTH];

        SG_ERR_CHECK(  SG_zinglink__unpack(pCtx, psz_link, buf_recid_from, buf_recid_to, buf_link_name)  );

        if (
                !psz_link_name
                || (0 == strcmp(psz_link_name, buf_link_name))
           )
        {
            if (b_from)
            {
				SG_bool has = SG_FALSE;

				SG_ERR_CHECK(  _recidMatch(pCtx, buf_recid_from, prb_recidlist, psz_recid, &has)  );

				if (has)
                {
                    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_result, psz_link, NULL)  );
                }
            }
            else
            {
				SG_bool has = SG_FALSE;

				SG_ERR_CHECK(  _recidMatch(pCtx, buf_recid_to, prb_recidlist, psz_recid, &has)  );

				if (has)
                {
                    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_result, psz_link, NULL)  );
                }
            }
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // remove all deleted links
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_links_delete, &b, &psz_hid_rec, (void**) &psz_link)  );
    while (b)
    {
        char buf_link_name[SG_GID_BUFFER_LENGTH];
        char buf_recid_from[SG_GID_BUFFER_LENGTH];
        char buf_recid_to[SG_GID_BUFFER_LENGTH];

        SG_ERR_CHECK(  SG_zinglink__unpack(pCtx, psz_link, buf_recid_from, buf_recid_to, buf_link_name)  );

        if (
                !psz_link_name
                || (0 == strcmp(psz_link_name, buf_link_name))
           )
        {
            if (b_from)
            {
				SG_bool has = SG_FALSE;

				SG_ERR_CHECK(  _recidMatch(pCtx, buf_recid_from, prb_recidlist, psz_recid, &has)  );

				if (has)
                {
                    SG_ERR_CHECK(  SG_rbtree__remove(pCtx, prb_result, psz_link)  );
                }
            }
            else
            {
				SG_bool has = SG_FALSE;

				SG_ERR_CHECK(  _recidMatch(pCtx, buf_recid_to, prb_recidlist, psz_recid, &has)  );

				if (has)
                {
                    SG_ERR_CHECK(  SG_rbtree__remove(pCtx, prb_result, psz_link)  );
                }
            }
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_rec, (void**) &psz_link)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    *pprb = prb_result;
    prb_result = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    SG_VARRAY_NULLFREE(pCtx, pva_crit_name);
    SG_VARRAY_NULLFREE(pCtx, pva_crit_recid);
	SG_VARRAY_NULLFREE(pCtx, rlcopy);
}

void SG_zingtx___find_links(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zinglinkattributes* pzla,
        const char* psz_recid,
        SG_bool bFrom,
        SG_rbtree** pprb
        )
{
    const char* psz_link_name = NULL;
    SG_rbtree* prb_links = NULL;

    SG_ERR_CHECK(  SG_zinglinkattributes__get_name(pCtx, pzla, &psz_link_name)  );
    SG_ERR_CHECK(  sg_zingtx__find_all_links_to_or_from(pCtx, pztx, psz_recid, NULL, psz_link_name, bFrom, &prb_links)  );

    *pprb = prb_links;

    // fall thru

fail:
    return;
}

void SG_zingtx___find_links_list(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zinglinkattributes* pzla,
        SG_rbtree *prb_recidlist,
        SG_bool bFrom,
        SG_rbtree** pprb
        )
{
    const char* psz_link_name = NULL;
    SG_rbtree* prb_links = NULL;

    SG_ERR_CHECK(  SG_zinglinkattributes__get_name(pCtx, pzla, &psz_link_name)  );
    SG_ERR_CHECK(  sg_zingtx__find_all_links_to_or_from(pCtx, pztx, NULL, prb_recidlist, psz_link_name, bFrom, &prb_links)  );

    *pprb = prb_links;

    // fall thru

fail:
    return;
}

void SG_zingtx__delete_links_from(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zinglinkattributes* pzla,
        const char* psz_recid_from
        )
{
    SG_rbtree* prb_links = NULL;
    const char* psz_link_name = NULL;

    SG_ERR_CHECK(  SG_zinglinkattributes__get_name(pCtx, pzla, &psz_link_name)  );
    SG_ERR_CHECK(  sg_zingtx__find_all_links_to_or_from(pCtx, pztx, psz_recid_from, NULL, psz_link_name, SG_TRUE, &prb_links)  );

    if (prb_links)
    {
        SG_ERR_CHECK(  SG_zingtx__add_links_to_be_deleted(pCtx, pztx, prb_links)  );
        SG_RBTREE_NULLFREE(pCtx, prb_links);
    }

fail:
    return;
}

void SG_zingtx__delete_links_to(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zinglinkattributes* pzla,
        const char* psz_recid_to
        )
{
    SG_rbtree* prb_links = NULL;
    const char* psz_link_name = NULL;

    SG_ERR_CHECK(  SG_zinglinkattributes__get_name(pCtx, pzla, &psz_link_name)  );
    SG_ERR_CHECK(  sg_zingtx__find_all_links_to_or_from(pCtx, pztx, psz_recid_to, NULL, psz_link_name, SG_FALSE, &prb_links)  );

    if (prb_links)
    {
        SG_ERR_CHECK(  SG_zingtx__add_links_to_be_deleted(pCtx, pztx, prb_links)  );
        SG_RBTREE_NULLFREE(pCtx, prb_links);
    }

fail:
    return;
}

void SG_zingtx__add_link__packed(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_link
        )
{
    SG_zingtemplate* pzt = NULL;
    char buf_link_name[SG_GID_BUFFER_LENGTH];
    char buf_recid_from[SG_GID_BUFFER_LENGTH];
    char buf_recid_to[SG_GID_BUFFER_LENGTH];
    SG_zingrecord* pzrec_from = NULL;
    SG_zingrecord* pzrec_to = NULL;
    const char* psz_rectype_from = NULL;
    const char* psz_rectype_to = NULL;
    SG_zinglinkattributes* pzla = NULL;
    SG_zinglinksideattributes* pzlsa_from = NULL;
    SG_zinglinksideattributes* pzlsa_to = NULL;
    SG_bool b_valid_rectype = SG_FALSE;
    struct sg_froms* pf = NULL;
    SG_bool b_already_exists = SG_FALSE;

    SG_ERR_CHECK(  SG_zinglink__unpack(pCtx, psz_link, buf_recid_from, buf_recid_to, buf_link_name)  );

    // ask the tx for its template
    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pzt)  );

    // ask the template for the link attributes
    SG_ERR_CHECK(  SG_zingtemplate__get_link_attributes(pCtx, pzt, buf_link_name, &pzla, &pzlsa_from, &pzlsa_to)  );

    if (!pzla)
    {
        SG_ERR_THROW(  SG_ERR_ZING_LINK_NOT_FOUND  );
    }

    // check from
    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, buf_recid_from, &pzrec_from)  );
    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec_from, &psz_rectype_from)  );
    b_valid_rectype = SG_FALSE;
    SG_ERR_CHECK(  SG_zinglinksideattributes__check_valid_rectype(pCtx, pzlsa_from, psz_rectype_from, &b_valid_rectype)  );
    if (!b_valid_rectype)
    {
        SG_ERR_THROW(  SG_ERR_ZING_INVALID_RECTYPE_FOR_LINK  );
    }

    // check to
    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, buf_recid_to, &pzrec_to)  );
    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec_to, &psz_rectype_to)  );
    b_valid_rectype = SG_FALSE;
    SG_ERR_CHECK(  SG_zinglinksideattributes__check_valid_rectype(pCtx, pzlsa_to, psz_rectype_to, &b_valid_rectype)  );
    if (!b_valid_rectype)
    {
        SG_ERR_THROW(  SG_ERR_ZING_INVALID_RECTYPE_FOR_LINK  );
    }

    // check to see if the link already exists in the db
    SG_ERR_CHECK(  sg_zingtx__does_link_already_exist(pCtx, pztx, buf_recid_from, buf_recid_to, buf_link_name, &b_already_exists)  );

    if (!b_already_exists)
    {
        // add the link
        SG_ERR_CHECK(  SG_rbtree__add(pCtx, pztx->prb_links_new, psz_link)  );

        SG_ERR_CHECK(  sg_zingtx__get_froms(pCtx, pztx, buf_link_name, buf_recid_to, &pf)  );
        SG_ERR_CHECK(  SG_rbtree__update(pCtx, pf->prb_froms, buf_recid_from)  );
        pf->bDirty = SG_TRUE;

        {
            SG_zingrecord* pzrec_from = NULL;
            SG_zingrecord* pzrec_to = NULL;

            // We mark both records dirty so that commit-time constraint checks
            // will be performed.  Not sure this is good.  We need some of those
            // commit-time constraints, but not all of them.  TODO

            SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, buf_recid_from, &pzrec_from)  );
            SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, buf_recid_to, &pzrec_to)  );
            pzrec_from->b_dirty_links = SG_TRUE;
            pzrec_to->b_dirty_links = SG_TRUE;
        }
    }

    // fall thru
fail:
    SG_zinglinkattributes__free(pCtx, pzla);
    SG_zinglinksideattributes__free(pCtx, pzlsa_from);
    SG_zinglinksideattributes__free(pCtx, pzlsa_to);
}

void SG_zingtx__add_link__unpacked(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid_from,
        const char* psz_recid_to,
        const char* psz_link_name
        )
{
    struct sg_zinglink zl;

    SG_ERR_CHECK_RETURN(  sg_zinglink__pack(pCtx, psz_recid_from, psz_recid_to, psz_link_name, &zl)  );
    SG_ERR_CHECK_RETURN(  SG_zingtx__add_link__packed(pCtx, pztx, zl.buf)  );
}

void SG_zingrecord__add_link_and_overwrite_other_if_singular(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        const char* psz_link_name_mine,
        const char* psz_recid_other
        )
{
    SG_zingtx* pztx = NULL;
    SG_zingtemplate* pztemplate = NULL;
    SG_zinglinkattributes* pzla = NULL;
    SG_zinglinksideattributes* pzlsa_mine = NULL;
    SG_zinglinksideattributes* pzlsa_other = NULL;
    const char* psz_rectype_mine = NULL;
    const char* psz_recid_mine = NULL;

    // ask the record for its tx
    SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pzr, &pztx)  );

    if (pztx->b_in_commit)
    {
        SG_ERR_THROW(  SG_ERR_ZING_ILLEGAL_DURING_COMMIT  );
    }

    // ask the tx for its template
    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pztemplate)  );

    // ask the template for the link attributes
    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzr, &psz_rectype_mine)  );
    SG_ERR_CHECK(  SG_zingtemplate__get_link_attributes__mine(pCtx, pztemplate, psz_rectype_mine, psz_link_name_mine, &pzla, &pzlsa_mine, &pzlsa_other)  );

    if (!pzla)
    {
        SG_ERR_THROW(  SG_ERR_ZING_LINK_NOT_FOUND  );
    }

    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzr, &psz_recid_mine)  );

    SG_ASSERT(!pzlsa_mine->bSingular);

    // If a link for "mine" already exists, delete it.
    if (pzlsa_mine->bFrom)
    {
        if (pzlsa_other->bSingular)
        {
            // If a link for "other" already exists, delete it.
            SG_ERR_CHECK(  SG_zingtx__delete_links_to(pCtx, pztx, pzla, psz_recid_other)  );
        }

        SG_ERR_CHECK(  SG_zingtx__add_link__unpacked(pCtx, pztx, psz_recid_mine, psz_recid_other, pzla->psz_link_name)  );
    }
    else
    {
        if (pzlsa_other->bSingular)
        {
            SG_ERR_CHECK(  SG_zingtx__delete_links_from(pCtx, pztx, pzla, psz_recid_other)  );
        }

        SG_ERR_CHECK(  SG_zingtx__add_link__unpacked(pCtx, pztx, psz_recid_other, psz_recid_mine, pzla->psz_link_name)  );
    }

fail:
    SG_zinglinkattributes__free(pCtx, pzla);
    SG_zinglinksideattributes__free(pCtx, pzlsa_mine);
    SG_zinglinksideattributes__free(pCtx, pzlsa_other);
}

void SG_zingrecord__set_singular_link_and_overwrite_other_if_singular(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        const char* psz_link_name_mine,
        const char* psz_recid_other
        )
{
    SG_zingtx* pztx = NULL;
    SG_zingtemplate* pztemplate = NULL;
    SG_zinglinkattributes* pzla = NULL;
    SG_zinglinksideattributes* pzlsa_mine = NULL;
    SG_zinglinksideattributes* pzlsa_other = NULL;
    const char* psz_rectype_mine = NULL;
    const char* psz_recid_mine = NULL;

    // ask the record for its tx
    SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pzr, &pztx)  );

    if (pztx->b_in_commit)
    {
        SG_ERR_THROW(  SG_ERR_ZING_ILLEGAL_DURING_COMMIT  );
    }

    // ask the tx for its template
    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pztemplate)  );

    // ask the template for the link attributes
    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzr, &psz_rectype_mine)  );
    SG_ERR_CHECK(  SG_zingtemplate__get_link_attributes__mine(pCtx, pztemplate, psz_rectype_mine, psz_link_name_mine, &pzla, &pzlsa_mine, &pzlsa_other)  );

    if (!pzla)
    {
        SG_ERR_THROW(  SG_ERR_ZING_LINK_NOT_FOUND  );
    }

    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzr, &psz_recid_mine)  );

    SG_ASSERT(pzlsa_mine->bSingular);

    // If a link for "mine" already exists, delete it.
    if (pzlsa_mine->bFrom)
    {
        SG_ERR_CHECK(  SG_zingtx__delete_links_from(pCtx, pztx, pzla, psz_recid_mine)  );
        if (pzlsa_other->bSingular)
        {
            // If a link for "other" already exists, delete it.
            SG_ERR_CHECK(  SG_zingtx__delete_links_to(pCtx, pztx, pzla, psz_recid_other)  );
        }

        SG_ERR_CHECK(  SG_zingtx__add_link__unpacked(pCtx, pztx, psz_recid_mine, psz_recid_other, pzla->psz_link_name)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_zingtx__delete_links_to(pCtx, pztx, pzla, psz_recid_mine)  );
        if (pzlsa_other->bSingular)
        {
            SG_ERR_CHECK(  SG_zingtx__delete_links_from(pCtx, pztx, pzla, psz_recid_other)  );
        }

        SG_ERR_CHECK(  SG_zingtx__add_link__unpacked(pCtx, pztx, psz_recid_other, psz_recid_mine, pzla->psz_link_name)  );
    }

fail:
    SG_zinglinkattributes__free(pCtx, pzla);
    SG_zinglinksideattributes__free(pCtx, pzlsa_mine);
    SG_zinglinksideattributes__free(pCtx, pzlsa_other);
}

void SG_zingrecord__get_field__int(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_int64* pval
        )
{
    const char* psz_val = NULL;
    SG_int64 i = 0;

    if (SG_ZING_TYPE__INT != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzr->prb_field_values, pzfa->name, NULL, (void**) &psz_val)  );
    SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &i, psz_val)  );

    *pval = i;

    return;

fail:
    return;
}

void SG_zingrecord__get_field__datetime(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_int64* pval
        )
{
    const char* psz_val = NULL;
    SG_int64 i = 0;

    if (SG_ZING_TYPE__DATETIME != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzr->prb_field_values, pzfa->name, NULL, (void**) &psz_val)  );
    SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &i, psz_val)  );

    *pval = i;

    return;

fail:
    return;
}

void SG_zingrecord__get_field__bool(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_bool* pval
        )
{
    const char* psz_val = NULL;
    SG_int64 i = 0;

    if (SG_ZING_TYPE__BOOL != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzr->prb_field_values, pzfa->name, NULL, (void**) &psz_val)  );
    SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &i, psz_val)  );
    *pval = (i != 0);

    return;

fail:
    return;
}

void SG_zingrecord__get_field__attachment(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char** pval
        )
{
    const char* psz_val = NULL;

    if (SG_ZING_TYPE__ATTACHMENT != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzr->prb_field_values, pzfa->name, NULL, (void**) &psz_val)  );
    *pval = psz_val;

    return;

fail:
    return;
}

void SG_zingrecord__get_field__string(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char** pval
        )
{
    const char* psz_val = NULL;

    if (SG_ZING_TYPE__STRING != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzr->prb_field_values, pzfa->name, NULL, (void**) &psz_val)  );
    *pval = psz_val;

    return;

fail:
    return;
}

void SG_zingrecord__get_field__userid(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char** pval
        )
{
    const char* psz_val = NULL;

    if (SG_ZING_TYPE__USERID != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzr->prb_field_values, pzfa->name, NULL, (void**) &psz_val)  );
    *pval = psz_val;

    return;

fail:
    return;
}

void SG_zingrecord__get_field__dagnode(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char** pval
        )
{
    const char* psz_val = NULL;

    if (SG_ZING_TYPE__DAGNODE != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzr->prb_field_values, pzfa->name, NULL, (void**) &psz_val)  );
    *pval = psz_val;

    return;

fail:
    return;
}

void sg_zing__add_error_object__idarray(
        SG_context* pCtx,
        SG_varray* pva_violations,
        const char* psz_name_array,
        SG_rbtree* prb_ids,
        ...
        )
{
    va_list ap;
    SG_vhash* pvh = NULL;
    SG_string* pstr = NULL;
    SG_varray* pva = NULL;

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh)  );

	SG_ERR_CHECK(  SG_rbtree__to_varray__keys_only(pCtx, prb_ids, &pva)  );
    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh, psz_name_array, &pva)  );

    va_start(ap, prb_ids);
    do
    {
        const char* psz_key = va_arg(ap, const char*);
        const char* psz_val = NULL;

        if (!psz_key)
        {
            break;
        }

        psz_val = va_arg(ap, const char*);
        SG_ASSERT(psz_val);

        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, psz_key, psz_val)  );
    } while (1);

    if (pva_violations)
    {
        SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pva_violations, &pvh)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
        SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh, pstr)  );
        SG_ERR_THROW2(  SG_ERR_ZING_CONSTRAINT,
                (pCtx, "%s", SG_string__sz(pstr))
                );
    }

fail:
    va_end(ap);
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_VHASH_NULLFREE(pCtx, pvh);
    SG_VARRAY_NULLFREE(pCtx, pva);
}

void sg_zing__add_error_object(
        SG_context* pCtx,
        SG_varray* pva_violations,
        ...
        )
{
    va_list ap;
    SG_vhash* pvh = NULL;
    SG_string* pstr = NULL;

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh)  );

    va_start(ap, pva_violations);
    do
    {
        const char* psz_key = va_arg(ap, const char*);
        const char* psz_val = NULL;

        if (!psz_key)
        {
            break;
        }

        psz_val = va_arg(ap, const char*);
        SG_ASSERT(psz_val);

        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, psz_key, psz_val)  );
    } while (1);

    if (pva_violations)
    {
        SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pva_violations, &pvh)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
        SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh, pstr)  );
        SG_ERR_THROW2(  SG_ERR_ZING_CONSTRAINT,
                (pCtx, "%s", SG_string__sz(pstr))
                );
    }

fail:
    va_end(ap);
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_VHASH_NULLFREE(pCtx, pvh);
}

static void sg_zing__check_field__bool(
        SG_context* pCtx,
        SG_zingfieldattributes* pzfa,
        const char* psz_recid,
        SG_bool val,
        SG_varray* pva_violations
        )
{
  SG_UNUSED(pCtx);
  SG_UNUSED(pzfa);
  SG_UNUSED(psz_recid);
  SG_UNUSED(val);
  SG_UNUSED(pva_violations);
}

static void sg_zing__check_field__attachment(
        SG_context* pCtx,
        SG_zingfieldattributes* pzfa,
        const char* psz_recid,
        const SG_pathname* pPath,
        SG_varray* pva_violations
        )
{
    SG_uint64 len = 0;

    SG_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPath, &len, NULL)  );

    // check maxlength
    if (pzfa->v._attachment.has_maxlength && (len > (SG_uint32) pzfa->v._attachment.val_maxlength))
    {
        SG_int_to_string_buffer sz_i;
        SG_int64_to_sz(pzfa->v._attachment.val_maxlength, sz_i);

        // TODO this check_field function isn't going to get used in the
        // same way as the others.
        SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                    SG_ZING_CONSTRAINT_VIOLATION__TYPE, "maxlength",
                    SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, pzfa->name,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE, sz_i,
                    NULL) );
    }

fail:
    return;
}

static void sg_zing__check_field__dagnode(
        SG_context* pCtx,
        SG_zingfieldattributes* pzfa,
        const char* psz_recid,
        const char* psz_id,
        SG_varray* pva_violations
        )
{
  SG_UNUSED(pCtx);
  SG_UNUSED(pzfa);
  SG_UNUSED(psz_recid);
  SG_UNUSED(psz_id);
  SG_UNUSED(pva_violations);
    // TODO verify that the HID looks right for this repo
    //
    // TODO verify that the dagnode actually exists?  not sure if this is ok.
    //
    // TODO can we constraint-check the dagnum?
}

static void sg_zing__check_field__string(
        SG_context* pCtx,
        SG_zingfieldattributes* pzfa,
        const char* psz_recid,
        const char* psz_val,
        SG_varray* pva_violations
        )
{
    // check minlength
    if (pzfa->v._string.has_minlength && ((SG_int64)strlen(psz_val) < pzfa->v._string.val_minlength))
    {
        SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                    SG_ZING_CONSTRAINT_VIOLATION__TYPE, "minlength",
                    SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, pzfa->name,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE, psz_val,
                    NULL) );
    }

    // check maxlength
    if (pzfa->v._string.has_maxlength && ((SG_int64)strlen(psz_val) > pzfa->v._string.val_maxlength))
    {
        SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                    SG_ZING_CONSTRAINT_VIOLATION__TYPE, "maxlength",
                    SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, pzfa->name,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE, psz_val,
                    NULL) );
    }

    if (pzfa->v._string.prohibited)
    {
        SG_uint32 count = 0;
        SG_uint32 i = 0;

        SG_ERR_CHECK(  SG_stringarray__count(pCtx, pzfa->v._string.prohibited, &count )  );
        for (i=0; i<count; i++)
        {
            const char* psz_thisval = NULL;

            SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pzfa->v._string.prohibited, i, &psz_thisval)  );
            if (0 == strcmp(psz_thisval, psz_val))
            {
                SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                            SG_ZING_CONSTRAINT_VIOLATION__TYPE, "prohibited",
                            SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                            SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, pzfa->name,
                            SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE, psz_val,
                            NULL) );
            }
        }
    }

    if (pzfa->v._string.allowed)
    {
        SG_bool b_found = SG_FALSE;

        SG_ERR_CHECK(  SG_vhash__has(pCtx, pzfa->v._string.allowed, psz_val, &b_found)  );
        if (!b_found)
        {
            SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                        SG_ZING_CONSTRAINT_VIOLATION__TYPE, "allowed",
                        SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                        SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, pzfa->name,
                        SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE, psz_val,
                        NULL) );
        }
    }

fail:
    return;
}

static void sg_zing__check_field__userid(
        SG_context* pCtx,
        SG_zingfieldattributes* pzfa,
        const char* psz_recid,
        const char* psz_id,
        SG_varray* pva_violations
        )
{
  SG_UNUSED(pCtx);
  SG_UNUSED(pzfa);
  SG_UNUSED(psz_recid);
  SG_UNUSED(psz_id);
  SG_UNUSED(pva_violations);

    // TODO verify that the user id looks right
    //
    // TODO verify that it actually exists?
}

static void sg_zing__check_field__datetime(
        SG_context* pCtx,
        SG_zingfieldattributes* pzfa,
        const char* psz_recid,
        SG_int64 val,
        SG_varray* pva_violations
        )
{
    // check min
    if (pzfa->v._datetime.has_min && (val < pzfa->v._datetime.val_min))
    {
        SG_int_to_string_buffer sz_i;
        SG_int64_to_sz(pzfa->v._datetime.val_min, sz_i);

        SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                    SG_ZING_CONSTRAINT_VIOLATION__TYPE, "min",
                    SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, pzfa->name,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE, sz_i,
                    NULL) );
    }

    // check max
    if (pzfa->v._datetime.has_max && (val > pzfa->v._datetime.val_max))
    {
        SG_int_to_string_buffer sz_i;
        SG_int64_to_sz(pzfa->v._datetime.val_max, sz_i);

        SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                    SG_ZING_CONSTRAINT_VIOLATION__TYPE, "max",
                    SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, pzfa->name,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE, sz_i,
                    NULL) );
    }

fail:
    return;
}

static void sg_zing__check_field__int(
        SG_context* pCtx,
        SG_zingfieldattributes* pzfa,
        const char* psz_recid,
        SG_int64 val,
        SG_varray* pva_violations
        )
{
    // check min
    if (pzfa->v._int.has_min && (val < pzfa->v._int.val_min))
    {
        SG_int_to_string_buffer sz_i;
        SG_int64_to_sz(pzfa->v._int.val_min, sz_i);

        SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                    SG_ZING_CONSTRAINT_VIOLATION__TYPE, "min",
                    SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, pzfa->name,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE, sz_i,
                    NULL) );
    }

    // check max
    if (pzfa->v._int.has_max && (val > pzfa->v._int.val_max))
    {
        SG_int_to_string_buffer sz_i;
        SG_int64_to_sz(pzfa->v._int.val_max, sz_i);

        SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                    SG_ZING_CONSTRAINT_VIOLATION__TYPE, "max",
                    SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, pzfa->name,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE, sz_i,
                    NULL) );
    }

    if (pzfa->v._int.prohibited)
    {
        SG_uint32 count = 0;
        SG_uint32 i = 0;

        SG_ERR_CHECK(  SG_vector_i64__length(pCtx, pzfa->v._int.prohibited, &count)  );
        for (i=0; i<count; i++)
        {
            SG_int64 thisval = 0;

            SG_ERR_CHECK(  SG_vector_i64__get(pCtx, pzfa->v._int.prohibited, i, &thisval)  );
            if (thisval == val)
            {
                SG_int_to_string_buffer sz_i;
                SG_int64_to_sz(val, sz_i);

                SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                            SG_ZING_CONSTRAINT_VIOLATION__TYPE, "prohibited",
                            SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                            SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, pzfa->name,
                            SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE, sz_i,
                            NULL) );
            }
        }
    }

    if (pzfa->v._int.allowed)
    {
        SG_uint32 count = 0;
        SG_uint32 i = 0;
        SG_bool b_found_one = SG_FALSE;


        SG_ERR_CHECK(  SG_vector_i64__length(pCtx, pzfa->v._int.allowed, &count)  );
        for (i=0; i<count; i++)
        {
            SG_int64 thisval = 0;

            SG_ERR_CHECK(  SG_vector_i64__get(pCtx, pzfa->v._int.allowed, i, &thisval)  );
            if (thisval == val)
            {
                b_found_one = SG_TRUE;
                break;
            }
        }
        if (!b_found_one)
        {
            SG_int_to_string_buffer sz_i;
            SG_int64_to_sz(val, sz_i);

            SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                        SG_ZING_CONSTRAINT_VIOLATION__TYPE, "allowed",
                        SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                        SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, pzfa->name,
                        SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE, sz_i,
                        NULL) );
        }
    }

    // call validation hook?  used to make sure only prime numbers?
fail:
    return;
}

void SG_zingrecord__set_field__bool(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_bool val
        )
{
    SG_zingtx* pztx = NULL;
    const char* psz_recid = NULL;

    // ask the record for its tx
    SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pzr, &pztx)  );

    if (SG_ZING_TYPE__BOOL != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzr, &psz_recid)  );
    SG_ERR_CHECK(  sg_zing__check_field__bool(pCtx, pzfa, psz_recid, val, NULL)  );

    // set the value
    if (val)
    {
        SG_ERR_CHECK(  sg_zingtx__change_record(pCtx, pztx, pzr, pzfa->name, "1")  );
    }
    else
    {
        SG_ERR_CHECK(  sg_zingtx__change_record(pCtx, pztx, pzr, pzfa->name, "0")  );
    }

fail:
    return;
}

void SG_zingrecord__set_field__attachment__pathname(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_pathname** ppPath
        )
{
    SG_zingtx* pztx = NULL;
	SG_fsobj_type type = SG_FSOBJ_TYPE__UNSPECIFIED;
	SG_fsobj_perms perms;
	SG_bool bExists = SG_FALSE;
    SG_pathname* pPath = NULL;
    const char* psz_recid = NULL;
    SG_pathname* pOldPath = NULL;

    SG_NULLARGCHECK_RETURN(ppPath);
    pPath = *ppPath;

    // ask the record for its tx
    SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pzr, &pztx)  );

    if (SG_ZING_TYPE__ATTACHMENT != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath, &bExists, &type, &perms)  );

	if (!bExists)
	{
        SG_ERR_THROW2(  SG_ERR_NOT_FOUND,
                (pCtx, "Path %s does not exist", SG_pathname__sz(pPath))
                );
	}

    if (SG_FSOBJ_TYPE__REGULAR != type)
    {
        SG_ERR_THROW2(  SG_ERR_NOT_FOUND,
                (pCtx, "Path %s is not a regular file", SG_pathname__sz(pPath))
                );
    }

    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzr, &psz_recid)  );
    SG_ERR_CHECK(  sg_zing__check_field__attachment(pCtx, pzfa, psz_recid, pPath, NULL)  );

    // store the pathname, and this recid, and the field name, in the tx.
    // on commit, store the attachment in the repo, thus getting its HID,
    // and then put that HID into the value of this field.
    if (!pzr->prb_attachments)
    {
        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pzr->prb_attachments)  );
    }
    SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, pzr->prb_attachments, pzfa->name, (void**) pPath, (void**) &pOldPath)  );
    SG_PATHNAME_NULLFREE(pCtx, pOldPath);

    SG_ERR_CHECK(  sg_zingtx__change_record(pCtx, pztx, pzr, pzfa->name, "to be replaced at commit time with attachment hid")  );

    // we own the pPath pointer now
    *ppPath = NULL;

    // fall thru

fail:
    return;
}

void SG_zingrecord__set_field__dagnode(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char* psz_hid
        )
{
    SG_zingtx* pztx = NULL;
    SG_string* pstr_val = NULL;
    const char* psz_recid = NULL;

    // ask the record for its tx
    SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pzr, &pztx)  );

    if (SG_ZING_TYPE__DAGNODE != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzr, &psz_recid)  );
    SG_ERR_CHECK(  sg_zing__check_field__dagnode(pCtx, pzfa, psz_recid, psz_hid, NULL)  );

    // set the value
    SG_ERR_CHECK(  sg_zingtx__change_record(pCtx, pztx, pzr, pzfa->name, psz_hid)  );

    // fall thru

fail:
    SG_STRING_NULLFREE(pCtx, pstr_val);
}

void SG_zingrecord__set_field__string(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char* psz_val
        )
{
    SG_zingtx* pztx = NULL;
    const char* psz_recid = NULL;

    // ask the record for its tx
    SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pzr, &pztx)  );

    if (SG_ZING_TYPE__STRING != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzr, &psz_recid)  );
    SG_ERR_CHECK(  sg_zing__check_field__string(pCtx, pzfa, psz_recid, psz_val, NULL)  );

    // set the value
    SG_ERR_CHECK(  sg_zingtx__change_record(pCtx, pztx, pzr, pzfa->name, psz_val)  );

    return;

fail:
    return;
}

void sg_zingtx__get_all_values(
    SG_context* pCtx,
    SG_zingtx* pztx,
    const char* psz_rectype,
    const char* psz_field,
    SG_rbtree** pprb
    )
{
    SG_rbtree* prb_values = NULL;
    SG_rbtree* prb_records = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_cur_rectype = NULL;
    const char* psz_recid = NULL;
    const char* psz_val = NULL;
    SG_zingrecord* pzrec = NULL;
    SG_varray* pva = NULL;

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_records)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 2)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, psz_field)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pztx->pRepo, pztx->iDagNum, pztx->psz_csid, psz_rectype, NULL, NULL, 0, 0, psa_fields, &pva)  );
    if (pva)
    {
        SG_uint32 i = 0;
        SG_uint32 count = 0;

        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count )  );
        for (i=0; i<count; i++)
        {
            SG_vhash* pvh = NULL;

            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh)  );

            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, SG_ZING_FIELD__RECID, &psz_recid)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, psz_field, &psz_val)  );
            // TODO the following line SHOULD be add, but changing it to update
            // works around a problem with hosed dbndxes
            SG_ERR_CHECK(  SG_rbtree__update__with_pooled_sz(pCtx, prb_records, psz_recid, psz_val)  );
        }
    }

    // now iterate over all dirty records
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_records, &b, &psz_recid, (void**) &pzrec)  );
    while (b)
    {
        SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec, &psz_cur_rectype)  );
        if (0 == strcmp(psz_rectype, psz_cur_rectype))
        {
            SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzrec->prb_field_values, psz_field, NULL, (void**) &psz_val)  );
            SG_ERR_CHECK(  SG_rbtree__update__with_pooled_sz(pCtx, prb_records, psz_recid, psz_val)  );
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid, (void**) &pzrec)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // now just the values
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_values)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_records, &b, &psz_recid, (void**) &psz_val)  );
    while (b)
    {
        SG_ERR_CHECK(  SG_rbtree__update(pCtx, prb_values, psz_val)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid, (void**) &psz_val)  );
    }

    *pprb = prb_values;
    prb_values = NULL;

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_RBTREE_NULLFREE(pCtx, prb_records);
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_RBTREE_NULLFREE(pCtx, prb_values);
}

void SG_zingrecord__do_defaultfunc__string(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char* psz_func
        )
{
    SG_string* pstr = NULL;

    if (0 == strcmp(psz_func, "gen_random_unique"))
    {
        SG_ERR_CHECK(  sg_zing__gen_random_unique_string(pCtx, pzr->pztx, pzfa, &pstr)  );
        SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, pzr, pzfa, SG_string__sz(pstr))  );
    }
    else if (0 == strcmp(psz_func, "gen_userprefix_unique"))
    {
        SG_ERR_CHECK(  sg_zing__gen_userprefix_unique_string(pCtx, pzr->pztx, pzfa, &pstr)  );
        SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, pzr, pzfa, SG_string__sz(pstr))  );
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
    }

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
}

void SG_zingrecord__set_field__userid(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char* psz_val
        )
{
    SG_zingtx* pztx = NULL;
    const char* psz_recid = NULL;

    // ask the record for its tx
    SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pzr, &pztx)  );

    if (SG_ZING_TYPE__USERID != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzr, &psz_recid)  );
    SG_ERR_CHECK(  sg_zing__check_field__userid(pCtx, pzfa, psz_recid, psz_val, NULL)  );

    // set the value
    SG_ERR_CHECK(  sg_zingtx__change_record(pCtx, pztx, pzr, pzfa->name, psz_val)  );

    return;

fail:
    return;
}

void SG_zingrecord__lookup_name(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        const char* psz_name,
        SG_zingfieldattributes** ppzfa,
        SG_zinglinkattributes** ppzla,
        SG_zinglinksideattributes** ppzlsa_mine,
        SG_zinglinksideattributes** ppzlsa_other
        )
{
    const char* psz_rectype = NULL;
    SG_zingtx* pztx = NULL;
    SG_zingtemplate* pztemplate = NULL;
    SG_zingfieldattributes* pzfa = NULL;
    SG_zinglinkattributes* pzla = NULL;
    SG_zinglinksideattributes* pzlsa_mine = NULL;
    SG_zinglinksideattributes* pzlsa_other = NULL;

    // ask the record for its tx
    SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pzr, &pztx)  );

    // ask the tx for its template
    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pztemplate)  );

    // get the rectype
    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzr, &psz_rectype)  );

    // now see if it's a field
    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype, psz_name, &pzfa)  );

    // otherwise, maybe it's a link
    if (!pzfa)
    {
        SG_ERR_CHECK(  SG_zingtemplate__get_link_attributes__mine(pCtx, pztemplate, psz_rectype, psz_name, &pzla, &pzlsa_mine, &pzlsa_other)  );
    }

    *ppzfa = pzfa;
    pzfa = NULL;

    *ppzla = pzla;
    pzla = NULL;

    *ppzlsa_mine = pzlsa_mine;
    pzlsa_mine = NULL;

    *ppzlsa_other = pzlsa_other;
    pzlsa_other = NULL;

    return;

fail:
    return;
}

void SG_zingrecord__set_field__int(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_int64 val
        )
{
    SG_zingtx* pztx = NULL;
    const char* psz_recid = NULL;

    // ask the record for its tx
    SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pzr, &pztx)  );

    if (SG_ZING_TYPE__INT != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzr, &psz_recid)  );
    SG_ERR_CHECK(  sg_zing__check_field__int(pCtx, pzfa, psz_recid, val, NULL)  );

    // set the value
	{
		SG_int_to_string_buffer buf;

		SG_int64_to_sz(val, buf);

		SG_ERR_CHECK(  sg_zingtx__change_record(pCtx, pztx, pzr, pzfa->name, buf)  );
	}

    return;

fail:
    return;
}

void SG_zingrecord__set_field__datetime(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_int64 val
        )
{
    SG_zingtx* pztx = NULL;
    const char* psz_recid = NULL;

    // ask the record for its tx
    SG_ERR_CHECK(  SG_zingrecord__get_tx(pCtx, pzr, &pztx)  );

    if (SG_ZING_TYPE__DATETIME != pzfa->type)
    {
        SG_ERR_THROW(  SG_ERR_ZING_TYPE_MISMATCH  );
    }

    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzr, &psz_recid)  );
    SG_ERR_CHECK(  sg_zing__check_field__datetime(pCtx, pzfa, psz_recid, val, NULL)  );

    // set the value
	{
		SG_int_to_string_buffer buf;

		SG_int64_to_sz(val, buf);

		SG_ERR_CHECK(  sg_zingtx__change_record(pCtx, pztx, pzr, pzfa->name, buf)  );
	}

    return;

fail:
    return;
}

void SG_zingrecord__free(SG_context* pCtx, SG_zingrecord* pThis)
{
    if (!pThis)
    {
        return;
    }

    SG_VARRAY_NULLFREE(pCtx, pThis->pva_history);
    SG_RBTREE_NULLFREE(pCtx, pThis->prb_field_values);
    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pThis->prb_attachments, (SG_free_callback*) SG_pathname__free);
    SG_NULLFREE(pCtx, pThis->psz_original_hid);
    SG_NULLFREE(pCtx, pThis);
}

void SG_zingtx__create_new_record(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_rectype,
        SG_zingrecord** pprec
        )
{
    SG_zingrecord* prec = NULL;
	char buf_recid[SG_GID_BUFFER_LENGTH];
    SG_uint32 i = 0;
    SG_uint32 count = 0;
    SG_stringarray* psa_fields = NULL;
    SG_zingtemplate* pztemplate = NULL;
    SG_zingfieldattributes* pzfa = NULL;

    // ask the tx for its template
    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pztemplate)  );

    if (pztx->b_in_commit)
    {
        SG_ERR_THROW(  SG_ERR_ZING_ILLEGAL_DURING_COMMIT  );
    }

    // TODO no recid here?
	SG_ERR_CHECK(  SG_gid__generate(pCtx, buf_recid, sizeof(buf_recid))  );
    SG_ERR_CHECK(  SG_zingrecord__alloc(pCtx, pztx, NULL, buf_recid, psz_rectype, &prec)  );
    prec->b_dirty_fields = SG_TRUE;

    // init defaults by getting the field list and enumerating

    SG_ERR_CHECK(  SG_zingtemplate__list_fields(pCtx, pztemplate, psz_rectype, &psa_fields)  );
    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_fields, &count )  );
    for (i=0; i<count; i++)
    {
        const char* psz_field_name = NULL;

        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa_fields, i, &psz_field_name)  );
        SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype, psz_field_name, &pzfa)  );

        switch (pzfa->type)
        {
            case SG_ZING_TYPE__BOOL:
                {
                    if (pzfa->v._bool.has_defaultvalue)
                    {
                        SG_ERR_CHECK(  SG_zingrecord__set_field__bool(pCtx, prec, pzfa, pzfa->v._bool.defaultvalue)  );
                    }
                    break;
                }
            case SG_ZING_TYPE__DATETIME:
                {
                    if (pzfa->v._datetime.has_defaultvalue)
                    {
                        if (0 == strcmp("now", pzfa->v._datetime.defaultvalue))
                        {
                            SG_int64 t = 0;

                            SG_ERR_CHECK(  SG_time__get_milliseconds_since_1970_utc(pCtx, &t)  );
                            SG_ERR_CHECK(  SG_zingrecord__set_field__datetime(pCtx, prec, pzfa, t)  );
                        }
                        else
                        {
                            SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                        }
                    }
                    break;
                }
            case SG_ZING_TYPE__INT:
                {
                    if (pzfa->v._int.has_defaultvalue)
                    {
                        SG_ERR_CHECK(  SG_zingrecord__set_field__int(pCtx, prec, pzfa, pzfa->v._int.defaultvalue)  );
                    }
                    break;
                }
            case SG_ZING_TYPE__STRING:
                {
                    if (pzfa->v._string.has_defaultvalue)
                    {
                        SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, prec, pzfa, pzfa->v._string.defaultvalue)  );
                    }
                    else if (pzfa->v._string.has_defaultfunc)
                    {
                        SG_ERR_CHECK(  SG_zingrecord__do_defaultfunc__string(pCtx, prec, pzfa, pzfa->v._string.defaultfunc)  );
                    }
                    break;
                }
            case SG_ZING_TYPE__USERID:
                {
                    if (pzfa->v._userid.has_defaultvalue)
                    {
                        // TODO this should be defaultfunc, not defaultvalue
                        if (0 == strcmp("whoami", pzfa->v._userid.defaultvalue))
                        {
                            SG_ERR_CHECK(  SG_zingrecord__set_field__userid(pCtx, prec, pzfa, pztx->buf_who)  );
                        }
                        else
                        {
                            SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                        }
                    }
                    break;
                }
        }
    }

    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);

    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pztx->prb_records, buf_recid, prec)  );

    *pprec = prec;
    prec = NULL;

    return;

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_ZINGRECORD_NULLFREE(pCtx, prec);
}

void SG_zingtx__delete_record(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid
        )
{
    SG_zingrecord* pzrec = NULL;
    SG_rbtree* prb_links_to_me = NULL;

    if (SG_DAGNUM__IS_NO_RECID(pztx->iDagNum))
    {
        SG_ERR_THROW(  SG_ERR_ZING_NO_RECID  );
    }

    if (pztx->b_in_commit)
    {
        SG_ERR_THROW(  SG_ERR_ZING_ILLEGAL_DURING_COMMIT  );
    }

    SG_ERR_CHECK(  sg_zingtx__find_all_links_to_or_from(pCtx, pztx, psz_recid, NULL, NULL, SG_FALSE, &prb_links_to_me)  );
    if (prb_links_to_me)
    {
        SG_uint32 count = 0;
        SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_links_to_me, &count)  );
        if (count)
        {
            // TODO allow the template to specify alternate behavior for this case.
            // Choices:
            // 1.  fail
            // 2.  delete the record and the links
            // 3.  delete the record and the links AND the records on the other side of the links.
            SG_ERR_THROW(  SG_ERR_ZING_CANNOT_DELETE_A_RECORD_WITH_LINKS_TO_IT  );
        }
        SG_RBTREE_NULLFREE(pCtx, prb_links_to_me);
    }

    SG_ERR_CHECK(  sg_zingtx__delete_all_links_from(pCtx, pztx, psz_recid)  );

    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid, &pzrec)  );
    pzrec->bDeleteMe = SG_TRUE;

    return;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_links_to_me);
}

void SG_zingrecord__get_original_hid(
        SG_context* pCtx,
        SG_zingrecord* pzrec,
        char** psz_hid_rec
        )
{
  SG_UNUSED(pCtx);

    *psz_hid_rec = pzrec->psz_original_hid;
}

void SG_zingrecord__is_delete_me(
        SG_context* pCtx,
        SG_zingrecord* pzrec,
        SG_bool* pb
        )
{
  SG_UNUSED(pCtx);

    *pb = pzrec->bDeleteMe;
}

static void SG_zingtx__find_all_records_linking_to(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid_to,
        const char* psz_link_name,
        SG_rbtree** pprb
        )
{
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    SG_varray* pva_crit = NULL;
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    SG_rbtree* prb_result = NULL;
    const char* psz_link = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_result)  );

    SG_ERR_CHECK(  SG_zingtx__create_link_crit(pCtx, psz_link_name, psz_recid_to, SG_FALSE, &pva_crit)  );
    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__LINK__FROM)  );
    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pztx->pRepo, pztx->iDagNum, pztx->psz_csid, pva_crit, psa_fields, &pva)  );

    SG_VARRAY_NULLFREE(pCtx, pva_crit);

    if (pva)
    {
        // get all links that already existed before this tx
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );

        for (i=0; i<count; i++)
        {
            const char* psz_from = NULL;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, i, &psz_from)  );
            SG_ERR_CHECK(  SG_rbtree__update(pCtx, prb_result, psz_from)  );
        }
        SG_VARRAY_NULLFREE(pCtx, pva);
    }

    // find all new links
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_links_new, &b, &psz_link, NULL)  );
    while (b)
    {
        char buf_recid_from[SG_GID_BUFFER_LENGTH];
        char buf_recid_to[SG_GID_BUFFER_LENGTH];

        SG_ERR_CHECK(  SG_zinglink__unpack(pCtx, psz_link, buf_recid_from, buf_recid_to, NULL)  );

        if (0 == strcmp(buf_recid_to, psz_recid_to))
        {
            SG_ERR_CHECK(  SG_rbtree__update(pCtx, prb_result, buf_recid_from)  );
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // remove any links that are pending deletion
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_links_delete, &b, NULL, (void**) &psz_link)  );
    while (b)
    {
        char buf_recid_from[SG_GID_BUFFER_LENGTH];
        char buf_recid_to[SG_GID_BUFFER_LENGTH];

        SG_ERR_CHECK(  SG_zinglink__unpack(pCtx, psz_link, buf_recid_from, buf_recid_to, NULL)  );

        if (0 == strcmp(buf_recid_to, psz_recid_to))
        {
            SG_ERR_CHECK(  SG_rbtree__remove(pCtx, prb_result, buf_recid_from)  );
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, NULL, (void**) &psz_link)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    *pprb = prb_result;
    prb_result = NULL;

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
}

static void sg_zingtx__builtin_action__count(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid_to,
        SG_rbtree* prb_froms,
        const char* psz_field_from,
        const char* psz_field_to
        )
{
    SG_zingrecord* pzrec_me = NULL;
    SG_uint32 count = 0;
    SG_zingtemplate* pztemplate = NULL;
    SG_zingfieldattributes* pzfa = NULL;
    const char* psz_rectype = NULL;

    SG_UNUSED(psz_field_from);

    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_to, &pzrec_me)  );
    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pztemplate)  );
    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec_me, &psz_rectype)  );
    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype, psz_field_to, &pzfa)  );
    if (!pzfa)
    {
        SG_ERR_THROW(  SG_ERR_ZING_FIELD_NOT_FOUND  );
    }

    SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_froms, &count)  );
    SG_ERR_CHECK(  SG_zingrecord__set_field__int(pCtx, pzrec_me, pzfa, count)  );

    // fall thru

fail:
    ;
}

static void sg_zingtx__builtin_action__min(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid_to,
        SG_rbtree* prb_froms,
        const char* psz_field_from,
        const char* psz_field_to
        )
{
    SG_int64 my_min = 0;
    SG_bool b_first = SG_TRUE;
    SG_zingrecord* pzrec_me = NULL;
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    const char* psz_recid_from = NULL;
    SG_zingrecord* pzrec_from = NULL;
    SG_zingtemplate* pztemplate = NULL;
    SG_zingfieldattributes* pzfa_to = NULL;
    SG_zingfieldattributes* pzfa_from = NULL;
    const char* psz_rectype_to = NULL;
    const char* psz_rectype_from = NULL;

    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_to, &pzrec_me)  );
    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pztemplate)  );
    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec_me, &psz_rectype_to)  );
    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype_to, psz_field_to, &pzfa_to)  );
    if (!pzfa_to)
    {
        SG_ERR_THROW(  SG_ERR_ZING_FIELD_NOT_FOUND  );
    }

    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_to, &pzrec_me)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_froms, &b, &psz_recid_from, NULL)  );
    while (b)
    {
        SG_int64 thisval = 0;

        SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_from, &pzrec_from)  );

        SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec_from, &psz_rectype_from)  );
        SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype_from, psz_field_from, &pzfa_from)  );
        if (!pzfa_from)
        {
            SG_ERR_THROW(  SG_ERR_ZING_FIELD_NOT_FOUND  );
        }

        SG_ERR_CHECK(  SG_zingrecord__get_field__int(pCtx, pzrec_from, pzfa_from, &thisval)  );

        if (b_first || (thisval < my_min))
        {
            my_min = thisval;
        }

        b_first = SG_FALSE;
        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid_from, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    SG_ERR_CHECK(  SG_zingrecord__set_field__int(pCtx, pzrec_me, pzfa_to, my_min)  );

    // fall thru

fail:
    return;
}

static void sg_zingtx__builtin_action__max(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid_to,
        SG_rbtree* prb_froms,
        const char* psz_field_from,
        const char* psz_field_to
        )
{
    SG_int64 my_max = 0;
    SG_bool b_first = SG_TRUE;
    SG_zingrecord* pzrec_me = NULL;
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    const char* psz_recid_from = NULL;
    SG_zingrecord* pzrec_from = NULL;
    SG_zingtemplate* pztemplate = NULL;
    SG_zingfieldattributes* pzfa_to = NULL;
    SG_zingfieldattributes* pzfa_from = NULL;
    const char* psz_rectype_to = NULL;
    const char* psz_rectype_from = NULL;

    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_to, &pzrec_me)  );
    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pztemplate)  );
    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec_me, &psz_rectype_to)  );
    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype_to, psz_field_to, &pzfa_to)  );
    if (!pzfa_to)
    {
        SG_ERR_THROW(  SG_ERR_ZING_FIELD_NOT_FOUND  );
    }

    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_to, &pzrec_me)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_froms, &b, &psz_recid_from, NULL)  );
    while (b)
    {
        SG_int64 thisval = 0;

        SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_from, &pzrec_from)  );

        SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec_from, &psz_rectype_from)  );
        SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype_from, psz_field_from, &pzfa_from)  );
        if (!pzfa_from)
        {
            SG_ERR_THROW(  SG_ERR_ZING_FIELD_NOT_FOUND  );
        }

        SG_ERR_CHECK(  SG_zingrecord__get_field__int(pCtx, pzrec_from, pzfa_from, &thisval)  );

        if (b_first || (thisval > my_max))
        {
            my_max = thisval;
        }

        b_first = SG_FALSE;
        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid_from, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    SG_ERR_CHECK(  SG_zingrecord__set_field__int(pCtx, pzrec_me, pzfa_to, my_max)  );

    // fall thru

fail:
    return;
}

static void sg_zingtx__builtin_action__sum(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid_to,
        SG_rbtree* prb_froms,
        const char* psz_field_from,
        const char* psz_field_to
        )
{
    SG_int64 sum = 0;
    SG_zingrecord* pzrec_me = NULL;
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    const char* psz_recid_from = NULL;
    SG_zingrecord* pzrec_from = NULL;
    SG_zingtemplate* pztemplate = NULL;
    SG_zingfieldattributes* pzfa_to = NULL;
    SG_zingfieldattributes* pzfa_from = NULL;
    const char* psz_rectype_to = NULL;
    const char* psz_rectype_from = NULL;

    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_to, &pzrec_me)  );
    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pztemplate)  );
    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec_me, &psz_rectype_to)  );
    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype_to, psz_field_to, &pzfa_to)  );
    if (!pzfa_to)
    {
        SG_ERR_THROW(  SG_ERR_ZING_FIELD_NOT_FOUND  );
    }

    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_to, &pzrec_me)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_froms, &b, &psz_recid_from, NULL)  );
    while (b)
    {
        SG_int64 thisval = 0;

        SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_from, &pzrec_from)  );

        SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec_from, &psz_rectype_from)  );
        SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype_from, psz_field_from, &pzfa_from)  );
        if (!pzfa_from)
        {
            SG_ERR_THROW(  SG_ERR_ZING_FIELD_NOT_FOUND  );
        }

        SG_ERR_CHECK(  SG_zingrecord__get_field__int(pCtx, pzrec_from, pzfa_from, &thisval)  );
        sum += thisval;

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid_from, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    SG_ERR_CHECK(  SG_zingrecord__set_field__int(pCtx, pzrec_me, pzfa_to, sum)  );

    // fall thru

fail:
    ;
}

static void sg_zingtx__builtin_action__average(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid_to,
        SG_rbtree* prb_froms,
        const char* psz_field_from,
        const char* psz_field_to
        )
{
    SG_int64 sum = 0;
    SG_uint32 count = 0;
    SG_zingrecord* pzrec_me = NULL;
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    const char* psz_recid_from = NULL;
    SG_zingrecord* pzrec_from = NULL;
    SG_zingtemplate* pztemplate = NULL;
    SG_zingfieldattributes* pzfa_to = NULL;
    SG_zingfieldattributes* pzfa_from = NULL;
    const char* psz_rectype_to = NULL;
    const char* psz_rectype_from = NULL;

    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_to, &pzrec_me)  );
    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pztemplate)  );
    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec_me, &psz_rectype_to)  );
    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype_to, psz_field_to, &pzfa_to)  );
    if (!pzfa_to)
    {
        SG_ERR_THROW(  SG_ERR_ZING_FIELD_NOT_FOUND  );
    }

    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_to, &pzrec_me)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_froms, &b, &psz_recid_from, NULL)  );
    while (b)
    {
        SG_int64 thisval = 0;

        SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid_from, &pzrec_from)  );

        SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec_from, &psz_rectype_from)  );
        SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype_from, psz_field_from, &pzfa_from)  );
        if (!pzfa_from)
        {
            SG_ERR_THROW(  SG_ERR_ZING_FIELD_NOT_FOUND  );
        }

        SG_ERR_CHECK(  SG_zingrecord__get_field__int(pCtx, pzrec_from, pzfa_from, &thisval)  );

        sum += thisval;
        count++;

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid_from, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    if (count > 0)
    {
        SG_ERR_CHECK(  SG_zingrecord__set_field__int(pCtx, pzrec_me, pzfa_to, sum/count)  );
    }
    else
    {
        // TODO what happens here? div by zero
        SG_ERR_CHECK(  SG_zingrecord__set_field__int(pCtx, pzrec_me, pzfa_to, 0)  );
    }

    // fall thru

fail:
    return;
}

static void bc__cross_check(
        SG_context* pCtx,
        SG_rbtree* prb_to,
        SG_rbtree* prb_froms,
        SG_bool* pb_match
        )
{
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    const char* psz_recid_from = NULL;
    SG_bool b_match = SG_FALSE;

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_froms, &b, &psz_recid_from, NULL)  );
    while (b)
    {

        SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_to, psz_recid_from, &b_match, NULL)  );
        if (b_match)
        {
            break;
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid_from, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    *pb_match = b_match;

    return;

fail:
    return;
}

static void sg_zingaction__do(
        SG_context* pCtx,
        SG_zingtx* pztx,
        struct sg_zingaction* pza,
        SG_rbtree* prb_froms,
        const char* psz_recid_to
        )
{
    if (0 == strcmp(pza->psz_type, "builtin"))
    {
        if (0 == strcmp(pza->psz_function, "sum"))
        {
            SG_ERR_CHECK(  sg_zingtx__builtin_action__sum(pCtx, pztx, psz_recid_to, prb_froms, pza->psz_field_from, pza->psz_field_to)  );
        }
        else if (0 == strcmp(pza->psz_function, "average"))
        {
            SG_ERR_CHECK(  sg_zingtx__builtin_action__average(pCtx, pztx, psz_recid_to, prb_froms, pza->psz_field_from, pza->psz_field_to)  );
        }
        else if (0 == strcmp(pza->psz_function, "count"))
        {
            SG_ERR_CHECK(  sg_zingtx__builtin_action__count(pCtx, pztx, psz_recid_to, prb_froms, pza->psz_field_from, pza->psz_field_to)  );
        }
        else if (0 == strcmp(pza->psz_function, "min"))
        {
            SG_ERR_CHECK(  sg_zingtx__builtin_action__min(pCtx, pztx, psz_recid_to, prb_froms, pza->psz_field_from, pza->psz_field_to)  );
        }
        else if (0 == strcmp(pza->psz_function, "max"))
        {
            SG_ERR_CHECK(  sg_zingtx__builtin_action__max(pCtx, pztx, psz_recid_to, prb_froms, pza->psz_field_from, pza->psz_field_to)  );
        }
        else
        {
            SG_ERR_THROW(  SG_ERR_ZING_UNKNOWN_BUILTIN_ACTION);
        }
    }
    else if (0 == strcmp(pza->psz_type, "js"))
    {
        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_ZING_UNKNOWN_ACTION_TYPE  );
    }

    return;

fail:
    return;
}

static void sg_zingtx__do_actions_for_one_rec(
        SG_context* pCtx,
        SG_rbtree* prb_froms,
        const char* psz_recid_to,
        struct sg_link_dep_info* pldi
        )
{
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    const char* psz_action_name = NULL;
    struct sg_zingaction* pza = NULL;

    if (pldi->prb_actions)
    {
        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pldi->prb_actions, &b, &psz_action_name, (void**) &pza)  );
        while (b)
        {
            SG_ERR_CHECK(  sg_zingaction__do(pCtx, pldi->pztx, pza, prb_froms, psz_recid_to)  );

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_action_name, (void**) &pza)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    }

    return;

fail:
    return;
}

static void bc__perform_actions(
        SG_context* pCtx,
        struct sg_link_dep_info* pldi,
        SG_uint32* piCount
        )
{
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    const char* psz_recid_to = NULL;
    struct sg_froms* pf = NULL;
    SG_uint32 count = 0;
    SG_rbtree* prb_do = NULL;

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_do)  );

    // first filter out all the ones that actually need to be done
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pldi->prb_to, &b, &psz_recid_to, (void**) &pf)  );
    while (b)
    {
        if (pf->bDirty)
        {
            SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_do, psz_recid_to, pf)  );
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid_to, (void**) &pf)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // now do the ones which were filtered
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_do, &b, &psz_recid_to, (void**) &pf)  );
    while (b)
    {
        SG_bool b_cant = SG_FALSE;

        SG_ERR_CHECK(  bc__cross_check(pCtx, prb_do, pf->prb_froms, &b_cant)  );

        if (!b_cant)
        {
            SG_ERR_CHECK(  sg_zingtx__do_actions_for_one_rec(pCtx, pf->prb_froms, psz_recid_to, pldi)  );
            pf->bDirty = SG_FALSE;
            count++;
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid_to, (void**) &pf)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_RBTREE_NULLFREE(pCtx, prb_do);

    *piCount = count;

    return;

fail:
    return;
}

#if 0
static void sg_ldi__one__check_for_cycles(
        SG_context* pCtx,
        struct sg_link_dep_info* pldi,
        const char* psz_recid_to,
        SG_rbtree* prb_froms,
        SG_rbtree* prb_seen
        )
{
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    const char* psz_recid_from = NULL;
    SG_bool b_already = SG_FALSE;
    struct sg_froms* pf_sub = NULL;

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_seen, psz_recid_to, &b_already, NULL)  );
    if (b_already)
    {
        SG_ERR_THROW(  SG_ERR_ZING_DIRECTED_LINK_CYCLE  );
    }

    SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb_seen, psz_recid_to)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_froms, &b, &psz_recid_from, NULL)  );
    while (b)
    {
        SG_ERR_CHECK(  sg_ldi__get_froms(pCtx, pldi, psz_recid_from, &pf_sub)  );
        SG_ERR_CHECK(  sg_ldi__one__check_for_cycles(pCtx, pldi, psz_recid_from, pf_sub->prb_froms, prb_seen)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid_from, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    return;

fail:
    return;
}

static void sg_ldi__check_for_cycles(
        SG_context* pCtx,
        struct sg_link_dep_info* pldi
        )
{
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    const char* psz_recid_to = NULL;
    struct sg_froms* pf = NULL;
    SG_rbtree* prb_seen = NULL;

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pldi->prb_to, &b, &psz_recid_to, (void**) &pf)  );
    while (b)
    {
        // TODO can we skip this if pf->bDirty is false?

        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_seen)  );
        SG_ERR_CHECK(  sg_ldi__one__check_for_cycles(pCtx, pldi, psz_recid_to, pf->prb_froms, prb_seen)  );
        SG_RBTREE_NULLFREE(pCtx, prb_seen);

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid_to, (void**) &pf)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    return;

fail:
    return;
}

static void sg_zingtx__check_for_cycles(
        SG_context* pCtx,
        SG_zingtx* pztx
        )
{
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_link_name = NULL;
    struct sg_link_dep_info* pldi = NULL;

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_dependencies, &b, &psz_link_name, (void**) &pldi)  );
    while (b)
    {
        SG_ERR_CHECK(  sg_ldi__check_for_cycles(pCtx, pldi)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link_name, (void**) &pldi)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    return;

fail:
    return;
}
#endif

static void sg_zingtx__do_one_unique_check(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_rectype,
        const char* psz_field_name,
        const char* psz_val,
        SG_varray* pva_violations
        )
{
    SG_varray* pva = NULL;
    SG_varray* pva_crit = NULL;
    SG_varray* pva_crit_rectype = NULL;
    SG_varray* pva_crit_field = NULL;
    SG_stringarray* psa_fields = NULL;
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    const char* psz_recid_other = NULL;
    const char* psz_rectype_other = NULL;
    const char* psz_val_other = NULL;
    SG_zingrecord* pzrec_other = NULL;
    SG_rbtree* prb_matches = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_matches)  );

    /* construct the crit */
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit_rectype)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_rectype, SG_ZING_FIELD__RECTYPE)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_rectype, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_rectype, psz_rectype)  );

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit_field)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_field, psz_field_name)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_field, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_field, psz_val)  );

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, &pva_crit_rectype)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "&&")  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, &pva_crit_field)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__RECID)  );
    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pztx->pRepo, pztx->iDagNum, pztx->psz_csid, pva_crit, psa_fields, &pva)  );
    SG_VARRAY_NULLFREE(pCtx, pva_crit);

    if (pva)
    {
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
        for (i=0; i<count; i++)
        {
            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, 0, &psz_recid_other)  );
            SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb_matches, psz_recid_other)  );
        }
        SG_VARRAY_NULLFREE(pCtx, pva);
    }

    // now iterate over all dirty records
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_records, &b, &psz_recid_other, (void**) &pzrec_other)  );
    while (b)
    {
        SG_bool bDeleteMe = SG_FALSE;

        SG_ERR_CHECK(  SG_zingrecord__is_delete_me(pCtx, pzrec_other, &bDeleteMe)  );

        if (!bDeleteMe)
        {
            if (pzrec_other->b_dirty_fields)
            {
                SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec_other, &psz_rectype_other)  );
                if (0 == strcmp(psz_rectype, psz_rectype_other))
                {
                    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzrec_other->prb_field_values, psz_field_name, NULL, (void**) &psz_val_other)  );
                    if (0 == strcmp(psz_val_other, psz_val))
                    {
                        SG_ERR_CHECK(  SG_rbtree__update(pCtx, prb_matches, psz_recid_other)  );
                    }
                }
            }
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid_other, (void**) &pzrec_other)  );
    }

    SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_matches, &count)  );
    SG_ASSERT(count != 0);
    if (count > 1)
    {
        SG_ERR_CHECK(  sg_zing__add_error_object__idarray(pCtx, pva_violations,
                    SG_ZING_CONSTRAINT_VIOLATION__IDS, prb_matches,
                    SG_ZING_CONSTRAINT_VIOLATION__TYPE, "unique",
                    SG_ZING_CONSTRAINT_VIOLATION__RECTYPE, psz_rectype,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, psz_field_name,
                    SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE, psz_val,
                    NULL) );
    }

    // fall thru

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_matches);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
}

static void sg_zingtx__do_unique_checks(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_vhash* pvh_unique_checks,
        SG_varray* pva_violations
        )
{
    SG_uint32 count = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_unique_checks, &count)  );
    for (i=0; i<count; i++)
    {
		const char* psz_key = NULL;
		const SG_variant* pv = NULL;
        SG_vhash* pvh = NULL;
        const char* psz_rectype = NULL;
        const char* psz_field_name = NULL;
        const char* psz_val = NULL;

		SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_unique_checks, i, &psz_key, &pv)  );
        SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &pvh)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "rectype", &psz_rectype)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "field", &psz_field_name)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "val", &psz_val)  );
        SG_ERR_CHECK(  sg_zingtx__do_one_unique_check(pCtx, pztx, psz_rectype, psz_field_name, psz_val, pva_violations)  );
    }

fail:
    ;
}

static void sg_zingrecord__find_unique_checks_to_be_done(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zingtemplate* pztemplate,
        SG_zingrecord* pzrec,
        SG_vhash* pvh_unique_checks
        )
{
    SG_stringarray* psa_fields = NULL;
    const char* psz_rectype = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;
    SG_zingfieldattributes* pzfa = NULL;
    SG_vhash* pvh = NULL;
    SG_string* pstr = NULL;

    SG_UNUSED(pztx);

    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec, &psz_rectype)  );

    SG_ERR_CHECK(  SG_zingtemplate__list_fields(pCtx, pztemplate, psz_rectype, &psa_fields)  );
    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_fields, &count )  );
    for (i=0; i<count; i++)
    {
        const char* psz_field_name = NULL;

        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa_fields, i, &psz_field_name)  );
        SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype, psz_field_name, &pzfa)  );

        switch (pzfa->type)
        {
            case SG_ZING_TYPE__INT:
                {
                    if (pzfa->v._int.unique)
                    {
                        const char* psz_val = NULL;
                        SG_bool b_already = SG_FALSE;

                        SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzrec->prb_field_values, pzfa->name, NULL, (void**) &psz_val)  );

                        SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
                        SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr, "%s.%s.%s", psz_rectype, psz_field_name, psz_val)  );
                        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_unique_checks, SG_string__sz(pstr), &b_already)  );
                        if (!b_already)
                        {
                            SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
                            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "rectype", psz_rectype)  );
                            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "field", psz_field_name)  );
                            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "val", psz_val)  );
                            SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh_unique_checks, SG_string__sz(pstr), &pvh)  );
                        }
                        SG_STRING_NULLFREE(pCtx, pstr);
                    }
                    break;
                }
            case SG_ZING_TYPE__STRING:
                {
                    if (pzfa->v._string.unique)
                    {
                        const char* psz_val = NULL;
                        SG_bool b_already = SG_FALSE;

                        SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzrec->prb_field_values, pzfa->name, NULL, (void**) &psz_val)  );

                        SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
                        SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr, "%s.%s.%s", psz_rectype, psz_field_name, psz_val)  );
                        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_unique_checks, SG_string__sz(pstr), &b_already)  );
                        if (!b_already)
                        {
                            SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
                            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "rectype", psz_rectype)  );
                            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "field", psz_field_name)  );
                            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "val", psz_val)  );
                            SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh_unique_checks, SG_string__sz(pstr), &pvh)  );
                        }
                        SG_STRING_NULLFREE(pCtx, pstr);
                    }
                    break;
                }
        }
    }

    // fall thru

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void sg_zingrecord__check_intrarecord(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        SG_zingrecord* pzrec,
        SG_varray* pva_violations
        )
{
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_field_name = NULL;
    const char* psz_field_val = NULL;
    const char* psz_rectype = NULL;
    SG_zingfieldattributes* pzfa = NULL;
    const char* psz_recid = NULL;

    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec, &psz_rectype)  );
    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzrec, &psz_recid)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pzrec->prb_field_values, &b, &psz_field_name, (void**) &psz_field_val)  );
    while (b)
    {
        if (
                (0 == strcmp(psz_field_name, SG_ZING_FIELD__RECID))
                || (0 == strcmp(psz_field_name, SG_ZING_FIELD__RECTYPE))
           )
        {
            goto continue_loop;
        }

        SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype, psz_field_name, &pzfa)  );
        if (!pzfa)
        {
            SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                        SG_ZING_CONSTRAINT_VIOLATION__TYPE, "illegal_field",
                        SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                        SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, psz_field_name,
                        NULL) );

            goto continue_loop;
        }

        switch (pzfa->type)
        {
            case SG_ZING_TYPE__BOOL:
            {
                SG_int64 ival = 0;

                SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &ival, psz_field_val)  );
                SG_ERR_CHECK(  sg_zing__check_field__bool(pCtx, pzfa, psz_recid, ival != 0, pva_violations)  );
            }
            break;

            case SG_ZING_TYPE__DATETIME:
            {
                SG_int64 ival = 0;

                SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &ival, psz_field_val)  );
                SG_ERR_CHECK(  sg_zing__check_field__datetime(pCtx, pzfa, psz_recid, ival, pva_violations)  );
            }
            break;

            case SG_ZING_TYPE__INT:
            {
                SG_int64 ival = 0;

                SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &ival, psz_field_val)  );
                SG_ERR_CHECK(  sg_zing__check_field__int(pCtx, pzfa, psz_recid, ival, pva_violations)  );
            }
            break;

            case SG_ZING_TYPE__STRING:
            {
                SG_ERR_CHECK(  sg_zing__check_field__string(pCtx, pzfa, psz_recid, psz_field_val, pva_violations)  );
            }
            break;

            case SG_ZING_TYPE__USERID:
            {
                SG_ERR_CHECK(  sg_zing__check_field__userid(pCtx, pzfa, psz_recid, psz_field_val, pva_violations)  );
            }
            break;

            case SG_ZING_TYPE__DAGNODE:
            {
                SG_ERR_CHECK(  sg_zing__check_field__dagnode(pCtx, pzfa, psz_recid, psz_field_val, pva_violations)  );
            }
            break;

            case SG_ZING_TYPE__ATTACHMENT:
            {
                // TODO
            }
            break;

            default:
                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                break;
        }

continue_loop:
        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_field_name, (void**) &psz_field_val)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // fall thru

fail:
    ;
}

static void sg_zingrecord__check_required_fields(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        SG_zingrecord* pzrec,
        SG_varray* pva_violations
        )
{
    SG_stringarray* psa_fields = NULL;
    const char* psz_rectype = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;
    SG_zingfieldattributes* pzfa = NULL;

    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec, &psz_rectype)  );

    SG_ERR_CHECK(  SG_zingtemplate__list_fields(pCtx, pztemplate, psz_rectype, &psa_fields)  );
    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_fields, &count )  );
    for (i=0; i<count; i++)
    {
        const char* psz_field_name = NULL;

        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa_fields, i, &psz_field_name)  );
        SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pztemplate, psz_rectype, psz_field_name, &pzfa)  );

        if (pzfa->required)
        {
            SG_bool b = SG_FALSE;

            SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzrec->prb_field_values, psz_field_name, &b, NULL)  );
            if (!b)
            {
                const char* psz_recid = NULL;
                SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzrec, &psz_recid)  );

                SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                            SG_ZING_CONSTRAINT_VIOLATION__TYPE, "required_field",
                            SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                            SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, pzfa->name,
                            NULL) );
            }
        }
    }

    // fall thru

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
}

static void x__check_singular_links(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid,
        const char* psz_rectype,
        SG_rbtree* prb_singular_links,
        SG_bool b_from,
        SG_varray* pva_violations
        )
{
    SG_rbtree* prb_links = NULL;
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;

	SG_UNUSED(psz_rectype);

    if (prb_singular_links)
    {
        const char* psz_link_name = NULL;
        const char* psz_side_name = NULL;

        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_singular_links, &b, &psz_link_name, (void**) &psz_side_name)  );
        while (b)
        {
            SG_uint32 count = 0;

            SG_ERR_CHECK(  sg_zingtx__find_all_links_to_or_from(pCtx, pztx, psz_recid, NULL, psz_link_name, b_from, &prb_links)  );

            count = 0;
            if (prb_links)
            {
                SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_links, &count)  );
            }

            SG_RBTREE_NULLFREE(pCtx, prb_links);

            if (count > 1)
            {
                SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                            SG_ZING_CONSTRAINT_VIOLATION__TYPE, "singular_link",
                            SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                            SG_ZING_CONSTRAINT_VIOLATION__LINK_NAME, psz_link_name,
                            SG_ZING_CONSTRAINT_VIOLATION__LINK_SIDE_NAME, psz_side_name,
                            NULL) );
            }

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link_name, (void**) &psz_side_name)  );
        }
    }

    // fall thru

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_links);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
}

static void x__check_required_links(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid,
        const char* psz_rectype,
        SG_rbtree* prb_required_links,
        SG_bool b_from,
        SG_varray* pva_violations
        )
{
    SG_rbtree* prb_links = NULL;
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;

	SG_UNUSED(psz_rectype);

    if (prb_required_links)
    {
        const char* psz_link_name = NULL;
        const char* psz_side_name = NULL;

        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_required_links, &b, &psz_link_name, (void**) &psz_side_name)  );
        while (b)
        {
            SG_uint32 count = 0;

            SG_ERR_CHECK(  sg_zingtx__find_all_links_to_or_from(pCtx, pztx, psz_recid, NULL, psz_link_name, b_from, &prb_links)  );

            count = 0;
            if (prb_links)
            {
                SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_links, &count)  );
            }

            SG_RBTREE_NULLFREE(pCtx, prb_links);

            if (!count)
            {
                SG_ERR_CHECK(  sg_zing__add_error_object(pCtx, pva_violations,
                            SG_ZING_CONSTRAINT_VIOLATION__TYPE, "required_link",
                            SG_ZING_CONSTRAINT_VIOLATION__RECID, psz_recid,
                            SG_ZING_CONSTRAINT_VIOLATION__LINK_NAME, psz_link_name,
                            SG_ZING_CONSTRAINT_VIOLATION__LINK_SIDE_NAME, psz_side_name,
                            NULL) );
            }

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link_name, (void**) &psz_side_name)  );
        }
    }

    // fall thru

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_links);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
}

static void sg_zingrecord__check_singular_links(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zingtemplate* pztemplate,
        SG_zingrecord* pzrec,
        SG_varray* pva_violations
        )
{
    const char* psz_rectype = NULL;
    SG_rbtree* prb_singular_links_from = NULL;
    SG_rbtree* prb_singular_links_to = NULL;
    const char* psz_recid = NULL;

    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec, &psz_rectype)  );
    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzrec, &psz_recid)  );

    SG_ERR_CHECK(  SG_zingtemplate__list_singular_links(pCtx, pztemplate, psz_rectype, &prb_singular_links_from, &prb_singular_links_to)  );

    SG_ERR_CHECK(  x__check_singular_links(pCtx, pztx, psz_recid, psz_rectype, prb_singular_links_from, SG_TRUE, pva_violations)  );
    SG_ERR_CHECK(  x__check_singular_links(pCtx, pztx, psz_recid, psz_rectype, prb_singular_links_to, SG_FALSE, pva_violations)  );

    // fall thru

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_singular_links_from);
    SG_RBTREE_NULLFREE(pCtx, prb_singular_links_to);
}

static void sg_zingrecord__check_required_links(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zingtemplate* pztemplate,
        SG_zingrecord* pzrec,
        SG_varray* pva_violations
        )
{
    const char* psz_rectype = NULL;
    SG_rbtree* prb_required_links_from = NULL;
    SG_rbtree* prb_required_links_to = NULL;
    const char* psz_recid = NULL;

    SG_ERR_CHECK(  SG_zingrecord__get_rectype(pCtx, pzrec, &psz_rectype)  );
    SG_ERR_CHECK(  SG_zingrecord__get_recid(pCtx, pzrec, &psz_recid)  );

    SG_ERR_CHECK(  SG_zingtemplate__list_required_links(pCtx, pztemplate, psz_rectype, &prb_required_links_from, &prb_required_links_to)  );

    SG_ERR_CHECK(  x__check_required_links(pCtx, pztx, psz_recid, psz_rectype, prb_required_links_from, SG_TRUE, pva_violations)  );
    SG_ERR_CHECK(  x__check_required_links(pCtx, pztx, psz_recid, psz_rectype, prb_required_links_to, SG_FALSE, pva_violations)  );

    // fall thru

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_required_links_from);
    SG_RBTREE_NULLFREE(pCtx, prb_required_links_to);
}

#if 0
static void sg_zingtx__add_recheck(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_rectype,
        const char* psz_where
        )
{
    const char* psz_cur = NULL;
    SG_bool b = SG_FALSE;

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pztx->prb_rechecks, psz_rectype, &b, (void**) &psz_cur)  );

    // TODO perform an OR operation
    // sprintf(newstring, "(%s) || (%s)", prev_string, psz_where)
    // if either string is "", that means just recheck everything,
    // so set the whole thing to ""

fail:
    ;
}
#endif

#if 1
static void sg_zingtx__figure_out_template_change(
        SG_context* pCtx,
        SG_zingtx* pztx
        )
{
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_rectype = NULL;

    // when the template has changed, we need to re-run
    // constraint checks on records currently in the
    // database.

    // The following code is WAY less efficient than it could be.
    //
    // When this code is smarter, it will compare the template to the
    // one we started with and figure out what has changed.  Then it
    // will create queries which describe the records that need to be
    // rechecked.  At the very least, this will mean that when somebody
    // tweaks the template for one rectype, we only need to recheck
    // records of that rectype.  If the code is really smart, it can
    // figure out that when somebody changes the max on a field from
    // 5 to 9, no records need to be rechecked at all.
    //
    // For now, we play dumb.  Anytime the template changes, we
    // trigger a recheck of every record in every rectype.  This
    // means that template changes will be really expensive when the
    // db already has lots of records in it.

    SG_ERR_CHECK(  SG_zingtemplate__list_rectypes(pCtx, pztx->ptemplate, &pztx->prb_rechecks)  );
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_rechecks, &b, &psz_rectype, NULL)  );
    while (b)
    {
        // the empty string "" is a query which matches all records of that rectype
        SG_ERR_CHECK(  SG_rbtree__update__with_pooled_sz(pCtx, pztx->prb_rechecks, psz_rectype, "")  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_rectype, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

fail:
    return;
}

static void sg_zingtx__do_one_recheck(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_rectype,
        const char* psz_where,
        SG_varray* pva_violations
        )
{
    SG_varray* pva = NULL;
    SG_dbrecord* pdbrec = NULL;
    SG_zingrecord* pzrec = NULL;
    SG_stringarray* psa_fields = NULL;

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "_rechid")  );
    SG_ERR_CHECK(  sg_zing__query__template(
                pCtx,
                pztx->pRepo,
                pztx->iDagNum,
                pztx->ptemplate,
                pztx->psz_csid,
                psz_rectype,
                psz_where,
                NULL,
                0,
                0,
                psa_fields,
                &pva
                )  );

    if (pva)
    {
        SG_uint32 count = 0;
        SG_uint32 i = 0;

        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count )  );
        for (i=0; i<count; i++)
        {
            const char* psz_hid_rec = NULL;
            const char* psz_recid = NULL;
            SG_bool b_already_loaded = SG_FALSE;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, i, &psz_hid_rec)  );
            // TODO load the dbrecord, stuff it into a zing record, check it,
            // then unload it.
            SG_ERR_CHECK(  SG_dbrecord__load_from_repo(pCtx, pztx->pRepo, psz_hid_rec, &pdbrec)  );
            SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, pdbrec, SG_ZING_FIELD__RECID, &psz_recid)  );
            b_already_loaded = SG_FALSE;
            SG_ERR_CHECK(  SG_rbtree__find(pCtx, pztx->prb_records, psz_recid, &b_already_loaded, (void**) &pzrec)  );
            if (b_already_loaded)
            {
                // TODO
            }
            else
            {
                SG_ERR_CHECK(  SG_zingrecord__alloc__from_dbrecord(pCtx, pztx, pdbrec, SG_TRUE, &pzrec)  );
                //SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pztx->prb_records, psz_recid, pzrec)  );
                // TODO what checks should be done here?
                SG_ERR_CHECK(  sg_zingrecord__check_required_fields(pCtx, pztx->ptemplate, pzrec, pva_violations)  );
                SG_ERR_CHECK(  sg_zingrecord__check_intrarecord(pCtx, pztx->ptemplate, pzrec, pva_violations)  );

                SG_DBRECORD_NULLFREE(pCtx, pdbrec);

                // now remove the pzrec
                //SG_ERR_CHECK(  SG_rbtree__remove(pCtx, pztx->prb_records, psz_recid)  );
                SG_ZINGRECORD_NULLFREE(pCtx, pzrec);
            }
        }

        SG_VARRAY_NULLFREE(pCtx, pva);
    }

fail:
    SG_DBRECORD_NULLFREE(pCtx, pdbrec);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
}

static void sg_zingtx__do_rechecks(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_varray* pva_violations
        )
{
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_rectype = NULL;
    const char* psz_where = NULL;

    if (pztx->prb_rechecks)
    {
        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_rechecks, &b, &psz_rectype, (void**) &psz_where)  );
        while (b)
        {
            SG_ERR_CHECK(  sg_zingtx__do_one_recheck(pCtx, pztx, psz_rectype, psz_where, pva_violations)  );

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_rectype, (void**) &psz_where)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    }

fail:
    ;
}
#endif

static void sg_zingtx__check_commit_time_constraints(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_varray* pva_violations
        )
{
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    const char* psz_recid = NULL;
    SG_zingrecord* pzrec = NULL;
    SG_zingtemplate* pztemplate = NULL;
    SG_vhash* pvh_unique_checks = NULL;

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh_unique_checks)  );

    // ask the tx for its template
    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pztemplate)  );

    // now iterate over all dirty records
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_records, &b, &psz_recid, (void**) &pzrec)  );
    while (b)
    {
        SG_bool bDeleteMe = SG_FALSE;

        SG_ERR_CHECK(  SG_zingrecord__is_delete_me(pCtx, pzrec, &bDeleteMe)  );

        if (!bDeleteMe)
        {
            if (pzrec->b_dirty_fields)
            {
                SG_ERR_CHECK(  sg_zingrecord__check_required_fields(pCtx, pztemplate, pzrec, pva_violations)  );
                SG_ERR_CHECK(  sg_zingrecord__check_intrarecord(pCtx, pztemplate, pzrec, pva_violations)  );
                SG_ERR_CHECK(  sg_zingrecord__find_unique_checks_to_be_done(pCtx, pztx, pztemplate, pzrec, pvh_unique_checks)  );
            }

            if (pzrec->b_dirty_links || pzrec->b_dirty_fields)
            {
                SG_ERR_CHECK(  sg_zingrecord__check_required_links(pCtx, pztx, pztemplate, pzrec, pva_violations)  );
                SG_ERR_CHECK(  sg_zingrecord__check_singular_links(pCtx, pztx, pztemplate, pzrec, pva_violations)  );
            }
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid, (void**) &pzrec)  );
    }

    SG_ERR_CHECK(  sg_zingtx__do_unique_checks(pCtx, pztx, pvh_unique_checks, pva_violations)  );

    // fall thru

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_unique_checks);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
}

static void sg_zingtx__invoke_actions(
        SG_context* pCtx,
        SG_zingtx* pztx
        )
{
	SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_link_name = NULL;
    struct sg_link_dep_info* pldi = NULL;

    // loop until everything is finished up
    while (1)
    {
        SG_uint32 done = 0;

        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_dependencies, &b, &psz_link_name, (void**) &pldi)  );
        while (b)
        {
            SG_uint32 count = 0;

            SG_ERR_CHECK(  bc__perform_actions(pCtx, pldi, &count)  );

            done += count;

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link_name, (void**) &pldi)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

        if (0 == done)
        {
            break;
        }
    }

    return;

fail:
    return;
}

void SG_zing__commit_tx(
        SG_context* pCtx,
        SG_int64 when,
        SG_zingtx** ppztx,
        SG_changeset** ppcs,
        SG_dagnode** ppdn,
        SG_varray** ppva_constraint_violations
        )
{
    SG_pendingdb* pdb = NULL;
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    const char* psz_recid = NULL;
	SG_rbtree_iterator* pit_att = NULL;
	SG_bool b_att = SG_FALSE;
    const char* psz_att_fieldname = NULL;
    SG_pathname* pPath_att = NULL;
    char* psz_hid_rec = NULL;
    SG_zingrecord* pzrec = NULL;
    SG_dbrecord* pdbrec = NULL;
    const char* psz_hid_link = NULL;
    const char* psz_link = NULL;
	SG_zingtx* pztx = NULL;
    const char* psz_csid_parent = NULL;
    SG_varray* pva_violations = NULL;
    SG_uint32 count_violations = 0;
    SG_audit q;

	if(ppztx==NULL||*ppztx==NULL)
    {
		return;
    }

	pztx = *ppztx;

    if (!SG_DAGNUM__IS_AUDIT(pztx->iDagNum))
    {
        SG_ERR_CHECK(  SG_strcpy(pCtx, q.who_szUserId, sizeof(q.who_szUserId), pztx->buf_who)  );
        q.when_int64 = when;
    }

#if 0
    SG_ERR_CHECK(  sg_zingtx__check_for_cycles(pCtx, pztx)  );
#endif

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_violations)  );

#if 1
    if (pztx->b_template_dirty)
    {
        SG_ERR_CHECK(  sg_zingtx__figure_out_template_change(pCtx, pztx)  );
    }

    if (pztx->psz_csid)
    {
        SG_ERR_CHECK(  sg_zingtx__do_rechecks(pCtx, pztx, pva_violations)  );
    }
    SG_RBTREE_NULLFREE(pCtx, pztx->prb_rechecks);
#endif

    SG_ERR_CHECK(  sg_zingtx__check_commit_time_constraints(pCtx, pztx, pva_violations)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_violations, &count_violations)  );
    if (count_violations)
    {
        if (ppva_constraint_violations)
        {
            *ppva_constraint_violations = pva_violations;
            pva_violations = NULL;
            goto done;
        }
        else
        {
            SG_VARRAY_NULLFREE(pCtx, pva_violations);
            SG_ERR_THROW(  SG_ERR_ZING_CONSTRAINT  );
        }
    }
    SG_VARRAY_NULLFREE(pCtx, pva_violations);

    // For now, it is illegal for link actions to add or remove records or
    // links.
    //
    // This is because we already did the cycle check.  If we allow
    // actions to modify the dependency list, then we would have
    // to redo the check.

    pztx->b_in_commit = SG_TRUE;

    SG_ERR_CHECK(  sg_zingtx__invoke_actions(pCtx, pztx)  );

    SG_ERR_CHECK(  SG_pendingdb__alloc(pCtx, pztx->pRepo, pztx->iDagNum, pztx->psz_csid, pztx->psz_hid_template, &pdb)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_parents, &b, &psz_csid_parent, NULL)  );
    while (b)
    {
        SG_ERR_CHECK(  SG_pendingdb__add_parent(pCtx, pdb, psz_csid_parent)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_csid_parent, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    if (pztx->b_template_dirty)
    {
        SG_vhash* pvh = NULL;

        SG_ERR_CHECK(  SG_zingtemplate__get_vhash(pCtx, pztx->ptemplate, &pvh)  );
        SG_ERR_CHECK(  SG_pendingdb__set_template(pCtx, pdb, pvh)  );
    }

    // first the records
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_records, &b, &psz_recid, (void**) &pzrec)  );
    while (b)
    {
        SG_bool bDeleteMe = SG_FALSE;

        SG_ERR_CHECK(  SG_zingrecord__is_delete_me(pCtx, pzrec, &bDeleteMe)  );

        SG_ERR_CHECK(  SG_zingrecord__get_original_hid(pCtx, pzrec, &psz_hid_rec)  );

        if (pzrec->prb_attachments)
        {
            SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit_att, pzrec->prb_attachments, &b_att, &psz_att_fieldname, (void**) &pPath_att)  );
            while (b_att)
            {
                char* psz_hid_att_blob = NULL;

                SG_ERR_CHECK(  SG_pendingdb__add_attachment__from_pathname(pCtx, pdb, pPath_att, &psz_hid_att_blob)  );
                SG_ERR_CHECK(  SG_rbtree__update__with_pooled_sz(pCtx, pzrec->prb_field_values, psz_att_fieldname, psz_hid_att_blob)  );
                SG_NULLFREE(pCtx, psz_hid_att_blob);

                SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit_att, &b_att, &psz_att_fieldname, (void**) &pPath_att)  );
            }
            SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit_att);
        }

        if (psz_hid_rec)
        {
            if (bDeleteMe)
            {
                SG_ERR_CHECK(  SG_pendingdb__remove(pCtx, pdb, psz_hid_rec)  );
            }
            else
            {
                if (pzrec->b_dirty_fields)
                {
                    SG_ERR_CHECK(  SG_pendingdb__remove(pCtx, pdb, psz_hid_rec)  );

                    SG_ERR_CHECK(  SG_zingrecord__to_dbrecord(pCtx, pzrec, &pdbrec)  );

                    SG_ERR_CHECK(  SG_pendingdb__add(pCtx, pdb, &pdbrec)  );
                }
            }
        }
        else
        {
            if (!bDeleteMe)
            {
                SG_ERR_CHECK(  SG_zingrecord__to_dbrecord(pCtx, pzrec, &pdbrec)  );

                SG_ERR_CHECK(  SG_pendingdb__add(pCtx, pdb, &pdbrec)  );
            }
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid, (void**) &pzrec)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // now the links to be deleted
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_links_delete, &b, &psz_hid_link, NULL)  );
    while (b)
    {
        SG_ERR_CHECK(  SG_pendingdb__remove(pCtx, pdb, psz_hid_link)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_link, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // now the newly added links
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pztx->prb_links_new, &b, &psz_link, NULL)  );
    while (b)
    {
        SG_ERR_CHECK(  sg_zinglink__to_dbrecord(pCtx, psz_link, &pdbrec)  );

        SG_ERR_CHECK(  SG_pendingdb__add(pCtx, pdb, &pdbrec)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // now commit everything
    if (SG_DAGNUM__IS_AUDIT(pztx->iDagNum))
    {
        SG_ERR_CHECK(  SG_pendingdb__commit(pCtx, pdb, NULL, ppcs, ppdn)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_pendingdb__commit(pCtx, pdb, &q, ppcs, ppdn)  );
    }

    SG_PENDINGDB_NULLFREE(pCtx, pdb);

    // TODO what is the state right now of the members of the zingtx struct?
    // Can anything happen to it now except for free() ?

    pztx->b_in_commit = SG_FALSE;
	SG_ERR_CHECK(  _sg_zingtx__free(pCtx, pztx)  );
	*ppztx = NULL;

done:

fail:
    // TODO free pendingdb
    SG_VARRAY_NULLFREE(pCtx, pva_violations);
}

void SG_zing__abort_tx(
        SG_context* pCtx,
        SG_zingtx** ppztx
        )
{
	if(ppztx==NULL||*ppztx==NULL)
		return;

    // TODO

	SG_ERR_CHECK(  _sg_zingtx__free(pCtx, *ppztx)  );
	*ppztx = NULL;

	return;
fail:
	;
}

void SG_zing__get_record__vhash(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_csid,
        const char* psz_recid,
        SG_vhash** ppvh
        )
{
    SG_varray* pva_crit = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_vhash* pvh_result = NULL;

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, SG_ZING_FIELD__RECID)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, psz_recid)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__HISTORY)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "*")  );
    SG_ERR_CHECK(  SG_repo__dbndx__query__one(pCtx, pRepo, iDagNum, psz_csid, pva_crit, psa_fields, &pvh_result)  );
    SG_VARRAY_NULLFREE(pCtx, pva_crit);

    *ppvh = pvh_result;
    pvh_result = NULL;

    // fall thru

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VHASH_NULLFREE(pCtx, pvh_result);
}

/**
 * Use this function to get a record you do not intend to modify.
 */
void SG_zing__get_record(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_csid,
        const char* psz_recid,
        SG_dbrecord** ppdbrec
        )
{
    SG_dbrecord* pdbrec = NULL;
    const char* psz_hid_rec = NULL;
    SG_varray* pva_crit = NULL;
    SG_varray* pva = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_uint32 count_results = 0;

    // look up the recid (a gid) to find the current hid
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, SG_ZING_FIELD__RECID)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, psz_recid)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "_rechid")  );
    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pRepo, iDagNum, psz_csid, pva_crit, psa_fields, &pva)  );
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    if (!pva)
    {
        SG_ERR_THROW(  SG_ERR_ZING_RECORD_NOT_FOUND  );
    }

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count_results)  );
    if(count_results==0)
    {
        SG_ERR_THROW(SG_ERR_ZING_RECORD_NOT_FOUND);
    }
    SG_ASSERT(count_results==1);

    // load the dbrecord from the repo
    SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, 0, &psz_hid_rec)  );

    SG_ERR_CHECK(  SG_dbrecord__load_from_repo(pCtx, pRepo, psz_hid_rec, &pdbrec)  );
    SG_VARRAY_NULLFREE(pCtx, pva);

    *ppdbrec = pdbrec;
    pdbrec = NULL;

    // fall thru

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_DBRECORD_NULLFREE(pCtx, pdbrec);
}

static void sg_zing__query_list__sorted(
	SG_context* pCtx,
	SG_repo* pRepo,
    SG_uint32 iDagNum,
	const char* pidState,
	const SG_varray* pcrit,
	const SG_varray* pSort,
	SG_int32 iNumRecordsToReturn,
	SG_int32 iNumRecordsToSkip,
    SG_stringarray* psa_slice_fields,
    SG_varray** ppva_sliced
	)
{
    SG_repo_qresult* pqr = NULL;
    SG_varray* pva = NULL;
    SG_uint32 count_received = 0;

    SG_ASSERT(ppva_sliced);

    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva)  );

    SG_ERR_CHECK(  SG_repo__dbndx__query(pCtx, pRepo, iDagNum, pidState, pcrit, pSort, psa_slice_fields, &pqr)  );
    if (pqr)
    {
        if (iNumRecordsToReturn > 0)
        {
            SG_uint32 count_rechid_sliced = 0;
            SG_uint32 count_remaining = 0;
            SG_uint32 count_getting = 0;

            SG_ERR_CHECK(  SG_repo__qresult__count(pCtx, pRepo, pqr, &count_rechid_sliced)  );

            if (iNumRecordsToSkip > 0)
            {
                SG_uint32 count_skipped = 0;

                SG_ERR_CHECK(  SG_repo__qresult__get__multiple(pCtx, pRepo, pqr, iNumRecordsToSkip, &count_skipped, NULL)  );
                SG_ASSERT(count_skipped == (SG_uint32) iNumRecordsToSkip);
                count_remaining = count_rechid_sliced - iNumRecordsToSkip;
            }
            else
            {
                count_remaining = count_rechid_sliced;
            }

            if (count_remaining > (SG_uint32) iNumRecordsToReturn)
            {
                count_getting = (SG_uint32) iNumRecordsToReturn;
            }
            else
            {
                count_getting = count_remaining;
            }

            SG_ERR_CHECK(  SG_repo__qresult__get__multiple(pCtx, pRepo, pqr, count_getting, &count_received, pva)  );
            SG_ASSERT(count_getting == count_received);
        }
        else
        {
            SG_ERR_CHECK(  SG_repo__qresult__get__multiple(pCtx, pRepo, pqr, -1, &count_received, pva)  );
        }
    }

    *ppva_sliced = pva;
    pva = NULL;

fail:
    if (pqr)
    {
        SG_ERR_IGNORE(  SG_repo__qresult__done(pCtx, pRepo, &pqr)  );
    }
    SG_VARRAY_NULLFREE(pCtx, pva);
}

static void sg_zing__query__one__template(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        SG_zingtemplate* pzt,
        const char* psz_csid,
        const char* psz_rectype,
        const char* psz_where,
        SG_stringarray* psa_slice_fields,
        SG_vhash** ppvh
        )
{
    SG_varray* pva_where = NULL;
    SG_bool b_is_a_rectype = SG_FALSE;
    const char* psz_where_ltrim = NULL;
    SG_uint32 count_rectypes = 0;

    SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK(  SG_zingtemplate__is_a_rectype(pCtx, pzt, psz_rectype, &b_is_a_rectype, &count_rectypes)  );
    if (!b_is_a_rectype)
    {
        SG_ERR_THROW(  SG_ERR_ZING_RECTYPE_NOT_FOUND  );
    }

    psz_where_ltrim = psz_where;
    if (psz_where_ltrim)
    {
        while (
                (*psz_where_ltrim)
                && (
                        (' ' == *psz_where_ltrim)
                        || ('\t' == *psz_where_ltrim)
                        || ('\r' == *psz_where_ltrim)
                        || ('\n' == *psz_where_ltrim)
                   )
              )
        {
            psz_where_ltrim++;
        }

        if (!*psz_where_ltrim)
        {
            psz_where_ltrim = NULL;
        }
    }

    SG_ERR_CHECK(  sg_zing__query__parse_where(pCtx, pzt, psz_rectype, psz_where_ltrim, (count_rectypes > 1), &pva_where)  );

    SG_ERR_CHECK(  SG_repo__dbndx__query__one(pCtx, pRepo, iDagNum, psz_csid, pva_where, psa_slice_fields, ppvh)  );

    // fall thru

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_where);
}

static void sg_zing__query__template(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        SG_zingtemplate* pzt,
        const char* psz_csid,
        const char* psz_rectype,
        const char* psz_where,
        const char* psz_sort,
        SG_int32 limit,
        SG_int32 skip,
        SG_stringarray* psa_slice_fields,
        SG_varray** ppva_sliced
        )
{
    SG_varray* pva_where = NULL;
    SG_varray* pva_sort = NULL;
    SG_bool b_is_a_rectype = SG_FALSE;
    const char* psz_where_ltrim = NULL;
    SG_bool b_include_rectype_in_query = SG_TRUE;
    SG_uint32 count_rectypes = 0;

    SG_NULLARGCHECK_RETURN(pRepo);

    SG_ERR_CHECK(  SG_zingtemplate__is_a_rectype(pCtx, pzt, psz_rectype, &b_is_a_rectype, &count_rectypes)  );
    if (!b_is_a_rectype)
    {
        SG_ERR_THROW(  SG_ERR_ZING_RECTYPE_NOT_FOUND  );
    }

    psz_where_ltrim = psz_where;
    if (psz_where_ltrim)
    {
        while (
                (*psz_where_ltrim)
                && (
                        (' ' == *psz_where_ltrim)
                        || ('\t' == *psz_where_ltrim)
                        || ('\r' == *psz_where_ltrim)
                        || ('\n' == *psz_where_ltrim)
                   )
              )
        {
            psz_where_ltrim++;
        }

        if (!*psz_where_ltrim)
        {
            psz_where_ltrim = NULL;
        }
    }

    if (1 == count_rectypes)
    {
        b_include_rectype_in_query = SG_FALSE;
    }

    SG_ERR_CHECK(  sg_zing__query__parse_where(pCtx, pzt, psz_rectype, psz_where_ltrim, b_include_rectype_in_query, &pva_where)  );

    if (psz_sort)
    {
        SG_ERR_CHECK(  sg_zing__query__parse_sort(pCtx, pzt, psz_rectype, psz_sort, &pva_sort)  );
    }

    if (pva_sort)
    {
        SG_ERR_CHECK(  sg_zing__query_list__sorted(pCtx, pRepo, iDagNum, psz_csid, pva_where, pva_sort, limit, skip, psa_slice_fields, ppva_sliced)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pRepo, iDagNum, psz_csid, pva_where, psa_slice_fields, ppva_sliced)  );
    }

    // fall thru

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_where);
    SG_VARRAY_NULLFREE(pCtx, pva_sort);
}

void SG_zing__query__one(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_csid,
        const char* psz_rectype,
        const char* psz_where,
        SG_stringarray* psa_slice_fields,
        SG_vhash** ppvh
        )
{
    SG_zingtemplate* pzt = NULL;

    SG_NULLARGCHECK_RETURN(psz_csid);

    SG_ERR_CHECK(  SG_zing__get_template__csid(pCtx, pRepo, iDagNum, psz_csid, &pzt)  );

    SG_ERR_CHECK(  sg_zing__query__one__template(pCtx, pRepo, iDagNum, pzt, psz_csid, psz_rectype, psz_where, psa_slice_fields, ppvh)  );

    // fall thru

fail:
    ;
}

void SG_zing__query_across_states(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_csid,
        const char* psz_rectype,
        const char* psz_where,
        SG_int32 gMin,
        SG_int32 gMax,
        SG_vhash** ppResult
        )
{
    SG_zingtemplate* pzt = NULL;
    SG_varray* pva_where = NULL;
    SG_vhash* pvh = NULL;
    SG_bool b_is_a_rectype = SG_FALSE;
    const char* psz_where_ltrim = NULL;
    SG_uint32 count_rectypes = 0;

    SG_ERR_CHECK(  sg_zing__load_template__csid(pCtx, pRepo, psz_csid, &pzt)  );

    SG_ERR_CHECK(  SG_zingtemplate__is_a_rectype(pCtx, pzt, psz_rectype, &b_is_a_rectype, &count_rectypes)  );
    if (!b_is_a_rectype)
    {
        SG_ERR_THROW(  SG_ERR_ZING_RECTYPE_NOT_FOUND  );
    }

    psz_where_ltrim = psz_where;
    if (psz_where_ltrim)
    {
        while (
                (*psz_where_ltrim)
                && (
                        (' ' == *psz_where_ltrim)
                        || ('\t' == *psz_where_ltrim)
                        || ('\r' == *psz_where_ltrim)
                        || ('\n' == *psz_where_ltrim)
                   )
              )
        {
            psz_where_ltrim++;
        }

        if (!*psz_where_ltrim)
        {
            psz_where_ltrim = NULL;
        }
    }

    SG_ERR_CHECK(  sg_zing__query__parse_where(pCtx, pzt, psz_rectype, psz_where_ltrim, SG_TRUE, &pva_where)  );

    SG_ERR_CHECK(  SG_repo__dbndx__query_across_states(pCtx, pRepo, iDagNum, pva_where, gMin, gMax, &pvh)  );

    *ppResult = pvh;
    pvh = NULL;

    // fall thru

fail:
    SG_ZINGTEMPLATE_NULLFREE(pCtx, pzt);
    SG_VHASH_NULLFREE(pCtx, pvh);
    SG_VARRAY_NULLFREE(pCtx, pva_where);
}

void SG_zing__extract_one_from_slice__string(
        SG_context* pCtx,
        SG_varray* pva,
        const char* psz_name,
        SG_varray** ppva2
        )
{
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_varray* pva2 = NULL;

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    SG_ERR_CHECK(  SG_varray__alloc__params(pCtx, &pva2, count, NULL, NULL)  );
    for (i=0; i<count; i++)
    {
        const char* psz_val = NULL;
        SG_vhash* pvh = NULL;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, psz_name, &psz_val)  );
        SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva2, psz_val)  );
    }

    *ppva2 = pva2;
    pva2 = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva2);
}

void SG_zingtx__lookup_recid(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_rectype,
        const char* psz_field_name,
        const char* psz_field_value,
        char** ppsz_recid
        )
{
    SG_ERR_CHECK_RETURN(  SG_zing__lookup_recid(pCtx, pztx->pRepo, pztx->iDagNum, pztx->psz_csid, psz_rectype, psz_field_name, psz_field_value, ppsz_recid)  );
}

void SG_zing__lookup_recid(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_state,
        const char* psz_rectype,
        const char* psz_field_name,
        const char* psz_field_value,
        char** ppsz_recid
        )
{
    SG_string* pstr_where = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    const char* psz_recid = NULL;
    SG_uint32 count_results = 0;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );

    // TODO fix escaping bug below
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s == '%s'", psz_field_name, psz_field_value)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, iDagNum, psz_state, psz_rectype, SG_string__sz(pstr_where), NULL, 0, 0, psa_fields, &pva)  );
    if (!pva)
    {
        SG_ERR_THROW(  SG_ERR_ZING_RECORD_NOT_FOUND  );
    }

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count_results)  );

    if(count_results==0)
    {
        SG_ERR_THROW(SG_ERR_ZING_RECORD_NOT_FOUND);
    }

    if(count_results > 1)
    {
        SG_ERR_THROW(SG_ERR_ZING_RECORD_NOT_FOUND);  // TODO better error
    }

    SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, 0, &psz_recid)  );
    SG_ERR_CHECK(  SG_strdup(pCtx, psz_recid, ppsz_recid)  );

fail:
    SG_STRING_NULLFREE(pCtx, pstr_where);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva);
}

void SG_zing__query(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_csid,
        const char* psz_rectype,
        const char* psz_where,
        const char* psz_sort,
        SG_int32 limit,
        SG_int32 skip,
        SG_stringarray* psa_slice_fields,
        SG_varray** ppva_sliced
        )
{
    SG_zingtemplate* pzt = NULL;

    SG_NULLARGCHECK_RETURN(psz_csid);

    SG_ERR_CHECK(  SG_zing__get_template__csid(pCtx, pRepo, iDagNum, psz_csid, &pzt)  );

    SG_ERR_CHECK(  sg_zing__query__template(pCtx, pRepo, iDagNum, pzt, psz_csid, psz_rectype, psz_where, psz_sort, limit, skip, psa_slice_fields, ppva_sliced)  );

    // fall thru

fail:
    ;
}

void SG_zing__get_leaf__fail_if_needs_merge(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        char** pp
        )
{
    SG_rbtree* prb_leaves = NULL;
    const char* psz_hid_cs_leaf = NULL;
    SG_uint32 count_leaves = 0;

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, iDagNum, &prb_leaves)  );
    SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_leaves, &count_leaves)  );

    if (0 == count_leaves)
    {
        *pp = NULL;
    }
    else if (1 == count_leaves)
    {
        SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, prb_leaves, &psz_hid_cs_leaf, NULL)  );
        SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_hid_cs_leaf, pp)  );
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_ZING_NEEDS_MERGE  );
    }

    // fall thru

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_leaves);
}

void SG_zing__get_leaf__merge_if_necessary(
        SG_context* pCtx,
        SG_repo* pRepo,
        const SG_audit* pq,
        SG_uint32 iDagNum,
        char** pp,
        SG_varray** ppva_errors,
        SG_varray** ppva_log
        )
{
    SG_rbtree* prb_leaves = NULL;
    const char* psz_hid_cs_leaf = NULL;
    SG_dagnode* pdn_merged = NULL;
    SG_varray* pva_log = NULL;
    SG_varray* pva_errors = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pp);
	SG_NULLARGCHECK_RETURN(ppva_errors);
	SG_NULLARGCHECK_RETURN(ppva_log);

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_log)  );
    *ppva_log = pva_log;

    while (1)
    {
        const char* aleaves[2];
        SG_uint32 count_leaves = 0;

        SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, iDagNum, &prb_leaves)  );
        SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_leaves, &count_leaves)  );

        if (0 == count_leaves)
        {
            *pp = NULL;
            break;
        }

        if (1 == count_leaves)
        {
            SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, prb_leaves, &psz_hid_cs_leaf, NULL)  );
            SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_hid_cs_leaf, pp)  );
            break;
        }

        if (count_leaves > 2)
        {
            // TODO select the best two leaves for merging.  for now
            // we just take the first two.

            SG_uint32 i = 0;
            const char* psz_csid = NULL;

            SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_leaves, &b, &psz_csid, NULL)  );
            while (b)
            {
                aleaves[i++] = psz_csid;
                if (2 == i)
                {
                    break;
                }

                SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_csid, NULL)  );
            }
            SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

            SG_ASSERT(2 == i);
        }
        else
        {
            SG_uint32 i = 0;
            const char* psz_csid = NULL;

            SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_leaves, &b, &psz_csid, NULL)  );
            while (b)
            {
                aleaves[i++] = psz_csid;

                SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_csid, NULL)  );
            }
            SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

            SG_ASSERT(2 == i);
        }

        SG_ERR_CHECK(  SG_zingmerge__attempt_automatic_merge(pCtx,
                    pRepo,
                    iDagNum,
                    pq,
                    aleaves[0],
                    aleaves[1],
                    &pdn_merged,
                    &pva_errors,
                    pva_log
                    )  );
        if (pdn_merged)
        {
            SG_DAGNODE_NULLFREE(pCtx, pdn_merged);
        }
        else
        {
            *pp = NULL;
            *ppva_errors = pva_errors;
            pva_errors = NULL;
            break;
        }
        SG_RBTREE_NULLFREE(pCtx, prb_leaves);
    }

    // fall thru

fail:
    SG_DAGNODE_NULLFREE(pCtx, pdn_merged);
    SG_RBTREE_NULLFREE(pCtx, prb_leaves);
    SG_VARRAY_NULLFREE(pCtx, pva_errors);
}

static void _auto_merge_one_dag(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_uint32 dagnum,
	const SG_audit* pAudit,
	SG_varray** ppvaErrors,
	SG_varray** ppvaLog)
{
	SG_varray* pvaErrors = NULL;
	SG_varray* pvaLog = NULL;
	char* pszLeaf = NULL;
	SG_vhash* pvhNew = NULL;

	if (SG_DAGNUM__IS_DB(dagnum))
	{
#if TRACE_PULL
		SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDOUT, "Auto-merging zing dag %d\n", dagnum)  );
#endif
		SG_ERR_CHECK(  SG_zing__get_leaf__merge_if_necessary(pCtx, pRepo, pAudit, dagnum, &pszLeaf, &pvaErrors, &pvaLog)  );
		SG_NULLFREE(pCtx, pszLeaf); // TODO: make zing handle null leaf arg, or write a merge-only routine
	}

	if (ppvaErrors && pvaErrors)
	{
		SG_uint32 errorIdx, errorCount;

		SG_ERR_CHECK(  SG_varray__count(pCtx, pvaErrors, &errorCount)  );
		if (errorCount > 0)
		{
			if (!*ppvaErrors)
				SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, ppvaErrors)  );
			for (errorIdx = 0; errorIdx < errorCount; errorIdx++)
			{
				SG_vhash* pvhNotMine = NULL;
				SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pvaErrors, errorIdx, &pvhNotMine)  );
				SG_ERR_CHECK(  SG_vhash__alloc__copy(pCtx, &pvhNew, pvhNotMine)  );
				SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, *ppvaErrors, &pvhNew)  );
			}
		}
		SG_VARRAY_NULLFREE(pCtx, pvaErrors);
	}
	if (ppvaLog && pvaLog)
	{
		SG_uint32 logIdx, logCount;

		SG_ERR_CHECK(  SG_varray__count(pCtx, pvaLog, &logCount)  );
		if (logCount > 0)
		{
			if (!*ppvaLog)
				SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx,ppvaLog)  );
			for (logIdx = 0; logIdx < logCount; logIdx++)
			{
				SG_vhash* pvhNotMine = NULL;
				SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pvaLog, logIdx, &pvhNotMine)  );
				SG_ERR_CHECK(  SG_vhash__alloc__copy(pCtx, &pvhNew, pvhNotMine)  );
				SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, *ppvaLog, &pvhNew)  );
			}

		}
		SG_VARRAY_NULLFREE(pCtx, pvaLog);
	}

	/* fall through */
fail:
	SG_VARRAY_NULLFREE(pCtx, pvaErrors);
	SG_VARRAY_NULLFREE(pCtx, pvaLog);
	SG_VHASH_NULLFREE(pCtx, pvhNew);
	SG_NULLFREE(pCtx, pszLeaf);
}


void SG_zing__auto_merge_all_dags(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_varray** ppvaErrors,
	SG_varray** ppvaLog)
{
	SG_uint32 i, count;
	SG_uint32* dagnumArray = NULL;
	SG_audit audit;

	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "Merging databases...")  );

	SG_ERR_CHECK(  SG_repo__list_dags(pCtx, pRepo, &count, &dagnumArray)  );
	// Do all the audit dags first, because other dags rely on finding a single
	// leaf in the audit dag.
	for (i = 0; i < count; i++)
	{
		SG_uint32 dagnum = dagnumArray[i];
		if (SG_DAGNUM__IS_AUDIT(dagnum))
		{
			SG_ERR_CHECK(  _auto_merge_one_dag(pCtx, pRepo, dagnum, NULL, ppvaErrors, ppvaLog)  );
		}

	}

    // and merge the users dag before the others
	SG_ERR_CHECK(  SG_audit__init__maybe_nobody(pCtx, &audit, pRepo, SG_AUDIT__WHEN__NOW, "")  );
    SG_ERR_CHECK(  _auto_merge_one_dag(pCtx, pRepo, SG_DAGNUM__USERS, &audit, ppvaErrors, ppvaLog)  );

    // now merge all the other dags
	SG_ERR_CHECK(  SG_audit__init(pCtx, &audit, pRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );

	for (i = 0; i < count; i++)
	{
		SG_uint32 dagnum = dagnumArray[i];
		if (
			!SG_DAGNUM__IS_AUDIT(dagnum)
			&& SG_DAGNUM__IS_DB(dagnum)
            && (SG_DAGNUM__USERS != dagnum)
			)
		{
			SG_ERR_CHECK(  _auto_merge_one_dag(pCtx, pRepo, dagnum, &audit, ppvaErrors, ppvaLog)  );
		}
	}

	SG_ERR_CHECK_RETURN(  SG_context__msg__emit(pCtx, "done")  );

	/* fall through */
fail:
	SG_NULLFREE(pCtx, dagnumArray);
	SG_ERR_IGNORE(  SG_context__msg__emit(pCtx, "\n")  );
}

static void my_add_pair(
        SG_context* pCtx,
        SG_vhash* pvh_to_from,
        const char* psz_key,
        const char* psz_a,
        const char* psz_b
        )
{
    SG_bool b = SG_FALSE;
    SG_vhash* pvh_sub = NULL;
    SG_varray* pva_list = NULL;

    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_to_from, psz_key, &pvh_sub)  );
    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_sub, psz_a, &b)  );
    if (b)
    {
        SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_sub, psz_a, &pva_list)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_vhash__addnew__varray(pCtx, pvh_sub, psz_a, &pva_list)  );
    }
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_list, psz_b)  );

fail:
    ;
}

void SG_zing__find_all_links(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 dagnum,
        const char* psz_hid_cs_leaf,
        SG_rbtree* prb_link_names,
        SG_vhash* pvh_top
        )
{
    SG_varray* pva_crit = NULL;
    SG_varray* pva = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_varray* pva_link_names = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_link_name = NULL;

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_link_names)  );
	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_link_names, &b, &psz_link_name, NULL)  );
	while (b)
	{
        SG_vhash* pvh_to_from = NULL;

        SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_link_names, psz_link_name)  );
        SG_ERR_CHECK(  SG_vhash__addnew__vhash(pCtx, pvh_top, psz_link_name, &pvh_to_from)  );
        SG_ERR_CHECK(  SG_vhash__addnew__vhash(pCtx, pvh_to_from, "to", NULL)  );
        SG_ERR_CHECK(  SG_vhash__addnew__vhash(pCtx, pvh_to_from, "from", NULL)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_link_name, NULL)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // TODO this crit just asks for all links where the name matches
    // one of the names given.  it does not constrain to a set of
    // recids.  when the caller of this function supports paging,
    // then it probably should do so.
    
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, SG_ZING_FIELD__LINK__NAME)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "in")  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, &pva_link_names)  );

	SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
	SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "*")  );

    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pRepo, dagnum, psz_hid_cs_leaf, pva_crit, psa_fields, &pva)  );

    if (pva)
    {
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
        for (i=0; i<count; i++)
        {
            SG_vhash* pvh_link = NULL;
            const char* psz_from = NULL;
            const char* psz_to = NULL;
            const char* psz_name = NULL;
            SG_vhash* pvh_to_from = NULL;

            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh_link)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_link, SG_ZING_FIELD__LINK__FROM, &psz_from)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_link, SG_ZING_FIELD__LINK__TO, &psz_to)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_link, SG_ZING_FIELD__LINK__NAME, &psz_name)  );
            SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_top, psz_link_name, &pvh_to_from)  );
            SG_ERR_CHECK(  my_add_pair(pCtx, pvh_to_from, "to", psz_to, psz_from)  );
            SG_ERR_CHECK(  my_add_pair(pCtx, pvh_to_from, "from", psz_from, psz_to)  );
        }
    }

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_VARRAY_NULLFREE(pCtx, pva_link_names);
}

