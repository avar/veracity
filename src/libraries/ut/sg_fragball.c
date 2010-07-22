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

/* TODO put a magic number at the top of a fragball file? */

/* TODO #defines for the vhash keys */

struct _sg_fragball_writer
{
    SG_file* pFile;
    SG_repo* pRepo;
    SG_byte* buf;
    SG_uint32 buf_size;
};

/*
 * A fragball is a byte stream (usually a file) which contains
 * a collection of dagfrags and blobs.  The structure of a
 * fragball is a series of objects.  Each object is:
 *
 * 4 bytes containing the length of the header
 *
 * A header, which is an uncompressed JSON object.
 * The length includes a trailing zero.
 *
 * Optional:  Payload, as specified in the object header.
 *
 *
 * The contents of the object header vary depending on
 * what kind of object it is.
 *
 * The first object in every fragball specifies what
 * version of the fragball format is being used.  For
 * now, this is version 1.
 *
 * Other kinds of objects:
 *
 * frag (header contains a dagnum and a frag)
 *
 * blob (header contains encoding and lengths.  payload
 * contains the encoded blob)
 *
 */

void SG_fragball__read_object_header(SG_context * pCtx, SG_file* pFile, SG_vhash** ppvh)
{
    SG_byte ba_len[4];
    SG_uint32 len = 0;
    SG_byte* p = NULL;
    SG_vhash* pvh = NULL;

    SG_file__read(pCtx,pFile, 4, ba_len, NULL);

	if (SG_context__err_equals(pCtx,SG_ERR_EOF))
	{
		SG_context__err_reset(pCtx);
		*ppvh = NULL;
		return;
	}

	SG_ERR_CHECK_CURRENT;

    len = (ba_len[0] << 24)
        | (ba_len[1] << 16)
        | (ba_len[2] <<  8)
        | (ba_len[3] <<  0);

    SG_ERR_CHECK(  SG_alloc(pCtx, len, 1, &p)  );
    SG_ERR_CHECK(  SG_file__read(pCtx, pFile, len, p, NULL)  );
	SG_ERR_CHECK(  SG_VHASH__ALLOC__FROM_JSON(pCtx, &pvh, (char*)p));
    SG_NULLFREE(pCtx, p);

    *ppvh = pvh;

    return;

fail:
    SG_NULLFREE(pCtx, p);
}

void sg_fragball__write_object_header(SG_context * pCtx, SG_file* pFile, SG_vhash* pvh)
{
    SG_string* pstr = NULL;
    SG_uint32 len = 0;
    SG_byte ba_len[4];

    SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx,&pstr)  );
    SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh, pstr)  );
    len = SG_string__length_in_bytes(pstr);
    len++;
    ba_len[0] = (SG_byte) ( (len >> 24) & 0xff );
    ba_len[1] = (SG_byte) ( (len >> 16) & 0xff );
    ba_len[2] = (SG_byte) ( (len >>  8) & 0xff );
    ba_len[3] = (SG_byte) ( (len >>  0) & 0xff );
    SG_ERR_CHECK(  SG_file__write(pCtx, pFile, 4, ba_len, NULL)  );
    SG_ERR_CHECK(  SG_file__write(pCtx, pFile, len, (SG_byte*) SG_string__sz(pstr), NULL)  );
    SG_STRING_NULLFREE(pCtx, pstr);

    return;

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
}

void sg_fragball__write_version_header(SG_context * pCtx, SG_file* pFile)
{
    SG_vhash* pvh = NULL;

    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "op", "version")  );
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "version", "1")  ); /* TODO verify this number? */

    SG_ERR_CHECK(  sg_fragball__write_object_header(pCtx, pFile, pvh)  );
    SG_VHASH_NULLFREE(pCtx, pvh);

    return;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void sg_fragball__write_frag(SG_context * pCtx, SG_file* pFile, SG_dagfrag* pFrag)
{
    SG_vhash* pvh = NULL;
    SG_vhash* pvh_frag = NULL;

    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
    SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "op", "frag")  );

    SG_ERR_CHECK(  SG_dagfrag__to_vhash__shared(pCtx, pFrag, pvh, &pvh_frag)  );
    SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvh, "frag", &pvh_frag)  );

    SG_ERR_CHECK(  sg_fragball__write_object_header(pCtx, pFile, pvh)  );
    SG_VHASH_NULLFREE(pCtx, pvh);

    return;

fail:
    SG_VHASH_NULLFREE(pCtx, pvh_frag);
    SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_fragball__append_blob__from_handle(
	SG_context* pCtx,
	SG_fragball_writer* pWriter,
	SG_repo_fetch_blob_handle** ppBlob,
	const char* psz_objectid,
	const char* pszHid,
	SG_blob_encoding encoding,
	const char* pszHidVcdiffRef,
	SG_uint64 lenEncoded,
	SG_uint64 lenFull)
{
	SG_vhash* pvh = NULL;
	SG_uint64 left = 0;
	SG_repo_fetch_blob_handle* pBlob;
    SG_bool b_done = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pWriter);
	SG_NULL_PP_CHECK_RETURN(ppBlob);

	pBlob = *ppBlob;

	/* write the header */
	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "op", "blob")  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "hid", pszHid)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, "encoding", (SG_int64) encoding)  ); /* TODO enum string */
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "vcdiff_ref", pszHidVcdiffRef)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh, "objectid", psz_objectid)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, "len_encoded", (SG_int64) lenEncoded)  );
	SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvh, "len_full", (SG_int64) lenFull)  );

	SG_ERR_CHECK(  sg_fragball__write_object_header(pCtx, pWriter->pFile, pvh)  );
	SG_VHASH_NULLFREE(pCtx, pvh);

	left = lenEncoded;
	while (!b_done)
	{
		SG_uint32 want = pWriter->buf_size;
		SG_uint32 got = 0;

		if (want > left)
		{
			want = (SG_uint32) left;
		}
		SG_ERR_CHECK(  SG_repo__fetch_blob__chunk(pCtx, pWriter->pRepo, pBlob, want, pWriter->buf, &got, &b_done)  );
		SG_ERR_CHECK(  SG_file__write(pCtx, pWriter->pFile, got, pWriter->buf, NULL)  );

		left -= got;
	}
	SG_ERR_CHECK(  SG_repo__fetch_blob__end(pCtx, pWriter->pRepo, ppBlob)  );

    SG_ASSERT(0 == left);

	return;

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void sg_fragball__write_blob(SG_context * pCtx, SG_fragball_writer* pfb, const char* psz_hid)
{
	SG_uint64 len_full;
	SG_repo_fetch_blob_handle* pbh = NULL;
	SG_blob_encoding encoding;
	char* psz_hid_vcdiff_reference = NULL;
	SG_uint64 len_encoded;
    char* psz_objectid = NULL;

	SG_ERR_CHECK(  SG_repo__fetch_blob__begin(pCtx, pfb->pRepo, psz_hid, SG_FALSE, &psz_objectid, &encoding, &psz_hid_vcdiff_reference, &len_encoded, &len_full, &pbh)  );
	SG_ERR_CHECK(  SG_fragball__append_blob__from_handle(pCtx, pfb, &pbh, psz_objectid, psz_hid, encoding, psz_hid_vcdiff_reference, len_encoded, len_full)  );


fail:
    SG_NULLFREE(pCtx, psz_hid_vcdiff_reference);
    SG_NULLFREE(pCtx, psz_objectid);
}

void sg_fragball__write_from_bloblist(SG_context * pCtx, SG_fragball_writer* pfb, SG_vhash* pvh)
{
    SG_uint32 count = 0;
	SG_uint32 i = 0;

	SG_ERR_CHECK(  SG_log_debug(pCtx, "Writing blob data to fragball")  );

    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh, &count)  );
	for (i = 0; i < count; i++)
	{
        const char* psz_hid = NULL;

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh, i, &psz_hid, NULL)  );
		SG_ERR_CHECK(  sg_fragball__write_blob(pCtx, pfb, psz_hid)  );
	}

	/* fall through */
fail:
	SG_ERR_CHECK(  SG_log_debug(pCtx, "Done writing blob data to fragball")  );
}

static void sg_fragball__write_from_list_of_bloblists(
        SG_context * pCtx,
        SG_fragball_writer* pfb,
        const SG_vhash* pvh_lbl
        )
{
    SG_uint32 count_lists = 0;
    SG_uint32 i = 0;

    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_lbl, &count_lists)  );
    for (i=0; i<count_lists; i++)
    {
        const char* psz_key = NULL;
        const SG_variant* pv = NULL;
        SG_vhash* pvh_bl = NULL;

        SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_lbl, i, &psz_key, &pv)  );
        SG_ERR_CHECK(  SG_variant__get__vhash(pCtx, pv, &pvh_bl)  );
        SG_ERR_CHECK(  sg_fragball__write_from_bloblist(pCtx, pfb, pvh_bl)  );
    }

fail:
    ;
}

static SG_dagfrag__foreach_member_callback my_cb_fragball_changesets;

static void my_cb_fragball_changesets(SG_context * pCtx,
									  const SG_dagnode * pdn,
									  SG_UNUSED_PARAM(SG_dagfrag_state qs),
									  void * pVoidCallerData)
{
    SG_fragball_writer* pfb = (SG_fragball_writer*) pVoidCallerData;
    SG_changeset* pcs = NULL;
    const char* psz_hid_cs = NULL;
    SG_vhash* pvh_lbl = NULL;

    SG_UNUSED(qs);

    SG_ERR_CHECK(  SG_dagnode__get_id_ref(pCtx, pdn, &psz_hid_cs)  );

    /* write the changeset blob */
    SG_ERR_CHECK(  sg_fragball__write_blob(pCtx, pfb, psz_hid_cs)  );

    /* and all the other blobs */
    SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pfb->pRepo, psz_hid_cs, &pcs)  );

    SG_ERR_CHECK(  SG_changeset__get_list_of_bloblists(pCtx, pcs, &pvh_lbl)  );
    SG_ERR_CHECK(  sg_fragball__write_from_list_of_bloblists(pCtx, pfb, pvh_lbl)  );

fail:
    SG_CHANGESET_NULLFREE(pCtx, pcs);
}

static void sg_fragball__write_whole_frag(SG_context * pCtx, SG_fragball_writer* pfb, SG_dagfrag* pFrag)
{
    SG_ERR_CHECK_RETURN(  sg_fragball__write_frag(pCtx, pfb->pFile, pFrag)  );
    SG_ERR_CHECK_RETURN(  SG_dagfrag__foreach_member(pCtx, pFrag, my_cb_fragball_changesets, pfb)  );
}

void SG_fragball_writer__free(SG_context * pCtx, SG_fragball_writer* pfb)
{
	SG_file* pFile = NULL;

	if (!pfb)
	{
		return;
	}

	pFile = pfb->pFile;

	SG_NULLFREE(pCtx, pfb->buf);
	SG_NULLFREE(pCtx, pfb);

	SG_ERR_CHECK_RETURN(  SG_file__close(pCtx, &pFile)  );
}

void SG_fragball_writer__alloc(SG_context * pCtx, SG_repo* pRepo, const SG_pathname* pPathFragball, SG_bool bCreateNewFile, SG_fragball_writer** ppResult)
{
    SG_fragball_writer* pfb = NULL;
	SG_bool bCreatedFile = SG_FALSE;

	SG_NULLARGCHECK_RETURN(pPathFragball);
	SG_NULLARGCHECK_RETURN(ppResult);

    SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(SG_fragball_writer), &pfb)  );

    pfb->pRepo = pRepo;
    pfb->buf_size = SG_STREAMING_BUFFER_SIZE;
    SG_ERR_CHECK(  SG_alloc(pCtx, pfb->buf_size, 1, &pfb->buf)  );

	if (bCreateNewFile)
	{
		SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathFragball, SG_FILE_WRONLY | SG_FILE_CREATE_NEW, SG_FSOBJ_PERMS__UNUSED, &pfb->pFile)  );
		bCreatedFile = SG_TRUE;
	}
	else
	{
		SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathFragball, SG_FILE_WRONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pfb->pFile)  );
		SG_ERR_CHECK(  SG_file__seek_end(pCtx, pfb->pFile, NULL)  );
	}

    *ppResult = pfb;

    return;

fail:
	if (pfb)
	{
		SG_FILE_NULLCLOSE(pCtx, pfb->pFile);
		SG_NULLFREE(pCtx, pfb->buf);
		SG_NULLFREE(pCtx, pfb);
	}
	if (bCreatedFile)
		SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPathFragball)  );
}

void SG_fragball__write(SG_context * pCtx, SG_repo* pRepo, const SG_pathname* pPath, SG_rbtree* prb_frags)
{
    SG_fragball_writer* pfb = NULL;
    SG_rbtree_iterator* pit = NULL;
    SG_bool b;
    const char* psz_dagnum = NULL;
    SG_dagfrag* pFrag = NULL;

    SG_ERR_CHECK(  SG_fragball_writer__alloc(pCtx, pRepo, pPath, SG_TRUE, &pfb)  );

    SG_ERR_CHECK(  sg_fragball__write_version_header(pCtx, pfb->pFile)  );

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pit, prb_frags, &b, &psz_dagnum, (void**) &pFrag)  );
    while (b)
    {
        SG_ERR_CHECK(  sg_fragball__write_whole_frag(pCtx, pfb, pFrag)  );

        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pit, &b, &psz_dagnum, (void**) &pFrag)  );
    }
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

    SG_ERR_CHECK(  SG_file__close(pCtx,&pfb->pFile)  );

    SG_fragball_writer__free(pCtx, pfb);

    return;

fail:
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pit);

	if (pfb)
	{
		SG_FILE_NULLCLOSE(pCtx, pfb->pFile);
		SG_ERR_IGNORE(  SG_fragball_writer__free(pCtx, pfb)  );
	}
}

void SG_fragball__create(SG_context * pCtx, const SG_pathname* pPath_dir, SG_pathname** ppPath_fragball)
{
    SG_file* pFile = NULL;
	char buf_filename[SG_TID_MAX_BUFFER_LENGTH];
    SG_pathname* pPath = NULL;

	SG_ERR_CHECK(  SG_tid__generate(pCtx, buf_filename, sizeof(buf_filename))  );

    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pPath_dir, buf_filename)  );

    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_WRONLY | SG_FILE_CREATE_NEW, SG_FSOBJ_PERMS__UNUSED, &pFile)  );
    SG_ERR_CHECK(  sg_fragball__write_version_header(pCtx, pFile)  );
    SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

    *ppPath_fragball = pPath;
    pPath = NULL;

    return;

fail:
	// REVIEW: Jeff says:  The original version of this had nothing but "return err" after the label.
	//                     I added the following after a quick glance at the code.  Is this sufficient?
	//                     Should we delete the file that we created, too?

	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

void SG_fragball__append__frag(SG_context * pCtx, SG_pathname* pPath, SG_dagfrag* pFrag)
{
    SG_file* pFile = NULL;

    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_WRONLY | SG_FILE_OPEN_EXISTING, SG_FSOBJ_PERMS__UNUSED, &pFile)  );
    SG_ERR_CHECK(  SG_file__seek_end(pCtx, pFile, NULL)  );
    SG_ERR_CHECK(  sg_fragball__write_frag(pCtx, pFile, pFrag)  );
    SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );

    return;

fail:
	// REVIEW: Jeff says:  The original version of this had nothing but "return err" after the label.
	//                     I added the following after a quick glance at the code.  Is this sufficient?
	//                     Do we care that we may have only written a partial frag to the file?

	SG_FILE_NULLCLOSE(pCtx, pFile);
}

void SG_fragball__append__blob(SG_context * pCtx, SG_pathname* pPath, SG_repo* pRepo, const char* psz_hid)
{
    SG_fragball_writer* pfb = NULL;

    SG_ERR_CHECK(  SG_fragball_writer__alloc(pCtx, pRepo, pPath, SG_FALSE, &pfb)  );

    SG_ERR_CHECK(  sg_fragball__write_blob(pCtx, pfb, psz_hid)  );

    SG_ERR_CHECK(  SG_file__close(pCtx, &pfb->pFile)  );

    SG_fragball_writer__free(pCtx, pfb);

    return;

fail:
	// REVIEW: Jeff says:  The original version of this had nothing but "return err" after the label.
	//                     I added the following after a quick glance at the code.  Is it sufficient?
	//                     Do we care that we may have only written a partial blob to the file?

	if (pfb)
	{
		SG_FILE_NULLCLOSE(pCtx, pfb->pFile);
		SG_ERR_IGNORE(  SG_fragball_writer__free(pCtx, pfb)  );
	}
}

void SG_fragball__append__blobs(SG_context * pCtx, SG_pathname* pPath, SG_repo* pRepo, const char* const* pasz_blob_hids, SG_uint32 countHids)
{
	SG_fragball_writer* pfb = NULL;
    SG_uint32 i = 0;

	SG_ERR_CHECK(  SG_fragball_writer__alloc(pCtx, pRepo, pPath, SG_FALSE, &pfb)  );

    for (i=0; i<countHids; i++)
    {
        SG_ERR_CHECK(  sg_fragball__write_blob(pCtx, pfb, pasz_blob_hids[i])  );
    }

	SG_ERR_CHECK(  SG_file__close(pCtx, &pfb->pFile)  );

	SG_fragball_writer__free(pCtx, pfb);

	return;

fail:
	// REVIEW: Jeff says:  The original version of this had nothing but "return err" after the label.
	//                     I added the following after a quick glance at the code.  Is it sufficient?
	//                     Do we care that we may have only written a partial blob to the file?

	if (pfb)
	{
		SG_FILE_NULLCLOSE(pCtx, pfb->pFile);
		SG_ERR_IGNORE(  SG_fragball_writer__free(pCtx, pfb)  );
	}
}

void SG_fragball__append__dagnode(SG_context* pCtx, SG_pathname* pPath, SG_repo* pRepo, SG_uint32 iDagNum, const char* pszHid)
{
	{
		SG_dagfrag* pFrag = NULL;
		char* psz_repo_id = NULL;
		char* psz_admin_id = NULL;
		SG_dagnode* pdn = NULL;

		SG_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pRepo, &psz_repo_id)  );
		SG_ERR_CHECK(  SG_repo__get_admin_id(pCtx, pRepo, &psz_admin_id)  );

		// NOTE: We're trusting that the caller has verified that the dagnode hid exists in iDagNum's dag.  Should we?
		//       It's easy to introduce a subtle bug by calling this with a valid dagnode hid but the wrong dagnum.

		SG_ERR_CHECK(  SG_dagfrag__alloc(pCtx, &pFrag, psz_repo_id, psz_admin_id, iDagNum)  );
		SG_NULLFREE(pCtx, psz_repo_id);
		SG_NULLFREE(pCtx, psz_admin_id);

		SG_ERR_CHECK(  SG_repo__fetch_dagnode(pCtx, pRepo, pszHid, &pdn)  );
		SG_ERR_CHECK(  SG_dagfrag__add_dagnode(pCtx, pFrag, &pdn)  );

		SG_ERR_CHECK(  SG_fragball__append__frag(pCtx, pPath, pFrag)  );

		SG_DAGFRAG_NULLFREE(pCtx, pFrag);

		return;

fail:
		SG_NULLFREE(pCtx, psz_repo_id);
		SG_NULLFREE(pCtx, psz_admin_id);
		SG_DAGFRAG_NULLFREE(pCtx, pFrag);
		SG_DAGNODE_NULLFREE(pCtx, pdn);
	}
}

void SG_fragball__append__dagnodes(SG_context * pCtx, SG_pathname* pPath, SG_repo* pRepo, SG_uint32 iDagNum, SG_rbtree* prb_ids)
{
    SG_dagfrag* pFrag = NULL;
    char* psz_repo_id = NULL;
    char* psz_admin_id = NULL;

    SG_ERR_CHECK(  SG_repo__get_repo_id(pCtx, pRepo, &psz_repo_id)  );
    SG_ERR_CHECK(  SG_repo__get_admin_id(pCtx, pRepo, &psz_admin_id)  );

	// NOTE: We're trusting that the caller has verified that the dagnode hids exist in iDagNum's dag.  Should we?
	//       It's easy to introduce a subtle bug by calling this with a valid dagnode hid but the wrong dagnum.

    SG_ERR_CHECK(  SG_dagfrag__alloc(pCtx, &pFrag, psz_repo_id, psz_admin_id, iDagNum)  );
    SG_NULLFREE(pCtx, psz_repo_id);
    SG_NULLFREE(pCtx, psz_admin_id);

    SG_ERR_CHECK(  SG_dagfrag__load_from_repo__simple(pCtx, pFrag, pRepo, prb_ids)  );

    SG_ERR_CHECK(  SG_fragball__append__frag(pCtx, pPath, pFrag)  );

    SG_DAGFRAG_NULLFREE(pCtx, pFrag);

	return;

fail:
    SG_NULLFREE(pCtx, psz_repo_id);
    SG_NULLFREE(pCtx, psz_admin_id);
    SG_DAGFRAG_NULLFREE(pCtx, pFrag);
}

void SG_fragball__free(SG_context* pCtx, SG_pathname* pPath_fragball)
{
	if (pPath_fragball)
	{
		SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPath_fragball)  );
		SG_PATHNAME_NULLFREE(pCtx, pPath_fragball);
	}
}
