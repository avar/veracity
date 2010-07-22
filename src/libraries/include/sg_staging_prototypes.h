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
 * @file sg_staging_prototypes.h
 */

#ifndef H_SG_STAGING_PROTOTYPES_H
#define H_SG_STAGING_PROTOTYPES_H

BEGIN_EXTERN_C

/*
 * A staging area is a directory which is used to accumulate the pieces of an
 * upcoming group of repo transactions.
 *
 * A staging area contains pending transactions for ONE repo.
 *
 * A staging area can have multiple transactions pending, one per dagnum.  For
 * each dagnum, there can be one dagfrag, which is optional.
 *
 * A staging area can be used during either a push or pull operation.
 *
 * A push is a network operation wherein the client sends fragballs to the
 * server which accumulates them into a staging area (on the server side) to
 * be committed to the repo once everything is received.
 *
 * A pull is a network operation wherein the client requests fragballs from
 * the server and accumulates them into a staging area (on the client side)
 * to be committed to the repo once everything is received.
 *
 * The layout and format of the contents of the staging area directory are
 * private to SG_staging.  Don't expect to read from this directory
 * directly.  And don't write anything to it.  Let SG_staging__* deal
 * with it.
 *
 * Except:  The way to add a fragball to the staging area is to write
 * the fragball file into the staging area directory using a unique name
 * like a GID.  Then call slurp_fragball to tell the staging area to
 * take that fragball and make it its own.
 *
 */

/**
 * Create a new, empty staging area.
 */
void SG_staging__create(
	SG_context* pCtx,
	const char* psz_repo_descriptor_name,
	char** psz_tid_staging, // An ID that uniquely identifies the staging area
	SG_staging** ppStaging);

/**
 * Get a handle for an existing staging area.
 */
void SG_staging__open(SG_context* pCtx,
					  const char* psz_tid_staging,
					  SG_staging** ppStaging);

/**
 * Delete the staging area.
 */
void SG_staging__cleanup(SG_context* pCtx,
						 SG_staging** ppStaging);
/**
 * Delete the staging area identified by the provided pushid.
 */
void SG_staging__cleanup__by_id(SG_context* pCtx,
								const char* pszPushId);

/**
 * Free staging instance data.
 */
void SG_staging__free(SG_context* pCtx, SG_staging* pStaging);

/**
 * Get a reference to the staging area's path on disk.
 */
void SG_staging__get_pathname(SG_context* pCtx, SG_staging* pStaging, const SG_pathname** ppPathname);

/**
 * Get the caller their own copy of the pathname (which they must free) from the push id.
 */
void SG_staging__get_pathname__from_id(SG_context* pCtx, const char* pszPushId, SG_pathname** ppPathname);

/**
 * Add the contents of a fragball into a staging area.
 */
void SG_staging__slurp_fragball(
	SG_context* pCtx,
	SG_staging* pStaging,
	const char* psz_filename
	);

/**
 * For each item in the staging area, provide information about
 * what would happen if the staging were committed right now.
 *
 * For each dagnode in the staging area, if the dagnode is already present in
 * the repo, say so.  If the dagnode cannot be committed to the
 * repo, say why not.  Reasons may include inability to connect the
 * frag, or the changeset blob is not available, or a violation of some policy.
 *
 * If b_every_blob is TRUE, for each blob reference in each changeset
 * in the staging area, report the status of that blob.  Choices for
 * this status are:
 *
 *   the blob is in the staging area (and may or may not already be in the repo)
 *
 *   the blob is already in the repo (and is not in the staging area)
 *
 *   the blob is in neither the repo nor the staging area
 *
 * If b_every_blob is FALSE, for each changeset in the staging
 * area, organized by blob reftype, report how many blobs referenced by that
 * changeset are in each of the three statuses above.
 *
 */
void SG_staging__check_status(
	SG_context* pCtx,
	SG_staging* pStaging,
	SG_bool bCheckConnectedness,
	SG_bool bCheckLeafDelta,
	SG_bool bListNewNodes,
	SG_bool bCheckChangesetBlobs,
	SG_bool bCheckDataBlobs,
	SG_vhash** ppResult
	);

/**
 * This method takes everything in the staging
 * area and inserts it into the given repo, one
 * tx per dagnum.  The staging area itself is
 * untouched; nothing there is removed.
 *
 * No policy checks are done.  Call check_status to do that.
 */
void SG_staging__commit(
	SG_context* pCtx,
	SG_staging* pStaging
	);

void SG_staging__commit_fragball(
	SG_context* pCtx,
	SG_staging* pStaging,
	const char* pszFragballName);

void SG_staging__fetch_blob_into_memory(SG_context* pCtx,
										SG_staging_blob_handle* pStagingBlobHandle,
										SG_byte ** ppBuf,
										SG_uint32 * pLenFull);

END_EXTERN_C

#endif
