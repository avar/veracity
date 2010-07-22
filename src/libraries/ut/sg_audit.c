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

void SG_audit__init__nobody(
        SG_context* pCtx,
        SG_audit* pInfo,
        SG_int64 itime
        )
{
    SG_NULLARGCHECK( pInfo );

    memset(pInfo, 0, sizeof(SG_audit));

    if (itime < 0)
    {
        SG_ERR_CHECK(  SG_time__get_milliseconds_since_1970_utc(pCtx, &pInfo->when_int64)  );
    }
    else
    {
        pInfo->when_int64 = itime;
    }

fail:
    ;
}

void SG_audit__init__maybe_nobody(
        SG_context* pCtx,
        SG_audit* pInfo,
        SG_repo* pRepo,
        SG_int64 itime,
        const char* psz_userid
        )
{
    char* psz = NULL;
    SG_vhash* pvh_user = NULL;

    SG_NULLARGCHECK( pInfo );

    memset(pInfo, 0, sizeof(SG_audit));

    if (itime < 0)
    {
        SG_ERR_CHECK(  SG_time__get_milliseconds_since_1970_utc(pCtx, &pInfo->when_int64)  );
    }
    else
    {
        pInfo->when_int64 = itime;
    }

    if (psz_userid)
    {
        SG_ERR_CHECK(  SG_strcpy(pCtx, pInfo->who_szUserId, sizeof(pInfo->who_szUserId), psz_userid)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__USERID, pRepo, &psz, NULL)  );
        if (psz)
        {
            SG_bool b_valid = SG_FALSE;

            SG_ERR_CHECK(  SG_gid__verify_format(pCtx, psz, &b_valid)  );
            if (!b_valid)
            {
                SG_ERR_THROW(  SG_ERR_INVALID_USERID  );
            }
            SG_ERR_CHECK(  SG_strcpy(pCtx, pInfo->who_szUserId, sizeof(pInfo->who_szUserId), psz)  );
        }
        else
        {
            if (pRepo)
            {
                SG_ERR_CHECK(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__USEREMAIL, pRepo, &psz, NULL)  );
                if (psz)
                {
                    SG_ERR_CHECK(  SG_user__lookup_by_email(pCtx, pRepo, psz, &pvh_user)  );
                    if (pvh_user)
                    {
                        const char* psz_id = NULL;

                        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_user, SG_ZING_FIELD__RECID, &psz_id)  );
                        SG_ERR_CHECK(  SG_strcpy(pCtx, pInfo->who_szUserId, sizeof(pInfo->who_szUserId), psz_id)  );
                    }
                }
            }
        }

//#if 1 && defined(DEBUG)
        if (pRepo && !pInfo->who_szUserId[0])
        {
            SG_ERR_CHECK(  SG_user__lookup_by_email(pCtx, pRepo, "debug@sourcegear.com", &pvh_user)  );
            if (pvh_user)
            {
                const char* psz_id = NULL;

                SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_user, SG_ZING_FIELD__RECID, &psz_id)  );
                SG_ERR_CHECK(  SG_strcpy(pCtx, pInfo->who_szUserId, sizeof(pInfo->who_szUserId), psz_id)  );
            }
        }
//#endif

    }

fail:
    SG_NULLFREE(pCtx, psz);
    SG_VHASH_NULLFREE(pCtx, pvh_user);
}

void SG_audit__init(
        SG_context* pCtx,
        SG_audit* pInfo,
        SG_repo* pRepo,
        SG_int64 itime,
        const char* psz_userid
        )
{
    SG_ERR_CHECK(  SG_audit__init__maybe_nobody(pCtx, pInfo, pRepo, itime, psz_userid)  );

    if (!pInfo->who_szUserId[0])
    {
#if 1
        SG_ERR_THROW(  SG_ERR_NO_CURRENT_WHOAMI  );
#else
        SG_ERR_CHECK(  SG_strcpy(pCtx, pInfo->who_szUserId, sizeof(pInfo->who_szUserId), "TODO nobody")  );
#endif
    }

fail:
    ;
}

void SG_audit__copy(
        SG_context* pCtx,
        const SG_audit* p,
        SG_audit** pp
        )
{
    SG_audit* pq = NULL;

    SG_ERR_CHECK(  SG_alloc(pCtx, 1,sizeof(SG_audit),&pq)  );
    memcpy(pq, p, sizeof(SG_audit));

    *pp = pq;

fail:
    return;
}
 extern void my_strip_comments(
        SG_context* pCtx,
        char* pjson,
        SG_string** ppstr
        );

static void sg_audit__set_template(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_audit* pq,
        SG_uint32 iDagNum
        )
{
    // TODO hack.  this borrows things from sg_zing_init.c
    extern unsigned char sg_ztemplate__audit_json[];


    SG_vhash* pvh_template = NULL;
    SG_changeset* pcs = NULL;
    SG_dagnode* pdn = NULL;
    SG_zingtx* ptx = NULL;
    SG_string* pstr = NULL;

    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, iDagNum | SG_DAGNUM__BIT__AUDIT, pq->who_szUserId, NULL, &ptx)  );

    SG_ERR_CHECK(  my_strip_comments(pCtx, (char*) sg_ztemplate__audit_json, &pstr)  );
	SG_ERR_CHECK(  SG_vhash__alloc__from_json(pCtx, &pvh_template, SG_string__sz(pstr)));

    SG_ERR_CHECK(  SG_zingtx__store_template(pCtx, ptx, &pvh_template)  );

    SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, pq->when_int64, &ptx, &pcs, &pdn, NULL)  );

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
}

void SG_audit__add(
        SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_hid_cs_target,
        SG_uint32 iDagNum,
        SG_audit* pq
        )
{
    char* psz_hid_cs_leaf = NULL;
    SG_zingtx* pztx = NULL;
    SG_zingrecord* prec = NULL;
    SG_dagnode* pdn = NULL;
    SG_changeset* pcs = NULL;
    SG_zingtemplate* pzt = NULL;
    SG_zingfieldattributes* pzfa = NULL;

    SG_ASSERT(!SG_DAGNUM__IS_AUDIT(iDagNum));

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, (iDagNum | SG_DAGNUM__BIT__AUDIT), &psz_hid_cs_leaf)  );
    if (!psz_hid_cs_leaf)
    {
        SG_ERR_CHECK(  sg_audit__set_template(pCtx, pRepo, pq, (iDagNum | SG_DAGNUM__BIT__AUDIT))  );
        SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, (iDagNum | SG_DAGNUM__BIT__AUDIT), &psz_hid_cs_leaf)  );
    }

    /* start a changeset */
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, (iDagNum | SG_DAGNUM__BIT__AUDIT), NULL, psz_hid_cs_leaf, &pztx)  );
    SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_hid_cs_leaf)  );
	SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pzt)  );

	SG_ERR_CHECK(  SG_zingtx__create_new_record(pCtx, pztx, "item", &prec)  );


    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, "item", "csid", &pzfa)  );
    SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, prec, pzfa, psz_hid_cs_target) );


    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, "item", "who", &pzfa)  );
    SG_ERR_CHECK(  SG_zingrecord__set_field__userid(pCtx, prec, pzfa, pq->who_szUserId) );


    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, "item", "when", &pzfa)  );
    SG_ERR_CHECK(  SG_zingrecord__set_field__datetime(pCtx, prec, pzfa, pq->when_int64) );


    /* commit the changes */
	SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, 0, &pztx, &pcs, &pdn, NULL)  );

    // fall thru

fail:
    if (pztx)
    {
        SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pztx)  );
    }
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
}

void SG_audit__list_all(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        SG_varray** ppva
        )
{
    SG_stringarray* psa_fields = NULL;
    char* psz_hid_cs_leaf = NULL;
    SG_vhash* pvh_audit = NULL;
    SG_varray* pva_result = NULL;

    SG_ASSERT(!SG_DAGNUM__IS_AUDIT(iDagNum));

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, (iDagNum | SG_DAGNUM__BIT__AUDIT), &psz_hid_cs_leaf)  );
    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 3)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "csid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "who")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "when")  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, (iDagNum | SG_DAGNUM__BIT__AUDIT), psz_hid_cs_leaf, "item", NULL, "when #DESC", 0, 0, psa_fields, &pva_result)  );

    *ppva = pva_result;
    pva_result = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_audit);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
    SG_VARRAY_NULLFREE(pCtx, pva_result);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
}

void SG_audit__list_for_given_changesets(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        SG_varray** ppva_csid_list,
        SG_varray** ppva
        )
{
    SG_stringarray* psa_fields = NULL;
    char* psz_hid_cs_leaf = NULL;
    SG_vhash* pvh_audit = NULL;
    SG_varray* pva_result = NULL;
    SG_varray* pva_crit = NULL;

    SG_ASSERT(!SG_DAGNUM__IS_AUDIT(iDagNum));

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, (iDagNum | SG_DAGNUM__BIT__AUDIT), &psz_hid_cs_leaf)  );
    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "csid")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "in")  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, ppva_csid_list)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 3)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "csid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "who")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "when")  );

    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pRepo, (iDagNum | SG_DAGNUM__BIT__AUDIT), psz_hid_cs_leaf, pva_crit, psa_fields, &pva_result)  );

    *ppva = pva_result;
    pva_result = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    SG_VHASH_NULLFREE(pCtx, pvh_audit);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
    SG_VARRAY_NULLFREE(pCtx, pva_result);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
}

void SG_audit__lookup(
        SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_hid_cs_target,
        SG_uint32 iDagNum,
        SG_varray** ppva
        )
{
    SG_string* pstr_where = NULL;
    SG_stringarray* psa_fields = NULL;
    char* psz_hid_cs_leaf = NULL;
    SG_vhash* pvh_audit = NULL;
    SG_varray* pva_result = NULL;

    SG_ASSERT(!SG_DAGNUM__IS_AUDIT(iDagNum));

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, (iDagNum | SG_DAGNUM__BIT__AUDIT), &psz_hid_cs_leaf)  );
    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "csid == '%s'", psz_hid_cs_target)  );
    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 2)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "who")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "when")  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, (iDagNum | SG_DAGNUM__BIT__AUDIT), psz_hid_cs_leaf, "item", SG_string__sz(pstr_where), "when #DESC", 0, 0, psa_fields, &pva_result)  );

    *ppva = pva_result;
    pva_result = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_audit);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
    SG_VARRAY_NULLFREE(pCtx, pva_result);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_STRING_NULLFREE(pCtx, pstr_where);
}

void SG_audit__query(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_query,
        SG_rbtree** pprb
        )
{
    SG_stringarray* psa_fields = NULL;
    char* psz_hid_cs_leaf = NULL;
    SG_rbtree* prb = NULL;
    SG_uint32 count_audits = 0;
    SG_uint32 i = 0;
    SG_varray* pva = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, (iDagNum | SG_DAGNUM__BIT__AUDIT), &psz_hid_cs_leaf)  );
    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "csid")  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, (iDagNum | SG_DAGNUM__BIT__AUDIT), psz_hid_cs_leaf, "item", psz_query, "when #DESC", 0, 0, psa_fields, &pva)  );

    SG_ERR_CHECK(  SG_rbtree__alloc(pCtx, &prb)  );

    if (pva)
    {
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count_audits )  );
        for (i=0; i<count_audits; i++)
        {
            const char* psz_csid = NULL;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, i, &psz_csid)  );
            SG_ERR_CHECK(  SG_rbtree__update(pCtx, prb, psz_csid)  );
        }
    }

    *pprb = prb;
    prb = NULL;

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
    SG_RBTREE_NULLFREE(pCtx, prb);
    SG_VARRAY_NULLFREE(pCtx, pva);
}


