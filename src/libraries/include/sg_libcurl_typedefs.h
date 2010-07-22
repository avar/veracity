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
 * @file sg_libcurl_typedefs.h
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_LIBCURL_TYPEDEFS_H
#define H_SG_LIBCURL_TYPEDEFS_H

BEGIN_EXTERN_C;

typedef struct
{
	SG_context* pCtx;
	SG_file* pFile;
} SG_curl_state_file;

typedef struct
{
	SG_context* pCtx;
	SG_string* pstrResponse;
} SG_curl_state_string;

END_EXTERN_C;

#endif//H_SG_LIBCURL_TYPEDEFS_H

