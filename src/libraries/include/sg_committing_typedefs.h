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
 * @file sg_committing_typedefs.h
 *
 * @details Routines to manage the transaction of creating a Changeset (a commit).
 *
 * When creating a Changeset you must do it within the context of a Transaction.
 * SG_committing is used to manage that process.  Use __alloc to begin a Transaction.
 * Then use various __store routines to add stuff to the Repository and Changeset.
 * Then use __end or __abort to complete or terminate the Transaction.
 *
 * ***Do not*** use SG_changeset routines directly to create or modify a Changeset
 * or use SG_repo routines directly to add stuff to the Repository.
 * Please use SG_committing instead.
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_COMMITTING_TYPEDEFS_H
#define H_SG_COMMITTING_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * A SG_committing is an in-memory only object to help manage the
 * transaction wrapping the creation of a permanent on-disk Changeset.
 */
typedef struct SG_committing SG_committing;

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_COMMITTING_TYPEDEFS_H
