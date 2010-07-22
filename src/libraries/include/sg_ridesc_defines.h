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
 * @file sg_ridesc_defines.h
 *
 * @details #defines for the string keys in a repo instance descriptor
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_RIDESC_DEFINES_H
#define H_SG_RIDESC_DEFINES_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

#define SG_RIDESC_KEY__STORAGE					"storage"

#define SG_RIDESC_STORAGE__FS2					"fs2"
#define SG_RIDESC_STORAGE__FS3					"fs3"
#define SG_RIDESC_FSLOCAL__PATH_PARENT_DIR		"path_parent_dir"
#define SG_RIDESC_FSLOCAL__DIR_NAME				"dir_name"

#define SG_RIDESC_STORAGE__SQLITE				"sqlite"
#define SG_RIDESC_SQLITE__FILENAME				"db_filename"

#define SG_RIDESC_STORAGE__DEFAULT				SG_RIDESC_STORAGE__FS3

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_RIDESC_DEFINES_H
