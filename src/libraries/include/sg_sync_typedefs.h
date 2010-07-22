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
 * @file sg_sync_typedefs.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_SYNC_TYPEDEFS_H
#define H_SG_SYNC_TYPEDEFS_H

BEGIN_EXTERN_C;

#define SG_SYNC_STATUS_KEY__GENERATIONS	"gens"
#define SG_SYNC_STATUS_KEY__CLONE		"clone"
#define SG_SYNC_STATUS_KEY__DAGS		"dags"
#define SG_SYNC_STATUS_KEY__BLOBS		"blobs"
#define SG_SYNC_STATUS_KEY__LEAFD		"leafd"
#define SG_SYNC_STATUS_KEY__NEW_NODES	"new-nodes"

#define SG_SYNC_REQUEST_VALUE_TAG			"tag"
#define SG_SYNC_REQUEST_VALUE_HID_PREFIX	"prefix"

END_EXTERN_C;

#endif//H_SG_SYNC_TYPEDEFS_H

