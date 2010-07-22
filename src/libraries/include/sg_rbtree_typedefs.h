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

#ifndef H_SG_RBTREE_TYPEDEFS_H
#define H_SG_RBTREE_TYPEDEFS_H

BEGIN_EXTERN_C;

typedef struct _sg_rbtree SG_rbtree;
typedef struct _sg_rbtree_trav SG_rbtree_iterator;

typedef void (SG_rbtree_foreach_callback)(SG_context* pCtx, const char* pszKey, void* assocData, void* ctx);

/**
 * A compare function to control the order of the keys in an RBTREE.
 * This is a generalization of strcmp() and used just like the last
 * arg to qsort() (but constrained to "char *" values).
 *
 * TODO The man page for strcmp() on the MAC says comparisons are done
 * TODO using "unsigned characters", but the prototype is for regular
 * TODO "char".  We always want the unsigned usage.
 * TODO
 * TODO Do we need to do something about this?  Don't we have "treat
 * TODO chars as unsigned" set on all platforms?
 */
typedef int (SG_rbtree_compare_function_callback)(const char * psz1, const char * psz2);

#define SG_RBTREE__DEFAULT__COMPARE_FUNCTION		((SG_rbtree_compare_function_callback *)strcmp)

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_RBTREE_TYPEDEFS_H
