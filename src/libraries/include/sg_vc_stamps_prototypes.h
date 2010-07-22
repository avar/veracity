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
 * @file sg_vc_stamps_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_VC_STAMPS_PROTOTYPES_H
#define H_SG_VC_STAMPS_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_vc_stamps__add(SG_context* pCtx, SG_repo* pRepo, const char* psz_hid_cs, const char* psz_annotation, const SG_audit* pq);

void SG_vc_stamps__lookup(SG_context* pCtx, SG_repo* pRepo, const char* psz_hid_cs, SG_varray** ppva_vc_stamps);

void SG_vc_stamps__is_stamp_already_applied(SG_context* pCtx, SG_repo* pRepo, const char* psz_hid_cs, const char * psz_stamp, SG_bool * pbStampIsThere);

/**
 * Return an array of all stamps in the repo.  It's a varray of vhashes.  The fields filled in are "stamp" and "count".
 */
void SG_vc_stamps__list_all_stamps(SG_context* pCtx, SG_repo* pRepo, SG_varray ** pVArrayResults);

void SG_vc_stamps__list_all(SG_context* pCtx, SG_repo* pRepo, SG_varray** ppva_vc_stamps);

void SG_vc_stamps__list_for_given_changesets(SG_context* pCtx, SG_repo* pRepo, SG_varray** ppva_csid_list, SG_varray** ppva);

/**
 * Lookup all of the changesetids that have the given stamp.
 */
void SG_vc_stamps__lookup_by_stamp(SG_context * pCtx, SG_repo* pRepo, const char * pszStamp, SG_stringarray ** pstringarray_Results);

/**
 * Remove stamps from a given changeset.  The psz_dagnode_hid is required.   If no stamps are supplied in the paszArgs, all stamps on that dagnode are removed.
 */
void SG_vc_stamps__remove(SG_context* pCtx, SG_repo* pRepo, SG_audit* pq, const char * psz_dagnode_hid, SG_uint32 count_args, const char * const* paszArgs);

END_EXTERN_C;

#endif //H_SG_VC_STAMPS_PROTOTYPES_H

