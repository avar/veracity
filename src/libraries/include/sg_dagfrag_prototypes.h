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
 * @file sg_dagfrag_prototypes.h
 *
 * @details Routines for manipulating a fragment of the dag.
 *
 * A DAGFRAG is a subset of the entire DAG.  It contains a
 * set of dagnodes that can be in one of three states:
 * QS_START_MEMBER -- a member of the fragment and a starting point.
 * QS_INTERIOR_MEMBER -- a member of the fragment, but not a starting point.
 * QS_END_FRINGE -- not a member of the fragment, but the immediate
 *                  parent of a member (of either type).
 *
 * We can use this to express a portion of the graph during
 * PUSH/PULL operations, for example.  Where we want to transmit
 * graph connectivity with a minimum number of round-trips.
 *
 * The START_MEMBERs gives the various starting points in the
 * graph included in this fragment -- this is usually the complete
 * set of LEAVES, but there is no reason to limit it to that.
 * In reality, it can be any collection of nodes that we want to
 * start with.  Think of this as the child-most-fringe and
 * included in the fragment.
 *
 * For each starting point, we add all of the ancestor dagnodes
 * for a given number of GENERATIONS.
 *
 * The END_FRINGE are the set of immediate parents of fragment
 * members that were too old (generation-wise).  Think of this
 * as the parent-most-fringe.
 *
 * This construction allows us, for example, to PUSH our complete
 * set of LEAVES and n GENERATIONS from one repo instance to
 * another.  If the receiving repo knows about all of the DAGNODES
 * in the END-FRINGE, then it can apply the fragment to its DAG
 * and know that it will have a completely connected graph; if
 * one of the END-FRINGE DAGNODES is unknown, it can reject the
 * fragment and request the sender send a deeper one.   It is
 * expected that most of the information in the fragment is
 * redundant (that the receiver already knows about most of the
 * DAGNODES in the fragment) and that it only needs information
 * on the most recent commits.  It will be important to tune
 * the GENERATION setting to minimize wasted bandwidth but also
 * minimize the odds of a rejection.
 *
 *
 * The graph fragment that we represent does not need to be
 * completely connected.  We could be representing 3 different
 * branches/heads/leaves and 2 generations of their ancestors,
 * so we might have 3 independent subsets of the graph in the
 * fragment.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_DAGFRAG_PROTOTYPES_H
#define H_SG_DAGFRAG_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Set up a DAGFRAG and prepare to load one or more pieces of
 * the DAG into it from the source implied by the given fetch-dagnode
 * callback.
 */
void SG_dagfrag__alloc(SG_context * pCtx,
					   SG_dagfrag ** ppNew,
					   const char* psz_repo_id,
					   const char* psz_admin_id,
					   SG_uint32 iDagNum);

/**
 * Set up a TRANSIENT DAGFRAG.  This will only be used in-memory to
 * ask some dagnode relationship questions and never serialized
 * and/or used in a push/pull.
 *
 * The only different between this and a REAL DAGFRAG is that we
 * don't care about the REPO_ID, the ADMIN_ID, and the DAGNUM, so
 * I created this version of __alloc that avoids argchecking/
 * allocating/copying them.  Those fields are mainly header info
 * anyway and used as a signature match.
 */
void SG_dagfrag__alloc_transient(SG_context * pCtx,
								 SG_dagfrag ** ppNew);

void SG_dagfrag__free(SG_context * pCtx, SG_dagfrag * pFrag);

/**
 * Add a single dagnode to a frag.  The frag takes ownership of the SG_dagnode.
 *
 * TODO Review: should we steal the dagode?  If we do, shouldn't this be a private
 *              static routine?
 */
void SG_dagfrag__add_dagnode(SG_context * pCtx,
							 SG_dagfrag * pFrag,
							 SG_dagnode** ppdn);

/**
 * Merge two dagfrags, enlarging one and freeing the other.
 * The dagfrags must overlap or be adjacent, with pConsumerFrag being closest
 * to root.
 *
 * Ian TODO: better automated tests for this routine.
 */
void SG_dagfrag__eat_other_frag(SG_context * pCtx,
								SG_dagfrag* pConsumerFrag,
								SG_dagfrag** ppFragToBeEaten);

void SG_dagfrag__load_from_repo__simple(SG_context * pCtx,
										SG_dagfrag * pFrag,
										SG_repo* pRepo,
										SG_rbtree * prbLeaves);

/**
 * Add n generations of dagnodes to the fragment starting
 * with the given dagnode.  These dagnodes will be merged
 * with dagnodes already in the fragment.
 *
 * All dagnodes will be loaded from the source using the
 * fetch-dagnode callback provided when we were allocated.
 *
 * For example, you might call this once for each leaf
 * node and build up a snapshot of (hopefully) all local
 * activity.
 *
 * It is safe to call this with a non-leaf node (and even
 * one already in the fragment) to get a view of the DAG
 * with more history.
 *
 * It is important to note that the number of generations
 * is computed as the difference in the generation values
 * of 2 nodes -- not the number of linear edges between
 * them.
 */
void SG_dagfrag__load_from_repo__one(SG_context * pCtx,
									 SG_dagfrag * pFrag,
									 SG_repo* pRepo,
									 const char * pszidHidStart,
									 SG_int32 nGenerations);

/**
 * Add n generations of a set of leaves.
 *
 * All dagnodes will be loaded from the source using the
 * fetch-dagnode callback provided when we were allocated.
 *
 * This basically calls __add_leaf() for each member of prbLeaves.
 *
 * We ***ASSUME*** that the given rbtree has dagnode HIDs for keys.
 * We do not use the data associated with the members.
 */
void SG_dagfrag__load_from_repo__multi(SG_context * pCtx,
									   SG_dagfrag * pFrag,
									   SG_repo* pRepo,
									   SG_rbtree * prbLeaves,
									   SG_int32 nGenerations);

/**
 * Inquire about the state of an individual dagnode within
 * the fragment.
 *
 * If the dagnode for the requested HID is a member (either
 * QS_INTERIOR_ or QS_START_) of the fragment, we return a
 * pointer to our private copy of it.  You *DO NOT* own this.
 *
 * We return a NULL dagnode pointer for HIDs in the QS_END_FRINGE
 * state.  The purpose of the END-FRINGE is to simply list the HIDs
 * that need to exist in a receiving REPO so that we can paste the
 * fragment onto their DAG.   [We *may or may not* have a cached
 * dagnode in memory, but that doesn't matter; the only reason for
 * the caller to actually *use* the end-fringe-dagnode would be to
 * walk the parents and/or try to add it to their instance of the
 * repo (on a push/pull operation).  This *may or may not* be
 * well-defined -- we computed a closure on this fragment from
 * the point of view of the set of leaves and STOPPED here; we
 * make no representations that the closure covers the stuff
 * in the end-fringe.  Furthermore, use of the END-FRINGE dagnode
 * would simply push the effective set of end-fringes into the
 * individual parent vectors of each end-fringe dagnode and this
 * would be a mess.
 *
 * Regardless of the type, we return the state and generation
 * of the dagnode.
 */
void SG_dagfrag__query(SG_context * pCtx,
					   SG_dagfrag * pFrag, const char * szHid,
					   SG_dagfrag_state * pQS, SG_int32 * pGen,
					   SG_bool * pbPresent,
					   const SG_dagnode ** ppDagnode);

/**
 * Iterate over all dagnodes in the END_FRINGE (which we may or
 * may not actually have in memory) and give the HID and generation
 * of the dagnode to the callback.
 *
 * The caller may use this, for example, to inspect a different
 * repo and determine if this fragment is completely connected to
 * the other dag and can therefore be applied to it and preserve
 * the connectedness of the graph.   [In this example, if the graph
 * would not be connected, the callback should add the HID to a
 * list that it is maintaining in pCtxCaller and then ***after***
 * the iteration have the caller request the unknown HIDs be added
 * to the fragment (or have the remote side send another fragment)
 * and try again.]
 *
 * End fringe nodes are presented in a somewhat random order
 * (based upon their HIDs (which is essentially random)).
 *
 * Iteration continues as long as the callback returns SG_ERR_OK.
 *
 * The callback ***MUST NOT*** modify the fragment during iteration.
 * (Don't call __add_{leaf,leaves}() for example.)
 */
void SG_dagfrag__foreach_end_fringe(SG_context * pCtx,
									const SG_dagfrag * pFrag,
									SG_dagfrag__foreach_end_fringe_callback* pcb,
									void * pVoidCallerData);

/**
 * Iterate over all (START and INTERIOR) member dagnodes in the
 * fragment.
 *
 * Member nodes are presented depth order.  That is, they are
 * sorted by generation so that the ancestors are presented
 * before descendants.  This allows you to insert them into a
 * destination repo and maintain connectivity.  It is up to the
 * caller and callback to decide when/if/whether they should
 * pull the associated dagnode blobs.
 *
 * Iteration continues as long as the callback returns SG_ERR_OK.
 *
 * The callback ***MUST NOT*** modify the fragment during iteration.
 * (Don't call __add_{leaf,leaves}() for example.)
 */
void SG_dagfrag__foreach_member(SG_context * pCtx,
								SG_dagfrag * pFrag,
								SG_dagfrag__foreach_member_callback* pcb,
								void * pVoidCallerData);

//////////////////////////////////////////////////////////////////

/**
 * Serialize the dagfrag to a vhash.
 *
 * This is a complete dump of the intermediate data that we have in
 * memory so that it can be recreated by a remote repo and (in theory)
 * let them augment (as if they had initially created it in memory).
 *
 * We ***DO NOT EVER*** store the contents of a dagfrag in a blob or
 * compute a hash on it, so we ***DO NOT*** worry about sorting or any
 * of those other tricks to ensure that a HID is uniquely/consistently
 * computed.
 *
 * This serialization therefore includes more information than we
 * share with callers of __query(), __foreach_end_fringe(), or
 * __foreach_member().  ***DON'T ABUSE THIS***  That is, if you
 * receive a serialization, convert it back to a dagfrag and then
 * use one of the above functions to examine items within the
 * fragment.  This is especially true for items in the END-FRINGE.
 *
 * pvhShared is optional and will let pool sharing happen.
 *
 * You must free the vhash we return.
 *
 */
void SG_dagfrag__to_vhash__shared(SG_context * pCtx,
								  SG_dagfrag * pFrag,
								  SG_vhash * pvhShared,
								  SG_vhash ** ppvhNew);

/**
 * deserialize a dagfrag from a vhash into a regular dagfrag.
 *
 * See the warnings described above for serializing.
 *
 */
void SG_dagfrag__alloc__from_vhash(SG_context * pCtx,
								   SG_dagfrag ** ppNew,
								   const SG_vhash * pvhFrag);

/**
 * Retrieve the dagfrag's dagnum.
 */
void SG_dagfrag__get_dagnum(SG_context * pCtx,
							SG_dagfrag* pFrag, SG_uint32* piDagNum);

#if defined(DEBUG)
/**
 * Compare 2 dagfrags for equality.
 *
 * I wrote this for debugging purposes to let me test serialization/deserialization,
 * but we can keep it if it would be useful in production.
 *
 */
void SG_dagfrag__equal(SG_context * pCtx,
					   const SG_dagfrag * pFrag1, const SG_dagfrag * pFrag2, SG_bool * pbResult);
#endif

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
void SG_dagfrag_debug__dump(SG_context * pCtx,
							SG_dagfrag * pFrag,
							const char * szLabel,
							SG_uint32 indent,
							SG_string * pStringOutput);

void SG_dagfrag_debug__dump__console(SG_context* pCtx,
									 SG_dagfrag* pFrag,
									 const char* szLabel,
									 SG_uint32 indent,
									 SG_console_stream cs);
#endif//DEBUG

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_DAGFRAG_PROTOTYPES_H
