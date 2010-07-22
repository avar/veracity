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
 * @file sg_inv_entry_typedefs.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_INV_ENTRY_TYPEDEFS_H
#define H_SG_INV_ENTRY_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

typedef struct _inv_entry SG_inv_entry;

typedef SG_uint32 SG_inv_entry_flags;
#define SG_INV_ENTRY_FLAGS__ZERO				((SG_inv_entry_flags)0x0000)
#define SG_INV_ENTRY_FLAGS__SCANNED				((SG_inv_entry_flags)0x0001)	// SG_dir__read() found it
#define SG_INV_ENTRY_FLAGS__ACTIVE_SOURCE		((SG_inv_entry_flags)0x0010)
#define SG_INV_ENTRY_FLAGS__NON_ACTIVE_SOURCE	((SG_inv_entry_flags)0x0020)
#define SG_INV_ENTRY_FLAGS__ACTIVE_TARGET		((SG_inv_entry_flags)0x0100)
#define SG_INV_ENTRY_FLAGS__NON_ACTIVE_TARGET	((SG_inv_entry_flags)0x0200)
#define SG_INV_ENTRY_FLAGS__PARKED_FOR_SWAP		((SG_inv_entry_flags)0x1000)	// parked/swapped because of entryname used by 2 entries; need swap file
#define SG_INV_ENTRY_FLAGS__PARKED_FOR_CYCLE	((SG_inv_entry_flags)0x2000)	// parked because directory moves caused cycle in ancestor
#define SG_INV_ENTRY_FLAGS__PARKED__MASK		((SG_inv_entry_flags)0xf000)

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_INV_ENTRY_TYPEDEFS_H
