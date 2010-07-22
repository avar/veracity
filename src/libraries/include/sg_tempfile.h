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

#ifndef H_SG_TEMPFILE_H
#define H_SG_TEMPFILE_H

BEGIN_EXTERN_C

//////////////////////////////////////////////////////////////////
struct _sg_tempfile_handle
{
	SG_file* pFile;
	SG_pathname* pPathname;
};
typedef struct _sg_tempfile_handle SG_tempfile;

void SG_tempfile__open_new(SG_context* pCtx, SG_tempfile** ppHandle);
void SG_tempfile__close(SG_context* pCtx, SG_tempfile* pHandle);
void SG_tempfile__delete(SG_context* pCtx, SG_tempfile** ppHandle);
void SG_tempfile__close_and_delete(SG_context* pCtx, SG_tempfile** ppHandle);

//////////////////////////////////////////////////////////////////

END_EXTERN_C

#endif //H_SG_TEMPFILE_H
