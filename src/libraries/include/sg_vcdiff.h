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

#ifndef H_SG_VCDIFF_H
#define H_SG_VCDIFF_H

BEGIN_EXTERN_C

typedef struct _sg_vcdiff_undeltify_state SG_vcdiff_undeltify_state;


void	SG_vcdiff__deltify__files(SG_context* pCtx, SG_pathname* pPathSource, SG_pathname* pPathTarget, SG_pathname* pPathDelta);
void	SG_vcdiff__deltify__streams(SG_context* pCtx, SG_seekreader* psrSource, SG_readstream* pstrmTarget, SG_writestream* pstrmDelta);

void	SG_vcdiff__undeltify__files(SG_context* pCtx, SG_pathname* pPathSource, SG_pathname* pPathTarget, SG_pathname* pPathDelta);
void	SG_vcdiff__undeltify__streams(SG_context* pCtx, SG_seekreader* psrSource, SG_writestream* pstrmTarget, SG_readstream* pstrmDelta);

void SG_vcdiff__undeltify__begin(SG_context* pCtx, SG_seekreader* psr_source, SG_readstream* pstrm_delta, SG_vcdiff_undeltify_state** ppst );
void SG_vcdiff__undeltify__chunk(SG_context* pCtx, SG_vcdiff_undeltify_state* pst, SG_byte** ppResult, SG_uint32* pi_got);
void SG_vcdiff__undeltify__end(SG_context* pCtx, SG_vcdiff_undeltify_state* pst);

END_EXTERN_C

#endif

