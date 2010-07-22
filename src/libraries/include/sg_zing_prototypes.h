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
 * @file sg_zing_prototypes.h
 *
 */

#ifndef H_SG_ZING_PROTOTYPES_H
#define H_SG_ZING_PROTOTYPES_H

BEGIN_EXTERN_C

void SG_zing__get_record__vhash(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_csid,
        const char* psz_recid,
        SG_vhash** ppvh
        );

void SG_zing__begin_tx(
    SG_context* pCtx,
	SG_repo* pRepo,
    SG_uint32 iDagNum,
    const char* who,
    const char* psz_hid_baseline,
	SG_zingtx** ppTx
	);

void SG_zingtx__add_parent(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_hid_cs_parent
        );

void SG_zing__commit_tx(
	SG_context* pCtx,
    SG_int64 when,
	SG_zingtx** ppTx,
    SG_changeset** ppcs,
    SG_dagnode** ppdn,
    SG_varray** ppva_constraint_violations
    );

void SG_zing__abort_tx(
	SG_context* pCtx,
	SG_zingtx** ppTx);

void SG_zingtx__create_new_record(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_rectype,
        SG_zingrecord** pprec
        );

void SG_zingrecord__set_field__dagnode(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char* psz_hid
        );

void SG_zingrecord__set_field__attachment__pathname(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_pathname** ppPath
        );

void SG_zingrecord__set_field__string(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char* psz_val
        );

void SG_zingrecord__set_field__userid(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char* psz_val
        );

void SG_zingrecord__set_field__datetime(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_int64 val
        );

void SG_zingrecord__set_field__int(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_int64 val
        );

void SG_zingrecord__set_field__bool(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_bool val
        );

void SG_zingrecord__get_field__dagnode(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char** ppsz_hid
        );

void SG_zingrecord__get_field__int(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_int64 *val
        );

void SG_zingrecord__get_field__datetime(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_int64 *val
        );

void SG_zingrecord__get_field__bool(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        SG_bool *val
        );

void SG_zingrecord__get_field__userid(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char** val
        );

void SG_zingrecord__get_field__string(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char** val
        );

void SG_zingrecord__get_field__attachment(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa,
        const char** val
        );

void SG_zingrecord__get_history(
        SG_context* pCtx,
        SG_zingrecord* pzrec,
        SG_varray** ppva
        );

void SG_zingrecord__get_rectype(
        SG_context* pCtx,
        SG_zingrecord* pzrec,
        const char** ppsz_rectype
        );

void SG_zingrecord__get_recid(
        SG_context* pCtx,
        SG_zingrecord* pzrec,
        const char** ppsz_recid
        );

void SG_zingrecord__remove_link(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        const char* psz_link_name_mine,
        const char* psz_recid_other
        );

void SG_zingrecord__set_singular_link_and_overwrite_other_if_singular(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        const char* psz_link_name_mine,
        const char* psz_recid_other
        );

void SG_zingrecord__add_link_and_overwrite_other_if_singular(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        const char* psz_link_name_mine,
        const char* psz_recid_other
        );

void SG_zingtemplate__get_rectype_info(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        SG_zingrectypeinfo** pp
        );

void SG_zingtemplate__get_field_attributes(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        const char* psz_field_name,
        SG_zingfieldattributes** ppzfa
        );

void SG_zingrecord__remove_field(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        SG_zingfieldattributes* pzfa
        );

void SG_zingtx__get_recid_from_uniquified_id_field(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_id,
        char** ppsz_recid ///< You own the result.
        );

void SG_zingtx__get_userid(
    SG_context *pCtx,
    SG_zingtx *pztx,
    const char **ppUserid);

void SG_zingtx__get_record(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid,
        SG_zingrecord** ppzrec
        );

void SG_zingtx__delete_record(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid
        );

void SG_zingtx__store_template(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_vhash** ppvh_template // We take ownership of the vhash and null your pointer for you.
        );

void SG_zing__get_template__csid(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_csid,
        SG_zingtemplate** ppzt
        );

void SG_zingtx__get_template(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zingtemplate** ppResult
        );

void SG_zing__get_template__hid_template(
        SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_hid_template,
        SG_zingtemplate** ppzt
        );

void SG_zingrecord__lookup_name(
        SG_context* pCtx,
        SG_zingrecord* pzr,
        const char* psz_name,
        SG_zingfieldattributes** ppzfa,
        SG_zinglinkattributes** ppzla,
        SG_zinglinksideattributes** ppzlsa_mine,
        SG_zinglinksideattributes** ppzlsa_other
        );

void SG_zinglinksideattributes__free(
        SG_context* pCtx,
        SG_zinglinksideattributes* pzlsa
        );

void SG_zinglinkattributes__free(
        SG_context* pCtx,
        SG_zinglinkattributes* pzla
        );

void SG_zingmerge__attempt_automatic_merge(
    SG_context* pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
    const SG_audit* pq,
    const char* psz_csid_node_0,
    const char* psz_csid_node_1,
    SG_dagnode** ppdn_merged,
    SG_varray** ppva_errors,
    SG_varray* pva_log
    );

void SG_zing__get_leaf__fail_if_needs_merge(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        char** pp
        );

void sg_zing__init_new_repo(
    SG_context* pCtx,
	SG_repo* pRepo
    );

void sg_zingfieldattributes__get_type(
	SG_context *pCtx,
	SG_zingfieldattributes *pzfa,
	SG_uint16 *pType);

void SG_zing__lookup_recid(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_state,
        const char* psz_rectype,
        const char* psz_field_name,
        const char* psz_field_value,
        char** psz_recid
        );

void SG_zingtx__lookup_recid(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_rectype,
        const char* psz_field_name,
        const char* psz_field_value,
        char** psz_recid
        );

void SG_zing__extract_one_from_slice__string(
        SG_context* pCtx,
        SG_varray* pva,
        const char* psz_name,
        SG_varray** ppva2
        );

void SG_zing__query(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_state,
        const char* psz_rectype,
        const char* psz_where,
        const char* psz_sort,
        SG_int32 limit,
        SG_int32 skip,
        SG_stringarray* psa_slice_fields,
        SG_varray** ppva_sliced
        );

void SG_zing__query__one(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_state,
        const char* psz_rectype,
        const char* psz_where,
        SG_stringarray* psa_slice_fields,
        SG_vhash** ppvh
        );

void SG_zing__get_leaf__merge_if_necessary(
        SG_context* pCtx,
        SG_repo* pRepo,
        const SG_audit* pq,
        SG_uint32 iDagNum,
        char** pp,
        SG_varray** ppva_errors,
        SG_varray** ppva_log
        );

void SG_zing__get_record(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_state,
        const char* psz_recid,
        SG_dbrecord** ppdbrec
        );

void SG_zingtemplate__validate(
        SG_context* pCtx,
        SG_vhash* pvh,
        SG_uint32* pi_version
        );

void sg_zing__validate_templates(
    SG_context* pCtx
    );

void sg_zing__get_dbtop(
        SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_hid_cs,
        char** ppsz_hid_template,
        char** ppsz_hid_root
        );

void SG_zingrectypeinfo__free(SG_context* pCtx, SG_zingrectypeinfo* pztx);
void SG_zingfieldattributes__free(SG_context* pCtx, SG_zingfieldattributes* pztx);
void SG_zingtemplate__free(SG_context* pCtx, SG_zingtemplate* pztx);
void SG_zingtemplate__get_vhash(SG_context* pCtx, SG_zingtemplate* pThis, SG_vhash ** ppResult);
void SG_zingrecord__free(SG_context* pCtx, SG_zingrecord* pThis);

void SG_zingtx___find_links(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zinglinkattributes* pzla,
        const char* psz_recid,
        SG_bool bFrom,
        SG_rbtree** pprb
        );

void SG_zing__find_all_links(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 dagnum,
        const char* psz_hid_cs_leaf,
        SG_rbtree* prb_link_names,
        SG_vhash* pvh_top
        );

void SG_zingtx___find_links_list(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zinglinkattributes* pzla,
        SG_rbtree* prb_recidlist,
        SG_bool bFrom,
        SG_rbtree** pprb
        );

void SG_zingrecord__to_dbrecord(
        SG_context* pCtx,
        SG_zingrecord* pzrec,
        SG_dbrecord** ppResult
        );

void SG_zingtx__add_link__unpacked(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_recid_from,
        const char* psz_recid_to,
        const char* psz_link_name
        );

void SG_zing__now_free_all_cached_templates(
        SG_context* pCtx
        );

void SG_zing__auto_merge_all_dags(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_varray** ppvaErrors,
	SG_varray** ppvaLog);

END_EXTERN_C

#endif
