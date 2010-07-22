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
 * @file sg_bitvec_prototypes.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_BITVEC_PROTOTYPES_H
#define H_SG_BITVEC_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Allocate a 256-bit bit-vector.
 *
 * All bits will be initialized to zero.
 */
void SG_bitvec256__alloc(SG_context* pCtx, SG_bitvec256** ppNew);

/**
 * Allocate a 256-bit bit-vector with an initial value.
 *
 * @a szHex represents a 64 character hex string of initial values for the bit-vector.
 */
void SG_bitvec256__alloc__hex(SG_context* pCtx, SG_bitvec256** ppNew, const char * szHex);

/**
 * Free a bit-vector.
 */
void SG_bitvec256__free(SG_context * pCtx, SG_bitvec256 * pbv);

//////////////////////////////////////////////////////////////////

/**
 * Set an individual bit in the bit-vector.  v is index of the bit.
 */
void SG_bitvec256__set_bit__byte(SG_context* pCtx, SG_bitvec256 * pbv, SG_byte v);

/**
 * Get an individual bit in the bit-vector.  v is index of the bit.
 */
SG_bool SG_bitvec256__get_bit__byte(const SG_bitvec256 * pbv, SG_byte v);

//////////////////////////////////////////////////////////////////

/**
 * Call __set_bit__byte for each byte in given string.  We do not clear
 * bit-vector first.  Note that you can't set bit[0] using this routine
 * because the NUL byte terminates the string.
 */
void SG_bitvec256__set_bits__sz(SG_context* pCtx, SG_bitvec256 * pbv, const SG_byte * sz);

//////////////////////////////////////////////////////////////////

/**
 * Initialize bit-vector with given hex-string.
 *
 * @a szHex represents a 64 character hex string of initial values for the bit-vector.
 */
void SG_bitvec256__set__hex(SG_context* pCtx, SG_bitvec256 * pbv, const char * szHex);

/**
 * Get the value of the bit-vector as a hex-string.  This call will write
 * exactly 65 bytes (64 hex data + 1 for null).
 * Caller must supply hex-string buffer containing at least 65 bytes.
 */
void SG_bitvec256__get__hex(SG_context* pCtx, const SG_bitvec256 * pbv, char * bufHex);

//////////////////////////////////////////////////////////////////

/**
 * Clear all bits in the bit-vector.
 */
void SG_bitvec256__clear_all_bits(SG_context* pCtx, SG_bitvec256 * pbv);

/**
 * Compare 2 bit-vectors.
 */
void SG_bitvec256__equal(SG_context* pCtx, const SG_bitvec256 * pbv1, const SG_bitvec256 * pbv2, SG_bool * pbEqual);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_BITVEC_PROTOTYPES_H
