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
 * @file sg_attributes_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_ATTRIBUTES_PROTOTYPES_H
#define H_SG_ATTRIBUTES_PROTOTYPES_H

BEGIN_EXTERN_C;

#if defined(MAC) || defined(LINUX)

void SG_attributes__xattrs__read(
	SG_context* pCtx,
	SG_repo * pRepo,			/**< We only need the REPO so that we can compute Hashes. */
	const SG_pathname* pPath,
    SG_vhash** ppvhXattrs,
    SG_rbtree** pprbBlobs       /**< This rbtree contains 1 memoryblob for each
                                  xattr in the vhash. */
	);

void SG_attributes__xattrs__update_with_baseline(
		SG_context* pCtx,
        const SG_vhash* pvhBaseline,
        SG_vhash* pvh
        );

void SG_attributes__xattrs__apply(
	SG_context* pCtx,
    const SG_pathname* pPath,
    const SG_vhash* pvhAttrs,
    SG_repo* pRepo              /**< This is so that xattr value blobs
                                  can be retrieved as needed. */
    );
#endif

void SG_attributes__bits__apply(
	SG_context* pCtx,
    const SG_pathname* pPath,
    SG_int64 iBits
    );

void SG_attributes__bits__read(
	SG_context* pCtx,
    const SG_pathname* pPath,
    SG_int64 iBaselineAttributeBits,
    SG_int64* piBits
    );


END_EXTERN_C;

#endif //H_SG_ATTRIBUTES_PROTOTYPES_H
