/**
 *
 * @file sg_mrg__private_typedefs.h
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#ifndef H_SG_MRG__PRIVATE_TYPEDEFS_H
#define H_SG_MRG__PRIVATE_TYPEDEFS_H

BEGIN_EXTERN_C;

//////////////////////////////////////////////////////////////////

#if defined(DEBUG)
#	define TRACE_MRG		0
#	define TRACE_MRG_DIRT	0
#else
#	define TRACE_MRG		0
#	define TRACE_MRG_DIRT	0
#endif

//////////////////////////////////////////////////////////////////

typedef struct _sg_mrg_cset_entry_collision		SG_mrg_cset_entry_collision;
typedef struct _sg_mrg_cset_entry_conflict		SG_mrg_cset_entry_conflict;
typedef struct _sg_mrg_cset_stats				SG_mrg_cset_stats;
typedef struct _sg_mrg_cset_entry				SG_mrg_cset_entry;
typedef struct _sg_mrg_cset						SG_mrg_cset;
typedef struct _sg_mrg_cset_dir					SG_mrg_cset_dir;

typedef struct _sg_mrg_preview_data				SG_mrg_preview_data;

/**
 * We use the NEQ when we compare 2 entries for equality. It tells us what is different.
 */
typedef SG_int8 SG_mrg_cset_entry_neq;
#define SG_MRG_CSET_ENTRY_NEQ__ALL_EQUAL		((SG_mrg_cset_entry_neq)0x00)

#define SG_MRG_CSET_ENTRY_NEQ__ATTRBITS			((SG_mrg_cset_entry_neq)0x01)
#define SG_MRG_CSET_ENTRY_NEQ__ENTRYNAME		((SG_mrg_cset_entry_neq)0x02)
#define SG_MRG_CSET_ENTRY_NEQ__GID_PARENT		((SG_mrg_cset_entry_neq)0x04)
#define SG_MRG_CSET_ENTRY_NEQ__HID_XATTR		((SG_mrg_cset_entry_neq)0x08)

#define SG_MRG_CSET_ENTRY_NEQ__DELETED			((SG_mrg_cset_entry_neq)0x10)	// this one doesn't mix well with the others...
#define SG_MRG_CSET_ENTRY_NEQ__SYMLINK_HID_BLOB	((SG_mrg_cset_entry_neq)0x20)
#define SG_MRG_CSET_ENTRY_NEQ__FILE_HID_BLOB	((SG_mrg_cset_entry_neq)0x40)

/**
 * Statistics on a merge.  We compute these as we build a candidate result-cset
 * when computing what the merge should be.  This might be for the full merge
 * or a sub-merge.
 *
 * TODO 2010/05/10 Does anyone use these?  Can I delete them?  These have the
 * TODO            problem that they are deltas between the ancestor and the
 * TODO            result.  And I now think that we'll want vv to show the
 * TODO            deltas relative to the baseline.
 * TODO
 * TODO            I might make this debug only.
 */
struct _sg_mrg_cset_stats
{
	SG_uint32						nrUnchanged;	// number of entries that were unchanged between the Ancestor and the result-cset.
	SG_uint32						nrChanged;		// number of entries that were changed in some way (and without conflicts).
	SG_uint32						nrDeleted;		// number of entries that were omitted from the result-cset.  TODO should match length of delete-list in cset.
	SG_uint32						nrAdded;		// number of new entries in result-cset that were not present in Ancestor.
	SG_uint32						nrConflicts;	// TODO should match length of conflict-list in cset.

	SG_uint32						nrAutoMerge_TBD;	// a subset of nrConflicts
	SG_uint32						nrAutoMerge_NoRule;	// a subset of nrConflicts
	SG_uint32						nrAutoMerge_Fail;	// a subset of nrConflicts
	SG_uint32						nrAutoMerge_OK;		// a subset of nrConflicts
};

//////////////////////////////////////////////////////////////////

/**
 * This structure is used to hold info for 1 entry (a file/symlink/directory);
 * this is basically a SG_treenode_entry converted into fields, but we also
 * know the GID of the parent directory.
 *
 * If we have a conflict for this entry, we also have a link to the conflict structure.
 */
struct _sg_mrg_cset_entry
{
	SG_mrg_cset *					pMrgCSet;				// the cset that we are a member of.  we do not own this.
	SG_mrg_cset_entry_conflict *	pMrgCSetEntryConflict;	// only set when we have a conflict.  we do not own this.
	SG_mrg_cset_entry_collision *	pMrgCSetEntryCollision;	// only set when we have a collision of some kind.  we do not own this.
	SG_string *						pStringEntryname;			// we own this
	SG_mrg_cset *					pMrgCSet_FileHidBlobInheritedFrom;	// only set on _clone_.  we do not own this.
	SG_mrg_cset *					pMrgCSet_CloneOrigin;	// we do not own this.
	SG_varray *						pvaPortLog;				// private per-entry portability log.  only used in final-result.  we own this.

	SG_int64						markerValue;			// data for caller to mark/unmark an entry
	SG_int64						attrBits;
	SG_portability_flags			portFlagsLogged;
	SG_treenode_entry_type			tneType;
	SG_bool							bMadeUniqueEntryname;	// true when we modified the entryname to resolve a collision/collision or other issue

	char							bufGid_Parent[SG_GID_BUFFER_LENGTH];	// GID of the containing directory.
	char							bufGid_Entry[SG_GID_BUFFER_LENGTH];		// GID of this entry.

	char							bufHid_Blob[SG_HID_MAX_BUFFER_LENGTH];		// not set for directories
	char							bufHid_XAttr[SG_HID_MAX_BUFFER_LENGTH];		// this will be blank when entry doesn't have XATTRs
};

//////////////////////////////////////////////////////////////////

/**
 * This structure is used to gather all of the entries present in a directory.
 *
 * We only use this when we need to see the version control tree in a hierarchial
 * manner -- as opposed to the flat view that we use most of the time.
 *
 * Our prbEntry_Children contains a list of all of the entries that are present
 * in the directory;  we borrow these pointers from our SG_mrg_cset->prbEntries.
 *
 * We hold a reference to the cset that we are a member of.
 */
struct _sg_mrg_cset_dir
{
	SG_mrg_cset *					pMrgCSet;						// the cset that we are a member of.                           we do not own this.
	SG_mrg_cset_entry *				pMrgCSetEntry_Self;				// back-link to our cset entry.    we do not own this.
	SG_rbtree *						prbEntry_Children;				// map[<gid-entry> --> SG_mrg_cset_entry *]            we DO NOT own these; we borrow from the SG_mrg_cset.prbEntries
	SG_portability_dir *			pPortabilityDir;				// we own this.
	SG_rbtree *						prbCollisions;					// map[<entryname> --> SG_mrg_cset_entry_collision *]  we own this and the values within it.

	char							bufGid_Entry_Self[SG_GID_BUFFER_LENGTH];	// GID of this entry.
};

//////////////////////////////////////////////////////////////////

enum _sg_mrg_cset_origin
{
	SG_MRG_CSET_ORIGIN__ZERO		= 0,	// unset
	SG_MRG_CSET_ORIGIN__LOADED		= 1,	// a real CSET that was loaded from the REPO.
	SG_MRG_CSET_ORIGIN__VIRTUAL		= 2,	// a synthetic CSET that we build during the merge computation.
};

typedef enum _sg_mrg_cset_origin	SG_mrg_cset_origin;

/**
 * This structure is used to hold everything that we know about a single CHANGE-SET.
 *
 * We start with a FLAT LIST of all of the entries in this version of the entire
 * version control tree.  Each of the SG_mrg_cset_entry's are stored in the .prbEntries.
 *
 * Later, we *MAY* build a hierarchial view of the tree using SG_mrg_cset_dir's
 * and .prgDirs.
 *
 * We do remember some fields to help name/find the root node to help with some things.
 *
 * We keep a copy of the change-set's HID (which matches the key in SG_mrg->prbCSets
 * used to find us).  This may be helpful later.
 */
struct _sg_mrg_cset
{
	SG_string *						pStringName;			// user-friendly name for this CSET (useful as interim name for candidate merge results)

	SG_rbtree *						prbEntries;				// map[<gid-entry> --> SG_mrg_cset_entry *]   we own these
	SG_mrg_cset_entry *				pMrgCSetEntry_Root;		// we do not own this. (borrowed from prbEntries assoc-value)

	SG_rbtree *						prbDirs;				// map[<gid-entry> --> SG_mrg_cset_dir *]     we own these -- only defined when needed
	SG_mrg_cset_dir *				pMrgCSetDir_Root;		// we do not own this.  root of hierarchial view -- only defined when actually needed

	SG_rbtree *						prbConflicts;			// map[<gid-entry> --> SG_mrg_cset_entry_conflict *]	we own these -- only defined when needed

	SG_rbtree *						prbDeletes;				// map[<gid-entry> --> SG_mrg_cset_entry * ancestor]	we do not own these -- only defined when needed

	char							bufGid_Root[SG_GID_BUFFER_LENGTH];				// GID of the actual-root directory.
	char							bufHid_CSet[SG_HID_MAX_BUFFER_LENGTH];			// HID of this CSET.  (will be empty for virtual CSETs)

	SG_mrg_cset_origin				origin;
	SG_daglca_node_type				nodeType;				// LEAF or SPCA or LCA -- only valid when (origin == SG_MRG_CSET_ORIGIN__LOADED)
};

//////////////////////////////////////////////////////////////////

struct _sg_mrg_automerge_plan_item
{
	SG_mrg *					pMrg;						// we do not own this
	SG_mrg_cset_entry *			pMrgCSetEntry_Ancestor;		// we do not own this

	SG_pathname *				pPath_Ancestor;		// we own this
	SG_pathname *				pPath_Mine;			// we own this    the mine/yours labels are a bit arbitrary
	SG_pathname *				pPath_Yours;		// we own this
	SG_pathname *				pPath_Result;		// we own this

	SG_mrg_automerge_result		result;

	char						bufHidBlob_Mine[SG_HID_MAX_BUFFER_LENGTH];		// these are only set when we get the contents from repo
	char						bufHidBlob_Yours[SG_HID_MAX_BUFFER_LENGTH];		// (they are not set when we get contents from sub-merge)
};

typedef struct _sg_mrg_automerge_plan_item SG_mrg_automerge_plan_item;

//////////////////////////////////////////////////////////////////

/**
 * This structure is used to represent what we know about a conflict
 * on a single entry.  We know the SG_mrg_cset_entry for the entry
 * as it appeared in the ancestor.  And we know the corresponding
 * entries in 2 or more branches where changes were made.  The
 * vectors only contain the (entry, neq-flags) of the branches
 * where there was a change.
 *
 * We use this structure to bind the ancestor entry with the
 * correpsonding entry in the branches so that a caller (such as
 * the command line client or gui) can decide how to present the
 * info and let the user choose how to resolve it.  (handwave)
 *
 * Note that we may have a combination of issues.
 * For example, we may have 2 branches that renamed an entry to
 * different values.  Or that renamed it to the same new name.
 * The former requires help.  The latter we can just eat and treat
 * like a simple rename in only 1 branch (and maybe even avoid
 * generating this conflict record).  But, we might have a combination
 * of issues, for example a double rename and a double move.  If one
 * is a duplicate and one is divergent, then we need to be able to
 * present the whole thing and let the resolve the divergent part
 * and still handle the duplicate part (with or without showing
 * them it).
 *
 * Granted, moves, renames, xattr, and attrbit changes are rare.
 * This most likely to be seen when 2 or more branches modify
 * the contents of a file.  In this case, we just know that the
 * contents changes -- we need to run something like gnu-diff3 or
 * DiffMerge to tell us if the files can be auto-merged or if we
 * need help.  (And cache the auto-merge result if successful.)
 * We use this structure to also hold the content-merge state
 * so that when we actually populate the WD, we know what to do
 * with the content.
 *
 * We accumulate these (potential and actual) conflicts in the
 * SG_mrg_cset in a rbtree.
 *
 * Note that when a branch/leaf deletes an entry, we don't have
 * an entry to put in the pVec_MrgCSetEntries vector, so we have
 * a second vector listing the branches/leaves where it was deleted.
 */

struct _sg_mrg_cset_entry_conflict
{
	SG_mrg_cset *						pMrgCSet;						// the virtual cset that we are building as we compute the merge.  we do not own this
	SG_mrg_cset_entry *					pMrgCSetEntry_Ancestor;			// the ancestor entry (owned by the ancestor cset).  we do not own this
	SG_mrg_cset_entry *					pMrgCSetEntry_Baseline;			// the bseline entry (when available).  we do not own this
	SG_vector *							pVec_MrgCSetEntry_Changes;		// vec[SG_mrg_cset_entry *]		we own the vector, but not the pointers within (they belong to their respective csets).
	SG_vector_i64 *						pVec_MrgCSetEntryNeq_Changes;	// vec[SG_mrg_cset_entry_neq]	we own the vector.
	SG_vector *							pVec_MrgCSet_Deletes;			// vec[SG_mrg_cset *]			we own the vector, but not the pointers within.
	SG_mrg_cset_entry *					pMrgCSetEntry_Composite;		// the merged composite entry.  we DO NOT own this.

	SG_rbtree *							prbUnique_AttrBits;				// map[#attrbits --> vec[SG_mrg_cset_entry *]]  we own vector, but not the pointers within.
	SG_rbtree *							prbUnique_Entryname;			// map[entryname --> vec[SG_mrg_cset_entry *]]  we own vector, but not the pointers within.
	SG_rbtree *							prbUnique_GidParent;			// map[gid       --> vec[SG_mrg_cset_entry *]]  we own vector, but not the pointers within.
	SG_rbtree *							prbUnique_HidXAttr;				// map[hid       --> vec[SG_mrg_cset_entry *]]  we own vector, but not the pointers within.
	SG_rbtree *							prbUnique_Symlink_HidBlob;		// map[hid       --> vec[SG_mrg_cset_entry *]]  we own vector, but not the pointers within.

	SG_rbtree *							prbUnique_File_HidBlob_or_AutoMergePlanResult;	// map[hid-blob_or_path --> {null, path}]		we do not own these

	SG_mrg_cset_entry_conflict_flags	flags;

	SG_vector *							pVec_AutoMergePlan;			// vec[SG_mrg_automerge_plan_item *]  we own these
	SG_pathname *						pPathAutoMergePlanResult;	// the merge-result temp-file (final part in plan).  we own this.
};

//////////////////////////////////////////////////////////////////

/**
 * Collision status for entries within a directory.  These reflect
 * real collisions as reported by SG_portability.
 *
 * Since SG_mrg_entry_conflict is used to bind multiple versions of the
 * same entry together and a collision indicates that different entries
 * map to the same entryname, collision problems DO NOT cause a
 * SG_mrg_cset_entry_conflict to be generated, rather they get a
 * SG_mrg_cset_entry_collision.
 *
 * Ideally, all colliding entries are the same.  We just accumulate them
 * in the order that we found them (probably GID-ENTRY order, but that
 * doesn't matter).
 */
struct _sg_mrg_cset_entry_collision
{
	SG_vector *		pVec_MrgCSetEntry;		// vec[SG_mrg_cset_entry *] the entries in the collision.  we own the vector, but do not own the values.
};

//////////////////////////////////////////////////////////////////

/**
 * The main structure for driving a version control tree merge.
 *
 * We give it a PENDING-TREE so that we can find the associated WD,
 * REPO, and BASELINE.
 */
struct _sg_mrg
{
	//////////////////////////////////////////////////////////////////
	// fields to let us find and use the Working-Directory and get the associated REPO.
	//////////////////////////////////////////////////////////////////

	SG_string *				pStrRepoDescriptorName;
	char *					pszGid_anchor_directory;		// TODO what is this and what is it used for?
	SG_vhash *				pvhRepoDescriptor;
	SG_pendingtree *		pPendingTree;					// we do not own this
	SG_repo *				pRepo;							// we do not own this
	SG_uint32				nrHexDigitsInHIDs;				// number of hex digits in a HID for HIDs in this repo.

	//////////////////////////////////////////////////////////////////
	// Remember any initial baseline-vs-wd dirt that may have to be dealt with.

	SG_treediff2 *			pTreeDiff_Baseline_WD_Dirt;

	//////////////////////////////////////////////////////////////////
	// fields to let us remember the set of input Leaves (L0, L1, ...)
	// and the DAG-LCA that we compute.  and a map of all of the CSets
	// that we have loaded (both for leaves and the various ancestors).
	// We also remember which leaf is is the baseline.

	SG_rbtree *				prbCSets_OtherLeaves;			// map[<hid-cset>]  simple list of just the other leaves/branches in the merge (not includeing the baseline)
	SG_daglca *				pDagLca;
	SG_rbtree *				prbCSets;						// map[<hid-cset> --> SG_mrg_cset *]          we own these.  This list includes everything loaded (the Leaves, SPCAs, and LCA).
	SG_mrg_cset *			pMrgCSet_LCA;					// link to LCA                                we do not own this
	SG_mrg_cset *			pMrgCSet_Baseline;				// link to Baseline (when member of merge)    we do not own this (borrowed from prbCSets)

	char					bufHid_CSet_Baseline[SG_HID_MAX_BUFFER_LENGTH];

	//////////////////////////////////////////////////////////////////
	// We also remember the CSet of final merge.  This is an IN-MEMORY-ONLY
	// representation of the results of the merge and reflects what the
	// WD *WILL EVENTUALLY* look like *IF* the merge is actually applied.
	// We can use this to dump a preview and/or to do the APPLY.

	SG_mrg_cset *			pMrgCSet_FinalResult;
	SG_mrg_cset_stats *		pMrgCSetStats_FinalResult;		// TODO think about getting rid of this.

	//////////////////////////////////////////////////////////////////
	//

	SG_pathname *			pPathOurTempDir;				// pathname to our scratch directory (which may or may not exist on disk yet).  we own this

	SG_inv_dirs *			pInvDirs;						// "inverted tree" issues between baseline and final-result.

	SG_wd_plan *			p_wd_plan;
	SG_varray *				pvaIssues;
	SG_bool					bIssuesSavedInPendingTree;

	char					bufGid_Temp[SG_GID_BUFFER_LENGTH];				// only needed when MERGE needs to TEMPORARILY
	char					bufGid_Temp_ParkingLot[SG_GID_BUFFER_LENGTH];	// add .sgtemp and parkinglot to the pendingtree.
};

//////////////////////////////////////////////////////////////////

struct _sg_mrg_preview_data
{
	SG_mrg * pMrg;
	SG_string * pStrWork;
	SG_diff_ext__compare_callback * pfnCompare;
	SG_file * pFileStdout;
	SG_file * pFileStderr;
};

//////////////////////////////////////////////////////////////////

/**
 * A routine to try to do an auto-merge on the various versions of a file.
 */
typedef void (SG_mrg_external_automerge_handler)(SG_context * pCtx, SG_mrg_automerge_plan_item * pItem);

//////////////////////////////////////////////////////////////////

END_EXTERN_C;

#endif//H_SG_MRG__PRIVATE_TYPEDEFS_H
