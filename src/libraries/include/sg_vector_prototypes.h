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
 * @file sg_vector_prototypes.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_VECTOR_PROTOTYPES_H
#define H_SG_VECTOR_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Allocate a new vector.  This is a variable-length/growable array of pointers.
 *
 * The suggested-initial-size is a guess at how much space to initially allocate
 * in the internal array; this (along with SG_vector__setChunkSize()) allow you
 * to control how often we have to realloc() internally.  This only controls the
 * alloc() -- it does not affect the in-use length of the vector.
 *
 * Regardless of the allocated size, the vector initially has an in-use length of 0.
 */
void SG_vector__alloc(SG_context* pCtx, SG_vector ** ppVector, SG_uint32 suggestedInitialSize);

#if defined(DEBUG)
#define SG_VECTOR__ALLOC(pCtx,ppVector,s)			SG_STATEMENT(  SG_vector * _p = NULL;										\
																   SG_vector__alloc(pCtx,&_p,s);								\
																   _sg_mem__set_caller_data(_p,__FILE__,__LINE__,"SG_vector");	\
																   *(ppVector) = _p;											)
#else
#define SG_VECTOR__ALLOC(pCtx,ppVector,s)			SG_vector__alloc(pCtx,ppVector,s)
#endif

/**
 * Free the given vector.  Since the vector does not own the pointers in the vector,
 * it only frees the vector itself and our internal array.
 */
void SG_vector__free(SG_context * pCtx, SG_vector * pVector);

/**
 * Free the given vector.  But first call the given callback function for each
 * element in the vector.
 */
void SG_vector__free__with_assoc(SG_context * pCtx, SG_vector * pVector, SG_free_callback * pcbFreeValue);

/**
 * Extend the in-use length of the vector and store the given pointer in the new cell.
 * Return the index of the new cell.
 *
 * We store the pointer but we do not take ownership of it.
 */
void SG_vector__append(SG_context* pCtx, SG_vector * pVector, void * pValue, SG_uint32 * pIndexReturned);

/**
 * Remove the last pointer in the list.
 * Return it in the optional ppValue output parameter.
 */
void SG_vector__pop_back(SG_context * pCtx, SG_vector * pVector, void ** ppValue);

/**
 * Set the in-use length to a known size.  This is like a traditional
 * dimension statement on a regular array.  Space is allocated and the
 * in-use length is set.  Newly created cells are initialized to NULL.
 *
 * Use this with __get() and __set() if this makes more sense than calling
 * __append() in your application.
 *
 * This *CANNOT* be used to shorten the in-use length of a vector.
 */
void SG_vector__reserve(SG_context* pCtx, SG_vector * pVector, SG_uint32 size);

/**
 * Fetch the pointer value in cell k in the vector.
 * This returns an error if k is >= the in-use length.
 */
void SG_vector__get(SG_context* pCtx, const SG_vector * pVector, SG_uint32 k, void ** ppValue);

/**
 * Set the pointer value in call k in the vector.
 * This returns an error if k is >= the in-use-length.
 */
void SG_vector__set(SG_context* pCtx, SG_vector * pVector, SG_uint32 k, void * pValue);

/**
 * Set the in-use length to 0.  This does not realloc() down.
 */
void SG_vector__clear(SG_context* pCtx, SG_vector * pVector);

/**
 * Returns the in-use length.
 */
void SG_vector__length(SG_context* pCtx, const SG_vector * pVector, SG_uint32 * pLength);

/**
 * Sets the chunk-size for our internal calls to realloc().
 */
void SG_vector__setChunkSize(SG_context* pCtx, SG_vector * pVector, SG_uint32 size);

/**
 * Call the given function with each item in the vector.
 */
void SG_vector__foreach(SG_context * pCtx,
						SG_vector * pVector,
						SG_vector_foreach_callback * pfn,
						void * pVoidCallerData);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_VECTOR_PROTOTYPES_H
