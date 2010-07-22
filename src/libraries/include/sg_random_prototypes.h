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
 * Currently this is just for quick-and-dirty random content
 * generation. The implementation uses rand(). If we need more
 * than this at some point, consider rewriting it, or adding
 * to it so that we have separate functions for "fast" versus
 * "strong" random content generation.
 */

#ifndef H_SG_RANDOM_H
#define H_SG_RANDOM_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

void SG_random__seed(SG_uint32);

SG_bool SG_random_bool(void);

SG_uint32 SG_random_uint32(SG_uint32 n); /// Random number in range [0,n)
SG_uint32 SG_random_uint32__2(SG_uint32 min, SG_uint32 max); /// Random number in range [min,max]

SG_uint64 SG_random_uint64(SG_uint64 n); /// Random number in range [0,n)
SG_uint64 SG_random_uint64__2(SG_uint64 min, SG_uint64 max); /// Random number in range [min,max]

void SG_random_bytes(SG_byte * pBuffer, SG_uint32 bufferLength);

void SG_random__write_random_bytes_to_file(SG_context * pCtx, SG_file * pFile, SG_uint64 numBytes);
void SG_random__generate_random_binary_file(SG_context * pCtx, SG_pathname * pPath, SG_uint64 length);

void SG_random__select_random_rbtree_node(SG_context * pCtx, SG_rbtree * pRbtree, const char ** ppKey, void ** ppAssocData);
void SG_random__select_2_random_rbtree_nodes(SG_context * pCtx, SG_rbtree * pRbtree,
	const char ** ppKey1, void ** ppAssocData1,
	const char ** ppKey2, void ** ppAssocData2);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_RANDOM_H
