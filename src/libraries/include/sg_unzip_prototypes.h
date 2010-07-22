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
 * @file sg_unzip_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_UNZIP_PROTOTYPES_H
#define H_SG_UNZIP_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_unzip__open(SG_context* pCtx, const SG_pathname* pPath, SG_unzip** ppResult);

void SG_unzip__goto_first_file(SG_context* pCtx, SG_unzip* s, SG_bool* pb, const char** ppsz_name, SG_uint64* piLength);
void SG_unzip__goto_next_file(SG_context* pCtx, SG_unzip* s, SG_bool *pb, const char** ppsz_name, SG_uint64* piLength);
void SG_unzip__locate_file(SG_context* pCtx, SG_unzip* s, const char* psz_filename, SG_bool* pb, SG_uint64* piLength);

void SG_unzip__currentfile__open(SG_context* pCtx, SG_unzip* s);
void SG_unzip__currentfile__read(SG_context* pCtx, SG_unzip* s, SG_byte* pBuf, SG_uint32 iLenBuf, SG_uint32* piBytesRead);
void SG_unzip__currentfile__close(SG_context* pCtx, SG_unzip* s);

void SG_unzip__fetch_vhash(SG_context* pCtx, SG_unzip* punzip, const char* psz_filename, SG_vhash** ppvh);
void SG_unzip__fetch_next_file_into_file(SG_context* pCtx, SG_unzip* punzip, SG_pathname* pPath, SG_bool* pb, const char** ppsz_name, SG_uint64* piLength);

void SG_unzip__nullclose(SG_context* pCtx, SG_unzip** ppunzip);

END_EXTERN_C;

#endif //H_SG_UNZIP_PROTOTYPES_H

