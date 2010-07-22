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
 * @file sg_vhash.h
 *
 * @details SG_vhash is a hash table for variants (typed values).  It
 * stores key-value pairs.  Each key is a utf-8 string.  Values are
 * variants, which means they can be a variety of things.  This data
 * structure was designed to serve as a representation of a JSON
 * object.  Think of it as an assoc table or a dictionary.  Think of
 * it as something that can be easily serialized to/from JSON.
 *
 */

#ifndef H_SG_VHASH_H
#define H_SG_VHASH_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Allocate a new vhash table
 */
void SG_vhash__alloc__params(
    SG_context* pCtx,
	SG_vhash** ppNew,
	SG_uint32 guess,         /**< An estimate of the number of items
					          * which will be added to this hash
					          * table.  If you guess way too low, the
					          * hash table may be slow.  If you guess
					          * way too high, the hash table may use
					          * too much RAM. */
	SG_strpool* pStrPool,    /**< If you pass NULL, then the hash
					          * table will allocate (and later free)
					          * its own string pool.  If you instead
					          * pass in a pointer to a string pool,
					          * the hash table will use it, but will
					          * not free it. */
	SG_varpool* pVarPool     /**< If you pass NULL, then the hash table
					          * will allocate (and later free) its own
					          * variant pool.  If you instead pass in
					          * a pointer to a variant pool, the hash
					          * table will use it, but will not free
					          * it. */
	);

/**
 * Allocate a new vhash, sharing pools with the given one.
 */
void SG_vhash__alloc__shared(
    SG_context* pCtx,
	SG_vhash** ppNew,
	SG_uint32 guess,
	SG_vhash* pvhShared
	);

/**
 * Construct a complete copy of a vhash
 */
void SG_vhash__alloc__copy(
    SG_context* pCtx,
	SG_vhash** ppNew,
	const SG_vhash* pvhOther
	);

/**
 * Allocate a new vhash table.  This function calls
 * SG_vhash__alloc__params(32, NULL, NULL)
 */
void SG_vhash__alloc(SG_context* pCtx, SG_vhash** ppNew);

/**
 * Free the vhash table and all memory associated with it.
 *
 * @note If this hash table owns its own string pool, it will be freed
 * as well.  On the other hand, if the pool was provided externally,
 * the vhash table will regard itself as a guest and will not free it.
 */
void SG_vhash__free(SG_context * pCtx, SG_vhash* pHash);

/**
 * Parse a JSON object and return it in the form a vhash table.
 */
void SG_vhash__alloc__from_json(
	SG_context* pCtx,
	SG_vhash** ppNew, /**< If there are no errors, the resulting vhash table will be returned here. */
	const char* pszJson /**< The JSON text */
	);

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#define __SG_VHASH__ALLOC__(pp,expr)		SG_STATEMENT(	SG_vhash * _p = NULL;										\
															expr;														\
															_sg_mem__set_caller_data(_p,__FILE__,__LINE__,"SG_vhash");	\
															*(pp) = _p;													)

#define SG_VHASH__ALLOC__PARAMS(pCtx,ppNew,guess,pStrPool,pVarPool)		__SG_VHASH__ALLOC__(ppNew, SG_vhash__alloc__params   (pCtx,&_p,guess,pStrPool,pVarPool) )
#define SG_VHASH__ALLOC__SHARED(pCtx,ppNew,guess,pvhShared)				__SG_VHASH__ALLOC__(ppNew, SG_vhash__alloc__shared   (pCtx,&_p,guess,pvhShared) )
#define SG_VHASH__ALLOC__COPY(pCtx,ppNew,pvhOther)						__SG_VHASH__ALLOC__(ppNew, SG_vhash__alloc__copy     (pCtx,&_p,pvhOther) )
#define SG_VHASH__ALLOC(pCtx,ppNew)										__SG_VHASH__ALLOC__(ppNew, SG_vhash__alloc           (pCtx,&_p) )
#define SG_VHASH__ALLOC__FROM_JSON(pCtx,ppNew,pszJson)					__SG_VHASH__ALLOC__(ppNew, SG_vhash__alloc__from_json(pCtx,&_p,pszJson) )

#else

#define SG_VHASH__ALLOC__PARAMS(pCtx,ppNew,guess,pStrPool,pVarPool)		SG_vhash__alloc__params   (pCtx,ppNew,guess,pStrPool,pVarPool)
#define SG_VHASH__ALLOC__SHARED(pCtx,ppNew,guess,pvhShared)				SG_vhash__alloc__shared   (pCtx,ppNew,guess,pvhShared)
#define SG_VHASH__ALLOC__COPY(pCtx,ppNew,pvhOther)						SG_vhash__alloc__copy     (pCtx,ppNew,pvhOther)
#define SG_VHASH__ALLOC(pCtx,ppNew)										SG_vhash__alloc           (pCtx,ppNew)
#define SG_VHASH__ALLOC__FROM_JSON(pCtx,ppNew,pszJson)					SG_vhash__alloc__from_json(pCtx,ppNew,pszJson)

#endif

//////////////////////////////////////////////////////////////////

/**
 * Write a vhash table in JSON format.  This function accepts an existing SG_jsonwriter.
 * The keys will be serialized in the "current" order.  See ordering.
 */
void SG_vhash__write_json(
    SG_context* pCtx,
	const SG_vhash* pvh, /**< The vhash to be serialized. */
	SG_jsonwriter* pjson /**< The SG_jsonwriter into which the serialized hash table will go. */
	);

/**
 * Write a vhash table in JSON format.  This function handles all the details of
 * setting up the SG_jsonwriter and places the output stream in the given string.
 */
void SG_vhash__to_json(
    SG_context* pCtx,
	const SG_vhash* pvh,
	SG_string* pStr
	);

/**
 * @result The number of key-value pairs
 */
void SG_vhash__count(
    SG_context* pCtx,
	const SG_vhash* pHash,
	SG_uint32* piResult
	);

/**
 * @result A varray containing the keys
 */
void SG_vhash__get_keys(
	SG_context* pCtx,
	const SG_vhash* pHash,
	SG_varray** ppNew
	);

/**
 * The following functions are used to add a key-value pair to the hash table.
 *
 * In all cases, the key is a pointer to a null-terminated utf-8 string.  This
 * pointer continues to be owned by the caller.  The vhash table will make its
 * own copy of the pointer (in its string pool).
 *
 * It is an error to add a key if that key already exists.
 *
 * Each of these functions return an SG_error.
 *
 * @defgroup SG_vhash__add Functions to add key-value pairs to a vhash table
 * @{
 */

/**
 * Add a key-value pair to the hash table where the value is a utf-8
 * string specified as null-terminated pointer.
 *
 */
void        SG_vhash__add__string__sz(
    SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key,
	const char* putf8Value /**< The value for the given key.  This
							* pointer continues to be owned by the
							* caller.  The vhash table will make its
							* own copy of this (in its string
							* pool). */
	);

/**
 * Add a key-value pair to the hash table where the value is a utf-8
 * string specified as pointer and a length.  The actual string added
 * will stop at the length or a terminating null, whichever comes
 * first.
 */
void        SG_vhash__add__string__buflen(
	SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key,
	const char* putf8Value, /**< The value for the given key.  This
							 * pointer continues to be owned by the
							 * caller.  The vhash table will make its
							 * own copy of this (in its string
							 * pool). */
	SG_uint32 len
	);

/**
 * Add a key-value pair to the hash table where the value is an integer.
 */
void SG_vhash__add__int64(
    SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key,
	SG_int64 intValue
	);

void SG_vhash__addtoval__int64(
    SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key,
    SG_int64 addend
	);

/**
 * Add a key-value pair to the hash table where the value is a floating point number.
 */
void SG_vhash__add__double(
    SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key,
	double fv
	);

/**
 * Add a key-value pair to the hash table where the value is a null.
 */
void SG_vhash__add__null(
    SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key
	);

/**
 * Add a key-value pair to the hash table where the value is a bool.
 */
void SG_vhash__add__bool(
    SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key,
	SG_bool b
	);

/**
 * Add a key-value pair to the hash table where the value is another vhash table.
 */
void SG_vhash__add__vhash(
    SG_context* pCtx,
    SG_vhash* pHash,
    const char* putf8Key,
    SG_vhash** ppHashValue /**< The value for the given key.  This
                            * pointer is now owned by @a pHash.  It will
                            * be freed when @a pHash is freed.  Note
                            * that it is acceptable to modify @a
                            * pHashValue (by adding or removing
                            * key-value pairs) even after it has been
                            * added to @a pHash. */
    );

/**
 * Add a key-value pair to the hash table where the value is an array.
 */
void SG_vhash__add__varray(
    SG_context* pCtx,
    SG_vhash* pHash,
    const char* putf8Key,
    SG_varray** ppva /**< The value for the given key.  This pointer is
                      * now owned by @a pHash.  It will be freed when @a
                      * pHash is freed.  Note that it is acceptable to
                      * modify @a pva even after it has been added to @a
                      * pHash */
    );

/** @} */

/**
 * The following functions are used to add or modify a key-value pair
 * to the hash table.  They are identical to the __add__ functions
 * except that if the key already exists then it is merely
 * overwritten.
 *
 * In all cases, the key is a pointer to a null-terminated utf-8 string.  This
 * pointer continues to be owned by the caller.  The vhash table will make its
 * own copy of the pointer (in its string pool).
 *
 * Each of these functions return an SG_error.
 *
 * @defgroup SG_vhash__update Functions to add or modify key-value pairs to a vhash table
 * @{
 */

/**
 * Add/update a key-value pair to the hash table where the value is a utf-8 string
 * specified as a null-terminated char*.
 */
void        SG_vhash__update__string__sz(
	SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key,
	const char* putf8Value /**< The value for the given key.  This
							* pointer continues to be owned by the
							* caller.  The vhash table will make its
							* own copy of this (in its string
							* pool). */
	);

/**
 * Add/update a key-value pair to the hash table where the value is a
 * utf-8 string specified as a pointer and a length.  The actual
 * string added will stop at the length or a terminating null,
 * whichever comes first.
 */
void       SG_vhash__update__string__buflen(
	SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key,
	const char* putf8Value, /**< The value for the given key.  This
							* pointer continues to be owned by the
							* caller.  The vhash table will make its
							* own copy of this (in its string
							* pool). */
	SG_uint32 len
	);

/**
 * Add/update a key-value pair to the hash table where the value is an integer.
 */
void SG_vhash__update__int64(
    SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key,
	SG_int64 intValue
	);

/**
 * Add/update a key-value pair to the hash table where the value is a floating point number.
 */
void SG_vhash__update__double(
    SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key,
	double fv
	);

/**
 * Add/update a key-value pair to the hash table where the value is a null.
 */
void SG_vhash__update__null(
	SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key
	);

/**
 * Add/update a key-value pair to the hash table where the value is a bool.
 */
void SG_vhash__update__bool(
	SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key,
	SG_bool b
	);

/**
 * Add/update a key-value pair to the hash table where the value is another vhash table.
 */
void SG_vhash__update__vhash(
    SG_context* pCtx,
    SG_vhash* pHash,
    const char* putf8Key,
    SG_vhash** ppHashValue /**< The value for the given key.  This
                            * pointer is now owned by @a pHash.  It will
                            * be freed when @a pHash is freed.  Note
                            * that it is acceptable to modify @a
                            * pHashValue (by adding or removing
                            * key-value pairs) even after it has been
                            * added to @a pHash. */
    );

/**
 * Add/update a key-value pair to the hash table where the value is an array.
 */
void        SG_vhash__update__varray(
    SG_context* pCtx,
    SG_vhash* pHash,
    const char* putf8Key,
    SG_varray** ppva /**< The value for the given key.  This pointer is
                      * now owned by @a pHash.  It will be freed when @a
                      * pHash is freed.  Note that it is acceptable to
                      * modify @a pva even after it has been added to @a
                      * pHash */
    );

/** @} */

/**
 * Remove a key-value pair.  If the key is not present, the err code
 * will be SG_ERR_VHASH_KEYNOTFOUND.
 */
void SG_vhash__remove(
    SG_context* pCtx,
	SG_vhash* pHash,
	const char* putf8Key
	);

/**
 * Check to see if a certain key is present
 * @result If there are no errors, SG_ERR_OK is returned, and @a pbResult is filled with a bool indicating whether the key is present or not.
 */
void SG_vhash__has(
    SG_context* pCtx,
	const SG_vhash* pHash,
	const char* putf8Key,
	SG_bool* pbResult
	);

void SG_vhash__key(
        SG_context* pCtx,
        const SG_vhash* pvh,
        const char* putf8Key,
        const char** pp
        );

void SG_vhash__typeof(
    SG_context* pCtx,
	const SG_vhash* pHash,
	const char* putf8Key,
	SG_uint16* pResult
	);

/**
 * The following functions are used to retrieve the value associated
 * with a given key.
 *
 * Each of these functions return an SG_error.  If the key is not
 * present, the err code will be SG_ERR_VHASH_KEYNOTFOUND.  If the
 * value is not of the type requested, the err code will be
 * SG_ERR_VARIANT_INVALIDTYPE.
 *
 * @defgroup SG_vhash__get Functions to get the value associated with a given key.
 * @{
 */

/** Get the value as a variant */
void SG_vhash__get__variant(
	SG_context* pCtx,
	const SG_vhash* pHash,
	const char* putf8Key,
	const SG_variant** ppResult
	);

void SG_vhash__slashpath__get__variant(
    SG_context* pCtx,
	const SG_vhash* pvh,
	const char* psz_path,
	const SG_variant** ppResult
    );

void SG_vhash__slashpath__update__string__sz(
	SG_context* pCtx,
	SG_vhash* pHash,
	const char* psz_path,
	const char* putf8Value
	);

void SG_vhash__slashpath__update__varray(
	SG_context* pCtx,
	SG_vhash* pHash,
	const char* psz_path,
    SG_varray** ppva
	);

/** Get the value as a utf-string */
void SG_vhash__get__sz(
    SG_context* pCtx,
	const SG_vhash* pHash,
	const char* putf8Key,
	const char** pputf8Value /**< The vhash (or rather, its string pool) still owns this pointer.  Don't free it. */
	);

void        SG_vhash__check__vhash(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, SG_bool* pb, SG_vhash** ppvh);
void        SG_vhash__check__varray(SG_context* pCtx, const SG_vhash* pvh, const char* putf8Key, SG_bool* pb, SG_varray** ppva);

void SG_vhash__check__sz(
        SG_context* pCtx,
        const SG_vhash* pvh,
        const char* putf8Key,
        SG_bool* pb,
        const char** pputf8Value
        );

/** Get the value as a double */
void        SG_vhash__get__double(
    SG_context* pCtx,
	const SG_vhash* pvh,
	const char* putf8Key,
	double* pResult
	);

/** Get the value as an integer */
void        SG_vhash__get__int64(
    SG_context* pCtx,
	const SG_vhash* pvh,
	const char* putf8Key,
	SG_int64* pResult
	);

void        SG_vhash__get__int64_or_double(
    SG_context* pCtx,
	const SG_vhash* pvh,
	const char* putf8Key,
	SG_int64* pResult
	);

void SG_vhash__get__uint32(
    SG_context* pCtx,
	const SG_vhash* pvh,
	const char* putf8Key,
	SG_uint32* pResult
	);

/** Get the value as a bool */
void SG_vhash__get__bool(
	SG_context* pCtx,
	const SG_vhash* pvh,
	const char* putf8Key,
	SG_bool* pResult
	);

void SG_vhash__addnew__vhash(
    SG_context* pCtx,
	SG_vhash* pvh,
	const char* putf8Key,
	SG_vhash** pResult
	);

void SG_vhash__addnew__varray(
    SG_context* pCtx,
	SG_vhash* pvh,
	const char* putf8Key,
	SG_varray** pResult
	);

/** Get the value as a vhash */
void SG_vhash__get__vhash(
    SG_context* pCtx,
	const SG_vhash* pvh,
	const char* putf8Key,
	SG_vhash** ppResult /**< The vhash still owns this pointer.  Don't
						* free it. TODO, so should it be const? */
	);

/** Get the value as a varray */
void SG_vhash__get__varray(
    SG_context* pCtx,
	const SG_vhash* pvh,
	const char* putf8Key,
	SG_varray** ppResult /**< The vhash still owns this pointer.  Don't
						 * free it. TODO so should it be const? */
	);

/** @} */

/** Get the nth pair */
void SG_vhash__get_nth_pair(
	SG_context* pCtx,
	const SG_vhash* pHash,
	SG_uint32 n,
	const char** putf8Key,
	const SG_variant** ppResult
	);

//////////////////////////////////////////////////////////////////

/**
 * vhash Ordering.
 *
 * The vhash defines a "current" ordering.  Initialially this is the order that
 * items were added to the vhash.  This ordering can be changed by sorting the
 * vhash.  Sorting only affects item currently in the vhash; items added after
 * sorting are appended in the order added.  You must re-sort if you want these
 * items also sorted.
 *
 * The "current" order is only important for the "foreach" iterator.  It will
 * iterate over the items in the "current" order.  The "current" order does not
 * affect random lookups.
 *
 * @defgroup SG_vhash__order VHash Ordering Functions.
 * @{
 */

/**
 * Iterator callback.
 *
 * TODO All of the other _foreach_callback typedefs have the "void * ctx" as the last argument.
 * TODO We should fix this one to be consistent with them.
 */
typedef void (SG_vhash_foreach_callback)(SG_context* pCtx, void* ctx, const SG_vhash* pvh, const char* putf8Key, const SG_variant* pv);

/**
 * The iterator will visit the keys in the "current" order.  For each key, it will
 * call the given callback routine.  If the callback routine returns an error, iteration
 * will stop and the error will be returned.
 *
 */
void SG_vhash__foreach(
    SG_context* pCtx,
	const SG_vhash* pvh,
	SG_vhash_foreach_callback* cb,
	void* ctx
	);

//////////////////////////////////////////////////////////////////

/**
 * Normal increasing, case sensitive ordering.
 */
SG_qsort_compare_function SG_vhash_sort_callback__increasing;

int SG_vhash_sort_callback__increasing(SG_context * pCtx,
									   const void * pVoid_pszKey1, // const char ** pszKey1
									   const void * pVoid_pszKey2, // const char ** pszKey2
									   void * pVoidCallerData);

/**
 * Sort the keys in the vhash.  This will change the "current" order of the keys
 * as visited by the iterator.  Keys added after sorting will be appended to the
 * end (they are not inserted in order).
 *
 * If bRecursive is SG_TRUE, we will do a recursive vhash sort.  That is, after
 * sorting the keys in the top-level (given) vhash, it will recursively call itself
 * to sort each value that is a vhash.
 *
 * If bRecursive is SG_FALSE, this routine only sorts the topl-level keys of the
 * given vhash; it does not recurse into values that happen to be vhashes.
 *
 * In either case, we ***DO NOT*** sort any of the varrays that we may find
 * within the vhash.  If you are using this sort to help ensure that you get
 * consistent HIDs when the vhash is converted to a JSON and a Blob, you may
 * want to consider sorting the varrays -- but only if that is appropriate
 * for them.
 *
 */
void SG_vhash__sort(SG_context* pCtx, SG_vhash * pvh, SG_bool bRecursive, SG_qsort_compare_function pfnCompare);

/**
 * @}
 */

//////////////////////////////////////////////////////////////////

/**
 * Compare 2 vhashes for equality.  The keys need not be in the same order.
 */
void SG_vhash__equal(
    SG_context* pCtx,
	const SG_vhash* pv1,
	const SG_vhash* pv2,
	SG_bool* pbResult
	);

void SG_vhash__path__get__vhash(
    SG_context* pCtx,
	const SG_vhash* pvh,
    SG_vhash** ppvh_result,
    SG_bool* pb,
    ...
    );

void SG_vhash__copy_items(
    SG_context* pCtx,
    const SG_vhash* pvh_from,
    SG_vhash* pvh_to
	);

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
/**
 * Dump a vhash to a string.
 */
void SG_vhash_debug__dump(SG_context* pCtx, const SG_vhash * pvh, SG_string * pStrOut);

/**
 * Dump a vhash to stderr.
 * You should be able to call this from GDB or from the VS command window.
 */
void SG_vhash_debug__dump_to_console(SG_context* pCtx, const SG_vhash * pvh);
void SG_vhash_debug__dump_to_console__named(SG_context* pCtx, const SG_vhash * pvh, const char* pszName);
#endif

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif

