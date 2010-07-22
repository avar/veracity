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

#ifndef H_SG_SAFEPTR_H
#define H_SG_SAFEPTR_H

BEGIN_EXTERN_C;

typedef struct _SG_safeptr         SG_safeptr;
typedef struct sg_zingdb sg_zingdb;
typedef struct sg_zinglinkcollection sg_zinglinkcollection;
typedef struct sg_statetxcombo sg_statetxcombo;

#define SG_SAFEPTR_TYPE__REPO "repo"
#define SG_SAFEPTR_TYPE__DBNDX "dbndx"
#define SG_SAFEPTR_TYPE__ZINGRECORD "zingrecord"
#define SG_SAFEPTR_TYPE__ZINGSTATE "zingdb"
#define SG_SAFEPTR_TYPE__STATETXCOMBO "zingdbtxcombo"
#define SG_SAFEPTR_TYPE__ZINGLINKCOLLECTION "zinglinkcollection"

void SG_safeptr__wrap__repo(SG_context* pCtx, SG_repo* pRepo, SG_safeptr** pp);
void SG_safeptr__unwrap__repo(SG_context* pCtx, SG_safeptr* psafe, SG_repo** pp);

void SG_safeptr__wrap__statetxcombo(SG_context* pCtx, sg_statetxcombo* pZingTx, SG_safeptr** pp);
void SG_safeptr__unwrap__statetxcombo(SG_context* pCtx, SG_safeptr* psafe, sg_statetxcombo** pp);

void SG_safeptr__wrap__zingdb(SG_context* pCtx, sg_zingdb* pZingTx, SG_safeptr** pp);
void SG_safeptr__unwrap__zingdb(SG_context* pCtx, SG_safeptr* psafe, sg_zingdb** pp);

void SG_safeptr__wrap__zingrecord(SG_context* pCtx, SG_zingrecord* pZingRecord, SG_safeptr** pp);
void SG_safeptr__unwrap__zingrecord(SG_context* pCtx, SG_safeptr* psafe, SG_zingrecord** pp);

void SG_safeptr__wrap__zinglinkcollection(SG_context* pCtx, sg_zinglinkcollection* pZingRecord, SG_safeptr** pp);
void SG_safeptr__unwrap__zinglinkcollection(SG_context* pCtx, SG_safeptr* psafe, sg_zinglinkcollection** pp);

void SG_safeptr__free(SG_context * pCtx, SG_safeptr* p);

#define SG_SAFEPTR_NULLFREE(pCtx,p)              SG_STATEMENT( SG_context__push_level(pCtx);             SG_safeptr__free(pCtx, p);    SG_ASSERT(!SG_context__has_err(pCtx)); SG_context__pop_level(pCtx); p=NULL; )

END_EXTERN_C;

#endif//H_SG_SAFEPTR_H

