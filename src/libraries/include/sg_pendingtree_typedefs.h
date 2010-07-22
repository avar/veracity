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
 * @file sg_pendingtree_typedefs.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_PENDINGTREE_TYPEDEFS_H
#define H_SG_PENDINGTREE_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

typedef struct _SG_pendingtree SG_pendingtree;

//////////////////////////////////////////////////////////////////

/**
 * ActionLog:
 *
 * Some commands take a "--test" argument to indicate that we should just
 * report what SHOULD be done, but NOT actually do it.  (I'm thinking that
 * they should also take a "--verbose" that would return the report but also
 * actually do the work.)  The default is to just do it quietly.
 *
 * When logging, we accumulate the individual modifications that we make
 * (should make) to the Working Directory in a VARRAY containing a series
 * of VHASHes describing the steps taken.
 */
enum _sg_pendingtree_action_log_enum { SG_PT_ACTION__ZERO             = 0x0,
									   SG_PT_ACTION__DO_IT            = 0x1,
									   SG_PT_ACTION__LOG_IT           = 0x2,
									   SG_PT_ACTION__DO_IT_AND_LOG_IT = 0x3
};

typedef enum _sg_pendingtree_action_log_enum SG_pendingtree_action_log_enum;

//////////////////////////////////////////////////////////////////

typedef SG_uint32 SG_pendingtree_wd_issue_status;

#define SG_ISSUE_STATUS__ZERO				((SG_pendingtree_wd_issue_status)0x0000)
#define SG_ISSUE_STATUS__MARKED_RESOLVED	((SG_pendingtree_wd_issue_status)0x0001)

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_PENDINGTREE_TYPEDEFS_H
