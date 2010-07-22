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

#ifndef H_SG_NULLFREE_H
#define H_SG_NULLFREE_H

BEGIN_EXTERN_C

#define SG_BITVEC256_NULLFREE(pCtx,p)             SG_STATEMENT(SG_context__push_level(pCtx);             SG_bitvec256__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_CHANGESET_NULLFREE(pCtx,p)             SG_STATEMENT(SG_context__push_level(pCtx);             SG_changeset__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_CLIENT_NULLFREE(pCtx,p)                SG_STATEMENT(SG_context__push_level(pCtx);          SG_client__close_free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_CONTEXT_NULLFREE(p)                    SG_STATEMENT(                                            SG_context__free(p);                                                                           p=NULL;)
#define SG_DAGFRAG_NULLFREE(pCtx,p)               SG_STATEMENT(SG_context__push_level(pCtx);               SG_dagfrag__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_DAGLCA_NULLFREE(pCtx,p)                SG_STATEMENT(SG_context__push_level(pCtx);                SG_daglca__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_DAGLCA_ITERATOR_NULLFREE(pCtx,p)       SG_STATEMENT(SG_context__push_level(pCtx);      SG_daglca__iterator__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_DAGNODE_NULLFREE(pCtx,p)               SG_STATEMENT(SG_context__push_level(pCtx);               SG_dagnode__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_DBNDX_NULLFREE(pCtx,p)                 SG_STATEMENT(SG_context__push_level(pCtx);                 SG_dbndx__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_DBRECORD_NULLFREE(pCtx,p)              SG_STATEMENT(SG_context__push_level(pCtx);              SG_dbrecord__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_DIFF_NULLFREE(pCtx,p)                  SG_STATEMENT(SG_context__push_level(pCtx);                  SG_diff__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_EXEC_ARGVEC_NULLFREE(pCtx,p)           SG_STATEMENT(SG_context__push_level(pCtx);           SG_exec_argvec__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_FRAGBALL_NULLFREE(pCtx, p)             SG_STATEMENT(SG_context__push_level(pCtx);              SG_fragball__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_INV_DIRS_NULLFREE(pCtx,p)              SG_STATEMENT(SG_context__push_level(pCtx);              SG_inv_dirs__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_INV_ENTRY_NULLFREE(pCtx,p)             SG_STATEMENT(SG_context__push_level(pCtx);             SG_inv_entry__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_JSONPARSER_NULLFREE(pCtx,p)            SG_STATEMENT(SG_context__push_level(pCtx);            SG_jsonparser__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_JSONWRITER_NULLFREE(pCtx,p)            SG_STATEMENT(SG_context__push_level(pCtx);            SG_jsonwriter__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_JSONDB_NULLFREE(pCtx,p)                SG_STATEMENT(SG_context__push_level(pCtx);          SG_jsondb__close_free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_MRG_NULLFREE(pCtx,p)                   SG_STATEMENT(SG_context__push_level(pCtx);                   SG_mrg__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_NULLFREE(pCtx,p)                       SG_STATEMENT(SG_context__push_level(pCtx);                        SG_free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_PATHNAME_NULLFREE(pCtx,p)              SG_STATEMENT(SG_context__push_level(pCtx);              SG_pathname__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_PENDINGDB_NULLFREE(pCtx,p)             SG_STATEMENT(SG_context__push_level(pCtx);             SG_pendingdb__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_PENDINGTREE_NULLFREE(pCtx,p)           SG_STATEMENT(SG_context__push_level(pCtx);           SG_pendingtree__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_PORTABILITY_DIR_NULLFREE(pCtx,p)       SG_STATEMENT(SG_context__push_level(pCtx);       SG_portability_dir__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_RBTREE_NULLFREE(pCtx,p)                SG_STATEMENT(SG_context__push_level(pCtx);                SG_rbtree__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx,p,cb)  SG_STATEMENT(SG_context__push_level(pCtx);    SG_rbtree__free__with_assoc(pCtx, p,cb);SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_RBTREE_ITERATOR_NULLFREE(pCtx,p)       SG_STATEMENT(SG_context__push_level(pCtx);      SG_rbtree__iterator__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_REPO_NULLFREE(pCtx,p)                  SG_STATEMENT(SG_context__push_level(pCtx);                  SG_repo__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_SERVER_NULLFREE(pCtx,p)                SG_STATEMENT(SG_context__push_level(pCtx);                SG_server__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_STAGING_NULLFREE(pCtx,p)               SG_STATEMENT(SG_context__push_level(pCtx);               SG_staging__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_STRING_NULLFREE(pCtx,p)                SG_STATEMENT(SG_context__push_level(pCtx);                SG_string__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_STRINGARRAY_NULLFREE(pCtx,p)           SG_STATEMENT(SG_context__push_level(pCtx);           SG_stringarray__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_STRPOOL_NULLFREE(pCtx,p)               SG_STATEMENT(SG_context__push_level(pCtx);               SG_strpool__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_TREEDIFF2_NULLFREE(pCtx,p)             SG_STATEMENT(SG_context__push_level(pCtx);             SG_treediff2__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_TREEDIFF2_ITERATOR_NULLFREE(pCtx,p)    SG_STATEMENT(SG_context__push_level(pCtx);   SG_treediff2__iterator__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_TREENDX_NULLFREE(pCtx,p)               SG_STATEMENT(SG_context__push_level(pCtx);               SG_treendx__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_TREENODE_NULLFREE(pCtx,p)              SG_STATEMENT(SG_context__push_level(pCtx);              SG_treenode__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_TREENODE_ENTRY_NULLFREE(pCtx,p)        SG_STATEMENT(SG_context__push_level(pCtx);        SG_treenode_entry__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_UTF8_CONVERTER_NULLFREE(pCtx,p)        SG_STATEMENT(SG_context__push_level(pCtx);        SG_utf8_converter__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_VARIANT_NULLFREE(pCtx,p)               SG_STATEMENT(SG_context__push_level(pCtx);               SG_variant__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_VARPOOL_NULLFREE(pCtx,p)               SG_STATEMENT(SG_context__push_level(pCtx);               SG_varpool__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_VARRAY_NULLFREE(pCtx,p)                SG_STATEMENT(SG_context__push_level(pCtx);                SG_varray__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_VECTOR_NULLFREE(pCtx,p)                SG_STATEMENT(SG_context__push_level(pCtx);                SG_vector__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_VECTOR_NULLFREE_WITH_ASSOC(pCtx,p,cb)  SG_STATEMENT(SG_context__push_level(pCtx);    SG_vector__free__with_assoc(pCtx, p,cb);SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_VECTOR_I64_NULLFREE(pCtx,p)            SG_STATEMENT(SG_context__push_level(pCtx);            SG_vector_i64__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_VHASH_NULLFREE(pCtx,p)                 SG_STATEMENT(SG_context__push_level(pCtx);                 SG_vhash__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_WD_PLAN_NULLFREE(pCtx,p)               SG_STATEMENT(SG_context__push_level(pCtx);               SG_wd_plan__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_ZINGFIELDATTRIBUTES_NULLFREE(pCtx,p)   SG_STATEMENT(SG_context__push_level(pCtx);   SG_zingfieldattributes__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_ZINGLINKATTRIBUTES_NULLFREE(pCtx,p)    SG_STATEMENT(SG_context__push_level(pCtx);    SG_zinglinkattributes__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_ZINGLINKSIDEATTRIBUTES_NULLFREE(pCtx,p)SG_STATEMENT(SG_context__push_level(pCtx);SG_zinglinksideattributes__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_ZINGRECTYPEINFO_NULLFREE(pCtx,p)         SG_STATEMENT(SG_context__push_level(pCtx);         SG_zingrectypeinfo__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_ZINGRECORD_NULLFREE(pCtx,p)            SG_STATEMENT(SG_context__push_level(pCtx);            SG_zingrecord__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)
#define SG_ZINGTEMPLATE_NULLFREE(pCtx,p)          SG_STATEMENT(SG_context__push_level(pCtx);          SG_zingtemplate__free(pCtx, p);   SG_ASSERT(!SG_context__has_err(pCtx));SG_context__pop_level(pCtx);p=NULL;)

END_EXTERN_C

#endif//H_SG_NULLFREE_H
