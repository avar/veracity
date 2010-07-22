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
 *
 * @file sg_client__api.c
 *
 * This file will contain the static declarations of all VTABLES.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_client__api.h"

/* The "c" implementation of the client vtable can push/pull with local repositories. */
#include "sg_client_vtable__c.h"
DCL__CLIENT_VTABLE(c);

/* The "http" implementation of the client vtable can push/pull with remote repositories. */
#include "sg_client_vtable__http.h"
DCL__CLIENT_VTABLE(http);

//////////////////////////////////////////////////////////////////

void sg_client__bind_vtable(SG_context* pCtx, SG_client * pClient)
{
	char* psz_protocol = NULL;

	SG_NULLARGCHECK_RETURN(pClient);
	SG_NULLARGCHECK_RETURN(pClient->psz_remote_repo_spec);

	if (pClient->p_vtable) // can only be bound once
		SG_ERR_THROW2_RETURN(SG_ERR_INVALIDARG, (pCtx, "vtable already bound"));

	// Select the vtable based on pClient's destination repo specification.
	if (strlen(pClient->psz_remote_repo_spec) > 6)
	{
		SG_ERR_CHECK(  SG_ascii__substring(pCtx, pClient->psz_remote_repo_spec, 0, 7, &psz_protocol)  );
		if (SG_stricmp("http://", (const char*)psz_protocol) == 0)
			pClient->p_vtable = &s_client_vtable__http;
	}
	if (!pClient->p_vtable)
		pClient->p_vtable = &s_client_vtable__c;

	// fall through
 fail:
 	SG_NULLFREE(pCtx, psz_protocol);
}

void sg_client__unbind_vtable(SG_client * pClient)
{
	if (!pClient)
		return;

	// TODO if we bound the repo to a dynamically-allocated VTABLE,
	// TODO free it.  (may also need to ref-count the containing dll.)

	// whether static or dynamic, we then set the vtable pointers to null.

	pClient->p_vtable = NULL;
}

