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
#define MY_FIELD__STAMP "stamp"
#define MY_FIELD__CSID "csid"

void SG_vc_stamps__add(
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
    SG_bool bIsAlreadyThere = SG_FALSE;

    SG_ERR_CHECK(  SG_vc_stamps__is_stamp_already_applied(pCtx, pRepo, psz_hid_cs_target, psz_value, &bIsAlreadyThere)  );
    if (bIsAlreadyThere)
    	return;
    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, &psz_hid_cs_leaf)  );

    /* start a changeset */
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, pq->who_szUserId, psz_hid_cs_leaf, &pztx)  );
    SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_hid_cs_leaf)  );
	SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pzt)  );

	SG_ERR_CHECK(  SG_zingtx__create_new_record(pCtx, pztx, "item", &prec)  );

    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, "item", MY_FIELD__CSID, &pzfa)  );
    SG_ERR_CHECK(  SG_zingrecord__set_field__dagnode(pCtx, prec, pzfa, psz_hid_cs_target) );

    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, "item", MY_FIELD__STAMP, &pzfa)  );
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

void SG_vc_stamps__list_all_stamps(SG_context* pCtx, SG_repo* pRepo, SG_varray ** pVArrayResults)
{
	char* psz_hid_cs_leaf = NULL;
	SG_stringarray* psa_fields = NULL;
	SG_varray* pva = NULL;
	SG_varray* pvaResults = NULL;

	SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, &psz_hid_cs_leaf)  );

	SG_ASSERT(psz_hid_cs_leaf);

	SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
	SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, MY_FIELD__STAMP)  );
	SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, psz_hid_cs_leaf, "item", NULL, "stamp #ASC", 0, 0, psa_fields, &pva)  );

	if (pva != NULL)
	{
		SG_uint32 count_records = 0;
		SG_uint32 index_records = 0;
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pvaResults)  );
		SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count_records)  );
		if(count_records>0)
		{
			const char * lastStamp = "";
			SG_int64 countFound = 0;
			const char * psz_stamp = NULL;
			for(index_records = 0; index_records < count_records; index_records++)
			{
				SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, index_records, &psz_stamp)  );

				if (strcmp(lastStamp, psz_stamp) != 0)
				{
					if (strcmp(lastStamp, "") != 0)
					{
						//This is not our first trip through
						SG_vhash * pvhResult = NULL;
						SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhResult)  );
						SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvhResult, "count", countFound)  );
						SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhResult, "stamp", lastStamp)  );
						SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pvaResults, &pvhResult) );
					}

					lastStamp = psz_stamp;
					countFound = 1;
				}
				else
					countFound++;
				if (index_records == (count_records - 1))
				{
					//This is the last stamp.
					SG_vhash * pvhResult = NULL;
					SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhResult)  );
					SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvhResult, "count", countFound)  );
					SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhResult, "stamp", lastStamp)  );
					SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pvaResults, &pvhResult) );
				}
			}
			SG_VARRAY_NULLFREE(pCtx, pva);
		}
	}

	*pVArrayResults = pvaResults;
	SG_VARRAY_NULLFREE(pCtx, pva);

fail:
	SG_VARRAY_NULLFREE(pCtx, pva);
	SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
	SG_NULLFREE(pCtx, psz_hid_cs_leaf);

}

void SG_vc_stamps__lookup(SG_context* pCtx, SG_repo* pRepo, const char* psz_hid_cs, SG_varray** ppva_stamps)
{
    char* psz_hid_cs_leaf = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    char buf_where[SG_HID_MAX_BUFFER_LENGTH + 64];

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_sprintf(pCtx, buf_where, sizeof(buf_where), "%s == '%s'", MY_FIELD__CSID, psz_hid_cs)  );
    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 2)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, MY_FIELD__STAMP)  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, psz_hid_cs_leaf, "item", buf_where, NULL, 0, 0, psa_fields, &pva)  );

    *ppva_stamps = pva;
    pva = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}

void SG_vc_stamps__is_stamp_already_applied(SG_context* pCtx, SG_repo* pRepo, const char* psz_hid_cs, const char * psz_stamp, SG_bool * pbStampIsThere)
{
    char* psz_hid_cs_leaf = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    SG_string * pstring_where = NULL;
    *pbStampIsThere = SG_FALSE;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstring_where)  );
    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstring_where, "(%s == '%s') && (%s == '%s')", MY_FIELD__CSID, psz_hid_cs, MY_FIELD__STAMP, psz_stamp)  );
    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 2)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, MY_FIELD__STAMP)  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, psz_hid_cs_leaf, "item", SG_string__sz(pstring_where), NULL, 0, 0, psa_fields, &pva)  );

    if (pva == NULL)
    	*pbStampIsThere = SG_FALSE;
    else
    {
    	SG_uint32 nCountOfResults = 0;
    	SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &nCountOfResults)  );
    	if (nCountOfResults == 0)
    		*pbStampIsThere = SG_FALSE;
    	else
    		*pbStampIsThere = SG_TRUE;
    }

fail:
	SG_STRING_NULLFREE(pCtx, pstring_where);
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}

void SG_vc_stamps__lookup_by_stamp(SG_context * pCtx, SG_repo* pRepo, const char * pszStamp, SG_stringarray ** pstringarray_Results)
{
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    char* psz_hid_cs_leaf = NULL;
    SG_string * pstring_where = NULL;
    SG_stringarray * pstringarray_Results_Temp = NULL;

	SG_NULLARGCHECK_RETURN(pszStamp);

	SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, &psz_hid_cs_leaf)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstring_where)  );
	SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
	SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, MY_FIELD__CSID)  );

	SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstring_where, "%s == '%s'", MY_FIELD__STAMP, pszStamp)  );
	SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, psz_hid_cs_leaf, "item", SG_string__sz(pstring_where), NULL, 0, 0, psa_fields, &pva)  );

	if (pva)
	{
		SG_uint32 count_records = 0;
		SG_uint32 index_records = 0;
		const char* psz_csid = NULL;
		SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &pstringarray_Results_Temp, 1)  );
		SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count_records)  );
		if(count_records>0)
		{
			for (index_records = 0; index_records < count_records; index_records++)
			{
				SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, index_records, &psz_csid)  );
				SG_ERR_CHECK(  SG_stringarray__add(pCtx, pstringarray_Results_Temp, psz_csid)  );
			}
		}

		SG_VARRAY_NULLFREE(pCtx, pva);
		*pstringarray_Results = pstringarray_Results_Temp;
	}
fail:
	SG_STRING_NULLFREE(pCtx, pstring_where);
	SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
	SG_NULLFREE(pCtx, psz_hid_cs_leaf);

}

void SG_vc_stamps__remove(SG_context* pCtx, SG_repo* pRepo, SG_audit* pq, const char * psz_dagnode_hid, SG_uint32 count_args, const char * const* paszArgs)
{
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    char* psz_hid_cs_leaf = NULL;
    SG_uint32 i = 0;
    SG_zingtx* pztx = NULL;
    SG_dagnode* pdn = NULL;
    SG_changeset* pcs = NULL;
    const char* psz_recid = NULL;
    SG_string * pstring_where = NULL;

    SG_NULLARGCHECK_RETURN(psz_dagnode_hid);

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    /* start a changeset */
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, pq->who_szUserId, psz_hid_cs_leaf, &pztx)  );
    SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_hid_cs_leaf)  );

    if (count_args == 0)
    {
    	char buf_where[SG_HID_MAX_BUFFER_LENGTH + 64];

    	SG_ERR_CHECK(  SG_sprintf(pCtx, buf_where, sizeof(buf_where), "%s == '%s'", MY_FIELD__CSID, psz_dagnode_hid)  );
		SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
		SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
		SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, psz_hid_cs_leaf, "item", buf_where, NULL, 0, 0, psa_fields, &pva)  );

		if (pva)
		{
			SG_uint32 count_records = 0;
			SG_uint32 index_records = 0;
			SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count_records)  );
			if(count_records>0)
			{
				for(index_records = 0; index_records < count_records; index_records++)
				{
					SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, index_records, &psz_recid)  );

					/* remove the record */
					SG_ERR_CHECK(  SG_zingtx__delete_record(pCtx, pztx, psz_recid)  );
				}
				SG_VARRAY_NULLFREE(pCtx, pva);
			}
		}
    }
    else
    {
    	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstring_where)  );
		/* for each stamp to be deleted, look up the HID of its dbrecord */
		for (i=0; i<count_args; i++)
		{
			SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstring_where, "(%s == '%s') && (%s == '%s')", MY_FIELD__CSID, psz_dagnode_hid, MY_FIELD__STAMP, paszArgs[i])  );
			SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
			SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
			SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, psz_hid_cs_leaf, "item", SG_string__sz(pstring_where), NULL, 0, 0, psa_fields, &pva)  );

			if (pva)
			{
				SG_uint32 count_records = 0;
				SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count_records)  );
				if(count_records > 0)
				{
					//Don't throw an error.

					SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, 0, &psz_recid)  );

					/* remove the record */
					SG_ERR_CHECK(  SG_zingtx__delete_record(pCtx, pztx, psz_recid)  );
				}

				SG_VARRAY_NULLFREE(pCtx, pva);
			}
			SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
		}
		SG_STRING_NULLFREE(pCtx, pstring_where);
    }

    /* commit the changes */
	SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, pq->when_int64, &pztx, &pcs, &pdn, NULL)  );

    // fall thru

fail:
    if (pztx)
    {
        SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pztx)  );
    }
    SG_STRING_NULLFREE(pCtx, pstring_where);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
}

void SG_vc_stamps__list_all(SG_context* pCtx, SG_repo* pRepo, SG_varray** ppva_stamps)
{
    char* psz_hid_cs_leaf = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 3)  );
    // TODO do we need _history here?
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "csid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, MY_FIELD__STAMP)  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, psz_hid_cs_leaf, "item", NULL, NULL, 0, 0, psa_fields, &pva)  );

    *ppva_stamps = pva;
    pva = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}


void SG_vc_stamps__list_for_given_changesets(SG_context* pCtx, SG_repo* pRepo, SG_varray** ppva_csid_list, SG_varray** ppva_stamps)
{
    char* psz_hid_cs_leaf = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_varray* pva = NULL;
    SG_varray* pva_crit = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, &psz_hid_cs_leaf)  );

    SG_ASSERT(psz_hid_cs_leaf);

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "csid")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "in")  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, ppva_csid_list)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 3)  );
    // TODO do we need _history here?
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "csid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, MY_FIELD__STAMP)  );

    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pRepo, SG_DAGNUM__VC_STAMPS, psz_hid_cs_leaf, pva_crit, psa_fields, &pva)  );

    *ppva_stamps = pva;
    pva = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}


