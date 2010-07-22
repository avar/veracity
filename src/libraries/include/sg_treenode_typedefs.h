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
 * @file sg_treenode_typedefs.h
 *
 * @details A Treenode is a representation of a single directory under version
 * control.  That is, a list of files, sub-directories, and etc. in a directory
 * as of a particular version.  This is a versioned list.  It contains information
 * for everything in the directory: the version of each file/sub-directory, their
 * names, permissions, and etc.  All of this information is serialized in a JSON
 * stream and that is stored in a Treenode-type Blob.  The JSON stream is stored
 * as the Blob Content.  The Blob HID (computed on the JSON stream) is called the
 * Treenode Hash Identifier (HID).  As with all Blobs, this HID uniquely identifies
 * the Treenode and is the only handle to lookup/access the Treenode content.
 *
 * A Treenode contains a list/array of Treenode-Entry where each Treenode-Entry
 * represents a single file/sub-directory/etc in the directory.
 *
 * Since the Treenode contains complete information for each directory-entry under
 * version control and the HID is computed over all of it, a COMMIT that makes a
 * change to any directory-entry (i.e., a change to a file, a rename to a
 * file/sub-directory, a mode-bit change, etc) will require a new Treenode HID to be
 * created.  This will cause a bubble-up effect as the Treenode HID is updated in
 * the Treenodes of containing directories.
 *
 * A COMMIT that makes a change to a directory-entry causes a new Treenode-Entry
 * to be created in a new version of the Treenode.  This new Treenode-Entry is
 * populated with fields defined by the current software reversion.  Treenode-Entries
 * for unchanged entries are carried forward as is from the previous Treenode.
 *
 * We have a Treenode-Version field on the entire Treenode and a separate
 * Treenode-Entry-Version field on each Treenode-Entry in the Treenode.
 *
 * The actual in-memory format of the Treenode is opaque, but there are routines to
 * access all the necessary fields.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_TREENODE_TYPEDEFS_H
#define H_SG_TREENODE_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * (TN) Version Identifier.
 *
 * Indicate what version of the software create the Treenode object.
 * This is important when we read an object from a blob.  For newly
 * created Treenode objects, we should always use the current software
 * version.
 */
/* TODO the members of this enum need the SG prefix */
enum SG_treenode_version
{
	TN_VERSION__INVALID=-1,					/**< Invalid Value, must be -1. */
	TN_VERSION_1=1,							/**< Version 1. */

	TN_VERSION__CURRENT=TN_VERSION_1,		/**< Current Software Version.  Must be latest version defined. */
	TN_VERSION__RANGE,						/**< Last defined value + 1. */
};

typedef enum SG_treenode_version SG_treenode_version;

/**
 * A Treenode describes a single versioned directory.
 *
 * A Treenode is a container for a series of Treenode-Entries.
 * It will contain one of them for each directory-entry (file/sub-directory/etc.)
 * that is present and under version control in the directory.
 *
 * Treenodes are serialized and stored in Treenode-type Blobs in the Repository.
 * They are named/addressed using the Treenode Hash Identifier (HID)
 * that is computed when the Treenode is added to the Repository.
 *
 * The Treenode does not contain the name or attributes of the
 * directory that it represents; these fields are stored in the
 * Treenode-Entry for this directory in the parent directory's
 * Treenode (just like in the filesystem).
 */
typedef struct SG_treenode SG_treenode;

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_TREENODE_TYPEDEFS_H
