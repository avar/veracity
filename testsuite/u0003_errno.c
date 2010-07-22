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

// testsuite/unittests/u0003_errno.c
// some trivial tests to exercise sg_errno.h
//////////////////////////////////////////////////////////////////

#include <sg.h>

#include "unittests.h"

//////////////////////////////////////////////////////////////////

int u0003_errno__test_errno(void)
{
	char buf[SG_ERROR_BUFFER_SIZE];

	// verify argument validation.

	SG_error__get_message(SG_ERR_UNSPECIFIED,NULL,0);

	// verify that we get the expected message.

	SG_error__get_message(SG_ERR_UNSPECIFIED,buf,sizeof(buf));
	VERIFY_COND( "test_errno", (strcmp(buf,"Error 1 (sglib): Unspecified error") == 0) );

	// verify that we get a properly truncated message for a short buffer.

	SG_error__get_message(SG_ERR_UNSPECIFIED,buf,10);
	VERIFY_COND( "test_errno", (strcmp(buf,"Error 1 (") == 0) );

	SG_error__get_message(SG_ERR_UNSPECIFIED,buf,1);
	VERIFY_COND( "test_errno", (buf[0] == 0) );

	return 1;
}

int u0003_errno__get_message(void)
{
	// we now have different types of errors.  verify that we get the proper message.

	char buf[SG_ERROR_BUFFER_SIZE];

	// errno-based messages will vary by platform.
	SG_error__get_message(SG_ERR_ERRNO(ENOENT), buf, sizeof(buf));
	VERIFYP_COND("get_message",(strcmp(buf,"No such file or directory")==0),
			("ENOENT: received[%s] expected[%s]",buf,"No such file or directory"));

#if defined(WINDOWS)
	// GetLastError-based messages
	SG_error__get_message(SG_ERR_GETLASTERROR(ERROR_PATH_NOT_FOUND), buf, sizeof(buf));
	VERIFYP_COND("get_message",(strcmp(buf,"The system cannot find the path specified. ")==0),
			("ERROR_PATH_NOT_FOUND: received[%s] expected[%s]",buf,"The system cannot find the path specified. "));
#endif

	// sg-library-based error codes.
	SG_error__get_message(SG_ERR_INVALIDARG, buf, sizeof(buf));
	VERIFYP_COND("get_message",(strcmp(buf,"Error 2 (sglib): Invalid argument")==0),
		("SG_ERR_INVALIDARG: received[%s] expected[%s]",buf,"Error 2 (sglib): Invalid argument"));

	return 1;
}

TEST_MAIN(u0003_errno)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0003_errno__test_errno()  );
	BEGIN_TEST(  u0003_errno__get_message()  );

	TEMPLATE_MAIN_END;
}



