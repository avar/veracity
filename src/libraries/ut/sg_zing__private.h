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

#ifndef H_SG_ZING__PRIVATE_H
#define H_SG_ZING__PRIVATE_H

BEGIN_EXTERN_C;

#define SG_ZING_DBDELTA__TEMPLATE      "template"

#define SG_ZING_FIELD__LINK__NAME    "*LINK*name"
#define SG_ZING_FIELD__LINK__FROM    "*LINK*from"
#define SG_ZING_FIELD__LINK__TO      "*LINK*to"

#define SG_ZING_CONSTRAINT_VIOLATION__TYPE         "type"
#define SG_ZING_CONSTRAINT_VIOLATION__RECID        "recid"
#define SG_ZING_CONSTRAINT_VIOLATION__OTHER_RECID  "other_recid"
#define SG_ZING_CONSTRAINT_VIOLATION__FIELD_NAME   "field_name"
#define SG_ZING_CONSTRAINT_VIOLATION__FIELD_VALUE  "field_value"
#define SG_ZING_CONSTRAINT_VIOLATION__LINK_NAME    "link_name"
#define SG_ZING_CONSTRAINT_VIOLATION__LINK_SIDE_NAME  "link_side_name"
#define SG_ZING_CONSTRAINT_VIOLATION__IDS               "ids"
#define SG_ZING_CONSTRAINT_VIOLATION__RECTYPE               "rectype"

// TODO use these #defines everywhere, not just in the validation code
#define SG_ZING_TEMPLATE__VERSION "version"
#define SG_ZING_TEMPLATE__RECTYPES "rectypes"
#define SG_ZING_TEMPLATE__DIRECTED_LINKTYPES "directed_linktypes"
#define SG_ZING_TEMPLATE__UNDIRECTED_LINKTYPES "undirected_linktypes"
#define SG_ZING_TEMPLATE__NO_RECID "no_recid"
#define SG_ZING_TEMPLATE__FIELDS "fields"
#define SG_ZING_TEMPLATE__DATATYPE "datatype"
#define SG_ZING_TEMPLATE__REQUIRED "required"
#define SG_ZING_TEMPLATE__DEFAULTVALUE "defaultvalue"
#define SG_ZING_TEMPLATE__DEFAULTFUNC "defaultfunc"
#define SG_ZING_TEMPLATE__GENCHARS "genchars"
#define SG_ZING_TEMPLATE__MIN "min"
#define SG_ZING_TEMPLATE__MAX "max"
#define SG_ZING_TEMPLATE__MINLENGTH "minlength"
#define SG_ZING_TEMPLATE__MAXLENGTH "maxlength"
#define SG_ZING_TEMPLATE__ALLOWED "allowed"
#define SG_ZING_TEMPLATE__SORT_BY_ALLOWED "sort_by_allowed"
#define SG_ZING_TEMPLATE__PROHIBITED "prohibited"
#define SG_ZING_TEMPLATE__READONLY "readonly"
#define SG_ZING_TEMPLATE__VISIBLE "visible"
#define SG_ZING_TEMPLATE__CONSTRAINTS "constraints"
#define SG_ZING_TEMPLATE__FORM "form"
#define SG_ZING_TEMPLATE__NAME "name"
#define SG_ZING_TEMPLATE__FROM "from"
#define SG_ZING_TEMPLATE__TO "to"
#define SG_ZING_TEMPLATE__SINGULAR "singular"
#define SG_ZING_TEMPLATE__TYPE "type"
#define SG_ZING_TEMPLATE__FUNCTION "function"
#define SG_ZING_TEMPLATE__FIELD_FROM "field_from"
#define SG_ZING_TEMPLATE__LINK_RECTYPES "link_rectypes"
#define SG_ZING_TEMPLATE__DAG "dag"
#define SG_ZING_TEMPLATE__MERGE_TYPE "merge_type"
#define SG_ZING_TEMPLATE__MERGE "merge"
#define SG_ZING_TEMPLATE__CALCULATED "calculated"
#define SG_ZING_TEMPLATE__UNIQIFY "uniqify"
#define SG_ZING_TEMPLATE__WHICH "which"
#define SG_ZING_TEMPLATE__AUTO "auto"
#define SG_ZING_TEMPLATE__OP "op"
#define SG_ZING_TEMPLATE__UNIQUE "unique"
#define SG_ZING_TEMPLATE__LABEL "label"
#define SG_ZING_TEMPLATE__HELP "help"
#define SG_ZING_TEMPLATE__TOOLTIP "tooltip"
#define SG_ZING_TEMPLATE__READONLY "readonly"
#define SG_ZING_TEMPLATE__VISIBLE "visible"
#define SG_ZING_TEMPLATE__SUGGESTED "suggested"
#define SG_ZING_TEMPLATE__ADDEND "addend"
#define SG_ZING_TEMPLATE__DEPENDS_ON "depends_on"

#define SG_ZING_MERGE__most_recent "most_recent"
#define SG_ZING_MERGE__least_recent "least_recent"
#define SG_ZING_MERGE__max "max"
#define SG_ZING_MERGE__min "min"
#define SG_ZING_MERGE__average "average"
#define SG_ZING_MERGE__sum "sum"
#define SG_ZING_MERGE__allowed_last "allowed_last"
#define SG_ZING_MERGE__allowed_first "allowed_first"
#define SG_ZING_MERGE__longest "longest"
#define SG_ZING_MERGE__shortest "shortest"
#define SG_ZING_MERGE__concat "concat"

struct SG_zingtx
{
    SG_repo* pRepo;
    SG_uint32 iDagNum;
    SG_zingtemplate* ptemplate;
    SG_bool b_template_dirty;
    char* psz_csid;
    char* psz_hid_delta;
    char* psz_hid_template;

    SG_rbtree* prb_records;
    SG_rbtree* prb_parents;

    SG_rbtree* prb_links_delete;
    SG_rbtree* prb_links_new;

    SG_rbtree* prb_dependencies;

    SG_rbtree* prb_rechecks;

    SG_bool b_in_commit;

    char buf_who[SG_GID_BUFFER_LENGTH];
};

struct sg_zinglink
{
	// a link is:  "<gid>:<gid>:<name><null>"

    char buf[
        SG_GID_ACTUAL_LENGTH
        + 1
        + SG_GID_ACTUAL_LENGTH
        + 1
        + SG_ZING_MAX_FIELD_NAME_LENGTH
        + 1
        ];
};

struct sg_zingaction
{
    const char* psz_type;
    const char* psz_function;
    const char* psz_field_from;
    const char* psz_field_to;
};

struct SG_zingfieldattributes
{
    const char* name;
    const char* rectype;
    SG_uint16 type;
    SG_varray* pva_automerge;
    SG_vhash* uniqify;

    // constraints which apply to all field types
    SG_bool required;

    union
    {
        struct
        {
            SG_bool has_defaultvalue;
            SG_bool defaultvalue;
        } _bool;

        struct
        {
            SG_bool has_defaultvalue;
            SG_bool has_min;
            SG_bool has_max;
            SG_int64 defaultvalue;
            SG_int64 val_min;
            SG_int64 val_max;
            SG_vector_i64* prohibited;
            SG_vector_i64* allowed;
            SG_bool unique;
        } _int;

        struct
        {
            SG_bool has_min;
            SG_bool has_max;
            SG_bool has_defaultvalue;
            const char* defaultvalue;
            SG_int64 val_min;
            SG_int64 val_max;
        } _datetime;

        struct
        {
            SG_bool has_defaultvalue;
            SG_bool has_minlength;
            SG_bool has_maxlength;
            const char* defaultvalue;
            SG_int64 val_minlength;
            SG_int64 val_maxlength;
            SG_stringarray* prohibited;
            SG_vhash* allowed;
            SG_bool sort_by_allowed;
            SG_bool unique;
            SG_bool has_defaultfunc;
            const char* defaultfunc;
            const char* genchars;
        } _string;

        struct
        {
            SG_bool has_defaultvalue;
            const char* defaultvalue;
        } _userid;

        struct
        {
            SG_bool has_maxlength;
            SG_int64 val_maxlength;
        } _attachment;

        struct
        {
            SG_uint32 dagnum;
        } _dagnode;

    } v;
};

struct SG_zinglinkattributes
{
    char* psz_link_name;

    // TODO
    // directed or not
    // cycles or not
};

struct SG_zingrectypeinfo
{
    SG_bool b_merge_type_is_record;
    SG_bool b_no_recid;
};

struct SG_zinglinksideattributes
{
    SG_bool bSingular;
    SG_rbtree* prb_rectypes;
    SG_bool bFrom;
    SG_bool bRequired;
};

void SG_zingtemplate__alloc(
        SG_context* pCtx,
        SG_vhash** ppvh,
        SG_zingtemplate** pptemplate
        );

void SG_zingtemplate__list_fields(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        SG_stringarray** pp
        );

void SG_zingtemplate__list_required_links(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        SG_rbtree** pp_from,
        SG_rbtree** pp_to
        );

void SG_zingtx__add_link__packed(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_link
        );

void SG_zingtemplate__list_all_links(
	        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        SG_rbtree** pp_from,
        SG_rbtree** pp_to
        );

void SG_zingtemplate__list_singular_links(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        SG_rbtree** pp_from,
        SG_rbtree** pp_to
        );

void SG_zingtemplate__get_field_attributes(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        const char* psz_field_name,
        SG_zingfieldattributes** ppzfa
        );

void sg_zingtemplate__get_link_actions(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_link_name,
        SG_rbtree** pprb
        );

void SG_zingtemplate__get_link_attributes(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_link_name,
        SG_zinglinkattributes** ppzla,
        SG_zinglinksideattributes** ppzlsa_from,
        SG_zinglinksideattributes** ppzlsa_to
        );

void SG_zingtemplate__get_link_attributes__mine(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype_mine,
        const char* psz_link_name_mine,
        SG_zinglinkattributes** ppzla,
        SG_zinglinksideattributes** ppzlsa_mine,
        SG_zinglinksideattributes** ppzlsa_other
        );

void SG_zingtemplate__get_json(
        SG_context* pCtx,
        SG_zingtemplate* pThis,
        SG_string** ppstr
        );

void SG_zingrecord__get_tx(
        SG_context* pCtx,
        SG_zingrecord* pzrec,
        SG_zingtx** pptx
        );

void SG_zinglink__unpack(
        SG_context* pCtx,
        const char* psz_link,
        char* psz_recid_from,
        char* psz_recid_to,
        char* psz_name
        );

void sg_zingtx__delete_link__packed(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_link,
        const char* psz_hid_rec
        );

void sg_zingtx__add__from_dbrecord(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_dbrecord* pdbrec
        );

void sg_zingtx__delete__from_dbrecord(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_dbrecord* pdbrec
        );

void sg_zingtx__mod__from_dbrecord(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_original_hid,
        SG_dbrecord* pdbrec_add
        );

void sg_zinglink__pack(
        SG_context* pCtx,
        const char* psz_recid_from,
        const char* psz_recid_to,
        const char* psz_link_name,
        struct sg_zinglink* pzlink
        );

void sg_zing__load_template__csid(
        SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_csid,
        SG_zingtemplate** ppzt
        );

void sg_zing__load_template__hid_template(
        SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_hid_template,
        SG_zingtemplate** ppzt
        );

void sg_zing__query__parse_where(
        SG_context* pCtx,
        SG_zingtemplate* pzt,
        const char* psz_rectype,
        const char* psz_where,
        SG_bool b_include_rectype_in_query,
        SG_varray** pp
        );

void sg_zing__query__parse_sort(
        SG_context* pCtx,
        SG_zingtemplate* pzt,
        const char* psz_rectype,
        const char* psz_sort,
        SG_varray** pp
        );

void SG_zingtemplate__is_a_rectype(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        const char* psz_rectype,
        SG_bool* pb,
        SG_uint32* pi_count_rectypes
        );

void SG_zingtx__delete_links_from(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zinglinkattributes* pzla,
        const char* psz_recid_from
        );

void SG_zingtx__delete_links_to(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_zinglinkattributes* pzla,
        const char* psz_recid_to
        );

void SG_zingtemplate__list_rectypes(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        SG_rbtree** pprb
        );

void sg_zing_uniqify_int__do(
    SG_context* pCtx,
    SG_vhash* pvh_error,
    SG_vhash* pvh_uniqify,
    SG_zingtx* pztx,
    SG_zingfieldattributes* pzfa,
    const SG_audit* pq,
    const char** leaves,
    const char* psz_hid_cs_ancestor,
    SG_varray* pva_log
    );

void sg_zing_uniqify_string__do(
    SG_context* pCtx,
    SG_vhash* pvh_error,
    SG_vhash* pvh_uniqify,
    SG_zingtx* pztx,
    SG_zingfieldattributes* pzfa,
    const SG_audit* pq,
    const char** leaves,
    const char* psz_hid_cs_ancestor,
    SG_varray* pva_log
    );

void sg_zingmerge__add_log_entry(
        SG_context* pCtx,
        SG_varray* pva_log,
        ...
        );

void SG_zing__query_across_states(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_state_for_template,
        const char* psz_rectype,
        const char* psz_where,
        SG_int32 gMin,
        SG_int32 gMax,
        SG_vhash** ppResult
        );

void sg_zingtx__query(
        SG_context* pCtx,
        SG_zingtx* pztx,
        const char* psz_rectype,
        const char* psz_where,
        const char* psz_sort,
        SG_int32 limit,
        SG_int32 skip,
        SG_stringarray** ppResult
        );

void sg_zing__gen_random_unique_string(
    SG_context* pCtx,
    SG_zingtx* pztx,
    SG_zingfieldattributes* pzfa,
    SG_string** ppstr
    );

void sg_zing__gen_userprefix_unique_string(
    SG_context* pCtx,
    SG_zingtx* pztx,
    SG_zingfieldattributes* pzfa,
    SG_string** ppstr
    );

void sg_zingtx__get_all_values(
    SG_context* pCtx,
    SG_zingtx* pztx,
    const char* psz_rectype,
    const char* psz_field,
    SG_rbtree** pprb
    );

void SG_zingtx__get_repo(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_repo** ppRepo
        );

void SG_zingtx__get_dagnum(
        SG_context* pCtx,
        SG_zingtx* pztx,
        SG_uint32* pdagnum
        );

void SG_zingtemplate__has_any_links(
        SG_context* pCtx,
        SG_zingtemplate* pztemplate,
        SG_bool* pb
        );

END_EXTERN_C;

#endif//H_SG_ZING__PRIVATE_H

