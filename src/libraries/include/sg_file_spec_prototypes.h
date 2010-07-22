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

// sg_file_spec_prototypes.h
// Declarations for the file globbing layer.
//////////////////////////////////////////////////////////////////

#ifndef H_SG_FILE_SPEC_PROTOTYPES_H
#define H_SG_FILE_SPEC_PROTOTYPES_H

BEGIN_EXTERN_C


void SG_file_spec__append_pattern(SG_context* pCtx, const char* pszPattern, SG_uint32 nCount, const char*** pppszPatterns);

void SG_file_spec__append_pattern_array(SG_context* pCtx, char** ppszPatterns, SG_uint32 nCount, const char*** pppszPatterns);

void SG_file_spec__read_patterns_from_file(SG_context* pCtx, const char* pszPathname, SG_stringarray** psaPatterns);

void SG_file_spec__patterns_from_ignores_localsetting(SG_context* pCtx, SG_stringarray** psaPatterns);

/**
 * These are REPO-PATHS.
 */
void SG_file_spec__should_include(SG_context* pCtx,
								  const char* const* ppszIncludePatterns, SG_uint32 count_includes,
								  const char* const* ppszExcludePatterns, SG_uint32 count_excludes,
								  const char* const* ppszIgnorePatterns,  SG_uint32 count_ignores,
								  const char* pszPathname,
								  SG_file_spec_eval * pEval);

void sg_file_spec__wildcmp(SG_context* pCtx, const char* pszWild, const char* pszTest, SG_bool * pbResult);

END_EXTERN_C

#endif//H_SG_FILE_PROTOTYPES_H
