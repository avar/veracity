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
 * @file sg_dagnode_typedefs.h
 *
 * @details Prototypes for DAGNODES.
 *
 * A DAGNODE (in the DAG) contains corresponds to a CHANGESET.
 *
 * A DAGNODE only contains information about the EDGES from a
 * child CHANGESET to all of its parent CHANGESETS.
 *
 * A DAGNODE is stored in the REPO independent of the contents
 * of the actual CHANGESET.  This allows various levels of
 * sparseness within the REPO.
 *
 * The actual in-memory format of a DAGNODE is opaque and may
 * or may not resemble how it is stored on-disk in the REPO.
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_DAGNODE_TYPEDEFS_H
#define H_SG_DAGNODE_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * A DAGNODE is a representation of the DAG EDGE relationships of a CHANGESET.
 */
typedef struct SG_dagnode SG_dagnode;

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_DAGNODE_TYPEDEFS_H
