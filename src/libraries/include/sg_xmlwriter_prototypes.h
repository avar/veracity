/*
  Copyright (C) 2008, SourceGear LLC.
*/

#ifndef H_SG_XMLWRITER_PROTOTYPES_H
#define H_SG_XMLWRITER_PROTOTYPES_H

BEGIN_EXTERN_C

#define SG_XMLWRITER_NULLFREE(c,w) SG_STATEMENT( if (w) { SG_xmlwriter__free((c),(w)); w = NULL; } )
void SG_xmlwriter__alloc(SG_context* pCtx, SG_xmlwriter** ppResult, SG_string* pDest, SG_bool bFormatted);
void SG_xmlwriter__free(SG_context* pCtx, SG_xmlwriter*);

void SG_xmlwriter__write_start_document(SG_context* pCtx, SG_xmlwriter* pxml);
void SG_xmlwriter__write_end_document(SG_context* pCtx, SG_xmlwriter* pxml);

void SG_xmlwriter__write_start_element__sz(SG_context* pCtx, SG_xmlwriter* pxml, const char* putf8Name);
void SG_xmlwriter__write_content__sz(SG_context* pCtx, SG_xmlwriter* pxml, const char* putf8Content);
void SG_xmlwriter__write_content__buflen(SG_context* pCtx, SG_xmlwriter* pxml, const char* putf8Text, SG_uint32 len_bytes);
void SG_xmlwriter__write_end_element(SG_context* pCtx, SG_xmlwriter* pxml);
void SG_xmlwriter__write_full_end_element(SG_context* pCtx, SG_xmlwriter* pxml);
void SG_xmlwriter__write_element__sz(SG_context* pCtx, SG_xmlwriter* pxml, const char* putf8Name, const char* putf8Content);

void SG_xmlwriter__write_attribute__sz(SG_context* pCtx, SG_xmlwriter* pxml, const char* putf8Name, const char* putf8Value);

void SG_xmlwriter__write_comment__sz(SG_context* pCtx, SG_xmlwriter* pxml, const char* putf8Text);

END_EXTERN_C

#endif

