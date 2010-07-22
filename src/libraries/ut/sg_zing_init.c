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

extern unsigned char sg_ztemplate__sup_json[];
extern unsigned char sg_ztemplate__stamps_json[];
extern unsigned char sg_ztemplate__comments_json[];
extern unsigned char sg_ztemplate__tags_json[];
extern unsigned char sg_ztemplate__users_json[];
extern unsigned char sg_ztemplate__wit_json[];
extern unsigned char sg_ztemplate__branches_json[];

// to add a template here, add it first as a json
// file to the sgtemplates library

struct
{
    unsigned char* pjson;
    SG_uint32      dagnum;
} templates[] =
{
    { sg_ztemplate__sup_json, SG_DAGNUM__SUP },
    { sg_ztemplate__stamps_json, SG_DAGNUM__VC_STAMPS },
    { sg_ztemplate__comments_json, SG_DAGNUM__VC_COMMENTS },
    { sg_ztemplate__tags_json, SG_DAGNUM__VC_TAGS },
    { sg_ztemplate__users_json, SG_DAGNUM__USERS },
    { sg_ztemplate__wit_json, SG_DAGNUM__WORK_ITEMS },
    // { sg_ztemplate__branches_json, SG_DAGNUM__BRANCHES },
};

void my_strip_comments(
    SG_context* pCtx,
    char* pjson,
    SG_string** ppstr
    )
{
    char *p = pjson;
    SG_string* pstr = NULL;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );

    while (*p)
    {
        char *q = NULL;
        char *r = NULL;

        q = p;
        while (*q)
        {
            if (13 == *q)
            {
                q++;
                if (10 == *q)
                {
                    q++;
                }
                break;
            }
            else if (10 == *q)
            {
                q++;
                break;
            }
            q++;
        }

        // now q points to the char after the EOL, or to the zero terminator
        r = p;
        while (
                (r < q)
                && (
                    (' ' == *r)
                    || ('\t' == *r)
                   )
              )
        {
            r++;
        }

        if (
                *r
                && ('/' == r[0])
                && ('/' == r[1])
           )
        {
            // this is a comment.  skip it.
        }
        else
        {
            SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pstr, (void*) p, q - p)  );
        }
        p = q;
    }

    *ppstr = pstr;
    pstr = NULL;

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
}

static void do_one_template(
    SG_context* pCtx,
	SG_repo* pRepo,
    SG_uint32 iDagNum,
    SG_audit* pq,
    unsigned char* pjson
    )
{
    SG_vhash* pvh_template = NULL;
    SG_changeset* pcs = NULL;
    SG_dagnode* pdn = NULL;
    SG_zingtx* ptx = NULL;
    SG_string* pstr = NULL;

    // now the main dag
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, iDagNum, pq->who_szUserId, NULL, &ptx)  );

    SG_ERR_CHECK(  my_strip_comments(pCtx, (char*) pjson, &pstr)  );
    SG_ERR_CHECK(  SG_vhash__alloc__from_json(pCtx, &pvh_template, SG_string__sz(pstr)));

    SG_ERR_CHECK(  SG_zingtx__store_template(pCtx, ptx, &pvh_template)  );

    SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, pq->when_int64, &ptx, &pcs, &pdn, NULL)  );
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_DAGNODE_NULLFREE(pCtx, pdn);

    // fall thru

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
}


void sg_zing__init_new_repo(
    SG_context* pCtx,
	SG_repo* pRepo
    )
{
    SG_uint32 count = sizeof(templates) / sizeof(templates[0]);
    SG_uint32 i;
    SG_audit q;

    SG_ERR_CHECK_RETURN(  SG_audit__init__nobody(pCtx, &q, SG_AUDIT__WHEN__NOW)  );

    for (i=0; i<count; i++)
    {
        SG_ERR_CHECK_RETURN(  do_one_template(pCtx, pRepo, templates[i].dagnum, &q, templates[i].pjson)  );
    }
}

void sg_zing__validate_templates(
    SG_context* pCtx
    )
{
    SG_uint32 count = sizeof(templates) / sizeof(templates[0]);
    SG_uint32 i;
    SG_vhash* pvh = NULL;
    SG_string* pstr = NULL;
    SG_uint32 version = 0;

    for (i=0; i<count; i++)
    {
        SG_ERR_CHECK(  my_strip_comments(pCtx, (char*) templates[i].pjson, &pstr)  );
        SG_ERR_CHECK(  SG_vhash__alloc__from_json(pCtx, &pvh, SG_string__sz(pstr)));
        SG_STRING_NULLFREE(pCtx, pstr);
        SG_ERR_CHECK(  SG_zingtemplate__validate(pCtx, pvh, &version)  );
        SG_VHASH_NULLFREE(pCtx, pvh);
    }

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_VHASH_NULLFREE(pCtx, pvh);
}

