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
 * @file sg_dagquery_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_DAGQUERY_PROTOTYPES_H
#define H_SG_DAGQUERY_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * See if/how the 2 dagnodes/changesets are related.
 *
 * Returns SG_DAGQUERY_RELATIONSHIP_ of hid1 relative to hid2.
 *
 * Currently, this routine focuses on ancestor/descendor/peer
 * relationships.  Eventually, it will also handle named branches.
 */
void SG_dagquery__how_are_dagnodes_related(SG_context * pCtx,
										   SG_repo * pRepo,
										   const char * pszHid1,
										   const char * pszHid2,
										   SG_dagquery_relationship * pdqRel);

/**
 * Search the DAG for LEAVES/HEADS that are descendants of the given DAGNODE.
 *
 * Returns SG_DAGQUERY_FIND_HEADS_STATUS__ and an rbtree containing the HIDs
 * of the relevant LEAVES/HEADS.
 *
 * Set bStopIfMultiple if you only want the answer if there is a SINGLE
 * descendant leaf (either the starting node or a single proper descendant).
 * This allows us to stop the search if there are multiple descendant leaves
 * when you can't use them.  (In this case, we return __MULTIPLE status and
 * a null rbtree.)
 *
 * Eventually, this routine will also handle named branches and limit the
 * results to LEAVES/HEADS that belong to the same named branch as the starting
 * DAGNODE.
 */
void SG_dagquery__find_descendant_heads(SG_context * pCtx,
										SG_repo * pRepo,
										const char * pszHidStart,
										SG_bool bStopIfMultiple,
										SG_dagquery_find_head_status * pdqfhs,
										SG_rbtree ** pprbHeads);

/**
 * Search the DAG for LEAVES/HEADS that are descendants of the given DAGNODE.
 *
 * Returns SG_DAGQUERY_FIND_HEADS_STATUS__.
 *
 * If there is exactly one LEAF/HEAD, copy the HID into the provided buffer.
 *
 * Eventually, this routine will also handle named branches and limit the
 * results to LEAVES/HEADS that belong to the same named branch as the starting
 * DAGNODE.
 *
 * This is a convenience wrapper for SG_dagquery__find_descendant_heads()
 * when you know that you only want 1 unique result.
 */
void SG_dagquery__find_single_descendant_head(SG_context * pCtx,
											  SG_repo * pRepo,
											  const char * pszHidStart,
											  SG_dagquery_find_head_status * pdqfhs,
											  char * bufHidHead,
											  SG_uint32 lenBufHidHead);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_DAGQUERY_PROTOTYPES_H
