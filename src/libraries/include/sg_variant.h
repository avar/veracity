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
 * @file sg_variant.h
 *
 * @details SG_variant is a struct which can represent any of the
 * values that can appear in a JSON object.
 *
 * We automatically convert back and forth between nullable C pointers
 * and variant/JSON nulls.  The idea is that the semantics of variant
 * NULLs should match the semantics of pointer NULLs.  The time to
 * fuss about NULLs is not on assignment, but on dereference.

 * This only applies to nullable types (C pointers).  OTOH, int64 is
 * not nullable.  Neither are double and bool.  So if you call
 * SG_variant__get__int64 on a variant null, you get an error.
 *
 * Note that storing a null pointer in a variant MUST be as
 * SG_VARIANT_TYPE_NULL.  You may NOT use SG_VARIANT_TYPE_VHASH
 * (or the others like it) and then set pv->v.val_vhash to NULL.
 *
 */

#ifndef H_SG_VARIANT_H
#define H_SG_VARIANT_H

BEGIN_EXTERN_C

#define SG_VARIANT_TYPE_NULL    1
#define SG_VARIANT_TYPE_INT64   2
#define SG_VARIANT_TYPE_DOUBLE  4
#define SG_VARIANT_TYPE_BOOL    8
#define SG_VARIANT_TYPE_SZ      16
#define SG_VARIANT_TYPE_VHASH   32
#define SG_VARIANT_TYPE_VARRAY  64

typedef union
{
	SG_int64 val_int64;
	const char* val_sz;
	SG_vhash* val_vhash;
	SG_varray* val_varray;
	SG_bool val_bool;
	double val_double;
} SG_variant_value;

typedef struct _sg_variant
{
	SG_variant_value v;
	/* we declare the type as 2 bytes instead of 1, for two
	 * reasons:
	 *
	 * 1.  to leave room for future expansion
	 *
	 * 2.  the compiler is probably going to insert padding for
	 * alignment purposes anyway
	 *
	 */
	SG_uint16 type;
} SG_variant;

/**
 * The following functions are used to access the value of a variant
 * as its actual type.  If the variant is not of the proper type,
 * the error will be SG_ERR_VARIANT_INVALIDTYPE.
 *
 * For nullable types, if the variant is of type NULL, then the
 * result will be NULL.  This includes id, sz, vhash and varray.
 * For int64, bool and double, if the variant is of type NULL, the
 * result will be SG_ERR_VARIANT_INVALIDTYPE.
 *
 * These routines do NOT convert sz to/from int64, bool or double.  If
 * you want int64 from a variant which is a string, get the string and
 * parse the int64 yourself.
 *
 * @defgroup SG_variant__get Functions to convert a variant to other types.
 *
 * @{
 *
 */

void SG_variant__get__sz(
	SG_context* pCtx,
	const SG_variant* pv,
	const char** pputf8Value
	);

void SG_variant__get__int64(
	SG_context* pCtx,
	const SG_variant* pv,
	SG_int64* pResult
	);

void SG_variant__get__int64_or_double(
	SG_context* pCtx,
	const SG_variant* pv,
	SG_int64* pResult
	);

void SG_variant__get__bool(
    SG_context* pCtx,
	const SG_variant* pv,
	SG_bool* pResult
	);

void SG_variant__get__double(
    SG_context* pCtx,
	const SG_variant* pv,
	double* pResult
	);

void SG_variant__get__vhash(
	SG_context* pCtx,
	const SG_variant* pv,
	SG_vhash** pResult
	);

void SG_variant__get__varray(
    SG_context* pCtx,
	const SG_variant* pv,
	SG_varray** pResult
	);

/** @} */

void SG_variant__compare(SG_context* pCtx, const SG_variant* pv1, const SG_variant* pv2, int* piResult);

void SG_variant__equal(
    SG_context* pCtx,
	const SG_variant* pv1,
	const SG_variant* pv2,
	SG_bool* pbResult
	);

void SG_variant__can_be_sorted(
    SG_context* pCtx,
	const SG_variant* pv1,
	const SG_variant* pv2,
	SG_bool* pbResult
	);

void SG_variant__to_string(
        SG_context* pCtx,
        SG_variant* pv,
        SG_string** ppstr
        );

void SG_variant__free(
        SG_context* pCtx,
        SG_variant* pv
        );

const char* sg_variant__type_name(SG_uint16 t);

//////////////////////////////////////////////////////////////////

/**
 * Normal increasing, case sensitive ordering.
 */
SG_qsort_compare_function SG_variant_sort_callback__increasing;

int SG_variant_sort_callback__increasing(SG_context * pCtx,
										 const void * pVoid_ppv1, // const SG_variant ** ppv1
										 const void * pVoid_ppv2, // const SG_variant ** ppv2
										 void * pVoidCallerData);

//////////////////////////////////////////////////////////////////

END_EXTERN_C

#endif
