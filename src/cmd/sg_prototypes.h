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
 * @file sg_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_PROTOTYPES_H
#define H_SG_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void sg_report_portability_warnings(SG_context * pCtx, SG_pendingtree* pPendingTree);
void sg_report_action_log(SG_context * pCtx, SG_pendingtree* pPendingTree);

//////////////////////////////////////////////////////////////////

void do_cmd_merge(SG_context * pCtx,
				  SG_option_state * pOptSt);

void do_cmd_resolve(SG_context * pCtx,
					SG_option_state * pOptSt,
					SG_uint32 count_args,
					const char ** paszArgs);

void do_cmd_update(SG_context * pCtx,
				   SG_option_state * pOptSt);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_PROTOTYPES_H
