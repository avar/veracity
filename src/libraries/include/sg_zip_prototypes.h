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
 * @file sg_zip_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_ZIP_PROTOTYPES_H
#define H_SG_ZIP_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_zip__open(SG_context* pCtx, const SG_pathname* pathname, SG_zip** pp);

void SG_zip__begin_file(SG_context* pCtx,  SG_zip* zi, const char* filename);
void SG_zip__write(SG_context* pCtx, SG_zip* zi, const SG_byte* buf, SG_uint32 len);
void SG_zip__end_file(SG_context* pCtx, SG_zip* file);

//adds an empty folder to the archive
void SG_zip__add_folder(SG_context* pCtx,  SG_zip* zi, const char* foldername);

void SG_zip__nullclose (SG_context* pCtx, SG_zip** pzi);

void SG_zip__store__bytes(SG_context* pCtx, SG_zip* pz, const char* filename, const SG_byte* buf, SG_uint32 len);
void SG_zip__store__file(SG_context* pCtx, SG_zip* pz, const char* filename, const SG_pathname* pPath);

END_EXTERN_C;

#endif //H_SG_ZIP_PROTOTYPES_H

