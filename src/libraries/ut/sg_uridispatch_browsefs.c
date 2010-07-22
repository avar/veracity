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


///////////////////////////////////////////////////////////////

static SG_bool _isIgnorable(const char *fn, SG_uint32 uriSubstringCount)
{
	return( eq(fn, ".") ||
			((uriSubstringCount == 0) && eq(fn, "..")) ||
			eq(fn, ".sgdrawer")
		  );
}

static void _uriToFsPath(
	SG_context *pCtx,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
	SG_pathname **pPathResult)	/**< caller owns if not null on return */
{
	SG_pathname *tpath = NULL;
	char *gid = NULL;
	SG_string *descriptor = NULL;
	SG_pathname *pPathCwd = NULL;
	SG_uint32 i;
	SG_fsobj_stat firstStat;

    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(pPathResult);

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &tpath)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, tpath)  );

	SG_ERR_CHECK(  SG_workingdir__find_mapping(pCtx, tpath, &pPathCwd, &descriptor, &gid)  );

	for (i = 0; i < uriSubstringsCount; ++i)
	{
		SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPathCwd, ppUriSubstrings[i])  );
	}

	SG_ERR_CHECK(  SG_pathname__add_final_slash(pCtx, pPathCwd)  );

	SG_fsobj__stat__pathname(pCtx, pPathCwd, &firstStat);

	if (SG_context__has_err(pCtx))
	{
		SG_ERR_RESET_THROW(SG_ERR_URI_HTTP_404_NOT_FOUND);
	}
	else if (firstStat.type != SG_FSOBJ_TYPE__DIRECTORY)
	{
		SG_ERR_RESET_THROW(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
	}
	else
	{
		*pPathResult = pPathCwd;
		pPathCwd = NULL;
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, tpath);
	SG_NULLFREE(pCtx, gid);
	SG_STRING_NULLFREE(pCtx, descriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
}

static void _GET__fs(SG_context * pCtx,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _response_handle ** ppResponseHandle)
{
	SG_pathname* pPathCwd = NULL;
	SG_dir *dir = NULL;
	SG_string *fn = NULL;
	SG_error theErr = SG_ERR_UNINITIALIZED;
	SG_varray *flist = NULL;
	SG_vhash *fi = NULL;
	SG_string *content = NULL;
	SG_vhash *dirinfo = NULL;
	SG_fsobj_stat firstStat;

	SG_ASSERT(ppUriSubstrings!=NULL);

	if ((uriSubstringsCount == 1) && eq(ppUriSubstrings[0], ""))
		--uriSubstringsCount;

	SG_ERR_CHECK(  _uriToFsPath(pCtx, ppUriSubstrings, uriSubstringsCount, &pPathCwd)  );

	// if we get here, our path exists, and is a directory

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &flist)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &fn)  );
	SG_dir__open(pCtx, pPathCwd, &dir, &theErr, fn, &firstStat);

	while (! SG_context__has_err(pCtx))
	{
		if (((firstStat.type == SG_FSOBJ_TYPE__UNSPECIFIED) ||
			 (firstStat.type == SG_FSOBJ_TYPE__REGULAR) ||
			 (firstStat.type == SG_FSOBJ_TYPE__DIRECTORY)
			) &&
			! _isIgnorable(SG_string__sz(fn), uriSubstringsCount)
		   )
		{
			SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &fi)  );
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, fi, "name", SG_string__sz(fn))  );
			SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, fi, "size", (SG_int64)firstStat.size)  );
			SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, fi, "mtimems", (SG_int64)firstStat.mtime_ms)  );
			SG_ERR_CHECK(  SG_vhash__add__bool(pCtx, fi, "isdir", (firstStat.type == SG_FSOBJ_TYPE__DIRECTORY) ? SG_TRUE : SG_FALSE)  );

			SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, flist, &fi)  );
		}

		SG_dir__read(pCtx, dir, fn, &firstStat);
	}

	SG_context__err_reset(pCtx);
	SG_DIR_NULLCLOSE(pCtx, dir);

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &dirinfo)  );
	SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, dirinfo, "files", &flist)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &content)  );
	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, dirinfo, content)  );

	_create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &content, ppResponseHandle);

fail:
	SG_DIR_NULLCLOSE(pCtx, dir);
	SG_STRING_NULLFREE(pCtx, fn);
	SG_VHASH_NULLFREE(pCtx, fi);
	SG_VHASH_NULLFREE(pCtx, dirinfo);
	SG_VARRAY_NULLFREE(pCtx, flist);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
}


static void _fs_replacer(
	SG_context * pCtx,
	const _request_headers * pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement)
{
	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "TITLE"))
	{
		SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, replacement, "veracity / browsing")  );
	}
}

void _dispatch__fs(SG_context * pCtx,
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _response_handle ** ppResponseHandle)
{
	SG_pathname *pPathCwd = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);

	if (! eq(pRequestHeaders->pRequestMethod, "GET"))
	{
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
	}

	if (pRequestHeaders->accept == SG_contenttype__json)
        SG_ERR_CHECK_RETURN(  _GET__fs(pCtx, ppUriSubstrings, uriSubstringsCount, ppResponseHandle)  );
	else
	{
		SG_ERR_CHECK(  _uriToFsPath(pCtx, ppUriSubstrings, uriSubstringsCount, &pPathCwd)  );
		SG_ERR_CHECK(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "browsefs.xhtml", _fs_replacer, ppResponseHandle)  );
	}

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
}
