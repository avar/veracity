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
 * @file sg_stream_prototypes.h
 *
 */

#ifndef H_SG_STREAM_PROTOTYPES_H
#define H_SG_STREAM_PROTOTYPES_H

BEGIN_EXTERN_C

void SG_seekreader__alloc(
        SG_context * pCtx,
        void* pUnderlying,
        SG_uint64 iHeaderOffset,
        SG_uint64 iLength,
        SG_nonstream__func__read*,
        SG_stream__func__seek*,
        SG_stream__func__close*,
        SG_seekreader** ppsr
        );

void SG_seekreader__read(
        SG_context * pCtx,
        SG_seekreader* psr,
        SG_uint64 iPos,
        SG_uint32 iNumBytesWanted,
        SG_byte* pBytes,
        SG_uint32* piNumBytesRetrieved /*optional*/
        );

void SG_seekreader__length(
        SG_context * pCtx,
        SG_seekreader* psr,
        SG_uint64* piLength
        );

void SG_seekreader__close(
        SG_context * pCtx,
        SG_seekreader* psr
        );

void SG_seekreader__alloc__for_file(
        SG_context * pCtx,
        SG_pathname* pPath,
        SG_uint64 iHeaderOffset,
        SG_seekreader** ppstrm
        );

void SG_readstream__alloc(
        SG_context * pCtx,
        void* pUnderlying,
        SG_stream__func__read*,
        SG_stream__func__close*,
        SG_readstream** ppstrm
        );

void SG_readstream__read(
        SG_context * pCtx,
        SG_readstream* pstrm,
        SG_uint32 iNumBytesWanted,
        SG_byte* pBytes,
        SG_uint32* piNumBytesRetrieved /*optional*/,
        SG_bool* pb_done
        );

void SG_readstream__get_count(
        SG_context * pCtx,
        SG_readstream* pstrm,
        SG_uint64* pi
        );

void SG_readstream__close(
        SG_context * pCtx,
        SG_readstream* pstrm
        );

void SG_readstream__alloc__for_file(
        SG_context * pCtx,
        SG_pathname* pPath,
        SG_readstream** ppstrm
        );

void SG_writestream__alloc(
        SG_context * pCtx,
        void* pUnderlying,
        SG_stream__func__write*,
        SG_stream__func__close*,
        SG_writestream** ppstrm
        );

void SG_writestream__write(
        SG_context * pCtx,
        SG_writestream* pstrm,
        SG_uint32 iNumBytes,
        SG_byte* pBytes,
        SG_uint32* piNumBytesWritten /*optional*/
        );

void SG_writestream__get_count(
        SG_context * pCtx,
        SG_writestream* pstrm,
        SG_uint64* pi
	);

void SG_writestream__close(
        SG_context * pCtx,
        SG_writestream* pstrm
        );

void SG_writestream__alloc__for_file(
        SG_context * pCtx,
        SG_pathname* pPath,
        SG_writestream** ppstrm
        );

END_EXTERN_C

#endif

