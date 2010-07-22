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
 * @file sg_inv_dirs_typedefs.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_INV_DIRS_TYPEDEFS_H
#define H_SG_INV_DIRS_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

typedef struct _inv_dir   SG_inv_dir;
typedef struct _inv_dirs  SG_inv_dirs;


typedef void (SG_inv_dirs_foreach_in_queue_for_parking_callback)(SG_context * pCtx,
																 const char * pszRepoPath_Key,
																 SG_inv_entry * pInvEntry_Value,
																 void * pVoidData);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_INV_DIRS_TYPEDEFS_H
