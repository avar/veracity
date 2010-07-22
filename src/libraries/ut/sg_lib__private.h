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
 * @file sg_lib__private.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_LIB__PRIVATE_H
#define H_SG_LIB__PRIVATE_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

typedef struct _sg_lib__global_data		 	sg_lib__global_data;
typedef struct _sg_lib_utf8__global_data	sg_lib_utf8__global_data;

//////////////////////////////////////////////////////////////////

/**
 * Initialize and cleanup UTF-8-private global data.
 *
 * These routines should only be called by
 * SG_lib__global_initialize() and SG_lib__global_cleanup().
 */
void sg_lib_utf8__global_initialize(SG_context * pCtx, sg_lib_utf8__global_data ** ppUtf8GlobalData);
void sg_lib_utf8__global_cleanup(SG_context * pCtx, sg_lib_utf8__global_data ** ppUtf8GlobalData);

void sg_lib_localsettings__global_cleanup(SG_context * pCtx);
void sg_lib_uridispatch__global_cleanup(SG_context * pCtx);

/**
 * Fetch the pointer for utf8 global data.
 */
void sg_lib__fetch_utf8_global_data(SG_context * pCtx, sg_lib_utf8__global_data ** ppUtf8GlobalData);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_LIB__PRIVATE_H
