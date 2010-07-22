/*
  Copyright (C) 2008, SourceGear LLC.
*/

#include <sg.h>

#include "sg_xmlwriter_typedefs.h"
#include "sg_xmlwriter_prototypes.h"
#include "unittests.h"

#define XML_STATIC

/* TODO if we put in an ASCII bell here, expat chokes on it.  why? */

#define JUNK1     "@*#&$^@*  hello there '\"&well<foo>1"

struct u0019xmlstate
{
	SG_xmlwriter* pxml;
	SG_error err;
};

#if 0
static void XMLCALL
u0019_startElement(SG_context *pCtx, void *userData, const char *name, const char **atts)
{
    struct u0019xmlstate* pst = (struct u0019xmlstate*) userData;
	int i;
	
	i = 0;
	while (atts[i])
	{
		if (0 == strcmp(atts[i], "junk1"))
		{
			VERIFY_COND_FAIL("junk1", (0 == strcmp(atts[i+1], JUNK1)));
		}
		i+=2;
	}

	VERIFY_ERR_CHECK( SG_xmlwriter__write_start_element__sz(pCtx, pst->pxml, name)  );

	i = 0;
	while (atts[i])
	{
		VERIFY_ERR_CHECK( SG_xmlwriter__write_attribute__sz(pCtx, pst->pxml, atts[i], atts[i+1])  );
		i+=2;
	}

fail:
	;
}

static void XMLCALL
u0019_endElement(SG_context *pCtx, void *userData, SG_UNUSED_PARAM(const char *name))
{
    struct u0019xmlstate* pst = (struct u0019xmlstate*) userData;

    SG_UNUSED(name);
    VERIFY_ERR_CHECK( SG_xmlwriter__write_end_element(pCtx, pst->pxml)  );
fail:
	;
}

static void XMLCALL
u0019_charData(SG_context *pCtx, void* userData, const XML_Char* ch, int len)
{
    struct u0019xmlstate* pst = (struct u0019xmlstate*) userData;
	VERIFY_ERR_CHECK( SG_xmlwriter__write_content__buflen(pCtx, pst->pxml, ch, len)  );
fail:
	;
}

static void XMLCALL
u0019_comment_handler(SG_context *pCtx, void* userData, const XML_Char* ch)
{
    struct u0019xmlstate* pst = (struct u0019xmlstate*) userData;
	VERIFY_ERR_CHECK( SG_xmlwriter__write_comment__sz(pCtx, pst->pxml, ch)  );
fail:
	;
}
#endif

void u0019_xmlwriter__test_1(SG_context *pCtx)
{
//	SG_error err;
	SG_string* pstr = NULL;
	SG_string* pstr2 = NULL;
	char* pid = NULL;

	/* TODO both xmlwriters here are in unformatted mode.  it would be
	 * kind of nice to run the first one formatted and the second one
	 * unformatted and get them to match */
	
	SG_xmlwriter* pxml = NULL;
//	XML_Parser parser;
//	int expatresult;
	const char* pszXML;
//	const char* pszXML2;
	int lenxml;
//	int lenxml2;
	struct u0019xmlstate pst;

	pst.err = SG_ERR_OK;
	pst.pxml = NULL;

	VERIFY_ERR_CHECK(  SG_string__alloc(pCtx, &pstr)  );
	
	VERIFY_ERR_CHECK(  SG_gid__alloc(pCtx, &pid)  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__alloc(pCtx, &pxml, pstr, SG_FALSE)  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_start_document(pCtx, pxml)  );
	
	VERIFY_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, pxml, "foo")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_content__sz(pCtx, pxml, NULL)  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_content__buflen(pCtx, pxml, NULL, 5)  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_content__buflen(pCtx, pxml, "hello", 0)  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_attribute__sz(pCtx, pxml, "version", "1")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_attribute__sz(pCtx, pxml, "junk1", JUNK1)  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_attribute__sz(pCtx, pxml, "idnotreally", NULL)  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, pxml, "bar", "5")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, pxml, "record")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, pxml, "header")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_content__sz(pCtx, pxml, "fried")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_content__sz(pCtx, pxml, "a")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, pxml, "title", "Lord of the Rings")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, pxml, "author", "J.R.R. Tolkien")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_end_element(pCtx, pxml)  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, pxml, "body")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, pxml, "character", "Frodo Baggins")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, pxml, "character", "Gandalf")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, pxml, "Arwen", "<different in the book & the movie>")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, pxml, "nothing", NULL)  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, pxml, "nada", "")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, pxml, "nobody")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_comment__sz(pCtx, pxml, "This is a comment")  );

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, pxml, "id", pid)  );
	

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, pxml, "idnotreally", NULL)  );
	

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_full_end_element(pCtx, pxml)  );
	

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_end_element(pCtx, pxml)  );
	

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_end_element(pCtx, pxml)  );
	

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_end_document(pCtx, pxml)  );
	

	SG_XMLWRITER_NULLFREE(pCtx, pxml);
	SG_NULLFREE(pCtx, pid);

	pszXML = SG_string__sz(pstr);
	lenxml = (int) strlen(pszXML);
	
	//printf("%s\n", pszXML);

	VERIFY_ERR_CHECK(  SG_string__alloc(pCtx, &pstr2)  );
	VERIFY_ERR_CHECK(  SG_xmlwriter__alloc(pCtx, &pst.pxml, pstr2, SG_FALSE)  );
	VERIFY_ERR_CHECK(  SG_xmlwriter__write_start_document(pCtx, pst.pxml)  );
#if 0	
	parser = XML_ParserCreate("utf-8");
	XML_SetUserData(parser, &pst);
	XML_SetElementHandler(parser, u0019_startElement, u0019_endElement);
	XML_SetCharacterDataHandler(parser, u0019_charData);
	XML_SetCommentHandler(parser, u0019_comment_handler);
	expatresult = XML_Parse(parser, pszXML, lenxml, 1);
#if 0
	fprintf(stderr,
			"%s at line %lu\n",
			XML_ErrorString(XML_GetErrorCode(parser)),
			XML_GetCurrentLineNumber(parser));
#endif
	VERIFY_COND_FAIL("expat", (expatresult != XML_STATUS_ERROR));
	XML_ParserFree(parser);

	err = SG_xmlwriter__write_end_document(pst.pxml);
	SG_XMLWRITER_NULLFREE(pst.pxml);
	
	pszXML2 = SG_string__sz(pstr2);
	lenxml2 = (int) strlen(pszXML2);

	VERIFY_COND_FAIL("match", (0 == SG_utf8__compare(pszXML, pszXML2)));
	
	//printf("%s\n", pszXML2);
#endif

fail:
	SG_STRING_NULLFREE(pCtx, pstr2);
	SG_STRING_NULLFREE(pCtx, pstr);
	SG_XMLWRITER_NULLFREE(pCtx, pst.pxml);
	SG_XMLWRITER_NULLFREE(pCtx, pxml);
	
}

void u0019_xmlwriter__test_3(SG_context *pCtx)
{
	SG_string* pstr = NULL;

	SG_xmlwriter* pxml = NULL;

	VERIFY_ERR_CHECK(  SG_string__alloc(pCtx, &pstr)  );
	
	VERIFY_ERR_CHECK(  SG_xmlwriter__alloc(pCtx, &pxml, pstr, SG_FALSE)  );

	SG_xmlwriter__write_end_element(pCtx, pxml);
	VERIFY_COND("cannot end an element when there is none",  SG_context__has_err(pCtx));
	SG_context__err_reset(pCtx);

	SG_xmlwriter__write_full_end_element(pCtx, pxml);
	VERIFY_COND("cannot end an element when there is none", (SG_context__has_err(pCtx)));
	SG_context__err_reset(pCtx);

	SG_xmlwriter__write_attribute__sz(pCtx, pxml, "version", "1");
	VERIFY_COND("cannot write an attribute when there is no start element", (SG_context__has_err(pCtx)));
	SG_context__err_reset(pCtx);

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_start_document(pCtx, pxml)  );
	

	SG_xmlwriter__write_end_element(pCtx, pxml);
	VERIFY_COND("cannot end an element when there is none", (SG_context__has_err(pCtx)));
	SG_context__err_reset(pCtx);

	SG_xmlwriter__write_start_element__sz(pCtx, pxml, "foo");
	

	SG_xmlwriter__write_start_document(pCtx, pxml);
	VERIFY_COND("can't start the document twice", (SG_context__has_err(pCtx)));
	SG_context__err_reset(pCtx);

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_attribute__sz(pCtx, pxml, "version", "1")  );
	

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_element__sz(pCtx, pxml, "bar", "5")  );
	

	SG_xmlwriter__write_attribute__sz(pCtx, pxml, "junk1", JUNK1);
	VERIFY_COND("there is no open start element", (SG_context__has_err(pCtx)));
	SG_context__err_reset(pCtx);

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_start_element__sz(pCtx, pxml, "record")  );
	

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_attribute__sz(pCtx, pxml, "quimby", "1")  );
	

	VERIFY_ERR_CHECK(  SG_xmlwriter__write_comment__sz(pCtx, pxml, "This is a comment")  );
	

	SG_xmlwriter__write_attribute__sz(pCtx, pxml, "quimby", "1");
	VERIFY_COND("there is no open start element", (SG_context__has_err(pCtx)));
	SG_context__err_reset(pCtx);

	
fail:
	SG_XMLWRITER_NULLFREE(pCtx, pxml);

	SG_STRING_NULLFREE(pCtx, pstr);
}

TEST_MAIN(u0019_xmlwriter)
{
	TEMPLATE_MAIN_START;

	BEGIN_TEST(  u0019_xmlwriter__test_1(pCtx)  );
    BEGIN_TEST(  u0019_xmlwriter__test_3(pCtx)  );

	TEMPLATE_MAIN_END;
}

