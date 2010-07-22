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

#include <sg.h>

void SG_variant__get__sz(SG_context* pCtx, const SG_variant* pv, const char** pputf8Value)
{
	if (pv->type == SG_VARIANT_TYPE_SZ)
	{
		*pputf8Value = pv->v.val_sz;
		return;
	}

	/* see comment in hdr file about nullable types */
	if (pv->type == SG_VARIANT_TYPE_NULL)
	{
		*pputf8Value = NULL;
		return;
	}

	SG_ERR_THROW_RETURN(  SG_ERR_VARIANT_INVALIDTYPE  );
}

void SG_variant__get__int64(SG_context* pCtx, const SG_variant* pv, SG_int64* pResult)
{
	if (pv->type == SG_VARIANT_TYPE_INT64)
	{
		*pResult = pv->v.val_int64;
		return;
	}
    else if (pv->type == SG_VARIANT_TYPE_SZ)
    {
        // TODO do we really want this?

        SG_int64 i64 = 0;
        SG_int64__parse__strict(pCtx, &i64, pv->v.val_sz);
        if (!SG_context__has_err(pCtx))
        {
            *pResult = i64;
            return;
        }
    }

	SG_ERR_THROW_RETURN(  SG_ERR_VARIANT_INVALIDTYPE  );
}

void SG_variant__get__int64_or_double(SG_context* pCtx, const SG_variant* pv, SG_int64* pResult)
{
	if (pv->type == SG_VARIANT_TYPE_INT64)
	{
		*pResult = pv->v.val_int64;
		return;
	}
    else if (pv->type == SG_VARIANT_TYPE_DOUBLE)
    {
        if (SG_double__fits_in_int64(pv->v.val_double))
        {
            *pResult = (SG_int64) pv->v.val_double;
            return;
        }
    }

	SG_ERR_THROW_RETURN(  SG_ERR_VARIANT_INVALIDTYPE  );
}

void SG_variant__get__bool(SG_context* pCtx, const SG_variant* pv, SG_bool* pResult)
{
	if (pv->type == SG_VARIANT_TYPE_BOOL)
	{
		*pResult = pv->v.val_bool;
		return;
	}

	SG_ERR_THROW_RETURN(  SG_ERR_VARIANT_INVALIDTYPE  );
}

void SG_variant__get__double(SG_context* pCtx, const SG_variant* pv, double* pResult)
{
	if (pv->type == SG_VARIANT_TYPE_DOUBLE)
	{
		*pResult = pv->v.val_double;
		return;
	}

	SG_ERR_THROW_RETURN(  SG_ERR_VARIANT_INVALIDTYPE  );
}

void SG_variant__get__vhash(SG_context* pCtx, const SG_variant* pv, SG_vhash** pResult)
{
	if (pv->type == SG_VARIANT_TYPE_VHASH)
	{
		*pResult = pv->v.val_vhash;
		return;
	}

	/* see comment in hdr file about nullable types */
	if (pv->type == SG_VARIANT_TYPE_NULL)
	{
		*pResult = NULL;
		return;
	}

	SG_ERR_THROW_RETURN(  SG_ERR_VARIANT_INVALIDTYPE  );
}

void SG_variant__get__varray(SG_context* pCtx, const SG_variant* pv, SG_varray** pResult)
{
	if (pv->type == SG_VARIANT_TYPE_VARRAY)
	{
		*pResult = pv->v.val_varray;
		return;
	}

	/* see comment in hdr file about nullable types */
	if (pv->type == SG_VARIANT_TYPE_NULL)
	{
		*pResult = NULL;
		return;
	}

	SG_ERR_THROW_RETURN(  SG_ERR_VARIANT_INVALIDTYPE  );
}

void SG_variant__equal(SG_context* pCtx, const SG_variant* pv1, const SG_variant* pv2, SG_bool* pbResult)
{
	SG_NULLARGCHECK_RETURN(pv1);
	SG_NULLARGCHECK_RETURN(pv2);

	if (pv1 == pv2)
	{
		*pbResult = SG_TRUE;
		return;
	}

	if (pv1->type != pv2->type)
	{
		*pbResult = SG_FALSE;
		return;
	}

	switch (pv1->type)
	{
	case SG_VARIANT_TYPE_DOUBLE:
		*pbResult = pv1->v.val_double == pv2->v.val_double;
		return;

	case SG_VARIANT_TYPE_INT64:
		*pbResult = pv1->v.val_int64 == pv2->v.val_int64;
		return;

	case SG_VARIANT_TYPE_BOOL:
		*pbResult = pv1->v.val_bool == pv2->v.val_bool;
		return;

	case SG_VARIANT_TYPE_NULL:
		*pbResult = SG_TRUE;
		return;

	case SG_VARIANT_TYPE_SZ:
		*pbResult = (0 == strcmp(pv1->v.val_sz, pv2->v.val_sz));
		return;

	case SG_VARIANT_TYPE_VHASH:
		SG_ERR_CHECK_RETURN(  SG_vhash__equal(pCtx, pv1->v.val_vhash, pv2->v.val_vhash, pbResult)  );
		return;

	case SG_VARIANT_TYPE_VARRAY:
		SG_ERR_CHECK_RETURN(  SG_varray__equal(pCtx, pv1->v.val_varray, pv2->v.val_varray, pbResult)  );
		return;
	}

	SG_ERR_THROW_RETURN(  SG_ERR_INVALIDARG  );
}

void SG_variant__can_be_sorted(SG_UNUSED_PARAM(SG_context* pCtx), const SG_variant* pv1, const SG_variant* pv2, SG_bool* pbResult)
{
	SG_UNUSED(pCtx);

	if (pv1->type != pv2->type)
	{
		/*
		 * We refuse to compare variants of differing
		 * types.  I suppose this is a little bit
		 * unfriendly in certain cases.  For example,
		 * we could implement comparison between double
		 * and int64 if we cared enough.  Not sure we do.
		 *
		 * We also consider things like vhash and varray to
		 * be non-sortable.  Only double, int and string
		 * are sortable.
		 *
		 * The implementation of SG_variant__compare currently
		 * assumes these restrictions.  If you modify these
		 * restrictions here, you'll need to go fix things
		 * up there as well.
		 */

		*pbResult = SG_FALSE;
		return;
	}

	switch (pv1->type)
	{
	case SG_VARIANT_TYPE_DOUBLE:
	case SG_VARIANT_TYPE_INT64:
	case SG_VARIANT_TYPE_SZ:
		*pbResult = SG_TRUE;
		return;

	default:
		*pbResult = SG_FALSE;
		return;
	}
}

void SG_variant__compare(SG_context* pCtx, const SG_variant* pv1, const SG_variant* pv2, int* piResult)
{
	SG_bool b;

	SG_NULLARGCHECK_RETURN(pv1);
	SG_NULLARGCHECK_RETURN(pv2);

	if (pv1 == pv2)
	{
		*piResult = 0;
		return;
	}

	SG_ERR_CHECK_RETURN(  SG_variant__can_be_sorted(pCtx, pv1, pv2, &b)  );

	if (!b)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_VARIANT_INVALIDTYPE  );
	}

	switch (pv1->type)
	{
	case SG_VARIANT_TYPE_DOUBLE:
		if (pv1->v.val_double == pv2->v.val_double)
		{
			*piResult = 0;
		}
		else if (pv1->v.val_double > pv2->v.val_double)
		{
			*piResult = 1;
		}
		else
		{
			*piResult = -1;
		}
		return;

	case SG_VARIANT_TYPE_INT64:
		if (pv1->v.val_int64 == pv2->v.val_int64)
		{
			*piResult = 0;
		}
		else if (pv1->v.val_int64 > pv2->v.val_int64)
		{
			*piResult = 1;
		}
		else
		{
			*piResult = -1;
		}
		return;

	case SG_VARIANT_TYPE_SZ:
		*piResult = strcmp(pv1->v.val_sz, pv2->v.val_sz);
		return;
	}

	SG_ERR_THROW_RETURN(  SG_ERR_INVALIDARG  );
}

//////////////////////////////////////////////////////////////////

int SG_variant_sort_callback__increasing(SG_context * pCtx,
										 const void * pVoid_ppv1, // const SG_variant ** ppv1
										 const void * pVoid_ppv2, // const SG_variant ** ppv2
										 SG_UNUSED_PARAM(void * pVoidCallerData))
{
	// see SG_qsort() for discussion.

	const SG_variant** ppv1 = (const SG_variant **)pVoid_ppv1;
	const SG_variant** ppv2 = (const SG_variant **)pVoid_ppv2;

	int c = 0;

	SG_UNUSED(pVoidCallerData);

	SG_variant__compare(pCtx, *ppv1, *ppv2, &c);

	// TODO decide if we want to do something more here...
	SG_ASSERT(!SG_context__has_err(pCtx));

	return c;
}

const char* sg_variant__type_name(SG_uint16 t)
{
    switch (t)
    {
	case SG_VARIANT_TYPE_DOUBLE:
        return "double";

	case SG_VARIANT_TYPE_INT64:
        return "int";

	case SG_VARIANT_TYPE_BOOL:
        return "bool";

	case SG_VARIANT_TYPE_NULL:
        return "null";

	case SG_VARIANT_TYPE_SZ:
        return "string";

	case SG_VARIANT_TYPE_VHASH:
        return "object";

	case SG_VARIANT_TYPE_VARRAY:
        return "array";
	}

    SG_ASSERT(0);

    return "unknown";
}

void SG_variant__to_string(
        SG_context* pCtx,
        SG_variant* pv,
        SG_string** ppstr
        )
{
    SG_string* pstr = NULL;
	SG_jsonwriter* pjson = NULL;

	SG_NULLARGCHECK_RETURN(pv);
	SG_NULLARGCHECK_RETURN(ppstr);

    SG_ERR_CHECK(  SG_string__alloc(pCtx, &pstr)  );
	SG_ERR_CHECK(  SG_jsonwriter__alloc(pCtx, &pjson, pstr)  );
	SG_ERR_CHECK(  sg_jsonwriter__write_variant(pCtx, pjson, pv)  );

    *ppstr = pstr;
    pstr = NULL;

fail:
    SG_JSONWRITER_NULLFREE(pCtx, pjson);
    SG_STRING_NULLFREE(pCtx, pstr);
}

static void SG_variant__free_contents(
        SG_context* pCtx,
        SG_variant* pv
        )
{
    if (!pv)
    {
        return;
    }

    switch (pv->type)
    {
    case SG_VARIANT_TYPE_SZ:
        SG_NULLFREE(pCtx, pv->v.val_sz);
        break;

    case SG_VARIANT_TYPE_VARRAY:
        SG_VARRAY_NULLFREE(pCtx, pv->v.val_varray);
        break;

    case SG_VARIANT_TYPE_VHASH:
        SG_VHASH_NULLFREE(pCtx, pv->v.val_vhash);
        break;
    }
}

void SG_variant__free(
        SG_context* pCtx,
        SG_variant* pv
        )
{
    if (!pv)
    {
        return;
    }

    SG_variant__free_contents(pCtx, pv);
    SG_NULLFREE(pCtx, pv);
}

