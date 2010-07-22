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
   Originally based on code written by Julienne Walker, obtained from
   the following location, and identified there as "public domain":

     http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_rbtree.aspx
*/

#include <sg.h>

typedef struct _sg_rbtree_node
{
	int red;
	const char* pszKey;
	void* assocData;
	struct _sg_rbtree_node *link[2];
} sg_rbtree_node;

typedef struct _sg_nodesubpool
{
	SG_uint32 count;
	SG_uint32 space;
	sg_rbtree_node* pNodes;
	struct _sg_nodesubpool* pNext;
} sg_nodesubpool;

typedef struct _sg_rbnodepool
{
	SG_uint32 subpool_space;
	SG_uint32 count_subpools;
	SG_uint32 count_items;
	sg_nodesubpool *pHead;
} sg_rbnodepool;

void sg_nodesubpool__alloc(SG_context * pCtx, SG_uint32 space, sg_nodesubpool* pNext,
						   sg_nodesubpool ** ppNew)
{
	sg_nodesubpool* pThis = NULL;
	sg_rbtree_node * pNodes = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pThis)  );
	SG_ERR_CHECK(  SG_allocN(pCtx, space, pNodes)  );

	pThis->space = space;
	pThis->pNodes = pNodes;
	pThis->pNext = pNext;
	pThis->count = 0;

	*ppNew = pThis;

	return;

fail:
	SG_NULLFREE(pCtx, pNodes);
	SG_NULLFREE(pCtx, pThis);
}

void sg_nodesubpool__free(SG_context * pCtx, sg_nodesubpool *psp)
{
	// free this node and everything in the next-chain.
	// i converted this to a loop rather than recursive
	// so that we don't blow a stack (either procedure or
	// error-level).

	while (psp)
	{
		sg_nodesubpool * pspNext = psp->pNext;

		SG_NULLFREE(pCtx, psp->pNodes);
		SG_NULLFREE(pCtx, psp);

		psp = pspNext;
	}
}

void sg_rbnodepool__alloc(SG_context* pCtx, sg_rbnodepool** ppResult, SG_uint32 subpool_space)
{
	sg_rbnodepool* pThis = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pThis)  );

	pThis->subpool_space = subpool_space;

	SG_ERR_CHECK(  sg_nodesubpool__alloc(pCtx, pThis->subpool_space, NULL, &pThis->pHead)  );

	pThis->count_subpools = 1;

	*ppResult = pThis;

	return;

fail:
	SG_NULLFREE(pCtx, pThis);
}

void sg_rbnodepool__free(SG_context * pCtx, sg_rbnodepool* pPool)
{
	if (!pPool)
	{
		return;
	}

	SG_ERR_IGNORE(  sg_nodesubpool__free(pCtx, pPool->pHead)  );
	SG_NULLFREE(pCtx, pPool);
}

void sg_rbnodepool__add(SG_context* pCtx, sg_rbnodepool* pPool, sg_rbtree_node** ppResult)
{
	if ((pPool->pHead->count + 1) > pPool->pHead->space)
	{
		sg_nodesubpool* psp = NULL;

		SG_ERR_CHECK_RETURN(  sg_nodesubpool__alloc(pCtx, pPool->subpool_space, pPool->pHead, &psp)  );

		pPool->pHead = psp;
		pPool->count_subpools++;
	}

	*ppResult = &(pPool->pHead->pNodes[pPool->pHead->count++]);
	pPool->count_items++;

	return;
}

struct _sg_rbtree
{
	sg_rbtree_node *root;
	sg_rbnodepool* pNodePool;
	SG_strpool *pStrPool;
	SG_rbtree_compare_function_callback * pfnCompare;
	SG_uint32 count;
	SG_bool strpool_is_mine;
};

void SG_rbtree__get_strpool(
	SG_context* pCtx,
    SG_rbtree* prb,
    SG_strpool** ppp
    )
{
    SG_NULLARGCHECK_RETURN(prb);
    SG_NULLARGCHECK_RETURN(ppp);

    *ppp = prb->pStrPool;
}

void SG_rbtree__alloc__params2(
	SG_context* pCtx,
	SG_rbtree** ppNew,
	SG_uint32 guess,
	SG_strpool* pStrPool,
	SG_rbtree_compare_function_callback * pfnCompare
	)
{
	SG_rbtree* ptree = NULL;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, ptree)  );

	if (guess == 0)
	{
		guess = 10;
	}

	if (pfnCompare)
		ptree->pfnCompare = pfnCompare;
	else
		ptree->pfnCompare = SG_RBTREE__DEFAULT__COMPARE_FUNCTION;

	ptree->root = NULL;

	if (pStrPool)
	{
		ptree->strpool_is_mine = SG_FALSE;
		ptree->pStrPool = pStrPool;
	}
	else
	{
		ptree->strpool_is_mine = SG_TRUE;

        // TODO this guess should probably not be passed down like this.
        // guess*64 is the total number of bytes, but it is not the
        // desired size of the subpool, which is what strpool__alloc wants.

		SG_ERR_CHECK(  SG_STRPOOL__ALLOC(pCtx, &ptree->pStrPool, guess * 64)  );
	}

	SG_ERR_CHECK(  sg_rbnodepool__alloc(pCtx, &ptree->pNodePool, guess)  );

	ptree->count = 0;

	*ppNew = ptree;

	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, ptree);
}

void SG_rbtree__alloc__params(SG_context* pCtx, SG_rbtree** ppNew, SG_uint32 guess, SG_strpool* pStrPool)
{
	SG_ERR_CHECK_RETURN(  SG_rbtree__alloc__params2(pCtx, ppNew, guess, pStrPool, NULL)  );		// do not use SG_RBTREE__ALLOC__PARAMS2 macro here.
}

void SG_rbtree__alloc(
	SG_context* pCtx,
	SG_rbtree** ppNew
	)
{
	SG_ERR_CHECK_RETURN(  SG_rbtree__alloc__params2(pCtx, ppNew, 128, NULL, NULL)  );		// do not use SG_RBTREE__ALLOC__PARAMS macro here.
}

static int sg_rbtree__is_red ( sg_rbtree_node *root )
{
	return root != NULL && root->red == 1;
}

static sg_rbtree_node *sg_rbtree_single ( sg_rbtree_node *root, int dir )
{
	sg_rbtree_node *save = root->link[!dir];

	root->link[!dir] = save->link[dir];
	save->link[dir] = root;

	root->red = 1;
	save->red = 0;

	return save;
}

static sg_rbtree_node *sg_rbtree_double ( sg_rbtree_node *root, int dir )
{
	root->link[!dir] = sg_rbtree_single ( root->link[!dir], !dir );
	return sg_rbtree_single ( root, dir );
}

#if 0
static int sg_rbtree_rb_assert ( sg_rbtree_node *root, SG_rbtree_compare_function_callback * pfnCompare )
{
	int lh, rh;

	if ( root == NULL )
		return 1;
	else {
		sg_rbtree_node *ln = root->link[0];
		sg_rbtree_node *rn = root->link[1];

		/* Consecutive red links */
		if ( sg_rbtree__is_red ( root ) )
		{
			if ( sg_rbtree__is_red ( ln ) || sg_rbtree__is_red ( rn ) )
			{
				puts ( "Red violation" );
				return 0;
			}
		}

		lh = sg_rbtree_rb_assert ( ln, pfnCompare );
		rh = sg_rbtree_rb_assert ( rn, pfnCompare );

		/* Invalid binary search tree */
		if ( ( ln != NULL && (*pfnCompare)(ln->pszKey, root->pszKey) >= 0 )
			 || ( rn != NULL && (*pfnCompare)(rn->pszKey, root->pszKey) <= 0 ) )
		{
			puts ( "Binary tree violation" );
			return 0;
		}

		/* Black height mismatch */
		if ( lh != 0 && rh != 0 && lh != rh )
		{
			puts ( "Black violation" );
			return 0;
		}

		/* Only count black links */
		if ( lh != 0 && rh != 0 )
			return sg_rbtree__is_red ( root ) ? lh : lh + 1;
		else
			return 0;
	}
}

void SG_rbtree__verify(
	SG_UNUSED_PARM( SG_context* pCtx ),
	const SG_rbtree* prb
	)
{
	SG_UNUSED(  pCtx  );
	sg_rbtree_rb_assert(prb->root, prb->pfnCompare);
	return SG_ERR_OK;
}
#endif

static void sg_rbtree__alloc_node (SG_context* pCtx, SG_rbtree *tree, sg_rbtree_node** ppnode, const char* pszKey, void* assocData )
{
	sg_rbtree_node* rn = NULL;

	SG_ERR_CHECK(  sg_rbnodepool__add(pCtx, tree->pNodePool, &rn)  );

	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, tree->pStrPool, pszKey, &(rn->pszKey))  );
	rn->assocData = assocData;
	rn->red = 1; /* 1 is red, 0 is black */
	rn->link[0] = NULL;
	rn->link[1] = NULL;

	*ppnode = rn;

	tree->count++;

fail:
	return;
}

void SG_rbtree__free(SG_context * pCtx, SG_rbtree* ptree)
{
	if (!ptree)
	{
		return;
	}

	if (ptree->strpool_is_mine && ptree->pStrPool)
	{
		SG_STRPOOL_NULLFREE(pCtx, ptree->pStrPool);
	}

	SG_ERR_IGNORE(  sg_rbnodepool__free(pCtx, ptree->pNodePool)  );
	SG_NULLFREE(pCtx, ptree);
}

static void sg_rbtreenode__find(
	SG_context* pCtx,
	const sg_rbtree_node* pnode,
	SG_rbtree_compare_function_callback * pfnCompare,	// i'm passing this down to avoid having a backlink in node to rbtree
	const char* pszKey,
	const sg_rbtree_node** pp_result
	)
{
	int cmp;

	if (!pnode)
		return;

	cmp = (*pfnCompare)(pnode->pszKey, pszKey);
	if (0 == cmp)
	{
        *pp_result = pnode;
	}
	else
	{
		int dir = cmp < 0;

		SG_ERR_CHECK_RETURN(  sg_rbtreenode__find(pCtx, pnode->link[dir], pfnCompare, pszKey, pp_result)  );
	}
}

void SG_rbtree__key(
	SG_context* pCtx,
	const SG_rbtree* prb,
	const char* psz,
    const char** ppsz_key
	)
{
    const sg_rbtree_node* pnode = NULL;

	SG_ERR_CHECK_RETURN(  sg_rbtreenode__find(pCtx, prb->root, prb->pfnCompare, psz, &pnode)  );

	if (pnode)
	{
        *ppsz_key = pnode->pszKey;
	}
	else
	{
        *ppsz_key = NULL;
	}
}

void SG_rbtree__find(
	SG_context* pCtx,
	const SG_rbtree* prb,
	const char* pszKey,
    SG_bool* pbFound,
	void** pAssocData
	)
{
    const sg_rbtree_node* pnode = NULL;

	SG_ERR_CHECK_RETURN(  sg_rbtreenode__find(pCtx, prb->root, prb->pfnCompare, pszKey, &pnode)  );

	if (pnode)
	{
        if (pbFound)
        {
            *pbFound = SG_TRUE;
        }

        if (pAssocData)
        {
            *pAssocData = pnode->assocData;
        }
	}
	else
	{
        if (pbFound)
        {
            *pbFound = SG_FALSE;
        }
        else
        {
            SG_ERR_THROW_RETURN(  SG_ERR_NOT_FOUND  );
        }
	}
}

static void sg_rbtree__add_or_update(
	SG_context* pCtx,
	SG_rbtree *tree,
	const char* pszKey,
	SG_bool bUpdate,
	void* assocData,
	void** pOldAssoc,
	const char** ppKeyPooled
	)
{
	SG_bool bCreated = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pszKey);

	if ( tree->root == NULL )
	{
		/* Empty tree case */
		SG_ERR_CHECK(  sg_rbtree__alloc_node (pCtx, tree, &tree->root, pszKey, assocData )  );
		bCreated = SG_TRUE;
		if (ppKeyPooled)
		{
			*ppKeyPooled = tree->root->pszKey;
		}
	}
	else
	{
		sg_rbtree_node head = {0,0,0,{0,0}}; /* False tree root */

		sg_rbtree_node *g, *t;     /* Grandparent & parent */
		sg_rbtree_node *p, *q;     /* Iterator & parent */
		int dir = 0;
		int last = 0;

		/* Set up helpers */
		t = &head;
		g = p = NULL;
		q = t->link[1] = tree->root;

		/* Search down the tree */
		for ( ; ; )
		{
			int cmp;

			if (!q)
			{
				/* Insert new node at the bottom */
				SG_ERR_CHECK(  sg_rbtree__alloc_node (pCtx, tree, &q, pszKey, assocData )  );
				if (ppKeyPooled)
				{
					*ppKeyPooled = q->pszKey;
				}
				p->link[dir] = q;
				bCreated = SG_TRUE;
			}
			else if ( sg_rbtree__is_red ( q->link[0] ) && sg_rbtree__is_red ( q->link[1] ) )
			{
				/* Color flip */
				q->red = 1;
				q->link[0]->red = 0;
				q->link[1]->red = 0;
			}

			/* Fix red violation */
			if ( sg_rbtree__is_red ( q ) && sg_rbtree__is_red ( p ) )
			{
				int dir2 = t->link[1] == g;

				if ( q == p->link[last] )
				{
					t->link[dir2] = sg_rbtree_single ( g, !last );
				}
				else
				{
					t->link[dir2] = sg_rbtree_double ( g, !last );
				}
			}

			/* Stop if found */
			if (bCreated)
			{
				break;
			}

			cmp = (*tree->pfnCompare)(q->pszKey, pszKey);

			if (0 == cmp)
			{
				if (bUpdate)
				{
					if (pOldAssoc)
					{
						*pOldAssoc = q->assocData;
					}
					q->assocData = assocData;
				}

				break;
			}

			last = dir;
			dir = cmp < 0;

			/* Update helpers */
			if ( g != NULL )
			{
				t = g;
			}
			g = p, p = q;
			q = q->link[dir];
		}

		/* Update root */
		tree->root = head.link[1];
	}

	/* Make root black */
	tree->root->red = 0;

	if (bCreated)
	{
		if (pOldAssoc)
		{
			*pOldAssoc = NULL;
		}
		return;
	}

	if (bUpdate)
	{
		return;
	}

	SG_ERR_THROW2_RETURN( SG_ERR_RBTREE_DUPLICATEKEY,
						  (pCtx, "Key [%s]", pszKey)  );

fail:
	return;
}

void SG_memoryblob__pack(
		SG_context* pCtx,
		const SG_byte* p,
		SG_uint32 len,
		SG_byte* pDest
		)
{
	SG_NULLARGCHECK_RETURN(pDest);
	SG_NULLARGCHECK_RETURN(len);

	pDest[0] = (SG_byte) ((len >> 24) & 0xff);
	pDest[1] = (SG_byte) ((len >> 16) & 0xff);
	pDest[2] = (SG_byte) ((len >>  8) & 0xff);
	pDest[3] = (SG_byte) ((len >>  0) & 0xff);

	if (p)
	{
		memcpy(pDest + 4, p, len);
	}

	return;
}

void SG_memoryblob__unpack(
		SG_context* pCtx,
		const SG_byte* pPacked,
		const SG_byte** pp,
		SG_uint32* piLen
		)
{
	SG_NULLARGCHECK_RETURN(pPacked);
	SG_NULLARGCHECK_RETURN(pp);
	SG_NULLARGCHECK_RETURN(piLen);

	*piLen = (
		  (pPacked[0] << 24)
		| (pPacked[1] << 16)
		| (pPacked[2] <<  8)
		|  pPacked[3]
		);

	*pp = pPacked + 4;

	return;
}

void SG_rbtree__memoryblob__alloc(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* pszKey,
	SG_uint32 len,
	SG_byte** pp
	)
{
	SG_byte* pPacked = NULL;

	SG_ERR_CHECK(  SG_strpool__add__len(pCtx, prb->pStrPool, len + 4, (const char**) &pPacked)  );
	SG_ERR_CHECK(  SG_memoryblob__pack(pCtx, NULL, len, pPacked)  );
	SG_ERR_CHECK(  sg_rbtree__add_or_update(pCtx, prb, pszKey, SG_FALSE, pPacked, NULL, NULL)  );
	*pp = (pPacked + 4);

fail:
	return;
}

void SG_rbtree__memoryblob__add__name(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* pszKey,
	const SG_byte* p,
	SG_uint32 len
	)
{
	SG_byte* pPacked = NULL;

	SG_ERR_CHECK(  SG_strpool__add__len(pCtx, prb->pStrPool, len + 4, (const char**) &pPacked)  );
	SG_ERR_CHECK(  SG_memoryblob__pack(pCtx, p, len, pPacked)  );
	SG_ERR_CHECK(  sg_rbtree__add_or_update(pCtx, prb, pszKey, SG_FALSE, pPacked, NULL, NULL)  );

fail:
	return;
}

void SG_rbtree__memoryblob__add__hid(
	SG_context* pCtx,
	SG_rbtree* prb,
	SG_repo * pRepo,
	const SG_byte* p,
	SG_uint32 len,
	const char** ppszhid
	)
{
	SG_byte* pPacked = NULL;
	char* pszhid = NULL;
	const char* pszKeyPooled;

	SG_ERR_CHECK(  SG_repo__alloc_compute_hash__from_bytes(pCtx, pRepo, len, p, &pszhid)  );

	SG_ERR_CHECK(  SG_strpool__add__len(pCtx, prb->pStrPool, len + 4, (const char**) &pPacked)  );
	SG_ERR_CHECK(  SG_memoryblob__pack(pCtx, p, len, pPacked)  );
	SG_ERR_CHECK(  sg_rbtree__add_or_update(pCtx, prb, pszhid, SG_FALSE, pPacked, NULL, &pszKeyPooled)  );
    SG_NULLFREE(pCtx, pszhid);

	*ppszhid = pszKeyPooled;

	return;

fail:
	SG_NULLFREE(pCtx, pszhid);
}

void SG_rbtree__memoryblob__get(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* pszhid,
	const SG_byte** pp,
	SG_uint32* piLen
	)
{
	SG_byte* pPacked = NULL;

	SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb, pszhid, NULL, (void**) &pPacked)  );
	SG_ERR_CHECK(  SG_memoryblob__unpack(pCtx, pPacked, pp, piLen)  );

fail:
	return;
}

void SG_rbtree__add__return_pooled_key(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* psz,
    const char** pp
	)
{
	SG_ERR_CHECK_RETURN(  sg_rbtree__add_or_update(pCtx, prb, psz, SG_FALSE, NULL, NULL, pp)  );
}

void SG_rbtree__add(
	SG_context* pCtx,
	SG_rbtree *tree,
	const char* pszKey
	)
{
	SG_ERR_CHECK_RETURN(  sg_rbtree__add_or_update(pCtx, tree, pszKey, SG_FALSE, NULL, NULL, NULL)  );
}

void SG_rbtree__add__with_assoc(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* pszKey,
	void* assoc
	)
{
	SG_ERR_CHECK_RETURN(  sg_rbtree__add_or_update(pCtx, prb, pszKey, SG_FALSE, assoc, NULL, NULL)  );
}

void SG_rbtree__add__with_pooled_sz(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* pszKey,
	const char* pszAssoc
	)
{
    const char* pszPooledAssoc = NULL;

	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, prb->pStrPool, pszAssoc, &(pszPooledAssoc))  );

	SG_ERR_CHECK_RETURN(  sg_rbtree__add_or_update(pCtx, prb, pszKey, SG_FALSE, (void*) pszPooledAssoc, NULL, NULL)  );

fail:
    return;
}

void SG_rbtree__update(
	SG_context* pCtx,
	SG_rbtree *tree,
	const char* pszKey
	)
{
	SG_ERR_CHECK_RETURN(  sg_rbtree__add_or_update(pCtx, tree, pszKey, SG_TRUE, NULL, NULL, NULL)  );
}

void SG_rbtree__update__with_assoc(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* pszKey,
	void* assoc,
	void** pOldAssoc
	)
{
	SG_ERR_CHECK_RETURN(  sg_rbtree__add_or_update(pCtx, prb, pszKey, SG_TRUE, assoc, pOldAssoc, NULL)  );
}

void SG_rbtree__update__with_pooled_sz(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* pszKey,
	const char* pszAssoc
	)
{
    const char* pszPooledAssoc = NULL;

	SG_ERR_CHECK(  SG_strpool__add__sz(pCtx, prb->pStrPool, pszAssoc, &(pszPooledAssoc))  );
	SG_ERR_CHECK_RETURN(  sg_rbtree__add_or_update(pCtx, prb, pszKey, SG_TRUE, (void*) pszPooledAssoc, NULL, NULL)  );

fail:
    return;
}


void SG_rbtree__remove__with_assoc( SG_context* pCtx, SG_rbtree *tree, const char* pszKey, void** ppAssocData )
{
	if (!tree->root)
	{
		SG_ERR_THROW_RETURN( SG_ERR_NOT_FOUND );
	}
	else
	{
		sg_rbtree_node head = {0,0,0,{0,0}}; /* False tree root */
		sg_rbtree_node *q, *p, *g; /* Helpers */
		sg_rbtree_node *f = NULL;  /* Found item */
		int dir = 1;

		/* Set up helpers */
		q = &head;
		g = p = NULL;
		q->link[1] = tree->root;

		/* Search and push a red down */
		while ( q->link[dir] != NULL )
		{
			int cmp;
			int last = dir;

			/* Update helpers */
			g = p, p = q;
			q = q->link[dir];
			cmp = (*tree->pfnCompare)(q->pszKey, pszKey);
			dir = cmp < 0;

			/* Save found node */
			if ( 0 == cmp )
			{
				f = q;
			}

			/* Push the red node down */
			if ( !sg_rbtree__is_red ( q ) && !sg_rbtree__is_red ( q->link[dir] ) )
			{
				if ( sg_rbtree__is_red ( q->link[!dir] ) )
				{
					p = p->link[last] = sg_rbtree_single ( q, dir );
				}
				else if ( !sg_rbtree__is_red ( q->link[!dir] ) )
				{
					sg_rbtree_node *s = p->link[!last];

					if ( s != NULL )
					{
						if ( !sg_rbtree__is_red ( s->link[!last] ) && !sg_rbtree__is_red ( s->link[last] ) )
						{
							/* Color flip */
							p->red = 0;
							s->red = 1;
							q->red = 1;
						}
						else
						{
							int dir2 = g->link[1] == p;

							if ( sg_rbtree__is_red ( s->link[last] ) )
							{
								g->link[dir2] = sg_rbtree_double ( p, last );
							}
							else if ( sg_rbtree__is_red ( s->link[!last] ) )
							{
								g->link[dir2] = sg_rbtree_single ( p, last );
							}

							/* Ensure correct coloring */
							q->red = g->link[dir2]->red = 1;
							g->link[dir2]->link[0]->red = 0;
							g->link[dir2]->link[1]->red = 0;
						}
					}
				}
			}
		}

		/* Replace and remove if found */
		if ( f != NULL )
		{
			if (ppAssocData)
			{
				*ppAssocData = f->assocData;
			}

			f->pszKey = q->pszKey;
			f->assocData = q->assocData;
			p->link[p->link[1] == q] =
				q->link[q->link[0] == NULL];

			tree->count--;
		}
		else
		{
			SG_ERR_THROW(  SG_ERR_NOT_FOUND  );
		}

		/* Update root and make it black */
		tree->root = head.link[1];
		if ( tree->root != NULL )
		{
			tree->root->red = 0;
		}
	}

fail:
	return;
}

void SG_rbtree__remove ( SG_context* pCtx, SG_rbtree *tree, const char* pszKey )
{
	SG_ERR_CHECK_RETURN(  SG_rbtree__remove__with_assoc(pCtx, tree, pszKey, NULL)  );
}

void sg_rbtreenode__calc_depth(
	SG_context* pCtx,
	const sg_rbtree_node* pnode,
	SG_uint16* piDepth,
	SG_uint16 curDepth
	)
{
	if (curDepth > *piDepth)
	{
		*piDepth = curDepth;
	}

	if (pnode->link[0])
	{
		SG_ERR_CHECK(  sg_rbtreenode__calc_depth(pCtx, pnode->link[0], piDepth, 1 + curDepth)  );
	}

	if (pnode->link[1])
	{
		SG_ERR_CHECK(  sg_rbtreenode__calc_depth(pCtx, pnode->link[1], piDepth, 1 + curDepth)  );
	}

fail:
	return;
}

struct _sg_rbtree_trav
{
	sg_rbtree_node *up[128]; /* Stack */
	sg_rbtree_node *it;     /* Current node */
	int top;                 /* Top of stack */
};

static void _sg_rbtree__iterator__first(
	SG_context* pCtx,
	SG_rbtree_iterator *pit,
	const SG_rbtree *tree,
	SG_bool* pbOK,
	const char** ppszKey,
	void** pAssocData
	)
{
	SG_NULLARGCHECK_RETURN(pit);
	SG_NULLARGCHECK_RETURN(pbOK);

	pit->it = tree->root;
	pit->top = 0;

	if ( pit->it != NULL )
	{
		while ( pit->it->link[0] != NULL )
		{
			pit->up[pit->top++] = pit->it;
			pit->it = pit->it->link[0];
		}
	}

	if ( pit->it != NULL )
	{
        if (ppszKey)
        {
		    *ppszKey = pit->it->pszKey;
        }
		if (pAssocData)
		{
			*pAssocData = pit->it->assocData;
		}
		*pbOK = SG_TRUE;
	}
	else
	{
		*pbOK = SG_FALSE;
	}
}

void SG_rbtree__get_only_entry(
	SG_context* pCtx,
	const SG_rbtree *tree,
	const char** ppszKey,
	void** pAssocData
    )
{
    SG_ARGCHECK_RETURN( (tree->count == 1), tree->count);

    if (ppszKey)
    {
        *ppszKey = tree->root->pszKey;
    }

    if (pAssocData)
    {
        *pAssocData = tree->root->assocData;
    }
}

void SG_rbtree__iterator__first(
	SG_context* pCtx,
	SG_rbtree_iterator **ppIterator,
	const SG_rbtree *tree,
	SG_bool* pbOK,
	const char** ppszKey,
	void** pAssocData
	)
{
	SG_rbtree_iterator* pit = NULL;

	SG_NULLARGCHECK_RETURN(tree);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pit)  );

	SG_ERR_CHECK( _sg_rbtree__iterator__first(pCtx, pit, tree, pbOK, ppszKey, pAssocData)  );

    if (ppIterator)
    {
        *ppIterator = pit;
    }
    else
    {
        SG_NULLFREE(pCtx, pit);
    }

	return;

fail:
	SG_NULLFREE(pCtx, pit);
}

void SG_rbtree__iterator__next(
	SG_context* pCtx,
	SG_rbtree_iterator *trav,
	SG_bool* pbOK,
	const char** ppszKey,
	void** pAssocData
	)
{
	SG_NULLARGCHECK_RETURN(trav);
	SG_NULLARGCHECK_RETURN(pbOK);

	if ( trav->it->link[1] != NULL )
	{
		trav->up[trav->top++] = trav->it;
		trav->it = trav->it->link[1];

		while ( trav->it->link[0] != NULL )
		{
			trav->up[trav->top++] = trav->it;
			trav->it = trav->it->link[0];
		}
	}
	else
	{
		sg_rbtree_node *last;

		do
		{
			if ( trav->top == 0 )
			{
				trav->it = NULL;
				break;
			}

			last = trav->it;
			trav->it = trav->up[--trav->top];
		} while ( last == trav->it->link[1] );
	}

	if ( trav->it != NULL )
	{
        if (ppszKey)
        {
		    *ppszKey = trav->it->pszKey;
        }
		if (pAssocData)
		{
			*pAssocData = trav->it->assocData;
		}
		*pbOK = SG_TRUE;
	}
	else
	{
		*pbOK = SG_FALSE;
	}
}

void SG_rbtree__iterator__free(SG_context * pCtx, SG_rbtree_iterator* pit)
{
	if (!pit)
	{
		return;
	}

	SG_NULLFREE(pCtx, pit);
}

void SG_rbtree__foreach(
	SG_context* pCtx,
	const SG_rbtree* prb,
	SG_rbtree_foreach_callback* cb,
	void* ctx
	)
{
	SG_rbtree_iterator trav;
	const char* pszKey = NULL;
	void* assocData = NULL;
	SG_bool b;

	SG_NULLARGCHECK_RETURN( prb );
	SG_NULLARGCHECK_RETURN( cb );

	/* Note that foreach is not allowed to alloc memory from the heap.
	 * We instantiate the iterator on the stack instead.
	 *
	 * REVIEW: Jeff says: Is the above comment still valid?  We're probably
	 *                    breaking that rule all over the place....
	 */

	SG_ERR_CHECK(  _sg_rbtree__iterator__first(pCtx, &trav, prb, &b, &pszKey, &assocData)  );

	while (b)
	{
		SG_ERR_CHECK(  cb(pCtx, pszKey, assocData, ctx)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, &trav, &b, &pszKey, &assocData)  );
	}

fail:
	return;
}

void SG_rbtree__compare__keys_only(SG_context* pCtx, const SG_rbtree* prb1, const SG_rbtree* prb2, SG_bool* pbIdentical, SG_rbtree* prbOnly1, SG_rbtree* prbOnly2, SG_rbtree* prbBoth)
{
	SG_bool bIdentical;
	SG_rbtree_iterator* ptrav1 = NULL;
	const char* pszKey1;
	SG_bool b1;

	SG_rbtree_iterator* ptrav2 = NULL;
	const char* pszKey2;
	SG_bool b2;

	SG_bool bNoOutputLists = (
		!prbOnly1
		&& !prbOnly2
		&& !prbBoth
		);

	// for now, we require that both rbtrees have the same ordering function
	// because we are just iterating on them in tandom.
	SG_ARGCHECK_RETURN(  (prb1->pfnCompare == prb2->pfnCompare),  prb1->pfnCompare  );

	bIdentical = SG_TRUE;

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &ptrav1, prb1, &b1, &pszKey1, NULL)  );
	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &ptrav2, prb2, &b2, &pszKey2, NULL)  );

	while (b1 || b2)
	{
		if (b1 && b2)
		{
			int cmp = (*prb1->pfnCompare)(pszKey1, pszKey2);
			if (0 == cmp)
			{
				if (prbBoth)
				{
					SG_ERR_CHECK(  SG_rbtree__add(pCtx, prbBoth, pszKey1)  );
				}
				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, ptrav1, &b1, &pszKey1, NULL)  );
				SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, ptrav2, &b2, &pszKey2, NULL)  );
			}
			else
			{
				bIdentical = SG_FALSE;
				if (bNoOutputLists)
				{
					if (pbIdentical)
					{
						*pbIdentical = SG_FALSE;
					}
					break;
				}

				if (cmp > 0)
				{

					// 1 is ahead.
					if (prbOnly2)
					{
						SG_ERR_CHECK(  SG_rbtree__add(pCtx, prbOnly2, pszKey2)  );
					}
					SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, ptrav2, &b2, &pszKey2, NULL)  );
				}
				else
				{
					bIdentical = SG_FALSE;

					// 2 is ahead.
					if (prbOnly1)
					{
						SG_ERR_CHECK(  SG_rbtree__add(pCtx, prbOnly1, pszKey1)  );
					}
					SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, ptrav1, &b1, &pszKey1, NULL)  );
				}
			}
		}
		else if (b1)
		{
			if (prbOnly1)
			{
				SG_ERR_CHECK(  SG_rbtree__add(pCtx, prbOnly1, pszKey1)  );
			}
			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, ptrav1, &b1, &pszKey1, NULL)  );
		}
		else
		{
			if (prbOnly2)
			{
				SG_ERR_CHECK(  SG_rbtree__add(pCtx, prbOnly2, pszKey2)  );
			}
			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, ptrav2, &b2, &pszKey2, NULL)  );
		}
	}

	SG_RBTREE_ITERATOR_NULLFREE(pCtx, ptrav1);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, ptrav2);

	if (pbIdentical)
	{
		*pbIdentical = bIdentical;
	}

	return;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, ptrav1);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, ptrav2);
}

void SG_rbtree__to_varray__keys_only(SG_context* pCtx, const SG_rbtree* prb, SG_varray** ppva)
{
	SG_varray* pva = NULL;
	SG_rbtree_iterator* ptrav = NULL;
	const char* pszKey;
	SG_bool b;

	SG_NULLARGCHECK(  prb  );
	SG_NULLARGCHECK(  ppva  );

	SG_ERR_CHECK(  SG_VARRAY__ALLOC(pCtx, &pva)  );

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &ptrav, prb, &b, &pszKey, NULL)  );

	while (b)
	{
		SG_ERR_CHECK(  SG_varray__append__string__sz(pCtx, pva, pszKey)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, ptrav, &b, &pszKey, NULL)  );
	}

	SG_RBTREE_ITERATOR_NULLFREE(pCtx, ptrav);

	*ppva = pva;

	return;

fail:
	SG_VARRAY_NULLFREE(pCtx, pva);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, ptrav);
}

void SG_rbtree__count(
	SG_context* pCtx,
	const SG_rbtree* prb,
	SG_uint32* piCount
	)
{
	SG_NULLARGCHECK_RETURN(prb);

	*piCount = prb->count;
}

void SG_rbtree__depth(
	SG_context* pCtx,
	const SG_rbtree* prb,
	SG_uint16* piDepth
	)
{
	SG_uint16 depth = 0;

	SG_NULLARGCHECK_RETURN(prb);
	SG_ERR_CHECK(  sg_rbtreenode__calc_depth(pCtx, prb->root, &depth, 0)  );

	*piDepth = depth;

fail:
	return;
}

void SG_rbtree__add__from_other_rbtree(
	SG_context* pCtx,
	SG_rbtree* prb,
	const SG_rbtree* prbOther
	)
{
	SG_rbtree_iterator* ptrav = NULL;
	const char* pszKey;
	void* assocData;
	SG_bool b;

	SG_NULLARGCHECK_RETURN( prb );
	SG_NULLARGCHECK_RETURN( prbOther );

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &ptrav, prbOther, &b, &pszKey, &assocData)  );

	while (b)
	{
		SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb, pszKey, assocData)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, ptrav, &b, &pszKey, &assocData)  );
	}

	SG_RBTREE_ITERATOR_NULLFREE(pCtx, ptrav);

	return;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, ptrav);
}

void SG_rbtree__update__from_other_rbtree__keys_only(
	SG_context* pCtx,
	SG_rbtree* prb,
	const SG_rbtree* prbOther
	)
{
	SG_rbtree_iterator* ptrav = NULL;
	const char* pszKey;
	SG_bool b;

	SG_NULLARGCHECK_RETURN( prb );
	SG_NULLARGCHECK_RETURN( prbOther );

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &ptrav, prbOther, &b, &pszKey, NULL)  );

	while (b)
	{
		SG_ERR_CHECK(  SG_rbtree__update(pCtx, prb, pszKey)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, ptrav, &b, &pszKey, NULL)  );
	}

	SG_RBTREE_ITERATOR_NULLFREE(pCtx, ptrav);

	return;

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, ptrav);
}

void SG_rbtree__free__with_assoc(SG_context * pCtx, SG_rbtree * prb, SG_free_callback* cb)
{
	SG_rbtree_iterator* ptrav = NULL;
	const char* pszKey;
	void* assocData;
	SG_bool b;

	if (!prb)
		return;

	if (cb)
	{
		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &ptrav, prb, &b, &pszKey, &assocData)  );

		while (b)
		{
			SG_ERR_IGNORE(  cb(pCtx, assocData)  );

			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, ptrav, &b, &pszKey, &assocData)  );
		}
	}

	// fall thru to common cleanup

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, ptrav);
	SG_RBTREE_NULLFREE(pCtx, prb);
}

void SG_rbtree__write_json_array__keys_only(SG_context* pCtx, const SG_rbtree* prb, SG_jsonwriter* pjson)
{
	SG_rbtree_iterator trav;
	const char* pszKey = NULL;
	SG_bool b;

	SG_ERR_CHECK(  SG_jsonwriter__write_start_array(pCtx, pjson)  );

	SG_ERR_CHECK(  _sg_rbtree__iterator__first(pCtx, &trav, prb, &b, &pszKey, NULL)  );
	while (b)
	{
        SG_ERR_CHECK(  SG_jsonwriter__write_element__string__sz(pCtx, pjson, pszKey)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, &trav, &b, &pszKey, NULL)  );
	}

	SG_ERR_CHECK(  SG_jsonwriter__write_end_array(pCtx, pjson)  );

fail:
	return;
}

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
void SG_rbtree_debug__dump_keys(SG_context* pCtx, const SG_rbtree * prb, SG_string * pStrOut)
{
	SG_rbtree_iterator * pIter = NULL;
	const char * szKey;
	SG_bool b;
	SG_uint32 k;
	SG_string * pStrLine = NULL;

	SG_NULLARGCHECK_RETURN( prb );
	SG_NULLARGCHECK_RETURN( pStrOut );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStrLine)  );

	k = 0;
	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIter,prb,&b,&szKey,NULL)  );
	while (b)
	{
		SG_ERR_IGNORE(  SG_string__sprintf(pCtx, pStrLine,"\t[%d]: %s\n",k,szKey)  );
		SG_ERR_CHECK(  SG_string__append__string(pCtx, pStrOut,pStrLine)  );

		k++;
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIter,&b,&szKey,NULL)  );
	}

	// fall thru to common cleanup

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
	SG_STRING_NULLFREE(pCtx, pStrLine);
}

void SG_rbtree_debug__dump_keys_to_console(SG_context* pCtx, const SG_rbtree * prb)
{
	SG_string * pStrOut = NULL;

	SG_NULLARGCHECK( SG_ERR_INVALIDARG );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStrOut)  );
	SG_ERR_CHECK(  SG_rbtree_debug__dump_keys(pCtx, prb,pStrOut)  );
	SG_ERR_CHECK(  SG_console(pCtx, SG_CS_STDERR,"RBTree:\n%s\n",SG_string__sz(pStrOut))  );

	// fall thru to common cleanup

fail:
	SG_STRING_NULLFREE(pCtx, pStrOut);
}
#endif

//////////////////////////////////////////////////////////////////

SG_rbtree_compare_function_callback SG_rbtree__compare_function__reverse_strcmp;

int SG_rbtree__compare_function__reverse_strcmp(const char * psz1, const char * psz2)
{
	return strcmp(psz2,psz1);
}

