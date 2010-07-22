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
 * @file sg_rbtree_typedefs.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_RBTREE_PROTOTYPES_H
#define H_SG_RBTREE_PROTOTYPES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * Reverse the order of strcmp().
 */
SG_rbtree_compare_function_callback SG_rbtree__compare_function__reverse_strcmp;

/**
 * Allocate an RBTREE specifying all options.
 *
 * If pfnCompare is NULL, we use strcmp().
 */
void SG_rbtree__alloc__params2(
	SG_context* pCtx,
	SG_rbtree** ppNew,
	SG_uint32 guess,
	SG_strpool* pStrPool,
	SG_rbtree_compare_function_callback * pfnCompare
	);

#if defined(DEBUG)
#define SG_RBTREE__ALLOC__PARAMS2(pCtx,ppNew,guess,pStrPool,pfnCompare)		SG_STATEMENT(	SG_rbtree * _prbNew = NULL;                                         \
																							SG_rbtree__alloc__params2(pCtx,&_prbNew,guess,pStrPool,pfnCompare); \
																					_sg_mem__set_caller_data(_prbNew,__FILE__,__LINE__,"SG_rbtree");            \
																					*(ppNew) = _prbNew;                                                         )
#else
#define SG_RBTREE__ALLOC__PARAMS2(pCtx,ppNew,guess,pStrPool,pfnCompare)		SG_rbtree__alloc__params2(pCtx,ppNew,guess,pStrPool,pfnCompare)
#endif

void SG_rbtree__alloc__params(
	SG_context* pCtx,
	SG_rbtree** ppNew,
	SG_uint32 guess,
	SG_strpool* pStrPool
	);

#if defined(DEBUG)
#define SG_RBTREE__ALLOC__PARAMS(pCtx,ppNew,guess,pStrPool)	SG_STATEMENT(	SG_rbtree * _prbNew = NULL;											\
																			SG_rbtree__alloc__params(pCtx,&_prbNew,guess,pStrPool);				\
																			_sg_mem__set_caller_data(_prbNew,__FILE__,__LINE__,"SG_rbtree");	\
																			*(ppNew) = _prbNew;													)
#else
#define SG_RBTREE__ALLOC__PARAMS(pCtx,ppNew,guess,pStrPool)	SG_rbtree__alloc__params(pCtx,ppNew,guess,pStrPool)
#endif

void SG_rbtree__alloc(
	SG_context* pCtx,
	SG_rbtree** ppNew
	);

#if defined(DEBUG)
#define SG_RBTREE__ALLOC(pCtx,ppNew)	SG_STATEMENT(	SG_rbtree * _prbNew = NULL;											\
														SG_rbtree__alloc(pCtx,&_prbNew);									\
														_sg_mem__set_caller_data(_prbNew,__FILE__,__LINE__,"SG_rbtree");	\
														*(ppNew) = _prbNew;													)
#else
#define SG_RBTREE__ALLOC(pCtx,ppNew)	SG_rbtree__alloc(pCtx,ppNew)
#endif

/**
 * Attempting to add a key which is already present
 * is an error.  The assocData will be NULL.
 */
void SG_rbtree__add(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* psz
	);

void SG_rbtree__add__return_pooled_key(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* psz,
    const char** pp
	);

/**
 * Use this if you just want to be sure the key is in the tree.
 * Unlike __add, it will not return an error if the key is already
 * there.  The assocData will be nulled out, regardless of what it was
 * before.
 */
void SG_rbtree__update(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* psz
	);

void SG_rbtree__update__with_pooled_sz(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* pszKey,
	const char* pszAssoc
	);

/**
 * This function returns an error if the key is already there.
 */
void SG_rbtree__add__with_assoc(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* psz,
	void* assoc
	);

void SG_rbtree__add__with_pooled_sz(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* psz,
	const char* pszAssoc
	);


void SG_memoryblob__pack(
		SG_context* pCtx,
		const SG_byte* p,
		SG_uint32 len,
		SG_byte* pDest
		);

void SG_memoryblob__unpack(
		SG_context* pCtx,
		const SG_byte* pPacked,
		const SG_byte** pp,
		SG_uint32* piLen
		);

void SG_rbtree__memoryblob__alloc(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* psz,
	SG_uint32 len,
	SG_byte** pp
	);

void SG_rbtree__memoryblob__add__name(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* psz,
	const SG_byte* p,
	SG_uint32 len
	);

void SG_rbtree__memoryblob__add__hid(
	SG_context* pCtx,
	SG_rbtree* prb,
	SG_repo * pRepo,
	const SG_byte* p,
	SG_uint32 len,
	const char** ppszhid
	);

void SG_rbtree__memoryblob__get(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* pszhid,
	const SG_byte** pp,
	SG_uint32* piLen
	);

/**
 * If the key is already there, this function updates the assocData
 * and optionally returns the old one.  If it is not already there, it
 * does an add.
 */
void SG_rbtree__update__with_assoc(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* psz,
	void* assoc,
	void** pOldAssoc
	);

/**
 * This function has update semantics, but it doesn't
 * do anything with assoc data.  It is useful when you
 * have two rbtrees with just keys, no assoc data,
 * and you want the union of their keys. */

void SG_rbtree__update__from_other_rbtree__keys_only(
	SG_context* pCtx,
	SG_rbtree* prb,
	const SG_rbtree* prbOther
    );

/**
 * This function has "add" semantics.  In other words, if the rb trees
 * have any keys in common, there will be an error.
 */
void SG_rbtree__add__from_other_rbtree(
	SG_context* pCtx,
	SG_rbtree* prb,
	const SG_rbtree* prbOther
	);

/**
 * Attempting to remove a key which is not present is an error.
 */
void SG_rbtree__remove(
	SG_context* pCtx,
	SG_rbtree* prb,
	const char* psz
	);

void SG_rbtree__remove__with_assoc( SG_context* pCtx, SG_rbtree *tree, const char* pszKey, void** ppAssocData );

/**
 * If the key is found:
 *
 *   If pbFound, set it to true.
 *   if pAssocData, set it to the assoc data (which may be null)
 *   Return SG_ERR_OK
 *
 * If the key is NOT found:
 *
 *   *pAssocData will be untouched
 *   If pbFound, set it to false and return SG_ERR_OK
 *   If !pbFound, return SG_ERR_NOT_FOUND
 *
 */
void SG_rbtree__find(
	SG_context* pCtx,
	const SG_rbtree* prb,
	const char* psz,
    SG_bool* pbFound,
	void** pAssocData
	);

void SG_rbtree__key(
	SG_context* pCtx,
	const SG_rbtree* prb,
	const char* psz,
    const char** ppsz_key
	);

void SG_rbtree__get_only_entry(
	SG_context* pCtx,
	const SG_rbtree *tree,
	const char** ppszKey,
	void** pAssocData
    );

void SG_rbtree__iterator__first(
	SG_context* pCtx,
	SG_rbtree_iterator **ppIterator,
	const SG_rbtree *tree,
	SG_bool* pbOK,
	const char** ppszKey,
	void** pAssocData
	);

/* TODO could we stuff ppszKey and pAssocData inside the iterator and remove
 * them as parameters to __next?  This would actually be a little safer,
 * because it would avoid the mistake of somebody passing different pointers to
 * __first and __next. */

/* TODO and maybe __next should accept the pointer to the pointer to the
 * iterator and automatically nullfree it when it's done? */

void SG_rbtree__iterator__next(
	SG_context* pCtx,
	SG_rbtree_iterator *trav,
	SG_bool* pbOK,
	const char** ppszKey,
	void** pAssocData
	);

void SG_rbtree__iterator__free(SG_context * pCtx, SG_rbtree_iterator *trav);

/**
 * Even though we have iterators for an rbtree, foreach is still handy
 * for some situations because it has one important feature: It
 * doesn't call malloc.  So, if your callback doesn't alloc anything
 * on the heap, then calling foreach will not touch the heap at all.
 * This makes it reasonably safe to call from within something like a
 * free method that returns void instead of SG_error.
 */
void SG_rbtree__foreach(
	SG_context* pCtx,
	const SG_rbtree* prb,
	SG_rbtree_foreach_callback* cb,
	void* ctx
	);

void SG_rbtree__count(
	SG_context* pCtx,
	const SG_rbtree* prb,
	SG_uint32* piCount
	);

void SG_rbtree__depth(
	SG_context* pCtx,
	const SG_rbtree* prb,
	SG_uint16* piDepth
	);

void SG_rbtree__free(SG_context * pCtx, SG_rbtree* prb);

/**
 * Free the RBTREE but call the given callback to free the associated data
 * pointer on each item first.
 */
void SG_rbtree__free__with_assoc(
	SG_context * pCtx,
	SG_rbtree * ptree,
	SG_free_callback* cb
	);

/*
 * Compare the keys of two red black trees.
 *
 */
void SG_rbtree__compare__keys_only(
	SG_context* pCtx,
	const SG_rbtree* prb1,
	const SG_rbtree* prb2,
	SG_bool* pbIdentical,    /**< Return true iff the two trees have
						      * identical keys */
	SG_rbtree* prbOnly1,   /**< All keys which are in prb1 but not
							  * in prb2 will be added to prbOnly1,
							  * which need not start out empty.  It is
							  * okay to pass NULL for this
							  * parameter. */
	SG_rbtree* prbOnly2,   /**< All keys which are in prb2 but no in
							  * prb1 will be added to prbOnly2, which
							  * need not start out empty.  It is okay
							  * to pass NULL for this parameter.  */
	SG_rbtree* prbBoth     /**< All keys which are in both prb1 and
							  * prb2 will be added to prbBoth, which
							  * need not start out empty.  It is okay
							  * to pass NULL for this parameter.  */
	);

/**
 * Create a varray containing all of the keys from this red black
 * tree.  The keys will be added to the varray in sorted order.
 */
void SG_rbtree__to_varray__keys_only(
	SG_context* pCtx,
	const SG_rbtree* prb,
	SG_varray** ppva
	);

void SG_rbtree__write_json_array__keys_only(SG_context* pCtx, const SG_rbtree* prb, SG_jsonwriter* pjson);

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
void SG_rbtree_debug__dump_keys(SG_context* pCtx, const SG_rbtree * prb, SG_string * pStrOut);
void SG_rbtree_debug__dump_keys_to_console(SG_context* pCtx, const SG_rbtree * prb);
#endif

void SG_rbtree__get_strpool(
	SG_context* pCtx,
    SG_rbtree* prb,
    SG_strpool** ppp
    );

END_EXTERN_C;

#endif//H_SG_RBTREE_PROTOTYPES_H
