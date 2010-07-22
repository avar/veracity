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
 * sg_history.h
 *
 *  Created on: Mar 23, 2010
 *      Author: jeremy
 */

#ifndef SG_HISTORY_H_
#define SG_HISTORY_H_

void SG_history__query(SG_context * pCtx,
						 SG_pendingtree * pPendingTree,
						 SG_repo * pRepo,
						 SG_uint32 count_args, const char ** paszArgs,
						 const char * const* pasz_changesets,
						 SG_uint32 nCountChangesets,
						 const char* pStrUser,
						 const char* pStrStamp,
						 SG_uint32 nResultLimit,
						 SG_int64 nFromDate,
						 SG_int64 nToDate,
						 SG_bool bAllLeaves,
						 SG_bool bForceDagWalk,
						 SG_varray ** ppVArrayResults);

void SG_history__get_id_and_parents(SG_context* pCtx,
									SG_repo* pRepo,
									SG_dagnode * pCurrentDagnode,
									SG_vhash** ppvh_changesetDescription);

void SG_history__get_changeset_description(SG_context* pCtx, 
										   SG_repo* pRepo, 
										   const char* pszChangesetId,
										   SG_bool bChildren,
										   SG_vhash** pvhChangesetDescription
										   );

void SG_history__get_changeset_comments(SG_context* pCtx, 
										SG_repo* pRepo, 
										const char* pszDagNodeHID, 
										SG_varray** ppvaComments);

#endif /* SG_HISTORY_H_ */
