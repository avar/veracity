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
 * @file sg_lib_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_LIB_PROTOTYPES_H
#define H_SG_LIB_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Global initialization of the sg library.  This routine
 * should be called at start up to allocate and initialize
 * any global variables used by this library.
 *
 * These variables are global to the library, but are not
 * exported/visible outside of the library.
 */
void SG_lib__global_initialize(SG_context * pCtx);

/**
 * Global cleanup routine for the sg library.  This routine
 * should be called before exiting to allow us to cleanup
 * any global variables, locks, and etc.
 */
void SG_lib__global_cleanup(SG_context * pCtx);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_LIB_PROTOTYPES_H
