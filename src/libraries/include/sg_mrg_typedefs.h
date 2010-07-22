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
 * @file sg_mrg_typedefs.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG_TYPEDEFS_H
#define H_SG_MRG_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

typedef struct _sg_mrg				SG_mrg;

//////////////////////////////////////////////////////////////////

/**
 * Conflict status flags for an entry.  These reflect a "structural"
 * problem with the entry, such as being moved to 2 different directories
 * or being renamed to 2 different names by different branches or being
 * changed in one branch and deleted in another.
 *
 * When one of these bits is set, we *WILL* have a SG_mrg_cset_entry_conflict
 * structure associated with the entry which has more of the details.
 *
 * These types of conflicts are tree-wide -- and indicate that we had a
 * problem constructing the result-cset version of the tree and maybe had
 * to make some compromises/substitutions just to get a sane view of the
 * tree.  For example, for an entry that was moved to 2 different locations
 * by different branches, we may just leave it in the location it was in
 * in the ancestor version of the tree and complain.
 */

typedef SG_uint32 SG_mrg_cset_entry_conflict_flags;

#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__ZERO									((SG_mrg_cset_entry_conflict_flags)0x00000000)

#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_MOVE						((SG_mrg_cset_entry_conflict_flags)0x00000001)
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_RENAME						((SG_mrg_cset_entry_conflict_flags)0x00000002)
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_ATTRBITS					((SG_mrg_cset_entry_conflict_flags)0x00000004)
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_XATTR						((SG_mrg_cset_entry_conflict_flags)0x00000008)
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_SYMLINK_EDIT				((SG_mrg_cset_entry_conflict_flags)0x00000010)

#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_VS_FILE_EDIT					((SG_mrg_cset_entry_conflict_flags)0x00000100)
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DELETE_CAUSED_ORPHAN					((SG_mrg_cset_entry_conflict_flags)0x00000200)

#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__MOVES_CAUSED_PATH_CYCLE				((SG_mrg_cset_entry_conflict_flags)0x00001000)

#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_MOVE						((SG_mrg_cset_entry_conflict_flags)0x00010000)
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_RENAME						((SG_mrg_cset_entry_conflict_flags)0x00020000)
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_ATTRBITS					((SG_mrg_cset_entry_conflict_flags)0x00040000)
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_XATTR						((SG_mrg_cset_entry_conflict_flags)0x00080000)
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_SYMLINK_EDIT				((SG_mrg_cset_entry_conflict_flags)0x00100000)

#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__TBD				((SG_mrg_cset_entry_conflict_flags)0x01000000)	// need to run diff/merge tool to do auto-merge
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__NO_RULE			((SG_mrg_cset_entry_conflict_flags)0x02000000)	// no rule to handle file type (binary?); manual help required
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__AUTO_FAILED		((SG_mrg_cset_entry_conflict_flags)0x04000000)	// auto-merge failed; manual help required
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__AUTO_OK			((SG_mrg_cset_entry_conflict_flags)0x08000000)	// auto-merge successful -- no text conflicts

// if you add a bit to the flags, update the msg table in sg_mrg__private_do_simple.h

#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__UNDELETE__MASK						((SG_mrg_cset_entry_conflict_flags)0x00000fff)	// mask of _DELETE_vs_* and _DELETE_CAUSED_ORPHAN

#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK__NOT_OK		((SG_mrg_cset_entry_conflict_flags)0x07000000)	// mask of _FILE_EDIT_ minus _OK bits
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK__MANUAL		((SG_mrg_cset_entry_conflict_flags)0x06000000)	// mask of _FILE_EDIT_ bits (NO_RULE or AUTO_FAILED)
#define SG_MRG_CSET_ENTRY_CONFLICT_FLAGS__DIVERGENT_FILE_EDIT__MASK				((SG_mrg_cset_entry_conflict_flags)0x0f000000)	// mask of _FILE_EDIT_ bits

//////////////////////////////////////////////////////////////////

#define SG_MRG_TOKEN__NO_XATTR		"*no-xattr*"

//////////////////////////////////////////////////////////////////

enum _sg_mrg_automerge_result
{
	SG_MRG_AUTOMERGE_RESULT__NOT_ATTEMPTED		= 0,
	SG_MRG_AUTOMERGE_RESULT__SUCCESSFUL			= 1,
	SG_MRG_AUTOMERGE_RESULT__CONFLICT			= 2,

	SG_MRG_AUTOMERGE_RESULT__LIMIT				// must be last
};

typedef enum _sg_mrg_automerge_result SG_mrg_automerge_result;

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG_TYPEDEFS_H
