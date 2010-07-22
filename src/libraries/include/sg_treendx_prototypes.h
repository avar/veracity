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
 * @file sg_treendx_prototypes.h
 *
 */

#ifndef H_SG_TREENDX_H
#define H_SG_TREENDX_H

BEGIN_EXTERN_C

/**
 * Create a new treendx object.  If the sql file doesn't exist yet,
 * it will be created.
 *
 */
void SG_treendx__open(
	SG_context*,

	SG_repo* pRepo,                         /**< You still own this, but don't
	                                          free it until the treendx is
	                                          freed.  It will retain this
	                                          pointer. */

	SG_uint32 iDagNum,                      /**< Which dag in that repo are we
	                                          indexing? */

	SG_pathname* pPath,

	SG_bool bQueryOnly,

	SG_treendx** ppNew
	);

/**
 * Release the memory for an SG_treendx object.  This does
 * nothing at all to the treendx on disk.
 *
 */
void SG_treendx__free(SG_context* pCtx, SG_treendx* pdbc);

void SG_treendx__update__multiple(SG_context* pCtx, SG_treendx* pTreeNdx, SG_stringarray* psa);
void SG_treendx__get_path_in_dagnode(SG_context* pCtx, SG_treendx* pTreeNdx, const char* psz_search_item_gid, const char* psz_changeset, SG_treenode_entry ** ppTreeNodeEntry);
void SG_treendx__get_all_paths(SG_context* pCtx, SG_treendx* pTreeNdx, const char* psz_gid, SG_stringarray ** ppResults);
END_EXTERN_C

#endif
