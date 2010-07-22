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

enum token
{
    TOK_UNKNOWN,
    TOK_IDENT,
    TOK_COMMA,
    TOK_ASC,
    TOK_DESC,
    TOK_WHEN,
    TOK_END
};

struct sg_sort_parser
{
    SG_zingtemplate* pzt;
    const char* psz_rectype;
    SG_uint32 next_token;
    const char* pcur;
    const char* token;
    SG_uint32 len_token;
    SG_varray* pva_top;
};

#define CURCHAR (*(pw->pcur))
#define NEXTCHAR (pw->pcur++)

static void w_token(
        SG_context* pCtx,
        struct sg_sort_parser* pw
        )
{
    // skip whitespace
    while (
            (CURCHAR)
            && (
                (' ' == CURCHAR)
                || ('\t' == CURCHAR)
                || ('\n' == CURCHAR)
                || ('\r' == CURCHAR)
               )
          )
    {
        pw->pcur++;
    }

    // token starts here
    pw->token = pw->pcur;
    if (0 == CURCHAR)
    {
        pw->next_token = TOK_END;
    }
    else if ('#' == CURCHAR)
    {
        SG_bool bok = SG_FALSE;

        NEXTCHAR;
        if ('A' == CURCHAR)
        {
            NEXTCHAR;
            if ('S' == CURCHAR)
            {
                NEXTCHAR;
                if ('C' == CURCHAR)
                {
                    NEXTCHAR;
                    bok = SG_TRUE;
                    pw->next_token = TOK_ASC;
                }
            }
        }
        else if ('D' == CURCHAR)
        {
            NEXTCHAR;
            if ('E' == CURCHAR)
            {
                NEXTCHAR;
                if ('S' == CURCHAR)
                {
                    NEXTCHAR;
                    if ('C' == CURCHAR)
                    {
                        NEXTCHAR;
                        bok = SG_TRUE;
                        pw->next_token = TOK_DESC;
                    }
                }
            }
        }
        else if ('W' == CURCHAR)
        {
            NEXTCHAR;
            if ('H' == CURCHAR)
            {
                NEXTCHAR;
                if ('E' == CURCHAR)
                {
                    NEXTCHAR;
                    if ('N' == CURCHAR)
                    {
                        NEXTCHAR;
                        bok = SG_TRUE;
                        pw->next_token = TOK_WHEN;
                    }
                }
            }
        }
        if (!bok)
        {
            SG_ERR_THROW2(  SG_ERR_ZING_SORT_PARSE_ERROR,
                    (pCtx, "Error at %s", pw->pcur)
                    );
        }
    }
    else if (',' == CURCHAR)
    {
        NEXTCHAR;
        pw->next_token = TOK_COMMA;
    }
    else if (
            ( (CURCHAR >= 'a') && (CURCHAR <= 'z') )
            || ( (CURCHAR >= 'A') && (CURCHAR <= 'Z') )
            || ('_' == CURCHAR)
            )
    {
        NEXTCHAR;
        while (
            ( (CURCHAR >= 'a') && (CURCHAR <= 'z') )
            ||( (CURCHAR >= 'A') && (CURCHAR <= 'Z') )
            ||( (CURCHAR >= '0') && (CURCHAR <= '9') )
            || ('_' == CURCHAR)
            )
        {
            NEXTCHAR;
        }
        pw->next_token = TOK_IDENT;
    }
    else
    {
        SG_ERR_THROW2(  SG_ERR_ZING_SORT_PARSE_ERROR,
                (pCtx, "Error at %s", pw->pcur)
                );
    }

    pw->len_token = pw->pcur - pw->token;

fail:
    return;
}

static void w_sortby__when(
        SG_context* pCtx,
        struct sg_sort_parser* pw
        )
{
    SG_vhash* pvh = NULL;

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh)  );

    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, SG_DBNDX_SORT__NAME, "#WHEN")  );
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, SG_DBNDX_SORT__TYPE, "numeric")  );

    SG_ERR_CHECK(  w_token(pCtx, pw)  );
    if (TOK_ASC == pw->next_token)
    {
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, SG_DBNDX_SORT__DIRECTION, "asc")  );
        SG_ERR_CHECK(  w_token(pCtx, pw)  );
    }
    else if (TOK_DESC == pw->next_token)
    {
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, SG_DBNDX_SORT__DIRECTION, "desc")  );
        SG_ERR_CHECK(  w_token(pCtx, pw)  );
    }

    SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pw->pva_top, &pvh)  );

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
}

static void w_sortby__ident(
        SG_context* pCtx,
        struct sg_sort_parser* pw
        )
{
    SG_vhash* pvh = NULL;
    SG_zingfieldattributes* pzfa = NULL;
    char buf_name[SG_ZING_MAX_FIELD_NAME_LENGTH + 1];

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh)  );

    if (pw->len_token > SG_ZING_MAX_FIELD_NAME_LENGTH)
    {
        SG_ERR_THROW2(  SG_ERR_ZING_SORT_PARSE_ERROR,
                (pCtx, "Name too long to be a field name", pw->pcur)
                );
    }

    memcpy(buf_name, pw->token, pw->len_token);
    buf_name[pw->len_token] = 0;

    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pw->pzt, pw->psz_rectype, buf_name, &pzfa)  );
    if (!pzfa)
    {
        SG_ERR_THROW2(  SG_ERR_ZING_SORT_PARSE_ERROR,
                (pCtx, "%s is not a field name", buf_name)
                );
    }

    SG_ERR_CHECK(  SG_vhash__add__string__buflen(pCtx, pvh, SG_DBNDX_SORT__NAME, pw->token, pw->len_token)  );

    switch (pzfa->type)
    {
        case SG_ZING_TYPE__STRING:
            if (pzfa->v._string.sort_by_allowed)
            {
                SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, SG_DBNDX_SORT__TYPE, "numeric")  );
            }
            break;

        case SG_ZING_TYPE__INT:
        case SG_ZING_TYPE__DATETIME:
            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, SG_DBNDX_SORT__TYPE, "numeric")  );
            break;
    }

    SG_ERR_CHECK(  w_token(pCtx, pw)  );
    if (TOK_ASC == pw->next_token)
    {
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, SG_DBNDX_SORT__DIRECTION, "asc")  );
        SG_ERR_CHECK(  w_token(pCtx, pw)  );
    }
    else if (TOK_DESC == pw->next_token)
    {
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, SG_DBNDX_SORT__DIRECTION, "desc")  );
        SG_ERR_CHECK(  w_token(pCtx, pw)  );
    }

    SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pw->pva_top, &pvh)  );

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
}

static void w_sortby(
        SG_context* pCtx,
        struct sg_sort_parser* pw
        )
{
    if (TOK_IDENT == pw->next_token)
    {
        SG_ERR_CHECK(  w_sortby__ident(pCtx, pw)  );
    }
    else if (TOK_WHEN == pw->next_token)
    {
        SG_ERR_CHECK(  w_sortby__when(pCtx, pw)  );
    }
    else
    {
        SG_ERR_THROW2(  SG_ERR_ZING_SORT_PARSE_ERROR,
                (pCtx, "Error at %s", pw->pcur)
                );
    }

fail:
    ;
}

static void sg_where__parse(
        SG_context* pCtx,
        struct sg_sort_parser* pw
        )
{
    SG_ERR_CHECK(  w_token(pCtx, pw)  );

    SG_ERR_CHECK(  w_sortby(pCtx, pw)  );
    if (TOK_COMMA == pw->next_token)
    {
        SG_ERR_CHECK(  w_token(pCtx, pw)  );
        SG_ERR_CHECK(  w_sortby(pCtx, pw)  );
    }
    if (TOK_COMMA == pw->next_token)
    {
        SG_ERR_CHECK(  w_token(pCtx, pw)  );
        SG_ERR_CHECK(  w_sortby(pCtx, pw)  );
    }

    if (TOK_END != pw->next_token)
    {
        SG_ERR_THROW2(  SG_ERR_ZING_SORT_PARSE_ERROR,
                (pCtx, "Error at %s", pw->token)
                );
    }

fail:
    return;
}

void sg_zing__query__parse_sort(
        SG_context* pCtx,
        SG_zingtemplate* pzt,
        const char* psz_rectype,
        const char* psz_sort,
        SG_varray** pp
        )
{
    struct sg_sort_parser w;

    memset(&w, 0, sizeof(w));
    w.pzt = pzt;
    w.psz_rectype = psz_rectype;
    w.pcur = psz_sort;

    // pre-init the top varray
    SG_ERR_CHECK(  SG_varray__alloc__params(pCtx, &w.pva_top, 3, NULL, NULL)  );

    // parse
    SG_ERR_CHECK(  sg_where__parse(pCtx, &w)  );

    *pp = w.pva_top;
    w.pva_top = NULL;

    // fall thru

fail:
    SG_VARRAY_NULLFREE(pCtx, w.pva_top);
}

