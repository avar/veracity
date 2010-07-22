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
 * @file sg_jsondb_prototypes.h
 *
 * @details A jsondb is a collection of JSON objects stored in a DB
 * whose primary purpose is to facilitate speedy manipulation of elements
 * within a json object.
 *
 * A jsondb can store any number of JSON objects, each referenced by
 * a unique object name.  Most routines assume a "current" JSON object.  A
 * reference to the current JSON object is stored in the SG_jsondb handle.  It
 * is possible to have no current object.  Calling a routine that requires a
 * current object when in this state results in a SG_JSONDB_NO_CURRENT_OBJECT
 * error.
 *
 * The sglib-ified version of each JSON object is a tree with these
 * characteristics:
 *
 * - Every node has a name and a value.
 * - The root node's name is always "/".
 * - A non-root node's name is its key in the parent vhash or index in the
 *   parent varray.
 * - The scalar variant types (sz, double, null, int64) can only be leaves.
 * - Vhashes and varrays are leaves when empty, and interior nodes when
 *   non-empty.
 * - If the root node is a scalar variant, the tree has only one node.
 *
 * For all the add functions, any non-existent descendants will be created if
 * bAddRecursive is SG_TRUE.  If bAddRecursive is SG_FALSE and the parent of
 * the new node doesn't already exist, SG_ERR_JSONDB_PARENT_DOESNT_EXIST
 * is thrown.  In either case, if the leaf being added already exists,
 * SG_ERR_EXISTS is thrown.
 *
 * Varrays have no keys, only indexes, which results in the following behavior:
 *
 * - Varray elements can be referenced only by index. If /my_varray is a
 *   varray, /my_varray/0 refers to its first element.
 * - When adding to a varray, '#' must be used in the path, e.g. /my_varray/#
 *   will add an element to to end of /my_varray.
 *   Trying to add to a varray with anything other than '#' yields
 *   SG_ERR_JSONDB_INVALID_PATH.
 * - Containers implicitly added because of a recursively added element
 *   are always vhashes unless '#' is specified in the path.  In an empty
 *   object, a recursive add of "/a/b/c" will create three vhashes, but
 *   "/#/#/#" will create three varrays.
 * - Note that if the root node is a varray (which is what happens when
 *   recursively adding "/#/#/#" to an empty json object like in the previous
 *   example), all subsequent additions to the root must use #.
 *
 * In all routines, pszPath is a pointer to a null-terminated utf-8 string.
 * This pointer continues to be owned by the caller.
 * Paths are delimited by '/' and are always absolute.
 * Failure to specify a leading slash always yields SG_ERR_JSONDB_INVALID_PATH.
 * Paths are case sensitive.
 *
 */

#ifndef H_SG_JSONDB_PROTOTYPES_H
#define H_SG_JSONDB_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_jsondb__create(
	SG_context* pCtx,
	const SG_pathname* pPathDbFile,
	SG_jsondb** ppJsondb);

void SG_jsondb__open(
	SG_context* pCtx,
	const SG_pathname* pPathDbFile,
	const char* pszObjectName,
	SG_jsondb** ppJsondb);

void SG_jsondb__create_or_open(
	SG_context* pCtx,
	const SG_pathname* pPathDbFile,
	const char* pszObjectName,
	SG_jsondb** ppJsondb);

void SG_jsondb__close_free(
	SG_context* pCtx,
	SG_jsondb* pThis);


/******************************************************************************
 * Routines to manipulate top-level JSON objects in the db
 */

/**
 * Retrieves the name of the current top-level JSON object.
 */
void SG_jsondb__get_current_object_name(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char** ppszObjectName); // This is our copy.  Don't free it.

/**
 * Set the current top-level JSON object.
 * Throws SG_ERR_NOT_FOUND if the specified object doesn't exist.
 */
void SG_jsondb__set_current_object(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszObjectName); // We'll make our own copy of this.  The caller still owns it.

/**
 * Retrieves the current top-level JSON object.
 */
void SG_jsondb__get__object(
	SG_context* pCtx,
	SG_jsondb* pThis,
	SG_variant** ppvhObject); // The caller owns this pointer and should free it.

/**
 * Adds the specified top-level JSON object and makes it the current object.
 */
void SG_jsondb__add__object(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszObjectName, // We'll make our own copy of this.  The caller still owns it.
	SG_variant* pvRootObject); // We'll make our own copy of this.  The caller still owns it.

/**
 * Removes the current top-level JSON object and sets the current object to NULL.
 */
void SG_jsondb__remove__object(
	SG_context* pCtx,
	SG_jsondb* pThis);


/******************************************************************************
 * Routines to retrieve elements within JSON objects
 */

/**
 * Check to see if a certain path is present.
 * If there are no errors, SG_ERR_OK is returned and pbResult is filled with a
 * bool indicating whether the key is present or not.
 */
void SG_jsondb__has(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool* pbResult);

void SG_jsondb__typeof(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_uint16* pResult);

/**
 * Return provided path's number of children.
 */
void SG_jsondb__count(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_uint32* piResult);

void SG_jsondb__get__variant(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_variant** ppResult); // The caller owns this pointer and should free it.

void SG_jsondb__get__sz(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	char** ppszValue); // The caller owns this pointer and should free it.

void SG_jsondb__get__vhash(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_vhash** ppvhObject); // The caller owns this pointer and should free it.

void SG_jsondb__check__sz(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool* pbExists,
	char** ppszValue); // The caller owns this pointer and should free it.

void SG_jsondb__get__double(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	double* pResult);

void SG_jsondb__get__int64(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_int64* pResult);

void SG_jsondb__get__int64_or_double(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_int64* pResult);

void SG_jsondb__get__uint32(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_uint32* pResult);

void SG_jsondb__get__bool(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool* pResult);

void SG_jsondb__get__varray(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_varray** ppResult); // The caller owns this pointer and should free it.


/******************************************************************************
 * Routines to add elements to JSON objects
 */

void SG_jsondb__add__string__sz(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const char* pszValue); // We'll make our own copy of this.  The caller still owns it.

/**
 * Add a string value at the specified path where the value is a utf-8
 * string specified as pointer and a length.  The actual string added
 * will stop at the length or a terminating null, whichever comes
 * first.
 */
void SG_jsondb__add__string__buflen(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const char* pszValue, // We'll make our own copy of this.  The caller still owns it.
	SG_uint32 len);

/**
 * Add an integer value at the specified path.
 */
void SG_jsondb__add__int64(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	SG_int64 intValue);

/**
* Add a floating-point value at the specified path.
*/
void SG_jsondb__add__double(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	double fv);

/**
 * Add a null value at the specified path.
 */
void SG_jsondb__add__null(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive);

/**
 * Add a boolean value at the specified path.
 */
void SG_jsondb__add__bool(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	SG_bool b);

/**
 * Add a vhash at the specified path.
 */
void SG_jsondb__add__vhash(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const SG_vhash* pHashValue); // We'll make our own copy of this.  The caller still owns it.

/**
 * Add an array at the specified path.
 */
void SG_jsondb__add__varray(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const SG_varray* pva); // We'll make our own copy of this.  The caller still owns it.


/******************************************************************************
 * Routines to add/update elements within JSON objects
 *
 * These are identical to the __add__ functions except that if the path already
 * exists then it is merely overwritten.
 */

void SG_jsondb__update__string__sz(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const char* pszValue); // We'll make our own copy of this.  The caller still owns it.

/**
 * Add or update a string value at the specified path where the value is a utf-8
 * string specified as pointer and a length.  The actual string added
 * will stop at the length or a terminating null, whichever comes
 * first.
 */
void SG_jsondb__update__string__buflen(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const char* pszValue, // We'll make our own copy of this.  The caller still owns it.
	SG_uint32 len);

/**
 * Add or update an integer value at the specified path.
 */
void SG_jsondb__update__int64(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	SG_int64 intValue);

/**
* Add or update a floating-point value at the specified path.
*/
void SG_jsondb__update__double(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	double fv);

/**
 * Add or update a null value at the specified path.
 */
void SG_jsondb__update__null(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive);

/**
 * Add or update a boolean value at the specified path.
 */
void SG_jsondb__update__bool(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	SG_bool b);

/**
 * Add or update a vhash at the specified path.
 */
void SG_jsondb__update__vhash(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const SG_vhash* pHashValue); // We'll make our own copy of this.  The caller still owns it.

/**
 * Add or update an array at the specified path.
 */
void SG_jsondb__update__varray(
    SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath,
	SG_bool bAddRecursive,
	const SG_varray* pva); // We'll make our own copy of this.  The caller still owns it.

/******************************************************************************
 * Routines to remove elements within JSON objects
 */

/**
 * Recursively remove the element at the specified path.
 */
void SG_jsondb__remove(
	SG_context* pCtx,
	SG_jsondb* pThis,
	const char* pszPath);

/******************************************************************************
 * Utility routines
 */

/**
 * When inserting a node with a key that may have slash characters this should
 * be called first to escape them.  If ppszEscapedKeyname is null, no escaping
 * was necessary.
 */
void SG_jsondb__escape_keyname(
	SG_context* pCtx,
	const char* pszKeyname,
	char** ppszEscapedKeyname);

void SG_jsondb__unescape_keyname(
	SG_context* pCtx,
	const char* pszKeyname,
	char** ppszUnescapedKeyname);

/**
 * Does nothing in release builds.
 */
void SG_jsondb_debug__verify_tree(
	SG_context* pCtx,
	SG_jsondb* pThis);

END_EXTERN_C;

#endif//H_SG_JSONDB_PROTOTYPES_H
