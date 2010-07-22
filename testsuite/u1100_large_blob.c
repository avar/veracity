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
 * @file u1100_large_blob.c
 *
 * @details A test to exercise the storage and retrieval of
 * a very large (4gig+several bytes) blob.  The test will be run
 * both with compression and without.
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"

//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN_WITH_ARGS(u1100_large_blob)
#define MyDcl(name)				u1100_large_blob__##name
#define MyFn(name)				u1100_large_blob__##name

#define MyFourGig				(1024LL*1024*1024*4)	// win build fails with constant overflow without LL

//////////////////////////////////////////////////////////////////

void MyFn(create_repo)(SG_context * pCtx, SG_repo ** ppRepo)
{
	// caller must free returned value.

	SG_repo * pRepo;
	SG_pathname * pPathnameRepoDir = NULL;
	SG_vhash* pvhPartialDescriptor = NULL;
    char buf_repo_id[SG_GID_BUFFER_LENGTH];
    char buf_admin_id[SG_GID_BUFFER_LENGTH];
	char* pszRepoImpl = NULL;

	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_repo_id, sizeof(buf_repo_id))  );
	VERIFY_ERR_CHECK(  SG_gid__generate(pCtx, buf_admin_id, sizeof(buf_admin_id))  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathnameRepoDir)  );
	VERIFY_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathnameRepoDir)  );

	VERIFY_ERR_CHECK(  SG_VHASH__ALLOC(pCtx, &pvhPartialDescriptor)  );

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

	*ppRepo = pRepo;

fail:
	SG_VHASH_NULLFREE(pCtx, pvhPartialDescriptor);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameRepoDir);

    SG_NULLFREE(pCtx, pszRepoImpl);
}

void MyFn(create_tmp_src_dir)(SG_context * pCtx, SG_pathname * ppPathnameTempDir)
{
	// create a temp directory in the current directory to be the
	// home of some userfiles, if the directory does not already exist.
	// caller must free returned value.

	SG_bool bExists;
	SG_fsobj_type FsObjType;
	SG_fsobj_perms FsObjPerms;

	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, ppPathnameTempDir, &bExists, &FsObjType, &FsObjPerms)  );
	if (!bExists)
		VERIFY_ERR_CHECK(  SG_fsobj__mkdir_recursive__pathname(pCtx, ppPathnameTempDir)  );

	INFOP("mktmpdir",("Temp Src Dir is [%s]",SG_pathname__sz(ppPathnameTempDir)));

	return;

fail:
	return;
}

//////////////////////////////////////////////////////////////////

void MyFn(create_blob_from_file)(SG_context * pCtx,
								 SG_repo * pRepo,
								 const SG_pathname * pPathnameTempDir,
								 SG_bool b_dont_bother_to_compress,
								 SG_uint64 lenFile)
{
	// create a file of length "lenFile" in the temp directory.
	// use it to create a blob.
	// try to create it a second time and verify that we get an duplicate-hid error.

	// Originally, this test created the 4gig test file each time on the fly.
	// Since creating a 4gig file takes some time, we've decided that if a file "largeblob"
	// exists at the given location, we will not re-create the file, allowing the user
	// (i.e. the build system) to just pass in the same location each time and re-use the file.
	// If the file is not found, it will be created.

	char* pszLargeBlobFilename = "largeblob";
	char buf_tid_random2[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname * pPathnameTempFile1 = NULL;
	SG_pathname * pPathnameTempFile2 = NULL;
	SG_file * pFileTempFile1 = NULL;
	SG_file * pFileTempFile2 = NULL;
	SG_uint64 lenWritten;
	char* pszidHidBlob1 = NULL;
	char* pszidHidBlob1Dup = NULL;
	char* pszidHidBlob2 = NULL;
	char* pszidHidVerify1 = NULL;
	char* pszidHidVerify2 = NULL;
	SG_bool bEqual;
	SG_repo_tx_handle* pTx = NULL;
	SG_uint64 iBlobFullLength = 0;
	SG_bool bExists;
	SG_fsobj_type FsObjType;
	SG_fsobj_perms FsObjPerms;
    char buf[SG_TID_MAX_BUFFER_LENGTH + 10];
	SG_uint32 len_buf;

	//////////////////////////////////////////////////////////////////
	// create temp-file-1 of length "lenFile" in the temp directory.

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathnameTempFile1,pPathnameTempDir,(pszLargeBlobFilename))  );

	// check to see if the file exists before we call open__pathname with SG_FILE_OPEN_OR_CREATE,
	// otherwise we won't know whether we need to write data to it
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPathnameTempFile1, &bExists, &FsObjType, &FsObjPerms)  );

	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathnameTempFile1,SG_FILE_RDWR|SG_FILE_OPEN_OR_CREATE,0644,&pFileTempFile1)  );

	// write data to the file if we just created it
	if (!bExists)
	{
		// generate lots of data in the file so that we'll cause the
		// blob routines to exercise the chunking stuff.

		VERIFY_ERR_CHECK(  SG_tid__generate(pCtx, buf, sizeof(buf))  );
		VERIFY_ERR_CHECK(  SG_strcat(pCtx, buf, sizeof(buf), "\n")  );

		len_buf = strlen(buf);

		// TODO we're not seriously generating a >4Gb file by writing to the file ~64 bytes at a time?
		// TODO no wonder this test takes forever..............

		lenWritten = 0;
		while (lenWritten < lenFile)
		{
			VERIFY_ERR_CHECK(  SG_file__write(pCtx, pFileTempFile1, len_buf, (SG_byte *)&buf, NULL)  );

			lenWritten += len_buf;
		}

		SG_ERR_IGNORE(  SG_file__seek(pCtx, pFileTempFile1,0)  );
	}

	//////////////////////////////////////////////////////////////////
	// use currently open temp file to create a blob.
	// we get the HID back.  (we need to free it later.)

	VERIFY_ERR_CHECK(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
	VERIFY_ERR_CHECK(  SG_repo__store_blob_from_file(pCtx, pRepo,pTx,NULL,b_dont_bother_to_compress,pFileTempFile1,&pszidHidBlob1,&iBlobFullLength)  );
	VERIFY_ERR_CHECK(  SG_repo__commit_tx(pCtx, pRepo, &pTx)  );

	INFOP("create_blob_from_file",("Created blob [%s]",(pszidHidBlob1)));

	//////////////////////////////////////////////////////////////////
	// try to create blob again and verify we get an duplicate-hid error.

	// Ian TODO: Put this back when SG_ERR_BLOBFILEALREADYEXISTS has been replaced.
// 	VERIFY_ERR_CHECK_DISCARD(  SG_repo__begin_tx(pCtx, pRepo, &pTx)  );
// 	err = SG_repo__store_blob_from_file(pRepo,pTx,SG_FALSE,pFileTempFile1,&pszidHidBlob1Dup);
// 	VERIFYP_CTX_ERR_IS("create_blob_from_file(duplicate)", pCtx, SG_ERR_BLOBFILEALREADYEXISTS, ("Duplicate create failed [%s][%s]",pszidHidBlob1,pszidHidBlob1Dup));
// 	VERIFY_ERR_CHECK_DISCARD(  SG_repo__commit_tx(pCtx, pRepo, SG_DAGNUM__NONE, NULL, &pTx)  );

	//////////////////////////////////////////////////////////////////
	// create empty temp-file-2 and try to read the blob from the repo.

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, buf_tid_random2, sizeof(buf_tid_random2), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathnameTempFile2,pPathnameTempDir,buf_tid_random2)  );

	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathnameTempFile2,SG_FILE_RDWR|SG_FILE_CREATE_NEW,0644,&pFileTempFile2)  );

	VERIFY_ERR_CHECK(  SG_repo__fetch_blob_into_file(pCtx, pRepo,pszidHidBlob1,pFileTempFile2,NULL)  );

	//////////////////////////////////////////////////////////////////
	// verify that the contents of temp-file-2 is identical to the
	// contents of temp-file-1.  (we already know that the HIDs match
	// and was verified during the fetch, but there are times when the
	// HID is just being used as a key -- it doesn't mean that what we
	// actually restored is correct.

	VERIFY_ERR_CHECK(  SG_repo__alloc_compute_hash__from_file(pCtx, pRepo, pFileTempFile1, &pszidHidVerify1)  );

	VERIFY_ERR_CHECK(  SG_repo__alloc_compute_hash__from_file(pCtx, pRepo, pFileTempFile2, &pszidHidVerify2)  );

	bEqual = (0 == (strcmp(pszidHidVerify1,pszidHidVerify2)));
	VERIFY_COND("create_blob_from_file(verify v1==v2)",bEqual);

	bEqual = (0 == (strcmp(pszidHidVerify1,pszidHidBlob1)));
	VERIFY_COND("create_blob_from_file(verify v1==id)",bEqual);

	//////////////////////////////////////////////////////////////////
	// TODO delete temp source file

	SG_ERR_IGNORE(  SG_file__close(pCtx, &pFileTempFile1)  );
	SG_ERR_IGNORE(  SG_file__close(pCtx, &pFileTempFile2)  );

	// do not remove temp file 1, it is left behind so that it can be reused in future test runs

	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPathnameTempFile2)  );

	//////////////////////////////////////////////////////////////////
	// cleanup

fail:
	SG_NULLFREE(pCtx, pszidHidBlob1);
	SG_NULLFREE(pCtx, pszidHidBlob1Dup);
	SG_NULLFREE(pCtx, pszidHidBlob2);
	SG_NULLFREE(pCtx, pszidHidVerify1);
	SG_NULLFREE(pCtx, pszidHidVerify2);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameTempFile1);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameTempFile2);
}

void MyFn(create_some_large_blobs_from_files)(SG_context * pCtx,
											  SG_repo * pRepo,
											  const char* pszPath)
{
	char * szRepoId;
	SG_pathname * pPathnameTempDir = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathnameTempDir, pszPath)  );

	VERIFY_ERR_CHECK(  MyFn(create_tmp_src_dir)(pCtx,pPathnameTempDir)  );

    VERIFY_ERR_CHECK_DISCARD(  SG_repo__get_repo_id(pCtx, pRepo, &szRepoId)  );

	INFOP("create_some_large_blobs_from_files",("RepoID[%s] TempDir[%s] ",
										  szRepoId,
										  SG_pathname__sz(pPathnameTempDir)
										  ));
    SG_NULLFREE(pCtx, szRepoId);

	VERIFY_ERR_CHECK(  MyFn(create_blob_from_file)(pCtx, pRepo,pPathnameTempDir, SG_TRUE,(SG_uint64)MyFourGig+5)  );

	// we don't delete pPathnameTempDir here so that it may be reused, to bypass creating the large file every time

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathnameTempDir);
}

//////////////////////////////////////////////////////////////////

MyMain()
{
    SG_getopt * pGetopt = NULL;

	SG_repo * pRepo = NULL;

	TEMPLATE_MAIN_START;

    SG_ERR_CHECK(  SG_getopt__alloc(pCtx, argc, argv, &pGetopt)  );

	VERIFY_COND("path of large blob directory missing", (pGetopt->count_args >= 2));

    if (pGetopt->count_args < 2)
    {
        fprintf(stderr, "\nERROR: You must pass the path of the large blob directory as the first argument.\n\n");
    }
    else if (pGetopt->count_args == 2)
    {
		VERIFY_ERR_CHECK(  MyFn(create_repo)(pCtx,&pRepo)  );

		// usage: u1100_large_blob <large_blob_directory>
		BEGIN_TEST(  MyFn(create_some_large_blobs_from_files)(pCtx, pRepo, pGetopt->paszArgs[1])  );
    }

	// fall-thru to common cleanup

fail:

    SG_GETOPT_NULLFREE(pCtx, pGetopt);

	SG_REPO_NULLFREE(pCtx, pRepo);

	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn

