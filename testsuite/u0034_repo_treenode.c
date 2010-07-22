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
 * @file u0034_repo_treenode.c
 *
 * @details Create a repo, and use SG_repo interfaces
 * to create a collection of userfiles and treenodes.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

//////////////////////////////////////////////////////////////////

SG_uint32 g_u0034_repo_treenode__kFile = 0;
SG_uint32 g_u0034_repo_treenode__kDir  = 0;

//////////////////////////////////////////////////////////////////
// a vhash sorted by full dollar-path containing the HID and GID
// of each treenode-entry.

SG_vhash * gpVHashListOfEntries = NULL;

void u0034_repo_treenode__print_entry_from_global_vhash_cb(SG_context* pCtx,SG_UNUSED_PARAM(void * ctx),
															   SG_UNUSED_PARAM(const SG_vhash * pvh),
															   const char * szKey,
															   const SG_variant * pVariantValue)
{
	const char * szValueHID;
	const char * szValueGID;
	SG_vhash * pVHashValue;

	SG_UNUSED(pvh);
	SG_UNUSED(ctx);

	SG_variant__get__vhash(pCtx, pVariantValue,&pVHashValue);
	SG_vhash__get__sz(pCtx, pVHashValue,"hidBlob",&szValueHID);
	SG_vhash__get__sz(pCtx, pVHashValue,"gidObject",&szValueGID);

	INFOP("list",("%s : hid[%s] gid[%s]",szKey,szValueHID,szValueGID));
	return;
}

void u0034_repo_treenode__add_entry_to_list(SG_context* pCtx,const char * szDollarPath, const char* pszidHidBlob, const char* pszidGidObject)
{
	// add full dollar-path of entry along with gid and hid to the global list
	// we're making so we can do some tests later.  this has nothing
	// to do with the repo and changesets.

	SG_vhash * pVHashValue = NULL;

	SG_VHASH__ALLOC(pCtx, &pVHashValue);
	SG_vhash__add__string__sz(pCtx, pVHashValue,"hidBlob",pszidHidBlob);
	SG_vhash__add__string__sz(pCtx, pVHashValue,"gidObject",pszidGidObject);

	SG_vhash__add__vhash(pCtx, gpVHashListOfEntries,szDollarPath,&pVHashValue);
}

//////////////////////////////////////////////////////////////////

SG_repo * u0034_repo_treenode__open_repo(SG_context* pCtx)
{
	// repo's are created on disk as <RepoContainerDirectory>/<RepoGID>/{blobs,...}
	// we pass the current directory as RepoContainerDirectory.
	// caller must free returned value.

	SG_repo * pRepo;
	SG_pathname * pPathnameRepoDir = NULL;
	SG_vhash* pvhPartialDescriptor = NULL;
    char buf_repo_id[SG_GID_BUFFER_LENGTH];
    char buf_admin_id[SG_GID_BUFFER_LENGTH];
	char* pszRepoImpl = NULL;

	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_repo_id, sizeof(buf_repo_id))  );
	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_admin_id, sizeof(buf_admin_id))  );

	VERIFY_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC(pCtx, &pPathnameRepoDir)  );
	VERIFY_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathnameRepoDir)  );

	VERIFY_ERR_CHECK( SG_VHASH__ALLOC(pCtx, &pvhPartialDescriptor)  );

	VERIFY_ERR_CHECK_DISCARD(  SG_localsettings__get__sz(pCtx, SG_LOCALSETTING__NEWREPO_DRIVER, NULL, &pszRepoImpl, NULL)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_vhash__add__string__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_KEY__STORAGE, pszRepoImpl)  );

	VERIFY_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, pvhPartialDescriptor, SG_RIDESC_FSLOCAL__PATH_PARENT_DIR, SG_pathname__sz(pPathnameRepoDir))  );

	VERIFY_ERR_CHECK(  SG_repo__create_repo_instance(pCtx,pvhPartialDescriptor,SG_TRUE,NULL,buf_repo_id,buf_admin_id,&pRepo)  );

	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);

	{
		const SG_vhash * pvhRepoDescriptor = NULL;
		VERIFY_ERR_CHECK(  SG_repo__get_descriptor(pCtx, pRepo,&pvhRepoDescriptor)  );

		//INFOP("open_repo",("Repo is [%s]",SG_string__sz(pstrRepoDescriptor)));
	}

    SG_NULLFREE(pCtx, pszRepoImpl);

	SG_PATHNAME_NULLFREE(pCtx, pPathnameRepoDir);
	return pRepo;

fail:
    SG_NULLFREE(pCtx, pszRepoImpl);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameRepoDir);
	return NULL;
}

SG_pathname * u0034_repo_treenode__create_tmp_src_dir(SG_context* pCtx)
{
	// create a temp directory in the current directory to be the
	// home of some userfiles.
	// caller must free returned value.

	SG_pathname * pPathnameTempDir = NULL;

	VERIFY_ERR_CHECK_RETURN(  unittest__alloc_unique_pathname_in_cwd(pCtx,&pPathnameTempDir)  );

	VERIFY_ERR_CHECK_RETURN( SG_fsobj__mkdir_recursive__pathname(pCtx, pPathnameTempDir)  );

	INFOP("mktmpdir",("Temp Src Dir is [%s]",SG_pathname__sz(pPathnameTempDir)));

	return pPathnameTempDir;
}

SG_file * u0034_repo_treenode__create_userfile_in_tmp_dir(SG_context* pCtx,SG_pathname * pPathnameTempDir,
														  char * bufFilenameGenerated)
{
	// create a temp file in the given temp directory.
	//
	// return the opened temp file.
	// caller must close this file.

	SG_pathname * pPathnameTempFile;
	SG_file * pFileTempFile;

	// construct a unique temp file name.
	// stuff the temp file name in the caller's buffer

	SG_sprintf(pCtx, bufFilenameGenerated,1024,"File%04d",g_u0034_repo_treenode__kFile++);

	SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathnameTempFile,pPathnameTempDir,bufFilenameGenerated);
	SG_file__open__pathname(pCtx, pPathnameTempFile,SG_FILE_RDWR|SG_FILE_CREATE_NEW,0644,&pFileTempFile);

	// write the filename into the file a couple of times just to have some (unique) data in the file.

	SG_file__write(pCtx, pFileTempFile,(SG_uint32)strlen(bufFilenameGenerated),(SG_byte *)bufFilenameGenerated,NULL);
	SG_file__write(pCtx, pFileTempFile,(SG_uint32)strlen(bufFilenameGenerated),(SG_byte *)bufFilenameGenerated,NULL);

	SG_PATHNAME_NULLFREE(pCtx, pPathnameTempFile);

	return pFileTempFile;
}

void u0034_repo_treenode__add_treenode_entry_to_treenode(SG_context* pCtx,
														 SG_treenode * pTreenode,
														 SG_treenode_entry_type tneType,
														 const char* pszidHidBlob,
														 const char * szFilename,
														 const char * szBufDollarPath)
{
	// allocate and populate a treenode-entry (this can be a file or sub-directory).
	// add it to the given treenode.

	SG_treenode_entry * pTreenodeEntry;
	char* pszidGidObject;

	VERIFY_ERR_CHECK(  SG_treenode_entry__alloc(pCtx, &pTreenodeEntry)  );

	// this test was written at version 1.

	VERIFY_ERR_CHECK(  SG_treenode_entry__set_entry_type(pCtx, pTreenodeEntry,tneType)   );

	VERIFY_ERR_CHECK(  SG_treenode_entry__set_hid_xattrs(pCtx, pTreenodeEntry,NULL)  );

	VERIFY_ERR_CHECK(  SG_treenode_entry__set_hid_blob(pCtx, pTreenodeEntry,pszidHidBlob)  );

	VERIFY_ERR_CHECK(  SG_treenode_entry__set_entry_name(pCtx, pTreenodeEntry,szFilename)  );

	VERIFY_ERR_CHECK(  SG_treenode_entry__set_attribute_bits(pCtx, pTreenodeEntry,0)  );

	// in this test, we use a random GID for the permanent object id.

	VERIFY_ERR_CHECK( SG_gid__alloc(pCtx, &pszidGidObject)  );

	// add treenode-entry to treenode

	VERIFY_ERR_CHECK(  SG_treenode__add_entry(pCtx, pTreenode,pszidGidObject,&pTreenodeEntry)  );

	u0034_repo_treenode__add_entry_to_list(pCtx, szBufDollarPath,pszidHidBlob,pszidGidObject);


	SG_NULLFREE(pCtx, pszidGidObject);

fail:
	SG_TREENODE_ENTRY_NULLFREE(pCtx, pTreenodeEntry);
}


void u0034_repo_treenode__populate_treenode_with_userfile(SG_context* pCtx,SG_repo * pRepo, SG_treenode * pTreenode,
														  SG_pathname * pPathnameTempDir,
														  const char * szDollarPathPrefix)
{
	// populate the given treenode with a userfile.
	// we create a temp file in the given temp directory.
	// we let the repo create a blob for it.
	// we use the blob's HID and add an entry to the treenode.
	//
	// construct full dollar-path for this entry and add it to our global list.
	// this has nothing to do with the repo and changesets, it'll
	// let us do some tests later.

	char* pszidHidBlob;
	SG_file * pFileTempFile;
	char bufFilename[1024];
	char bufDollarPath[2048];
	SG_repo_tx_handle* pTx = NULL;
	SG_uint64 iBlobFullLength = 0;

	// create a temp file. (and populate bufFilename)

	pFileTempFile = u0034_repo_treenode__create_userfile_in_tmp_dir(pCtx, pPathnameTempDir,bufFilename);
	SG_sprintf(pCtx, bufDollarPath,SG_NrElements(bufDollarPath),"%s/%s",szDollarPathPrefix,bufFilename);

	// let repo create a blob for us.
	VERIFY_ERR_CHECK_DISCARD(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  SG_repo__store_blob_from_file(pCtx, pRepo,pTx,NULL,SG_FALSE,pFileTempFile,&pszidHidBlob,&iBlobFullLength)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );

	// create a treenode-entry for this file and add it to the treenode.

	u0034_repo_treenode__add_treenode_entry_to_treenode(pCtx, pTreenode,
														SG_TREENODEENTRY_TYPE_REGULAR_FILE,
														pszidHidBlob,
														bufFilename,
														bufDollarPath);

	// clean up
fail:
	SG_FILE_NULLCLOSE(pCtx, pFileTempFile);
	SG_NULLFREE(pCtx, pszidHidBlob);
}

char* u0034_repo_treenode__create_treenode(SG_context* pCtx,
										   SG_repo * pRepo,
											 SG_pathname * pPathnameTempDir,
											 const char * szDollarPathPrefix,
											 int nrUserfiles,
											 int nrSubTreenodes,
											 int depthSubTreenodes)
{
	// create a treenode.
	// populate it with 'nrUserfiles' random files.
	// recursively populate it with 'nrSubTreenodes' sub-directories
	// (each having 'nrUserfiles' random files).
	// each sub-directory should have 'depthSubTreenodes' depth.
	//
	// add a treenode blob to the repo for treenode we create.
	//
	// return the HID of the treenode.
	// the caller must free this.

	int k;
	SG_treenode * pTreenode;
	char* pszidHidTreenode;
	SG_repo_tx_handle* pTx = NULL;

	SG_uint64 iBlobFullLength = 0;

	// create an empty treenode.

	VERIFY_ERR_CHECK_RETURN( SG_treenode__alloc(pCtx, &pTreenode)  );

	// this test was written assuming a TN_VERSION_1 version treenode.

	VERIFY_ERR_CHECK(  SG_treenode__set_version(pCtx, pTreenode,TN_VERSION_1)  );

	// add n userfiles to the treenode.

	for (k=0; k<nrUserfiles; k++)
	{
		u0034_repo_treenode__populate_treenode_with_userfile(pCtx, pRepo,pTreenode,pPathnameTempDir,szDollarPathPrefix);
	}

	if (depthSubTreenodes > 0)
	{
		// add n sub-directories to the treenode.

		for (k=0; k<nrSubTreenodes; k++)
		{
			char* pszidHidSubTreenode;
			char bufDirname[1024];
			char bufDollarPath[2048];

			SG_sprintf(pCtx, bufDirname,1024,"Dir%05d",g_u0034_repo_treenode__kDir++);
			SG_sprintf(pCtx, bufDollarPath,SG_NrElements(bufDollarPath),"%s/%s",szDollarPathPrefix,bufDirname);

			pszidHidSubTreenode = u0034_repo_treenode__create_treenode(pCtx, pRepo,pPathnameTempDir,
																   bufDollarPath,
																   nrUserfiles,nrSubTreenodes,depthSubTreenodes-1);
			VERIFY_COND("create_treenode",(pszidHidSubTreenode));
			if (pszidHidSubTreenode)
			{
				// generate a random directory name for putting this sub-treenode into the current treenode.

				u0034_repo_treenode__add_treenode_entry_to_treenode(pCtx, pTreenode,
																	SG_TREENODEENTRY_TYPE_DIRECTORY,
																	pszidHidSubTreenode,
																	bufDirname,
																	bufDollarPath);
				SG_NULLFREE(pCtx, pszidHidSubTreenode);
			}
		}
	}

	VERIFY_ERR_CHECK_DISCARD(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK( SG_treenode__save_to_repo(pCtx, pTreenode,pRepo,pTx,&iBlobFullLength)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );

	VERIFY_ERR_CHECK(  SG_treenode__get_id(pCtx, pTreenode,&pszidHidTreenode)  );

	SG_TREENODE_NULLFREE(pCtx, pTreenode);

	return pszidHidTreenode;

fail:
	return NULL;
}


//////////////////////////////////////////////////////////////////

int u0034_repo_treenode__verify_bogus_treenode_not_found(SG_context* pCtx,SG_repo * pRepo)
{
	// try to lookup a non-existent treenode/blob and verify that
	// we get the expected error code -- SG_ERR_BLOB_NOT_FOUND.
	//
	// i made the blob_open code catch ENOENT and ERROR_FILE_NOT_FOUND,
	// but i want to make sure that we don't get other things too.
	//
	// also, if we ever move the blobs into a SQL DB, this we'd want
	// the blob-sql layer to hide that fact.

	SG_treenode * pTreenodeFoo;
	char bufGidFoo[SG_GID_BUFFER_LENGTH];
	char * pszHidFoo = NULL;

	// create a random GID to use as fodder for the hash to get a real HID.
	// this HID should not exist in the repo.

	VERIFY_ERR_CHECK_RETURN(  SG_gid__generate(pCtx, bufGidFoo, sizeof(bufGidFoo))  );
	VERIFY_ERR_CHECK_RETURN(  SG_repo__alloc_compute_hash__from_bytes(pCtx,
																	  pRepo,
																	  sizeof(bufGidFoo),
																	  (SG_byte *)bufGidFoo,
																	  &pszHidFoo)  );

	VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(SG_treenode__load_from_repo(pCtx, pRepo,pszHidFoo,&pTreenodeFoo),
										SG_ERR_BLOB_NOT_FOUND);

	SG_NULLFREE(pCtx, pszHidFoo);

	return 1;
}

//////////////////////////////////////////////////////////////////


int u0034_repo_treenode__run(SG_context* pCtx)
{
	// run the main test.

	SG_repo * pRepo;
	SG_pathname * pPathnameTempDir;
	char* pszidHidTreenodeRoot;

	VERIFY_ERR_CHECK_RETURN(  SG_VHASH__ALLOC(pCtx, &gpVHashListOfEntries)  );

	// create a new repo.

	pRepo = u0034_repo_treenode__open_repo(pCtx);
	pPathnameTempDir = u0034_repo_treenode__create_tmp_src_dir(pCtx);

	// verify that non-existent HIDs give us an error.

	u0034_repo_treenode__verify_bogus_treenode_not_found(pCtx, pRepo);

	// create a small tree of files and sub-directories.

	pszidHidTreenodeRoot = u0034_repo_treenode__create_treenode(pCtx, pRepo,pPathnameTempDir,"$",5,3,3);
	if (pszidHidTreenodeRoot)
	{
		char* pszidGidObjectRoot = NULL;

		INFOP("run",("Root Treenode is [%s]",(pszidHidTreenodeRoot)));

		// since root treenode is not contained within a user directory, we fake a little
		// data for it in our list of entries.

		VERIFY_ERR_CHECK_RETURN(  SG_gid__alloc(pCtx, &pszidGidObjectRoot)  );
		u0034_repo_treenode__add_entry_to_list(pCtx, "$",pszidHidTreenodeRoot,pszidGidObjectRoot);
		SG_NULLFREE(pCtx, pszidGidObjectRoot);

		SG_NULLFREE(pCtx, pszidHidTreenodeRoot);
	}

	SG_PATHNAME_NULLFREE(pCtx, pPathnameTempDir);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_VHASH_NULLFREE(pCtx, gpVHashListOfEntries);

	return 1;
}



TEST_MAIN(u0034_repo_treenode)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0034_repo_treenode__run(pCtx)  );

	TEMPLATE_MAIN_END;
}
