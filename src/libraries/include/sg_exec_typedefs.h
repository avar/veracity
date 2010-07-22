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
 * @file sg_exec_typedefs.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_EXEC_TYPEDEFS_H
#define H_SG_EXEC_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Exit status from a child process.  This is command-specific and
 * somewhat platform-specific.  I have these typedefs so that we
 * don't confuse them with SG_error error codes (and as opposed to
 * adding a different SG_ERR_ domain).
 *
 * Generally, we can only test for 0 and non-zero.
 * The command-line diff/merge utilities typically return 0,1,2,3
 * to indicate the result whether there were changes, conflicts, errors.
 * YMMV.
 */
typedef SG_int32 SG_exit_status;

/**
 * Process Identifier.  It's an SG_uint64
 */
typedef SG_uint64 SG_process_id;

#define SG_EXIT_STATUS_OK		((SG_exit_status) 0)

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_EXEC_TYPEDEFS_H
