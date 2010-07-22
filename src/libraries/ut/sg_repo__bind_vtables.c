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
 * @file sg_repo__bind_vtable.c
 *
 * @details Private parts of SG_repo related to binding the
 * REPO vtable.
 *
 * This file will contain the static declarations of all VTABLES
 * (and eventually manage the dynamic loading of other VTABLES)
 * to allow a repo to choose between multiple implementations
 * of the REPO interfaces.
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_repo__private.h"

#include "sg_repo_vtable__fs2.h"
DCL__REPO_VTABLE(SG_RIDESC_STORAGE__FS2, fs2);

#include "sg_repo_vtable__fs3.h"
DCL__REPO_VTABLE(SG_RIDESC_STORAGE__FS3, fs3);

#include "sg_repo_vtable__sqlite.h"
DCL__REPO_VTABLE(SG_RIDESC_STORAGE__SQLITE, sqlite);

//////////////////////////////////////////////////////////////////

void sg_repo__bind_vtable(SG_context* pCtx, SG_repo * pRepo, const char * pszStorage)
{
	SG_NULLARGCHECK_RETURN(pRepo);

	if (pRepo->p_vtable)			// can only be bound once
		SG_ERR_THROW2_RETURN(SG_ERR_INVALIDARG, (pCtx, "pRepo->p_vtable is already bound"));
	if (pRepo->pvh_descriptor)
		SG_ERR_THROW2_RETURN(SG_ERR_INVALIDARG, (pCtx, "pRepo->pvh_descriptor is already bound"));

	if (!pszStorage || !*pszStorage)
		pszStorage = SG_RIDESC_STORAGE__DEFAULT;

    if (0 == strcmp(pszStorage, SG_RIDESC_STORAGE__FS2))
    {
        pRepo->p_vtable = &s_repo_vtable__fs2;
    }
    else if (0 == strcmp(pszStorage, SG_RIDESC_STORAGE__FS3))
    {
        pRepo->p_vtable = &s_repo_vtable__fs3;
    }
	else if (0 == strcmp(pszStorage, SG_RIDESC_STORAGE__SQLITE))
	{
		pRepo->p_vtable = &s_repo_vtable__sqlite;
	}
    else
    {
        SG_ERR_THROW_RETURN(  SG_ERR_UNKNOWN_STORAGE_IMPLEMENTATION  );
    }

	SG_ASSERT(  (0 == strcmp(pszStorage, pRepo->p_vtable->pszStorage))  );
}

/*static*/ void sg_repo__unbind_vtable(SG_context* pCtx, SG_repo * pRepo)
{
	SG_NULLARGCHECK_RETURN(pRepo);

	// TODO if we bound the repo to a dynamically-allocated VTABLE,
	// TODO free it.  (may also need to ref-count the containing dll.)

	// whether static or dynamic, we then set the vtable pointers to null.

	pRepo->p_vtable = NULL;
}

//////////////////////////////////////////////////////////////////

void sg_repo__query_implementation__list_vtables(SG_context * pCtx, SG_vhash ** pp_vhash)
{
	SG_vhash * pvh = NULL;

	SG_ERR_CHECK_RETURN(  SG_VHASH__ALLOC(pCtx, &pvh)  );

	SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pvh, SG_RIDESC_STORAGE__FS2)  );
	SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pvh, SG_RIDESC_STORAGE__FS3)  );
	SG_ERR_CHECK(  SG_vhash__add__null(pCtx, pvh, SG_RIDESC_STORAGE__SQLITE)  );

	*pp_vhash = pvh;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);
}
