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

#include <sg.h>

void SG_user__set_user__repo(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_email
    )
{
    SG_vhash* pvh_user = NULL;
    char* psz_admin_id = NULL;
    const char* psz_userid = NULL;
    SG_string* pstr_path = NULL;

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr_path)  );

    if (pRepo)
    {
        SG_ERR_CHECK(  SG_user__lookup_by_email(pCtx, pRepo, psz_email, &pvh_user)  );
        if (!pvh_user)
        {
            SG_ERR_THROW(  SG_ERR_USER_NOT_FOUND  );
        }

        SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_user, SG_ZING_FIELD__RECID, &psz_userid)  );
        SG_ERR_CHECK(  SG_repo__get_admin_id(pCtx, pRepo, &psz_admin_id)  );

        // we store this userid under the admin scope of the repo we were given
        SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s/%s/%s",
                    SG_LOCALSETTING__SCOPE__ADMIN,
                    psz_admin_id,
                    SG_LOCALSETTING__USERID
                    )  );
        SG_ERR_CHECK(  SG_localsettings__update__sz(pCtx, SG_string__sz(pstr_path), psz_userid)  );
    }

    // AND we store this email address in machine scope for fallback lookups
    SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr_path, "%s/%s",
                SG_LOCALSETTING__SCOPE__MACHINE,
                SG_LOCALSETTING__USEREMAIL
                )  );
    SG_ERR_CHECK(  SG_localsettings__update__sz(pCtx, SG_string__sz(pstr_path), psz_email)  );

fail:
    SG_STRING_NULLFREE(pCtx, pstr_path);
    SG_NULLFREE(pCtx, psz_admin_id);
    SG_VHASH_NULLFREE(pCtx, pvh_user);
}

void SG_group__create(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_name
    )
{
    char* psz_hid_cs_leaf = NULL;
    SG_zingtx* pztx = NULL;
    SG_zingrecord* prec = NULL;
    SG_dagnode* pdn = NULL;
    SG_changeset* pcs = NULL;
    SG_zingtemplate* pzt = NULL;
    SG_zingfieldattributes* pzfa = NULL;
    SG_audit q;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__USERS, &psz_hid_cs_leaf)  );

    SG_ERR_CHECK(  SG_audit__init(pCtx, &q, pRepo, SG_AUDIT__WHEN__NOW,  SG_AUDIT__WHO__FROM_SETTINGS)  );

    /* start a changeset */
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, SG_DAGNUM__USERS, q.who_szUserId, psz_hid_cs_leaf, &pztx)  );
    SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_hid_cs_leaf)  );
	SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pzt)  );

	SG_ERR_CHECK(  SG_zingtx__create_new_record(pCtx, pztx, "group", &prec)  );

    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, "group", "name", &pzfa)  );
    SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, prec, pzfa, psz_name) );

    /* commit the changes */
	SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, q.when_int64, &pztx, &pcs, &pdn, NULL)  );

    // fall thru

fail:
    if (pztx)
    {
        SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pztx)  );
    }
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
}

void SG_user__create(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_email
    )
{
    char* psz_hid_cs_leaf = NULL;
    SG_zingtx* pztx = NULL;
    SG_zingrecord* prec = NULL;
    SG_dagnode* pdn = NULL;
    SG_changeset* pcs = NULL;
    SG_zingtemplate* pzt = NULL;
    SG_zingfieldattributes* pzfa = NULL;
    SG_audit q;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__USERS, &psz_hid_cs_leaf)  );

    SG_ERR_CHECK(  SG_audit__init__maybe_nobody(pCtx, &q, pRepo, SG_AUDIT__WHEN__NOW,  SG_AUDIT__WHO__FROM_SETTINGS)  );

    /* start a changeset */
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, SG_DAGNUM__USERS, q.who_szUserId, psz_hid_cs_leaf, &pztx)  );
    SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_hid_cs_leaf)  );
	SG_ERR_CHECK(  SG_zingtx__get_template(pCtx, pztx, &pzt)  );

	SG_ERR_CHECK(  SG_zingtx__create_new_record(pCtx, pztx, "user", &prec)  );

    SG_ERR_CHECK(  SG_zingtemplate__get_field_attributes(pCtx, pzt, "user", "email", &pzfa)  );
    SG_ERR_CHECK(  SG_zingrecord__set_field__string(pCtx, prec, pzfa, psz_email) );

    /* commit the changes */
	SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, q.when_int64, &pztx, &pcs, &pdn, NULL)  );

    // fall thru

fail:
    if (pztx)
    {
        SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pztx)  );
    }
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
}

void SG_user__get_email_for_repo(SG_context * pCtx, SG_repo* pRepo, const char ** ppsz_email)
{
	char * psz_admin_id = NULL;
	char * psz_userid = NULL;
	const char * psz_email_temp = NULL;
	SG_string * pstr_path = NULL;
	SG_vhash * pvh_userhash = NULL;
	if (pRepo)
	{
		SG_ERR_CHECK(  SG_repo__get_admin_id(pCtx, pRepo, &psz_admin_id)  );

		// we store this userid under the admin scope of the repo we were given
		SG_ERR_CHECK(  SG_string__alloc__format(pCtx, &pstr_path, "%s/%s/%s",
					SG_LOCALSETTING__SCOPE__ADMIN,
					psz_admin_id,
					SG_LOCALSETTING__USERID
					)  );
		SG_ERR_CHECK(  SG_localsettings__get__sz(pCtx, SG_string__sz(pstr_path), pRepo, &psz_userid, NULL)  );
		if (psz_userid == NULL || *psz_userid == 0)
			SG_ERR_THROW(SG_ERR_USER_NOT_FOUND);
		SG_ERR_CHECK(  SG_user__lookup_by_userid(pCtx, pRepo, psz_userid, &pvh_userhash)  );
		SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvh_userhash, "email", &psz_email_temp)  );
		SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_email_temp, (char**)ppsz_email)  );
	}

fail:
	SG_VHASH_NULLFREE(pCtx, pvh_userhash);
	SG_NULLFREE(pCtx, psz_admin_id);
	SG_NULLFREE(pCtx, psz_userid);
	SG_STRING_NULLFREE(pCtx, pstr_path);

}

void SG_user__lookup_by_userid(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_userid,
    SG_vhash** ppvh
    )
{
    char* psz_hid_cs_leaf = NULL;
    SG_vhash* pvh_user = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__USERS, &psz_hid_cs_leaf)  );

    SG_ERR_CHECK(  SG_zing__get_record__vhash(pCtx, pRepo, SG_DAGNUM__USERS, psz_hid_cs_leaf, psz_userid, &pvh_user)  );

    *ppvh = pvh_user;
    pvh_user = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_user);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}

void SG_user__lookup_by_email(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_email,
    SG_vhash** ppvh
    )
{
    char* psz_hid_cs_leaf = NULL;
    SG_vhash* pvh_user = NULL;
    char buf_where[256 + 64];
    SG_stringarray* psa_fields = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__USERS, &psz_hid_cs_leaf)  );

    if (psz_hid_cs_leaf)
    {
        SG_ERR_CHECK(  SG_sprintf(pCtx, buf_where, sizeof(buf_where), "email == '%s'", psz_email)  );

        SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 4)  );
        SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
        SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "email")  );
        //SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "prefix")  );
        SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "key")  );
        SG_ERR_CHECK(  SG_zing__query__one(pCtx, pRepo, SG_DAGNUM__USERS, psz_hid_cs_leaf, "user", buf_where, psa_fields, &pvh_user)  );
    }

    *ppvh = pvh_user;
    pvh_user = NULL;

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
    SG_VHASH_NULLFREE(pCtx, pvh_user);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
}

void SG_user__list_all(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_varray** ppva
    )
{
    SG_string* pstr_where = NULL;
    SG_stringarray* psa_results = NULL;
    char* psz_hid_cs_leaf = NULL;
    SG_vhash* pvh_user = NULL;
    SG_varray* pva_result = NULL;
    SG_stringarray* psa_fields = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__USERS, &psz_hid_cs_leaf)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 3)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "email")  );
    //SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "prefix")  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__USERS, psz_hid_cs_leaf, "user", NULL, "email #ASC", 0, 0, psa_fields, &pva_result)  );

    *ppva = pva_result;
    pva_result = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_user);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
    SG_VARRAY_NULLFREE(pCtx, pva_result);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_results);
    SG_STRING_NULLFREE(pCtx, pstr_where);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
}

void SG_group__list(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_varray** ppva
    )
{
    SG_string* pstr_where = NULL;
    SG_stringarray* psa_results = NULL;
    char* psz_hid_cs_leaf = NULL;
    SG_vhash* pvh_group = NULL;
    SG_varray* pva_result = NULL;
    SG_stringarray* psa_fields = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__USERS, &psz_hid_cs_leaf)  );

    SG_ERR_CHECK(  SG_STRINGARRAY__ALLOC(pCtx, &psa_fields, 2)  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "recid")  );
    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa_fields, "name")  );
    SG_ERR_CHECK(  SG_zing__query(pCtx, pRepo, SG_DAGNUM__USERS, psz_hid_cs_leaf, "group", NULL, "name #ASC", 0, 0, psa_fields, &pva_result)  );

    *ppva = pva_result;
    pva_result = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_group);
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
    SG_VARRAY_NULLFREE(pCtx, pva_result);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_results);
    SG_STRING_NULLFREE(pCtx, pstr_where);
    SG_STRINGARRAY_NULLFREE(pCtx, psa_fields);
}

void SG_group__add_users(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_group_name,
    const char** paszMemberNames,
    SG_uint32 count_names
    )
{
    char* psz_hid_cs_leaf = NULL;
    SG_zingtx* pztx = NULL;
    SG_dagnode* pdn = NULL;
    SG_changeset* pcs = NULL;
    SG_audit q;
    SG_uint32 i = 0;
    char* psz_recid_group = NULL;
    char* psz_recid_user = NULL;

    SG_ERR_CHECK(  SG_zing__get_leaf__fail_if_needs_merge(pCtx, pRepo, SG_DAGNUM__USERS, &psz_hid_cs_leaf)  );

    SG_ERR_CHECK(  SG_audit__init(pCtx, &q, pRepo, SG_AUDIT__WHEN__NOW,  SG_AUDIT__WHO__FROM_SETTINGS)  );

    // lookup the recid of the group
    SG_ERR_CHECK(  SG_zing__lookup_recid(pCtx, pRepo, SG_DAGNUM__USERS, psz_hid_cs_leaf, "group", "name", psz_group_name, &psz_recid_group)  );

    /* start a changeset */
    SG_ERR_CHECK(  SG_zing__begin_tx(pCtx, pRepo, SG_DAGNUM__USERS, q.who_szUserId, psz_hid_cs_leaf, &pztx)  );
    SG_ERR_CHECK(  SG_zingtx__add_parent(pCtx, pztx, psz_hid_cs_leaf)  );

    for (i=0; i<count_names; i++)
    {
        // lookup the recid of the user
        SG_ERR_CHECK(  SG_zing__lookup_recid(pCtx, pRepo, SG_DAGNUM__USERS, psz_hid_cs_leaf, "user", "email", paszMemberNames[i], &psz_recid_user)  );
        SG_ERR_CHECK(  SG_zingtx__add_link__unpacked(pCtx, pztx, psz_recid_user, psz_recid_group, "member")  );
        SG_NULLFREE(pCtx, psz_recid_user);
    }

    /* commit the changes */
	SG_ERR_CHECK(  SG_zing__commit_tx(pCtx, q.when_int64, &pztx, &pcs, &pdn, NULL)  );

    // fall thru

fail:
    if (pztx)
    {
        SG_ERR_IGNORE(  SG_zing__abort_tx(pCtx, &pztx)  );
    }
    SG_NULLFREE(pCtx, psz_hid_cs_leaf);
    SG_NULLFREE(pCtx, psz_recid_group);
    SG_NULLFREE(pCtx, psz_recid_user);
    SG_DAGNODE_NULLFREE(pCtx, pdn);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
}

void SG_group__add_subgroups(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_group_name,
    const char** paszMemberNames,
    SG_uint32 count_names
    )
{
	SG_UNUSED(pCtx);
    SG_UNUSED(pRepo);
    SG_UNUSED(psz_group_name);
    SG_UNUSED(paszMemberNames);
	SG_UNUSED(count_names);

    SG_ERR_THROW_RETURN(  SG_ERR_NOTIMPLEMENTED  );
}

void SG_group__remove_users(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_group_name,
    const char** paszMemberNames,
    SG_uint32 count_names
    )
{
	SG_UNUSED(pCtx);
    SG_UNUSED(pRepo);
    SG_UNUSED(psz_group_name);
    SG_UNUSED(paszMemberNames);
    SG_UNUSED(count_names);

	SG_ERR_THROW_RETURN(  SG_ERR_NOTIMPLEMENTED  );
}

void SG_group__remove_subgroups(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_group_name,
    const char** paszMemberNames,
    SG_uint32 count_names
    )
{
	SG_UNUSED(pCtx);
    SG_UNUSED(pRepo);
    SG_UNUSED(psz_group_name);
    SG_UNUSED(paszMemberNames);
    SG_UNUSED(count_names);

    SG_ERR_THROW_RETURN(  SG_ERR_NOTIMPLEMENTED  );
}

void SG_group__list_members(
	SG_context* pCtx,
    SG_repo* pRepo,
    const char* psz_group_name,
    SG_varray** ppva
    )
{
	SG_UNUSED(pCtx);
    SG_UNUSED(pRepo);
    SG_UNUSED(psz_group_name);
    SG_UNUSED(ppva);

    SG_ERR_THROW_RETURN(  SG_ERR_NOTIMPLEMENTED  );
}

