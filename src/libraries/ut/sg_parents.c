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
 * sg_parents.c
 *
 *  Created on: Apr 19, 2010
 *      Author: jeremy
 */
#include "sg.h"


void SG_parents__list(SG_context * pCtx,
					  SG_repo * pRepo,
					  SG_pendingtree * pPendingTree,
					  const char * psz_rev_or_tag,
					  SG_bool bRev,
					  const char* pszFilePath,
					  SG_stringarray ** pStringArrayResults)
{
	char *  pszHid = NULL;
	SG_bool bDisposeOfPendingTree = SG_FALSE;
	SG_stringarray * pTempStringArrayResults = NULL;

	if (psz_rev_or_tag != NULL)
	{
		if (bRev)
		{
			SG_ERR_CHECK(  SG_repo__hidlookup__dagnode(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, psz_rev_or_tag, &pszHid)  );
			if (!pszHid)
			{
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Could not locate changeset:  %s\n", psz_rev_or_tag)  );
				goto fail;
			}
		}
		else
		{
			SG_ERR_CHECK(  SG_vc_tags__lookup__tag(pCtx, pRepo, psz_rev_or_tag, &pszHid)  );
			if (!pszHid)
			{
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "Could not locate changeset for tag:  %s\n", psz_rev_or_tag)  );
				goto fail;
			}
		}

	}
	//If a file argument is given, the revision in which the file was last changed (before the working
	//directory revision or the argument to --rev if given) is printed
	if (pszFilePath != NULL)
	{
		SG_uint32 nRevCount = 0;
		SG_varray * pVArrayResults = NULL;
		if (pszHid != NULL)
		{
			nRevCount = 1;
		}
		//This is basically history -max 1 file.
		SG_ERR_CHECK(  SG_history__query(pCtx, pPendingTree, pRepo, 1, (const char **)&pszFilePath, (const char * const*)&pszHid, nRevCount, NULL, NULL, 1, SG_INT64_MIN, SG_INT64_MAX, SG_FALSE, SG_TRUE /*force the dag walk*/, &pVArrayResults)  );
		if (pVArrayResults != NULL)
		{
			SG_uint32 nResultsCount = 0;
			SG_uint32 nResultsIndex = 0;
			SG_vhash * pvh_currentParent = NULL;
			const char * pszParentHid = NULL;

			SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &pTempStringArrayResults, 1));
			SG_ERR_CHECK(  SG_varray__count(pCtx, pVArrayResults, &nResultsCount)  );
			for (nResultsIndex = 0; nResultsIndex < nResultsCount; nResultsIndex++)
			{
				SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pVArrayResults, nResultsIndex, &pvh_currentParent)  );
				SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_currentParent,  "changeset_id", &pszParentHid)  );
				SG_ERR_CHECK(  SG_stringarray__add(pCtx, pTempStringArrayResults, (char*)pszParentHid)  );
			}
			SG_VARRAY_NULLFREE(pCtx, pVArrayResults);
		}
	}
	else
	{
		if (pszHid == NULL)
		{
			// They want the baseline of the current working folder.

			const SG_varray * pva_wd_parents;			// we do not own this
			SG_uint32 k, nrParents;
			SG_bool bIgnoreWarnings = SG_FALSE;

			if (pPendingTree == NULL)
			{
				SG_ERR_CHECK(  SG_pendingtree__alloc_from_cwd(pCtx, bIgnoreWarnings, &pPendingTree)  );
				bDisposeOfPendingTree = SG_TRUE;
			}
			SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &pTempStringArrayResults, 1));
			SG_ERR_CHECK(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pva_wd_parents)  );
			SG_ERR_CHECK(  SG_varray__count(pCtx, pva_wd_parents, &nrParents)  );
			for (k=0; k<nrParents; k++)
			{
				const char * psz_hid_parent_k;

				SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_wd_parents, k, &psz_hid_parent_k)  );
				SG_ERR_CHECK(  SG_stringarray__add(pCtx, pTempStringArrayResults, psz_hid_parent_k)  );
			}
		}
		else
		{
			//They want the parents of a specific revision.
			SG_dagnode * pCurrentDagnode = NULL;
			SG_rbtree * prb_parents = NULL;
			SG_rbtree_iterator * pit = NULL;
			const char * pszParentID = NULL;
			SG_bool b = SG_FALSE;

			SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, pszHid, &pCurrentDagnode)  );
			SG_ERR_CHECK(  SG_dagnode__get_parents__rbtree_ref(pCtx, pCurrentDagnode, &prb_parents)  );
			if (prb_parents)
			{
				SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &pTempStringArrayResults, 1));
				SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_parents, &b, &pszParentID, NULL)  );
				while (b)
				{
					SG_ERR_CHECK(  SG_stringarray__add(pCtx, pTempStringArrayResults, (char*)pszParentID)  );
					SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, NULL, NULL)  );
				}
				SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
			}
			SG_DAGNODE_NULLFREE(pCtx, pCurrentDagnode);
		}
	}
	*pStringArrayResults =  pTempStringArrayResults;
fail:
	if (bDisposeOfPendingTree)
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	if (pszHid)
		SG_NULLFREE(pCtx, pszHid);

}

