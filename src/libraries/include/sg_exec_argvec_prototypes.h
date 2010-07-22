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
 * @file sg_exec_argvec_prototypes.h
 *
 * @details Wrapper and prototypes for argument vector used by sg_exec.
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_EXEC_ARGVEC_PROTOTYPES_H
#define H_SG_EXEC_ARGVEC_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_exec_argvec__alloc(SG_context* pCtx, SG_exec_argvec ** ppArgVec);

void SG_exec_argvec__free(SG_context * pCtx, SG_exec_argvec * pArgVec);

void SG_exec_argvec__append__sz(SG_context* pCtx, SG_exec_argvec * pArgVec, const char * sz);
void SG_exec_argvec__remove_last(SG_context* pCtx, SG_exec_argvec * pArgVec);

void SG_exec_argvec__length(SG_context* pCtx, const SG_exec_argvec * pArgVec, SG_uint32 * pLength);

/**
 * fetch the k-th argument into a SG_string.
 *
 * you do not own this string.
 */
void SG_exec_argvec__get(SG_context* pCtx, const SG_exec_argvec * pArgVec, SG_uint32 k, const SG_string ** ppStr);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_EXEC_ARGVEC_PROTOTYPES_H
