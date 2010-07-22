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
 * @file sg_dagfrag.c
 *
 * NOTE: the dagnode generation is now SG_int32 rather than SG_int64.
 *
 *================================================================
 *
 * TODO verify that we handle HID case properly.  that is, if someone
 * TODO does an __add_leaf("aaaa...") and __add_leaf("AAAA...") will we
 * TODO catch that they are the same?
 * TODO
 * TODO update 3/4/10: just assert that all HIDs are lowercase.
 *
 *================================================================
 *
 * TODO verify that this works when we have more than one initial
 * TODO checkin -- that is, we don't explicitly do anything to
 * TODO address the NULL / *root* / FAKE_PARENT described in
 * TODO sg_dag_sqlite3.c.  I don't think we need to do anything
 * TODO special, but I want to verify that.
 *
 *================================================================
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

#define TRACE_DAGFRAG 0

//////////////////////////////////////////////////////////////////

/**
 * SG_dagfrag is a structure we use to collect information on a
 * fragment of the dag.  This fragment may be serialized and sent
 * to a remote instance of the repository during PUSH/PULL.
 */
struct _SG_dagfrag
{
	SG_rbtree *		m_pRB_Cache;

	SG_rbtree *		m_pRB_GenerationSortedMemberCache;

    SG_uint32       m_iDagNum;

    char m_sz_repo_id[SG_GID_BUFFER_LENGTH];
    char m_sz_admin_id[SG_GID_BUFFER_LENGTH];
};

/**
 * Instance data to associate with items in Cache RBTREE.
 */
struct _my_data
{
	SG_dagfrag_state	m_state;

    /* the following things are not valid for stuff in the end fringe */
	SG_dagnode *		m_pDagnode;
	SG_int32			m_genDagnode;
};

typedef struct _my_data _my_data;

//////////////////////////////////////////////////////////////////

void _my_data__free(SG_context *pCtx, void * pVoidData)
{
	_my_data * pData = (_my_data *)pVoidData;

	if (!pData)
		return;

	SG_DAGNODE_NULLFREE(pCtx, pData->m_pDagnode);
	SG_NULLFREE(pCtx, pData);
}

//////////////////////////////////////////////////////////////////

void SG_dagfrag__alloc(SG_context * pCtx,
					   SG_dagfrag ** ppNew,
					   const char* psz_repo_id,
					   const char* psz_admin_id,
					   SG_uint32 iDagNum)
{
	SG_dagfrag * pFrag = NULL;

	SG_NULLARGCHECK_RETURN(ppNew);
	SG_ARGCHECK_RETURN( (iDagNum != SG_DAGNUM__NONE), iDagNum );
	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, psz_repo_id)  );
	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, psz_admin_id)  );

    SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(SG_dagfrag), &pFrag)  );

    SG_ERR_CHECK(  SG_strcpy(pCtx, pFrag->m_sz_repo_id, sizeof(pFrag->m_sz_repo_id), psz_repo_id)  );
    SG_ERR_CHECK(  SG_strcpy(pCtx, pFrag->m_sz_admin_id, sizeof(pFrag->m_sz_admin_id), psz_admin_id)  );

    pFrag->m_iDagNum = iDagNum;

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx,&pFrag->m_pRB_Cache)  );

	pFrag->m_pRB_GenerationSortedMemberCache = NULL;		// only create this if needed

	*ppNew = pFrag;
	return;

fail:
	SG_DAGFRAG_NULLFREE(pCtx, pFrag);
}

void SG_dagfrag__alloc_transient(SG_context * pCtx,
								 SG_dagfrag ** ppNew)
{
	SG_dagfrag * pFrag = NULL;

	SG_NULLARGCHECK_RETURN(ppNew);

    SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(SG_dagfrag), &pFrag)  );

	pFrag->m_sz_repo_id[0] = '*';
	pFrag->m_sz_admin_id[0] = '*';
    pFrag->m_iDagNum = SG_DAGNUM__NONE;

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx,&pFrag->m_pRB_Cache)  );

	pFrag->m_pRB_GenerationSortedMemberCache = NULL;		// only create this if needed

	*ppNew = pFrag;
	return;

fail:
	SG_DAGFRAG_NULLFREE(pCtx, pFrag);
}

#define IS_TRANSIENT(pFrag)		( ((pFrag)->m_sz_repo_id[0] == '*') || ((pFrag)->m_sz_admin_id[0] == '*') || ((pFrag)->m_iDagNum == SG_DAGNUM__NONE) )

void SG_dagfrag__free(SG_context * pCtx, SG_dagfrag * pFrag)
{
	if (!pFrag)
		return;

	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pFrag->m_pRB_Cache, _my_data__free);
	SG_RBTREE_NULLFREE(pCtx, pFrag->m_pRB_GenerationSortedMemberCache);	// we borrowed the assoc data from the real cache, so we don't free them.

	SG_NULLFREE(pCtx, pFrag);
}

//////////////////////////////////////////////////////////////////

static void _cache__add__fringe(SG_context * pCtx,
								SG_dagfrag * pFrag,
								const char* psz_id)
{
	_my_data * pDataAllocated = NULL;
	_my_data * pDataCached = NULL;
	_my_data * pOldData = NULL;

	SG_NULLARGCHECK_RETURN(pFrag);				// this is probably not necessary for an internal routine
	SG_NONEMPTYCHECK_RETURN(psz_id);			// this is probably not necessary for an internal routine

    SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(_my_data), &pDataAllocated)  );

	pDataAllocated->m_genDagnode = 0;
	pDataAllocated->m_state = SG_DFS_END_FRINGE;
	pDataAllocated->m_pDagnode = NULL;

	SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx,pFrag->m_pRB_Cache,psz_id,pDataAllocated,(void **)&pOldData)  );
	pDataCached = pDataAllocated;
	pDataAllocated = NULL;				// cache now owns pData and pDagnode.

	SG_ASSERT_RELEASE_RETURN2(  (!pOldData),
						(pCtx,"Possible memory leak adding [%s] to dagfrag fringe.",psz_id)  );

	return;

fail:
	// TODO is the following test and assignment really necessary?  or did it get carried
	// TODO over from another change?  we weren't given a pDagnode in the first place.
	if (pDataCached)					// caller still owns pDagnode on errors even if we got pData
		pDataCached->m_pDagnode = NULL;	// into the cache. This may cause problems later if you keep going.

	SG_ERR_IGNORE(  _my_data__free(pCtx, pDataAllocated)  );  // free pData if we did not get it stuck into the cache.
}

static void _cache__add__dagnode(SG_context * pCtx,
								 SG_dagfrag * pFrag,
								 SG_int32 gen,
								 SG_dagnode * pDagnode,		// if successful, we take ownership of dagnode
								 SG_uint32 state,
								 _my_data ** ppData)		// we retain ownership of DATA (but you may modify non-pointer values within it)
{
	const char * szHid;
	_my_data * pDataAllocated = NULL;
	_my_data * pDataCached = NULL;
	_my_data * pOldData = NULL;

	SG_NULLARGCHECK_RETURN(pFrag);				// this is probably not necessary for an internal routine
	SG_NULLARGCHECK_RETURN(pDagnode);			// this is probably not necessary for an internal routine

	SG_ERR_CHECK_RETURN(  SG_dagnode__get_id_ref(pCtx,pDagnode,&szHid)	);

	SG_ASSERT_RELEASE_RETURN2(  (SG_DFS_END_FRINGE != state),
						(pCtx,"Adding end-fringe dagnode [%s] to dagfrag.",szHid)  );

	SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(_my_data), &pDataAllocated)  );

	pDataAllocated->m_genDagnode = gen;
	pDataAllocated->m_state = state;

	SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx,pFrag->m_pRB_Cache,szHid,pDataAllocated,(void **)&pOldData)  );
	SG_ASSERT_RELEASE_FAIL2(  (!pOldData),
					  (pCtx,"Possible memory leak adding [%s] to dagfrag.",szHid)  );

	// if everything is successful, the cache now owns pData and pDagnode.

	pDataCached = pDataAllocated;
	pDataAllocated = NULL;
	pDataCached->m_pDagnode = pDagnode;

	if (ppData)
		*ppData = pDataCached;

	return;

fail:
	if (pDataCached)					// caller still owns pDagnode on errors even if we got pData
		pDataCached->m_pDagnode = NULL;	// into the cache. This may cause problems later if you keep going.

	SG_ERR_IGNORE(  _my_data__free(pCtx, pDataAllocated)  );  // free pData if we did not get it stuck into the cache.
}

static void _cache__lookup(SG_context * pCtx,
						   SG_dagfrag * pFrag, const char * szHid,
						   _my_data ** ppData,		// you do not own this (but you may modify non-pointer values within)
						   SG_bool * pbPresent)
{
	// see if szHid is present in cache.  return the associated data.

	_my_data * pData;
    SG_bool b = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pFrag);
	SG_NONEMPTYCHECK_RETURN(szHid);
	SG_NULLARGCHECK_RETURN(ppData);

	SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx,pFrag->m_pRB_Cache,szHid,&b,((void **)&pData))  );
	if (b)
	{
		*pbPresent = SG_TRUE;
		*ppData = pData;
	}
    else
    {
		*pbPresent = SG_FALSE;
		*ppData = NULL;
    }
}

static void _add_one_parent(SG_context * pCtx, SG_dagfrag* pFrag, const char * szHid)
{
	_my_data * pDataCached = NULL;
	SG_bool bPresent = SG_FALSE;

	SG_ERR_CHECK_RETURN(  _cache__lookup(pCtx,pFrag,szHid,&pDataCached,&bPresent)  );
	if (!bPresent)
	{
		// dagnode is not present in the cache.  therefore, it's in the fringe

		SG_ERR_CHECK_RETURN(  _cache__add__fringe(pCtx, pFrag, szHid)  );
	}
	else
	{
		// dagnode already present in the cache. therefore, we have already visited it
		// before.  we can change our minds about the state of this dagnode if something
		// has changed (such as the fragment bounds being widened).

		switch (pDataCached->m_state)
		{
		default:
		//case SG_DFS_UNKNOWN:
			SG_ASSERT_RELEASE_RETURN2(  (0),
								(pCtx,"Invalid state [%d] in DAGFRAG Cache for [%s]",
								 pDataCached->m_state,szHid)  );

		case SG_DFS_START_MEMBER:
			// a dagnode has a parent that we are considering a START node.
			// this can happen when we were started from a non-leaf node and
			// then a subsequent call to __load is given a true leaf node or
			// a node deeper in the tree that has our original start node as
			// a parent.
			//
			// clear the start bit.  (we only want true fragment-terminal
			// nodes marked as start nodes.)

			pDataCached->m_state = SG_DFS_INTERIOR_MEMBER;
			break;

		case SG_DFS_INTERIOR_MEMBER:
            /* nothing to do here */
			break;

		case SG_DFS_END_FRINGE:
            /* nothing to do here */
			break;
		}
	}
}

static void _add_parents(SG_context * pCtx, SG_dagfrag* pFrag, SG_dagnode * pDagnode)
{
	const char ** aszHidParents = NULL;
	SG_uint32 k, nrParents;

	SG_ERR_CHECK(  SG_dagnode__get_parents(pCtx,pDagnode,&nrParents,&aszHidParents)  );
	for (k=0; k<nrParents; k++)
	{
	    SG_ERR_CHECK(  _add_one_parent(pCtx,pFrag,aszHidParents[k])  );
	}

	// fall thru to common cleanup

fail:
	SG_NULLFREE(pCtx, aszHidParents);
}


static void _add_parents_to_work_queue(SG_context * pCtx, SG_dagnode * pDagnode, SG_rbtree* prb_WorkQueue)
{
	const char ** aszHidParents = NULL;
	SG_uint32 k, nrParents;

	SG_ERR_CHECK(  SG_dagnode__get_parents(pCtx,pDagnode,&nrParents,&aszHidParents)  );
	for (k=0; k<nrParents; k++)
	{
	    SG_ERR_CHECK(  SG_rbtree__update(pCtx,prb_WorkQueue,aszHidParents[k])  );
	}

	// fall thru to common cleanup

fail:
	SG_NULLFREE(pCtx, aszHidParents);
}

//////////////////////////////////////////////////////////////////

struct _work_queue_data
{
    SG_rbtree*      prb_WorkQueue;
	SG_dagfrag *	pFrag;
	SG_int32		generationEnd;
    SG_repo*        pRepo;
};

static SG_rbtree_foreach_callback _process_work_queue_cb;

static void _process_work_queue_cb(SG_context * pCtx,
								   const char * szHid, SG_UNUSED_PARAM(void * pAssocData), void * pVoidCallerData)
{
	// we are given a random item in the work_queue.
	//
	// lookup the corresponding DATA node in the Cache, if it has one.
	//
	// and then evaluate where this node belongs:

	struct _work_queue_data * pWorkQueueData = (struct _work_queue_data *)pVoidCallerData;
	_my_data * pDataCached = NULL;
	SG_dagnode * pDagnodeAllocated = NULL;
	SG_bool bPresent = SG_FALSE;
	SG_UNUSED(pAssocData);

	SG_ERR_CHECK(  _cache__lookup(pCtx, pWorkQueueData->pFrag,szHid,&pDataCached,&bPresent)  );
	if (!bPresent)
	{
		// dagnode is not present in the cache.  therefore, we've never visited this
		// dagnode before.  add it to the cache with proper settings and maybe add
		// all of the parents to the work queue.

		SG_int32 myGeneration;

        SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pWorkQueueData->pRepo, szHid,&pDagnodeAllocated)  );

		SG_ERR_CHECK(  SG_dagnode__get_generation(pCtx, pDagnodeAllocated,&myGeneration)  );

        if ((myGeneration > pWorkQueueData->generationEnd))
        {
            SG_ERR_CHECK(  _cache__add__dagnode(pCtx,
												pWorkQueueData->pFrag,
												myGeneration,
												pDagnodeAllocated,SG_DFS_INTERIOR_MEMBER,
												&pDataCached)  );
            pDagnodeAllocated = NULL;	// cache takes ownership of dagnode
			SG_ERR_CHECK(  _add_parents_to_work_queue(pCtx, pDataCached->m_pDagnode, pWorkQueueData->prb_WorkQueue)  );
        }
        else
        {
            SG_ERR_CHECK(  _cache__add__fringe(pCtx, pWorkQueueData->pFrag, szHid)  );
            SG_DAGNODE_NULLFREE(pCtx, pDagnodeAllocated);
        }
	}
	else
	{
		// dagnode already present in the cache. therefore, we have already visited it
		// before.  we can change our minds about the state of this dagnode if something
		// has changed (such as the fragment bounds being widened).

		switch (pDataCached->m_state)
		{
		default:
		//case SG_DFS_UNKNOWN:
			SG_ASSERT_RELEASE_FAIL2(  (0),
							  (pCtx,"Invalid state [%d] in DAGFRAG Cache for [%s]",
							   pDataCached->m_state,szHid)  );

		case SG_DFS_START_MEMBER:
			// a dagnode has a parent that we are considering a START node.
			// this can happen when we were started from a non-leaf node and
			// then a subsequent call to __load is given a true leaf node or
			// a node deeper in the tree that has our original start node as
			// a parent.
			//
			// clear the start bit.  (we only want true fragment-terminal
			// nodes marked as start nodes.)

			pDataCached->m_state = SG_DFS_INTERIOR_MEMBER;
			// FALL-THRU-INTENDED

		case SG_DFS_INTERIOR_MEMBER:
			// a dagnode that we have already visited is being re-visited.
			// this happpens for a number of reasons, such as when we hit
			// the parent of a branch/fork.  we might get visisted because
			// we are a parent of each child.
			//
			// we also get revisited when the caller expands the scope of
			// the fragment.

			if (pWorkQueueData->generationEnd < pDataCached->m_genDagnode)
			{
				// the caller has expanded the scope of the fragment to include
				// older generations than the last time we visited this node.
				// this doesn't affect the state of this node, but it could mean
				// that older ancestors of this node should be looked at.

				SG_ERR_CHECK(  _add_parents_to_work_queue(pCtx,pDataCached->m_pDagnode,pWorkQueueData->prb_WorkQueue)  );
			}
			break;

		case SG_DFS_END_FRINGE:
			// a dagnode that was on the end-fringe is being re-evaluated.

			if (pDataCached->m_genDagnode > pWorkQueueData->generationEnd)
			{
				// it looks like the bounds of the fragment were expanded and
				// now includes this dagnode.
				//
				// move it from END-FRINGE to INCLUDE state.
				// and re-eval all of its parents.

				pDataCached->m_state = SG_DFS_INTERIOR_MEMBER;
				SG_ERR_CHECK(  _add_parents_to_work_queue(pCtx,pDataCached->m_pDagnode,pWorkQueueData->prb_WorkQueue)  );
			}
			break;
		}
	}

	// we have completely dealt with this dagnode, so remove it from the work queue
	// and cause our caller to restart the iteration (because we changed the queue).

	SG_ERR_CHECK(  SG_rbtree__remove(pCtx,pWorkQueueData->prb_WorkQueue,szHid)  );
	SG_ERR_THROW(  SG_ERR_RESTART_FOREACH  );

fail:
	SG_DAGNODE_NULLFREE(pCtx, pDagnodeAllocated);
}

static void _process_work_queue_item(SG_context * pCtx,
									 SG_dagfrag * pFrag, SG_rbtree* prb_WorkQueue, SG_int32 generationEnd, SG_repo* pRepo)
{
	// fetch first item in work_queue and process it.
	// we let the SG_rbtree__foreach() give us the first
	// item in the queue.  (we don't actually care which
	// one; a random item would do.)
	//
	// we stop the iteration after 1 step because we will
	// be modifying the queue.
	//
	// our caller must call us in a loop to do a complete
	// iteration over the whole queue.
	//
	// this would be a little clearer if we sg_rbtree__first() was public
	// and we could just:
	//     while (1) _process(__first)

	struct _work_queue_data wq_data;
    wq_data.prb_WorkQueue = prb_WorkQueue;
	wq_data.pFrag = pFrag;
	wq_data.generationEnd = generationEnd;
    wq_data.pRepo = pRepo;

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,prb_WorkQueue,_process_work_queue_cb,&wq_data)  );
}

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
struct _dump_data
{
	SG_uint32 indent;
	SG_uint32 nrDigits;
	SG_dagfrag_state stateWanted;
	SG_string * pStringOutput;
};

static void my_dump_id(SG_context * pCtx,
					   const char* psz_hid,
					   SG_uint32 nrDigits,
					   SG_uint32 indent,
					   SG_string * pStringOutput)
{
	char buf[4000];

	// we can abbreviate the full IDs.

	nrDigits = SG_MIN(nrDigits, SG_HID_MAX_BUFFER_LENGTH);
	nrDigits = SG_MAX(nrDigits, 4);

	// create:
	//     Dagnode[addr]: <child_id> <gen> [<parent_id>...]

	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,buf,SG_NrElements(buf),"%*c ",indent,' ')  );
	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx,pStringOutput,buf)  );
	SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx,pStringOutput,(SG_byte *)(psz_hid),nrDigits)  );
	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx,pStringOutput,"\n")  );
}

static void _dump_cb(SG_context * pCtx, const char * szHid, void * pAssocData, void * pVoidDumpData)
{
	struct _dump_data * pDumpData = (struct _dump_data *)pVoidDumpData;
	_my_data * pMyData = (_my_data *)pAssocData;

	if (pDumpData->stateWanted == pMyData->m_state)
	{
		if (pMyData->m_pDagnode)
		{
			SG_ERR_CHECK_RETURN(  SG_dagnode_debug__dump(pCtx,
														 pMyData->m_pDagnode,
														 pDumpData->nrDigits,
														 pDumpData->indent,
														 pDumpData->pStringOutput)  );
		}
		else
		{
			SG_ERR_CHECK_RETURN(  my_dump_id(pCtx,
											 szHid,
											 pDumpData->nrDigits,
											 pDumpData->indent,
											 pDumpData->pStringOutput)  );
		}
	}
}

void SG_dagfrag_debug__dump(SG_context * pCtx,
							SG_dagfrag * pFrag,
							const char * szLabel,
							SG_uint32 indent,
							SG_string * pStringOutput)
{
	char buf[4000];
	struct _dump_data dump_data;
	dump_data.indent = indent+4;
	dump_data.nrDigits = 6;
	dump_data.pStringOutput = pStringOutput;

	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx, buf,SG_NrElements(buf),"%*cDagFrag[%p] [%s]\n",indent,' ',pFrag,szLabel)  );
	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pStringOutput,buf)  );

	dump_data.stateWanted = SG_DFS_START_MEMBER;
	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx, buf,SG_NrElements(buf),"%*cStartMembers:\n",indent+2,' ')  );
	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pStringOutput,buf)  );
	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx, pFrag->m_pRB_Cache,_dump_cb,&dump_data)  );

	dump_data.stateWanted = SG_DFS_INTERIOR_MEMBER;
	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,buf,SG_NrElements(buf),"%*cInteriorMembers:\n",indent+2,' ')  );
	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx,pStringOutput,buf)  );
	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,pFrag->m_pRB_Cache,_dump_cb,&dump_data)  );

	dump_data.stateWanted = SG_DFS_END_FRINGE;
	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,buf,SG_NrElements(buf),"%*cEndFringe:\n",indent+2,' ')  );
	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx,pStringOutput,buf)  );
	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,pFrag->m_pRB_Cache,_dump_cb,&dump_data)  );
}

void SG_dagfrag_debug__dump__console(SG_context* pCtx,
									 SG_dagfrag* pFrag,
									 const char* szLabel,
									 SG_uint32 indent,
									 SG_console_stream cs)
{
	SG_string* pString;
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );
	SG_ERR_CHECK(  SG_dagfrag_debug__dump(pCtx, pFrag, szLabel, indent, pString)  );
	SG_ERR_CHECK(  SG_console(pCtx, cs, SG_string__sz(pString))  );
	SG_ERR_CHECK(  SG_console__flush(pCtx, cs)  );

	// fall through
fail:
	SG_STRING_NULLFREE(pCtx, pString);
}
#endif//DEBUG

//////////////////////////////////////////////////////////////////

void SG_dagfrag__add_dagnode(SG_context * pCtx,
							 SG_dagfrag * pFrag,
							 SG_dagnode** ppdn)
{
	_my_data * pMyDataCached = NULL;
	SG_bool bPresent = SG_FALSE;
    const char* psz_id = NULL;
	SG_dagnode* pdn = NULL;

	SG_NULLARGCHECK_RETURN(pFrag);
	SG_NULLARGCHECK_RETURN(ppdn);

	pdn = *ppdn;

	// if we are extending the fragment, delete the generation-sorted
	// member cache copy.  (see __foreach_member()).  it's either that
	// or update it in parallel as we change the real CACHE and that
	// doesn't seem worth the bother.

	SG_RBTREE_NULLFREE(pCtx, pFrag->m_pRB_GenerationSortedMemberCache);
	pFrag->m_pRB_GenerationSortedMemberCache = NULL;

	// fetch the starting dagnode and compute the generation bounds.
	// first, see if the cache already has info for this dagnode.
	// if not, fetch it from the source and then add it to the cache.

    SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pdn, &psz_id)  );

#if DEBUG && TRACE_DAGFRAG && 0
	SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR, "Adding [%s] to frag.\r\n", psz_id)  );
	SG_ERR_CHECK(  SG_console__flush(pCtx, SG_CS_STDERR)  );
#endif

	SG_ERR_CHECK(  _cache__lookup(pCtx, pFrag,psz_id,&pMyDataCached,&bPresent)  );

	if (!bPresent)
	{
		// this dagnode was not already present in the cache.
		// add it to the cache directly and set the state.
		// we don't need to go thru the work queue for it.
		//
		// then the add all of its parents.

		SG_ERR_CHECK(  _cache__add__dagnode(pCtx,
											pFrag,
											0,
											pdn,SG_DFS_START_MEMBER,
											&pMyDataCached)  );
		*ppdn = NULL;

		SG_ERR_CHECK(  _add_parents(pCtx, pFrag, pMyDataCached->m_pDagnode)  );
	}
	else
	{
		// the node was already present in the cache, so we have already
		// walked at least part of the graph around it.

		switch (pMyDataCached->m_state)
		{
			case SG_DFS_END_FRINGE:
				if (!pMyDataCached->m_pDagnode)
				{
					pMyDataCached->m_pDagnode = pdn;
					*ppdn = NULL;
				}
				pMyDataCached->m_state = SG_DFS_INTERIOR_MEMBER;
				SG_ERR_CHECK(  _add_parents(pCtx, pFrag, pMyDataCached->m_pDagnode)  );
				break;
			default:
			  break;
/*
				SG_ASSERT_RELEASE_FAIL2(  (0),
					(pCtx,"Invalid state [%d] in DAGFRAG Cache for [%s]",
					pMyDataCached->m_state,psz_id)  );
*/
		}
	}

	// fall through

fail:
	SG_DAGNODE_NULLFREE(pCtx, *ppdn);
}

void SG_dagfrag__load_from_repo__simple(SG_context * pCtx,
										SG_dagfrag * pFrag,
										SG_repo* pRepo,
										SG_rbtree * prb_ids)
{
	SG_dagnode* pdn = NULL;
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
	const char* psz_id = NULL;

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit,prb_ids,&b,&psz_id,NULL)  );
	while (b)
	{
		SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, psz_id, &pdn)  );
		SG_ERR_CHECK(  SG_dagfrag__add_dagnode(pCtx, pFrag, &pdn)  );
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit,&b,&psz_id,NULL)  );
	}

	// fall thru to common cleanup

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
}

/* TODO this function may not stay around.  If SG_push
 * needs something like this, it will stay.  Not sure yet. */

void SG_dagfrag__load_from_repo__one(SG_context * pCtx,
									 SG_dagfrag * pFrag,
									 SG_repo* pRepo,
									 const char * szHidStart,
									 SG_int32 nGenerations)
{
	// load a fragment of the dag starting with the given dagnode
	// for nGenerations of parents.
	//
	// we add this portion of the graph to whatevery we already
	// have in our fragment.  this may either augment (give us
	// a larger connected piece) or it may be an independent
	// subset.
	//
	// if nGenerations <= 0, load everything from this starting point
	// back to the NULL/root.
	//
	// generationStart is the generation of the starting dagnode.
	//
	// the starting dagnode *MAY* be in the final start-fringe.
	// normally, it will be.  but if we are called multiple times
	// (and have more than one start point), it may be the case
	// that this node is a parent of one of the other start points.
	//
	// we compute generationEnd as the generation that we will NOT
	// include in the fragment; nodes of that generation will be in
	// the end-fringe.  that is, we include [start...end) like most
	// C++ iterators.

	_my_data * pMyDataCached = NULL;
	SG_dagnode * pDagnodeAllocated = NULL;
	SG_dagnode * pDagnodeStart;
	SG_int32 generationStart, generationEnd;
	SG_bool bPresent = SG_FALSE;
    SG_rbtree* prb_WorkQueue = NULL;

	SG_NULLARGCHECK_RETURN(pFrag);
	SG_NONEMPTYCHECK_RETURN(szHidStart);

	// if we are extending the fragment, delete the generation-sorted
	// member cache copy.  (see __foreach_member()).  it's either that
	// or update it in parallel as we change the real CACHE and that
	// doesn't seem worth the bother.

	SG_RBTREE_NULLFREE(pCtx, pFrag->m_pRB_GenerationSortedMemberCache);
	pFrag->m_pRB_GenerationSortedMemberCache = NULL;

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_WorkQueue)  );

	// fetch the starting dagnode and compute the generation bounds.
	// first, see if the cache already has info for this dagnode.
	// if not, fetch it from the source and then add it to the cache.

	SG_ERR_CHECK(  _cache__lookup(pCtx, pFrag,szHidStart,&pMyDataCached,&bPresent)  );
	if (!bPresent)
	{
		if (!pRepo)
			SG_ERR_THROW(  SG_ERR_INVALID_WHILE_FROZEN  );

        SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, szHidStart, &pDagnodeAllocated)  );

		pDagnodeStart = pDagnodeAllocated;
	}
	else
	{
		pDagnodeStart = pMyDataCached->m_pDagnode;
	}

	SG_ERR_CHECK(  SG_dagnode__get_generation(pCtx, pDagnodeStart,&generationStart)  );
	SG_ASSERT_RELEASE_FAIL2(  (generationStart > 0),
					  (pCtx,"Invalid generation value [%d] for dagnode [%s]",
					   generationStart,szHidStart)  );
	if ((nGenerations <= 0)  ||  (generationStart <= nGenerations))
		generationEnd = 0;
	else
		generationEnd = generationStart - nGenerations;

	if (!bPresent)
	{
		// this dagnode was not already present in the cache.
		// add it to the cache directly and set the state.
		// we don't need to go thru the work queue for it.
		//
		// then the add all of its parents to the work queue.

		SG_ERR_CHECK(  _cache__add__dagnode(pCtx,
											pFrag,
											generationStart,
											pDagnodeAllocated,SG_DFS_START_MEMBER,
											&pMyDataCached)  );
		pDagnodeAllocated = NULL;

		SG_ERR_CHECK(  _add_parents_to_work_queue(pCtx, pMyDataCached->m_pDagnode,prb_WorkQueue)  );
	}
	else
	{
		// the node was already present in the cache, so we have already
		// walked at least part of the graph around it.

		switch (pMyDataCached->m_state)
		{
		default:
		//case SG_DFS_UNKNOWN:
			SG_ASSERT_RELEASE_FAIL2(  (0),
							  (pCtx,"Invalid state [%d] in DAGFRAG Cache for [%s]",
							   pMyDataCached->m_state,szHidStart)  );

		case SG_DFS_INTERIOR_MEMBER:				// already in fragment
		case SG_DFS_START_MEMBER:	// already in fragment, duplicated leaf?
			if (generationEnd < pMyDataCached->m_genDagnode)
			{
				// they've expanded the bounds of the fragment since we
				// last visited this dagnode.  keep this dagnode in the
				// fragment and revisit the ancestors in case any were
				// put in the end-fringe that should now be included.
				//
				// we leave the state as INCLUDE or INCLUDE_AND_START
				// because a duplicate start point should remain a
				// start point.

				SG_ERR_CHECK(  _add_parents_to_work_queue(pCtx, pMyDataCached->m_pDagnode,prb_WorkQueue)  );
			}
			else
			{
				// the current end-generation requested is >= the previous
				// end-generation, then we've completely explored this dagnode
				// already.  that is, a complete walk from this node for nGenerations
				// would not reveal any new information.
			}
			break;

		case SG_DFS_END_FRINGE:
			{
				// they want to start at a dagnode that we put in the
				// end-fringe.  this can happen if they need to expand
				// the bounds of the fragment to include older ancestors.
				//
				// we do not mark this as a start node because someone
				// else already has it as a parent.

				pMyDataCached->m_state = SG_DFS_INTERIOR_MEMBER;
				SG_ERR_CHECK(  _add_parents_to_work_queue(pCtx, pMyDataCached->m_pDagnode,prb_WorkQueue)  );
			}
			break;
		}
	}

	// we optionally put the parents of the current node into the work queue.
	//
	// service the work queue until it is empty.  this allows us to walk the graph without
	// recursion.  that is, as we decide what to do with a node, we add the parents
	// to the queue.  we then iterate thru the work queue until we have dealt with
	// everything -- that is, until all parents have been properly placed.
	//
	// we cannot use a standard iterator to drive this loop because we
	// modify the queue.

	while (1)
	{
		_process_work_queue_item(pCtx, pFrag,prb_WorkQueue,generationEnd,pRepo);
		if (!SG_context__has_err(pCtx))
			break;							// we processed everything in the queue and are done

		if (!SG_context__err_equals(pCtx,SG_ERR_RESTART_FOREACH))
			SG_ERR_RETHROW;

		SG_context__err_reset(pCtx);		// queue changed, restart iteration
	}

	SG_RBTREE_NULLFREE(pCtx, prb_WorkQueue);

	/*
	** we have loaded a piece of the dag (starting with the given start node
	** and tracing all parent edges back n generations).  we leave with everything
	** in our progress queues so that other start nodes can be added to the
	** fragment.  this allows the processing of subsequent start nodes to
	** override some of the decisions that we made.  for example:
	**
	**           Q_15
	**             |
	**             |
	**           Z_16
	**           /  \
	**          /    \
	**      Y_17      A_17
	**          \    /   \
	**           \  /     \
	**           B_18     C_18
	**             |
	**             |
	**           D_19
	**             |
	**             |
	**           E_20
	**
	** if we started with the leaf E_20 and requested 3 generations, we would have:
	**     start_set := { E }
	**     include_set := { B, D, E }
	**     end_set := { Y, A }
	**
	** after a subsequent call with the leaf C_18 and 3 generations, we would have:
	**     start_set := { C, E }
	**     include_set := { Z, A, B, C, D, E }
	**     end_set := { Q, Y }
	**
	*/

	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, prb_WorkQueue);
	SG_DAGNODE_NULLFREE(pCtx, pDagnodeAllocated);
}

//////////////////////////////////////////////////////////////////

struct _add_leaves_data
{
	SG_dagfrag * pFrag;
	SG_int32 nGenerations;
    SG_repo* pRepo;
};

static void _sg_dagfrag__add_leaves_callback(SG_context * pCtx,
											 const char * szKey, SG_UNUSED_PARAM(void * pAssocData), void * pVoidAddLeavesData)
{
	struct _add_leaves_data * pAddLeavesData = (struct _add_leaves_data *)pVoidAddLeavesData;

	SG_UNUSED(pAssocData);

	SG_ERR_CHECK_RETURN(  SG_dagfrag__load_from_repo__one(pCtx,
														  pAddLeavesData->pFrag,
														  pAddLeavesData->pRepo,
														  szKey,
														  pAddLeavesData->nGenerations)  );
}

void SG_dagfrag__load_from_repo__multi(SG_context * pCtx,
									   SG_dagfrag * pFrag,
									   SG_repo* pRepo,
									   SG_rbtree * prbLeaves,
									   SG_int32 nGenerations)
{
	struct _add_leaves_data add_leaves_data;

	SG_NULLARGCHECK_RETURN(pFrag);
	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(prbLeaves);

	add_leaves_data.pFrag = pFrag;
	add_leaves_data.nGenerations = nGenerations;
    add_leaves_data.pRepo = pRepo;

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx, prbLeaves,_sg_dagfrag__add_leaves_callback,&add_leaves_data)  );
}

//////////////////////////////////////////////////////////////////

void SG_dagfrag__query(SG_context * pCtx,
					   SG_dagfrag * pFrag, const char * szHid,
					   SG_dagfrag_state * pQS, SG_int32 * pGen,
					   SG_bool * pbPresent,
					   const SG_dagnode ** ppDagnode)
{
	_my_data * pMyData = NULL;
	SG_bool bPresent = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pFrag);
	SG_NONEMPTYCHECK_RETURN(szHid);
	SG_NULLARGCHECK_RETURN(pbPresent);

	SG_ERR_CHECK_RETURN(  _cache__lookup(pCtx, pFrag,szHid,&pMyData,&bPresent)  );

	*pbPresent = bPresent;
	if (!bPresent)
		return;

	if (pQS)
		*pQS = pMyData->m_state;
	if (pGen)
		*pGen = pMyData->m_genDagnode;

	switch (pMyData->m_state)
	{
	default:
	//case SG_DFS_UNKNOWN:
		SG_ASSERT_RELEASE_RETURN2(  (0),
							(pCtx,"Invalid state [%d] in DAGFRAG Cache for [%s]",
							 pMyData->m_state,szHid)  );

	case SG_DFS_START_MEMBER:
	case SG_DFS_INTERIOR_MEMBER:
		if (ppDagnode)
			*ppDagnode = pMyData->m_pDagnode;
		break;

	case SG_DFS_END_FRINGE:
		if (ppDagnode)
			*ppDagnode = NULL;
		break;
	}
}

//////////////////////////////////////////////////////////////////

struct _fef_data
{
	SG_dagfrag__foreach_end_fringe_callback * pcb;
	void * pVoidCallerData;
};

static void _sg_dagfrag__foreach_end_fringe_callback(SG_context * pCtx,
													 const char * szKey, void * pAssocData, void * pVoidFefData)
{
	// a standard SG_rbtree_foreach_callback.
	//
	// we only pass along the end_fringe nodes to the caller's real callback.

	struct _fef_data * pFefData = (struct _fef_data *)pVoidFefData;
	_my_data * pMyData = (_my_data *)pAssocData;

	if (pMyData->m_state == SG_DFS_END_FRINGE)
		SG_ERR_CHECK_RETURN(  (*pFefData->pcb)(pCtx,szKey,pFefData->pVoidCallerData)  );
}

void SG_dagfrag__foreach_end_fringe(SG_context * pCtx,
									const SG_dagfrag * pFrag,
									SG_dagfrag__foreach_end_fringe_callback * pcb,
									void * pVoidCallerData)
{
	// our caller wants to iterate over the end_fringe nodes in the CACHE.

	struct _fef_data fef_data;

	SG_NULLARGCHECK_RETURN(pFrag);
	SG_NULLARGCHECK_RETURN(pcb);

	fef_data.pcb = pcb;
	fef_data.pVoidCallerData = pVoidCallerData;

	// we wrap their callback with our own so that we can munge the arguments
	// that they see.

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,
											 pFrag->m_pRB_Cache,_sg_dagfrag__foreach_end_fringe_callback,&fef_data)  );
}

//////////////////////////////////////////////////////////////////

static SG_rbtree_foreach_callback _sg_dagfrag__my_create_generation_sorted_member_cache_callback;

static void _sg_dagfrag__my_create_generation_sorted_member_cache_callback(SG_context * pCtx,
																		   const char * szKey,
																		   void * pAssocData,
																		   void * pVoidFrag)
{
	// a standard SG_rbtree_foreach_callback.
	//
	// insert a parallel item in the SORTED CACHE for this item from the regular CACHE.
	// we only want items of type START_MEMBER and INTERIOR_MEMBER in our sorted cache.

	_my_data * pMyData = (_my_data *)pAssocData;
	SG_dagfrag * pFrag = (SG_dagfrag *)pVoidFrag;
	char bufSortKey[SG_HID_MAX_BUFFER_LENGTH + 20];

	switch (pMyData->m_state)
	{
	default:
	//case SG_DFS_UNKNOWN:
	//case SG_DFS_END_FRINGE:
		return;

	case SG_DFS_START_MEMBER:
	case SG_DFS_INTERIOR_MEMBER:
		break;
	}

	// the sort key looks like <generation>.<hid> "%08lx.%s"

	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,
									 bufSortKey,SG_NrElements(bufSortKey),
									 "%08lx.%s",
									 pMyData->m_genDagnode,szKey)  );

	SG_ERR_CHECK_RETURN(  SG_rbtree__update__with_assoc(pCtx,
														pFrag->m_pRB_GenerationSortedMemberCache,bufSortKey,pMyData,NULL)  );
}

static void _my_create_generation_sorted_member_cache(SG_context * pCtx,
													  SG_dagfrag * pFrag)
{
	// everything in the main CACHE is (implicitly) sorted by HID.  there are
	// times when we need this list sorted by generation.
	//
	// here we construct a copy of the CACHE in that alternate ordering.  we
	// simply borrow the associated data pointers of the items in the real
	// CACHE, so we don't own/free them.  we only include items of type
	// START_MEMBER and INTERIOR_MEMBER.
	//
	// WARNING: whenever you change the CACHE (such as during __add_{leaf,leaves}()),
	// WARNING: you must delete/recreate or likewise update this copy.

	SG_RBTREE_NULLFREE(pCtx, pFrag->m_pRB_GenerationSortedMemberCache);
	pFrag->m_pRB_GenerationSortedMemberCache = NULL;

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx,
									&pFrag->m_pRB_GenerationSortedMemberCache)  );
	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,
									  pFrag->m_pRB_Cache,
									  _sg_dagfrag__my_create_generation_sorted_member_cache_callback,
									  pFrag)  );
	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, pFrag->m_pRB_GenerationSortedMemberCache);
	pFrag->m_pRB_GenerationSortedMemberCache = NULL;
}

//////////////////////////////////////////////////////////////////

struct _fm_data
{
	SG_dagfrag * pFrag;
	SG_dagfrag__foreach_member_callback * pcb;
	void * pVoidCallerData;
};

static SG_rbtree_foreach_callback _sg_dagfrag__my_foreach_member_callback;

static void _sg_dagfrag__my_foreach_member_callback(SG_context * pCtx,
													SG_UNUSED_PARAM(const char * szKey),
													void * pAssocData,
													void * pVoidFmData)
{
	// a standard SG_rbtree_foreach_callback.
	//
	// we are iterating on the SORTED MEMBER CACHE.  use the info provided and
	// call out to the caller's real callback.
	//
	// here the key is the funky sort key that we synthesized.  no one needs to
	// see it.
	//
	// our assoc data pointer is the one we borrowed from the real CACHE.

	struct _fm_data * pFmData = (struct _fm_data *)pVoidFmData;
	_my_data * pMyData = (_my_data *)pAssocData;

	SG_UNUSED(szKey);

	SG_ERR_CHECK_RETURN(  (*pFmData->pcb)(pCtx,pMyData->m_pDagnode,pMyData->m_state,pFmData->pVoidCallerData)  );
}

void SG_dagfrag__foreach_member(SG_context * pCtx,
								SG_dagfrag * pFrag,
								SG_dagfrag__foreach_member_callback * pcb,
								void * pVoidCallerData)
{
	// we want to iterate over the START_ and INTERIOR_ MEMBERS in the CACHE.
	// we need to use the SORTED MEMBER CACHE so that ancestors are presented
	// before descendants.

	struct _fm_data fm_data;

	SG_NULLARGCHECK_RETURN(pFrag);
	SG_NULLARGCHECK_RETURN(pcb);

	if (!pFrag->m_pRB_GenerationSortedMemberCache)
		SG_ERR_CHECK_RETURN(  _my_create_generation_sorted_member_cache(pCtx,pFrag)  );

	fm_data.pFrag = pFrag;
	fm_data.pcb = pcb;
	fm_data.pVoidCallerData = pVoidCallerData;

	// we wrap their callback with our own so that we can munge the arguments
	// that they see.

#if TRACE_DAGFRAG && 0
	SG_ERR_CHECK_RETURN(  SG_console(pCtx, SG_CS_STDERR, "SORTED MEMBER CACHE:\r\n")  );
	SG_ERR_CHECK_RETURN(  SG_rbtree_debug__dump_keys_to_console(pCtx, pFrag->m_pRB_GenerationSortedMemberCache)  );
	SG_ERR_CHECK_RETURN(  SG_console__flush(pCtx, SG_CS_STDERR)  );
#endif

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,
											 pFrag->m_pRB_GenerationSortedMemberCache,
											 _sg_dagfrag__my_foreach_member_callback,
											 &fm_data)  );
}

//////////////////////////////////////////////////////////////////
// We serialize a dagfrag to a vhash.  This is a complete dump of
// the intermediate data that we have in memory so that it can be
// recreated by a remote repo and (in theory) let them augment (as
// if they had initially created it in memory).  WE DO NOT EVER
// store the contents of a dagfrag in a blob or compute a hash on
// it, so we DO NOT worry about sorting or any of those other tricks
// to ensure that a HID is uniquely/consistently computed.
//
// This serialization therefore includes more information than we
// share with callers of __query(), __foreach_end_fringe(), or
// __foreach_member().  ***DON'T ABUSE THIS***  That is, if you
// receive a serialization, convert it back to a dagfrag and then
// use one of the above functions to examine items within the
// fragment.  This is especially true for items in the END-FRINGE.
//
// The "data" array will contain the complete contents of the
// pRB_Cache -- all dagnodes (START, INTERIOR, and END-FRINGE)
// and is not sorted.
//
// We do not send the work-queue (because it should be empty and
// it is only needed while processing an __add_leaf()).
//
// We do not send the generation-sorted-member-cache because
// it can be rebuilt from the pRB_Cache as needed.
//
//
// The serialization has the following form:
//
// TODO This summary of the vhash is out-of-date.  Fix this.
//
// vhash "Frag" {
//   KEY_VERSION : string = 1
//   KEY_DATA    : varray [
//     vhash "my_data" {
//       KEY_DAGNODE   : vhash "dagnode" { ... }
//       KEY_GEN       : integer m_genDagnode
//       KEY_DFS_STATE : integer m_state
//     }
//     ...
//   ]
// }

/* TODO if we put the repo ids in the frag, make sure we
 * serialize them here */

#define KEY_VERSION			"ver"
#define KEY_DAGNUM			"dagnum"
#define KEY_DATA			"data"
#define KEY_ACTUAL_DAGNODE	"dagnode"
#define KEY_DAGNODE_ID		"id"
#define KEY_REPO_ID		    "repo_id"
#define KEY_ADMIN_ID		"admin_id"
#define KEY_GEN				"gen"
#define KEY_DFS_STATE		"state"

#define VALUE_VERSION		"1"

struct _serialize_data
{
	SG_vhash * pvhFrag;			// entire vhash of dagfrag
	SG_varray * pvaMyData;		// varray of my_data
};

static SG_rbtree_foreach_callback _serialize_data_cb;

static void _serialize_data_cb(SG_context * pCtx,
							   const char * szHid,
							   void * pAssocData,
							   void * pVoidSerializeData)
{
	struct _serialize_data * pSerializeData = (struct _serialize_data *)pVoidSerializeData;
	_my_data * pMyData = (_my_data *)pAssocData;

	SG_vhash * pvhMyData = NULL;
	SG_vhash * pvhDagnode = NULL;

	SG_ERR_CHECK(  SG_VHASH__ALLOC__SHARED(pCtx,&pvhMyData,5,pSerializeData->pvhFrag)  );

	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx,pvhMyData,KEY_DFS_STATE,(SG_int64)pMyData->m_state)  );

    if (SG_DFS_END_FRINGE == pMyData->m_state)
    {
        SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx,pvhMyData,KEY_DAGNODE_ID,szHid)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_dagnode__to_vhash__shared(pCtx,pMyData->m_pDagnode,pvhMyData,&pvhDagnode)  );
        SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx,pvhMyData,KEY_ACTUAL_DAGNODE,&pvhDagnode)  );
    }

#if DEBUG && TRACE_DAGFRAG && 0
	SG_ERR_CHECK(  SG_vhash_debug__dump_to_console(pCtx, pvhMyData)  );
#endif
	SG_ERR_CHECK(  SG_varray__append__vhash(pCtx,pSerializeData->pvaMyData,&pvhMyData)  );

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhMyData);
	SG_VHASH_NULLFREE(pCtx, pvhDagnode);
}

void SG_dagfrag__to_vhash__shared(SG_context * pCtx,
								  SG_dagfrag * pFrag,
								  SG_vhash * pvhShared,
								  SG_vhash ** ppvhNew)
{
	SG_vhash * pvhFrag = NULL;
	SG_varray * pvaMyData = NULL;
	struct _serialize_data serialize_data;

	SG_NULLARGCHECK_RETURN(pFrag);
	SG_NULLARGCHECK_RETURN(ppvhNew);

	SG_ASSERT(  (! IS_TRANSIENT(pFrag))  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC__SHARED(pCtx,&pvhFrag,5,pvhShared)  );

	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx,pvhFrag,KEY_REPO_ID,pFrag->m_sz_repo_id)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx,pvhFrag,KEY_ADMIN_ID,pFrag->m_sz_admin_id)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx,pvhFrag,KEY_DAGNUM,(SG_int64)pFrag->m_iDagNum)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx,pvhFrag,KEY_VERSION,VALUE_VERSION)  );

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx,&pvaMyData)  );

	serialize_data.pvhFrag = pvhFrag;
	serialize_data.pvaMyData = pvaMyData;

	// walk the complete RB_Cache and add complete info for each my_data item
	// (regardless of whether the dagnode is a START or INTERIOR member or in
	// the END-FRINGE.  These will be in an essentially random order (HID).

	SG_ERR_CHECK(  SG_rbtree__foreach(pCtx,
									  pFrag->m_pRB_Cache,
									  _serialize_data_cb,
									  &serialize_data)  );

	SG_ERR_CHECK(  SG_vhash__add__varray(pCtx,pvhFrag,KEY_DATA,&pvaMyData)  );

	*ppvhNew = pvhFrag;
	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhFrag);
	SG_VARRAY_NULLFREE(pCtx, pvaMyData);
}

//////////////////////////////////////////////////////////////////

struct _deserialize_data
{
	SG_dagfrag *		pFrag;
};

static SG_varray_foreach_callback _deserialize_data_ver_1_cb;

static void _deserialize_data_ver_1_cb(SG_context * pCtx,
									   void * pVoidDeserializeData,
									   SG_UNUSED_PARAM(const SG_varray * pva),
									   SG_UNUSED_PARAM(SG_uint32 ndx),
									   const SG_variant * pVariant)
{
	struct _deserialize_data * pDeserializeData = (struct _deserialize_data *)pVoidDeserializeData;
	SG_vhash * pvhMyData;
	SG_vhash * pvhDagnode;
	SG_int64 gen64, state64;
	_my_data * pMyData;
	SG_dagnode * pDagnode = NULL;
    const char* psz_id = NULL;

	SG_UNUSED(pva);
	SG_UNUSED(ndx);


	SG_ERR_CHECK(  SG_variant__get__vhash(pCtx,pVariant,&pvhMyData)  );
#if DEBUG && TRACE_DAGFRAG && 0
	SG_ERR_CHECK(  SG_vhash_debug__dump_to_console(pCtx, pvhMyData)  );
#endif

	SG_ERR_CHECK(  SG_vhash__get__int64(pCtx,pvhMyData,KEY_DFS_STATE,&state64)  );

    if (SG_DFS_END_FRINGE == state64)
    {
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx,pvhMyData,KEY_DAGNODE_ID,&psz_id)  );
		SG_ERR_CHECK(  _cache__add__fringe(pCtx,pDeserializeData->pFrag, psz_id)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx,pvhMyData,KEY_ACTUAL_DAGNODE,&pvhDagnode)  );
		SG_ERR_CHECK(  SG_vhash__get__int64(pCtx,pvhDagnode,KEY_GEN,&gen64)  );
        SG_ERR_CHECK(  SG_dagnode__alloc__from_vhash(pCtx, &pDagnode, pvhDagnode)  );

        SG_ERR_CHECK(  _cache__add__dagnode(pCtx,
											pDeserializeData->pFrag,
											(SG_int32)gen64,
											pDagnode,
											(SG_uint32)state64,
											&pMyData)  );
        pDagnode = NULL;	// cache owns it now.
    }
	return;

fail:
	SG_DAGNODE_NULLFREE(pCtx, pDagnode);
}

void SG_dagfrag__alloc__from_vhash(SG_context * pCtx,
								   SG_dagfrag ** ppNew,
								   const SG_vhash * pvhFrag)
{
	const char * szVersion;
	SG_dagfrag * pFrag = NULL;
	struct _deserialize_data deserialize_data;
    SG_int64 iDagNum = 0;

	SG_NULLARGCHECK_RETURN(ppNew);
	SG_NULLARGCHECK_RETURN(pvhFrag);

	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx,pvhFrag,KEY_VERSION,&szVersion)  );
	if (strcmp(szVersion,"1") == 0)
	{
		// handle dagfrags that were serialized by software compiled with
		// VALUE_VERSION == 1.

		SG_varray * pvaMyData;
        const char* psz_repo_id = NULL;
        const char* psz_admin_id = NULL;

        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx,pvhFrag,KEY_REPO_ID,&psz_repo_id)  );
        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx,pvhFrag,KEY_ADMIN_ID,&psz_admin_id)  );
        SG_ERR_CHECK(  SG_vhash__get__int64(pCtx,pvhFrag,KEY_DAGNUM,&iDagNum)  );

        SG_ERR_CHECK(  SG_dagfrag__alloc(pCtx,&pFrag,psz_repo_id,psz_admin_id,(SG_uint32) iDagNum)  );

		SG_ERR_CHECK(  SG_vhash__get__varray(pCtx,pvhFrag,KEY_DATA,&pvaMyData)  );

		deserialize_data.pFrag = pFrag;
		SG_ERR_CHECK(  SG_varray__foreach(pCtx,
										  pvaMyData,
										  _deserialize_data_ver_1_cb,
										  &deserialize_data)  );

		*ppNew = pFrag;
		return;
	}
	else
	{
		SG_ERR_THROW(  SG_ERR_DAGFRAG_DESERIALIZATION_VERSION  );
	}

fail:
	SG_DAGFRAG_NULLFREE(pCtx, pFrag);
}

//////////////////////////////////////////////////////////////////

void SG_dagfrag__get_dagnum(SG_context * pCtx, SG_dagfrag* pFrag, SG_uint32* piDagNum)
{
	SG_NULLARGCHECK_RETURN(pFrag);
	SG_NULLARGCHECK_RETURN(piDagNum);

	*piDagNum = pFrag->m_iDagNum;
}

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
void SG_dagfrag__equal(SG_context * pCtx,
					   const SG_dagfrag * pFrag1, const SG_dagfrag * pFrag2, SG_bool * pbResult)
{
	SG_rbtree_iterator * pIter1 = NULL;
	SG_rbtree_iterator * pIter2 = NULL;
	const char * szKey1;
	const char * szKey2;
	_my_data * pMyData1;
	_my_data * pMyData2;
	SG_bool bFound1, bFound2, bEqualDagnodes;
	SG_bool bFinalResult = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pFrag1);
	SG_NULLARGCHECK_RETURN(pFrag2);
	SG_NULLARGCHECK_RETURN(pbResult);

	// we compare the RB-Cache because it has everything in it.
	// the work-queues are transient (used during __add_leaf()).
	// the generation-sorted-member-cache is only around when
	// needed and is a subset of the RB-Cache.

	// since the RB-Cache is ordered by HID, we don't need to do
	// any sorting.  just walk both versions in parallel.

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,&pIter1,pFrag1->m_pRB_Cache,&bFound1,&szKey1,(void **)&pMyData1)  );
	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,&pIter2,pFrag2->m_pRB_Cache,&bFound2,&szKey2,(void **)&pMyData2)  );

	while (1)
	{
		if (!bFound1 && !bFound2)
			goto Equal;
		if (!bFound1 || !bFound2)
			goto Different;
		if (strcmp(szKey1,szKey2) != 0)
			goto Different;

		if (pMyData1->m_state != pMyData2->m_state)
			goto Different;

        if (SG_DFS_END_FRINGE != pMyData1->m_state)
        {
            if (pMyData1->m_genDagnode != pMyData2->m_genDagnode)
                goto Different;
            SG_ERR_CHECK(  SG_dagnode__equal(pCtx,pMyData1->m_pDagnode,pMyData2->m_pDagnode,&bEqualDagnodes)  );
            if (!bEqualDagnodes)
                goto Different;
        }

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter1,&bFound1,&szKey1,(void **)&pMyData1)  );
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter2,&bFound2,&szKey2,(void **)&pMyData2)  );
	}

Equal:
	bFinalResult = SG_TRUE;
Different:
	*pbResult = bFinalResult;
fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter1);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter2);
}
#endif//DEBUG

void SG_dagfrag__eat_other_frag(SG_context * pCtx,
								SG_dagfrag* pConsumerFrag,
								SG_dagfrag** ppFragToBeEaten)
{
	SG_rbtree_iterator* pit = NULL;
	SG_bool b = SG_FALSE;
	_my_data * pMyData = NULL;
	const char* psz_id = NULL;
	SG_dagfrag* pFragToBeEaten = NULL;

	SG_NULLARGCHECK_RETURN(pConsumerFrag);
	SG_NULLARGCHECK_RETURN(ppFragToBeEaten);
	SG_NULLARGCHECK_RETURN(*ppFragToBeEaten);

	pFragToBeEaten = *ppFragToBeEaten;

#if DEBUG && TRACE_DAGFRAG
	SG_ERR_CHECK(  SG_dagfrag_debug__dump__console(pCtx, pFragToBeEaten, "frag to be eaten", 0, SG_CS_STDOUT)  );
	SG_ERR_CHECK(  SG_rbtree_debug__dump_keys_to_console(pCtx, pFragToBeEaten->m_pRB_Cache)  );
	SG_ERR_CHECK(  SG_console__flush(pCtx, SG_CS_STDERR)  );
	SG_ERR_CHECK(  SG_dagfrag_debug__dump__console(pCtx, pConsumerFrag, "new frag before meal", 0, SG_CS_STDOUT)  );
	SG_ERR_CHECK(  SG_rbtree_debug__dump_keys_to_console(pCtx, pConsumerFrag->m_pRB_Cache)  );
	SG_ERR_CHECK(  SG_console__flush(pCtx, SG_CS_STDERR)  );
#endif

	if (
		(pConsumerFrag->m_iDagNum != pFragToBeEaten->m_iDagNum)
		|| (0 != strcmp(pConsumerFrag->m_sz_repo_id, pFragToBeEaten->m_sz_repo_id))
		|| (0 != strcmp(pConsumerFrag->m_sz_admin_id, pFragToBeEaten->m_sz_admin_id))
		)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_REPO_MISMATCH  );
	}

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit,pFragToBeEaten->m_pRB_Cache,&b,&psz_id,(void **)&pMyData)  );
	while (b)
	{
		if (pMyData->m_pDagnode)
		{
			SG_ERR_CHECK(  SG_dagfrag__add_dagnode(pCtx, pConsumerFrag, &pMyData->m_pDagnode)  );
		}
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit,&b,&psz_id,(void **)&pMyData)  );
	}
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

	SG_DAGFRAG_NULLFREE(pCtx, pFragToBeEaten);
	*ppFragToBeEaten = NULL;

#if DEBUG && TRACE_DAGFRAG
	SG_ERR_CHECK(  SG_dagfrag_debug__dump__console(pCtx, pConsumerFrag, "new frag after meal", 0, SG_CS_STDOUT)  );
	SG_ERR_CHECK(  SG_rbtree_debug__dump_keys_to_console(pCtx, pConsumerFrag->m_pRB_Cache)  );
	SG_ERR_CHECK(  SG_console__flush(pCtx, SG_CS_STDERR)  );
#endif

	return;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
}
