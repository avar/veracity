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
 *
 * @details Web service handler - retrieves the full status of the repository instance
 * associated with the current working folder.
 */

#include <sg.h>
#include "sg_uridispatch_private_typedefs.h"
#include "sg_uridispatch_private_prototypes.h"

//////////////////////////////////////////////////////////////////



static void _GET__status_json(SG_context * pCtx,
    _response_handle ** ppResponseHandle)
{
	SG_vhash* pvh_descriptor = NULL;
	SG_treediff2 * pTreeDiff = NULL;
	SG_string * pStrStatusReport = NULL;
	SG_pathname* pPathCwd = NULL;
	SG_pendingtree* pPendingTree = NULL;
	SG_bool bIgnoreWarnings = SG_FALSE;
	SG_vhash *results = NULL;
	SG_string * content = NULL;

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
	SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );

	SG_ERR_CHECK(  SG_pendingtree__diff_or_status(pCtx, pPendingTree, NULL, NULL, &pTreeDiff)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &results) );

	SG_ERR_CHECK(  SG_treediff2__report_status_to_vhash(pCtx, pTreeDiff, results, SG_FALSE) );


	SG_STRING__ALLOC(pCtx, &content);
	SG_vhash__to_json(pCtx, results, content);

	_create_response_handle_for_string(pCtx, SG_HTTP_STATUS_OK, SG_contenttype__json, &content, ppResponseHandle);

fail:
	SG_VHASH_NULLFREE(pCtx, pvh_descriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRING_NULLFREE(pCtx, pStrStatusReport);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff);
	SG_VHASH_NULLFREE(pCtx, results);
}


static void _status_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement)
{
	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "TITLE"))
	{
		SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, replacement, "veracity / status")  );
	}
}

void _dispatch__status(SG_context * pCtx,
    _request_headers * pRequestHeaders,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _response_handle ** ppResponseHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"GET"))
        {
            if(pRequestHeaders->accept==SG_contenttype__json)
                SG_ERR_CHECK_RETURN(  _GET__status_json(pCtx, ppResponseHandle)  );
            else
                SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "status.xhtml", _status_replacer, ppResponseHandle)  );
        }
        else
            SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    }
    else
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);
}

