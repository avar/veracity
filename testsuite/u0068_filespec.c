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

// testsuite/unittests/u0001_stdint.c
// test basic functionality of libraries/ut/sg_file_spec.c

//////////////////////////////////////////////////////////////////

#include <sg.h>

#include "unittests.h"

//////////////////////////////////////////////////////////////////


void u0068_file_spec__run_do_false_compares(SG_context * pCtx)
{
	SG_file_spec_eval eval;
	SG_bool b;
	const char** ppszExcludes = NULL;


	/* rootfile.c
   rootinc.h
   src/
   include/
   src/fred.c
   src/barney.c
   include/dino.h
   include/hoppy.h
   include/allpets.h*/

	SG_ERR_CHECK(  SG_alloc(pCtx, 5, sizeof(char *), &ppszExcludes)  );

		VERIFY_ERR_CHECK(  sg_file_spec__wildcmp(pCtx, "*.c", "@/rootfile", &b)  );
		VERIFY_COND("%.c", !b);
		VERIFY_ERR_CHECK(  sg_file_spec__wildcmp(pCtx, "**/include/*.h", "@/include.h", &b)  );
		VERIFY_COND("**/include/*.h", b);

	ppszExcludes[0] = "*.c";
	ppszExcludes[1] = "*.h";
	ppszExcludes[2] = "**/include/*.h";

		// TODO 2010/06/15 This test passes the INCLUDES in the variable ppszExcludes.
		// TODO            If we really want to test this routine here, we should
		// TODO            look for all 5 possible return values.
	VERIFY_ERR_CHECK(  SG_file_spec__should_include(pCtx,
													ppszExcludes, 2,
													NULL, 0,
													NULL, 0,
													"@/rootfile",
													&eval)  );
	VERIFY_COND("*.c", !SG_FILE_SPEC_EVAL__IS_INCLUDED(eval));
	//VERIFY_ERR_CHECK(  SG_file_spec__should_include(pCtx, ppszExcludes, 2, NULL, 0,  "/include.h", &b)  );
	//VERIFY_COND("**/include/*.h", !b);
fail:

	SG_NULLFREE(pCtx, ppszExcludes);
	return;
}

void u0068_file_spec__run_do_true_compares(SG_context * pCtx)
{
	SG_file_spec_eval eval;
	SG_bool b;
	const char** ppszIncludes = NULL;


	/* rootfile.c
   rootinc.h
   src/
   include/
   src/fred.c
   src/barney.c
   include/dino.h
   include/hoppy.h
   include/allpets.h*/

	SG_ERR_CHECK(  SG_alloc(pCtx, 6, sizeof(char *), &ppszIncludes)  );

		VERIFY_ERR_CHECK(  sg_file_spec__wildcmp(pCtx, "*.c", "@/rootfile.c", &b)  );
		VERIFY_COND("*.c", b);
		VERIFY_ERR_CHECK(  sg_file_spec__wildcmp(pCtx, "**.c", "@/foo/rootfile.c", &b)  );
		VERIFY_COND("**.c", b);
		VERIFY_ERR_CHECK(  sg_file_spec__wildcmp(pCtx, "roo?file.c", "@/rootfile.c", &b)  );
		VERIFY_COND("**.c", b);
		VERIFY_ERR_CHECK(  sg_file_spec__wildcmp(pCtx, "**/include/*.h", "@/include/level1.h", &b)  );
		VERIFY_COND("**/include/*.h", b);
		VERIFY_ERR_CHECK(  sg_file_spec__wildcmp(pCtx, "**/include/*.h", "@/further/down/include/level3.h", &b)  );
		VERIFY_COND("**/include/*.h", b);
		VERIFY_ERR_CHECK(  sg_file_spec__wildcmp(pCtx, "**/d2/*.txt", "@/d1/d2/*.txt", &b)  );
		VERIFY_COND("**/include/*.h", b);
		VERIFY_ERR_CHECK(  sg_file_spec__wildcmp(pCtx, "**/include/*.h", "@/further/down/include/level3.h", &b)  );
		VERIFY_COND("**/include/*.h", b);
		VERIFY_ERR_CHECK(  sg_file_spec__wildcmp(pCtx, "@/include/*.h", "@/include/level3.h", &b)  );
		VERIFY_COND("**/include/*.h", b);
		VERIFY_ERR_CHECK(  sg_file_spec__wildcmp(pCtx, "foo", "@/foo", &b)  );
		VERIFY_COND("**/include/*.h", b);
		VERIFY_ERR_CHECK(  sg_file_spec__wildcmp(pCtx, "**/foo", "@/foo", &b)  );
		VERIFY_COND("**/include/*.h", b);


	ppszIncludes[0] = "*.c";
	ppszIncludes[1] = "*.h";
	ppszIncludes[2] = "**/include/*.h";
	VERIFY_ERR_CHECK(  SG_file_spec__should_include(pCtx,
													ppszIncludes, 3,
													NULL, 0,
													NULL, 0,
													"@/rootfile.c",
													&eval)  );
	VERIFY_COND("should be included", SG_FILE_SPEC_EVAL__IS_INCLUDED(eval));
fail:
	SG_NULLFREE(pCtx, ppszIncludes);
	//SG_NULLFREE(pCtx, ppszExcludes);
	return;
}
void u0068SG_file_spec__read_patterns_from_file(SG_context* pCtx)
{
	FILE* fp;
	SG_pathname* pPathCR = NULL;
	SG_pathname* pPathLF = NULL;
	SG_pathname* pPathCRLF = NULL;
	SG_stringarray* ppszPatterns = NULL;
	const char* const* ppszArray = NULL;
    SG_uint32 count = 0;

	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPathCR)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPathLF)  );
	VERIFY_ERR_CHECK_DISCARD(  unittest__get_nonexistent_pathname(pCtx, &pPathCRLF)  );

	fp = fopen(SG_pathname__sz(pPathCR), "w");
	fprintf(fp, "#comment\r;comment\rincludes\r\r\tbin  \r*.sgbak\r  test space  \r  \r\r");
	fclose(fp);

	fp = fopen(SG_pathname__sz(pPathLF), "w");
	fprintf(fp, "#comment\n;comment\nincludes\n\n\tbin\t\t\n*.sgbak\n  test space  \n  \n\n");
	fclose(fp);

	fp = fopen(SG_pathname__sz(pPathCRLF), "w");
	fprintf(fp, "#comment\r\n;comment\r\nincludes\r\n\r\nbin\t\r\n*.sgbak\r\n  test space  \r\n  \r\n\r\n");
	fclose(fp);

	VERIFY_ERR_CHECK_DISCARD( SG_file_spec__read_patterns_from_file(pCtx, SG_pathname__sz(pPathCR), &ppszPatterns) );
	VERIFY_ERR_CHECK_DISCARD( SG_file_spec__read_patterns_from_file(pCtx, SG_pathname__sz(pPathLF), &ppszPatterns) );
	VERIFY_ERR_CHECK_DISCARD( SG_file_spec__read_patterns_from_file(pCtx, SG_pathname__sz(pPathCRLF), &ppszPatterns) );

    VERIFY_ERR_CHECK_DISCARD(  SG_stringarray__sz_array_and_count(pCtx, ppszPatterns, &ppszArray, &count)  );
	VERIFY_COND("count should be 12", count == 12);
	VERIFY_COND("trailing space", strcmp(ppszArray[1],"bin") == 0);
	VERIFY_COND("leading and trailing space", strcmp(ppszArray[3],"test space") == 0);


	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPathCR)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPathLF)  );
	SG_ERR_IGNORE(  SG_fsobj__remove__pathname(pCtx, pPathCRLF)  );

	SG_PATHNAME_NULLFREE(pCtx, pPathCR);
	SG_PATHNAME_NULLFREE(pCtx, pPathLF);
	SG_PATHNAME_NULLFREE(pCtx, pPathCRLF);
	SG_STRINGARRAY_NULLFREE(pCtx, ppszPatterns);


}

TEST_MAIN(u0068_filespec)
{
	TEMPLATE_MAIN_START;

	 BEGIN_TEST(  u0068_file_spec__run_do_true_compares(pCtx)  );
	 BEGIN_TEST(  u0068_file_spec__run_do_false_compares(pCtx)  );
	 BEGIN_TEST(  u0068SG_file_spec__read_patterns_from_file(pCtx)  );

	TEMPLATE_MAIN_END;
}



