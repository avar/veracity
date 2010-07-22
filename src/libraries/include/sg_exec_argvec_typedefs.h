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
 * @file sg_exec_argvec_typedefs.h
 *
 * @details Wrapper and typedefs for argument vector used by sg_exec.
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_EXEC_ARGVEC_TYPEDEFS_H
#define H_SG_EXEC_ARGVEC_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Argument vector for invoking an external executable.
 *
 * This is basically a SG_vector of SG_string that we own.
 * And we have some
 *
 * The program to exec IS NOT in the vector.
 */
typedef struct _sg_exec_argvec SG_exec_argvec;

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_EXEC_ARGVEC_TYPEDEFS_H
