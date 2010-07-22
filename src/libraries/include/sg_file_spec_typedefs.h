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
 *
 * @file sg_file_spec_typedefs.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_FILE_SPEC_TYPEDEFS_H
#define H_SG_FILE_SPEC_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

enum _sg_file_spec_eval
{
	SG_FILE_SPEC_EVAL__MAYBE			    = 0x00,

	SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED  = 0x01,
	SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED   = 0x02,

	SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED  = 0x10,
	SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED  = 0x20,
};

typedef enum _sg_file_spec_eval SG_file_spec_eval;

#define SG_FILE_SPEC_EVAL__IS_MAYBE(eval)					((eval) == SG_FILE_SPEC_EVAL__MAYBE)

#define SG_FILE_SPEC_EVAL__IS_EXCLUDED_OR_IGNORED(eval)		(((eval) & (SG_FILE_SPEC_EVAL__EXPLICITLY_EXCLUDED | SG_FILE_SPEC_EVAL__EXPLICITLY_IGNORED)) != 0)

#define SG_FILE_SPEC_EVAL__IS_INCLUDED(eval)				(((eval) & (SG_FILE_SPEC_EVAL__EXPLICITLY_INCLUDED | SG_FILE_SPEC_EVAL__IMPLICITLY_INCLUDED)) != 0)

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_FILE_SPEC_TYPEDEFS_H
