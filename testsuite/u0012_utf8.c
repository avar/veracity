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

// testsuite/unittests/u0012_utf8.c
// test various utf8 utilities.
//////////////////////////////////////////////////////////////////

#include <sg.h>

#include "unittests.h"

//////////////////////////////////////////////////////////////////

int u0012_utf8__test_misc(SG_context* pCtx)
{
	SG_uint32 utf8len = 0;
	char buf[] = { 'a', 'b', 0 };

	VERIFY_COND("test_misc", (SG_utf8__length_in_bytes(buf) == sizeof(buf)));	// length in bytes includes null terminator.

	VERIFY_ERR_CHECK_RETURN(  SG_utf8__length_in_characters__sz(pCtx, buf, &utf8len)  );
	VERIFY_COND("utf8length", (utf8len == 2));  // length in characters does NOT include null terminator

	return 1;
}

#if defined(WINDOWS)
int u0012_utf8__test_conversions_windows(SG_context* pCtx)
{
	wchar_t wbuf[] = { 0x0041, // A
					   0x00C0, // A w/ Grave
					   0x0041, // A
					   0x00C6, // AE
					   0x0041, // A
					   0x0100, // A w/ Macron
					   0x0041, // A
					   0x01DC, // A w/ Caron
					   0x0041, // A
					   0x0394, // Delta
					   0x0041, // A
					   0x0410, // Cyrillic A
					   0x0041, // A
					   0x222B, // Integral
					   0x0041, // A
					   0x0000,
	};

	SG_string * pString = NULL;
	wchar_t * pwbufNew = NULL;
	SG_uint32 lenResult2;
	char bufDump[2048];

	VERIFY_ERR_CHECK_RETURN(  SG_STRING__ALLOC(pCtx, &pString)  );

	// convert wchar unicode to utf8.

	VERIFY_ERR_CHECK( SG_utf8__intern_from_os_buffer__wchar(pCtx, pString,wbuf)  );

	SG_hex__format_buf(bufDump,(SG_byte *)SG_string__sz(pString),(SG_uint32)strlen(SG_string__sz(pString)));
	INFOP("test_conversion",("Utf8 buffer contains [%s]",bufDump));

	// convert utf8 back to wchar unicode.

	VERIFY_ERR_CHECK(  SG_utf8__extern_to_os_buffer__wchar(pCtx, SG_string__sz(pString),&pwbufNew,&lenResult2)  );	// we must free pwbufNew

	// verify round trip.

	VERIFY_COND("test_conversion",(memcmp(wbuf,pwbufNew,sizeof(wbuf)) == 0));

	SG_STRING_NULLFREE(pCtx, pString);
	SG_NULLFREE(pCtx, pwbufNew);

	return 1;

fail:
	SG_STRING_NULLFREE(pCtx, pString);
	SG_NULLFREE(pCtx, pwbufNew);
	return 0;
}
#endif

int u0012_utf8__test_conversions_utf32(SG_context* pCtx)
{
	SG_int32 wbuf[] = { 0x0041, // A
					   0x00C0, // A w/ Grave
					   0x0041, // A
					   0x00C6, // AE
					   0x0041, // A
					   0x0100, // A w/ Macron
					   0x0041, // A
					   0x01DC, // A w/ Caron
					   0x0041, // A
					   0x0394, // Delta
					   0x0041, // A
					   0x0410, // Cyrillic A
					   0x0041, // A
					   0x222B, // Integral
					   0x0041, // A
					   0x0000,
	};

	char * szUtf8New = NULL;
	SG_int32 * pwbufNew = NULL;
	SG_uint32 lenResult1, lenResult2;
	char bufDump[2048];
	SG_uint32 utf8len = 0;

	// convert utf32 unicode to utf8.

	VERIFY_ERR_CHECK(  SG_utf8__from_utf32(pCtx, wbuf,&szUtf8New,&lenResult1)  );		// we must free szUtf8New

	VERIFY_ERR_CHECK(  SG_utf8__length_in_characters__sz(pCtx, szUtf8New, &utf8len)  );
	VERIFY_COND("utf8length", (utf8len == 15));

	SG_hex__format_buf(bufDump,(SG_byte*)szUtf8New,lenResult1);
	INFOP("test_conversion",("Utf8 buffer contains [%s]",bufDump));

	// convert utf8 back to utf32 unicode.

	VERIFY_ERR_CHECK(  SG_utf8__to_utf32__sz(pCtx, szUtf8New,&pwbufNew,&lenResult2)  );	// we must free pwbufNew

	// verify round trip.

	VERIFY_COND("test_conversion",(memcmp(wbuf,pwbufNew,sizeof(wbuf)) == 0));

	SG_NULLFREE(pCtx, szUtf8New);
	SG_NULLFREE(pCtx, pwbufNew);

	return 1;

fail:
	SG_NULLFREE(pCtx, szUtf8New);
	SG_NULLFREE(pCtx, pwbufNew);

	return 0;
}

void u0012_utf8__test_locale_conversions(SG_context* pCtx, const char * szCharSetAliasIn,
										 const char ** aszCharSetCanonicalNameIn,
										 const char * szLocaleStringIn,
										 const char * szUtf8StringIn)
{
	// create a locale/charset converter and try some things.

	SG_utf8_converter * pCvtr = NULL;
	char * szUtf8Out = NULL;
	char * szLocaleOut = NULL;
	const char * szCharSetCanonicalNameReturned;
	const char ** argv;
	const char * sz_k;

	// create a converter for the named alias encoding.
	// verify that the canonical name of the item created matches what we thought it would be.

	VERIFY_ERR_CHECK(  SG_utf8_converter__alloc(pCtx, szCharSetAliasIn,&pCvtr)  );
	VERIFY_ERR_CHECK(  SG_utf8_converter__get_canonical_name(pCtx, pCvtr,&szCharSetCanonicalNameReturned)  );
	INFOP("local",("[%s] mapped to [%s]",szCharSetAliasIn,szCharSetCanonicalNameReturned));

	// see if the name returned matches one of the values we were expecting.
	// this is a little annoying because we are using different versions of ICU
	// on (the one installed on the system) and they might have different versions
	// of the spec.

	argv = aszCharSetCanonicalNameIn;
	sz_k = *argv++;
	while (sz_k)
	{
		if (strcmp(szCharSetCanonicalNameReturned,sz_k)==0)
			goto found_alias;

		sz_k = *argv++;
	}
	VERIFYP_COND("local",(0),("Name [%s] not expected.",szCharSetCanonicalNameReturned));
	// either way, we want to continue testing the data.

found_alias:

	// use converter to convert a locale/charset-based string into utf8.
	// verify it matches what we expected.
	// then repeat going the other way.

	VERIFY_ERR_CHECK(  SG_utf8_converter__from_charset__sz(pCtx, pCvtr,szLocaleStringIn,&szUtf8Out)  );
	VERIFY_COND("locale",(strcmp(szUtf8Out,szUtf8StringIn)==0));

	VERIFY_ERR_CHECK(  SG_utf8_converter__to_charset__sz(pCtx, pCvtr,szUtf8StringIn,&szLocaleOut)  );
	VERIFY_COND("locale",(strcmp(szLocaleOut,szLocaleStringIn)==0));

	// fall-thru to common cleanup

fail:
	SG_NULLFREE(pCtx, szUtf8Out);
	SG_NULLFREE(pCtx, szLocaleOut);
	SG_UTF8_CONVERTER_NULLFREE(pCtx, pCvtr);
}

TEST_MAIN(u0012_utf8)
{
	const char * aszArgvLatin1[] = { "ISO-8859-1", NULL };
	const char * aszArgvCP1252[] = { "ibm-5348_P100-1997", NULL };
	const char * aszArgvLatin7[] = { "ibm-9005_X100-2005", "ibm-9005_X110-2007", NULL };

	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0012_utf8__test_misc(pCtx)  );
#if defined(WINDOWS)
	BEGIN_TEST(  u0012_utf8__test_conversions_windows(pCtx)  );
#endif
	BEGIN_TEST(  u0012_utf8__test_conversions_utf32(pCtx)  );

	BEGIN_TEST(  u0012_utf8__test_locale_conversions(pCtx, "latin1",
										aszArgvLatin1,
										"aaa\xc0\xcf.txt",				//   xC0 latin capital a with grave    xCF latin capital latter I with diaeresis
										"aaa\xc3\x80\xc3\x8f.txt")  ); // u00c0 latin capital a with grave  u00cf latin capital latter I with diaeresis
	BEGIN_TEST(  u0012_utf8__test_locale_conversions(pCtx, "cp1252",
										aszArgvCP1252,
										"aaa\xc0\xcf.txt",			//   xC0 latin capital a with grave    xCF latin capital latter I with diaeresis
										"aaa\xc3\x80\xc3\x8f.txt")  ); // u00c0 latin capital a with grave  u00cf latin capital latter I with diaeresis

	BEGIN_TEST(  u0012_utf8__test_locale_conversions(pCtx, "iso-8859-7",
										aszArgvLatin7,
										"aaa\xc1\xc2.txt",			//   xC1 Alpha                         xC2 Beta
										"aaa\xce\x91\xce\x92.txt")  );	// u0391 greek capital letter alpha  u0392 greek capital letter beta

	TEMPLATE_MAIN_END;
}
