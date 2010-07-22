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
 * @file sg_stream_typedefs.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_STREAM_TYPEDEFS_H
#define H_SG_STREAM_TYPEDEFS_H

BEGIN_EXTERN_C;

typedef void SG_stream__func__seek(
	SG_context * pCtx,
    void* pUnderlying,
    SG_uint64 iPos
    );

typedef void SG_stream__func__close(
	SG_context * pCtx,
    void* pUnderlying
    );

typedef void SG_stream__func__read(
	SG_context * pCtx,
    void* pUnderlying,
    SG_uint32 iNumBytesWanted,
    SG_byte* pBytes,
    SG_uint32* piNumBytesRetrieved /*optional*/,
    SG_bool* pb_done
    );

typedef void SG_nonstream__func__read(
	SG_context * pCtx,
    void* pUnderlying,
    SG_uint32 iNumBytesWanted,
    SG_byte* pBytes,
    SG_uint32* piNumBytesRetrieved /*optional*/
    );

typedef void SG_stream__func__write(
	SG_context * pCtx,
    void* pUnderlying,
    SG_uint32 iNumBytes,
    SG_byte* pBytes,
    SG_uint32* piNumBytesWritten /*optional*/
    );

typedef struct _sg_seekreader SG_seekreader;
typedef struct _sg_readstream SG_readstream;
typedef struct _sg_writestream SG_writestream;

END_EXTERN_C;

#endif//H_SG_STREAM_TYPEDEFS_H

