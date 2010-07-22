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
 * @file sg_dagnode_prototypes.h
 *
 * @details Routines for manipulating DAGNODES.
 *
 * A DAGNODE (in the DAG) corresponds to a CHANGESET.
 *
 * A DAGNODE only contains information about the EDGES from a
 * child CHANGESET to all of its parent CHANGESET(S).  Most
 * CHANGESETS will only have 1 parent; MERGE CHANGESETS will
 * have at least 2 - most merges will be 2-way, but we allow
 * for the possibility of more.
 *
 * A DAGNODE is stored in the REPO independent of the contents
 * of the actual CHANGESET.  This allows various levels of
 * sparseness within the REPO.
 *
 * The information in a DAGNODE is used to allows REPOS
 * exchange DAG-related information without worrying about
 * the contents of the CHANGESET.  The information in a
 * DAGNODE is also stored within the CHANGESET.
 *
 * The DAGNODE's on-disk format is determined by the SQL layer
 * hiding underneath the SG_repo__ API; it is completely opaque
 * to us.  The REPO layer can use SG_dagnode__ API to build the
 * DAGNODE memory object from scratch.
 *
 * The DAGNODE memory object is used:
 * <ol>
 * <li>during the COMMIT process to capture the essense of a
 * newly-created CHANGESET blob and update the REPO's DAG.</li>
 *
 * <li>to represent a DAGNODE in memory when the application
 * needs to build (all or a portion of) the DAG in memory in
 * order to perform various history queries - such as computing
 * the common ancestor of 2 nodes.  In these instances, the
 * in-memory DAGNODE object may have additional fields defined
 * that are not stored on disk and are completely ignored by
 * the SG_repo API.</li>
 * </ol>
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_DAGNODE_PROTOTYPES_H
#define H_SG_DAGNODE_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Allocate an empty DAGNODE and allow caller to populate it
 * with parents (EDGES) via _set_ calls.
 *
 * The given HID is the HID of the associated CHANGESET; this is
 * the child in the child-->parent relationships.
 *
 * Please freeze the DAGNODE after you have finished populating it.
 */
void SG_dagnode__alloc(SG_context* pCtx, SG_dagnode ** ppNew, const char* pszidHidChangeset, SG_int32 generation);

/**
 * Free a DAGNODE memory object.
 */
void SG_dagnode__free(SG_context * pCtx, SG_dagnode * pdn);

//////////////////////////////////////////////////////////////////

/**
 * Compare 2 DAGNODE memory objects and see if they are equal.
 * We only test the HIDs of the child and parents (the stuff
 * that is/was on disk); any other memory-only cache-like
 * fields are ignored.
 */
void SG_dagnode__equal(SG_context* pCtx, const SG_dagnode * pdn1, const SG_dagnode * pdn2, SG_bool * pbEqual);

//////////////////////////////////////////////////////////////////

// ASSERT: no version number is required the DAGNODE memory object;
// ASSERT: if the SQL layer hiding under the SG_repo API wants to
// ASSERT: embed a version number in the on-disk format it can, but
// ASSERT: we don't care about it.

//////////////////////////////////////////////////////////////////

/**
 * The DAGNODE ID is the HID of the corresponding CHANGESET.
 * This is the child in the child/parent relationship.
 *
 * This routine returns a COPY of the HID; you are responsible for freeing it.
 *
 * TODO ERIC Yes, let's make this not allocate.  Just return a pointer
 * to the private data.  malloc is slow, so performance concerns
 * suggest that we should less often rather than more.
 *
 * TODO ERIC But maybe we do need a naming convention of some kind to
 * help distinguish things that need to be freed vs. things that do
 * not.  The comment above seems to be suggesting _ref.  Or maybe any
 * function that returns something newly allocated should have alloc
 * in the name, or _a_.  Similarly, we want it to be quite clear when
 * we pass a pointer to a function, does the caller still own the
 * pointer or not?
 *
 * TODO JEFF In a parallel checkin I added the _ref version and
 * started using it in the test case that I was working on.  I
 * also added a similar routine to SG_chagneset.  Since this was
 * in the middle of an already large merge, I didn't want to start
 * something new here.  Let's convert the __get_id's to return a
 * pointer without allocating and revisit our naming scheme elsewhere.
 */
void SG_dagnode__get_id(SG_context* pCtx, const SG_dagnode * pdn, char** ppszidHidChangeset);

/**
 * The DAGNODE ID is the HID of the corresponding CHANGESET.
 * This is the child in the child/parent relationship.
 *
 * This routine returns a referencde to our internal copy of the HID.  You ***MUST NOT***
 * free this.  This string becomes undefined if you free the dagnode.
 */
void SG_dagnode__get_id_ref(SG_context* pCtx, const SG_dagnode * pdn, const char** ppszidHidChangeset);

//////////////////////////////////////////////////////////////////

/**
 * A DAGNODE can have any number of parents. We expect either 1 or 2,
 * but allow any number (to hopefully support multi-way merges like GIT).
 *
 * The order of the parents is not defined.  We will keep them
 * in some order internally (either the order added or sorted
 * order).  The only guarantee that we provide is that it is
 * safe to iterate over them as long as you don't call __add_
 * during the iteration.
 *
 * TODO We probably want to sort the parents to make certain
 * graph walking exercises more efficient - but it is not
 * essential (because we don't need to compute a HID on it).
 *
 * TODO We may decide to do the sort during the freeze.
 *
 * @defgroup SG_dagnode__parent (DAGNODE) Routines to Get/Set DAGNODE Parents.
 * @{
 */

/**
 * Add the given HID to the set of parents for the DAGNODE.
 *
 * We silently ignore the HID if it is already in the set.
 *
 * We make a copy of the contents of pszidHidParent; you still own it.
 *
 * Returns error if DAGNODE is frozen.
 */
void SG_dagnode__add_parent(SG_context* pCtx, SG_dagnode * pdn, const char* pszidHidParent);

/**
 * Return the number of parents.
 */
void SG_dagnode__count_parents(SG_context* pCtx, const SG_dagnode * pdn, SG_uint32 * pCount);

void SG_dagnode__get_parents(
	SG_context* pCtx,
	const SG_dagnode * pdn,
	SG_uint32* piCount,            /**< This returns the number of
							        * parents.  If there are
							        * none, this will be zero and the
							        * function will return
							        * SG_ERR_OK. */
	const char*** pppResult   /**< This returns an array of pointers
								 * to SG_id objects.  Caller must free
								 * this.  Caller must not free or
								 * alter the SG_ids. */
	);
/*
	Return the rbtree that contains the parents.  This is useful when you don't want
	to allocate new strings for a copy of the IDs.  You do not own the reference.

*/
void SG_dagnode__get_parents__rbtree_ref(
	SG_context* pCtx,
	const SG_dagnode * pdn,
	SG_rbtree ** ppRBTreeRef //You do not own this!  Caller should NOT free.
	);
/**
 * See if the given HID is a parent of this DAGNODE/CHANGESET.
 */
void SG_dagnode__is_parent(SG_context* pCtx, const SG_dagnode * pdn, const char* pszidHidTest, SG_bool * pbIsParent);

/**
 * @}
 */

//////////////////////////////////////////////////////////////////

/**
 * Freezing the DAGNODE prevents the child HID and the set of
 * parent HIDs from being changed.  We use an "extended constructor"
 * model for creating a DAGNODE; that is, allocate an empty one and
 * populate it with the parent HIDs as is conveient for
 * the caller and then freeze it.
 *
 * It is OK to freeze an already frozen DAGNODE.
 *
 * TODO decide if we want this to cause the parents to be sorted.
 */
void SG_dagnode__freeze(SG_context* pCtx, SG_dagnode * pdn);

/**
 * See if the given DAGNODE is frozen.
 */
void SG_dagnode__is_frozen(SG_context* pCtx, const SG_dagnode * pdn, SG_bool * pbFrozen);

//////////////////////////////////////////////////////////////////

/**
 * Get the DAGNODE GENERATION value.  This is an informative field
 * computed when the corresponding CHANGESET is created.  It is an
 * indication of the depth of the DAGNODE in the DAG.
 *
 * See the GENERATION discussion in SG_changeset.
 */
void SG_dagnode__get_generation(SG_context* pCtx, const SG_dagnode * pdn, SG_int32 * pGen);

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
void SG_dagnode_debug__dump(SG_context* pCtx,
							const SG_dagnode * pdn,
							SG_uint32 nrDigits,
							SG_uint32 indent,
							SG_string * pStringOutput);
#endif//DEBUG

//////////////////////////////////////////////////////////////////

/**
 * convert in-memory dagnode into a vhash.
 *
 * we only use this for pushing/pulling dagnodes as part of a
 * dagfrag when talking with another repo or building a zip
 * file for external exchange with another repo.
 *
 * we *DO NOT* store dagnodes like this in the DAG.
 *
 * pvhShared is optional and allows pool sharing if appropriate.
 *
 * You must free the returned vhash.
 */
void SG_dagnode__to_vhash__shared(SG_context* pCtx,
								  const SG_dagnode * pDagnode,
									  SG_vhash * pvhShared,
									  SG_vhash ** ppvhNew);

/**
 * deserialize a dagnode from a vhash into a regular dagnode.
 *
 * we only use this for pushing/pulling dagnodes as part of a
 * dagfrag when talking with another repo or importing from a
 * zip file from an external exchange with another repo.
 *
 * we *DO NOT* store dagnodes like this in the DAG.
 *
 * You must free the returned dagnode.
 *
 */
void SG_dagnode__alloc__from_vhash(SG_context* pCtx,
								   SG_dagnode ** ppNew,
									   const SG_vhash * pvhDagnode);


//////////////////////////////////////////////////////////////////

// TODO add convenience fields to DAGNODE to help us walk an
// TODO in-memory graph of the nodes.  this might include things
// TODO like counters to tell how many times we've touched a
// TODO node during a walk.
// TODO
// TODO this might also include a "leaf" flag.
// TODO
// TODO this includes an optional SG_changeset pointer to keep
// TODO track of the corresponding full CHANGESET if we needed
// TODO load it.

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_DAGNODE_PROTOTYPES_H
