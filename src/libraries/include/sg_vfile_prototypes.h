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
 * @file sg_vfile_prototypes.h
 *
 * @details A vfile is a simple text file containing a
 * json object (a vhash serialized).
 *
 * This module provides functions to read and write
 * such files in a concurrency-safe way.
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_VFILE_PROTOTYPES_H
#define H_SG_VFILE_PROTOTYPES_H

BEGIN_EXTERN_C;

/**
 * Read a vfile.  Don't expect to use this vhash to write back to the file.
 * You can not (should not) do that.  Instead, use SG_vfile__begin, which
 * returns a vhash.
 *
 * Use this function only for cases where you need to read the
 * information but do not plan to modify it.
 *
 * If the file is zero-length, this function will succeed, with pvh
 * set to NULL.
 */
void SG_vfile__slurp(
	SG_context*,
	const SG_pathname* pPath,
	SG_vhash** ppvh /**< Caller must free */
	);

/**
 * If you want to modify a vfile, use SG_vfile__begin and SG_vfile__end.
 * __begin opens the file, reads the whole thing into a vhash which is
 * returned, and leaves the file open and LOCKED.
 * __end writes new contents and closes the file. */
void SG_vfile__begin(
	SG_context*,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	SG_file_flags mode, /**< Pass the same flags you would pass to SG_file__open */
	SG_vhash** ppvh, /**< If there are no errors, the resulting vhash table will be returned here. */
	SG_vfile** ppvf
	);

void SG_vfile__end(
	SG_context*,
	SG_vfile** ppvf,
	const SG_vhash* pvh /**< Pass NULL here if the file was opened in
						 * RDONLY mode.  If the file is writable and
						 * you pass NULL, the file will end up
						 * truncated to zero length. */
	);

/**
 * If you call SG_vfile__begin and later decide you don't want
 * to write, call this to abort the transaction. */
void SG_vfile__abort(
	SG_context*,
	SG_vfile** ppvf
	);

/**
 * Re-read the contents of an already opened VFILE.
 */
void SG_vfile__reread(
	SG_context* pCtx,
	SG_vfile * pvf,
	SG_vhash** ppvh);

/* TODO It could be argued that every SG_vhash__add/update function
 * should have a counterpart here.  For now, I am simply adding them
 * as I need them. */

void SG_vfile__add__vhash(
	SG_context*,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	const char* putf8Key,
	const SG_vhash* pHashValue /**< The value for the given key.  You still own this pointer. */
	);

void SG_vfile__update__vhash(
	SG_context*,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	const char* putf8Key,
	const SG_vhash* pHashValue /**< The value for the given key.  You still own this pointer. */
	);

void SG_vfile__update__string__sz(
	SG_context*,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	const char* putf8Key,
	const char* putf8Value /**< The value for the given key.  This
							* pointer continues to be owned by the
							* caller.  The vhash table will make its
							* own copy of this (in its string
							* pool). */
	);

void SG_vfile__update__bool(
	SG_context*,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	const char* putf8Key,
	SG_bool value);
void SG_vfile__update__double(
	SG_context*,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	const char* putf8Key,
	double value);
void SG_vfile__update__int64(
	SG_context*,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	const char* putf8Key,
	SG_int64 value);
void SG_vfile__update__varray(
	SG_context*,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	const char* putf8Key,
	SG_varray ** ppValue); /**< We steal your pointer, free it(!!), and null it. */

void SG_vfile__remove(
	SG_context*,
	const SG_pathname* pPath, /**< The path of the file containing the JSON text */
	const char* putf8Key
	);

END_EXTERN_C;

#endif //H_SG_VFILE_PROTOTYPES_H
