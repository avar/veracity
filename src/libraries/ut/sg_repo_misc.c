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

// TODO This file is intended to have common functions that are *above* the
// TODO REPO API line.  For example, computing a hash on a buffer using the
// TODO current REPO will make multiple calls to the REPO API.
// TODO
// TODO We should probably formalize this (and make sure we don't have any
// TODO below-the-line stuff in here).

#include <sg.h>

void SG_repo__validate_repo_name(SG_context* pCtx, const char* psz_new_descriptor_name)
{
	if (psz_new_descriptor_name == NULL || *psz_new_descriptor_name == 0)
		SG_ERR_THROW(SG_ERR_INVALID_REPO_NAME);
	return;
fail:
	return;
}

//////////////////////////////////////////////////////////////////

void SG_repo__fetch_vhash(SG_context* pCtx,
						  SG_repo * pRepo,
						  const char* pszidHidBlob,
						  SG_vhash ** ppVhashReturned)
{
	// fetch contents of a blob and convert to an Vhash object.

	SG_byte * pbuf = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pszidHidBlob);
	SG_NULLARGCHECK_RETURN(ppVhashReturned);

	*ppVhashReturned = NULL;

	SG_ERR_CHECK_RETURN(  SG_repo__fetch_blob_into_memory(pCtx,pRepo,pszidHidBlob,&pbuf,NULL)  );

	SG_VHASH__ALLOC__FROM_JSON(pCtx, ppVhashReturned, (const char *)pbuf);

	SG_NULLFREE(pCtx, pbuf);
}

void SG_repo__create_user_root_directory(
	SG_context* pCtx,
	SG_repo* pRepo,
	const char* pszName,
	SG_changeset** ppChangeset,
	char** ppszidGidActualRoot
	)
{
	SG_committing* ptx = NULL;
	SG_changeset * pChangesetOriginal = NULL;
	SG_dagnode * pDagnodeOriginal = NULL;
	SG_treenode* pTreenodeActualRoot = NULL;
	SG_treenode* pTreenodeSuperRoot = NULL;
	char* pszidHidActualRoot = NULL;
	char* pszidHidSuperRoot = NULL;
	SG_treenode_entry* pEntry = NULL;
    const char* psz_hid_cs = NULL;
    SG_audit q;

	SG_ERR_CHECK(  SG_audit__init__maybe_nobody(pCtx, &q, pRepo, SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
	SG_ERR_CHECK(  SG_committing__alloc(pCtx, &ptx, pRepo, SG_DAGNUM__VERSION_CONTROL, &q, CSET_VERSION_1)  );
    /* we don't add a parent here */

	/* First add an empty tree node to be the actual root directory */
	SG_ERR_CHECK(  SG_treenode__alloc(pCtx, &pTreenodeActualRoot)  );
	SG_ERR_CHECK(  SG_treenode__set_version(pCtx, pTreenodeActualRoot, TN_VERSION__CURRENT)  );
	SG_ERR_CHECK(  SG_committing__add_treenode(pCtx, ptx, pTreenodeActualRoot, &pszidHidActualRoot)  );
	SG_TREENODE_NULLFREE(pCtx, pTreenodeActualRoot);
	pTreenodeActualRoot = NULL;

	/* Now create the super root with the actual root as its only entry */
	SG_ERR_CHECK(  SG_treenode__alloc(pCtx, &pTreenodeSuperRoot)  );
	SG_ERR_CHECK(  SG_treenode__set_version(pCtx, pTreenodeSuperRoot, TN_VERSION__CURRENT)  );

	SG_ERR_CHECK(  SG_TREENODE_ENTRY__ALLOC(pCtx, &pEntry)  );
	SG_ERR_CHECK(  SG_treenode_entry__set_entry_name(pCtx, pEntry, pszName)  );
	SG_ERR_CHECK(  SG_treenode_entry__set_entry_type(pCtx, pEntry, SG_TREENODEENTRY_TYPE_DIRECTORY)  );
	SG_ERR_CHECK(  SG_treenode_entry__set_hid_blob(pCtx, pEntry, pszidHidActualRoot)  );
	SG_ERR_CHECK(  SG_treenode_entry__set_hid_xattrs(pCtx, pEntry, NULL)  );
	SG_ERR_CHECK(  SG_treenode_entry__set_attribute_bits(pCtx, pEntry, 0)  );

	SG_ERR_CHECK(  SG_gid__alloc(pCtx, ppszidGidActualRoot)  );

	SG_ERR_CHECK(  SG_treenode__add_entry(pCtx, pTreenodeSuperRoot, *ppszidGidActualRoot, &pEntry)  );

	SG_ERR_CHECK(  SG_committing__add_treepath(pCtx, ptx, *ppszidGidActualRoot, pszName)  );
	SG_ERR_CHECK(  SG_committing__add_treenode(pCtx, ptx, pTreenodeSuperRoot, &pszidHidSuperRoot)  );
	SG_TREENODE_NULLFREE(pCtx, pTreenodeSuperRoot);
	pTreenodeSuperRoot = NULL;

	SG_ERR_CHECK(  SG_committing__set_root(pCtx, ptx, pszidHidSuperRoot)  );

	SG_ERR_CHECK(  SG_committing__end(pCtx, ptx, &pChangesetOriginal, &pDagnodeOriginal)  );
    ptx = NULL;

    SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pDagnodeOriginal, &psz_hid_cs)  );

	SG_DAGNODE_NULLFREE(pCtx, pDagnodeOriginal);

	SG_NULLFREE(pCtx, pszidHidActualRoot);
	SG_NULLFREE(pCtx, pszidHidSuperRoot);

	*ppChangeset = pChangesetOriginal;

	return;

fail:
	/* TODO free other stuff */

	SG_ERR_IGNORE(  SG_committing__abort(pCtx, ptx)  );
    SG_TREENODE_ENTRY_NULLFREE(pCtx, pEntry);
}

static void _create_empty_repo(SG_context* pCtx,
							   const char* psz_hash_method,
							   const char* psz_repo_id,
							   const char* psz_admin_id,
							   const char* psz_new_descriptor_name)
{
	SG_vhash* pvh_descriptor_partial = NULL;
	SG_vhash* pvh_descriptor_new = NULL;
	SG_repo* pNewRepo = NULL;
	SG_bool b_indexes = SG_TRUE; // TODO hack.  don't hard-code this to true.  add a param to this function

	SG_ERR_CHECK(  SG_repo__validate_repo_name(pCtx, psz_new_descriptor_name)  );

	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, psz_repo_id)  );
	SG_ERR_CHECK_RETURN(  SG_gid__argcheck(pCtx, psz_admin_id)  );

	/* Now construct a new repo with the same ID. */
	SG_ERR_CHECK(  SG_closet__get_partial_repo_instance_descriptor_for_new_local_repo(pCtx, &pvh_descriptor_partial)  );
	SG_ERR_CHECK(  SG_repo__create_repo_instance(pCtx,pvh_descriptor_partial,b_indexes,psz_hash_method,psz_repo_id,psz_admin_id,&pNewRepo)  );
	SG_VHASH_NULLFREE(pCtx, pvh_descriptor_partial);

	/* And save the descriptor for that new repo */
	SG_ERR_CHECK(  SG_repo__get_descriptor(pCtx, pNewRepo, (const SG_vhash**)&pvh_descriptor_new)  );
	SG_ERR_CHECK(  SG_closet__descriptors__add(pCtx, psz_new_descriptor_name, pvh_descriptor_new)  );

	/* fall through */
fail:
	SG_VHASH_NULLFREE(pCtx, pvh_descriptor_partial);
	SG_REPO_NULLFREE(pCtx, pNewRepo);
}

void SG_repo__create_empty_clone_from_remote(
	SG_context* pCtx,
	SG_client* pClient,
	const char* psz_new_descriptor_name
	)
{
	char* psz_repo_id = NULL;
	char* psz_admin_id = NULL;
	char* psz_hash_method = NULL;

	SG_NULLARGCHECK_RETURN(pClient);

	SG_ERR_CHECK(  SG_client__get_repo_info(pCtx, pClient, &psz_repo_id, &psz_admin_id, &psz_hash_method)  );

	SG_ERR_CHECK(  _create_empty_repo(pCtx, psz_hash_method, psz_repo_id, psz_admin_id, psz_new_descriptor_name)  );

	/* fall through */
fail:
	SG_NULLFREE(pCtx, psz_repo_id);
	SG_NULLFREE(pCtx, psz_admin_id);
	SG_NULLFREE(pCtx, psz_hash_method);
}



void SG_repo__create_empty_clone(
	SG_context* pCtx,
    const char* psz_existing_descriptor_name,
    const char* psz_new_descriptor_name
    )
{
    SG_repo* pSrcRepo = NULL;
    char* psz_hash_method = NULL;
    char* psz_repo_id = NULL;
    char* psz_admin_id = NULL;

    /* We need the ID of the existing repo. */
    SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, psz_existing_descriptor_name, &pSrcRepo)  );
    SG_ERR_CHECK(  SG_repo__get_hash_method(pCtx, pSrcRepo, &psz_hash_method)  );
    SG_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pSrcRepo, &psz_repo_id)  );
    SG_ERR_CHECK(  SG_repo__get_admin_id(pCtx, pSrcRepo, &psz_admin_id)  );

	SG_ERR_CHECK(  _create_empty_repo(pCtx, psz_hash_method, psz_repo_id, psz_admin_id, psz_new_descriptor_name)  );

	/* fall through */
fail:
	SG_NULLFREE(pCtx, psz_hash_method);
    SG_NULLFREE(pCtx, psz_repo_id);
    SG_NULLFREE(pCtx, psz_admin_id);
    SG_REPO_NULLFREE(pCtx, pSrcRepo);
}

void SG_repo__create__completely_new__closet(
	SG_context* pCtx,
    const char* psz_admin_id,
    const char* psz_hash_method,
    const char* pszRidescName,
    SG_changeset** ppcs,
    char** ppsz_gid_root
    )
{
    char * pszHashMethodSetting = NULL;
	SG_vhash* pvhPartialDescriptor = NULL;
	SG_repo* pRepo = NULL;
	const SG_vhash* pvhActualDescriptor = NULL;
	SG_changeset* pcsFirst = NULL;
	char* pszidGidActualRoot = NULL;
    char buf_repo_id[SG_GID_BUFFER_LENGTH];
    SG_bool b_indexes = SG_TRUE;

    SG_ERR_CHECK(  SG_repo__validate_repo_name(pCtx, pszRidescName)  );
	if(psz_hash_method==NULL)
    {
        SG_ERR_CHECK(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__NEWREPO_HASHMETHOD, NULL, &pszHashMethodSetting, NULL)  );
        psz_hash_method = pszHashMethodSetting;
    }

	SG_ERR_CHECK(  SG_closet__get_partial_repo_instance_descriptor_for_new_local_repo(pCtx, &pvhPartialDescriptor)  );

    SG_ERR_CHECK(  SG_gid__generate(pCtx, buf_repo_id, sizeof(buf_repo_id))  );

	SG_ERR_CHECK(  SG_repo__create_repo_instance(pCtx,pvhPartialDescriptor,b_indexes,psz_hash_method,buf_repo_id, psz_admin_id,&pRepo)  );

    // ask zing to install default templates for all our builtin databases
    SG_ERR_CHECK(  sg_zing__init_new_repo(pCtx, pRepo)  );

    // create an initial user
//#if 1 && defined(DEBUG)
    SG_ERR_CHECK(  SG_user__create(pCtx, pRepo, "debug@sourcegear.com")  );
//#endif

    // now create the tree
	SG_ERR_CHECK(  SG_repo__create_user_root_directory(pCtx, pRepo, "@", &pcsFirst, &pszidGidActualRoot)  );

	SG_ERR_CHECK(  SG_repo__get_descriptor(pCtx, pRepo, &pvhActualDescriptor)  );

	/* TODO should this give an error if the ridesc name already exists? */

	SG_ERR_CHECK(  SG_closet__descriptors__add(pCtx, pszRidescName, pvhActualDescriptor)  );

    if (ppcs)
    {
        *ppcs = pcsFirst;
        pcsFirst = NULL;
    }
    if (ppsz_gid_root)
    {
        *ppsz_gid_root = pszidGidActualRoot;
        pszidGidActualRoot = NULL;
    }

    /* fall */

fail:
    SG_NULLFREE(pCtx, pszHashMethodSetting);
    SG_CHANGESET_NULLFREE(pCtx, pcsFirst);
    SG_NULLFREE(pCtx, pszidGidActualRoot);
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);
	SG_REPO_NULLFREE(pCtx, pRepo);
}

void SG_repo__hidlookup__dagnode(
	SG_context* pCtx,
	SG_repo* pRepo,
	SG_uint32 iDagNum,
	const char* psz_hid_prefix,
	char** ppsz_hid
	)
{
    const char* psz_hid = NULL;
    SG_rbtree* prb = NULL;
    SG_uint32 count = 0;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(psz_hid_prefix);
	SG_NULLARGCHECK_RETURN(ppsz_hid);

    SG_ERR_CHECK(  SG_repo__find_dagnodes_by_prefix(pCtx, pRepo, iDagNum, psz_hid_prefix, &prb)  );

    SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );

    if (1 == count)
    {
        SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, prb, &psz_hid, NULL)  );
        SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_hid, ppsz_hid)  );
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_AMBIGUOUS_ID_PREFIX  );
    }

    SG_RBTREE_NULLFREE(pCtx, prb);

    return;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb);
}

void SG_repo__hidlookup__blob(
		SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_hid_prefix,
        char** ppsz_hid
        )
{
    const char* psz_hid = NULL;
    SG_rbtree* prb = NULL;
    SG_uint32 count = 0;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(psz_hid_prefix);
	SG_NULLARGCHECK_RETURN(ppsz_hid);

    SG_ERR_CHECK(  SG_repo__find_blobs_by_prefix(pCtx, pRepo, psz_hid_prefix, &prb)  );

    SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );

    if (1 == count)
    {
        SG_ERR_CHECK(  SG_rbtree__get_only_entry(pCtx, prb, &psz_hid, NULL)  );
        SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_hid, ppsz_hid)  );
    }
    else
    {
        SG_ERR_THROW(  SG_ERR_AMBIGUOUS_ID_PREFIX  );
    }

    SG_RBTREE_NULLFREE(pCtx, prb);

    return;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb);
}

// TODO this call is expensive.  is it used anywhere real?
void SG_repo__does_blob_exist(SG_context* pCtx,
							  SG_repo* pRepo,
							  const char* psz_hid,
							  SG_bool* pbExists)
{
	SG_repo_fetch_blob_handle* pHandle = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(psz_hid);
	SG_NULLARGCHECK_RETURN(pbExists);

	SG_repo__fetch_blob__begin(pCtx, pRepo, psz_hid, SG_FALSE, NULL, NULL, NULL, NULL, NULL, &pHandle);
	if (SG_context__err_equals(pCtx, SG_ERR_BLOB_NOT_FOUND))
	{
		*pbExists = SG_FALSE;
		SG_context__err_reset(pCtx);
		return;
	}
	SG_ERR_CHECK_CURRENT;

	*pbExists = SG_TRUE;

	// fall through
fail:
	if (pHandle)
		SG_ERR_IGNORE(  SG_repo__fetch_blob__abort(pCtx, pRepo, &pHandle)  );
}


void SG_repo__store_dagnode(SG_context* pCtx,
							SG_repo* pRepo,
							SG_repo_tx_handle* pTx,
							SG_uint32 iDagNum,
							SG_dagnode * pDagnode)
{
    SG_dagfrag* pFrag = NULL;
	SG_bool bIsFrozen;
    char* psz_repo_id = NULL;
    char* psz_admin_id = NULL;

	SG_NULLARGCHECK_RETURN(pTx);

	// If there's a dagnode provided, a dagnum must also be provided.
	// Null dagnode and SG_DAGNUM__NONE is allowed, but Ian doesn't remember why.
	//
	// TODO Review: revisit this argcheck.  if pDagnode is null, the following
	//              __is_frozen call is going to fail anyway.
	if ( (pDagnode == NULL) ^ (iDagNum == SG_DAGNUM__NONE) )
		SG_ERR_THROW_RETURN(SG_ERR_INVALIDARG);

    SG_ERR_CHECK_RETURN(  SG_dagnode__is_frozen(pCtx, pDagnode,&bIsFrozen) );
    if (!bIsFrozen)
        SG_ERR_THROW_RETURN(SG_ERR_INVALID_UNLESS_FROZEN);

    SG_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pRepo, &psz_repo_id)  );
    SG_ERR_CHECK(  SG_repo__get_admin_id(pCtx, pRepo, &psz_admin_id)  );

    SG_ERR_CHECK(  SG_dagfrag__alloc(pCtx, &pFrag, psz_repo_id, psz_admin_id, iDagNum)  );
    SG_NULLFREE(pCtx, psz_repo_id);
    SG_NULLFREE(pCtx, psz_admin_id);

    SG_ERR_CHECK(  SG_dagfrag__add_dagnode(pCtx, pFrag, &pDagnode)  );
    SG_ERR_CHECK(  SG_repo__store_dagfrag(pCtx, pRepo, pTx, pFrag)  );

    return;

fail:
    SG_DAGFRAG_NULLFREE(pCtx, pFrag);
}

void SG_repo__alloc_compute_hash__from_bytes(SG_context * pCtx,
											 SG_repo * pRepo,
											 SG_uint32 lenBuf,
											 const SG_byte * pBuf,
											 char ** ppsz_hid_returned)
{
	SG_repo_hash_handle * pRHH = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(ppsz_hid_returned);
	if (lenBuf > 0)
		SG_NULLARGCHECK_RETURN(pBuf);

	SG_ERR_CHECK(  SG_repo__hash__begin(pCtx,pRepo,&pRHH)  );
	SG_ERR_CHECK(  SG_repo__hash__chunk(pCtx,pRepo,pRHH,lenBuf,pBuf)  );
	SG_ERR_CHECK(  SG_repo__hash__end(pCtx,pRepo,&pRHH,ppsz_hid_returned)  );
	return;

fail:
	SG_ERR_CHECK(  SG_repo__hash__abort(pCtx,pRepo,&pRHH)  );
}

void SG_repo__alloc_compute_hash__from_string(SG_context * pCtx,
											  SG_repo * pRepo,
											  const SG_string * pString,
											  char ** ppsz_hid_returned)
{
	SG_NULLARGCHECK_RETURN(pString);

	SG_ERR_CHECK_RETURN(  SG_repo__alloc_compute_hash__from_bytes(pCtx,pRepo,
																  SG_string__length_in_bytes(pString),
																  (SG_byte *)SG_string__sz(pString),
																  ppsz_hid_returned)  );
}

// TODO What buffer size should we use when hashing the contents of a file?

#define COMPUTE_HASH_FILE_BUFFER_SIZE					(64 * 1024)

void SG_repo__alloc_compute_hash__from_file(SG_context * pCtx,
											SG_repo * pRepo,
											SG_file * pFile,
											char ** ppsz_hid_returned)
{
	SG_repo_hash_handle * pRHH = NULL;
	SG_byte buf[COMPUTE_HASH_FILE_BUFFER_SIZE];
	SG_uint32 nbr;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pFile);
	SG_NULLARGCHECK_RETURN(ppsz_hid_returned);

	SG_ERR_CHECK(  SG_repo__hash__begin(pCtx,pRepo,&pRHH)  );

	SG_ERR_CHECK(  SG_file__seek(pCtx,pFile,0)  );
	while (1)
	{
		SG_file__read(pCtx,pFile,sizeof(buf),buf,&nbr);
		if (SG_context__err_equals(pCtx, SG_ERR_EOF))
		{
			SG_context__err_reset(pCtx);
			break;
		}
		SG_ERR_CHECK_CURRENT;

		SG_ERR_CHECK(  SG_repo__hash__chunk(pCtx,pRepo,pRHH,nbr,buf)  );
	}
	SG_ERR_CHECK(  SG_repo__hash__end(pCtx,pRepo,&pRHH,ppsz_hid_returned)  );
	return;

fail:
	SG_ERR_CHECK(  SG_repo__hash__abort(pCtx,pRepo,&pRHH)  );
}

static void sg_repo__add_stats_for_one_changeset(
    SG_context * pCtx,
    SG_repo * pRepo,
    SG_changeset* pcs,
    SG_vhash* pvh_stats
    )
{
    const SG_uint16* p_cur_reftype = NULL;
    SG_vhash* pvh_blobs = NULL;
    SG_bool b = SG_FALSE;
    SG_vhash* pvh_lbl = NULL;

    SG_UNUSED(pRepo);

    SG_ERR_CHECK(  SG_vhash__addtoval__int64(pCtx, pvh_stats, "count_changesets", 1)  );

    b = SG_FALSE;
    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_stats, "blobs", &b)  );
    if (b)
    {
        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_stats, "blobs", &pvh_blobs)  );
    }
    else
    {
        SG_ERR_CHECK(  SG_vhash__addnew__vhash(pCtx, pvh_stats, "blobs", &pvh_blobs)  );
    }

    /* load the blob list */

    SG_ERR_CHECK(  SG_changeset__get_list_of_bloblists(pCtx, pcs, &pvh_lbl)  );

    p_cur_reftype = g_reftypes;
    while (*p_cur_reftype)
    {
        SG_uint32 count_blobs = 0;
        const char* psz_name = NULL;
        SG_vhash* pvh_bl = NULL;

        SG_ERR_CHECK(  SG_changeset__get_bloblist_name(pCtx, *p_cur_reftype, &psz_name)  );
        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_lbl, psz_name, &b)  );
        if (b)
        {
            SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_lbl, psz_name, &pvh_bl)  );
            SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_bl, &count_blobs)  );
        }

        if (count_blobs)
        {
            SG_uint32 i = 0;
            SG_int64 total_length = 0;
            SG_vhash* pvh_reftype = NULL;


            b = SG_FALSE;
            SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_blobs, psz_name, &b)  );
            if (b)
            {
                SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_blobs, psz_name, &pvh_reftype)  );
            }
            else
            {
                SG_ERR_CHECK(  SG_vhash__addnew__vhash(pCtx, pvh_blobs, psz_name, &pvh_reftype)  );
            }

            SG_ERR_CHECK(  SG_vhash__addtoval__int64(pCtx, pvh_reftype, "count", count_blobs)  );

            total_length = 0;
            for (i=0; i<count_blobs; i++)
            {
                SG_int64 this_length = 0;
                const SG_variant* pv = NULL;
                const char* psz_hid = NULL;

                SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_bl, i, &psz_hid, &pv)  );
                SG_ERR_CHECK(  SG_variant__get__int64(pCtx, pv, &this_length)  );

                total_length += this_length;
            }

            SG_ERR_CHECK(  SG_vhash__addtoval__int64(pCtx, pvh_reftype, "size", total_length)  );
        }

        p_cur_reftype++;
    }

fail:
    ;
}

static void sg_repo__walk_back_to_root(
    SG_context * pCtx,
    SG_repo * pRepo,
    const char* psz_hid_cs,
    SG_rbtree* prb_cs_done,
    SG_vhash* pvh_stats
    )
{
    SG_changeset* pcs = NULL;
    SG_bool b_already = SG_FALSE;

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_cs_done, psz_hid_cs, &b_already, NULL)  );
    if (!b_already)
    {
        SG_varray* pva_parents = NULL;
        SG_uint32 count_parents = 0;
        SG_uint32 i = 0;

        SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, psz_hid_cs, &pcs)  );
        SG_ERR_CHECK(  sg_repo__add_stats_for_one_changeset(pCtx, pRepo, pcs, pvh_stats)  );
        SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb_cs_done, psz_hid_cs)  );

        SG_ERR_CHECK(  SG_changeset__get_parents(pCtx, pcs, &pva_parents)  );
        if (pva_parents)
        {
            SG_ERR_CHECK(  SG_varray__count(pCtx, pva_parents, &count_parents)  );
            for (i=0; i<count_parents; i++)
            {
                const char* psz_hid = NULL;

                SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_parents, i, &psz_hid)  );

                // recursion considered acceptable here because it's a diagnostic
                // function
                SG_ERR_CHECK(  sg_repo__walk_back_to_root(pCtx, pRepo, psz_hid, prb_cs_done, pvh_stats)  );
            }
        }
    }

fail:
    SG_CHANGESET_NULLFREE(pCtx, pcs);
}

static void SG_repo__add_stats_for_one_dag(
    SG_context * pCtx,
    SG_repo * pRepo,
    SG_uint32 iDagNum,
    SG_vhash* pvh_top
    )
{
    SG_vhash* pvh = NULL;
    char buf_dagnum[SG_DAGNUM__BUF_MAX__NAME];
    SG_rbtree* prb_leaves = NULL;
    SG_uint32 count_leaves = 0;
    SG_rbtree* prb_cs_done = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_hid_cs = NULL;

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh)  );
    SG_ERR_CHECK(  SG_dagnum__to_name(pCtx, iDagNum, buf_dagnum, sizeof(buf_dagnum))  );

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo, iDagNum, &prb_leaves)  );
    SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_leaves, &count_leaves)  );
    SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, "count_leaves", (SG_int64) count_leaves)  );

    SG_ERR_CHECK(  SG_rbtree__alloc(pCtx, &prb_cs_done)  );

    b = SG_FALSE;
    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_leaves, &b, &psz_hid_cs, NULL)  );
    while (b)
    {
        SG_ERR_CHECK(  sg_repo__walk_back_to_root(pCtx, pRepo, psz_hid_cs, prb_cs_done, pvh)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_cs, NULL)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh_top, buf_dagnum, &pvh)  );

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_cs_done);
    SG_RBTREE_NULLFREE(pCtx, prb_leaves);
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_repo__diag_blobs(
    SG_context * pCtx,
    SG_repo * pRepo,
    SG_vhash** ppvh
    )
{
    SG_vhash* pvh = NULL;
	SG_uint32* paDagNums = NULL;
    SG_uint32 count_dagnums = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_vhash__alloc(pCtx, &pvh)  );

    SG_ERR_CHECK(  SG_repo__list_dags(pCtx, pRepo, &count_dagnums, &paDagNums)  );

    for (i=0; i<count_dagnums; i++)
    {
        SG_ERR_CHECK(  SG_repo__add_stats_for_one_dag(pCtx, pRepo, paDagNums[i], pvh)  );
    }

    *ppvh = pvh;
    pvh = NULL;

fail:
	SG_NULLFREE(pCtx, paDagNums);
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void sg_repo__update_ndx(
    SG_context * pCtx,
    SG_repo* pRepo,
    SG_rbtree* prb_new_dagnodes,
    SG_get_dbndx_callback* pfn_get_dbndx,
    void* p_arg_get_dbndx,
    SG_get_treendx_callback* pfn_get_treendx,
    void* p_arg_get_treendx
    )
{
    const char* psz_dagnum = NULL;
    SG_bool b = SG_FALSE;
    SG_rbtree_iterator* pit = NULL;
    SG_uint32 count = 0;
    SG_dbndx* pNdx = NULL;
    SG_treendx* pTreeNdx = NULL;
	SG_stringarray* psa_new = NULL;
    SG_uint32 i = 0;
    SG_varray* pva_audits = NULL;

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_new_dagnodes, &b, &psz_dagnum, (void**) &psa_new)  );
    while (b)
    {
        SG_uint32 iDagNum = 0;
        SG_uint32 iBaseDagNum = 0;

        SG_ERR_CHECK(  SG_dagnum__from_sz__decimal(pCtx, psz_dagnum, &iDagNum)  );

        if (SG_DAGNUM__IS_AUDIT(iDagNum))
        {
            iBaseDagNum = iDagNum & ~(SG_DAGNUM__BIT__AUDIT);

            if (SG_DAGNUM__IS_DB(iBaseDagNum))
            {
                SG_ERR_CHECK(  SG_varray__alloc(pCtx, &pva_audits)  );
                SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_new, &count )  );
                for (i=0; i<count; i++)
                {
                    const char* psz_hid = NULL;

                    SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa_new, i, &psz_hid)  );

                    SG_ERR_CHECK(  SG_dbndx__harvest_audits_for_injection(pCtx, pRepo, psz_hid, pva_audits)  );
                }

                SG_ERR_CHECK(  pfn_get_dbndx(pCtx, p_arg_get_dbndx, iBaseDagNum, &pNdx)  );
                SG_ERR_CHECK(  SG_dbndx__inject_audits(pCtx, pNdx, pva_audits)  );
                SG_DBNDX_NULLFREE(pCtx, pNdx);
                SG_VARRAY_NULLFREE(pCtx, pva_audits);
            }
            else
            {
                SG_ERR_CHECK(  pfn_get_dbndx(pCtx, p_arg_get_dbndx, iDagNum, &pNdx)  );
                SG_ERR_CHECK(  SG_dbndx__update__multiple(pCtx, pNdx, psa_new)  );
                SG_DBNDX_NULLFREE(pCtx, pNdx);
            }
        }

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_dagnum, (void**) &psa_new)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_new_dagnodes, &b, &psz_dagnum, (void**) &psa_new)  );
    while (b)
    {
        SG_uint32 iDagNum = 0;

        SG_ERR_CHECK(  SG_dagnum__from_sz__decimal(pCtx, psz_dagnum, &iDagNum)  );

        if (SG_DAGNUM__IS_DB(iDagNum))
        {
            // don't update the audit dags here.  we just did it, above.
            if (!SG_DAGNUM__IS_AUDIT(iDagNum))
            {
                SG_ERR_CHECK(  pfn_get_dbndx(pCtx, p_arg_get_dbndx, iDagNum, &pNdx)  );
                SG_ERR_CHECK(  SG_dbndx__update__multiple(pCtx, pNdx, psa_new)  );
                SG_DBNDX_NULLFREE(pCtx, pNdx);
            }
        }
        else if (iDagNum == SG_DAGNUM__VERSION_CONTROL)
        {
            SG_ERR_CHECK(  pfn_get_treendx(pCtx, p_arg_get_treendx, iDagNum, &pTreeNdx)  );
            SG_ERR_CHECK(  SG_treendx__update__multiple(pCtx, pTreeNdx, psa_new)  );
            SG_TREENDX_NULLFREE(pCtx, pTreeNdx);
        }
        SG_STRINGARRAY_NULLFREE(pCtx, psa_new);

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_dagnum, (void**) &psa_new)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

fail:
    SG_VARRAY_NULLFREE(pCtx, pva_audits);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
	SG_STRINGARRAY_NULLFREE(pCtx, psa_new);
}


