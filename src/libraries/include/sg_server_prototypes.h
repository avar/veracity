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
 * @file sg_server_prototypes.h
 *
 */

#ifndef H_SG_SERVER_PROTOTYPES_H
#define H_SG_SERVER_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_server__alloc(
	SG_context*,
	SG_server** ppNew
	);

void SG_server__free(SG_context * pCtx, SG_server* pServer);

void SG_server__push_get_staging_path(
	SG_context* pCtx,
	SG_server* pServer,
	const char* pPushId,
	SG_pathname** pStagingPathname);

void SG_server__push_begin(
	SG_context*,
	SG_server * pServer,
	const char* psz_repo_descriptor_name,
	const char** ppszPushId
	);

void SG_server__push_add(
	SG_context*,
	SG_server * pServer,
	const char* pPush,
	const char* psz_fragball_name,
	SG_vhash** ppResult
	);

void SG_server__push_remove(
	SG_context*,
	SG_server * pServer,
	const char* pPush,
	SG_vhash* pvh_stuff_to_be_removed_from_staging_area,
	SG_vhash** ppResult
	);

void SG_server__push_commit(
	SG_context*,
	SG_server * pServer,
	const char* pPush,
	SG_vhash** ppResult
	);

void SG_server__push_end(
	SG_context*,
	SG_server * pServer,
	const char* ppPush
	);

/**
 *	The request vhash can take 3 forms:
 *
 *	(1) A null pvhRequest will fetch the leaves of every dag.
 *
 *	(2) A vhash requesting a full clone of the entire repository looks like this:
 *	{
 *		"clone" : NULL
 *	}
 *
 *	(3) A request for specific nodes and/or blobs looks like this:
 *	{
 *		"gens" : <number of generations to walk back from each requested dagnode>
 *		"dags" : <NULL for all leaves of all dags, else vhash where keys are dag numbers.
 *		{
 *			<dagnum> : <NULL for all leaves, else vhash where keys are hids, hid prefixes, or tags>
 *			{
 *				<hid> : NULL
 *				<hid prefix> : "prefix"
 *				<tag> : "tag"
 *				...
 *			}
 *		}
 *		"blobs" :
 *		{
 *			<hid> : NULL
 *			...
 *		}
 *	}
 *
 *	Any of the top level vhashes can be omitted: gens, dags, or blobs.
 *	- If "gens" is missing, we behave as though 0 were provided: no descendant nodes are included in the fragball.
 *	- If "dags" is missing, no dagnodes are included in the fragball and gens is ignored, if present.
 *	- If "blobs" is missing, no blobs are included in the fragball.
 *
 *	Ian TODO: describe pvhStatus format
 */
void SG_server__pull_request_fragball(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_vhash* pvhRequest,
	const SG_pathname* pFragballDirPathname,
	char** ppszFragballName,
	SG_vhash** pvhStatus
	);

void SG_server__get_repo_info(SG_context* pCtx,
							  SG_repo* pRepo,
							  char** ppszRepoId,
							  char** ppszAdminId,
							  char** ppszHashMethod);

void SG_server__get_dagnode_info(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_vhash* pvhRequest,
	SG_varray** ppvaInfo);

END_EXTERN_C;

#endif//H_SG_SERVER_PROTOTYPES_H

