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

// sg_web_utils.c
// web utils.
//////////////////////////////////////////////////////////////////

#include <sg.h>
#include <ctype.h>

//////////////////////////////////////////////////////////////////



static const char *_findEncoding(
	char ch)
{
	const char *result = NULL;

	switch (ch)
	{
		case '<':
			result = "&lt;";
			break;

		case '>':
			result = "&gt;";
			break;

		case '&':
			result = "&amp;";
			break;

		case '"':
			result = "&quot;";
			break;
	}

	return(result);
}



void SG_htmlencode(
	SG_context *pCtx,
	SG_string *raw,			/**< plain text, can contain <, >, &, etc */
	SG_string *encoded)		/**< HTML-escapged version of 'raw'. must be pre-allocated */
{
	const char *s = NULL;

	SG_NULLARGCHECK_RETURN(raw);
	SG_NULLARGCHECK_RETURN(encoded);

	SG_ERR_CHECK_RETURN(  SG_string__clear(pCtx, encoded)  );
	SG_ERR_CHECK_RETURN(  SG_string__make_space(pCtx, encoded, SG_string__length_in_bytes(raw))  );

	for ( s = SG_string__sz(raw); *s; ++s )
	{
		const char *enc = _findEncoding(*s);

		if (enc == NULL)
		{
			SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, encoded, (const SG_byte *)s, sizeof(char))  );
		}
		else
		{
			SG_ERR_CHECK_RETURN(  SG_string__append__sz(pCtx, encoded, enc)  );
		}
	}
}

#define xval(c)	(isdigit(c) ? ((c) - '0') : ((tolower(c) - 'a') + 10))

static void _decodeString(SG_context *pCtx, const char *sz, SG_string *str)
{
	int		ccode = 0;
	char	ch;

	SG_ERR_CHECK_RETURN(  SG_string__clear(pCtx, str)  );

	while (*sz)
	{
		if (*sz == '%')
		{
			if (! *++sz)
				break;
			if (! isxdigit(*sz))
				break;

			ccode = xval(*sz);

			if (! *++sz)
				break;
			if (! isxdigit(*sz))
				break;

			ccode = (ccode * 16) + xval(*sz);

			ch = (char)ccode;

			SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, str, (const SG_byte *)&ch, sizeof(char))  );
		}
		else if (*sz == '+')
		{
			ch = ' ';
			SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, str, (const SG_byte *)&ch, sizeof(char))  );
		}
		else
		{
			SG_ERR_CHECK_RETURN(  SG_string__append__buf_len(pCtx, str, (const SG_byte *)sz, sizeof(char))  );
		}

		++sz;
	}
}


void SG_querystring_to_vhash(
	SG_context *pCtx,
	const char *pQueryString,	/**< query string to parse. Must not be null */
	SG_vhash   *params)  		/**< each parameter and its associated, decoded value */
{
#	define MAXPAIRS	256
	char **pairs = NULL;
	SG_string *qscopy = NULL;
	SG_uint32 count = 0;
	SG_uint32 partCount = 0;
	char **parts = NULL;
	SG_uint32 i;
	SG_string *vari = NULL;
	SG_string *value = NULL;

	SG_NULLARGCHECK_RETURN(params);

	SG_ERR_CHECK(  SG_string__split__sz_asciichar(pCtx, pQueryString, '&', MAXPAIRS, &pairs, &count)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &vari)  );
	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &value)  );

	for ( i = 0; i < count; ++i )
	{
		if (pairs[i][0] == 0)
			continue;

		SG_ERR_CHECK(  SG_string__split__sz_asciichar(pCtx, pairs[i], '=', 2, &parts, &partCount)  );
		SG_ERR_CHECK(  _decodeString(pCtx, parts[0], vari)  );
		SG_ERR_CHECK(  _decodeString(pCtx, parts[1], value)  );

		SG_ERR_CHECK(  SG_vhash__add__string__sz(pCtx, params, SG_string__sz(vari), SG_string__sz(value))  );
		SG_ERR_CHECK(  SG_freeStringList(pCtx, (const char ***)&parts, partCount)  );
	}

fail:
	SG_ERR_IGNORE(	SG_freeStringList(pCtx, (const char ***)&pairs, count)  );
	SG_STRING_NULLFREE(pCtx, qscopy);
	SG_STRING_NULLFREE(pCtx, vari);
	SG_STRING_NULLFREE(pCtx, value);
}
