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

/**
 * Print error message on STDERR for the given error and convert the
 * SG_ERR_ value into a proper ExitStatus for the platform.
 *
 * TODO investigate if (-1,errno) is appropriate for all 3 platforms.
 *
 * DO NOT WRAP THE CALL TO THIS FUNCTION WITH SG_ERR_IGNORE() (because we
 * need to be able to access the original error/status).
 */
static int _compute_exit_status_and_print_error_message(SG_context * pCtx)
{
	SG_error err;
	char bufErrorMessage[SG_ERROR_BUFFER_SIZE+1];
	int exitStatus;

	SG_context__get_err(pCtx,&err);

	if (err == SG_ERR_OK)
	{
		exitStatus = 0;
	}
	else if (err == SG_ERR_USAGE)
	{
		// We special case SG_ERR_USAGE because the code that signalled that
		// error has already printed whatever context-specific usage info.

		exitStatus = -1;
	}
	else
	{
		SG_string * pStrContextDump = NULL;

		SG_error__get_message(err, bufErrorMessage, SG_ERROR_BUFFER_SIZE);

		SG_context__err_to_string(pCtx,&pStrContextDump);

		SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR, "%s\n",
								   (  (pStrContextDump)
										? SG_string__sz(pStrContextDump)
										: bufErrorMessage))  );

		SG_STRING_NULLFREE(pCtx, pStrContextDump);

		// the shell and command tools in general only expect ERRNO-type errors.

		if (SG_ERR_TYPE(err) == __SG_ERR__ERRNO__)
			exitStatus = SG_ERR_ERRNO_VALUE(err);
		else
			exitStatus = 1;  // this means we threw an error, there's no unix error code that will
							 // cover all the possible errors that could come out of vv, so this
							 // one will have to do (EPERM)
	}

	return exitStatus;
}

static int _my_main(SG_context * pCtx)
{
	int exitStatus;

    SG_ERR_CHECK(  sg_zing__validate_templates(pCtx)  );

fail:
	exitStatus = _compute_exit_status_and_print_error_message(pCtx);		// DO NOT WRAP THIS WITH SG_ERR_IGNORE

	return exitStatus;
}

#if defined(WINDOWS)
int wmain(void)
#else
int main(void)
#endif
{
	SG_context * pCtx = NULL;
	int exitStatus = 0;

	SG_context__alloc(&pCtx);
	SG_ERR_CHECK(  SG_lib__global_initialize(pCtx)  );

	SG_ERR_CHECK(  exitStatus = _my_main(pCtx)  );

fail:
	SG_ERR_IGNORE(  SG_lib__global_cleanup(pCtx)  );
	SG_CONTEXT_NULLFREE(pCtx);

	return exitStatus;
}

