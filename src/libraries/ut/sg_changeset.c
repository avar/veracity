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
 * @file sg_changeset.c
 *
 * @details Routines for manipulating the contents of a Changeset.
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#	define TRACE_CHANGESET		0
#else
#	define TRACE_CHANGESET		0
#endif

/**
 * SG_changeset is a structure containing information on a single
 * changeset (a commit).  This includes all of the parent changesets
 * that it depends on (that it was a change from) and handles to
 * the various data structures that we need to represent this state
 * of the user's project and a list of blobs that are considered
 * new with respect to the parents.
 *
 * Changesets are converted to JSON and stored in Changeset-type
 * Blobs in the Repository.
 */
struct SG_changeset
{
    /**
     * A changeset struct can be in one of two states: constructing
     * or frozen.  During the constructing phase, data is accumulated
     * in members used only during that phase, in a substruct.  Then,
     * when committing to the repo, the changeset is frozen, the data
     * converted to a vhash.
     *
     * When a changeset is read from the repo, it is frozen, and the
     * constructing substruct is not used.
     */

    struct
    {
        SG_changeset_version ver;
        SG_uint32 iDagNum;
        SG_int32 generation;
        char* psz_hid_root;
        SG_rbtree* prb_bloblists;
        SG_rbtree * prb_parents;
        SG_rbtree * prb_treepaths;
    } constructing;

    struct
    {
        SG_vhash* pvh;
        char* psz_hid;
    } frozen;
};

//////////////////////////////////////////////////////////////////

/*
 * We store the following keys/values in the hash.  Some keys are
 * always present and some may be version-dependent.  Since we
 * don't know what future keys/values may be required, we leave this
 * an open-ended list.
 */

/* Always present. */
#define KEY_VERSION                 "ver"

/* Present in Version 1 changeset (may also be used by future versions). */
#define KEY_DAGNUM                  "dagnum"
#define KEY_PARENTS                 "parents"
#define KEY_ROOT                    "root"
#define KEY_GENERATION              "generation"
#define KEY_BLOBS                   "blobs"
#define KEY_TREEPATHS               "treepaths"

#define KEY_BLOBLIST_TREENODE       "treenode"
#define KEY_BLOBLIST_TREEUSERFILE   "treeuserfile"
#define KEY_BLOBLIST_TREESYMLINK    "treesymlink"
#define KEY_BLOBLIST_TREEATTRIBS    "treeattribs"
#define KEY_BLOBLIST_DBINFO         "dbinfo"
#define KEY_BLOBLIST_DBRECORD       "dbrecord"
#define KEY_BLOBLIST_DBUSERFILE     "dbuserfile"
#define KEY_BLOBLIST_DBTEMPLATE     "dbtemplate"

//////////////////////////////////////////////////////////////////

#define FAIL_IF_FROZEN(pChangeset)                                               \
	SG_STATEMENT(                                                                \
		SG_bool b = SG_TRUE;                                                     \
		SG_ERR_CHECK_RETURN(  SG_changeset__is_frozen(pCtx,(pChangeset),&b)  );  \
		if (b)                                                                   \
			SG_ERR_THROW_RETURN( SG_ERR_INVALID_WHILE_FROZEN );                  )

#define FAIL_IF_NOT_FROZEN(pChangeset)                                           \
	SG_STATEMENT(                                                                \
		SG_bool b = SG_FALSE;                                                    \
		SG_ERR_CHECK_RETURN(  SG_changeset__is_frozen(pCtx,(pChangeset),&b)  );  \
		if (!b)                                                                  \
			SG_ERR_THROW_RETURN( SG_ERR_INVALID_UNLESS_FROZEN );                 )

#define FAIL_IF_DB_DAG(pChangeset)                                               \
	SG_STATEMENT(                                                                \
		if (SG_DAGNUM__IS_DB(pChangeset->constructing.iDagNum))                  \
			SG_ERR_THROW_RETURN( SG_ERR_WRONG_DAG_TYPE );                        )

//////////////////////////////////////////////////////////////////

void SG_changeset__alloc__committing(SG_context * pCtx, SG_uint32 iDagNum, SG_changeset ** ppNew)
{
	SG_changeset* pChangeset = NULL;

	SG_NULLARGCHECK_RETURN(ppNew);

	SG_ERR_CHECK(  SG_alloc1(pCtx, pChangeset)  );

    pChangeset->constructing.iDagNum = iDagNum;

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pChangeset->constructing.prb_bloblists)  );

	*ppNew = pChangeset;
    pChangeset = NULL;

fail:
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
}

//////////////////////////////////////////////////////////////////

void SG_changeset__free(SG_context * pCtx, SG_changeset * pChangeset)
{
	if (!pChangeset)
    {
		return;
    }

	SG_VHASH_NULLFREE(pCtx, pChangeset->frozen.pvh);
	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pChangeset->constructing.prb_bloblists, (SG_free_callback*) SG_rbtree__free);
	SG_RBTREE_NULLFREE(pCtx, pChangeset->constructing.prb_parents);
	SG_RBTREE_NULLFREE(pCtx, pChangeset->constructing.prb_treepaths);
	SG_NULLFREE(pCtx, pChangeset->frozen.psz_hid);
	SG_NULLFREE(pCtx, pChangeset->constructing.psz_hid_root);

	SG_NULLFREE(pCtx, pChangeset);
}

void SG_changeset__get_bloblist_name(
        SG_context* pCtx,
        SG_uint16 iBlobRefType,
        const char** ppsz_key
        )
{
	switch (iBlobRefType)
	{
		case SG_BLOB_REFTYPE__TREENODE:
            *ppsz_key = KEY_BLOBLIST_TREENODE;
			break;

		case SG_BLOB_REFTYPE__TREEUSERFILE:
            *ppsz_key = KEY_BLOBLIST_TREEUSERFILE;
			break;

		case SG_BLOB_REFTYPE__TREESYMLINK:
            *ppsz_key = KEY_BLOBLIST_TREESYMLINK;
			break;

		case SG_BLOB_REFTYPE__TREEATTRIBS:
            *ppsz_key = KEY_BLOBLIST_TREEATTRIBS;
			break;

		case SG_BLOB_REFTYPE__DBINFO:
            *ppsz_key = KEY_BLOBLIST_DBINFO;
			break;

		case SG_BLOB_REFTYPE__DBRECORD:
            *ppsz_key = KEY_BLOBLIST_DBRECORD;
			break;

		case SG_BLOB_REFTYPE__DBUSERFILE:
            *ppsz_key = KEY_BLOBLIST_DBUSERFILE;
			break;

		case SG_BLOB_REFTYPE__DBTEMPLATE:
            *ppsz_key = KEY_BLOBLIST_DBTEMPLATE;
			break;

		default:
			SG_ERR_THROW(  SG_ERR_UNKNOWN_BLOB_TYPE  );
	}

fail:
    ;
}

/**
 * Returns a reference to one of SG_changeset's internal bloblists, which corresponds to a single blob type.
 * Optionally returns a key corresponding to the blob type apppropriate for vhash serialization of the changeset.
 * If bCreateIfNeeded is true, a new (empty) rbtree for the requested blob type's list will be created if it doesn't already exist.
 */
static void sg_changeset__get_internal_blob_list__named(
        SG_context * pCtx,
        SG_changeset* pcs,
        const char* psz_key,
        SG_bool bCreateIfNeeded,
        SG_rbtree** pprb
        )
{
    SG_bool b_already = SG_FALSE;
    SG_rbtree* prb = NULL;

	SG_ERR_CHECK(  SG_rbtree__find(pCtx, pcs->constructing.prb_bloblists, psz_key, &b_already, (void**) &prb)  );
    if (!prb)
    {
        if (bCreateIfNeeded)
        {
            SG_ERR_CHECK(  SG_rbtree__alloc(pCtx, &prb)  );
            SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, pcs->constructing.prb_bloblists, psz_key, prb)  );
        }
    }

	*pprb = prb;

fail:
	return;
}

static void sg_changeset__get_internal_blob_list(
        SG_context * pCtx,
        SG_changeset* pcs,
        SG_uint16 iBlobRefType,
        SG_bool bCreateIfNeeded,
        SG_rbtree** pprb,
        const char** ppsz_key
        )
{
    const char* psz_key = NULL;

    SG_ERR_CHECK(  SG_changeset__get_bloblist_name(pCtx, iBlobRefType, &psz_key)  );
    SG_ERR_CHECK(  sg_changeset__get_internal_blob_list__named(pCtx, pcs, psz_key, bCreateIfNeeded, pprb)  );

    if (ppsz_key)
    {
        *ppsz_key = psz_key;
    }

fail:
	return;
}

static void _sg_changeset__serialize_parents(
	SG_context * pCtx,
    SG_changeset* pcs,
	SG_vhash* pvh
	)
{
	// convert the given idset into a sorted varray and add it to
	// or update the value of the n/v pair with the given key in
	// the top-level vhash.
	//
	// if the idset is null, make sure that the corresponding key
	// in the top-level vhash is set correctly.

	SG_varray * pva = NULL;
	SG_uint32 count;
    SG_bool b_has = SG_FALSE;

	if (!pcs->constructing.prb_parents)
		goto NoIds;

	SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx, pcs->constructing.prb_parents, &count)  );

	if (count == 0)
		goto NoIds;

	SG_ERR_CHECK_RETURN(  SG_rbtree__to_varray__keys_only(pCtx, pcs->constructing.prb_parents, &pva)  );

	SG_ERR_CHECK( SG_vhash__add__varray(pCtx, pvh, KEY_PARENTS, &pva) );

    goto done;

NoIds:
    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh, KEY_PARENTS, &b_has)  );
    if (b_has)
    {
        SG_ERR_CHECK(  SG_vhash__remove(pCtx, pvh, KEY_PARENTS)  );
    }

done:
fail:
    SG_VARRAY_NULLFREE(pCtx, pva);
}

static void _sg_changeset__serialize_treepaths(
	SG_context * pCtx,
    SG_changeset* pcs,
	SG_vhash* pvh
	)
{
	// convert the given idset into a sorted varray and add it to
	// or update the value of the n/v pair with the given key in
	// the top-level vhash.
	//
	// if the idset is null, make sure that the corresponding key
	// in the top-level vhash is set correctly.

	SG_vhash * pvh_treepaths = NULL;
	SG_uint32 count;
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_gid = NULL;
    const char* psz_path = NULL;
    SG_bool b_has = SG_FALSE;

	if (!pcs->constructing.prb_treepaths)
    {
		goto NoIds;
    }

	SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx, pcs->constructing.prb_treepaths, &count)  );

	if (count == 0)
    {
		goto NoIds;
    }

    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_treepaths)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pcs->constructing.prb_treepaths, &b, &psz_gid, (void**) &psz_path)  );
    while (b)
    {
        SG_uint32 len = strlen(psz_path);

        // treendx paths never end in a slash
        if ('/' == psz_path[len-1])
        {
            len--;
        }

        SG_ERR_CHECK(  SG_vhash__add__string__buflen(pCtx, pvh_treepaths, psz_gid, psz_path, len)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_gid, (void**) &psz_path)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

	SG_ERR_CHECK( SG_vhash__add__vhash(pCtx, pvh, KEY_TREEPATHS, &pvh_treepaths) );

    goto done;

NoIds:
    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh, KEY_TREEPATHS, &b_has)  );
    if (b_has)
    {
        SG_ERR_CHECK(  SG_vhash__remove(pCtx, pvh, KEY_TREEPATHS)  );
    }

done:
fail:
    SG_VHASH_NULLFREE(pCtx, pvh_treepaths);
}

static void _sg_changeset__convert_rbtree_with_lengths_to_vhash(
	SG_context * pCtx,
	SG_vhash* pvh,
	SG_rbtree* prb,
	const char* szKey
	)
{
	// convert the given idset into a sorted varray and add it to
	// or update the value of the n/v pair with the given key in
	// the top-level vhash.
	//
	// if the idset is null, make sure that the corresponding key
	// in the top-level vhash is set correctly.

	SG_vhash * pvh_bl = NULL;
	SG_uint32 count;
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;
    const char* psz_hid_blob = NULL;
    const char* psz_len = NULL;
    SG_bool b_has = SG_FALSE;

	if (!prb)
		goto NoIds;

	SG_ERR_CHECK_RETURN(  SG_rbtree__count(pCtx, prb, &count)  );

	if (count == 0)
		goto NoIds;

    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_bl)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb, &b, &psz_hid_blob, (void**) &psz_len)  );
    while (b)
    {
        SG_int64 ival = 0;

        SG_ERR_CHECK(  SG_int64__parse__strict(pCtx, &ival, psz_len)  );
        SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh_bl, psz_hid_blob, ival)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_blob, (void**) &psz_len)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

	SG_ERR_CHECK( SG_vhash__add__vhash(pCtx, pvh, szKey, &pvh_bl) );

    goto done;

NoIds:
    SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh, szKey, &b_has)  );
    if (b_has)
    {
        SG_ERR_CHECK(  SG_vhash__remove(pCtx, pvh, szKey)  );
    }

done:
fail:
    SG_VHASH_NULLFREE(pCtx, pvh_bl);
}

static void sg_changeset__convert_rbtree_to_bloblist(SG_context * pCtx, SG_changeset* pChangeset, SG_vhash* pvh, SG_uint16 reftype)
{
	SG_rbtree* prb = NULL;
	const char* psz_key = NULL;

	SG_ERR_CHECK(  sg_changeset__get_internal_blob_list(pCtx, pChangeset, reftype, SG_FALSE, &prb, &psz_key)  );
	SG_ERR_CHECK(  _sg_changeset__convert_rbtree_with_lengths_to_vhash(pCtx, pvh, prb, psz_key)  );

	return;
fail:
	return;
}

static void _sg_changeset__create_the_vhash(SG_context * pCtx, SG_changeset * pChangeset)
{
	SG_vhash* pvh_bloblists = NULL;
	SG_vhash* pvh_bloblengths = NULL;
    SG_rbtree_iterator* pIterator = NULL;
	const SG_uint16* p_cur_reftype = NULL;
    SG_vhash* pvh = NULL;

	SG_NULLARGCHECK_RETURN(pChangeset);

	FAIL_IF_FROZEN(pChangeset);

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_bloblists)  );

	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, KEY_GENERATION, (SG_int64)(pChangeset->constructing.generation))  );

	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, KEY_DAGNUM, (SG_int64)(pChangeset->constructing.iDagNum))  );

	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, KEY_VERSION, (SG_int64)(pChangeset->constructing.ver))  );

	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, KEY_ROOT, pChangeset->constructing.psz_hid_root)  );

	SG_ERR_CHECK(  _sg_changeset__serialize_parents(pCtx, pChangeset, pvh)  );

	SG_ERR_CHECK(  _sg_changeset__serialize_treepaths(pCtx, pChangeset, pvh)  );

	p_cur_reftype = g_reftypes;
	while (*p_cur_reftype)
	{
		SG_ERR_CHECK(  sg_changeset__convert_rbtree_to_bloblist(pCtx, pChangeset, pvh_bloblists, *p_cur_reftype)  );
		p_cur_reftype++;
	}

	SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh, KEY_BLOBS, &pvh_bloblists)  );

    pChangeset->frozen.pvh = pvh;
    pvh = NULL;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
	SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIterator);
	SG_VHASH_NULLFREE(pCtx, pvh_bloblists);
	SG_VHASH_NULLFREE(pCtx, pvh_bloblengths);
}

void SG_changeset__set_version(SG_context * pCtx, SG_changeset * pChangeset, SG_changeset_version ver)
{
	SG_NULLARGCHECK_RETURN(pChangeset);

	FAIL_IF_FROZEN(pChangeset);

    pChangeset->constructing.ver = ver;
}

void SG_changeset__add_treepath(SG_context * pCtx, SG_changeset * pChangeset, const char* psz_gid, const char* psz_path)
{
	SG_NULLARGCHECK_RETURN(pChangeset);

	FAIL_IF_FROZEN(pChangeset);
	FAIL_IF_DB_DAG(pChangeset);

	// create rbtree if necessary (we deferred allocation until needed)

	if (!pChangeset->constructing.prb_treepaths)
	{
		SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC__PARAMS(pCtx, &pChangeset->constructing.prb_treepaths, 4, NULL)  );
	}

	SG_ERR_CHECK_RETURN(  SG_rbtree__update__with_pooled_sz(pCtx, pChangeset->constructing.prb_treepaths, psz_gid, psz_path)  );
}

void SG_changeset__add_parent(SG_context * pCtx, SG_changeset * pChangeset, const char* pszidHidParent)
{
	SG_NULLARGCHECK_RETURN(pChangeset);

	FAIL_IF_FROZEN(pChangeset);

	if (!pszidHidParent)
		return;

	// create rbtree if necessary (we deferred allocation until needed)

	if (!pChangeset->constructing.prb_parents)
	{
		SG_ERR_CHECK_RETURN(  SG_RBTREE__ALLOC__PARAMS(pCtx, &pChangeset->constructing.prb_parents, 4, NULL)  );
	}

	// finally, actually add HID to idset.

	SG_ERR_CHECK_RETURN(  SG_rbtree__update(pCtx, pChangeset->constructing.prb_parents, pszidHidParent)  );
}

void SG_changeset__set_root(SG_context * pCtx, SG_changeset * pChangeset, const char* pszidHid)
{
	SG_NULLARGCHECK_RETURN(pChangeset);

	FAIL_IF_FROZEN(pChangeset);

    SG_ASSERT(!pChangeset->constructing.psz_hid_root);

    if (pszidHid)
    {
        SG_ERR_CHECK_RETURN(  SG_strdup(pCtx, pszidHid, &pChangeset->constructing.psz_hid_root)  );
    }
}

static void sg_changeset__add_blob_to_list__named(SG_context * pCtx, SG_changeset * pChangeset, const char* pszidHidBlob, const char* psz_bloblist_name, const char* psz_len)
{
	SG_rbtree* prb = NULL;

	SG_NULLARGCHECK_RETURN(pChangeset);
	SG_NULLARGCHECK_RETURN(pszidHidBlob);

	FAIL_IF_FROZEN(pChangeset);

	SG_ERR_CHECK(  sg_changeset__get_internal_blob_list__named(pCtx, pChangeset, psz_bloblist_name, SG_TRUE, &prb)  );

	SG_ERR_CHECK(  SG_rbtree__update__with_pooled_sz(pCtx, prb, pszidHidBlob, psz_len)  );

fail:
    ;
}

void SG_changeset__add_blob_to_list(SG_context * pCtx, SG_changeset * pChangeset, const char* pszidHidBlob, SG_uint16 iBlobRefType, SG_int64 iBlobFullLength)
{
	SG_rbtree* prb = NULL;
    SG_int_to_string_buffer buf;

	SG_NULLARGCHECK_RETURN(pChangeset);
	SG_NULLARGCHECK_RETURN(pszidHidBlob);

	FAIL_IF_FROZEN(pChangeset);

#if defined(DEBUG)
    if (
            (iBlobRefType != SG_BLOB_REFTYPE__TREEUSERFILE)
            && (iBlobRefType != SG_BLOB_REFTYPE__DBUSERFILE)
       )
    {
        SG_ASSERT(iBlobFullLength > 0);
    }
#endif

	SG_ERR_CHECK(  sg_changeset__get_internal_blob_list(pCtx, pChangeset, iBlobRefType, SG_TRUE, &prb, NULL)  );

    SG_int64_to_sz(iBlobFullLength,buf);
	SG_ERR_CHECK(  SG_rbtree__update__with_pooled_sz(pCtx, prb, pszidHidBlob, buf)  );

fail:
    ;
}

static void _get_generation_from_repo(SG_context * pCtx, SG_repo * pRepo, const char* pszidHid, SG_int32 * pGen)
{
	// load the dagnode for the given HID into memory and
	// extract the generation.

	SG_dagnode* pdn = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pszidHid);
	SG_NULLARGCHECK_RETURN(pGen);

	SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, pszidHid, &pdn)  );
	SG_ERR_CHECK(  SG_dagnode__get_generation(pCtx, pdn, pGen)  );

	SG_DAGNODE_NULLFREE(pCtx, pdn);

	return;
fail:
	SG_DAGNODE_NULLFREE(pCtx, pdn);
}

static void _sg_changeset__add_to_big_bloblist(SG_context * pCtx, SG_rbtree* prb_big, SG_vhash* pvh_lbl)
{
	const SG_uint16* p_cur_reftype = NULL;
    SG_rbtree* prb_alloc = NULL;

	p_cur_reftype = g_reftypes;
	while (*p_cur_reftype)
	{
        const char* psz_key = NULL;
        SG_bool b = SG_FALSE;
        SG_bool b_has = SG_FALSE;
        SG_vhash* pvh_one_from = NULL;

        SG_ERR_CHECK(  SG_changeset__get_bloblist_name(pCtx, *p_cur_reftype, &psz_key)  );
        SG_ERR_CHECK(  SG_vhash__has(pCtx, pvh_lbl, psz_key, &b_has)  );
        if (b_has)
        {
            SG_rbtree* prb_one_big = NULL;
            SG_uint32 count = 0;
            SG_uint32 i = 0;

            SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_lbl, psz_key, &pvh_one_from)  );

            SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_big, psz_key, &b, (void**) &prb_one_big)  );
            if (!prb_one_big)
            {
                SG_ERR_CHECK(  SG_rbtree__alloc(pCtx, &prb_alloc)  );
                SG_ERR_CHECK(  SG_rbtree__add__with_assoc(pCtx, prb_big, psz_key, prb_alloc)  );
                prb_one_big = prb_alloc;
                prb_alloc = NULL;
            }

            SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_one_from, &count)  );
            for (i = 0; i < count; i++)
            {
                const char* psz_hid = NULL;
                const SG_variant* pv = NULL;
                SG_int64 len = 0;
                SG_int_to_string_buffer buf;

                SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_one_from, i, &psz_hid, &pv)  );
                SG_ERR_CHECK(  SG_variant__get__int64(pCtx, pv, &len)  );
                SG_int64_to_sz(len,buf);
                SG_ERR_CHECK(  SG_rbtree__update__with_pooled_sz(pCtx, prb_one_big, psz_hid, buf)  );
            }
        }

		p_cur_reftype++;
	}

fail:
    SG_RBTREE_NULLFREE(pCtx, prb_alloc);
}

static void _sg_changeset__get_big_bloblist(SG_context * pCtx, SG_repo * pRepo, SG_stringarray* psa_trail, SG_rbtree** pprb)
{
    SG_changeset* pcs = NULL;
    SG_rbtree* prb = NULL;
    SG_vhash* pvh_lbl = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_trail, &count)  );
    SG_ERR_CHECK(  SG_rbtree__alloc(pCtx, &prb)  );

    for (i=0; i<count; i++)
    {
        const char* psz_csid = NULL;

        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa_trail, i, &psz_csid)  );
        SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, psz_csid, &pcs)  );
        SG_ERR_CHECK(  SG_changeset__get_list_of_bloblists(pCtx, pcs, &pvh_lbl)  );
        SG_ERR_CHECK(  _sg_changeset__add_to_big_bloblist(pCtx, prb, pvh_lbl)  );
        SG_CHANGESET_NULLFREE(pCtx, pcs);
    }

    *pprb = prb;
    prb = NULL;

fail:
    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_RBTREE_NULLFREE(pCtx, prb);
}

static void _sg_changeset__get_big_treepathlist(SG_context * pCtx, SG_repo * pRepo, SG_stringarray* psa_trail, SG_rbtree** pprb)
{
    SG_changeset* pcs = NULL;
    SG_rbtree* prb = NULL;
    SG_vhash* pvh_treepaths = NULL;
    SG_uint32 count = 0;
    SG_uint32 i = 0;
    SG_string* pstr = NULL;

    SG_ERR_CHECK(  SG_stringarray__count(pCtx, psa_trail, &count)  );
    SG_ERR_CHECK(  SG_rbtree__alloc(pCtx, &prb)  );

    for (i=0; i<count; i++)
    {
        const char* psz_csid = NULL;

        SG_ERR_CHECK(  SG_stringarray__get_nth(pCtx, psa_trail, i, &psz_csid)  );
        SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, psz_csid, &pcs)  );
        SG_ERR_CHECK(  SG_changeset__get_treepaths(pCtx, pcs, &pvh_treepaths)  );
        if (pvh_treepaths)
        {
            SG_uint32 count_treepaths = 0;
            SG_uint32 i = 0;

            SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_treepaths, &count_treepaths)  );
            for (i=0; i<count_treepaths; i++)
            {
                const char* psz_gid = NULL;
                const SG_variant* pv = NULL;
                const char* psz_path = NULL;

                SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_treepaths, i, &psz_gid, &pv)  );
                SG_ERR_CHECK(  SG_variant__get__sz(pCtx, pv, &psz_path)  );

                SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
                SG_ERR_CHECK(  SG_string__sprintf(pCtx, pstr, "%s.%s", psz_gid, psz_path)  );
                SG_ERR_CHECK(  SG_rbtree__update(pCtx, prb, SG_string__sz(pstr))  );
                SG_STRING_NULLFREE(pCtx, pstr);
            }
        }
        SG_CHANGESET_NULLFREE(pCtx, pcs);
    }

    *pprb = prb;
    prb = NULL;

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
    SG_CHANGESET_NULLFREE(pCtx, pcs);
    SG_RBTREE_NULLFREE(pCtx, prb);
}

static void x_walk_back(
        SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_csid_cur,
        SG_dagnode* pdn_to,
        SG_stringarray* psa,
        SG_bool* pb_found
        )
{
	SG_dagnode* pdn_cur = NULL;
	SG_uint32 count_parents = 0;
	SG_uint32 i = 0;
	const char** paParents = NULL;
    SG_bool b_found = SG_FALSE;
    const char* psz_csid_to = NULL;
    SG_int32 gen_to = 0;
    SG_int32 gen_cur = 0;

    SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pdn_to, &psz_csid_to)  );
    SG_ERR_CHECK(  SG_dagnode__get_generation(pCtx, pdn_to, &gen_to)  );

    if (0 == strcmp(psz_csid_cur, psz_csid_to))
    {
        b_found = SG_TRUE;
    }
    else
    {
        SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, psz_csid_cur, &pdn_cur)  );
        SG_ERR_CHECK(  SG_dagnode__get_generation(pCtx, pdn_cur, &gen_cur)  );
        if (gen_cur > gen_to)
        {
            SG_ERR_CHECK(  SG_dagnode__get_parents(pCtx, pdn_cur, &count_parents, &paParents)  );
            for (i=0; i<count_parents; i++)
            {
                SG_bool b = SG_FALSE;

                SG_ERR_CHECK(  x_walk_back(pCtx, pRepo, paParents[i], pdn_to, psa, &b)  );
                if (b)
                {
                    SG_ERR_CHECK(  SG_stringarray__add(pCtx, psa, psz_csid_cur)  );
                    b_found = SG_TRUE;
                    break;
                }
            }
        }
    }

    *pb_found = b_found;

fail:
    SG_NULLFREE(pCtx, paParents);
    SG_DAGNODE_NULLFREE(pCtx, pdn_cur);
}

static void x_find_dag_trail(
        SG_context* pCtx,
        SG_repo* pRepo,
        const char* psz_csid_from,
        const char* psz_csid_to,
        SG_stringarray** ppsa
        )
{
    SG_stringarray* psa = NULL;
    SG_dagnode* pdn_to = NULL;
    SG_bool b_found = SG_FALSE;

    SG_ERR_CHECK(  SG_stringarray__alloc(pCtx, &psa, 5)  );

    SG_ASSERT(psz_csid_to);
    SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, psz_csid_to, &pdn_to)  );
    SG_ERR_CHECK(  x_walk_back(pCtx, pRepo, psz_csid_from, pdn_to, psa, &b_found)  );

    if (!b_found)
    {
        SG_ERR_THROW(  SG_ERR_NOTIMPLEMENTED  ); // TODO real error
    }

    *ppsa = psa;
    psa = NULL;

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa);
    SG_DAGNODE_NULLFREE(pCtx, pdn_to);
}

static void _sg_changeset__normalize_blob_lists(SG_context * pCtx, SG_changeset * pChangeset, SG_repo * pRepo)
{
    SG_uint32 count_parents = 0;
    SG_uint32 i = 0;
    SG_daglca* pDagLca = NULL;
    const char* psz_temp = NULL;
    char* psz_hid_cs_lca = NULL;
    SG_rbtree** a_prb_lists = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_vhash* pvh_counts = NULL;
	const SG_uint16* p_cur_reftype = NULL;
    SG_bool b = SG_FALSE;
    SG_rbtree* prb_lengths = NULL;
    SG_stringarray* psa_trail = NULL;

	SG_NULLARGCHECK_RETURN(pChangeset);
	SG_NULLARGCHECK_RETURN(pRepo);

	FAIL_IF_FROZEN(pChangeset);

    if (pChangeset->constructing.prb_parents)
    {
        SG_ERR_CHECK(  SG_rbtree__count(pCtx, pChangeset->constructing.prb_parents, &count_parents)  );
        if (count_parents > 1)
        {
            const char* psz_hid_parent = NULL;

            // find the LCA

            SG_ERR_CHECK(  SG_repo__get_dag_lca(pCtx, pRepo, pChangeset->constructing.iDagNum, pChangeset->constructing.prb_parents, &pDagLca)  );
            SG_ERR_CHECK(  SG_daglca__iterator__first(pCtx,
                                                      NULL,
                                                      pDagLca,SG_FALSE,
                                                      &psz_temp,	// we do not own this
                                                      NULL,NULL,NULL)  );

            SG_ERR_CHECK(  SG_strdup(pCtx, psz_temp, &psz_hid_cs_lca)  );

            // get a big bloblist for each parent
            SG_ERR_CHECK(  SG_allocN(pCtx, count_parents, a_prb_lists)  );
            b = SG_FALSE;
            i = 0;
            SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pChangeset->constructing.prb_parents, &b, &psz_hid_parent, NULL)  );
            while (b)
            {
                SG_ERR_CHECK(  x_find_dag_trail(pCtx, pRepo, psz_hid_parent, psz_hid_cs_lca, &psa_trail)  );
                SG_ERR_CHECK(  _sg_changeset__get_big_bloblist(pCtx, pRepo, psa_trail, &a_prb_lists[i++])  );
                SG_STRINGARRAY_NULLFREE(pCtx, psa_trail);

                SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_parent, NULL)  );
            }
            SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

            // init the counts
            SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_counts)  );
            p_cur_reftype = g_reftypes;
            while (*p_cur_reftype)
            {
                const char* psz_key = NULL;
                SG_vhash* pvh_dontcare = NULL;

                SG_ERR_CHECK(  SG_changeset__get_bloblist_name(pCtx, *p_cur_reftype, &psz_key)  );
                SG_ERR_CHECK(  SG_vhash__addnew__vhash(pCtx, pvh_counts, psz_key, &pvh_dontcare)  );

                p_cur_reftype++;
            }

            // go through all the big blob lists and add them to the counts
            SG_ERR_CHECK(  SG_rbtree__alloc(pCtx, &prb_lengths)  );
            for (i=0; i<count_parents; i++)
            {
                p_cur_reftype = g_reftypes;
                while (*p_cur_reftype)
                {
                    const char* psz_key = NULL;
                    const char* psz_len = NULL;
                    SG_rbtree* prb_one_from = NULL;
                    SG_vhash* pvh_one_counts = NULL;
                    const char* psz_hid_blob = NULL;

                    SG_ERR_CHECK(  SG_changeset__get_bloblist_name(pCtx, *p_cur_reftype, &psz_key)  );
                    SG_ERR_CHECK(  SG_rbtree__find(pCtx, a_prb_lists[i], psz_key, &b, (void**) &prb_one_from)  );

                    if (prb_one_from)
                    {
                        SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_counts, psz_key, &pvh_one_counts)  );

                        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_one_from, &b, &psz_hid_blob, (void**) &psz_len)  );
                        while (b)
                        {
                            SG_ERR_CHECK(  SG_rbtree__update__with_pooled_sz(pCtx, prb_lengths, psz_hid_blob, psz_len)  );
                            SG_ERR_CHECK(  SG_vhash__addtoval__int64(pCtx, pvh_one_counts, psz_hid_blob, 1)  );

                            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_blob, (void**) &psz_len)  );
                        }
                        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
                    }

                    p_cur_reftype++;
                }
            }

            // now find all the blobs with counts too low
            p_cur_reftype = g_reftypes;
            while (*p_cur_reftype)
            {
                const char* psz_key = NULL;
                SG_vhash* pvh_one_counts = NULL;
                SG_uint32 count_this_key = 0;
                SG_uint32 i = 0;

                SG_ERR_CHECK(  SG_changeset__get_bloblist_name(pCtx, *p_cur_reftype, &psz_key)  );
                SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh_counts, psz_key, &pvh_one_counts)  );
                SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_one_counts, &count_this_key)  );
                for (i=0; i<count_this_key; i++)
                {
                    const char* psz_hid_blob = NULL;
                    const SG_variant * pv = NULL;
                    SG_int64 val = 0;

                    SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_one_counts, i, &psz_hid_blob, &pv)  );
                    SG_ERR_CHECK(  SG_variant__get__int64(pCtx, pv, &val)  );
                    if (val < count_parents)
                    {
                        const char* psz_len = NULL;

                        SG_ERR_CHECK(  SG_rbtree__find(pCtx, prb_lengths, psz_hid_blob, NULL, (void**) &psz_len)  );
                        SG_ERR_CHECK(  sg_changeset__add_blob_to_list__named(pCtx, pChangeset, psz_hid_blob, psz_key, psz_len)  );
                    }
                }

                p_cur_reftype++;
            }
        }
    }

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_trail);
    SG_VHASH_NULLFREE(pCtx, pvh_counts);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_RBTREE_NULLFREE(pCtx, prb_lengths);
    SG_NULLFREE(pCtx, psz_hid_cs_lca);
    SG_DAGLCA_NULLFREE(pCtx, pDagLca);
    if (a_prb_lists)
    {
        for (i=0; i<count_parents; i++)
        {
            SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, a_prb_lists[i], (SG_free_callback*) SG_rbtree__free);
        }
        SG_NULLFREE(pCtx, a_prb_lists);
    }
}

static void _sg_changeset__normalize_treepaths(SG_context * pCtx, SG_changeset * pChangeset, SG_repo * pRepo)
{
    SG_uint32 count_paths = 0;
    SG_uint32 count_parents = 0;
    SG_uint32 i = 0;
    SG_daglca* pDagLca = NULL;
    const char* psz_temp = NULL;
    char* psz_hid_cs_lca = NULL;
    SG_rbtree** a_prb_lists = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_vhash* pvh_counts = NULL;
    SG_bool b = SG_FALSE;
    SG_stringarray* psa_trail = NULL;

	SG_NULLARGCHECK_RETURN(pChangeset);
	SG_NULLARGCHECK_RETURN(pRepo);

	FAIL_IF_FROZEN(pChangeset);

    if (pChangeset->constructing.prb_parents)
    {
        SG_ERR_CHECK(  SG_rbtree__count(pCtx, pChangeset->constructing.prb_parents, &count_parents)  );
        if (count_parents > 1)
        {
            const char* psz_hid_parent = NULL;

            // find the LCA

            SG_ERR_CHECK(  SG_repo__get_dag_lca(pCtx, pRepo, pChangeset->constructing.iDagNum, pChangeset->constructing.prb_parents, &pDagLca)  );
            SG_ERR_CHECK(  SG_daglca__iterator__first(pCtx,
                                                      NULL,
                                                      pDagLca,SG_FALSE,
                                                      &psz_temp,	// we do not own this
                                                      NULL,NULL,NULL)  );

            SG_ERR_CHECK(  SG_strdup(pCtx, psz_temp, &psz_hid_cs_lca)  );

            // get a big treepathlist for each parent
            SG_ERR_CHECK(  SG_allocN(pCtx, count_parents, a_prb_lists)  );
            b = SG_FALSE;
            i = 0;
            SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pChangeset->constructing.prb_parents, &b, &psz_hid_parent, NULL)  );
            while (b)
            {
                SG_ERR_CHECK(  x_find_dag_trail(pCtx, pRepo, psz_hid_parent, psz_hid_cs_lca, &psa_trail)  );
                SG_ERR_CHECK(  _sg_changeset__get_big_treepathlist(pCtx, pRepo, psa_trail, &a_prb_lists[i++])  );
                SG_STRINGARRAY_NULLFREE(pCtx, psa_trail);

                SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_parent, NULL)  );
            }
            SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

            // init the counts
            SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_counts)  );

            // go through all the big lists and add them to the counts
            for (i=0; i<count_parents; i++)
            {
                const char* psz_pair = NULL;

                SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, a_prb_lists[i], &b, &psz_pair, NULL)  );
                while (b)
                {
                    SG_ERR_CHECK(  SG_vhash__addtoval__int64(pCtx, pvh_counts, psz_pair, 1)  );

                    SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_pair, NULL)  );
                }
                SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
            }

            // now find all the paths with counts too low
            SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_counts, &count_paths)  );
            for (i=0; i<count_paths; i++)
            {
                const char* psz_pair = NULL;
                const SG_variant * pv = NULL;
                SG_int64 val = 0;

                SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_counts, i, &psz_pair, &pv)  );
                SG_ERR_CHECK(  SG_variant__get__int64(pCtx, pv, &val)  );
                if (val < count_parents)
                {
                    char buf_gid[SG_GID_BUFFER_LENGTH];

                    memcpy(buf_gid, psz_pair, SG_GID_BUFFER_LENGTH);
                    SG_ASSERT('.' == buf_gid[SG_GID_ACTUAL_LENGTH]);
                    buf_gid[SG_GID_ACTUAL_LENGTH] = 0;

                    SG_ERR_CHECK(  SG_changeset__add_treepath(pCtx, pChangeset, buf_gid, psz_pair + SG_GID_BUFFER_LENGTH)  );
                }
            }
        }
    }

fail:
    SG_STRINGARRAY_NULLFREE(pCtx, psa_trail);
    SG_VHASH_NULLFREE(pCtx, pvh_counts);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    SG_NULLFREE(pCtx, psz_hid_cs_lca);
    SG_DAGLCA_NULLFREE(pCtx, pDagLca);
    if (a_prb_lists)
    {
        for (i=0; i<count_parents; i++)
        {
            SG_RBTREE_NULLFREE(pCtx, a_prb_lists[i]);
        }
        SG_NULLFREE(pCtx, a_prb_lists);
    }
}

static void _sg_changeset__compute_generation(SG_context * pCtx, SG_changeset * pChangeset, SG_repo * pRepo)
{
	// compute the GENERATION of this CHANGESET and stuff the value into the vhash.
	//
	// the generation is defined as  1+max(generation(parents))
	//
	// the generation is the length of the longest dag path from this
	// changeset/commit/dagnode to the null/*root* node.  the initial
	// checkin has g==1. a checkin based on it has g==2.  and so on.
	// for merges (where there are more than one parent), we take the
	// maximum g over the set of parents.
	// this gives us the length of the longest path.
	//
	// to compute this, we must fetch each parent and extract
	// its generation value.  for performance, we fetch the
	// parent dagnode instead of the parent changeset itself.
	// the generation is stored in both places, and the dagnode
	// is smaller.

	SG_int32 gen = 0, gen_k = 0;
    const char* psz_hid_parent = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_bool b = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pChangeset);
	SG_NULLARGCHECK_RETURN(pRepo);

	FAIL_IF_FROZEN(pChangeset);

	// fetch the generation for each of the parents.

	gen = 0;

	if (pChangeset->constructing.prb_parents)
    {
        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, pChangeset->constructing.prb_parents, &b, &psz_hid_parent, NULL)  );
        while (b)
        {
            SG_ERR_CHECK(  _get_generation_from_repo(pCtx, pRepo, psz_hid_parent, &gen_k)  );
            gen = SG_MAX(gen, gen_k);

            SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_hid_parent, NULL)  );
        }
        SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);
    }

	// set our generation in the vhash.

    pChangeset->constructing.generation = gen+1;

fail:
    ;
}

//////////////////////////////////////////////////////////////////

static void _sg_changeset_v1__validate(SG_context * pCtx, const SG_changeset * pChangeset)
{
	// validate the CSET_VERSION_1 fields.
	//
	// most of these fields are in the top-level vhash.
	//
	// we assume that the blob list and parents list have been
	// imported into their idsets, since we do not reference the varrays
	// in the _getters_.

	const char* pszidHid_u = NULL;
	//SG_uint32 nrBlobs;
	SG_int32 gen = 0;
	SG_uint32 iDagNum = 0;
    SG_varray* pva = NULL;

	// parent list is optional -- but make sure that if it is
	// present that it is of the right form.

	SG_changeset__get_parents(pCtx, pChangeset, &pva);
	if (SG_context__has_err(pCtx))
		SG_ERR_RESET_THROW_RETURN(SG_ERR_CHANGESET_VALIDATION_FAILED);

	// verify that the root was set to either an ID or NULL.
	// that is, we complain if they didn't call the __set_ routine.

	SG_changeset__get_root(pCtx, pChangeset, &pszidHid_u);
	if (SG_context__has_err(pCtx))
		SG_ERR_RESET_THROW_RETURN(SG_ERR_CHANGESET_VALIDATION_FAILED);

	iDagNum = 0;
	SG_changeset__get_dagnum(pCtx, pChangeset, &iDagNum);
	if (SG_context__has_err(pCtx))
		SG_ERR_RESET_THROW_RETURN(SG_ERR_CHANGESET_VALIDATION_FAILED);
	if (!iDagNum)
		SG_ERR_THROW_RETURN(SG_ERR_CHANGESET_VALIDATION_FAILED);

#if 0 /* TODO */
	SG_changeset__count_blobs_in_list(pCtx, pChangeset,&nrBlobs);
	if (SG_context__has_err(pCtx))
		SG_ERR_RESET_THROW_RETURN(SG_ERR_CHANGESET_VALIDATION_FAILED);
#endif

	// verify that we have a generation value.  we don't bother
	// checking the math -- just verify that we have one.

	SG_changeset__get_generation(pCtx, pChangeset,&gen);
	if (SG_context__has_err(pCtx))
		SG_ERR_RESET_THROW_RETURN(SG_ERR_CHANGESET_VALIDATION_FAILED);
	if (gen <= 0)
		SG_ERR_THROW_RETURN(SG_ERR_CHANGESET_VALIDATION_FAILED);

}

static void _sg_changeset__validate(SG_context * pCtx, const SG_changeset * pChangeset, SG_changeset_version * pver)
{
	// validate the given vhash and make sure that it is a well-formed representation
	// of a Changeset.  This is used after parsing a JSON script of incoming
	// information.  The JSON parser only knows JSON syntax; it doesn't know anything
	// about what fields we have or need and the data types (and value ranges) they have.
	//
	// We return the version number that we found on the Changeset and do our
	// validation to that version -- to make sure that it is a well-formed instance
	// of a Changeset of that version.
	//
	// If we get a newer version of a Changeset than this instance of the software
	// knows about, we return an error.
	//
	// we silently map all parsing/inspection errors to a generic
	// SG_ERR_CHANGESET_VALIDATION_FAILED error code.   the caller doesn't need to
	// see our dirty laundry.

	SG_NULLARGCHECK_RETURN(pChangeset);
	SG_NULLARGCHECK_RETURN(pver);

	SG_changeset__get_version(pCtx, pChangeset, pver);
	if (SG_context__has_err(pCtx))
		SG_ERR_RESET_THROW_RETURN(SG_ERR_CHANGESET_VALIDATION_FAILED);

	switch (*pver)
	{
	default:
		SG_ERR_THROW_RETURN(SG_ERR_CHANGESET_VALIDATION_FAILED);

	case CSET_VERSION_1:
		_sg_changeset_v1__validate(pCtx, pChangeset);
		if (SG_context__has_err(pCtx))
			SG_ERR_RESET_THROW_RETURN(SG_ERR_CHANGESET_VALIDATION_FAILED);
	}
}

static void _sg_changeset__freeze(SG_context * pCtx, SG_changeset * pChangeset, char** ppszidHid)
{
	// this is static because we don't trust anyone to just hand us
	// any old HID and say it matches the value that ***would be***
	// computed from the ***current*** contents of the vhash.
	//
	// we take ownership of the given HID -- you no longer own it.

	SG_ASSERT(pChangeset);
	SG_ASSERT(ppszidHid);
	SG_ASSERT(*ppszidHid);

	if (pChangeset->frozen.psz_hid)
    {
		SG_NULLFREE(pCtx, pChangeset->frozen.psz_hid);
    }

	pChangeset->frozen.psz_hid = *ppszidHid;
    *ppszidHid = NULL;
}

/**
 * Allocate a Changeset structure and populate with the contents of the
 * given JSON stream.  We assume that this was generated by our
 * SG_changeset__to_json() routine.
 */
static void my_parse_and_freeze(SG_context * pCtx, const char * szString, const char* psz_hid, SG_changeset ** ppNew)
{
	SG_changeset * pChangeset;
	SG_changeset_version ver = CSET_VERSION__INVALID;
    char* psz_copy = NULL;

	SG_NULLARGCHECK_RETURN(ppNew);
	SG_NONEMPTYCHECK_RETURN(szString);

	SG_ERR_CHECK(  SG_alloc1(pCtx, pChangeset)  );

	SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &pChangeset->frozen.pvh, szString)  );

	// make a copy of the HID so that we can stuff it into the Changeset and freeze it.
	// we freeze it because the caller should not be able to modify the Changeset by
	// accident -- since we are a representation of something on disk already.

	SG_ERR_CHECK(  SG_strdup(pCtx, psz_hid, &psz_copy)  );
	_sg_changeset__freeze(pCtx, pChangeset,&psz_copy);

	// validate the fields in the changeset and verify that it is well-formed
	// and defines a version that we understand.

	SG_ERR_CHECK(  _sg_changeset__validate(pCtx, pChangeset, &ver)  );

	*ppNew = pChangeset;
    pChangeset = NULL;

fail:
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
}

//////////////////////////////////////////////////////////////////

static void sg_changeset__estimate_json_size(SG_context * pCtx, SG_changeset * pcs, SG_uint32* piResult)
{
	SG_uint32 result = 0;
	const SG_uint16* p_cur_reftype = NULL;

	p_cur_reftype = g_reftypes;
	while (*p_cur_reftype)
	{
		SG_rbtree* prb = NULL;
		SG_uint32 count = 0;

		SG_ERR_CHECK(  sg_changeset__get_internal_blob_list(pCtx, pcs, *p_cur_reftype, SG_FALSE, &prb, NULL)  );
		if (prb)
		{
			SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb, &count)  );
			result += count * 72;
		}

		p_cur_reftype++;
	}

	*piResult = result + 256;

	return;
fail:
	return;
}

/**
 * Convert the given Changeset to a JSON stream in the given string.
 * The string should be empty (but we do not check or enforce this).
 *
 * We sort various fields within the Changeset before exporting.
 * (That is why we do not make the Changeset 'const'.)
 *
 * Our caller should freeze us ASAP.
 */
static void _sg_changeset__to_json(SG_context * pCtx, SG_changeset * pChangeset, SG_string * pStr)
{
	SG_uint32 estimate = 0;

	SG_NULLARGCHECK_RETURN(pChangeset);
	SG_NULLARGCHECK_RETURN(pStr);

    FAIL_IF_FROZEN(pChangeset);

	SG_ERR_CHECK_RETURN(  _sg_changeset__create_the_vhash(pCtx, pChangeset)  );

	//////////////////////////////////////////////////////////////////
	// This must be the last step before we convert to JSON.
	//
	// sort the vhash in increasing key order.  this is necessary
	// so that when we create the HID from the JSON stream we get a
	// consistent value (independent of the order in which the keys
	// were created).
	//
	// WARNING: SG_vhash__sort (even with bRecursive==TRUE) only sorts
	// WARNING: the vhash and the sub vhashes within it -- it DOES NOT
	// WARNING: sort the varrays contained within.  We must sort them
	// WARNING: if we want/need them sorted.  The idea is that a varray
	// WARNING: may contain ordered information.  In our case (we're
	// WARNING: storing 2 lists of IDs), we want them sorted because
	// WARNING: order doesn't matter and we want our HID to be generated
	// WARNING: consistently.  So we sorted them during __create_the_vhash

	SG_ERR_CHECK_RETURN(  SG_vhash__sort(pCtx, pChangeset->frozen.pvh, SG_TRUE, SG_vhash_sort_callback__increasing)  );

	SG_ERR_CHECK(  sg_changeset__estimate_json_size(pCtx, pChangeset, &estimate)  );
	SG_ERR_CHECK(  SG_string__make_space(pCtx, pStr, estimate)  );

	// export the vhash to a JSON stream.  the vhash becomes the top-level
	// object in the JSON stream.

	SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pChangeset->frozen.pvh, pStr)  );

    // now this object has transitioned to frozen.  free/invalidate
    // all the struct members except for the vhash and the hid

	SG_RBTREE_NULLFREE_WITH_ASSOC(pCtx, pChangeset->constructing.prb_bloblists, (SG_free_callback*) SG_rbtree__free);
	SG_RBTREE_NULLFREE(pCtx, pChangeset->constructing.prb_parents);
	SG_NULLFREE(pCtx, pChangeset->constructing.psz_hid_root);

    // fall thru

fail:
    ;
}

void SG_changeset__is_frozen(SG_context * pCtx, const SG_changeset * pChangeset, SG_bool * pbFrozen)
{
	SG_NULLARGCHECK_RETURN(pChangeset);
	SG_NULLARGCHECK_RETURN(pbFrozen);

	*pbFrozen = (pChangeset->frozen.psz_hid != NULL);
}

void SG_changeset__get_id_ref(SG_context * pCtx, const SG_changeset * pChangeset, const char** ppszidHidChangeset)
{
	// the caller wants to know our HID -- this is only valid when we are frozen.
	//
	// we give them a const pointer to our actual value.  they ***MUST NOT*** free this.

	SG_NULLARGCHECK_RETURN(pChangeset);
	SG_NULLARGCHECK_RETURN(ppszidHidChangeset);

	FAIL_IF_NOT_FROZEN(pChangeset);

	*ppszidHidChangeset = pChangeset->frozen.psz_hid;
}

void SG_changeset__save_to_repo(
	SG_context * pCtx,
	SG_changeset * pChangeset,
	SG_repo * pRepo,
	SG_repo_tx_handle* pRepoTx
	)
{
	SG_string * pString = NULL;
	char* pszHidComputed = NULL;

	SG_NULLARGCHECK_RETURN(pChangeset);
	SG_NULLARGCHECK_RETURN(pRepo);

	FAIL_IF_FROZEN(pChangeset);

	// compute the changeset's generation using the set of parent changesets.
	// this is expensive and we only do it right before we freeze/save the
	// changeset.  this computes the generation and stuffs it into the vhash.

	SG_ERR_CHECK(  _sg_changeset__compute_generation(pCtx, pChangeset, pRepo)  );

	SG_ERR_CHECK(  _sg_changeset__normalize_blob_lists(pCtx, pChangeset, pRepo)  );
    if (!SG_DAGNUM__IS_DB(pChangeset->constructing.iDagNum))
    {
        SG_ERR_CHECK(  _sg_changeset__normalize_treepaths(pCtx, pChangeset, pRepo)  );
    }

	// serialize changeset into JSON string.

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pString)  );
	SG_ERR_CHECK(  _sg_changeset__to_json(pCtx, pChangeset, pString)  );

#if TRACE_CHANGESET
	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR,"SG_changeset__save_to_repo:\n%s\n",SG_string__sz(pString))  );
#endif

    // create a blob in the repository using the JSON string.  this computes
    // the HID and returns it.

	SG_repo__store_blob_from_memory(
		pCtx,
		pRepo, pRepoTx, NULL,
        SG_FALSE,
		(const SG_byte *)SG_string__sz(pString),
		SG_string__length_in_bytes(pString),
		&pszHidComputed);

	// TODO: this check should be done just once.  sqlite repo already does it.  do others?
	if (!SG_context__has_err(pCtx) || SG_context__err_equals(pCtx, SG_ERR_BLOBFILEALREADYEXISTS))
	{
		SG_context__err_reset(pCtx);

		// freeze the changeset memory-object and effectively make it read-only
		// from this point forward.  we give up our ownership of the HID.

		SG_ASSERT(  pszHidComputed  );
		_sg_changeset__freeze(pCtx, pChangeset, &pszHidComputed);

		SG_STRING_NULLFREE(pCtx, pString);
		return;  // return _OK or _BLOBFILEALREADYEXISTS
	}

fail:
	SG_NULLFREE(pCtx, pszHidComputed);
	SG_STRING_NULLFREE(pCtx, pString);
}

void SG_changeset__load_from_repo(
	SG_context * pCtx,
	SG_repo * pRepo,
	const char* pszidHidBlob,
	SG_changeset ** ppChangesetReturned
	)
{
	// fetch contents of a Changeset-type blob and convert to a Changeset object.

	SG_byte * pbuf = NULL;
	SG_changeset * pChangeset = NULL;
	char* pszidHidCopy = NULL;

	SG_NULLARGCHECK_RETURN(pRepo);
	SG_NULLARGCHECK_RETURN(pszidHidBlob);
	SG_NULLARGCHECK_RETURN(ppChangesetReturned);

	*ppChangesetReturned = NULL;

	// fetch the Blob for the given HID and convert from a JSON stream into an
	// allocated Changeset object.

	SG_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pRepo, pszidHidBlob, &pbuf, NULL)  );
	SG_ERR_CHECK(  my_parse_and_freeze(pCtx, (const char *)pbuf, pszidHidBlob, &pChangeset)  );
	SG_NULLFREE(pCtx, pbuf);

	// make a copy of the HID so that we can stuff it into the Changeset and freeze it.
	// we freeze it because the caller should not be able to modify the Changeset by
	// accident -- since we are a representation of something on disk already.

	SG_ERR_CHECK(  SG_strdup(pCtx, pszidHidBlob, &pszidHidCopy)  );
	_sg_changeset__freeze(pCtx, pChangeset,&pszidHidCopy);

	*ppChangesetReturned = pChangeset;
    pChangeset = NULL;

fail:
	SG_NULLFREE(pCtx, pbuf);
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	SG_NULLFREE(pCtx, pszidHidCopy);
}

void SG_changeset__load_from_staging(SG_context* pCtx,
									 const char* pszidHidBlob,
									 SG_staging_blob_handle* pStagingBlobHandle,
									 SG_changeset** ppChangesetReturned)
{
	// fetch contents of a Changeset-type blob and convert to a Changeset object.

	SG_byte * pbuf = NULL;
	SG_changeset * pChangeset = NULL;
	char* pszidHidCopy = NULL;

	SG_NULLARGCHECK_RETURN(ppChangesetReturned);

	*ppChangesetReturned = NULL;

	// fetch the Blob for the given HID and convert from a JSON stream into an
	// allocated Changeset object.

	SG_ERR_CHECK(  SG_staging__fetch_blob_into_memory(pCtx, pStagingBlobHandle, &pbuf, NULL)  );
	SG_ERR_CHECK(  my_parse_and_freeze(pCtx, (const char *)pbuf, pszidHidBlob, &pChangeset)  );
	SG_NULLFREE(pCtx, pbuf);

	// make a copy of the HID so that we can stuff it into the Changeset and freeze it.
	// we freeze it because the caller should not be able to modify the Changeset by
	// accident -- since we are a representation of something on disk already.

	SG_ERR_CHECK(  SG_strdup(pCtx, pszidHidBlob, &pszidHidCopy)  );
	_sg_changeset__freeze(pCtx, pChangeset,&pszidHidCopy);

	*ppChangesetReturned = pChangeset;
    pChangeset = NULL;

fail:
	SG_NULLFREE(pCtx, pbuf);
	SG_CHANGESET_NULLFREE(pCtx, pChangeset);
	SG_NULLFREE(pCtx, pszidHidCopy);
}

//////////////////////////////////////////////////////////////////

void SG_changeset__create_dagnode(
	SG_context * pCtx,
	const SG_changeset * pChangeset,
	SG_dagnode ** ppNewDagnode
	)
{
	SG_dagnode * pdn = NULL;
	SG_int32 gen = 0;
    SG_varray* pva = NULL;
    SG_uint32 i = 0;
    SG_uint32 count = 0;

	SG_NULLARGCHECK_RETURN(pChangeset);
	SG_NULLARGCHECK_RETURN(ppNewDagnode);

	FAIL_IF_NOT_FROZEN(pChangeset);

	SG_ERR_CHECK( SG_changeset__get_generation(pCtx, pChangeset, &gen) );

	// allocate a new DAGNODE using the CHANGESET's HID (as the child).
	// set the GENERATION on the dagnode to match the CHANGESET.

	SG_ERR_CHECK( SG_dagnode__alloc(pCtx, &pdn, pChangeset->frozen.psz_hid, gen)  );

	// add each of the CHANGESET's parents to the set of parents in the DAGNODE.

	SG_ERR_CHECK(  SG_changeset__get_parents(pCtx, pChangeset, &pva)  );
    if (pva)
    {
        SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );

        for (i=0; i<count; i++)
        {
            const char* psz_hid = NULL;

            SG_ERR_CHECK(  SG_varray__get__sz(pCtx, pva, i, &psz_hid)  );
            SG_ERR_CHECK( SG_dagnode__add_parent(pCtx, pdn, psz_hid) );
        }
    }

	// now freeze the dagnode so that no other parents can be added.

	SG_ERR_CHECK( SG_dagnode__freeze(pCtx, pdn) );

	*ppNewDagnode = pdn;
    pdn = NULL;

fail:
	SG_DAGNODE_NULLFREE(pCtx, pdn);
}

/* This array contains all the reftypes which can actually be used as
 * references from a changeset.
 *
 * Note that __NONE is the last entry.  Loops rely on this and on the
 * fact that __NONE is 0. */
const SG_uint16 g_reftypes[] = {
	SG_BLOB_REFTYPE__TREENODE      ,
	SG_BLOB_REFTYPE__TREEUSERFILE  ,
	SG_BLOB_REFTYPE__TREEATTRIBS   ,
	SG_BLOB_REFTYPE__TREESYMLINK   ,
	SG_BLOB_REFTYPE__DBINFO        ,
	SG_BLOB_REFTYPE__DBRECORD      ,
	SG_BLOB_REFTYPE__DBUSERFILE    ,
	SG_BLOB_REFTYPE__DBTEMPLATE    ,
	SG_BLOB_REFTYPE__NONE          ,
};

void SG_changeset__get_version(SG_context * pCtx, const SG_changeset * pChangeset, SG_changeset_version * pver)
{
	SG_int64 v = 0;

	SG_NULLARGCHECK_RETURN(pChangeset);
	FAIL_IF_NOT_FROZEN(pChangeset);
	SG_NULLARGCHECK_RETURN(pver);

	SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, pChangeset->frozen.pvh,KEY_VERSION,&v)  );

    *pver = v;
}

void SG_changeset__get_dagnum(SG_context * pCtx, const SG_changeset * pChangeset, SG_uint32 * piDagNum)
{
	SG_int64 v = 0;

	SG_NULLARGCHECK_RETURN(pChangeset);
	FAIL_IF_NOT_FROZEN(pChangeset);
	SG_NULLARGCHECK_RETURN(piDagNum);

	SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, pChangeset->frozen.pvh, KEY_DAGNUM, &v)  );

	*piDagNum  = (SG_uint32) v;
}

void SG_changeset__get_root(SG_context * pCtx, const SG_changeset * pChangeset, const char** ppszidHid)
{
	SG_NULLARGCHECK_RETURN(pChangeset);
	FAIL_IF_NOT_FROZEN(pChangeset);
	SG_NULLARGCHECK_RETURN(ppszidHid);

	// we expect 3 results:
	// [] OK, HID != NULL
	// [] OK, HID == NULL
	// [] ERR_VHASH_KEYNOTFOUND

	SG_ERR_CHECK_RETURN(  SG_vhash__get__sz(pCtx, pChangeset->frozen.pvh,KEY_ROOT,ppszidHid)  );
}

void SG_changeset__get_generation(SG_context * pCtx, const SG_changeset * pChangeset, SG_int32 * pGen)
{
	SG_int64 gen64;

	SG_NULLARGCHECK_RETURN(pChangeset);
    FAIL_IF_NOT_FROZEN(pChangeset);
	SG_NULLARGCHECK_RETURN(pGen);

	SG_ERR_CHECK_RETURN(  SG_vhash__get__int64(pCtx, pChangeset->frozen.pvh,KEY_GENERATION,&gen64)  );

	*pGen = (SG_int32)gen64;
}

void SG_changeset__get_parents(
	SG_context * pCtx,
	const SG_changeset * pChangeset,
    SG_varray** ppva
	)
{
    SG_bool b = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pChangeset);
	FAIL_IF_NOT_FROZEN(pChangeset);
	SG_NULLARGCHECK_RETURN(ppva);

    SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, pChangeset->frozen.pvh, KEY_PARENTS, &b)  );
    if (b)
    {
        SG_ERR_CHECK_RETURN(  SG_vhash__get__varray(pCtx, pChangeset->frozen.pvh, KEY_PARENTS, ppva)  );
    }
    else
    {
        *ppva = NULL;
    }
}

void SG_changeset__get_list_of_bloblists(
	SG_context * pCtx,
	const SG_changeset * pChangeset,
    SG_vhash** ppvh
	)
{
	SG_NULLARGCHECK_RETURN(pChangeset);
	FAIL_IF_NOT_FROZEN(pChangeset);
	SG_NULLARGCHECK_RETURN(ppvh);

    SG_ERR_CHECK_RETURN(  SG_vhash__get__vhash(pCtx, pChangeset->frozen.pvh, KEY_BLOBS, ppvh)  );
}

void SG_changeset__get_treepaths(
	SG_context * pCtx,
	const SG_changeset * pChangeset,
    SG_vhash** ppvh
	)
{
    SG_bool b = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pChangeset);
	FAIL_IF_NOT_FROZEN(pChangeset);
	SG_NULLARGCHECK_RETURN(ppvh);
	FAIL_IF_DB_DAG(pChangeset);

    SG_ERR_CHECK_RETURN(  SG_vhash__has(pCtx, pChangeset->frozen.pvh, KEY_TREEPATHS, &b)  );
    if (b)
    {
        SG_ERR_CHECK_RETURN(  SG_vhash__get__vhash(pCtx, pChangeset->frozen.pvh, KEY_TREEPATHS, ppvh)  );
    }
    else
    {
        *ppvh = NULL;
    }
}



