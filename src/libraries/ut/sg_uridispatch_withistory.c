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
 * @file sg_uridispatch_log.c
 */

#include <sg.h>
#include "sg_uridispatch_private_typedefs.h"
#include "sg_uridispatch_private_prototypes.h"
#include "sg_zing__private.h"

//////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////

void _burndown_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement)
{
	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "TITLE"))
	{
		SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, replacement, "veracity / burndown chart")  );
	}
}


static void add_to_buckets(
	SG_context *pCtx,
    const char* psz_recid,
	SG_varray *history,
    SG_rbtree* prb_recs_by_rechid,
    SG_vhash* dateBuckets
    )
{
	SG_uint32 i, count;
    SG_vhash* newList = NULL;
    SG_vhash* pvh_rec = NULL;
    SG_vhash* pvh_copy = NULL;
	SG_vhash *wiList = NULL;

	SG_ERR_CHECK(  SG_varray__count(pCtx, history, &count)  );

	for ( i = 0; i < count; ++i )
	{
		SG_vhash *entry = NULL;
		SG_bool already;
        SG_varray *audits = NULL;
        SG_int64 when = -1;
        SG_vhash *audit = NULL;
        SG_time tm;
        char timeBuf[SG_TIME_FORMAT_LENGTH + 1];
        const char* psz_rechid = NULL;
		SG_vhash *prevInstance = NULL;
		SG_int64 oldwhen = 0;
		SG_bool had = SG_FALSE;

		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, history, i, &entry)  );

        SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, entry, "audits", &audits)  );
        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, audits, 0, &audit)  );
        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, audit, "when", &when)  );

        // convert milliseconds-since-epoch into individual fields.

        SG_ERR_CHECK_RETURN(  SG_time__decode__local(pCtx, when, &tm)  );

        SG_ERR_CHECK_RETURN( SG_sprintf(pCtx, timeBuf, sizeof(timeBuf) / sizeof(char), "%04d/%02d/%02d",
                         tm.year,tm.month,tm.mday,
                         tm.hour,tm.min,tm.sec,tm.msec,
                         tm.offsetUTC/3600,abs((tm.offsetUTC/60)%60)) );

        SG_ERR_CHECK(  SG_vhash__has(pCtx, dateBuckets, timeBuf, &already)  );

        if (! already)
        {
            SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &newList)  );
            wiList = newList;
            SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, dateBuckets, timeBuf, &newList)  );
        }
        else
        {
            SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, dateBuckets, timeBuf, &wiList)  );
        }

        // find the proper version of the record
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, entry, "rechid", &psz_rechid)  );
        SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_recs_by_rechid, psz_rechid, NULL, (void**) &pvh_rec)  );

        // make our own copy
        SG_ERR_CHECK(  SG_vhash__alloc__copy(pCtx, &pvh_copy, pvh_rec)  );

        // TODO why do we need to add when and watch for the latest?

		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_copy, "when", when)  );

		SG_ERR_CHECK(  SG_vhash__has(pCtx, wiList, psz_recid, &had)  );

		if (had)
		{
			SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, wiList, psz_recid, &prevInstance)  );
			SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, prevInstance, "when", &oldwhen)  );

			if (when > oldwhen)
			{
				SG_ERR_CHECK(  SG_vhash__update__vhash(pCtx, wiList, psz_recid, &pvh_copy)  );
			}
			else
			{
				SG_VHASH_NULLFREE(pCtx, pvh_copy);
			}
		}
		else
		{

			SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, wiList, psz_recid, &pvh_copy)  );
		}
    }

fail:
	SG_VHASH_NULLFREE(pCtx, pvh_copy);
	SG_VHASH_NULLFREE(pCtx, newList);
}

static void make_link_crit(
        SG_context* pCtx,
        const char* psz_sprintid,
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
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_name, "item_to_sprint")  );

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit_recid)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_recid, SG_ZING_FIELD__LINK__TO)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_recid, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit_recid, psz_sprintid)  );

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, &pva_crit_name)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "&&")  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, &pva_crit_recid)  );

    *ppva = pva_crit;
    pva_crit = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
}

static void get_all_the_record_histories(
	SG_context *pCtx,
    SG_repo * pRepo,
    const char* psz_hid_cs_leaf,
    SG_varray** ppva_recids,
    SG_varray** ppva
    )
{
    SG_varray* pva_crit = NULL;
    SG_varray* pva = NULL;
    SG_stringarray* psa_fields = NULL;

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "recid")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "in")  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, ppva_recids)  );

	SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 4)  );
	SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "id")  );
	SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "_rechid")  );
	SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
	SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "history")  );

    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pRepo, SG_DAGNUM__WORK_ITEMS, psz_hid_cs_leaf, pva_crit, psa_fields, &pva)  );

    *ppva = pva;
    pva = NULL;

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    SG_VARRAY_NULLFREE(pCtx, pva);
}

static void get_all_the_records(
	SG_context *pCtx,
    SG_repo * pRepo,
    SG_varray** ppva_recids,
    SG_varray** ppva
    )
{
    SG_varray* pva_crit = NULL;
    SG_varray* pva = NULL;
    SG_stringarray* psa_fields = NULL;

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "recid")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "in")  );
    SG_ERR_CHECK(  SG_varray__append__varray(pCtx, pva_crit, ppva_recids)  );

	SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 2)  );
	SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "_rechid")  );
	SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "*")  );

    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pRepo, SG_DAGNUM__WORK_ITEMS, NULL, pva_crit, psa_fields, &pva)  );

    *ppva = pva;
    pva = NULL;

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    SG_VARRAY_NULLFREE(pCtx, pva);
}

static void get_recids_in_sprint(
	SG_context *pCtx,
    SG_repo * pRepo,
    const char* pBaseline,
    const char* psz_sprintid,
    SG_varray** ppva
    )
{
    SG_varray* pva_crit = NULL;
    SG_varray* pva = NULL;
    SG_stringarray* psa_fields = NULL;

    SG_ERR_CHECK(  make_link_crit(pCtx, psz_sprintid, &pva_crit)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__LINK__FROM)  );
    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pRepo, SG_DAGNUM__WORK_ITEMS, pBaseline, pva_crit, psa_fields, &pva)  );

    *ppva = pva;
    pva = NULL;

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    SG_VARRAY_NULLFREE(pCtx, pva);
}

static void _GET__withistory_json(
	SG_context * pCtx,
	SG_repo* pRepo,
	const char *sprintid,
    _response_handle ** ppResponseHandle)
{
	SG_string *content = NULL;
    SG_varray* pva_histories = NULL;
    char* psz_hid_cs_leaf = NULL;
	SG_vhash *dateBuckets = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_varray* pva_recids_in_sprint = NULL;
    SG_varray* pva_recids_copy = NULL;
    SG_varray* pva_recs = NULL;
    SG_rbtree* prb_recs_by_rechid = NULL;

	SG_UNUSED(ppResponseHandle);

	SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__WORK_ITEMS, &psz_hid_cs_leaf)  );

    // TODO do some check here to make sure the sprint is valid?
    // return 404 when it's not

    // get a list of all the recids in the sprint
    SG_ERR_CHECK(  get_recids_in_sprint(pCtx, pRepo, psz_hid_cs_leaf, sprintid, &pva_recids_in_sprint)  );

    if (!pva_recids_in_sprint)
    {
        SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
    }

    // TODO silly that we need two copies of this
    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_recids_copy)  );
    SG_ERR_CHECK(  SG_varray__copy_items(pCtx, pva_recids_in_sprint, pva_recids_copy)  );

    // now fetch all the old records (without histories).  this query
    // does not pass a db state csid, so it returns all versions of
    // the records
    SG_ERR_CHECK(  get_all_the_records(pCtx, pRepo, &pva_recids_copy, &pva_recs)  );

    // build an rbtree so we can lookup the old records by their hid
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_recs, &count)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_recs_by_rechid)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh_rec = NULL;
        const char* psz_rechid = NULL;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_recs, i, &pvh_rec)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_rec, "_rechid", &psz_rechid)  );
        SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_recs_by_rechid, psz_rechid, pvh_rec)  );
    }

    // get a history for every recid in the sprint
    SG_ERR_CHECK(  get_all_the_record_histories(pCtx, pRepo, psz_hid_cs_leaf, &pva_recids_in_sprint, &pva_histories)  );

    // now go through all the histories, and for each spot where
    // a record was modified, add an entry to the proper bucket
	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &dateBuckets)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_histories, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh_rec = NULL;
		SG_varray *history = NULL;
        const char* psz_recid = NULL;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_histories, i, &pvh_rec)  );
		SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_rec, "history", &history)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_rec, "recid", &psz_recid)  );
		SG_ERR_CHECK(  add_to_buckets(pCtx, psz_recid, history, prb_recs_by_rechid, dateBuckets)  );
    }
    SG_VARRAY_NULLFREE(pCtx, pva_histories);

    // convert our result to json and we're done
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &content)  );
	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, dateBuckets, content)  );

	_create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &content, ppResponseHandle);

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_recs_by_rechid);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
	SG_VARRAY_NULLFREE(pCtx, pva_histories);
	SG_NULLFREE(pCtx, content);
	SG_VARRAY_NULLFREE(pCtx, pva_recs);
    SG_VHASH_NULLFREE(pCtx, dateBuckets);
}

void _dispatch__withistory(SG_context * pCtx,
    _request_headers * pRequestHeaders,
	SG_repo ** ppRepo,
	const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _response_handle ** ppResponseHandle)
{

	SG_bool found = SG_FALSE;
	SG_vhash* params = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"GET"))
		{
            if(pRequestHeaders->accept==SG_contenttype__json)			{
				SG_uint32 maxResults = 50;
				const char *sprintid = NULL;

				SG_VHASH__ALLOC(pCtx, &params);

				if (pRequestHeaders->pQueryString)
				{
					SG_ERR_CHECK(  SG_querystring_to_vhash(pCtx, pRequestHeaders->pQueryString, params)  );
					SG_ERR_CHECK(  SG_vhash__has(pCtx, params, "sprint", &found)  );

					if (found)
					{
						 SG_ERR_CHECK(   SG_vhash__get__sz(pCtx, params, "sprint", &sprintid)  );
						 if (maxResults == 0)
							  maxResults = SG_INT32_MAX;
					}

				}

				if (sprintid == NULL)
					SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
                SG_ERR_CHECK(  _GET__withistory_json(pCtx, *ppRepo, sprintid, ppResponseHandle)  );
			}
            else
                SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "burndown.xhtml", _burndown_replacer, ppResponseHandle)  );
		}
        else
            SG_ERR_THROW(  SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED  );
    }
    else
        SG_ERR_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);

fail:
	if (params)
		SG_VHASH_NULLFREE(pCtx, params);
}

