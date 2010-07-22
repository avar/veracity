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
 * @file sg_repo_typedefs.h
 *
 * @details Typedefs for accessing a REPO.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_REPO_TYPEDEFS_H
#define H_SG_REPO_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

typedef struct _sg_repo_store_blob_handle_is_undefined SG_repo_store_blob_handle;
typedef struct _sg_repo_fetch_blob_handle_is_undefined SG_repo_fetch_blob_handle;
typedef struct _sg_repo_fetch_repo_handle_is_undefined SG_repo_fetch_repo_handle;
typedef struct _sg_repo_hash_handle_is_undefined SG_repo_hash_handle;

typedef struct _sg_repo_tx_handle_is_undefined SG_repo_tx_handle;
typedef struct _sg_repo_qresult_is_undefined SG_repo_qresult;

typedef struct _SG_repo         SG_repo;

//////////////////////////////////////////////////////////////////

/**
 * A hash-method is a string of the form "SHA1/160", "SHA2/256", or "SHA2/512".
 * The first part gives the algorithm and the second part gives the bit-length
 * of the hash.
 *
 * The (hex digit string) string length of a hash computed using a hash-method
 * is then ((bit-length / 8) * 2).  For example, for "SHA1/160" we have 40 and
 * for SHA2/256 we have 64 character hex digit strings.  (Plus 1 for the NULL.)
 */
#define SG_REPO_HASH_METHOD__MAX_BUFFER_LENGTH				100
#define SG_REPO_HASH_METHOD__NAME_FORMAT					"%s/%d"

//////////////////////////////////////////////////////////////////

/**
 * SG_repo__check_integrity() allows the following checks to be performed.
 */
#define SG_REPO__CHECK_INTEGRITY__DAG_CONSISTENCY           1

//////////////////////////////////////////////////////////////////

/**
 * SG_repo__query_implementation() allows the following questions to be asked.
 *
 * These are divided into 2 groups.
 *
 * [1] Questions not specific to one REPO implementation.  Such as requesting
 *     a list of the installed REPO implementations.
 *
 * [2] Questions about a specific REPO implementation.  Such as what optional
 *     features it supports.
 *
 * Type [1] questions can be asked any time (with or without a valid SG_repo pointer).
 * Type [2] questions con only be asked about a properly allocated SG_repo pointer.
 *
 * When given a valid SG_repo pointer, we ONLY use it to determine which REPO
 * implementation it is bound to; we DO NOT care whether it refers to an OPENED
 * or CREATED repository.  That is, we DO NOT use SG_repo__query_implementation()
 * to ask specific questions about a repo instance on disk.
 *
 *
 * SG_repo__query_implementation() takes a single question input parameter and
 * takes various output parameters; which one is used depends upon the type of
 * question.  We indicate which output parameters are used in the declaration.
 * When the output is a vhash, you own the returned vhash and must free it.
 */

#define SG_REPO__QUESTION__TYPE_1                                        ((SG_uint16)0x1000)
#define SG_REPO__QUESTION__TYPE_2                                        ((SG_uint16)0x2000)

#define SG_REPO__QUESTION__VHASH__LIST_REPO_IMPLEMENTATIONS              (SG_REPO__QUESTION__TYPE_1 | 0x000)

#define SG_REPO__QUESTION__VHASH__LIST_HASH_METHODS                      (SG_REPO__QUESTION__TYPE_2 | 0x000)
#define SG_REPO__QUESTION__BOOL__SUPPORTS_CHANGE_BLOB_ENCODING           (SG_REPO__QUESTION__TYPE_2 | 0x001)
#define SG_REPO__QUESTION__BOOL__SUPPORTS_OBLITERATE_BLOB                (SG_REPO__QUESTION__TYPE_2 | 0x002)
#define SG_REPO__QUESTION__BOOL__SUPPORTS_VACUUM                         (SG_REPO__QUESTION__TYPE_2 | 0x003)
#define SG_REPO__QUESTION__BOOL__SUPPORTS_LIVE_WITH_WORKING_DIRECTORY    (SG_REPO__QUESTION__TYPE_2 | 0x004)
#define SG_REPO__QUESTION__BOOL__SUPPORTS_ZLIB                           (SG_REPO__QUESTION__TYPE_2 | 0x005)
#define SG_REPO__QUESTION__BOOL__SUPPORTS_VCDIFF                         (SG_REPO__QUESTION__TYPE_2 | 0x006)
#define SG_REPO__QUESTION__BOOL__SUPPORTS_DBNDX                          (SG_REPO__QUESTION__TYPE_2 | 0x007)
#define SG_REPO__QUESTION__BOOL__SUPPORTS_TREEDX                         (SG_REPO__QUESTION__TYPE_2 | 0x008)

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_REPO_TYPEDEFS_H

