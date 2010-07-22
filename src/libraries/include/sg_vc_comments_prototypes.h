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
 * @file sg_vc_comments_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_VC_COMMENTS_PROTOTYPES_H
#define H_SG_VC_COMMENTS_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_vc_comments__add(SG_context* pCtx, SG_repo* pRepo, const char* psz_hid_cs_target, const char* psz_comment, const SG_audit* pq);

void SG_vc_comments__lookup(SG_context* pCtx, SG_repo* pRepo, const char* psz_hid_cs, SG_varray** ppvacomments);

void SG_vc_comments__list_all(SG_context* pCtx, SG_repo* pRepo, SG_varray** ppvacomments);

void SG_vc_comments__list_for_given_changesets(SG_context* pCtx, SG_repo* pRepo, SG_varray** ppva_csid_list, SG_varray** ppva_comments);

END_EXTERN_C;

#endif //H_SG_VC_COMMENTS_PROTOTYPES_H

