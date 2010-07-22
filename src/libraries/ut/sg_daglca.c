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
 * @file sg_daglca.c
 *
 * NOTE: the generation is now a SG_int32 rather than a SG_int64.
 *
 *================================================================
 *
 * TODO verify that we handle HID case properly.  that is, if someone
 * TODO does an __add_leaf("aaaa...") and __add_leaf("AAAA...") will we
 * TODO catch that they are the same?
 *
 *================================================================
 *
 * TODO see if we want to change the work-queue key to <-gen>.<static++>
 *
 *================================================================
 *
 * TODO think about whether it would be helpful for us to include
 * TODO info on criss-cross-peers when we iterate over SPCAs as we
 * TODO return the results.
 *
 *        NOTE: Here's how to do this.  The problem should be restated
 *        to say which nodes have multiple significant ancestors; that
 *        is significant ancestors that are effectively peers from the
 *        point of view of any given node.
 *
 *        Add a bitvector for immediate-ancestors and a counter to
 *        count the number of immediate-ancestors.
 *
 *        After the computation is completed and _fixup_result_map()
 *        has been called.  We then visit each significant node (either
 *        using the aData array or the sorted-result map) and OR our
 *        bit into each descendant's ancestor bitvector and increment
 *        the count.
 *
 *        When finished, nodes which have more than one ancestor bit
 *        (or ancestor-count > 1) are the result of some form of
 *        criss-cross.
 *
 *================================================================
 *
 * @details Routine/Class to determine the DAGNODE which is the
 * Least Common Ancestor (LCA) on a given set of DAGNODEs in a DAG.
 *
 * Our callers need the LCA to allow them to perform a MERGE operation.
 * For our purposes here, we describe this operation as:
 *
 * M := MERGE(dag, Am, {L0,L1,...,Ln-1})
 *
 * That is, a new dagnode M is the result of "merging" n leaf dagnodes
 * (named L0, L1, thru Ln-1) and some yet-to-be-determined LCA Am which
 * is defined as:
 *
 * Am := LCA(dag, {L0,L1,...,Ln-1})
 *
 * In the simple case, this LCA is unique and can then be used as the
 * ancestor in a traditional 3-way merge using a tool like DiffMerge.
 * However, this summary over-simplifies the problem.
 *
 * The literature talks about this problem as finding the LCA of 2
 * nodes.  We want to solve it for n nodes and we want to be aware of
 * the criss-cross merge problem.  (http://revctrl.org/CrissCrossMerge)
 *
 *================================================================
 *
 * Let me define a "Partial" Common Ancestor (PCA) as a common ancestor
 * of 2 or more leaves (a subset of the complete set of input leaves).
 *
 * Let me define a "Significant" Partial Common Ancestor (SPCA) as:
 * [SPCA1] one of (possibly many) PCAs where all of the leaves in the
 *         subset came together for the first time.  That is, the branch
 *         point that is "closest" to the leaves in question.
 * [SPCA2] or one where "peer" SPCAs and/or leaves came together for the
 *         first time.  (needed for criss-cross)
 *
 * For example, consider:
 *
 *                     A
 *                    / \.
 *                   /   \.
 *                  /     \.
 *                 /       \.
 *                /         \.
 *               /           \.
 *              B             C
 *             / \           / \.
 *            /   \         /   \.
 *           /     D       E     F
 *          /       \     /      /\.
 *         /         \   /      /  \.
 *        /           \ /      /    \.
 *       G             H      I      J
 *        \           / \     |\    /|
 *         \         /   \    | \  / |
 *          \       /     \   |  \/  |
 *           \     /       \  |  /\  |
 *            \   /         \ | /  \ |
 *             \ /           \|/    \|
 *              L0            L1    L2
 *
 *                 Figure 1.
 *
 * [I use "\." in the figure rather than a plain backslash to keep
 * the compiler from complaining.]
 *
 *    SPCA(dag, {L0,L1}) --> {H}
 *     LCA(dag, {L0,L1}) --> {H}
 * nonSPCA(dag, {L0,L1}) --> {A,B,C,D,E}.
 *
 *    SPCA(dag, {L1,L2}) --> {I,J, F*}
 *     LCA(dag, {L1,L2}) --> {F}
 * nonSPCA(dag, {L1,L2}) --> {A,C}.
 *
 *    SPCA(dag, {L0,L1,L2}) --> {C,F,H,I,J}
 *     LCA(dag, {L0,L1,L2}) --> {C}
 * nonSPCA(dag, {L0,L1,L2}) --> {A}.
 *
 * *F is special because of criss-cross merge.  This is somewhat like
 * a recursive SPCA(dag, {I,J}) --> {F}.
 *
 * When the SPCA of a set of leaves contains more than one item, it
 * implies that there is some type of criss-cross peering.
 *
 * The absolute LCA is defined as the single SPCA over the entire
 * set of input leaves.
 *
 * The set of input leaves and SPCAs define the "essense" and/or
 * topology of the dag (FROM THE POINT OF VIEW OF THOSE LEAVES).
 * Other dagnodes may be elided from the dag.  For example, in the
 * above graph we may have 1000 commits/branches/merges between G
 * and L0 and as long as there are no edges from them to anything
 * else in the dag, we can ignore them here.
 *
 *================================================================
 *
 * Let me define a "Significant" Node (SN) as a leaf or one of
 * these SPCAs.
 *
 * For the purposes of discussion, we assign each SN an ItemIndex
 * as we find it.  Leaves are given index values 0..n-1, SPCAs
 * are assigned values starting with n.  The root-most SPCA will
 * be the LCA.
 *
 * We use the ItemIndex as subscripts into 3 BitVectors:
 *
 * [a] bvLeaf - for Leaf node k (Lk), it contains {k}; for non-leaf
 *              nodes it is empty.
 *
 * [b] bvImmediateDescendants - for any given node this contains the
 *              indexes of the SNs that are immediately below it.
 *              When we distill the dag down to its essense (and
 *              get rid of irrelevant nodes), these nodes are
 *              immediate children of this node.
 *
 * [c] bvClosure - for any given node this contains the indexes of
 *              ALL SNs that are below this node.
 *
 * We can use this information if we want to build a new
 * DAG-LCA-GRAPH that contains only the significant nodes.
 * In this graph, links would go downward from the root/LCA
 * to the leaves.  This graph would capture the *ESSENTIAL* structure
 * of the full dag FROM THE POINT OF VIEW OF THE SET OF LEAVES.
 *
 *================================================================
 *
 * [1] In the 2-way case, we have 2 different graph topologies that we
 *     need to address:
 *
 *     [1a] 2-Way-Normal
 *
 *                  A
 *                 / \.
 *               [X] [Y]
 *               /     \.
 *             L0       L1
 *
 *          Let [X] and [Y] be an arbitrary graph fragments that do
 *          not intersect.  So, from the point of view of {L0,L1}, we
 *          can reduce this to:
 *
 *                  A
 *                 / \.
 *               L0   L1
 *
 *          So, in this case the value is:
 *
 *          SPCA(dag, {L0,L1}) --> {A}
 *           LCA(dag, {L0,L1}) --> {A}.
 *
 *     [1b] Criss-Cross
 *
 *          [I've eliminated the [Bx] and [Cy] that would really clutter
 *          up this graph, but they are there and must be addressed.]
 *
 *                  A
 *                 / \.
 *               B0   C0
 *                |\ /|
 *                | X |
 *                |/ \|
 *               B1   C1
 *                |\ /|
 *                | X |
 *                |/ \|
 *               B2   C2
 *                |\ /|
 *                | X |
 *                |/ \|
 *               L0   L1
 *
 *          In this case L0 and L1 have 2 common ancestors B2 and C2.
 *          They have 2 common ancestors B1 and C1 and so on.  B0 and C0
 *          have common ancestor A.  So the value of LCA(dag, {L0,L1})
 *          ***depends upon what you want it to be***.
 *
 *          The problem is that the typical auto-merge code ***may*** generate
 *          different results for
 *                 MERGE(dag, B2, {L0,L1})
 *             and MERGE(dag, C2, {L0,L1}).
 *
 *          And if we auto-merge with MERGE(dag, A, {L0,L1}), the user ***may***
 *          have to re-resolve any conflicts that they already addressed when
 *          they created B1, C1, B2, or C2.
 *
 *          So using the (somewhat recursive) definition:
 *
 *          SPCA(dag, {L0,L1}) --> {B2,C2, B1*,C1*, B0**,C0**, A***}.
 *           LCA(dag, {L0,L1}) --> {A}.
 *
 *     [1c] Extra branches.
 *
 *          Consider the following case:
 *
 *                   A0
 *                  / \.
 *                 /   \.
 *                /     \.
 *               /       \.
 *              /         \.
 *             B           C
 *            / \         / \.
 *           /   \       /   \.
 *          /     \     /     \.
 *         /       \   /       \.
 *        /         \ /         \.
 *      [X0]         A1         [Y0]
 *        \         / \         /
 *         \       /   \       /
 *          \   [X1]   [Y1]   /
 *           \   /       \   /
 *            \ /         \ /
 *             L0          L1
 *
 *          CLAIM: The LCA of {L0,L1} is {A1}.
 *
 *                 One could argue that we should consider B and C as peer
 *                 SPCAs of {L0,L1} because of the effects of [X0] and [Y0],
 *                 respectively.  And therefore, the LCA would be {A0}.
 *
 *                 But I claim that the changes in [X0] and [Y0] are just
 *                 noise in this graph and could be considered as if they were
 *                 part of [X1] and [Y1], respectively.
 *
 *                 That is, from an LCA-perspective we could think of this as:
 *
 *                   A0
 *                  / \.
 *                 /   \.
 *                /     \.
 *               B       C
 *                \     /
 *                 \   /
 *                  \ /
 *                   A1
 *                  / \.
 *                 /   \.
 *               [X]   [Y]
 *               /       \.
 *              /         \.
 *             L0          L1
 *
 *                 This makes B and C non-significant (nonSPCAs) rather than
 *                 SPCAs.
 *
 *          TODO: Think about this with some actual test data.
 *
 *     [1d] A leaf which is an ancestor of the other leaf.
 *
 *          Consider the following case:
 *
 *                  A
 *                  |
 *                  L0
 *                  |
 *                  L1
 *
 *          One could argue that we should just return {L0} as the LCA.
 *          But that might cause the merge code to force the user thru
 *          the trivial merge:
 *
 *               MERGE(dag, L0, {L0,L1}) --> L1
 *
 *          I think it would be better to stop the music as soon as we
 *          detect this condition.  (It is kind of a coin-toss for the
 *          2-way case, but adds lots of noise to the n-way case.)
 *
 * [2] In the n-way case, there are several things to consider:
 *
 *     [2a] Does the n-way merge produce one result dagnode M or a series
 *          of intermediate merges and one final merge?  For example:
 *
 *                  A
 *                 / \.
 *                B   C
 *               / \   \.
 *             L0   L1  L2
 *               \  |  /
 *                \ | /
 *                 \|/
 *                  M
 *
 *          versus:
 *
 *                  A
 *                 / \.
 *                B   C
 *               / \   \.
 *             L0   L1  L2
 *               \ /   /
 *                m0  /
 *                 \ /
 *                  M
 *
 *          I believe that GIT does the former with its OCTOPUS merge.
 *          I know that Monotone does the latter.
 *
 *          We don't care how the caller does the merge and whether or
 *          not it generates only an M dagnode or both M and m0 dagnodes
 *          or whether it does a combination of the 2 using tempfiles to
 *          simulate m0 and only generate M.
 *
 *          Our only concern here is that we provide enough information
 *          for the caller to decide the "best possible" merge.  It is
 *          free to use or ignore any SPCAs that we may provide.
 *
 *     [2b] Do we report only the "absolute" LCA or also SPCAs?
 *
 *          It is possible that the "correct" answer depends upon what the
 *          caller wants to try to do.  In the graph in [2a], if we have a
 *          bunch of version controlled files and some have been modified
 *          in L0 and L1 and some in L0 and L2, we may want to show the user
 *          several invocations of DiffMerge using B as the SPCA for some
 *          files and A as the LCA for the other files.  For files that were
 *          modified in L0, L1, and L2, we may want to show 2 invocations of
 *          DiffMerge in series first with B and then with A.  Even that may
 *          depend on whether the changes within the files are orthogonal.
 *
 *          So, we have:
 *
 *          SPCA(dag, {L0,L1,L2}) --> {A,B}
 *           LCA(dag, {L0,L1,L2}) --> {A}.
 *
 *     [2c] Sorting SPCAs.
 *
 *          When we do present SPCAs, we should give an ordering so
 *          that the caller can sanely present them to the user.  For
 *          example:
 *
 *                  A
 *                 / \.
 *                /   \.
 *               /     \.
 *              /       \.
 *             B         C
 *            / \       / \.
 *          L0   L1   L2   L3
 *
 *          The caller will probably want to do this as:
 *
 *               MERGE(dag, B, {L0,L1}) --> m0
 *               MERGE(dag, C, {L2,L3}) --> m1
 *               MERGE(dag, A, {m0,m1}) --> M
 *
 *          rather than:
 *
 *               MERGE(dag, A, {L0,L1}) --> m0
 *               MERGE(dag, A, {m0,L2}) --> m1
 *               MERGE(dag, A, {m1,L3}) --> M
 *
 *          or some other permutation.
 *
 *     [2d] n-way with Criss-Crosses
 *
 *          Things get really funky when we have a criss-cross and
 *          extra leaves threading into the mess.
 *
 *                  A
 *                 / \.
 *               B0   C0
 *                |\ /|\.
 *                | X | \.
 *                |/ \|  \.
 *               B1   C1  L3
 *                |\ /|\.
 *                | X | \.
 *                |/ \|  \.
 *               B2   C2  L2
 *                |\ /|
 *                | X |
 *                |/ \|
 *               L0   L1
 *
 *          TODO describe what we want to have happen here.
 *
 *     [2e] another form of criss-cross
 *
 *          things also get a little confusing when we have
 *          something like this where L1 has multiple significant
 *          peer ancestors and we want to merge {L0,L1,L2} in one
 *          step.
 *
 *                     A
 *                    / \.
 *                   /   \.
 *                  /     \.
 *                 /       \.
 *                /         \.
 *               /           \.
 *              B             C
 *             / \           / \.
 *            /   \         /   \.
 *           /     D       E     F
 *          /       \     /       \.
 *         /         \   /         \.
 *        /           \ /           \.
 *       G             H             J
 *        \           / \           / \.
 *         \         /   \         /   \.
 *          \       /     \       /     \.
 *           \     /       \     /       \.
 *            \   /         \   /         \.
 *             \ /           \ /           \.
 *              L0            L1            L2
 *
 *
 * [3] Multiple initial checkins.
 *
 *     As discussed in sg_dag_sqlite3.c, we have to allow for multiple
 *     initial checkins.  This causes us to have (potentially) a forest
 *     of non-connected dags -or- (potentially) a (somewhat) connected
 *     dag with an "implied" null root.
 *
 *     A merge across disconnected pieces would make the null root the
 *     LCA.
 *
 *                A0     A1
 *               / \     |
 *              B   C    X
 *             / \       |
 *            L0 L1      L2
 *
 *     LCA(dag, {L0,L1})    --> {B}
 *     LCA(dag, {L0,L1,L2}) --> {NULLROOT}
 *
 *
 *     Likewise, with a connected dag:
 *
 *                A0     A1
 *               / \     |
 *              B   C    X
 *             / \   \  / \.
 *            L0 L1   L2   L3
 *
 *     LCA(dag, {L0,L1})       --> {B}
 *     LCA(dag, {L0,L1,L2})    --> {A0}
 *     LCA(dag, {L2,L3})       --> {X}
 *     LCA(dag, {L0,L1,L2,L3}) --> {NULLROOT}
 *
 *     9/21/09 UPDATE: I don't think we have to worry about this case
 *                     on properly created/initialized REPOs because we
 *                     now create a trivial/initial checkin as soon as
 *                     the REPO is created.  In some of our unittests
 *                     before vv was available (where we created REPOs
 *                     and CSETs in separate steps), we were able to exploit
 *                     this.
 *
 *                     TODO go thru and remove some of the asserts and
 *                     TODO stuff that deals with nullroots (probably have
 *                     TODO to fix some unit tests first).
 *
 * TODO to be continued...
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

/**
 * For simplicity we use a SG_uint64 for the bit-vector
 * because this will fit in a register and let us do
 * operations on it quickly.
 *
 * THIS IMPLIES that the we can have a maximum of 64
 * leaves and SPCAs.  This shouldn't be a problem unless
 * they have a long criss-cross chain.
 *
 * THIS IMPLIES that they can't go crazy and do a 100-way
 * merge.
 *
 * Because an n-way merge may produce, for example, n-1
 * SPCAs and 1 absolute LCA, this might limit them to doing
 * 32-way merges in practice.  If this turns out to be a
 * problem, we can always substitute a real unlimited
 * bit-vector and remove the limit....  TODO ?really?
 */

typedef SG_uint64 SG_daglca_bitvector;

#define SG_DAGLCA_BIT_VECTOR_LENGTH 64		// (sizeof(SG_daglca_bitvector)*8)

//////////////////////////////////////////////////////////////////

/**
 * Instance data to associate with the dagnodes that we have
 * fetched from disk as we walk the dag.
 */
struct _my_data
{
	SG_dagnode *			pDagnode;
	SG_int32				genDagnode;

	// the ItemIndex of this dagnode if it is a Significant Node.
	// Otherwise, -1.

	SG_int32				itemIndex;
	SG_daglca_node_type		nodeType;

	// if this node is a leaf, bvLeaf contains a single 1 bit
	// that corresponds to the itemIndex.  that is, bvLeaf[itemIndex] is 1.
	// it also means that daglca.aData_SignificantNodes[itemIndex] contains
	// this node.

	SG_daglca_bitvector		bvLeaf;

	// a bitvector of the significant nodes that are the immediate descendants
	// of this node.  (this can be converted into child-links in the lca
	// essense graph.)

	SG_daglca_bitvector		bvImmediateDescendants;

	// a bitvector of all significant nodes that are descendants of this node
	// (and this node itself).

	SG_daglca_bitvector		bvClosure;
};

typedef struct _my_data my_data;

/**
 * SG_daglca is a structure that we use to collection information
 * on a portion of the dag and compute common ancestor(s).  This is
 * our workspace as we compute.
 */
struct _SG_daglca
{
	// a callback to do the work of actually fetching a dagnode from disk.

	FN__sg_fetch_dagnode *	pFnFetchDagnode;
	void *					pVoidFetchDagnodeData;

	SG_uint32				nrLeafNodes;
	SG_uint32				nrSignificantNodes;
	SG_bool					bFrozen;

	// TODO Jeff says 3/4/10: do we acutally use the iDagNum for anything?
	// TODO                   i think we can get rid of it and remove it
	// TODO                   from the arglist of __alloc().
    SG_uint32               iDagNum;

	// pRB_FetchedCache is a cache of all of the fetched nodes.
	// It owns the associated my_data structures (which in turn,
	// own all the individual dagnodes).
	//
	// The key is the HID of the dagnode.
	//
	// Think of this as map< dn.hid, my_data * >

	SG_rbtree *				pRB_FetchedCache;

	// pRB_SortedWorkQueue is an ordered to-do list.  It is a
	// sorted list of nodes that we should visit.
	//
	// It ***DOES NOT*** own the associated data pointers;
	// it borrows them from pRB_Fetched.
	//
	// The key is a combination of the generation and HID
	// of the dagnode, such that the deepest nodes in the
	// DAG are listed first.
	//
	// Note: we don't need the second part of the key to be
	// the HID, it could be a static monotonically increasing
	// sequence number.  (anything to give us a unique value.)
	//
	// Think of this as map< (-dn.generation,dn.hid), my_data * >

	SG_rbtree *				pRB_SortedWorkQueue;

	// pRB_SortedResults is a sorted list of the resulting
	// Significant Nodes.  It sorted by generation so that the
	// absolute LCA should be first.   SPCAs and Leaves will
	// follow but may be interleaved depending on the individual
	// depths in the tree.  Nodes at the same depth will be in
	// random order.
	//
	// It ***DOES NOT*** own the associated data pointers;
	// it borrows them from pRB_Fetched.
	//
	// The key is a combination of the generation and HID
	// of the dagnode, such that the shallowest nodes in the
	// DAG are listed first.
	//
	// Think of this as map< (+dn.generation,dn.hid), my_data * >

	SG_rbtree *				pRB_SortedResultMap;

	// a fixed-length vector containing only Significant Nodes.
	//
	// we do not own the my_data structures; they are borrowed
	// from pRB_Fetched.
	//
	// We use this as a map from an itemIndex to a pointer.
	//
	// Nodes are added to this list as we identify that they
	// are significant.  This matches the bitvector ordering.
	// So aData[0] corresponds to bit[0].
	//
	// This ordering is of *NO* use to the caller.

	my_data *				aData_SignificantNodes[SG_DAGLCA_BIT_VECTOR_LENGTH];
};

//////////////////////////////////////////////////////////////////

static void _my_data__free(SG_context * pCtx, my_data * pData)
{
	if (!pData)
		return;

	SG_DAGNODE_NULLFREE(pCtx, pData->pDagnode);
	SG_NULLFREE(pCtx, pData);
}
static void _my_data__free__cb(SG_context * pCtx, void * pVoid)
{
	_my_data__free(pCtx, (my_data *)pVoid);
}

//////////////////////////////////////////////////////////////////

/**
 * Allocate a my_data structure for this dagnode.  Add it to the
 * fetched-cache.  if dagnode is null we assume the implied null
 * root node.
 *
 * The fetched-cache takes ownership of both the dagnode and the
 * my_data structure.
 *
 * We only populate the basic fields in my_data; WE DO NOT SET
 * ANY OF THE BIT-VECTOR OR ITEM-INDEX FIELDS.
 */
static void _cache__add(SG_context * pCtx,
						SG_daglca * pDagLca,
						SG_dagnode * pDagnode,	// if we are successful, you no longer own this.
						my_data ** ppData)		// you do not own this.
{
	const char * szHid;
	my_data * pDataAllocated = NULL;
	my_data * pDataInstalled = NULL;
	my_data * pOldData = NULL;

	SG_NULLARGCHECK_RETURN(pDagLca);
	SG_NULLARGCHECK_RETURN(ppData);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pDataAllocated)  );

	pDataAllocated->itemIndex = -1;
	pDataAllocated->nodeType = SG_DAGLCA_NODE_TYPE__NOBODY;

	pDataAllocated->bvLeaf = 0;
	pDataAllocated->bvImmediateDescendants = 0;
	pDataAllocated->bvClosure = 0;

	if (pDagnode)
	{
		SG_ERR_CHECK(  SG_dagnode__get_generation(pCtx,pDagnode,&pDataAllocated->genDagnode)  );
		SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx,pDagnode,&szHid)  );
	}
	else
	{
		// we allow a null dagnode to represent the implied null root
		// which is the implied parent of initial checkins.

		pDataAllocated->genDagnode = 0;

		szHid = "*root*";
	}

	SG_ERR_CHECK(  SG_rbtree__update__with_assoc(pCtx,
												 pDagLca->pRB_FetchedCache,szHid,
												 pDataAllocated,(void **)&pOldData)  );
	pDataInstalled = pDataAllocated;
	pDataAllocated = NULL;

	pDataInstalled->pDagnode = pDagnode;

	// fetched-cache now owns pData and pDagnode.

	SG_ASSERT_RELEASE_RETURN2(  (!pOldData),
						(pCtx,"Possible memory leak adding [%s] to daglca cache.",szHid)  );

	*ppData = pDataInstalled;
	return;

fail:
	SG_ERR_IGNORE(  _my_data__free(pCtx, pDataAllocated)  );
}

static void _cache__lookup(SG_context * pCtx,
						   const SG_daglca * pDagLca, const char * szHid,
						   SG_bool * pbAlreadyPresent,
						   my_data ** ppData)	// you do not own this; don't free it.
{
	// see if szHid is present in cache.  return the associated my_data structure.
	// if szHid is null, we look for the implied null root.

	SG_bool bFound;
	my_data * pData = NULL;

	SG_NULLARGCHECK_RETURN(pDagLca);
	SG_NULLARGCHECK_RETURN(ppData);
	SG_NULLARGCHECK_RETURN(pbAlreadyPresent);

	if (szHid)
		SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx, pDagLca->pRB_FetchedCache,szHid,&bFound,(void **)&pData)  );
	else
		SG_ERR_CHECK_RETURN(  SG_rbtree__find(pCtx, pDagLca->pRB_FetchedCache,"*root*",&bFound,(void **)&pData)  );

	*pbAlreadyPresent = bFound;
	*ppData = ((bFound) ? pData : NULL);
}

//////////////////////////////////////////////////////////////////

static void _work_queue__insert(SG_context * pCtx,
								SG_daglca * pDagLca, my_data * pData)
{
	// insert an entry into sorted work-queue for the my_data structure
	// associated with a dagnode.
	//
	// the work-queue contains a list of the dagnodes that we need
	// to examine.  we seed it with the set of n leaves.  it then
	// evolves as we walk the parent links in each node.  it maintains
	// the "active frontier" of the dag that we are exploring.
	//
	// to avoid lots of O(n^2) and worse problems, we maintain the
	// queue sorted by dagnode depth/generation in the dag.  this
	// allows us to visit nodes in an efficient order.
	//
	// the sort key looks like <-generation>.<hid> "%08x.%s"
	//
	// Note: we don't need the second part of the key to be
	// the HID, it could be a static monotonically increasing
	// sequence number.  (anything to give us a unique value.)
	// I'm going to use the HID for debugging.

	char bufSortKey[SG_HID_MAX_BUFFER_LENGTH + 20];
	const char * szHid;

	if (pData->pDagnode)
	{
		SG_ERR_CHECK_RETURN(  SG_dagnode__get_id_ref(pCtx,pData->pDagnode,&szHid)  );
		SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,
										 bufSortKey,sizeof(bufSortKey),
										 "%08lx.%s", (-pData->genDagnode), szHid)  );
	}
	else
	{
		// ensure that the null root node gets appended to the end of the list.
		// (because -0 is 0, the above generation trick doesn't work)
		szHid = "*root*";
		SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,
										 bufSortKey,sizeof(bufSortKey),
										 "z.%s", szHid)  );
	}

	// the work-queue ***DOES NOT*** take ownership of pData; it
	// borrows pData from the fetched-cache which owns all pData's.

	SG_ERR_CHECK_RETURN(  SG_rbtree__update__with_assoc(pCtx,
														pDagLca->pRB_SortedWorkQueue,
														bufSortKey,
														pData,NULL)  );
}

static void _work_queue__remove_first(SG_context * pCtx,
									  SG_daglca * pDagLca,
									  SG_bool * pbFound,
									  my_data ** ppData)	// you do not own this
{
	// remove the first element in the work-queue.
	// this should be one of the deepest nodes in the frontier
	// of the dag that we have yet to visit.
	//
	// you don't own the returned pData (because the work-queue
	// borrowed it from the fetched-queue).

	// TODO we should add a SG_rbtree__remove__first__with_assoc().
	// TODO that would save a full lookup.

	const char * szKey;
	my_data * pData;
	SG_bool bFound;

	SG_ERR_CHECK_RETURN(  SG_rbtree__iterator__first(pCtx,
													 NULL,pDagLca->pRB_SortedWorkQueue,
													 &bFound,
													 &szKey,
													 (void**)&pData)  );
	if (bFound)
		SG_ERR_CHECK_RETURN(  SG_rbtree__remove(pCtx,
												pDagLca->pRB_SortedWorkQueue,szKey)  );

	*pbFound = bFound;
	*ppData = ((bFound) ? pData : NULL);
}

//////////////////////////////////////////////////////////////////

static void _result_map__insert(SG_context * pCtx,
								SG_daglca * pDagLca, my_data * pData)
{
	// insert an entry into the sorted-result map for the my_data structure
	// associated with a dagnode.
	//
	// the sorted-result map contains a list of the Significant Nodes
	// that we have identified.  it is ordered by generation so that
	// the absolute LCA should be first when we are finished walking
	// the dag.
	//
	// the sort key looks like <+generation>.<hid> "%08x.%s"
	//
	// Note: we don't need the second part of the key to be
	// the HID, it could be a static monotonically increasing
	// sequence number.  (anything to give us a unique value.)
	// I'm going to use the HID for debugging.

	char bufSortKey[SG_HID_MAX_BUFFER_LENGTH + 20];
	const char * szHid;

	if (pData->pDagnode)
		SG_ERR_CHECK_RETURN(  SG_dagnode__get_id_ref(pCtx,pData->pDagnode,&szHid)  );
	else
		szHid = "*root*";

	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx,
									 bufSortKey,sizeof(bufSortKey),
									 "%08lx.%s", (pData->genDagnode), szHid)  );

	// the sorted-result map ***DOES NOT*** take ownership of pData; it
	// borrows pData from the fetched-cache which owns all pData's.

	SG_ERR_CHECK_RETURN(  SG_rbtree__update__with_assoc(pCtx,
														pDagLca->pRB_SortedResultMap,
														bufSortKey,
														pData,NULL)  );
}

//////////////////////////////////////////////////////////////////

static void _significant_node_vec__insert_leaf(SG_context * pCtx,
											   SG_daglca * pDagLca,
											   my_data * pData,
											   SG_int32 * pItemIndex)
{
	// claim a spot in the significant-node vector for this leaf.

	SG_int32 kItem;

	SG_NULLARGCHECK_RETURN(pDagLca);
	SG_NULLARGCHECK_RETURN(pData);
	SG_NULLARGCHECK_RETURN(pItemIndex);

	SG_ASSERT_RELEASE_RETURN2(  (pData->nodeType==SG_DAGLCA_NODE_TYPE__NOBODY),
						(pCtx,"Cannot insert item twice in daglca.")  );

	// we have a fixed-sized significant-node vector.  if they exceed
	// this, we just give up.  (it's not that i'm too lazy to do a
	// realloc; this is the width of the bit-vectors so we can't address
	// more than this.)

	if (pDagLca->nrSignificantNodes >= SG_DAGLCA_BIT_VECTOR_LENGTH)
		SG_ERR_THROW_RETURN(  SG_ERR_LIMIT_EXCEEDED  );

	kItem = pDagLca->nrSignificantNodes++;
	pDagLca->aData_SignificantNodes[kItem] = pData;	// we borrow this pointer

	SG_ERR_CHECK_RETURN(  _result_map__insert(pCtx,pDagLca,pData)  );

	pData->itemIndex = kItem;
	pData->nodeType = SG_DAGLCA_NODE_TYPE__LEAF;

	pData->bvLeaf = (1ULL << kItem);
	pData->bvImmediateDescendants = 0;
	pData->bvClosure = pData->bvLeaf;

	pDagLca->nrLeafNodes++;

	// since we always put the leaves first in the significant-node vector,
	// these counts should match.

	SG_ASSERT_RELEASE_RETURN(  (pDagLca->nrLeafNodes==pDagLca->nrSignificantNodes)  );

	*pItemIndex = kItem;
}

static void _significant_node_vec__insert_spca(SG_context * pCtx,
											   SG_daglca * pDagLca,
											   my_data * pData,
											   SG_int32 * pItemIndex)
{
	// claim a spot in the significant-node vector for this SPCA.

	SG_int32 kItem;

	SG_NULLARGCHECK_RETURN(pDagLca);
	SG_NULLARGCHECK_RETURN(pData);
	SG_NULLARGCHECK_RETURN(pItemIndex);

	SG_ASSERT_RELEASE_RETURN(  ((pData->nodeType==SG_DAGLCA_NODE_TYPE__NOBODY) && "Cannot insert twice.")  );
	SG_ASSERT_RELEASE_RETURN(  ((pData->bvLeaf==0) && "Leaf node cannot be a parent of another leaf.")  );

	// we have a fixed-sized significant-node vector.  if they exceed
	// this, we just give up.  (it's not that i'm too lazy to do a
	// realloc; this is the width of the bit-vectors so we can't address
	// more than this.)

	if (pDagLca->nrSignificantNodes >= SG_DAGLCA_BIT_VECTOR_LENGTH)
		SG_ERR_THROW_RETURN(  SG_ERR_LIMIT_EXCEEDED  );

	kItem = pDagLca->nrSignificantNodes++;
	pDagLca->aData_SignificantNodes[kItem] = pData;	// we borrow this pointer

	SG_ERR_CHECK_RETURN(  _result_map__insert(pCtx,pDagLca,pData)  );

	pData->itemIndex = kItem;
	pData->nodeType = SG_DAGLCA_NODE_TYPE__SPCA;

	// we don't set "pData->bvImmediateDescendants" because caller must compute it.

	// by definition, we are defining a new significant-node.  the bit value
	// for this is "(1ULL << kItem)".  we OR this into our closure so that it
	// will trickle up.  BTW, this allows us to handle the criss-cross (as
	// shown in Figure 1 and section [1b]) without needing to go recursive.

	pData->bvClosure |= (1ULL << kItem);

	*pItemIndex = kItem;
}

//////////////////////////////////////////////////////////////////

static void _fetch_dagnode_into_cache_and_queue(SG_context * pCtx,
												SG_daglca * pDagLca,
												const char * szHid,
												my_data ** ppData)
{
	// fetch the dagnode from disk, allocating a dagnode.
	// install it into the fetched-cache, allocating a my_data structure.
	// both of these will be owned by the fetched-cache.
	// add it to the frontier of the work-queue.
	//
	// if szHid is null, we assume the implied null root node.

	SG_dagnode * pDagnodeAllocated = NULL;
	my_data * pDataCached = NULL;

	if (szHid)
		SG_ERR_CHECK(  (*pDagLca->pFnFetchDagnode)(pCtx,
												   pDagLca->pVoidFetchDagnodeData,
												   szHid,
												   &pDagnodeAllocated)  );

	SG_ERR_CHECK(  _cache__add(pCtx,pDagLca,pDagnodeAllocated,&pDataCached)  );
	pDagnodeAllocated = NULL;		// fetched-cache takes ownership of dagnode

	SG_ERR_CHECK(  _work_queue__insert(pCtx,pDagLca,pDataCached)  );
	// we never owned pDataCached.

	*ppData = pDataCached;
	return;

fail:
	SG_DAGNODE_NULLFREE(pCtx, pDagnodeAllocated);
}

//////////////////////////////////////////////////////////////////

void SG_daglca__alloc(SG_context * pCtx,
					  SG_daglca ** ppNew,
					  SG_uint32 iDagNum,
					  FN__sg_fetch_dagnode * pFnFetchDagnode,
					  void * pVoidFetchDagnodeData)
{
	SG_daglca * pDagLca = NULL;

	SG_NULLARGCHECK_RETURN(ppNew);
	SG_NULLARGCHECK_RETURN(pFnFetchDagnode);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pDagLca)  );

    pDagLca->iDagNum = iDagNum;
	pDagLca->pFnFetchDagnode = pFnFetchDagnode;
	pDagLca->pVoidFetchDagnodeData = pVoidFetchDagnodeData;

	pDagLca->nrLeafNodes = 0;
	pDagLca->nrSignificantNodes = 0;
	pDagLca->bFrozen = SG_FALSE;

	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx,&pDagLca->pRB_FetchedCache)  );
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx,&pDagLca->pRB_SortedWorkQueue)  );
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx,&pDagLca->pRB_SortedResultMap)  );

	*ppNew = pDagLca;
	return;

fail:
	SG_DAGLCA_NULLFREE(pCtx, pDagLca);
}

void SG_daglca__free(SG_context * pCtx, SG_daglca * pDagLca)
{
	if (!pDagLca)
		return;

	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pDagLca->pRB_FetchedCache, _my_data__free__cb);

	SG_RBTREE_NULLFREE(pCtx, pDagLca->pRB_SortedWorkQueue);
	SG_RBTREE_NULLFREE(pCtx, pDagLca->pRB_SortedResultMap);

	SG_NULLFREE(pCtx, pDagLca);
}

//////////////////////////////////////////////////////////////////

void SG_daglca__add_leaf(SG_context * pCtx, SG_daglca * pDagLca, const char * szHid)
{
	// add a leaf to the set of leaves.

	SG_bool bAlreadyExists = SG_FALSE;
	my_data * pDataFound;
	my_data * pDataCached;
	SG_int32 kIndex;

	SG_NULLARGCHECK_RETURN(pDagLca);
	SG_NONEMPTYCHECK_RETURN(szHid);

	// you cannot add leaves after we start grinding because
	// of the way we compute the SPCAs (and we want the leaves
	// to be in spots 0 .. n-1 in the significant-node vector).

	if (pDagLca->bFrozen)
		SG_ERR_THROW_RETURN(  SG_ERR_INVALID_WHILE_FROZEN  );

	if (pDagLca->nrLeafNodes >= SG_DAGLCA_BIT_VECTOR_LENGTH)
		SG_ERR_THROW_RETURN(  SG_ERR_LIMIT_EXCEEDED  );

	// if they give us the same dagnode twice, we abort.
	// this problem is hard enough without having to worry
	// about duplicate leaves gumming up the works.

	SG_ERR_CHECK_RETURN(  _cache__lookup(pCtx,pDagLca,szHid,&bAlreadyExists,&pDataFound)  );
	if (bAlreadyExists)
		SG_ERR_THROW_RETURN(  SG_ERR_DAGNODE_ALREADY_EXISTS  );

	// fetch the dagnode from disk into the fetched-cache
	// and add it to the work-queue.  we don't own pDataCached
	// (the fetched-cache does).

	SG_ERR_CHECK_RETURN(  _fetch_dagnode_into_cache_and_queue(pCtx,pDagLca,szHid,&pDataCached)  );

	// since this is a leaf, we always insert it into the  significant-node vector
	// and assign it an index.

	SG_ERR_CHECK_RETURN(  _significant_node_vec__insert_leaf(pCtx,pDagLca,pDataCached,&kIndex)  );
}

//////////////////////////////////////////////////////////////////

struct _add_leaves_data
{
	SG_daglca * pDagLca;
};

static SG_rbtree_foreach_callback _sg_daglca__add_leaves_callback;

static void _sg_daglca__add_leaves_callback(SG_context * pCtx,
											const char * szKey,
											SG_UNUSED_PARAM(void * pAssocData),
											void * pVoidAddLeavesData)
{
	struct _add_leaves_data * pAddLeavesData = (struct _add_leaves_data *)pVoidAddLeavesData;

	SG_UNUSED(pAssocData);

	SG_ERR_CHECK_RETURN(  SG_daglca__add_leaf(pCtx,pAddLeavesData->pDagLca,szKey)  );
}

void SG_daglca__add_leaves(SG_context * pCtx,
						   SG_daglca * pDagLca,
						   const SG_rbtree * prbLeaves)
{
	struct _add_leaves_data add_leaves_data;

	SG_NULLARGCHECK_RETURN(pDagLca);
	SG_NULLARGCHECK_RETURN(prbLeaves);

	add_leaves_data.pDagLca = pDagLca;

	SG_ERR_CHECK_RETURN(  SG_rbtree__foreach(pCtx,
											 prbLeaves,
											 _sg_daglca__add_leaves_callback,
											 &add_leaves_data)  );
}

//////////////////////////////////////////////////////////////////

static void _process_parent_closure(SG_context * pCtx,
									SG_daglca * pDagLca,
									my_data * pDataChild,
									my_data * pDataParent)
{
	// decide how to merge the closure bitvectors in the child and parent dagnodes
	// and if necessary add the parent to the significant-node vector.

	SG_int32 kItem;
	SG_daglca_bitvector bvUnion = (pDataChild->bvClosure | pDataParent->bvClosure);

	if (bvUnion == pDataParent->bvClosure)
	{
		// our set of significant descendants is a equal to or a subset of the parent's.
		// this edge adds no new "significant" paths to the parent.   and so, we have
		// no reason to mark the parent as a SPCA.
		//
		//         A
		//          \.
		//           C
		//          / \.
		//         D   E
		//        / \ /
		//       L0  L1
		//
		//       Figure 2.
		//
		// If D-->C is processed first, it sets bits {L0,L1,D} on C.
		// Then when E-->C is processed, it only has bits {L1}, so as
		// described in [1c], we don't mark C.

		return;
	}

	pDataParent->bvClosure = bvUnion;

	if (bvUnion == pDataChild->bvClosure)
	{
		// the parent's set of significant descendants is a proper subset of ours.
		// we have no reason to mark the parent as a SPCA because the child has
		// all the same descendands and is closer to the leaves.
		//
		// in Figure 2, suppose E-->C went first setting bits {L1} on C.
		// then when D-->C is processed, D has more bits set, but this should not
		// cause C to be marked, because D has all the same bits and is closer to
		// L0 and L1.
		//
		// we do update the significant descendants because that needs to trickle up.

		return;
	}

	// the union is larger than either, so we should mark this as a SPCA
	// on the union.  consider L0-->D and then L1-->D in Figure 2.

	if (pDataParent->itemIndex != -1)
	{
		// the parent already is a SPCA on the nodes that it has in its
		// bit-vector.  this can happen when we have more than 2 way branch.
		//
		//         A
		//          \.
		//           C
		//          / \.
		//      ___D   E
		//     /  / \ /
		//    L2 L0  L1
		//
		//       Figure 3.
		//
		// if L0-->D sets bits {L0}, then L1-->D adds {L1} creating an SPCA
		// with {L0,L1}, then when L2-->D is processed, we add {L2} to D.

		return;
	}

	// we must mark the parent as a SPCA and add it to the significant-node
	// vector.   this will allocate a new bit in the bitvector too.
	//
	// so in Figure 3, D will have bits {L0,L1,L2,D} set.
	// [setting the D bit allows us to avoid recursion in the
	// criss-cross case.]

	SG_ERR_CHECK_RETURN(  _significant_node_vec__insert_spca(pCtx,pDagLca,pDataParent,&kItem)  );
}

static void _process_parent(SG_context * pCtx,
							SG_daglca * pDagLca,
							my_data * pDataChild,
							const char * szHidParent)
{
	// analyze the child-->parent relationship and update attributes
	// on the parent.
	//
	// if szHidParent is NULL, we assume the implied null root node.

	SG_bool bFound = SG_FALSE;
	my_data * pDataParent;		// we never own this

	SG_ERR_CHECK(  _cache__lookup(pCtx,pDagLca,szHidParent,&bFound,&pDataParent)  );
	if (!bFound)
	{
		// we have never visited this parent dagnode, so we don't know anything about it.
		// add it to the fetched-cache and the work-queue.
		//
		// let the parent start accumulating the closure on significant descendants by
		// starting with the child's closure.
		//
		// if the child is a significant node, then it is the only immediate descendant for
		// the parent (so far).  if not, we bubble up the child's immediate descendants.
		//
		// we won't know if the parent is a SPCA until it has been referenced by multiple
		// "significant" paths thru the dag.

		SG_ERR_CHECK(  _fetch_dagnode_into_cache_and_queue(pCtx,pDagLca,szHidParent,&pDataParent)  );

		pDataParent->bvClosure = pDataChild->bvClosure;

		if (pDataChild->itemIndex != -1)
			pDataParent->bvImmediateDescendants = (1ULL << pDataChild->itemIndex);
		else
			pDataParent->bvImmediateDescendants = pDataChild->bvImmediateDescendants;
	}
	else
	{
		// the parent is in the fetched-cache, so we have seen it before.  and since it
		// will have a smaller generation than us (by construction), it must also be in
		// the work-queue.

		// if the parent is a leaf, then it is a leaf ***and*** an ancestor of one
		// of the other leaves.  We agreed in [1d] to shut this down.

		if (pDataParent->bvLeaf != 0)
			SG_ERR_THROW(  SG_ERR_DAGLCA_LEAF_IS_ANCESTOR  );

		// compare the child and parent's closure bitvectors and decide the significance
		// of the parent.

		SG_ERR_CHECK(  _process_parent_closure(pCtx,pDagLca,pDataChild,pDataParent)  );

		// if the child is a significant node, then we want to make it an
		// immediate descendant of the parent -- BUT -- we need to remove any
		// immediate descendants from the parent that are also ours -- because
		// the child is between them.  (If the child is not a significant node,
		// we just bubble up the child's immediate descendants.)
		//
		//         A
		//          \.
		//           C
		//          /|
		//         D |
		//        / \|
		//       L0  L1
		//
		// Figure 2a.  (For our purposes here, this graph is equivalent to Figure 2.)
		//
		// In this example, after L1 is processed, ImmDesc(D)-->{L0,L1} and
		// ImmDesc(C)-->{L1}.  After D is processed, we want ImmDesc(C)-->{D}
		// because the edge L1-->C or L1-->E-->C are discardable.

		if (pDataChild->itemIndex != -1)
		{
			pDataParent->bvImmediateDescendants &= ~pDataChild->bvImmediateDescendants;
			pDataParent->bvImmediateDescendants |= (1ULL << pDataChild->itemIndex);
		}
		else
			pDataParent->bvImmediateDescendants |= pDataChild->bvImmediateDescendants;
	}

	return;

fail:
	return;
}

static void _verify_closure_complete(SG_daglca * pDagLca,
									 my_data * pData,
									 SG_bool * pbComplete)
{
	// we have processed all the various significant paths in the
	// dag and have reached a single node.  this closure computed
	// for this node should be complete -- contain all leaves and
	// all SPCAs (optionally including itself).
	//
	// verify that.

	SG_daglca_bitvector bvComplete;

	if (pDagLca->nrSignificantNodes == SG_DAGLCA_BIT_VECTOR_LENGTH)
		bvComplete = ~0ULL;
	else
		bvComplete = (1ULL << pDagLca->nrSignificantNodes) - 1;

	*pbComplete = (pData->bvClosure == bvComplete);
}

static void _fixup_result_map(SG_context * pCtx,
							  SG_daglca * pDagLca)
{
	// change the node-type on the first SPCA to LCA to aid our callers.

	const char * szKey;
	my_data * pDataFirst;
	SG_bool bFound;
	SG_bool bComplete;
	SG_rbtree_iterator * pIter = NULL;

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,
											  &pIter,
											  pDagLca->pRB_SortedResultMap,
											  &bFound,
											  &szKey,
											  (void**)&pDataFirst)  );
	SG_ASSERT_RELEASE_FAIL(  ((bFound) && "DAGLCA Empty sorted-result map")  );

	SG_ASSERT_RELEASE_FAIL(  ((pDataFirst->nodeType == SG_DAGLCA_NODE_TYPE__SPCA) && "DAGLCA Invalid node type on LCA")  );

	_verify_closure_complete(pDagLca,pDataFirst,&bComplete);
	SG_ASSERT_RELEASE_FAIL(  ((bComplete) && "DAGLCA LCA does not have complete closure")  );

	pDataFirst->nodeType = SG_DAGLCA_NODE_TYPE__LCA;

#if defined(DEBUG)
	{
		// verify that the second significant node is deeper than the LCA.
		// it must be because the LCA is a parent of it.

		my_data * pDataSecond;

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bFound,&szKey,(void**)&pDataSecond)  );
		SG_ASSERT_RELEASE_FAIL(  ((bFound) && "DAGLCA should have second item in sorted-result map")  );
		SG_ASSERT_RELEASE_FAIL(  ((pDataSecond->genDagnode > pDataFirst->genDagnode)
						  && "DAGLCA second item in sorted-result set should be deeper than first")  );

		_verify_closure_complete(pDagLca,pDataSecond,&bComplete);
		SG_ASSERT_RELEASE_FAIL(  ((!bComplete) && "DAGLCA second item should not have complete closure")  );

		// TODO we *could* scan all the remaining items in our sorted-result map
		// TODO and verify that they don't have complete closures too.
	}
#endif

	// fall thru to common cleanup

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}

//////////////////////////////////////////////////////////////////

void SG_daglca__compute_lca(SG_context * pCtx, SG_daglca * pDagLca)
{
	// using the set of leaves already given, freeze pDagLca
	// and compute the SPCAs and LCA.

	SG_bool bFound = SG_FALSE;
	my_data * pDataQueued = NULL;
	SG_uint32 nrParents, kParent;
	SG_uint32 nrQueued;
	const char ** aszHidParents = NULL;

	SG_NULLARGCHECK_RETURN(pDagLca);

	SG_ARGCHECK_RETURN(  (pDagLca->nrLeafNodes >= 2),  "nrLeafNodes"  );

	// we can only do this computation once.

	if (pDagLca->bFrozen)
		SG_ERR_THROW(  SG_ERR_INVALID_WHILE_FROZEN  );

	pDagLca->bFrozen = SG_TRUE;

	while (1)
	{
		// process the work-queue until we find the LCA.
		//
		// remove the first dagnode in the work-queue.  because of
		// our sorting, this is the deepest dagnode in the dag (or
		// one of several at that depth).

		SG_ERR_CHECK(  _work_queue__remove_first(pCtx,pDagLca,&bFound,&pDataQueued)  );

		// this cannot happen because we should have stopped when we
		// saw either the implied null root or when we were down to
		// only one node.
		SG_ASSERT_RELEASE_FAIL(  ((bFound) && "DAGLCA Emptied work-queue??")  );

		if (pDataQueued->pDagnode == NULL)
		{
			// we have the my_data structure for the implied null root.
			//
			// by construction, this will be the last item in the work-queue
			// (because no other node will have this generation number.)
			//
			// since we set all the attributes on this node when we were
			// evaluating the initial checkins (and this node can't have
			// any parents), we can stop.

			goto FinishedScan;
		}

		SG_ERR_CHECK(  SG_dagnode__count_parents(pCtx,pDataQueued->pDagnode,&nrParents)  );
		if (nrParents == 0)
		{
			// we have an initial checkin.  it implicitly refers to a fake/synthetic
			// NULL (root) dagnode.  (see discussion in sg_dag_sqlite3.c for why we
			// have to allow multiple initial checkins.)
			//
			// we pass a NULL for szHid and let the lower layers lookup/create a
			// version of my_data that has a null dagnode.  if necessary, this
			// will add the "*root*" node to the work-queue.

			SG_ERR_CHECK(  _process_parent(pCtx,pDagLca,pDataQueued,NULL)  );
			continue;
		}

		// we have a regular (non-initial) checkin.

		SG_ERR_CHECK(  SG_rbtree__count(pCtx,pDagLca->pRB_SortedWorkQueue,&nrQueued)  );
		if (nrQueued == 0)
		{
			// we took the last entry in the work-queue.
			//
			// we should be able to stop without looking at the parents of this dagnode.
			//
			// by construction, all the parents of this node (that we will be processing
			// in the loop below) can only have the closure bits that we already have
			// and therefore none of the parents can be a SPCA.  so we don't need to go
			// any higher in the dag.
			//
			// NOTE THAT THIS NODE MAY OR MAY NOT BE THE LCA.  IN FACT, IT MAY NOT EVEN
			// BE AN SPCA.
			//
			// For example, in Figure 2, when we take C from the work-queue, it is not
			// an SPCA/LCA because D is and SPCA and is closer.  However, C should have
			// a complete set of bits set in its closure bitvector.

			goto FinishedScan;
		}

		// it is tempting to put in a check here to see if this node has
		// a complete closure (possibly the first ancestor of all of the
		// leaves) and stop searching.
		//
		// but this would not allow us to resolve the criss-cross
		// case because we extend the set of bits with each criss-cross peer
		// and we need to continue grinding until we get *their* common ancestor.

		SG_ERR_CHECK(  SG_dagnode__get_parents(pCtx,pDataQueued->pDagnode,&nrParents,&aszHidParents)  );

		for (kParent=0; kParent<nrParents; kParent++)
		{
			SG_ERR_CHECK(  _process_parent(pCtx,pDagLca,pDataQueued,aszHidParents[kParent])  );
		}

		SG_NULLFREE(pCtx, aszHidParents);
	}

FinishedScan:
	SG_ERR_CHECK(  _fixup_result_map(pCtx,pDagLca)  );
	return;

fail:
	SG_NULLFREE(pCtx, aszHidParents);
	return;
}

//////////////////////////////////////////////////////////////////

void SG_daglca__get_stats(SG_context * pCtx,
						  const SG_daglca * pDagLca,
						  SG_uint32 * pNrLCA,
						  SG_uint32 * pNrSPCA,
						  SG_uint32 * pNrLeaves)
{
	// return some stats.
	//
	// TODO is there any other summary data that would be useful?

	SG_NULLARGCHECK_RETURN(pDagLca);

	if (pNrLCA)				// this arg is trivial, i added it so that the
		*pNrLCA = 1;		// caller would not forget to include it their counts.

	if (pNrSPCA)
		*pNrSPCA = pDagLca->nrSignificantNodes - pDagLca->nrLeafNodes - 1;

	if (pNrLeaves)
		*pNrLeaves = pDagLca->nrLeafNodes;

	// TODO it might be interesting to compute these values by
	// TODO scanning the pDagLca->aData_SignfificantNodes or
	// TODO pDagLca->pRB_SortedResultMap and make sure that they
	// TODO match.
}

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
static const char * aszNodeTypeNames[] = { "Nobody",
										   "Leaf  ",
										   "SPCA  ",
										   "LCA   " };

void SG_daglca_debug__dump(SG_context * pCtx,
						   const SG_daglca * pDagLca,
						   const char * szLabel,
						   SG_uint32 indent,
						   SG_string * pStringOutput)
{
	SG_uint32 nrLeaves, nrSPCAs, nrLCAs;	// size of our results
	SG_uint32 nrFetched;					// of interest for big-O complexity
	SG_rbtree_iterator * pIter = NULL;
	SG_bool bFound;
	const char * szKey;
	my_data * pDataFound;
	char buf[4000];
	char bufbv1[100], bufbv2[100];

	SG_NULLARGCHECK_RETURN(pDagLca);
	SG_NULLARGCHECK_RETURN(pStringOutput);

	if (!pDagLca->bFrozen)
		SG_ERR_THROW_RETURN(  SG_ERR_INVALID_UNLESS_FROZEN  );

	SG_ERR_CHECK(  SG_sprintf(pCtx,
							  buf,SG_NrElements(buf),
							  "%*cDagLca[%s]\n",
							  indent,' ',
							  ((szLabel) ? szLabel : ""))  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx,pStringOutput,buf)  );

	SG_ERR_CHECK(  SG_daglca__get_stats(pCtx,pDagLca,&nrLCAs,&nrSPCAs,&nrLeaves)  );
	SG_ERR_CHECK(  SG_rbtree__count(pCtx,pDagLca->pRB_FetchedCache,&nrFetched)  );
	SG_ERR_CHECK(  SG_sprintf(pCtx,
							  buf,SG_NrElements(buf),
							  "%*c    [LCAs %d][SPCAs %d][Leaves %d] [Touched %d]\n",
							  indent,' ',
							  nrLCAs,nrSPCAs,nrLeaves,nrFetched)  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx,pStringOutput,buf)  );

	// dump the list of significant nodes in sorted order.
	// the LCA should be first.  this is a raw dump with
	// very little decoding.

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,
											  &pIter,pDagLca->pRB_SortedResultMap,
											  &bFound,
											  &szKey,(void**)&pDataFound)  );
	while (bFound)
	{
		SG_hex__format_uint64(bufbv1,pDataFound->bvImmediateDescendants);
		SG_hex__format_uint64(bufbv2,pDataFound->bvClosure);

		SG_ERR_CHECK(  SG_sprintf(pCtx,buf,SG_NrElements(buf),
								  "%*c    %s %s %s %s\n",
								  indent,' ',
								  szKey,
								  aszNodeTypeNames[pDataFound->nodeType],
								  bufbv1,
								  bufbv2)  );
		SG_ERR_CHECK(  SG_string__append__sz(pCtx,pStringOutput,buf)  );

		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,pIter,&bFound,&szKey,(void**)&pDataFound)  );
	}

	// fall thru

fail:
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIter);
}

void SG_daglca_debug__dump_to_console(SG_context * pCtx,
									  const SG_daglca * pDagLca)
{
	SG_string * pString = NULL;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );
	SG_ERR_CHECK(  SG_daglca_debug__dump(pCtx, pDagLca, "", 4, pString)  );
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "DAGLCA Dump:\n%s\n", SG_string__sz(pString))  );

fail:
	SG_STRING_NULLFREE(pCtx, pString);
}

#endif//DEBUG

//////////////////////////////////////////////////////////////////

/**
 * convert the 1 bits in the given bitvector to an rbtree
 * containing the HIDs of the associated significant nodes.
 *
 * This is more expensive than I would like, but it lets the
 * caller have, for example, the set of immediate descendants
 * of a particular node in a sorted thing that they can do
 * something with.
 *
 * Since we expect most significant nodes to have 2 immediate
 * descendants, this may be overkill.  But it does allow for
 * n-way merges.
 *
 * TODO we may also want to have a version of this that builds
 * TODO an array of HIDs (like SG_dagnode__get_parents()).
 *
 */
static void _bitvector_to_rbtree(SG_context * pCtx,
								 const SG_daglca * pDagLca,
								 const SG_daglca_bitvector bvInput,
								 SG_rbtree ** pprb)
{
	SG_rbtree * prb = NULL;
	SG_daglca_bitvector bv;
	SG_uint32 k;

	if (bvInput == 0)
	{
		*pprb = NULL;
		return;
	}

	SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS(pCtx,&prb,5,NULL)  );

	bv = bvInput;
	for (k=0; (bv != 0); k++)
	{
		if (bv & 1)
		{
			my_data * pDataK;
			const char * szHidK;

			pDataK = pDagLca->aData_SignificantNodes[k];
			SG_ASSERT(  (pDataK)  &&  "Invalid bit in bitvector"  );

			if ((pDataK->pDagnode == NULL) || (pDataK->genDagnode == 0))
				szHidK = "*root*";		// this can't happen for imm-desc, but can for closure
			else
				SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx,pDataK->pDagnode,&szHidK)  );

			SG_ERR_CHECK(  SG_rbtree__add(pCtx,prb,szHidK)  );
		}

		bv >>= 1;
	}

	*pprb = prb;
	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, prb);
}

//////////////////////////////////////////////////////////////////

struct _SG_daglca_iterator
{
	const SG_daglca *		pDagLca;
	SG_rbtree_iterator *	prbIter;
	SG_bool					bWantLeaves;
};

void SG_daglca__iterator__free(SG_context * pCtx, SG_daglca_iterator * pDagLcaIter)
{
	if (!pDagLcaIter)
		return;

	// we do not own pDagLcaIter->pDagLca.

	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pDagLcaIter->prbIter);

	SG_NULLFREE(pCtx, pDagLcaIter);
}

void SG_daglca__iterator__first(SG_context * pCtx,
								SG_daglca_iterator ** ppDagLcaIter,		// returned (you must free this)
								const SG_daglca * pDagLca,				// input
								SG_bool bWantLeaves,					// input
								const char ** pszHid,					// output
								SG_daglca_node_type * pNodeType,		// output
								SG_int32 * pGeneration,					// output
								SG_rbtree ** pprbImmediateDescendants)	// output (you must free this)
{
	const char * szHid = NULL;
	SG_daglca_iterator * pDagLcaIter = NULL;
	SG_rbtree * prbImmediateDescendants = NULL;
	SG_bool bFound;
	my_data * pDataFirst;

	SG_NULLARGCHECK_RETURN(pDagLca);

	// alloc ppIter to be null.  it means that they only want the
	// first ancestor and don't want to iterate.

	if (!pDagLca->bFrozen)
		SG_ERR_THROW_RETURN(  SG_ERR_INVALID_UNLESS_FROZEN  );

	SG_ERR_CHECK(  SG_alloc1(pCtx,pDagLcaIter)  );

	pDagLcaIter->pDagLca = pDagLca;
	pDagLcaIter->bWantLeaves = bWantLeaves;

	SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx,
											  &pDagLcaIter->prbIter,
											  pDagLca->pRB_SortedResultMap,
											  &bFound,
											  NULL,(void **)&pDataFirst)  );
	SG_ASSERT_RELEASE_FAIL(  ((bFound) && "DAGLCA Empty sorted-result map")  );
	SG_ASSERT_RELEASE_FAIL(  ((pDataFirst->nodeType == SG_DAGLCA_NODE_TYPE__LCA)  &&  "DAGLCA first node not LCA?")  );

	if (pszHid)
	{
		if (pDataFirst->pDagnode == NULL)
			szHid = "*root*";
		else
			SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx,pDataFirst->pDagnode,&szHid)  );
	}

	if (pprbImmediateDescendants)
	{
		SG_ERR_CHECK(  _bitvector_to_rbtree(pCtx,
											pDagLca,
											pDataFirst->bvImmediateDescendants,
											&prbImmediateDescendants)  );
	}

	// now that we have all of the results, populate the
	// callers variables.  we don't want to do this until
	// we are sure that we won't fail.

	if (pszHid)
		*pszHid = szHid;
	if (pNodeType)
		*pNodeType = pDataFirst->nodeType;
	if (pGeneration)
		*pGeneration = pDataFirst->genDagnode;
	if (pprbImmediateDescendants)
		*pprbImmediateDescendants = prbImmediateDescendants;

	if (ppDagLcaIter)
		*ppDagLcaIter = pDagLcaIter;
	else
		SG_DAGLCA_ITERATOR_NULLFREE(pCtx, pDagLcaIter);

	return;

fail:
	SG_DAGLCA_ITERATOR_NULLFREE(pCtx, pDagLcaIter);
	SG_RBTREE_NULLFREE(pCtx, prbImmediateDescendants);
}

void SG_daglca__iterator__next(SG_context * pCtx,
							   SG_daglca_iterator * pDagLcaIter,
							   SG_bool * pbFound,
							   const char ** pszHid,
							   SG_daglca_node_type * pNodeType,
							   SG_int32 * pGeneration,
							   SG_rbtree ** pprbImmediateDescendants)
{
	const char * szHid = NULL;
	SG_rbtree * prbImmediateDescendants = NULL;
	SG_bool bFound;
	my_data * pData;

	SG_NULLARGCHECK_RETURN(pDagLcaIter);

	if (!pDagLcaIter->pDagLca->bFrozen)
		SG_ERR_THROW_RETURN(  SG_ERR_INVALID_UNLESS_FROZEN  );

	while (1)
	{
		SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx,
												 pDagLcaIter->prbIter,
												 &bFound,
												 NULL,(void **)&pData)  );
		if (!bFound)
		{
			*pbFound = SG_FALSE;
			if (pszHid)
				*pszHid = NULL;
			if (pNodeType)
				*pNodeType = SG_DAGLCA_NODE_TYPE__NOBODY;
			if (pGeneration)
				*pGeneration = -1;
			if (pprbImmediateDescendants)
				*pprbImmediateDescendants = NULL;

			return;
		}

		if ((pData->nodeType == SG_DAGLCA_NODE_TYPE__SPCA)
			|| (pData->nodeType == SG_DAGLCA_NODE_TYPE__LCA)
			|| (pDagLcaIter->bWantLeaves && (pData->nodeType == SG_DAGLCA_NODE_TYPE__LEAF)))
		{
			if (pszHid)
			{
				if (pData->pDagnode == NULL)
					szHid = "*root*";
				else
					SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx,pData->pDagnode,&szHid)  );
			}

			if (pprbImmediateDescendants)
			{
				SG_ERR_CHECK(  _bitvector_to_rbtree(pCtx,
													pDagLcaIter->pDagLca,
													pData->bvImmediateDescendants,
													&prbImmediateDescendants)  );
			}

			// now that we have all of the results, populate the
			// callers variables.  we don't want to do this until
			// we are sure that we won't fail.

			if (pbFound)
				*pbFound = bFound;
			if (pszHid)
				*pszHid = szHid;
			if (pNodeType)
				*pNodeType = pData->nodeType;
			if (pGeneration)
				*pGeneration = pData->genDagnode;
			if (pprbImmediateDescendants)
				*pprbImmediateDescendants = prbImmediateDescendants;	// this may be null if we don't have any

			return;
		}

		SG_ASSERT_RELEASE_RETURN(  ((pData->nodeType == SG_DAGLCA_NODE_TYPE__LEAF)  &&  "DAGLCA unknown significant node type")  );
	}

	/*NOTREACHED*/

fail:
	SG_RBTREE_NULLFREE(pCtx, prbImmediateDescendants);
}

//////////////////////////////////////////////////////////////////

void SG_daglca__foreach(SG_context * pCtx,
						const SG_daglca * pDagLca, SG_bool bWantLeaves,
						SG_daglca__foreach_callback * pfn, void * pVoidCallerData)
{
	SG_daglca_iterator * pDagLcaIter = NULL;
	SG_bool bFound;
	const char * szHid;
	SG_daglca_node_type nodeType;
	SG_int32 gen;
	SG_rbtree * prb;

	// the callback must free prb.

	SG_ERR_CHECK(  SG_daglca__iterator__first(pCtx,
											  &pDagLcaIter,
											  pDagLca,bWantLeaves,
											  &szHid,&nodeType,&gen,&prb)  );
	do
	{
		SG_ERR_CHECK(  (*pfn)(pCtx,szHid,nodeType,gen,prb, pVoidCallerData)  );

		SG_ERR_CHECK(  SG_daglca__iterator__next(pCtx,
												 pDagLcaIter,
												 &bFound,&szHid,&nodeType,&gen,&prb)  );
	} while (bFound);

	// fall thru to common cleanup

fail:
	SG_DAGLCA_ITERATOR_NULLFREE(pCtx, pDagLcaIter);
}

//////////////////////////////////////////////////////////////////

void SG_daglca__get_all_descendant_leaves(SG_context * pCtx,
										  const SG_daglca * pDagLca,
										  const char * pszHid,
										  SG_rbtree ** pprbDescendantLeaves)
{
	SG_rbtree * prb = NULL;
	my_data * pData;
	SG_daglca_bitvector bv;
	SG_uint32 k;
	SG_bool bFound;

	SG_NULLARGCHECK_RETURN(pDagLca);
	SG_NONEMPTYCHECK_RETURN(pszHid);
	SG_NULLARGCHECK_RETURN(pprbDescendantLeaves);

	SG_ERR_CHECK(  _cache__lookup(pCtx, pDagLca, pszHid, &bFound, &pData)  );
	if (!bFound)
		SG_ERR_THROW2(  SG_ERR_INVALIDARG,
						(pCtx, "Node [%s] not found in DAGLCA graph.", pszHid)  );

	SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS(pCtx,&prb,5,NULL)  );

	bv = pData->bvClosure;
	for (k=0; (bv != 0); k++)
	{
		if (bv & 1)
		{
			my_data * pDataK = pDagLca->aData_SignificantNodes[k];
			SG_ASSERT(  (pDataK)  &&  "Invalid bit in bitvector"  );

			if (pDataK->nodeType == SG_DAGLCA_NODE_TYPE__LEAF)
			{
				const char * szHidK;
				SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx,pDataK->pDagnode,&szHidK)  );

				SG_ERR_CHECK(  SG_rbtree__add(pCtx,prb,szHidK)  );
			}
		}

		bv >>= 1;
	}

	*pprbDescendantLeaves = prb;
	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, prb);
}
