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
 * @file sg_repo_vtable__sqlite.h
 *
 * @details Private declarations for the sqlite
 * implementation of the VTABLE mechanism.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_REPO_VTABLE__SQLITE_H
#define H_SG_REPO_VTABLE__SQLITE_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////
// declare prototypes for all routines in the vtable.

DCL__REPO_VTABLE_PROTOTYPES(sqlite);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_REPO_VTABLE__SQLITE_H

