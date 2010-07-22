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

/*
 * sg_tag.c
 *
 *  Created on: May 26, 2010
 *      Author: jeremy
 */

#include "sg.h"



void SG_tag__remove(SG_context * pCtx, SG_repo * pRepo, const char* pszRev, SG_bool bRev, SG_uint32 count_args, const char** paszArgs)
{
    SG_audit q;
	char* psz_hid_given = NULL;
	char* psz_hid_assoc_with_tag = NULL;
	SG_uint32 count_valid_tags = 0;
	const char** paszValidArgs = NULL;
	SG_uint32 i = 0;

    if (0 == count_args)
		return;

    SG_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW,SG_AUDIT__WHO__FROM_SETTINGS)  );

	if (pszRev)
	{
		if (bRev)
			SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, pszRev, &psz_hid_given);
		else
			SG_vc_tags__lookup__tag(pCtx, pRepo, pszRev, &psz_hid_given);

		if (SG_context__has_err(pCtx))
		{
			if (SG_context__err_equals(pCtx, SG_ERR_AMBIGUOUS_ID_PREFIX))
			{
				SG_context__push_level(pCtx);
				SG_console(pCtx, SG_CS_STDERR, "The revision or tag could not be found:  %s\n", pszRev);
				SG_context__pop_level(pCtx);
				SG_ERR_RETHROW;
			}
			else
				SG_ERR_RETHROW;
		}
	}

	// SG_vc_tags__remove will throw on the first non-existant tag and stop
	// we don't want to issue errors, just warnings and keep going
	// weed out all the invalid tags here before calling SG_vc_tags__remove
	SG_ERR_CHECK(  SG_alloc(pCtx, count_args, sizeof(const char*), &paszValidArgs)  );
	for (i =0; i < count_args; i++)
	{
		SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, paszArgs[i], &psz_hid_assoc_with_tag)  );
		if (psz_hid_assoc_with_tag) // tag exists
		{
			if (psz_hid_given)
			{
				if (strcmp(psz_hid_given, psz_hid_assoc_with_tag) == 0) // tag is assoc with given changeset
					paszValidArgs[count_valid_tags++] = paszArgs[i];
				else // tag not assoc with given changeset, no error, but warn
					SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT, "Tag not associated with given revision:  %s\n", paszArgs[i])  );
			}
			else
				paszValidArgs[count_valid_tags++] = paszArgs[i];
		}
		else
		{
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDOUT, "Tag not found:  %s\n", paszArgs[i])  );
		}
		SG_NULLFREE(pCtx, psz_hid_assoc_with_tag);
		psz_hid_assoc_with_tag = NULL;
	}

	SG_ERR_CHECK(  SG_vc_tags__remove(pCtx, pRepo, &q, count_valid_tags, paszValidArgs)  );


fail:
	SG_NULLFREE(pCtx, paszValidArgs);
	SG_NULLFREE(pCtx, psz_hid_given);
	SG_NULLFREE(pCtx, psz_hid_assoc_with_tag);

}

void SG_tag__add_tags(SG_context * pCtx, SG_repo * pRepo, SG_pendingtree * pPendingTree, const char* psz_spec_cs, SG_bool bRev, SG_bool bForce, const char** ppszTags, SG_uint32 count_args)
{
	SG_pathname* pPathCwd = NULL;
	char* psz_hid_cs = NULL;
	SG_audit q;
	SG_uint32 i = 0;
	char * psz_current_hid_with_that_tag = NULL;
	SG_bool bFreePendingTree = SG_FALSE;

	SG_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW,SG_AUDIT__WHO__FROM_SETTINGS)  );

	// TODO 4/21/10 pendingtree contains a pRepo inside it.  we should
	// TODO 4/21/10 refactor this to alloc the pendingtree first and then
	// TODO 4/21/10 just borrow the pRepo from it.



	if (psz_spec_cs)
	{
		if (bRev)
		{
			SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, psz_spec_cs, &psz_hid_cs)  );
		}
		else
		{
			SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, psz_spec_cs, &psz_hid_cs)  );
			if (psz_hid_cs == NULL)
				SG_ERR_THROW(SG_ERR_TAG_NOT_FOUND);
		}
	}
	else
	{
		// tag the current baseline.
		//
		// when we have an uncomitted merge, we will have more than one parent.
		// what does this command mean then?  It feels like we we should throw
		// an error and say that you have to commit first.

		const SG_varray * pva_wd_parents;		// we do not own this
		const char * psz_hid_parent_0;			// we do not own this
		SG_uint32 nrParents;

		if (pPendingTree == NULL)
		{

			SG_ERR_CHECK(  SG_pendingtree__alloc_from_cwd(pCtx, SG_TRUE, &pPendingTree)  );
			bFreePendingTree = SG_TRUE;
		}
		SG_ERR_CHECK(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pva_wd_parents)  );
		SG_ERR_CHECK(  SG_varray__count(pCtx, pva_wd_parents, &nrParents)  );
		if (nrParents > 1)
			SG_ERR_THROW(  SG_ERR_CANNOT_DO_WHILE_UNCOMMITTED_MERGE  );

		SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_wd_parents, 0, &psz_hid_parent_0)  );
		SG_ERR_CHECK(  SG_strdup(pCtx, psz_hid_parent_0, &psz_hid_cs)  );
	}

	if (!bForce)
	{
		//Go through and check all tags to make sure that they are not already applied.
		for (i = 0; i < count_args; i++)
		{
			const char * pszTag = ppszTags[i];
			SG_ERR_IGNORE(  SG_vc_tags__lookup__tag(pCtx, pRepo, pszTag, &psz_current_hid_with_that_tag)  );
			if (psz_current_hid_with_that_tag != NULL && 0 != strcmp(psz_current_hid_with_that_tag, psz_hid_cs)) //The tag has been applied, but not to the given changeset.
				SG_ERR_THROW(SG_ERR_TAG_ALREADY_EXISTS);
			SG_NULLFREE(pCtx, psz_current_hid_with_that_tag);
		}
	}
	for (i = 0; i < count_args; i++)
	{
		const char * pszTag = ppszTags[i];
		SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, pszTag, &psz_current_hid_with_that_tag)  );
		if (psz_current_hid_with_that_tag == NULL || 0 != strcmp(psz_current_hid_with_that_tag, psz_hid_cs))
		{
			//The tag has not been applied, or it's been applied to a different dagnode.
			if ( psz_current_hid_with_that_tag != NULL && bForce)  //Remove it, if it's already there
					SG_ERR_CHECK(  SG_vc_tags__remove(pCtx, pRepo, &q, 1, &pszTag)  );
			SG_ERR_CHECK(  SG_vc_tags__add(pCtx, pRepo, psz_hid_cs, pszTag, &q)  );
		}
		SG_NULLFREE(pCtx, psz_current_hid_with_that_tag);
	}

fail:
	SG_NULLFREE(pCtx, psz_current_hid_with_that_tag);
	if (bFreePendingTree == SG_TRUE)
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_NULLFREE(pCtx, psz_hid_cs);
	SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
}
