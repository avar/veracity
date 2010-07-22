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
 * @file sg_logfile.c
 */

#include <sg.h>
#include "sg_logfile_typedefs.h"
#include "sg_logfile_prototypes.h"

//////////////////////////////////////////////////////////////////

struct _sg_logfile
{
	SG_pathname * pDirectoryPath;
	SG_pathname * pFilePath;
	SG_file * pFile;
	SG_time fileCreationTime; // Hack. Actually the mod time, but since we're rolling over daily for now, should be on the same day.
};

#define mpDirectoryPath pThis->pDirectoryPath
#define mpFilePath pThis->pFilePath
#define mpFile pThis->pFile
#define mFileCreationTime pThis->fileCreationTime

#define CHECK_VALID_THIS_POINTER \
	SG_NULLARGCHECK_RETURN(pThis);\
	SG_ARGCHECK_RETURN( mpDirectoryPath != NULL , pThis );\
	SG_ARGCHECK_RETURN( mpFilePath != NULL , pThis )

#define ASSERT_VALID_THIS_POINTER \
	SG_ASSERT(pThis!=NULL);\
	SG_ASSERT( mpDirectoryPath != NULL );\
	SG_ASSERT( mpFilePath != NULL )

//////////////////////////////////////////////////////////////////

void _sg_logfile__open_file(SG_context * pCtx, SG_logfile * pThis)
{
	SG_file * pFile = NULL;
	SG_uint64 fileEndPosition = 0;
	const SG_byte UTF8_BOM[3] = {0xEF, 0xBB, 0xBF};

	ASSERT_VALID_THIS_POINTER;

	SG_ASSERT(mpFile==NULL);

	SG_ERR_CHECK(  SG_file__open__pathname(pCtx, mpFilePath, SG_FILE_OPEN_OR_CREATE|SG_FILE_WRONLY, 0777, &pFile)  );
	SG_ERR_CHECK(  SG_file__seek_end(pCtx, pFile, &fileEndPosition)  );
	if( fileEndPosition == 0 )
		SG_ERR_CHECK(  SG_file__write(pCtx, pFile, 3, UTF8_BOM, NULL)  );

	mpFile = pFile;

	return;
fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
}

// TODO REVIEW: Jeff says: SG_time is a structure.  We shouldn't be passing it by *value*
//                         to this function.  We should pass "SG_int64 iTime" instead.
void _sg_logfile__make_sure_file_is_open_and_roll_over_if_needed(SG_context * pCtx, SG_logfile * pThis, SG_time currentTime)
{
	SG_bool fileAlreadyExists = SG_FALSE;
	SG_fsobj_stat stat;
	SG_string * pArchivedFileFilename = NULL;
	SG_pathname * pArchivedFilePath = NULL;

	ASSERT_VALID_THIS_POINTER;

	if( mFileCreationTime.year==0 )
	{
		SG_ERR_CHECK(  SG_fsobj__exists_stat__pathname(pCtx, mpFilePath, &fileAlreadyExists, &stat)  );
		if( fileAlreadyExists )
			SG_ERR_CHECK(  SG_time__decode__local(pCtx, stat.mtime_ms,&mFileCreationTime)  );
	}
	else
	{
		fileAlreadyExists = SG_TRUE;
	}

	if( !fileAlreadyExists )
	{
		// Case 1: file doesn't exist. Just create it.

		SG_ERR_CHECK(  _sg_logfile__open_file(pCtx, pThis)  );
		SG_memcpy2(currentTime, mFileCreationTime);
	}
	else if( mFileCreationTime.year==currentTime.year && mFileCreationTime.month==currentTime.month && mFileCreationTime.mday==currentTime.mday )
	{
		// Case 2: file exists and doesn't need a roll-over.

		if( mpFile == NULL )
			SG_ERR_CHECK(  _sg_logfile__open_file(pCtx, pThis)  ); // Just because we know the creation date doesn't mean the file is already open :-)
	}
	else
	{
		// Case 3: Log file exists and needs rolled over...

		if (mpFile != NULL)
			SG_ERR_CHECK(  SG_file__close(pCtx, &mpFile)  );
		SG_ERR_CHECK(  SG_string__alloc__format(pCtx, &pArchivedFileFilename, "sg-%d-%02d-%02d.log", mFileCreationTime.year, mFileCreationTime.month, mFileCreationTime.mday)  );
		SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pArchivedFilePath, mpDirectoryPath, SG_string__sz(pArchivedFileFilename))  );
		SG_ERR_CHECK(  SG_fsobj__move__pathname_pathname(pCtx, mpFilePath,pArchivedFilePath)  );
// REVIEW: Jeff says: Note that there is an implicit race here.  We just renamed the log file
//                    and are about to start a new one.  Another process could have come thru
//                    here and created the file for us.  The create time here is close enough
//                    to not matter -- as long as we don't do anything to assume that we actually
//                    did the create on disk.  Right?
//               PWE: We didn't used to be doing anything assuming we created the log file...
//                    until we decided to put the UTF8-BOM at the beginning of the file. Now what
//                    might happen--we get a code point U+FEFF "zero-width non-breaking space"
//                    somewhere in the middle of the file? If we actually do want to avoid this,
//                    what can be done about it? If more than one process can have access to the
//                    file at the same time, how will we ever know if bytes have already been
//                    written to the file between when we seek_end and when we write the BOM?
//
// REVIEW: Jeff says: It looks like SG_logger keeps the log file open for the duration of the
//                    process (right?).  This is probably OK for a short-lived command, but if
//                    we are a long-lived process and there are 2 processes doing something,
//                    then one might be writing to the rolled-out logfile (using the FD/FILE
//                    POINTER/HANDLE) long after one of them does the roll-over.  Right?
//                    Alternatively, if they are both long-lived, couldn't they both do a
//                    roll-over and then we'd have some confusion.  Right?
//               PWE: To me, writing to a rolled-out log file doesn't sound like a problem. It's
//                    just writing queued output from the previous day anyway, right?
//                    As for simultaneous roll-over "confusion", i think the worst that can happen
//                    is that fsobj__rename fails for one of the processes, and the message that
//                    it was about to print gets omitted from either log file (since we don't
//                    attempt a retry). I'd again like to see this fixed and working cleanly but am
//                    not sure how to fix it, being a bit fuzzy on the details of what tools we
//                    might have available in (cross-platform) file i/o.
		SG_ERR_CHECK(  _sg_logfile__open_file(pCtx, pThis)  );
		SG_memcpy2(currentTime, mFileCreationTime);

		SG_STRING_NULLFREE(pCtx, pArchivedFileFilename);
		SG_PATHNAME_NULLFREE(pCtx, pArchivedFilePath);
	}

	return;
fail:
	SG_STRING_NULLFREE(pCtx, pArchivedFileFilename);
	SG_PATHNAME_NULLFREE(pCtx, pArchivedFilePath);
}

//////////////////////////////////////////////////////////////////

void SG_logfile__alloc(SG_context * pCtx, SG_pathname** ppPath, SG_logfile ** ppResult)
{
    SG_logfile * pThis = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(ppResult);
    SG_NULLARGCHECK_RETURN(ppPath);
    SG_NULLARGCHECK_RETURN(*ppPath);

    SG_ERR_CHECK(  SG_alloc1(pCtx, pThis)  );
    pThis->pDirectoryPath = *ppPath;
    *ppPath = NULL;
	SG_ERR_CHECK(  SG_pathname__add_final_slash(pCtx, pThis->pDirectoryPath)  );
	SG_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &(pThis->pFilePath), pThis->pDirectoryPath, "sg.log")  );

    *ppResult = pThis;

    return;
fail:
    SG_ERR_IGNORE(  SG_logfile__nullfree(pCtx, &pThis)  );
}

void SG_logfile__nullfree(SG_context * pCtx, SG_logfile ** ppThis)
{
	if (ppThis==NULL)
		return;
	if (*ppThis==NULL)
		return;

	SG_PATHNAME_NULLFREE(pCtx, (*ppThis)->pDirectoryPath);
	SG_PATHNAME_NULLFREE(pCtx, (*ppThis)->pFilePath);
	SG_FILE_NULLCLOSE(pCtx, (*ppThis)->pFile);

	SG_free(pCtx, *ppThis);
	*ppThis = NULL;
}

void SG_logfile__write(SG_context * pCtx, SG_logfile * pThis, const char * sz)
{
	SG_time time;
	SG_ERR_CHECK_RETURN(  SG_time__local_time(pCtx, &time)  );
	SG_ERR_CHECK_RETURN(  SG_logfile__write__time(pCtx, pThis, sz, time)  );
}

// TODO REVIEW: Jeff says: SG_time is a structure.  We shouldn't be passing it by *value*
//                         to this function.  We should pass "SG_int64 iTime" instead.
//                         See Jira:SPRAWL-79.
void SG_logfile__write__time(SG_context * pCtx, SG_logfile * pThis, const char * sz, SG_time time)
{
	CHECK_VALID_THIS_POINTER;

	SG_ERR_CHECK(  _sg_logfile__make_sure_file_is_open_and_roll_over_if_needed(pCtx, pThis, time)  );
	SG_ERR_CHECK(  SG_file__write__sz(pCtx, mpFile, sz)  );

	return;
fail:
	return;
}

//////////////////////////////////////////////////////////////////
