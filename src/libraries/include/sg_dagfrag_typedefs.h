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
 * @file sg_dagfrag_typedefs.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_DAGFRAG_TYPEDEFS_H
#define H_SG_DAGFRAG_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

typedef struct _SG_dagfrag SG_dagfrag;

enum _SG_dagfrag_state
{
	SG_DFS_UNKNOWN				= 0,	/**< private invalid state. */
	SG_DFS_END_FRINGE			= 1,	/**< dagnode is not a member of the fragment, but is a parent of a member. */
	SG_DFS_INTERIOR_MEMBER		= 2,	/**< dagnode is a member of the fragment, but not a starting point. */
	SG_DFS_START_MEMBER			= 3,	/**< dagnode is a member of the fragment and a starting point. */
};

typedef enum _SG_dagfrag_state SG_dagfrag_state;

typedef void (SG_dagfrag__foreach_end_fringe_callback)(SG_context * pCtx,
													   const char * szHid,
													   void * pVoidCallerData);

typedef void (SG_dagfrag__foreach_member_callback)(SG_context * pCtx,
												   const SG_dagnode * pDagnodeMember,
												   SG_dagfrag_state qs,
												   void * pVoidCallerData);

//////////////////////////////////////////////////////////////////

/**
 * This is like a callback.  It can be given to sg_dagfrag__alloc()
 * to let the dagfrag code request a dagnode from disk.  This allows
 * the dagfrag code to not know whether the source is a (raw) repo,
 * a cache, or a hybrid of some kind.
 *
 * This is not specific to dagfrags.  We may also use this for computing
 * the LCA, for example.
 */
typedef void FN__sg_fetch_dagnode(SG_context * pCtx,
								  void * pVoidFetchDagnodeData,
								  const char * szHidDagnode,
								  SG_dagnode ** ppDagnode);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_DAGFRAG_TYPEDEFS_H
