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
 * @file sg_gid_typedefs.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_GID_TYPEDEFS_H
#define H_SG_GID_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * A GID is a Generated ID.  Similar to a traditional UUID or GUID,
 * it is a unique, generated, random value (and not a hash).
 *
 * A GID is used to UNIQUELY IDENTIFY an OBJECT over time.  We expect
 * this to be a permanent, long-term, global (in the "planetary" sense)
 * association.  For example, when a file is first added to a repository,
 * it is assigned a permanent GID which can be used to find the file no
 * matter how it is moved and/or renamed over time.
 *
 * A GID is VERY LONG (64 lowercase hex digit) string.  It is constructed
 * in such a way that we should never get a collision (both within a
 * computer over time and across all computers on the project).  To
 * distinguish a GID from other types of IDs, a GID starts with a 'g'.
 *
 * A GID is just a "const char *".  We DO NOT wrap this in a complicated
 * typedef because we use GIDs as keys in RBTREEs and VHASHes and in
 * callbacks and etc.  Tis better to just be honest here.
 *
 * SG_GID_ACTUAL_LENGTH is the length of the actual ID string -- NOT
 *                      COUNTING the trailing NULL.
 *
 * SG_GID_BUFFER_LENGTH is the length for a proper stack buffer AND
 *                      INCLUDES the trailing NULL.
 */
#define SG_GID_PREFIX_CHAR			('g')

#define SG_GID_LENGTH_PREFIX		(1)
#define SG_GID_LENGTH_RANDOM		(64)

#define SG_GID_ACTUAL_LENGTH		(SG_GID_LENGTH_PREFIX + SG_GID_LENGTH_RANDOM)
#define SG_GID_BUFFER_LENGTH		(SG_GID_LENGTH_PREFIX + SG_GID_LENGTH_RANDOM + 1)

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_GID_TYPEDEFS_H
