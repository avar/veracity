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
 * @file sg_uridispatch_ui.c
 */

#include <sg.h>
#include "sg_uridispatch_private_typedefs.h"
#include "sg_uridispatch_private_prototypes.h"

#define eq(str1,str2) (strcmp(str1,str2)==0)
#define sgeq(sgstr1,sgstr2) (strcmp(SG_string__sz(sgstr1),SG_string__sz(sgstr2))==0)

SG_pathname *_sg_uridispatch__templatePath = NULL;

//////////////////////////////////////////////////////////////////


void _read_template_file(
	SG_context *pCtx,
	const char *templateFn,
	SG_string **pContent,	/**< we allocate, we free on error, else caller owns it */
	const _request_headers *pRequestHeaders,
	_replacer_cb replacer);

static void _default_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement,
	SG_bool *needEncoding);


/**
 *	Let the UI dispatch methods know where to find their template files.
 */
void _sgui_set_templatePath(SG_context * pCtx)
{
	SG_pathname *collateralRoot = NULL;
	SG_pathname * templatePath = NULL;
    char* psz = NULL;

    SG_ERR_CHECK(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__SSI_DIR, NULL, &psz, NULL)  );
    SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &collateralRoot, psz)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &templatePath, collateralRoot, "templates")  );

    SG_NULLFREE(pCtx, psz);
	SG_PATHNAME_NULLFREE(pCtx, collateralRoot);

	_sg_uridispatch__templatePath = templatePath;

	return;
fail:
    SG_NULLFREE(pCtx, psz);
	SG_PATHNAME_NULLFREE(pCtx, templatePath);
	SG_PATHNAME_NULLFREE(pCtx, collateralRoot);
}

//////////////////////////////////////////////////////////////////

static void _templatize(
	SG_context *pCtx,
	SG_string *content,
	const _request_headers *pRequestHeaders,
	_replacer_cb replacer)
{
	SG_string *instr = NULL;
	const char *next = NULL;
	SG_string *piece = NULL;
	SG_string *replacement = NULL;
	SG_string *inclusion = NULL;
	SG_string *replaceRaw = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC__COPY(pCtx, &instr, content)  );
	SG_ERR_CHECK(  SG_string__clear(pCtx, content)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &piece)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &replacement)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &replaceRaw)  );

	next = SG_string__sz(instr);

	while (*next)
	{
		const char *delim = strstr(next, "{{{");

		if (delim == NULL)
		{
			SG_ERR_CHECK(  SG_string__append__sz(pCtx, content, next)  );
			break;
		}
		else
		{
			const char *rest = delim + 3;
			const char *end = strstr(rest, "}}}");

			if (end == NULL)
			{
				SG_ERR_CHECK(  SG_string__append__sz(pCtx, content, next)  );
				break;
			}

			SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, content, (const SG_byte *)next, (delim - next) * sizeof(char))  );

			SG_ERR_CHECK(  SG_string__clear(pCtx, piece)  );
			SG_ERR_CHECK(  SG_string__clear(pCtx, replacement)  );

			SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, piece, (const SG_byte *)rest, (end - rest) * sizeof(char))  );
			SG_ERR_CHECK(  SG_string__append__string(pCtx, replacement, piece)  );

			if (SG_string__sz(piece)[0] == '<')
			{
				SG_string__remove(pCtx, piece, 0, 1);

				SG_ASSERT(inclusion == NULL);
				SG_ERR_CHECK( _read_template_file(pCtx, SG_string__sz(piece), &inclusion, pRequestHeaders, replacer)  );

				SG_STRING_NULLFREE(pCtx, replacement);
				replacement = inclusion;
				inclusion = NULL;
			}
			else
			{
				SG_bool needEncoding = SG_TRUE;

				SG_ERR_CHECK(  replacer(pCtx, pRequestHeaders, piece, replaceRaw)  );

				// no verb-specific replacement? try generics
				if (sgeq(piece, replacement))
				{
					SG_ERR_CHECK(  _default_replacer(pCtx, pRequestHeaders, piece, replaceRaw,  &needEncoding)  );
				}

				if (needEncoding)
					SG_ERR_CHECK(  SG_htmlencode(pCtx, replaceRaw, replacement)  );
				else
					SG_ERR_CHECK(  SG_string__set__string(pCtx, replacement, replaceRaw)  );
			}

			SG_ERR_CHECK(  SG_string__append__string(pCtx, content, replacement)  );

			next = end + 3;
		}
	}

fail:
	SG_STRING_NULLFREE(pCtx, instr);
	SG_STRING_NULLFREE(pCtx, piece);
	SG_STRING_NULLFREE(pCtx, replacement);
	SG_STRING_NULLFREE(pCtx, replaceRaw);
	SG_STRING_NULLFREE(pCtx, inclusion);
}

void _read_template_file(
	SG_context *pCtx,
	const char *templateFn,
	SG_string **pContent,	/**< we allocate, we free on error, else caller owns it */
	const _request_headers *pRequestHeaders,
	_replacer_cb replacer)
{
	SG_pathname *tpath = NULL;
	SG_file *pFile = NULL;
	SG_uint32 got = 0;
	char	tbuf[1024];

    //todo: make this thread-safe:
    if(_sg_uridispatch__templatePath==NULL)
        SG_ERR_CHECK(  _sgui_set_templatePath(pCtx)  );

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &tpath, _sg_uridispatch__templatePath)  );
	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, tpath, templateFn)  );

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, tpath, SG_FILE_RDONLY|SG_FILE_OPEN_EXISTING, 0644, &pFile)  );
	SG_PATHNAME_NULLFREE(pCtx, tpath);

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, pContent)  );

	do
	{
		SG_file__read(pCtx, pFile, sizeof(tbuf), (SG_byte *)tbuf, &got);
        if (SG_context__err_equals(pCtx, SG_ERR_EOF))
        {
			SG_context__err_reset(pCtx);
            break;
        }
		SG_ERR_CHECK_CURRENT;

		SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, *pContent, (const SG_byte *)tbuf, got) );
	} while (got > 0);

	SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

	SG_ERR_CHECK(  _templatize(pCtx, *pContent, pRequestHeaders, replacer)  );

	return;

fail:
	SG_STRING_NULLFREE(pCtx, *pContent);
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, tpath);
}

static void _getEncoded(
	SG_context *pCtx,
	const char *raw,
	SG_string *encoded)
{
	SG_string *rawstr = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &rawstr, raw)  );

	SG_ERR_CHECK(  SG_htmlencode(pCtx, rawstr, encoded)  );

fail:
	SG_STRING_NULLFREE(pCtx, rawstr);
}

static SG_bool _startsWith(
	const char *prefix,
	const char *st)
{
	if ((prefix == NULL) || (st == NULL))
		return(SG_FALSE);

	return( (strlen(st) >= strlen(prefix)) &&
			(memcmp(st, prefix, strlen(prefix) * sizeof(char)) == 0)
		  );
}

static void _getDescriptorName(
	SG_context *pCtx,
	const _request_headers *pRequestHeaders,
	SG_string **ppRepoName)
{
	SG_pathname *pPathCwd = NULL;

	if (_startsWith("repos/", pRequestHeaders->pUri) && (strlen(pRequestHeaders->pUri) > 6))
	{
		const char *nameStart = pRequestHeaders->pUri + 6;
		const char *nameEnd = strchr(nameStart, '/');

		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, ppRepoName)  );

		if (nameEnd == NULL)
			SG_ERR_CHECK(  SG_string__set__sz(pCtx, *ppRepoName, nameStart)  );
		else
			SG_ERR_CHECK(  SG_string__set__buf_len(pCtx, *ppRepoName, (const SG_byte *)nameStart,
												   (nameEnd - nameStart) * sizeof(char))  );
	}
	else
	{
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
		SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );
		SG_ERR_CHECK(  SG_workingdir__find_mapping(pCtx, pPathCwd, NULL, ppRepoName, NULL)  );
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
}

static void _getUserId(
	SG_context *pCtx,
	SG_repo *repo,
	SG_string *userid)
{
	SG_audit u;

	SG_ERR_CHECK(  SG_audit__init__maybe_nobody(pCtx, &u, repo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
	SG_ERR_CHECK(  SG_string__set__sz(pCtx, userid, u.who_szUserId)  );

fail:
	;
}


static void _fillInUserSelection(
	SG_context *pCtx,
	SG_string *pstrRepoDescriptorName,
	SG_string *replacement)
{
	SG_repo *repo = NULL;
	SG_varray *users = NULL;
	SG_vhash *user = NULL;
	SG_uint32 i = 0;
	SG_uint32 count;
	SG_string *semail = NULL;
	SG_string *suid = NULL;
	SG_string *entry = NULL;
	SG_string *curuid = NULL;

	SG_ERR_CHECK(  SG_string__clear(pCtx, replacement)  );
	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, SG_string__sz(pstrRepoDescriptorName), &repo)  );

	SG_ERR_CHECK(  SG_user__list_all(pCtx, repo, &users)  );
	SG_ERR_CHECK(  SG_varray__count(pCtx, users, &count)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &semail)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &suid)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &entry)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &curuid)  );
	SG_ERR_CHECK(  _getUserId(pCtx, repo, curuid)  );

	for ( i = 0; i < count; ++i )
	{
		const char *uid = NULL;
		const char *email = NULL;
		const char *selected = NULL;

		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, users, i, &user)  );

		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, user, "recid", &uid)  );
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, user, "email", &email)  );

		SG_ERR_CHECK(  _getEncoded(pCtx, uid, suid)  );
		SG_ERR_CHECK(  _getEncoded(pCtx, email, semail)  );

		if (eq(SG_string__sz(curuid), uid))
		{
			selected = " selected='selected' ";
		}
		else
		{
			selected = "";
		}

		SG_ERR_CHECK(  SG_string__sprintf(pCtx, entry, "<option value=\"%s\" %s>%s</option>",
											SG_string__sz(suid), selected, SG_string__sz(semail))  );

		SG_ERR_CHECK(  SG_string__append__string(pCtx, replacement, entry)  );
	}
fail:
	SG_VARRAY_NULLFREE(pCtx, users);
	SG_REPO_NULLFREE(pCtx, repo);
	SG_STRING_NULLFREE(pCtx, semail);
	SG_STRING_NULLFREE(pCtx, suid);
	SG_STRING_NULLFREE(pCtx, entry);
	SG_STRING_NULLFREE(pCtx, curuid);
}

static void _getUserEmail(SG_context *pCtx,
	SG_repo* pRepo,	
	SG_string *replacement)
{
	SG_vhash* pvh_user = NULL;
	SG_string* pstrUser = NULL;
	const char* psz_email = NULL;

	SG_STRING__ALLOC(pCtx, &pstrUser);
	SG_ERR_CHECK(  _getUserId(pCtx, pRepo, pstrUser)  );
	SG_ERR_CHECK(  SG_user__lookup_by_userid(pCtx, pRepo, SG_string__sz(pstrUser), &pvh_user)  );
    if (pvh_user)
    {
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_user, "email", &psz_email)  );			
    }
	SG_ERR_CHECK(  SG_string__set__sz(pCtx, replacement, psz_email)  );
fail:
	SG_VHASH_NULLFREE(pCtx, pvh_user);
	SG_STRING_NULLFREE(pCtx, pstrUser);

}

static void _default_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement,
	SG_bool *needEncoding)
{
	SG_string *pstrRepoDescriptorName = NULL;
	SG_repo *repo = NULL;

	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "WFREPO"))
	{
		SG_ERR_CHECK(  _getDescriptorName(pCtx, pRequestHeaders, &pstrRepoDescriptorName)  );
		SG_ERR_CHECK(  SG_string__set__string(pCtx, replacement, pstrRepoDescriptorName)  );
	}
	else if (seq(keyword, "USERNAME"))
	{
		SG_ERR_CHECK(  _getDescriptorName(pCtx, pRequestHeaders, &pstrRepoDescriptorName)  );

		if (pstrRepoDescriptorName != NULL)
		{
			SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, SG_string__sz(pstrRepoDescriptorName), &repo)  );
		}

		SG_ERR_CHECK(  _getUserId(pCtx, repo, replacement)  );
	}
	else if (seq(keyword, "EMAIL"))
	{
		SG_ERR_CHECK(  _getDescriptorName(pCtx, pRequestHeaders, &pstrRepoDescriptorName)  );

		if (pstrRepoDescriptorName != NULL)
		{
			SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, SG_string__sz(pstrRepoDescriptorName), &repo)  );
		}

		SG_ERR_CHECK( _getUserEmail(pCtx, repo, replacement)  );
	}
	else if (seq(keyword, "USERSELECTION"))
	{
		SG_ERR_CHECK(  _getDescriptorName(pCtx, pRequestHeaders, &pstrRepoDescriptorName)  );
		SG_ERR_CHECK(  _fillInUserSelection(pCtx, pstrRepoDescriptorName, replacement)  );
		*needEncoding = SG_FALSE;
	}

fail:
	SG_STRING_NULLFREE(pCtx, pstrRepoDescriptorName);
	SG_REPO_NULLFREE(pCtx, repo);
}
