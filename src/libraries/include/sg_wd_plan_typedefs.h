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
 * @file sg_wd_plan_typedefs.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_WD_PLAN_TYPEDEFS_H
#define H_SG_WD_PLAN_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

typedef struct sg_wd_plan SG_wd_plan;

struct sg_wd_plan_stats
{
	SG_uint32 nrDirsAdded;
	SG_uint32 nrFilesAdded;			// files + symlinks

	SG_uint32 nrDirsChanged;
	SG_uint32 nrFilesChanged;		// files + symlinks

	SG_uint32 nrDirsDeleted;
	SG_uint32 nrFilesDeleted;		// files + symlinks

	SG_uint32 nrFilesAutoMerged;	// files only and only successful auto-merges
};

typedef struct sg_wd_plan_stats SG_wd_plan_stats;

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_WD_PLAN_TYPEDEFS_H
