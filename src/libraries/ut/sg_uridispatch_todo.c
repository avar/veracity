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
 * @file sg_uridispatch_todo.c
 *
 * @details Web service handler
 */

#include <sg.h>
#include "sg_uridispatch_private_typedefs.h"
#include "sg_uridispatch_private_prototypes.h"

//////////////////////////////////////////////////////////////////



static void _todo_replacer(
	SG_context * pCtx,
	const _request_headers *pRequestHeaders,
	SG_string *keyword,
	SG_string *replacement)
{
	SG_UNUSED(pRequestHeaders);

	if (seq(keyword, "TITLE"))
	{
		SG_ERR_CHECK_RETURN(  SG_string__set__sz(pCtx, replacement, "veracity / todo")  );
	}
}

void _dispatch__todo(SG_context * pCtx,
    _request_headers * pRequestHeaders,
	SG_repo **ppRepo,
    const char ** ppUriSubstrings,
    SG_uint32 uriSubstringsCount,
    _response_handle ** ppResponseHandle)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pRequestHeaders);
    SG_NULLARGCHECK_RETURN(ppUriSubstrings);
    SG_NULLARGCHECK_RETURN(ppResponseHandle);
    SG_ASSERT(*ppResponseHandle==NULL);

	SG_UNUSED(ppRepo);

    if(uriSubstringsCount==0)
    {
        if(eq(pRequestHeaders->pRequestMethod,"GET"))
        {
            SG_ERR_CHECK_RETURN(  _create_response_handle_for_template(pCtx, pRequestHeaders, SG_HTTP_STATUS_OK, "todo.xhtml", _todo_replacer, ppResponseHandle)  );
        }
        else
            SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_405_METHOD_NOT_ALLOWED);
    }
    else
        SG_ERR_THROW_RETURN(SG_ERR_URI_HTTP_404_NOT_FOUND);
}

