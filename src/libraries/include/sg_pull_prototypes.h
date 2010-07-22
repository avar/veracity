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
 * @file sg_pull_prototypes.h
 *
 */

#ifndef H_SG_PULL_PROTOTYPES_H
#define H_SG_PULL_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_pull__all(SG_context* pCtx,
				  const char* pszPullIntoRepoDescriptorName,
				  SG_client* pClient,
				  SG_varray** ppvaErr,
				  SG_varray** ppvaLog);

void SG_pull__clone(
	SG_context* pCtx,
	const char* pszPullIntoRepoDescriptorName,
	SG_client* pClient);

void SG_pull__begin(
	SG_context* pCtx,
	const char* pszPullIntoRepoDescriptorName, 
	SG_client* pClient,
	SG_pull** ppPull);

void SG_pull__add(
	SG_context* pCtx,
	SG_pull* pPull,
	SG_uint32 iDagnum,
	SG_rbtree* prbDagnodes,
	SG_rbtree* prbTags,
	SG_rbtree* prbDagnodePrefixes);

void SG_pull__commit(
	SG_context* pCtx,
	SG_pull** ppPull,
	SG_varray** ppvaZingErr,
	SG_varray** ppvaZingLog);

void SG_pull__abort(
	SG_context* pCtx,
	SG_pull** ppPull);

void SG_pull__all__list_incoming(
	SG_context* pCtx,
	const char* pszPullIntoRepoDescriptorName,
	SG_client* pClient,
	SG_varray** ppvaInfo)

END_EXTERN_C;

#endif//H_SG_PULL_PROTOTYPES_H

