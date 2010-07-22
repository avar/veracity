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

// sg_hex.h
// Utility routines for parsing hex strings.
//////////////////////////////////////////////////////////////////

#ifndef H_SG_HEX_H
#define H_SG_HEX_H

BEGIN_EXTERN_C

//////////////////////////////////////////////////////////////////

void SG_hex__parse_one_hex_char(SG_context* pCtx, char c, SG_uint8*);
void SG_hex__parse_hex_byte(SG_context* pCtx, const char*, SG_uint8*);
void SG_hex__parse_hex_string(SG_context* pCtx, const char * szIn, SG_uint32 lenIn,
								  SG_byte * pByteBuf, SG_uint32 lenByteBuf,
								  SG_uint32 * pLenInBytes);
void SG_hex__parse_hex_uint8(SG_context* pCtx, const char * szIn, SG_uint8 * pui8Result);
void SG_hex__parse_hex_uint32(SG_context* pCtx, const char * szIn, SG_uint32 lenIn, SG_uint32 * pui32Result);
void SG_hex__parse_hex_uint64(SG_context* pCtx, const char * szIn, SG_uint32 lenIn, SG_uint64 * pui64Result);
void SG_hex__validate_hex_string(SG_context* pCtx, const char * szIn, SG_uint32 lenIn);
char * SG_hex__format_buf(char * pbuf, const SG_byte * pV, SG_uint32 lenV);
char * SG_hex__format_uint8(char * pbuf, SG_uint8 v);
char * SG_hex__format_uint16(char * pbuf, SG_uint16 v);
char * SG_hex__format_uint32(char * pbuf, SG_uint32 v);
char * SG_hex__format_uint64(char * pbuf, SG_uint64 v);

//////////////////////////////////////////////////////////////////

END_EXTERN_C

#endif//H_SG_HEX_H
