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
 * @file sg_client_prototypes.h
 *
 */

#ifndef H_SG_CLIENT_PROTOTYPES_H
#define H_SG_CLIENT_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_client__open(
	SG_context* pCtx,
	const char* psz_remote_repo_spec,
    const SG_vhash* pvh_credentials,
	SG_client** ppNew
	);

void SG_client__close_free(SG_context * pCtx, SG_client * pClient);

void SG_client__push_begin(
		SG_context* pCtx,
		SG_client * pClient,
		SG_pathname** pFragballDirPathname,
        SG_client_push_handle** ppPush
        );

// SG_client takes ownership of the fragball with this call.  It will clean up the file and the SG_pathname.
void SG_client__push_add(
		SG_context* pCtx,
		SG_client * pClient,
        SG_client_push_handle* pPush,
        SG_pathname** ppPath_fragball,
        SG_vhash** ppResult
        );

void SG_client__push_remove(
		SG_context* pCtx,
		SG_client * pClient,
        SG_client_push_handle* pPush,
        SG_vhash* pvh_stuff_to_be_removed_from_staging_area,
        SG_vhash** ppResult
        );

void SG_client__push_commit(
		SG_context* pCtx,
		SG_client * pClient,
        SG_client_push_handle* pPush,
        SG_vhash** ppResult
        );

void SG_client__push_end(
		SG_context* pCtx,
		SG_client * pClient,
        SG_client_push_handle** ppPush
        );

/**
 * Request a fragball from the repo specified in pClient.  It could be local or remote.
 * If prbDagnodes is null, the fragball will contain all the leaves.
 * If psaBlobs is null, the fragball will contain no blobs.
 * The fragball is saved in pStagingPathname with file name ppszFragballName.
 *
 * See SG_server__pull_request_fragball for the vhash formats of pvhRequest and pvhStatus.
 */
void SG_client__pull_request_fragball(
		SG_context* pCtx,
		SG_client* pClient,
		SG_vhash* pvhRequest,
		const SG_pathname* pStagingPathname,
		char** ppszFragballName,
		SG_vhash** ppvhStatus
        );

void SG_client__pull_clone(
	SG_context* pCtx,
	SG_client* pClient,
	const SG_pathname* pStagingPathname,
	char** ppszFragballName);

void SG_client__get_repo_info(
		SG_context* pCtx,
		SG_client* pClient,
		char** ppszRepoId,
		char** ppszAdminId,
		char** ppszHashMethod
		);

void SG_client__get_dagnode_info(
	SG_context* pCtx,
	SG_client* pClient,
	SG_vhash* pvhRequest,
	SG_varray** ppvaInfo);

END_EXTERN_C;

#endif//H_SG_CLIENT_PROTOTYPES_H

