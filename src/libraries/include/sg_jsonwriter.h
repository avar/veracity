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

#ifndef H_SG_JSONWRITER_H
#define H_SG_JSONWRITER_H

BEGIN_EXTERN_C

void SG_jsonwriter__alloc(SG_context * pCtx, SG_jsonwriter** ppResult, SG_string* pDest);
void SG_jsonwriter__free(SG_context * pCtx, SG_jsonwriter*);

/*
  http://www.json.org/

  Two kinds of containers: object and array

  An object contains pairs.  Each pair is "name" : value

  An array contains elements.  Each element is a value.
*/

void sg_jsonwriter__write_variant(SG_context * pCtx, SG_jsonwriter* pjson, const SG_variant* pv);
void SG_jsonwriter__write_start_object(SG_context * pCtx, SG_jsonwriter* pjson);
void SG_jsonwriter__write_end_object(SG_context * pCtx, SG_jsonwriter* pjson);

void SG_jsonwriter__write_start_array(SG_context * pCtx, SG_jsonwriter* pjson);
void SG_jsonwriter__write_end_array(SG_context * pCtx, SG_jsonwriter* pjson);

void SG_jsonwriter__write_begin_pair(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8Name);

/* These are for writing object pairs. */
void SG_jsonwriter__write_pair__variant(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8Name, const SG_variant* pv);

void SG_jsonwriter__write_pair__null(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8Name);
void SG_jsonwriter__write_pair__bool(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8Name, SG_bool b);
void SG_jsonwriter__write_pair__int64(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8Name, SG_int64 v);
void SG_jsonwriter__write_pair__double(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8Name, double v);
void SG_jsonwriter__write_pair__string__sz(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8Name, const char* putf8Value);
void SG_jsonwriter__write_pair__vhash(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8Name, SG_vhash* pvh);
void SG_jsonwriter__write_pair__varray(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8Name, SG_varray* pva);

void SG_jsonwriter__write_pair__varray__rbtree(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8Name, SG_rbtree* pva);

/* These are for writing array elements. */
void SG_jsonwriter__write_begin_element(SG_context * pCtx, SG_jsonwriter* pjson);

void SG_jsonwriter__write_element__variant(SG_context * pCtx, SG_jsonwriter* pjson, const SG_variant* pv);

void SG_jsonwriter__write_element__null(SG_context * pCtx, SG_jsonwriter* pjson);
void SG_jsonwriter__write_element__bool(SG_context * pCtx, SG_jsonwriter* pjson, SG_bool b);
void SG_jsonwriter__write_element__int64(SG_context * pCtx, SG_jsonwriter* pjson, SG_int64 v);
void SG_jsonwriter__write_element__double(SG_context * pCtx, SG_jsonwriter* pjson, double v);
void SG_jsonwriter__write_element__string__sz(SG_context * pCtx, SG_jsonwriter* pjson, const char* putf8Value);
void SG_jsonwriter__write_element__vhash(SG_context * pCtx, SG_jsonwriter* pjson, SG_vhash* pvh);
void SG_jsonwriter__write_element__varray(SG_context * pCtx, SG_jsonwriter* pjson, SG_varray* pva);

END_EXTERN_C

#endif
