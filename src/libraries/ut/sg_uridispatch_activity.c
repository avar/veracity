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

#define eq(str1,str2) (strcmp(str1,str2)==0)

static void _addHistoryActivity(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
	SG_varray *acts,
    SG_repo * pRepo)
{
    SG_varray * pVArrayResults = NULL;
    SG_int64 nFromDate = 0;
    SG_int64 nToDate = SG_INT64_MAX;
	SG_vhash *act = NULL;
	SG_string *what = NULL;
	SG_string *who  = NULL;
	SG_string *link = NULL;
	SG_int64 lastwhen = 0;

	SG_NULLARGCHECK_RETURN(acts);
	SG_NULLARGCHECK_RETURN(pRepo);

	if (pRequestHeaders->ifModifiedSince > 0)
		nFromDate = pRequestHeaders->ifModifiedSince;

	SG_ERR_CHECK(  SG_history__query(pCtx, NULL, pRepo, 0, NULL, NULL, 0, NULL, NULL, 25, nFromDate, nToDate, SG_TRUE, SG_FALSE, &pVArrayResults)  );

	// fall thru to common cleanup
	if (pVArrayResults != NULL)
	{
		SG_uint32 resultsLength = 0;
		SG_uint32 i = 0;
		SG_vhash * currentDagnode = NULL;
		SG_varray * pVArray = NULL;
		SG_vhash * pVHashCurrentThingy = NULL;
		SG_uint32 nCount = 0;
		SG_uint32 nIndex = 0;
		SG_int64 when = 0;
		const char * pszUser = NULL;
		const char * pszComment = NULL;

		SG_ERR_CHECK(  SG_varray__count(pCtx, pVArrayResults, &resultsLength)  );

		for (i = 0; i < resultsLength; i++)
		{
			const char *csid = NULL;

			SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &act)  );
			SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &what)  );
			SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &who)  );
			SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &link)  );

			SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pVArrayResults, i, &currentDagnode)  );

			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, currentDagnode, "changeset_id", &csid)  );

			SG_ERR_CHECK(  SG_string__sprintf(pCtx, link, "/changesets/%s", csid)  );

			SG_ERR_CHECK(  SG_string__set__sz(pCtx, what, "checkin")  );

			SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, currentDagnode, "audits", &pVArray)  );
			SG_ERR_CHECK(  SG_varray__count(pCtx, pVArray, &nCount)  );

			lastwhen = 0;

			for (nIndex = 0; nIndex < nCount; nIndex++)
			{
				SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pVArray, nIndex, &pVHashCurrentThingy)  );
				SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pVHashCurrentThingy, "when", &when)  );
				SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVHashCurrentThingy, "who", &pszUser)  );

				if (when > lastwhen)
				{
					lastwhen = when;
					SG_ERR_CHECK(  SG_string__set__sz(pCtx, who, pszUser)  );
				}
			}

			SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, currentDagnode, "comments", &pVArray)  );
			SG_ERR_CHECK(  SG_varray__count(pCtx, pVArray, &nCount)  );

			if (nCount > 0)
			{
				SG_vhash *vhcmt = NULL;

				SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pVArray, nCount - 1, &vhcmt)  );
				SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, vhcmt, "text", &pszComment)  );
				SG_ERR_CHECK(  SG_string__append__sz(pCtx, what, ": ")  );
				SG_ERR_CHECK(  SG_string__append__sz(pCtx, what, pszComment)  );
			}

			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, act, "what", SG_string__sz(what))  );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, act, "who", SG_string__sz(who))  );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, act, "link", SG_string__sz(link))  );
			SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, act, "when", lastwhen)  );

			SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, acts, &act)  );

			SG_STRING_NULLFREE(pCtx, who);
			SG_STRING_NULLFREE(pCtx, what);
			SG_STRING_NULLFREE(pCtx, link);
		}
	}

fail:
	SG_VARRAY_NULLFREE(pCtx, pVArrayResults);
	SG_VHASH_NULLFREE(pCtx, act);
	SG_STRING_NULLFREE(pCtx, what);
	SG_STRING_NULLFREE(pCtx, who);
	SG_STRING_NULLFREE(pCtx, link);
}

static void get_wit_comment_links(
	SG_context *pCtx,
    SG_repo * pRepo,
    const char* pBaseline,
    SG_rbtree** pprb
    )
{
    SG_varray* pva_crit = NULL;
    SG_varray* pva = NULL;
    SG_rbtree* prb = NULL;
    SG_stringarray* psa_fields = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;

    /* construct the crit */
    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva_crit)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, SG_ZING_FIELD__LINK__NAME)  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "==")  );
    SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_crit, "comment_to_item")  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 2)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__LINK__FROM)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, SG_ZING_FIELD__LINK__TO)  );
    SG_ERR_CHECK(  SG_repo__dbndx__query_list(pCtx, pRepo, SG_DAGNUM__WORK_ITEMS, pBaseline, pva_crit, psa_fields, &pva)  );

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    for (i=0; i<count; i++)
    {
        const char *psz_from = NULL;
        const char *psz_to = NULL;
        SG_vhash* vhrec = NULL;

		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &vhrec)  );

        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, vhrec, SG_ZING_FIELD__LINK__FROM, &psz_from)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, vhrec, SG_ZING_FIELD__LINK__TO, &psz_to)  );

        SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, prb, psz_from, psz_to)  );
    }

    *pprb = prb;
    prb = NULL;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VARRAY_NULLFREE(pCtx, pva_crit);
    SG_VARRAY_NULLFREE(pCtx, pva);
}

static void do_wit_comments(
	SG_context *pCtx,
	SG_varray *acts,
    SG_repo * pRepo,
	SG_int64 nFromDate,
    const char* pBaseline
    )
{
    SG_varray* pva_query_result = NULL;
	SG_vhash *pvh_copy = NULL;
	SG_vhash *vhrec = NULL;
    SG_stringarray* psa_fields = NULL;
	SG_uint32 count = 0;
    SG_rbtree* prb = NULL;
    SG_uint32 i;
	SG_string *link = NULL;
	SG_string *what = NULL;
    SG_rbtree* prb_comment_links = NULL;

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 1)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "*")  ); // TODO just get what we need
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__WORK_ITEMS, pBaseline, "comment", NULL, "#WHEN #DESC", 25, 0, psa_fields, &pva_query_result)  );

    if(pva_query_result==NULL)
		goto fail;	// no error, the rest of the activity stream still needs to go out

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_query_result, &count )  );
    if(count==0)
		goto fail;	// no error, the rest of the activity stream still needs to go out

    SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS(pCtx, &prb, count, NULL)  );

    SG_ERR_CHECK(  get_wit_comment_links(pCtx, pRepo, pBaseline, &prb_comment_links)  );

	for (i=0; i<count; i++)
	{
        const char *text = NULL;
        const char *psz_recid_comment = NULL;
        const char *psz_recid_item = NULL;
        SG_int64	when = 0;

		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_query_result, i, &vhrec)  );

        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, vhrec, "text", &text)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, vhrec, "recid", &psz_recid_comment)  );
        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, vhrec, "when", &when)  );

        if (when >= nFromDate)
        {
            // TODO do we need to copy here?
            SG_ERR_CHECK(  SG_vhash__alloc__copy(pCtx, &pvh_copy, vhrec)  );

            SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &what)  );
            SG_ERR_CHECK(  SG_string__sprintf(pCtx, what, "Comment: %s", text)  );
            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_copy, "what", SG_string__sz(what))  );
            SG_STRING_NULLFREE(pCtx, what);

            SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &link)  );
            SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_comment_links, psz_recid_comment, NULL, (void**) &psz_recid_item)  );
            SG_ERR_CHECK(  SG_string__sprintf(pCtx, link, "/wit/records/%s", psz_recid_item)  );
            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_copy, "link", SG_string__sz(link))  );
            SG_STRING_NULLFREE(pCtx, link);

            SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, acts, &pvh_copy)  );
        }
	}

fail:
	SG_STRING_NULLFREE(pCtx, link);
	SG_VARRAY_NULLFREE(pCtx, pva_query_result);
	SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
	SG_RBTREE_NULLFREE(pCtx, prb);
	SG_RBTREE_NULLFREE(pCtx, prb_comment_links);
	SG_VHASH_NULLFREE(pCtx, pvh_copy);
}

static void do_wit_items(
    SG_context * pCtx,
	SG_varray *acts,
    SG_repo * pRepo,
	SG_int64 nFromDate,
    const char* pBaseline
    )
{
    SG_varray* pva_query_result = NULL;
	SG_vhash *pvh_copy = NULL;
	SG_vhash *vhrec = NULL;
	SG_varray *recHistory = NULL;
	SG_vhash *histItem = NULL;
    SG_stringarray* psa_fields = NULL;
	SG_uint32 count = 0;
    SG_rbtree* prb = NULL;
    SG_uint32 i;
	SG_uint32 hcount = 0;
	const char *recid = NULL;
	SG_varray *pva_audits = NULL;
	SG_vhash *pvh_audit = NULL;
	const char *psz_who = NULL;
	SG_string *link = NULL;
	SG_int64 time;

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 3)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "title")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "history")  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__WORK_ITEMS, pBaseline, "item", NULL, "#WHEN #DESC", 25, 0, psa_fields, &pva_query_result)  );

    if(pva_query_result==NULL)
		goto fail;	// no error, the rest of the activity stream still needs to go out

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_query_result, &count )  );
    if(count==0)
		goto fail;	// no error, the rest of the activity stream still needs to go out

    SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS(pCtx, &prb, count, NULL)  );

	for (i=0; i<count; i++)
	{
		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_query_result, i, &vhrec)  );

		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, vhrec, SG_ZING_FIELD__RECID, &recid)  );
		SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, vhrec, SG_ZING_FIELD__HISTORY, &recHistory)  );

		hcount = 0;

		if (recHistory)
			SG_ERR_CHECK(  SG_varray__count(pCtx, recHistory, &hcount)  );

		if (hcount > 0)
		{
			SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, recHistory, 0, &histItem)  );

			if (histItem)
			{
				const char *what = NULL;

				SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, histItem, "audits", &pva_audits)  );
				SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_audits, 0, &pvh_audit)  );
				SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_audit, "when", &time)  );

				if (time >= nFromDate)
				{
                    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh_copy)  );

					SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &link)  );
					SG_ERR_CHECK(  SG_string__sprintf(pCtx, link, "/wit/records/%s", recid)  );

					SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_audit, "who", &psz_who)  );
					SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_copy, "when", time)  );
					SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_copy, "who", psz_who)  );
					SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_copy, "link", SG_string__sz(link))  );
					SG_STRING_NULLFREE(pCtx, link);

					SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, vhrec, "title", &what)  );
					SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_copy, "what", what)  );

					SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, acts, &pvh_copy)  );
				}

				pva_audits = NULL;
				pvh_audit = NULL;
			}
		}
	}

fail:
	SG_STRING_NULLFREE(pCtx, link);
	SG_VARRAY_NULLFREE(pCtx, pva_query_result);
	SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
	SG_RBTREE_NULLFREE(pCtx, prb);
	SG_VHASH_NULLFREE(pCtx, pvh_copy);
}

static void _addWitActivity(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
	SG_varray *acts,
    SG_repo * pRepo)
{
	SG_rbtree * pLeaves = NULL;
    SG_uint32 leafCount = 0;
    const char * pBaseline = NULL;
    SG_int64 nFromDate = 0;


    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, SG_DAGNUM__WORK_ITEMS, &pLeaves)  );
    SG_ERR_CHECK(  SG_rbtree__count(pCtx, pLeaves, &leafCount)  );
    if(leafCount==0)
		goto fail;	// no error, the rest of the activity stream still needs to go out

    if(leafCount>1)
        SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED); 

    SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, pLeaves, &pBaseline, NULL)  );

	if (pRequestHeaders->ifModifiedSince > 0)
    {
		nFromDate = pRequestHeaders->ifModifiedSince;
    }

    SG_ERR_CHECK(  do_wit_items(pCtx, acts, pRepo, nFromDate, pBaseline)  );
    SG_ERR_CHECK(  do_wit_comments(pCtx, acts, pRepo, nFromDate, pBaseline)  );

fail:
	SG_RBTREE_NULLFREE(pCtx, pLeaves);
}

struct _expandContext {
	SG_repo *pRepo;
	SG_vhash *users;
};

static void _expandWho(SG_context* pCtx, void* pVoidData, const SG_varray* pva, SG_uint32 ndx, const SG_variant* pv)
{
	SG_vhash * pVhu = NULL;
	SG_vhash *act;
	struct _expandContext *context = (struct _expandContext *)pVoidData;
	SG_bool got = SG_FALSE;
	const char *uid = NULL;
	const char *expanded = NULL;

	SG_UNUSED(pva);
	SG_UNUSED(ndx);

	SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &act)  );
	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, act, "who", &uid)  );

	if ((uid == NULL) || (strlen(uid) == 0))
	{
		return;	
	}

	expanded = uid;

	SG_ERR_CHECK(  SG_vhash__has(pCtx, context->users, uid, &got)  );

	if (got)
	{
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, context->users, uid, &expanded)  );
	}
	else
	{
		SG_user__lookup_by_userid(pCtx, context->pRepo, uid, &pVhu);

		if (SG_context__has_err(pCtx))
		{
			if (SG_context__err_equals(pCtx, SG_ERR_ZING_RECORD_NOT_FOUND))
			{
				SG_context__err_reset(pCtx);
			}
			else
			{
				SG_ERR_CHECK_CURRENT;
			}
		}
		else if (pVhu != NULL)
		{
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pVhu, "email", &expanded)  );
		}

		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, context->users, uid, expanded)  );
	}

	SG_ERR_CHECK(  SG_vhash__update__string__sz(pCtx, act, "who", expanded)  );

fail:
	SG_VHASH_NULLFREE(pCtx, pVhu);
}


static int _revWhenSort(
	SG_context * pCtx,
	const void * pArg1,
	const void * pArg2,
	void * pVoidCallerData)
{
	SG_vhash *act1 = NULL;
	SG_vhash *act2 = NULL;
	const SG_variant *v1 = *(const SG_variant **)pArg1;
	const SG_variant *v2 = *(const SG_variant **)pArg2;
	SG_int64	when1 = 0;
	SG_int64	when2 = 0;

	SG_UNUSED(pVoidCallerData);

	SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, v1, &act1)  );
	SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, v2, &act2)  );

	SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, act1, "when", &when1)  );
	SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, act2, "when", &when2)  );

	if (when1 > when2)
		return(-1);
	else if (when1 < when2)
		return(1);
	else
		return(0);

fail:
	return(0);
}


static void _removeTimelessActs(
	SG_context *pCtx,
	SG_varray *acts,
	SG_uint32 *pCount)
{
	SG_NULLARGCHECK_RETURN(acts);
	SG_NULLARGCHECK_RETURN(pCount);

	if (*pCount > 0)
	{
		SG_uint32 i = *pCount;

		while (i > 0)
		{
			SG_vhash *act = NULL;
			SG_int64 when = 0;

			--i;

			SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, acts, i, &act)  );
			SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, act, "when", &when)  );

			if (when == 0)
			{
				SG_ERR_CHECK(  SG_varray__remove(pCtx, acts, i)  );
				--*pCount;
			}
		}
	}

fail:
	;
}

static void _collect_activity_stream(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
	SG_varray **ppActs,
	const char **status)
{
    char * pBaseline = NULL;
	SG_uint32 count = 0;
	SG_varray *acts = NULL;
	SG_bool	hasIms = SG_FALSE;
	SG_string *pstr_where = NULL;
	SG_stringarray *psa_fields = NULL;
	SG_vhash *users = NULL;
	struct _expandContext ctxt;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, *ppRepo, SG_DAGNUM__SUP, &pBaseline)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );

	if (pRequestHeaders->ifModifiedSince != ((SG_int64)0xffffffffffffffffLL))
		SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "when >= %lld", pRequestHeaders->ifModifiedSince)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 3)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "who")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "when")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "what")  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, *ppRepo, SG_DAGNUM__SUP, pBaseline, "item", SG_string__sz(pstr_where), "when #DESC", 0, 0, psa_fields, &acts)  );

	if (acts == NULL)
		SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &acts)  );

	SG_ERR_CHECK(  _addHistoryActivity(pCtx, pRequestHeaders, acts, *ppRepo)  );
	SG_ERR_CHECK(  _addWitActivity(pCtx, pRequestHeaders, acts, *ppRepo)  );

	SG_ERR_CHECK(  SG_varray__count(pCtx, acts, &count)  );

	SG_ERR_CHECK(  _removeTimelessActs(pCtx, acts, &count)  );

	hasIms = (pRequestHeaders->ifModifiedSince > 0);

	if (hasIms && (count == 0))
	{
		//	nothing new?  say so, but still send back (empty) JSON
		//	so clients won't explode when a simple Ajax call assumes
		//	we always return something
		*status = SG_HTTP_STATUS_NOT_MODIFIED;
	}
	else
	{
		*status = SG_HTTP_STATUS_OK;
	}

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &users)  );
	ctxt.pRepo = *ppRepo;
	ctxt.users = users;
	SG_ERR_CHECK(  SG_varray__foreach(pCtx, acts, _expandWho, &ctxt)  );

	SG_ERR_CHECK(  SG_varray__sort(pCtx, acts, _revWhenSort)  );

	while (count > 25)
	{
		SG_ERR_CHECK(  SG_varray__remove(pCtx, acts, count - 1)  );
		--count;
	}

 	*ppActs = acts;
	acts = NULL;

fail:
	SG_VARRAY_NULLFREE(pCtx, acts);
	SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
	SG_STRING_NULLFREE(pCtx, pstr_where);
	SG_VHASH_NULLFREE(pCtx, users);
    SG_NULLFREE(pCtx, pBaseline);
}

static void _atom_time__format_utc__i64(SG_context* pCtx, SG_int64 iTime,
								  char * pBuf, SG_uint32 lenBuf)
{

	SG_time tm;

	SG_NULLARGCHECK_RETURN( pBuf );
	SG_ARGCHECK_RETURN( (lenBuf >= SG_TIME_FORMAT_LENGTH+1), lenBuf);
	
	// convert milliseconds-since-epoch into individual fields.

	SG_ERR_CHECK_RETURN(  SG_time__decode(pCtx, iTime,&tm)  );

	SG_ERR_CHECK_RETURN( SG_sprintf(pCtx, pBuf,lenBuf,"%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
					 tm.year,tm.month,tm.mday,
					 tm.hour,tm.min,tm.sec, tm.msec) );

}


static void _genId(
	SG_context *pCtx,
	SG_string *idstr,
	const char *hostname,
	const char *descriptor,
	SG_bool haslink,
	const char *linkprefix,
	const char *linkstr,
	const char *who,
	SG_int64 when)
{
	SG_string *ourlink = NULL;
	const char *linksz = NULL;

	SG_time tm;

	SG_ERR_CHECK_RETURN(  SG_time__decode(pCtx, when, &tm)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &ourlink)  );

	if (haslink)
	{
		SG_ERR_CHECK(  SG_string__sprintf(pCtx, ourlink, "%s%s", linkprefix, linkstr)  );
	}
	else
	{
		SG_ERR_CHECK( SG_string__sprintf(pCtx, ourlink,
					 "/%s/%s/%04d%02d%02d%02d%02d%02d%03d",
					 descriptor,
					 who,
					 tm.year,tm.month,tm.mday,
					 tm.hour,tm.min,tm.sec, tm.msec) );
	}

	linksz = SG_string__sz(ourlink);

	SG_ERR_CHECK(  SG_string__sprintf(pCtx, idstr, "tag:%s,%04d-%02d-%02d:%s",
									  hostname, tm.year, tm.month, tm.mday, linksz)  );

fail:
	SG_STRING_NULLFREE(pCtx, ourlink);
}


static void _get_activity_stream_atom(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    _response_handle ** ppResponseHandle)
{
	SG_string *xmlstr = NULL;
	SG_xmlwriter *w = NULL;
	SG_string *linkstr = NULL;
	const char *desc = NULL;
	SG_varray *acts = NULL;
	const char *status = NULL;
	SG_uint32 count = 0;
	SG_uint32 i = 0;
	SG_string *idstr = NULL;
	const char *hostname = NULL;
	SG_int64 maxwhen = 0;
	char timebuf[SG_TIME_FORMAT_LENGTH + 1];
	SG_bool hasIms = SG_FALSE;
	SG_int64	lastmod;
	char	lastmodstr[SG_MAX_RFC850_LENGTH + 1];
	SG_int64	maxmod = 0;

	SG_NULL_PP_CHECK_RETURN(ppRepo);

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &idstr)  );
	SG_ERR_CHECK(  _collect_activity_stream(pCtx, pRequestHeaders, ppRepo, &acts, &status)  );

	SG_ERR_CHECK(  SG_varray__count(pCtx, acts, &count)  );

	for ( i = 0; i < count; ++i )
	{
		SG_vhash *act = NULL;
		SG_int64 when;

		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, acts, i, &act)  );
		SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, act, "when", &when)  );

		if (when > maxwhen)
			maxwhen = when;
	}

	hasIms = (pRequestHeaders->ifModifiedSince > 0);
	maxmod = (maxwhen / 1000) * 1000;

	if (hasIms && (count > 0) && (maxmod <= pRequestHeaders->ifModifiedSince))
	{
		status = SG_HTTP_STATUS_NOT_MODIFIED;
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &xmlstr)  );
		_create_response_handle_for_string(pCtx, SG_HTTP_STATUS_NOT_MODIFIED, SG_contenttype__text, &xmlstr, ppResponseHandle);
	}
	else
	{
		SG_ERR_CHECK_RETURN(  _getHostName(pCtx, &hostname)  );

		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &xmlstr)  );
		SG_ERR_CHECK(  SG_xmlwriter__alloc(pCtx, &w, xmlstr, SG_TRUE)  );
		SG_ERR_CHECK(  SG_xmlwriter__write_start_document(pCtx, w)  );
		SG_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, w, "feed")  );
		SG_ERR_CHECK(  SG_xmlwriter__write_attribute__sz(pCtx, w, "xmlns", "http://www.w3.org/2005/Atom")  );
		
		SG_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, w, "title")  );
		SG_ERR_CHECK(  SG_xmlwriter__write_content__sz(pCtx, w, "activity in ")  );
		SG_ERR_CHECK(  SG_repo__get_descriptor_name(pCtx, *ppRepo, &desc)  );
		SG_ERR_CHECK(  SG_xmlwriter__write_content__sz(pCtx, w, desc)  );
		SG_ERR_CHECK(  SG_xmlwriter__write_end_element(pCtx, w)  );

		SG_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, w, "author")  );
		SG_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, w, "name", "Veracity")  );
		SG_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, w, "email", "veracity@example.com")  );
		SG_ERR_CHECK(  SG_xmlwriter__write_end_element(pCtx, w)  );

		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &linkstr)  );
		SG_ERR_CHECK(  SG_string__sprintf(pCtx, linkstr, "http%s://%s/repos/%s/activity", pRequestHeaders->isSsl ? "s" : "",  pRequestHeaders->pHost, desc)  );
		SG_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, w, "link")  );
		SG_ERR_CHECK(  SG_xmlwriter__write_attribute__sz(pCtx, w, "rel", "self")  );
		SG_ERR_CHECK(  SG_xmlwriter__write_attribute__sz(pCtx, w, "href", SG_string__sz(linkstr))  );
		SG_ERR_CHECK(  SG_xmlwriter__write_attribute__sz(pCtx, w, "type", "application/atom+xml")  );
		SG_ERR_CHECK(  SG_xmlwriter__write_end_element(pCtx, w)  );
		SG_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, w, "id", SG_string__sz(linkstr))  );

		SG_ERR_CHECK(  _atom_time__format_utc__i64(pCtx, maxwhen, timebuf, sizeof(timebuf) / sizeof(char))  );
		SG_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, w, "updated", timebuf)  );

		for ( i = 0; i < count; ++i )
		{
			SG_vhash *act = NULL;
			SG_int64 when;
			const char *who;
			const char *what;
			const char *link = NULL;
			SG_bool haslink = SG_FALSE;

			SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, acts, i, &act)  );

			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, act, "who", &who)  );
			SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, act, "when", &when)  );
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, act, "what", &what)  );
			SG_ERR_CHECK(  SG_vhash__has(pCtx, act, "link", &haslink)  );

			if (haslink)
			{
				SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, act, "link", &link)  );
				SG_ERR_CHECK(  SG_string__sprintf(pCtx, linkstr, "http%s://%s/repos/%s%s", 
					pRequestHeaders->isSsl ? "s" : "",
					pRequestHeaders->pHost,
					desc, link)  );
			}

			SG_ERR_CHECK(  _genId(pCtx, idstr, hostname, desc, haslink, desc, link, who, when)  );

			SG_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, w, "entry")  );

			SG_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, w, "id", SG_string__sz(idstr))  );
			SG_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, w, "author")  );
			SG_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, w, "email", who)  );
			SG_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, w, "name", who)  );
			SG_ERR_CHECK(  SG_xmlwriter__write_end_element(pCtx, w)  );
			SG_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, w, "title", what)  );
			SG_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, w, "summary", what)  );
			
			if (haslink)
			{
				SG_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, w, "link")  );
				SG_ERR_CHECK(  SG_xmlwriter__write_attribute__sz(pCtx, w, "rel", "alternate")  );
				SG_ERR_CHECK(  SG_xmlwriter__write_attribute__sz(pCtx, w, "type", "text/html")  );
				SG_ERR_CHECK(  SG_xmlwriter__write_attribute__sz(pCtx, w, "href", SG_string__sz(linkstr))  );
				SG_ERR_CHECK(  SG_xmlwriter__write_end_element(pCtx, w)  );
			}

			SG_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, w, "content", what)  );

			// todo: updated in 2010-06-24T15:34:23Z
			SG_ERR_CHECK(  _atom_time__format_utc__i64(pCtx, when, timebuf, sizeof(timebuf) / sizeof(char))  );
			SG_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, w, "updated", timebuf)  );

			SG_ERR_CHECK(  SG_xmlwriter__write_end_element(pCtx, w)  );
		}

		SG_ERR_CHECK(  SG_xmlwriter__write_end_element(pCtx, w)  );
		SG_ERR_CHECK(  SG_xmlwriter__write_end_document(pCtx, w)  );

		SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, status, SG_contenttype__xml, &xmlstr, ppResponseHandle)  );

		if (maxwhen > 0)
			lastmod = maxwhen;
		else
			SG_ERR_CHECK(  SG_time__get_milliseconds_since_1970_utc(pCtx, &lastmod)  );

		SG_ERR_CHECK(  SG_time__formatRFC850(pCtx, lastmod, lastmodstr, SG_MAX_RFC850_LENGTH + 1)  );

		SG_ERR_CHECK( _response_handle__add_header(pCtx, *ppResponseHandle, "Last-Modified", lastmodstr)  );
	}

fail:
	SG_VARRAY_NULLFREE(pCtx, acts);
	SG_STRING_NULLFREE(pCtx, idstr);
	SG_REPO_NULLFREE(pCtx, *ppRepo);
	SG_VARRAY_NULLFREE(pCtx, acts);
	SG_STRING_NULLFREE(pCtx, xmlstr);
	SG_STRING_NULLFREE(pCtx, linkstr);
	SG_XMLWRITER_NULLFREE(pCtx, w);
}

static void _get_activity_stream_json(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    _response_handle ** ppResponseHandle)
{
	SG_string *actsJSON = NULL;
	SG_varray *acts = NULL;
	const char *status = NULL;
	SG_uint32 count = 0;
	SG_bool hasIms = SG_FALSE;

	SG_ERR_CHECK(  _collect_activity_stream(pCtx, pRequestHeaders, ppRepo, &acts, &status)  );

	SG_ERR_CHECK(  SG_varray__count(pCtx, acts, &count)  );
	hasIms = (pRequestHeaders->ifModifiedSince > 0);

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &actsJSON)  );
	SG_ERR_CHECK(  SG_varray__to_json(pCtx, acts, actsJSON)  );
	SG_VARRAY_NULLFREE(pCtx, acts);

	SG_ERR_CHECK(  _create_response_handle_for_string(pCtx, status, SG_contenttype__json, &actsJSON, ppResponseHandle)  );


	if ((count > 0) || ! hasIms)
	{
		SG_int64	lastmod;
		char	lastmodstr[SG_MAX_RFC850_LENGTH + 1];

		SG_ERR_CHECK(  SG_time__get_milliseconds_since_1970_utc(pCtx, &lastmod)  );
		SG_ERR_CHECK(  SG_time__formatRFC850(pCtx, lastmod, lastmodstr, SG_MAX_RFC850_LENGTH + 1)  );

		SG_ERR_CHECK( _response_handle__add_header(pCtx, *ppResponseHandle, "Last-Modified", lastmodstr)  );
	}

fail:
	SG_VARRAY_NULLFREE(pCtx, acts);
	SG_STRING_NULLFREE(pCtx, actsJSON);
	SG_REPO_NULLFREE(pCtx, *ppRepo);
}

void _dispatch__activity_stream(
    SG_context * pCtx,
    _request_headers * pRequestHeaders,
    SG_repo ** ppRepo, // On success we've taken ownership and nulled the caller's copy.
    _response_handle ** ppResponseHandle)
{
	if (pRequestHeaders->accept == SG_contenttype__json)
		_get_activity_stream_json(pCtx, pRequestHeaders, ppRepo, ppResponseHandle);
	else 
		_get_activity_stream_atom(pCtx, pRequestHeaders, ppRepo, ppResponseHandle);
}
