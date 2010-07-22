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

#ifdef WINDOWS
//#pragma warning(disable:4100)
#pragma warning(disable:4706)
#endif

struct link_changes_for_one_changeset
{
    SG_rbtree* prb_added;
    SG_rbtree* prb_deleted;
};

/** This struct represents what one changeset has done to the record(s) for a
 * given recid.
 *
 * If it simply deleted the record, then only prec_deleted will
 * be set.
 *
 * If it added the record when it was previously not present, then
 * only prec_added will be set.
 *
 * If it modified the record, then both will be set.
 */
struct record_changes_from_one_changeset
{
    SG_dbrecord* prec_deleted;
    SG_dbrecord* prec_added;
};

struct changes_for_one_record
{
    struct record_changes_from_one_changeset leaves[2];
};

struct sg_zing_merge_situation
{
    SG_zingtemplate* pztemplate_lca;

    char* psz_hid_cs_ancestor;
    const SG_audit* pq;
    const char* leaves[2];
    SG_varray* audits[2];
    SG_rbtree* prb_keys;
    struct link_changes_for_one_changeset links[2];

    SG_rbtree* prb_pending_adds;
    SG_rbtree* prb_pending_mods;
    SG_rbtree* prb_pending_deletes;

    SG_rbtree* prb_conflicts__delete_mod;
    SG_rbtree* prb_conflicts__mod_mod;
    SG_rbtree* prb_conflicts__add_add;

    SG_rbtree* prb_links__adds;
    SG_rbtree* prb_links__deletes;

    SG_vector* pvec_pcfor;
    SG_vector* pvec_dbrecords;

    SG_varray* pva_log;
};

static void my_alloc_cfor(
    SG_context* pCtx,
    struct sg_zing_merge_situation* pzms,
    struct changes_for_one_record** ppcfor
    );

static void SG_zingmerge__load_records__rbtree(
    SG_context* pCtx,
    SG_rbtree* prb,
    SG_repo* pRepo,
    SG_vector* pvec
    )
{
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* pszHid_record = NULL;
    SG_dbrecord* prec = NULL;

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb, &b, &pszHid_record, NULL)  );
    while (b)
    {
        SG_ERR_CHECK(  SG_dbrecord__load_from_repo(pCtx, pRepo, pszHid_record, &prec)  );
        SG_ERR_CHECK(  SG_vector__append(pCtx, pvec, prec, NULL)  );
        SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx, prb, pszHid_record, (void*) prec, NULL)  );
        prec = NULL;

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &pszHid_record, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    return;
fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_DBRECORD_NULLFREE(pCtx, prec);
}

/** Given a list of records deleted and added in a single changeset, this
 * function matches up the records according by recid.
 */
static void sg_zing__connect_the_edits(
    SG_context* pCtx,
    SG_rbtree* prb_records_deleted,
    SG_rbtree* prb_records_added,
    struct sg_zing_merge_situation* pzms,
    SG_uint32 ileaf
    )
{
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    SG_dbrecord* prec = NULL;
    SG_rbtree* prb_links_deleted = NULL;
    SG_rbtree* prb_links_added = NULL;
    const char* psz_hid_record = NULL;
    struct changes_for_one_record* pcfor = NULL;

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_links_added)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_links_deleted)  );

    /* First, review all the deletes */
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_records_deleted, &b, &psz_hid_record, (void**) &prec)  );
    while (b)
    {
        const char* psz_key = NULL;
        SG_bool b_has = SG_FALSE;

        SG_ERR_CHECK(  SG_dbrecord__check_value(pCtx, prec, SG_ZING_FIELD__RECID, &b_has, &psz_key)  );

        if (b_has)
        {
            SG_ASSERT(psz_key);

            SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzms->prb_keys, psz_key, &b_has, (void**) &pcfor)  );
            if (!b_has)
            {
                SG_ERR_CHECK(  my_alloc_cfor(pCtx, pzms, &pcfor)  );
                SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pzms->prb_keys, psz_key, pcfor)  );
            }

            pcfor->leaves[ileaf].prec_deleted = prec;
        }
        else
        {
            const char* psz_link_name = NULL;

            SG_ERR_CHECK(  SG_dbrecord__check_value(pCtx, prec, SG_ZING_FIELD__LINK__NAME, &b_has, &psz_link_name)  );

            // everything in a zing db is either a record with a recid and a rectype,
            // or it is a link.  nothing else is allowed.
            SG_ASSERT(b_has && psz_link_name);

            {
                const char* psz_link_from = NULL;
                const char* psz_link_to = NULL;
                struct sg_zinglink zl;

                SG_ASSERT(psz_link_name);

                SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, prec, SG_ZING_FIELD__LINK__FROM, &psz_link_from)  );
                SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, prec, SG_ZING_FIELD__LINK__TO, &psz_link_to)  );

                SG_ERR_CHECK(  sg_zinglink__pack(pCtx, psz_link_from, psz_link_to, psz_link_name, &zl)  );
                SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, prb_links_deleted, psz_hid_record, zl.buf)  );
            }
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_record, (void**) &prec)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    /* Now all the adds */
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_records_added, &b, &psz_hid_record, (void**) &prec)  );
    while (b)
    {
        const char* psz_key = NULL;
        SG_bool b_has = SG_FALSE;

        SG_ERR_CHECK(  SG_dbrecord__check_value(pCtx, prec, SG_ZING_FIELD__RECID, &b_has, &psz_key)  );

        if (b_has)
        {
            SG_ASSERT(psz_key);

            SG_ERR_CHECK(  SG_rbtree__find(pCtx, pzms->prb_keys, psz_key, &b_has, (void**) &pcfor)  );
            if (!b_has)
            {
                SG_ERR_CHECK(  my_alloc_cfor(pCtx, pzms, &pcfor)  );
                SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pzms->prb_keys, psz_key, pcfor)  );
            }

            pcfor->leaves[ileaf].prec_added = prec;
        }
        else
        {
            const char* psz_link_name = NULL;

            SG_ERR_CHECK(  SG_dbrecord__check_value(pCtx, prec, SG_ZING_FIELD__LINK__NAME, &b_has, &psz_link_name)  );

            // everything in a zing db is either a record with a recid and a rectype,
            // or it is a link.  nothing else is allowed.
            {
                const char* psz_link_from = NULL;
                const char* psz_link_to = NULL;
                struct sg_zinglink zl;

                SG_ASSERT(psz_link_name);

                SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, prec, SG_ZING_FIELD__LINK__FROM, &psz_link_from)  );
                SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, prec, SG_ZING_FIELD__LINK__TO, &psz_link_to)  );

                SG_ERR_CHECK(  sg_zinglink__pack(pCtx, psz_link_from, psz_link_to, psz_link_name, &zl)  );
                SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, prb_links_added, psz_hid_record, zl.buf)  );
            }
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_record, (void**) &prec)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    pzms->links[ileaf].prb_added = prb_links_added;
    prb_links_added = NULL;
    pzms->links[ileaf].prb_deleted = prb_links_deleted;
    prb_links_deleted = NULL;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_links_added);
    SG_RBTREE_NULLFREE(pCtx, prb_links_deleted);
}

static void my_free_cfor(SG_UNUSED_PARAM(SG_context* pCtx), void* p)
{
	SG_UNUSED(pCtx);

    SG_NULLFREE(pCtx, p);
}

static void my_alloc_cfor(
    SG_context* pCtx,
    struct sg_zing_merge_situation* pzms,
    struct changes_for_one_record** ppcfor
    )
{
    struct changes_for_one_record* p = NULL;

    SG_ERR_CHECK(  SG_alloc1(pCtx, p)  );
    SG_ERR_CHECK(  SG_vector__append(pCtx, pzms->pvec_pcfor, p, NULL)  );
    *ppcfor = p;
    p = NULL;

    // fall thru
fail:
    SG_NULLFREE(pCtx, p);
}

void SG_db__diff(
    SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_csid_ancestor,
    const char* psz_csid,
    SG_rbtree** pprb_deleted,
    SG_rbtree** pprb_added
    )
{
    SG_rbtree* prb_added = NULL;
    SG_rbtree* prb_deleted = NULL;
    SG_vhash* pvh_dbtop = NULL;
    SG_vhash* pvh_delta = NULL;
    SG_changeset* pcs = NULL;
    SG_int32 gen_ancestor = -1;
    SG_dagnode* pdn = NULL;
    char buf_csid_cur[SG_HID_MAX_BUFFER_LENGTH];

    SG_NULLARGCHECK_RETURN(pRepo);
    SG_NULLARGCHECK_RETURN(psz_csid_ancestor);
    SG_NULLARGCHECK_RETURN(psz_csid);
    SG_NULLARGCHECK_RETURN(pprb_deleted);
    SG_NULLARGCHECK_RETURN(pprb_added);

    // TODO we know the ancestor from the find_ca code.  pass it through
    // instead of looking it up here
    SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, psz_csid_ancestor, &pdn)  );
    SG_ERR_CHECK(  SG_dagnode__get_generation(pCtx, pdn, &gen_ancestor)  );
    SG_DAGNODE_NULLFREE(pCtx, pdn);

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_deleted)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_added)  );

    SG_ERR_CHECK(  SG_strcpy(pCtx, buf_csid_cur, sizeof(buf_csid_cur), psz_csid)  );
    while (1)
    {
        const char* psz_baseline = NULL;
        const char* psz_hid_delta = NULL;
        SG_varray* pva_record_adds = NULL;
        SG_varray* pva_record_removes = NULL;
        SG_uint32 count_adds = 0;
        SG_uint32 count_removes = 0;
        SG_uint32 i = 0;
        SG_int32 gen = -1;

        SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, buf_csid_cur, &pcs)  );
        SG_ERR_CHECK(  SG_changeset__get_generation(pCtx, pcs, &gen)  );
        SG_ASSERT(gen >= gen_ancestor);
        SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs,&psz_hid_delta)  );
        if (!psz_hid_delta)
        {
            SG_ERR_THROW(  SG_ERR_INVALIDARG  );
        }
        SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, pRepo, psz_hid_delta, &pvh_delta)  );
        SG_CHANGESET_NULLFREE(pCtx, pcs);

        SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_delta, "add", &pva_record_adds)  );
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva_record_adds, &count_adds)  );
        for (i=0; i<count_adds; i++)
        {
            const char* psz_hid_rec = NULL;
            SG_bool b = SG_FALSE;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_record_adds, i, &psz_hid_rec)  );
            SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_deleted, psz_hid_rec, &b, NULL)  );
            if (b)
            {
                SG_ERR_CHECK(  SG_rbtree__remove(pCtx, prb_deleted, psz_hid_rec)  );
            }
            else
            {
                SG_ERR_CHECK(  SG_rbtree__update(pCtx, prb_added, psz_hid_rec)  );
            }
        }

        SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_delta, "remove", &pva_record_removes)  );
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva_record_removes, &count_removes)  );
        for (i=0; i<count_removes; i++)
        {
            const char* psz_hid_rec = NULL;
            SG_bool b = SG_FALSE;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_record_removes, i, &psz_hid_rec)  );
            SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_added, psz_hid_rec, &b, NULL)  );
            if (b)
            {
                SG_ERR_CHECK(  SG_rbtree__remove(pCtx, prb_added, psz_hid_rec)  );
            }
            else
            {
                SG_ERR_CHECK(  SG_rbtree__update(pCtx, prb_deleted, psz_hid_rec)  );
            }
        }

        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_delta, "baseline", &psz_baseline)  );
        if (!psz_baseline)
        {
            SG_ASSERT(gen == 1);
            break;
        }
        if (0 == strcmp(psz_baseline, psz_csid_ancestor))
        {
            break;
        }
        SG_ERR_CHECK(  SG_strcpy(pCtx, buf_csid_cur, sizeof(buf_csid_cur), psz_baseline)  );
        SG_VHASH_NULLFREE(pCtx, pvh_delta);
    }

    *pprb_deleted = prb_deleted;
    prb_deleted = NULL;
    *pprb_added = prb_added;
    prb_added = NULL;

fail:
    SG_DAGNODE_NULLFREE(pCtx, pdn);
    SG_RBTREE_NULLFREE(pCtx, prb_deleted);
    SG_RBTREE_NULLFREE(pCtx, prb_added);
    SG_VHASH_NULLFREE(pCtx, pvh_delta);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_VHASH_NULLFREE(pCtx, pvh_dbtop);
}

/** This function builds "prb_keys", an rbtree containing one entry for each
 * recid.  The assoc data for each entry is another rbtree
 * which contains a list of all the changes that were made which involve that
 * recid.  Our goal here is to collect all the record changes
 * from all of the leafs and group them by recid. */
static void sg_zing__collect_and_group_all_changes(
    SG_context* pCtx,
    SG_repo* pRepo,
    struct sg_zing_merge_situation* pzms
    )
{
    SG_rbtree* prb_records_added = NULL;
    SG_rbtree* prb_records_deleted = NULL;
    SG_uint32 ileaf = 0;

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pzms->prb_keys)  );

    for (ileaf=0; ileaf<2; ileaf++)
    {
        /* Diff it */
        SG_ERR_CHECK(  SG_db__diff(pCtx, pRepo, pzms->psz_hid_cs_ancestor, pzms->leaves[ileaf], &prb_records_deleted, &prb_records_added)  );

        /* Fetch all the records that were deleted/added */
        SG_ERR_CHECK(  SG_zingmerge__load_records__rbtree(pCtx, prb_records_deleted, pRepo, pzms->pvec_dbrecords)  );
        SG_ERR_CHECK(  SG_zingmerge__load_records__rbtree(pCtx, prb_records_added, pRepo, pzms->pvec_dbrecords)  );

        /* Match things up by recid */
        SG_ERR_CHECK(  sg_zing__connect_the_edits(pCtx, prb_records_deleted, prb_records_added, pzms, ileaf)  );

        SG_RBTREE_NULLFREE(pCtx, prb_records_added);
        SG_RBTREE_NULLFREE(pCtx, prb_records_deleted);
    }

    // fall thru

fail:
    return;
}

static void sg_zingmerge__resolve_commit_errors(
    SG_context* pCtx,
    struct sg_zing_merge_situation* pzms,
    SG_zingtx* pztx,
    SG_varray* pva_errors,
    SG_bool* pb_fail
    )
{
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_zingtemplate* pzt = NULL;

    SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pzt)  );

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_errors, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh = NULL;
        const char* psz_err_type = NULL;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_errors, i, &pvh)  );

        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, SG_ZING_CONSTRAINT_VIOLATION__TYPE, &psz_err_type)  );

        if (0 == strcmp(psz_err_type, "unique"))
        {
            const char* psz_field_name = NULL;
            const char* psz_rectype = NULL;
            SG_zingfieldattributes* pzfa = NULL;

            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, SG_ZING_CONSTRAINT_VIOLATION__RECTYPE, &psz_rectype)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME, &psz_field_name)  );
            SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, psz_rectype, psz_field_name, &pzfa)  );
            if (!pzfa->uniqify)
            {
                *pb_fail = SG_TRUE;
                goto done;
            }

            if (SG_ZING_TYPE__INT == pzfa->type)
            {
                SG_ERR_CHECK(  sg_zing_uniqify_int__do(
                            pCtx,
                            pvh,
                            pzfa->uniqify,
                            pztx,
                            pzfa,
                            pzms->pq,
                            pzms->leaves,
                            pzms->psz_hid_cs_ancestor,
                            pzms->pva_log
                            )  );
            }
            else if (SG_ZING_TYPE__STRING == pzfa->type)
            {
                SG_ERR_CHECK(  sg_zing_uniqify_string__do(
                            pCtx,
                            pvh,
                            pzfa->uniqify,
                            pztx,
                            pzfa,
                            pzms->pq,
                            pzms->leaves,
                            pzms->psz_hid_cs_ancestor,
                            pzms->pva_log
                            )  );
            }
            else
            {
                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
            }
        }
        else
        {
            *pb_fail = SG_TRUE;
            goto done;
        }
    }

done:

fail:
    ;
}

/** This function takes the prb_keys data structure and figures out if there
 * are any conflicts.
 * */
static void sg_zingmerge__check_for_conflicts(
    SG_context* pCtx,
    struct sg_zing_merge_situation* pzms
    )
{
    SG_rbtree* prb_pending_adds = NULL;
    SG_rbtree* prb_pending_mods = NULL;
    SG_rbtree* prb_pending_deletes = NULL;
    SG_rbtree* prb_conflicts__delete_mod = NULL;
    SG_rbtree* prb_conflicts__mod_mod = NULL;
    SG_rbtree* prb_conflicts__add_add = NULL;
    SG_rbtree_iterator* pit_keys = NULL;
    SG_bool b_keys;
    const char* psz_key = NULL;
    struct changes_for_one_record* pcfor = NULL;

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_pending_adds)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_pending_mods)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_pending_deletes)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_conflicts__delete_mod)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_conflicts__mod_mod)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_conflicts__add_add)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit_keys, pzms->prb_keys, &b_keys, &psz_key, (void**) &pcfor)  );
    while (b_keys)
    {
        if (
            (pcfor->leaves[0].prec_added || pcfor->leaves[0].prec_deleted)
            && (pcfor->leaves[1].prec_added || pcfor->leaves[1].prec_deleted)
           )
        {
            /* Both leaves were involved, so we MIGHT have a conflict.  We
             * don't know yet.  We need to review all the changes and see. */

            SG_uint32 count_delete = 0;
            SG_uint32 count_modify = 0;
            SG_uint32 count_add = 0;
            SG_uint32 ileaf = 0;
            SG_bool b_different = SG_FALSE;

            for (ileaf=0; ileaf<2; ileaf++)
            {
                if (pcfor->leaves[ileaf].prec_added)
                {
                    /* if prec_added is set, then this changeset either
                     * modified the record or added it anew. */
                    if (pcfor->leaves[ileaf].prec_deleted)
                    {
                        count_modify++;
                    }
                    else
                    {
                        count_add++;
                    }
                }
                else
                {
                    /* if this changeset did not contribute a new version of
                     * the record, it must have simply deleted it. */
                    SG_ASSERT(pcfor->leaves[ileaf].prec_deleted);
                    count_delete++;
                }
            }

            /* either the recid was present in the ancestor or it was
             * not.  It is not possible for one leaf to have deleted the record
             * and another leaf to have added it anew. */
            SG_ASSERT(!(count_delete && count_add));

            /* it is therefore also not possible for one leaf to have modifies
             * the record and another leaf to have added it anew. */
            SG_ASSERT(!(count_modify && count_add));

            if (
                    (count_add > 1)
                    || (count_modify > 1)
               )
            {
                const char* psz_hid_0 = NULL;
                const char* psz_hid_1 = NULL;

                SG_ERR_CHECK(  SG_dbrecord__get_hid__ref(pCtx, pcfor->leaves[0].prec_added, &psz_hid_0)  );
                SG_ERR_CHECK(  SG_dbrecord__get_hid__ref(pCtx, pcfor->leaves[1].prec_added, &psz_hid_1)  );
                if (0 != strcmp(psz_hid_0, psz_hid_1))
                {
                    b_different = SG_TRUE;
                }
            }

            if (count_delete && count_modify)
            {
                /* if one changeset modified the record and another one deletes
                 * it, we just report this conflict up and let the caller decide
                 * what to do with it.
                 * */

                SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb_conflicts__delete_mod, psz_key)  );
            }
            else if (count_delete)
            {
                /* More than one leaf has deleted this record, but nobody tried
                 * to modify it. So we're all in agreement that the record
                 * should go away. */

                SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_pending_deletes, psz_key, pcfor->leaves[ileaf].prec_deleted)  );
            }
            else if (count_add)
            {
                SG_ASSERT(!count_modify);
                SG_ASSERT(2 == count_add);

                // how is this possible?  Multiple changesets
                // added the same record?  But the recid is a gid!
                // Can it happen in a merge case?  OK, but why didn't
                // the LCA code find a node before it?

                if (b_different)
                {
                    SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb_conflicts__add_add, psz_key)  );
                }
                else
                {
                    /* whew!  that was close.  it turns out that both who
                     * added this record did it in exactly the same way. */

                    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_pending_adds, psz_key, pcfor->leaves[0].prec_added)  );
                }
            }
            else
            {
                SG_ASSERT(2 == count_modify);

                if (b_different)
                {
                    /* yep, we need a record-level merge */

                    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_conflicts__mod_mod, psz_key, pcfor)  );
                }
                else
                {
                    /* whew!  that was close.  it turns out that both who
                     * modified this record did it in exactly the same way. */

                    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_pending_mods, psz_key, &pcfor->leaves[0])  );
                }
            }
        }
        else
        {
            /* The trivial case.  Only one changeset involved with this key value. */

            SG_uint32 ileaf = 0;

            if (pcfor->leaves[0].prec_added || pcfor->leaves[0].prec_deleted)
            {
                ileaf = 0;
            }
            else
            {
                ileaf = 1;
            }

            if (pcfor->leaves[ileaf].prec_added
                    && !pcfor->leaves[ileaf].prec_deleted)
            {
                SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_pending_adds, psz_key, pcfor->leaves[ileaf].prec_added)  );
            }
            if (!pcfor->leaves[ileaf].prec_added
                    && pcfor->leaves[ileaf].prec_deleted)
            {
                SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_pending_deletes, psz_key, pcfor->leaves[ileaf].prec_deleted)  );
            }
            if (pcfor->leaves[ileaf].prec_added
                    && pcfor->leaves[ileaf].prec_deleted)
            {
                SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_pending_mods, psz_key, &pcfor->leaves[ileaf])  );
            }
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit_keys, &b_keys, &psz_key, (void**) &pcfor)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit_keys);

    pzms->prb_pending_adds = prb_pending_adds;
    pzms->prb_pending_mods = prb_pending_mods;
    pzms->prb_pending_deletes = prb_pending_deletes;
    pzms->prb_conflicts__delete_mod = prb_conflicts__delete_mod;
    pzms->prb_conflicts__mod_mod = prb_conflicts__mod_mod;
    pzms->prb_conflicts__add_add = prb_conflicts__add_add;

    prb_pending_adds = NULL;
    prb_pending_mods = NULL;
    prb_pending_deletes = NULL;
    prb_conflicts__delete_mod = NULL;
    prb_conflicts__mod_mod = NULL;
    prb_conflicts__add_add = NULL;

    // fall thru

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_pending_adds);
    SG_RBTREE_NULLFREE(pCtx, prb_pending_deletes);
    SG_RBTREE_NULLFREE(pCtx, prb_pending_mods);
    SG_RBTREE_NULLFREE(pCtx, prb_conflicts__delete_mod);
    SG_RBTREE_NULLFREE(pCtx, prb_conflicts__mod_mod);
    SG_RBTREE_NULLFREE(pCtx, prb_conflicts__add_add);
}

static void sg_zingmerge__add_add(
    SG_context* pCtx,
    struct sg_zing_merge_situation* pzms
    )
{
  SG_UNUSED(pCtx);
  SG_UNUSED(pzms);

    // TODO fix this.  even though I think it's actually impossible
    // for this to happen.  and if it did, we don't have a common
    // ancestor to use as the third record in the merge.  so it
    // can only be resolved by policy.  or maybe by splitting the
    // records into two and giving a new recid/gid to one of them.
}

static void sg_zingmerge__delete_mod(
    SG_context* pCtx,
    struct sg_zing_merge_situation* pzms
    )
{
  SG_UNUSED(pCtx);
  SG_UNUSED(pzms);

    // TODO fix this.  the template should specify a policy.
    // if not, this is an unresolvable conflict.
}

static void sg_zingmerge__get_timestamps(
    SG_context* pCtx,
    SG_varray* pva,
    SG_int64* pimin,
    SG_int64* pimax
    )
{
    SG_uint32 i;
    SG_int64 imin = 0;
    SG_int64 imax = 0;
    SG_uint32 count = 0;

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh = NULL;
        SG_int64 itime = -1;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh)  );
        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh, "when", &itime)  );

        if (0 == i)
        {
            imin = itime;
            imax = itime;
        }
        else
        {
            imin = SG_MIN(imin, itime);
            imax = SG_MAX(imax, itime);
        }

        i++;
    }

    if (pimin)
    {
        *pimin = imin;
    }
    if (pimax)
    {
        *pimax = imax;
    }

    // fall thru

fail:
    return;
}

static void sg_zingmerge__least_recent(
    SG_context* pCtx,
    struct sg_zing_merge_situation* pzms,
    SG_int16* pi_which
    )
{
    SG_int64 times[2];

    times[0] = 0;
    times[1] = 0;

    SG_ERR_CHECK(  sg_zingmerge__get_timestamps(pCtx, pzms->audits[0], &times[0], NULL)  );
    SG_ERR_CHECK(  sg_zingmerge__get_timestamps(pCtx, pzms->audits[1], &times[1], NULL)  );

    if (times[0] < times[1])
    {
        *pi_which = 0;
    }
    else if (times[0] > times[1])
    {
        *pi_which = 1;
    }
    else
    {
        // this should never happen.  coding for it is pathological.
        *pi_which = -1;
    }

    // fall thru

fail:
    return;
}

static void sg_zingmerge__most_recent(
    SG_context* pCtx,
    struct sg_zing_merge_situation* pzms,
    SG_int16* pi_which
    )
{
    SG_int64 times[2];

    times[0] = 0;
    times[1] = 0;

    SG_ERR_CHECK(  sg_zingmerge__get_timestamps(pCtx, pzms->audits[0], NULL, &times[0])  );
    SG_ERR_CHECK(  sg_zingmerge__get_timestamps(pCtx, pzms->audits[1], NULL, &times[1])  );

    if (times[0] > times[1])
    {
        *pi_which = 0;
    }
    else if (times[0] < times[1])
    {
        *pi_which = 1;
    }
    else
    {
        // this should never happen.  coding for it is pathological.
        *pi_which = -1;
    }

    // fall thru

fail:
    return;
}

static void sg_zing__merge_record_by_field(
    SG_context* pCtx,
    const char* psz_recid,
    struct sg_zing_merge_situation* pzms,
    SG_zingtemplate* pzt,
    const char* psz_rectype,
    SG_dbrecord* pdbrec,
    SG_zingrectypeinfo* pzmi,
    struct changes_for_one_record* pcfor
    )
{
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_zingfieldattributes* pzfa = NULL;
    SG_stringarray* psa_fields = NULL;
    const char* psz_field_name = NULL;

    SG_UNUSED(pzmi);

    // load both dagnodes
    // get the rectype
    // the lca version of the record should be in prec_deleted
    // iterate over all the fields in the template
    // if a field has no conflict, just take the change
    // for each one that conflicts, check its merge rules
    // in the end construct a new dbrecord add add it to prb_pending_mods

    SG_ERR_CHECK(  SG_zingtemplate__list_fields(pCtx, pzt, psz_rectype, &psa_fields)  );

    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_fields, &count )  );
    for (i=0; i<count; i++)
    {
        const char* psz_val_lca = NULL;
        const char* psz_vals[2];

        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa_fields, i, &psz_field_name)  );

        // get all three values of the field
        SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, pcfor->leaves[0].prec_deleted, psz_field_name, &psz_val_lca)  );
        SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, pcfor->leaves[0].prec_added, psz_field_name, &psz_vals[0])  );
        SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, pcfor->leaves[1].prec_added, psz_field_name, &psz_vals[1])  );

        if (0 == strcmp(psz_vals[0], psz_vals[1]))
        {
            // if the two leaves agree, we're fine.  no conflict.
            SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec, psz_field_name, psz_vals[0])  );
        }
        else
        {
            SG_bool b0 = SG_FALSE;
            SG_bool b1 = SG_FALSE;

            b0 = (0 != strcmp(psz_val_lca, psz_vals[0]));
            b1 = (0 != strcmp(psz_val_lca, psz_vals[1]));

            if (b0 && b1)
            {
                // see if the template has a way of resolving this conflict
                //
                SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, psz_rectype, psz_field_name, &pzfa)  );

                if (!pzfa->pva_automerge)
                {
                    goto conflict;
                }

                switch (pzfa->type)
                {
                    case SG_ZING_TYPE__BOOL:
                    {
                        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                    }
                    break;

                    case SG_ZING_TYPE__ATTACHMENT:
                    {
                        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                    }
                    break;

                    case SG_ZING_TYPE__DAGNODE:
                    {
                        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                    }
                    break;

                    case SG_ZING_TYPE__STRING:
                    {
                        SG_uint32 count_autos = 0;
                        SG_uint32 j = 0;

                        SG_ERR_CHECK(  SG_varray__count(pCtx, pzfa->pva_automerge, &count_autos)  );
                        for (j=0; j<count_autos; j++)
                        {
                            SG_vhash* pvh_auto = NULL;
                            const char* psz_op = NULL;

                            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pzfa->pva_automerge, j, &pvh_auto)  );
                            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_auto, SG_ZING_TEMPLATE__OP, &psz_op)  );

                            if (0 == strcmp(psz_op, SG_ZING_MERGE__most_recent))
                            {
                                SG_int16 which = -1;
                                SG_ERR_CHECK(  sg_zingmerge__most_recent(pCtx, pzms, &which)  );
                                if (
                                        (0 == which)
                                        || (1 == which)
                                   )
                                {
                                    SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec, psz_field_name, psz_vals[which])  );
                                    SG_ERR_CHECK(  sg_zingmerge__add_log_entry(pCtx, pzms->pva_log,
                                                "recid", psz_recid,
                                                "field_name", psz_field_name,
                                                "other_value", psz_vals[!which],
                                                "merge_value", psz_vals[which],
                                                "reason", psz_op,
                                                NULL) );
                                }
                                else
                                {
                                    // this should never happen.  coding for it is pathological.
                                    goto conflict;
                                }
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__least_recent))
                            {
                                SG_int16 which = -1;
                                SG_ERR_CHECK(  sg_zingmerge__least_recent(pCtx, pzms, &which)  );
                                if (
                                        (0 == which)
                                        || (1 == which)
                                   )
                                {
                                    SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec, psz_field_name, psz_vals[which])  );
                                    SG_ERR_CHECK(  sg_zingmerge__add_log_entry(pCtx, pzms->pva_log,
                                                "recid", psz_recid,
                                                "field_name", psz_field_name,
                                                "other_value", psz_vals[!which],
                                                "merge_value", psz_vals[which],
                                                "reason", psz_op,
                                                NULL) );
                                }
                                else
                                {
                                    // this should never happen.  coding for it is pathological.
                                    goto conflict;
                                }
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__longest))
                            {
                                SG_int16 which = -1;
                                SG_uint32 len0 = strlen(psz_vals[0]);
                                SG_uint32 len1 = strlen(psz_vals[1]);

                                if (len0 > len1)
                                {
                                    which = 0;
                                }
                                else if (len1 > len0)
                                {
                                    which = 1;
                                }
                                else
                                {
                                    which = -1;
                                }
                                if (
                                        (0 == which)
                                        || (1 == which)
                                   )
                                {
                                    SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec, psz_field_name, psz_vals[which])  );
                                    SG_ERR_CHECK(  sg_zingmerge__add_log_entry(pCtx, pzms->pva_log,
                                                "recid", psz_recid,
                                                "field_name", psz_field_name,
                                                "other_value", psz_vals[!which],
                                                "merge_value", psz_vals[which],
                                                "reason", psz_op,
                                                NULL) );
                                }
                                else
                                {
                                    // this can actually happen.  coding for it is reasonable.
                                    goto conflict;
                                }
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__shortest))
                            {
                                SG_int16 which = -1;
                                SG_uint32 len0 = strlen(psz_vals[0]);
                                SG_uint32 len1 = strlen(psz_vals[1]);

                                if (len0 < len1)
                                {
                                    which = 0;
                                }
                                else if (len1 < len0)
                                {
                                    which = 1;
                                }
                                else
                                {
                                    which = -1;
                                }
                                if (
                                        (0 == which)
                                        || (1 == which)
                                   )
                                {
                                    SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec, psz_field_name, psz_vals[which])  );
                                    SG_ERR_CHECK(  sg_zingmerge__add_log_entry(pCtx, pzms->pva_log,
                                                "recid", psz_recid,
                                                "field_name", psz_field_name,
                                                "other_value", psz_vals[!which],
                                                "merge_value", psz_vals[which],
                                                "reason", psz_op,
                                                NULL) );
                                }
                                else
                                {
                                    // this can actually happen.  coding for it is reasonable.
                                    goto conflict;
                                }
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__concat))
                            {
                                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__allowed_last))
                            {
                                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__allowed_first))
                            {
                                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                            }
                            else
                            {
                                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                            }
                        }
                    }
                    break;

                    case SG_ZING_TYPE__INT:
                    {
                        SG_int64 i_vals[2];
                        SG_uint32 count_autos = 0;
                        SG_uint32 j = 0;

                        SG_ERR_CHECK(  SG_dbrecord__get_value__int64(pCtx, pcfor->leaves[0].prec_added, psz_field_name, &i_vals[0])  );
                        SG_ERR_CHECK(  SG_dbrecord__get_value__int64(pCtx, pcfor->leaves[1].prec_added, psz_field_name, &i_vals[1])  );

                        SG_ERR_CHECK(  SG_varray__count(pCtx, pzfa->pva_automerge, &count_autos)  );
                        for (j=0; j<count_autos; j++)
                        {
                            SG_vhash* pvh_auto = NULL;
                            const char* psz_op = NULL;

                            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pzfa->pva_automerge, j, &pvh_auto)  );
                            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_auto, SG_ZING_TEMPLATE__OP, &psz_op)  );

                            if (0 == strcmp(psz_op, SG_ZING_MERGE__most_recent))
                            {
                                SG_int16 which = -1;
                                SG_ERR_CHECK(  sg_zingmerge__most_recent(pCtx, pzms, &which)  );
                                if (
                                        (0 == which)
                                        || (1 == which)
                                   )
                                {
                                    SG_ERR_CHECK(  SG_dbrecord__add_pair__int64(pCtx, pdbrec, psz_field_name, i_vals[which])  );
                                    SG_ERR_CHECK(  sg_zingmerge__add_log_entry(pCtx, pzms->pva_log,
                                                "recid", psz_recid,
                                                "field_name", psz_field_name,
                                                "other_value", psz_vals[!which],
                                                "merge_value", psz_vals[which],
                                                "reason", psz_op,
                                                NULL) );
                                }
                                else
                                {
                                    // this should never happen.  coding for it is pathological.
                                    goto conflict;
                                }
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__least_recent))
                            {
                                SG_int16 which = -1;
                                SG_ERR_CHECK(  sg_zingmerge__least_recent(pCtx, pzms, &which)  );
                                if (
                                        (0 == which)
                                        || (1 == which)
                                   )
                                {
                                    SG_ERR_CHECK(  SG_dbrecord__add_pair__int64(pCtx, pdbrec, psz_field_name, i_vals[which])  );
                                    SG_ERR_CHECK(  sg_zingmerge__add_log_entry(pCtx, pzms->pva_log,
                                                "recid", psz_recid,
                                                "field_name", psz_field_name,
                                                "other_value", psz_vals[!which],
                                                "merge_value", psz_vals[which],
                                                "reason", psz_op,
                                                NULL) );
                                }
                                else
                                {
                                    // this should never happen.  coding for it is pathological.
                                    goto conflict;
                                }
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__max))
                            {
                                SG_int16 which = -1;
                                if (i_vals[0] > i_vals[1])
                                {
                                    which = 0;
                                }
                                else
                                {
                                    which = 1;
                                }
                                SG_ERR_CHECK(  SG_dbrecord__add_pair__int64(pCtx, pdbrec, psz_field_name, i_vals[which])  );
                                SG_ERR_CHECK(  sg_zingmerge__add_log_entry(pCtx, pzms->pva_log,
                                            "recid", psz_recid,
                                            "field_name", psz_field_name,
                                            "other_value", psz_vals[!which],
                                            "merge_value", psz_vals[which],
                                            "reason", psz_op,
                                            NULL) );
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__min))
                            {
                                SG_int16 which = -1;
                                if (i_vals[0] < i_vals[1])
                                {
                                    which = 0;
                                }
                                else
                                {
                                    which = 1;
                                }
                                SG_ERR_CHECK(  SG_dbrecord__add_pair__int64(pCtx, pdbrec, psz_field_name, i_vals[which])  );
                                SG_ERR_CHECK(  sg_zingmerge__add_log_entry(pCtx, pzms->pva_log,
                                            "recid", psz_recid,
                                            "field_name", psz_field_name,
                                            "other_value", psz_vals[!which],
                                            "merge_value", psz_vals[which],
                                            "reason", psz_op,
                                            NULL) );
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__average))
                            {
                                // TODO log
                                SG_ERR_CHECK(  SG_dbrecord__add_pair__int64(pCtx, pdbrec, psz_field_name, (i_vals[0] + i_vals[1]) / 2)  );
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__sum))
                            {
                                // TODO log
                                SG_ERR_CHECK(  SG_dbrecord__add_pair__int64(pCtx, pdbrec, psz_field_name, (i_vals[0] + i_vals[1]))  );
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__allowed_last))
                            {
                                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                            }
                            else if (0 == strcmp(psz_op, SG_ZING_MERGE__allowed_first))
                            {
                                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                            }
                            else
                            {
                                SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                            }
                        }
                    }
                    break;

                    default:
                    {
                        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
                    }
                    break;
                }
            }
            else if (b0)
            {
                SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec, psz_field_name, psz_vals[0])  );
            }
            else
            {
                SG_ASSERT(b1);
                SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec, psz_field_name, psz_vals[1])  );
            }
        }
    }

    goto done;

conflict:
    SG_ASSERT(psz_field_name);
    SG_ERR_THROW2(  SG_ERR_ZING_MERGE_CONFLICT,
            (pCtx, "Unresolvable conflict in field '%s'", psz_field_name)
            );

done:
    // fall thru

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
}

static void sg_zingmerge__mod_mod__one_record(
    SG_context* pCtx,
    struct sg_zing_merge_situation* pzms,
    const char* psz_recid,
    struct changes_for_one_record* pcfor,
    SG_bool* pb
    )
{
    const char* psz_rectype = NULL;
    SG_zingtemplate* pzt = NULL;
    SG_zingrectypeinfo* pmi = NULL;
    SG_dbrecord* pdbrec_freeme = NULL;
    SG_dbrecord* pdbrec_keepme = NULL;
    struct changes_for_one_record* my_pcfor = NULL;

    SG_ASSERT(pcfor);
    SG_ASSERT(pcfor->leaves[0].prec_deleted);
    SG_ASSERT(pcfor->leaves[0].prec_added);
    SG_ASSERT(pcfor->leaves[1].prec_deleted);
    SG_ASSERT(pcfor->leaves[1].prec_added);

#if defined(DEBUG)
    {
        const char* psz_hid_deleted_0 = NULL;
        const char* psz_hid_deleted_1 = NULL;
        SG_ERR_CHECK(  SG_dbrecord__get_hid__ref(pCtx, pcfor->leaves[0].prec_deleted, &psz_hid_deleted_0)  );
        SG_ERR_CHECK(  SG_dbrecord__get_hid__ref(pCtx, pcfor->leaves[1].prec_deleted, &psz_hid_deleted_1)  );
        SG_ASSERT(0 == strcmp(psz_hid_deleted_0, psz_hid_deleted_1));
    }
#endif

    pzt = pzms->pztemplate_lca; // TODO if the template has changed, this is wrong.

    SG_ERR_CHECK(  SG_dbrecord__get_value(pCtx, pcfor->leaves[1].prec_added, SG_ZING_FIELD__RECTYPE, &psz_rectype)  );

    SG_ERR_CHECK(  SG_zingtemplate__get_rectype_info(pCtx, pzt, psz_rectype, &pmi)  );

    SG_ERR_CHECK(  SG_DBRECORD__ALLOC(pCtx, &pdbrec_freeme)  );
    pdbrec_keepme = pdbrec_freeme;
    SG_ERR_CHECK(  SG_vector__append(pCtx, pzms->pvec_dbrecords, pdbrec_freeme, NULL)  );
    pdbrec_freeme = NULL;
    SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec_keepme, SG_ZING_FIELD__RECTYPE, psz_rectype)  );
    SG_ERR_CHECK(  SG_dbrecord__add_pair(pCtx, pdbrec_keepme, SG_ZING_FIELD__RECID, psz_recid)  );

    if (pmi->b_merge_type_is_record)
    {
        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
    }
    else
    {
        SG_ERR_CHECK(  sg_zing__merge_record_by_field(pCtx, psz_recid, pzms, pzt, psz_rectype, pdbrec_keepme, pmi, pcfor)  );
    }

    SG_ERR_CHECK(  my_alloc_cfor(pCtx, pzms, &my_pcfor)  );
    my_pcfor->leaves[0].prec_deleted = pcfor->leaves[0].prec_deleted;
    my_pcfor->leaves[0].prec_added = pdbrec_keepme;
    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pzms->prb_pending_mods, psz_recid, &my_pcfor->leaves[0])  );
    my_pcfor = NULL;

    *pb = SG_TRUE;

    // fall thru

fail:
    SG_NULLFREE(pCtx, my_pcfor);
    SG_DBRECORD_NULLFREE(pCtx, pdbrec_freeme);
    SG_ZINGRECTYPEINFO_NULLFREE(pCtx, pmi);
}

static void sg_zingmerge__mod_mod(
    SG_context* pCtx,
    struct sg_zing_merge_situation* pzms
    )
{
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_recid = NULL;
    struct changes_for_one_record* pcfor = NULL;
    SG_vector* pvec_fixed = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;

    SG_ERR_CHECK(  SG_rbtree__count(pCtx, pzms->prb_conflicts__mod_mod, &count)  );

    if (count)
    {
        SG_ERR_CHECK(  SG_vector__alloc(pCtx, &pvec_fixed, count)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pzms->prb_conflicts__mod_mod, &b, &psz_recid, (void**) &pcfor)  );
        while (b)
        {
            SG_bool b_conflict_fixed = SG_FALSE;

            SG_ERR_CHECK(  sg_zingmerge__mod_mod__one_record(pCtx, pzms, psz_recid, pcfor, &b_conflict_fixed)  );
            if (b_conflict_fixed)
            {
                SG_ERR_CHECK(  SG_vector__append(pCtx, pvec_fixed, (void*) psz_recid, NULL)  );
            }

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid, (void**) &pcfor)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

        SG_ERR_CHECK(  SG_vector__length(pCtx, pvec_fixed, &count)  );
        for (i=0; i<count; i++)
        {
            SG_ERR_CHECK(  SG_vector__get(pCtx, pvec_fixed, i, (void**) &psz_recid)  );
            SG_ERR_CHECK(  SG_rbtree__remove(pCtx, pzms->prb_conflicts__mod_mod, psz_recid)  );
        }
        SG_VECTOR_NULLFREE(pCtx, pvec_fixed);
    }

    return;

fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
}


static void sg_zingmerge__handle_record_conflicts(
    SG_context* pCtx,
    struct sg_zing_merge_situation* pzms
    )
{
    SG_ERR_CHECK_RETURN(  sg_zingmerge__add_add(pCtx, pzms)  );
    SG_ERR_CHECK_RETURN(  sg_zingmerge__delete_mod(pCtx, pzms)  );
    SG_ERR_CHECK_RETURN(  sg_zingmerge__mod_mod(pCtx, pzms)  );
}

static void my_copy_rbtree_int_rbtree__pooled_sz(
    SG_context* pCtx,
    SG_rbtree* prb_from,
    SG_rbtree* prb_to
    )
{
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_key = NULL;
    const char* psz_assoc = NULL;

    if (prb_from)
    {
        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_from, &b, &psz_key, (void**) &psz_assoc)  );
        while (b)
        {
            SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, prb_to, psz_key, psz_assoc)  );

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_key, (void**)&psz_assoc)  );
        }
    }

fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
}

static void sg_zingmerge__handle_links(
    SG_context* pCtx,
    struct sg_zing_merge_situation* pzms
    )
{
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pzms->prb_links__deletes)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pzms->prb_links__adds)  );

    SG_ERR_CHECK(  my_copy_rbtree_int_rbtree__pooled_sz(pCtx, pzms->links[0].prb_deleted, pzms->prb_links__deletes)  );
    SG_ERR_CHECK(  my_copy_rbtree_int_rbtree__pooled_sz(pCtx, pzms->links[1].prb_deleted, pzms->prb_links__deletes)  );
    SG_ERR_CHECK(  my_copy_rbtree_int_rbtree__pooled_sz(pCtx, pzms->links[0].prb_added, pzms->prb_links__adds)  );
    SG_ERR_CHECK(  my_copy_rbtree_int_rbtree__pooled_sz(pCtx, pzms->links[1].prb_added, pzms->prb_links__adds)  );

    // TODO is this the right place to try to figure out constraint
    // problems on single links?

fail:
    return;
}

void sg_zingmerge__add_log_entry(
        SG_context* pCtx,
        SG_varray* pva_log,
        ...
        )
{
    va_list ap;
    SG_vhash* pvh = NULL;
    SG_string* pstr = NULL;

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh)  );

    va_start(ap, pva_log);
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

    SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pva_log, &pvh)  );

fail:
    va_end(ap);
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_VHASH_NULLFREE(pCtx, pvh);
}

static void x__append_one_step(
    SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_csid,
    SG_varray* pva,
    SG_vhash** ppvh
    )
{
    SG_changeset* pcs = NULL;
    SG_vhash* pvh_new = NULL;
    const char* psz_baseline = NULL;
    const char* psz_hid_delta = NULL;
    SG_vhash* pvh_delta = NULL;
    SG_int32 gen = 0;

    SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, psz_csid, &pcs)  );
    SG_ERR_CHECK(  SG_changeset__get_generation(pCtx, pcs, &gen)  );
    SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs, &psz_hid_delta)  );
    if (!psz_hid_delta)
    {
        SG_ERR_THROW(  SG_ERR_INVALIDARG  );
    }
    SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, pRepo, psz_hid_delta, &pvh_delta)  );

    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_delta, "baseline", &psz_baseline)  );

    SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva, &pvh_new)  );
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_new, "csid", psz_csid)  );
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_new, "baseline", psz_baseline)  );
    SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_new, "generation", gen)  );

    if (ppvh)
    {
        *ppvh = pvh_new;
    }

fail:
    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_VHASH_NULLFREE(pCtx, pvh_delta);
}

static void x__go_back_one_step(
    SG_context* pCtx,
    SG_repo* pRepo,
    SG_varray* pva,
    SG_bool* pb,
    SG_vhash** ppvh
    )
{
    SG_uint32 count = 0;
    SG_vhash* pvh_entry = NULL;
    const char* psz_baseline = NULL;
    SG_bool b = SG_FALSE;

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    SG_ASSERT(count > 0);
    SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, count-1, &pvh_entry)  );
    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_entry, "baseline", &b)  );
    if (b)
    {
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_entry, "baseline", &psz_baseline)  );
        if (psz_baseline)
        {
            SG_ERR_CHECK(  x__append_one_step(pCtx, pRepo, psz_baseline, pva, ppvh)  );
            *pb = SG_TRUE;
        }
        else
        {
            *pb = SG_FALSE;
        }
    }
    else
    {
        *pb = SG_FALSE;
    }

fail:
    ;
}

static void x__add_to_index(
    SG_context* pCtx,
    SG_vhash* pvh_index,
    SG_vhash* pvh_entry
    )
{
    const char* psz_csid = NULL;
    SG_int64 gen = 0;

    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_entry, "csid", &psz_csid)  );
    SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_entry, "generation", &gen)  );
    SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_index, psz_csid, gen)  );

fail:
    ;
}

static void x_find_ca(
    SG_context* pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
    const char* leaves[2],
    char** ppsz_csid_ca
    )
{
    SG_varray* pva_back_0 = NULL;
    SG_varray* pva_back_1 = NULL;
    SG_vhash* pvh_index_1 = NULL;
    SG_uint32 count_0 = 0;
    SG_uint32 i = 0;
    SG_int64 gen_winning = -1;
    const char* psz_csid_winning = NULL;
    SG_vhash* pvh_entry = NULL;

    SG_UNUSED(iDagNum);

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh_index_1)  );
    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_back_0)  );
    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_back_1)  );

    SG_ERR_CHECK(  x__append_one_step(pCtx, pRepo, leaves[0], pva_back_0, NULL)  );
    SG_ERR_CHECK(  x__append_one_step(pCtx, pRepo, leaves[1], pva_back_1, &pvh_entry)  );
    SG_ERR_CHECK(  x__add_to_index(pCtx, pvh_index_1, pvh_entry)  );

    while (1)
    {
        SG_uint32 count_got_0 = 0;
        SG_uint32 count_got_1 = 0;

        for (i=0; i<4; i++)
        {
            SG_bool b_got_0 = SG_FALSE;
            SG_bool b_got_1 = SG_FALSE;

            SG_ERR_CHECK(  x__go_back_one_step(pCtx, pRepo, pva_back_0, &b_got_0, NULL)  );
            if (b_got_0)
            {
                count_got_0++;
            }

            SG_ERR_CHECK(  x__go_back_one_step(pCtx, pRepo, pva_back_1, &b_got_1, &pvh_entry)  );
            if (b_got_1)
            {
                count_got_1++;
                SG_ERR_CHECK(  x__add_to_index(pCtx, pvh_index_1, pvh_entry)  );
            }
        }

        if (
                (0 == count_got_0)
                && (0 == count_got_1)
           )
        {
            break;
        }

        // find an intersection
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva_back_0, &count_0)  );
        for (i=0; i<count_0; i++)
        {
            const char* psz_csid = NULL;
            SG_bool b = SG_FALSE;
            SG_int64 gen = 0;

            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_back_0, i, &pvh_entry)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_entry, "csid", &psz_csid)  );

            SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_index_1, psz_csid, &b)  );
            if (b)
            {
                SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_entry, "generation", &gen)  );
                if (
                        (gen_winning < 0)
                        || (gen > gen_winning)
                   )
                {
                    // TODO not sure we need this "winning" approach to the
                    // algorithm.  the entries should be in generation order.
                    // can't we just look at them in order (reverse order, I
                    // think) and know that we found the right one?
                    gen_winning = gen;
                    psz_csid_winning = psz_csid;
                }
            }
        }

        if (psz_csid_winning)
        {
            break;
        }
    }

    if (psz_csid_winning)
    {
        SG_ERR_CHECK(  SG_strdup(pCtx, psz_csid_winning, ppsz_csid_ca)  );
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_ZING_NO_ANCESTOR  );
    }

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_index_1);
    SG_VARRAY_NULLFREE(pCtx, pva_back_0);
    SG_VARRAY_NULLFREE(pCtx, pva_back_1);
}

void SG_zingmerge__attempt_automatic_merge(
    SG_context* pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
    const SG_audit* pq,
    const char* psz_csid_node_0,
    const char* psz_csid_node_1,
    SG_dagnode** ppdn_merged,
    SG_varray** ppva_errors,
    SG_varray* pva_log
    )
{
    SG_dagnode* pdn_merged = NULL;
	SG_changeset* pcs = NULL;
    SG_zingtx* pztx = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_recid = NULL;
    SG_dbrecord* pdbrec = NULL;
    struct sg_zing_merge_situation zms;
    struct record_changes_from_one_changeset* rcfoc = NULL;
    SG_bool b_template_conflict = SG_FALSE;
    const char* psz_link = NULL;
    const char* psz_hid_link = NULL;
    SG_varray* pva_violations = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
    memset(&zms, 0, sizeof(zms));

    SG_ERR_CHECK(  SG_vector__alloc(pCtx, &zms.pvec_pcfor, 10)  );
    SG_ERR_CHECK(  SG_vector__alloc(pCtx, &zms.pvec_dbrecords, 10)  );
    zms.pva_log = pva_log;
    zms.pq = pq;

    zms.leaves[0] = psz_csid_node_0;
    zms.leaves[1] = psz_csid_node_1;

    if (!SG_DAGNUM__IS_AUDIT(iDagNum))
    {
        SG_ERR_CHECK(  SG_repo__dbndx__lookup_audits(pCtx, pRepo, iDagNum, zms.leaves[0], &zms.audits[0])  );
        SG_ERR_CHECK(  SG_repo__dbndx__lookup_audits(pCtx, pRepo, iDagNum, zms.leaves[1], &zms.audits[1])  );
    }

    /* All computations will be against the common ancestor for the
     * leaves.  The CA will also be used as the starting point for the new
     * changeset to be committed if the merge succeeds, but the CA will not be
     * a parent for that changeset. */
    SG_ERR_CHECK(  x_find_ca(pCtx, pRepo, iDagNum, zms.leaves, &zms.psz_hid_cs_ancestor)  );

    SG_ERR_CHECK(  sg_zing__load_template__csid(pCtx, pRepo, zms.psz_hid_cs_ancestor, &zms.pztemplate_lca)  );

    /* Step 1:  collect all the changes together and group them by recid */

    SG_ERR_CHECK(  sg_zing__collect_and_group_all_changes(pCtx, pRepo, &zms)  );

    /* Step 2:  Run through prb_keys and see if any of them have conflicts */

    SG_ERR_CHECK(  sg_zingmerge__check_for_conflicts(
                pCtx,
                &zms
                )  );

    // TODO check for conflicts on the template itself and deal with it

    // TODO compare the hids on the templates
    if (b_template_conflict)
    {
        // TODO deal with what happens when both leaves changed the template
    }

    // TODO if the template has changed (in either or both leaves), we
    // need to be using the new template going forward.  get it and
    // stuff it in zms for use.

    // now see if we can resolve the conflicts using rules
    // from the template.

    SG_ERR_CHECK(  sg_zingmerge__handle_record_conflicts(
                pCtx,
                &zms
                )  );

    SG_ERR_CHECK(  sg_zingmerge__handle_links(
                pCtx,
                &zms
                )  );

    // Now commit

    if (pq)
    {
        SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, iDagNum, pq->who_szUserId, zms.psz_hid_cs_ancestor, &pztx)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, iDagNum, NULL, zms.psz_hid_cs_ancestor, &pztx)  );
    }

    // add the leaves as PARENTS
    SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_csid_node_0)  );
    SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_csid_node_1)  );

    // the ADDS
    b = SG_FALSE;
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, zms.prb_pending_adds, &b, &psz_recid, (void**) &pdbrec)  );
    while (b)
    {
        SG_ERR_CHECK(  sg_zingtx__add__from_dbrecord(pCtx, pztx, pdbrec)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid, (void**)&pdbrec)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // the DELETES
    b = SG_FALSE;
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, zms.prb_pending_deletes, &b, &psz_recid, (void**) &pdbrec)  );
    while (b)
    {
        SG_ERR_CHECK(  sg_zingtx__delete__from_dbrecord(pCtx, pztx, pdbrec)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid, (void**)&pdbrec)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // the MODS
    b = SG_FALSE;
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, zms.prb_pending_mods, &b, &psz_recid, (void**) &rcfoc)  );
    while (b)
    {
        const char* psz_hid_delete = NULL;

        SG_ERR_CHECK(  SG_dbrecord__get_hid__ref(pCtx, rcfoc->prec_deleted, &psz_hid_delete)  );
        SG_ERR_CHECK(  sg_zingtx__mod__from_dbrecord(pCtx, pztx, psz_hid_delete, rcfoc->prec_added)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_recid, (void**)&rcfoc)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    // remove links
    b = SG_FALSE;
    if (zms.prb_links__deletes)
    {
        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, zms.prb_links__deletes, &b, &psz_hid_link, (void**) &psz_link)  );
        while (b)
        {
            SG_ERR_CHECK(  sg_zingtx__delete_link__packed(pCtx, pztx, psz_link, psz_hid_link)  );

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_link, (void**)&psz_link)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    }

    if (zms.prb_links__adds)
    {
        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, zms.prb_links__adds, &b, &psz_hid_link, (void**) &psz_link)  );
        while (b)
        {
            SG_ERR_CHECK(  SG_zingtx__add_link__packed(pCtx, pztx, psz_link)  );

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_link, (void**)&psz_link)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    }

    // and COMMIT
    while (1)
    {
        if (pq)
        {
            SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, pq->when_int64, &pztx, &pcs, &pdn_merged, &pva_violations)  );
        }
        else
        {
            SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, 0, &pztx, &pcs, &pdn_merged, &pva_violations)  );
        }
        SG_CHANGESET_NULLFREE(pCtx, pcs);

        if (pdn_merged)
        {
            break;
        }
        else
        {
            SG_bool b_fail = SG_FALSE;

            SG_ERR_CHECK(  sg_zingmerge__resolve_commit_errors(
                        pCtx,
                        &zms,
                        pztx,
                        pva_violations,
                        &b_fail
                        )
                    );
            if (b_fail)
            {
                break;
            }
            SG_VARRAY_NULLFREE(pCtx, pva_violations);
        }
    }

    *ppva_errors = pva_violations;
    pva_violations = NULL;

    *ppdn_merged = pdn_merged;
    pdn_merged = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_violations);
	SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pztx)  );
    SG_ZINGTEMPLATE_NULLFREE(pCtx, zms.pztemplate_lca);
    SG_RBTREE_NULLFREE(pCtx, zms.prb_keys);
    SG_RBTREE_NULLFREE(pCtx, zms.prb_pending_adds);
    SG_RBTREE_NULLFREE(pCtx, zms.prb_pending_mods);
    SG_RBTREE_NULLFREE(pCtx, zms.prb_pending_deletes);
    SG_RBTREE_NULLFREE(pCtx, zms.prb_conflicts__add_add);
    SG_RBTREE_NULLFREE(pCtx, zms.prb_conflicts__mod_mod);
    SG_RBTREE_NULLFREE(pCtx, zms.prb_conflicts__delete_mod);
    SG_RBTREE_NULLFREE(pCtx, zms.prb_links__deletes);
    SG_RBTREE_NULLFREE(pCtx, zms.prb_links__adds);

    SG_RBTREE_NULLFREE(pCtx, zms.links[0].prb_added);
    SG_RBTREE_NULLFREE(pCtx, zms.links[0].prb_deleted);
    SG_RBTREE_NULLFREE(pCtx, zms.links[1].prb_added);
    SG_RBTREE_NULLFREE(pCtx, zms.links[1].prb_deleted);

    SG_NULLFREE(pCtx, zms.psz_hid_cs_ancestor);
    SG_VECTOR_NULLFREE_WITH_ASSOC(pCtx, zms.pvec_pcfor, my_free_cfor);
    SG_VECTOR_NULLFREE_WITH_ASSOC(pCtx, zms.pvec_dbrecords, (SG_free_callback*) SG_dbrecord__free);

    SG_VARRAY_NULLFREE(pCtx, zms.audits[0]);
    SG_VARRAY_NULLFREE(pCtx, zms.audits[1]);

    /* the dbrecord ptrs in prb_pending_adds/deletes were freed just above */

    SG_DAGNODE_NULLFREE(pCtx, pdn_merged);
}

