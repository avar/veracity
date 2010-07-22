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
 * @file sg_diff_ext_typedefs.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_DIFF_EXT_TYPEDEFS_H
#define H_SG_DIFF_EXT_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * A routine to use an external tool to compare one pair of files.
 * Each callback handles a different builtin or custom tool.
 */
typedef void (SG_diff_ext__compare_callback)(
    SG_context * pCtx,

    // Allow pPathname0 or pPathname1, but not both, to be NULL, meaning empty file
    const SG_pathname * pPathname0,
    const SG_pathname * pPathname1,

    const char * szLabel0,
    const char * szLabel1,

    SG_file * pFileStdout, // If passed NULL, send output to STDOUT (otherwise to the file)
    SG_file * pFileStderr, // If passed NULL, send error information to STDERR (otherwise to the file)

    SG_bool * pbFoundChanges);


//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_DIFF_EXT_TYPEDEFS_H
