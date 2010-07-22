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
 * @file sg_localsettings_prototypes.h
 *
 * @details Routines for getting and setting local settings.
 */

#ifndef H_SG_LOCALSETTINGS_PROTOTYPES_H
#define H_SG_LOCALSETTINGS_PROTOTYPES_H

BEGIN_EXTERN_C;

void SG_localsettings__update__sz(
        SG_context * pCtx,
        const char * psz_path,
        const char * pValue
        );

void SG_localsettings__descriptor__update__sz(
	SG_context * pCtx,
	const char * psz_descriptor_name,
	const char * psz_path,
	const char * pValue);

void SG_localsettings__update__varray(
    SG_context * pCtx,
    const char * psz_path,
    const SG_varray * pValue
    );

void SG_localsettings__update__vhash(
        SG_context * pCtx,
        const char * psz_path,
        const SG_vhash * pValue
        );

void SG_localsettings__get__variant(
	SG_context * pCtx,
	const char * psz_path,
	SG_repo* pRepo,
	SG_variant** ppv,
	SG_string** ppstr_where_found
	);

void SG_localsettings__get__sz(
        SG_context * pCtx,
        const char * psz_path,
        SG_repo* pRepo,
        char** ppsz,
        SG_string** ppstr_where_found
        );

void SG_localsettings__get__varray(
        SG_context * pCtx,
        const char * psz_path,
        SG_repo* pRepo,
        SG_varray** ppva,
        SG_string** ppstr_where_found
        );

void SG_localsettings__varray__empty(
        SG_context * pCtx,
        const char* psz_path
        );

void SG_localsettings__varray__append(
        SG_context * pCtx,
        const char* psz_path,
        const char* psz
        );

void SG_localsettings__varray__remove_first_match(
        SG_context * pCtx,
        const char* psz_path,
        const char* psz
        );

void SG_localsettings__reset(
        SG_context * pCtx,
        const char* psz_path
        );

void SG_localsettings__factory__list__vhash(
        SG_context* pCtx,
        SG_vhash** ppvh     /**< Caller must free this */
        );

void SG_localsettings__descriptor__get__sz(
	SG_context * pCtx,
	const char * psz_descriptor_name,
	const char * psz_path,
	char ** ppszValue); /**< Caller must free this */

void SG_localsettings__list__vhash(
        SG_context * pCtx,
        SG_vhash** ppResult /**< Caller must free this */
        );

void SG_localsettings__import(
        SG_context * pCtx,
        SG_vhash* pvh /**< Caller still owns this */
        );

/**
 * Splits a fully-qualified setting name into its constituent scope and local parts.
 * Example: Given full name "/instance/veracity/paths/default", this function
 *          will split it into scope "/instance/veracity" and name "paths/default".
 */
void SG_localsettings__split_full_name(
	SG_context* pCtx,        //< Error and context information.
	const char* szFullName,  //< The full name to split.
	SG_uint32*  uSplitIndex, //< If non-NULL, will be filled with the index that the full name was split at.
	SG_string*  pScopeName,  //< If non-NULL, will be filled with the scope name.
	SG_string*  pSettingName //< If non-NULL, will be filled with the local setting name.
	);

/**
 * Callback used to consume settings provided by SG_localsettings__foreach.
 */
typedef void (SG_localsettings_foreach_callback)(
	SG_context*       pCtx,       //< Error and context information.
	const char*       szFullName, //< The fully-qualified name of the current setting, including scope.
	const char*       szScope,    //< The name of the scope that the current setting is in, including repo/admin/instance name.
	const char*       szName,     //< The name of the current setting, without scope.
	const SG_variant* pValue,     //< The value of the current setting.
	void*             pCallerData //< Data provided by the caller of SG_localsettings__foreach.
	);

/**
 * Iterates through each local setting and provides it to a callback function.
 * Setting names are provided fully-qualified, using '/' to indicate nesting and scope.
 */
void SG_localsettings__foreach(
	SG_context*                        pCtx,             //< Error and context information.
	const char*                        szPattern,        //< If non-NULL, only local setting keys that match the pattern will be sent to the callback.
	                                                     //< Patterns that start with '/' will be matched against the beginning of the fully qualified setting name.
	                                                     //< Other patterns will match anywhere in the setting's non-scoped name.
	                                                     //< No wildcards are supported.
	SG_bool                            bIncludeDefaults, //< If true, then default values will be iterated as well, in a scope named "default".
	SG_localsettings_foreach_callback* pCallback,        //< The callback function to call with each matching name/value pair.
	void*                              pCallerData       //< Caller-specific data to pass through to the callback.
	);

END_EXTERN_C;

#endif//H_SG_LOCALSETTINGS_PROTOTYPES_H

