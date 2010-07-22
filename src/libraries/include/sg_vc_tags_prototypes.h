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
 * @file sg_vc_tags_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_VC_TAGS_PROTOTYPES_H
#define H_SG_VC_TAGS_PROTOTYPES_H

BEGIN_EXTERN_C;

/**
 * Add one tag to the repo.  If the tag already exists,
 * this will fail.
 */
void SG_vc_tags__add(SG_context*, SG_repo* pRepo, const char* psz_hidcs, const char* psz_tag, const SG_audit* pq);

/**
 * Remove a tag from the repo.  If the tag does not exist,
 * this will fail. */
void SG_vc_tags__remove(SG_context*, SG_repo* pRepo, SG_audit* pq, SG_uint32 count_args, const char** paszArgs);

/**
 * Return a list of all tags.  The rbtree has the tag names as the keys.  The
 * assoc data is the HID of the changeset to which the tag is mapped. */
void SG_vc_tags__list(SG_context*, SG_repo* pRepo, SG_rbtree** pprb);

void SG_vc_tags__list_all(SG_context* pCtx, SG_repo* pRepo, SG_varray** ppva_tags);

void SG_vc_tags__list_for_given_changesets(SG_context* pCtx, SG_repo* pRepo, SG_varray** ppva_csid_list, SG_varray** ppva);

/**
 * Given a tags tree (from SG_tags__list), return a new one which has the HIDs
 * as keys instead of the tags.  If a HID maps to more than one tag, only one
 * of them is returned, and which one is undefined. */
void SG_vc_tags__build_reverse_lookup(SG_context*, const SG_rbtree* prbTagToHid, SG_rbtree** pprbHidToTag);

void SG_vc_tags__lookup(SG_context*, SG_repo* pRepo, const char* psz_hid_cs, SG_varray** ppva_tags);

void SG_vc_tags__lookup__tag(SG_context*, SG_repo* pRepo, const char* psz_tag, char** ppsz_hid_cs);

END_EXTERN_C;

#endif //H_SG_VC_TAGS_PROTOTYPES_H

