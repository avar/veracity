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

//This is used to pass around caches and repos while calling back to fill in the history results details.
struct _fill_in_result_details_data
{
	SG_repo *		pRepo;
	SG_bool			bChildren;
};

void _sg_history_find_treenodeentry_in_one_of_these_paths(SG_context * pCtx, SG_repo * pRepo, SG_treenode * pTreenodeRoot, const char * pszSearchGID, SG_int32 vectorIndex, SG_vector * pVectorKnownPaths, SG_vector * pVectorCurrentPathIndex, SG_treenode_entry ** ppTreeNodeEntry)
{
	SG_uint32 pathVectorIndex = 0;
	SG_uint32 pathVectorCount = 0;
	const char* pszSearchPath = NULL;
	SG_uint32 currentPathIndex = 0;
	char* pGID = NULL;
	SG_stringarray * pStringArrayPathsForCurrentGID = NULL;
    SG_uint32 countPathsForCurrentGID = 0;
	SG_treenode_entry * ptne = NULL;

	SG_ERR_CHECK(  SG_vector__get(pCtx, pVectorCurrentPathIndex, vectorIndex, (void**)&currentPathIndex)  );

	SG_ERR_CHECK(  SG_vector__get(pCtx, pVectorKnownPaths, vectorIndex, (void**)&pStringArrayPathsForCurrentGID)  );

	if (pStringArrayPathsForCurrentGID  == NULL)
	{
		*ppTreeNodeEntry = NULL;
		return;
	}
    SG_ERR_CHECK(  SG_stringarray__count(pCtx, pStringArrayPathsForCurrentGID, &countPathsForCurrentGID)  );
	if(countPathsForCurrentGID==0)
	{
		*ppTreeNodeEntry = NULL;
		return;
	}
	SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pStringArrayPathsForCurrentGID, currentPathIndex, &pszSearchPath)  );

	SG_ERR_CHECK(  SG_treenode__find_treenodeentry_by_path(pCtx, pRepo, pTreenodeRoot, pszSearchPath, &pGID, &ptne)  );
	if (ptne == NULL || strcmp(pGID, pszSearchGID) != 0)
	{
		if (ptne != NULL)
		{
			SG_TREENODE_ENTRY_NULLFREE(pCtx, ptne);
			SG_NULLFREE(pCtx, pGID);
		}
		//We didn't find it at the last path that we tried.  Retry all of them, until we get a hit.
		//Then cache that path index in the pVectorCurrentPathIndex vector.
		SG_ERR_CHECK(  SG_stringarray__count(pCtx, pStringArrayPathsForCurrentGID, &pathVectorCount )  );

		for (pathVectorIndex = 0; pathVectorIndex < pathVectorCount; pathVectorIndex++)
		{
			SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pStringArrayPathsForCurrentGID, pathVectorIndex, &pszSearchPath)  );
			SG_ERR_CHECK(  SG_treenode__find_treenodeentry_by_path(pCtx, pRepo, pTreenodeRoot, pszSearchPath, &pGID, &ptne)  );
			if (ptne != NULL && strcmp(pGID, pszSearchGID) == 0)
			{
				//Found it!
				SG_ERR_CHECK(  SG_vector__set(pCtx, pVectorCurrentPathIndex, vectorIndex, (void*)pathVectorIndex)  );
				break;
			}
			SG_NULLFREE(pCtx, pGID);
			SG_TREENODE_ENTRY_NULLFREE(pCtx, ptne);
			ptne = NULL;
		}
	}
	SG_NULLFREE(pCtx, pGID);
	*ppTreeNodeEntry = ptne;
	return;
fail:
	return;
}

void SG_history__get_id_and_parents(SG_context* pCtx, SG_repo* pRepo, SG_dagnode * pCurrentDagnode, SG_vhash** ppvh_changesetDescription)
{
    SG_bool b = SG_FALSE;
    SG_rbtree_iterator * pit = NULL;
    SG_rbtree * prb_parents = NULL;
    SG_varray * pVArrayParents = NULL;
    SG_varray * pVArrayParents_ref = NULL;
    const char * pszParentID = NULL;
    const char* pszDagNodeHID = NULL;

    SG_UNUSED(pRepo);

    SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pCurrentDagnode, &pszDagNodeHID)  );
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, *ppvh_changesetDescription, "changeset_id", pszDagNodeHID)  );

    SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pVArrayParents)  );
    pVArrayParents_ref = pVArrayParents;
    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, *ppvh_changesetDescription, "parents", &pVArrayParents)  );
    SG_ERR_CHECK(  SG_dagnode__get_parents__rbtree_ref(pCtx, pCurrentDagnode, &prb_parents)  );
    if (prb_parents)
    {

        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_parents, &b, &pszParentID, NULL)  );
        while (b)
        {
            SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pVArrayParents_ref, pszParentID)  );
            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &pszParentID, NULL)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    }

fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_VARRAY_NULLFREE(pCtx, pVArrayParents);
}

void _sg_history_check_dagnode_for_inclusion(SG_context * pCtx, SG_repo * pRepo, SG_dagnode * pCurrentDagnode, SG_bool bHistoryOnRoot, SG_stringarray * pStringArrayGIDs, SG_vector * pVectorKnownPaths, SG_vector * pVectorCurrentPathIndex, SG_rbtree * pCandidateChangesets, SG_varray * pArrayReturnResults)
{
	SG_changeset * pChangeset = NULL;
	SG_changeset * pChangesetParent = NULL;
	SG_treenode_entry * ptne = NULL;
	SG_treenode_entry * ptneParent = NULL;
	const char* pszHidTreeNode = NULL;
	const char* pszHidTreeNodeParent = NULL;
	SG_treenode * pTreenodeRoot = NULL;
	SG_treenode * pTreenodeRootParent = NULL;
	SG_vhash * pvh_changesetDescription = NULL;
	SG_treenode_entry ** ptneChildTreeNodeEntries = NULL;
	const char* pszCurrentSearchGID = NULL;
	SG_rbtree * pRBT_parents = NULL;
	SG_rbtree_iterator * pIterator = NULL;

	SG_uint32 numberOfGIDs = 0;
	SG_bool bEqual = SG_FALSE;
	SG_uint32 currentGID_index = 0;
	SG_uint32 parentCount = 0;
	SG_bool bIncludeChangeset = SG_FALSE;
	SG_bool bContinue = SG_FALSE;
	SG_bool bFoundItInCandidateList = SG_FALSE;


	void * unusedPointer;
	const char * pszDagNodeHID;
	const char * pszParentHid = NULL;

	SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pCurrentDagnode, &pszDagNodeHID)  );
	if (pCandidateChangesets != NULL)
	{
		SG_ERR_CHECK(  SG_rbtree__find(pCtx, pCandidateChangesets, pszDagNodeHID, &bFoundItInCandidateList, &unusedPointer)  );
		//If the DagNode HID is not in the Candidate list, skip it.
		if (bFoundItInCandidateList == SG_FALSE)
			return;
	}
	//bHistoryOnRoot true means that we include every dagnode.
	if (bHistoryOnRoot == SG_FALSE)
	{
		SG_ERR_CHECK(  SG_dagnode__count_parents(pCtx, pCurrentDagnode, &parentCount)  );
		SG_ERR_CHECK(  SG_dagnode__get_parents__rbtree_ref(pCtx, pCurrentDagnode, &pRBT_parents)  );

		SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, pszDagNodeHID, &pChangeset)  );
		SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pChangeset, &pszHidTreeNode) );
		SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszHidTreeNode, &pTreenodeRoot)  );

		SG_ERR_CHECK(  SG_stringarray__count(pCtx, pStringArrayGIDs, &numberOfGIDs )  );

		SG_ERR_CHECK(  SG_allocN(pCtx, numberOfGIDs, ptneChildTreeNodeEntries)  );
		for(currentGID_index = 0; currentGID_index < numberOfGIDs; currentGID_index++)
		{
			if (ptneChildTreeNodeEntries[currentGID_index] == NULL)
			{
				//Find the treenodeentry in the child.
				SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pStringArrayGIDs, currentGID_index, &pszCurrentSearchGID)  );
				SG_ERR_CHECK(  _sg_history_find_treenodeentry_in_one_of_these_paths(pCtx, pRepo, pTreenodeRoot, pszCurrentSearchGID, currentGID_index, pVectorKnownPaths, pVectorCurrentPathIndex, &ptne) );
				ptneChildTreeNodeEntries[currentGID_index] = ptne;

				if (ptne != NULL && parentCount == 0) //Special case for the @ sign.  It gets added in the one changeset with no parent.
				{
					bIncludeChangeset = SG_TRUE;
					break;
				}

			}
		}

		if (parentCount > 0)
		{
			SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, pRBT_parents, &bContinue, &pszParentHid, &unusedPointer) );
			while (bContinue == SG_TRUE && bIncludeChangeset == SG_FALSE)
			{
				//Load the parent changeset
				SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, pszParentHid, &pChangesetParent)  );
				SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pChangesetParent, &pszHidTreeNodeParent) );
				SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszHidTreeNodeParent, &pTreenodeRootParent)  );
				for(currentGID_index = 0; currentGID_index < numberOfGIDs; currentGID_index++)
				{
					SG_ERR_CHECK(  _sg_history_find_treenodeentry_in_one_of_these_paths(pCtx, pRepo, pTreenodeRootParent, pszCurrentSearchGID, currentGID_index, pVectorKnownPaths, pVectorCurrentPathIndex, &ptneParent) );

					if ( (ptneChildTreeNodeEntries[currentGID_index] == NULL && ptneParent != NULL) || (ptneParent == NULL && ptneChildTreeNodeEntries[currentGID_index] != NULL))
					{
						//The thing was deleted or added (possibly secondarily deleted).  hrmph
						bIncludeChangeset = SG_TRUE;
					}
					else if ( ptneChildTreeNodeEntries[currentGID_index] != NULL && ptneParent != NULL )
					{
						//They're both not null.  Compare them.
						SG_ERR_CHECK(  SG_treenode_entry__equal(pCtx, ptneChildTreeNodeEntries[currentGID_index], ptneParent, &bEqual)  );
						if (bEqual != SG_TRUE)
						{
							bIncludeChangeset = SG_TRUE;
						}
					}
					SG_TREENODE_ENTRY_NULLFREE(pCtx, ptneParent);
					if (bIncludeChangeset == SG_TRUE)  //We got a hit, so we don't need to check any more parents.
						break;
				}

				SG_TREENODE_NULLFREE(pCtx, pTreenodeRootParent);
				SG_CHANGESET_NULLFREE(pCtx, pChangesetParent);
				if (bIncludeChangeset == SG_TRUE)  //We got a hit, so we don't need to check any more GIDs.
						break;
				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &bContinue, &pszParentHid, NULL)  );
			}
			SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
		}
		for(currentGID_index = 0; currentGID_index < numberOfGIDs; currentGID_index++)
		{
			SG_TREENODE_ENTRY_NULLFREE(pCtx, ptneChildTreeNodeEntries[currentGID_index]);
		}
	}
	else //bHistoryOnRoot is TRUE.
		bIncludeChangeset = SG_TRUE;

	if (bIncludeChangeset == SG_TRUE)
	{
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_changesetDescription)  );
		SG_ERR_CHECK(  SG_history__get_id_and_parents(pCtx, pRepo, pCurrentDagnode, &pvh_changesetDescription)  );
		SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pArrayReturnResults, &pvh_changesetDescription)  );
		SG_VHASH_NULLFREE(pCtx, pvh_changesetDescription);

	}
	SG_NULLFREE(pCtx, ptneChildTreeNodeEntries);
	SG_TREENODE_NULLFREE(pCtx, pTreenodeRoot);
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	return;

fail:
	SG_NULLFREE(pCtx, ptneChildTreeNodeEntries);
	SG_TREENODE_NULLFREE(pCtx, pTreenodeRoot);
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
    SG_VHASH_NULLFREE(pCtx, pvh_changesetDescription);
}

static void sg_history__slice__one(
        SG_context* pCtx,
        SG_vector* pvec,
        const char* psz_field,
        SG_varray** ppva
        )
{
    SG_varray* pva = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva)  );
    SG_ERR_CHECK(  SG_vector__length(pCtx, pvec, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh_orig = NULL;
        const char* psz_val = NULL;

        SG_ERR_CHECK(  SG_vector__get(pCtx, pvec, i, (void**) &pvh_orig)  );

        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_orig, psz_field, &psz_val)  );
        SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, psz_val)  );
    }

    *ppva = pva;
    pva = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
}

static void sg_history__slice__audits(
        SG_context* pCtx,
        SG_vector* pvec,
        SG_rbtree* prb_users,
        SG_varray** ppva
        )
{
    SG_varray* pva = NULL;
    SG_vhash* pvh_copy = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva)  );
    SG_ERR_CHECK(  SG_vector__length(pCtx, pvec, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh_orig = NULL;
        const char* psz_val = NULL;
        SG_int64 i_time = 0;
        SG_vhash* pvh_user = NULL;
        SG_bool b_found_user = SG_FALSE;

        SG_ERR_CHECK(  SG_vector__get(pCtx, pvec, i, (void**) &pvh_orig)  );
        SG_ERR_CHECK(  SG_varray__appendnew__vhash(pCtx, pva, &pvh_copy)  );

        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_orig, "who", &psz_val)  );
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_copy, "who", psz_val)  );
        SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_users, psz_val, &b_found_user, (void**) &pvh_user)  );
        if (b_found_user && pvh_user)
        {
            const char* psz_email = NULL;

            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_user, "email", &psz_email)  );
            SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_copy, "email", psz_email)  );
        }

        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx, pvh_orig, "when", &i_time)  );
        SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_copy, "when", i_time)  );
    }

    *ppva = pva;
    pva = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
}

static void sg_history__comments(
        SG_context* pCtx,
        SG_vector* pvec,
        SG_rbtree* prb_users,
        SG_varray** ppva
        )
{
    SG_varray* pva = NULL;
    SG_vhash* pvh_copy = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_bool b_has_history = SG_FALSE;

    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva)  );
    SG_ERR_CHECK(  SG_vector__length(pCtx, pvec, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh_orig = NULL;
        SG_varray* pva_history = NULL;
        SG_varray* pva_audits = NULL;
        SG_uint32 count_audits = 0;
        SG_uint32 count_entries = 0;
        SG_uint32 j = 0;
        SG_uint32 k = 0;

        SG_ERR_CHECK(  SG_vector__get(pCtx, pvec, i, (void**) &pvh_orig)  );
        SG_ERR_CHECK(  SG_vhash__alloc__copy(pCtx, &pvh_copy, pvh_orig)  );

        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_copy, "history", &b_has_history)  );
        if (b_has_history)
        {
            SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_copy, "history", &pva_history)  );
            SG_ERR_CHECK(  SG_varray__count(pCtx, pva_history, &count_entries)  );

            for (k=0; k<count_entries; k++)
            {
                SG_vhash* pvh_entry = NULL;

                SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_history, k, &pvh_entry)  );

                SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_entry, "audits", &pva_audits)  );
                SG_ERR_CHECK(  SG_varray__count(pCtx, pva_audits, &count_audits)  );
                for (j=0; j<count_audits; j++)
                {
                    SG_vhash* pvh_audit = NULL;
                    const char* psz_who = NULL;
                    SG_vhash* pvh_user = NULL;
                    SG_bool b_found = SG_FALSE;

                    SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_audits, j, &pvh_audit)  );
                    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_audit, "who", &psz_who)  );
                    SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_users, psz_who, &b_found, (void**) &pvh_user)  );
                    if (b_found && pvh_user)
                    {
                        const char* psz_email = NULL;

                        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_user, "email", &psz_email)  );
                        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_audit, "email", psz_email)  );
                    }
                }
            }
        }

        SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pva, &pvh_copy)  );
    }

    *ppva = pva;
    pva = NULL;

fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
}

static void sg_history__details_for_one_changeset(
        SG_context* pCtx,
        SG_vhash * pCurrentHash,
        SG_rbtree* prb_all_audits,
        SG_rbtree* prb_all_stamps,
        SG_rbtree* prb_all_tags,
        SG_rbtree* prb_all_comments,
        SG_rbtree* prb_all_users
        )
{
    SG_bool b_found = SG_FALSE;
    SG_vector* pvec = NULL;
    SG_varray * pva_audits = NULL;
    SG_varray * pva_tags = NULL;
    SG_varray * pva_stamps = NULL;
    SG_varray * pva_comments = NULL;
    const char* psz_csid = NULL;

    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pCurrentHash, "changeset_id", &psz_csid)  );

    // -------- audits --------
    b_found = SG_FALSE;
    pvec = NULL;
    SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_all_audits, psz_csid, &b_found, (void**) &pvec)  );
    if (pvec)
    {
        SG_ERR_CHECK(  sg_history__slice__audits(pCtx, pvec, prb_all_users, &pva_audits)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_audits)  );
    }
    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pCurrentHash, "audits", &pva_audits)  );

    // -------- tags --------
    b_found = SG_FALSE;
    pvec = NULL;
    SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_all_tags, psz_csid, &b_found, (void**) &pvec)  );
    if (pvec)
    {
        SG_ERR_CHECK(  sg_history__slice__one(pCtx, pvec, "tag", &pva_tags)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_tags)  );
    }
    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pCurrentHash, "tags", &pva_tags)  );

    // -------- stamps --------
    b_found = SG_FALSE;
    pvec = NULL;
    SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_all_stamps, psz_csid, &b_found, (void**) &pvec)  );
    if (pvec)
    {
        SG_ERR_CHECK(  sg_history__slice__one(pCtx, pvec, "stamp", &pva_stamps)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_stamps)  );
    }
    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pCurrentHash, "stamps", &pva_stamps)  );

    // -------- comments --------
    b_found = SG_FALSE;
    pvec = NULL;
    SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_all_comments, psz_csid, &b_found, (void**) &pvec)  );
    if (pvec)
    {
        SG_ERR_CHECK(  sg_history__comments(pCtx, pvec, prb_all_users, &pva_comments)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_comments)  );
    }
    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pCurrentHash, "comments", &pva_comments)  );

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_audits);
    SG_VARRAY_NULLFREE(pCtx, pva_tags);
    SG_VARRAY_NULLFREE(pCtx, pva_stamps);
    SG_VARRAY_NULLFREE(pCtx, pva_comments);
}

void SG_history__get_changeset_comments(SG_context* pCtx, SG_repo* pRepo, const char* pszDagNodeHID, SG_varray** ppvaComments)
{
	SG_varray* pvaComments = NULL;
	SG_varray* pvaResults = NULL;
	SG_bool b_has_history;
	SG_uint32 count;
	SG_uint32 i;

	SG_VARRAY__ALLOC(pCtx, &pvaResults);
	SG_ERR_CHECK(  SG_vc_comments__lookup(pCtx, pRepo, pszDagNodeHID, &pvaComments)  );
	SG_ERR_CHECK(  SG_varray__count(pCtx, pvaComments, &count)  );

	for (i=0; i<count; i++)
	{
		SG_vhash* pvh_orig = NULL;
		SG_vhash* pvh_copy = NULL;
		SG_varray* pva_history = NULL;
		SG_uint32 count_entries;
		SG_uint32 k;

		SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pvaComments, i, &pvh_orig)  );
		SG_ERR_CHECK(  SG_VHASH__ALLOC__COPY(pCtx, &pvh_copy, pvh_orig)  );

		SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_copy, "history", &b_has_history)  );
		if (b_has_history)
		{
			SG_varray* pva_audits = NULL;

			SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_copy, "history", &pva_history)  );
			SG_ERR_CHECK(  SG_varray__count(pCtx, pva_history, &count_entries)  );

            for (k=0; k<count_entries; k++)
            {
                SG_vhash* pvh_entry = NULL;
				SG_uint32 count_audits;
				SG_uint32 j;

                SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_history, k, &pvh_entry)  );

                SG_ERR_CHECK(  SG_vhash__get__varray(pCtx, pvh_entry, "audits", &pva_audits)  );
                SG_ERR_CHECK(  SG_varray__count(pCtx, pva_audits, &count_audits)  );
                for (j=0; j<count_audits; j++)
                {
                    SG_vhash* pvh_audit = NULL;
                    const char* psz_who = NULL;
                    SG_vhash* pvh_user = NULL;

                    SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_audits, j, &pvh_audit)  );
                    SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_audit, "who", &psz_who)  );
                    SG_ERR_CHECK(  SG_user__lookup_by_userid(pCtx, pRepo, psz_who, &pvh_user)  );
                    if (pvh_user)
                    {
                        const char* psz_email = NULL;

                        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_user, "email", &psz_email)  );
                        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_audit, "email", psz_email)  );
                    }
					SG_VHASH_NULLFREE(pCtx, pvh_user);
				}
            }
        }
		SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pvaResults, &pvh_copy) );
		SG_VHASH_NULLFREE(pCtx, pvh_copy);
	}

	*ppvaComments = pvaResults;
	pvaResults = NULL;
fail:
	SG_VARRAY_NULLFREE(pCtx, pvaComments);
	SG_VARRAY_NULLFREE(pCtx, pvaResults);
}

void SG_history__get_parents_description(SG_context* pCtx, SG_repo* pRepo, const char* pChangesetHid, SG_vhash** ppvhParents)
{	  
	SG_bool b = SG_FALSE;
    SG_rbtree_iterator * pit = NULL;
    SG_rbtree * prbParents = NULL;
    SG_vhash * pvhParents = NULL;
    const char * pszParentID = NULL;
	SG_vhash* pvhAuditResult = NULL;
	SG_uint32 count_audits;
	SG_varray* pvaAudits = NULL;
	SG_varray* pvaAuditsResult = NULL;
	SG_varray* pvaComments = NULL;			
	SG_vhash* pvh_result = NULL;
	SG_dagnode* pdn = NULL;


	SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, pChangesetHid, &pdn)  );

    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhParents)  );    
   
    SG_ERR_CHECK(  SG_dagnode__get_parents__rbtree_ref(pCtx, pdn, &prbParents)  );
	
    if (prbParents)
    {       
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prbParents, &b, &pszParentID, NULL)  );
		while (b)
		{				
			SG_uint32 i; 

			SG_VHASH__ALLOC(pCtx, &pvh_result);
			SG_VARRAY__ALLOC(pCtx, &pvaAuditsResult);
			SG_ERR_CHECK(  SG_audit__lookup(pCtx, pRepo, pszParentID, SG_DAGNUM__VERSION_CONTROL, &pvaAudits)  );
			SG_ERR_CHECK(  SG_varray__count(pCtx, pvaAudits, &count_audits)  );
			for (i=0; i<count_audits; i++)
			{
				SG_vhash* pvh = NULL;  
				SG_vhash* pvh_user = NULL;     
				const char* psz_userid = NULL;				
									
				SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pvaAudits, i, &pvh)  );
				SG_ERR_CHECK(  SG_VHASH__ALLOC__COPY(pCtx, &pvhAuditResult, pvh)  );  
						
				SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhAuditResult, "who", &psz_userid)  );		
				SG_ERR_CHECK(  SG_user__lookup_by_userid(pCtx, pRepo, psz_userid, &pvh_user)  );
				if (pvh_user)
				{
					const char* psz_email = NULL;
					SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_user, "email", &psz_email)  );
					SG_ERR_CHECK( SG_vhash__add__string__sz(pCtx, pvhAuditResult, "email", psz_email) );
				}
				     
				SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pvaAuditsResult, &pvhAuditResult)  );	
				SG_VHASH_NULLFREE(pCtx, pvhAuditResult);
				SG_VHASH_NULLFREE(pCtx, pvh_user);
			}	
			
			SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh_result, "audits", &pvaAuditsResult)  );
			
			SG_ERR_CHECK(  SG_history__get_changeset_comments(pCtx, pRepo, pszParentID, &pvaComments)  );
			SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh_result, "comments", &pvaComments)  );		
			SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhParents,  pszParentID, &pvh_result)  );
			SG_VHASH_NULLFREE(pCtx, pvh_result);
			SG_VARRAY_NULLFREE(pCtx, pvaComments);	
			SG_VARRAY_NULLFREE(pCtx, pvaAudits);
			SG_VHASH_NULLFREE(pCtx, pvh_result);
			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &pszParentID, NULL)  );
		}
		SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);			
		
    }
	*ppvhParents = pvhParents;
	pvhParents = NULL;
	
fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_VHASH_NULLFREE(pCtx, pvhParents);
	SG_VARRAY_NULLFREE(pCtx, pvaComments);
	SG_VARRAY_NULLFREE(pCtx, pvaAuditsResult);	
	SG_VHASH_NULLFREE(pCtx, pvh_result);
	SG_VARRAY_NULLFREE(pCtx, pvaAudits);
	SG_VHASH_NULLFREE(pCtx, pvhAuditResult);
	SG_DAGNODE_NULLFREE(pCtx, pdn);
	
}

void SG_history__get_changeset_description(SG_context* pCtx, SG_repo* pRepo, const char* pChangesetHid, SG_bool bChildren, SG_vhash** ppvhChangesetDescription)
{
    SG_rbtree_iterator * pit = NULL;
	SG_rbtree* prbChildren = NULL;
    SG_varray * pvaUsers = NULL;
    SG_varray * pva_tags = NULL;
    SG_varray * pva_stamps = NULL;
    SG_varray * pvaTags = NULL;
    SG_varray * pvaComments = NULL;
	SG_varray * pvaChildren = NULL;
    SG_varray * pvaStamps = NULL;
 	SG_varray * pvaAudits = NULL;
	SG_vhash * pvhParents = NULL;
	SG_uint32 count_audits;
	SG_uint32 i;
	SG_vhash* pvh_result = NULL;
	SG_uint32 parent_count;

 
	SG_ERR_CHECK( SG_VHASH__ALLOC(pCtx, &pvh_result)  );  
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_result, "changeset_id", pChangesetHid)  );

	SG_ERR_CHECK( SG_varray__alloc(pCtx, &pvaUsers)  );
    SG_ERR_CHECK(  SG_audit__lookup(pCtx, pRepo, pChangesetHid, SG_DAGNUM__VERSION_CONTROL, &pvaAudits)  );
	SG_ERR_CHECK(  SG_varray__count(pCtx, pvaAudits, &count_audits)  );
 
    for (i=0; i<count_audits; i++)
    {
		SG_vhash* pvh = NULL;
		SG_vhash* pvh_user = NULL;
        const char* psz_userid = NULL;
        const char* psz_email = NULL;
		SG_vhash* pvhAuditResult = NULL;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pvaAudits, i, &pvh)  );
		SG_ERR_CHECK(  SG_VHASH__ALLOC__COPY(pCtx, &pvhAuditResult, pvh)  );  
	
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhAuditResult, "who", &psz_userid)  );		
        SG_ERR_CHECK(  SG_user__lookup_by_userid(pCtx, pRepo, psz_userid, &pvh_user)  );
        if (pvh_user)
        {
            SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_user, "email", &psz_email)  );
			SG_ERR_CHECK( SG_vhash__add__string__sz(pCtx, pvhAuditResult, "email", psz_email) );
        }
       
		SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pvaUsers, &pvhAuditResult)  );
        SG_VHASH_NULLFREE(pCtx, pvh_user);
		SG_VHASH_NULLFREE(pCtx, pvhAuditResult);
	}

    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh_result, "audits", &pvaUsers)  );
	SG_VARRAY_NULLFREE(pCtx, pvaUsers);
	SG_VARRAY_NULLFREE(pCtx, pvaAudits );

	SG_ERR_CHECK(  SG_history__get_parents_description(pCtx, pRepo, pChangesetHid, &pvhParents)  );	
	SG_ERR_CHECK(  SG_vhash__count(pCtx, pvhParents, &parent_count) );	

	SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh_result, "parents", &pvhParents)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_result, "parent_count", (SG_int64) parent_count)  );
	SG_VHASH_NULLFREE(pCtx, pvhParents);

    SG_ERR_CHECK(  SG_vc_tags__lookup(pCtx, pRepo, pChangesetHid, &pva_tags)  );
    if (pva_tags)
    {
        SG_ERR_CHECK(  SG_zing__extract_one_from_slice__string(pCtx, pva_tags, "tag", &pvaTags)  );
        SG_VARRAY_NULLFREE(pCtx, pva_tags);
    }
    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh_result, "tags", &pvaTags)  );
	SG_VARRAY_NULLFREE(pCtx, pvaTags);

    SG_ERR_CHECK(  SG_vc_stamps__lookup(pCtx, pRepo, pChangesetHid, &pva_stamps)  );
    if (pva_stamps)
    {
        SG_ERR_CHECK(  SG_zing__extract_one_from_slice__string(pCtx, pva_stamps, "stamp", &pvaStamps)  );
        SG_VARRAY_NULLFREE(pCtx, pva_stamps);
    }
    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh_result, "stamps", &pvaStamps)  );
    SG_VARRAY_NULLFREE(pCtx, pva_stamps);

    SG_ERR_CHECK(  SG_history__get_changeset_comments(pCtx, pRepo, pChangesetHid, &pvaComments)  );
    SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh_result, "comments", &pvaComments)  );
    SG_VARRAY_NULLFREE(pCtx, pvaComments);	

	if (bChildren)
	{
		 SG_ERR_CHECK(  SG_repo__fetch_dagnode_children(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, pChangesetHid, &prbChildren)  );
		 SG_ERR_CHECK( SG_rbtree__to_varray__keys_only(pCtx, prbChildren, &pvaChildren)  );
		 SG_ERR_CHECK(  SG_vhash__add__varray(pCtx, pvh_result, "children", &pvaChildren)  );
		 SG_VARRAY_NULLFREE(pCtx, pvaChildren);
		 SG_RBTREE_NULLFREE(pCtx, prbChildren);
	}

	*ppvhChangesetDescription = pvh_result;
	pvh_result = NULL;

fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_VARRAY_NULLFREE(pCtx, pvaUsers);
	SG_VARRAY_NULLFREE(pCtx, pvaAudits );
    SG_VARRAY_NULLFREE(pCtx, pva_tags);
	SG_VHASH_NULLFREE(pCtx, pvh_result);
    SG_VARRAY_NULLFREE(pCtx, pva_stamps);
    SG_VARRAY_NULLFREE(pCtx, pvaTags);
    SG_VARRAY_NULLFREE(pCtx, pvaComments);	
	SG_VARRAY_NULLFREE(pCtx, pvaChildren);
	SG_RBTREE_NULLFREE(pCtx, prbChildren);
	SG_VHASH_NULLFREE(pCtx, pvhParents);
	
}

struct _my_history_dagwalk_data
{
	SG_uint32 nResultLimit;
	SG_stringarray * pStringArrayGIDs;
	SG_vector * pVectorKnownPaths;
	SG_vector * pVectorCurrentPathIndex;
	SG_rbtree * pCandidateChangesets;
	SG_varray * pArrayReturnResults;
	SG_bool bHistoryOnRoot;
};
void _pending_tree__history__dag_walk_callback(SG_context * pCtx, SG_repo * pRepo, void * myData, SG_dagnode * currentDagnode, SG_rbtree * pDagnodeCache, SG_bool * bContinue)
{
	struct _my_history_dagwalk_data * pData = NULL;
	SG_uint32 resultsSoFar = 0;

	SG_UNUSED(pDagnodeCache);
	pData = (struct _my_history_dagwalk_data*)myData;
	SG_ERR_CHECK(  _sg_history_check_dagnode_for_inclusion(pCtx, pRepo, currentDagnode, pData->bHistoryOnRoot, pData->pStringArrayGIDs, pData->pVectorKnownPaths, pData->pVectorCurrentPathIndex, pData->pCandidateChangesets, pData->pArrayReturnResults)  );

	SG_ERR_CHECK(  SG_varray__count(pCtx, pData->pArrayReturnResults, &resultsSoFar)  );
	if (resultsSoFar >= pData->nResultLimit)
		*bContinue = SG_FALSE;

	return;
fail:
	return;

}

void SG_tuple_array__build_reverse_lookup__multiple(
        SG_context * pCtx,
        SG_varray* pva,
        const char* psz_field,
        SG_rbtree** pprb
        )
{
    SG_rbtree* prb = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_vector* pvec_new = NULL;

    SG_ERR_CHECK(  SG_rbtree__alloc(pCtx, &prb)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh = NULL;
        const char* psz_val = NULL;
        SG_vector* pvec = NULL;
        SG_bool b_already = SG_FALSE;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, psz_field, &psz_val)  );
        SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb, psz_val, &b_already, (void**) &pvec)  );
        if (!b_already)
        {
            SG_ERR_CHECK(  SG_vector__alloc(pCtx, &pvec_new, 1)  );
            SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb, psz_val, pvec_new)  );
            pvec = pvec_new;
            pvec_new = NULL;
        }
        SG_ERR_CHECK(  SG_vector__append(pCtx, pvec, pvh, NULL)  );
    }

    *pprb = prb;
    prb = NULL;

fail:
    SG_VECTOR_NULLFREE(pCtx, pvec_new);
    SG_RBTREE_NULLFREE(pCtx, prb);
}

void SG_tuple_array__build_reverse_lookup__single(
        SG_context * pCtx,
        SG_varray* pva,
        const char* psz_field,
        SG_rbtree** pprb
        )
{
    SG_rbtree* prb = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_rbtree__alloc(pCtx, &prb)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh = NULL;
        const char* psz_val = NULL;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, psz_field, &psz_val)  );
#if 1 && defined(DEBUG)
		// TODO 2010/06/11 Jeff here.  My local db has a duplicate record in the users table.
		// TODO            [g23c8723921b24146854c7af8e18a2cee5d400ca46d8711df996b005056c00001]
		{
			SG_bool b_already;
			SG_vhash * pvh_existing;
			SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb, psz_val, &b_already, (void **) &pvh_existing)  );
			if (b_already)
			{
				SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,
										   "TODO build_reverse_lookup__single: Duplicate record [%s]\n",
										   psz_val)  );
				SG_ERR_IGNORE(  SG_vhash_debug__dump_to_console__named(pCtx, pvh_existing, "Existing Record")  );
				SG_ERR_IGNORE(  SG_vhash_debug__dump_to_console__named(pCtx, pvh, "New Record")  );
			}
			else
			{
				SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb, psz_val, pvh)  );
			}
		}
#else
        SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb, psz_val, pvh)  );
#endif
    }

    *pprb = prb;
    prb = NULL;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb);
}

static void sg_history__fill_in_details(
        SG_context * pCtx,
        SG_repo * pRepo,
        SG_varray* pva_history
        )
{
    SG_varray* pva_all_audits = NULL;
    SG_varray* pva_all_users = NULL;
    SG_varray* pva_all_tags = NULL;
    SG_varray* pva_all_stamps = NULL;
    SG_varray* pva_all_comments = NULL;
    SG_rbtree* prb_all_users = NULL;
    SG_rbtree* prb_all_audits = NULL;
    SG_rbtree* prb_all_stamps = NULL;
    SG_rbtree* prb_all_tags = NULL;
    SG_rbtree* prb_all_comments = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_varray* pva_csid_list__comments = NULL;
    SG_varray* pva_csid_list__stamps = NULL;
    SG_varray* pva_csid_list__tags = NULL;
    SG_varray* pva_csid_list__audits = NULL;

    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_csid_list__comments)  );
    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_history, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh = NULL;
        const char* psz_csid = NULL;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_history, i, &pvh)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh, "changeset_id", &psz_csid)  );
        SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva_csid_list__comments, psz_csid)  );
    }
    // TODO it's very silly that we have to make four copies of this list
    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_csid_list__stamps)  );
    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_csid_list__audits)  );
    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_csid_list__tags)  );

    SG_ERR_CHECK(  SG_varray__copy_items(pCtx, pva_csid_list__comments, pva_csid_list__stamps)  );
    SG_ERR_CHECK(  SG_varray__copy_items(pCtx, pva_csid_list__comments, pva_csid_list__audits)  );
    SG_ERR_CHECK(  SG_varray__copy_items(pCtx, pva_csid_list__comments, pva_csid_list__tags)  );

    // TODO if we're getting all the history, it would be
    // faster to just call list_all like we did before

    SG_ERR_CHECK(  SG_vc_comments__list_for_given_changesets(pCtx, pRepo, &pva_csid_list__comments, &pva_all_comments)  );

    SG_ERR_CHECK(  SG_audit__list_for_given_changesets(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, &pva_csid_list__audits, &pva_all_audits)  );
    SG_ERR_CHECK(  SG_vc_stamps__list_for_given_changesets(pCtx, pRepo, &pva_csid_list__stamps, &pva_all_stamps)  );
    SG_ERR_CHECK(  SG_vc_tags__list_for_given_changesets(pCtx, pRepo, &pva_csid_list__tags, &pva_all_tags)  );
    SG_ERR_CHECK(  SG_user__list_all(pCtx, pRepo, &pva_all_users)  );

    SG_ERR_CHECK(  SG_tuple_array__build_reverse_lookup__single(pCtx, pva_all_users, "recid", &prb_all_users)  );
    SG_ERR_CHECK(  SG_tuple_array__build_reverse_lookup__multiple(pCtx, pva_all_audits, "csid", &prb_all_audits)  );
    SG_ERR_CHECK(  SG_tuple_array__build_reverse_lookup__multiple(pCtx, pva_all_stamps, "csid", &prb_all_stamps)  );
    SG_ERR_CHECK(  SG_tuple_array__build_reverse_lookup__multiple(pCtx, pva_all_tags, "csid", &prb_all_tags)  );
    SG_ERR_CHECK(  SG_tuple_array__build_reverse_lookup__multiple(pCtx, pva_all_comments, "csid", &prb_all_comments)  );

    SG_ERR_CHECK(  SG_varray__count(pCtx, pva_history, &count)  );
    for (i=0; i<count; i++)
    {
        SG_vhash* pvh = NULL;

        SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva_history, i, &pvh)  );
        SG_ERR_CHECK(  sg_history__details_for_one_changeset(pCtx, pvh, prb_all_audits, prb_all_stamps, prb_all_tags, prb_all_comments, prb_all_users)  );
    }

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_all_users);
    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, prb_all_audits, (SG_free_callback*) SG_vector__free);
    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, prb_all_stamps, (SG_free_callback*) SG_vector__free);
    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, prb_all_tags, (SG_free_callback*) SG_vector__free);
    SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, prb_all_comments, (SG_free_callback*) SG_vector__free);
    SG_VARRAY_NULLFREE(pCtx, pva_all_tags);
    SG_VARRAY_NULLFREE(pCtx, pva_all_users);
    SG_VARRAY_NULLFREE(pCtx, pva_all_audits);
    SG_VARRAY_NULLFREE(pCtx, pva_all_stamps);
    SG_VARRAY_NULLFREE(pCtx, pva_all_comments);
}

void SG_history__query(
        SG_context * pCtx,
        SG_pendingtree * pPendingTree,
        SG_repo * pRepo,
        SG_uint32 count_args, const char ** paszArgs,
        const char * const* pasz_changesets,
        SG_uint32 nCountChangesets,
        const char* pStrUser,
        const char* pStrStamp,
        SG_uint32 nResultLimit,
        SG_int64 nFromDate,
        SG_int64 nToDate,
        SG_bool bAllLeaves,
        SG_bool bForceDagWalk,
        SG_varray ** ppVArrayResults
        )
{
	SG_bool bPendingTreeWasPassedIn = SG_FALSE;
	SG_varray * pVArrayResults = NULL;
	SG_stringarray * pStringArrayGIDs = NULL;
	SG_vector * pVectorPathArrays = NULL;
	//SG_varray * pVArrayCurrentPaths = NULL;
	SG_pathname * pPathName = NULL;
	SG_stringarray * pStringArrayPaths = NULL;
	SG_rbtree * pRBTreeCandidateChangesets = NULL;
	SG_vector * pVectorCurrentPathIndex = NULL;
	SG_stringarray * pStringArraySearchNodes = NULL;
	SG_vhash * pvh_query = NULL;
	SG_treendx * pTreeNdx = NULL;
	const char * currentGID = NULL;
	SG_pathname* pPathCwd = NULL;
    SG_string* pstr_where = NULL;
	const SG_varray * pva_wd_parents;
	SG_stringarray * pstringarray_csids_with_stamps = NULL;
	SG_bool bFilteredOnUserOrDate = SG_FALSE;
	SG_dagnode * pdnCurrent = NULL;

	SG_uint32 candidateCount = 0;
	SG_bool bUnlimited = SG_FALSE;
	SG_bool bHistoryOnRoot = SG_FALSE;
	SG_bool bFreeArgs = SG_FALSE;
	SG_uint32 i = 0;
	SG_uint32 arrayIndexReturned = 0;
	SG_bool bWalkDag = SG_TRUE;

	if (pPendingTree != NULL)
	{
		bPendingTreeWasPassedIn = SG_TRUE;
	}

    if (count_args == 0)
	{
		bHistoryOnRoot = SG_TRUE;
	}

	if (count_args == 1 && nCountChangesets >= 1 &&  (strcmp(paszArgs[0], "@") == 0  || strcmp(paszArgs[0], "@/") == 0))
	{
		bHistoryOnRoot = SG_TRUE;
		count_args = 0;
	}

	if ((pStrUser == NULL || strcmp(pStrUser, "") == 0) && (pStrStamp == NULL || strcmp(pStrStamp, "") == 0) && nFromDate == 0 && nToDate == SG_INT64_MAX)
		bUnlimited = SG_TRUE;
	//Figure out which dagnodes we are starting from.
	SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &pStringArraySearchNodes, count_args == 0 ? 1 : count_args)  );
	if (nCountChangesets > 0)
	{
		SG_uint32 revLen = nCountChangesets;
		SG_uint32 k = 0;
		for (k=0; k < revLen; k++)
		{
			SG_ERR_CHECK(  SG_stringarray__add(pCtx, pStringArraySearchNodes, pasz_changesets[k])  );
		}
		if (bForceDagWalk != SG_TRUE)  //They passed in a flag to force the dag walk.  That usually happens from the parents command.
			bWalkDag = SG_FALSE;
	}
	else if (bAllLeaves == SG_TRUE)
	{
		void * data = NULL;
		const char * rbKey = NULL;
		SG_rbtree * prbtLeaves = NULL;
		SG_rbtree_iterator * prbtIterator = NULL;
		SG_bool b = SG_FALSE;
		SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, &prbtLeaves)  );
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &prbtIterator, prbtLeaves, &b, &rbKey, &data)  );
		while (b)
		{
			SG_ERR_CHECK(  SG_stringarray__add(pCtx, pStringArraySearchNodes, rbKey)  );
			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, prbtIterator, &b, &rbKey, &data)  );
			nCountChangesets++;

		}
		SG_RBTREE_NULLFREE(pCtx, prbtLeaves);
		SG_RBTREE_ITERATOR_NULLFREE(pCtx, prbtIterator);

	}
	else
	{
		SG_bool bIgnoreWarnings = SG_FALSE;
		SG_uint32 k, nrParents;

		if (bPendingTreeWasPassedIn == SG_FALSE)
		{
			SG_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathCwd)  );
			SG_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathCwd)  );
			SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathCwd, bIgnoreWarnings, &pPendingTree)  );
		}


		// TODO 4/20/10 @Jeremy: do you want all of the wd parents in your array?

		SG_ERR_CHECK(  SG_pendingtree__get_wd_parents__ref(pCtx, pPendingTree, &pva_wd_parents)  );
		SG_ERR_CHECK(  SG_varray__count(pCtx, pva_wd_parents, &nrParents)  );
		SG_ASSERT(  (nrParents > 0)  );
		for (k=0; k<nrParents; k++)
		{
			const char * psz_hid_parent_k;

			SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_wd_parents, k, &psz_hid_parent_k)  );
			SG_ERR_CHECK(  SG_stringarray__add(pCtx, pStringArraySearchNodes, psz_hid_parent_k)  );
		}
		SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	}


	//Now loop over all of the supplied paths, converting them all to GID
	SG_ERR_CHECK( SG_STRINGARRAY__ALLOC(pCtx, &pStringArrayGIDs, count_args == 0 ? 1 : count_args)  );
	SG_ERR_CHECK( SG_VECTOR__ALLOC(pCtx, &pVectorPathArrays, count_args == 0 ? 1 : count_args)  );
	SG_ERR_CHECK( SG_VECTOR__ALLOC(pCtx, &pVectorCurrentPathIndex, count_args == 0 ? 1 : count_args)  );
	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pVArrayResults)  );
	for (i = 0; i <count_args; i++)
	{  //fill the array index cache with zeroes
		SG_ERR_CHECK( SG_vector__append(pCtx, pVectorCurrentPathIndex, (void*)0, &arrayIndexReturned )  );
	}

	if (nCountChangesets == 0 && bHistoryOnRoot == SG_FALSE)
	{
		//They didn't specify a changeset, so use the pending tree to look up the objects.
		SG_ERR_CHECK(  SG_pendingtree__get_gids_for_paths(pCtx, pPendingTree, count_args, paszArgs, pStringArrayGIDs, &bHistoryOnRoot)  );
	}
	else
	{
		//This might be used if you have --leaves, or if there are multiple parents
		//since they specified a changeset, we need to use the full repo path @/blah/blah to look up the objects
		SG_changeset * pcsSpecified = NULL;
		const char * pszHidForRoot = NULL;
		const char * pszCurrentDagnodeID = NULL;
		char * pszSearchItemGID = NULL;
		SG_treenode * pTreeNodeRoot = NULL;
		SG_treenode_entry * ptne = NULL;
		SG_uint32 revLen = nCountChangesets;
		SG_uint32 k = 0;
		for (k=0; k < revLen; k++)
		{

			SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pStringArraySearchNodes, k, &pszCurrentDagnodeID)  );
			SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, pszCurrentDagnodeID, &pcsSpecified)  );
			SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcsSpecified, &pszHidForRoot)  );
			SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszHidForRoot, &pTreeNodeRoot)  );
			if (bHistoryOnRoot)
			{
				SG_ERR_CHECK(  SG_treenode__find_treenodeentry_by_path(pCtx, pRepo, pTreeNodeRoot, "@", &pszSearchItemGID, &ptne)  );
				if (pszSearchItemGID == NULL)
				{
					SG_TREENODE_NULLFREE(pCtx, pTreeNodeRoot);
					SG_CHANGESET_NULLFREE(pCtx, pcsSpecified);
					SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
				}
				SG_ERR_CHECK(  SG_stringarray__add(pCtx, pStringArrayGIDs, pszSearchItemGID)  );
				SG_NULLFREE(pCtx, pszSearchItemGID);
				SG_TREENODE_ENTRY_NULLFREE(pCtx, ptne);
			}
			else
			{
				for (i = 0; i < count_args; i++)
				{
					SG_ERR_CHECK(  SG_treenode__find_treenodeentry_by_path(pCtx, pRepo, pTreeNodeRoot, paszArgs[i], &pszSearchItemGID, &ptne)  );
					if (pszSearchItemGID == NULL)
					{
						SG_TREENODE_NULLFREE(pCtx, pTreeNodeRoot);
						SG_CHANGESET_NULLFREE(pCtx, pcsSpecified);
						SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
					}
					SG_ERR_CHECK(  SG_stringarray__add(pCtx, pStringArrayGIDs, pszSearchItemGID)  );
					SG_NULLFREE(pCtx, pszSearchItemGID);
					SG_TREENODE_ENTRY_NULLFREE(pCtx, ptne);
				}
			}
			SG_TREENODE_NULLFREE(pCtx, pTreeNodeRoot);
			SG_CHANGESET_NULLFREE(pCtx, pcsSpecified);
		}
	}

	//Use the treendx to look up all the paths for the GIDs that we just got.
	for (i = 0; i < count_args; i++)
	{
		SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pStringArrayGIDs, i, &currentGID)  );
		SG_ERR_CHECK(  SG_repo__treendx__get_all_paths(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, currentGID, &pStringArrayPaths)  );
		SG_ERR_CHECK(  SG_vector__append(pCtx, pVectorPathArrays, pStringArrayPaths, &arrayIndexReturned)  );
	}
	//We now have two arrays, pVArrayGIDs, which contains a GID for each queried item.
	//pVectorPathArrays, which must have the same length.

	if (bWalkDag == SG_FALSE)
	{
		//They gave us specific revisions and tags, we need to just output
		//information about those.

		SG_uint32 revisionsIndex = 0;
		SG_uint32 revisionsCount = 0;
		SG_vhash * pvhCurrent = NULL;
		SG_ERR_CHECK(  SG_stringarray__count(pCtx, pStringArraySearchNodes, &revisionsCount)  );
		for (revisionsIndex = 0; revisionsIndex < revisionsCount; revisionsIndex++)
		{
			SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pStringArraySearchNodes, revisionsIndex, &currentGID)  );
			SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, currentGID, &pdnCurrent)  );
			SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhCurrent)  );
			SG_ERR_CHECK(  SG_history__get_id_and_parents(pCtx, pRepo, pdnCurrent, &pvhCurrent)  );
			SG_ERR_CHECK(  SG_varray__append__vhash(pCtx, pVArrayResults, &pvhCurrent)  );

			SG_DAGNODE_NULLFREE(pCtx, pdnCurrent);

		}
	}
	else if (bUnlimited == SG_TRUE)
	{
		//There are no user or date restrictions, we don't need to build the candidate list at all.
		//Passing in NULL for the candidate list will trigger the "all changesets are candidates" case.
		struct _my_history_dagwalk_data myData;
		myData.nResultLimit = nResultLimit;
		myData.pArrayReturnResults = pVArrayResults;
		myData.pCandidateChangesets = NULL;
		myData.pStringArrayGIDs = pStringArrayGIDs;
		myData.pVectorCurrentPathIndex = pVectorCurrentPathIndex;
		myData.pVectorKnownPaths = pVectorPathArrays;
		myData.bHistoryOnRoot = bHistoryOnRoot;
		SG_ERR_CHECK(  SG_dagwalker__walk_dag(pCtx, pRepo, pStringArraySearchNodes, _pending_tree__history__dag_walk_callback, (void*)&myData)  );
//		SG_ERR_CHECK(  _sg_pendingtree__history__walk_dag(pCtx, pPendingTree, nResultLimit, pStringArraySearchNodes, pStringArrayGIDs, pVectorPathArrays, pVectorCurrentPathIndex, NULL/*pVectorCandidateChangesets*/, pVArrayResults)  );
	}
	else
	{
		struct _my_history_dagwalk_data  myData;
        SG_int_to_string_buffer buf_min;
        SG_int_to_string_buffer buf_max;

		myData.nResultLimit = nResultLimit;
		myData.pArrayReturnResults = pVArrayResults;
		myData.pStringArrayGIDs = pStringArrayGIDs;
		myData.pVectorCurrentPathIndex = pVectorCurrentPathIndex;
		myData.pVectorKnownPaths = pVectorPathArrays;
		myData.bHistoryOnRoot = bHistoryOnRoot;

        if ((pStrUser && pStrUser[0]) || nToDate != SG_INT64_MAX || nFromDate != 0)
        {
        	bFilteredOnUserOrDate = SG_TRUE;
        	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_where)  );

			SG_int64_to_sz(nFromDate, buf_min);
			SG_int64_to_sz(nToDate, buf_max);

			if (pStrUser && pStrUser[0])
			{
				if (nFromDate != 0)
				{
					if (nToDate != SG_INT64_MAX)
					{
						SG_ERR_CHECK(  SG_string__sprintf(
									pCtx,
									pstr_where,
									"(who == '%s') && ((when >= %s) && (when <= %s))",
									pStrUser,
									buf_min,
									buf_max
									)  );
					}
					else
					{
						SG_ERR_CHECK(  SG_string__sprintf(
									pCtx,
									pstr_where,
									"(who == '%s') && (when >= %s)",
									pStrUser,
									buf_min
									)  );
					}
				}
				else
				{
					if (nToDate != SG_INT64_MAX)
					{
						SG_ERR_CHECK(  SG_string__sprintf(
									pCtx,
									pstr_where,
									"(who == '%s') && (when <= %s)",
									pStrUser,
									buf_max
									)  );
					}
					else
					{
						SG_ERR_CHECK(  SG_string__sprintf(
									pCtx,
									pstr_where,
									"who == '%s'",
									pStrUser
									)  );
					}
				}
			}
			else
			{
				if (nFromDate != 0)
				{
					if (nToDate != SG_INT64_MAX)
					{
						SG_ERR_CHECK(  SG_string__sprintf(
									pCtx,
									pstr_where,
									"(when >= %s) && (when <= %s)",
									buf_min,
									buf_max
									)  );
					}
					else
					{
						SG_ERR_CHECK(  SG_string__sprintf(
									pCtx,
									pstr_where,
									"when >= %s",
									buf_min
									)  );
					}
				}
				else
				{
					if (nToDate != SG_INT64_MAX)
					{
						SG_ERR_CHECK(  SG_string__sprintf(
									pCtx,
									pstr_where,
									"when <= %s",
									buf_max
									)  );
					}
					else
					{
						SG_ERR_CHECK(  SG_string__sprintf(
									pCtx,
									pstr_where,
									""
									)  );
					}
				}
			}

			SG_ERR_CHECK(  SG_audit__query(pCtx, pRepo, SG_DAGNUM__VERSION_CONTROL, SG_string__sz(pstr_where), &pRBTreeCandidateChangesets)  );
			SG_STRING_NULLFREE(pCtx, pstr_where);
        }
        //Now filter by stamps.
        if (pStrStamp != NULL && strcmp(pStrStamp, "") != 0)
        {
        	SG_ERR_CHECK(  SG_vc_stamps__lookup_by_stamp(pCtx, pRepo, pStrStamp, &pstringarray_csids_with_stamps)  );
        	if (pstringarray_csids_with_stamps != NULL)
        	{
        		SG_uint32 csidCount = 0;
        		SG_uint32 csidIndex = 0;
        		SG_bool bAlreadyThere = SG_FALSE;
        		const char * psz_currentcsid = NULL;
        		SG_rbtree * prb_newTree = NULL;

        		SG_ERR_CHECK(  SG_stringarray__count(pCtx, pstringarray_csids_with_stamps, &csidCount)  );
        		SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_newTree)  );
				for (csidIndex = 0; csidIndex < csidCount; csidIndex++)
				{
					if (pRBTreeCandidateChangesets == NULL) //There were now
						SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pRBTreeCandidateChangesets)  );
					SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, pstringarray_csids_with_stamps, csidIndex, &psz_currentcsid)  );

					if (bFilteredOnUserOrDate == SG_TRUE)  //We need to look to see if it's already there before adding it to the new rbtree.
					{
						SG_ERR_CHECK(  SG_rbtree__find(pCtx, pRBTreeCandidateChangesets, psz_currentcsid, &bAlreadyThere, NULL)  );
						if (bAlreadyThere)
							SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb_newTree, psz_currentcsid)  );
					}
					else//We just put our entry in, no matter what.
					{
						SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb_newTree, psz_currentcsid)  );
					}
				}
				SG_RBTREE_NULLFREE(pCtx, pRBTreeCandidateChangesets);
				pRBTreeCandidateChangesets = prb_newTree;
        	}
        }
		if (pRBTreeCandidateChangesets != NULL)
			SG_ERR_CHECK(  SG_rbtree__count(pCtx, pRBTreeCandidateChangesets, &candidateCount)  );
		//We now have the GIDs, and the candidate change sets.
		if (candidateCount > 0)  //If there are no candidates, then we don't need to walk the dag at all.
		{
			myData.pCandidateChangesets = pRBTreeCandidateChangesets;
			SG_ERR_CHECK(  SG_dagwalker__walk_dag(pCtx, pRepo, pStringArraySearchNodes, _pending_tree__history__dag_walk_callback, (void*)&myData)  );
			//SG_ERR_CHECK(  _sg_pendingtree__history__walk_dag(pCtx, pPendingTree, nResultLimit, pStringArraySearchNodes, pStringArrayGIDs, pVectorPathArrays, pVectorCurrentPathIndex, pRBTreeCandidateChangesets, pVArrayResults)  );
		}
	}

    SG_ERR_CHECK(  sg_history__fill_in_details(pCtx, pRepo, pVArrayResults)  );

	if (bFreeArgs)
		SG_NULLFREE(pCtx, paszArgs);
	SG_TREENDX_NULLFREE(pCtx, pTreeNdx);
	SG_PATHNAME_NULLFREE(pCtx, pPathName);
	SG_VHASH_NULLFREE(pCtx, pvh_query);
	SG_STRINGARRAY_NULLFREE(pCtx, pStringArrayGIDs);
	SG_STRINGARRAY_NULLFREE(pCtx, pStringArraySearchNodes);
	SG_VECTOR_NULLFREE(pCtx, pVectorCurrentPathIndex);
	SG_RBTREE_NULLFREE(pCtx, pRBTreeCandidateChangesets);
	SG_VECTOR_NULLFREE_WITH_ASSOC(pCtx, pVectorPathArrays, (SG_free_callback *)SG_stringarray__free);
	SG_VECTOR_NULLFREE(pCtx, pVectorPathArrays);
	if (bPendingTreeWasPassedIn == SG_FALSE)
		SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRINGARRAY_NULLFREE(pCtx, pstringarray_csids_with_stamps);
	SG_DAGNODE_NULLFREE(pCtx, pdnCurrent);
	//SG_STRINGARRAY_NULLFREE(pCtx, pStringArrayPaths);
	*ppVArrayResults = pVArrayResults;


	return;

fail:
    SG_STRING_NULLFREE(pCtx, pstr_where);
	if (bFreeArgs)
		SG_NULLFREE(pCtx, paszArgs);
	SG_TREENDX_NULLFREE(pCtx, pTreeNdx);
	SG_PATHNAME_NULLFREE(pCtx, pPathName);
	SG_VECTOR_NULLFREE(pCtx, pVectorPathArrays);
	SG_VHASH_NULLFREE(pCtx, pvh_query);
	SG_STRINGARRAY_NULLFREE(pCtx, pStringArrayGIDs);
	SG_VECTOR_NULLFREE(pCtx, pVectorCurrentPathIndex);
	SG_STRINGARRAY_NULLFREE(pCtx, pStringArraySearchNodes);
	SG_RBTREE_NULLFREE(pCtx, pRBTreeCandidateChangesets);
	SG_VECTOR_NULLFREE_WITH_ASSOC(pCtx, pVectorPathArrays, (SG_free_callback *)SG_stringarray__free);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	//SG_STRINGARRAY_NULLFREE(pCtx, pStringArrayPaths);
    SG_VARRAY_NULLFREE(pCtx, pVArrayResults);
    SG_PATHNAME_NULLFREE(pCtx, pPathCwd);
	SG_DAGNODE_NULLFREE(pCtx, pdnCurrent);
    SG_STRINGARRAY_NULLFREE(pCtx, pstringarray_csids_with_stamps);
}

