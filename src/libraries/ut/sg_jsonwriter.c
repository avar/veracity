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

#define SG_JSON_TYPE_OBJECT 1
#define SG_JSON_TYPE_ARRAY  2

typedef struct _sg_jsonstate
{
	SG_uint8 type;
	SG_uint8 depth;
	SG_uint32 count_items;
	struct _sg_jsonstate* pNext;
} sg_jsonstate;

struct _SG_jsonwriter
{
	SG_string* pDest;
	sg_jsonstate* pState;
};

/*
  For readability in diagnostic situations, our canonical form
  for JSON contains a little bit of whitespace and formatting.

  DO NOT CHANGE THIS.  We calculate hashes on the JSON representation.
  If you have two JSON objects which are identical except for
  whitespace between the tokens, a JSON parser will see them as
  identical, but the SHA-256 algorithm will not.

  So, if you change this, the SHA-256 HID (hash ID) of lots of
  objects will change even the object itself has not changed.
  This will cause all kinds of problems, including that the
  storage layer will begin storing two copies of all those objects.

  Bottom line:  DO NOT CHANGE THIS.
*/

void sg_jsonwriter__indent(SG_context * pCtx, SG_jsonwriter* pjson)
{
	SG_uint8 i;

	if (!pjson->pState)
	{
		return;
	}

	switch (pjson->pState->depth)
	{
	case 1:
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\t")  );
		break;

	case 2:
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\t\t")  );
		break;

	case 3:
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\t\t\t")  );
		break;

	case 4:
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\t\t\t\t")  );
		break;

	case 5:
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\t\t\t\t\t")  );
		break;

	case 6:
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\t\t\t\t\t\t")  );
		break;

	case 7:
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\t\t\t\t\t\t\t")  );
		break;

	case 8:
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\t\t\t\t\t\t\t\t")  );
		break;

	default:
		for (i=0; i<pjson->pState->depth; i++)
		{
			SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\t")  );
		}
	}

	return;

fail:
	return;
}

void SG_jsonwriter__alloc(SG_context * pCtx, SG_jsonwriter** ppResult, SG_string* pDest)
{
	SG_jsonwriter * pThis;

	SG_ERR_CHECK_RETURN(  SG_alloc1(pCtx, pThis)  );

	pThis->pDest = pDest;
	pThis->pState = NULL;

	*ppResult = pThis;
}

void SG_jsonwriter__free(SG_context * pCtx, SG_jsonwriter* pjson)
{
	if (!pjson)
		return;

	while (pjson->pState)
	{
		sg_jsonstate* pst = pjson->pState;
		pjson->pState = pst->pNext;
		SG_NULLFREE(pCtx, pst);
	}

	SG_NULLFREE(pCtx, pjson);
}

void SG_jsonwriter__write_start_object(SG_context * pCtx, SG_jsonwriter* pjson)
{
	sg_jsonstate* pst = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pst)  );

	if (pjson->pState
		&& (pjson->pState->type == SG_JSON_TYPE_OBJECT)
		)
	{
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\n")  );
		SG_ERR_CHECK(  sg_jsonwriter__indent(pCtx, pjson)  );
	}

	pst->type = SG_JSON_TYPE_OBJECT;
	pst->count_items = 0;
	pst->pNext = pjson->pState;
	if (pst->pNext)
	{
		pst->depth = 1 + pst->pNext->depth;
	}
	else
	{
		pst->depth = 1;
	}
	pjson->pState = pst;

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "{\n")  );

	SG_ERR_CHECK(  sg_jsonwriter__indent(pCtx, pjson)  );

	return;

fail:
	return;
}

void SG_jsonwriter__write_end_object(SG_context * pCtx, SG_jsonwriter* pjson)
{
	sg_jsonstate* pst = NULL;

	if (!pjson->pState)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_JSONWRITERNOTWELLFORMED  );
	}

	if (pjson->pState->type != SG_JSON_TYPE_OBJECT)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_JSONWRITERNOTWELLFORMED  );
	}

	pst = pjson->pState;
	pjson->pState = pst->pNext;

	SG_NULLFREE(pCtx, pst);
	pst = NULL;

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\n")  );
	SG_ERR_CHECK(  sg_jsonwriter__indent(pCtx, pjson)  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "}")  );

	if (!pjson->pState)
	{
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\n")  );
	}

	return;

fail:
	return;
}

void SG_jsonwriter__write_start_array(SG_context * pCtx, SG_jsonwriter* pjson)
{
	sg_jsonstate* pst = NULL;

	SG_ERR_CHECK(  SG_alloc1(pCtx, pst)  );

	if (pjson->pState
		&& (pjson->pState->type == SG_JSON_TYPE_OBJECT)
		)
	{
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\n")  );
		SG_ERR_CHECK(  sg_jsonwriter__indent(pCtx, pjson)  );
	}

	pst->type = SG_JSON_TYPE_ARRAY;
	pst->count_items = 0;
	pst->pNext = pjson->pState;
	if (pst->pNext)
	{
		pst->depth = 1 + pst->pNext->depth;
	}
	else
	{
		pst->depth = 1;
	}
	pjson->pState = pst;

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "[\n")  );
	SG_ERR_CHECK(  sg_jsonwriter__indent(pCtx, pjson)  );

	return;

fail:
	return;
}

void SG_jsonwriter__write_end_array(SG_context * pCtx, SG_jsonwriter* pjson)
{
	sg_jsonstate* pst = NULL;

	if (!pjson->pState)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_JSONWRITERNOTWELLFORMED  );
	}

	if (pjson->pState->type != SG_JSON_TYPE_ARRAY)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_JSONWRITERNOTWELLFORMED  );
	}

	pst = pjson->pState;
	pjson->pState = pst->pNext;

	SG_NULLFREE(pCtx, pst);
	pst = NULL;

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\n")  );
	SG_ERR_CHECK(  sg_jsonwriter__indent(pCtx, pjson)  );

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "]")  );

	return;

fail:
	return;
}

void sg_jsonwriter__write_comma_if_needed(SG_context * pCtx, SG_jsonwriter* pjson)
{
	if (!pjson->pState)
	{
		return;
	}

	if (pjson->pState->count_items > 0)
	{
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, ",\n")  );
		SG_ERR_CHECK(  sg_jsonwriter__indent(pCtx, pjson)  );
	}

	return;

fail:
	return;
}

void sg_jsonwriter__write_int64(SG_context * pCtx, SG_jsonwriter* pjson, SG_int64 i)
{
	SG_int_to_string_buffer tmp;
	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, SG_int64_to_sz(i,tmp))  );
}

void sg_jsonwriter__write_double(SG_context * pCtx, SG_jsonwriter* pjson, double d)
{
	char buf[256];

	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx, buf, 255, "%f", d)  );

	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, buf)  );
}

void sg_jsonwriter__write_unescaped_string__sz(SG_context * pCtx,
											   SG_jsonwriter* pjson, const char* putf8)
{
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\"")  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, putf8)  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\"")  );

	return;

fail:
	return;
}

void sg_jsonwriter__write_escaped_string__sz(SG_context * pCtx,
											 SG_jsonwriter* pjson, const char* putf8)
{
	SG_string* pstr = NULL;
    const char* p = putf8;

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
    while (*p)
    {
        unsigned char c = (unsigned char) *p;

        if (c == '\\')
        {
            SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pstr, (void*) "\\\\", 2)  );
        }
        else if (c == '"')
        {
            SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pstr, (void*) "\\\"", 2)  );
        }
        else if (c == '\n')
        {
            SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pstr, (void*) "\\n", 2)  );
        }
        else if (c == '\r')
        {
            SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pstr, (void*) "\\r", 2)  );
        }
        else if (c == '\t')
        {
            SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pstr, (void*) "\\t", 2)  );
        }
        else if (c < 32)
        {
            char buf[8];
            static const char* hex = "0123456789abcdef";

            buf[0] = '\\';
            buf[1] = 'u';
            buf[2] = '0';
            buf[3] = '0';
            buf[4] = hex[ (c >> 4) & 0x0f ];
            buf[5] = hex[ (c     ) & 0x0f ];
            buf[6] = 0;

            SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pstr, (void*) buf, 6)  );
        }
        else
        {
            SG_ERR_CHECK(  SG_string__append__buf_len(pCtx, pstr, (void*) &c, 1)  );
        }

        p++;
    }

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\"")  );

	SG_ERR_CHECK(  SG_string__append__string(pCtx, pjson->pDest, pstr)  );

	SG_STRING_NULLFREE(pCtx, pstr);

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, "\"")  );

	return;

fail:
	SG_STRING_NULLFREE(pCtx, pstr);
}

void sg_jsonwriter__does_string_need_to_be_escaped(const char* psz, SG_bool* pb)
{
    const unsigned char* p = (unsigned char*) psz;

    while (*p)
    {
        if (
                (*p == '\\')
                || (*p == '"')
                || (*p < 32)
           )
        {
            *pb = SG_TRUE;
            return;
        }
        p++;
    }

    *pb = SG_FALSE;
}

void sg_jsonwriter__write_string__sz(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8)
{
    SG_bool b_escape_it = SG_FALSE;

    sg_jsonwriter__does_string_need_to_be_escaped(putf8, &b_escape_it);
    if (b_escape_it)
    {
        SG_ERR_CHECK(  sg_jsonwriter__write_escaped_string__sz(pCtx, pjson, putf8)  );
    }
    else
    {
        SG_ERR_CHECK(  sg_jsonwriter__write_unescaped_string__sz(pCtx, pjson, putf8)  );
    }

    return;

fail:
    return;
}

void SG_jsonwriter__write_begin_pair(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8Name)
{
	if (!pjson->pState)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_JSONWRITERNOTWELLFORMED  );
	}

	if (pjson->pState->type != SG_JSON_TYPE_OBJECT)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_JSONWRITERNOTWELLFORMED  );
	}

	SG_ERR_CHECK(  sg_jsonwriter__write_comma_if_needed(pCtx, pjson)  );

	SG_ERR_CHECK(  sg_jsonwriter__write_string__sz(pCtx, pjson, putf8Name)  );

	pjson->pState->count_items++;

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pjson->pDest, " : ")  );

	return;

fail:
	return;
}

void sg_jsonwriter__write_variant(SG_context * pCtx, SG_jsonwriter* pjson, const SG_variant* pv)
{
	switch (pv->type)
	{
	case SG_VARIANT_TYPE_NULL:
		SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, "null")  );
		return;

	case SG_VARIANT_TYPE_SZ:
		SG_ERR_CHECK_RETURN(  sg_jsonwriter__write_string__sz(pCtx, pjson, pv->v.val_sz)  );
		return;

	case SG_VARIANT_TYPE_INT64:
        if (SG_int64__fits_in_int32(pv->v.val_int64))
        {
            SG_ERR_CHECK_RETURN(  sg_jsonwriter__write_int64(pCtx, pjson, pv->v.val_int64)  );
			return;
        }
        else
        {
            SG_int_to_string_buffer buf;
            SG_int64_to_sz(pv->v.val_int64,buf);
		    SG_ERR_CHECK_RETURN(  sg_jsonwriter__write_string__sz(pCtx, pjson, buf)  );
			return;
        }

	case SG_VARIANT_TYPE_DOUBLE:
		SG_ERR_CHECK_RETURN(  sg_jsonwriter__write_double(pCtx, pjson, pv->v.val_double)  );
		return;

	case SG_VARIANT_TYPE_BOOL:
		if (pv->v.val_bool)
		{
			SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, "true")  );
			return;
		}
		else
		{
			SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, "false")  );
			return;
		}

	case SG_VARIANT_TYPE_VHASH:
		SG_ERR_CHECK_RETURN(  SG_vhash__write_json(pCtx, pv->v.val_vhash, pjson)  );
		return;

	case SG_VARIANT_TYPE_VARRAY:
		SG_ERR_CHECK_RETURN(  SG_varray__write_json(pCtx, pv->v.val_varray, pjson)  );
		return;

	default:
		SG_ERR_THROW_RETURN(  SG_ERR_UNSPECIFIED  );
	}
}

void SG_jsonwriter__write_pair__varray__rbtree(SG_context * pCtx,
											   SG_jsonwriter* pjson, const char* putf8Name, SG_rbtree* pva)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_pair(pCtx, pjson, putf8Name)  );

	SG_ERR_CHECK_RETURN(  SG_rbtree__write_json_array__keys_only(pCtx, pva, pjson)  );
}

void SG_jsonwriter__write_pair__varray(SG_context * pCtx,
									   SG_jsonwriter* pjson, const char* putf8Name, SG_varray* pva)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_pair(pCtx, pjson, putf8Name)  );

	SG_ERR_CHECK_RETURN(  SG_varray__write_json(pCtx, pva, pjson)  );
}

void SG_jsonwriter__write_pair__vhash(SG_context * pCtx,
									  SG_jsonwriter* pjson, const char* putf8Name, SG_vhash* pvh)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_pair(pCtx, pjson, putf8Name)  );

	SG_ERR_CHECK_RETURN(  SG_vhash__write_json(pCtx, pvh, pjson)  );
}

void SG_jsonwriter__write_pair__variant(SG_context * pCtx,
										SG_jsonwriter* pjson, const char* putf8Name, const SG_variant* pv)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_pair(pCtx, pjson, putf8Name)  );

	SG_ERR_CHECK_RETURN(  sg_jsonwriter__write_variant(pCtx, pjson, pv)  );
}

void SG_jsonwriter__write_pair__string__sz(SG_context * pCtx,
										   SG_jsonwriter* pjson, const char* putf8Name, const char* putf8Value)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_pair(pCtx, pjson, putf8Name)  );

	SG_ERR_CHECK_RETURN(  sg_jsonwriter__write_string__sz(pCtx, pjson, putf8Value)  );
}

void SG_jsonwriter__write_pair__double(SG_context * pCtx,
									   SG_jsonwriter* pjson, const char* putf8Name, double v)
{
	char buf[64];

	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_pair(pCtx, pjson, putf8Name)  );

	SG_ERR_CHECK_RETURN(  SG_sprintf(pCtx, buf, 63, "%f", v)  );

	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, buf)  ); // no need to escape this.
}

void SG_jsonwriter__write_pair__int64(SG_context * pCtx,
									  SG_jsonwriter* pjson, const char* putf8Name, SG_int64 v)
{
	SG_int_to_string_buffer buf;

	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_pair(pCtx, pjson, putf8Name)  );

	SG_int64_to_sz(v,buf);

	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, buf)  ); // no need to escape this.
}

void SG_jsonwriter__write_pair__bool(SG_context * pCtx,
									 SG_jsonwriter* pjson, const char* putf8Name, SG_bool b)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_pair(pCtx, pjson, putf8Name)  );

	if (b)
	{
		SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, "true")  );
		return;
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, "false")  );
		return;
	}
}

void SG_jsonwriter__write_pair__null(SG_context * pCtx,
									 SG_jsonwriter* pjson, const char* putf8Name)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_pair(pCtx, pjson, putf8Name)  );

	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, "null")  );
}

void SG_jsonwriter__write_begin_element(SG_context * pCtx,
										SG_jsonwriter* pjson)
{
	if (!pjson->pState)
	{
		SG_ERR_THROW(  SG_ERR_JSONWRITERNOTWELLFORMED  );
	}

	if (pjson->pState->type != SG_JSON_TYPE_ARRAY)
	{
		SG_ERR_THROW(  SG_ERR_JSONWRITERNOTWELLFORMED  );
	}

	SG_ERR_CHECK(  sg_jsonwriter__write_comma_if_needed(pCtx, pjson)  );

	pjson->pState->count_items++;

	return;

fail:
	return;
}

void SG_jsonwriter__write_element__variant(SG_context * pCtx,
										   SG_jsonwriter* pjson, const SG_variant* pv)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_element(pCtx, pjson)  );

	SG_ERR_CHECK_RETURN(  sg_jsonwriter__write_variant(pCtx, pjson, pv)  );
}

void SG_jsonwriter__write_element__string__sz(SG_context * pCtx,
											  SG_jsonwriter* pjson, const char* putf8)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_element(pCtx, pjson)  );

	SG_ERR_CHECK_RETURN(  sg_jsonwriter__write_string__sz(pCtx, pjson, putf8)  );
}

void SG_jsonwriter__write_element__null(SG_context * pCtx,
										SG_jsonwriter* pjson)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_element(pCtx, pjson)  );

	SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, "null")  );
}

void SG_jsonwriter__write_element__bool(SG_context * pCtx,
										SG_jsonwriter* pjson, SG_bool b)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_element(pCtx, pjson)  );

	if (b)
	{
		SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, "true")  );
		return;
	}
	else
	{
		SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pjson->pDest, "false")  );
		return;
	}
}

void SG_jsonwriter__write_element__int64(SG_context * pCtx,
										 SG_jsonwriter* pjson, SG_int64 v)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_element(pCtx, pjson)  );

	SG_ERR_CHECK_RETURN(  sg_jsonwriter__write_int64(pCtx, pjson, v)  );
}

void SG_jsonwriter__write_element__double(SG_context * pCtx,
										  SG_jsonwriter* pjson, double v)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_element(pCtx, pjson)  );

	SG_ERR_CHECK_RETURN(  sg_jsonwriter__write_double(pCtx, pjson, v)  );
}

void SG_jsonwriter__write_element__vhash(SG_context * pCtx,
										 SG_jsonwriter* pjson, SG_vhash* pvh)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_element(pCtx, pjson)  );

	SG_ERR_CHECK_RETURN(  SG_vhash__write_json(pCtx, pvh, pjson)  );
}

void SG_jsonwriter__write_element__varray(SG_context * pCtx,
										  SG_jsonwriter* pjson, SG_varray* pva)
{
	SG_ERR_CHECK_RETURN(  SG_jsonwriter__write_begin_element(pCtx, pjson)  );

	SG_ERR_CHECK_RETURN(  SG_varray__write_json(pCtx, pva, pjson)  );
}

