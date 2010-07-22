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
#include "unittests.h"

void _u0078__write_file(SG_context *pCtx, const char * sz, SG_pathname * pFilePath)
{
    SG_file * pFile = NULL;

    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pFilePath, SG_FILE_OPEN_OR_CREATE|SG_FILE_WRONLY, 0666, &pFile)  );
    SG_ERR_CHECK(  SG_file__write__sz(pCtx, pFile, sz)  );
    SG_ERR_CHECK(  SG_file__truncate(pCtx, pFile)  );
    SG_FILE_NULLCLOSE(pCtx, pFile);

    return;
fail:
    SG_FILE_NULLCLOSE(pCtx, pFile);
}

void _u0078__file_diff_sanity_check(SG_context *pCtx)
{
    // Silly example text from wikipedia.

    const char * const DIFF_FROM =
        "This part of the\n"
        "document has stayed the\n"
        "same from version to\n"
        "version.  It shouldn't\n"
        "be shown if it doesn't\n"
        "change.  Otherwise, that\n"
        "would not be helping to\n"
        "compress the size of the\n"
        "changes.\n"
        "\n"
        "This paragraph contains\n"
        "text that is outdated.\n"
        "It will be deleted in the\n"
        "near future.\n"
        "\n"
        "It is important to spell\n"
        "check this dokument. On\n"
        "the other hand, a\n"
        "misspelled word isn't\n"
        "the end of the world.\n"
        "Nothing in the rest of\n"
        "this paragraph needs to\n"
        "be changed. Things can\n"
        "be added after it.\n";

    const char * const DIFF_TO =
        "This is an important\n"
        "notice! It should\n"
        "therefore be located at\n"
        "the beginning of this\n"
        "document!\n"
        "\n"
        "This part of the\n"
        "document has stayed the\n"
        "same from version to\n"
        "version.  It shouldn't\n"
        "be shown if it doesn't\n"
        "change.  Otherwise, that\n"
        "would not be helping to\n"
        "compress anything.\n"
        "\n"
        "It is important to spell\n"
        "check this document. On\n"
        "the other hand, a\n"
        "misspelled word isn't\n"
        "the end of the world.\n"
        "Nothing in the rest of\n"
        "this paragraph needs to\n"
        "be changed. Things can\n"
        "be added after it.\n"
        "\n"
        "This paragraph contains\n"
        "important new additions\n"
        "to this document.\n";

    const char * const DIFF_EXPECTED_RESULT =
        "--- diff-from.txt" SG_PLATFORM_NATIVE_EOL_STR
        "+++ diff-to.txt" SG_PLATFORM_NATIVE_EOL_STR
        "@@ -1,3 +1,9 @@" SG_PLATFORM_NATIVE_EOL_STR
        "+This is an important" SG_PLATFORM_NATIVE_EOL_STR
        "+notice! It should" SG_PLATFORM_NATIVE_EOL_STR
        "+therefore be located at" SG_PLATFORM_NATIVE_EOL_STR
        "+the beginning of this" SG_PLATFORM_NATIVE_EOL_STR
        "+document!" SG_PLATFORM_NATIVE_EOL_STR
        "+" SG_PLATFORM_NATIVE_EOL_STR
        " This part of the" SG_PLATFORM_NATIVE_EOL_STR
        " document has stayed the" SG_PLATFORM_NATIVE_EOL_STR
        " same from version to" SG_PLATFORM_NATIVE_EOL_STR
        "@@ -5,16 +11,10 @@" SG_PLATFORM_NATIVE_EOL_STR
        " be shown if it doesn't" SG_PLATFORM_NATIVE_EOL_STR
        " change.  Otherwise, that" SG_PLATFORM_NATIVE_EOL_STR
        " would not be helping to" SG_PLATFORM_NATIVE_EOL_STR
        "-compress the size of the" SG_PLATFORM_NATIVE_EOL_STR
        "-changes." SG_PLATFORM_NATIVE_EOL_STR
        "-" SG_PLATFORM_NATIVE_EOL_STR
        "-This paragraph contains" SG_PLATFORM_NATIVE_EOL_STR
        "-text that is outdated." SG_PLATFORM_NATIVE_EOL_STR
        "-It will be deleted in the" SG_PLATFORM_NATIVE_EOL_STR
        "-near future." SG_PLATFORM_NATIVE_EOL_STR
        "+compress anything." SG_PLATFORM_NATIVE_EOL_STR
        " " SG_PLATFORM_NATIVE_EOL_STR
        " It is important to spell" SG_PLATFORM_NATIVE_EOL_STR
        "-check this dokument. On" SG_PLATFORM_NATIVE_EOL_STR
        "+check this document. On" SG_PLATFORM_NATIVE_EOL_STR
        " the other hand, a" SG_PLATFORM_NATIVE_EOL_STR
        " misspelled word isn't" SG_PLATFORM_NATIVE_EOL_STR
        " the end of the world." SG_PLATFORM_NATIVE_EOL_STR
        "@@ -22,3 +22,7 @@" SG_PLATFORM_NATIVE_EOL_STR
        " this paragraph needs to" SG_PLATFORM_NATIVE_EOL_STR
        " be changed. Things can" SG_PLATFORM_NATIVE_EOL_STR
        " be added after it." SG_PLATFORM_NATIVE_EOL_STR
        "+" SG_PLATFORM_NATIVE_EOL_STR
        "+This paragraph contains" SG_PLATFORM_NATIVE_EOL_STR
        "+important new additions" SG_PLATFORM_NATIVE_EOL_STR
        "+to this document." SG_PLATFORM_NATIVE_EOL_STR;

    SG_pathname * pDiffFromFilePath = NULL;
    SG_pathname * pDiffToFilePath = NULL;
    SG_pathname * pOutputFilePath = NULL;

    SG_diff_t * pDiff = NULL;
    SG_file * pOutputFile = NULL;

    SG_byte diff_expected_result[1024];
    SG_byte diff_result[1024];

    SG_bool foundChanges = SG_FALSE;

    SG_ASSERT(pCtx!=NULL);

    //
    // Set up files.
    //

    VERIFY_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pDiffFromFilePath, "u0078-diff-from.txt")  );
    VERIFY_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pDiffToFilePath, "u0078-diff-to.txt")  );
    VERIFY_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pOutputFilePath, "u0078-output.txt")  );

    VERIFY_ERR_CHECK(  _u0078__write_file(pCtx, DIFF_FROM, pDiffFromFilePath)  );
    VERIFY_ERR_CHECK(  _u0078__write_file(pCtx, DIFF_TO, pDiffToFilePath)  );

    VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx, pOutputFilePath, SG_FILE_OPEN_OR_CREATE|SG_FILE_RDWR, 0666, &pOutputFile)  );

    SG_zero(diff_expected_result);
    VERIFY_ERR_CHECK(  SG_strcpy(pCtx, (char*)diff_expected_result, sizeof(diff_expected_result), DIFF_EXPECTED_RESULT)  );

    //
    // Test raw functions in SG_diffcore_prototypes.h.
    //

    VERIFY_ERR_CHECK(  SG_diff_file(pCtx, pDiffFromFilePath, pDiffToFilePath, SG_DIFF_OPTION__ANY_EOL, &pDiff)  );

    VERIFY_COND("",  SG_diff_contains_diffs(pDiff)  );

    VERIFY_ERR_CHECK(  SG_file__seek(pCtx, pOutputFile, 0)  );
    VERIFY_ERR_CHECK(  SG_file__truncate(pCtx, pOutputFile)  );
    VERIFY_ERR_CHECK(  SG_diff_file_output_unified(
        pCtx, pDiff,
        pDiffFromFilePath, pDiffToFilePath, "diff-from.txt", "diff-to.txt",
        SG_DIFF_OPTION__ANY_EOL, 3, pOutputFile)  );

    SG_DIFF_NULLFREE(pCtx, pDiff);

    VERIFY_ERR_CHECK(  SG_file__truncate(pCtx, pOutputFile)  );

    SG_zero(diff_result);
    VERIFY_ERR_CHECK(  SG_file__seek(pCtx, pOutputFile, 0)  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_file__read(pCtx, pOutputFile, sizeof(diff_result)-1, diff_result, NULL), SG_ERR_INCOMPLETEREAD  );

    VERIFY_COND("",  0==strcmp((const char *)diff_result, (const char *)diff_expected_result)  );

    //
    // Now test the same diff as performed by SG_internalfilediff__unified (should be identical).
    //

    VERIFY_ERR_CHECK(  SG_file__seek(pCtx, pOutputFile, 0)  );
    VERIFY_ERR_CHECK(  SG_file__truncate(pCtx, pOutputFile)  );
    VERIFY_ERR_CHECK(  SG_internalfilediff__unified(pCtx,
        pDiffFromFilePath, pDiffToFilePath,
        "diff-from.txt", "diff-to.txt",
        pOutputFile, NULL,
        &foundChanges)  );
    VERIFY_COND("", foundChanges);

    VERIFY_ERR_CHECK(  SG_file__truncate(pCtx, pOutputFile)  );

    SG_zero(diff_result);
    VERIFY_ERR_CHECK(  SG_file__seek(pCtx, pOutputFile, 0)  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_file__read(pCtx, pOutputFile, sizeof(diff_result)-1, diff_result, NULL), SG_ERR_INCOMPLETEREAD  );

    VERIFY_COND("",  0==strcmp((const char *)diff_result, (const char *)diff_expected_result)  );

#if defined(LINUX) || defined(MAC)

    //
    // Test the same diff as performed by SG_diff_ext__gnu_diff (which calls gnu diff as an external diff utility).
    //

    VERIFY_ERR_CHECK(  SG_file__seek(pCtx, pOutputFile, 0)  );
    VERIFY_ERR_CHECK(  SG_file__truncate(pCtx, pOutputFile)  );
    VERIFY_ERR_CHECK(  SG_diff_ext__gnu_diff(pCtx,
        pDiffFromFilePath, pDiffToFilePath,
        "diff-from.txt", "diff-to.txt",
        pOutputFile, NULL,
        &foundChanges)  );
    VERIFY_COND("", foundChanges);

    VERIFY_ERR_CHECK(  SG_file__truncate(pCtx, pOutputFile)  );

    SG_zero(diff_result);
    VERIFY_ERR_CHECK(  SG_file__seek(pCtx, pOutputFile, 0)  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_file__read(pCtx, pOutputFile, sizeof(diff_result)-1, diff_result, NULL), SG_ERR_INCOMPLETEREAD  );

    VERIFY_COND("",  0==strcmp((const char *)diff_result, (const char *)diff_expected_result)  );

#endif

    //
    // Done.
    //

    SG_FILE_NULLCLOSE(pCtx, pOutputFile);

    SG_PATHNAME_NULLFREE(pCtx, pOutputFilePath);
    SG_PATHNAME_NULLFREE(pCtx, pDiffToFilePath);
    SG_PATHNAME_NULLFREE(pCtx, pDiffFromFilePath);
    return;
fail:
    SG_FILE_NULLCLOSE(pCtx, pOutputFile);
    SG_DIFF_NULLFREE(pCtx, pDiff);
    SG_PATHNAME_NULLFREE(pCtx, pOutputFilePath);
    SG_PATHNAME_NULLFREE(pCtx, pDiffToFilePath);
    SG_PATHNAME_NULLFREE(pCtx, pDiffFromFilePath);
}

// "diff3" aka "merge"
void _u0078__file_diff3_no_conflicts_sanity_check(SG_context *pCtx)
{
    const char * const ANCESTOR =
        "123 456" SG_PLATFORM_NATIVE_EOL_STR
        "789"     SG_PLATFORM_NATIVE_EOL_STR
        "abc def" SG_PLATFORM_NATIVE_EOL_STR
        "ghi jkl" SG_PLATFORM_NATIVE_EOL_STR
        "mno pqr" SG_PLATFORM_NATIVE_EOL_STR
        "stu vwx" SG_PLATFORM_NATIVE_EOL_STR
        "yz"      SG_PLATFORM_NATIVE_EOL_STR;

    const char * const THEIR_VERSION =
        "123 456" SG_PLATFORM_NATIVE_EOL_STR
        "789"     SG_PLATFORM_NATIVE_EOL_STR
        "abc def" SG_PLATFORM_NATIVE_EOL_STR
        "ghi jkl" SG_PLATFORM_NATIVE_EOL_STR
        "mno pqr" SG_PLATFORM_NATIVE_EOL_STR
        "yz"      SG_PLATFORM_NATIVE_EOL_STR;

    const char * const MY_VERSION =
        "123 456" SG_PLATFORM_NATIVE_EOL_STR
        "789 abc" SG_PLATFORM_NATIVE_EOL_STR
        "def ---" SG_PLATFORM_NATIVE_EOL_STR
        "abc def" SG_PLATFORM_NATIVE_EOL_STR
        "ghi jkl" SG_PLATFORM_NATIVE_EOL_STR
        "mno pqr" SG_PLATFORM_NATIVE_EOL_STR
        "stu vwx" SG_PLATFORM_NATIVE_EOL_STR
        "yz"      SG_PLATFORM_NATIVE_EOL_STR;

    const char * const EXPECTED_RESULT =
        "123 456" SG_PLATFORM_NATIVE_EOL_STR
        "789 abc" SG_PLATFORM_NATIVE_EOL_STR
        "def ---" SG_PLATFORM_NATIVE_EOL_STR
        "abc def" SG_PLATFORM_NATIVE_EOL_STR
        "ghi jkl" SG_PLATFORM_NATIVE_EOL_STR
        "mno pqr" SG_PLATFORM_NATIVE_EOL_STR
        "yz"      SG_PLATFORM_NATIVE_EOL_STR;

    SG_pathname * pAncestorPath = NULL;
    SG_pathname * pTheirFilePath = NULL;
    SG_pathname * pMyFilePath = NULL;
    SG_pathname * pOutputFilePath = NULL;

    SG_diff_t * pDiff = NULL;
    SG_file * pOutputFile = NULL;

    SG_byte expected_result[1024];
    SG_byte result[1024];

    SG_ASSERT(pCtx!=NULL);

    //
    // Set up files.
    //

    VERIFY_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pAncestorPath, "u0078-file-ancestor.txt")  );
    VERIFY_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pTheirFilePath, "u0078-file-theirs.txt")  );
    VERIFY_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pMyFilePath, "u0078-file-mine.txt")  );
    VERIFY_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pOutputFilePath, "u0078-file-merged.txt")  );

    VERIFY_ERR_CHECK(  _u0078__write_file(pCtx, ANCESTOR, pAncestorPath)  );
    VERIFY_ERR_CHECK(  _u0078__write_file(pCtx, THEIR_VERSION, pTheirFilePath)  );
    VERIFY_ERR_CHECK(  _u0078__write_file(pCtx, MY_VERSION, pMyFilePath)  );

    VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx, pOutputFilePath, SG_FILE_OPEN_OR_CREATE|SG_FILE_RDWR, 0666, &pOutputFile)  );

    SG_zero(expected_result);
    VERIFY_ERR_CHECK(  SG_strcpy(pCtx, (char*)expected_result, sizeof(expected_result), EXPECTED_RESULT)  );

    //
    // Test raw functions in SG_diffcore_prototypes.h.
    //

    VERIFY_ERR_CHECK(  SG_diff3_file(pCtx, pAncestorPath, pTheirFilePath, pMyFilePath, SG_DIFF_OPTION__REGULAR, &pDiff)  );

    VERIFY_COND("",  SG_diff_contains_diffs(pDiff)  );
    VERIFY_COND("",  !SG_diff_contains_conflicts(pDiff)  );

    VERIFY_ERR_CHECK(  SG_file__seek(pCtx, pOutputFile, 0)  );
    VERIFY_ERR_CHECK(  SG_file__truncate(pCtx, pOutputFile)  );
    VERIFY_ERR_CHECK(  SG_diff3_file_output(
        pCtx, pDiff,
        pAncestorPath, pTheirFilePath, pMyFilePath,
        "<---<CONFLICT-ORIGIN>--->", "<---<CONFLICT-MODIFIED>--->", "<---<CONFLICT-LATEST>--->", "<---<CONFLICT-SEPARATOR>--->",
        SG_TRUE, SG_TRUE, pOutputFile)  );

    SG_DIFF_NULLFREE(pCtx, pDiff);

    VERIFY_ERR_CHECK(  SG_file__truncate(pCtx, pOutputFile)  );

    SG_zero(result);
    VERIFY_ERR_CHECK(  SG_file__seek(pCtx, pOutputFile, 0)  );
    VERIFY_ERR_CHECK_ERR_EQUALS_DISCARD(  SG_file__read(pCtx, pOutputFile, sizeof(result)-1, result, NULL), SG_ERR_INCOMPLETEREAD  );

    VERIFY_COND("",  0==strcmp((const char *)result, (const char *)expected_result)  );

    //
    // Done.
    //

    SG_FILE_NULLCLOSE(pCtx, pOutputFile);

    SG_PATHNAME_NULLFREE(pCtx, pOutputFilePath);
    SG_PATHNAME_NULLFREE(pCtx, pMyFilePath);
    SG_PATHNAME_NULLFREE(pCtx, pTheirFilePath);
    SG_PATHNAME_NULLFREE(pCtx, pAncestorPath);
    return;
fail:
    SG_FILE_NULLCLOSE(pCtx, pOutputFile);
    SG_DIFF_NULLFREE(pCtx, pDiff);
    SG_PATHNAME_NULLFREE(pCtx, pOutputFilePath);
    SG_PATHNAME_NULLFREE(pCtx, pMyFilePath);
    SG_PATHNAME_NULLFREE(pCtx, pTheirFilePath);
    SG_PATHNAME_NULLFREE(pCtx, pAncestorPath);
}

// ----- _u0078__gnu_diff_compatibility_test_suite_from_diffmerge -----

void _u0078__exec_gnu_diff(SG_context *pCtx, const char * pPathA, const char * pPathB, SG_file * pFileStdOut, SG_bool ignoreWhitespace, SG_bool ignoreCase, SG_exit_status * pExitStatus)
{
#if defined(WINDOWS)
    const char * const szGnuDiffCommand = "diff.exe";
#else
    const char * const szGnuDiffCommand = "diff";
#endif

    SG_exec_argvec * pGnuDiffArgs = NULL;
    SG_exit_status exitStatus;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pPathA);
    SG_NULLARGCHECK_RETURN(pPathB);

    SG_ERR_CHECK(  SG_exec_argvec__alloc(pCtx, &pGnuDiffArgs)  );
    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pGnuDiffArgs, "-u")  );
    if(ignoreWhitespace)
        SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pGnuDiffArgs, "-b")  );
    if(ignoreCase)
        SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pGnuDiffArgs, "-i")  );
    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pGnuDiffArgs, pPathA)  );
    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pGnuDiffArgs, pPathB)  );

    SG_ERR_CHECK(  SG_exec__exec_sync__files(pCtx, szGnuDiffCommand, pGnuDiffArgs, NULL, pFileStdOut, NULL, &exitStatus)  );
    if(pExitStatus!=NULL)
        *pExitStatus = exitStatus;

    SG_EXEC_ARGVEC_NULLFREE(pCtx, pGnuDiffArgs);

    return;
fail:
    SG_EXEC_ARGVEC_NULLFREE(pCtx, pGnuDiffArgs);
}

void _u0078__advance_past_eol(SG_context *pCtx, SG_file * pFile)
{
    SG_byte b;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pFile);

    do
    {
        SG_file__read(pCtx, pFile, 1, &b, NULL);
    }
    while(!SG_context__has_err(pCtx) && b!=SG_LF);

    SG_ERR_CHECK_RETURN_CURRENT_DISREGARD(SG_ERR_EOF);
}

void _u0078__read1(SG_context * pCtx, SG_file * pFile, SG_byte * pByte, SG_bool * pEof)
{
    SG_byte b;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pFile);
    SG_NULLARGCHECK_RETURN(pByte);
    SG_NULLARGCHECK_RETURN(pEof);

    SG_file__read(pCtx, pFile, 1, &b, NULL);
    if(SG_context__err_equals(pCtx, SG_ERR_EOF))
    {
        SG_context__err_reset(pCtx);
        *pEof = SG_TRUE;
        return;
    }
    SG_ERR_CHECK_RETURN_CURRENT;

    if(b==SG_CR)
    {
        SG_file__read(pCtx, pFile, 1, &b, NULL);
        if(SG_context__err_equals(pCtx, SG_ERR_EOF))
        {
            SG_context__err_reset(pCtx);
            *pEof = SG_TRUE;
            return;
        }
        SG_ERR_CHECK_RETURN_CURRENT;
    }

    *pByte = b;
}

// Use this function to compare our diff (of two files) with gnu's diff (of the same files) to see if the diffs are identical.
void _u0078__diff_diffs(SG_context *pCtx, SG_file * pDiffA, SG_file * pDiffB, SG_bool * pDiffsDiffer)
{
    SG_byte a;
    SG_byte b;

    SG_bool eofA = SG_FALSE;
    SG_bool eofB = SG_FALSE;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pDiffA);
    SG_NULLARGCHECK_RETURN(pDiffB);
    SG_NULLARGCHECK_RETURN(pDiffsDiffer);

    SG_ERR_CHECK(  SG_file__seek(pCtx, pDiffA, 0)  );
    SG_ERR_CHECK(  SG_file__seek(pCtx, pDiffB, 0)  );

    // Skip the headers. They have a timestamp in them and will be different.
    SG_ERR_CHECK(  _u0078__advance_past_eol(pCtx, pDiffA)  );
    SG_ERR_CHECK(  _u0078__advance_past_eol(pCtx, pDiffA)  );
    SG_ERR_CHECK(  _u0078__advance_past_eol(pCtx, pDiffB)  );
    SG_ERR_CHECK(  _u0078__advance_past_eol(pCtx, pDiffB)  );

    do
    {
        SG_ERR_CHECK(  _u0078__read1(pCtx, pDiffA, &a, &eofA)  );
        SG_ERR_CHECK(  _u0078__read1(pCtx, pDiffB, &b, &eofB)  );
    }
    while(!eofA && !eofB && a==b);

	// Allow an extra newline at the end.
	while(!eofA && (a==SG_CR||a==SG_LF))
        SG_ERR_CHECK(  _u0078__read1(pCtx, pDiffA, &a, &eofA)  );
    while(!eofB && (b==SG_CR||b==SG_LF))
        SG_ERR_CHECK(  _u0078__read1(pCtx, pDiffB, &b, &eofB)  );

    *pDiffsDiffer = !(eofA&&eofB);

    return;
fail:
    ;
}

// When the previous test has failed, fall back to this one. It might have been a false negative.
void _u0078__diff_patches(SG_context *pCtx, const SG_pathname * pOriginalFile, SG_file * pPatch1, SG_file * pPatch2, SG_bool ignoreWhitespace, SG_bool ignoreCase, SG_bool * pPatchesDiffer)
{
#if defined(WINDOWS)
    const char * const szPatchCommand = "patch.exe";
#else
    const char * const szPatchCommand = "patch";
#endif

    SG_exec_argvec * pPatchArgs = NULL;
    SG_exit_status exitStatus;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pOriginalFile);
    SG_NULLARGCHECK_RETURN(pPatch1);
    SG_NULLARGCHECK_RETURN(pPatch2);
    SG_NULLARGCHECK_RETURN(pPatchesDiffer);

    SG_ERR_CHECK(  SG_exec_argvec__alloc(pCtx, &pPatchArgs)  );
    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pPatchArgs, "-u")  );
    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pPatchArgs, SG_pathname__sz(pOriginalFile))  );
    //if(ignoreWhitespace)
    //    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pPatchArgs, "-l")  );
    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pPatchArgs, "-o")  );
    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pPatchArgs, "u0078-patched-1")  );

    SG_ERR_CHECK(  SG_exec__exec_sync__files(pCtx, szPatchCommand, pPatchArgs, pPatch1, NULL, NULL, &exitStatus)  );

    SG_ERR_CHECK(  SG_exec_argvec__remove_last(pCtx, pPatchArgs)  );
    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pPatchArgs, "u0078-patched-2")  );

    SG_ERR_CHECK(  SG_exec__exec_sync__files(pCtx, szPatchCommand, pPatchArgs, pPatch2, NULL, NULL, &exitStatus)  );

    SG_ERR_CHECK(  _u0078__exec_gnu_diff(pCtx, "u0078-patched-1", "u0078-patched-2", NULL, ignoreWhitespace, ignoreCase, &exitStatus)  );

    *pPatchesDiffer = (exitStatus!=0);

    SG_EXEC_ARGVEC_NULLFREE(pCtx, pPatchArgs);

    return;
fail:
    SG_EXEC_ARGVEC_NULLFREE(pCtx, pPatchArgs);
}

void _u0078__gnu_diff_compatibility_test(SG_context *pCtx, const SG_pathname * pPathA, const SG_pathname * pPathB, SG_bool ignoreWhitespace, SG_bool ignoreCase)
{
    SG_pathname * pPath = NULL;
    SG_file * pResult = NULL;
    SG_file * pExpectedResult = NULL;
    SG_bool different;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pPathA);
    SG_NULLARGCHECK_RETURN(pPathB);

    SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pPath, "u0078-result")  );
    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_OPEN_OR_CREATE|SG_FILE_RDWR, 0666, &pResult)  );
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_ERR_CHECK(  SG_file__seek(pCtx, pResult, 0)  );
    SG_ERR_CHECK(  SG_file__truncate(pCtx, pResult)  );
    if(!ignoreWhitespace && !ignoreCase)
        SG_ERR_CHECK(  SG_internalfilediff__unified(pCtx, pPathA, pPathB, NULL, NULL, pResult, NULL, &different)  );
    else if(!ignoreCase)
        SG_ERR_CHECK(  SG_internalfilediff__unified__ignore_whitespace(pCtx, pPathA, pPathB, NULL, NULL, pResult, NULL, &different)  );
    else if(!ignoreWhitespace)
        SG_ERR_CHECK(  SG_internalfilediff__unified__ignore_case(pCtx, pPathA, pPathB, NULL, NULL, pResult, NULL, &different)  );
    else
        SG_ERR_CHECK(  SG_internalfilediff__unified__ignore_whitespace_and_case(pCtx, pPathA, pPathB, NULL, NULL, pResult, NULL, &different)  );

    SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pPath, "u0078-expected-result")  );
    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_OPEN_OR_CREATE|SG_FILE_RDWR, 0666, &pExpectedResult)  );
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_ERR_CHECK(  SG_file__seek(pCtx, pExpectedResult, 0)  );
    SG_ERR_CHECK(  SG_file__truncate(pCtx, pExpectedResult)  );
    SG_ERR_CHECK(  _u0078__exec_gnu_diff(pCtx, SG_pathname__sz(pPathA), SG_pathname__sz(pPathB), pExpectedResult, ignoreWhitespace, ignoreCase, NULL)  );

    SG_ERR_CHECK(_u0078__diff_diffs(pCtx, pExpectedResult, pResult, &different)  );

    if(different)
    {
        // The diffs were different.
        // Try running patch on both sets and see if they produce the same output (the true test).
        SG_ERR_CHECK(  SG_file__seek(pCtx, pExpectedResult, 0)  );
        SG_ERR_CHECK(  SG_file__seek(pCtx, pResult, 0)  );
        SG_ERR_CHECK(  _u0078__diff_patches(pCtx, pPathA, pExpectedResult, pResult, ignoreWhitespace, ignoreCase, &different)  );
    }
    VERIFY_COND(  SG_pathname__sz(pPathA)  ,  !different  );

    SG_ERR_CHECK(  SG_file__close(pCtx, &pResult)  );
    SG_ERR_CHECK(  SG_file__close(pCtx, &pExpectedResult)  );

    return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_FILE_NULLCLOSE(pCtx, pResult);
    SG_FILE_NULLCLOSE(pCtx, pExpectedResult);
}

void _u0078__gnu_diff_compatibility_test_suite_from_diffmerge(SG_context *pCtx, const SG_pathname * pDataDir)
{
    SG_pathname * pPath = NULL;

    SG_error errReadStat;
    SG_dir * pDir = NULL;
    SG_rbtree * pFilenames = NULL; // Since SG_dir__read() doesn't return files in alphabetical order...
    SG_rbtree_iterator * pIt = NULL;
    SG_bool ok = SG_FALSE;
    SG_string * pFilename = NULL;
    const char * szFilenameA = NULL;
    SG_pathname * pPathA = NULL;
    SG_pathname * pPathB = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pDataDir);

    VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pFilenames)  );

    VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pDataDir, "u0078_diff_data")  );
    VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath, "Diff")  );
    VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pFilename)  );
    VERIFY_ERR_CHECK(  SG_dir__open(pCtx, pPath, &pDir, &errReadStat, pFilename, NULL)  );
    while(SG_IS_OK(errReadStat))
    {
        if(SG_string__sz(pFilename)[0]!='.')
            VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, pFilenames, SG_string__sz(pFilename))  );

        SG_dir__read(pCtx, pDir, pFilename, NULL);
        SG_context__get_err(pCtx, &errReadStat);
        SG_context__err_reset(pCtx);
    }
    VERIFY_COND("", errReadStat==SG_ERR_NOMOREFILES);
    SG_DIR_NULLCLOSE(pCtx, pDir);
    SG_STRING_NULLFREE(pCtx, pFilename);

    VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIt, pFilenames, &ok, &szFilenameA, NULL)  );
    while(ok)
    {
        const char * szFilenameB = NULL;
        VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIt, &ok, &szFilenameB, NULL)  );
        VERIFY_COND("", ok);

        VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathA, pPath, szFilenameA)  );
        VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathB, pPath, szFilenameB)  );

        VERIFY_ERR_CHECK(  _u0078__gnu_diff_compatibility_test(pCtx, pPathA, pPathB, SG_FALSE, SG_FALSE)  );
        VERIFY_ERR_CHECK(  _u0078__gnu_diff_compatibility_test(pCtx, pPathB, pPathA, SG_FALSE, SG_FALSE)  );

        VERIFY_ERR_CHECK(  _u0078__gnu_diff_compatibility_test(pCtx, pPathA, pPathB, SG_FALSE, SG_TRUE)  );
        VERIFY_ERR_CHECK(  _u0078__gnu_diff_compatibility_test(pCtx, pPathB, pPathA, SG_FALSE, SG_TRUE)  );

        VERIFY_ERR_CHECK(  _u0078__gnu_diff_compatibility_test(pCtx, pPathA, pPathB, SG_TRUE, SG_FALSE)  );
        VERIFY_ERR_CHECK(  _u0078__gnu_diff_compatibility_test(pCtx, pPathB, pPathA, SG_TRUE, SG_FALSE)  );

        VERIFY_ERR_CHECK(  _u0078__gnu_diff_compatibility_test(pCtx, pPathA, pPathB, SG_TRUE, SG_TRUE)  );
        VERIFY_ERR_CHECK(  _u0078__gnu_diff_compatibility_test(pCtx, pPathB, pPathA, SG_TRUE, SG_TRUE)  );

        SG_PATHNAME_NULLFREE(pCtx, pPathA);
        SG_PATHNAME_NULLFREE(pCtx, pPathB);

        VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIt, &ok, &szFilenameA, NULL)  );
    }

    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIt);
    SG_RBTREE_NULLFREE(pCtx, pFilenames);

    SG_PATHNAME_NULLFREE(pCtx, pPath);

    return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_DIR_NULLCLOSE(pCtx, pDir);
    SG_STRING_NULLFREE(pCtx, pFilename);
    SG_PATHNAME_NULLFREE(pCtx, pPathA);
    SG_PATHNAME_NULLFREE(pCtx, pPathB);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIt);
    SG_RBTREE_NULLFREE(pCtx, pFilenames);
}

// ----- _u0078__gnu_diff3_compatibility_test_suite_from_diffmerge -----

void _u0078__exec_gnu_diff3(
    SG_context *pCtx,
    const SG_pathname * pAncestor,
    const SG_pathname * pMod1,
    const SG_pathname * pMod2,
    SG_file * pFileStdOut,
    SG_bool * pDidFindConflicts)
{
#if defined(WINDOWS)
    const char * const szGnuDiff3Command = "diff3.exe";
#else
    const char * const szGnuDiff3Command = "diff3";
#endif

    SG_exec_argvec * pGnuDiff3Args = NULL;
    SG_exit_status exitStatus;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pAncestor);
    SG_NULLARGCHECK_RETURN(pMod1);
    SG_NULLARGCHECK_RETURN(pMod2);

    SG_ERR_CHECK(  SG_exec_argvec__alloc(pCtx, &pGnuDiff3Args)  );
    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pGnuDiff3Args, "-m")  );
    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pGnuDiff3Args, SG_pathname__sz(pMod1))  );
    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pGnuDiff3Args, SG_pathname__sz(pAncestor))  );
    SG_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pGnuDiff3Args, SG_pathname__sz(pMod2))  );

    SG_ERR_CHECK(  SG_exec__exec_sync__files(pCtx, szGnuDiff3Command, pGnuDiff3Args, NULL, pFileStdOut, NULL, &exitStatus)  );
    if(pDidFindConflicts!=NULL)
        *pDidFindConflicts = (exitStatus==1);

    SG_EXEC_ARGVEC_NULLFREE(pCtx, pGnuDiff3Args);

    return;
fail:
    SG_EXEC_ARGVEC_NULLFREE(pCtx, pGnuDiff3Args);
}

void _u0078__exec_internal_file_merge(
    SG_context *pCtx,
    const SG_pathname * pAncestor,
    const SG_pathname * pMod1,
    const SG_pathname * pMod2,
    SG_file * pOutputFile,
    SG_bool * pDidFindConflicts)
{
    SG_diff_t * pDiff = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pAncestor);
    SG_NULLARGCHECK_RETURN(pMod1);
    SG_NULLARGCHECK_RETURN(pMod2);

    VERIFY_ERR_CHECK(  SG_diff3_file(pCtx, pAncestor, pMod1, pMod2, SG_DIFF_OPTION__REGULAR, &pDiff)  );
    VERIFY_ERR_CHECK(  SG_diff3_file_output(pCtx, pDiff, pAncestor, pMod1, pMod2, NULL, NULL, NULL, NULL, SG_TRUE, SG_FALSE, pOutputFile)  );
    if(pDidFindConflicts!=NULL)
        *pDidFindConflicts = SG_diff_contains_conflicts(pDiff);

    SG_DIFF_NULLFREE(pCtx, pDiff);

    return;
fail:
    SG_DIFF_NULLFREE(pCtx, pDiff);
}

// Use this function to compare our merge result with gnu diff3's merge result (of the same files) to see if the merge results are identical.
void _u0078__diff_merge_results(SG_context *pCtx, SG_file * pDiffA, SG_file * pDiffB, SG_bool * pDiffsDiffer)
{
    SG_byte a;
    SG_byte b;

    SG_bool eofA = SG_FALSE;
    SG_bool eofB = SG_FALSE;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pDiffA);
    SG_NULLARGCHECK_RETURN(pDiffB);
    SG_NULLARGCHECK_RETURN(pDiffsDiffer);

    SG_ERR_CHECK(  SG_file__seek(pCtx, pDiffA, 0)  );
    SG_ERR_CHECK(  SG_file__seek(pCtx, pDiffB, 0)  );

    do
    {
        SG_ERR_CHECK(  _u0078__read1(pCtx, pDiffA, &a, &eofA)  );
        SG_ERR_CHECK(  _u0078__read1(pCtx, pDiffB, &b, &eofB)  );
    }
    while(!eofA && !eofB && a==b);

	// Allow an extra newline at the end.
	while(!eofA && (a==SG_CR||a==SG_LF))
        SG_ERR_CHECK(  _u0078__read1(pCtx, pDiffA, &a, &eofA)  );
    while(!eofB && (b==SG_CR||b==SG_LF))
        SG_ERR_CHECK(  _u0078__read1(pCtx, pDiffB, &b, &eofB)  );

    *pDiffsDiffer = !(eofA&&eofB);

    return;
fail:
    ;
}

void _u0078__gnu_diff3_compatibility_test(SG_context *pCtx, const SG_pathname * pAncestor, const SG_pathname * pMod1, const SG_pathname * pMod2)
{
    SG_pathname * pPath = NULL;
    SG_file * pResult = NULL;
    SG_file * pExpectedResult = NULL;
    SG_bool gnuDiffReportedConflicts;
    SG_bool internalDiffReportedConflicts;
        const char * szFile = NULL;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pAncestor);
    SG_NULLARGCHECK_RETURN(pMod1);
    SG_NULLARGCHECK_RETURN(pMod2);

    SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pPath, "u0078-result")  );
    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_OPEN_OR_CREATE|SG_FILE_RDWR, 0666, &pResult)  );
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_ERR_CHECK(  SG_file__seek(pCtx, pResult, 0)  );
    SG_ERR_CHECK(  SG_file__truncate(pCtx, pResult)  );
    SG_ERR_CHECK(  _u0078__exec_internal_file_merge(pCtx, pAncestor, pMod1, pMod2, pResult, &internalDiffReportedConflicts)  );

    SG_ERR_CHECK(  SG_pathname__alloc__sz(pCtx, &pPath, "u0078-expected-result")  );
    SG_ERR_CHECK(  SG_file__open__pathname(pCtx, pPath, SG_FILE_OPEN_OR_CREATE|SG_FILE_RDWR, 0666, &pExpectedResult)  );
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_ERR_CHECK(  SG_file__seek(pCtx, pExpectedResult, 0)  );
    SG_ERR_CHECK(  SG_file__truncate(pCtx, pExpectedResult)  );
    SG_ERR_CHECK(  _u0078__exec_gnu_diff3(pCtx, pAncestor, pMod1, pMod2, pExpectedResult, &gnuDiffReportedConflicts)  );

    SG_ERR_CHECK(  SG_pathname__filename(pCtx, pAncestor, &szFile)  );
    if(strcmp(szFile, "m001_mod1.txt")==0 || strcmp(szFile, "m001_mod2.txt")==0 || strcmp(szFile, "m005_ancestor.txt")==0)
    {
        // Known issue, if both files made an identical change, we
        // do not call it a conflict, whereas gnu diff3 does.
        VERIFY_COND(  SG_pathname__sz(pAncestor)  ,  !internalDiffReportedConflicts && gnuDiffReportedConflicts  );
    }
    else if(strcmp(szFile, "bug7043_00_ancestor.txt")==0 || strcmp(szFile, "bug7043_01_ancestor.txt")==0)
    {
        // Known issue, if files made changes to adjacent lines, we
        // do not call it a conflict, whereas gnu diff3 does.
        VERIFY_COND(  SG_pathname__sz(pAncestor)  ,  !internalDiffReportedConflicts && gnuDiffReportedConflicts  );
    }
    else
        VERIFY_COND(  SG_pathname__sz(pAncestor)  ,  internalDiffReportedConflicts==gnuDiffReportedConflicts  );

    // Only verify the output if there was no conflicts, due to bug #SPRAWL-725.
    if(!internalDiffReportedConflicts && !gnuDiffReportedConflicts)
    {
        SG_bool different;
        SG_ERR_CHECK(_u0078__diff_merge_results(pCtx, pExpectedResult, pResult, &different)  );
        VERIFY_COND(  SG_pathname__sz(pAncestor)  , !different  );
    }

    SG_ERR_CHECK(  SG_file__close(pCtx, &pResult)  );
    SG_ERR_CHECK(  SG_file__close(pCtx, &pExpectedResult)  );

    return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_FILE_NULLCLOSE(pCtx, pResult);
    SG_FILE_NULLCLOSE(pCtx, pExpectedResult);
}

void _u0078__gnu_diff3_compatibility_test_suite_from_diffmerge(SG_context *pCtx, const SG_pathname * pDataDir)
{
    SG_pathname * pPath = NULL;

    SG_error errReadStat;
    SG_dir * pDir = NULL;
    SG_rbtree * pFilenames = NULL; // Since SG_dir__read() doesn't return files in alphabetical order...
    SG_rbtree_iterator * pIt = NULL;
    SG_bool ok = SG_FALSE;
    SG_string * pFilename = NULL;
    const char * szAncestor = NULL;
    SG_pathname * pAncestor = NULL;
    SG_pathname * pMod1 = NULL;
    SG_pathname * pMod2 = NULL;
	SG_bool bDirExists = SG_FALSE;

    SG_ASSERT(pCtx!=NULL);
    SG_NULLARGCHECK_RETURN(pDataDir);

    VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &pFilenames)  );

    VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPath, pDataDir, "u0078_diff_data")  );
    VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPath, "Merge")  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx, pPath, &bDirExists, NULL, NULL)  );
	SG_ASSERT(bDirExists);
    VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pFilename)  );
    VERIFY_ERR_CHECK(  SG_dir__open(pCtx, pPath, &pDir, &errReadStat, pFilename, NULL)  );
    while(SG_IS_OK(errReadStat))
    {
        if(SG_string__sz(pFilename)[0]!='.')
            VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, pFilenames, SG_string__sz(pFilename))  );

        SG_dir__read(pCtx, pDir, pFilename, NULL);
        SG_context__get_err(pCtx, &errReadStat);
        SG_context__err_reset(pCtx);
    }
    VERIFY_COND("", errReadStat==SG_ERR_NOMOREFILES);
    SG_ERR_CHECK(  SG_dir__close(pCtx, pDir)  );
    SG_STRING_NULLFREE(pCtx, pFilename);

    VERIFY_ERR_CHECK(  SG_rbtree__iterator__first(pCtx, &pIt, pFilenames, &ok, &szAncestor, NULL)  );
    while(ok)
    {
        const char * szMod1 = NULL;
        const char * szMod2 = NULL;

        VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIt, &ok, &szMod1, NULL)  );
        VERIFY_COND("", ok);
        VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIt, &ok, &szMod2, NULL)  );
        VERIFY_COND("", ok);

        VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pAncestor, pPath, szAncestor)  );
        VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pMod1, pPath, szMod1)  );
        VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pMod2, pPath, szMod2)  );

        VERIFY_ERR_CHECK(  _u0078__gnu_diff3_compatibility_test(pCtx, pAncestor, pMod1, pMod2)  );
        VERIFY_ERR_CHECK(  _u0078__gnu_diff3_compatibility_test(pCtx, pAncestor, pMod2, pMod1)  );

        // And 'cuz we can!
        VERIFY_ERR_CHECK(  _u0078__gnu_diff3_compatibility_test(pCtx, pMod1, pAncestor, pMod2)  );
        VERIFY_ERR_CHECK(  _u0078__gnu_diff3_compatibility_test(pCtx, pMod1, pMod2, pAncestor)  );
        VERIFY_ERR_CHECK(  _u0078__gnu_diff3_compatibility_test(pCtx, pMod2, pAncestor, pMod1)  );
        VERIFY_ERR_CHECK(  _u0078__gnu_diff3_compatibility_test(pCtx, pMod2, pMod1, pAncestor)  );

        SG_PATHNAME_NULLFREE(pCtx, pAncestor);
        SG_PATHNAME_NULLFREE(pCtx, pMod1);
        SG_PATHNAME_NULLFREE(pCtx, pMod2);

        VERIFY_ERR_CHECK(  SG_rbtree__iterator__next(pCtx, pIt, &ok, &szAncestor, NULL)  );
    }

    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIt);
    SG_RBTREE_NULLFREE(pCtx, pFilenames);

    SG_PATHNAME_NULLFREE(pCtx, pPath);

    return;
fail:
    SG_PATHNAME_NULLFREE(pCtx, pPath);
    SG_DIR_NULLCLOSE(pCtx, pDir);
    SG_STRING_NULLFREE(pCtx, pFilename);
    SG_PATHNAME_NULLFREE(pCtx, pAncestor);
    SG_PATHNAME_NULLFREE(pCtx, pMod1);
    SG_PATHNAME_NULLFREE(pCtx, pMod2);
    SG_RBTREE_ITERATOR_NULLFREE(pCtx, pIt);
    SG_RBTREE_NULLFREE(pCtx, pFilenames);
}

TEST_MAIN(u0078_diff)
{
    TEMPLATE_MAIN_START;

    BEGIN_TEST(  _u0078__file_diff_sanity_check(pCtx)  );
    BEGIN_TEST(  _u0078__file_diff3_no_conflicts_sanity_check(pCtx)  );
    BEGIN_TEST(  _u0078__gnu_diff_compatibility_test_suite_from_diffmerge(pCtx, pDataDir)  );
    BEGIN_TEST(  _u0078__gnu_diff3_compatibility_test_suite_from_diffmerge(pCtx, pDataDir)  );

    TEMPLATE_MAIN_END;
}

#undef MyDcl
#undef MyFn
