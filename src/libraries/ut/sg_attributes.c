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

#define SG_ATTRIBUTE_BIT__X  (1 <<  0)
/* TODO we need other attr bits */

/* TODO we probably want to build in some special handling for
 * ENOTSUP.  On my home Linux system, xattrs are not enabled.
 *
 * TODO Jeff says 8/18/2009:
 * TODO On Linux (with SE Linux extensions (read Fedora et al)), they partition
 * TODO XAttrs into "namespaces" (see attr(5)).  All user-defined XAttrs must have
 * TODO a "user." prefix (to distinguish them from "security.", "system.", and
 * TODO "trusted." attributes.  If a plain user tries to set an XATTR without the
 * TODO using the "user." prefix, they get a ENOTSUP (rather than an EPERM).
 * TODO
 * TODO For plain-files and directories, they use the mode bits on the entry to
 * TODO decide if the user to can set a "user." XAttr.
 * TODO
 * TODO Because mode bits for symlinks are bogus, they don't allow users to set
 * TODO any XAttrs on symlinks.  This gives EPERM.
 * TODO
 * TODO All of this assumes, of course, that the object resides on a filesystem
 * TODO that supports XATTRS.  (Fortunately, ext2/3/4 do.)
 *
 * TODO Jeff says 8/18/2009:
 * TODO SE Linux is also free to write whatever XAttrs they want in their domains.
 * TODO
 * TODO The "getfattr(1)" command defaults to only showing "user." domain XAttrs;
 * TODO use "-m -" to see all of the somewhat hidden ones.
 * TODO
 * TODO However, the listxattr(2) reports them all (or possibly all of the ones
 * TODO that the user is permitted to see), so the count that we compute in
 * TODO SG_fsobj__xattr__get_all() may be different than expected.
 * TODO
 */

#ifdef SG_BUILD_FLAG_FEATURE_XATTR

void SG_attributes__get_recognized_list(
	SG_context* pCtx,
    SG_rbtree** ppResult
    )
{
    SG_rbtree* prb = NULL;

    SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );

    /* TODO this should retrieve a list from the repo
     * configuration settings.  For now, I'm just hard-coding
     * something. */

#ifdef MAC
    SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, "com.apple.ResourceFork")  );
    SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, "com.apple.FinderInfo")  );
#endif

#if 1 && defined(DEBUG)
    SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, "com.sourcegear.test1")  );
    SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, "com.sourcegear.test2")  );
    SG_ERR_CHECK(  SG_rbtree__add(pCtx, prb, "com.sourcegear.test3")  );
#endif

    *ppResult = prb;

    return;

fail:
    SG_RBTREE_NULLFREE(pCtx, prb);
}


void SG_attributes__xattrs__read(
	SG_context* pCtx,
	SG_repo * pRepo,
	const SG_pathname* pPath,
   	SG_vhash** ppvhAttrs,
	SG_rbtree** pprbBlobs
	)
{
	SG_rbtree* prb = NULL;
	SG_rbtree* prb_xattrs = NULL;
	SG_vhash* pvh_xattrs = NULL;
	SG_rbtree_iterator* pIterator = NULL;
	const char* pszKey = NULL;
	const SG_byte* pPacked = NULL;
	SG_bool b;
	SG_uint32 len;
	SG_uint32 count;
	const SG_byte* pData = NULL;
    SG_rbtree* prbRecognized = NULL;

	SG_NULLARGCHECK_RETURN(pPath);
	SG_NULLARGCHECK_RETURN(ppvhAttrs);
	SG_NULLARGCHECK_RETURN(pprbBlobs);

    SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvh_xattrs)  );
	SG_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prb)  );

    SG_ERR_CHECK(  SG_attributes__get_recognized_list(pCtx, &prbRecognized)  );
	SG_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx, pPath, prbRecognized, &prb_xattrs)  );
	SG_RBTREE_NULLFREE(pCtx, prbRecognized);
	SG_ERR_CHECK(  SG_rbtree__count(pCtx, prb_xattrs, &count)  );

	if (count)
	{
		/* In the xattrs vhash, we put one entry for each attribute,
		 * containing the HID of the value of that item, which will be
		 * stored as a separate blob. */

		SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, prb_xattrs, &b, &pszKey, (void**) &pPacked)  );
		while (b)
		{
			const char* pszHid = NULL;

			SG_ERR_CHECK(  SG_memoryblob__unpack(pCtx, pPacked, &pData, &len)  );

			SG_ERR_CHECK(  SG_rbtree__memoryblob__add__hid(pCtx, prb, pRepo, pData, len, &pszHid)  );

			/* TODO should this be vhash__update? */
			SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvh_xattrs, pszKey, pszHid)  );

			SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszKey, (void**) &pPacked)  );
		}
		SG_rbtree__iterator__free(pCtx, pIterator);
		pIterator = NULL;
	}

	SG_RBTREE_NULLFREE(pCtx, prb_xattrs);

	/* TODO go through the existing xattrs and see if anything
	 * needs to be removed */

    *ppvhAttrs = pvh_xattrs;
	*pprbBlobs = prb;

	return;

fail:
	SG_RBTREE_NULLFREE(pCtx, prbRecognized);
	SG_RBTREE_NULLFREE(pCtx, prb_xattrs);
}

void SG_attributes__xattrs__update_with_baseline(
        SG_context* pCtx,
		const SG_vhash* pvhBaseline,
        SG_vhash* pvh
        )
{
    SG_uint32 count_baseline;

    SG_NULLARGCHECK_RETURN(pvhBaseline);
	SG_NULLARGCHECK_RETURN(pvh);

    SG_ERR_CHECK(  SG_vhash__count(pCtx, pvhBaseline, &count_baseline)  );

    /* TODO take any unrecognized xattr from the baseline and put it in pvh.  I
     * think. */

fail:
	return;
}

void SG_attributes__xattrs__apply(
        SG_context* pCtx,
		const SG_pathname* pPath,
        const SG_vhash* pvh_xattrs,
        SG_repo* pRepo
        )
{
    SG_bool b = SG_FALSE;
    SG_uint32 count;
    SG_uint32 i;
    SG_byte* pData = NULL;
    SG_uint64 len;
    SG_rbtree* prbRecognized = NULL;
    SG_rbtree* prbBefore = NULL;
    SG_bool bRecognized = SG_FALSE;
	SG_rbtree_iterator* pIterator = NULL;
    const char* pszName = NULL;

    SG_ERR_CHECK(  SG_attributes__get_recognized_list(pCtx, &prbRecognized)  );
    SG_ERR_CHECK(  SG_fsobj__xattr__get_all(pCtx, pPath, prbRecognized, &prbBefore)  );

    if (pvh_xattrs)
    {
        SG_ERR_CHECK(  SG_vhash__count(pCtx, pvh_xattrs, &count)  );
        if (count)
        {
            for (i=0; i<count; i++)
            {
                const SG_variant* pv = NULL;
                const char* pszHid = NULL;

                SG_ERR_CHECK(  SG_vhash__get_nth_pair(pCtx, pvh_xattrs, i, &pszName, &pv)  );
                SG_ERR_CHECK(  SG_rbtree__find(pCtx, prbRecognized, pszName, &bRecognized, NULL)  );
                if (bRecognized)
                {
                    SG_ERR_CHECK(  SG_rbtree__find(pCtx, prbBefore, pszName, &b, NULL)  );
                    if (b)
                    {
                        SG_ERR_CHECK(  SG_rbtree__remove(pCtx, prbBefore, pszName)  );
                    }
                    SG_ERR_CHECK(  SG_variant__get__sz(pCtx, pv, &pszHid)  );
                    SG_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pRepo, pszHid, &pData, &len)  );
                    SG_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pPath, pszName, (SG_uint32) len, pData)  );
                    SG_NULLFREE(pCtx, pData);
                }
            }
        }
    }

    SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIterator, prbBefore, &b, &pszName, NULL)  );
    while (b)
    {
        SG_ERR_CHECK(  SG_fsobj__xattr__remove(pCtx, pPath, pszName)  );
        SG_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIterator, &b, &pszName, NULL)  );
    }
    SG_ERR_IGNORE(  SG_rbtree__iterator__free(pCtx, pIterator)  );
    pIterator = NULL;

    SG_RBTREE_NULLFREE(pCtx, prbBefore);
    SG_RBTREE_NULLFREE(pCtx, prbRecognized);

    return;

fail:
    SG_RBTREE_NULLFREE(pCtx, prbRecognized);
    SG_NULLFREE(pCtx, pData);
}
#endif

void SG_attributes__bits__read(
    SG_context* pCtx,
	const SG_pathname* pPath,
    SG_int64 iBaselineAttributeBits,
    SG_int64* piBits
    )
{
	SG_fsobj_stat st;
    SG_int64 iBits = iBaselineAttributeBits;

	/* Get stat information first */
	SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath, &st)  );

    /* TODO do the mask trick to ignore stuff that is not appropriate for this
     * platform */

#if defined(LINUX) || defined(MAC)
    if (SG_FSOBJ_TYPE__REGULAR == st.type)
    {
        if (st.perms & S_IXUSR)
        {
            iBits |= SG_ATTRIBUTE_BIT__X;
        }
        else
        {
            iBits &= ~SG_ATTRIBUTE_BIT__X;
        }
    }
#endif

    *piBits = iBits;

fail:
	return;
}

void SG_attributes__bits__apply(
		SG_context* pCtx,
        const SG_pathname* pPath,
        SG_int64 iBits
        )
{
	SG_fsobj_stat st;

	/* Get stat information first */
	SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPath, &st)  );

#if defined(LINUX) || defined(MAC)
    if (SG_FSOBJ_TYPE__REGULAR == st.type)
    {
        if (iBits & SG_ATTRIBUTE_BIT__X)
        {
            st.perms |= S_IXUSR;
        }
        else
        {
            st.perms &= ~S_IXUSR;
        }
        SG_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pPath, st.perms)  );
    }
#endif

#if defined(WINDOWS)
    if (iBits & 2) // TODO
    {
    }
#endif

fail:
	return;
}

