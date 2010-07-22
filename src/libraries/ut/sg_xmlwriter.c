/*
  Copyright (C) 2008, SourceGear LLC.
*/

#include <sg.h>
#include "sg_xmlwriter_typedefs.h"
#include "sg_xmlwriter_prototypes.h"

typedef struct _sg_xmlstate
{
	char tag[255+1];
	SG_bool bCompletedStartTag;
	SG_bool bContainsSubTags;
	struct _sg_xmlstate* pNext;
} sg_xmlstate;

struct _SG_xmlwriter
{
	SG_string* pDest;
	sg_xmlstate* pState;
	SG_bool bFormatted;
};

void SG_xmlwriter__alloc(SG_context* pCtx, SG_xmlwriter** ppResult, SG_string* pDest, SG_bool bFormatted)
{
	SG_xmlwriter * pThis;

	SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(SG_xmlwriter), &pThis)  );

	pThis->pDest = pDest;
	pThis->pState = NULL;
	pThis->bFormatted = bFormatted;

	*ppResult = pThis;

fail:
	return;
}

SG_uint32 sg_xmlwriter__get_current_depth(SG_xmlwriter* pxml)
{
	SG_uint32 result = 0;
	sg_xmlstate* pst = pxml->pState;
	while (pst)
	{
		result++;
		pst = pst->pNext;
	}	
	return result;
}

void SG_xmlwriter__free(SG_context *pCtx, SG_xmlwriter* pxml)
{
	if (!pxml)
	{
		return;
	}
	
	while (pxml->pState)
	{
		sg_xmlstate* pst = pxml->pState;
		pxml->pState = pst->pNext;
		SG_NULLFREE(pCtx, pst);
	}	
	
	SG_NULLFREE(pCtx, pxml);
}

void SG_xmlwriter__write_start_document(SG_context* pCtx, SG_xmlwriter* pxml)
{
	if (pxml->pState)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_XMLWRITERNOTWELLFORMED  );
	}

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, "<?xml version=\"1.0\"?>")  );

#if 0
	if (pxml->bFormatted)
	{
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, "\n")  );		
	}
#endif
	
	return;

fail:
	return;
}

void SG_xmlwriter__write_end_document(SG_context* pCtx, SG_xmlwriter* pxml)
{
	while (pxml->pState)
	{
		SG_ERR_CHECK(  SG_xmlwriter__write_end_element(pCtx, pxml)  );
	}

#if 0
	if (pxml->bFormatted)
	{
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, "\n")  );
	}
#endif
	
	return;

fail:
	return;
}

void sg_xmlwriter__complete_start_tag(SG_context* pCtx, SG_xmlwriter* pxml)
{
	if (
		pxml->pState
		&& !pxml->pState->bCompletedStartTag
		)
	{
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, ">")  );
		pxml->pState->bCompletedStartTag = SG_TRUE;
	}
	return;

fail:
	return;
}

void sg_xmlwriter__indent(SG_context* pCtx, SG_xmlwriter* pxml)
{
	SG_uint32 depth;
	SG_uint32 i;

	if (pxml->bFormatted)
	{
		SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pxml->pDest, "\n")  );
		
		depth = sg_xmlwriter__get_current_depth(pxml);
		for (i=0; i<depth; i++)
		{
			SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, pxml->pDest, "  ")  );
		}
	}
}

void SG_xmlwriter__write_start_element__sz(SG_context* pCtx, SG_xmlwriter* pxml, const char* putf8Name)
{
	sg_xmlstate* pst = NULL;
	
	SG_ERR_CHECK(  sg_xmlwriter__complete_start_tag(pCtx, pxml)  );

	SG_ERR_CHECK(  sg_xmlwriter__indent(pCtx, pxml)  );
	
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, "<")  );
	
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, putf8Name)  );

	if (pxml->pState)
	{
		pxml->pState->bContainsSubTags = SG_TRUE;
	}
	
	SG_ERR_CHECK(  SG_alloc(pCtx, 1, sizeof(sg_xmlstate), &pst)  );

	SG_ERR_CHECK(  SG_strcpy(pCtx, pst->tag, sizeof(pst->tag), putf8Name)  );

	pst->bCompletedStartTag = SG_FALSE;
	pst->bContainsSubTags = SG_FALSE;
	pst->pNext = pxml->pState;
	
	pxml->pState = pst;

	return;

fail:
	if (pst)
	{
		SG_NULLFREE(pCtx, pst);
	}
}

void SG_xmlwriter__write_end_element(SG_context* pCtx, SG_xmlwriter* pxml)
{
	sg_xmlstate* pst = NULL;
	
	if (!pxml->pState)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_XMLWRITERNOTWELLFORMED  );
	}

	pst = pxml->pState;
	pxml->pState = pst->pNext;

	if (pst->bCompletedStartTag)
	{
		if (pst->bContainsSubTags)
		{
			SG_ERR_CHECK(  sg_xmlwriter__indent(pCtx, pxml)  );
		}
		
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, "</")  );

		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, pst->tag)  );

		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, ">")  );
	}
	else
	{
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, "/>")  );
	}
	
	SG_NULLFREE(pCtx, pst);
	return;
	
fail:
	if (pst)
	{
		SG_NULLFREE(pCtx, pst);
	}
}

void SG_xmlwriter__write_full_end_element(SG_context* pCtx, SG_xmlwriter* pxml)
{
	sg_xmlstate* pst = NULL;
	
	if (!pxml->pState)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_XMLWRITERNOTWELLFORMED  );
	}

	SG_ERR_CHECK(  sg_xmlwriter__complete_start_tag(pCtx, pxml)  );

	pst = pxml->pState;
	
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, "</")  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, pst->tag)  );
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, ">")  );
	
	pxml->pState = pst->pNext;
	SG_NULLFREE(pCtx, pst);
	return;
	
fail:
	return;
}

void SG_xmlwriter__write_element__sz(SG_context* pCtx, SG_xmlwriter* pxml, const char* putf8Name, const char* putf8Content)
{
	SG_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, pxml, putf8Name)  );
	
	SG_ERR_CHECK(  SG_xmlwriter__write_content__sz(pCtx, pxml, putf8Content)  );
	
	SG_ERR_CHECK(  SG_xmlwriter__write_end_element(pCtx, pxml)  );

	return;
	
fail:
	return;
}


/*

  The characters &, <, and > are replaced with &amp;, &lt;, and &gt;,
  respectively.

  Character values in the range 0x-0x1F (excluding white space
  characters 0x9, 0xA, and 0xD) are replaced with numeric character
  entities (&#0; through &#0x1F).

  If bAttribute,
  double and single quotes are replaced with &quot; and &apos;
  respectively.

*/

void sg_xmlwriter__escape_one_codepoint(SG_UNUSED_PARAM(SG_context* pCtx), SG_int32 c, SG_int32* pBuf, SG_uint32* pLen, SG_bool bAttribute)
{
	SG_UNUSED(pCtx);

	if (c == '&')
	{
		pBuf[(*pLen)++] = '&';
		pBuf[(*pLen)++] = 'a';
		pBuf[(*pLen)++] = 'm';
		pBuf[(*pLen)++] = 'p';
		pBuf[(*pLen)++] = ';';
		return;
	}
	else if (c == '<')
	{
		pBuf[(*pLen)++] = '&';
		pBuf[(*pLen)++] = 'l';
		pBuf[(*pLen)++] = 't';
		pBuf[(*pLen)++] = ';';
		return;
	}
	else if (c == '>')
	{
		pBuf[(*pLen)++] = '&';
		pBuf[(*pLen)++] = 'g';
		pBuf[(*pLen)++] = 't';
		pBuf[(*pLen)++] = ';';
		return;
	}
	else if (
		(c == 0x9)
		|| (c == 0xa)
		|| (c == 0xd)
		)
	{
		pBuf[(*pLen)++] = c;
		return;
	}
	else if (
		(c >= 0x0)
		&& (c <= 0x1f)
		)
	{
		static const char* hex = "0123456789ABCDEF";
		
		pBuf[(*pLen)++] = '&';
		pBuf[(*pLen)++] = '#';
		pBuf[(*pLen)++] = 'x';
		pBuf[(*pLen)++] = hex[ (c >> 4) & 0x0f ];
		pBuf[(*pLen)++] = hex[ (c     ) & 0x0f ];
		pBuf[(*pLen)++] = ';';
		return;
	}
	else if (
		bAttribute
		&& (c == '"')
		)
	{
		pBuf[(*pLen)++] = '&';
		pBuf[(*pLen)++] = 'q';
		pBuf[(*pLen)++] = 'u';
		pBuf[(*pLen)++] = 'o';
		pBuf[(*pLen)++] = 't';
		pBuf[(*pLen)++] = ';';
		return;
	}
	else if (
		bAttribute
		&& (c == '\'')
		)
	{
		pBuf[(*pLen)++] = '&';
		pBuf[(*pLen)++] = 'a';
		pBuf[(*pLen)++] = 'p';
		pBuf[(*pLen)++] = 'o';
		pBuf[(*pLen)++] = 's';
		pBuf[(*pLen)++] = ';';
		return;
	}
	else
	{
		pBuf[(*pLen)++] = c;
		return;
	}
}

void sg_xmlwriter__escape(SG_context* pCtx, const char* putf8_orig, SG_uint32 len_utf8_bytes_orig, char** putf8_escaped, SG_bool bAttribute)
{
	SG_int32* putf32_orig = NULL;
	SG_int32* putf32_escaped = NULL;
	SG_uint32 len_codepoints_orig;
	SG_uint32 len_codepoints_escaped = 0;
	SG_uint32 len_utf8_bytes_escaped;
	SG_uint32 ndx;
	SG_uint32 i;
	
	SG_ERR_CHECK(  SG_utf8__to_utf32__buflen(pCtx, putf8_orig,len_utf8_bytes_orig,&putf32_orig,&len_codepoints_orig)  );

	for (i=0; i<len_codepoints_orig; i++)
	{
		SG_int32 buf[31+1];
		SG_int32 c = putf32_orig[i];
		SG_uint32 len = 0;

		SG_ERR_CHECK(  sg_xmlwriter__escape_one_codepoint(pCtx, c, buf, &len, bAttribute)  );

		len_codepoints_escaped += len;
	}

	SG_ERR_CHECK(  SG_alloc(pCtx, len_codepoints_escaped+1, sizeof(SG_int32), &putf32_escaped)  );

	ndx = 0;
	for (i=0; i<len_codepoints_orig; i++)
	{
		SG_int32 c = putf32_orig[i];

		SG_ERR_CHECK(  sg_xmlwriter__escape_one_codepoint(pCtx, c, putf32_escaped, &ndx, bAttribute)  );
	}
	putf32_escaped[ndx] = 0;

	SG_NULLFREE(pCtx, putf32_orig);
	putf32_orig = NULL;
	
	SG_ERR_CHECK(  SG_utf8__from_utf32(pCtx, putf32_escaped,putf8_escaped,&len_utf8_bytes_escaped)  );

	SG_NULLFREE(pCtx, putf32_escaped);

	return;
	
fail:
	if (putf32_orig)
	{
		SG_NULLFREE(pCtx, putf32_orig);
	}

	if (putf32_escaped)
	{
		SG_NULLFREE(pCtx, putf32_escaped);
	}
}

void SG_xmlwriter__write_content__buflen(SG_context* pCtx, SG_xmlwriter* pxml, const char* putf8Text, SG_uint32 len_bytes)
{
	char* putf8Escaped = NULL;
	
	if (
		(!putf8Text)
		|| (len_bytes == 0)
		)
	{
		return;
	}

	SG_ERR_CHECK(  sg_xmlwriter__complete_start_tag(pCtx, pxml)  );

	SG_ERR_CHECK(  sg_xmlwriter__escape(pCtx, putf8Text, len_bytes, &putf8Escaped, SG_FALSE)  );
	
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, putf8Escaped)  );

	SG_NULLFREE(pCtx, putf8Escaped);
	putf8Escaped = NULL;
	
	return;

fail:
	if (putf8Escaped)
	{
		SG_NULLFREE(pCtx, putf8Escaped);
	}
}

void SG_xmlwriter__write_content__sz(SG_context* pCtx, SG_xmlwriter* pxml, const char* putf8Text)
{
	if (
		(!putf8Text)
		|| (!putf8Text[0])
		)
	{
		return;
	}

	SG_xmlwriter__write_content__buflen(pCtx, pxml, putf8Text, SG_utf8__length_in_bytes(putf8Text));
}

void SG_xmlwriter__write_attribute__sz(SG_context* pCtx, SG_xmlwriter* pxml, const char* putf8Name, const char * putf8Value)
{
	sg_xmlstate* pst = NULL;
	char* putf8Escaped = NULL;
	
	if (!pxml->pState)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_XMLWRITERNOTWELLFORMED  );
	}

	pst = pxml->pState;
	if (pst->bCompletedStartTag)
	{
		SG_ERR_THROW_RETURN(  SG_ERR_XMLWRITERNOTWELLFORMED  );
	}

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, " ")  );

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, putf8Name)  );

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, "=\"")  );

	if (putf8Value && putf8Value[0])
	{
		SG_ERR_CHECK(  sg_xmlwriter__escape(pCtx, putf8Value, SG_utf8__length_in_bytes(putf8Value), &putf8Escaped, SG_TRUE)  );
		
		SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, putf8Escaped)  );
		
		SG_NULLFREE(pCtx, putf8Escaped);
		putf8Escaped = NULL;
	}
	
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, "\" ")  );

	return;
	
fail:
	if (putf8Escaped)
	{
		SG_NULLFREE(pCtx, putf8Escaped);
	}
}


void SG_xmlwriter__write_comment__sz(SG_context* pCtx, SG_xmlwriter* pxml, const char* putf8Text)
{
	SG_ERR_CHECK(  sg_xmlwriter__complete_start_tag(pCtx, pxml)  );

	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, "<!--")  );

	/* TODO how should we escape this text? */
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, putf8Text)  );
	
	SG_ERR_CHECK(  SG_string__append__sz(pCtx, pxml->pDest, "-->")  );

	return;
	
fail:
	return;
}

