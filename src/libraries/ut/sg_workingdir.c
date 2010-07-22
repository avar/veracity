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

#define sg_DRAWER_DIRECTORY_NAME ".sgdrawer"
#define sg_WD_TEMP_DIRECTORY_NAME ".sgtemp"

void SG_workingdir__get_drawer_path(
	SG_context* pCtx,
	const SG_pathname* pPathWorkingDirectoryTop,
	SG_pathname** ppResult)
{
	SG_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, ppResult, pPathWorkingDirectoryTop, sg_DRAWER_DIRECTORY_NAME)  );
}

void SG_workingdir__verify_drawer_exists(
	SG_context* pCtx,
	const SG_pathname* pPathWorkingDirectoryTop,
	SG_pathname** ppResult)
{
	SG_pathname* pDrawerPath = NULL;
	SG_fsobj_type type = SG_FSOBJ_TYPE__UNSPECIFIED;
	SG_fsobj_perms perms;
	SG_bool bExists = SG_FALSE;

	SG_ERR_CHECK(  SG_workingdir__get_drawer_path(pCtx, pPathWorkingDirectoryTop, &pDrawerPath)  );
	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pDrawerPath, &bExists, &type, &perms)  );

	if (!bExists)
	{
		SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pDrawerPath)  );
	}
	else
	{
		if (SG_FSOBJ_TYPE__DIRECTORY != type)
		{
			SG_ERR_THROW(SG_ERR_NOT_A_DIRECTORY);
		}
	}

	*ppResult = pDrawerPath;

	return;
fail:
	return;
}

void SG_workingdir__set_mapping(
	SG_context* pCtx,
	const SG_pathname* pPathLocalDirectory,
	const char* pszNameRepoInstanceDescriptor, /**< The name of the repo instance descriptor */
	const char* pszidGidAnchorDirectory /**< The GID of the directory within the repo to which this is anchored.  Usually it's user root. */
	)
{
	SG_vhash* pvhNew = NULL;
	SG_vhash* pvh = NULL;
	SG_pathname* pMyPath = NULL;
	SG_pathname* pDrawerPath = NULL;
	SG_pathname* pMappingFilePath = NULL;

	SG_NULLARGCHECK_RETURN(pPathLocalDirectory);
	SG_NULLARGCHECK_RETURN(pszNameRepoInstanceDescriptor);

	/* make a copy of the path so we can modify it (adding the final slash) */
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pMyPath, pPathLocalDirectory)  );

	/* null the original parameter pointer to make sure we don't use it anymore */
	pPathLocalDirectory = NULL;

	/* make sure the path we were given is a directory that exists */
	SG_ERR_CHECK(  SG_fsobj__verify_directory_exists_on_disk__pathname(pCtx, pMyPath)  );

	/* it's a directory, so it should have a final slash */
	SG_ERR_CHECK(  SG_pathname__add_final_slash(pCtx, pMyPath)  );

	/* make sure the name of the repo instance descriptor is valid */
	SG_ERR_CHECK(  SG_closet__descriptors__get(pCtx, pszNameRepoInstanceDescriptor, &pvh) );
	SG_VHASH_NULLFREE(pCtx, pvh);

	// TODO verify that the anchor GID is valid for that repo?

	SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhNew)  );

	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhNew, "descriptor", pszNameRepoInstanceDescriptor)  );
	SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhNew, "anchor", pszidGidAnchorDirectory)  );

	SG_ERR_CHECK(  SG_workingdir__verify_drawer_exists(pCtx, pMyPath, &pDrawerPath)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pMappingFilePath, pDrawerPath, "repo.json")  );

	SG_ERR_CHECK(  SG_vfile__update__vhash(pCtx, pMappingFilePath, "mapping", pvhNew)  );

	SG_VHASH_NULLFREE(pCtx, pvhNew);
	SG_PATHNAME_NULLFREE(pCtx, pMyPath);
	SG_PATHNAME_NULLFREE(pCtx, pDrawerPath);
	SG_PATHNAME_NULLFREE(pCtx, pMappingFilePath);

	return;
fail:
	SG_PATHNAME_NULLFREE(pCtx, pDrawerPath);
	SG_PATHNAME_NULLFREE(pCtx, pMappingFilePath);
	SG_PATHNAME_NULLFREE(pCtx, pMyPath);
	SG_VHASH_NULLFREE(pCtx, pvhNew);
	SG_VHASH_NULLFREE(pCtx, pvh);
}

void SG_workingdir__find_mapping(
	SG_context* pCtx,
	const SG_pathname* pPathLocalDirectory,
	SG_pathname** ppPathMappedLocalDirectory, /**< Return the actual local directory that contains the mapping */
	SG_string** ppstrNameRepoInstanceDescriptor, /**< Return the name of the repo instance descriptor */
	char** ppszidGidAnchorDirectory /**< Return the GID of the repo directory */
	)
{
	SG_pathname* curpath = NULL;
	SG_string* result_pstrDescriptorName = NULL;
	char* result_pszidGid = NULL;
	SG_pathname* result_mappedLocalDirectory = NULL;
	SG_vhash* pvhMapping = NULL;
	SG_pathname* pDrawerPath = NULL;
	SG_pathname* pMappingFilePath = NULL;
	SG_vhash* pvh = NULL;

	SG_NULLARGCHECK_RETURN(pPathLocalDirectory);

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &curpath, pPathLocalDirectory)  );

	/* it's a directory, so it should have a final slash */
	SG_ERR_CHECK(  SG_pathname__add_final_slash(pCtx, curpath)  );

	while (SG_TRUE)
	{
		SG_ERR_CHECK(  SG_workingdir__get_drawer_path(pCtx, curpath, &pDrawerPath)  );

		SG_fsobj__verify_directory_exists_on_disk__pathname(pCtx, pDrawerPath);
		if (!SG_context__has_err(pCtx))
		{
			const char* pszDescriptorName = NULL;
			const char* pszGid = NULL;

			SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pMappingFilePath, pDrawerPath, "repo.json")  );
			SG_PATHNAME_NULLFREE(pCtx, pDrawerPath);

			SG_ERR_CHECK(  SG_vfile__slurp(pCtx, pMappingFilePath, &pvh)  );

			SG_ERR_CHECK(  SG_vhash__get__vhash(pCtx, pvh, "mapping", &pvhMapping)  );

			SG_PATHNAME_NULLFREE(pCtx, pMappingFilePath);

			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhMapping, "descriptor", &pszDescriptorName)  );
			SG_ERR_CHECK(  SG_vhash__get__sz(pCtx, pvhMapping, "anchor", &pszGid)  );

			SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &result_pstrDescriptorName)  );
			SG_ERR_CHECK(  SG_string__set__sz(pCtx, result_pstrDescriptorName, pszDescriptorName)  );

			if (pszGid)
			{
				SG_ERR_CHECK(  SG_gid__alloc_clone(pCtx, pszGid, &result_pszidGid)  );
			}
			else
			{
				result_pszidGid = NULL;
			}

			SG_VHASH_NULLFREE(pCtx, pvh);

			result_mappedLocalDirectory = curpath;
			curpath = NULL;

			break;
		}
		else
			SG_context__err_reset(pCtx);

		SG_PATHNAME_NULLFREE(pCtx, pDrawerPath);

		SG_pathname__remove_last(pCtx, curpath);

		if (SG_context__err_equals(pCtx, SG_ERR_CANNOTTRIMROOTDIRECTORY))
		{
			SG_context__err_reset(pCtx);
			break;
		}
		else
		{
			SG_ERR_CHECK_CURRENT;
		}
	}

	if (result_mappedLocalDirectory)
	{
        if (ppPathMappedLocalDirectory)
        {
		    *ppPathMappedLocalDirectory = result_mappedLocalDirectory;
        }
        else
        {
	        SG_PATHNAME_NULLFREE(pCtx, result_mappedLocalDirectory);
        }
        if (ppstrNameRepoInstanceDescriptor)
        {
            *ppstrNameRepoInstanceDescriptor = result_pstrDescriptorName;
        }
        else
        {
            SG_STRING_NULLFREE(pCtx, result_pstrDescriptorName);
        }
        if (ppszidGidAnchorDirectory)
        {
            *ppszidGidAnchorDirectory = result_pszidGid;
        }
        else
        {
            SG_NULLFREE(pCtx, result_pszidGid);
        }

		return;
	}
	else
	{
		SG_PATHNAME_NULLFREE(pCtx, curpath);

		SG_ERR_THROW_RETURN(SG_ERR_NOT_FOUND);
	}

fail:
	SG_VHASH_NULLFREE(pCtx, pvh);
	SG_PATHNAME_NULLFREE(pCtx, pDrawerPath);
	SG_PATHNAME_NULLFREE(pCtx, pMappingFilePath);
	SG_PATHNAME_NULLFREE(pCtx, result_mappedLocalDirectory);
	SG_PATHNAME_NULLFREE(pCtx, curpath);
}

//////////////////////////////////////////////////////////////////

/**
 * TODO Jeff says:  the functions __get_{entry,entry2,dir} are confusingly named.
 * To me, the __get_ implies that they are going to fill in a variable from a
 * private structure RATHER than populating the working directory -- yeah, i know
 * that the vv verb is called GET (perhaps that is the problem).
 */
static void _sg_workingdir__get_entry2(SG_context * pCtx,
									   SG_repo * pRepo,
									   const SG_pathname * pPathSub,
									   const char * pszGid,
									   SG_treenode_entry_type type,
									   const char * pszidHidContent,
									   const char * pszidHidXattrs,
									   SG_int64 iAttributeBits,
									   SG_vhash * pvhTimestamps);

static void _sg_workingdir__get_entry(SG_context* pCtx,
									  SG_repo* pRepo,
									  const SG_pathname* pPathSub,
									  const char * pszGid,
									  const SG_treenode_entry* pEntry,
									  SG_vhash * pvhTimestamps);

static void _sg_workingdir__get_dir(SG_context* pCtx,
									SG_repo* pRepo,
									const SG_pathname* pPathLocal,
									const char* pszidHidTreeNode,
									SG_vhash * pvhTimestamps);

//////////////////////////////////////////////////////////////////

#ifdef SG_BUILD_FLAG_FEATURE_XATTR
static void _sg_workingdir__set_xattrs(SG_context * pCtx,
									   SG_repo * pRepo,
									   const SG_pathname * pPathSub,
									   const char * pszidHidXattrs)
{
    SG_vhash* pvhAttributes = NULL;

	SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, pRepo, pszidHidXattrs, &pvhAttributes)  );
	SG_ERR_CHECK(  SG_attributes__xattrs__apply(pCtx, pPathSub, pvhAttributes, pRepo)  );

fail:
	SG_VHASH_NULLFREE(pCtx, pvhAttributes);
}
#endif

static void _sg_workingdir__get_entry2(SG_context * pCtx,
									   SG_repo * pRepo,
									   const SG_pathname * pPathSub,
									   const char * pszGid,
									   SG_treenode_entry_type type,
									   const char * pszidHidContent,
									   const char * pszidHidXattrs,
									   SG_int64 iAttributeBits,
									   SG_vhash * pvhTimestamps)
{
	SG_file* pFile = NULL;
	SG_string* pstrLink = NULL;
	SG_byte* pBytes = NULL;
	SG_vhash * pvhGid = NULL;

    if (SG_TREENODEENTRY_TYPE_DIRECTORY == type)
    {
        /* create the directory and then recurse into it */
        SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathSub)  );
        SG_ERR_CHECK(  _sg_workingdir__get_dir(pCtx, pRepo, pPathSub, pszidHidContent, pvhTimestamps)  );
    }
    else if (SG_TREENODEENTRY_TYPE_REGULAR_FILE == type)
    {
        SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathSub, SG_FILE_RDWR | SG_FILE_CREATE_NEW, SG_FSOBJ_PERMS__MASK, &pFile)  );
        SG_ERR_CHECK(  SG_repo__fetch_blob_into_file(pCtx, pRepo, pszidHidContent, pFile, NULL)  );
        SG_ERR_CHECK(  SG_file__close(pCtx, &pFile)  );
    }
    else if (SG_TREENODEENTRY_TYPE_SYMLINK == type)
    {
        SG_uint64 iLenBytes = 0;

        SG_ERR_CHECK(  SG_repo__fetch_blob_into_memory(pCtx, pRepo, pszidHidContent, &pBytes, &iLenBytes)  );
        SG_ERR_CHECK(  SG_STRING__ALLOC__BUF_LEN(pCtx, &pstrLink, pBytes, (SG_uint32) iLenBytes)  );
        SG_ERR_CHECK(  SG_fsobj__symlink(pCtx, pstrLink, pPathSub)  );
        SG_NULLFREE(pCtx, pBytes);
        SG_STRING_NULLFREE(pCtx, pstrLink);
    }
    else
    {
        SG_ERR_THROW(SG_ERR_NOTIMPLEMENTED);
    }

    if (pszidHidXattrs)
    {
#ifdef SG_BUILD_FLAG_FEATURE_XATTR
        SG_ERR_CHECK(  _sg_workingdir__set_xattrs(pCtx, pRepo, pPathSub, pszidHidXattrs)  );
#else
		// TODO do we need to stuff something into the pendingtree to remind us
		// TODO that the entry originally had an XAttr and we just didn't restore
		// TODO it when we populated the WD on this Windows system?
#endif
    }

    SG_ERR_CHECK(  SG_attributes__bits__apply(pCtx, pPathSub, iAttributeBits)  );

	if (pvhTimestamps && (SG_TREENODEENTRY_TYPE_REGULAR_FILE == type))
	{
		SG_fsobj_stat stat;
		SG_int64 iTimeNow;

		SG_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pPathSub, &stat)  );
		SG_ERR_CHECK(  SG_time__get_milliseconds_since_1970_utc(pCtx, &iTimeNow)  );

		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhGid)  );
		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvhGid, "mtime_ms", stat.mtime_ms)  );
		SG_ERR_CHECK(  SG_vhash__add__int64(pCtx, pvhGid, "clock_ms", iTimeNow)  );

		SG_ERR_CHECK(  SG_vhash__add__vhash(pCtx, pvhTimestamps, pszGid, &pvhGid)  );	// this steals our vhash
	}

fail:
	SG_VHASH_NULLFREE(pCtx, pvhGid);
}

static void _sg_workingdir__get_entry(SG_context* pCtx,
									  SG_repo* pRepo,
									  const SG_pathname* pPathSub,
									  const char * pszGid,
									  const SG_treenode_entry* pEntry,
									  SG_vhash * pvhTimestamps)
{
    SG_treenode_entry_type type;
    const char* pszidHidContent = NULL;
    const char* pszidHidXattrs = NULL;
    SG_int64 iAttributeBits = 0;

    SG_ERR_CHECK(  SG_treenode_entry__get_entry_type(pCtx, pEntry, &type)  );
    SG_ERR_CHECK(  SG_treenode_entry__get_hid_blob(pCtx, pEntry, &pszidHidContent)  );
    SG_ERR_CHECK(  SG_treenode_entry__get_hid_xattrs(pCtx, pEntry, &pszidHidXattrs)  );
    SG_ERR_CHECK(  SG_treenode_entry__get_attribute_bits(pCtx, pEntry, &iAttributeBits)  );

	SG_ERR_CHECK(  _sg_workingdir__get_entry2(pCtx, pRepo, pPathSub,
											  pszGid,
											  type, pszidHidContent, pszidHidXattrs, iAttributeBits,
											  pvhTimestamps)  );

fail:
    return;
}

static void _sg_workingdir__get_dir(SG_context* pCtx,
									SG_repo* pRepo,
									const SG_pathname* pPathLocal,
									const char* pszidHidTreeNode,
									SG_vhash * pvhTimestamps)
{
	SG_uint32 count;
	SG_uint32 i;
	SG_pathname* pPathSub = NULL;
	SG_treenode* pTreenode = NULL;

	SG_ERR_CHECK(  SG_treenode__load_from_repo(pCtx, pRepo, pszidHidTreeNode, &pTreenode)  );
	SG_ERR_CHECK(  SG_treenode__count(pCtx, pTreenode, &count)  );

	for (i=0; i<count; i++)
	{
		const char * pszGid;
		const SG_treenode_entry* pEntry;
		const char* pszName;

		SG_ERR_CHECK(  SG_treenode__get_nth_treenode_entry__ref(pCtx, pTreenode, i, &pszGid, &pEntry)  );
		SG_ERR_CHECK(  SG_treenode_entry__get_entry_name(pCtx, pEntry, &pszName)  );

        SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathSub, pPathLocal, pszName)  );

        SG_ERR_CHECK(  _sg_workingdir__get_entry(pCtx, pRepo, pPathSub, pszGid, pEntry, pvhTimestamps)  );

        SG_PATHNAME_NULLFREE(pCtx, pPathSub);
	}

fail:
	SG_TREENODE_NULLFREE(pCtx, pTreenode);
	SG_PATHNAME_NULLFREE(pCtx, pPathSub);
}

/**
 * This function is used to get a new tree.  It must be called with the HID of
 * the top-level user root directory.  Not the super-root.  The directory which
 * corresponds to @.
 *
 * Record the file timestamp of each file we fetch in the pvhTimestamps; this
 * provides the basis for the timestamp check in scan-dir.
 */
static void sg_workingdir__do_get_dir__top(SG_context* pCtx,
										   SG_repo* pRepo,
										   const SG_pathname* pPathLocal,
										   const char* pszidHidTreeNode,
										   SG_vhash * pvhTimestamps)
{
	SG_pathname* pPathSub = NULL;
	SG_treenode* pTreenode = NULL;
	SG_string* pstrLink = NULL;
	SG_byte* pBytes = NULL;
    SG_vhash* pvhAttributes = NULL;
    SG_int64 iAttributeBits = 0;
    const SG_treenode_entry* pEntry = NULL;
    const char* pszidHidContent = NULL;
#ifdef SG_BUILD_FLAG_FEATURE_XATTR
    const char* pszidHidXattrs = NULL;
#endif
	SG_bool bExists = SG_FALSE;

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
    SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPathSub, pPathLocal)  );
	SG_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathSub, &bExists, NULL, NULL)  );
	if (!bExists)
		SG_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathSub)  );
    SG_ERR_CHECK(  _sg_workingdir__get_dir(pCtx, pRepo, pPathSub, pszidHidContent, pvhTimestamps)  );

#ifdef SG_BUILD_FLAG_FEATURE_XATTR
    /* fix up the xattrs on that directory */
    SG_ERR_CHECK(  SG_treenode_entry__get_hid_xattrs(pCtx, pEntry, &pszidHidXattrs)  );
    if (pszidHidXattrs)
    {
        SG_ERR_CHECK(  SG_repo__fetch_vhash(pCtx, pRepo, pszidHidXattrs, &pvhAttributes)  );
        SG_ERR_CHECK(  SG_attributes__xattrs__apply(pCtx, pPathSub, pvhAttributes, pRepo)  );
        SG_VHASH_NULLFREE(pCtx, pvhAttributes);
    }
#endif

    /* and the attrbits too */
    SG_ERR_CHECK(  SG_treenode_entry__get_attribute_bits(pCtx, pEntry, &iAttributeBits)  );
    SG_ERR_CHECK(  SG_attributes__bits__apply(pCtx, pPathSub, iAttributeBits)  );

    SG_PATHNAME_NULLFREE(pCtx, pPathSub);
	SG_TREENODE_NULLFREE(pCtx, pTreenode);

	return;
fail:
	/* TODO free stuff */
    SG_VHASH_NULLFREE(pCtx, pvhAttributes);
	SG_NULLFREE(pCtx, pBytes);
	SG_STRING_NULLFREE(pCtx, pstrLink);
}

void SG_workingdir__create_and_get(
	SG_context* pCtx,
	const char* pszDescriptorName,
	const SG_pathname* pPathDirPutTopLevelDirInHere,
	SG_bool bCreateDrawer,
    const char* psz_spec_hid_cs_baseline
	)
{
	SG_repo* pRepo = NULL;
	SG_rbtree* pIdsetLeaves = NULL;
	SG_uint32 count_leaves = 0;
	SG_changeset* pcs = NULL;
	const char* pszidUserSuperRoot = NULL;
	SG_bool b = SG_FALSE;
    char* psz_hid_cs_baseline = NULL;
	SG_pendingtree * pPendingTree = NULL;
	SG_vhash * pvhTimestamps = NULL;

	/*
	 * Fetch the descriptor by its given name and use it to connect to
	 * the repo.
	 */
	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, pszDescriptorName, &pRepo)  );


	if (psz_spec_hid_cs_baseline)
	{
		SG_ERR_CHECK(  SG_strdup(pCtx, psz_spec_hid_cs_baseline, &psz_hid_cs_baseline)  );
	}
	else
    {
        const char* psz_hid = NULL;
        /*
         * If you do not specify a hid to be the baseline, then this routine
         * currently only works if there is exactly one leaf in the repo.
         */
        SG_ERR_CHECK(  SG_repo__fetch_dag_leaves(pCtx, pRepo,SG_DAGNUM__VERSION_CONTROL,&pIdsetLeaves)  );
        SG_ERR_CHECK(  SG_rbtree__count(pCtx, pIdsetLeaves, &count_leaves)  );

		if (count_leaves != 1)
			SG_ERR_THROW(  SG_ERR_MULTIPLE_HEADS_FROM_DAGNODE  );

        SG_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, NULL, pIdsetLeaves, &b, &psz_hid, NULL)  );

        SG_ERR_CHECK(  SG_STRDUP(pCtx, psz_hid, &psz_hid_cs_baseline)  );
    }

	/*
	 * Load the desired changeset from the repo so we can look up the
	 * id of its user root directory
	 */
	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx, pRepo, psz_hid_cs_baseline, &pcs)  );
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx, pcs, &pszidUserSuperRoot)  );

	if (bCreateDrawer)
	{
		SG_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhTimestamps)  );

		// Retrieve everything into the WD and capture the timestamps on the files that we create.
		SG_ERR_CHECK(  sg_workingdir__do_get_dir__top(pCtx, pRepo, pPathDirPutTopLevelDirInHere, pszidUserSuperRoot, pvhTimestamps)  );

		// this creates "repo.json" with the repo-descriptor.
		SG_ERR_CHECK(  SG_workingdir__set_mapping(pCtx, pPathDirPutTopLevelDirInHere, pszDescriptorName, NULL)  );

		// this creates an empty "wd.json" file (which doesn't know anything).
		SG_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathDirPutTopLevelDirInHere, SG_TRUE, &pPendingTree)  );

		// force set the initial parents to the current changeset.
		SG_ERR_CHECK(  SG_pendingtree__set_single_wd_parent(pCtx, pPendingTree, psz_hid_cs_baseline)  );

		// force initialize the timestamp cache to the list that we just built; this should
		// be the only timestamps in the cache since we just populated the WD.
		SG_ERR_CHECK(  SG_pendingtree__set_wd_timestamp_cache(pCtx, pPendingTree, &pvhTimestamps)  );	// this steals our vhash

		SG_ERR_CHECK(  SG_pendingtree__save(pCtx, pPendingTree)  );
	}
	else
	{
		// Retrieve everything into the WD but do not create .sgdrawer or record timestamps.
		// This is more like an EXPORT operation.
		SG_ERR_CHECK(  sg_workingdir__do_get_dir__top(pCtx, pRepo, pPathDirPutTopLevelDirInHere, pszidUserSuperRoot, NULL)  );
	}


fail:
	SG_VHASH_NULLFREE(pCtx, pvhTimestamps);
    SG_NULLFREE(pCtx, psz_hid_cs_baseline);
	SG_CHANGESET_NULLFREE(pCtx, pcs);
	SG_RBTREE_NULLFREE(pCtx, pIdsetLeaves);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

//////////////////////////////////////////////////////////////////

void SG_workingdir__get_temp_directory_name(SG_context * pCtx,
											const char ** ppszEntryname)
{
	SG_NULLARGCHECK_RETURN(ppszEntryname);

	*ppszEntryname = sg_WD_TEMP_DIRECTORY_NAME;
}

void SG_workingdir__get_temp_path(SG_context* pCtx,
								  const SG_pathname* pPathWorkingDirectoryTopAsGiven,
								  SG_pathname** ppResult)
{
	SG_pathname * pPathname = NULL;

	SG_NULLARGCHECK_RETURN(ppResult);

	// I don't trust the given <wd-top>, get the actual one from the disk
	// using this as a starting point.

	SG_ERR_CHECK(  SG_workingdir__find_mapping(pCtx,pPathWorkingDirectoryTopAsGiven,&pPathname,NULL,NULL)  );
	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pPathname,sg_WD_TEMP_DIRECTORY_NAME)  );

	*ppResult = pPathname;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathname);
}

void SG_workingdir__generate_and_create_temp_dir_for_purpose(SG_context * pCtx,
															 const SG_pathname * pPathWorkingDirectoryTop,
															 const char * pszPurpose,
															 SG_pathname ** ppPathTempDir)
{
	SG_pathname * pPathTempRoot = NULL;
	SG_pathname * pPath = NULL;
	SG_string * pString = NULL;
	SG_int64 iTimeUTC;
	SG_time tmLocal;
	SG_uint32 kAttempt = 0;

	SG_NONEMPTYCHECK_RETURN(pszPurpose);
	SG_NULLARGCHECK_RETURN(ppPathTempDir);

	// get path to "<wd-top>/.sgtemp".
	SG_ERR_CHECK(  SG_workingdir__get_temp_path(pCtx,pPathWorkingDirectoryTop,&pPathTempRoot)  );

	SG_ERR_CHECK(  SG_time__get_milliseconds_since_1970_utc(pCtx,&iTimeUTC)  );
	SG_ERR_CHECK(  SG_time__decode__local(pCtx,iTimeUTC,&tmLocal)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx,&pString)  );

	while (1)
	{
		// build path "<wd-top>/.sgtemp/<purpose>_20091201_0".
		// where <purpose> is something like "revert" or "merge".

		SG_ERR_CHECK(  SG_string__sprintf(pCtx,pString,"%s_%04d%02d%02d_%d",
										  pszPurpose,
										  tmLocal.year,tmLocal.month,tmLocal.mday,
										  kAttempt++)  );
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pPath,pPathTempRoot,SG_string__sz(pString))  );

		// try to create a NEW temp directory.  if this path already exists on disk,
		// loop and try again.  if we have a hard errors, just give up.

		SG_fsobj__mkdir_recursive__pathname(pCtx,pPath);
		if (SG_context__has_err(pCtx) == SG_FALSE)
			goto success;

		if (SG_context__err_equals(pCtx,SG_ERR_DIR_ALREADY_EXISTS) == SG_FALSE)
			SG_ERR_RETHROW;

		SG_context__err_reset(pCtx);
		SG_PATHNAME_NULLFREE(pCtx,pPath);
	}

success:
	*ppPathTempDir = pPath;

	SG_STRING_NULLFREE(pCtx, pString);
	SG_PATHNAME_NULLFREE(pCtx, pPathTempRoot);
	return;

fail:
	SG_STRING_NULLFREE(pCtx, pString);
	SG_PATHNAME_NULLFREE(pCtx, pPathTempRoot);
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

//////////////////////////////////////////////////////////////////

void SG_workingdir__construct_absolute_path_from_repo_path(SG_context * pCtx,
														   const SG_pathname * pPathWorkingDirectoryTop,
														   const char * pszRepoPath,
														   SG_pathname ** ppPathAbsolute)
{
	SG_pathname * pPath = NULL;

#if defined(DEBUG)
	// TODO we should verify "<wd-top>/.sgdrawer" exists to make sure
	// TODO that we have the actual wd-root.  especially when using vv
	// TODO with relative paths while cd'd somewhere deep inside the tree.
#endif

	if (pszRepoPath[0] != '@')
	{
		SG_ERR_THROW2_RETURN(  SG_ERR_INVALID_REPO_PATH,
							   (pCtx, "Must begin with '@/': %s", pszRepoPath)  );
	}
	else
	{
		if (pszRepoPath[1] == 0)		// quietly allow "@" as substitute for "@/"
		{
			SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPath, pPathWorkingDirectoryTop)  );
		}
		else if (pszRepoPath[1] != '/')
		{
			SG_ERR_THROW2_RETURN(  SG_ERR_INVALID_REPO_PATH,
								   (pCtx, "Must begin with '@/': %s", pszRepoPath)  );
		}
		else
		{
			if (pszRepoPath[2])
				SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pPathWorkingDirectoryTop, &pszRepoPath[2])  );
			else
				SG_ERR_CHECK(  SG_PATHNAME__ALLOC__COPY(pCtx, &pPath, pPathWorkingDirectoryTop)  );
		}
	}

	*ppPathAbsolute = pPath;
	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPath);
}

void SG_workingdir__construct_absolute_path_from_repo_path2(SG_context * pCtx,
															const SG_pendingtree * pPendingTree,
															const char * pszRepoPath,
															SG_pathname ** ppPathAbsolute)
{
	const SG_pathname * pPathWorkingDirectoryTop;				// we do not own this

	SG_ERR_CHECK_RETURN(  SG_pendingtree__get_working_directory_top__ref(pCtx, pPendingTree, &pPathWorkingDirectoryTop)  );
	SG_ERR_CHECK_RETURN(  SG_workingdir__construct_absolute_path_from_repo_path(pCtx, pPathWorkingDirectoryTop, pszRepoPath, ppPathAbsolute)  );
}

void SG_workingdir__wdpath_to_repopath(
	SG_context* pCtx,
	const SG_pathname* pPathWorkingDirectoryTop,
	const SG_pathname* pPath,
	SG_bool bFinalSlash,
	SG_string** ppStrRepoPath)
{
	SG_string* pstrRoot = NULL;
	SG_string* pstrRelative = NULL;
	SG_string* pstrRepoPath = NULL;

    /* TODO fix the following line to get the real anchor path for this working
     * dir */
	SG_ERR_CHECK(  SG_STRING__ALLOC__SZ(pCtx, &pstrRoot, "@/")  );

	SG_ERR_CHECK(  SG_pathname__make_relative(pCtx, pPathWorkingDirectoryTop, pPath, &pstrRelative)  );

	if (pstrRelative)
	{
		SG_ERR_CHECK(  SG_repopath__combine(pCtx, pstrRoot, pstrRelative, bFinalSlash, &pstrRepoPath)  );
		SG_STRING_NULLFREE(pCtx, pstrRelative);
		SG_STRING_NULLFREE(pCtx, pstrRoot);

		*ppStrRepoPath = pstrRepoPath;
	}
	else
	{
		*ppStrRepoPath = pstrRoot;
	}

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pstrRoot);
	SG_STRING_NULLFREE(pCtx, pstrRelative);
	SG_STRING_NULLFREE(pCtx, pstrRepoPath);
}
