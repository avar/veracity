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
 * @file u0056_exec.c
 *
 * @details A simple test to exercise SG_exec.
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

#define MyMain()				TEST_MAIN(u0056_exec)
#define MyDcl(name)				u0056_exec__##name
#define MyFn(name)				u0056_exec__##name

//////////////////////////////////////////////////////////////////

#if defined(MAC) || defined(LINUX)
#define MY_CAT_COMMAND				"/bin/cat"
#endif
#if defined(WINDOWS)
#define MY_CAT_COMMAND				"cat.exe"
#endif

//////////////////////////////////////////////////////////////////

/**
 * create 1 test file.
 * cat it to a second file using files bound to child's STDIN and STDOUT.
 * cat both files to third file using cl args for input and third bound to child's STDOUT.
 * cat all 3 files using cl args to OUR STDOUT.
 */
void MyFn(test1)(SG_context * pCtx)
{
	SG_exit_status exitStatusChild;
	SG_file * pFileF1 = NULL;
	SG_file * pFileF2 = NULL;
	SG_file * pFileF3 = NULL;
	SG_pathname * pPathTempDir = NULL;
	SG_pathname * pPathF1 = NULL;
	SG_pathname * pPathF2 = NULL;
	SG_pathname * pPathF3 = NULL;
	SG_exec_argvec * pArgVec = NULL;

	// create a GID temp directory in the current directory.

	VERIFY_ERR_CHECK(  unittest__alloc_unique_pathname_in_cwd(pCtx,&pPathTempDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir_recursive__pathname(pCtx,pPathTempDir)  );

	// create a couple of pathnames to test files in the temp directory.

	VERIFY_ERR_CHECK(  unittest__alloc_unique_pathname_in_dir(pCtx,SG_pathname__sz(pPathTempDir),&pPathF1)  );
	INFOP("exec",("PathF1 is %s",SG_pathname__sz(pPathF1)));
	VERIFY_ERR_CHECK(  unittest__alloc_unique_pathname_in_dir(pCtx,SG_pathname__sz(pPathTempDir),&pPathF2)  );
	INFOP("exec",("PathF2 is %s",SG_pathname__sz(pPathF2)));
	VERIFY_ERR_CHECK(  unittest__alloc_unique_pathname_in_dir(pCtx,SG_pathname__sz(pPathTempDir),&pPathF3)  );
	INFOP("exec",("PathF3 is %s",SG_pathname__sz(pPathF3)));

	//////////////////////////////////////////////////////////////////
	// create F1 and write some data to it.

	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx,pPathF1,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,0644,&pFileF1)  );
	VERIFY_ERR_CHECK(  SG_file__write(pCtx,pFileF1,
									  SG_pathname__length_in_bytes(pPathF1),
									  (SG_byte *)SG_pathname__sz(pPathF1),
									  NULL)  );
	VERIFY_ERR_CHECK(  SG_file__write(pCtx,pFileF1,
									  1,
									  (SG_byte *)"\n",
									  NULL)  );
	VERIFY_ERR_CHECK(  SG_file__write(pCtx,pFileF1,
									  SG_pathname__length_in_bytes(pPathF1),
									  (SG_byte *)SG_pathname__sz(pPathF1),
									  NULL)  );
	VERIFY_ERR_CHECK(  SG_file__write(pCtx,pFileF1,
									  1,
									  (SG_byte *)"\n",
									  NULL)  );
	SG_FILE_NULLCLOSE(pCtx, pFileF1);

	//////////////////////////////////////////////////////////////////
	// re-open F1 for reading.
	// create F2 as a place for STDOUT of command.

	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx,pPathF1,SG_FILE_RDONLY|SG_FILE_OPEN_EXISTING,0644,&pFileF1)  );
	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx,pPathF2,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,   0644,&pFileF2)  );

	// exec: /bin/cat <f1 >f2

	VERIFY_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,MY_CAT_COMMAND,NULL,pFileF1,pFileF2,NULL,&exitStatusChild)  );
	VERIFY_COND("child status",(exitStatusChild == 0));

	INFOP("/bin/cat <f1 >f2",("Child exit status is [%d]",exitStatusChild));
	SG_FILE_NULLCLOSE(pCtx, pFileF1);
	SG_FILE_NULLCLOSE(pCtx, pFileF2);

	//////////////////////////////////////////////////////////////////
	// let F1 and F2 be given on the command line.
	// create F3 as a place for STDOUT of command.

	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx,pPathF3,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,   0644,&pFileF3)  );

	// exec: /bin/cat -n f1 f2 >f3

	VERIFY_ERR_CHECK(  SG_exec_argvec__alloc(pCtx,&pArgVec)  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,"-n")  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pPathF1))  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pPathF2))  );

	VERIFY_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,MY_CAT_COMMAND,pArgVec,NULL,pFileF3,NULL,&exitStatusChild)  );
	VERIFY_COND("child status",(exitStatusChild == 0));

	INFOP("/bin/cat -n f1 f2 >f3",("Child exit status is [%d]",exitStatusChild));
	SG_FILE_NULLCLOSE(pCtx, pFileF3);
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);

	//////////////////////////////////////////////////////////////////
	// exec: /bin/cat f1 f2 f3 (to our stdout)

	VERIFY_ERR_CHECK(  SG_exec_argvec__alloc(pCtx,&pArgVec)  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pPathF1))  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pPathF2))  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pPathF3))  );

	VERIFY_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,MY_CAT_COMMAND,pArgVec,NULL,NULL,NULL,&exitStatusChild)  );
	VERIFY_COND("child status",(exitStatusChild == 0));

	INFOP("/bin/cat -n f1 f2 f3",("Child exit status is [%d]",exitStatusChild));
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);

	//////////////////////////////////////////////////////////////////
	// exec: /bin/cat f3 f2 f1 (to our stdout)

	VERIFY_ERR_CHECK(  SG_exec_argvec__alloc(pCtx,&pArgVec)  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pPathF3))  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pPathF2))  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pPathF1))  );

	VERIFY_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,MY_CAT_COMMAND,pArgVec,NULL,NULL,NULL,&exitStatusChild)  );
	VERIFY_COND("child status",(exitStatusChild == 0));

	INFOP("/bin/cat -n f3 f2 f1",("Child exit status is [%d]",exitStatusChild));
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);

	// fall through to common cleanup.

fail:
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);
	SG_PATHNAME_NULLFREE(pCtx, pPathTempDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathF1);
	SG_PATHNAME_NULLFREE(pCtx, pPathF2);
	SG_PATHNAME_NULLFREE(pCtx, pPathF3);
	SG_FILE_NULLCLOSE(pCtx, pFileF1);
	SG_FILE_NULLCLOSE(pCtx, pFileF2);
	SG_FILE_NULLCLOSE(pCtx, pFileF3);
}

//////////////////////////////////////////////////////////////////

/**
 * create 1 test file.
 * cat it to a second file using files bound to child's STDIN and STDOUT.
 * repeat using still open files.
 * verify that we get an append effect.
 *
 * do this again using input file on the command line into a third file.
 */
void MyFn(test2)(SG_context * pCtx)
{
	SG_exit_status exitStatusChild;
	SG_file * pFileF1 = NULL;
	SG_file * pFileF2 = NULL;
	SG_file * pFileF3 = NULL;
	SG_pathname * pPathTempDir = NULL;
	SG_pathname * pPathF1 = NULL;
	SG_pathname * pPathF2 = NULL;
	SG_pathname * pPathF3 = NULL;
	SG_exec_argvec * pArgVec = NULL;
	SG_uint64 lenFileF1, lenFileF2, lenFileF3;
	SG_uint64 posFileF1, posFileF2, posFileF3;

	// create a GID temp directory in the current directory.

	VERIFY_ERR_CHECK(  unittest__alloc_unique_pathname_in_cwd(pCtx,&pPathTempDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir_recursive__pathname(pCtx,pPathTempDir)  );

	// create a couple of pathnames to test files in the temp directory.

	VERIFY_ERR_CHECK(  unittest__alloc_unique_pathname_in_dir(pCtx,SG_pathname__sz(pPathTempDir),&pPathF1)  );
	INFOP("exec",("PathF1 is %s",SG_pathname__sz(pPathF1)));
	VERIFY_ERR_CHECK(  unittest__alloc_unique_pathname_in_dir(pCtx,SG_pathname__sz(pPathTempDir),&pPathF2)  );
	INFOP("exec",("PathF2 is %s",SG_pathname__sz(pPathF2)));
	VERIFY_ERR_CHECK(  unittest__alloc_unique_pathname_in_dir(pCtx,SG_pathname__sz(pPathTempDir),&pPathF3)  );
	INFOP("exec",("PathF3 is %s",SG_pathname__sz(pPathF3)));

	//////////////////////////////////////////////////////////////////
	// create F1 and write some data to it.

	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx,pPathF1,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,0644,&pFileF1)  );
	VERIFY_ERR_CHECK(  SG_file__write(pCtx,pFileF1,
									  SG_pathname__length_in_bytes(pPathF1),
									  (SG_byte *)SG_pathname__sz(pPathF1),
									  NULL)  );
	VERIFY_ERR_CHECK(  SG_file__write(pCtx,pFileF1,
									  1,
									  (SG_byte *)"\n",
									  NULL)  );
	SG_FILE_NULLCLOSE(pCtx, pFileF1);

	// get length of F1 as created on disk.

	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx,pPathF1,&lenFileF1,NULL)  );
	INFOP("lenCheck",("Length F1[%d]",(SG_uint32)lenFileF1));
	VERIFY_COND("lenCheck",(lenFileF1 > 0));

	//////////////////////////////////////////////////////////////////
	// re-open F1 for reading.
	// create F2 as a place for STDOUT of command.

	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx,pPathF1,SG_FILE_RDONLY|SG_FILE_OPEN_EXISTING,0644,&pFileF1)  );
	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx,pPathF2,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,   0644,&pFileF2)  );

	// exec: "/bin/cat <f1 >f2" three times holding the file handles open between runs.

	VERIFY_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,MY_CAT_COMMAND,NULL,pFileF1,pFileF2,NULL,&exitStatusChild)  );
	VERIFY_COND("child status",(exitStatusChild == 0));
	INFOP("/bin/cat <f1 >f2",("Child exit status is [%d]",exitStatusChild));
	VERIFY_ERR_CHECK(  SG_file__tell(pCtx,pFileF1,&posFileF1)  );
	VERIFY_ERR_CHECK(  SG_file__tell(pCtx,pFileF2,&posFileF2)  );
	INFOP("tell",("Position F1[%d] Position F2[%d]",(SG_uint32)posFileF1,(SG_uint32)posFileF2));
	VERIFY_COND("tell",(posFileF1 == lenFileF1));		// child has dup'd version of handles and so we share seek positions.
	VERIFY_COND("tell",(posFileF2 == posFileF1));		// so, the child should have caused our position to change.

	VERIFY_ERR_CHECK(  SG_file__seek(pCtx,pFileF1,0)  );

	VERIFY_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,MY_CAT_COMMAND,NULL,pFileF1,pFileF2,NULL,&exitStatusChild)  );
	VERIFY_COND("child status",(exitStatusChild == 0));
	INFOP("/bin/cat <f1 >f2",("Child exit status is [%d]",exitStatusChild));
	VERIFY_ERR_CHECK(  SG_file__tell(pCtx,pFileF1,&posFileF1)  );
	VERIFY_ERR_CHECK(  SG_file__tell(pCtx,pFileF2,&posFileF2)  );
	INFOP("tell",("Position F1[%d] Position F2[%d]",(SG_uint32)posFileF1,(SG_uint32)posFileF2));
	VERIFY_COND("tell",(posFileF1 == lenFileF1));		// child has dup'd version of handles and so we share seek positions.
	VERIFY_COND("tell",(posFileF2 == 2*posFileF1));		// so, the child should have caused our position to change.

	VERIFY_ERR_CHECK(  SG_file__seek(pCtx,pFileF1,0)  );

	VERIFY_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,MY_CAT_COMMAND,NULL,pFileF1,pFileF2,NULL,&exitStatusChild)  );
	VERIFY_COND("child status",(exitStatusChild == 0));
	INFOP("/bin/cat <f1 >f2",("Child exit status is [%d]",exitStatusChild));
	VERIFY_ERR_CHECK(  SG_file__tell(pCtx,pFileF1,&posFileF1)  );
	VERIFY_ERR_CHECK(  SG_file__tell(pCtx,pFileF2,&posFileF2)  );
	INFOP("tell",("Position F1[%d] Position F2[%d]",(SG_uint32)posFileF1,(SG_uint32)posFileF2));
	VERIFY_COND("tell",(posFileF1 == lenFileF1));		// child has dup'd version of handles and so we share seek positions.
	VERIFY_COND("tell",(posFileF2 == 3*posFileF1));		// so, the child should have caused our position to change.

	SG_FILE_NULLCLOSE(pCtx, pFileF1);
	SG_FILE_NULLCLOSE(pCtx, pFileF2);

	// get length of F2 and see how it compares with F1.

	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx,pPathF2,&lenFileF2,NULL)  );
	INFOP("lenCheck",("Length F1[%d] Length F2[%d]",(SG_uint32)lenFileF1,(SG_uint32)lenFileF2));
	VERIFY_COND("lenCheck",(lenFileF2 == 3*lenFileF1));

	//////////////////////////////////////////////////////////////////
	// let F1 be given on the command line.
	// create F3 as a place for STDOUT of command.

	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx,pPathF3,SG_FILE_WRONLY|SG_FILE_CREATE_NEW,   0644,&pFileF3)  );

	// exec: "/bin/cat f1 >f3" three times holding F3 open between runs.

	VERIFY_ERR_CHECK(  SG_exec_argvec__alloc(pCtx,&pArgVec)  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx,pArgVec,SG_pathname__sz(pPathF1))  );

	VERIFY_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,MY_CAT_COMMAND,pArgVec,NULL,pFileF3,NULL,&exitStatusChild)  );
	VERIFY_COND("child status",(exitStatusChild == 0));
	INFOP("/bin/cat f1 >f3",("Child exit status is [%d]",exitStatusChild));
	VERIFY_ERR_CHECK(  SG_file__tell(pCtx,pFileF3,&posFileF3)  );
	INFOP("tell",("Position F3[%d]",(SG_uint32)posFileF3));
	VERIFY_COND("tell",(posFileF3 == lenFileF1));

	VERIFY_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,MY_CAT_COMMAND,pArgVec,NULL,pFileF3,NULL,&exitStatusChild)  );
	VERIFY_COND("child status",(exitStatusChild == 0));
	INFOP("/bin/cat f1 >f3",("Child exit status is [%d]",exitStatusChild));
	VERIFY_ERR_CHECK(  SG_file__tell(pCtx,pFileF3,&posFileF3)  );
	INFOP("tell",("Position F3[%d]",(SG_uint32)posFileF3));
	VERIFY_COND("tell",(posFileF3 == 2*lenFileF1));

	VERIFY_ERR_CHECK(  SG_exec__exec_sync__files(pCtx,MY_CAT_COMMAND,pArgVec,NULL,pFileF3,NULL,&exitStatusChild)  );
	VERIFY_COND("child status",(exitStatusChild == 0));
	INFOP("/bin/cat f1 >f3",("Child exit status is [%d]",exitStatusChild));
	VERIFY_ERR_CHECK(  SG_file__tell(pCtx,pFileF3,&posFileF3)  );
	INFOP("tell",("Position F3[%d]",(SG_uint32)posFileF3));
	VERIFY_COND("tell",(posFileF3 == 3*lenFileF1));

	SG_FILE_NULLCLOSE(pCtx, pFileF3);

	// get length of F3 and see how it compares with F1.

	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx,pPathF3,&lenFileF3,NULL)  );
	INFOP("lenCheck",("Length F1[%d] Length F3[%d]",(SG_uint32)lenFileF1,(SG_uint32)lenFileF3));
	VERIFY_COND("tell",(lenFileF3 == 3*lenFileF1));

	//////////////////////////////////////////////////////////////////

	// fall through to common cleanup.

fail:
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec);
	SG_PATHNAME_NULLFREE(pCtx, pPathTempDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathF1);
	SG_PATHNAME_NULLFREE(pCtx, pPathF2);
	SG_PATHNAME_NULLFREE(pCtx, pPathF3);
	SG_FILE_NULLCLOSE(pCtx, pFileF1);
	SG_FILE_NULLCLOSE(pCtx, pFileF2);
	SG_FILE_NULLCLOSE(pCtx, pFileF3);
}

//////////////////////////////////////////////////////////////////

MyMain()
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  MyFn(test1)(pCtx)  );
	BEGIN_TEST(  MyFn(test2)(pCtx)  );

	TEMPLATE_MAIN_END;
}

//////////////////////////////////////////////////////////////////

#undef MY_CAT_COMMAND

#undef MyMain
#undef MyDcl
#undef MyFn
