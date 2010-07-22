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

#include <sg.h>
#include "sg_repo__private.h"

/* This file is basically just functions that call
 * through the vtable.  Nothing here does any actual
 * work. */

//////////////////////////////////////////////////////////////////

void SG_repo__get_descriptor(SG_context* pCtx,
							 const SG_repo * pRepo,
							 const SG_vhash** ppvhDescriptor)
{
	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(ppvhDescriptor);

    *ppvhDescriptor = pRepo->pvh_descriptor;
}

void SG_repo__get_descriptor_name(
	SG_context* pCtx,
	const SG_repo* pRepo,
	const char** ppszDescriptorName)
{
	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(ppszDescriptorName);

	*ppszDescriptorName = pRepo->psz_descriptor_name;
}

//////////////////////////////////////////////////////////////////

void SG_repo__alloc(SG_context * pCtx, SG_repo ** ppRepo, const char * pszStorage)
{
	SG_repo * pRepo = NULL;

	SG_NULLARGCHECK_RETURN(ppRepo);

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pRepo)  );
	SG_ERR_CHECK(  sg_repo__bind_vtable(pCtx, pRepo, pszStorage)  );

	*ppRepo = pRepo;
	return;

fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
}

//////////////////////////////////////////////////////////////////

/**
 * Verify that the value of SG_RIDESC_KEY__STORAGE in the repo-descriptor
 * matches the value provided in SG_repo__alloc().
 */
static void _sg_repo__verify_storage(SG_context * pCtx, const SG_repo * pRepo, const SG_vhash * pvhDescriptor)
{
	const char * pszValueStorage;

	SG_ERR_CHECK_RETURN(  SG_vhash__get__sz(pCtx, pvhDescriptor, SG_RIDESC_KEY__STORAGE, &pszValueStorage)  );

	if (strcmp(pszValueStorage, pRepo->p_vtable->pszStorage) != 0)
		SG_ERR_THROW_RETURN(  SG_ERR_INVALIDARG  );
}

//////////////////////////////////////////////////////////////////

void SG_repo__open_repo_instance(
    SG_context* pCtx,
	const char* pszDescriptorName,
	SG_repo** ppRepo
	)
{
	SG_repo* pRepo = NULL;
	SG_vhash* pvhDescriptor = NULL;

	SG_ERR_CHECK(  SG_repo__validate_repo_name(pCtx, pszDescriptorName)  );

	SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, pszDescriptorName, &pvhDescriptor)  );
	SG_ERR_CHECK(  SG_repo__open_repo_instance__unnamed(pCtx, &pvhDescriptor, &pRepo)  );
	SG_ERR_CHECK(  SG_STRDUP(pCtx, pszDescriptorName, &(pRepo->psz_descriptor_name))  );

	SG_RETURN_AND_NULL(pRepo, ppRepo);

	/* fall through */
fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_VHASH_NULLFREE(pCtx, pvhDescriptor);
}

void SG_repo__open_repo_instance2(
    SG_context* pCtx,
	const char* pszDescriptorName,
	SG_repo* pRepo)
{
	SG_vhash* pvhDescriptor = NULL;

	SG_NULLARGCHECK_RETURN(pszDescriptorName);

	SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, pszDescriptorName, &pvhDescriptor)  );
	SG_ERR_CHECK(  SG_repo__open_repo_instance2__unnamed(pCtx, &pvhDescriptor, pRepo)  );
	SG_ERR_CHECK(  SG_STRDUP(pCtx, pszDescriptorName, &(pRepo->psz_descriptor_name))  );

	/* fall through */
fail:
	SG_VHASH_NULLFREE(pCtx, pvhDescriptor);
}

void SG_repo__open_repo_instance2__unnamed(
	SG_context* pCtx,
	SG_vhash** ppvhDescriptor,
	SG_repo * pRepo)
{
	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULL_PP_CHECK_RETURN(ppvhDescriptor);

	// with SG_repo__alloc() changes, we should always be bound to an implementation now.
	SG_ASSERT(  (pRepo->p_vtable)  );

	// but we should not be connected to an open repository on disk.
	SG_ARGCHECK_RETURN(  (pRepo->p_vtable_instance_data == NULL), pRepo->p_vtable_instance_data  );

	// make sure that the implementation specified in the repo-descriptor matches what was used to allocate the pRepo.
	SG_ERR_CHECK_RETURN(  _sg_repo__verify_storage(pCtx, pRepo, *ppvhDescriptor)  );

	// actually open the repository on disk.
	pRepo->pvh_descriptor = *ppvhDescriptor;
	*ppvhDescriptor = NULL;
	SG_ERR_CHECK_RETURN(  pRepo->p_vtable->open_repo_instance(pCtx, pRepo)  );
}

void SG_repo__open_repo_instance__unnamed(
	SG_context* pCtx,
	SG_vhash** ppvhDescriptor,
	SG_repo ** ppRepo)
{
	SG_repo * pRepo = NULL;
	const char * pszValueStorage;

	SG_NULLARGCHECK_RETURN(ppRepo);
	SG_NULL_PP_CHECK_RETURN(ppvhDescriptor);

	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, *ppvhDescriptor, SG_RIDESC_KEY__STORAGE, &pszValueStorage)  );

	SG_ERR_CHECK(  SG_repo__alloc(pCtx, &pRepo, pszValueStorage)  );
	SG_ERR_CHECK(  SG_repo__open_repo_instance2__unnamed(pCtx, ppvhDescriptor, pRepo)  );

	*ppRepo = pRepo;
	return;

fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
}

//////////////////////////////////////////////////////////////////

void SG_repo__create_repo_instance2(
	SG_context* pCtx,
	const SG_vhash* pvhPartialDescriptor,
	SG_repo * pRepo,
    SG_bool b_indexes,
    const char* psz_hash_method,
    const char* psz_repo_id,
    const char* psz_admin_id
	)
{
    char * pszHashMethodSetting = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pvhPartialDescriptor);
	// allow psz_hash_method to be null/empty to indicate that we pick the default hash-method.
	SG_NULLARGCHECK_RETURN(psz_repo_id);
	SG_NULLARGCHECK_RETURN(psz_admin_id);

	// with SG_repo__alloc() changes, we should always be bound to an implementation now.
	SG_ASSERT(  (pRepo->p_vtable)  );

	// but we should not be connected to an open repository on disk.
	SG_ARGCHECK_RETURN(  (pRepo->p_vtable_instance_data == NULL), pRepo->p_vtable_instance_data  );

	// make sure that the implementation specified in the repo-descriptor matches what was used to allocate the pRepo.
	SG_ERR_CHECK_RETURN(  _sg_repo__verify_storage(pCtx, pRepo, pvhPartialDescriptor)  );

    if(psz_hash_method==NULL)
    {
        SG_ERR_CHECK(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__NEWREPO_HASHMETHOD, NULL, &pszHashMethodSetting, NULL)  );
        psz_hash_method = pszHashMethodSetting;
    }

	// Actually create a new repo instance on disk.
    SG_ERR_CHECK(  SG_VHASH__ALLOC__COPY(pCtx, &pRepo->pvh_descriptor, pvhPartialDescriptor)  );
	SG_ERR_CHECK(  SG_gid__argcheck(pCtx, psz_repo_id)  );
	SG_ERR_CHECK(  SG_gid__argcheck(pCtx, psz_admin_id)  );
	SG_ERR_CHECK(  pRepo->p_vtable->create_repo_instance(
                pCtx,
                pRepo,
                b_indexes,
                psz_hash_method,
                psz_repo_id,
                psz_admin_id)  );

    SG_NULLFREE(pCtx, pszHashMethodSetting);

    return;
fail:
    SG_NULLFREE(pCtx, pszHashMethodSetting);
}

void SG_repo__open_repo_instance__copy(
	SG_context* pCtx,
	const SG_repo* pRepoSrc,
	SG_repo** ppRepo
	)
{
	const char* pszDescriptorName;
	SG_vhash* pvhDescriptor = NULL;

	SG_NULLARGCHECK_RETURN(pRepoSrc);
	SG_NULLARGCHECK_RETURN(ppRepo);

	SG_ERR_CHECK_RETURN(  SG_repo__get_descriptor_name(pCtx, pRepoSrc, &pszDescriptorName)  );
	if(pszDescriptorName)
		SG_ERR_CHECK_RETURN(  SG_repo__open_repo_instance(pCtx, pszDescriptorName, ppRepo)  );
	else
	{
		SG_VHASH__ALLOC__COPY(pCtx, &pvhDescriptor, pRepoSrc->pvh_descriptor);
		SG_ERR_CHECK(  SG_repo__open_repo_instance__unnamed(pCtx, &pvhDescriptor, ppRepo)  );
	}

	/* fall through */
fail:
	SG_VHASH_NULLFREE(pCtx, pvhDescriptor);
}

void SG_repo__create_repo_instance(
	SG_context* pCtx,
	const SG_vhash* pvhPartialDescriptor,
    SG_bool b_indexes,
    const char* psz_hash_method,
    const char* psz_repo_id,
    const char* psz_admin_id,
	SG_repo ** ppRepo)
{
	SG_repo * pRepo = NULL;
	const char * pszValueStorage;

	SG_NULLARGCHECK_RETURN(ppRepo);
	SG_NULLARGCHECK_RETURN(pvhPartialDescriptor);

	SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_KEY__STORAGE, &pszValueStorage)  );

	SG_ERR_CHECK(  SG_repo__alloc(pCtx, &pRepo, pszValueStorage)  );
	SG_ERR_CHECK(  SG_repo__create_repo_instance2(pCtx, pvhPartialDescriptor, pRepo,
												  b_indexes,psz_hash_method,psz_repo_id,psz_admin_id)  );

	*ppRepo = pRepo;
	return;

fail:
	SG_REPO_NULLFREE(pCtx, pRepo);
}

//////////////////////////////////////////////////////////////////

void SG_repo__free(SG_context* pCtx, SG_repo* pRepo)
{
	if (!pRepo)
		return;

	// if we actually OPENED or CREATED a REPO on disk (are
	// connected to it), we CLOSE it and let the implementation
	// free the instance data.
	if (pRepo->p_vtable  &&  pRepo->p_vtable_instance_data)
    {
		SG_ERR_IGNORE(  pRepo->p_vtable->close_repo_instance(pCtx, pRepo)  );
		SG_ASSERT(  (pRepo->p_vtable_instance_data == NULL)  );
    }

	// unbind the vtables incase it was dynamically loaded and
	// needs to be freed.
	SG_ERR_IGNORE(  sg_repo__unbind_vtable(pCtx, pRepo)  );

    SG_VHASH_NULLFREE(pCtx, pRepo->pvh_descriptor);

	SG_NULLFREE(pCtx, pRepo->psz_descriptor_name);

	SG_NULLFREE(pCtx, pRepo);
}

//////////////////////////////////////////////////////////////////
// Since we split SG_repo__{open,create}_repo_instance() into 2 steps:
// SG_repo__alloc() and SG_repo__{open,create}_repo_instance2(), we can
// now have pRepo pointers in 1 of 2 states:
// [1] bound (p_vtable is set, p_vtable_instance_data is null)
// [2] bound and open (both set)
//
// Most REPO API routines (that actually do something) require [2].
// Some (such as __{open,create}_repo_instance2()) can ONLY be used on [1].
// Some (such as __query_implementation()) can be used on [1] or [2].

/**
 * Verify that pRepo is BOUND to a specific REPO implementation.
 * It may or may not be OPEN.
 */
#define VERIFY_VTABLE_ONLY(pRepo)												\
	SG_STATEMENT( SG_NULLARGCHECK_RETURN(pRepo);								\
				  if (!(pRepo)->p_vtable)										\
					  SG_ERR_THROW_RETURN(  SG_ERR_VTABLE_NOT_BOUND  );	\
		)

/**
 * Verify that pRepo is BOUND to a specific REPO implementation and that it
 * is associated with something on disk (either OPENED or CREATED).
 */
#define VERIFY_VTABLE_AND_INSTANCE(pRepo)										\
	SG_STATEMENT( SG_NULLARGCHECK_RETURN(pRepo);								\
				  if (!(pRepo)->p_vtable)										\
					  SG_ERR_THROW_RETURN(  SG_ERR_VTABLE_NOT_BOUND  );	\
				  if (!(pRepo)->p_vtable_instance_data)							\
					  SG_ERR_THROW_RETURN(  SG_ERR_REPO_NOT_OPEN  );			\
		)

/**
 * Verify that pRepo is BOUND to a specific REPO implementation and that it
 * is NOT associated with something on disk.
 */
#define VERIFY_VTABLE_AND_NO_INSTANCE(pRepo)									\
	SG_STATEMENT( SG_NULLARGCHECK_RETURN(pRepo);								\
				  if (!(pRepo)->p_vtable)										\
					  SG_ERR_THROW_RETURN(  SG_ERR_VTABLE_NOT_BOUND  );	\
				  if ((pRepo)->p_vtable_instance_data)							\
					  SG_ERR_THROW_RETURN(  SG_ERR_REPO_ALREADY_OPEN  );		\
		)

//////////////////////////////////////////////////////////////////

void SG_repo__delete_repo_instance(
    SG_context * pCtx,
    SG_repo ** ppRepo
    )
{
	VERIFY_VTABLE_AND_INSTANCE(*ppRepo);

    SG_ASSERT((*ppRepo)->p_vtable->delete_repo_instance!=NULL);
    SG_ERR_CHECK(  (*ppRepo)->p_vtable->delete_repo_instance(pCtx, *ppRepo)  );

    SG_REPO_NULLFREE(pCtx, *ppRepo);

	return;
fail:
	;
}

//////////////////////////////////////////////////////////////////

void SG_repo__begin_tx(SG_context* pCtx,
					   SG_repo* pRepo,
						   SG_repo_tx_handle** ppTx)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->begin_tx(pCtx,pRepo,ppTx);
}

void SG_repo__commit_tx(SG_context* pCtx,
						SG_repo* pRepo,
							SG_repo_tx_handle** ppTx)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->commit_tx(pCtx,pRepo,ppTx);
}

void SG_repo__abort_tx(SG_context* pCtx,
					   SG_repo* pRepo,
						   SG_repo_tx_handle** ppTx)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->abort_tx(pCtx,pRepo,ppTx);
}

//////////////////////////////////////////////////////////////////

void SG_repo__fetch_dagnode(SG_context* pCtx, SG_repo * pRepo, const char* pszidHidChangeset, SG_dagnode ** ppNewDagnode)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(pszidHidChangeset);
	SG_NULLARGCHECK_RETURN(ppNewDagnode);

	pRepo->p_vtable->fetch_dagnode(pCtx,pRepo,pszidHidChangeset,ppNewDagnode);
}

//////////////////////////////////////////////////////////////////

void SG_repo__list_dags(SG_context* pCtx, SG_repo* pRepo, SG_uint32* piCount, SG_uint32** paDagNums)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(piCount);
	SG_NULLARGCHECK_RETURN(paDagNums);

	pRepo->p_vtable->list_dags(pCtx,pRepo,piCount,paDagNums);
}

void SG_repo__fetch_dag_leaves(SG_context* pCtx, SG_repo * pRepo, SG_uint32 iDagNum, SG_rbtree ** ppIdsetReturned)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(ppIdsetReturned);

	pRepo->p_vtable->fetch_dag_leaves(pCtx,pRepo,iDagNum,ppIdsetReturned);
}

void SG_repo__fetch_dagnode_children(SG_context* pCtx, SG_repo * pRepo, SG_uint32 iDagNum, const char * pszDagnodeID, SG_rbtree ** ppIdsetReturned)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(ppIdsetReturned);

	pRepo->p_vtable->fetch_dagnode_children(pCtx,pRepo,iDagNum,pszDagnodeID,ppIdsetReturned);
}

//////////////////////////////////////////////////////////////////

void SG_repo__check_dagfrag(
	SG_context* pCtx,
	SG_repo * pRepo,
	SG_dagfrag * pFrag,
	SG_bool * pbConnected,
	SG_rbtree ** ppIdsetMissing,
	SG_stringarray ** ppsa_new,
	SG_rbtree ** pprbLeaves)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(pFrag);

	pRepo->p_vtable->check_dagfrag(pCtx,pRepo,pFrag,pbConnected,ppIdsetMissing,ppsa_new,pprbLeaves);
}

void SG_repo__store_dagfrag(SG_context* pCtx,
							SG_repo * pRepo,
								 SG_repo_tx_handle* pTx,
								 SG_dagfrag * pFrag)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(pFrag);

	pRepo->p_vtable->store_dagfrag(pCtx,pRepo,pTx,pFrag);
}

//////////////////////////////////////////////////////////////////

void SG_repo__find_dagnodes_by_prefix(SG_context* pCtx, SG_repo * pRepo, SG_uint32 iDagNum, const char* psz_hid_prefix, SG_rbtree** pprb)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->find_dagnodes_by_prefix(pCtx, pRepo, iDagNum, psz_hid_prefix, pprb);
}

void SG_repo__find_blobs_by_prefix(SG_context* pCtx, SG_repo * pRepo, const char* psz_hid_prefix, SG_rbtree** pprb)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->find_blobs_by_prefix(pCtx, pRepo, psz_hid_prefix, pprb);
}

//////////////////////////////////////////////////////////////////

#if 0
void SG_repo__dag2__insert_dagnode(SG_context* pCtx,
								   SG_repo * pRepo,
									   SG_repo_tx_handle* pTx,
									   const SG_dagnode * pDagnode,
									   SG_int32 nrRetries)
{
	// we are a level-2 convenience routine.  we attempt the level-1
	// routine and retry n times if we get a BUSY.  if nrRetries < 0,
	// we loop forever.

	SG_int32 kRetry;

	kRetry = 0;
	while (1)
	{
		SG_repo__store_dagnode(pCtx,pRepo,pTx,pDagnode);
		if (!SG_context__err_equals(SG_ERR_DAG_DB_BUSY))
			break;			// stop on _OK or any other error.

		if (kRetry >= nrRetries)
			break;

		SG_sleep_ms(1000);
		kRetry++;
	}

	return;
}
#endif

void SG_repo__fetch_blob_into_memory(SG_context* pCtx,
							  SG_repo * pRepo,
							  const char* pszidHidBlob,
							  SG_byte ** ppBuf,
							  SG_uint64 * pLenRawData)
{
    SG_repo_fetch_blob_handle* pbh = NULL;
    SG_uint64 len = 0;
    SG_uint32 got = 0;
    SG_byte* p_buf = NULL;
    SG_uint64 so_far = 0;
    SG_bool b_done = SG_FALSE;

	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(pszidHidBlob);

    SG_ERR_CHECK(  SG_repo__fetch_blob__begin(pCtx, pRepo, pszidHidBlob, SG_TRUE, NULL, NULL, NULL, NULL, &len, &pbh)  );
    SG_ERR_CHECK(  SG_alloc(pCtx, (SG_uint32) (len+1), 1, &p_buf)  );
    p_buf[len] = 0;
    while (!b_done)
    {
        SG_ERR_CHECK(  SG_repo__fetch_blob__chunk(pCtx, pRepo, pbh, (SG_uint32) (len - so_far), p_buf + so_far, &got, &b_done)  );
        so_far += got;
    }
    SG_ERR_CHECK(  SG_repo__fetch_blob__end(pCtx, pRepo, &pbh)  );

    SG_ASSERT(so_far == len);

    /* TODO when ppBuf is NULL, can't we just never allocate? */
    if (ppBuf)
    {
        *ppBuf = p_buf;
    }
    else
    {
        SG_NULLFREE(pCtx, p_buf);
    }

    if (pLenRawData)
    {
        *pLenRawData = len;
    }

    return;

fail:
	SG_ERR_IGNORE(  SG_repo__fetch_blob__abort(pCtx, pRepo, &pbh)  );
    SG_NULLFREE(pCtx, p_buf);
}

/* TODO allow passing psz_hid_known into this */
void SG_repo__store_blob_from_memory(SG_context* pCtx,
									 SG_repo * pRepo,
										 SG_repo_tx_handle* pTx,
                                         const char* psz_objectid,
										 SG_bool b_dont_bother,
										 const SG_byte * pBufRawData,
										 SG_uint32 lenRawData,
										 char** ppszidHidReturned)
{
    SG_repo_store_blob_handle* pbh = NULL;

	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(pTx);
	SG_NULLARGCHECK_RETURN(pBufRawData);
	SG_NULLARGCHECK_RETURN(ppszidHidReturned);

#if 0
    b_dont_bother = SG_TRUE;
#endif

    SG_ERR_CHECK(  SG_repo__store_blob__begin(pCtx, pRepo, pTx, psz_objectid, b_dont_bother ? SG_BLOBENCODING__ALWAYSFULL : SG_BLOBENCODING__FULL, NULL, lenRawData, 0, NULL, &pbh)  );
    SG_ERR_CHECK(  SG_repo__store_blob__chunk(pCtx, pRepo, pbh, lenRawData, pBufRawData, NULL)  );
    SG_repo__store_blob__end(pCtx, pRepo, pTx, &pbh, ppszidHidReturned);
    if (!SG_context__has_err(pCtx) || SG_context__err_equals(pCtx, SG_ERR_BLOBFILEALREADYEXISTS))
    {
        pbh = NULL;
    }

    /* fall through */

fail:
    if (pbh)
    {
        SG_ERR_IGNORE(  SG_repo__store_blob__abort(pCtx, pRepo, pTx, &pbh)  );
	}
}

void SG_repo__store_blob_from_file(SG_context* pCtx,
								   SG_repo * pRepo,
									   SG_repo_tx_handle* pTx,
                                       const char* psz_objectid,
									   SG_bool b_dont_bother,
									   SG_file * pFileRawData,
									   char** ppszidHidReturned,
									   SG_uint64* iBlobFullLength)
{
    SG_repo_store_blob_handle* pbh = NULL;
    SG_byte* p_buf = NULL;
    SG_uint64 len = 0;

	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(pTx);
	SG_NULLARGCHECK_RETURN(pFileRawData);
	SG_NULLARGCHECK_RETURN(ppszidHidReturned);

    SG_ERR_CHECK(  SG_file__seek_end(pCtx, pFileRawData, &len)  );

	// remember the length of the blob
	*iBlobFullLength = len;

    SG_ERR_CHECK(  SG_file__seek(pCtx, pFileRawData, 0)  );
    SG_ERR_CHECK(  SG_repo__store_blob__begin(pCtx, pRepo, pTx, psz_objectid, b_dont_bother ? SG_BLOBENCODING__ALWAYSFULL : SG_BLOBENCODING__FULL, NULL, len, 0, NULL, &pbh)  );
    SG_ERR_CHECK(  SG_alloc(pCtx, SG_STREAMING_BUFFER_SIZE, 1, &p_buf)  );

    while (1)
    {
        SG_uint32 got = 0;
        SG_uint32 want = SG_STREAMING_BUFFER_SIZE;

        SG_file__read(pCtx, pFileRawData, want, p_buf, &got);
        if (SG_context__err_equals(pCtx, SG_ERR_EOF))
        {
			SG_context__err_reset(pCtx);
            break;
        }
        if (SG_context__has_err(pCtx))
        {
            goto fail;
        }
        SG_ERR_CHECK(  SG_repo__store_blob__chunk(pCtx, pRepo, pbh, got, p_buf, NULL)  );
    }

    SG_repo__store_blob__end(pCtx, pRepo, pTx, &pbh, ppszidHidReturned);
    if (!SG_context__has_err(pCtx) || SG_context__err_equals(pCtx, SG_ERR_BLOBFILEALREADYEXISTS))
    {
        pbh = NULL;
    }

    /* fall through */

fail:
    if (pbh)
    {
        SG_ERR_IGNORE(  SG_repo__store_blob__abort(pCtx, pRepo, pTx, &pbh)  );
    }
    SG_NULLFREE(pCtx, p_buf);
}

void SG_repo__fetch_blob_into_file(SG_context* pCtx,
								   SG_repo * pRepo,
									   const char* pszidHidBlob,
									   SG_file * pFileRawData,
									   SG_uint64* pLenRawData)
{
    SG_repo_fetch_blob_handle* pbh = NULL;
    SG_uint64 len = 0;
    SG_uint32 got = 0;
    SG_byte* p_buf = NULL;
    SG_uint64 left = 0;
    SG_bool b_done = SG_FALSE;

	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(pszidHidBlob);

    SG_ERR_CHECK(  SG_repo__fetch_blob__begin(pCtx, pRepo, pszidHidBlob, SG_TRUE, NULL, NULL, NULL, NULL, &len, &pbh)  );
    left = len;
    SG_ERR_CHECK(  SG_alloc(pCtx, SG_STREAMING_BUFFER_SIZE, 1, &p_buf)  );
    while (!b_done)
    {
        SG_uint32 want = SG_STREAMING_BUFFER_SIZE;
        if (want > left)
        {
            want = (SG_uint32) left;
        }
        SG_ERR_CHECK(  SG_repo__fetch_blob__chunk(pCtx, pRepo, pbh, want, p_buf, &got, &b_done)  );
        SG_ERR_CHECK(  SG_file__write(pCtx, pFileRawData, got, p_buf, NULL)  );

        left -= got;
    }
    SG_ERR_CHECK(  SG_repo__fetch_blob__end(pCtx, pRepo, &pbh)  );
    SG_NULLFREE(pCtx, p_buf);

    SG_ASSERT(0 == left);

    if (pLenRawData)
    {
        *pLenRawData = len;
    }

    return;

fail:
    SG_ERR_IGNORE(  SG_repo__fetch_blob__abort(pCtx, pRepo, &pbh)  );
    SG_NULLFREE(pCtx, p_buf);
}

void SG_repo__get_hash_method(SG_context* pCtx, SG_repo * pRepo, char** ppsz_hash_method)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(ppsz_hash_method);

	pRepo->p_vtable->get_hash_method(pCtx, pRepo,ppsz_hash_method);		// TODO jeff says: put SG_ERR_CHECK_RETURN() around all of the vtable calls so that our stack trace is complete
}

void SG_repo__get_repo_id(SG_context* pCtx, SG_repo * pRepo, char** ppsz_id)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(ppsz_id);

	pRepo->p_vtable->get_repo_id(pCtx, pRepo,ppsz_id);		// TODO jeff says: put SG_ERR_CHECK_RETURN() around all of the vtable calls so that our stack trace is complete
}

void SG_repo__get_admin_id(SG_context* pCtx, SG_repo * pRepo, char** ppsz_id)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(ppsz_id);

	pRepo->p_vtable->get_admin_id(pCtx,pRepo,ppsz_id);
}

void SG_repo__get_dag_lca(
    SG_context* pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
    const SG_rbtree* prbNodes,
    SG_daglca ** ppDagLca
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(prbNodes);
	SG_NULLARGCHECK_RETURN(ppDagLca);

	pRepo->p_vtable->get_dag_lca(pCtx,pRepo,iDagNum,prbNodes,ppDagLca);
}

void SG_repo__store_blob__begin(
	SG_context* pCtx,
    SG_repo * pRepo,
	SG_repo_tx_handle* pTx,
    const char* psz_objectid,
    SG_blob_encoding blob_encoding,
    const char* psz_hid_vcdiff_reference,
    SG_uint64 lenFull,
	SG_uint64 lenEncoded,
    const char* psz_hid_known,
    SG_repo_store_blob_handle** ppHandle
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	SG_NULLARGCHECK_RETURN(pTx);

    pRepo->p_vtable->store_blob__begin(pCtx, pRepo, pTx, psz_objectid, blob_encoding, psz_hid_vcdiff_reference, lenFull, lenEncoded, psz_hid_known, ppHandle);
}

void SG_repo__store_blob__chunk(
	SG_context* pCtx,
    SG_repo * pRepo,
    SG_repo_store_blob_handle* pHandle,
    SG_uint32 len_chunk,
    const SG_byte* p_chunk,
    SG_uint32* piWritten
    )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);

    pRepo->p_vtable->store_blob__chunk(pCtx, pRepo, pHandle, len_chunk, p_chunk, piWritten);
}

void SG_repo__store_blob__end(
    SG_context* pCtx,
    SG_repo * pRepo,
	SG_repo_tx_handle* pTx,
    SG_repo_store_blob_handle** ppHandle,
    char** ppsz_hid_returned
    )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);

    pRepo->p_vtable->store_blob__end(pCtx, pRepo, pTx, ppHandle, ppsz_hid_returned);
}

void SG_repo__store_blob__abort(
	SG_context* pCtx,
    SG_repo * pRepo,
	SG_repo_tx_handle* pTx,
    SG_repo_store_blob_handle** ppHandle
    )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);

    pRepo->p_vtable->store_blob__abort(pCtx, pRepo, pTx, ppHandle);
}

void SG_repo__fetch_blob__begin(
	SG_context* pCtx,
    SG_repo * pRepo,
    const char* psz_hid_blob,
    SG_bool b_convert_to_full,
    char** ppsz_objectid,
    SG_blob_encoding* pBlobFormat,
    char** ppsz_hid_vcdiff_reference,
    SG_uint64* pLenRawData,
    SG_uint64* pLenFull,
    SG_repo_fetch_blob_handle** ppHandle
    )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);

    pRepo->p_vtable->fetch_blob__begin(pCtx, pRepo, psz_hid_blob, b_convert_to_full, ppsz_objectid, pBlobFormat, ppsz_hid_vcdiff_reference, pLenRawData, pLenFull, ppHandle);

}

void SG_repo__fetch_blob__chunk(
	SG_context* pCtx,
    SG_repo * pRepo,
    SG_repo_fetch_blob_handle* pHandle,
    SG_uint32 len_buf,
    SG_byte* p_buf,
    SG_uint32* p_len_got,
    SG_bool* pb_done
    )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);
	SG_NULLARGCHECK_RETURN(pHandle);

    pRepo->p_vtable->fetch_blob__chunk(pCtx, pRepo, pHandle, len_buf, p_buf, p_len_got, pb_done);
}

void SG_repo__fetch_blob__end(
    SG_context* pCtx,
    SG_repo * pRepo,
    SG_repo_fetch_blob_handle** ppHandle
    )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);

    pRepo->p_vtable->fetch_blob__end(pCtx, pRepo, ppHandle);
}

void SG_repo__fetch_blob__abort(
	SG_context* pCtx,
    SG_repo * pRepo,
    SG_repo_fetch_blob_handle** ppHandle
    )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);

    pRepo->p_vtable->fetch_blob__abort(pCtx, pRepo, ppHandle);
}

void SG_repo__fetch_repo__fragball(
	SG_context* pCtx,
	SG_repo* pRepo,
	const SG_pathname* pFragballDirPathname,
	char** ppszFragballName
	)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->fetch_repo__fragball(pCtx, pRepo, pFragballDirPathname, ppszFragballName);
}


void SG_repo__get_blob_stats(
    SG_context* pCtx,
    SG_repo * pRepo,
    SG_uint32* p_count_blobs_full,
    SG_uint32* p_count_blobs_alwaysfull,
    SG_uint32* p_count_blobs_zlib,
    SG_uint32* p_count_blobs_vcdiff,
    SG_uint64* p_total_blob_size_full,
    SG_uint64* p_total_blob_size_alwaysfull,
    SG_uint64* p_total_blob_size_zlib_full,
    SG_uint64* p_total_blob_size_zlib_encoded,
    SG_uint64* p_total_blob_size_vcdiff_full,
    SG_uint64* p_total_blob_size_vcdiff_encoded
    )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);

    pRepo->p_vtable->get_blob_stats(pCtx,
		pRepo,
        p_count_blobs_full,
        p_count_blobs_alwaysfull,
        p_count_blobs_zlib,
        p_count_blobs_vcdiff,
        p_total_blob_size_full,
        p_total_blob_size_alwaysfull,
        p_total_blob_size_zlib_full,
        p_total_blob_size_zlib_encoded,
        p_total_blob_size_vcdiff_full,
        p_total_blob_size_vcdiff_encoded
    );
}

void SG_repo__treendx__get_path_in_dagnode(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_search_item_gid,
        const char* psz_changeset,
        SG_treenode_entry ** ppTreeNodeEntry
        )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);

    pRepo->p_vtable->treendx__get_path_in_dagnode(pCtx,
		pRepo,
        iDagNum,
        psz_search_item_gid,
        psz_changeset,
        ppTreeNodeEntry
    );
}

void SG_repo__treendx__get_all_paths(
        SG_context* pCtx,
        SG_repo* pRepo,
        SG_uint32 iDagNum,
        const char* psz_gid,
        SG_stringarray ** ppResults
        )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);

    pRepo->p_vtable->treendx__get_all_paths(pCtx,
		pRepo,
        iDagNum,
        psz_gid,
        ppResults
    );
}

void SG_repo__check_integrity(
    SG_context* pCtx,
    SG_repo * pRepo,
    SG_uint16 cmd,
    SG_uint32 iDagNum,
    SG_bool* p_bool,
    SG_vhash** pp_vhash
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->check_integrity(
            pCtx,
            pRepo,
            cmd,
            iDagNum,
            p_bool,
            pp_vhash
            );

    // TODO jeff says: put SG_ERR_CHECK_RETURN() around all of the vtable calls
    // so that our stack trace is complete
}

void SG_repo__dbndx__lookup_audits(
	SG_context* pCtx,
    SG_repo* pRepo, 
    SG_uint32 iDagNum,
    const char* psz_csid,
    SG_varray** ppva
	)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->dbndx__lookup_audits(
            pCtx, 
            pRepo,
            iDagNum,
            psz_csid,
            ppva
            );
    
    // TODO jeff says: put SG_ERR_CHECK_RETURN() around all of the vtable calls
    // so that our stack trace is complete
}

void SG_repo__dbndx__query_record_history(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
    const char* psz_recid,  // this is NOT the hid of a record.  it is the zing recid
    SG_varray** ppva
	)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->dbndx__query_record_history(
            pCtx,
            pRepo,
            iDagNum,
            psz_recid,
            ppva
            );

    // TODO jeff says: put SG_ERR_CHECK_RETURN() around all of the vtable calls
    // so that our stack trace is complete
}

void SG_repo__dbndx__query_list(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
	const char* pidState,
	const SG_varray* pcrit,
    SG_stringarray* psa_slice_fields,
    SG_varray** ppva_sliced
	)
{
    SG_repo_qresult* pqr = NULL;
    SG_varray* pva = NULL;

    SG_ASSERT(ppva_sliced);

    SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva)  );

    SG_ERR_CHECK(  SG_repo__dbndx__query(pCtx, pRepo, iDagNum, pidState, pcrit, NULL, psa_slice_fields, &pqr)  );
    if (pqr)
    {
        SG_uint32 count_received = 0;

        SG_ERR_CHECK(  SG_repo__qresult__get__multiple(pCtx, pRepo, pqr, -1, &count_received, pva)  );
    }

    *ppva_sliced = pva;
    pva = NULL;

fail:
    if (pqr)
    {
        SG_ERR_IGNORE(  SG_repo__qresult__done(pCtx, pRepo, &pqr)  );
    }
    SG_VARRAY_NULLFREE(pCtx, pva);
}

void SG_repo__dbndx__query__one(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
	const char* pidState,
	const SG_varray* pcrit,
    SG_stringarray* psa_slice_fields,
    SG_vhash** ppvh
	)
{
    SG_repo_qresult* pqr = NULL;
    SG_vhash* pvh = NULL;

    SG_ASSERT(ppvh);

    SG_ERR_CHECK(  SG_repo__dbndx__query(pCtx, pRepo, iDagNum, pidState, pcrit, NULL, psa_slice_fields, &pqr)  );
    if (pqr)
    {
        SG_ERR_CHECK(  SG_repo__qresult__get__one(pCtx, pRepo, pqr, &pvh)  );
    }

    *ppvh = pvh;
    pvh = NULL;

fail:
    if (pqr)
    {
        SG_ERR_IGNORE(  SG_repo__qresult__done(pCtx, pRepo, &pqr)  );
    }
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_repo__dbndx__query(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
	const char* pidState,
	const SG_varray* pcrit,
	const SG_varray* pSort,
    SG_stringarray* psa_slice_fields,
    SG_repo_qresult** ppqr
	)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->dbndx__query(
            pCtx,
            pRepo,
            iDagNum,
            pidState,
            pcrit,
            pSort,
            psa_slice_fields,
            ppqr
            );

    // TODO jeff says: put SG_ERR_CHECK_RETURN() around all of the vtable calls
    // so that our stack trace is complete
}

void SG_repo__qresult__count(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_repo_qresult* pqr,
    SG_uint32* picount
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->qresult__count(
            pCtx,
            pRepo,
            pqr,
            picount
            );

    // TODO jeff says: put SG_ERR_CHECK_RETURN() around all of the vtable calls
    // so that our stack trace is complete
}

void SG_repo__qresult__get__multiple(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_repo_qresult* pqr,
    SG_int32 count_wanted,
    SG_uint32* pi_count_retrieved,
    SG_varray* pva
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->qresult__get__multiple(
            pCtx,
            pRepo,
            pqr,
            count_wanted,
            pi_count_retrieved,
            pva
            );

    // TODO jeff says: put SG_ERR_CHECK_RETURN() around all of the vtable calls
    // so that our stack trace is complete
}

void SG_repo__qresult__get__one(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_repo_qresult* pqr,
    SG_vhash** ppvh
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->qresult__get__one(
            pCtx,
            pRepo,
            pqr,
            ppvh
            );

    // TODO jeff says: put SG_ERR_CHECK_RETURN() around all of the vtable calls
    // so that our stack trace is complete
}

void SG_repo__qresult__done(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_repo_qresult** ppqr
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->qresult__done(
            pCtx,
            pRepo,
            ppqr
            );

    // TODO jeff says: put SG_ERR_CHECK_RETURN() around all of the vtable calls
    // so that our stack trace is complete
}

void SG_repo__dbndx__query_across_states(
	SG_context* pCtx,
    SG_repo* pRepo,
    SG_uint32 iDagNum,
	const SG_varray* pcrit,
	SG_int32 gMin,
	SG_int32 gMax,
	SG_vhash** ppResults
	)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->dbndx__query_across_states(
            pCtx,
            pRepo,
            iDagNum,
            pcrit,
            gMin,
            gMax,
            ppResults
            );

    // TODO jeff says: put SG_ERR_CHECK_RETURN() around all of the vtable calls
    // so that our stack trace is complete
}

void SG_repo__change_blob_encoding(
    SG_context* pCtx,
    SG_repo * pRepo,
	SG_repo_tx_handle* pTx,
    const char* psz_hid_blob,
    SG_blob_encoding blob_encoding_desired,
    const char* psz_hid_vcdiff_reference_desired,
    SG_blob_encoding* p_blob_encoding_new,
    char** ppsz_hid_vcdiff_reference,
    SG_uint64* p_len_encoded,
    SG_uint64* p_len_full
    )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);

    pRepo->p_vtable->change_blob_encoding(pCtx,
			pRepo,
			pTx,
            psz_hid_blob,
            blob_encoding_desired,
            psz_hid_vcdiff_reference_desired,
            p_blob_encoding_new,
            ppsz_hid_vcdiff_reference,
            p_len_encoded,
            p_len_full
            );
}

void SG_repo__list_blobs(
	    SG_context* pCtx,
        SG_repo * pRepo,
        SG_blob_encoding blob_encoding, /**< only return blobs of this encoding */
        SG_bool b_sort_descending,      /**< otherwise sort ascending */
        SG_bool b_sort_by_len_encoded,  /**< otherwise sort by len_full */
        SG_uint32 limit,                /**< only return this many entries */
        SG_uint32 offset,               /**< skip the first group of the sorted entries */
        SG_vhash** ppvh                 /**< Caller must free */
        )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);

    pRepo->p_vtable->list_blobs(pCtx, pRepo, blob_encoding, b_sort_descending, b_sort_by_len_encoded, limit, offset, ppvh);
}

void SG_repo__vacuum(
    SG_context* pCtx,
    SG_repo * pRepo
    )
{
    VERIFY_VTABLE_AND_INSTANCE(pRepo);

    pRepo->p_vtable->vacuum(pCtx, pRepo);
}

void SG_repo__hash__begin(
	SG_context* pCtx,
    SG_repo * pRepo,
    SG_repo_hash_handle** ppHandle
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->hash__begin(pCtx,pRepo,ppHandle);
}

void SG_repo__hash__chunk(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_hash_handle* pHandle,
    SG_uint32 len_chunk,
    const SG_byte* p_chunk
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->hash__chunk(pCtx,pRepo,pHandle,len_chunk,p_chunk);
}

void SG_repo__hash__end(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_hash_handle** pHandle,
    char** ppsz_hid_returned
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->hash__end(pCtx,pRepo,pHandle,ppsz_hid_returned);
}

void SG_repo__hash__abort(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_repo_hash_handle** pHandle
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->hash__abort(pCtx,pRepo,pHandle);
}

void SG_repo__query_implementation(
    SG_context* pCtx,
	SG_repo * pRepo,
    SG_uint16 question,
    SG_bool* p_bool,
    SG_int64* p_int,
    char* p_buf_string,
    SG_uint32 len_buf_string,
    SG_vhash** pp_vhash
    )
{
	if (question & SG_REPO__QUESTION__TYPE_1)
	{
		// we don't need/use pRepo for Type [1] questions.

		switch (question)
		{
		case SG_REPO__QUESTION__VHASH__LIST_REPO_IMPLEMENTATIONS:
			SG_ERR_CHECK_RETURN(  sg_repo__query_implementation__list_vtables(pCtx, pp_vhash)  );
			return;

		default:
			SG_ERR_THROW_RETURN(  SG_ERR_NOTIMPLEMENTED  );
		}
	}

	if (question & SG_REPO__QUESTION__TYPE_2)
	{
		// pRepo must be bound to a valid vtable so that we can ask
		// repo-implementation-specific questions.

		VERIFY_VTABLE_ONLY(pRepo);

		// pRepo->p_vtable must be set.
		// pRepo->p_vtable_instance_data may or may not be set.
		//
		// note that p_vtable->query_implementation() DOES NOT get pRepo
		// so it CANNOT be asked questions about a specific disk instance.

		SG_ERR_CHECK_RETURN(  pRepo->p_vtable->query_implementation(pCtx,question,p_bool,p_int,p_buf_string,len_buf_string,pp_vhash)  );
		return;
	}

	SG_ERR_THROW_RETURN(  SG_ERR_INVALIDARG  );
}

void SG_repo__fetch_blob__info(
    SG_context* pCtx,
	SG_repo * pRepo,
    const char* psz_hid_blob,
    SG_bool* pb_blob_exists,
    char** ppsz_objectid,
    SG_blob_encoding* pBlobFormat,
    char** ppsz_hid_vcdiff_reference,
    SG_uint64* pLenRawData,
    SG_uint64* pLenFull
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->fetch_blob__info(pCtx,pRepo,psz_hid_blob, pb_blob_exists, ppsz_objectid, pBlobFormat, ppsz_hid_vcdiff_reference, pLenRawData, pLenFull);
}

void SG_repo__query_blob_existence(
	SG_context* pCtx,
	SG_repo * pRepo,
	SG_stringarray* psaBlobHids,
	SG_stringarray** ppsaNonexistentBlobs
	)
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->query_blob_existence(pCtx,pRepo,psaBlobHids,ppsaNonexistentBlobs);
}

void SG_repo__obliterate_blob(
    SG_context* pCtx,
	SG_repo * pRepo,
    const char* psz_hid_blob
    )
{
	VERIFY_VTABLE_AND_INSTANCE(pRepo);

	pRepo->p_vtable->obliterate_blob(pCtx,pRepo,psz_hid_blob);
}


