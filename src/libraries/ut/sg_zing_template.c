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

struct SG_zingtemplate
{
    SG_uint32 version;
    SG_vhash* pvh;
    SG_rbtree* prb_field_attributes;
};

void SG_zing__is_reserved_field_name(
        SG_context* pCtx,
        const char* psz_name,
        SG_bool* pb
        )
{
  SG_UNUSED(pCtx);

    if (
               (0 == strcmp(psz_name, SG_ZING_FIELD__RECID))
            || (0 == strcmp(psz_name, SG_ZING_FIELD__RECHID))
            || (0 == strcmp(psz_name, SG_ZING_FIELD__RECTYPE))
            || (0 == strcmp(psz_name, SG_ZING_FIELD__HISTORY))
       )
    {
        *pb = SG_TRUE;
    }
    else
    {
        *pb = SG_FALSE;
    }
}

void SG_zingfieldattributes__free(
        SG_context* pCtx,
        SG_zingfieldattributes* pzfa
        )
{
    if (pzfa)
    {
        switch (pzfa->type)
        {
            case SG_ZING_TYPE__INT:
                {
                    SG_VECTOR_I64_NULLFREE(pCtx, pzfa->v._int.allowed);
                    SG_VECTOR_I64_NULLFREE(pCtx, pzfa->v._int.prohibited);
                    break;
                }
            case SG_ZING_TYPE__STRING:
                {
                    SG_VHASH_NULLFREE(pCtx, pzfa->v._string.allowed);
                    SG_STRINGARRAY_NULLFREE(pCtx, pzfa->v._string.prohibited);
                    break;
                }
        }
        SG_NULLFREE(pCtx, pzfa);
    }
}

void SG_zingtemplate__free(
        SG_context* pCtx,
        SG_zingtemplate* pThis
        )
{
    if (pThis)
    {
        SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pThis->prb_field_attributes, (SG_free_callback*) SG_zingfieldattributes__free);
        SG_VHASH_NULLFREE(pCtx, pThis->pvh);
        SG_NULLFREE(pCtx, pThis);
    }
}

static void sg_zingtemplate__allowed_members(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        ...
        )
{
    va_list ap;
    SG_uint32 count = 0;
    SG_uint32 i = 0;

    // So this n^2-ish loop doesn't look like the fastest way
    // to do this.  But the lists should always be short,
    // right?  The alternative is to build a vhash or
    // rbtree, but then we're involving the memory
    // allocator.  For short lists, the overhead
    // probably isn't worth it.

    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );
    for (i=0; i<count; i++)
    {
        const SG_variant* pv = NULL;
        const char* psz_candidate = NULL;

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh, i, &psz_candidate, &pv)  );
        va_start(ap, pvh);
        do
        {
            const char* psz_key = va_arg(ap, const char*);

            if (!psz_key)
            {
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "%s.%s:  '%s' is not allowed here", psz_where, psz_cur, psz_candidate)
                        );
            }

            if (*psz_key) // skip empty string
            {
                if (0 == strcmp(psz_candidate, psz_key))
                {
                    break;
                }
            }

        } while (1);
    }

    return;

fail:
    return;
}

static void sg_zingtemplate__required(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        SG_uint16 type
        )
{
    SG_bool b = SG_FALSE;
    SG_uint16 i2;

    b = SG_FALSE;
    SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, pvh, psz_key, &b)  );
    if (!b)
    {
		SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  Required element '%s' is missing", psz_where, psz_cur, psz_key)
                );
    }
    SG_ERR_CHECK_RETURN(  SG_vhash__typeof(pCtx, pvh, psz_key, &i2)  );
    if (type != i2)
    {
		SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  Element '%s' must be of type %s", psz_where, psz_cur, psz_key, sg_variant__type_name(type))
                );
    }

    // fall thru
fail:
    return;
}

static void sg_zingtemplate__optional(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        SG_uint16 type,
        SG_bool* pb
        )
{
    SG_bool b = SG_FALSE;
    SG_uint16 i2;

    b = SG_FALSE;
    SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, pvh, psz_key, &b)  );
    if (!b)
    {
        *pb = SG_FALSE;
        return;
    }
    SG_ERR_CHECK_RETURN(  SG_vhash__typeof(pCtx, pvh, psz_key, &i2)  );
    if (!(type & i2))
    {
		SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  Element '%s' must be of type %s", psz_where, psz_cur, psz_key, sg_variant__type_name(type))
                );
    }
    *pb = SG_TRUE;

    // fall thru
fail:
    return;
}

static void sg_zingtemplate__required__int64(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        SG_int64* pResult
        )
{
    SG_NULLARGCHECK_RETURN(pResult);
    SG_ERR_CHECK_RETURN(  sg_zingtemplate__required(pCtx, psz_where, psz_cur, pvh, psz_key, SG_VARIANT_TYPE_INT64)  );
    SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, pvh, psz_key, pResult)  );
}

static void sg_zingtemplate__required__bool(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        SG_bool* pResult
        )
{
    SG_NULLARGCHECK_RETURN(pResult);
    SG_ERR_CHECK_RETURN(  sg_zingtemplate__required(pCtx, psz_where, psz_cur, pvh, psz_key, SG_VARIANT_TYPE_BOOL)  );
    SG_ERR_CHECK_RETURN(  SG_vhash__get__bool(pCtx, pvh, psz_key, pResult)  );
}

static void sg_zingtemplate__required__sz(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        const char** pResult
        )
{
    SG_NULLARGCHECK_RETURN(pResult);
    SG_ERR_CHECK_RETURN(  sg_zingtemplate__required(pCtx, psz_where, psz_cur, pvh, psz_key, SG_VARIANT_TYPE_SZ)  );
    SG_ERR_CHECK_RETURN(  SG_vhash__get__sz(pCtx, pvh, psz_key, pResult)  );
}

static void sg_zingtemplate__required__vhash(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        SG_vhash** pResult
        )
{
    SG_NULLARGCHECK_RETURN(pResult);
    SG_ERR_CHECK_RETURN(  sg_zingtemplate__required(pCtx, psz_where, psz_cur, pvh, psz_key, SG_VARIANT_TYPE_VHASH)  );
    SG_ERR_CHECK_RETURN(  SG_vhash__get__vhash(pCtx, pvh, psz_key, pResult)  );
}

static void sg_zingtemplate__optional__vhash(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        SG_vhash** pResult
        )
{
    SG_bool b = SG_FALSE;

    SG_NULLARGCHECK_RETURN(pResult);
    SG_ERR_CHECK_RETURN(  sg_zingtemplate__optional(pCtx, psz_where, psz_cur, pvh, psz_key, SG_VARIANT_TYPE_VHASH, &b)  );
    if (!b)
    {
        *pResult = NULL;
        return;
    }
    SG_ERR_CHECK_RETURN(  SG_vhash__get__vhash(pCtx, pvh, psz_key, pResult)  );
}

static void sg_zingtemplate__optional__varray(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        SG_varray** pResult
        )
{
    SG_bool b = SG_FALSE;

    SG_NULLARGCHECK_RETURN(pResult);
    SG_ERR_CHECK_RETURN(  sg_zingtemplate__optional(pCtx, psz_where, psz_cur, pvh, psz_key, SG_VARIANT_TYPE_VARRAY, &b)  );
    if (!b)
    {
        *pResult = NULL;
        return;
    }
    SG_ERR_CHECK_RETURN(  SG_vhash__get__varray(pCtx, pvh, psz_key, pResult)  );
}

static void sg_zingtemplate__optional__bool(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        SG_bool* pb_field_present,
        SG_bool* pResult
        )
{
    SG_bool b = SG_FALSE;

    SG_NULLARGCHECK_RETURN(pResult);
    SG_ERR_CHECK_RETURN(  sg_zingtemplate__optional(pCtx, psz_where, psz_cur, pvh, psz_key, SG_VARIANT_TYPE_BOOL, &b)  );
    if (!b)
    {
        *pb_field_present = SG_FALSE;
        return;
    }
    *pb_field_present = SG_TRUE;
    SG_ERR_CHECK_RETURN(  SG_vhash__get__bool(pCtx, pvh, psz_key, pResult)  );
}

static void sg_zingtemplate__optional__int64(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        SG_bool* pb_field_present,
        SG_int64* pResult
        )
{
    SG_bool b = SG_FALSE;

    SG_NULLARGCHECK_RETURN(pResult);
    SG_ERR_CHECK_RETURN(  sg_zingtemplate__optional(pCtx, psz_where, psz_cur, pvh, psz_key, SG_VARIANT_TYPE_INT64, &b)  );
    if (!b)
    {
        *pb_field_present = SG_FALSE;
        return;
    }
    *pb_field_present = SG_TRUE;
    SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, pvh, psz_key, pResult)  );
}

static void sg_zingtemplate__optional__int64_or_double(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        SG_bool* pb_field_present,
        SG_int64* pResult
        )
{
    SG_bool b = SG_FALSE;

    SG_NULLARGCHECK_RETURN(pResult);
    SG_ERR_CHECK_RETURN(  sg_zingtemplate__optional(pCtx, psz_where, psz_cur, pvh, psz_key, SG_VARIANT_TYPE_INT64 | SG_VARIANT_TYPE_DOUBLE, &b)  );
    if (!b)
    {
        *pb_field_present = SG_FALSE;
        return;
    }
    *pb_field_present = SG_TRUE;
    SG_ERR_CHECK_RETURN(  SG_vhash__get__int64_or_double(pCtx, pvh, psz_key, pResult)  );
}

static void sg_zingtemplate__optional__sz(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        const char** pResult
        )
{
    SG_bool b = SG_FALSE;

    SG_NULLARGCHECK_RETURN(pResult);
    SG_ERR_CHECK_RETURN(  sg_zingtemplate__optional(pCtx, psz_where, psz_cur, pvh, psz_key, SG_VARIANT_TYPE_SZ, &b)  );
    if (!b)
    {
        *pResult = NULL;
        return;
    }
    SG_ERR_CHECK_RETURN(  SG_vhash__get__sz(pCtx, pvh, psz_key, pResult)  );
}

static void sg_zingtemplate__required__varray(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_key,
        SG_varray** pResult
        )
{
    SG_NULLARGCHECK_RETURN(pResult);
    SG_ERR_CHECK_RETURN(  sg_zingtemplate__required(pCtx, psz_where, psz_cur, pvh, psz_key, SG_VARIANT_TYPE_VARRAY)  );
    SG_ERR_CHECK_RETURN(  SG_vhash__get__varray(pCtx, pvh, psz_key, pResult)  );
}

static void sg_zingtemplate__vhash__check_type_of_members(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        SG_uint16 type,
        SG_uint32* p_count
        )
{
    SG_uint32 count = 0;
    SG_uint32 i;

    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );
    for (i=0; i<count; i++)
    {
        const SG_variant* pv = NULL;
        const char* psz_key = NULL;

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh, i, &psz_key, &pv)  );
        if (type != pv->type)
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  Element '%s' must be of type %s", psz_where, psz_cur, psz_key, sg_variant__type_name(type))
                    );
        }
    }

    *p_count = count;

    return;

fail:
    return;
}

static void sg_zingtemplate__varray__check_type_of_members(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_varray* pva,
        SG_uint16 type,
        SG_uint32* p_count
        )
{
    SG_uint32 count = 0;
    SG_uint32 i;

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    for (i=0; i<count; i++)
    {
        const SG_variant* pv = NULL;

        SG_ERR_CHECK(  SG_varray__get__variant(pCtx, pva, i, &pv)  );
        if (type != pv->type)
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  Elements must be of type %s", psz_where, psz_cur, sg_variant__type_name(type))
                    );
        }
    }

    *p_count = count;

    return;

fail:
    return;
}

static void SG_zingtemplate__validate_field_form(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        const char* psz_datatype,
        SG_vhash* pvh_constraints,
        SG_vhash* pvh
        )
{
    SG_string* pstr_where = NULL;
    const char* psz_label = NULL;
    const char* psz_help = NULL;
    const char* psz_tooltip = NULL;
    SG_bool b_has_readonly = SG_FALSE;
    SG_bool b_readonly = SG_FALSE;
    SG_bool b_has_visible = SG_FALSE;
    SG_bool b_visible = SG_FALSE;

    SG_UNUSED(psz_datatype);
    SG_UNUSED(pvh_constraints);

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s", psz_where, psz_cur)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__sz(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__LABEL, &psz_label)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__sz(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__HELP, &psz_help)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__sz(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__TOOLTIP, &psz_tooltip)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__READONLY, &b_has_readonly, &b_readonly)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__VISIBLE, &b_has_visible, &b_visible)  );

    // TODO suggested.  for ints and strings.

    // TODO lots of other things can go here.  width of the text field.  height of the text field.  etc.

    // fall thru

fail:
    SG_STRING_NULLFREE(pCtx, pstr_where);
}

static void SG_zingtemplate__validate_field_merge(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        const char* psz_datatype,
        SG_vhash* pvh_constraints,
        SG_vhash* pvh
        )
{
    SG_varray* pva_auto = NULL;
    SG_string* pstr_where = NULL;
    SG_bool b_has_allowed = SG_FALSE;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s", psz_where, psz_cur)  );

    if (pvh_constraints)
    {
        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__ALLOWED, &b_has_allowed)  );
    }

    SG_ERR_CHECK(  sg_zingtemplate__optional__varray(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__AUTO, &pva_auto)  );

    if (pva_auto)
    {
        SG_uint32 count_auto = 0;
        SG_uint32 i = 0;

        SG_ERR_CHECK(  sg_zingtemplate__varray__check_type_of_members(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__AUTO, pva_auto, SG_VARIANT_TYPE_VHASH, &count_auto)  );

        if (0 == count_auto)
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  auto is empty", psz_where, psz_cur)
                    );
        }

        for (i=0; i<count_auto; i++)
        {
            SG_vhash* pvh_automerge = NULL;
            const char* psz_op = NULL;
            SG_bool b_ok = SG_FALSE;

            SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_auto, i, &pvh_automerge)  );

            SG_ERR_CHECK(  sg_zingtemplate__required__sz(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__AUTO, pvh_automerge, SG_ZING_TEMPLATE__OP, &psz_op)  );

            // TODO put other ops here.  such as ops which choose a side
            // based on the authority of the users.

            if (
                    (0 == strcmp(SG_ZING_MERGE__most_recent, psz_op))
                    || (0 == strcmp(SG_ZING_MERGE__least_recent, psz_op))
               )
            {
                b_ok = SG_TRUE;
            }

            if (!b_ok && (0 == strcmp(psz_datatype, "int")))
            {
                if (
                        (0 == strcmp(SG_ZING_MERGE__max, psz_op))
                        || (0 == strcmp(SG_ZING_MERGE__min, psz_op))
                        || (0 == strcmp(SG_ZING_MERGE__average, psz_op))
                        || (0 == strcmp(SG_ZING_MERGE__sum, psz_op))
                   )
                {
                    b_ok = SG_TRUE;
                }
            }

            if (!b_ok && (0 == strcmp(psz_datatype, "string")))
            {
                if (
                        (0 == strcmp(SG_ZING_MERGE__longest, psz_op))
                        || (0 == strcmp(SG_ZING_MERGE__shortest, psz_op))
                        || (0 == strcmp(SG_ZING_MERGE__concat, psz_op))
                   )
                {
                    b_ok = SG_TRUE;
                }
            }

            if (
                    !b_ok
                    && b_has_allowed
                    && (
                        (0 == strcmp(SG_ZING_MERGE__allowed_last, psz_op))
                        || (0 == strcmp(SG_ZING_MERGE__allowed_first, psz_op))
                    )
               )
            {
                b_ok = SG_TRUE;
            }

            if (!b_ok)
            {
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "%s.%s:  invalid automerge op: %s", psz_where, psz_cur, psz_op)
                        );
            }
        }
    }

    if (0 == strcmp(psz_datatype, "string"))
    {
        SG_vhash* pvh_uniqify = NULL;

        SG_ERR_CHECK(  sg_zingtemplate__optional__vhash(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__UNIQIFY, &pvh_uniqify)  );
        if (pvh_uniqify)
        {
            const char* psz_op = NULL;
            const char* psz_which = NULL;

            SG_ERR_CHECK(  sg_zingtemplate__required__sz(pCtx, psz_where, psz_cur, pvh_uniqify, SG_ZING_TEMPLATE__OP, &psz_op)  );
            SG_ERR_CHECK(  sg_zingtemplate__required__sz(pCtx, psz_where, psz_cur, pvh_uniqify, SG_ZING_TEMPLATE__WHICH, &psz_which)  );
            // TODO validate these
        }
    }
    else if (0 == strcmp(psz_datatype, "int"))
    {
        SG_vhash* pvh_uniqify = NULL;

        SG_ERR_CHECK(  sg_zingtemplate__optional__vhash(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__UNIQIFY, &pvh_uniqify)  );
        if (pvh_uniqify)
        {
            const char* psz_op = NULL;
            const char* psz_which = NULL;

            SG_ERR_CHECK(  sg_zingtemplate__required__sz(pCtx, psz_where, psz_cur, pvh_uniqify, SG_ZING_TEMPLATE__OP, &psz_op)  );
            SG_ERR_CHECK(  sg_zingtemplate__required__sz(pCtx, psz_where, psz_cur, pvh_uniqify, SG_ZING_TEMPLATE__WHICH, &psz_which)  );
            // TODO validate these
        }
    }

    // fall thru

fail:
    SG_STRING_NULLFREE(pCtx, pstr_where);
}

static void SG_zingtemplate__validate__calculated__bool(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh
        )
{
  SG_UNUSED(pCtx);
  SG_UNUSED(psz_where);
  SG_UNUSED(psz_cur);
  SG_UNUSED(pvh);
}

static void SG_zingtemplate__validate__calculated__int(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh
        )
{
    const char* psz_action_type = NULL;
    SG_string* pstr_where = NULL;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s", psz_where, psz_cur)  );

    SG_ERR_CHECK(  sg_zingtemplate__required__sz(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__TYPE, &psz_action_type)  );

    if (0 == strcmp(psz_action_type, "builtin"))
    {
        const char* psz_builtin_function = NULL;

        SG_ERR_CHECK(  sg_zingtemplate__required__sz(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__FUNCTION, &psz_builtin_function)  );

        if (
                (0 == strcmp(psz_builtin_function, "sum"))
                || (0 == strcmp(psz_builtin_function, "count"))
                || (0 == strcmp(psz_builtin_function, "min"))
                || (0 == strcmp(psz_builtin_function, "max"))
                || (0 == strcmp(psz_builtin_function, "average"))
           )
        {
            const char* psz_field_from = NULL;
            const char* psz_depends_on = NULL;

            SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, psz_where, psz_cur, pvh,
                        SG_ZING_TEMPLATE__TYPE,
                        SG_ZING_TEMPLATE__FUNCTION,
                        SG_ZING_TEMPLATE__DEPENDS_ON,
                        SG_ZING_TEMPLATE__FIELD_FROM,
                        NULL)  );
            SG_ERR_CHECK(  sg_zingtemplate__required__sz(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__FIELD_FROM, &psz_field_from)  );
            SG_ERR_CHECK(  sg_zingtemplate__required__sz(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__DEPENDS_ON, &psz_depends_on)  );

            // TODO validate these
        }
        else
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  %s is not a supported builtin function name", SG_string__sz(pstr_where), psz_cur, psz_builtin_function)
                    );
        }
    }
    else
    {
        SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  %s is not a supported action type", SG_string__sz(pstr_where), psz_cur, psz_action_type)
                );
    }

    // fall thru

fail:
    SG_STRING_NULLFREE(pCtx, pstr_where);
}

static void SG_zingtemplate__validate__calculated__datetime(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh
        )
{
  SG_UNUSED(pCtx);
  SG_UNUSED(psz_where);
  SG_UNUSED(psz_cur);
  SG_UNUSED(pvh);

}

static void SG_zingtemplate__validate__calculated__userid(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh
        )
{
  SG_UNUSED(pCtx);
  SG_UNUSED(psz_where);
  SG_UNUSED(psz_cur);
  SG_UNUSED(pvh);
}

static void SG_zingtemplate__validate__calculated__string(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh
        )
{
  SG_UNUSED(pCtx);
  SG_UNUSED(psz_where);
  SG_UNUSED(psz_cur);
  SG_UNUSED(pvh);
}

static void SG_zingtemplate__validate__constraints__bool(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh_constraints
        )
{
    SG_bool b_field_present = SG_FALSE;
    SG_bool b_required = SG_FALSE;

    SG_bool b_defaultvalue;

    SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, psz_where, psz_cur, pvh_constraints,
                SG_ZING_TEMPLATE__REQUIRED,
                SG_ZING_TEMPLATE__DEFAULTVALUE,
                NULL)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__REQUIRED, &b_field_present, &b_required)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &b_field_present, &b_defaultvalue)  );

    // fall thru

fail:
    return;
}

static void SG_zingtemplate__validate__constraints__attachment(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh_constraints
        )
{
    SG_bool b_field_present = SG_FALSE;
    SG_bool b_required = SG_FALSE;
    SG_int64 i_maxlength = 0;

    SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, psz_where, psz_cur, pvh_constraints,
                SG_ZING_TEMPLATE__MAXLENGTH,
                SG_ZING_TEMPLATE__REQUIRED,
                NULL)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__REQUIRED, &b_field_present, &b_required)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__int64(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__MAXLENGTH, &b_field_present, &i_maxlength)  );

    // fall thru

fail:
    return;
}

static void SG_zingtemplate__validate__constraints__userid(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh_constraints
        )
{
    SG_bool b_has_required = SG_FALSE;
    SG_bool b_required = SG_FALSE;

    const char* psz_defaultvalue = NULL;

    SG_string* pstr_where = NULL;

    SG_rbtree* prb = NULL;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s", psz_where, psz_cur)  );

    // TODO allow constraints for group membership and role

    SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, psz_where, psz_cur, pvh_constraints,
                SG_ZING_TEMPLATE__REQUIRED,
                SG_ZING_TEMPLATE__DEFAULTVALUE,
                NULL)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__REQUIRED, &b_has_required, &b_required)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__sz(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &psz_defaultvalue)  );

    if (psz_defaultvalue)
    {
        // TODO this should be a defaultfunc
        if (0 == strcmp(psz_defaultvalue, "whoami"))
        {
        }
        else
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  invalid defaultvalue for userid field: %s", psz_where, psz_cur, psz_defaultvalue)
                    );
        }
    }

    // fall thru

fail:
    SG_STRING_NULLFREE(pCtx, pstr_where);
    SG_RBTREE_NULLFREE(pCtx, prb);
}

static void SG_zingtemplate__validate__constraints__string(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh_constraints
        )
{
    SG_uint32 i = 0;

    SG_bool b_has_required = SG_FALSE;
    SG_bool b_required = SG_FALSE;
    SG_bool b_has_unique = SG_FALSE;
    SG_bool b_unique = SG_FALSE;
    SG_bool b_has_sort_by_allowed = SG_FALSE;
    SG_bool b_sort_by_allowed = SG_FALSE;

    const char* psz_defaultvalue = NULL;
    const char* psz_defaultfunc = NULL;
    const char* psz_genchars = NULL;

    SG_bool b_has_minlength = SG_FALSE;
    SG_int64 i_minlength = 0;

    SG_bool b_has_maxlength = SG_FALSE;
    SG_int64 i_maxlength = 0;

    SG_varray* pva_allowed = NULL;
    SG_uint32 count_allowed = 0;
    SG_varray* pva_prohibited = NULL;
    SG_uint32 count_prohibited = 0;
    SG_string* pstr_where = NULL;

    SG_rbtree* prb = NULL;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s", psz_where, psz_cur)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__varray(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__ALLOWED, &pva_allowed)  );

    SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, psz_where, psz_cur, pvh_constraints,
                SG_ZING_TEMPLATE__REQUIRED,
                SG_ZING_TEMPLATE__MINLENGTH,
                SG_ZING_TEMPLATE__MAXLENGTH,
                SG_ZING_TEMPLATE__GENCHARS,
                SG_ZING_TEMPLATE__DEFAULTFUNC,
                SG_ZING_TEMPLATE__DEFAULTVALUE,
                SG_ZING_TEMPLATE__ALLOWED,
                pva_allowed ? SG_ZING_TEMPLATE__SORT_BY_ALLOWED : "",
                SG_ZING_TEMPLATE__PROHIBITED,
                SG_ZING_TEMPLATE__UNIQUE,
                NULL)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__SORT_BY_ALLOWED, &b_has_sort_by_allowed, &b_sort_by_allowed)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__UNIQUE, &b_has_unique, &b_unique)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__REQUIRED, &b_has_required, &b_required)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__sz(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &psz_defaultvalue)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__sz(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTFUNC, &psz_defaultfunc)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__sz(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__GENCHARS, &psz_genchars)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__int64(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__MINLENGTH, &b_has_minlength, &i_minlength)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__int64(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__MAXLENGTH, &b_has_maxlength, &i_maxlength)  );

    if (psz_defaultvalue && psz_defaultfunc)
    {
        SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  can't have both defaultvalue and defaultfunc", psz_where, psz_cur)
                );
    }

    if (psz_defaultfunc)
    {
        if (0 == strcmp(psz_defaultfunc, "gen_random_unique"))
        {
            if (!psz_genchars)
            {
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "%s.%s:  gen_random_unique requires genchars", psz_where, psz_cur)
                        );
            }
            if (!b_has_minlength)
            {
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "%s.%s:  gen_random_unique requires minlength", psz_where, psz_cur)
                        );
            }
            if (!b_has_maxlength)
            {
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "%s.%s:  gen_random_unique requires maxlength", psz_where, psz_cur)
                        );
            }
            // TODO legalchars
        }
        else if (0 == strcmp(psz_defaultfunc, "gen_userprefix_unique"))
        {
            // TODO more checks here?
        }
        else
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  illegal defaultfunc", psz_where, psz_cur)
                    );
        }
    }
    else
    {
        if (psz_genchars)
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  genchars requires gen_random_unique", psz_where, psz_cur)
                    );
        }
    }

    if (b_has_maxlength && b_has_minlength && (i_maxlength < i_minlength))
    {
        SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  maxlength < minlength", psz_where, psz_cur)
                );
    }

    if (b_has_maxlength && psz_defaultvalue && ((SG_int64)strlen(psz_defaultvalue) > i_maxlength))
    {
        SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  defaultvalue longer than maxlength", psz_where, psz_cur)
                );
    }

    if (b_has_minlength && psz_defaultvalue && ((SG_int64)strlen(psz_defaultvalue) < i_minlength))
    {
        SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  defaultvalue shorter than minlength", psz_where, psz_cur)
                );
    }

    if (pva_allowed)
    {
        SG_ERR_CHECK(  sg_zingtemplate__varray__check_type_of_members(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__ALLOWED, pva_allowed, SG_VARIANT_TYPE_SZ, &count_allowed)  );

        if (0 == count_allowed)
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  allowed values is empty", psz_where, psz_cur)
                    );
        }

        if (psz_defaultvalue)
        {
            SG_bool b_defaultvalue_is_allowed = SG_FALSE;
            for (i=0; i<count_allowed; i++)
            {
                const char* psz_va = NULL;

                SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_allowed, i, &psz_va)  );
                if (0 == strcmp(psz_defaultvalue, psz_va))
                {
                    b_defaultvalue_is_allowed = SG_TRUE;
                    break;
                }
            }
            if (!b_defaultvalue_is_allowed)
            {
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "%s.%s:  defaultvalue which is not in list of values allowed", psz_where, psz_cur)
                        );
            }
        }

        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );

        for (i=0; i<count_allowed; i++)
        {
            const char* psz_va = NULL;
            SG_bool b = SG_FALSE;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_allowed, i, &psz_va)  );
            if (b_has_minlength)
            {
                if ((SG_int64)strlen(psz_va) < i_minlength)
                {
                    SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                            (pCtx, "%s.%s: allowed value is shorter than minlength", psz_where, psz_cur)
                            );
                }
            }
            if (b_has_maxlength)
            {
                if ((SG_int64)strlen(psz_va) > i_maxlength)
                {
                    SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                            (pCtx, "%s.%s:  allowed value is longer than maxlength", psz_where, psz_cur)
                            );
                }
            }

            b = SG_FALSE;
            SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb, psz_va, &b, NULL)  );
            if (b)
            {
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "%s.%s:  duplicate value in allowed[]", psz_where, psz_cur)
                        );
            }

            SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, psz_va)  );
        }
        SG_RBTREE_NULLFREE(pCtx, prb);
    }

    SG_ERR_CHECK(  sg_zingtemplate__optional__varray(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__PROHIBITED, &pva_prohibited)  );
    if (pva_prohibited)
    {
        SG_ERR_CHECK(  sg_zingtemplate__varray__check_type_of_members(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__PROHIBITED, pva_prohibited, SG_VARIANT_TYPE_SZ, &count_prohibited)  );

        if (count_allowed && count_prohibited)
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  has both prohibited and allowed", psz_where, psz_cur)
                    );
        }

        if (psz_defaultvalue)
        {
            for (i=0; i<count_prohibited; i++)
            {
                const char* psz_va = NULL;

                SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_prohibited, i, &psz_va)  );
                if (0 == strcmp(psz_defaultvalue, psz_va))
                {
                    SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                            (pCtx, "%s.%s:  has defaultvalue which is in list of values prohibited", psz_where, psz_cur)
                            );
                }
            }
        }

        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );

        for (i=0; i<count_prohibited; i++)
        {
            const char* psz_va = NULL;
            SG_bool b = SG_FALSE;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_prohibited, i, &psz_va)  );

            b = SG_FALSE;
            SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb, psz_va, &b, NULL)  );
            if (b)
            {
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "%s.%s:  duplicate value in prohibited[]", psz_where, psz_cur)
                        );
            }

            SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, psz_va)  );
        }
        SG_RBTREE_NULLFREE(pCtx, prb);
    }

    // fall thru

fail:
    SG_STRING_NULLFREE(pCtx, pstr_where);
    SG_RBTREE_NULLFREE(pCtx, prb);
}

static void SG_zingtemplate__validate__constraints__dagnode(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh_constraints
        )
{
    SG_bool b_has_required = SG_FALSE;
    SG_bool b_required = SG_FALSE;
    const char* psz_dag = 0;

    SG_string* pstr_where = NULL;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s", psz_where, psz_cur)  );

    SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, psz_where, psz_cur, pvh_constraints,
                SG_ZING_TEMPLATE__REQUIRED,
                SG_ZING_TEMPLATE__DAG,
                NULL)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__REQUIRED, &b_has_required, &b_required)  );

    SG_ERR_CHECK(  sg_zingtemplate__required__sz(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__DAG, &psz_dag)  );
    if (
            (0 == strcmp(psz_dag, "VERSION_CONTROL"))
            || (0 == strcmp(psz_dag, "WIKI"))
            || (0 == strcmp(psz_dag, "FORUM"))
            || (0 == strcmp(psz_dag, "WORK_ITEMS"))
            || (0 == strcmp(psz_dag, "TESTING_DB"))
       )
    {
        // ok
    }
    else
    {
        SG_uint32 dagnum = 0;

        SG_ERR_CHECK(  SG_uint32__parse__strict(pCtx, &dagnum, psz_dag)  );
    }

    // fall thru

fail:
    SG_STRING_NULLFREE(pCtx, pstr_where);
}

static void SG_zingtemplate__validate__constraints__datetime(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh_constraints
        )
{
    SG_bool b_has_required = SG_FALSE;
    SG_bool b_required = SG_FALSE;

    const char* psz_defaultvalue = NULL;

    SG_bool b_has_min = SG_FALSE;
    SG_int64 i_min = 0;

    SG_bool b_has_max = SG_FALSE;
    SG_int64 i_max = 0;

    SG_string* pstr_where = NULL;
    SG_rbtree* prb = NULL;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s", psz_where, psz_cur)  );

    SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, psz_where, psz_cur, pvh_constraints,
                SG_ZING_TEMPLATE__REQUIRED,
                SG_ZING_TEMPLATE__MIN,
                SG_ZING_TEMPLATE__MAX,
                SG_ZING_TEMPLATE__DEFAULTVALUE,
                NULL)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__REQUIRED, &b_has_required, &b_required)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__sz(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &psz_defaultvalue)  );

    if (psz_defaultvalue)
    {
        if (0 == strcmp(psz_defaultvalue, "now"))
        {
            // TODO
        }
        else
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  invalid defaultvalue for datetime field: %s", psz_where, psz_cur, psz_defaultvalue)
                    );
        }
    }

    SG_ERR_CHECK(  sg_zingtemplate__optional__int64_or_double(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__MIN, &b_has_min, &i_min)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__int64_or_double(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__MAX, &b_has_max, &i_max)  );

    if (b_has_max && b_has_min && (i_max < i_min))
    {
        SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  max < min", psz_where, psz_cur)
                );
    }

    // fall thru

fail:
    SG_RBTREE_NULLFREE(pCtx, prb);
    SG_STRING_NULLFREE(pCtx, pstr_where);
}

static void SG_zingtemplate__validate__constraints__int(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh_constraints
        )
{
    SG_uint32 i = 0;

    SG_bool b_has_required = SG_FALSE;
    SG_bool b_required = SG_FALSE;
    SG_bool b_has_unique = SG_FALSE;
    SG_bool b_unique = SG_FALSE;

    SG_bool b_has_defaultvalue = SG_FALSE;
    SG_int64 i_defaultvalue = 0;

    SG_bool b_has_min = SG_FALSE;
    SG_int64 i_min = 0;

    SG_bool b_has_max = SG_FALSE;
    SG_int64 i_max = 0;

    SG_varray* pva_allowed = NULL;
    SG_uint32 count_allowed = 0;
    SG_varray* pva_prohibited = NULL;
    SG_uint32 count_prohibited = 0;
    SG_string* pstr_where = NULL;
    SG_rbtree* prb = NULL;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s", psz_where, psz_cur)  );

    SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, psz_where, psz_cur, pvh_constraints,
                SG_ZING_TEMPLATE__REQUIRED,
                SG_ZING_TEMPLATE__MIN,
                SG_ZING_TEMPLATE__MAX,
                SG_ZING_TEMPLATE__DEFAULTVALUE,
                SG_ZING_TEMPLATE__ALLOWED,
                SG_ZING_TEMPLATE__PROHIBITED,
                SG_ZING_TEMPLATE__UNIQUE,
                NULL)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__UNIQUE, &b_has_unique, &b_unique)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__REQUIRED, &b_has_required, &b_required)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__int64(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &b_has_defaultvalue, &i_defaultvalue)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__int64(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__MIN, &b_has_min, &i_min)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__int64(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__MAX, &b_has_max, &i_max)  );

    if (b_has_max && b_has_min && (i_max < i_min))
    {
        SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  max < min", psz_where, psz_cur)
                );
    }

    if (b_has_max && b_has_defaultvalue && (i_defaultvalue > i_max))
    {
        SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  defaultvalue > max", psz_where, psz_cur)
                );
    }

    if (b_has_min && b_has_defaultvalue && (i_defaultvalue < i_min))
    {
        SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  defaultvalue < min", psz_where, psz_cur)
                );
    }

    SG_ERR_CHECK(  sg_zingtemplate__optional__varray(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__ALLOWED, &pva_allowed)  );
    if (pva_allowed)
    {
        SG_ERR_CHECK(  sg_zingtemplate__varray__check_type_of_members(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__ALLOWED, pva_allowed, SG_VARIANT_TYPE_INT64, &count_allowed)  );

        if (0 == count_allowed)
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  allowed values is empty", psz_where, psz_cur)
                    );
        }

        if (b_has_defaultvalue)
        {
            SG_bool b_defaultvalue_is_allowed = SG_FALSE;
            for (i=0; i<count_allowed; i++)
            {
                SG_int64 va = 0;

                SG_ERR_CHECK(  SG_varray__get__int64(pCtx, pva_allowed, i, &va)  );
                if (i_defaultvalue == va)
                {
                    b_defaultvalue_is_allowed = SG_TRUE;
                    break;
                }
            }
            if (!b_defaultvalue_is_allowed)
            {
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "%s.%s:  defaultvalue which is not in list of values allowed", psz_where, psz_cur)
                        );
            }
        }

        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );

        for (i=0; i<count_allowed; i++)
        {
            SG_int64 va = 0;
            SG_bool b = SG_FALSE;
            SG_int_to_string_buffer sz_i;

            SG_ERR_CHECK(  SG_varray__get__int64(pCtx, pva_allowed, i, &va)  );
            if (b_has_min)
            {
                if (va < i_min)
                {
                    SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                            (pCtx, "%s.%s: allowed value is less than the specified min", psz_where, psz_cur)
                            );
                }
            }
            if (b_has_max)
            {
                if (va > i_max)
                {
                    SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                            (pCtx, "%s.%s:  allowed value is greater than the specified max", psz_where, psz_cur)
                            );
                }
            }

            b = SG_FALSE;

            SG_int64_to_sz(va, sz_i);
            SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb, sz_i, &b, NULL)  );
            if (b)
            {
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "%s.%s:  duplicate value in allowed[]", psz_where, psz_cur)
                        );
            }

            SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, sz_i)  );
        }

        SG_RBTREE_NULLFREE(pCtx, prb);
    }

    SG_ERR_CHECK(  sg_zingtemplate__optional__varray(pCtx, psz_where, psz_cur, pvh_constraints, SG_ZING_TEMPLATE__PROHIBITED, &pva_prohibited)  );
    if (pva_prohibited)
    {
        SG_ERR_CHECK(  sg_zingtemplate__varray__check_type_of_members(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__PROHIBITED, pva_prohibited, SG_VARIANT_TYPE_INT64, &count_prohibited)  );

        if (count_allowed && count_prohibited)
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  has both prohibited and allowed", psz_where, psz_cur)
                    );
        }

        if (b_has_defaultvalue)
        {
            for (i=0; i<count_prohibited; i++)
            {
                SG_int64 va = 0;

                SG_ERR_CHECK(  SG_varray__get__int64(pCtx, pva_prohibited, i, &va)  );
                if (i_defaultvalue == va)
                {
                    SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                            (pCtx, "%s.%s:  has defaultvalue which is in list of values prohibited", psz_where, psz_cur)
                            );
                }
            }
        }

        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );

        for (i=0; i<count_prohibited; i++)
        {
            SG_int64 va = 0;
            SG_bool b = SG_FALSE;
            SG_int_to_string_buffer sz_i;

            SG_ERR_CHECK(  SG_varray__get__int64(pCtx, pva_prohibited, i, &va)  );
            SG_int64_to_sz(va, sz_i);

            b = SG_FALSE;
            SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb, sz_i, &b, NULL)  );
            if (b)
            {
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "%s.%s:  duplicate value in prohibited[]", psz_where, psz_cur)
                        );
            }

            SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, sz_i)  );
        }
        SG_RBTREE_NULLFREE(pCtx, prb);
    }

    // fall thru

fail:
    SG_RBTREE_NULLFREE(pCtx, prb);
    SG_STRING_NULLFREE(pCtx, pstr_where);
}

static void sg_zingtemplate__validate_field_name(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur
        )
{
    SG_uint32 i = 0;
    SG_bool b = SG_FALSE;

    if (strlen(psz_cur) > SG_ZING_MAX_FIELD_NAME_LENGTH)
    {
        SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  name too long (max %d characters)", psz_where, psz_cur, SG_ZING_MAX_FIELD_NAME_LENGTH)
                );
    }

    SG_ERR_CHECK(  SG_zing__is_reserved_field_name(pCtx, psz_cur, &b)  );
    if (b)
    {
        SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                (pCtx, "%s.%s:  cannot use reseved field name", psz_where, psz_cur)
                );
    }

    while (1)
    {
        char c = psz_cur[i];

        if (!c)
        {
            break;
        }

        if (
                (0 == i)
                && (c >= '0')
                && (c <= '9')
           )
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  name cannot start with a digit", psz_where, psz_cur)
                    );
        }

        if (
                ('_' == c)
                || (  (c >= '0') && (c <= '9')  )
                || (  (c >= 'a') && (c <= 'z')  )
                || (  (c >= 'A') && (c <= 'Z')  )
           )
        {
            // no problem
        }
        else
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  invalid character in name", psz_where, psz_cur)
                    );
        }

        i++;
    }

    // fall thru

fail:
    return;
}

static void sg_zingtemplate__validate_linkside(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh_top,
        SG_vhash* pvh
        )
{
    SG_varray* pva_link_rectypes = NULL;
    SG_uint32 count_link_rectypes = 0;
    SG_bool b_singular = SG_FALSE;
    const char* psz_name = NULL;
    SG_uint32 i = 0;
    SG_vhash* pvh_rectypes = NULL;
    SG_string* pstr_where = NULL;
    SG_bool b_has_required = SG_FALSE;
    SG_bool b_required = SG_FALSE;
    SG_rbtree* prb = NULL;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s", psz_where, psz_cur)  );

    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_top, SG_ZING_TEMPLATE__RECTYPES, &pvh_rectypes)  );

    SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, psz_where, psz_cur, pvh,
                SG_ZING_TEMPLATE__LINK_RECTYPES,
                SG_ZING_TEMPLATE__NAME,
                SG_ZING_TEMPLATE__SINGULAR,
                SG_ZING_TEMPLATE__REQUIRED,
                NULL)  );

    SG_ERR_CHECK(  sg_zingtemplate__required__bool(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__SINGULAR, &b_singular)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__REQUIRED, &b_has_required, &b_required)  );

    // TODO can we allow a non-singular link to be required?  what does this
    // mean?  the collection of links must be non-empty?

    SG_ERR_CHECK(  sg_zingtemplate__required__sz(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__NAME, &psz_name)  );
    // the rules for the name of a link are the same as for a field name
    SG_ERR_CHECK(  sg_zingtemplate__validate_field_name(pCtx, psz_where, psz_name)  );

    SG_ERR_CHECK(  sg_zingtemplate__required__varray(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__LINK_RECTYPES, &pva_link_rectypes)  );
    SG_ERR_CHECK(  sg_zingtemplate__varray__check_type_of_members(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__LINK_RECTYPES, pva_link_rectypes, SG_VARIANT_TYPE_SZ, &count_link_rectypes)  );

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );
    for (i=0; i<count_link_rectypes; i++)
    {
        const char* psz_rectype = NULL;
        SG_bool b = SG_FALSE;

        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_link_rectypes, i, &psz_rectype)  );
        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_rectypes, psz_rectype, &b)  );
        if (!b)
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  %s is not a rectype", SG_string__sz(pstr_where), psz_cur, psz_rectype)
                    );
        }

        b = SG_FALSE;
        SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb, psz_rectype, &b, NULL)  );
        if (b)
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  duplicate value in link_rectypes", SG_string__sz(pstr_where), psz_cur)
                    );
        }

        SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, psz_rectype)  );
    }
    SG_RBTREE_NULLFREE(pCtx, prb);

    // make sure the name is not a field name for any of the given rectypes
    for (i=0; i<count_link_rectypes; i++)
    {
        const char* psz_rectype = NULL;
        SG_vhash* pvh_rt = NULL;
        SG_vhash* pvh_fields = NULL;
        SG_bool b = SG_FALSE;

        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_link_rectypes, i, &psz_rectype)  );
        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_rectypes, psz_rectype, &pvh_rt)  );
        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_rt, SG_ZING_TEMPLATE__FIELDS, &pvh_fields)  );
        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_fields, psz_name, &b)  );
        if (b)
        {
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "%s.%s:  %s is not a valid name because it is a field in %s", SG_string__sz(pstr_where), psz_name, psz_rectype)
                    );
        }
    }

    // fall thru

fail:
    SG_RBTREE_NULLFREE(pCtx, prb);
    SG_STRING_NULLFREE(pCtx, pstr_where);
}

static void SG_zingtemplate__validate__one_field(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh,
        const char* psz_merge_type
        )
{
    const char* psz_datatype = NULL;
    SG_vhash* pvh_constraints = NULL;
    SG_string* pstr_where = NULL;
    SG_bool b_field_merge = SG_FALSE;
    SG_vhash* pvh_merge = NULL;
    SG_vhash* pvh_form = NULL;
    SG_vhash* pvh_calculated = NULL;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s", psz_where, psz_cur)  );

    SG_ERR_CHECK(  sg_zingtemplate__validate_field_name(pCtx, psz_where, psz_cur)  );

    SG_ERR_CHECK(  sg_zingtemplate__required__sz(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__DATATYPE, &psz_datatype)  );

    // TODO clean this up.  should merge_type be required?
    // currently this code assumes merge type is record if the merge_type is absent,
    // but other parts of the code make the opposite assumption

    b_field_merge = (
            psz_merge_type
            && (0 == strcmp("field", psz_merge_type))
       );

    SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, psz_where, psz_cur, pvh,
                SG_ZING_TEMPLATE__DATATYPE,
                SG_ZING_TEMPLATE__CONSTRAINTS,
                SG_ZING_TEMPLATE__FORM,
                b_field_merge ? SG_ZING_TEMPLATE__MERGE : "",
                SG_ZING_TEMPLATE__CALCULATED,
                NULL)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__vhash(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__CALCULATED, &pvh_calculated)  );
    if (pvh_calculated)
    {
        if (0 == strcmp(psz_datatype, "int"))
        {
            SG_ERR_CHECK(  SG_zingtemplate__validate__calculated__int(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__CALCULATED, pvh_calculated)  );
        }
        else if (0 == strcmp(psz_datatype, "datetime"))
        {
            SG_ERR_CHECK(  SG_zingtemplate__validate__calculated__datetime(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__CALCULATED, pvh_calculated)  );
        }
        else if (0 == strcmp(psz_datatype, "userid"))
        {
            SG_ERR_CHECK(  SG_zingtemplate__validate__calculated__userid(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__CALCULATED, pvh_calculated)  );
        }
        else if (0 == strcmp(psz_datatype, "string"))
        {
            SG_ERR_CHECK(  SG_zingtemplate__validate__calculated__string(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__CALCULATED, pvh_calculated)  );
        }
        else if (0 == strcmp(psz_datatype, "bool"))
        {
            SG_ERR_CHECK(  SG_zingtemplate__validate__calculated__bool(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__CALCULATED, pvh_calculated)  );
        }
        else
        {
            // TODO handle other field types here
            SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                    (pCtx, "Invalid datatype for calculated field: '%s'", psz_datatype)
                    );
        }
    }

    // Note that if there is a "calculated" section, then some constraints
    // aren't going to make sense.  We could flag them as errors if we wanted
    // to be fussier.  TODO

    if (0 == strcmp(psz_datatype, "dagnode"))
    {
        SG_ERR_CHECK(  sg_zingtemplate__required__vhash(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__CONSTRAINTS, &pvh_constraints)  );
        SG_ERR_CHECK(  SG_zingtemplate__validate__constraints__dagnode(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__CONSTRAINTS, pvh_constraints)  );
    }
    else
    {
        SG_ERR_CHECK(  sg_zingtemplate__optional__vhash(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__CONSTRAINTS, &pvh_constraints)  );

        if (pvh_constraints)
        {
            if (0 == strcmp(psz_datatype, "int"))
            {
                SG_ERR_CHECK(  SG_zingtemplate__validate__constraints__int(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__CONSTRAINTS, pvh_constraints)  );
            }
            else if (0 == strcmp(psz_datatype, "datetime"))
            {
                SG_ERR_CHECK(  SG_zingtemplate__validate__constraints__datetime(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__CONSTRAINTS, pvh_constraints)  );
            }
            else if (0 == strcmp(psz_datatype, "userid"))
            {
                SG_ERR_CHECK(  SG_zingtemplate__validate__constraints__userid(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__CONSTRAINTS, pvh_constraints)  );
            }
            else if (0 == strcmp(psz_datatype, "string"))
            {
                SG_ERR_CHECK(  SG_zingtemplate__validate__constraints__string(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__CONSTRAINTS, pvh_constraints)  );
            }
            else if (0 == strcmp(psz_datatype, "bool"))
            {
                SG_ERR_CHECK(  SG_zingtemplate__validate__constraints__bool(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__CONSTRAINTS, pvh_constraints)  );
            }
            else if (0 == strcmp(psz_datatype, "attachment"))
            {
                SG_ERR_CHECK(  SG_zingtemplate__validate__constraints__attachment(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__CONSTRAINTS, pvh_constraints)  );
            }
            else
            {
                // TODO handle other field types here
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "Invalid datatype: '%s'", psz_datatype)
                        );
            }
        }
    }

    SG_ERR_CHECK(  sg_zingtemplate__optional__vhash(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__FORM, &pvh_form)  );
    if (pvh_form)
    {
        SG_ERR_CHECK(  SG_zingtemplate__validate_field_form(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__FORM, psz_datatype, pvh_constraints, pvh_form)  );
    }

    if (b_field_merge)
    {
        SG_ERR_CHECK(  sg_zingtemplate__optional__vhash(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__MERGE, &pvh_merge)  );
        if (pvh_merge)
        {
            SG_ERR_CHECK(  SG_zingtemplate__validate_field_merge(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__MERGE, psz_datatype, pvh_constraints, pvh_merge)  );
        }
    }

    // TODO if we need fulltext keyword indexing of string fields (and we do),
    // it should be a separate flag.

    // fall thru

fail:
    SG_STRING_NULLFREE(pCtx, pstr_where);
}

static void SG_zingtemplate__validate__one_rectype(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh
        )
{
    SG_vhash* pvh_fields = NULL;
    SG_uint32 count_fields = 0;
    SG_uint32 i = 0;
    SG_string* pstr_where = NULL;
    const char* psz_merge_type = NULL;
    SG_vhash* pvh_merge = NULL;
    SG_bool b_has_no_recid = SG_FALSE;
    SG_bool b_no_recid = SG_FALSE;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s", psz_where, psz_cur)  );

    // the rules for the name of a rectype are the same as for a field name
    SG_ERR_CHECK(  sg_zingtemplate__validate_field_name(pCtx, psz_where, psz_cur)  );

    SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, psz_where, psz_cur, pvh,
                SG_ZING_TEMPLATE__FIELDS,
                SG_ZING_TEMPLATE__NO_RECID,
                SG_ZING_TEMPLATE__MERGE_TYPE,
                NULL)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__bool(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__NO_RECID, &b_has_no_recid, &b_no_recid)  );

    SG_ERR_CHECK(  sg_zingtemplate__optional__sz(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__MERGE_TYPE, &psz_merge_type)  );

    SG_ERR_CHECK(  sg_zingtemplate__required__vhash(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__FIELDS, &pvh_fields)  );
    SG_ERR_CHECK(  sg_zingtemplate__vhash__check_type_of_members(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__FIELDS, pvh_fields, SG_VARIANT_TYPE_VHASH, &count_fields)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s.%s", psz_where, psz_cur, SG_ZING_TEMPLATE__FIELDS)  );
    for (i=0; i<count_fields; i++)
    {
        const SG_variant* pv = NULL;
        const char* psz_field = NULL;
        SG_vhash* pvh_one_field = NULL;

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_fields, i, &psz_field, &pv)  );
        SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &pvh_one_field)  );
        SG_ERR_CHECK(  SG_zingtemplate__validate__one_field(pCtx, SG_string__sz(pstr_where), psz_field, pvh_one_field, psz_merge_type)  );
    }

    if (psz_merge_type &&
            (0 == strcmp("record", psz_merge_type))
       )
    {
        SG_ERR_CHECK(  sg_zingtemplate__required__vhash(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__MERGE, &pvh_merge)  );

        if (pvh_merge)
        {
            // TODO there are basically three choices here:
            // 1.  auto-choose one record based on who did it
            // 2.  auto-choose one record based on when it was done
            // 3.  call a JS hook to resolve the merge
        }
    }

    // fall thru

fail:
    SG_STRING_NULLFREE(pCtx, pstr_where);
}

static void SG_zingtemplate__validate__one_directed_linktype(
        SG_context* pCtx,
        const char* psz_where,
        const char* psz_cur,
        SG_vhash* pvh_top,
        SG_vhash* pvh
        )
{
    SG_vhash* pvh_from = NULL;
    SG_vhash* pvh_to = NULL;
    SG_string* pstr_where = NULL;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_where, "%s.%s", psz_where, psz_cur)  );

    // the rules for the name of a directed_linktype are the same as for a field name
    SG_ERR_CHECK(  sg_zingtemplate__validate_field_name(pCtx, psz_where, psz_cur)  );

    SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, psz_where, psz_cur, pvh,
                SG_ZING_TEMPLATE__FROM,
                SG_ZING_TEMPLATE__TO,
                NULL)  );

    SG_ERR_CHECK(  sg_zingtemplate__required__vhash(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__FROM, &pvh_from)  );
    SG_ERR_CHECK(  sg_zingtemplate__validate_linkside(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__FROM, pvh_top, pvh_from)  );
    SG_ERR_CHECK(  sg_zingtemplate__required__vhash(pCtx, psz_where, psz_cur, pvh, SG_ZING_TEMPLATE__TO, &pvh_to)  );
    SG_ERR_CHECK(  sg_zingtemplate__validate_linkside(pCtx, SG_string__sz(pstr_where), SG_ZING_TEMPLATE__TO, pvh_top, pvh_to)  );

    // fall thru

fail:
    SG_STRING_NULLFREE(pCtx, pstr_where);
}

static void x_zingtemplate__validate(
        SG_context* pCtx,
        SG_vhash* pvh
        )
{
    SG_vhash* pvh_rectypes = NULL;
    SG_vhash* pvh_directed_linktypes = NULL;
    SG_vhash* pvh_undirected_linktypes = NULL;
    SG_uint32 count_rectypes = 0;
    SG_uint32 count_directed_linktypes = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  sg_zingtemplate__allowed_members(pCtx, "", "", pvh,
                SG_ZING_TEMPLATE__VERSION,
                SG_ZING_TEMPLATE__RECTYPES,
                SG_ZING_TEMPLATE__DIRECTED_LINKTYPES,
                SG_ZING_TEMPLATE__UNDIRECTED_LINKTYPES,
                NULL)  );

    SG_ERR_CHECK(  sg_zingtemplate__required__vhash(pCtx, "", "", pvh, SG_ZING_TEMPLATE__RECTYPES, &pvh_rectypes)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__vhash(pCtx, "", "", pvh, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, &pvh_directed_linktypes)  );
    SG_ERR_CHECK(  sg_zingtemplate__optional__vhash(pCtx, "", "", pvh, SG_ZING_TEMPLATE__UNDIRECTED_LINKTYPES, &pvh_undirected_linktypes)  );

    SG_ERR_CHECK(  sg_zingtemplate__vhash__check_type_of_members(pCtx, "", SG_ZING_TEMPLATE__RECTYPES, pvh_rectypes, SG_VARIANT_TYPE_VHASH, &count_rectypes)  );
    for (i=0; i<count_rectypes; i++)
    {
        const SG_variant* pv = NULL;
        const char* psz_rectype = NULL;
        SG_vhash* pvh_one_rectype = NULL;

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_rectypes, i, &psz_rectype, &pv)  );
        SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &pvh_one_rectype)  );
        SG_ERR_CHECK(  SG_zingtemplate__validate__one_rectype(pCtx, SG_ZING_TEMPLATE__RECTYPES, psz_rectype, pvh_one_rectype)  );
    }

    if (pvh_directed_linktypes)
    {
        SG_ERR_CHECK(  sg_zingtemplate__vhash__check_type_of_members(pCtx, "", SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, pvh_directed_linktypes, SG_VARIANT_TYPE_VHASH, &count_directed_linktypes)  );
        for (i=0; i<count_directed_linktypes; i++)
        {
            const SG_variant* pv = NULL;
            const char* psz_directed_linktype = NULL;
            SG_vhash* pvh_one_directed_linktype = NULL;

            SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_directed_linktypes, i, &psz_directed_linktype, &pv)  );
            SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &pvh_one_directed_linktype)  );
            SG_ERR_CHECK(  SG_zingtemplate__validate__one_directed_linktype(pCtx, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, psz_directed_linktype, pvh, pvh_one_directed_linktype)  );
        }
    }

    return;

fail:
    return;
}

void SG_zingtemplate__validate(
        SG_context* pCtx,
        SG_vhash* pvh,
        SG_uint32* pi_version
        )
{
    SG_int64 version = 0;

    SG_ERR_CHECK(  sg_zingtemplate__required__int64(pCtx, "", "", pvh, SG_ZING_TEMPLATE__VERSION, &version)  );
    switch (version)
    {
        case 1:
            {
                SG_ERR_CHECK(  x_zingtemplate__validate(pCtx, pvh)  );
                *pi_version = (SG_uint32) version;
            }
            break;
        default:
            {
                SG_ERR_THROW2(  SG_ERR_ZING_INVALID_TEMPLATE,
                        (pCtx, "Template version '%d' is unsupported", version)
                        );
            }
            break;
    }

    return;

fail:
    return;
}

void SG_zingtemplate__alloc(
        SG_context* pCtx,
        SG_vhash** ppvh,
        SG_zingtemplate** pptemplate
        )
{
    SG_zingtemplate* pThis = NULL;
    SG_uint32 version = 0;

    SG_ERR_CHECK(  SG_zingtemplate__validate(pCtx, *ppvh, &version)  );

	SG_ERR_CHECK(  SG_alloc1(pCtx, pThis)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pThis->prb_field_attributes)  );
    pThis->version = version;
    pThis->pvh = *ppvh;
    *ppvh = NULL;

    *pptemplate = pThis;
    pThis = NULL;

    return;

fail:
    SG_NULLFREE(pCtx, pThis);
    return;
}

static void x__list_singular_links__one_side(
        SG_context* pCtx,
        const char* psz_rectype,
        const char* psz_link_name,
        SG_vhash* pvh,
        SG_rbtree* prb
        )
{
    SG_bool b_has_singular = SG_FALSE;
    SG_bool b_singular = SG_FALSE;
    SG_varray* pva_rectypes = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;
    SG_bool b_found = SG_FALSE;

    SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, pvh, SG_ZING_TEMPLATE__SINGULAR, &b_has_singular)  );
    if (b_has_singular)
    {
        SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvh, SG_ZING_TEMPLATE__SINGULAR, &b_singular)  );
    }

    if (!b_singular)
    {
        return;
    }

    SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh, SG_ZING_TEMPLATE__LINK_RECTYPES, &pva_rectypes)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_rectypes, &count)  );
    for (i=0; i<count; i++)
    {
        const char* psz_listed_rectype = NULL;

        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_rectypes, i, &psz_listed_rectype)  );
        if (0 == strcmp(psz_rectype, psz_listed_rectype))
        {
            b_found = SG_TRUE;
            break;
        }
    }

    if (b_found)
    {
        const char* psz_side_name = NULL;
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, SG_ZING_TEMPLATE__NAME, &psz_side_name)  );
        SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, prb, psz_link_name, psz_side_name)  );
    }

    // fall thru

fail:
    return;
}

static void x__list_required_links__one_side(
        SG_context* pCtx,
        const char* psz_rectype,
        const char* psz_link_name,
        SG_vhash* pvh,
        SG_rbtree* prb
        )
{
    SG_bool b_has_required = SG_FALSE;
    SG_bool b_required = SG_FALSE;
    SG_varray* pva_rectypes = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;
    SG_bool b_found = SG_FALSE;

    SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, pvh, SG_ZING_TEMPLATE__REQUIRED, &b_has_required)  );
    if (b_has_required)
    {
        SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvh, SG_ZING_TEMPLATE__REQUIRED, &b_required)  );
    }

    if (!b_required)
    {
        return;
    }

    SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh, SG_ZING_TEMPLATE__LINK_RECTYPES, &pva_rectypes)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_rectypes, &count)  );
    for (i=0; i<count; i++)
    {
        const char* psz_listed_rectype = NULL;

        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_rectypes, i, &psz_listed_rectype)  );
        if (0 == strcmp(psz_rectype, psz_listed_rectype))
        {
            b_found = SG_TRUE;
            break;
        }
    }

    if (b_found)
    {
        const char* psz_side_name = NULL;
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, SG_ZING_TEMPLATE__NAME, &psz_side_name)  );
        SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, prb, psz_link_name, psz_side_name)  );
    }

    // fall thru

fail:
    return;
}

static void x__list_required_links(
        SG_context* pCtx,
        const char* psz_rectype,
        const char* psz_link_name,
        SG_vhash* pvh,
        SG_rbtree* prb_from,
        SG_rbtree* prb_to
        )
{
    SG_vhash* pvh_from = NULL;
    SG_vhash* pvh_to = NULL;

    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, SG_ZING_TEMPLATE__FROM, &pvh_from)  );
    SG_ERR_CHECK(  x__list_required_links__one_side(pCtx, psz_rectype, psz_link_name, pvh_from, prb_from)  );

    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, SG_ZING_TEMPLATE__TO, &pvh_to)  );
    SG_ERR_CHECK(  x__list_required_links__one_side(pCtx, psz_rectype, psz_link_name, pvh_to, prb_to)  );

    // fall thru

fail:
    return;
}

static void x__list_all_links__one_side(
        SG_context* pCtx,
        const char* psz_rectype,
        const char* psz_link_name,
        SG_vhash* pvh,
        SG_rbtree* prb
        )
{
    SG_varray* pva_rectypes = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;
    SG_bool b_found = SG_FALSE;

    SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh, SG_ZING_TEMPLATE__LINK_RECTYPES, &pva_rectypes)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_rectypes, &count)  );
    for (i=0; i<count; i++)
    {
        const char* psz_listed_rectype = NULL;

        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_rectypes, i, &psz_listed_rectype)  );
        if (0 == strcmp(psz_rectype, psz_listed_rectype))
        {
            b_found = SG_TRUE;
            break;
        }
    }

    if (b_found)
    {
        const char* psz_side_name = NULL;
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, SG_ZING_TEMPLATE__NAME, &psz_side_name)  );
        SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, prb, psz_link_name, psz_side_name)  );
    }

    // fall thru

fail:
    return;
}

static void x__list_all_links(
        SG_context* pCtx,
        const char* psz_rectype,
        const char* psz_link_name,
        SG_vhash* pvh,
        SG_rbtree* prb_from,
        SG_rbtree* prb_to
        )
{
    SG_vhash* pvh_from = NULL;
    SG_vhash* pvh_to = NULL;

    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, SG_ZING_TEMPLATE__FROM, &pvh_from)  );
    SG_ERR_CHECK(  x__list_all_links__one_side(pCtx, psz_rectype, psz_link_name, pvh_from, prb_from)  );

    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, SG_ZING_TEMPLATE__TO, &pvh_to)  );
    SG_ERR_CHECK(  x__list_all_links__one_side(pCtx, psz_rectype, psz_link_name, pvh_to, prb_to)  );

    // fall thru

fail:
    return;
}

static void x__list_singular_links(
        SG_context* pCtx,
        const char* psz_rectype,
        const char* psz_link_name,
        SG_vhash* pvh,
        SG_rbtree* prb_from,
        SG_rbtree* prb_to
        )
{
    SG_vhash* pvh_from = NULL;
    SG_vhash* pvh_to = NULL;

    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, SG_ZING_TEMPLATE__FROM, &pvh_from)  );
    SG_ERR_CHECK(  x__list_singular_links__one_side(pCtx, psz_rectype, psz_link_name, pvh_from, prb_from)  );

    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, SG_ZING_TEMPLATE__TO, &pvh_to)  );
    SG_ERR_CHECK(  x__list_singular_links__one_side(pCtx, psz_rectype, psz_link_name, pvh_to, prb_to)  );

    // fall thru

fail:
    return;
}

void SG_zingtemplate__has_any_links(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        SG_bool* pb
        )
{
    SG_bool b_has_directed_linktypes = SG_FALSE;
    SG_vhash* pvh = NULL;
    SG_uint32 count = 0;
    SG_bool b_result = SG_FALSE;

    SG_ERR_CHECK(  SG_vhash__has(pCtx, pztemplate->pvh, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, &b_has_directed_linktypes)  );
    if (b_has_directed_linktypes)
    {
        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pztemplate->pvh, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, &pvh)  );
        SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );
        if (count > 0)
        {
            b_result = SG_TRUE;
        }
    }

    *pb = b_result;

fail:
    ;
}

void SG_zingtemplate__list_all_links(
	        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        SG_rbtree** pp_from,
        SG_rbtree** pp_to
        )
{
    SG_bool b_has_directed_linktypes = SG_FALSE;
    SG_vhash* pvh = NULL;
    SG_rbtree* prb_from = NULL;
    SG_rbtree* prb_to = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;

    SG_ERR_CHECK(  SG_vhash__has(pCtx, pztemplate->pvh, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, &b_has_directed_linktypes)  );
    if (b_has_directed_linktypes)
    {
        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pztemplate->pvh, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, &pvh)  );
        SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );

        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_from)  );
        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_to)  );

        for (i=0; i<count; i++)
        {
            const SG_variant* pv = NULL;
            const char* psz_directed_linktype = NULL;
            SG_vhash* pvh_one_directed_linktype = NULL;

            SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh, i, &psz_directed_linktype, &pv)  );
            SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &pvh_one_directed_linktype)  );
            SG_ERR_CHECK(  x__list_all_links(pCtx, psz_rectype, psz_directed_linktype, pvh_one_directed_linktype, prb_from, prb_to)  );
        }
    }

    *pp_from = prb_from;
    prb_from = NULL;

    *pp_to = prb_to;
    prb_to = NULL;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_from);
    SG_RBTREE_NULLFREE(pCtx, prb_to);
}

void SG_zingtemplate__list_required_links(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        SG_rbtree** pp_from,
        SG_rbtree** pp_to
        )
{
    SG_bool b_has_directed_linktypes = SG_FALSE;
    SG_vhash* pvh = NULL;
    SG_rbtree* prb_from = NULL;
    SG_rbtree* prb_to = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;

    SG_ERR_CHECK(  SG_vhash__has(pCtx, pztemplate->pvh, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, &b_has_directed_linktypes)  );
    if (b_has_directed_linktypes)
    {
        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pztemplate->pvh, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, &pvh)  );
        SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );

        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_from)  );
        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_to)  );

        for (i=0; i<count; i++)
        {
            const SG_variant* pv = NULL;
            const char* psz_directed_linktype = NULL;
            SG_vhash* pvh_one_directed_linktype = NULL;

            SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh, i, &psz_directed_linktype, &pv)  );
            SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &pvh_one_directed_linktype)  );
            SG_ERR_CHECK(  x__list_required_links(pCtx, psz_rectype, psz_directed_linktype, pvh_one_directed_linktype, prb_from, prb_to)  );
        }
    }

    *pp_from = prb_from;
    prb_from = NULL;

    *pp_to = prb_to;
    prb_to = NULL;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_from);
    SG_RBTREE_NULLFREE(pCtx, prb_to);
}

void SG_zingtemplate__list_singular_links(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        SG_rbtree** pp_from,
        SG_rbtree** pp_to
        )
{
    SG_bool b_has_directed_linktypes = SG_FALSE;
    SG_vhash* pvh = NULL;
    SG_rbtree* prb_from = NULL;
    SG_rbtree* prb_to = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;

    SG_ERR_CHECK(  SG_vhash__has(pCtx, pztemplate->pvh, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, &b_has_directed_linktypes)  );
    if (b_has_directed_linktypes)
    {
        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pztemplate->pvh, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, &pvh)  );
        SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );

        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_from)  );
        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_to)  );

        for (i=0; i<count; i++)
        {
            const SG_variant* pv = NULL;
            const char* psz_directed_linktype = NULL;
            SG_vhash* pvh_one_directed_linktype = NULL;

            SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh, i, &psz_directed_linktype, &pv)  );
            SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &pvh_one_directed_linktype)  );
            SG_ERR_CHECK(  x__list_singular_links(pCtx, psz_rectype, psz_directed_linktype, pvh_one_directed_linktype, prb_from, prb_to)  );
        }
    }

    *pp_from = prb_from;
    prb_from = NULL;

    *pp_to = prb_to;
    prb_to = NULL;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_from);
    SG_RBTREE_NULLFREE(pCtx, prb_to);
}

void SG_zingtemplate__list_fields(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        SG_stringarray** pp
        )
{
    SG_stringarray* psa = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;
    SG_vhash* pvh_fields = NULL;

    SG_ERR_CHECK(  SG_vhash__path__get__vhash(pCtx, pztemplate->pvh, &pvh_fields, NULL, SG_ZING_TEMPLATE__RECTYPES, psz_rectype, SG_ZING_TEMPLATE__FIELDS, NULL)  );
    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_fields, &count)  );
    if (count > 0)
    {
        SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa, count)  );
        for (i=0; i<count; i++)
        {
            const char* psz_name = NULL;
            const SG_variant* pv = NULL;

            SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_fields, i, &psz_name, &pv)  );
            SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa, psz_name)  );
        }
    }

    *pp = psa;
    psa = NULL;

    // fall thru
fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa);
}

void SG_zingtemplate__is_a_rectype(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        SG_bool* pb,
        SG_uint32* pi_count_rectypes
        )
{
    SG_vhash* pvh_rectypes = NULL;

    SG_ERR_CHECK_RETURN(  SG_vhash__path__get__vhash(pCtx, pztemplate->pvh, &pvh_rectypes, NULL, SG_ZING_TEMPLATE__RECTYPES, NULL)  );
    SG_ERR_CHECK_RETURN(  SG_vhash__count(pCtx, pvh_rectypes, pi_count_rectypes)  );
    SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, pvh_rectypes, psz_rectype, pb)  );
}

void SG_zingtemplate__list_rectypes(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        SG_rbtree** pprb
        )
{
    SG_vhash* pvh_rectypes = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_rbtree* prb = NULL;

    SG_ERR_CHECK(  SG_vhash__path__get__vhash(pCtx, pztemplate->pvh, &pvh_rectypes, NULL, SG_ZING_TEMPLATE__RECTYPES, NULL)  );
    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_rectypes, &count)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS(pCtx, &prb, count, NULL)  );
    for (i=0; i<count; i++)
    {
		const char* psz_rectype = NULL;
		const SG_variant* pv = NULL;

		SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_rectypes, i, &psz_rectype, &pv)  );
        SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, psz_rectype)  );
    }

    *pprb = prb;
    prb = NULL;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb);
}

void SG_zingtemplate__get_field_attributes(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        const char* psz_field_name,
        SG_zingfieldattributes** ppzfa
        )
{
    SG_zingfieldattributes* pzfa = NULL;
    SG_vhash* pvh_fa = NULL;
    SG_vhash* pvh_constraints = NULL;
    const char* psz_temp = NULL;
    const char* psz_owned_field_name = NULL;
    const char* psz_owned_rectype = NULL;
    SG_bool b = SG_FALSE;
    SG_vhash* pvh_fields = NULL;
    SG_vhash* pvh_rectypes = NULL;
    char buf_key[256];

    SG_NULLARGCHECK_RETURN(psz_rectype);
    SG_NULLARGCHECK_RETURN(psz_field_name);

    SG_ERR_CHECK(  SG_sprintf(pCtx, buf_key, sizeof(buf_key), "%s.%s", psz_rectype, psz_field_name)  );
    SG_ERR_CHECK(  SG_rbtree__find(pCtx, pztemplate->prb_field_attributes, buf_key, &b, (void**) &pzfa)  );
    if (b && pzfa)
    {
        *ppzfa = pzfa;
        return;
    }

    SG_ERR_CHECK(  SG_vhash__path__get__vhash(pCtx, pztemplate->pvh, &pvh_rectypes, NULL, SG_ZING_TEMPLATE__RECTYPES, NULL)  );
    SG_ERR_CHECK(  SG_vhash__key(pCtx, pvh_rectypes, psz_rectype, &psz_owned_rectype)  );
    if (!psz_owned_rectype)
    {
        *ppzfa = NULL;
        return;
    }

    SG_ERR_CHECK(  SG_vhash__path__get__vhash(pCtx, pztemplate->pvh, &pvh_fields, NULL, SG_ZING_TEMPLATE__RECTYPES, psz_rectype, SG_ZING_TEMPLATE__FIELDS, NULL)  );

    SG_ERR_CHECK(  SG_vhash__key(pCtx, pvh_fields, psz_field_name, &psz_owned_field_name)  );
    if (!psz_owned_field_name)
    {
        *ppzfa = NULL;
        return;
    }

	SG_ERR_CHECK(  SG_alloc1(pCtx, pzfa)  );
    pzfa->name = psz_owned_field_name;
    pzfa->rectype = psz_owned_rectype;
    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_fields, psz_field_name, &pvh_fa)  );
    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_fa, SG_ZING_TEMPLATE__DATATYPE, &psz_temp)  );
    if (0 == strcmp(psz_temp, "int"))
    {
        pzfa->type = SG_ZING_TYPE__INT;
    }
    else if (0 == strcmp(psz_temp, "datetime"))
    {
        pzfa->type = SG_ZING_TYPE__DATETIME;
    }
    else if (0 == strcmp(psz_temp, "userid"))
    {
        pzfa->type = SG_ZING_TYPE__USERID;
    }
    else if (0 == strcmp(psz_temp, "string"))
    {
        pzfa->type = SG_ZING_TYPE__STRING;
    }
    else if (0 == strcmp(psz_temp, "bool"))
    {
        pzfa->type = SG_ZING_TYPE__BOOL;
    }
    else if (0 == strcmp(psz_temp, "attachment"))
    {
        pzfa->type = SG_ZING_TYPE__ATTACHMENT;
    }
    else if (0 == strcmp(psz_temp, "dagnode"))
    {
        pzfa->type = SG_ZING_TYPE__DAGNODE;
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
    }

    // TODO init all the constraints to nothing, for each type
    pzfa->required = SG_FALSE;

    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_fa, SG_ZING_TEMPLATE__CONSTRAINTS, &b)  );
    if (b)
    {
        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_fa, SG_ZING_TEMPLATE__CONSTRAINTS, &pvh_constraints)  );

        // First do the type-independent stuff
        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__REQUIRED, &b)  );
        if (b)
        {
            SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvh_constraints, SG_ZING_TEMPLATE__REQUIRED, &pzfa->required)  );
        }

        switch (pzfa->type)
        {
            case SG_ZING_TYPE__BOOL:
                {
                    pzfa->v._bool.has_defaultvalue = SG_FALSE;

                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &b)  );
                    if (b)
                    {
                        pzfa->v._bool.has_defaultvalue = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &pzfa->v._bool.defaultvalue)  );
                    }
                    break;
                }

            case SG_ZING_TYPE__DAGNODE:
                {
                    SG_uint32 i = 0;
                    const char* psz_dag = NULL;

                    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DAG, &psz_dag)  );
                    if (0 == strcmp(psz_dag, "VERSION_CONTROL"))
                    {
                        i = SG_DAGNUM__VERSION_CONTROL;
                    }
                    else if (0 == strcmp(psz_dag, "TESTING_DB"))
                    {
                        i = SG_DAGNUM__TESTING__DB;
                    }
                    else if (0 == strcmp(psz_dag, "WIKI"))
                    {
                        i = SG_DAGNUM__WIKI;
                    }
                    else if (0 == strcmp(psz_dag, "FORUM"))
                    {
                        i = SG_DAGNUM__FORUM;
                    }
                    else if (0 == strcmp(psz_dag, "WORK_ITEMS"))
                    {
                        i = SG_DAGNUM__WORK_ITEMS;
                    }
                    else
                    {
                        SG_ERR_CHECK(  SG_uint32__parse__strict(pCtx, &i, psz_dag)  );
                    }
                    pzfa->v._dagnode.dagnum = i;
                    break;
                }

            case SG_ZING_TYPE__ATTACHMENT:
                {
                    pzfa->v._attachment.has_maxlength = SG_FALSE;

                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MAXLENGTH, &b)  );
                    if (b)
                    {
                        pzfa->v._attachment.has_maxlength = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MAXLENGTH, &pzfa->v._attachment.val_maxlength)  );
                    }
                    break;
                }

            case SG_ZING_TYPE__USERID:
                {
                    pzfa->v._userid.has_defaultvalue = SG_FALSE;

                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &b)  );
                    if (b)
                    {
                        pzfa->v._userid.has_defaultvalue = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &pzfa->v._userid.defaultvalue)  );
                    }
                    break;
                }

            case SG_ZING_TYPE__STRING:
                {
                    pzfa->v._string.has_defaultfunc = SG_FALSE;
                    pzfa->v._string.has_defaultvalue = SG_FALSE;
                    pzfa->v._string.has_minlength = SG_FALSE;
                    pzfa->v._string.has_maxlength = SG_FALSE;
                    pzfa->v._string.prohibited = NULL;
                    pzfa->v._string.allowed = NULL;
                    pzfa->v._string.unique = SG_FALSE;

                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__UNIQUE, &b)  );
                    if (b)
                    {
                        SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvh_constraints, SG_ZING_TEMPLATE__UNIQUE, &pzfa->v._string.unique)  );
                    }

                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__GENCHARS, &b)  );
                    if (b)
                    {
                        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_constraints, SG_ZING_TEMPLATE__GENCHARS, &pzfa->v._string.genchars)  );
                    }

                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTFUNC, &b)  );
                    if (b)
                    {
                        pzfa->v._string.has_defaultfunc = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTFUNC, &pzfa->v._string.defaultfunc)  );
                    }

                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &b)  );
                    if (b)
                    {
                        pzfa->v._string.has_defaultvalue = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &pzfa->v._string.defaultvalue)  );
                    }
                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MINLENGTH, &b)  );
                    if (b)
                    {
                        pzfa->v._string.has_minlength = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MINLENGTH, &pzfa->v._string.val_minlength)  );
                    }
                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MAXLENGTH, &b)  );
                    if (b)
                    {
                        pzfa->v._string.has_maxlength = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MAXLENGTH, &pzfa->v._string.val_maxlength)  );
                    }
                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__PROHIBITED, &b)  );
                    if (b)
                    {
                        SG_varray* pva = NULL;

                        SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_constraints, SG_ZING_TEMPLATE__PROHIBITED, &pva)  );
                        SG_ERR_CHECK(  SG_varray__to_stringarray(pCtx, pva, &pzfa->v._string.prohibited)  );
                    }
                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__ALLOWED, &b)  );
                    if (b)
                    {
                        SG_varray* pva = NULL;

                        SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_constraints, SG_ZING_TEMPLATE__ALLOWED, &pva)  );
                        SG_ERR_CHECK(  SG_varray__to_vhash_with_indexes(pCtx, pva, &pzfa->v._string.allowed)  );
                        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__SORT_BY_ALLOWED, &b)  );
                        if (b)
                        {
                            SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvh_constraints, SG_ZING_TEMPLATE__SORT_BY_ALLOWED, &pzfa->v._string.sort_by_allowed)  );
                        }
                    }
                    break;
                }

            case SG_ZING_TYPE__INT:
                {
                    pzfa->v._int.has_defaultvalue = SG_FALSE;
                    pzfa->v._int.has_min = SG_FALSE;
                    pzfa->v._int.has_max = SG_FALSE;
                    pzfa->v._int.prohibited = NULL;
                    pzfa->v._int.allowed = NULL;
                    pzfa->v._int.unique = SG_FALSE;

                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__UNIQUE, &b)  );
                    if (b)
                    {
                        SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvh_constraints, SG_ZING_TEMPLATE__UNIQUE, &pzfa->v._int.unique)  );
                    }

                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &b)  );
                    if (b)
                    {
                        pzfa->v._int.has_defaultvalue = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &pzfa->v._int.defaultvalue)  );
                    }
                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MIN, &b)  );
                    if (b)
                    {
                        pzfa->v._int.has_min = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MIN, &pzfa->v._int.val_min)  );
                    }
                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MAX, &b)  );
                    if (b)
                    {
                        pzfa->v._int.has_max = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MAX, &pzfa->v._int.val_max)  );
                    }
                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__PROHIBITED, &b)  );
                    if (b)
                    {
                        SG_varray* pva = NULL;

                        SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_constraints, SG_ZING_TEMPLATE__PROHIBITED, &pva)  );
                        SG_ERR_CHECK(  SG_vector_i64__alloc__from_varray(pCtx, pva, &pzfa->v._int.prohibited)  );
                    }
                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__ALLOWED, &b)  );
                    if (b)
                    {
                        SG_varray* pva = NULL;

                        SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_constraints, SG_ZING_TEMPLATE__ALLOWED, &pva)  );
                        SG_ERR_CHECK(  SG_vector_i64__alloc__from_varray(pCtx, pva, &pzfa->v._int.allowed)  );
                    }
                    break;
                }

            case SG_ZING_TYPE__DATETIME:
                {
                    pzfa->v._datetime.has_defaultvalue = SG_FALSE;
                    pzfa->v._datetime.defaultvalue = NULL;
                    pzfa->v._datetime.has_min = SG_FALSE;
                    pzfa->v._datetime.has_max = SG_FALSE;

                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &b)  );
                    if (b)
                    {
                        pzfa->v._datetime.has_defaultvalue = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_constraints, SG_ZING_TEMPLATE__DEFAULTVALUE, &pzfa->v._datetime.defaultvalue)  );
                    }
                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MIN, &b)  );
                    if (b)
                    {
                        pzfa->v._datetime.has_min = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__int64_or_double(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MIN, &pzfa->v._datetime.val_min)  );
                    }
                    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MAX, &b)  );
                    if (b)
                    {
                        pzfa->v._datetime.has_max = SG_TRUE;
                        SG_ERR_CHECK(  SG_vhash__get__int64_or_double(pCtx, pvh_constraints, SG_ZING_TEMPLATE__MAX, &pzfa->v._datetime.val_max)  );
                    }
                    break;
                }
        }
    }

    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_fa, SG_ZING_TEMPLATE__MERGE, &b)  );
    if (b)
    {
        SG_vhash* pvh_merge = NULL;

        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_fa, SG_ZING_TEMPLATE__MERGE, &pvh_merge)  );

        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_merge, SG_ZING_TEMPLATE__AUTO, &b)  );
        if (b)
        {
            SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_merge, SG_ZING_TEMPLATE__AUTO, &pzfa->pva_automerge)  );
        }

        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_merge, SG_ZING_TEMPLATE__UNIQIFY, &b)  );
        if (b)
        {
            SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_merge, SG_ZING_TEMPLATE__UNIQIFY, &pzfa->uniqify)  );
        }
    }

    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pztemplate->prb_field_attributes, buf_key, pzfa)  );
    *ppzfa = pzfa;
    pzfa = NULL;

fail:
    SG_ZINGFIELDATTRIBUTES_NULLFREE(pCtx, pzfa);
}

void SG_zingtemplate_match_side(
        SG_context* pCtx,
        SG_vhash* pvh_side,
        const char* psz_rectype_mine,
        const char* psz_link_name_mine,
        SG_bool *pb
        )
{
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_varray* pva = NULL;
    const char* psz_rectype = NULL;
    SG_bool b_result = SG_FALSE;
    const char* psz_side_name = NULL;

    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_side, SG_ZING_TEMPLATE__NAME, &psz_side_name)  );
    if (0 == strcmp(psz_side_name, psz_link_name_mine))
    {
        SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_side, SG_ZING_TEMPLATE__LINK_RECTYPES, &pva)  );
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
        for (i=0; i<count; i++)
        {
            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, i, &psz_rectype)  );
            if (0 == strcmp(psz_rectype, psz_rectype_mine))
            {
                b_result = SG_TRUE;
                break;
            }
        }
    }

    *pb = b_result;

    return;

fail:
    return;
}

void SG_zingtemplate__find_link(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype_mine,
        const char* psz_link_name_mine,
        const char** ppsz_link_name,
        SG_bool* pb_mine_is_from,
        SG_vhash** ppvh_link
        )
{
    SG_vhash* pvh_directed_links = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_bool b_match_side = SG_FALSE;

    SG_ERR_CHECK(  SG_vhash__path__get__vhash(pCtx, pztemplate->pvh, &pvh_directed_links, NULL, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, NULL)  );

	SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_directed_links, &count)  );
	for (i=0; i<count; i++)
	{
		const char* psz_link_name = NULL;
		const SG_variant* pv = NULL;
        SG_vhash* pvh_link = NULL;
        SG_vhash* pvh_from = NULL;
        SG_vhash* pvh_to = NULL;

		SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_directed_links, i, &psz_link_name, &pv)  );
		SG_ERR_CHECK(  SG_variant__get__vhash(pCtx,pv,&pvh_link)  );

        // check from
        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_link, SG_ZING_TEMPLATE__FROM, &pvh_from)  );
        SG_ERR_CHECK(  SG_zingtemplate_match_side(pCtx, pvh_from, psz_rectype_mine, psz_link_name_mine, &b_match_side)  );
        if (b_match_side)
        {
            *pb_mine_is_from = SG_TRUE;
            *ppsz_link_name = psz_link_name;
            *ppvh_link = pvh_link;
            break;
        }

        // check to
        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_link, SG_ZING_TEMPLATE__TO, &pvh_to)  );
        SG_ERR_CHECK(  SG_zingtemplate_match_side(pCtx, pvh_to, psz_rectype_mine, psz_link_name_mine, &b_match_side)  );
        if (b_match_side)
        {
            *pb_mine_is_from = SG_FALSE;
            *ppsz_link_name = psz_link_name;
            *ppvh_link = pvh_link;
            break;
        }
	}

    if (!b_match_side)
    {
        *ppsz_link_name = NULL;
        *ppvh_link = NULL;
    }

    return;

fail:
    return;
}

void SG_zingtemplate__fill_in_link_side_attributes(
        SG_context* pCtx,
        SG_vhash* pvh,
        SG_zinglinksideattributes* pzlsa
        )
{
    SG_varray* pva_rectypes = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;
    const char* psz_rectype = NULL;
    SG_bool b_has_required = SG_FALSE;

    SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvh, SG_ZING_TEMPLATE__SINGULAR, &pzlsa->bSingular)  );

    b_has_required = SG_FALSE;
    SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, pvh, SG_ZING_TEMPLATE__REQUIRED, &b_has_required)  );
    if (b_has_required)
    {
        SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvh, SG_ZING_TEMPLATE__REQUIRED, &pzlsa->bRequired)  );
    }

    SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh, SG_ZING_TEMPLATE__LINK_RECTYPES, &pva_rectypes)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_rectypes, &count)  );
    for (i=0; i<count; i++)
    {
        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_rectypes, i, &psz_rectype)  );
        SG_ERR_CHECK(  SG_rbtree__add(pCtx, pzlsa->prb_rectypes, psz_rectype)  );
    }

    // fall thru

fail:
    return;
}

void sg_zingtemplate__get_link_actions(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_link_name,
        SG_rbtree** pprb
        )
{
    SG_rbtree* prb_actions = NULL;
    struct sg_zingaction* pza = NULL;
    SG_uint32 count_rectypes = 0;
    SG_uint32 i_rectype = 0;
    SG_vhash* pvh_link_to = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_name = NULL;
    SG_varray* pva_rectypes = NULL;

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_actions)  );

    // get the link record
    // get the link_rectypes and the name of the "to" side
    // for each of those rectypes, review every field, looking
    // for a calculated field with a depends_on the name of the link.
    // construct an action record for each one
    SG_ERR_CHECK(  SG_vhash__path__get__vhash(pCtx, pztemplate->pvh, &pvh_link_to, &b, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, psz_link_name, SG_ZING_TEMPLATE__TO, NULL)  );

    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_link_to, SG_ZING_TEMPLATE__NAME, &psz_name)  );
    SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_link_to, SG_ZING_TEMPLATE__LINK_RECTYPES, &pva_rectypes)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_rectypes, &count_rectypes)  );
    for (i_rectype=0; i_rectype<count_rectypes; i_rectype++)
    {
        const char* psz_rectype = NULL;
        SG_vhash* pvh_fields = NULL;
        SG_uint32 count_fields = 0;
        SG_uint32 i_field = 0;
        SG_vhash* pvh_field = NULL;

        SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_rectypes, i_rectype, &psz_rectype)  );

        SG_ERR_CHECK(  SG_vhash__path__get__vhash(pCtx, pztemplate->pvh, &pvh_fields, &b, SG_ZING_TEMPLATE__RECTYPES, psz_rectype, SG_ZING_TEMPLATE__FIELDS, NULL)  );

        SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_fields, &count_fields)  );

        for (i_field = 0; i_field<count_fields; i_field++)
        {
            const char* psz_field_name = NULL;
            const SG_variant* pv = NULL;
            SG_vhash* pvh_calculated = NULL;

            SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_fields, i_field, &psz_field_name, &pv)  );
            SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &pvh_field)  );
            SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_field, SG_ZING_TEMPLATE__CALCULATED, &b)  );
            if (b)
            {
                const char* psz_depends_on = NULL;

                SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_field, SG_ZING_TEMPLATE__CALCULATED, &pvh_calculated)  );
                SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_calculated, SG_ZING_TEMPLATE__DEPENDS_ON, &psz_depends_on)  );

                if (0 == strcmp(psz_depends_on, psz_name))
                {
                    SG_ERR_CHECK(  SG_alloc1(pCtx, pza)  );

                    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_calculated, SG_ZING_TEMPLATE__TYPE, &pza->psz_type)  );
                    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_calculated, SG_ZING_TEMPLATE__FUNCTION, &pza->psz_function)  );
                    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_calculated, SG_ZING_TEMPLATE__FIELD_FROM, &pza->psz_field_from)  );
                    pza->psz_field_to = psz_field_name;

                    SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_actions, psz_field_name, pza)  );
                }
            }
        }
    }

    *pprb = prb_actions;
    prb_actions = NULL;

    return;

fail:
    return;
}

void SG_zingtemplate__get_link_attributes__mine(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype_mine,
        const char* psz_link_name_mine,
        SG_zinglinkattributes** ppzla,
        SG_zinglinksideattributes** ppzlsa_mine,
        SG_zinglinksideattributes** ppzlsa_other
        )
{
    SG_zinglinkattributes* pzla = NULL;
    SG_zinglinksideattributes* pzlsa_mine = NULL;
    SG_zinglinksideattributes* pzlsa_other = NULL;
    SG_vhash* pvh_link = NULL;
    const char* psz_link_name = NULL;
    SG_bool b_mine_is_from = SG_FALSE;
    SG_vhash* pvh_from = NULL;
    SG_vhash* pvh_to = NULL;

    SG_ERR_CHECK(  SG_zingtemplate__find_link(pCtx, pztemplate, psz_rectype_mine, psz_link_name_mine, &psz_link_name, &b_mine_is_from, &pvh_link)  );

    if (!pvh_link)
    {
        *ppzla = NULL;
        *ppzlsa_mine = NULL;
        *ppzlsa_other = NULL;
        return;
    }

	SG_ERR_CHECK(  SG_alloc1(pCtx, pzla)  );
	SG_ERR_CHECK(  SG_alloc1(pCtx, pzlsa_mine)  );
	SG_ERR_CHECK(  SG_alloc1(pCtx, pzlsa_other)  );

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pzlsa_mine->prb_rectypes)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pzlsa_other->prb_rectypes)  );

    pzla->psz_link_name = (char*) psz_link_name;

    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_link, SG_ZING_TEMPLATE__FROM, &pvh_from)  );
    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_link, SG_ZING_TEMPLATE__TO, &pvh_to)  );

    if (b_mine_is_from)
    {
        pzlsa_mine->bFrom = SG_TRUE;
        pzlsa_other->bFrom = SG_FALSE;
        SG_ERR_CHECK(  SG_zingtemplate__fill_in_link_side_attributes(pCtx, pvh_from, pzlsa_mine)  );
        SG_ERR_CHECK(  SG_zingtemplate__fill_in_link_side_attributes(pCtx, pvh_to, pzlsa_other)  );
    }
    else
    {
        pzlsa_mine->bFrom = SG_FALSE;
        pzlsa_other->bFrom = SG_TRUE;

        SG_ERR_CHECK(  SG_zingtemplate__fill_in_link_side_attributes(pCtx, pvh_to, pzlsa_mine)  );
        SG_ERR_CHECK(  SG_zingtemplate__fill_in_link_side_attributes(pCtx, pvh_from, pzlsa_other)  );
    }

    *ppzla = pzla;
    pzla = NULL;

    *ppzlsa_mine = pzlsa_mine;
    pzlsa_mine = NULL;
    *ppzlsa_other = pzlsa_other;
    pzlsa_other = NULL;

    return;

fail:
    SG_zinglinksideattributes__free(pCtx, pzlsa_mine);
    SG_zinglinksideattributes__free(pCtx, pzlsa_other);
    SG_zinglinkattributes__free(pCtx, pzla);
    return;
}

void SG_zingtemplate__get_link_attributes(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_link_name,
        SG_zinglinkattributes** ppzla,
        SG_zinglinksideattributes** ppzlsa_from,
        SG_zinglinksideattributes** ppzlsa_to
        )
{
    SG_zinglinkattributes* pzla = NULL;
    SG_zinglinksideattributes* pzlsa_from = NULL;
    SG_zinglinksideattributes* pzlsa_to = NULL;
    SG_vhash* pvh_link = NULL;
    SG_vhash* pvh_from = NULL;
    SG_vhash* pvh_to = NULL;

    SG_ERR_CHECK(  SG_vhash__path__get__vhash(pCtx, pztemplate->pvh, &pvh_link, NULL, SG_ZING_TEMPLATE__DIRECTED_LINKTYPES, psz_link_name, NULL)  );

    if (!pvh_link)
    {
        *ppzla = NULL;
        *ppzlsa_from = NULL;
        *ppzlsa_to = NULL;
        return;
    }

	SG_ERR_CHECK(  SG_alloc1(pCtx, pzla)  );
	SG_ERR_CHECK(  SG_alloc1(pCtx, pzlsa_from)  );
	SG_ERR_CHECK(  SG_alloc1(pCtx, pzlsa_to)  );

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pzlsa_from->prb_rectypes)  );
    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pzlsa_to->prb_rectypes)  );

    pzla->psz_link_name = (char*) psz_link_name;

    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_link, SG_ZING_TEMPLATE__FROM, &pvh_from)  );
    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_link, SG_ZING_TEMPLATE__TO, &pvh_to)  );

    pzlsa_from->bFrom = SG_TRUE;
    pzlsa_to->bFrom = SG_FALSE;
    SG_ERR_CHECK(  SG_zingtemplate__fill_in_link_side_attributes(pCtx, pvh_from, pzlsa_from)  );
    SG_ERR_CHECK(  SG_zingtemplate__fill_in_link_side_attributes(pCtx, pvh_to, pzlsa_to)  );

    *ppzla = pzla;
    pzla = NULL;

    *ppzlsa_from = pzlsa_from;
    pzlsa_from = NULL;
    *ppzlsa_to = pzlsa_to;
    pzlsa_to = NULL;

    return;

fail:
    SG_zinglinksideattributes__free(pCtx, pzlsa_from);
    SG_zinglinksideattributes__free(pCtx, pzlsa_to);
    SG_zinglinkattributes__free(pCtx, pzla);
}

void SG_zingtemplate__get_json(
        SG_context* pCtx,
        SG_zingtemplate* pThis,
        SG_string** ppstr
        )
{
    SG_string* pstr = NULL;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
    SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pThis->pvh, pstr)  );

    *ppstr = pstr;
    pstr = NULL;

    return;

fail:
    return;
}

void SG_zingtemplate__get_vhash(SG_context* pCtx, SG_zingtemplate* pThis, SG_vhash ** ppResult)
{
  SG_UNUSED(pCtx);
    *ppResult = pThis->pvh;
}

void SG_zingtemplate__get_rectype_info(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        SG_zingrectypeinfo** pp
        )
{
    SG_zingrectypeinfo* pmi = NULL;
    SG_vhash* pvh_rectype = NULL;
    SG_bool b_has = SG_FALSE;
    const char* psz_merge_type = NULL;

    SG_ERR_CHECK(  SG_alloc1(pCtx, pmi)  );

    SG_ERR_CHECK(  SG_vhash__path__get__vhash(pCtx, pztemplate->pvh, &pvh_rectype, NULL, SG_ZING_TEMPLATE__RECTYPES, psz_rectype, NULL)  );

    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_rectype, SG_ZING_TEMPLATE__NO_RECID, &b_has)  );
    if (b_has)
    {
        SG_ERR_CHECK(  SG_vhash__get__bool(pCtx, pvh_rectype, SG_ZING_TEMPLATE__NO_RECID, &pmi->b_no_recid)  );
    }

	SG_ERR_CHECK(  SG_vhash__check__sz(pCtx, pvh_rectype, SG_ZING_TEMPLATE__MERGE_TYPE, &b_has, &psz_merge_type)  );

    if (psz_merge_type
            && (0 == strcmp("record", psz_merge_type))
       )
    {
        pmi->b_merge_type_is_record = SG_TRUE;
    }
    else
    {
        pmi->b_merge_type_is_record = SG_FALSE;
    }

    *pp = pmi;
    pmi = NULL;

    // fall thru

fail:
    SG_ZINGRECTYPEINFO_NULLFREE(pCtx, pmi);
}

void SG_zingrectypeinfo__free(SG_context* pCtx, SG_zingrectypeinfo* p)
{
    if (!p)
    {
        return;
    }

    SG_NULLFREE(pCtx, p);
}

// TODO the use of these globals in the following function is not threadsafe
SG_rbtree* g_template_cache_map;
SG_rbtree* g_template_cache_store;

void SG_zing__get_template__hid_template(
        SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_hid_template,
        SG_zingtemplate** ppzt
        )
{
    char *psz_repoid = NULL;
    SG_zingtemplate* pzt = NULL;
    SG_bool b = SG_FALSE;

    if (!g_template_cache_store)
    {
        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &g_template_cache_store)  );
    }

    b = SG_FALSE;
    SG_ERR_CHECK(  SG_rbtree__find(pCtx, g_template_cache_store, psz_hid_template, &b, (void**) &pzt)  );

    if (!b)
    {
        SG_ERR_CHECK(  sg_zing__load_template__hid_template(pCtx, pRepo, psz_hid_template, &pzt)  );
        SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, g_template_cache_store, psz_hid_template, pzt)  );
    }

    *ppzt = pzt;
    pzt = NULL;

fail:
    SG_NULLFREE(pCtx, psz_repoid);
}

void SG_zing__get_template__csid(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_csid,
        SG_zingtemplate** ppzt
        )
{
    char buf_dagnum[SG_DAGNUM__BUF_MAX__DEC];
    char buf_cache_key[SG_GID_ACTUAL_LENGTH + SG_DAGNUM__BUF_MAX__DEC + SG_HID_MAX_BUFFER_LENGTH + 10];
    char *psz_repoid = NULL;
    SG_zingtemplate* pzt = NULL;
    SG_bool b = SG_FALSE;
    char* psz_hid_template = NULL;
    char* psz_hid_template_freeme = NULL;

    if (!g_template_cache_map)
    {
        SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &g_template_cache_map)  );
    }

    SG_ERR_CHECK(  SG_dagnum__to_sz__decimal(pCtx, iDagNum, buf_dagnum, sizeof(buf_dagnum))  );
	SG_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pRepo, &psz_repoid)  );
    SG_ERR_CHECK(  SG_sprintf(pCtx, buf_cache_key, sizeof(buf_cache_key), "%s.%s.%s",
                psz_repoid,
                buf_dagnum,
                psz_csid)  );
    SG_ERR_CHECK(  SG_rbtree__find(pCtx, g_template_cache_map, buf_cache_key, &b, (void**) &psz_hid_template)  );
    if (!b)
    {
        SG_ERR_CHECK(  sg_zing__get_dbtop(pCtx, pRepo, psz_csid, &psz_hid_template_freeme, NULL)  );
        psz_hid_template = psz_hid_template_freeme;
        SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, g_template_cache_map, buf_cache_key, psz_hid_template)  );
    }

    SG_ERR_CHECK(  SG_zing__get_template__hid_template(pCtx, pRepo, psz_hid_template, &pzt)  );

    *ppzt = pzt;
    pzt = NULL;

fail:
    SG_NULLFREE(pCtx, psz_repoid);
    SG_NULLFREE(pCtx, psz_hid_template_freeme);
}

void SG_zing__now_free_all_cached_templates(
        SG_context* pCtx
        )
{
    SG_RBTREE_NULLFREE(pCtx, g_template_cache_map);
    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, g_template_cache_store, (SG_free_callback*) SG_zingtemplate__free);
}


