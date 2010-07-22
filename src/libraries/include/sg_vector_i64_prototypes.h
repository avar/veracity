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
 * @file sg_vector_i64_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_VECTOR_I64_PROTOTYPES_H
#define H_SG_VECTOR_I64_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Allocate a new vector.  This is a variable-length/growable array of SG_int64.
 *
 * The suggested-initial-size is a guess at how much space to initially allocate
 * in the internal array; this (along with sg_vector_i64__setChunkSize()) allow you
 * to control how often we have to realloc() internally.  This only controls the
 * alloc() -- it does not affect the in-use length of the vector.
 *
 * Regardless of the allocated size, the vector initially has an in-use length of 0.
 */
void SG_vector_i64__alloc(SG_context* pCtx, SG_vector_i64 ** ppVector, SG_uint32 suggestedInitialSize);

/**
 * Allocate a new vector from a varray, every element of which must be
 * an int.
 */
void SG_vector_i64__alloc__from_varray(SG_context* pCtx, SG_varray* pva, SG_vector_i64 ** ppVector);

/**
 * Free the given vector.
 */
void SG_vector_i64__free(SG_context * pCtx, SG_vector_i64 * pVector);

/**
 * Extend the in-use length of the vector and store the given value in the new cell.
 * Return the index of the new cell.
 */
void SG_vector_i64__append(SG_context* pCtx, SG_vector_i64 * pVector, SG_int64 value, SG_uint32 * pIndexReturned);

/**
 * Set the in-use length to a known size.  This is like a traditional
 * dimension statement on a regular array.  Space is allocated and the
 * in-use length is set.  Newly created cells are initialized to zero.
 *
 * Use this with __get() and __set() if this makes more sense than calling
 * __append() in your application.
 *
 * This *CANNOT* be used to shorten the in-use length of a vector.
 */
void SG_vector_i64__reserve(SG_context* pCtx, SG_vector_i64 * pVector, SG_uint32 size);

/**
 * Fetch the value in cell k in the vector.
 * This returns an error if k is >= the in-use length.
 */
void SG_vector_i64__get(SG_context* pCtx, const SG_vector_i64 * pVector, SG_uint32 k, SG_int64 * pValue);

/**
 * Set the value in call k in the vector.
 * This returns an error if k is >= the in-use-length.
 */
void SG_vector_i64__set(SG_context* pCtx, SG_vector_i64 * pVector, SG_uint32 k, SG_int64 value);

/**
 * Set the in-use length to 0.  This does not realloc() down.
 */
void SG_vector_i64__clear(SG_context* pCtx, SG_vector_i64 * pVector);

/**
 * Returns the in-use length.
 */
void SG_vector_i64__length(SG_context* pCtx, const SG_vector_i64 * pVector, SG_uint32 * pLength);

/**
 * Sets the chunk-size for our internal calls to realloc().
 */
void SG_vector_i64__setChunkSize(SG_context* pCtx, SG_vector_i64 * pVector, SG_uint32 size);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_VECTOR_I64_PROTOTYPES_H
