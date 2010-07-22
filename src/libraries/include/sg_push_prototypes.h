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
 * @file sg_push_prototypes.h
 *
 */

#ifndef H_SG_PUSH_PROTOTYPES_H
#define H_SG_PUSH_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_push__all(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_client* pClient,
	SG_bool bAllowHeadCountIncrease);

void SG_push__begin(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_client* pClient,
	SG_push** ppPush);

void SG_push__add(
	SG_context* pCtx,
	SG_push* pPush,
	SG_uint32 iDagnum,
	SG_rbtree* prbDagnodes);

void SG_push__commit(
	SG_context* pCtx,
	SG_push** ppPush,
	SG_bool bAllowHeadCountIncrease);

void SG_push__abort(
	SG_context* pCtx,
	SG_push** ppPush);

void SG_push__all__list_outgoing(
	SG_context* pCtx, 
	SG_repo* pSrcRepo, 
	SG_client* pClient, 
	SG_varray** ppvaInfo);

END_EXTERN_C;

#endif//H_SG_PUSH_PROTOTYPES_H

