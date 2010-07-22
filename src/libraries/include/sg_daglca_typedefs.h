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
 * @file sg_daglca_typedefs.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_DAGLCA_TYPEDEFS_H
#define H_SG_DAGLCA_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * The DAG Least Common Ancestor (LCA) computation produces
 * a GRAPH of the "Significant" DAG Nodes.  That is, nodes
 * that are significant in the branch/merge history of the
 * overall DAG from the point of view of the given set of
 * Leaf Nodes.
 *
 * Nodes in the LCA GRAPH can be one of the following types:
 *
 * A LEAF node is one of the initial leaves that we were given.
 * It is in the result vector so that ancestors may reference
 * it by index in their bit-vectors.
 *
 * A SPCA node represents a join (when looking from the leaves
 * back towards the root).  This is a Significant (possibly Partial)
 * Common Ancestor.
 *
 * A LCA node is the final/absolute LCA of the entire set
 * of leaves.
 *
 * A NOBODY node is a non-significant dagnode. These will not
 * appear in the final results.
 */
enum _SG_daglca_node_type
{
	SG_DAGLCA_NODE_TYPE__NOBODY  =0,
	SG_DAGLCA_NODE_TYPE__LEAF    =1,
	SG_DAGLCA_NODE_TYPE__SPCA    =2,
	SG_DAGLCA_NODE_TYPE__LCA     =3,
};

typedef enum _SG_daglca_node_type SG_daglca_node_type;

typedef struct _SG_daglca SG_daglca;

typedef struct _SG_daglca_iterator SG_daglca_iterator;

/**
 * A callback typedef for use with SG_daglca__foreach().
 *
 * This will be called for each LCA and SPCA node in the LCA
 * graph.  It will also include the LEAF nodes if requested.
 *
 * You are provided with the HID of the dagnode and the HIDs of
 * descendants and *NOT* actual dagnodes objects.  This is because
 * dagnodes objects refer to *parent* edges in the *actual* DAG;
 * when using the LCA GRAPH, we refer to *child* edges in a
 * distilled graph.
 *
 * prbImmediateDescendants contains a list of the HIDs of the
 * "Immediate Descendants" -- the child edges in the LCA GRAPH.
 * This will be NULL for Leaf nodes.  For other node types, it
 * will contain AT LEAST 2 items.
 * You own this and must free it.
 */
typedef void SG_daglca__foreach_callback(SG_context * pCtx,
										 const char * szHid,
										 SG_daglca_node_type nodeType,
										 SG_int32 generation,
										 SG_rbtree * prbImmediateDescendants,
										 void * pVoidCallerData);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_DAGLCA_TYPEDEFS_H
