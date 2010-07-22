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

// u0015_dir.c
// test routines to read a directory.
//////////////////////////////////////////////////////////////////

#include <sg.h>

#include "unittests.h"

//////////////////////////////////////////////////////////////////

void u0015_dir__print_file_info(const SG_string * pStringFileName, SG_fsobj_stat * pFsStat)
{
	switch (pFsStat->type)
	{
	case SG_FSOBJ_TYPE__REGULAR:
		INFOP("u0015_dir:file:",("name[%s] perms[%04o] type[%d] size[%lld]",
								 SG_string__sz(pStringFileName),
								 pFsStat->perms, pFsStat->type, pFsStat->size));
		return;

	case SG_FSOBJ_TYPE__SYMLINK:
		INFOP("u0015_dir:symlink:",("name[%s] perms[%04o] type[%d] size[%lld]",
									SG_string__sz(pStringFileName),
									pFsStat->perms, pFsStat->type, pFsStat->size));
		return;

	case SG_FSOBJ_TYPE__DIRECTORY:
		INFOP("u0015_dir:directory:",("name[%s] perms[%04o] type[%d]",
									  SG_string__sz(pStringFileName),
									  pFsStat->perms, pFsStat->type));
		return;

	default:
		INFOP("u0015_dir:???:",("name[%s] perms[%04o] type[%d]",
								SG_string__sz(pStringFileName),
								pFsStat->perms, pFsStat->type));
		return;
	}
}

//////////////////////////////////////////////////////////////////

void u0015_dir__readdir(SG_context * pCtx)
{
	SG_pathname * pPathnameDirectory = NULL;
	SG_string * pStringFileName = NULL;
	SG_fsobj_stat fsStat;
	SG_dir * pDir = NULL;
	SG_error errReadStat;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathnameDirectory)  );
	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStringFileName)  );

	VERIFY_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathnameDirectory)  );
	INFOP("dir__opendir",("SG_dir__open[%s]:",SG_pathname__sz(pPathnameDirectory)));

	VERIFY_ERR_CHECK_DISCARD(  SG_dir__open(pCtx,
											pPathnameDirectory,
											&pDir,
											&errReadStat,
											pStringFileName,
											&fsStat)  );
	VERIFYP_COND("dir__readdir",(SG_IS_OK(errReadStat)),("SG_dir__read[%s]",SG_pathname__sz(pPathnameDirectory)));

	do
	{
		u0015_dir__print_file_info(pStringFileName,&fsStat);

		SG_dir__read(pCtx, pDir,pStringFileName,&fsStat);
		SG_context__get_err(pCtx,&errReadStat);

	} while (SG_IS_OK(errReadStat));

	VERIFY_CTX_ERR_EQUALS("dir_readdir",pCtx,SG_ERR_NOMOREFILES);
	SG_context__err_reset(pCtx);

fail:
	SG_DIR_NULLCLOSE(pCtx, pDir);
	SG_STRING_NULLFREE(pCtx, pStringFileName);
	SG_PATHNAME_NULLFREE(pCtx, pPathnameDirectory);
}

//////////////////////////////////////////////////////////////////

static SG_dir_foreach_callback u0015_dir__foreach_cb;

static void u0015_dir__foreach_cb(SG_UNUSED_PARAM( SG_context * pCtx ),
								  const SG_string * pStringEntryName,
								  SG_fsobj_stat * pfsStat,
								  SG_UNUSED_PARAM( void * pVoidData ))
{
	SG_UNUSED(pCtx);
	SG_UNUSED(pVoidData);

	u0015_dir__print_file_info(pStringEntryName,pfsStat);
}

void u0015_dir__readdir_using_foreach(SG_context * pCtx)
{
	SG_pathname * pPathnameDirectory = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPathnameDirectory)  );
	VERIFY_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPathnameDirectory)  );
	INFOP("readdir_using_foreach",("SG_dir__foreach[%s]:",SG_pathname__sz(pPathnameDirectory)));

	VERIFY_ERR_CHECK(  SG_dir__foreach(pCtx,pPathnameDirectory,SG_TRUE,u0015_dir__foreach_cb,NULL)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathnameDirectory);
}

//////////////////////////////////////////////////////////////////

TEST_MAIN(u0015_dir)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0015_dir__readdir(pCtx)  );
	BEGIN_TEST(  u0015_dir__readdir_using_foreach(pCtx)  );

	TEMPLATE_MAIN_END;
}
