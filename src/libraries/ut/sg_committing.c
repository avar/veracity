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
 * @file sg_committing.c
 *
 * @details Routines to manage the transaction of creating a Changeset (a commit).
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////
// We call SG_changeset__load_from_repo() to get info about a parent Changeset.
//
// TODO decide how anal we want to be when loading blobs into data structures.
// TODO that is, do we want the blob layer to re-compute and verify the HID
// TODO as it it reads the blob.
// TODO for now, i've turned it on for debugging.
//
// Note that this has no effect on data structure VALIDATION.  The Changeset
// routines validate the imported object -- regardless of whether the blob
// layer verifies the HID.

#if 1 || defined(DEBUG)
#define MY_VERIFY_HID_ON_FETCH SG_TRUE
#else
#define MY_VERIFY_HID_ON_FETCH SG_FALSE
#endif

// TODO we call SG_repo__store_* for all types of objects.
// TODO we allow our caller to specify how to compress files and raw byte buffers,
// TODO because they may know something about the contents.
// TODO
// TODO we control the compression setting for all other objects.
// TODO decide how we want to handle compression by object type.

//////////////////////////////////////////////////////////////////

struct SG_committing
{
	SG_repo *			pRepo;			// we don't own this.
	SG_changeset *		pChangeset;
	SG_dagnode *		pDagnode;
    SG_uint32           iDagNum;
	SG_repo_tx_handle*	pRepoTx;
    SG_audit*           pq;
};

//////////////////////////////////////////////////////////////////

static void _sg_committing__free(SG_context * pCtx, SG_committing * pCommitting)
{
	// This is for internal use only.  The calling layer that called our __alloc
	// routine must call either __end or __abort to properly resolve the Transaction.
	// They will, in turn, call us.

	if (!pCommitting)
		return;

    SG_NULLFREE(pCtx, pCommitting->pq);
	SG_CHANGESET_NULLFREE(pCtx, pCommitting->pChangeset);
	SG_DAGNODE_NULLFREE(pCtx, pCommitting->pDagnode);

	SG_NULLFREE(pCtx, pCommitting);
}

void SG_committing__alloc(SG_context * pCtx,
						  SG_committing ** ppNew,
						  SG_repo * pRepo,
						  SG_uint32 iDagNum,
						  const SG_audit* pq,
						  SG_changeset_version verChangeset)
{
	SG_committing * pCommitting = NULL;

	SG_NULLARGCHECK_RETURN(ppNew);
	SG_NULLARGCHECK_RETURN(pRepo);

    if (!SG_DAGNUM__IS_AUDIT(iDagNum))
    {
        SG_NULLARGCHECK_RETURN(pq);
    }

	SG_ERR_CHECK(  SG_alloc1(pCtx, pCommitting)  );

	pCommitting->pRepo = pRepo;
    pCommitting->iDagNum = iDagNum;

    if (pq)
    {
        SG_ERR_CHECK(  SG_audit__copy(pCtx, pq, &pCommitting->pq)  );
    }

	SG_ERR_CHECK(  SG_changeset__alloc__committing(pCtx, iDagNum, &pCommitting->pChangeset)  );
	SG_ERR_CHECK(  SG_changeset__set_version(pCtx, pCommitting->pChangeset,verChangeset)  );
	SG_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pCommitting->pRepoTx)  );

	*ppNew = pCommitting;
	return;

fail:
	SG_ERR_IGNORE(  _sg_committing__free(pCtx, pCommitting)  );
}

void SG_committing__add_parent(SG_context * pCtx,
							   SG_committing * pCommitting,
							   const char* pszidHidParent)
{
	SG_NULLARGCHECK_RETURN(pCommitting);
	SG_NULLARGCHECK_RETURN(pCommitting->pChangeset);

	// we *DO NOT* add the given Parent's HID to the Blob-List.
	// because the Blob-List should reflect things *in* this Changeset that
	// are not *in* the parent(s).

	SG_ERR_CHECK_RETURN(  SG_changeset__add_parent(pCtx, pCommitting->pChangeset,pszidHidParent)  );
}

void SG_committing__abort(SG_context * pCtx,
						  SG_committing * pCommitting)
{
	SG_NULLARGCHECK_RETURN(pCommitting);

	// TODO......

	SG_ERR_CHECK(  SG_repo__abort_tx(pCtx, pCommitting->pRepo, &pCommitting->pRepoTx)  );
	_sg_committing__free(pCtx, pCommitting);

	return;

fail:
	// TODO what does it mean to fail an abort?
	// TODO who frees pCommitting?
	return;
}

static void _sg_committing__end_changeset_tx(SG_context * pCtx, SG_committing * pCommitting)
{
	SG_NULLARGCHECK_RETURN(pCommitting);

	//////////////////////////////////////////////////////////////////
	// Write the Changeset to the Repository.  This must be the LAST STEP.
	// After this step, the Changeset will be frozen and considered read-only.

	SG_ERR_CHECK_RETURN(  SG_changeset__save_to_repo(pCtx,
													 pCommitting->pChangeset,
													 pCommitting->pRepo,pCommitting->pRepoTx)  );
}

void SG_committing__end(SG_context * pCtx,
						SG_committing * pCommitting,
						SG_changeset ** ppChangesetResult,
						SG_dagnode ** ppDagnodeResult)
{
    const char* psz_hid_cs = NULL;

	SG_NULLARGCHECK_RETURN(pCommitting);

	// step 1 is to complete the CHANGESET TRANSACTION -- this has to do
	// with the creation of the various blobs (including the blob for the
	// actual CHANGESET) and computing the HID for the CHANGESET.

	SG_ERR_CHECK(  _sg_committing__end_changeset_tx(pCtx, pCommitting)  );

	// at this point pCommitting->pChangeset is complete and the blob
	// for it is on disk.  (we are creating a new commit, but it is
	// possible that the blob was already present (from an interrupted
	// session), but that detail is hidden from us.)
	//
	// create a DAGNODE memory object that captures the EDGE relationships
	// in the CHANGESET.

	SG_ERR_CHECK(  SG_changeset__create_dagnode(pCtx,
												pCommitting->pChangeset,
												&pCommitting->pDagnode)  );

	// step 2 is to use the DAGNODE to update the on-disk DAG using a
	// DAG TRANSACTION.

	SG_repo__store_dagnode(pCtx,
						   pCommitting->pRepo,
						   pCommitting->pRepoTx,
						   pCommitting->iDagNum,
						   pCommitting->pDagnode);
    pCommitting->pDagnode = NULL;

	if (SG_context__err_equals(pCtx,SG_ERR_DAGNODE_ALREADY_EXISTS))
		SG_context__err_reset(pCtx);
	else
		SG_ERR_CHECK_CURRENT;


	SG_ERR_CHECK(  SG_repo__commit_tx(pCtx, pCommitting->pRepo, &pCommitting->pRepoTx)  );

	// The repository transaction succeeded.

    // Add an audit record
    SG_ERR_CHECK(  SG_changeset__get_id_ref(pCtx, pCommitting->pChangeset, &psz_hid_cs)  );
    if (!SG_DAGNUM__IS_AUDIT(pCommitting->iDagNum))
    {
        SG_ERR_CHECK(  SG_audit__add(pCtx, pCommitting->pRepo, psz_hid_cs, pCommitting->iDagNum, pCommitting->pq)  );
    }

	// we optionally return the handles to the changeset and dagnode
	// that we created if the caller wants either of them.  (this keeps
	// them from having to immediately re-read them from disk.)
	// both of the returned objects are frozen and the caller must free
	// them.

    if (ppDagnodeResult)
    {
        SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pCommitting->pRepo, psz_hid_cs, ppDagnodeResult)  );
    }
    SG_RETURN_AND_NULL(pCommitting->pChangeset, ppChangesetResult);

	_sg_committing__free(pCtx, pCommitting);
	return;

fail:
	// The transaction may or may not have completed.
	// Propagate the error code and let the caller do
	// any cleanup -- such as calling __abort().
	//
	// Now that we are doing 2 serial transactions, we have to think about
	// the case where the changeset transaction succeeds but the dag
	// transaction fails.  we consider this an overall failure and
	// disregard the changeset blob -- we treat this as just another
	// unreferenced blob.

	return;
}

void SG_committing__add_bytes__buflen(SG_context * pCtx,
									  SG_committing * pCommitting,
                                      const char* psz_objectid,
									  const SG_byte* pBytes,
									  SG_uint32 len,
									  SG_uint16 iBlobRefType,
									  char** ppszidHidReturned)
{
	char* pszidHid = NULL;

	SG_NULLARGCHECK_RETURN(pCommitting);
	SG_NULLARGCHECK_RETURN(pCommitting->pRepo);
	SG_NULLARGCHECK_RETURN(pBytes);

	// Try to save the given string into a Blob and receive its HID.
	// We silently keep going if the Blob already exists in the Repository.

	SG_repo__store_blob_from_memory(pCtx,
									pCommitting->pRepo, pCommitting->pRepoTx, psz_objectid, SG_FALSE,
									pBytes, len, &pszidHid);

	if (SG_context__err_equals(pCtx, SG_ERR_BLOBFILEALREADYEXISTS))
		SG_context__err_reset(pCtx);
	else
		SG_ERR_CHECK_RETURN_CURRENT;

	// Try to add the HID of the Blob to the Changeset Blob-List.
	// We ***ASSUME*** that if the HID is already present in the Blob-List, it just returns OK.

	SG_ERR_CHECK(  SG_changeset__add_blob_to_list(pCtx,pCommitting->pChangeset,pszidHid,iBlobRefType,(SG_uint64)len)  );

	if (ppszidHidReturned)
		*ppszidHidReturned = pszidHid;
	else
		SG_NULLFREE(pCtx, pszidHid);

	return;

fail:
	SG_NULLFREE(pCtx, pszidHid);
}

void SG_committing__add_bytes__string(SG_context * pCtx,
									  SG_committing * pCommitting,
                                      const char* psz_objectid,
									  SG_string * pString,
									  SG_uint16 iBlobRefType,
									  char** ppszidHidReturned)
{
	SG_uint32 lenInBytes;

	SG_NULLARGCHECK_RETURN(pCommitting);
	SG_NULLARGCHECK_RETURN(pString);

	lenInBytes = SG_string__length_in_bytes(pString);

    SG_ERR_CHECK_RETURN(  SG_committing__add_bytes__buflen(pCtx,
														   pCommitting,
                                                           psz_objectid,
														   (SG_byte*) SG_string__sz(pString),
														   lenInBytes,
														   iBlobRefType,
														   ppszidHidReturned)  );
}

void SG_committing__add_dbrecord(SG_context * pCtx,
								 SG_committing * pCommitting,
								 SG_dbrecord * pRecord)
{
	// we don't take a "char** ppszidHidReturned" argument because
	// we put the computed HID inside the dbrecord when we freeze it.

	const char* pszidHid = NULL;
	SG_uint64 iBlobFullLength = 0;

	SG_NULLARGCHECK_RETURN(pCommitting);
	SG_NULLARGCHECK_RETURN(pCommitting->pRepo);
	SG_NULLARGCHECK_RETURN(pRecord);

	SG_ASSERT_RELEASE_RETURN(  SG_DAGNUM__IS_DB(pCommitting->iDagNum)  );

	// Try to save the given dbrecord into a Blob and receive its HID.
	// We silently keep going if the Blob already exists in the Repository.

	SG_dbrecord__save_to_repo(pCtx,
							  pRecord,pCommitting->pRepo,pCommitting->pRepoTx,&iBlobFullLength);

	if (SG_context__err_equals(pCtx, SG_ERR_BLOBFILEALREADYEXISTS))
		SG_context__err_reset(pCtx);
	else
		SG_ERR_CHECK_RETURN_CURRENT;

	SG_ERR_CHECK_RETURN(  SG_dbrecord__get_hid__ref(pCtx, pRecord, &pszidHid)  );

	// add the HID we computed for the dbrecord to the Blob-List in the Changeset.

	SG_ERR_CHECK_RETURN(  SG_changeset__add_blob_to_list(pCtx,pCommitting->pChangeset,pszidHid,SG_BLOB_REFTYPE__DBRECORD,iBlobFullLength)  );
}

void SG_committing__add_file(SG_context * pCtx,
							 SG_committing * pCommitting,
                             const char* psz_objectid,
							 SG_bool b_dont_bother,
							 SG_file * pFileRawData,
							 SG_uint16 iBlobRefType,
							 char** ppszidHidReturned)
{
	char* pszidHid = NULL;
	SG_uint64 iBlobFullLength = 0;

	SG_NULLARGCHECK_RETURN(pCommitting);
	SG_NULLARGCHECK_RETURN(pCommitting->pRepo);
	SG_NULLARGCHECK_RETURN(pFileRawData);

	SG_repo__store_blob_from_file(pCtx,
								  pCommitting->pRepo,
								  pCommitting->pRepoTx,
                                  psz_objectid,
								  b_dont_bother,
								  pFileRawData,
								  &pszidHid,
								  &iBlobFullLength);

	if (SG_context__err_equals(pCtx,SG_ERR_BLOBFILEALREADYEXISTS))
		SG_context__err_reset(pCtx);
	else
		SG_ERR_CHECK_CURRENT;


	// Try to add the HID of the Blob to the Changeset Blob-List.
	// We ***ASSUME*** that if the HID is already present in the Blob-List, it just returns OK.

	SG_ERR_CHECK(  SG_changeset__add_blob_to_list(pCtx,pCommitting->pChangeset,pszidHid,iBlobRefType,iBlobFullLength)  );

	if (ppszidHidReturned)
		*ppszidHidReturned = pszidHid;
	else
		SG_NULLFREE(pCtx, pszidHid);

	return;

fail:
	SG_NULLFREE(pCtx, pszidHid);
}

//////////////////////////////////////////////////////////////////

void SG_committing__add_treepath(SG_context * pCtx,
								 SG_committing * pCommitting,
                                 const char* psz_gid,
                                 const char* psz_path)
{
	SG_NULLARGCHECK_RETURN(pCommitting);
	SG_NULLARGCHECK_RETURN(psz_gid);
	SG_NULLARGCHECK_RETURN(psz_path);

    SG_ERR_CHECK_RETURN(  SG_changeset__add_treepath(pCtx, pCommitting->pChangeset, psz_gid, psz_path)  );
}

void SG_committing__add_treenode(SG_context * pCtx,
								 SG_committing * pCommitting,
								 SG_treenode * pTreenode,
								 char** ppszidHidReturned)
{
	char* pszidHid = NULL;
	SG_uint64 iBlobFullLength = 0;

	SG_NULLARGCHECK_RETURN(pCommitting);
	SG_NULLARGCHECK_RETURN(pCommitting->pRepo);
	SG_NULLARGCHECK_RETURN(pTreenode);

    SG_ASSERT_RELEASE_RETURN(  (!SG_DAGNUM__IS_DB(pCommitting->iDagNum))  );



	SG_treenode__save_to_repo(pCtx, pTreenode,pCommitting->pRepo,pCommitting->pRepoTx, &iBlobFullLength);
	if (SG_context__err_equals(pCtx,SG_ERR_BLOBFILEALREADYEXISTS))
		SG_context__err_reset(pCtx);
	else
		SG_ERR_CHECK_RETURN_CURRENT;

	// Try to add the HID of the Blob to the Changeset Blob-List.
	// We ***ASSUME*** that if the HID is already present in the Blob-List, it just returns OK.

	SG_ERR_CHECK(  SG_treenode__get_id(pCtx, pTreenode,&pszidHid)  );

	SG_ERR_CHECK(  SG_changeset__add_blob_to_list(pCtx, pCommitting->pChangeset,pszidHid,SG_BLOB_REFTYPE__TREENODE,iBlobFullLength)  );

	if (ppszidHidReturned)
		*ppszidHidReturned = pszidHid;
	else
		SG_NULLFREE(pCtx, pszidHid);

	return;

fail:
	SG_NULLFREE(pCtx, pszidHid);
}

//////////////////////////////////////////////////////////////////

void SG_committing__set_root(SG_context * pCtx,
							 SG_committing * pCommitting,
							 const char* psz_hid_root)
{
	SG_NULLARGCHECK_RETURN(pCommitting);

	SG_ERR_CHECK_RETURN(  SG_changeset__set_root(pCtx,pCommitting->pChangeset,psz_hid_root)  );
}

