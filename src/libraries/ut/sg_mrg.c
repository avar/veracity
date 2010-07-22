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
 * @file sg_mrg.c
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "sg_mrg__private_typedefs.h"
#include "sg_mrg__private_prototypes.h"

#include "sg_mrg__private_automerge_plan_item.h"
#include "sg_mrg__private_build_wd_issues.h"
#include "sg_mrg__private_cset.h"
#include "sg_mrg__private_cset_entry.h"
#include "sg_mrg__private_cset_dir.h"
#include "sg_mrg__private_cset_stats.h"
#include "sg_mrg__private_cset_entry_collision.h"
#include "sg_mrg__private_cset_entry_conflict.h"
#include "sg_mrg__private_dirt.h"
#include "sg_mrg__private_do_simple.h"
#include "sg_mrg__private_file_mrg.h"
#include "sg_mrg__private_file_mrg__builtin_diff3.h"
#include "sg_mrg__private_master_plan__typedefs.h"
#include "sg_mrg__private_master_plan__utils.h"
#include "sg_mrg__private_master_plan__park.h"
#include "sg_mrg__private_master_plan__add.h"
#include "sg_mrg__private_master_plan__overrule.h"
#include "sg_mrg__private_master_plan__peer.h"
#include "sg_mrg__private_master_plan__entry.h"
#include "sg_mrg__private_master_plan__delete.h"
#include "sg_mrg__private_master_plan.h"
#include "sg_mrg__private_mrg.h"
#include "sg_mrg__private_temp_dir.h"

//////////////////////////////////////////////////////////////////
