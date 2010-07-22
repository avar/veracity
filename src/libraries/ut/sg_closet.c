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

static void sg_closet__verify_root_path_exists(SG_context * pCtx, SG_pathname** ppPath)
{
	SG_pathname* pPath = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__USER_APPDATA_DIRECTORY(pCtx, &pPath)  );
	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath, ".sgcloset")  );

	SG_fsobj__verify_directory_exists_on_disk__pathname(pCtx, pPath);
	if (!SG_context__has_err(pCtx))
	{
		*ppPath = pPath;
		return;
	}

	if (SG_context__err_equals(pCtx, SG_ERR_NOT_A_DIRECTORY))
		SG_ERR_RETHROW;

	if (SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
	{
		SG_context__err_reset(pCtx);
		SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath)  );
	}

	*ppPath = pPath;

	return;
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

#define sg_CLOSET_CONFIG_DESCRIPTORS        1
#define sg_CLOSET_CONFIG_LOCAL_SETTINGS     2

static void sg_closet__config__get(
	SG_context * pCtx,
	SG_uint32 which,
	SG_jsondb** ppJsondb
    )
{
	SG_pathname* pPath = NULL;
	SG_jsondb* pJsondb = NULL;
	SG_bool bHasRoot;

	SG_ERR_CHECK(  sg_closet__verify_root_path_exists(pCtx, &pPath)  );

	switch (which)
	{
	case sg_CLOSET_CONFIG_DESCRIPTORS:
		SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath, "descriptors.jsondb")  );
		SG_ERR_CHECK(  SG_jsondb__create_or_open(pCtx, pPath, "descriptors", &pJsondb)  );
		break;
	case sg_CLOSET_CONFIG_LOCAL_SETTINGS:
		SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath, "settings.jsondb")  );
		SG_ERR_CHECK(  SG_jsondb__create_or_open(pCtx, pPath, "settings", &pJsondb)  );
		break;
	default:
		SG_ERR_THROW(  SG_ERR_INVALIDARG  );
	}

	SG_ERR_CHECK(  SG_jsondb__has(pCtx, pJsondb, "/", &bHasRoot)  );
	if (!bHasRoot)
		SG_ERR_CHECK(  SG_jsondb__add__vhash(pCtx, pJsondb, "/", SG_FALSE, NULL)  );

	SG_PATHNAME_NULLFREE(pCtx, pPath);

	*ppJsondb = pJsondb;

	return;
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_JSONDB_NULLFREE(pCtx, pJsondb);
}

void SG_closet__get_localsettings(
	SG_context * pCtx,
	SG_jsondb** ppJsondb
    )
{
    SG_ERR_CHECK_RETURN(  sg_closet__config__get(pCtx, sg_CLOSET_CONFIG_LOCAL_SETTINGS, ppJsondb)  );
}

static void _name_to_path(SG_context* pCtx, const char* pszName, char** ppszPath)
{
	char* pszEscapedName = NULL;
	char* pszPath = NULL;
	SG_uint32 len;

	SG_ERR_CHECK(  SG_jsondb__escape_keyname(pCtx, pszName, &pszEscapedName)  );

	len = strlen(pszEscapedName) + 2;

	SG_ERR_CHECK(  SG_allocN(pCtx, len, pszPath)  );
	pszPath[0] = '/';
	pszPath[1] = 0;
	SG_ERR_CHECK(  SG_strcat(pCtx, pszPath, len, pszEscapedName)  );

	SG_NULLFREE(pCtx, pszEscapedName);

	*ppszPath = pszPath;

	return;

fail:
	SG_NULLFREE(pCtx, pszPath);
	SG_NULLFREE(pCtx, pszEscapedName);
}
void SG_closet__descriptors__list(SG_context * pCtx, SG_vhash** ppResult)
{
	SG_jsondb* p = NULL;

	SG_ERR_CHECK_RETURN(  sg_closet__config__get(pCtx, sg_CLOSET_CONFIG_DESCRIPTORS, &p)  );
	SG_ERR_CHECK(  SG_jsondb__get__vhash(pCtx, p, "/", ppResult)  );

	/* fall through */
fail:
	SG_JSONDB_NULLFREE(pCtx, p);
}

void SG_closet__descriptors__add(SG_context * pCtx, const char* pszName, const SG_vhash* pvhDescriptor)
{
	SG_jsondb* p = NULL;
	char* pszPath = NULL;

	SG_ERR_CHECK(  sg_closet__config__get(pCtx, sg_CLOSET_CONFIG_DESCRIPTORS, &p)  );
	SG_ERR_CHECK(  _name_to_path(pCtx, pszName, &pszPath)  );
	SG_ERR_CHECK(  SG_jsondb__add__vhash(pCtx, p, pszPath, SG_TRUE, pvhDescriptor)  );

	/* fall through */
fail:
	SG_JSONDB_NULLFREE(pCtx, p);
	SG_NULLFREE(pCtx, pszPath);
}

void SG_closet__descriptors__exists(SG_context* pCtx, const char* pszName, SG_bool* pbExists)
{
	SG_jsondb* p = NULL;
	char* pszPath = NULL;

	// SG_jsondb__has does null arg check on pbExists.
	SG_ERR_CHECK(  SG_repo__validate_repo_name(pCtx, pszName)  );
	SG_ERR_CHECK(  sg_closet__config__get(pCtx, sg_CLOSET_CONFIG_DESCRIPTORS, &p)  );
	SG_ERR_CHECK(  _name_to_path(pCtx, pszName, &pszPath)  );
	SG_ERR_CHECK(  SG_jsondb__has(pCtx, p, pszPath, pbExists)  );

	/* fall through */
fail:
	SG_JSONDB_NULLFREE(pCtx, p);
	SG_NULLFREE(pCtx, pszPath);
}

void SG_closet__descriptors__get(SG_context * pCtx, const char* pszName, SG_vhash** ppResult)
{
	SG_jsondb* p = NULL;
	char* pszPath = NULL;

	SG_ERR_CHECK(  SG_repo__validate_repo_name(pCtx, pszName)  );
	SG_ERR_CHECK(  sg_closet__config__get(pCtx, sg_CLOSET_CONFIG_DESCRIPTORS, &p)  );
	SG_ERR_CHECK(  _name_to_path(pCtx, pszName, &pszPath)  );

	SG_jsondb__get__vhash(pCtx, p, pszPath, ppResult);
	if(SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
		SG_ERR_RESET_THROW2(SG_ERR_NOTAREPOSITORY, (pCtx, "%s", pszName));
	else
		SG_ERR_CHECK_CURRENT;

	/* fall through */
fail:
	SG_JSONDB_NULLFREE(pCtx, p);
	SG_NULLFREE(pCtx, pszPath);
}

void SG_closet__descriptors__remove(SG_context * pCtx, const char* pszName)
{
	SG_jsondb* p = NULL;
	char* pszPath = NULL;

	SG_ERR_CHECK(  sg_closet__config__get(pCtx, sg_CLOSET_CONFIG_DESCRIPTORS, &p)  );
	SG_ERR_CHECK(  _name_to_path(pCtx, pszName, &pszPath)  );
	SG_ERR_CHECK(  SG_jsondb__remove(pCtx, p, pszPath)  );

	/* fall through */
fail:
	SG_JSONDB_NULLFREE(pCtx, p);
	SG_NULLFREE(pCtx, pszPath);
}

void SG_closet__get_partial_repo_instance_descriptor_for_new_local_repo(SG_context * pCtx, SG_vhash** ppvhDescriptor)
{
	SG_vhash* pvhPartialDescriptor = NULL;
	SG_pathname* pPath = NULL;
	char* pszRepoImpl = NULL;

	SG_ERR_CHECK(  sg_closet__verify_root_path_exists(pCtx, &pPath)  );

	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath, "repo")  );

	SG_fsobj__verify_directory_exists_on_disk__pathname(pCtx, pPath);
	if (SG_context__err_equals(pCtx, SG_ERR_NOT_FOUND))
	{
		SG_context__err_reset(pCtx);
		SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPath)  );
	}

	SG_ERR_CHECK_CURRENT;


	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhPartialDescriptor)  );

    SG_ERR_CHECK(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__NEWREPO_DRIVER, NULL, &pszRepoImpl, NULL)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_KEY__STORAGE, pszRepoImpl)  );

    SG_NULLFREE(pCtx, pszRepoImpl);

	/* This is used by non-filesystem repo implementations because the sqlite dbndx files go here. */
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_FSLOCAL__PATH_PARENT_DIR, SG_pathname__sz(pPath))  );

	SG_PATHNAME_NULLFREE(pCtx, pPath);

	*ppvhDescriptor = pvhPartialDescriptor;

	return;
fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);
    SG_NULLFREE(pCtx, pszRepoImpl);
}

