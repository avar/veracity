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

#define SG_DAGNUM__NAME__AUDIT_PREFIX "audit-"
#define SG_DAGNUM__NAME__AUDIT_PREFIX_LEN 6

static struct
{
    SG_uint32 iDagNum;
    const char* psz_name;
} g_dagnum_names[] =
{
    {  SG_DAGNUM__VERSION_CONTROL,	"version_control" },
    {  SG_DAGNUM__VC_COMMENTS,		"vc_comments" },
    {  SG_DAGNUM__WORK_ITEMS,		"work_items" },
    {  SG_DAGNUM__WIKI,				"wiki" },
    {  SG_DAGNUM__FORUM,			"forum" },
    {  SG_DAGNUM__SUP,				"sup" },
    {  SG_DAGNUM__VC_STAMPS,		"vc_stamps" },
    {  SG_DAGNUM__VC_TAGS,			"vc_tags" },
    {  SG_DAGNUM__USERS,			"users" },
    {  SG_DAGNUM__BRANCHES,			"branches" },
    {  SG_DAGNUM__TESTING__DB,		"test1" },
};

void SG_dagnum__from_sz(
        SG_context* pCtx,
		const char* pszDagnum,
        SG_uint32* piResult)
{
    SG_uint32 count_names = sizeof(g_dagnum_names) / sizeof(g_dagnum_names[0]);
    SG_uint32 i = 0;
	SG_bool isAuditName = SG_FALSE;
	char bufNonAuditName[SG_DAGNUM__BUF_MAX__NAME];

	if ( 0 == strncmp(pszDagnum, SG_DAGNUM__NAME__AUDIT_PREFIX, SG_DAGNUM__NAME__AUDIT_PREFIX_LEN))
	{
		isAuditName = SG_TRUE;
		SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx, bufNonAuditName, sizeof(bufNonAuditName), (pszDagnum+SG_DAGNUM__NAME__AUDIT_PREFIX_LEN))  );
	}
	else
		SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx, bufNonAuditName, sizeof(bufNonAuditName), pszDagnum)  );

    for (i=0; i<count_names; i++)
    {
        if (0 == strcmp(bufNonAuditName, g_dagnum_names[i].psz_name))
        {
            *piResult = g_dagnum_names[i].iDagNum;
			if (isAuditName)
				*piResult |= SG_DAGNUM__BIT__AUDIT;
            return;
        }
    }

    // if the name lookup fails, we parse it as a hex string
    SG_ERR_CHECK_RETURN(  SG_hex__parse_hex_uint32(pCtx, pszDagnum, strlen(pszDagnum), piResult)  );
}

void SG_dagnum__to_name(
        SG_context* pCtx,
		SG_uint32 iDagNum,
        char* buf,
        SG_uint32 iBufSize
        )
{
    SG_uint32 count_names = sizeof(g_dagnum_names) / sizeof(g_dagnum_names[0]);
    SG_uint32 i = 0;
    char tmp[128];
    SG_uint32 base_dagnum = 0;
    SG_bool b_audit = SG_FALSE;

	SG_ASSERT(iBufSize >= SG_DAGNUM__BUF_MAX__NAME);

    if (SG_DAGNUM__IS_AUDIT(iDagNum))
    {
        base_dagnum = iDagNum & ~SG_DAGNUM__BIT__AUDIT;
        b_audit = SG_TRUE;
    }
    else
    {
        base_dagnum = iDagNum;
    }

    tmp[0] = 0;
    for (i=0; i<count_names; i++)
    {
        if (base_dagnum == g_dagnum_names[i].iDagNum)
        {
            SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx, tmp, sizeof(tmp), g_dagnum_names[i].psz_name)  );
            break;
        }
    }

    if (tmp[0])
    {
        if (b_audit)
        {
            SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx, buf, iBufSize, SG_DAGNUM__NAME__AUDIT_PREFIX)  );
            SG_ERR_CHECK_RETURN(  SG_strcat(pCtx, buf, iBufSize, tmp)  );
        }
        else
        {
            SG_ERR_CHECK_RETURN(  SG_strcpy(pCtx, buf, iBufSize, tmp)  );
        }
    }
    else
    {
        // if there is no name, we just return a hex string
        SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx, buf, iBufSize, "%08x", iDagNum)  );
    }

}

void SG_dagnum__to_sz__decimal(
        SG_context* pCtx,
		SG_uint32 iDagNum,
        char* buf,
        SG_uint32 iBufSize
        )
{
	SG_ASSERT(iBufSize >= SG_DAGNUM__BUF_MAX__DEC);
    SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx, buf, iBufSize, "%d", iDagNum)  );
}

void SG_dagnum__from_sz__decimal(
        SG_context* pCtx,
		const char* buf,
        SG_uint32* piResult
        )
{
    SG_ERR_CHECK_RETURN(  SG_uint32__parse__strict(pCtx, piResult, buf)  );
}

