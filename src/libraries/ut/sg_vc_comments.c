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

/* TODO use #defines for the record keys */

void SG_vc_comments__add(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_hid_cs_target,
    const char* psz_value,
    const SG_audit* pq
    )
{
    char* psz_hid_cs_leaf = NULL;
    SG_zingtx* pztx = NULL;
    SG_zingrecord* prec = NULL;
    SG_dagnode* pdn = NULL;
    SG_changeset* pcs = NULL;
    SG_zingtemplate* pzt = NULL;
    SG_zingfieldattributes* pzfa = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_COMMENTS, &psz_hid_cs_leaf)  );

    /* start a changeset */
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, SG_DAGNUM__VC_COMMENTS, pq->who_szUserId, psz_hid_cs_leaf, &pztx)  );
    SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_hid_cs_leaf)  );
	SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pzt)  );

	SG_ERR_CHECK(  SG_zingtx__create_new_record(pCtx, pztx, "item", &prec)  );

    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, "item", "csid", &pzfa)  );
    SG_ERR_CHECK(  SG_zingrecord__set_field__dagnode(pCtx, prec, pzfa, psz_hid_cs_target) );

    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, "item", "text", &pzfa)  );
    SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, prec, pzfa, psz_value) );

    /* commit the changes */
	SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, pq->when_int64, &pztx, &pcs, &pdn, NULL)  );

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

void SG_vc_comments__lookup(SG_context* pCtx, SG_repo* pRepo, const char* psz_hid_cs, SG_varray** ppva_comments)
{
    char* psz_hid_cs_leaf = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    char buf_where[SG_HID_MAX_BUFFER_LENGTH + 64];

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_COMMENTS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_sprintf(pCtx, buf_where, sizeof(buf_where), "csid == '%s'", psz_hid_cs)  );
    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 2)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "text")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__HISTORY)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__RECID)  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_COMMENTS, psz_hid_cs_leaf, "item", buf_where, NULL, 0, 0, psa_fields, &pva)  );

    *ppva_comments = pva;
    pva= NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}

void SG_vc_comments__list_all(SG_context* pCtx, SG_repo* pRepo, SG_varray** ppva_comments)
{
    char* psz_hid_cs_leaf = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_COMMENTS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 4)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "text")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__RECID)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "csid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__HISTORY)  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_COMMENTS, psz_hid_cs_leaf, "item", NULL, NULL, 0, 0, psa_fields, &pva)  );

    *ppva_comments = pva;
    pva= NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}

void SG_vc_comments__list_for_given_changesets(SG_context* pCtx, SG_repo* pRepo, SG_varray** ppva_csid_list, SG_varray** ppva_comments)
{
    char* psz_hid_cs_leaf = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    SG_varray* pva_crit = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_COMMENTS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "csid")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "in")  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, ppva_csid_list)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 4)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "text")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__RECID)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "csid")  );
    //SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__HISTORY)  );
    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pRepo, SG_DAGNUM__VC_COMMENTS, psz_hid_cs_leaf, pva_crit, psa_fields, &pva)  );

    *ppva_comments = pva;
    pva= NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}

