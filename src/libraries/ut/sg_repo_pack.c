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

// TODO NOTE
// The functions in this file are not ready to be features.
// These functions aren't much more than sample code to help
// test change_blob_encoding() in repo implementations.

void sg_pack__do_blob(SG_context* pCtx, const char* psz_gid, const char* psz_hid, SG_int32 gen, SG_rbtree* prb_blobs, SG_rbtree* prb_new)
{
    SG_rbtree* prb = NULL;
    SG_bool b = SG_FALSE;
    char buf[64];

    SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_new, psz_hid, &b, NULL)  );
    if (b)
    {
        SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_blobs, psz_gid, &b, (void**) &prb)  );
        if (!b)
        {
            SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );
            SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_blobs, psz_gid, prb)  );
        }
        SG_ERR_CHECK(  SG_sprintf(pCtx, buf, sizeof(buf), "%05d", (int) gen)  );
        SG_ERR_CHECK(  SG_rbtree__add__with_pooled_sz(pCtx, prb, buf, psz_hid)  );
    }

    return;

fail:
    return;
}

void sg_pack__get_dir(SG_context* pCtx, SG_repo* pRepo, SG_int32 gen, const char* pszidHidTreeNode, SG_rbtree* prb_blobs, SG_rbtree* prb_new)
{
	SG_uint32 count;
	SG_uint32 i;
	SG_treenode* pTreenode = NULL;
    const char* psz_gid = NULL;
    SG_treenode_entry_type type;
    const char* psz_hid = NULL;

	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszidHidTreeNode, &pTreenode)  );
	SG_ERR_CHECK(  SG_treenode__count(pCtx, pTreenode, &count)  );

	for (i=0; i<count; i++)
	{
		const SG_treenode_entry* pEntry = NULL;
		const char* pszName = NULL;

		SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx, pTreenode, i, &psz_gid, &pEntry)  );
		SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx, pEntry, &pszName)  );

        SG_ERR_CHECK(  SG_treenode_entry__get_entry_type(pCtx, pEntry, &type)  );
        SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, pEntry, &psz_hid)  );

        if (SG_TREENODEENTRY_TYPE_DIRECTORY == type)
        {
            SG_ERR_CHECK(  sg_pack__do_blob(pCtx, psz_gid, psz_hid, gen, prb_blobs, prb_new)  );
            SG_ERR_CHECK(  sg_pack__get_dir(pCtx, pRepo, gen, psz_hid, prb_blobs, prb_new)  );
        }
        else if (SG_TREENODEENTRY_TYPE_REGULAR_FILE == type)
        {
            /* TODO use pszName file extension to decide whether to bother? */
            SG_ERR_CHECK(  sg_pack__do_blob(pCtx, psz_gid, psz_hid, gen, prb_blobs, prb_new)  );
        }
        else if (SG_TREENODEENTRY_TYPE_SYMLINK == type)
        {
            /* ignore symlinks */
        }
        else
        {
            SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  );
        }
	}

	SG_TREENODE_NULLFREE(pCtx, pTreenode);
	pTreenode = NULL;

	return;

fail:
	SG_TREENODE_NULLFREE(pCtx, pTreenode);
}

/**
 * This function is used to get a new tree.  It must be called with the HID of
 * the top-level user root directory.  Not the super-root.  The directory which
 * corresponds to @. */
static void sg_pack__do_get_dir__top(SG_context* pCtx, SG_repo* pRepo, SG_int32 gen, const char* pszidHidTreeNode, SG_rbtree* prb_blobs, SG_rbtree* prb_new)
{
	SG_treenode* pTreenode = NULL;
	SG_byte* pBytes = NULL;
    const SG_treenode_entry* pEntry = NULL;
    const char* pszidHidContent = NULL;

    /* Load the treenode.  It should have exactly one entry, a subdirectory,
     * named @ */
	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszidHidTreeNode, &pTreenode)  );
    SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx, pTreenode, 0, NULL, &pEntry)  );
    SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, pEntry, &pszidHidContent)  );

#ifdef DEBUG
    {
        SG_uint32 count;
        SG_treenode_entry_type type;
        const char* pszName = NULL;

        SG_ERR_CHECK(  SG_treenode__count(pCtx, pTreenode, &count)  );
        SG_ASSERT(1 == count);

		SG_ERR_CHECK(  SG_treenode_entry__get_entry_type(pCtx, pEntry, &type)  );
		SG_ASSERT (SG_TREENODEENTRY_TYPE_DIRECTORY == type);

		SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx, pEntry, &pszName)  );
        SG_ASSERT(0 == strcmp(pszName, "@"));
    }
#endif

    /* create the directory and then dive into it */
    SG_ERR_CHECK(  sg_pack__get_dir(pCtx, pRepo, gen, pszidHidContent, prb_blobs, prb_new)  );

	SG_TREENODE_NULLFREE(pCtx, pTreenode);

	return;

fail:
	/* TODO free stuff */
	SG_NULLFREE(pCtx, pBytes);
}

void sg_pack__do_changeset(SG_context* pCtx, SG_repo* pRepo, const char* psz_hid_cs, SG_rbtree* prb_blobs)
{
	SG_changeset* pcs = NULL;
    SG_int32 gen = 0;
    SG_uint32 count_blobs = 0;
    SG_uint32 count_parents = 0;
    SG_varray* pva_parents = NULL;
    SG_uint32 i;
    SG_rbtree* prb_new = NULL;
	const char* psz_hid_root_treenode = NULL;
    const char* psz_key = NULL;
    SG_vhash* pvh_lbl = NULL;
    SG_vhash* pvh_blobs = NULL;

	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, psz_hid_cs, &pcs)  );
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs, &psz_hid_root_treenode)  );
    SG_ERR_CHECK(  SG_changeset__get_generation(pCtx, pcs, &gen)  );

    SG_ERR_CHECK(  SG_RBTREE__ALLOC__PARAMS(pCtx, &prb_new, count_blobs, NULL)  );

    SG_ERR_CHECK(  SG_changeset__get_list_of_bloblists(pCtx, pcs, &pvh_lbl)  );

    /* add all the tree user file blobs */
    SG_ERR_CHECK(  SG_changeset__get_bloblist_name(pCtx, SG_BLOB_REFTYPE__TREEUSERFILE, &psz_key)  );
    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_lbl, psz_key, &pvh_blobs)  );
    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_blobs, &count_blobs)  );
    /* now write all the blobs */
    for (i=0; i<count_blobs; i++)
    {
        const char* psz_hid = NULL;

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_blobs, i, &psz_hid, NULL)  );
        SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb_new, psz_hid)  );
    }

    /* and the treenode blobs */
    SG_ERR_CHECK(  SG_changeset__get_bloblist_name(pCtx, SG_BLOB_REFTYPE__TREENODE, &psz_key)  );
    SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_lbl, psz_key, &pvh_blobs)  );
    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_blobs, &count_blobs)  );
    /* now write all the blobs */
    for (i=0; i<count_blobs; i++)
    {
        const char* psz_hid = NULL;

        SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb_new, psz_hid)  );
    }

    SG_ERR_CHECK(  sg_pack__do_get_dir__top(pCtx, pRepo, gen, psz_hid_root_treenode, prb_blobs, prb_new)  );

    SG_RBTREE_NULLFREE(pCtx, prb_new);

    SG_ERR_CHECK(  SG_changeset__get_parents(pCtx, pcs, &pva_parents)  );
    if (pva_parents)
    {
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva_parents, &count_parents)  );
        for (i=0; i<count_parents; i++)
        {
            const char* psz_hid = NULL;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva_parents, i, &psz_hid)  );

            SG_ERR_CHECK(  sg_pack__do_changeset(pCtx, pRepo, psz_hid, prb_blobs)  );
        }
    }

    SG_CHANGESET_NULLFREE(pCtx, pcs);

    return;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_new);
}

void SG_repo__unpack(SG_context* pCtx, SG_repo * pRepo, SG_blob_encoding blob_encoding)
{
    SG_vhash* pvh = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
	SG_repo_tx_handle* pTx;

    SG_ERR_CHECK(  SG_repo__list_blobs(pCtx, pRepo, blob_encoding, SG_FALSE, SG_FALSE, 500, 0, &pvh)  );
    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );
    for (i=0; i<count; i++)
    {
        const char* psz_hid = NULL;
        const SG_variant* pv = NULL;
        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh, i, &psz_hid, &pv)  );

		// Not a lot of thought went into doing each of these in its own repo tx.  Consider alternatives.
		SG_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
        SG_ERR_CHECK(  SG_repo__change_blob_encoding(pCtx, pRepo, pTx, psz_hid, SG_BLOBENCODING__FULL, NULL, NULL, NULL, NULL, NULL)  );
		SG_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );
    }
    SG_VHASH_NULLFREE(pCtx, pvh);

    return;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_repo__pack__zlib(SG_context* pCtx, SG_repo * pRepo)
{
    SG_vhash* pvh = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
	SG_repo_tx_handle* pTx;

    SG_ERR_CHECK(  SG_repo__list_blobs(pCtx, pRepo, SG_BLOBENCODING__FULL, SG_TRUE, SG_TRUE, 500, 0, &pvh)  );
    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );
    SG_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
    for (i=0; i<count; i++)
    {
        const char* psz_hid = NULL;
        const SG_variant* pv = NULL;
        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh, i, &psz_hid, &pv)  );
        SG_repo__change_blob_encoding(pCtx, pRepo, pTx,  psz_hid, SG_BLOBENCODING__ZLIB, NULL, NULL, NULL, NULL, NULL);

		if (SG_context__has_err(pCtx))
        {
            if (!SG_context__err_equals(pCtx,SG_ERR_REPO_BUSY))
            {
                SG_ERR_RETHROW;
            }
            else
            {
                SG_context__err_reset(pCtx);
            }
        }
    }
    SG_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );
    SG_VHASH_NULLFREE(pCtx, pvh);

    return;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
}

SG_free_callback _sg_repo__free_rbtree;
void _sg_repo__free_rbtree(SG_context* pCtx, void* assocData)
{
	SG_rbtree__free(pCtx, (SG_rbtree*)assocData);
}

void SG_repo__pack__vcdiff(SG_context* pCtx, SG_repo * pRepo)
{
	SG_rbtree* prb_leaves = NULL;
	SG_uint32 count_leaves = 0;
    const char* psz_hid_cs = NULL;
    SG_rbtree* prb_blobs = NULL;
    SG_bool b;
    SG_rbtree_iterator* pit = NULL;
    SG_rbtree_iterator* pit_for_gid = NULL;
    SG_bool b_for_gid;
    const char* psz_hid_ref = NULL;
    const char* psz_hid_blob = NULL;
    const char* psz_gid = NULL;
    SG_rbtree* prb = NULL;
    const char* psz_gen = NULL;
	SG_repo_tx_handle* pTx;

    SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo,SG_DAGNUM__VERSION_CONTROL,&prb_leaves)  );
    SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_leaves, &count_leaves)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, NULL, prb_leaves, &b, &psz_hid_cs, NULL)  );

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb_blobs)  );

    SG_ERR_CHECK(  sg_pack__do_changeset(pCtx, pRepo, psz_hid_cs, prb_blobs)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_blobs, &b, &psz_gid, (void**) &prb)  );
    while (b)
    {
        SG_uint32 count_for_gid = 0;
        SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count_for_gid)  );
        if (count_for_gid > 1)
        {
            psz_hid_ref = NULL;
            SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit_for_gid, prb, &b_for_gid, &psz_gen, (void**) &psz_hid_blob)  );
            while (b_for_gid)
            {
				// Not a lot of thought went into doing each of these in its own repo tx.  Consider alternatives.
				SG_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );

				if (psz_hid_ref)
                {
                    SG_ERR_CHECK(  SG_repo__change_blob_encoding(pCtx, pRepo, pTx, psz_hid_blob, SG_BLOBENCODING__VCDIFF, psz_hid_ref, NULL, NULL, NULL, NULL)  );
                    // TODO be tolerant here of SG_ERR_REPO_BUSY
                }
                else
                {
                    psz_hid_ref = psz_hid_blob;

                    SG_ERR_CHECK(  SG_repo__change_blob_encoding(pCtx, pRepo, pTx, psz_hid_ref, SG_BLOBENCODING__FULL, NULL, NULL, NULL, NULL, NULL)  );
                    // TODO be tolerant here of SG_ERR_REPO_BUSY
                }

				SG_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );

                SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit_for_gid, &b_for_gid, &psz_gen, (void**) &psz_hid_blob)  );
            }
            SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit_for_gid);
            psz_hid_ref = NULL;
        }
        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_gid, (void**) &prb)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, prb_blobs, _sg_repo__free_rbtree);

    SG_RBTREE_NULLFREE(pCtx, prb_leaves);

    return;

fail:
    return;
}

