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
 * @file sg_dagquery_typedefs.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_DAGQUERY_TYPEDEFS_H
#define H_SG_DAGQUERY_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

enum _sg_dagquery_relationship
{
	SG_DAGQUERY_RELATIONSHIP__UNKNOWN			= 0,
	SG_DAGQUERY_RELATIONSHIP__SAME				= 1,	// same dagnode/changeset
	SG_DAGQUERY_RELATIONSHIP__PEER				= 2,	// siblings/cousins/etc
	SG_DAGQUERY_RELATIONSHIP__ANCESTOR			= 3,	// cs1 is an ancestor of cs2
	SG_DAGQUERY_RELATIONSHIP__DESCENDANT		= 4,	// cs1 is a descendant of cs2
	SG_DAGQUERY_RELATIONSHIP__UNRELATED			= 5,	// probably an error; cs1 and cs2 unrelated
};

typedef enum _sg_dagquery_relationship SG_dagquery_relationship;

//////////////////////////////////////////////////////////////////

enum _sg_dagquery_find_head_status
{
	SG_DAGQUERY_FIND_HEAD_STATUS__UNKNOWN		= 0,
	SG_DAGQUERY_FIND_HEAD_STATUS__IS_LEAF		= 1,	// the node we started searching from is actually a leaf/head
	SG_DAGQUERY_FIND_HEAD_STATUS__UNIQUE		= 2,	// the node has a single/unique head
	SG_DAGQUERY_FIND_HEAD_STATUS__MULTIPLE		= 3,	// the node has multiple leaves/heads
};

typedef enum _sg_dagquery_find_head_status SG_dagquery_find_head_status;

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_DAGQUERY_TYPEDEFS_H
