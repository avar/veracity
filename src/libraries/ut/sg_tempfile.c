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

// sg_tempfile.c
// cross-platform temp file manipulation
//////////////////////////////////////////////////////////////////

#include <sg.h>

//////////////////////////////////////////////////////////////////
void SG_tempfile__open_new(SG_context* pCtx, SG_tempfile** ppHandle)
{
	SG_pathname* pPathTemp = NULL;
	char buf_tid[SG_TID_MAX_BUFFER_LENGTH];
	SG_file* pFile = NULL;
	SG_tempfile* pHandle = NULL;

	SG_ERR_CHECK(  SG_tid__generate2(pCtx, buf_tid, sizeof(buf_tid), 32)  );

	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__USER_TEMP_DIRECTORY(pCtx, &pPathTemp)  );
	SG_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPathTemp, buf_tid)  );

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathTemp, SG_FILE_RDWR | SG_FILE_CREATE_NEW, SG_FSOBJ_PERMS__MASK, &pFile) );

	SG_ERR_CHECK(  SG_alloc1(pCtx, pHandle)  );

	pHandle->pFile = pFile;
	pHandle->pPathname = pPathTemp;

	*ppHandle = pHandle;

	return;

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTemp);
}

void SG_tempfile__close(SG_context* pCtx, SG_tempfile* pHandle)
{
	SG_NULLARGCHECK_RETURN(pHandle);

	SG_ERR_CHECK_RETURN(  SG_file__close(pCtx, &pHandle->pFile)  );
}

void SG_tempfile__delete(SG_context* pCtx, SG_tempfile** ppHandle)
{
	if (!ppHandle || !(*ppHandle))
		return;

	SG_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, (*ppHandle)->pPathname)  );

	// Fall through to common cleanup.

fail:
	SG_PATHNAME_NULLFREE(pCtx, (*ppHandle)->pPathname);
	SG_NULLFREE(pCtx, *ppHandle);
}

void SG_tempfile__close_and_delete(SG_context* pCtx, SG_tempfile** ppHandle)
{
	if (ppHandle && *ppHandle) // so we handle nulls in error-handling routines nicely.
		SG_ERR_IGNORE(  SG_tempfile__close(pCtx, *ppHandle)  );

	SG_tempfile__delete(pCtx, ppHandle);
}
