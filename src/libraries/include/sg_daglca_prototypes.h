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
 * @file sg_daglca_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_DAGLCA_PROTOTYPES_H
#define H_SG_DAGLCA_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Allocate a DAGLCA object and prepare for the DAG LCA computation.
 *
 * Subsequent calls to __add_leaf() will use the given callback to
 * fetch dagnodes from disk.
 */
void SG_daglca__alloc(SG_context * pCtx,
					  SG_daglca ** ppNew,
					  SG_uint32 iDagNum,
					  FN__sg_fetch_dagnode * pFnFetchDagnode,
					  void * pVoidFetchDagnodeData);

void SG_daglca__free(SG_context * pCtx, SG_daglca * pDagLca);

/**
 * Add a LEAF to the LCA computation.  This may be any dagnode in the
 * DAG.  It will be considered a leaf from the point of view of the
 * LCA computation; it may or may not be an actual leaf in the whole
 * DAG.
 *
 * Returns:
 * <ul>
 * <li>SG_ERR_INVALID_WHILE_FROZEN if the LCA computation has already been
 * performed.</li>
 * <li>SG_ERR_DAGNODE_ALREADY_EXISTS if this is a duplicate leaf.</li>
 * <li>something if HID is not a valid dagnode.</li>
 * <li>SG_ERR_LIMIT_EXCEEDED if too many leaf nodes have been added; this
 * limit is currrently about 64.</li>
 * <li>TODO others?</li>
 * </ul>
 */
void SG_daglca__add_leaf(SG_context * pCtx,
						 SG_daglca * pDagLca, const char * szHid);

/**
 * Add a bunch of leaves.  Iterates over the given RBTREE and calls
 * SG_daglca__add_leaf() for each.  Assumes that the RBTREE has simple
 * HIDs as keys.  Does not use any associated data that may be present
 * in the RBTREE.
 */
void SG_daglca__add_leaves(SG_context * pCtx,
						   SG_daglca * pDagLca, const SG_rbtree * prbLeaves);

/**
 * Freezes the DAGLCA and runs the LCA computation.
 *
 * Returns:
 * <ul>
 * <li>SG_ERR_INVALID_WHILE_FROZEN if the LCA computation has already been
 * performed.</li>
 * <li>SG_ERR_LIMIT_EXCEEDED if the computation generates too many internal
 * SPCA nodes; currently the sum of the number of LEAVES, SPCA, and LCA must
 * be <= 64.</li>
 * <li>SG_ERR_DAGLCA_LEAF_IS_ANCESTOR if one of the leaves is an ancestor of
 * another one of the leaves.</li>
 * </ul>
 */
void SG_daglca__compute_lca(SG_context * pCtx,
							SG_daglca * pDagLca);

/**
 * Returns some simple statistics on the computed LCA.
 */
void SG_daglca__get_stats(SG_context * pCtx,
						  const SG_daglca * pDagLca,
						  SG_uint32 * pNrLCA,
						  SG_uint32 * pNrSPCA,
						  SG_uint32 * pNrLeaves);

#if defined(DEBUG)
void SG_daglca_debug__dump(SG_context * pCtx,
						   const SG_daglca * pDagLca,
						   const char * szLabel,
						   SG_uint32 indent,
						   SG_string * pStringOutput);

void SG_daglca_debug__dump_to_console(SG_context * pCtx,
									  const SG_daglca * pDagLca);
#endif

//////////////////////////////////////////////////////////////////

void SG_daglca__iterator__free(SG_context * pCtx, SG_daglca_iterator * pDagLcaIter);

/**
 * Begin an iteratation over the set of significant nodes in
 * the LCA result set.
 *
 * This also returns information about the first node in the LCA graph.
 * This is the actual/absolute LCA that we computed.
 *
 * pprbImmediateDescendants will be set to a RBTREE containing the
 * immediate descendants of this node in the LCA graph.  You may set
 * this to NULL if you don't want them.  You must free this.
 *
 * If ppDagLcaIter is not NULL, an iterator will be returned to allow
 * subsequent members of the LCA graph to be fetched using __iterator__next().
 * You must free this.
 *
 * If bWantLeaves is SG_TRUE, leaf nodes will be include in the iteration.
 * Otherwise, only the LCA and SPCA nodes will be included.
 *
 * pszHid will be set to the address of a private buffer; you do not own this.
 */
void SG_daglca__iterator__first(SG_context * pCtx,
								SG_daglca_iterator ** ppDagLcaIter,
								const SG_daglca * pDagLca,
								SG_bool bWantLeaves,
								const char ** pszHid,
								SG_daglca_node_type * pNodeType,
								SG_int32 * pGeneration,
								SG_rbtree ** pprbImmediateDescendants);

/**
 * Fetch next node in the LCA graph.  This will be an SPCA or LEAF node.
 *
 * pprbImmediateDescendants will be set to a RBTREE containing the
 * immediate descendants of this node in the LCA graph.  You may set
 * this to NULL if you don't want them.  This will be set to NULL for
 * LEAF nodes.   You must free this.
 *
 * pbFound will be set to SG_TRUE if a node was returned.
 */
void SG_daglca__iterator__next(SG_context * pCtx,
							   SG_daglca_iterator * pDagLcaIter,
							   SG_bool * pbFound,
							   const char ** pszHid,
							   SG_daglca_node_type * pNodeType,
							   SG_int32 * pGeneration,
							   SG_rbtree ** pprbImmediateDescendants);

/**
 * Iterate over the nodes in the LCA graph and call the given callback
 * function for each.
 */
void SG_daglca__foreach(SG_context * pCtx,
						const SG_daglca * pDagLca, SG_bool bWantLeaves,
						SG_daglca__foreach_callback * pfn, void * pVoidCallerData);

//////////////////////////////////////////////////////////////////

/**
 * Allocate and return a RBTREE containing all of the LEAVES
 * that are descended from the given node.
 *
 * If given the LCA, we return the set of all leaves.
 * If given an SPCA, this may be a subset of the leaves or all
 * of the leaves depending on the bushiness of the graph.
 * If given a LEAF, we return a set containing only it.
 *
 * You own the returned RBTREE.
 */
void SG_daglca__get_all_descendant_leaves(SG_context * pCtx,
										  const SG_daglca * pDagLca,
										  const char * pszHid,
										  SG_rbtree ** pprbDescendantLeaves);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_DAGLCA_PROTOTYPES_H
