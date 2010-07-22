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

// testsuite/unittests/u0040_unc.c
// test UNC pathnames on Windows.
//////////////////////////////////////////////////////////////////

#include <sg.h>

#include "unittests.h"

//////////////////////////////////////////////////////////////////

int u0040_unc__stat_dir(SG_context * pCtx, const char * szDir)
{
	SG_pathname * pPathname = NULL;
	SG_pathname * pPathnameFile = NULL;
	SG_file * pf = NULL;
	SG_fsobj_stat fsobjStat;
	SG_bool bFileExists;
	SG_int_to_string_buffer bufSize;
	char bufDate[100];

	SG_context__err_reset(pCtx);

	//////////////////////////////////////////////////////////////////
	// stat the given directory.
	//////////////////////////////////////////////////////////////////

	INFOP("u0040_unc",("Inspecting [%s]",szDir));

	VERIFY_ERR_CHECK_RETURN(  SG_PATHNAME__ALLOC__SZ(pCtx,&pPathname,szDir)  );

	VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx,pPathname,&fsobjStat)  );
	VERIFY_COND("u0040_unc",(fsobjStat.type == SG_FSOBJ_TYPE__DIRECTORY));

	// TODO should we verify length == 0 ?
	// TODO should we verify modtime ?

	SG_uint64_to_sz(fsobjStat.size, bufSize);
	VERIFY_ERR_CHECK_DISCARD(  SG_time__format_utc__i64(pCtx,fsobjStat.mtime_ms,bufDate,SG_NrElements(bufDate))  );

	INFOP("u0040_unc",("Result: [perms %04o][type %d][size %s][mtime %s]",
					   fsobjStat.perms,fsobjStat.type,
					   bufSize,bufDate));

	//////////////////////////////////////////////////////////////////
	// create a unique file in the directory and stat it.
	//////////////////////////////////////////////////////////////////

	VERIFY_ERR_CHECK(  unittest__alloc_unique_pathname(pCtx,szDir,&pPathnameFile)  );

	INFOP("u0040_unc",("    Creating file [%s]",SG_pathname__sz(pPathnameFile)));

	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx,pPathnameFile,SG_FILE_CREATE_NEW | SG_FILE_RDWR,0777,&pf)  );

	VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx,pPathnameFile,&fsobjStat)  );
	VERIFY_COND("u0040_unc",(fsobjStat.type == SG_FSOBJ_TYPE__REGULAR));
	VERIFY_COND("u0040_unc",(fsobjStat.size == 0));
	VERIFY_COND("u0040_unc",(SG_fsobj__equivalent_perms(fsobjStat.perms,0777)));
	// TODO should we verify modtime ?

	SG_uint64_to_sz(fsobjStat.size, bufSize);
	VERIFY_ERR_CHECK_DISCARD(  SG_time__format_utc__i64(pCtx,fsobjStat.mtime_ms,bufDate,SG_NrElements(bufDate))  );

	INFOP("u0040_unc",("    Result: [perms %04o][type %d][size %s][mtime %s]",
					   fsobjStat.perms,fsobjStat.type,
					   bufSize,bufDate));

	VERIFY_ERR_CHECK_DISCARD(  SG_file__close(pCtx, &pf)  );

	// delete the file and stat it again

	VERIFY_ERR_CHECK_DISCARD(  SG_fsobj__remove__pathname(pCtx,pPathnameFile)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_fsobj__exists__pathname(pCtx,pPathnameFile,&bFileExists,NULL,NULL)  );
	VERIFY_COND("u0040_unc",(!bFileExists));

	//////////////////////////////////////////////////////////////////
	// clean up
	//////////////////////////////////////////////////////////////////

	SG_PATHNAME_NULLFREE(pCtx, pPathnameFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathname);
	return 1;

fail:
	SG_FILE_NULLCLOSE(pCtx, pf);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathname);
	return 0;
}

//////////////////////////////////////////////////////////////////

TEST_MAIN(u0040_unc)
{
	TEMPLATE_MAIN_START;

#if defined(WINDOWS)
//	BEGIN_TEST(  u0040_unc__stat_dir(pCtx, "c:/")  ); // builder will need special permission accommodations if we want to write to c:/ - Jeff to review
	BEGIN_TEST(  u0040_unc__stat_dir(pCtx, "c:/sgneeds")  );
	BEGIN_TEST(  u0040_unc__stat_dir(pCtx, "c:/sgneeds/")  );
	//TODO:  Jeremy commented these out, since we know that //kilmister will only be accessible in 
	// the SourceGear network.
	//BEGIN_TEST(  u0040_unc__stat_dir(pCtx, "//kilmister/tmp")  );
	//BEGIN_TEST(  u0040_unc__stat_dir(pCtx, "//kilmister/tmp/")  );
	//BEGIN_TEST(  u0040_unc__stat_dir(pCtx, "//kilmister/tmp/sprawl")  );
	//BEGIN_TEST(  u0040_unc__stat_dir(pCtx, "//kilmister/tmp/sprawl/")  );
	
//	BEGIN_TEST(  u0040_unc__test_mkdir_recursive(pCtx, "c:/temp",3)  );
//	BEGIN_TEST(  u0040_unc__test_mkdir_recursive(pCtx, "c:/temp",5)  );	// this generates 332 character pathname; this should fail without UNC stuff.
#else
	SG_UNUSED(pCtx);
#endif

	TEMPLATE_MAIN_END;
}
