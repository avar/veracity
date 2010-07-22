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
 * @file sg_varpool.h
 *
 * @details SG_varpool is a way to have lots of variants pooled into
 * fewer mallocs.
 *
 */

#ifndef H_SG_VARPOOL_H
#define H_SG_VARPOOL_H

BEGIN_EXTERN_C

void SG_varpool__alloc(
    SG_context* pCtx,
	SG_varpool** ppNew,
	SG_uint32 subpool_space
	);

#if defined(DEBUG)
#define SG_VARPOOL__ALLOC(pCtx,ppNew,subpool_space)		SG_STATEMENT(	SG_varpool * _p = NULL;											\
																		SG_varpool__alloc(pCtx,&_p,subpool_space);						\
																		_sg_mem__set_caller_data(_p,__FILE__,__LINE__,"SG_varpool");	\
																		*(ppNew) = _p;													)
#else
#define SG_VARPOOL__ALLOC(pCtx,ppNew,subpool_space)		SG_varpool__alloc(pCtx,ppNew,subpool_space)
#endif

//////////////////////////////////////////////////////////////////

void SG_varpool__free(SG_context * pCtx, SG_varpool* pPool);

void SG_varpool__add(
	SG_context* pCtx,
	SG_varpool* pPool,
	SG_variant** ppResult
	);

END_EXTERN_C

#endif
