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
 * @file sg_treediff2_typedefs.h
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_TREEDIFF2_TYPEDEFS_H
#define H_SG_TREEDIFF2_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

/**
 * These are the types of changes that can be made to a file/folder
 * between 2 versions.  Some combinations are not possible (like
 * adding and deleting), but others are (like modified and renamed).
 *
 * We use this field to capture everything that changed in one place.
 *
 * Note: The __LOST and __FOUND bits only apply to things
 * lost/found in a baseline vs working-directory comparison; they
 * cannot exist in a changeset vs changeset comparison.
 */
typedef SG_uint32 SG_diffstatus_flags;

#define SG_DIFFSTATUS_FLAGS__ZERO						(0x00000000)

#define SG_DIFFSTATUS_FLAGS__ADDED						(0x00000001)	// [__ADDED .. __FOUND] are mutually exclusive
#define SG_DIFFSTATUS_FLAGS__DELETED					(0x00000002)
#define SG_DIFFSTATUS_FLAGS__MODIFIED					(0x00000004)
#define SG_DIFFSTATUS_FLAGS__LOST						(0x00000008)
#define SG_DIFFSTATUS_FLAGS__FOUND						(0x00000010)
#define SG_DIFFSTATUS_FLAGS__RENAMED					(0x00000020)
#define SG_DIFFSTATUS_FLAGS__MOVED						(0x00000040)
#define SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS				(0x00000080)
#define SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS			(0x00000100)

#if 1 // TODO I'm not sure I really want to do this.
#define SG_DIFFSTATUS_FLAGS__UNDONE_ADDED				(0x00010000)	// the composition of an __ADDED and a __DELETED produces a ghost
#define SG_DIFFSTATUS_FLAGS__UNDONE_DELETED				(0x00020000)
#define SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED			(0x00040000)
#define SG_DIFFSTATUS_FLAGS__UNDONE_LOST				(0x00080000)
#define SG_DIFFSTATUS_FLAGS__UNDONE_FOUND				(0x00100000)
#define SG_DIFFSTATUS_FLAGS__UNDONE_RENAMED				(0x00200000)
#define SG_DIFFSTATUS_FLAGS__UNDONE_MOVED				(0x00400000)
#define SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_XATTRS		(0x00800000)
#define SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_ATTRBITS	(0x01000000)
#endif

/**
 * By construction, only one of these is possible at a time.
 *
 * (Even when computing the compose of 2 statuses.  If the
 * compose needs to set 2 of these, it will re-express the
 * entry.  For example an Add + XAttr is just an Add with a
 * different value for the XAttr.)
 */
#define SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE	(  SG_DIFFSTATUS_FLAGS__ADDED		\
														 | SG_DIFFSTATUS_FLAGS__DELETED		\
														 | SG_DIFFSTATUS_FLAGS__MODIFIED	\
														 | SG_DIFFSTATUS_FLAGS__LOST		\
														 | SG_DIFFSTATUS_FLAGS__FOUND		)

/**
 * One or more of these may be present at any given time.
 */
#define SG_DIFFSTATUS_FLAGS__MASK_MODIFIERS				(  SG_DIFFSTATUS_FLAGS__RENAMED				\
														 | SG_DIFFSTATUS_FLAGS__MOVED				\
														 | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS		\
														 | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS	)

#if 1 // TODO do we really want to do this.
/**
 * The UNDONE bits.
 */
#define SG_DIFFSTATUS_FLAGS__MASK_UNDONE				(  SG_DIFFSTATUS_FLAGS__UNDONE_ADDED			\
														 | SG_DIFFSTATUS_FLAGS__UNDONE_DELETED			\
														 | SG_DIFFSTATUS_FLAGS__UNDONE_MODIFIED			\
														 | SG_DIFFSTATUS_FLAGS__UNDONE_LOST				\
														 | SG_DIFFSTATUS_FLAGS__UNDONE_FOUND			\
														 | SG_DIFFSTATUS_FLAGS__UNDONE_RENAMED 			\
														 | SG_DIFFSTATUS_FLAGS__UNDONE_MOVED			\
														 | SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_XATTRS	\
														 | SG_DIFFSTATUS_FLAGS__UNDONE_CHANGED_ATTRBITS	)
#endif

//////////////////////////////////////////////////////////////////

/**
 * A little thing to remember what kind of treediff2 was computed.
 */
enum _sg_treediff2_kind
{
	SG_TREEDIFF2_KIND__ZERO					= 0,
	SG_TREEDIFF2_KIND__CSET_VS_CSET			= 1,	// historical, from treediff
	SG_TREEDIFF2_KIND__BASELINE_VS_WD		= 2,	// live, from pendingtree
	SG_TREEDIFF2_KIND__CSET_VS_WD			= 3,	// composite, arbitrary cset vs working-directory
};

typedef enum _sg_treediff2_kind SG_treediff2_kind;

//////////////////////////////////////////////////////////////////

typedef struct _SG_treediff2 SG_treediff2;

//////////////////////////////////////////////////////////////////

// TODO decide if we want to actually publish the ObjectData format
// TODO and/or make the OD routines public.
typedef struct _SG_treediff2_ObjectData SG_treediff2_ObjectData;

typedef void (SG_treediff2_foreach_callback)(SG_context * pCtx,
											 SG_treediff2 * pTreeDiff,
											 const char * szGidObject,
											 const SG_treediff2_ObjectData * pOD,
											 void * pVoidData);

typedef struct _sg_treediff2_iterator SG_treediff2_iterator;

//////////////////////////////////////////////////////////////////

typedef struct _SG_treediff2_debug__Stats
{
	SG_uint32 nrTotalChanges;
	SG_uint32 nrSectionsWithChanges;
	SG_uint32 nrAdded;
	SG_uint32 nrDeleted;
	SG_uint32 nrDirectoryModified;		// these are both part
	SG_uint32 nrFileSymlinkModified;	// of the same section.
	SG_uint32 nrLost;
	SG_uint32 nrFound;
	SG_uint32 nrRenamed;
	SG_uint32 nrMoved;
	SG_uint32 nrChangedXAttrs;
	SG_uint32 nrChangedAttrBits;
} SG_treediff2_debug__Stats;

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_TREEDIFF2_TYPEDEFS_H
