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
 * @file sg_fragball_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_FRAGBALL_PROTOTYPES_H
#define H_SG_FRAGBALL_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_fragball__write(SG_context * pCtx,
						SG_repo* pRepo,
						const SG_pathname* pPath,
						SG_rbtree* prb_frags);

void SG_fragball__create(SG_context * pCtx, const SG_pathname* pPath_dir, SG_pathname** ppPath_fragball);
void SG_fragball__append__frag(SG_context * pCtx, SG_pathname* pPath, SG_dagfrag* pFrag);
void SG_fragball__append__blob(SG_context * pCtx, SG_pathname* pPath, SG_repo* pRepo, const char* psz_hid);

void SG_fragball__append_blob__from_handle(
	SG_context* pCtx,
	SG_fragball_writer* pWriter,
	SG_repo_fetch_blob_handle** ppBlob,
	const char* psz_objectid,
	const char* pszHid,
	SG_blob_encoding encoding,
	const char* pszHidVcdiffRef,
	SG_uint64 lenEncoded,
	SG_uint64 lenFull);

void SG_fragball__append__blobs(SG_context * pCtx, SG_pathname* pPath, SG_repo* pRepo, const char* const* pasz_blob_hids, SG_uint32 countHids);
void SG_fragball__append__dagnode(SG_context* pCtx, SG_pathname* pPath, SG_repo* pRepo, SG_uint32 iDagNum, const char* pszHid);
void SG_fragball__append__dagnodes(SG_context * pCtx, SG_pathname* pPath, SG_repo* pRepo, SG_uint32 iDagNum, SG_rbtree* prb_ids);

void SG_fragball__read_object_header(SG_context * pCtx, SG_file* pFile, SG_vhash** ppvh);

void SG_fragball__free(SG_context* pCtx, SG_pathname* pPath_fragball);

void SG_fragball_writer__alloc(SG_context * pCtx, SG_repo* pRepo, const SG_pathname* pPathFragball, SG_bool bCreateNewFile, SG_fragball_writer** ppResult);
void SG_fragball_writer__free(SG_context * pCtx, SG_fragball_writer* pfb);

END_EXTERN_C;

#endif //H_SG_FRAGBALL_PROTOTYPES_H

