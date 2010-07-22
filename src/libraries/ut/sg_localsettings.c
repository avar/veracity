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
 * @file sg_localsettings.c
 */

#include <sg.h>

void SG_localsettings__update__sz(SG_context * pCtx, const char * psz_path, const char * pValue)
{
	SG_jsondb* p = NULL;
    SG_string* pstr_path = NULL;

	SG_ASSERT(pCtx);
	SG_NONEMPTYCHECK_RETURN(psz_path);

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_path)  );
    if ('/' == psz_path[0])
    {
        SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s", psz_path)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s/%s", SG_LOCALSETTING__SCOPE__MACHINE, psz_path)  );
    }

    SG_ERR_CHECK(  SG_closet__get_localsettings(pCtx, &p)  );
	SG_ERR_CHECK(  SG_jsondb__update__string__sz(pCtx, p, SG_string__sz(pstr_path), SG_TRUE, pValue)  );

fail:
    SG_STRING_NULLFREE(pCtx, pstr_path);
	SG_JSONDB_NULLFREE(pCtx, p);
}

void SG_localsettings__descriptor__update__sz(
	SG_context * pCtx,
	const char * psz_descriptor_name,
	const char * psz_path,
	const char * pValue)
{
	SG_jsondb* p = NULL;
	SG_string* pstr_path = NULL;

	SG_ASSERT(pCtx);
	SG_NONEMPTYCHECK_RETURN(psz_descriptor_name);
	SG_NONEMPTYCHECK_RETURN(psz_path);
	SG_ARGCHECK_RETURN('/' != psz_path[0], psz_path);

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_path)  );
	SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s/%s/%s",
		SG_LOCALSETTING__SCOPE__INSTANCE, psz_descriptor_name, psz_path)  );

	SG_ERR_CHECK(  SG_closet__get_localsettings(pCtx, &p)  );
	SG_ERR_CHECK(  SG_jsondb__update__string__sz(pCtx, p, SG_string__sz(pstr_path), SG_TRUE, pValue)  );

fail:
	SG_STRING_NULLFREE(pCtx, pstr_path);
	SG_JSONDB_NULLFREE(pCtx, p);
}

void SG_localsettings__update__varray(SG_context * pCtx, const char * psz_path, const SG_varray * pValue)
{
	SG_jsondb* p = NULL;
    SG_string* pstr_path = NULL;

    SG_ASSERT(pCtx);
    SG_NONEMPTYCHECK_RETURN(psz_path);
    SG_NULLARGCHECK_RETURN(pValue);

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_path)  );
    if ('/' == psz_path[0])
    {
        SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s", psz_path)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s/%s", SG_LOCALSETTING__SCOPE__MACHINE, psz_path)  );
    }

    SG_ERR_CHECK(  SG_closet__get_localsettings(pCtx, &p)  );

	SG_ERR_CHECK(  SG_jsondb__update__varray(pCtx, p, SG_string__sz(pstr_path), SG_TRUE, pValue)  );

fail:
	SG_JSONDB_NULLFREE(pCtx, p);
    SG_STRING_NULLFREE(pCtx, pstr_path);
}

void SG_localsettings__varray__empty(SG_context * pCtx, const char* psz_path)
{
    SG_varray* pva = NULL;

    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva)  );
    SG_ERR_CHECK(  SG_localsettings__update__varray(pCtx, psz_path, pva)  );

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
}

void SG_localsettings__reset(SG_context * pCtx, const char* psz_path)
{
    SG_jsondb* p = NULL;
    SG_string* pstr_path = NULL;

    SG_ASSERT(pCtx);
    SG_NONEMPTYCHECK_RETURN(psz_path);

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_path)  );
    if ('/' == psz_path[0])
    {
        SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s", psz_path)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s/%s", SG_LOCALSETTING__SCOPE__MACHINE, psz_path)  );
    }

    SG_ERR_CHECK(  SG_closet__get_localsettings(pCtx, &p)  );
	SG_jsondb__remove(pCtx, p, SG_string__sz(pstr_path));
    SG_ERR_CHECK_CURRENT_DISREGARD(SG_ERR_NOT_FOUND);

fail:
	SG_JSONDB_NULLFREE(pCtx, p);
    SG_STRING_NULLFREE(pCtx, pstr_path);
}

const char * DEFAULT_IGNORES[] = {"Debug", "debug", "Release", "release", "temp", "~", NULL};
const char * DEFAULT_DIFF_ARGUMENTS[] = {"--ro1", "--title1="SG_LOCALSETTING__DIFF_SUBS__LEFT_LABEL, "--title2="SG_LOCALSETTING__DIFF_SUBS__RIGHT_LABEL, SG_LOCALSETTING__DIFF_SUBS__LEFT_PATH, SG_LOCALSETTING__DIFF_SUBS__RIGHT_PATH, NULL};

const struct localsetting_factory_default
{
    const char* psz_path_partial;
    SG_uint16 type;
    const char* psz_val;
    const char** pasz_array;
} g_factory_defaults[] = {
    {
        "answer/life/universe/everything",
        SG_VARIANT_TYPE_SZ,
        "42",
        NULL
    },
    {
		"diff/2/arguments",
		SG_VARIANT_TYPE_VARRAY,
		NULL,
		DEFAULT_DIFF_ARGUMENTS
	},
    {
        SG_LOCALSETTING__NEWREPO_HASHMETHOD,
        SG_VARIANT_TYPE_SZ,
        "SHA1/160",
        NULL
    },
    {
        SG_LOCALSETTING__NEWREPO_DRIVER,
        SG_VARIANT_TYPE_SZ,
        SG_RIDESC_STORAGE__DEFAULT,
        NULL
    },
    {
        SG_LOCALSETTING__IGNORES,
        SG_VARIANT_TYPE_VARRAY,
        NULL,
        DEFAULT_IGNORES
    },
    {
        SG_LOCALSETTING__LOG_LEVEL,
        SG_VARIANT_TYPE_SZ,
        "normal",
        NULL
    },
};

static void SG_localsettings__factory__get_nth__variant(
    SG_context* pCtx,
    SG_uint32 i,
    SG_variant** ppv
    )
{
	SG_variant* pv = NULL;

    if (SG_VARIANT_TYPE_SZ == g_factory_defaults[i].type)
    {
        SG_ERR_CHECK(  SG_alloc1(pCtx, pv)  );
        pv->type = g_factory_defaults[i].type;
        SG_ERR_CHECK(  SG_strdup(pCtx, g_factory_defaults[i].psz_val, (char**) &pv->v.val_sz)  );
    }
    else if (SG_VARIANT_TYPE_VARRAY == g_factory_defaults[i].type)
    {
        const char** pp_el = NULL;

        SG_ERR_CHECK(  SG_alloc1(pCtx, pv)  );
        pv->type = g_factory_defaults[i].type;
        SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pv->v.val_varray)  );

        pp_el = g_factory_defaults[i].pasz_array;
        while (*pp_el)
        {
            SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pv->v.val_varray, *pp_el)  );
            pp_el++;
        }
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
    }

    *ppv = pv;
    pv = NULL;

fail:
    SG_VARIANT_NULLFREE(pCtx, pv);
}

static void SG_localsettings__factory__get__variant(
    SG_context* pCtx,
    const char* psz_path,
    SG_variant** ppv
    )
{
    SG_uint32 i = 0;
	SG_variant* pv = NULL;

    for (i=0; i<(sizeof(g_factory_defaults) / sizeof(struct localsetting_factory_default)); i++)
    {
        if (0 == strcmp(psz_path, g_factory_defaults[i].psz_path_partial))
        {
            SG_ERR_CHECK(  SG_localsettings__factory__get_nth__variant(pCtx, i, &pv)  );
            break;
        }
    }

    *ppv = pv;
    pv = NULL;

fail:
    SG_VARIANT_NULLFREE(pCtx, pv);
}

void SG_localsettings__factory__list__vhash(
    SG_context* pCtx,
    SG_vhash** ppvh
    )
{
    SG_uint32 i = 0;
    SG_vhash* pvh = NULL;
    SG_varray* pva = NULL;

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh)  );
    for (i=0; i<(sizeof(g_factory_defaults) / sizeof(struct localsetting_factory_default)); i++)
    {
        if (SG_VARIANT_TYPE_SZ == g_factory_defaults[i].type)
        {
            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, g_factory_defaults[i].psz_path_partial, g_factory_defaults[i].psz_val)  );
        }
        else if (SG_VARIANT_TYPE_VARRAY == g_factory_defaults[i].type)
        {
            const char** pp_el = NULL;

            SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva)  );

            pp_el = g_factory_defaults[i].pasz_array;
            while (*pp_el)
            {
                SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, *pp_el)  );
                pp_el++;
            }
            SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh, g_factory_defaults[i].psz_path_partial, &pva)  );
        }
        else
        {
            SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
        }

        }

    *ppvh = pvh;
    pvh = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_localsettings__descriptor__get__sz(
	SG_context * pCtx,
	const char * psz_descriptor_name,
	const char * psz_path,
	char ** ppszValue)
{
	SG_jsondb* p = NULL;
	SG_string* pstr_path = NULL;

	SG_ASSERT(pCtx);
	SG_NONEMPTYCHECK_RETURN(psz_descriptor_name);
	SG_NONEMPTYCHECK_RETURN(psz_path);
	SG_ARGCHECK_RETURN('/' != psz_path[0], psz_path);

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_path)  );
	SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s/%s/%s",
		SG_LOCALSETTING__SCOPE__INSTANCE, psz_descriptor_name, psz_path)  );

	SG_ERR_CHECK(  SG_closet__get_localsettings(pCtx, &p)  );
	SG_ERR_CHECK(  SG_jsondb__get__sz(pCtx, p, SG_string__sz(pstr_path), ppszValue)  );

fail:
	SG_STRING_NULLFREE(pCtx, pstr_path);
	SG_JSONDB_NULLFREE(pCtx, p);
}

void SG_localsettings__get__variant(SG_context * pCtx, const char * psz_path, SG_repo* pRepo, SG_variant** ppv, SG_string** ppstr_where_found)
{
    SG_jsondb* p = NULL;
    SG_string* pstr_path = NULL;
    SG_variant* pv = NULL;
    char* psz_repo_id = NULL;
    char* psz_admin_id = NULL;
    const char* psz_ref_descriptor_name = NULL;

    SG_ASSERT(pCtx);
    SG_NONEMPTYCHECK_RETURN(psz_path);

    SG_ERR_CHECK(  SG_closet__get_localsettings(pCtx, &p)  );

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_path)  );
    if ('/' == psz_path[0])
    {
        SG_bool b = SG_FALSE;

        SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s", psz_path)  );
        SG_ERR_CHECK(  SG_jsondb__has(pCtx, p, SG_string__sz(pstr_path), &b)  );
        if (b)
        {
            SG_ERR_CHECK(  SG_jsondb__get__variant(pCtx, p, SG_string__sz(pstr_path), &pv)  );
        }
    }
    else
    {
        SG_bool b_has_val = SG_FALSE;

        // try the instance of the repo
        if (!b_has_val && pRepo)
        {
            SG_ERR_CHECK(  SG_repo__get_descriptor_name(pCtx, pRepo, &psz_ref_descriptor_name)  );

            SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s/%s/%s",
                        SG_LOCALSETTING__SCOPE__INSTANCE,
                        psz_ref_descriptor_name,
                        psz_path
                        )  );
            SG_ERR_CHECK(  SG_jsondb__has(pCtx, p, SG_string__sz(pstr_path), &b_has_val)  );
            if (b_has_val)
            {
                SG_ERR_CHECK(  SG_jsondb__get__variant(pCtx, p, SG_string__sz(pstr_path), &pv)  );
            }
        }

        // then the repo
        if (!b_has_val && pRepo)
        {
            SG_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pRepo, &psz_repo_id)  );

            SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s/%s/%s",
                        SG_LOCALSETTING__SCOPE__REPO,
                        psz_repo_id,
                        psz_path
                        )  );
            SG_ERR_CHECK(  SG_jsondb__has(pCtx, p, SG_string__sz(pstr_path), &b_has_val)  );
            if (b_has_val)
            {
                SG_ERR_CHECK(  SG_jsondb__get__variant(pCtx, p, SG_string__sz(pstr_path), &pv)  );
            }
        }

        // then the admin group of repos
        if (!b_has_val && pRepo)
        {
            SG_ERR_CHECK(  SG_repo__get_admin_id(pCtx, pRepo, &psz_admin_id)  );

            SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s/%s/%s",
                        SG_LOCALSETTING__SCOPE__ADMIN,
                        psz_admin_id,
                        psz_path
                        )  );
            SG_ERR_CHECK(  SG_jsondb__has(pCtx, p, SG_string__sz(pstr_path), &b_has_val)  );
            if (b_has_val)
            {
                SG_ERR_CHECK(  SG_jsondb__get__variant(pCtx, p, SG_string__sz(pstr_path), &pv)  );
            }
        }

        // then the machine
        if (!b_has_val)
        {
            SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s/%s", SG_LOCALSETTING__SCOPE__MACHINE, psz_path)  );
            SG_ERR_CHECK(  SG_jsondb__has(pCtx, p, SG_string__sz(pstr_path), &b_has_val)  );
            if (b_has_val)
            {
                SG_ERR_CHECK(  SG_jsondb__get__variant(pCtx, p, SG_string__sz(pstr_path), &pv)  );
            }
        }

        // then the factory default
        if (!b_has_val)
        {
            SG_STRING_NULLFREE(pCtx, pstr_path);
            SG_ERR_CHECK(  SG_localsettings__factory__get__variant(pCtx, psz_path, &pv)  );
        }
    }

    *ppv = pv;
    pv = NULL;

    if (ppstr_where_found)
    {
        *ppstr_where_found = pstr_path;
        pstr_path = NULL;
    }

fail:
    SG_NULLFREE(pCtx, psz_repo_id);
    SG_NULLFREE(pCtx, psz_admin_id);
    SG_VARIANT_NULLFREE(pCtx, pv);
    SG_STRING_NULLFREE(pCtx, pstr_path);
    SG_JSONDB_NULLFREE(pCtx, p);
}

void SG_localsettings__get__sz(SG_context * pCtx, const char * psz_path, SG_repo* pRepo, char** ppsz, SG_string** ppstr_where_found)
{
    char* psz_val = NULL;
    SG_string* pstr_path = NULL;
    SG_variant* pv = NULL;

    SG_ERR_CHECK(  SG_localsettings__get__variant(pCtx, psz_path, pRepo, &pv, &pstr_path)  );
    if (pv)
    {
        if (SG_VARIANT_TYPE_SZ == pv->type)
        {
            psz_val = (char*) pv->v.val_sz;
            pv->v.val_sz = NULL;
        }
    }

    *ppsz = psz_val;
    psz_val = NULL;

    if (ppstr_where_found)
    {
        *ppstr_where_found = pstr_path;
        pstr_path = NULL;
    }

fail:
    SG_VARIANT_NULLFREE(pCtx, pv);
    SG_STRING_NULLFREE(pCtx, pstr_path);
    SG_NULLFREE(pCtx, psz_val);
}

void SG_localsettings__get__varray(SG_context * pCtx, const char * psz_path, SG_repo* pRepo, SG_varray** ppva, SG_string** ppstr_where_found)
{
    SG_varray* pva = NULL;
    SG_string* pstr_path = NULL;
    SG_variant* pv = NULL;

    SG_ERR_CHECK(  SG_localsettings__get__variant(pCtx, psz_path, pRepo, &pv, &pstr_path)  );
    if (pv)
    {
        if (SG_VARIANT_TYPE_VARRAY == pv->type)
        {
            pva = pv->v.val_varray;
            pv->v.val_varray = NULL;
        }
    }

    *ppva = pva;
    pva = NULL;

    if (ppstr_where_found)
    {
        *ppstr_where_found = pstr_path;
        pstr_path = NULL;
    }

fail:
    SG_VARIANT_NULLFREE(pCtx, pv);
    SG_STRING_NULLFREE(pCtx, pstr_path);
    SG_VARRAY_NULLFREE(pCtx, pva);
}

void SG_localsettings__list__vhash(SG_context * pCtx, SG_vhash** ppResult)
{
	SG_jsondb* p = NULL;

    SG_ERR_CHECK(  SG_closet__get_localsettings(pCtx, &p)  );
	SG_ERR_CHECK(  SG_jsondb__get__vhash(pCtx, p, "/", ppResult)  );

	/* fall through */
fail:
	SG_JSONDB_NULLFREE(pCtx, p);
}

void SG_localsettings__varray__append(SG_context * pCtx, const char* psz_path, const char* pValue)
{
	SG_jsondb* p = NULL;
    SG_string* pstr = NULL;
    SG_varray* pva = NULL;
    SG_string* pstr_path_found = NULL;

	SG_ASSERT(pCtx);
	SG_NONEMPTYCHECK_RETURN(psz_path);

    SG_ERR_CHECK(  SG_closet__get_localsettings(pCtx, &p)  );
    SG_ERR_CHECK(  SG_localsettings__get__varray(pCtx, psz_path, NULL, &pva, &pstr_path_found)  );
    if (!pstr_path_found)
    {
        // this came from factory defaults.
        SG_ERR_CHECK(  SG_string__alloc(pCtx, &pstr_path_found)  );
        SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path_found, "%s/%s", SG_LOCALSETTING__SCOPE__MACHINE, psz_path)  );
        SG_ERR_CHECK(  SG_localsettings__update__varray(pCtx, SG_string__sz(pstr_path_found), pva)  );
    }

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr, "%s/#", SG_string__sz(pstr_path_found))  );
	SG_ERR_CHECK(  SG_jsondb__update__string__sz(pCtx, p, SG_string__sz(pstr), SG_TRUE, pValue)  );

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
	SG_JSONDB_NULLFREE(pCtx, p);
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRING_NULLFREE(pCtx, pstr_path_found);
}

void SG_localsettings__varray__remove_first_match(SG_context * pCtx, const char* psz_path, const char* psz_val)
{
	SG_jsondb* p = NULL;
    SG_string* pstr_path_element = NULL;
    SG_varray* pva = NULL;
    SG_uint32 ndx = 0;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_bool b_found = SG_FALSE;
    SG_string* pstr_path_found = NULL;

	SG_ASSERT(pCtx);
	SG_NONEMPTYCHECK_RETURN(psz_path);

    SG_ERR_CHECK(  SG_closet__get_localsettings(pCtx, &p)  );
    SG_ERR_CHECK(  SG_localsettings__get__varray(pCtx, psz_path, NULL, &pva, &pstr_path_found)  );
    if (pva)
    {
        if (!pstr_path_found)
        {
            // this came from factory defaults.
            SG_ERR_CHECK(  SG_string__alloc(pCtx, &pstr_path_found)  );
            SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path_found, "%s/%s", SG_LOCALSETTING__SCOPE__MACHINE, psz_path)  );
            SG_ERR_CHECK(  SG_localsettings__update__varray(pCtx, SG_string__sz(pstr_path_found), pva)  );
        }

        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
        for (i=0; i<count; i++)
        {
            const char* psz = NULL;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, i, &psz)  );
            if (0 == strcmp(psz, psz_val))
            {
                b_found = SG_TRUE;
                ndx = i;
                break;
            }
        }
        if (b_found)
        {
            SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_path_element)  );
            SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path_element, "%s/%d", SG_string__sz(pstr_path_found), ndx)  );
            SG_ERR_CHECK(  SG_jsondb__remove(pCtx, p, SG_string__sz(pstr_path_element))  );
        }
    }

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
    SG_STRING_NULLFREE(pCtx, pstr_path_found);
    SG_STRING_NULLFREE(pCtx, pstr_path_element);
	SG_JSONDB_NULLFREE(pCtx, p);
}

void SG_localsettings__import(
	SG_context * pCtx,
	SG_vhash* pvh /**< Caller still owns this */
	)
{
    SG_jsondb* p = NULL;

    SG_ERR_CHECK(  SG_closet__get_localsettings(pCtx, &p)  );
    SG_ERR_CHECK(  SG_jsondb__update__vhash(pCtx, p, "/", SG_TRUE, pvh)  );

fail:
    SG_JSONDB_NULLFREE(pCtx, p);
}

void SG_localsettings__split_full_name(
	SG_context* pCtx,
	const char* szFullName,
	SG_uint32* uSplitIndex,
	SG_string* pScopeName,
	SG_string* pSettingName
	)
{
	SG_uint32 uSlashCount = 0;
	SG_uint32 uCurrent = 0;

	SG_ARGCHECK(szFullName[0] == '/', szFullName);

	// this is basically implemented by locating the Nth '/' character
	// where N depends on which scope the name is in
	if (0 == strncmp(szFullName, SG_LOCALSETTING__SCOPE__INSTANCE, strlen(SG_LOCALSETTING__SCOPE__INSTANCE)))
	{
		uSlashCount = 2;
	}
	else if (0 == strncmp(szFullName, SG_LOCALSETTING__SCOPE__REPO, strlen(SG_LOCALSETTING__SCOPE__REPO)))
	{
		uSlashCount = 2;
	}
	else if (0 == strncmp(szFullName, SG_LOCALSETTING__SCOPE__ADMIN, strlen(SG_LOCALSETTING__SCOPE__ADMIN)))
	{
		uSlashCount = 2;
	}
	else if (0 == strncmp(szFullName, SG_LOCALSETTING__SCOPE__MACHINE, strlen(SG_LOCALSETTING__SCOPE__MACHINE)))
	{
		uSlashCount = 1;
	}
	else
	{
		uSlashCount = 1;
	}

	// iterate through the string until we find the slash that we're looking for
	while (szFullName[uCurrent] != '\0' && uSlashCount > 0u)
	{
		// the first time through, this will always skip the leading slash character
		// this is why uSlashCount is one less than it seems like it should be
		uCurrent = uCurrent + 1u;

		// if this is a slash, update our count
		if (szFullName[uCurrent] == '/')
		{
			uSlashCount = uSlashCount - 1;
		}
	}

	// if the caller wants the index, fill it in
	if (uSplitIndex != NULL)
	{
		*uSplitIndex = uCurrent;
	}

	// if the caller wants the scope name, fill it in
	if (pScopeName != NULL)
	{
		SG_ERR_CHECK(  SG_string__set__buf_len(pCtx, pScopeName, (const SG_byte*)szFullName, uCurrent)  );
	}

	// if the caller wants the setting name, fill it in
	if (pSettingName != NULL)
	{
		SG_ERR_CHECK(  SG_string__set__sz(pCtx, pSettingName, szFullName + uCurrent + 1)  );
	}

fail:
	return;
}

/**
 * Structure used by provide_matching_values as pCallerData.
 */
typedef struct
{
	const char*                        szPattern;   //< The pattern to match names against.  See SG_localsettings__foreach.
	SG_localsettings_foreach_callback* pCallback;   //< The callback to provide matching values to.
	void*                              pCallerData; //< Caller-specific data to send back to pCallback.
	SG_string*                         pPrefix;     //< The prefix of the current value.  Used to built fully-qualified names.
}
provide_matching_values__data;

/**
 * Provides each value from a vhash whose name matches a pattern to another callback.
 * Values that are themselves vhashes are recursed into.
 * Each value's name is prefixed such that it's fully-qualified when passed to the callback.
 * Intended for use as a SG_vhash_foreach_callback.
 */
static void provide_matching_values(
	SG_context*       pCtx,        //< Error and context information.
	void*             pCallerData, //< An allocated instance of provide_matching_values__data.
	const SG_vhash*   pHash,       //< The hash that the current value is from.
	const char*       szName,      //< The name of the current value.
	const SG_variant* pValue       //< The current value.
	)
{
	SG_string* pFullName = NULL;
	SG_string* pScopeName = NULL;
	SG_string* pSettingName = NULL;
	SG_uint32 uValueSize = 0u;
	provide_matching_values__data* pData = NULL;

	SG_UNUSED(pHash);

	SG_NULLARGCHECK_RETURN(pCallerData);
	pData = (provide_matching_values__data*) pCallerData;

	// build the full name of this value from the incoming prefix and name
	SG_ERR_CHECK(  SG_STRING__ALLOC__COPY(pCtx, &pFullName, pData->pPrefix)  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pFullName, "/")  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pFullName, szName)  );

	// if this is a vhash, get its size
	if (pValue->type == SG_VARIANT_TYPE_VHASH)
	{
		SG_ERR_CHECK(  SG_vhash__count(pCtx, pValue->v.val_vhash, &uValueSize)  );
	}

	// if this is a vhash with children, then recurse into it
	// otherwise provide it to the callback
	if (uValueSize > 0u)
	{
		// use our full name as the prefix during recursion
		// to accomplish that, we'll swap pData->pPrefix and pFullName
		// that way if an error occurs during recursion, everything will still be freed
		SG_string* pTemp = NULL;
		pTemp = pData->pPrefix;
		pData->pPrefix = pFullName;
		pFullName = pTemp;
		SG_ERR_CHECK(  SG_vhash__foreach(pCtx, pValue->v.val_vhash, provide_matching_values, pData)  );
		pTemp = pData->pPrefix;
		pData->pPrefix = pFullName;
		pFullName = pTemp;
	}
	else
	{
		// if we have a pattern that starts with a slash
		// then match it against the start of the full name
		// if the name doesn't match, skip this one
		if (pData->szPattern != NULL && pData->szPattern[0] == '/')
		{
			if (strncmp(SG_string__sz(pFullName), pData->szPattern, strlen(pData->szPattern)) != 0)
			{
				goto fail;
			}
		}

		// split our full name into a scope name and a local name
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pScopeName)  );
		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pSettingName)  );
		SG_ERR_CHECK(  SG_localsettings__split_full_name(pCtx, SG_string__sz(pFullName), NULL, pScopeName, pSettingName)  );

		// if we have a pattern that doesn't start with a slash
		// then match it against just the local name of the setting
		// if the name doesn't match, skip this one
		if (pData->szPattern != NULL && pData->szPattern[0] != '/')
		{
			if (strstr(SG_string__sz(pSettingName), pData->szPattern) == NULL)
			{
				goto fail;
			}
		}

		// send the data to the callback
		pData->pCallback(pCtx, SG_string__sz(pFullName), SG_string__sz(pScopeName), SG_string__sz(pSettingName), pValue, pData->pCallerData);
	}

fail:
	SG_STRING_NULLFREE(pCtx, pFullName);
	SG_STRING_NULLFREE(pCtx, pScopeName);
	SG_STRING_NULLFREE(pCtx, pSettingName);
}

void SG_localsettings__foreach(
	SG_context* pCtx,
	const char* szPattern,
	SG_bool bIncludeDefaults,
	SG_localsettings_foreach_callback* pCallback,
	void* pCallerData
	)
{
	SG_vhash* pValues = NULL;
	SG_vhash* pDefaults = NULL;
	provide_matching_values__data ProvideMatchingValuesData = {NULL, NULL, NULL, NULL};

	SG_UNUSED(pCallback);
	SG_UNUSED(pCallerData);

	// get the settings
	SG_ERR_CHECK(  SG_localsettings__list__vhash(pCtx, &pValues)  );

	// if defaults were requested, get those too
	if (bIncludeDefaults)
	{
		SG_ERR_CHECK(  SG_localsettings__factory__list__vhash(pCtx, &pDefaults)  );
		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pValues, SG_LOCALSETTING__SCOPE__DEFAULT + 1, &pDefaults)  ); // +1 to remove the slash at the beginning
	}

	// sort the settings
	SG_ERR_CHECK(  SG_vhash__sort(pCtx, pValues, SG_TRUE, SG_vhash_sort_callback__increasing)  );

	// setup our callback data
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &(ProvideMatchingValuesData.pPrefix))  );
	ProvideMatchingValuesData.szPattern = szPattern;
	ProvideMatchingValuesData.pCallback = pCallback;
	ProvideMatchingValuesData.pCallerData = pCallerData;

	// iterate through the vhash
	SG_ERR_CHECK(  SG_vhash__foreach(pCtx, pValues, provide_matching_values, &ProvideMatchingValuesData)  );

fail:
	SG_VHASH_NULLFREE(pCtx, pValues);
	SG_VHASH_NULLFREE(pCtx, pDefaults);
	SG_STRING_NULLFREE(pCtx, ProvideMatchingValuesData.pPrefix);
}
