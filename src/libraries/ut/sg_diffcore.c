/**
 * @copyright
 * 
 * Copyright (C) 2003, 2010 SourceGear LLC. All rights reserved.
 * This file contains file-diffing-related source from subversion's
 * libsvn_delta, forked at version 0.20.0, which contained the
 * following copyright notice:
 *
 *      Copyright (c) 2000-2003 CollabNet.  All rights reserved.
 *      
 *      This software is licensed as described in the file COPYING, which
 *      you should have received as part of this distribution.  The terms
 *      are also available at http://subversion.tigris.org/license-1.html.
 *      If newer versions of this license are posted there, you may use a
 *      newer version instead, at your option.
 *      
 *      This software consists of voluntary contributions made by many
 *      individuals.  For exact contribution history, see the revision
 *      history and logs, available at http://subversion.tigris.org/.
 * 
 * @endcopyright
 * 
 * @file sg_diffcore.c
 * 
 * @details
 * 
 */

#include <sg.h>

//////////////////////////////////////////////////////////////////


#define SG_HAS_SET(bits_buffer, which_bits) ((bits_buffer&which_bits)==which_bits)


enum __sg_diff_type
{
    SG_DIFF_TYPE__COMMON        = 0,
    SG_DIFF_TYPE__DIFF_MODIFIED = 1,
    SG_DIFF_TYPE__DIFF_LATEST   = 2,
    SG_DIFF_TYPE__DIFF_COMMON   = 3,
    SG_DIFF_TYPE__CONFLICT      = 4,
    SG_DIFF_TYPES_COUNT
};
typedef enum __sg_diff_type _sg_diff_type;


struct _sg_diff_t // semi-public (outside SG_diffcore treat as "void")
{
    SG_diff_t * pNext;
    _sg_diff_type type;
    SG_int32 start[3]; // Index into this array with an SG_diffcore__doc_ndx enum
    SG_int32 length[3]; // Index into this array with an SG_diffcore__doc_ndx enum
    SG_diff_t * pResolvedDiff;
};


typedef struct __sg_diff__node _sg_diff__node_t;
struct __sg_diff__node
{
    _sg_diff__node_t * pParent;
    _sg_diff__node_t * pLeft;
    _sg_diff__node_t * pRight;

    void * pToken;
};

void _sg_diff__node__nullfree(_sg_diff__node_t ** ppNode)
{
    if(ppNode==NULL||*ppNode==NULL)
        return;
    _sg_diff__node__nullfree(&(*ppNode)->pLeft);
    _sg_diff__node__nullfree(&(*ppNode)->pRight);
    SG_free__no_ctx(*ppNode);
    *ppNode = NULL;
}


struct __sg_diff__tree
{
    _sg_diff__node_t * pRoot;
};
typedef struct __sg_diff__tree _sg_diff__tree_t;


typedef struct __sg_diff__position _sg_diff__position_t;
struct __sg_diff__position
{
    _sg_diff__position_t * pNext;
    _sg_diff__position_t * pPrev; // SourceGear-added
    _sg_diff__node_t * pNode;
    SG_int32 offset;
};


// "Longest Common Subsequence" -- See note at _sg_diff__lcs() function.
typedef struct __sg_diff__lcs_t _sg_diff__lcs_t;
struct __sg_diff__lcs_t
{
    _sg_diff__lcs_t * pNext;
    _sg_diff__position_t * positions[2];
    SG_int32 length;
};


// Manage memory for _sg_diff__position_t and _sg_diff__lcs_t by keeping
// a linked list of all allocated instances.
typedef struct __sg_diff__mempool _sg_diff__mempool;
struct __sg_diff__mempool
{
    union
    {
        _sg_diff__position_t position;
        _sg_diff__lcs_t lcs;
    } p;

    _sg_diff__mempool * pNext;
};

void _sg_diff__mempool__flush(_sg_diff__mempool * pMempool)
{
    if(pMempool==NULL)
        return;

    while(pMempool->pNext!=NULL)
    {
        _sg_diff__mempool * pNext = pMempool->pNext->pNext;
        SG_free__no_ctx(pMempool->pNext);
        pMempool->pNext = pNext;
    }
}

void _sg_diff__position_t__alloc(SG_context * pCtx, _sg_diff__mempool * pMempool, _sg_diff__position_t ** ppNew)
{
    _sg_diff__mempool * pMempoolNode = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pMempool);
    SG_NULLARGCHECK_RETURN(ppNew);

    SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pMempoolNode)  );
    pMempoolNode->pNext = pMempool->pNext;
    pMempool->pNext = pMempoolNode;

    *ppNew = &pMempoolNode->p.position;
}

void _sg_diff__lcs_t__alloc(SG_context * pCtx, _sg_diff__mempool * pMempool, _sg_diff__lcs_t ** ppNew)
{
    _sg_diff__mempool * pMempoolNode = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pMempool);
    SG_NULLARGCHECK_RETURN(ppNew);

    SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pMempoolNode)  );
    pMempoolNode->pNext = pMempool->pNext;
    pMempool->pNext = pMempoolNode;

    *ppNew = &pMempoolNode->p.lcs;
}


struct __sg_diff__snake_t
{
    SG_int32 y;
    _sg_diff__lcs_t * pLcs;
    _sg_diff__position_t * positions[2];
};
typedef struct __sg_diff__snake_t _sg_diff__snake_t;


void _sg_diff__tree_insert_token(
    SG_context * pCtx,
    _sg_diff__tree_t * pTree,
    const SG_diff_functions * pVtable,
    void * pDiffBaton,
    void * pToken,
    _sg_diff__node_t ** ppResult)
{
    _sg_diff__node_t * pParent = NULL;
    _sg_diff__node_t ** ppNode = NULL;
    _sg_diff__node_t * pNewNode = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pTree);
    SG_NULLARGCHECK_RETURN(pVtable);
    SG_NULLARGCHECK_RETURN(ppResult);

    ppNode = &pTree->pRoot;
    while(*ppNode!=NULL)
    {
        SG_int32 rv;
        rv = pVtable->token_compare(pDiffBaton, (*ppNode)->pToken, pToken);
        if (rv == 0)
        {
            // Discard the token
            if (pVtable->token_discard != NULL)
                pVtable->token_discard(pDiffBaton, pToken);

            *ppResult = *ppNode;
            return;
        }
        else if (rv > 0)
        {
            pParent = *ppNode;
            ppNode = &pParent->pLeft;
        }
        else
        {
            pParent = *ppNode;
            ppNode = &pParent->pRight;
        }
    }

    // Create a new node
    {
        SG_ERR_CHECK(  SG_alloc1(pCtx, pNewNode)  );
        pNewNode->pParent = pParent;
        pNewNode->pLeft = NULL;
        pNewNode->pRight = NULL;
        pNewNode->pToken = pToken;

        *ppNode = pNewNode;
        *ppResult = pNewNode;
    }

    return;
fail:
    ;
}

void _sg_diff__get_tokens(
    SG_context * pCtx,
    _sg_diff__mempool * pMempool,
    _sg_diff__tree_t * pTree,
    const SG_diff_functions * pVtable,
    void * pDiffBaton,
    SG_diff_datasource datasource,
    _sg_diff__position_t ** ppPositionList)
{
    _sg_diff__position_t * pStartPosition = NULL;
    _sg_diff__position_t * pPosition = NULL;
    _sg_diff__position_t ** ppPosition;
    _sg_diff__position_t * pPrev = NULL;
    _sg_diff__node_t * pNode;
    void * pToken;
    SG_int32 offset;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(ppPositionList);

    SG_ERR_CHECK(  pVtable->datasource_open(pCtx, pDiffBaton, datasource)  );

    ppPosition = &pStartPosition;
    offset = 0;
    while (1)
    {
        SG_ERR_CHECK(  pVtable->datasource_get_next_token(pCtx, pDiffBaton, datasource, &pToken)  );
        if (pToken == NULL)
            break;

        offset++;
        SG_ERR_CHECK(  _sg_diff__tree_insert_token(pCtx, pTree, pVtable, pDiffBaton, pToken, &pNode)  );

        // Create a new position
        SG_ERR_CHECK(  _sg_diff__position_t__alloc(pCtx, pMempool, &pPosition)  );
        pPosition->pNext = NULL;
        pPosition->pNode = pNode;
        pPosition->offset = offset;
        pPosition->pPrev = pPrev;
        pPrev = pPosition;
  
        *ppPosition = pPosition;
        ppPosition = &pPosition->pNext;
    }

    *ppPosition = pStartPosition;

    if (pStartPosition)
        pStartPosition->pPrev = pPosition;

    SG_ERR_CHECK(  pVtable->datasource_close(pCtx, pDiffBaton, datasource)  );

    *ppPositionList = pPosition;

    // NOTE: for some strange reason we return the postion_list
    // NOTE: pointing to the last position in the datasource
    // NOTE: (so pl->pNext gives the beginning of the datasource).

    return;
fail:
    ;
}

void _sg_diff__snake(SG_context * pCtx, _sg_diff__mempool *pMempool, SG_int32 k, _sg_diff__snake_t *fp, int idx)
{
    _sg_diff__position_t * startPositions[2] = {NULL, NULL};
    _sg_diff__position_t * positions[2] = {NULL, NULL};
    _sg_diff__lcs_t * pLcs = NULL;
    _sg_diff__lcs_t * pPreviousLcs = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(fp);

    if (fp[k - 1].y + 1 > fp[k + 1].y)
    {
        startPositions[0] = fp[k - 1].positions[0];
        startPositions[1] = fp[k - 1].positions[1]->pNext;

        pPreviousLcs = fp[k - 1].pLcs;
    }
    else
    {
        startPositions[0] = fp[k + 1].positions[0]->pNext;
        startPositions[1] = fp[k + 1].positions[1];

        pPreviousLcs = fp[k + 1].pLcs;
    }

    // ### Optimization, skip all positions that don't have matchpoints
    // ### anyway. Beware of the sentinel, don't skip it!

    positions[0] = startPositions[0];
    positions[1] = startPositions[1];

    while (positions[0]->pNode == positions[1]->pNode)
    {
        positions[0] = positions[0]->pNext;
        positions[1] = positions[1]->pNext;
    }

    if (positions[1] != startPositions[1])
    {
        SG_ERR_CHECK(  _sg_diff__lcs_t__alloc(pCtx, pMempool, &pLcs)  );

        pLcs->positions[idx] = startPositions[0];
        pLcs->positions[1-idx] = startPositions[1];
        pLcs->length = positions[1]->offset - startPositions[1]->offset;

        pLcs->pNext = pPreviousLcs;
        fp[k].pLcs = pLcs;
    }
    else
    {
        fp[k].pLcs = pPreviousLcs;
    }

    fp[k].positions[0] = positions[0];
    fp[k].positions[1] = positions[1];

    fp[k].y = positions[1]->offset;

    return;
fail:
    ;
}

_sg_diff__lcs_t * _sg_diff__lcs_reverse(_sg_diff__lcs_t * pLcs)
{
    _sg_diff__lcs_t * pIter = pLcs;
    _sg_diff__lcs_t * pIterPrev = NULL;
    while(pIter!=NULL)
    {
        _sg_diff__lcs_t * pIterNext = pIter->pNext;
        pIter->pNext = pIterPrev;

        pIterPrev = pIter;
        pIter = pIterNext;
    }
    pLcs = pIterPrev;

    return pLcs;
}

//////////////////////////////////////////////////////////////////
// _sg_diff__lcs_juggle() -- juggle the lcs list to eliminate
// minor items.  the lcs algorithm is greedy in that it will
// accept a one line match as a "common" sequence and then create
// another item for the next portion of a large change (a line
// of code containing only a '{' or '}', for example).  if the
// gap between two "common" portions ends with the same text as
// is in the first "common" portion, it is sort of arbitrary
// whether we use the first "common" portion or the one in the
// gap.  but by selecting the one at the end of the gap, we can
// coalesce the item with the following one (and append the gap
// onto the prior node).
SG_bool _sg_diff__lcs_juggle(_sg_diff__lcs_t * pLcsHead)
{
    _sg_diff__lcs_t * pLcs = pLcsHead;
    SG_bool bDidJuggling = SG_FALSE;
    while (pLcs && pLcs->pNext)
    {
        SG_int32 lenGap0;
        SG_int32 lenGap1;
        SG_int32 ndxGap;
        SG_int32 k;

        _sg_diff__lcs_t * pLcsNext = pLcs->pNext;

        _sg_diff__position_t * pGapTail = NULL;
        _sg_diff__position_t * pBegin = NULL;
        _sg_diff__position_t * pEnd = NULL;

        // if the next lcs is length zero, we are on the last one and can't do anything.
        if (pLcsNext->length == 0)
            goto NoShift;

        // compute the gap between this sequence and the next one.
        lenGap0 = (pLcsNext->positions[0]->offset - pLcs->positions[0]->offset - pLcs->length);
        lenGap1 = (pLcsNext->positions[1]->offset - pLcs->positions[1]->offset - pLcs->length);

        // if both gaps are zero we're probably at the end.
        // whether we are or not, we don't have anything to do.
        if ((lenGap0==0) && (lenGap1==0))
            goto NoShift;

        // if both are > 0, this is not a minor item and
        // we shouldn't mess with it.
        if ((lenGap0>0) && (lenGap1>0))
            goto NoShift;

        // compute the index (0 or 1) of the one which has
        // the gap past the end of the common portion.  the
        // other is contiguous with the next lcs.
        ndxGap = (lenGap1 > 0);

        // get the end of the gap and back up the length of common part.
        for (pGapTail=pLcsNext->positions[ndxGap],k=0; (k<pLcs->length); pGapTail=pGapTail->pPrev, k++)
        {
        }

        // see if the common portion (currently prior to
        // the gap) is exactly repeated at the end of the
        // gap.
        for (k=0, pBegin=pLcs->positions[ndxGap], pEnd=pGapTail; k < pLcs->length; k++, pBegin=pBegin->pNext, pEnd=pEnd->pNext)
        {
            if (pBegin->pNode != pEnd->pNode)
                goto NoShift;
        }
        
        // we can shift the common portion to the end of the gap
        // and push the first common portion and the remainder
        // of the gap onto the tail of the previous lcs.
        pLcs->positions[ndxGap] = pGapTail;

        // now, we have 2 contiguous lcs's (pLcs and pLcsNext) that
        // are contiguous on both parts.  so pLcsNext is redundant.
        // we just add the length of pLcsNext to pLcs and remove
        // pLcsNext from the list.
        //
        // but wait, the last thing in the lcs list is a "sentinal",
        // a zero length lcs item that we can't delete.

        if (pLcsNext->length)
        {
            pLcs->length += pLcsNext->length;
            pLcs->pNext = pLcsNext->pNext;
            bDidJuggling = SG_TRUE;
        }
        
        // let's go back and try this again on this lcs and see
        // if it has overlap with the next one.
        continue;

    NoShift:
        pLcs=pLcs->pNext;
    }

    return bDidJuggling;
}

// Calculate the Longest Common Subsequence between two datasources.
// This function is what makes the diff code tick.
// 
// The LCS algorithm implemented here is described by Sun Wu,
// Udi Manber and Gene Meyers in "An O(NP) Sequence Comparison Algorithm"
// 
// The "Juggle" routine is original to the SourceGear version.
// 
// NOTE: Use position lists and tree to compute LCS using graph
// NOTE: algorithm described in the paper.
// NOTE:
// NOTE: This produces a "LCS list".  Where each "LCS" represents
// NOTE: a length and an offset in source-a and source-c where they
// NOTE: are identical.
// NOTE:
// NOTE: "Gaps" (in my terminology) are implicit between LCS's and
// NOTE: represent places where they are different.  A Gap is said
// NOTE: to exist when the offset in the next LCS is greater than
// NOTE: the offset in this LCS plus the length for a particular
// NOTE: source.
// NOTE:
// NOTE: From the LCS list, they compute the "diff list" which
// NOTE: represents the changes (and is basically what we see as the
// NOTE: output of the "diff" command).
void _sg_diff__lcs(
    SG_context * pCtx,
    _sg_diff__mempool * pMempool,
    _sg_diff__position_t * pPositionList1,
    _sg_diff__position_t * pPositionList2,
    _sg_diff__lcs_t ** ppResult)
{
    int idx;
    SG_int32 length[2];
    _sg_diff__snake_t *fp = NULL;
    _sg_diff__snake_t *fp0 = NULL;
    SG_int32 d;
    SG_int32 p = 0;
    _sg_diff__lcs_t * pLcs = NULL;

    _sg_diff__position_t sentinelPositions[2];
    _sg_diff__node_t     sentinelNodes[2];

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(ppResult);

    // Since EOF is always a sync point we tack on an EOF link with sentinel positions
    SG_ERR_CHECK(  _sg_diff__lcs_t__alloc(pCtx, pMempool, &pLcs)  );
    SG_ERR_CHECK(  _sg_diff__position_t__alloc(pCtx, pMempool, &pLcs->positions[0])  );
    pLcs->positions[0]->offset = pPositionList1 ? pPositionList1->offset + 1 : 1;
    SG_ERR_CHECK(  _sg_diff__position_t__alloc(pCtx, pMempool, &pLcs->positions[1])  );
    pLcs->positions[1]->offset = pPositionList2 ? pPositionList2->offset + 1 : 1;
    pLcs->length = 0;
    pLcs->pNext = NULL;

    if (pPositionList1 == NULL || pPositionList2 == NULL)
    {
        *ppResult = pLcs;
        return;
    }

    // Calculate length of both sequences to be compared
    length[0] = pPositionList1->offset - pPositionList1->pNext->offset + 1;
    length[1] = pPositionList2->offset - pPositionList2->pNext->offset + 1;
    idx = length[0] > length[1] ? 1 : 0;

    SG_ERR_CHECK(  SG_allocN(pCtx, (length[0]+length[1]+3), fp)  );
    memset(fp, 0, sizeof(*fp)*(length[0]+length[1]+3));
    fp0 = fp;
    fp += length[idx] + 1;

    sentinelPositions[idx].pNext = pPositionList1->pNext;
    pPositionList1->pNext = &sentinelPositions[idx];
    sentinelPositions[idx].offset = pPositionList1->offset + 1;

    sentinelPositions[1-idx].pNext = pPositionList2->pNext;
    pPositionList2->pNext = &sentinelPositions[1-idx];
    sentinelPositions[1-idx].offset = pPositionList2->offset + 1;

    sentinelPositions[0].pNode = &sentinelNodes[0];
    sentinelPositions[1].pNode = &sentinelNodes[1];

    d = length[1-idx] - length[idx];

    // k = -1 will be the first to be used to get previous position
    // information from, make sure it holds sane data
    fp[-1].positions[0] = sentinelPositions[0].pNext;
    fp[-1].positions[1] = &sentinelPositions[1];

    p = 0;
    do
    {
        SG_int32 k;

        // Forward
        for (k = -p; k < d; k++)
            SG_ERR_CHECK(  _sg_diff__snake(pCtx, pMempool, k, fp, idx)  );

        for (k = d + p; k >= d; k--)
            SG_ERR_CHECK(  _sg_diff__snake(pCtx, pMempool, k, fp, idx)  );

        p++;
    }
    while (fp[d].positions[1] != &sentinelPositions[1]);

    pLcs->pNext = fp[d].pLcs;
    pLcs = _sg_diff__lcs_reverse(pLcs);

    pPositionList1->pNext = sentinelPositions[idx].pNext;
    pPositionList2->pNext = sentinelPositions[1-idx].pNext;

    // run our juggle algorithm on the LCS list to try to
    // eliminate minor annoyances -- this has the effect of
    // joining large chunks that were split by a one line
    // change -- where it is arbitrary whether the one line
    // is handled before or after the second large chunk.
    //
    // we let this run until it reaches (a kind of transitive)
    // closure.  this is required because the concatenation
    // tends to cascade backwards -- that is, the tail of a
    // large change get joined to the prior change and then that
    // gets joined to the prior change (see filediffs/d176_*).
    //
    // TODO figure out how to do the juggle in one pass.
    // it's possible that we could do this in one trip if we
    // had back-pointers in the lcs list or if we tried it
    // before the list is (un)reversed.  but i'll save that
    // for a later experiment.  it generally only runs once,
    // but i've seen it go 4 times for a large block (with
    // several blank or '{' lines) (on d176).  it is linear
    // in the number of lcs'es and since we now have back
    // pointers on the position list, it's probably not worth
    // the effort (or the amount of text in this comment :-).

    while(_sg_diff__lcs_juggle(pLcs))
    {
    }

    SG_NULLFREE(pCtx, fp0);

    *ppResult = pLcs;

    return;
fail:
    SG_NULLFREE(pCtx, fp0);
}

void _sg_diff__diff(SG_context * pCtx, _sg_diff__lcs_t *pLcs, SG_int32 originalStart, SG_int32 modifiedStart, SG_bool want_common, SG_diff_t ** ppResult)
{
    // Morph a _sg_lcs_t into a _sg_diff_t.
    SG_diff_t * pResult = NULL;
    SG_diff_t ** ppNext = &pResult;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(ppResult);

    while (1)
    {
        if( (originalStart < pLcs->positions[0]->offset) || (modifiedStart < pLcs->positions[1]->offset) )
        {
            SG_ERR_CHECK(  SG_alloc1(pCtx, *ppNext)  );

            (*ppNext)->type = SG_DIFF_TYPE__DIFF_MODIFIED;
            (*ppNext)->start[SG_DIFFCORE__DOC_NDX_ORIGINAL] = originalStart - 1;
            (*ppNext)->length[SG_DIFFCORE__DOC_NDX_ORIGINAL] = pLcs->positions[0]->offset - originalStart;
            (*ppNext)->start[SG_DIFFCORE__DOC_NDX_MODIFIED] = modifiedStart - 1;
            (*ppNext)->length[SG_DIFFCORE__DOC_NDX_MODIFIED] = pLcs->positions[1]->offset - modifiedStart;
            (*ppNext)->start[SG_DIFFCORE__DOC_NDX_LATEST] = 0;
            (*ppNext)->length[SG_DIFFCORE__DOC_NDX_LATEST] = 0;

            ppNext = &(*ppNext)->pNext;
        }

        // Detect the EOF
        if (pLcs->length == 0)
            break;

        originalStart = pLcs->positions[0]->offset;
        modifiedStart = pLcs->positions[1]->offset;

        if (want_common)
        {
            SG_ERR_CHECK(  SG_alloc1(pCtx, *ppNext)  );

            (*ppNext)->type = SG_DIFF_TYPE__COMMON;
            (*ppNext)->start[SG_DIFFCORE__DOC_NDX_ORIGINAL] = originalStart - 1;
            (*ppNext)->length[SG_DIFFCORE__DOC_NDX_ORIGINAL] = pLcs->length;
            (*ppNext)->start[SG_DIFFCORE__DOC_NDX_MODIFIED] = modifiedStart - 1;
            (*ppNext)->length[SG_DIFFCORE__DOC_NDX_MODIFIED] = pLcs->length;
            (*ppNext)->start[SG_DIFFCORE__DOC_NDX_LATEST] = 0;
            (*ppNext)->length[SG_DIFFCORE__DOC_NDX_LATEST] = 0;

            ppNext = &(*ppNext)->pNext;
        }

        originalStart += pLcs->length;
        modifiedStart += pLcs->length;

        pLcs = pLcs->pNext;
    }

    *ppNext = NULL;

    *ppResult = pResult;

    return;
fail:
    ;
}


static char * my_memchr2_byte(char * pBuf, const char * pattern, SG_uint32 bufSize)
{
    // do a 1- or 2- byte memchr() (like strstr() but with an upperbound)
    char * pEnd = pBuf+bufSize;
    while(bufSize>0)
    {
        char * p = memchr(pBuf,pattern[0],bufSize);
        if(p==NULL)
            return p;
        if(pattern[1]=='\0') // 1 byte pattern, just use memchr()
            return p;
        if(p[1]==pattern[1]) // 2 byte pattern, and a match
            return p;

        pBuf = p+1;
        bufSize = pEnd - pBuf;
    }
    return NULL;
}
static char * my_anyeol_byte(char * pBuf, SG_uint32 bufSize)
{
    // find the first EOL marker using either CR, LF, or CRLF
    char * pCR = NULL;
    char * pLF = NULL;

    pCR = memchr(pBuf,SG_CR,bufSize);
    pLF = memchr(pBuf,SG_LF,bufSize);

    if(pCR==NULL && pLF==NULL) // no EOL marker of any kind found
        return NULL;
    else if(pCR!=NULL && pLF!=NULL) // both set (probably CRLF)
        return SG_MIN(pCR,pLF);
    else // only one set
        return SG_MAX(pCR,pLF);
}













#define SG_DIFF__UNIFIED_CONTEXT_SIZE 3


struct __sg_diff__file_output_baton
{
    SG_file * pOutputFile;

    const SG_pathname * path[2];
    SG_file * file[2];

    SG_int32 current_line[2];

    SG_bool previous_chunk_ended_in_CR[2];
    char buffer[2][4096];
    SG_uint32 length[2];
    char * curp[2];

    SG_int32 hunkStart[2]; // Use 32 bit, not 64. This is essentially a line number, not an offset in bytes into the file.
    SG_int32 hunkLength[2];
    SG_string * pHunk;

    SG_diff_options options;
    int context; // number of lines of context, when appropriate
};
typedef struct __sg_diff__file_output_baton _sg_diff__file_output_baton_t;


enum __sg_diff__file_output_unified_type
{
    SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__SKIP,
    SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__CONTEXT,
    SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__DELETE,
    SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__INSERT
};
typedef enum __sg_diff__file_output_unified_type _sg_diff__file_output_unified_type;


static void _sg_diff__file_output_unified_line(SG_context * pCtx, _sg_diff__file_output_baton_t *pBaton, _sg_diff__file_output_unified_type type, int idx)
{
    char * pCurp;
    SG_uint32 length;
    SG_bool bytes_processed = SG_FALSE;
    SG_bool bAnyEOL;

    length = pBaton->length[idx];
    pCurp = pBaton->curp[idx];

    // Lazily update the current line even if we're at EOF.
    // This way we fake output of context at EOF
    pBaton->current_line[idx]++;

    if(pBaton->file[idx]==NULL)
        return;

    {
        SG_bool eof = SG_FALSE;
        SG_ERR_CHECK(  SG_file__eof(pCtx, pBaton->file[idx], &eof)  );
        if( (length==0) && (eof) )
            return;
    }

    bAnyEOL = SG_HAS_SET(pBaton->options,SG_DIFF_OPTION__ANY_EOL);

    do
    {
        if (length > 0)
        {
            char * pEol = NULL;
            SG_uint32 eolLen = 0;

            if (!bytes_processed)
            {
                switch (type)
                {
                    case SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__CONTEXT:
                        SG_ERR_CHECK(  SG_string__append__sz(pCtx, pBaton->pHunk, " ")  );
                        pBaton->hunkLength[0]++;
                        pBaton->hunkLength[1]++;
                        break;
                    case SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__DELETE:
                        SG_ERR_CHECK(  SG_string__append__sz(pCtx, pBaton->pHunk, "-")  );
                        pBaton->hunkLength[0]++;
                        break;
                    case SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__INSERT:
                        SG_ERR_CHECK(  SG_string__append__sz(pCtx, pBaton->pHunk, "+")  );
                        pBaton->hunkLength[1]++;
                        break;
                    default:
                        break;
                }
            }

            if (bAnyEOL) // we accept any EOL style
            {
                pEol = my_anyeol_byte(pCurp,length);
                if(pEol!=NULL)
                {
                    if(*pEol==SG_CR && ((SG_uint32)(pEol-pCurp)<length-1) && *(pEol+1)==SG_LF)
                        eolLen = 2;
                    else
                        eolLen = 1;
                }
            }
            else // we accept only the platform-native EOL style
            {
                pEol = my_memchr2_byte(pCurp,SG_PLATFORM_NATIVE_EOL_STR,length);

                if(pEol==NULL && SG_PLATFORM_NATIVE_EOL_IS_CRLF)
                {
                    // We are reading in the file in 4K chunks. Make sure a CRLF didn't get split across a chunk boundary.

                    pEol = my_memchr2_byte(pCurp,SG_CR_STR,length);
                    if(pEol!=NULL && (SG_uint32)(pEol-pCurp)==length-1)
                    {
                        // Found the CR at the end of this chunk.
                        // For complete correctness we would peek ahead in the file and make sure an LF came next... but why bother...

                        eolLen = 1;
                    }
                    else
                    {
                        pEol = NULL;
                    }
                }

                if(pEol!=NULL && eolLen==0)
                {
                    if(SG_PLATFORM_NATIVE_EOL_IS_CRLF)
                        eolLen = 2;
                    else
                        eolLen = 1;
                }
            }

            if(pEol!=NULL)
            {
                SG_uint32 len;

                pEol += eolLen;
                len = (SG_uint32)(pEol - pCurp);
                length -= len;

                if(length==0 && eolLen==1 && pEol[-1]==SG_CR)
                    pBaton->previous_chunk_ended_in_CR[idx] = SG_TRUE;

                if (type != SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__SKIP)
                {
                    SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pBaton->pHunk, (const SG_byte *)pCurp, len-eolLen)  );
                    SG_ERR_CHECK(  SG_string__append__sz(pCtx, pBaton->pHunk, SG_PLATFORM_NATIVE_EOL_STR)  );
                }

                pBaton->curp[idx] = pEol;
                pBaton->length[idx] = length;

                break;
            }

            if (type != SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__SKIP)
                SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pBaton->pHunk, (const SG_byte *)pCurp, length)  );

            bytes_processed = SG_TRUE;
        }

        pCurp = pBaton->buffer[idx];
        length = sizeof(pBaton->buffer[idx]);

        SG_file__read(pCtx, pBaton->file[idx], length, (SG_byte *)pCurp, &length);
        if(pBaton->previous_chunk_ended_in_CR[idx])
        {
            if(!SG_context__has_err(pCtx) && *pCurp==SG_LF)
            {
                ++pCurp;
                --length;
            }
            pBaton->previous_chunk_ended_in_CR[idx] = SG_FALSE;
        }
    }
    while(!SG_context__has_err(pCtx));

    if(SG_context__err_equals(pCtx, SG_ERR_EOF))
    {
        SG_context__err_reset(pCtx);

        // Special case if we reach the end of file AND the last line is in the
        // changed range AND the file doesn't end with a newline
        if (bytes_processed && (type == SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__DELETE || type == SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__INSERT))
            SG_ERR_CHECK(  SG_string__append__sz(pCtx, pBaton->pHunk, SG_PLATFORM_NATIVE_EOL_STR "\\ No newline at end of file" SG_PLATFORM_NATIVE_EOL_STR)  );

        pBaton->length[idx] = 0;
    }
    else if(SG_context__has_err(pCtx))
        SG_ERR_RETHROW2(  (pCtx, "error reading from '%s'.", pBaton->path[idx])  );

    return;
fail:
    ;
}

static void _sg_diff__file_output_unified_flush_hunk(SG_context * pCtx, _sg_diff__file_output_baton_t *pBaton)
{
    SG_int32 target_line;
    int i;

    SG_ARGCHECK_RETURN(pBaton->pHunk!=NULL, pBaton);

    if (SG_string__length_in_bytes(pBaton->pHunk)==0)
        return; // Nothing to flush

    target_line = pBaton->hunkStart[0] + pBaton->hunkLength[0] + pBaton->context;

    // Add trailing context to the hunk
    while (pBaton->current_line[0] < target_line)
        SG_ERR_CHECK(  _sg_diff__file_output_unified_line(pCtx, pBaton, SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__CONTEXT, 0));

    // If the file is non-empty, convert the line indexes from zero based to one based
    for (i = 0; i < 2; i++)
        if (pBaton->hunkLength[i] > 0)
            pBaton->hunkStart[i]++;

    // Output the hunk header.  If the hunk length is 1, the file is a one line
    // file.  In this case, surpress the number of lines in the hunk (it is
    // 1 implicitly)
    SG_ERR_CHECK(  SG_file__write__format(pCtx, pBaton->pOutputFile, "@@ -%d", pBaton->hunkStart[0])  );
    if (pBaton->hunkLength[0] != 1)
        SG_ERR_CHECK(  SG_file__write__format(pCtx, pBaton->pOutputFile, ",%d", pBaton->hunkLength[0])  );

    SG_ERR_CHECK(  SG_file__write__format(pCtx, pBaton->pOutputFile, " +%d", pBaton->hunkStart[1])  );
    if (pBaton->hunkLength[1] != 1)
        SG_ERR_CHECK(  SG_file__write__format(pCtx, pBaton->pOutputFile, ",%d", pBaton->hunkLength[1])  );

    SG_ERR_CHECK(  SG_file__write__sz(pCtx, pBaton->pOutputFile, " @@" SG_PLATFORM_NATIVE_EOL_STR)  );

    // Output the hunk content
    SG_ERR_CHECK(  SG_file__write__string(pCtx, pBaton->pOutputFile, pBaton->pHunk)  );

    // Prepare for the next hunk
    pBaton->hunkLength[0] = 0;
    pBaton->hunkLength[1] = 0;
    SG_ERR_CHECK(  SG_string__clear(pCtx, pBaton->pHunk)  );

    return;
fail:
    ;
}

static void _sg_diff__file_output_unified_default_hdr(SG_context * pCtx, const SG_pathname * pPath, char * pBuffer, SG_uint32 bufLen)
{
    SG_fsobj_stat file_info;
    SG_time time;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pBuffer);
    SG_ARGCHECK_RETURN(bufLen>0, bufLen);

    if(pPath!=NULL)
    {
        SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath, &file_info)  );
        SG_ERR_CHECK(  SG_time__decode__local(pCtx, file_info.mtime_ms, &time)  );

        SG_ERR_CHECK(  SG_sprintf(pCtx, pBuffer, bufLen, "%s\t%d-%02d-%02d %02d:%02d:%02d %+03d%02d",
            SG_pathname__sz(pPath),
            time.year, time.month, time.mday,
            time.hour, time.min, time.sec,
            time.offsetUTC/3600,abs(time.offsetUTC)/60%60)  );
    }
    else
    {
        *pBuffer = '\0';
    }

    return;
fail:
    ;
}


static void _sg_diff__file_output_unified_diff_modified(SG_context * pCtx,
                                                        void *pOutputBaton,
                                                        SG_int32 originalStart, SG_int32 originalLength,
                                                        SG_int32 modifiedStart, SG_int32 modifiedLength,
                                                        SG_int32 latestStart, SG_int32 latestLength)
{
    _sg_diff__file_output_baton_t *pFileOutputBaton = pOutputBaton;
    SG_int32 target_line[2];
    int i;

    SG_UNUSED(latestStart);
    SG_UNUSED(latestLength);

    target_line[0] = originalStart >= pFileOutputBaton->context
    ? originalStart - pFileOutputBaton->context : 0;
    target_line[1] = modifiedStart;
    
    // If the changed ranges are far enough apart (no overlapping or connecting
    // context), flush the current hunk, initialize the next hunk and skip the
    // lines not in context.  Also do this when this is the first hunk.
    if (pFileOutputBaton->current_line[0] < target_line[0]
        && (pFileOutputBaton->hunkStart[0] + pFileOutputBaton->hunkLength[0]
            + pFileOutputBaton->context < target_line[0]
            || pFileOutputBaton->hunkLength[0] == 0))
    {
        SG_ERR_CHECK(  _sg_diff__file_output_unified_flush_hunk(pCtx, pFileOutputBaton));
        
        pFileOutputBaton->hunkStart[0] = target_line[0];
        pFileOutputBaton->hunkStart[1] = target_line[1] + target_line[0] 
        - originalStart;
        
        // Skip lines until we are at the beginning of the context we want to display
        while (pFileOutputBaton->current_line[0] < target_line[0])
        {
            SG_ERR_CHECK(  _sg_diff__file_output_unified_line(pCtx, pFileOutputBaton,
                                                       SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__SKIP, 0));
        }
    }
    
    // Skip lines until we are at the start of the changed range
    while (pFileOutputBaton->current_line[1] < target_line[1])
    {
        SG_ERR_CHECK(  _sg_diff__file_output_unified_line(pCtx, pFileOutputBaton,
                                                   SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__SKIP, 1));
    }
    
    // Output the context preceding the changed range
    while (pFileOutputBaton->current_line[0] < originalStart)
    {
        SG_ERR_CHECK(  _sg_diff__file_output_unified_line(pCtx, pFileOutputBaton, 
                                                   SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__CONTEXT, 0));
    }
    
    target_line[0] = originalStart + originalLength;
    target_line[1] = modifiedStart + modifiedLength;
    
    // Output the changed range
    for (i = 0; i < 2; i++)
    {
        while (pFileOutputBaton->current_line[i] < target_line[i])
        {
            SG_ERR_CHECK(  _sg_diff__file_output_unified_line(pCtx, pFileOutputBaton, i == 0 
                                                       ? SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__DELETE
                                                       : SG_DIFF__FILE_OUTPUT_UNIFIED_TYPE__INSERT, i));
        }
    }
    
    return;
fail:
    ;
}















struct __sg_diff3__file_output_baton_t
{
    const SG_pathname * path[3];
    SG_file * pOutputFile;

    SG_int32 currentLine[3];

    char * buffer[3];
    char * endp[3];
    char * curp[3];

    const char * szConflictOriginal;
    const char * szConflictModified;
    const char * szConflictLatest;
    const char * szConflictSeparator;

    SG_bool displayOriginalInConflict;
    SG_bool displayResolvedConflicts;

    SG_string * encoding[3];
};
typedef struct __sg_diff3__file_output_baton_t _sg_diff3__file_output_baton_t;

enum __sg_diff3_file_output_type
{
    SG_SG_DIFF3_FILE_OUTPUT_TYPE__SKIP,
    SG_SG_DIFF3_FILE_OUTPUT_TYPE__NORMAL,
};
typedef enum __sg_diff3_file_output_type _sg_diff3_file_output_type;

static void _sg_diff3__file_output_line(SG_context * pCtx, _sg_diff3__file_output_baton_t * pOutputBaton, _sg_diff3_file_output_type type, SG_diffcore__doc_ndx idx)
{
    char * pCurp = NULL;
    char * pEndp = NULL;
    char * pEol = NULL;

    pCurp = pOutputBaton->curp[idx];
    pEndp = pOutputBaton->endp[idx];

    // Lazily update the current line even if we're at EOF.
    pOutputBaton->currentLine[idx]++;

    if(pCurp==pEndp)
        return;

    pEol = memchr(pCurp, '\n', pEndp - pCurp);
    if(pEol==NULL)
        pEol=pEndp;
    else
        pEol++;

    if(type!=SG_SG_DIFF3_FILE_OUTPUT_TYPE__SKIP)
        SG_ERR_CHECK(  SG_file__write(pCtx, pOutputBaton->pOutputFile, pEol-pCurp, (const SG_byte *)pCurp, NULL)  );

    pOutputBaton->curp[idx] = pEol;

    return;
fail:
    ;
}

static void _sg_diff3__file_output_hunk(SG_context * pCtx, void * pOutputBaton, SG_diffcore__doc_ndx idx, SG_int32 targetLine, SG_int32 targetLength)
{
    _sg_diff3__file_output_baton_t * pFileOutputBaton = pOutputBaton;

    // Skip lines until we are at the start of the changed range.
    while(pFileOutputBaton->currentLine[idx] < targetLine)
        SG_ERR_CHECK(  _sg_diff3__file_output_line(pCtx, pFileOutputBaton, SG_SG_DIFF3_FILE_OUTPUT_TYPE__SKIP, idx)  );

    targetLine += targetLength;

    while (pFileOutputBaton->currentLine[idx] < targetLine)
        SG_ERR_CHECK(  _sg_diff3__file_output_line(pCtx, pFileOutputBaton, SG_SG_DIFF3_FILE_OUTPUT_TYPE__NORMAL, idx)  );

    return;
fail:
    ;
}

static void _sg_diff3__file_output_common(SG_context * pCtx, void * pOutputBaton, SG_int32 originalStart, SG_int32 originalLength, SG_int32 modifiedStart, SG_int32 modifiedLength, SG_int32 latestStart, SG_int32 latestLength)
{
    SG_UNUSED(modifiedStart);
    SG_UNUSED(modifiedLength);
    SG_UNUSED(latestStart);
    SG_UNUSED(latestLength);
    SG_ERR_CHECK_RETURN(  _sg_diff3__file_output_hunk(pCtx, pOutputBaton, 0, originalStart, originalLength)  );
}

static void _sg_diff3__file_output_diff_modified(SG_context * pCtx, void * pOutputBaton, SG_int32 originalStart, SG_int32 originalLength, SG_int32 modifiedStart, SG_int32 modifiedLength, SG_int32 latestStart, SG_int32 latestLength)
{
    SG_UNUSED(originalStart);
    SG_UNUSED(originalLength);
    SG_UNUSED(latestStart);
    SG_UNUSED(latestLength);
    SG_ERR_CHECK_RETURN(  _sg_diff3__file_output_hunk(pCtx, pOutputBaton, 1, modifiedStart, modifiedLength)  );
}

static void _sg_diff3__file_output_diff_latest(SG_context * pCtx, void * pOutputBaton, SG_int32 originalStart, SG_int32 originalLength, SG_int32 modifiedStart, SG_int32 modifiedLength, SG_int32 latestStart, SG_int32 latestLength)
{
    SG_UNUSED(originalStart);
    SG_UNUSED(originalLength);
    SG_UNUSED(modifiedStart);
    SG_UNUSED(modifiedLength);
    SG_ERR_CHECK_RETURN(  _sg_diff3__file_output_hunk(pCtx, pOutputBaton, 2, latestStart, latestLength)  );
}

static void _sg_diff3__file_output_conflict(
    SG_context * pCtx,
    void * pOutputBaton,
    SG_int32 originalStart, SG_int32 originalLength,
    SG_int32 modifiedStart, SG_int32 modifiedLength,
    SG_int32 latestStart, SG_int32 latestLength,
    SG_diff_t * pResolvedDiff);

const SG_diff_output_functions _sg_diff3__file_output_vtable = {
        _sg_diff3__file_output_common,
        _sg_diff3__file_output_diff_modified,
        _sg_diff3__file_output_diff_latest,
        _sg_diff3__file_output_diff_modified, // output_diff_common
        _sg_diff3__file_output_conflict};

static void _sg_diff3__file_output_conflict(
    SG_context * pCtx,
    void * pOutputBaton,
    SG_int32 originalStart, SG_int32 originalLength,
    SG_int32 modifiedStart, SG_int32 modifiedLength,
    SG_int32 latestStart, SG_int32 latestLength,
    SG_diff_t * pResolvedDiff)
{
    _sg_diff3__file_output_baton_t * pFileOutputBaton = pOutputBaton;

    if(pResolvedDiff!=NULL && pFileOutputBaton->displayResolvedConflicts)
    {
        SG_ERR_CHECK_RETURN(  SG_diff_output(pCtx, &_sg_diff3__file_output_vtable, pOutputBaton, pResolvedDiff)  );
        return;
    }

    SG_ERR_CHECK(  SG_file__write__sz(pCtx, pFileOutputBaton->pOutputFile, pFileOutputBaton->szConflictModified)  );
    SG_ERR_CHECK(  SG_file__write__sz(pCtx, pFileOutputBaton->pOutputFile, SG_PLATFORM_NATIVE_EOL_STR)  );

    SG_ERR_CHECK(  _sg_diff3__file_output_hunk(pCtx, pOutputBaton, 1, modifiedStart, modifiedLength)  );

    if(pFileOutputBaton->displayOriginalInConflict)
    {
        SG_ERR_CHECK(  SG_file__write__sz(pCtx, pFileOutputBaton->pOutputFile, pFileOutputBaton->szConflictOriginal)  );
        SG_ERR_CHECK(  SG_file__write__sz(pCtx, pFileOutputBaton->pOutputFile, SG_PLATFORM_NATIVE_EOL_STR)  );

        SG_ERR_CHECK(  _sg_diff3__file_output_hunk(pCtx, pOutputBaton, 0, originalStart, originalLength)  );
    }

    SG_ERR_CHECK(  SG_file__write__sz(pCtx, pFileOutputBaton->pOutputFile, pFileOutputBaton->szConflictSeparator)  );
    SG_ERR_CHECK(  SG_file__write__sz(pCtx, pFileOutputBaton->pOutputFile, SG_PLATFORM_NATIVE_EOL_STR)  );

    SG_ERR_CHECK(  _sg_diff3__file_output_hunk(pCtx, pOutputBaton, 2, latestStart, latestLength)  );

    SG_ERR_CHECK(  SG_file__write__sz(pCtx, pFileOutputBaton->pOutputFile, pFileOutputBaton->szConflictLatest)  );
    SG_ERR_CHECK(  SG_file__write__sz(pCtx, pFileOutputBaton->pOutputFile, SG_PLATFORM_NATIVE_EOL_STR)  );

    return;
fail:
    ;
}

void _sg_diff__resolve_conflict(
    SG_context * pCtx,
    _sg_diff__mempool * pMempool,
    SG_diff_t * pHunk,
    _sg_diff__position_t ** ppPositionList1,
    _sg_diff__position_t ** ppPositionList2)
{
    SG_int32 modified_start = pHunk->start[SG_DIFFCORE__DOC_NDX_MODIFIED] + 1;
    SG_int32 latest_start = pHunk->start[SG_DIFFCORE__DOC_NDX_LATEST] + 1;
    SG_int32 commonLength;
    SG_int32 modifiedLength = pHunk->length[SG_DIFFCORE__DOC_NDX_MODIFIED];
    SG_int32 latestLength = pHunk->length[SG_DIFFCORE__DOC_NDX_LATEST];
    _sg_diff__position_t * startPositions[2];
    _sg_diff__position_t * positions[2];
    _sg_diff__lcs_t * pLcs = NULL;
    _sg_diff__lcs_t ** ppNextLcs = &pLcs;

    SG_diff_t ** ppNextDiff = &pHunk->pResolvedDiff;

    SG_ASSERT(pCtx!=NULL);

    // First find the starting positions for the comparison

    startPositions[0] = *ppPositionList1;
    startPositions[1] = *ppPositionList2;

    while (startPositions[0]->offset < modified_start)
        startPositions[0] = startPositions[0]->pNext;

    while (startPositions[1]->offset < latest_start)
        startPositions[1] = startPositions[1]->pNext;

    positions[0] = startPositions[0];
    positions[1] = startPositions[1];

    commonLength = modifiedLength < latestLength
        ? modifiedLength : latestLength;

    while (commonLength > 0
           && positions[0]->pNode == positions[1]->pNode)
    {
        positions[0] = positions[0]->pNext;
        positions[1] = positions[1]->pNext;

        commonLength--;
    }

    if (commonLength == 0
        && modifiedLength == latestLength)
    {
        pHunk->type = SG_DIFF_TYPE__COMMON;
        pHunk->pResolvedDiff = NULL;

        *ppPositionList1 = positions[0];
        *ppPositionList2 = positions[1];

        return;
    }

    pHunk->type = SG_DIFF_TYPE__CONFLICT;

    // ### If we have a conflict we can try to find the
    // ### common parts in it by getting an lcs between
    // ### modified (start to start + length) and
    // ### latest (start to start + length).
    // ### We use this lcs to create a simple diff.  Only
    // ### where there is a diff between the two, we have
    // ### a conflict.
    // ### This raises a problem; several common diffs and
    // ### conflicts can occur within the same original
    // ### block.  This needs some thought.
    // ###
    // ### NB: We can use the node _pointers_ to identify
    // ###     different tokens

    // Calculate how much of the two sequences was actually the same.
    commonLength = (modifiedLength < latestLength
                     ? modifiedLength : latestLength)
        - commonLength;

    // If there were matching symbols at the start of both sequences, record that fact.
    if (commonLength > 0)
    {
        SG_ERR_CHECK(  _sg_diff__lcs_t__alloc(pCtx, pMempool, &pLcs)  );
        pLcs->pNext = NULL;
        pLcs->positions[0] = startPositions[0];
        pLcs->positions[1] = startPositions[1];
        pLcs->length = commonLength;

        ppNextLcs = &pLcs->pNext;
    }

    modifiedLength -= commonLength;
    latestLength -= commonLength;

    modified_start = startPositions[0]->offset;
    latest_start = startPositions[1]->offset;

    startPositions[0] = positions[0];
    startPositions[1] = positions[1];

    // Create a new ring for svn_diff__lcs to grok.
    // We can safely do this given we don't need the
    // positions we processed anymore.
    if (modifiedLength == 0)
    {
        *ppPositionList1 = positions[0];
        positions[0] = NULL;
    }
    else
    {
        while (--modifiedLength)
            positions[0] = positions[0]->pNext;

        *ppPositionList1 = positions[0]->pNext;
        positions[0]->pNext = startPositions[0];
    }

    if (latestLength == 0)
    {
        *ppPositionList2 = positions[1];
        positions[1] = NULL;
    }
    else
    {
        while (--latestLength)
            positions[1] = positions[1]->pNext;

        *ppPositionList2 = positions[1]->pNext;
        positions[1]->pNext = startPositions[1];
    }

    SG_ERR_CHECK(  _sg_diff__lcs(pCtx, pMempool, positions[0], positions[1], ppNextLcs)  );

    // Fix up the EOF lcs element in case one of the two sequences was NULL.
    if ((*ppNextLcs)->positions[0]->offset == 1)
        (*ppNextLcs)->positions[0] = *ppPositionList1;

    if ((*ppNextLcs)->positions[1]->offset == 1)
        (*ppNextLcs)->positions[1] = *ppPositionList2;

    // Restore modifiedLength and latestLength
    modifiedLength = pHunk->length[SG_DIFFCORE__DOC_NDX_MODIFIED];
    latestLength = pHunk->length[SG_DIFFCORE__DOC_NDX_LATEST];

    // Produce the resolved diff
    while (1)
    {
        if (modified_start < pLcs->positions[0]->offset
            || latest_start < pLcs->positions[1]->offset)
        {
            SG_ERR_CHECK(  SG_alloc1(pCtx, *ppNextDiff)  );

            (*ppNextDiff)->type = SG_DIFF_TYPE__CONFLICT;
            (*ppNextDiff)->start[SG_DIFFCORE__DOC_NDX_ORIGINAL] = pHunk->start[SG_DIFFCORE__DOC_NDX_ORIGINAL];
            (*ppNextDiff)->length[SG_DIFFCORE__DOC_NDX_ORIGINAL] = pHunk->length[SG_DIFFCORE__DOC_NDX_ORIGINAL];
            (*ppNextDiff)->start[SG_DIFFCORE__DOC_NDX_MODIFIED] = modified_start - 1;
            (*ppNextDiff)->length[SG_DIFFCORE__DOC_NDX_MODIFIED] = pLcs->positions[0]->offset - modified_start;
            (*ppNextDiff)->start[SG_DIFFCORE__DOC_NDX_LATEST] = latest_start - 1;
            (*ppNextDiff)->length[SG_DIFFCORE__DOC_NDX_LATEST] = pLcs->positions[1]->offset - latest_start;

            ppNextDiff = &(*ppNextDiff)->pNext;
        }

        // Detect the EOF
        if (pLcs->length == 0)
            break;

        modified_start = pLcs->positions[0]->offset;
        latest_start = pLcs->positions[1]->offset;

        SG_ERR_CHECK(  SG_alloc1(pCtx, *ppNextDiff)  );

        (*ppNextDiff)->type = SG_DIFF_TYPE__COMMON;
        (*ppNextDiff)->start[SG_DIFFCORE__DOC_NDX_ORIGINAL] = pHunk->start[SG_DIFFCORE__DOC_NDX_ORIGINAL];
        (*ppNextDiff)->length[SG_DIFFCORE__DOC_NDX_ORIGINAL] = pHunk->length[SG_DIFFCORE__DOC_NDX_ORIGINAL];
        (*ppNextDiff)->start[SG_DIFFCORE__DOC_NDX_MODIFIED] = modified_start - 1;
        (*ppNextDiff)->length[SG_DIFFCORE__DOC_NDX_MODIFIED] = pLcs->length;
        (*ppNextDiff)->start[SG_DIFFCORE__DOC_NDX_LATEST] = latest_start - 1;
        (*ppNextDiff)->length[SG_DIFFCORE__DOC_NDX_LATEST] = pLcs->length;

        ppNextDiff = &(*ppNextDiff)->pNext;

        modified_start += pLcs->length;
        latest_start += pLcs->length;

        pLcs = pLcs->pNext;
    }

    *ppNextDiff = NULL;

    return;
fail:
    ;
}

void _sg_diff3__diff3(
    SG_context * pCtx,
    _sg_diff__mempool * pMempool,
    _sg_diff__lcs_t * pLcs_om, _sg_diff__lcs_t * pLcs_ol,
    SG_int32 originalStart, SG_int32 modifiedStart, SG_int32 latestStart,
    _sg_diff__position_t * positionLists[3],
    SG_diff_t ** ppResult)
{
    SG_diff_t * pResult = NULL;
    SG_diff_t ** ppNext = &pResult;

    SG_int32 originalSync;
    SG_int32 modifiedSync;
    SG_int32 latestSync;
    SG_int32 commonLength;
    SG_int32 originalLength;
    SG_int32 modifiedLength;
    SG_int32 latestLength;
    SG_bool isModified;
    SG_bool isLatest;
    _sg_diff__position_t sentinelPositions[2];

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(ppResult);

    // Point the position lists to the start of the list so that
    // common_diff/conflict detection actually is able to work.
    if(positionLists[1]!=NULL)
    {
        sentinelPositions[0].pNext = positionLists[1]->pNext;
        sentinelPositions[0].offset = positionLists[1]->offset + 1;
        positionLists[1]->pNext = &sentinelPositions[0];
        positionLists[1] = sentinelPositions[0].pNext;
    }
    else
    {
        sentinelPositions[0].offset = 1;
        sentinelPositions[0].pNext = NULL;
        positionLists[1] = &sentinelPositions[0];
    }

    if (positionLists[2])
    {
        sentinelPositions[1].pNext = positionLists[2]->pNext;
        sentinelPositions[1].offset = positionLists[2]->offset + 1;
        positionLists[2]->pNext = &sentinelPositions[1];
        positionLists[2] = sentinelPositions[1].pNext;
    }
    else
    {
        sentinelPositions[1].offset = 1;
        sentinelPositions[1].pNext = NULL;
        positionLists[2] = &sentinelPositions[1];
    }

    while (1)
    {
        // Find the sync points
        while (1)
        {
            if (pLcs_om->positions[0]->offset > pLcs_ol->positions[0]->offset)
            {
                originalSync = pLcs_om->positions[0]->offset;

                while (pLcs_ol->positions[0]->offset + pLcs_ol->length < originalSync)
                    pLcs_ol = pLcs_ol->pNext;

                // If the sync point is the EOF, and our current lcs segment
                // doesn't reach as far as EOF, we need to skip this segment.
                if (pLcs_om->length == 0 && pLcs_ol->length > 0
                    && pLcs_ol->positions[0]->offset + pLcs_ol->length == originalSync
                    && pLcs_ol->positions[1]->offset + pLcs_ol->length != pLcs_ol->pNext->positions[1]->offset)
                    pLcs_ol = pLcs_ol->pNext;

                if (pLcs_ol->positions[0]->offset <= originalSync)
                    break;
            }
            else
            {
                originalSync = pLcs_ol->positions[0]->offset;

                while (pLcs_om->positions[0]->offset + pLcs_om->length < originalSync)
                    pLcs_om = pLcs_om->pNext;

                // If the sync point is the EOF, and our current lcs segment
                // doesn't reach as far as EOF, we need to skip this segment.
                if (pLcs_ol->length == 0 && pLcs_om->length > 0
                    && pLcs_om->positions[0]->offset + pLcs_om->length == originalSync
                    && pLcs_om->positions[1]->offset + pLcs_om->length != pLcs_om->pNext->positions[1]->offset)
                    pLcs_om = pLcs_om->pNext;

                if (pLcs_om->positions[0]->offset <= originalSync)
                    break;
            }
        }

        modifiedSync = pLcs_om->positions[1]->offset + (originalSync - pLcs_om->positions[0]->offset);
        latestSync = pLcs_ol->positions[1]->offset + (originalSync - pLcs_ol->positions[0]->offset);

        // Determine what is modified, if anything
        isModified = pLcs_om->positions[0]->offset - originalStart > 0
            || pLcs_om->positions[1]->offset - modifiedStart > 0;

        isLatest = pLcs_ol->positions[0]->offset - originalStart > 0
            || pLcs_ol->positions[1]->offset - latestStart > 0;

        if(isModified||isLatest)
        {
            originalLength = originalSync - originalStart;
            modifiedLength = modifiedSync - modifiedStart;
            latestLength = latestSync - latestStart;

            SG_ERR_CHECK(  SG_alloc1(pCtx, *ppNext)  );

            (*ppNext)->start[SG_DIFFCORE__DOC_NDX_ORIGINAL] = originalStart - 1;
            (*ppNext)->length[SG_DIFFCORE__DOC_NDX_ORIGINAL] = originalSync - originalStart;
            (*ppNext)->start[SG_DIFFCORE__DOC_NDX_MODIFIED] = modifiedStart - 1;
            (*ppNext)->length[SG_DIFFCORE__DOC_NDX_MODIFIED] = modifiedLength;
            (*ppNext)->start[SG_DIFFCORE__DOC_NDX_LATEST] = latestStart - 1;
            (*ppNext)->length[SG_DIFFCORE__DOC_NDX_LATEST] = latestLength;
            (*ppNext)->pResolvedDiff = NULL;

            if (isModified && isLatest)
                SG_ERR_CHECK(  _sg_diff__resolve_conflict(pCtx, pMempool, *ppNext, &positionLists[1], &positionLists[2])  );
            else if (isModified)
                (*ppNext)->type = SG_DIFF_TYPE__DIFF_MODIFIED;
            else
                (*ppNext)->type = SG_DIFF_TYPE__DIFF_LATEST;

            ppNext = &(*ppNext)->pNext;
        }

        // Detect EOF
        if (pLcs_om->length == 0 || pLcs_ol->length == 0)
            break;

        modifiedLength = pLcs_om->length - (originalSync - pLcs_om->positions[0]->offset);
        latestLength = pLcs_ol->length - (originalSync - pLcs_ol->positions[0]->offset);
        commonLength = modifiedLength < latestLength ? modifiedLength : latestLength;

        SG_ERR_CHECK(  SG_alloc1(pCtx, *ppNext)  );

        (*ppNext)->type = SG_DIFF_TYPE__DIFF_COMMON;
        (*ppNext)->start[SG_DIFFCORE__DOC_NDX_ORIGINAL] = originalSync - 1;
        (*ppNext)->length[SG_DIFFCORE__DOC_NDX_ORIGINAL] = commonLength;
        (*ppNext)->start[SG_DIFFCORE__DOC_NDX_MODIFIED] = modifiedSync - 1;
        (*ppNext)->length[SG_DIFFCORE__DOC_NDX_MODIFIED] = commonLength;
        (*ppNext)->start[SG_DIFFCORE__DOC_NDX_LATEST] = latestSync - 1;
        (*ppNext)->length[SG_DIFFCORE__DOC_NDX_LATEST] = commonLength;
        (*ppNext)->pResolvedDiff = NULL;

        ppNext = &(*ppNext)->pNext;

        // Set the new offsets
        originalStart = originalSync + commonLength;
        modifiedStart = modifiedSync + commonLength;
        latestStart = latestSync + commonLength;

        // Make it easier for diff_common/conflict detection by recording last lcs start positions
        if (positionLists[1]->offset < pLcs_om->positions[1]->offset)
            positionLists[1] = pLcs_om->positions[1];

        if (positionLists[2]->offset < pLcs_ol->positions[1]->offset)
            positionLists[2] = pLcs_ol->positions[1];

        // Make sure we are pointing to lcs entries beyond the range we just processed
        while (originalStart >= pLcs_om->positions[0]->offset + pLcs_om->length && pLcs_om->length > 0)
        {
            pLcs_om = pLcs_om->pNext;
        }

        while (originalStart >= pLcs_ol->positions[0]->offset + pLcs_ol->length && pLcs_ol->length > 0)
        {
            pLcs_ol = pLcs_ol->pNext;
        }
    }

    *ppNext = NULL;

    *ppResult = pResult;

    return;
fail:
    ;
}































// A "file_token" represented the data in a "line" of a file that we load and run the diff/diff3 algorithm on.
typedef struct __sg_diff__file_token_t _sg_diff__file_token_t;
struct __sg_diff__file_token_t
{
    SG_int32 length;
    const char * line;

    _sg_diff__file_token_t * pPrev; // We only maintain a linked list of all tokens for the purpose of freeing them all at the end.
};

void _sg_diff__file_token__nullfree(_sg_diff__file_token_t ** ppToken)
{
    _sg_diff__file_token_t * pToken = NULL;

    if(ppToken==NULL||*ppToken==NULL)
        return;

    pToken = *ppToken;
    while(pToken!=NULL)
    {
        _sg_diff__file_token_t * pPrev = pToken->pPrev;
        SG_free__no_ctx(pToken);
        pToken = pPrev;
    }
    *ppToken = NULL;
}


struct __sg_diff__file_baton
{
    const SG_pathname * path[4];

    char * buffer[4];
    char * curp[4];
    char * endp[4];

    _sg_diff__file_token_t * pPreviousToken;
    _sg_diff__file_token_t * pTokenToReuse;

    SG_diff_options options;
    SG_int32 (*m_fncmp)(const char * buf1, const char * buf2, SG_uint32 nrChars);
};
typedef struct __sg_diff__file_baton _sg_diff__file_baton_t;


// A "sub-class" of _sg_diff__file_baton_t for dealing with piecetables for line-oriented diffs
struct __sg_utp_vtfn_line__file_baton
{
    _sg_diff__file_baton_t file_baton; // must be first
    void * pVoid_sg_diffcore;
    void * pVoid_Frag[3];
};
typedef struct __sg_utp_vtfn_line__file_baton _sg_utp_vtfn_line__file_baton_t;


// A "sub-class" of _sg_diff__file_baton_t for intra-line diffs with piecetables
struct __sg_utp_vtfn_char__file_baton
{
    _sg_diff__file_baton_t file_baton; // must be first
    void * pVoid_sg_diffcore;
    void * pVoid_Frag[3];
    void * pVoid_FragEnd[3];
    SG_int32 offset[3]; // within a span
};
typedef struct __sg_utp_vtfn_char__file_baton _sg_utp_vtfn_char__file_baton_t;

static SG_bool my_isspace(char b)
{
    // Full list of unicode whitespace characters is at the top of this page:
    // http://unicode.org/Public/UNIDATA/PropList.txt

    // Our ignore whitespace setting is really just ignoring regular space
    // and regular tab. (Note, line breaks are n/a for us since we're only
    // looking at one line--not including the linebreaks--at a time.)

    // This is easier because we can just look at one byte at a time.

    return (b==' ' || b=='\t');
}


static SG_int32 my_memcmp(const char * p1, const char * p2, SG_uint32 numBytes)
{
    return memcmp(p1, p2, numBytes);
}
static SG_int32 my_memicmp(const char * p1, const char * p2, SG_uint32 numBytes)
{
    return SG_memicmp((const SG_byte *)p1, (const SG_byte *)p2, numBytes);
}

void _sg_diff__file_load_fncmp(void * pDiffBaton)
{
    _sg_diff__file_baton_t * pFileDiffBaton = (_sg_diff__file_baton_t *)pDiffBaton;

    if(SG_HAS_SET(pFileDiffBaton->options, SG_DIFF_OPTION__IGNORE_CASE))
        pFileDiffBaton->m_fncmp = my_memicmp;
    else
        pFileDiffBaton->m_fncmp = my_memcmp;
}


static void _sg_diff__file_datasource_open(SG_context * pCtx, void * pDiffBaton, SG_diff_datasource datasource)
{
    // Read raw file and convert from whatever encoding into utf-8.

    // Note we don't have to keep track of the original encoding at
    // this point because the output of the diff is purely in terms
    // of line numbers and is encoding-neutral.

    _sg_diff__file_baton_t *pFileDiffBaton = (_sg_diff__file_baton_t *)pDiffBaton;
    SG_fsobj_stat finfo;
    SG_file * pFile = NULL;
    SG_byte * pFileContents = NULL;
    SG_enumeration idx = datasource;

    SG_ASSERT(pCtx!=NULL);

    _sg_diff__file_load_fncmp(pDiffBaton);

    pFileDiffBaton->buffer[idx] = NULL;
    pFileDiffBaton->curp[idx] = NULL;
    pFileDiffBaton->endp[idx] = NULL;

    if(pFileDiffBaton->path[idx]==NULL)
        return;

    SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pFileDiffBaton->path[idx], &finfo)  );
    if(finfo.size!=0)
    {
        SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pFileDiffBaton->path[idx], SG_FILE_OPEN_EXISTING|SG_FILE_RDONLY, SG_FSOBJ_PERMS__UNUSED, &pFile)  );

        //todo: don't store the whole file in memory all at once
        if(finfo.size>SG_UINT32_MAX)
            SG_ERR_THROW2(SG_ERR_LIMIT_EXCEEDED, (pCtx, "File '%s' is too large to diff.", pFileDiffBaton->path));
        SG_ERR_CHECK(  SG_allocN(pCtx, (SG_uint32)finfo.size, pFileContents)  );
        SG_ERR_CHECK(  SG_file__read(pCtx, pFile,(SG_uint32)finfo.size, pFileContents, NULL)  );
        SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

        SG_ERR_CHECK(  SG_utf8__import_buffer(pCtx, pFileContents, (SG_uint32)finfo.size, &pFileDiffBaton->buffer[idx], NULL)  );

        pFileDiffBaton->curp[idx] = pFileDiffBaton->buffer[idx];
        pFileDiffBaton->endp[idx] = pFileDiffBaton->buffer[idx] + strlen(pFileDiffBaton->buffer[idx]);

        SG_NULLFREE(pCtx, pFileContents);
    }

    return;
fail:
    SG_FILE_NULLCLOSE(pCtx, pFile);
    SG_NULLFREE(pCtx, pFileContents);
}
static void _sg_diff__file_datasource_close(SG_context * pCtx, void *pDiffBaton, SG_diff_datasource datasource)
{
    SG_UNUSED(pCtx);
    SG_UNUSED(pDiffBaton);
    SG_UNUSED(datasource);
}
static void _sg_diff__file_datasource_get_next_token(SG_context * pCtx, void *pDiffBaton, SG_diff_datasource datasource, void **ppToken)
{
    _sg_diff__file_baton_t * pFileDiffBaton = (_sg_diff__file_baton_t *)pDiffBaton;
    _sg_diff__file_token_t * pFileToken = NULL;
    SG_enumeration idx = datasource;
    char * pEndp = NULL;
    char *pCurp = NULL;
    char * pEol = NULL;
    SG_uint32 eolLen;
    SG_bool bAnyEOL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(ppToken);

    pCurp = pFileDiffBaton->curp[idx];
    pEndp = pFileDiffBaton->endp[idx];

    if(pCurp==pEndp)
    {
        *ppToken = NULL;
        return;
    }

    if(pFileDiffBaton->pTokenToReuse==NULL)
    {
        SG_ERR_CHECK(  SG_alloc1(pCtx, pFileToken)  );
        pFileToken->pPrev = pFileDiffBaton->pPreviousToken;
    }
    else
    {
        pFileToken = pFileDiffBaton->pTokenToReuse;
        pFileDiffBaton->pTokenToReuse = NULL;
    }
    pFileDiffBaton->pPreviousToken = pFileToken;

    pFileToken->length = 0;

    bAnyEOL = SG_HAS_SET(pFileDiffBaton->options,SG_DIFF_OPTION__ANY_EOL); // do we accept any EOL style
    if (bAnyEOL)    
        pEol = my_anyeol_byte(pCurp,pEndp-pCurp);
    else                                                                                // we accept only the platform-native EOL style
        pEol = my_memchr2_byte(pCurp, SG_PLATFORM_NATIVE_EOL_STR, pEndp-pCurp);

    if(pEol==NULL)
    {
        eolLen = 0;
        pEol = pEndp;
    }
    else if( (pEol[0]==SG_CR && pEol[1]==SG_LF) && (bAnyEOL || SG_PLATFORM_NATIVE_EOL_IS_CRLF) )
        eolLen = 2; // We have a dos-style CRLF, treat as CRLF only if we allow anything or we are on a CRLF host
    else
        eolLen = 1; // mac style CR or unix style LF

    pFileToken->line = pCurp;
    pFileToken->length = pEol - pCurp; // don't include the EOL in the token

    pFileDiffBaton->curp[idx] = pEol + eolLen;
    *ppToken = pFileToken;

    return;
fail:
    ;
}
SG_int32 _sg_diff__file_token_compare(void * pDiffBaton, void * pToken1, void * pToken2)
{
    _sg_diff__file_baton_t * pFileDiffBaton = (_sg_diff__file_baton_t *)pDiffBaton;
    _sg_diff__file_token_t * pFileToken1 = (_sg_diff__file_token_t *)pToken1;
    _sg_diff__file_token_t * pFileToken2 = (_sg_diff__file_token_t *)pToken2;
    SG_int32 minLength;
    SG_int32 result;

    //todo: Casts like this to uint32 are a bit fishy. They exist because we loaded the whole file into memory.
    // Something probably needs to change to make some of these casts go away...
    minLength = (SG_uint32)SG_MIN(pFileToken1->length,pFileToken2->length);
    result = (*pFileDiffBaton->m_fncmp)(pFileToken1->line, pFileToken2->line, minLength);

    if (pFileToken1->length == pFileToken2->length)
        return result;

    if (pFileToken1->length < pFileToken2->length)
        return -1;
    else
        return 1;
}
int _sg_diff__file_token_ignorewhitespace_compare(void * pDiffBaton, void * pToken1, void * pToken2)
{
    _sg_diff__file_baton_t * pFileDiffBaton = (_sg_diff__file_baton_t *)pDiffBaton;
    _sg_diff__file_token_t * pFileToken1 = (_sg_diff__file_token_t *)pToken1;
    _sg_diff__file_token_t * pFileToken2 = (_sg_diff__file_token_t *)pToken2;

    const char * p1;
    const char * p2;
    const char * p1end;
    const char * p2end;

    // quick test if exactly the same.
    // See "WARNING C4244: __int64 to uint conversion" in libsgdcore_private.h

    //todo: Casts like this to uint32 are a bit fishy. They exist because we loaded the whole file into memory.
    // Something probably needs to change to make some of these casts go away...
    if((pFileToken1->length==pFileToken2->length) && ((*pFileDiffBaton->m_fncmp)(pFileToken1->line, pFileToken2->line, (SG_uint32)pFileToken1->length) == 0))
        return 0;

    p1      = pFileToken1->line;
    p2      = pFileToken2->line;
    p1end   = pFileToken1->line + pFileToken1->length;
    p2end   = pFileToken2->line + pFileToken2->length;

    // trim trailing whitespace, since we probably don't get the LF
    // in the buffer (fgets semantics) and we want trailing spaces,
    // TABs, and CR to sync up with the LF.
    while( (p1end>p1) && my_isspace(p1end[-1]) )
        p1end--;
    while( (p2end>p2) && my_isspace(p2end[-1]) )
        p2end--;

    while ( (p1<p1end) && (p2<p2end) )
    {
        if (my_isspace(*p1) && my_isspace(*p2))
        {
            // both are whitespace.  collapse multiple to one, effectively.
            do{ p1++; } while( (p1<p1end) && my_isspace(*p1) );
            do{ p2++; } while( (p2<p2end) && my_isspace(*p2) );
        }
        else
        {
            // otherwise, one or both not whitespace, compare.
            SG_int32 r;
            r = (*pFileDiffBaton->m_fncmp)(p1,p2,1); // compare this character
            if(r!=0)
                return r; // if different, we're done.

            // otherwise, we had a match.  keep looking.
            p1++;
            p2++;
        }
    }

    // we reached the end of one or both.
    if(p1==p1end)
        if(p2==p2end)
            return 0; // end of both, so effectively equal.
        else
            return -1; // buf1 shorter
    else
        return 1; // buf2 shorter
}
static void _sg_diff__file_token_discard(void * pDiffBaton, void * pToken)
{
    _sg_diff__file_baton_t * pFileDiffBaton = (_sg_diff__file_baton_t *)pDiffBaton;
    SG_ASSERT(pFileDiffBaton->pTokenToReuse==NULL);
    pFileDiffBaton->pTokenToReuse = pToken;
}
static void _sg_diff__file_token_discard_all(void * pDiffBaton)
{
    _sg_diff__file_baton_t * pFileDiffBaton = (_sg_diff__file_baton_t *)pDiffBaton;

    if(pFileDiffBaton->pTokenToReuse!=NULL)
    {
        _sg_diff__file_token__nullfree(&pFileDiffBaton->pTokenToReuse);
        pFileDiffBaton->pPreviousToken = NULL; // Also got freed.
    }
    else
        _sg_diff__file_token__nullfree(&pFileDiffBaton->pPreviousToken);

    SG_free__no_ctx(pFileDiffBaton->buffer[SG_DIFF_DATASOURCE__ORIGINAL]);
    pFileDiffBaton->buffer[SG_DIFF_DATASOURCE__ORIGINAL] = NULL;
    SG_free__no_ctx(pFileDiffBaton->buffer[SG_DIFF_DATASOURCE__MODIFIED]);
    pFileDiffBaton->buffer[SG_DIFF_DATASOURCE__MODIFIED] = NULL;
    SG_free__no_ctx(pFileDiffBaton->buffer[SG_DIFF_DATASOURCE__LATEST]);
    pFileDiffBaton->buffer[SG_DIFF_DATASOURCE__LATEST] = NULL;
    SG_free__no_ctx(pFileDiffBaton->buffer[SG_DIFF_DATASOURCE__ANCESTOR]);
    pFileDiffBaton->buffer[SG_DIFF_DATASOURCE__ANCESTOR] = NULL;
}
static const SG_diff_functions _sg_diff__file_vtable = {
    _sg_diff__file_datasource_open,
    _sg_diff__file_datasource_close,
    _sg_diff__file_datasource_get_next_token,
    _sg_diff__file_token_compare,
    _sg_diff__file_token_discard,
    _sg_diff__file_token_discard_all
};
static const SG_diff_functions _sg_diff__file_ignorewhitespace_vtable = {
    _sg_diff__file_datasource_open,
    _sg_diff__file_datasource_close,
    _sg_diff__file_datasource_get_next_token,
    _sg_diff__file_token_ignorewhitespace_compare,
    _sg_diff__file_token_discard,
    _sg_diff__file_token_discard_all
};
const SG_diff_functions * _sg_diff__selectDiffFnsVTable(SG_diff_options eOptions)
{
	if (SG_HAS_SET(eOptions,SG_DIFF_OPTION__IGNORE_WHITESPACE))
		return &_sg_diff__file_ignorewhitespace_vtable;
	else
		return &_sg_diff__file_vtable;
}


//////////////////////////////////////////////////////////////////
// The Main Events
//////////////////////////////////////////////////////////////////

void SG_diff(SG_context * pCtx, const SG_diff_functions * pVtable, void * pDiffBaton, SG_diff_t ** ppDiff)
{
    _sg_diff__tree_t tree;
    _sg_diff__mempool mempool;
    _sg_diff__position_t * positionLists[2] = {NULL, NULL};
    _sg_diff__lcs_t * pLcs = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pVtable);
    SG_NULLARGCHECK_RETURN(ppDiff);

    tree.pRoot = NULL;
    SG_zero(mempool);

    // Insert the data into the tree
    SG_ERR_CHECK(  _sg_diff__get_tokens(pCtx, &mempool, &tree, pVtable, pDiffBaton, SG_DIFF_DATASOURCE__ORIGINAL, &positionLists[0])  );
    SG_ERR_CHECK(  _sg_diff__get_tokens(pCtx, &mempool, &tree, pVtable, pDiffBaton, SG_DIFF_DATASOURCE__MODIFIED, &positionLists[1])  );

    // The cool part is that we don't need the tokens anymore.
    // Allow the app to clean them up if it wants to.
    if(pVtable->token_discard_all!=NULL)
        pVtable->token_discard_all(pDiffBaton);

    _sg_diff__node__nullfree(&tree.pRoot);

    // Get the lcs
    SG_ERR_CHECK(  _sg_diff__lcs(pCtx, &mempool, positionLists[0], positionLists[1], &pLcs)  );

    // Produce the diff
    SG_ERR_CHECK(  _sg_diff__diff(pCtx, pLcs, 1, 1, SG_TRUE, ppDiff)  );

    _sg_diff__mempool__flush(&mempool);

    return;
fail:
    _sg_diff__node__nullfree(&tree.pRoot);
    _sg_diff__mempool__flush(&mempool);
}

void SG_diff3(SG_context * pCtx, const SG_diff_functions * pVtable, void * pDiffBaton, SG_diff_t ** ppDiff)
{
    _sg_diff__tree_t tree;
    _sg_diff__mempool mempool;
    _sg_diff__position_t * positionLists[3] = {NULL, NULL, NULL};
    _sg_diff__lcs_t * pLcs_om = NULL;
    _sg_diff__lcs_t * pLcs_ol = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pVtable);
    SG_NULLARGCHECK_RETURN(ppDiff);

    tree.pRoot = NULL;
    SG_zero(mempool);

    // Insert the data into the tree
    SG_ERR_CHECK(  _sg_diff__get_tokens(pCtx, &mempool, &tree, pVtable, pDiffBaton, SG_DIFF_DATASOURCE__ORIGINAL, &positionLists[0])  );
    SG_ERR_CHECK(  _sg_diff__get_tokens(pCtx, &mempool, &tree, pVtable, pDiffBaton, SG_DIFF_DATASOURCE__MODIFIED, &positionLists[1])  );
    SG_ERR_CHECK(  _sg_diff__get_tokens(pCtx, &mempool, &tree, pVtable, pDiffBaton, SG_DIFF_DATASOURCE__LATEST, &positionLists[2])  );

    // Get rid of the tokens, we don't need them to calc the diff
    if(pVtable->token_discard_all!=NULL)
        pVtable->token_discard_all(pDiffBaton);

    _sg_diff__node__nullfree(&tree.pRoot);

    // Get the lcs for original-modified and original-latest
    SG_ERR_CHECK(  _sg_diff__lcs(pCtx, &mempool, positionLists[0], positionLists[1], &pLcs_om)  );
    SG_ERR_CHECK(  _sg_diff__lcs(pCtx, &mempool, positionLists[0], positionLists[2], &pLcs_ol)  );

    // Produce a merged diff
    SG_ERR_CHECK(  _sg_diff3__diff3(pCtx, &mempool, pLcs_om, pLcs_ol, 1, 1, 1, positionLists, ppDiff)  );

    _sg_diff__mempool__flush(&mempool);

    return;
fail:
    _sg_diff__node__nullfree(&tree.pRoot);
    _sg_diff__mempool__flush(&mempool);
}

void SG_diff__free(SG_context * pCtx, SG_diff_t * pDiff)
{
    SG_ASSERT(pCtx!=NULL);
    while(pDiff!=NULL)
    {
        SG_diff_t * pNext = pDiff->pNext;
        SG_DIFF_NULLFREE(pCtx, pDiff->pResolvedDiff);
        SG_NULLFREE(pCtx, pDiff);
        pDiff = pNext;
    }
}


//////////////////////////////////////////////////////////////////
// Utility Functions
//////////////////////////////////////////////////////////////////

SG_bool SG_diff_contains_conflicts(SG_diff_t * pDiff)
{
    while(pDiff!=NULL)
    {
        if(pDiff->type==SG_DIFF_TYPE__CONFLICT)
            return SG_TRUE;
        pDiff = pDiff->pNext;
    }
    return SG_FALSE;
}

SG_bool SG_diff_contains_diffs(SG_diff_t * pDiff)
{
    while(pDiff!=NULL)
    {
        if(pDiff->type!=SG_DIFF_TYPE__COMMON)
            return SG_TRUE;
        pDiff = pDiff->pNext;
    }
    return SG_FALSE;
}


//////////////////////////////////////////////////////////////////
// Displaying Diffs
//////////////////////////////////////////////////////////////////

void SG_diff_output(SG_context * pCtx, const SG_diff_output_functions * pVtable, void * pOutputBaton, SG_diff_t * pDiff)
{
    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pVtable);

    while(pDiff!=NULL)
    {
        SG_diff_output_function output_function = NULL;
        switch(pDiff->type)
        {
        case SG_DIFF_TYPE__COMMON:        output_function = pVtable->output_common; break;
        case SG_DIFF_TYPE__DIFF_COMMON:   output_function = pVtable->output_diff_common; break;
        case SG_DIFF_TYPE__DIFF_MODIFIED: output_function = pVtable->output_diff_modified; break;
        case SG_DIFF_TYPE__DIFF_LATEST:   output_function = pVtable->output_diff_latest; break;
        case SG_DIFF_TYPE__CONFLICT:
            if (pVtable->output_conflict != NULL)
            {
                SG_ERR_CHECK(  pVtable->output_conflict(
                    pCtx, pOutputBaton,
                    pDiff->start[SG_DIFFCORE__DOC_NDX_ORIGINAL], pDiff->length[SG_DIFFCORE__DOC_NDX_ORIGINAL],
                    pDiff->start[SG_DIFFCORE__DOC_NDX_MODIFIED], pDiff->length[SG_DIFFCORE__DOC_NDX_MODIFIED],
                    pDiff->start[SG_DIFFCORE__DOC_NDX_LATEST], pDiff->length[SG_DIFFCORE__DOC_NDX_LATEST],
                    pDiff->pResolvedDiff)  );
            }
            break;
        default:
            break;
        }

        if(output_function!=NULL)
        {
            SG_ERR_CHECK(  output_function(
                pCtx,
                pOutputBaton,
                pDiff->start[SG_DIFFCORE__DOC_NDX_ORIGINAL], pDiff->length[SG_DIFFCORE__DOC_NDX_ORIGINAL],
                pDiff->start[SG_DIFFCORE__DOC_NDX_MODIFIED], pDiff->length[SG_DIFFCORE__DOC_NDX_MODIFIED],
                pDiff->start[SG_DIFFCORE__DOC_NDX_LATEST], pDiff->length[SG_DIFFCORE__DOC_NDX_LATEST])  );
        }

        pDiff = pDiff->pNext;
    }

    return;
fail:
    ;
}


//////////////////////////////////////////////////////////////////
// Diffs on files
//////////////////////////////////////////////////////////////////

void SG_diff_file(
    SG_context * pCtx,
    const SG_pathname * pPathnameOriginal, const SG_pathname * pPathnameModified,
    SG_diff_options options,
    SG_diff_t ** ppDiff)
{
    _sg_diff__file_baton_t baton;
    const SG_diff_functions * pVtable;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(ppDiff);

    SG_zero(baton);
    baton.path[0] = pPathnameOriginal;
    baton.path[1] = pPathnameModified;
    baton.options = options;

    pVtable = _sg_diff__selectDiffFnsVTable(options);

    SG_ERR_CHECK(  SG_diff(pCtx, pVtable, &baton, ppDiff)  );

    return;
fail:
    ;
}

void SG_diff3_file(
    SG_context * pCtx,
    const SG_pathname * pPathnameOriginal, const SG_pathname * pPathnameModified, const SG_pathname * pPathnameLatest,
    SG_diff_options options,
    SG_diff_t ** ppDiff)
{
    _sg_diff__file_baton_t baton;
    const SG_diff_functions * pVtable;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(ppDiff);

    SG_zero(baton);
    baton.path[0] = pPathnameOriginal;
    baton.path[1] = pPathnameModified;
    baton.path[2] = pPathnameLatest;
    baton.options = options;

    pVtable  = _sg_diff__selectDiffFnsVTable(options);

    SG_ERR_CHECK(  SG_diff3(pCtx, pVtable, &baton, ppDiff));

    return;
fail:
    ;
}

void SG_diff_file_output_unified(
    SG_context * pCtx,
    SG_diff_t * pDiff,
    const SG_pathname * pPathnameOriginal, const SG_pathname * pPathnameModified,
    const char * szHeaderOriginal, const char * szHeaderModified,
    SG_diff_options options,
    int context,
    SG_file * pOutputFile)
{
    SG_diff_output_functions output_functions = {
        NULL, // output_common
        _sg_diff__file_output_unified_diff_modified,
        NULL, // output_diff_latest
        NULL, // output_diff_common
        NULL}; // output_conflict
    _sg_diff__file_output_baton_t baton;

    int i;
    char defaultHeaderOriginal[512];
    char defaultHeaderModified[512];

    SG_zero(baton);

    if(SG_diff_contains_diffs(pDiff))
    {
        baton.pOutputFile = pOutputFile;
        baton.path[0] = pPathnameOriginal;
        baton.path[1] = pPathnameModified;
        SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &baton.pHunk)  );
        baton.options = options;

        baton.context = ((context >= 0) ? context : SG_DIFF__UNIFIED_CONTEXT_SIZE); // if negative, use default

        for (i = 0; i < 2; i++)
            if(baton.path[i]!=NULL)
                SG_ERR_CHECK(  SG_file__open__pathname(pCtx, baton.path[i], SG_FILE_OPEN_EXISTING|SG_FILE_RDONLY, SG_FSOBJ_PERMS__UNUSED, &baton.file[i]) );

        if (szHeaderOriginal == NULL)
        {
            SG_ERR_CHECK(  _sg_diff__file_output_unified_default_hdr(pCtx, pPathnameOriginal, defaultHeaderOriginal, sizeof(defaultHeaderOriginal))  );
            szHeaderOriginal = defaultHeaderOriginal;
        }

        if (szHeaderModified == NULL)
        {
            SG_ERR_CHECK(  _sg_diff__file_output_unified_default_hdr(pCtx, pPathnameModified, defaultHeaderModified, sizeof(defaultHeaderModified))  );
            szHeaderModified = defaultHeaderModified;
        }

        SG_ERR_CHECK(  SG_file__write__format(pCtx, pOutputFile,
            "--- %s" SG_PLATFORM_NATIVE_EOL_STR
            "+++ %s" SG_PLATFORM_NATIVE_EOL_STR,
            szHeaderOriginal, szHeaderModified) );
        SG_ERR_CHECK(  SG_diff_output(pCtx, &output_functions, &baton, pDiff) );
        SG_ERR_CHECK(  _sg_diff__file_output_unified_flush_hunk(pCtx, &baton) );

        for (i = 0; i < 2; i++)
        { 
            SG_file__close(pCtx, &baton.file[i]);
            if(SG_context__has_err(pCtx))
                SG_ERR_RETHROW2(  (pCtx, "failed to close file '%s'.", baton.path[i])  );
        }

        SG_STRING_NULLFREE(pCtx, baton.pHunk);
    }

    return;
fail:
    SG_STRING_NULLFREE(pCtx, baton.pHunk);
    for (i = 0; i < 2; i++)
        SG_FILE_NULLCLOSE(pCtx, baton.file[i]);
}

void SG_diff_file_output_normal(
    SG_context * pCtx,
    SG_diff_t * pDiff,
    const SG_pathname * pPathnameOriginal, const SG_pathname * pPathnameModified,
    SG_diff_options options,
    SG_file * pOutputFile)
{
    SG_ASSERT(pCtx!=NULL);

    SG_UNUSED(pDiff);
    SG_UNUSED(pPathnameOriginal);
    SG_UNUSED(pPathnameModified);
    SG_UNUSED(options);
    SG_UNUSED(pOutputFile);
    SG_ERR_THROW_RETURN(SG_ERR_NOTIMPLEMENTED);
}

void SG_diff3_file_output(
    SG_context * pCtx,
    SG_diff_t * pDiff,
    const SG_pathname * pPathnameOriginal, const SG_pathname * pPathnameModified, const SG_pathname * pPathnameLatest,
    const char * szConflictOriginal, const char * szConflictModified, const char * szConflictLatest,
    const char * szConflictSeparator,
    SG_bool displayOriginalInConflict, SG_bool displayResolvedConflicts,
    SG_file *pOutputFile)
{
    _sg_diff3__file_output_baton_t baton;
    SG_string * pDefaultConflictOriginal = NULL;
    SG_string * pDefaultConflictModified = NULL;
    SG_string * pDefaultConflictLatest = NULL;
    SG_file * pFile = NULL;
    SG_byte * pFileContents = NULL;
    int idx;

//#if APR_HAS_MMAP
//    apr_mmap_t *mm[3] = { 0 };
//#endif

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pPathnameOriginal);
    SG_NULLARGCHECK_RETURN(pPathnameModified);
    SG_NULLARGCHECK_RETURN(pPathnameLatest);

    SG_zero(baton);

    baton.pOutputFile = pOutputFile;
    baton.path[0] = pPathnameOriginal;
    baton.path[1] = pPathnameModified;
    baton.path[2] = pPathnameLatest;

    if(szConflictOriginal!=NULL)
        baton.szConflictOriginal = szConflictOriginal;
    else
    {
        SG_ERR_CHECK(  SG_string__alloc__format(pCtx, &pDefaultConflictOriginal, "||||||| %s", SG_pathname__sz(pPathnameOriginal))  );
        baton.szConflictOriginal = SG_string__sz(pDefaultConflictOriginal);
    }

    if(szConflictModified!=NULL)
        baton.szConflictModified = szConflictModified;
    else
    {
        SG_ERR_CHECK(  SG_string__alloc__format(pCtx, &pDefaultConflictModified, "<<<<<<< %s", SG_pathname__sz(pPathnameModified))  );
        baton.szConflictModified = SG_string__sz(pDefaultConflictModified);
    }

    if(szConflictLatest!=NULL)
        baton.szConflictLatest = szConflictLatest;
    else
    {
        SG_ERR_CHECK(  SG_string__alloc__format(pCtx, &pDefaultConflictLatest, ">>>>>>> %s", SG_pathname__sz(pPathnameLatest))  );
        baton.szConflictLatest = SG_string__sz(pDefaultConflictLatest);
    }

    baton.szConflictSeparator = szConflictSeparator ? szConflictSeparator : "=======";

    baton.displayOriginalInConflict = displayOriginalInConflict;
    baton.displayResolvedConflicts = displayResolvedConflicts && !displayOriginalInConflict;

    for(idx=0;idx<3;++idx)
    {
        SG_fsobj_stat finfo;

        SG_ERR_CHECK( SG_fsobj__stat__pathname(pCtx, baton.path[idx], &finfo)  );
        if(finfo.size!=0)
        {
            SG_ERR_CHECK(  SG_file__open__pathname(pCtx, baton.path[idx], SG_FILE_OPEN_EXISTING|SG_FILE_RDONLY, SG_FSOBJ_PERMS__UNUSED, &pFile)  );

            //todo: don't store the whole file in memory all at once
            if(finfo.size>SG_UINT32_MAX)
                SG_ERR_THROW2(SG_ERR_LIMIT_EXCEEDED, (pCtx, "File '%s' is too large to diff.", baton.path[idx]));
            SG_ERR_CHECK(  SG_allocN(pCtx, (SG_uint32)finfo.size, pFileContents)  );
            SG_ERR_CHECK(  SG_file__read(pCtx, pFile,(SG_uint32)finfo.size, pFileContents, NULL)  );
            SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

            SG_ERR_CHECK(  SG_utf8__import_buffer(pCtx,
                pFileContents, (SG_uint32)finfo.size,
                &baton.buffer[idx],
                &baton.encoding[idx])  );

            baton.curp[idx] = baton.buffer[idx];
            baton.endp[idx] = baton.buffer[idx] + finfo.size;

            SG_NULLFREE(pCtx, pFileContents);
        }
    }

    SG_ERR_CHECK(  SG_diff_output(pCtx, &_sg_diff3__file_output_vtable, &baton, pDiff)  );

    for(idx=0;idx<3;++idx)
    {
        SG_NULLFREE(pCtx, baton.buffer[idx]);
        SG_STRING_NULLFREE(pCtx, baton.encoding[idx]);
    }

    SG_STRING_NULLFREE(pCtx, pDefaultConflictOriginal);
    SG_STRING_NULLFREE(pCtx, pDefaultConflictModified);
    SG_STRING_NULLFREE(pCtx, pDefaultConflictLatest);

    return;
fail:
    SG_FILE_NULLCLOSE(pCtx, pFile);
    SG_NULLFREE(pCtx, pFileContents);

    for(idx=0;idx<3;++idx)
    {
        SG_NULLFREE(pCtx, baton.buffer[idx]);
        SG_STRING_NULLFREE(pCtx, baton.encoding[idx]);
    }

    SG_STRING_NULLFREE(pCtx, pDefaultConflictOriginal);
    SG_STRING_NULLFREE(pCtx, pDefaultConflictModified);
    SG_STRING_NULLFREE(pCtx, pDefaultConflictLatest);
}


//////////////////////////////////////////////////////////////////

