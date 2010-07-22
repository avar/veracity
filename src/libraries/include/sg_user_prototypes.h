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
 * @file sg_user_prototypes.h
 *
 */

#ifndef H_SG_USER_PROTOTYPES_H
#define H_SG_USER_PROTOTYPES_H

BEGIN_EXTERN_C

void SG_user__set_user__repo(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_email
    );

void SG_user__create(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_email
    );

void SG_user__list_all(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_varray** ppva
    );

void SG_group__list(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_varray** ppva
    );

void SG_user__lookup_by_userid(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_userid,
    SG_vhash** ppvh
    );

void SG_user__lookup_by_email(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_email,
    SG_vhash** ppvh
    );
void SG_user__get_email_for_repo(
	SG_context * pCtx,
	SG_repo* pRepo,
	const char ** ppsz_email);

void SG_group__create(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_name
    );

void SG_group__add_users(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_group_name,
    const char** paszMemberNames,
    SG_uint32 count_names
    );

void SG_group__add_subgroups(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_group_name,
    const char** paszMemberNames,
    SG_uint32 count_names
    );

void SG_group__remove_users(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_group_name,
    const char** paszMemberNames,
    SG_uint32 count_names
    );

void SG_group__remove_subgroups(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_group_name,
    const char** paszMemberNames,
    SG_uint32 count_names
    );

void SG_group__list_members(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_group_name,
    SG_varray** ppva
    );

END_EXTERN_C

#endif
