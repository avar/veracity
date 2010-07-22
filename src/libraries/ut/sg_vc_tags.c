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

#define MY_FIELD__TAG "tag"

static void sg_tags__list_specific_state(SG_context* pCtx, SG_repo* pRepo, const char* pszHid_cs, SG_rbtree** pprb)
{
    SG_stringarray* psa_fields = NULL;
    SG_rbtree* prbTags = NULL;
    SG_uint32 count_tags = 0;
    SG_uint32 i;
    SG_varray* pva = NULL;

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 2)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, MY_FIELD__TAG)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "csid")  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_TAGS, pszHid_cs, "item", NULL, NULL, 0, 0, psa_fields, &pva)  );
    if (pva)
    {
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count_tags )  );
        SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS(pCtx, &prbTags, count_tags, NULL)  );

        for (i=0; i<count_tags; i++)
        {
            SG_vhash* pvh = NULL;
            const char* psz_tag = NULL;
            const char* psz_hidcs = NULL;

            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, MY_FIELD__TAG, &psz_tag)  );
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "csid", &psz_hidcs)  );
            SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, prbTags, psz_tag, psz_hidcs)  );
        }
    }

    *pprb = prbTags;
    prbTags = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_RBTREE_NULLFREE(pCtx, prbTags);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
}


void SG_vc_tags__list(SG_context* pCtx, SG_repo* pRepo, SG_rbtree** pprb)
{
    char* psz_hid_cs_leaf = NULL;
    SG_rbtree* prbTags = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_TAGS, &psz_hid_cs_leaf)  );

    if (psz_hid_cs_leaf)
    {
        SG_ERR_CHECK(  sg_tags__list_specific_state(pCtx, pRepo, psz_hid_cs_leaf, &prbTags)  );

        *pprb = prbTags;
    }
    else
    {
        *pprb = NULL;
    }

    SG_NULLFREE(pCtx, psz_hid_cs_leaf);

	return;
fail:
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}

void SG_vc_tags__remove(SG_context* pCtx, SG_repo* pRepo, SG_audit* pq, SG_uint32 count_args, const char** paszArgs)
{
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    char* psz_hid_cs_leaf = NULL;
    SG_uint32 i = 0;
    SG_zingtx* pztx = NULL;
    SG_dagnode* pdn = NULL;
    char buf_where[SG_HID_MAX_BUFFER_LENGTH + 64];
    SG_changeset* pcs = NULL;
    const char* psz_recid = NULL;

    if (0 == count_args)
        return;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_TAGS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    /* start a changeset */
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, SG_DAGNUM__VC_TAGS, pq->who_szUserId, psz_hid_cs_leaf, &pztx)  );
    SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_hid_cs_leaf)  );

    /* for each tag to be deleted, look up the HID of its dbrecord */
    for (i=0; i<count_args; i++)
    {
        SG_ERR_CHECK(  SG_sprintf(pCtx, buf_where, sizeof(buf_where), "%s == '%s'", MY_FIELD__TAG, paszArgs[i])  );
        SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
        SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
        SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_TAGS, psz_hid_cs_leaf, "item", buf_where, NULL, 0, 0, psa_fields, &pva)  );

        if (pva)
        {
            SG_uint32 count_records = 0;
            SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count_records)  );
            if(count_records==0)
            {
                SG_ERR_THROW2(  SG_ERR_TAG_NOT_FOUND,
                        (pCtx, "%s", paszArgs[i])
                        );
            }

            /* there should be ONE result */
            SG_ASSERT(1 == count_records);

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, 0, &psz_recid)  );

            /* remove the record */
            SG_ERR_CHECK(  SG_zingtx__delete_record(pCtx, pztx, psz_recid)  );

            SG_VARRAY_NULLFREE(pCtx, pva);
        }
        else
        {
            SG_ERR_THROW2(  SG_ERR_TAG_NOT_FOUND,
                    (pCtx, "%s", paszArgs[i])
                    );
        }
        SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    }

    /* commit the changes */
	SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, pq->when_int64, &pztx, &pcs, &pdn, NULL)  );

    // fall thru

fail:
    if (pztx)
    {
        SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pztx)  );
    }
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
}

void SG_vc_tags__add(SG_context* pCtx, SG_repo* pRepo, const char* psz_hid_cs_target, const char* psz_tag, const SG_audit* pq)
{
    char* psz_hid_cs_leaf = NULL;
    SG_zingtx* pztx = NULL;
    SG_zingrecord* prec = NULL;
    SG_dagnode* pdn = NULL;
    SG_changeset* pcs = NULL;
    SG_zingtemplate* pzt = NULL;
    SG_zingfieldattributes* pzfa = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_TAGS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    /* start a changeset */
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, SG_DAGNUM__VC_TAGS, pq->who_szUserId, psz_hid_cs_leaf, &pztx)  );
    SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_hid_cs_leaf)  );
	SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pzt)  );

	SG_ERR_CHECK(  SG_zingtx__create_new_record(pCtx, pztx, "item", &prec)  );

    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, "item", "csid", &pzfa)  );
    SG_ERR_CHECK(  SG_zingrecord__set_field__dagnode(pCtx, prec, pzfa, psz_hid_cs_target) );

    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, "item", MY_FIELD__TAG, &pzfa)  );
    SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, prec, pzfa, psz_tag) );

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

void SG_vc_tags__lookup(SG_context* pCtx, SG_repo* pRepo, const char* psz_hid_cs, SG_varray** ppva_tags)
{
    char* psz_hid_cs_leaf = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    char buf_where[SG_HID_MAX_BUFFER_LENGTH + 64];

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_TAGS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_sprintf(pCtx, buf_where, sizeof(buf_where), "csid == '%s'", psz_hid_cs)  );
    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 2)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "tag")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_TAGS, psz_hid_cs_leaf, "item", buf_where, NULL, 0, 0, psa_fields, &pva)  );

    *ppva_tags = pva;
    pva = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}

void SG_vc_tags__lookup__tag(SG_context* pCtx, SG_repo* pRepo, const char* psz_tag, char** ppsz_hid_cs)
{
    char* psz_hid_cs_leaf = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    SG_uint32 count_query_results = 0;
    const char* psz_csid = NULL;
    char buf_where[SG_HID_MAX_BUFFER_LENGTH + 64];

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_TAGS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_sprintf(pCtx, buf_where, sizeof(buf_where), "%s == '%s'", MY_FIELD__TAG, psz_tag)  );
    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "csid")  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_TAGS, psz_hid_cs_leaf, "item", buf_where, NULL, 0, 0, psa_fields, &pva)  );

    if (pva)
    {
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count_query_results)  );

		if (count_query_results == 0)
		{
			  *ppsz_hid_cs = NULL;
		}
		else
		{
			//the assert fails if the tag is invalid
			SG_ASSERT(count_query_results==1);
			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, 0, &psz_csid)  );
			SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_csid, ppsz_hid_cs)  );
		}
    }
    else
    {
        *ppsz_hid_cs = NULL;
    }

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}

// TODO this is used only from a unit test
void SG_vc_tags__build_reverse_lookup(SG_context* pCtx, const SG_rbtree* prbTagToHid, SG_rbtree** pprbHidToTag)
{
SG_rbtree* prb = NULL;
SG_uint32 count = 0;
SG_rbtree_iterator* pIterator = NULL;
const char* psz_tag = NULL;
const char* psz_hid = NULL;
SG_bool b;
SG_bool bAlreadyThere;

SG_ERR_CHECK(  SG_rbtree__count(pCtx, prbTagToHid, &count)  );
SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS(pCtx, &prb, count, NULL)  );

SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, prbTagToHid, &b, &psz_tag, (void**) &psz_hid)  );
while (b)
{
SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb, psz_hid, &bAlreadyThere, NULL)  );
if (!bAlreadyThere)
{
SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, prb, psz_hid, psz_tag)  );
}

SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &psz_tag, (void**) &psz_hid)  );
}
SG_rbtree__iterator__free(pCtx, pIterator);
pIterator = NULL;

*pprbHidToTag = prb;

return;
fail:
SG_RBTREE_NULLFREE(pCtx, prb);
}

void SG_vc_tags__list_all(SG_context* pCtx, SG_repo* pRepo, SG_varray** ppva_tags)
{
    char* psz_hid_cs_leaf = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_TAGS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 3)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "tag")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "csid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
    // TODO do we need history here?
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_TAGS, psz_hid_cs_leaf, "item", NULL, NULL, 0, 0, psa_fields, &pva)  );

    *ppva_tags = pva;
    pva = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}

void SG_vc_tags__list_for_given_changesets(SG_context* pCtx, SG_repo* pRepo, SG_varray** ppva_csid_list, SG_varray** ppva_tags)
{
    char* psz_hid_cs_leaf = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    SG_varray* pva_crit = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_TAGS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "csid")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "in")  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, ppva_csid_list)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 3)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "tag")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "csid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
    // TODO do we need history here?

    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pRepo, SG_DAGNUM__VC_TAGS, psz_hid_cs_leaf, pva_crit, psa_fields, &pva)  );

    *ppva_tags = pva;
    pva = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}

