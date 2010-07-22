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
 * @file sg_sync_prototypes.h
 *
 */

#ifndef H_SG_SYNC_PROTOTYPES_H
#define H_SG_SYNC_PROTOTYPES_H

BEGIN_EXTERN_C

/**
 * Compares two repositories, returns whether or not their DAGs are identical.
 * This doesn't compare blobs and doesn't worry about differences in repo storage, compression, sparseness, etc.
 * However because a dagnode holds a HID that uniquely identifies its contents, this can be used to determine if
 * two repos are effectively identical.
 */
void SG_sync__compare_repo_dags(SG_context* pCtx,
								SG_repo* pRepo1,
								SG_repo* pRepo2,
								SG_bool* pbIdentical);

void SG_sync__compare_repo_blobs(SG_context* pCtx,
								 SG_repo* pRepo1,
								 SG_repo* pRepo2,
								 SG_bool* pbIdentical);

void SG_sync__add_n_generations(SG_context* pCtx,
										   SG_repo* pRepo,
										   const char* pszDagnodeHid,
										   SG_rbtree* prbDagnodeHids,
										   SG_uint32 generations);

void SG_sync__add_blobs_to_fragball(SG_context* pCtx,
									SG_repo* pRepo,
									SG_pathname* pPath_fragball,
									SG_vhash* pvh_missing_blobs);



END_EXTERN_C

#endif

