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

// note that it's "uniqify", not "eunuchify".  :-)

#define MY_TIMESTAMP__LAST_MODIFIED 1
#define MY_TIMESTAMP__LAST_CREATED  2

static void sg_uniqify__get_most_recent_timestamp(
        SG_context* pCtx,
        SG_vhash* pvh_entry,
        SG_int64* pi
        )
{
    SG_varray* pva_audits = NULL;
    SG_vhash* pvh_one = NULL;
    SG_int64 i = 0;

    SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_entry, "audits", &pva_audits)  );
    SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_audits, 0, &pvh_one)  );
    SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_one, "when", &i)  );
    *pi = i;

fail:
    ;
}

static void x_generate_random_string(
    SG_context* pCtx,
    SG_string* pstr,
    SG_uint32 len,
    const char* psz_genchars,
    SG_uint32 len_genchars
    )
{
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_string__clear(pCtx, pstr)  );
    for (i=0; i<len; i++)
    {
        char c = psz_genchars[SG_random_uint32__2(0, len_genchars - 1)];

        SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pstr, (SG_byte*) &c, 1)  );
    }

fail:
    ;
}

static SG_bool x_is_legit(
    const char* p
    )
{
    // TODO check here for undesirables
    if (0 == strcmp(p, "BAD"))
    {
        return SG_FALSE;
    }

    return SG_TRUE;
}

void sg_zing__gen_userprefix_unique_string(
    SG_context* pCtx,
    SG_zingtx* pztx,
    SG_zingfieldattributes* pzfa,
    SG_string** ppstr
    )
{
    SG_vhash* pvh_user = NULL;
    const char* psz_prefix = NULL;
    SG_string* pstr = NULL;
    SG_rbtree* prb = NULL;
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
    SG_uint32 maxval = 0;
    const char* psz_val = NULL;
    SG_uint32 len_prefix = 0;

    // get the prefix
    SG_ERR_CHECK(  SG_user__lookup_by_userid(pCtx, pztx->pRepo, pztx->buf_who, &pvh_user)  );
    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_user, "prefix", &psz_prefix)  );
    len_prefix = strlen(psz_prefix);

    // TODO this is NOT a good way to do this.  we're grabbing all
    // current values of the field and then finding one that isn't
    // being used.  slow.  doesn't scale.  but correct, so it'll do
    // for now.  but it's horrible.
    SG_ERR_CHECK(  sg_zingtx__get_all_values(pCtx, pztx, pzfa->rectype, pzfa->name, &prb)  );
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb, &b, &psz_val, NULL)  );
    while (b)
    {
        SG_uint32 len_val = strlen(psz_val);
        if (len_val > len_prefix)
        {
            if (0 == memcmp(psz_prefix, psz_val, len_prefix))
            {
                const char* cur = psz_val + len_prefix;
                SG_uint32 val = 0;

                while (*cur)
                {
                    if ('0' <= *cur && *cur <='9' )
                    {
                        val = val * 10 + (*cur) - '0';
                        cur++;
                    }
                    else
                    {
                        val = 0;
                        break;
                    }
                }

                if (val > maxval)
                {
                    maxval = val;
                }
            }
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_val, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr, "%s%05d", 
                psz_prefix, 
                maxval+1
                )  );

    *ppstr = pstr;
    pstr = NULL;

fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_RBTREE_NULLFREE(pCtx, prb);
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_VHASH_NULLFREE(pCtx, pvh_user);
}

void sg_zing__gen_random_unique_string(
    SG_context* pCtx,
    SG_zingtx* pztx,
    SG_zingfieldattributes* pzfa,
    SG_string** ppstr
    )
{
    SG_rbtree* prb = NULL;
    SG_uint32 try_this_len = 0;
    SG_uint32 count_fails_this_len = 0;
    SG_string* pstr = NULL;
    SG_uint32 len_genchars = 0;

    SG_ASSERT(pzfa->v._string.has_minlength);
    SG_ASSERT(pzfa->v._string.has_minlength);
    SG_ASSERT(pzfa->v._string.genchars);

    len_genchars = strlen(pzfa->v._string.genchars);
    // TODO errcheck on len_genchars

	SG_ASSERT(pzfa->v._string.val_minlength <= (SG_int64)SG_UINT32_MAX);
    try_this_len = (SG_uint32)pzfa->v._string.val_minlength;

    SG_ERR_CHECK(  sg_zingtx__get_all_values(pCtx, pztx, pzfa->rectype, pzfa->name, &prb)  );
    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );

    while (1)
    {
        SG_bool b;

        while (1)
        {
            SG_ERR_CHECK(  x_generate_random_string(pCtx, pstr, try_this_len, pzfa->v._string.genchars, len_genchars)  );
            if (x_is_legit(SG_string__sz(pstr)))
            {
                break;
            }
        }

        b = SG_FALSE;
        SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb, SG_string__sz(pstr), &b, NULL)  );
        if (!b)
        {
            *ppstr = pstr;
            pstr = NULL;
            break;
        }

        count_fails_this_len++;
        if (
                (count_fails_this_len >= 10)
                && (try_this_len < pzfa->v._string.val_maxlength)
           )
        {
            try_this_len++;
            count_fails_this_len = 0;
        }
    }


fail:
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_RBTREE_NULLFREE(pCtx, prb);
}

static void sg_zing_uniqify__which__least_impact(
    SG_context* pCtx,
    SG_varray* pva_recids,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
    const SG_audit* pq,
    const char** leaves,
    const char* psz_hid_cs_ancestor,
    const char** pp
    )
{
    SG_uint32 count_recids = 0;
    SG_uint32 i = 0;
    SG_varray* pva_history = NULL;
    struct impact
    {
        const char* psz_recid;
        SG_bool b_me_created;
        SG_uint32 count_history_entries;
        SG_int64 time_last_modified;
        SG_int64 time_created;
        SG_uint32 count_who_modified;
        SG_int32 gen_leaf;
    } winning;
    SG_rbtree* prb_who = NULL;

    memset(&winning, 0, sizeof(winning));

    // TODO use the leaves and ancestor to figure out which
    // leaf each record is associated with.
    SG_UNUSED(leaves);
    SG_UNUSED(psz_hid_cs_ancestor);

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_recids, &count_recids)  );
    for (i=0; i<count_recids; i++)
    {
        struct impact cur;
        SG_uint32 j = 0;

        memset(&cur, 0, sizeof(cur));
        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_recids, i, &cur.psz_recid)  );

        // Grab the record history and write down a few details from it
        SG_ERR_CHECK(  SG_repo__dbndx__query_record_history(pCtx, pRepo, iDagNum, cur.psz_recid, &pva_history)  );
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva_history, &cur.count_history_entries)  );
        SG_ASSERT(cur.count_history_entries >= 1);

        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_who)  );
        for (j=0; j<cur.count_history_entries; j++)
        {
            const char* psz_who = NULL;
            SG_varray* pva_audits = NULL;
            SG_vhash* pvh_one_audit = NULL;
            SG_vhash* pvh_entry = NULL;

            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_history, j, &pvh_entry)  );

            SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_entry, "audits", &pva_audits)  );
            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_audits, 0, &pvh_one_audit)  );

            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_one_audit, "who", &psz_who)  );
            if (j == (cur.count_history_entries-1))
            {
                if (0 == strcmp(psz_who, pq->who_szUserId))
                {
                    cur.b_me_created = SG_TRUE;
                }
                else
                {
                    cur.b_me_created = SG_FALSE;
                }
                SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_one_audit, "when", &cur.time_created)  );
            }
            else if (0 == j)
            {
                SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_one_audit, "when", &cur.time_last_modified)  );
            }
            SG_ERR_CHECK(  SG_rbtree__update(pCtx, prb_who, psz_who)  );
        }
        SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_who, &cur.count_who_modified)  );
        SG_RBTREE_NULLFREE(pCtx, prb_who);

        // TODO now look at the dag.  find how many changesets are children on
        // each branch and how many people modified those changesets.

        if (!winning.psz_recid)
        {
            winning = cur;
        }
        else
        {
            // This is the code that decides between two choices
            if (cur.count_who_modified < winning.count_who_modified)
            {
                winning = cur;
            }
            else if (cur.count_who_modified > winning.count_who_modified)
            {
                // still winning
            }
            else
            {
                if (cur.count_history_entries < winning.count_history_entries)
                {
                    winning = cur;
                }
                else if (cur.count_history_entries > winning.count_history_entries)
                {
                    // still winning
                }
                else
                {
                    if (cur.gen_leaf < winning.gen_leaf)
                    {
                        winning = cur;
                    }
                    else if (cur.gen_leaf > winning.gen_leaf)
                    {
                        // still winning
                    }
                    else
                    {
                        if ((!winning.b_me_created) && cur.b_me_created)
                        {
                            winning = cur;
                        }
                        else if (winning.b_me_created && cur.b_me_created)
                        {
                            if (cur.time_created > winning.time_created)
                            {
                                winning = cur;
                            }
                            else
                            {
                            }
                        }
                        else
                        {
                        }
                    }
                }
            }
        }
        SG_VARRAY_NULLFREE(pCtx, pva_history);
    }

    *pp = winning.psz_recid;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_history);
    SG_RBTREE_NULLFREE(pCtx, prb_who);
}

static void sg_zing_uniqify__which__timestamp(
    SG_context* pCtx,
    SG_uint32 iHow,
    SG_varray* pva_recids,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
    const char** pp
    )
{
    SG_int64 timestamp_winning = 0;
    SG_uint32 count_recids = 0;
    SG_uint32 i = 0;
    SG_varray* pva_history = NULL;
    const char* psz_recid_winning = NULL;

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_recids, &count_recids)  );
    for (i=0; i<count_recids; i++)
    {
        const char* psz_recid = NULL;
        SG_vhash* pvh_entry = NULL;
        SG_uint32 count_entries = 0;
        SG_int64 cur_timestamp = 0;

        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_recids, i, &psz_recid)  );
        SG_ERR_CHECK(  SG_repo__dbndx__query_record_history(pCtx, pRepo, iDagNum, psz_recid, &pva_history)  );
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva_history, &count_entries)  );
        SG_ASSERT(count_entries >= 1);
        // record history is always sorted by generation, with the most recent event
        // at index 0.
        if (MY_TIMESTAMP__LAST_MODIFIED == iHow)
        {
            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_history, 0, &pvh_entry)  );
        }
        else if (MY_TIMESTAMP__LAST_CREATED == iHow)
        {
            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_history, count_entries-1, &pvh_entry)  );
        }
        else
        {
            SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
        }
        SG_ERR_CHECK(  sg_uniqify__get_most_recent_timestamp(pCtx, pvh_entry, &cur_timestamp)  );

        if (
                (!psz_recid_winning)
                || (cur_timestamp > timestamp_winning)
           )
        {
            psz_recid_winning = psz_recid;
            timestamp_winning = cur_timestamp;
        }
        SG_VARRAY_NULLFREE(pCtx, pva_history);
    }

    *pp = psz_recid_winning;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_history);
}

static void sg_zing_uniqify__which(
    SG_context* pCtx,
    SG_vhash* pvh_uniqify,
    SG_varray* pva_recids,
    SG_zingtx* pztx,
    const SG_audit* pq,
    const char** leaves,
    const char* psz_hid_cs_ancestor,
    const char** pp
    )
{
    const char* psz_which = NULL;
    const char* psz_recid_result = NULL;
    SG_repo* pRepo = NULL;
    SG_uint32 iDagNum = 0;

    SG_ERR_CHECK(  SG_zingtx__get_repo(pCtx, pztx, &pRepo)  );
    SG_ERR_CHECK(  SG_zingtx__get_dagnum(pCtx, pztx, &iDagNum)  );

    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_uniqify, SG_ZING_TEMPLATE__WHICH, &psz_which)  );
    if (0 == strcmp("last_modified", psz_which))
    {
        SG_ERR_CHECK(  sg_zing_uniqify__which__timestamp(
                    pCtx,
                    MY_TIMESTAMP__LAST_MODIFIED,
                    pva_recids,
                    pRepo,
                    iDagNum,
                    &psz_recid_result
                    )
                );
    }
    else if (0 == strcmp("last_created", psz_which))
    {
        SG_ERR_CHECK(  sg_zing_uniqify__which__timestamp(
                    pCtx,
                    MY_TIMESTAMP__LAST_CREATED,
                    pva_recids,
                    pRepo,
                    iDagNum,
                    &psz_recid_result
                    )
                );
    }
    else if (0 == strcmp("least_impact", psz_which))
    {
        SG_ERR_CHECK(  sg_zing_uniqify__which__least_impact(
                    pCtx,
                    pva_recids,
                    pRepo,
                    iDagNum,
                    pq,
                    leaves,
                    psz_hid_cs_ancestor,
                    &psz_recid_result
                    )
                );
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
    }

    *pp = psz_recid_result;

fail:
    ;
}

void sg_zing_uniqify_int__do(
    SG_context* pCtx,
    SG_vhash* pvh_error,
    SG_vhash* pvh_uniqify,
    SG_zingtx* pztx,
    SG_zingfieldattributes* pzfa,
    const SG_audit* pq,
    const char** leaves,
    const char* psz_hid_cs_ancestor,
    SG_varray* pva_log
    )
{
    SG_varray* pva_recids = NULL;
    SG_zingrecord* pzr = NULL;
    const char* psz_op = NULL;
    SG_int64 val_to_add = 0;
    SG_int64 curval = 0;

    SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_error, SG_ZING_CONSTRAINT_VIOLATION__IDS, &pva_recids)  );

    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_uniqify, SG_ZING_TEMPLATE__OP, &psz_op)  );

    if (0 == strcmp(psz_op, "add"))
    {
        const char* psz_recid = NULL;
        SG_int_to_string_buffer sz_old;
        SG_int_to_string_buffer sz_new;

        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_uniqify, SG_ZING_TEMPLATE__ADDEND, &val_to_add)  );
        SG_ERR_CHECK(  sg_zing_uniqify__which(pCtx, pvh_uniqify, pva_recids, pztx, pq, leaves, psz_hid_cs_ancestor, &psz_recid)  );
        SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid, &pzr)  );
        SG_ERR_CHECK(  SG_zingrecord__get_field__int(pCtx, pzr, pzfa, &curval)  );
        SG_ERR_CHECK(  SG_zingrecord__set_field__int(pCtx, pzr, pzfa, curval + val_to_add) );
        SG_int64_to_sz(curval, sz_old);
        SG_int64_to_sz(curval + val_to_add, sz_new);
        SG_ERR_CHECK(  sg_zingmerge__add_log_entry(pCtx, pva_log,
                    "recid", psz_recid,
                    "field_name", pzfa->name,
                    "old_value", sz_old,
                    "new_value", sz_new,
                    "reason", "unique",
                    NULL) );
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
    }

fail:
    return;
}

static SG_bool my_isdigit(char c)
{
    return (c >= '0') && (c <= '9');
}

void sg_zing_uniqify_string__do(
    SG_context* pCtx,
    SG_vhash* pvh_error,
    SG_vhash* pvh_uniqify,
    SG_zingtx* pztx,
    SG_zingfieldattributes* pzfa,
    const SG_audit* pq,
    const char** leaves,
    const char* psz_hid_cs_ancestor,
    SG_varray* pva_log
    )
{
    SG_varray* pva_recids = NULL;
    SG_zingrecord* pzr = NULL;
    const char* psz_op = NULL;
    SG_string* pstr = NULL;
    const char* psz_recid = NULL;
    const char* psz_val = NULL;

    SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_error, SG_ZING_CONSTRAINT_VIOLATION__IDS, &pva_recids)  );

    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_uniqify, SG_ZING_TEMPLATE__OP, &psz_op)  );

    SG_ERR_CHECK(  sg_zing_uniqify__which(pCtx, pvh_uniqify, pva_recids, pztx, pq, leaves, psz_hid_cs_ancestor, &psz_recid)  );
    SG_ERR_CHECK(  SG_zingtx__get_record(pCtx, pztx, psz_recid, &pzr)  );
    SG_ERR_CHECK(  SG_zingrecord__get_field__string(pCtx, pzr, pzfa, &psz_val)  );

    if (0 == strcmp(psz_op, "inc_digits_end"))
    {
        const char* p = NULL;
        SG_uint32 len_orig = 0;
        SG_uint32 len_digits = 0;
        SG_uint32 len_base = 0;

        SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
        len_orig = strlen(psz_val);
        p = psz_val + len_orig;
        while (
                (p > psz_val)
                && my_isdigit(*(p-1))
                )
        {
            p--;
        }
        len_digits = strlen(p);
        len_base = len_orig - len_digits;
        if (len_digits > 0)
        {
            SG_int64 ival = 0;
            SG_int_to_string_buffer sz_i;
            SG_uint32 new_len_digits = 0;

            SG_ERR_CHECK(  SG_int64__parse(pCtx, &ival, p)  );
            ival++;
            SG_int64_to_sz(ival, sz_i);
            new_len_digits = strlen(sz_i);

            if (len_base)
            {
                SG_ERR_CHECK(  SG_string__set__buf_len(pCtx, pstr, (void*) psz_val, len_base)  );
            }
            else
            {
                SG_ERR_CHECK(  SG_string__set__sz(pCtx, pstr, "")  );
            }
            if (new_len_digits < len_digits)
            {
                // need to append some zeroes here
                SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pstr, (void*) "00000000000000000000", (len_digits - new_len_digits))  );
            }
            SG_ERR_CHECK(  SG_string__append__sz(pCtx, pstr, sz_i)  );
        }
        else
        {
            SG_ERR_CHECK(  SG_string__set__sz(pCtx, pstr, psz_val)  );
            SG_ERR_CHECK(  SG_string__append__sz(pCtx, pstr, "_0")  );
        }
        SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, pzr, pzfa, SG_string__sz(pstr)) );
        SG_ERR_CHECK(  sg_zingmerge__add_log_entry(pCtx, pva_log,
                    "recid", psz_recid,
                    "field_name", pzfa->name,
                    "old_value", psz_val,
                    "new_value", SG_string__sz(pstr),
                    "reason", "unique",
                    NULL) );
        SG_STRING_NULLFREE(pCtx, pstr);
    }
    else if (0 == strcmp(psz_op, "gen_random_unique"))
    {
        SG_ERR_CHECK(  sg_zing__gen_random_unique_string(pCtx, pztx, pzfa, &pstr)  );
        SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, pzr, pzfa, SG_string__sz(pstr)) );
        SG_ERR_CHECK(  sg_zingmerge__add_log_entry(pCtx, pva_log,
                    "recid", psz_recid,
                    "field_name", pzfa->name,
                    "old_value", psz_val,
                    "new_value", SG_string__sz(pstr),
                    "reason", "unique",
                    NULL) );
        SG_STRING_NULLFREE(pCtx, pstr);
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
    }

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
}


