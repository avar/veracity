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
 * @file sg_changeset_typedefs.h
 *
 * @details A Changeset is a representation of a check-in/commit.
 * Everything we need to rebuild the state of the user's project as
 * of a particular version.  The Changeset does not directly contain
 * all of the change information; rather, it should be thought of as
 * a handle (containing pointers) to the various data structures.
 *
 * The actual in-memory format of a Changeset is opaque.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_CHANGESET_TYPEDEFS_H
#define H_SG_CHANGESET_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/* NOTICE:  If you add one of these, make sure you update g_reftypes as needed. */

#define SG_BLOB_REFTYPE__NONE          0x0000
#define SG_BLOB_REFTYPE__TREENODE      0x0001
#define SG_BLOB_REFTYPE__TREEUSERFILE  0x0002
#define SG_BLOB_REFTYPE__TREESYMLINK   0x0004
#define SG_BLOB_REFTYPE__TREEATTRIBS   0x0008
#define SG_BLOB_REFTYPE__DBINFO        0x0010
#define SG_BLOB_REFTYPE__DBRECORD      0x0020
#define SG_BLOB_REFTYPE__DBUSERFILE    0x0040
#define SG_BLOB_REFTYPE__DBTEMPLATE    0x0080

#define SG_BLOB_REFTYPE__ALL           0xFFFF

extern const SG_uint16 g_reftypes[];

/**
 * (CSET) Version Identifier.
 *
 * Indicates what version of the software created the Changeset object.
 * This is important when we read an object from a blob.  For
 * newly created Changeset objects, we should always use the current
 * software versrion.
 */
/* TODO these need the SG_ prefix */
enum SG_changeset_version
{
	CSET_VERSION__INVALID=-1,					/**< Invalid Value, must be -1. */
	CSET_VERSION_1=1,							/**< Version 1. */

	CSET_VERSION__CURRENT=CSET_VERSION_1,		/**< Current Software Version.  Must be latest version defined. */
	CSET_VERSION__RANGE,						/**< Last defined valued + 1. */
};

typedef enum SG_changeset_version SG_changeset_version;

/**
 * A Changeset is a representation of a check-in/commit.
 *
 * A SG_changeset is used to represent an existing commit already
 * stored in the repository.  To create a new Changeset, use SG_committing.
 */
typedef struct SG_changeset SG_changeset;

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_CHANGESET_TYPEDEFS_H
