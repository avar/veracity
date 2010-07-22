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

#ifndef H_SG_JSONPARSER_H
#define H_SG_JSONPARSER_H

BEGIN_EXTERN_C

enum
{
    SG_JSONPARSER_TYPE_NONE = 0,
    SG_JSONPARSER_TYPE_ARRAY_BEGIN,
    SG_JSONPARSER_TYPE_ARRAY_END,
    SG_JSONPARSER_TYPE_OBJECT_BEGIN,
    SG_JSONPARSER_TYPE_OBJECT_END,
    SG_JSONPARSER_TYPE_INTEGER,
    SG_JSONPARSER_TYPE_FLOAT,
    SG_JSONPARSER_TYPE_NULL,
    SG_JSONPARSER_TYPE_TRUE,
    SG_JSONPARSER_TYPE_FALSE,
    SG_JSONPARSER_TYPE_STRING,
    SG_JSONPARSER_TYPE_KEY,
    SG_JSONPARSER_TYPE_MAX
};

typedef struct
{
    union
	{
        SG_int64 integer_value;
        long double float_value;
        struct
		{
            const char* value;
            size_t length;
        } str;
    } vu;
} SG_jsonparser_value;

/*! \brief JSON parser callback

    \param ctx The pointer passed to new_JSON_parser.
    \param type An element of JSON_type but not SG_JSONPARSER_TYPE_NONE.
    \param value A representation of the parsed value. This parameter is NULL for
        SG_JSONPARSER_TYPE_ARRAY_BEGIN, SG_JSONPARSER_TYPE_ARRAY_END, SG_JSONPARSER_TYPE_OBJECT_BEGIN, SG_JSONPARSER_TYPE_OBJECT_END,
        SG_JSONPARSER_TYPE_NULL, SG_JSONPARSER_TYPE_TRUE, and SON_T_FALSE. String values are always returned
        as zero-terminated C strings.

    \return Non-zero if parsing should continue, else zero.
*/
typedef void (*SG_jsonparser_callback)(SG_context * pCtx,
									   void* pVoidCallbackData,
									   int type,
									   const SG_jsonparser_value* value);

void SG_jsonparser__alloc(SG_context * pCtx,
						  SG_jsonparser** ppNew,
						  SG_jsonparser_callback callback, void* pVoidCallbackData);

/*! \brief Destroy a previously created JSON parser object. */
extern void SG_jsonparser__free(SG_context * pCtx, SG_jsonparser* jc);

/*! \brief Parse a character.

    \return Non-zero, if all characters passed to this function are part of are valid JSON.
*/
extern void SG_jsonparser__chars(SG_context * pCtx, SG_jsonparser* jc, const char* psz);

/*! \brief Finalize parsing.

    Call this method once after all input characters have been consumed.

    \return Non-zero, if all parsed characters are valid JSON, zero otherwise.
*/
extern void SG_jsonparser__done(SG_context * pCtx, SG_jsonparser* jc);

END_EXTERN_C

#endif
