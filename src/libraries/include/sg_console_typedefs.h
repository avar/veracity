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
 * @file sg_console_typedefs.h
 *
 * @details Typedefs for working with SG_console__ routines.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_CONSOLE_TYPEDEFS_H
#define H_SG_CONSOLE_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

enum _sg_console_stream
{
	SG_CS_STDOUT = 1,
	SG_CS_STDERR = 2
};

typedef enum _sg_console_stream SG_console_stream;

//////////////////////////////////////////////////////////////////

enum _sg_console_qa
{
	SG_QA_DEFAULT_YES =  1,
	SG_QA_DEFAULT_NO  =  0,
	SG_QA_NO_DEFAULT  = -1,
};

typedef enum _sg_console_qa SG_console_qa;

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_CONSOLE_TYPEDEFS_H
