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
 * @file u0064_mrg.c
 *
 * @details
 *
 */

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"
#include "unittests_pendingtree.h"

//////////////////////////////////////////////////////////////////

#if defined(SG_LONGTESTS)
#define DO_USE_UPDATE	1		// run all tests using UPDATE to switch between branches within 1 WD
#define DO_USE_NEW_WD	1		// run all tests by creating a parallel WD for each branch
#else
#define DO_USE_UPDATE	0		// run all tests using UPDATE to switch between branches within 1 WD
#define DO_USE_NEW_WD	1		// run all tests by creating a parallel WD for each branch
#endif

#define DO_000		1
#define DO_100		1
#define DO_200		1
#define DO_300		1
#define DO_400		1
#define DO_500		1
#define DO_600		1
#define DO_700		1

//////////////////////////////////////////////////////////////////
// only bother with xattr tests on systems where it is configured (see cmake file)

#ifdef SG_BUILD_FLAG_TEST_XATTR
#	define AA 1
#else
#	define AA 0
#endif

//////////////////////////////////////////////////////////////////
// attrbits work on unix-based systems

#if defined(MAC) || defined(LINUX)
#	define AB 1
#else
#	define AB 0
#endif

//////////////////////////////////////////////////////////////////
// only bother with symlink tests on systems that support them

#if defined(MAC) || defined(LINUX)
#	define SYM 1
#else
#	define SYM 0
#endif

//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0064_mrg)
#define MyDcl(name)				u0064_mrg__##name
#define MyFn(name)				u0064_mrg__##name

//////////////////////////////////////////////////////////////////

static void MyFn(switch_to_baseline)(SG_context * pCtx,
										 const SG_pathname * pPathWorkingDir,
										 const char * szHidCSet)
{
	SG_pendingtree * pPendingTree = NULL;
	SG_bool bIgnorePortabilityWarnings = SG_TRUE;
	SG_bool bForce = SG_FALSE;
	SG_pendingtree_action_log_enum e = SG_PT_ACTION__DO_IT;

	SG_ERR_CHECK(  SG_pendingtree__alloc(pCtx,pPathWorkingDir,bIgnorePortabilityWarnings,&pPendingTree)  );
	SG_ERR_CHECK(  SG_pendingtree__update_baseline(pCtx,pPendingTree,szHidCSet,bForce,e)  );

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

//////////////////////////////////////////////////////////////////

#if SYM
static void MyFn(create_symlink)(
    SG_context * pCtx,
	SG_pathname* pPath,
	const char* pszSymlink,
	const char* pszTarget)
{
	SG_pathname* pPathSymlink = NULL;
	SG_string * pStringTarget = NULL;

	VERIFY_ERR_CHECK(  SG_string__alloc__sz(pCtx,&pStringTarget,pszTarget)  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathSymlink, pPath, pszSymlink)  );
	VERIFY_ERR_CHECK(  SG_fsobj__symlink(pCtx, pStringTarget, pPathSymlink)  );

fail:
	SG_STRING_NULLFREE(pCtx, pStringTarget);
	SG_PATHNAME_NULLFREE(pCtx, pPathSymlink);
}

/**
 * replace the target in a symlink.
 *
 * since there is no system call for this, we delete it and re-create it.
 */
static void MyFn(replace_symlink)(
    SG_context * pCtx,
	SG_pathname* pPath,
	const char* pszSymlink,
	const char* pszTarget)
{
	SG_fsobj_type fsobjType;
	SG_bool bExists;
	SG_pathname* pPathSymlink = NULL;
	SG_string * pStringTarget = NULL;

	// verify that file1 is a symlink
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathSymlink, pPath, pszSymlink)  );
	VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pPathSymlink,&bExists,&fsobjType,NULL)  );
	VERIFY_COND("replace_symlink",(bExists));
	VERIFY_COND("replace_symlink",(fsobjType == SG_FSOBJ_TYPE__SYMLINK));

	// delete it and re-create the symlink with the new value.
	VERIFY_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx,pPathSymlink)  );

	VERIFY_ERR_CHECK(  SG_string__alloc__sz(pCtx,&pStringTarget,pszTarget)  );
	VERIFY_ERR_CHECK(  SG_fsobj__symlink(pCtx, pStringTarget, pPathSymlink)  );

fail:
	SG_STRING_NULLFREE(pCtx, pStringTarget);
	SG_PATHNAME_NULLFREE(pCtx, pPathSymlink);
}
#endif

//////////////////////////////////////////////////////////////////

static void MyFn(delete_file)(
    SG_context * pCtx,
	SG_pathname* pPath,
	const char* pszName
	)
{
	SG_pathname* pPathFile = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__remove__pathname(pCtx, pPathFile)  );

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

static void MyFn(append_to_file__numbers)(
	SG_context * pCtx,
	SG_pathname* pPath,
	const char* pszName,
	const SG_uint32 countLines
	)
{
	SG_pathname* pPathFile = NULL;
	SG_uint32 i;
	SG_file* pFile = NULL;
	SG_uint64 len = 0;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
	VERIFY_ERR_CHECK(  SG_fsobj__length__pathname(pCtx, pPathFile, &len, NULL)  );
	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathFile, SG_FILE_OPEN_EXISTING | SG_FILE_RDWR, 0600, &pFile)  );
	VERIFY_ERR_CHECK(  SG_file__seek(pCtx, pFile, len)  );
	for (i=0; i<countLines; i++)
	{
		char buf[64];

		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, sizeof(buf), "%d\n", i)  );
		VERIFY_ERR_CHECK(  SG_file__write(pCtx, pFile, (SG_uint32) strlen(buf), (SG_byte*) buf, NULL)  );
	}

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

/**
 * create a silly little file of line numbers.
 */
static void MyFn(create_file__numbers)(
    SG_context * pCtx,
	SG_pathname* pPath,
	const char* pszName,
	const SG_uint32 countLines
	)
{
	SG_pathname* pPathFile = NULL;
	SG_uint32 i;
	SG_file* pFile = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathFile, SG_FILE_CREATE_NEW | SG_FILE_RDWR, 0600, &pFile)  );
	for (i=0; i<countLines; i++)
	{
		char buf[64];

		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, sizeof(buf), "%d\n", i)  );
		VERIFY_ERR_CHECK(  SG_file__write(pCtx, pFile, (SG_uint32) strlen(buf), (SG_byte*) buf, NULL)  );
	}

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

/**
 * create a silly little file of line numbers with some prefix lines with something else.
 * (so that it will look like an insert when merging 2 files).
 */
static void MyFn(create_file__numbers2)(
    SG_context * pCtx,
	SG_pathname* pPath,
	const char* pszName,
	const SG_uint32 countLines,
	const SG_uint32 countPrefix
	)
{
	SG_pathname* pPathFile = NULL;
	SG_uint32 i;
	SG_file* pFile = NULL;

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathFile, pPath, pszName)  );
	VERIFY_ERR_CHECK(  SG_file__open__pathname(pCtx, pPathFile, SG_FILE_CREATE_NEW | SG_FILE_RDWR, 0600, &pFile)  );
	for (i=0; i<countPrefix; i++)
	{
		char buf[64];

		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf,sizeof(buf), "Insert %c\n", 'A' + (i%26))  );
		VERIFY_ERR_CHECK(  SG_file__write(pCtx, pFile, (SG_uint32)strlen(buf), (SG_byte *)buf, NULL)  );
	}

	for (i=0; i<countLines; i++)
	{
		char buf[64];

		VERIFY_ERR_CHECK(  SG_sprintf(pCtx, buf, sizeof(buf), "%d\n", i)  );
		VERIFY_ERR_CHECK(  SG_file__write(pCtx, pFile, (SG_uint32) strlen(buf), (SG_byte*) buf, NULL)  );
	}

fail:
	SG_FILE_NULLCLOSE(pCtx, pFile);
	SG_PATHNAME_NULLFREE(pCtx, pPathFile);
}

//////////////////////////////////////////////////////////////////

/**
 * like a "vv dump_treenodes <cset>"
 */
static void MyFn(dump_treenodes_in_changeset)(SG_context * pCtx, const char * pszLabel, SG_repo * pRepo, const char * pszHidCSet)
{
	SG_changeset * pcs = NULL;
	SG_string * pStr = NULL;
	const char * pszHidSuperRoot;

	SG_ERR_CHECK(  SG_changeset__load_from_repo(pCtx,pRepo,pszHidCSet,&pcs)  );
	SG_ERR_CHECK(  SG_changeset__get_root(pCtx,pcs,&pszHidSuperRoot)  );

	SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx,&pStr)  );
#ifdef DEBUG
	SG_treenode_debug__dump__by_hid(pCtx,pszHidSuperRoot,pRepo,4,SG_TRUE,pStr);
#endif
	INFOP("Treenodes_in_Changeset",("[Label %s] : [CSet %s]\n%s\n",pszLabel,pszHidCSet,SG_string__sz(pStr)));

	// fall thru

fail:
	SG_CHANGESET_NULLFREE(pCtx, pcs);
	SG_STRING_NULLFREE(pCtx, pStr);
}

//////////////////////////////////////////////////////////////////

static void MyFn(c2tfe_helper)(SG_context * pCtx,
								   const SG_treediff2_ObjectData * pOD_opaque,
								   const char ** pszRepoPath,
								   const char ** pszEntryName,
								   const char ** pszHidContent,
								   const char ** pszHidXAttrs,
								   const char ** pszGidParent,
								   SG_int64 * pAttrbits,
								   SG_diffstatus_flags * pdsFlags,
								   SG_treenode_entry_type * ptneType)
{
	VERIFY_ERR_CHECK_DISCARD(  SG_treediff2__ObjectData__get_repo_path(pCtx, pOD_opaque,			pszRepoPath)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_treediff2__ObjectData__get_name(pCtx, pOD_opaque,				pszEntryName)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_treediff2__ObjectData__get_content_hid(pCtx, pOD_opaque,		pszHidContent)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_treediff2__ObjectData__get_xattrs(pCtx, pOD_opaque,			pszHidXAttrs)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_treediff2__ObjectData__get_parent_gid(pCtx, pOD_opaque,		pszGidParent)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_treediff2__ObjectData__get_attrbits(pCtx, pOD_opaque,			pAttrbits)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD_opaque,			pdsFlags)  );
	VERIFY_ERR_CHECK_DISCARD(  SG_treediff2__ObjectData__get_tneType(pCtx, pOD_opaque,			ptneType)  );

	// directories get the _MODIFIED bit set when anything *within* the directory changes.  this
	// helps the SG_treediff2 code build the list of changes (because parents entries will always
	// be present).  but this gives us some false positives in our report here.  so we pretend
	// that they aren't present (unless there were changes *on* the directory).

	if ((*ptneType == SG_TREENODEENTRY_TYPE_DIRECTORY) && (*pdsFlags & SG_DIFFSTATUS_FLAGS__MODIFIED))
		*pdsFlags &= ~SG_DIFFSTATUS_FLAGS__MODIFIED;

	return;
}

static void MyFn(compare_2_treediffs_for_equivalence)(SG_context * pCtx,
													  SG_treediff2 * pTreeDiff_a,
													  SG_treediff2 * pTreeDiff_b,
													  SG_bool bIncludeUndoneEntries)
{
	SG_treediff2_iterator * pIter_a = NULL;		SG_treediff2_iterator * pIter_b = NULL;
	const char * szGidObject_a;					const char * szGidObject_b;
	SG_treediff2_ObjectData * pOD_opaque_a;		SG_treediff2_ObjectData * pOD_opaque_b;
	const char * szRepoPath_a = NULL;			const char * szRepoPath_b = NULL;
	const char * szEntryName_a = NULL;			const char * szEntryName_b = NULL;
	const char * szHidContent_a = NULL;			const char * szHidContent_b = NULL;
	const char * szHidXAttrs_a = NULL;			const char * szHidXAttrs_b = NULL;
	const char * szGidParent_a = NULL;			const char * szGidParent_b = NULL;
	const char * szHidCSetOriginal_a = NULL;	const char * szHidCSetOriginal_b = NULL;
	SG_int64 attrbits_a = 0;					SG_int64 attrbits_b = 0;
	SG_diffstatus_flags dsFlags_a = 0;			SG_diffstatus_flags dsFlags_b = 0;
	SG_bool bOK_a;								SG_bool bOK_b;
	SG_treenode_entry_type tneType = SG_TREENODEENTRY_TYPE__INVALID;
	int cmpResult = 0;
	int sumDifferences = 0;

	// we assume that both tree-diffs are relative to the same starting point.
	// (this may not be strictly necessary, but it is for now.)

	VERIFY_ERR_CHECK(  SG_treediff2__get_hid_cset_original(pCtx,pTreeDiff_a,&szHidCSetOriginal_a)  );
	VERIFY_ERR_CHECK(  SG_treediff2__get_hid_cset_original(pCtx,pTreeDiff_b,&szHidCSetOriginal_b)  );
	VERIFY_COND("TestDiff2_Equivalence",(strcmp(szHidCSetOriginal_a,szHidCSetOriginal_b) == 0));

	// since both treediffs are sorted by Object GID, we can simply walk both lists in parallel
	// and look at the changes.

	VERIFY_ERR_CHECK(  SG_treediff2__iterator__first(pCtx,pTreeDiff_a,&bOK_a,&szGidObject_a,&pOD_opaque_a, &pIter_a)  );
	if (bOK_a)
		VERIFY_ERR_CHECK(  MyFn(c2tfe_helper)(pCtx,
											  pOD_opaque_a,
											  &szRepoPath_a,&szEntryName_a,
											  &szHidContent_a,&szHidXAttrs_a,
											  &szGidParent_a,
											  &attrbits_a,
											  &dsFlags_a,
											  &tneType)  );

	VERIFY_ERR_CHECK(  SG_treediff2__iterator__first(pCtx,pTreeDiff_b,&bOK_b,&szGidObject_b,&pOD_opaque_b, &pIter_b)  );
	if (bOK_b)
		VERIFY_ERR_CHECK(  MyFn(c2tfe_helper)(pCtx,
											  pOD_opaque_b,
											  &szRepoPath_b,&szEntryName_b,
											  &szHidContent_b,&szHidXAttrs_b,
											  &szGidParent_b,
											  &attrbits_b,
											  &dsFlags_b,
											  &tneType)  );

	while (bOK_a  &&  bOK_b)
	{
		cmpResult = strcmp(szGidObject_a,szGidObject_b);
		if (cmpResult == 0)
		{
			// gid present in both lists.  so the entry changed somehow in both.

			int significance = 0;

#define BOTHSET(ds1,ds2,b)		(((ds1) & (b)) && ((ds2) & (b)))
#define ONE_SET(ds1,ds2,b)		(((ds1) & (b)) || ((ds2) & (b)))						// if one or both set

			if (ONE_SET(dsFlags_a,dsFlags_b,SG_DIFFSTATUS_FLAGS__RENAMED))				// if either (or both) renamed
				if (strcmp(szEntryName_a,szEntryName_b) != 0)							// and the new name(s) are different...
					significance++;

			if (ONE_SET(dsFlags_a,dsFlags_b,SG_DIFFSTATUS_FLAGS__MOVED))
				if (strcmp(szGidParent_a,szGidParent_b) != 0)
					significance++;

			if (ONE_SET(dsFlags_a,dsFlags_b,SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS))		// if either (or both) changed xattrs
				if (((szHidXAttrs_a == NULL) != (szHidXAttrs_b == NULL))				// and is-set differs or
					|| (szHidXAttrs_a && strcmp(szHidXAttrs_a,szHidXAttrs_b) != 0))		// both set and different hids...
					significance++;

			if (ONE_SET(dsFlags_a,dsFlags_b,SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS))
				if (attrbits_a != attrbits_b)
					significance++;

			// the content HID on folders may or may not be valid -- since pendingtree version
			// cannot compute it.
			if ((tneType == SG_TREENODEENTRY_TYPE_REGULAR_FILE)  ||  (tneType == SG_TREENODEENTRY_TYPE_SYMLINK))
			{
				if (!szHidContent_a || !*szHidContent_a)
					significance++;		// TODO force compute HID in pendingtree and then let the strcmp() run.
				else if (!szHidContent_b || !*szHidContent_b)
					significance++;		// TODO force compute HID in pendingtree and then let the strcmp() run.
				else
					if (strcmp(szHidContent_a,szHidContent_b) != 0)
						significance++;
			}

			// remember __MODIFIED bit has already been stripped for folders.
			if ((dsFlags_a & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE) != (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE))
				significance++;
			else if (bIncludeUndoneEntries && ((dsFlags_a & SG_DIFFSTATUS_FLAGS__MASK_UNDONE) != (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_UNDONE)))
				significance++;

			// we don't test repo-path because it can be affected by parent renames/moves.

			if (significance)
			{
				sumDifferences++;

				INFOP("TreeDiff2_Equivalence",("Object [GID %s][Name %s %s] Overlapping changes\n\t[dsFlags %x %x]\n\t[parentGid %s %s]\n\t[hid %s %s]\n\t[xattrs %s %s]\n\t[attrbits %d %d]",
											   szGidObject_a,
											   szEntryName_a,szEntryName_b,
											   (SG_uint32)dsFlags_a,(SG_uint32)dsFlags_b,
											   ((szGidParent_a && *szGidParent_a) ? szGidParent_a : "(null)"),    ((szGidParent_b && *szGidParent_b) ? szGidParent_b : "(null)"),
											   ((szHidContent_a && *szHidContent_a) ? szHidContent_a : "(null)"), ((szHidContent_b && *szHidContent_b) ? szHidContent_b : "(null)"),
											   ((szHidXAttrs_a && *szHidXAttrs_a) ? szHidXAttrs_a : "(null)"),    ((szHidXAttrs_b && *szHidXAttrs_b) ? szHidXAttrs_b : "(null)"),
											   (SG_uint32)attrbits_a,                                             (SG_uint32)attrbits_b)  );
			}

			VERIFY_ERR_CHECK(  SG_treediff2__iterator__next(pCtx,pIter_a,&bOK_a,&szGidObject_a,&pOD_opaque_a)  );
			if (bOK_a)
				VERIFY_ERR_CHECK(  MyFn(c2tfe_helper)(pCtx,
													  pOD_opaque_a,
													  &szRepoPath_a,&szEntryName_a,
													  &szHidContent_a,&szHidXAttrs_a,
													  &szGidParent_a,
													  &attrbits_a,
													  &dsFlags_a,
													  &tneType)  );
			VERIFY_ERR_CHECK(  SG_treediff2__iterator__next(pCtx,pIter_b,&bOK_b,&szGidObject_b,&pOD_opaque_b)  );
			if (bOK_b)
				VERIFY_ERR_CHECK(  MyFn(c2tfe_helper)(pCtx,
													  pOD_opaque_b,
													  &szRepoPath_b,&szEntryName_b,
													  &szHidContent_b,&szHidXAttrs_b,
													  &szGidParent_b,
													  &attrbits_b,
													  &dsFlags_b,
													  &tneType)  );
		}
		else if (cmpResult < 0)		// gid_a < gid_b ==> gid_a is not present in list _b
		{
			// we require an actual change -- remember __MODIFIED bit was stripped on folders

			if (   (dsFlags_a & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE)
				|| (dsFlags_a & SG_DIFFSTATUS_FLAGS__MASK_MODIFIERS)
				|| (bIncludeUndoneEntries && (dsFlags_a & SG_DIFFSTATUS_FLAGS__MASK_UNDONE)))
			{
				sumDifferences++;

				INFOP("TreeDiff2_Equivalence",("Object [GID %s][Name %s] present in list _a [dsFlags %x][repo-path %s]",
											   szGidObject_a,szEntryName_a,(SG_uint32)dsFlags_a,szRepoPath_a)  );
			}

			VERIFY_ERR_CHECK(  SG_treediff2__iterator__next(pCtx,pIter_a,&bOK_a,&szGidObject_a,&pOD_opaque_a)  );
			if (bOK_a)
				VERIFY_ERR_CHECK(  MyFn(c2tfe_helper)(pCtx,
													  pOD_opaque_a,
													  &szRepoPath_a,&szEntryName_a,
													  &szHidContent_a,&szHidXAttrs_a,
													  &szGidParent_a,
													  &attrbits_a,
													  &dsFlags_a,
													  &tneType)  );
		}
		else						// gid_b is not present in list _b
		{
			if (   (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE)
				|| (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_MODIFIERS)
				|| (bIncludeUndoneEntries && (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_UNDONE)))
			{
				sumDifferences++;

				INFOP("TreeDiff2_Equivalence",("Object [GID %s][Name %s] present in list _b [dsFlags %x][repo-path %s]",
											   szGidObject_b,szEntryName_b,(SG_uint32)dsFlags_b,szRepoPath_b)  );
			}

			VERIFY_ERR_CHECK(  SG_treediff2__iterator__next(pCtx,pIter_b,&bOK_b,&szGidObject_b,&pOD_opaque_b)  );
			if (bOK_b)
				VERIFY_ERR_CHECK(  MyFn(c2tfe_helper)(pCtx,
													  pOD_opaque_b,
													  &szRepoPath_b,&szEntryName_b,
													  &szHidContent_b,&szHidXAttrs_b,
													  &szGidParent_b,
													  &attrbits_b,
													  &dsFlags_b,
													  &tneType)  );
		}
	}

	// reached end-of-list one or both of them.

	while (bOK_a)
	{
		if (   (dsFlags_a & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE)
			|| (dsFlags_a & SG_DIFFSTATUS_FLAGS__MASK_MODIFIERS)
			|| (bIncludeUndoneEntries && (dsFlags_a & SG_DIFFSTATUS_FLAGS__MASK_UNDONE)))
		{
			sumDifferences++;

			INFOP("TreeDiff2_Equivalence",("Object [GID %s][Name %s] present in list _a [dsFlags %x][repo-path %s]",
									   szGidObject_a,szEntryName_a,(SG_uint32)dsFlags_a,szRepoPath_a)  );
		}

		VERIFY_ERR_CHECK(  SG_treediff2__iterator__next(pCtx,pIter_a,&bOK_a,&szGidObject_a,&pOD_opaque_a)  );
		if (bOK_a)
			VERIFY_ERR_CHECK(  MyFn(c2tfe_helper)(pCtx,
												  pOD_opaque_a,
												  &szRepoPath_a,&szEntryName_a,
												  &szHidContent_a,&szHidXAttrs_a,
												  &szGidParent_a,
												  &attrbits_a,
												  &dsFlags_a,
												  &tneType)  );
	}

	while (bOK_b)
	{
		if (   (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_MUTUALLY_EXCLUSIVE)
			|| (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_MODIFIERS)
			|| (bIncludeUndoneEntries && (dsFlags_b & SG_DIFFSTATUS_FLAGS__MASK_UNDONE)))
		{
			sumDifferences++;

			INFOP("TreeDiff2_Equivalence",("Object [GID %s][Name %s] present in list _b [dsFlags %x][repo-path %s]",
										   szGidObject_b,szEntryName_b,(SG_uint32)dsFlags_b,szRepoPath_b)  );
		}

		VERIFY_ERR_CHECK(  SG_treediff2__iterator__next(pCtx,pIter_b,&bOK_b,&szGidObject_b,&pOD_opaque_b)  );
		if (bOK_b)
			VERIFY_ERR_CHECK(  MyFn(c2tfe_helper)(pCtx,
												  pOD_opaque_b,
												  &szRepoPath_b,&szEntryName_b,
												  &szHidContent_b,&szHidXAttrs_b,
												  &szGidParent_b,
												  &attrbits_b,
												  &dsFlags_b,
												  &tneType)  );
	}

	VERIFY_COND("TreeDiff2_Equivalence",(sumDifferences == 0));

	// fall thru to common cleanup

fail:
	INFOP("TreeDiff2_Equivalence",("Sum Differences %d",sumDifferences));
	SG_TREEDIFF2_ITERATOR_NULLFREE(pCtx, pIter_a);
	SG_TREEDIFF2_ITERATOR_NULLFREE(pCtx, pIter_b);
}

static void MyFn(commit_all)(SG_context * pCtx,
							 const SG_pathname* pPathWorkingDir,
							 SG_bool bWeThinkWeHaveChanges)
{
	SG_pendingtree* pPendingTree = NULL;
    SG_audit q;
	SG_repo * pRepo;
	SG_treediff2_debug__Stats tdStats;
	SG_treediff2 * pTreeDiff_before_commit = NULL;
	SG_treediff2 * pTreeDiff_after_commit = NULL;
	char bufBaseline_before_commit[SG_HID_MAX_BUFFER_LENGTH];
	char bufBaseline_after_commit[SG_HID_MAX_BUFFER_LENGTH];

	// get the changeset HID of the baseline before the commit.

	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx,pPathWorkingDir,bufBaseline_before_commit,sizeof(bufBaseline_before_commit))  );

	// compute treediff of baseline-vs-pendingtree before the commit.
	VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx,pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx,pPendingTree,NULL,&pTreeDiff_before_commit)  );
	SG_PENDINGTREE_NULLFREE(pCtx,pPendingTree);
	VERIFY_ERR_CHECK(  SG_treediff2_debug__compute_stats(pCtx,pTreeDiff_before_commit,&tdStats)  );
	VERIFY_COND("commit_all",(bWeThinkWeHaveChanges == (tdStats.nrTotalChanges > 0)));

#if 0		// less noise
	INFOP("commit_all",("treediff [baseline before commit %s] vs pendingtree",bufBaseline_before_commit));
	VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff_before_commit,SG_FALSE)  );
#endif

	// actually do the commit
	VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx,pPathWorkingDir, SG_FALSE, &pPendingTree)  );
    VERIFY_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );
    VERIFY_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
	VERIFY_ERR_CHECK(  unittests_pendingtree__commit(pCtx, pPendingTree, &q, NULL, 0, NULL, NULL, 0, NULL, 0, NULL, 0, NULL)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

	// get the changeset HID of the baseline after the commit.
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx,pPathWorkingDir,bufBaseline_after_commit,sizeof(bufBaseline_after_commit))  );
	if (strcmp(bufBaseline_before_commit,bufBaseline_after_commit) == 0)
	{
		VERIFY_COND("commit_all",(!bWeThinkWeHaveChanges));
	}
	else
	{
		// construct a (non-pendingtree) treediff using the 2 changeset HIDs.
		VERIFY_ERR_CHECK(  SG_treediff2__get_repo(pCtx,pTreeDiff_before_commit,&pRepo)  );
		VERIFY_ERR_CHECK(  SG_treediff2__alloc(pCtx,pRepo,&pTreeDiff_after_commit)  );
		VERIFY_ERR_CHECK(  SG_treediff2__compare_cset_vs_cset(pCtx,pTreeDiff_after_commit,bufBaseline_before_commit,bufBaseline_after_commit)  );
#if 0	// less noise
		INFOP("commit_all",("treediff [baseline before commit %s][baseline after commit [%s]",bufBaseline_before_commit,bufBaseline_after_commit));
		VERIFY_ERR_CHECK(  SG_treediff2_debug__dump_to_console(pCtx,pTreeDiff_after_commit,SG_FALSE)  );
#endif

		// verify that the baseline-vs-pendingtree and the old-baseline-vs-new-baseline
		// tree-diffs are equivalent....

		VERIFY_ERR_CHECK_DISCARD(  MyFn(compare_2_treediffs_for_equivalence)(pCtx,
																			  pTreeDiff_before_commit,
																			  pTreeDiff_after_commit,
																			  SG_FALSE)  );
	}

	// fall thru to common cleanup

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_before_commit);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_after_commit);
}

//////////////////////////////////////////////////////////////////

typedef struct MyDcl(row)
{
	const char *			szDir1;
	const char *			szDir2;
	const char *			szFile1;
	const char *			szFile2;
	SG_diffstatus_flags		dsFlags;
	SG_int32				nrContentLines;
	const char *			szXAttrParam;

	SG_pathname *			pPathDir;
	SG_pathname *			pPathFile;
	const char *			szLastDir;
	const char *			szLastFile;
} MyDcl(row);

#define ROW(d1,d2,f1,f2,ds,nrCL,xp)		{ d1, d2, f1, f2, ds, nrCL, xp, NULL, NULL, d1, f1 }

//////////////////////////////////////////////////////////////////

static void MyFn(process_row)(SG_context * pCtx,
							  const char * szLabel,
							  SG_uint32 kRow,
							  MyDcl(row) * pRow,
							  const SG_pathname * pPathWorkingDir)
{
	SG_pendingtree * pPendingTree = NULL;
	SG_string * pStrEntryName = NULL;
	SG_pathname * pPathTemp = NULL;

	// build <wd>/<dir>         := <wd>/<dir_k[1]>
	// and   <wd>/<dir>/<file>  := <wd>/<dir_k[1]>/<file_k[1]>

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pRow->pPathDir,pPathWorkingDir,pRow->szDir1)  );
	if (pRow->szFile1)
		VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pRow->pPathFile,pRow->pPathDir,pRow->szFile1)  );

	INFOP(szLabel,("\tkRow[%d]: %s %s",kRow,pRow->szDir1,((pRow->szFile1) ? pRow->szFile1 : "(null)")));

#define NR_CONTENT_LINES_IN_BASE_ADD 20
	if (pRow->dsFlags & SG_DIFFSTATUS_FLAGS__ADDED)
	{
		// mkdir <wd>/<dir>
		// if (filename starts with '>' it is a symlink)
		//     create symlink <wd>/<dir>/<file_k[1]> --> <wd>/<dir>/<file_k[2]>
		// else
		//     create file <wd>/<dir>/<file_k[1]>

		SG_bool bDirExists;

		VERIFY_ERR_CHECK(  SG_fsobj__exists__pathname(pCtx,pRow->pPathDir,&bDirExists,NULL,NULL)  );
		if (!bDirExists)
			VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx,pRow->pPathDir)  );
		if (pRow->szFile1)
		{
			if (pRow->szFile1[0] == '>')
			{
				SG_ASSERT(  (pRow->szFile2 && pRow->szFile2[0])  );
#if SYM
				VERIFY_ERR_CHECK(  MyFn(create_symlink)(pCtx,pRow->pPathDir,&pRow->szFile1[1],pRow->szFile2)  );
#else
				SG_ASSERT(  (0  && "This platform does not support symlinks.")  );
#endif
			}
			else
			{
				VERIFY_ERR_CHECK(  MyFn(create_file__numbers)(pCtx,pRow->pPathDir,pRow->szFile1,NR_CONTENT_LINES_IN_BASE_ADD)  );
			}
		}
	}

	if (pRow->dsFlags & SG_DIFFSTATUS_FLAGS__DELETED)
	{
		if (pRow->szFile1)
		{
			// delete <wd>/<dir>/<file_k[1]>

			VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx,pPathWorkingDir,SG_FALSE,&pPendingTree)  );
			VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx,pPendingTree,pRow->pPathDir,1,&pRow->szFile1, NULL, 0 , NULL, 0, SG_FALSE)  );
			SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
		}
		else
		{
			// rmdir <wd>/<dir>
			// TODO does this need to be empty?

			VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx,pPathWorkingDir,SG_FALSE,&pPendingTree)  );
			VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx,pPendingTree,pPathWorkingDir,1,&pRow->szDir1, NULL, 0, NULL, 0, SG_FALSE)  );
			SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
		}
	}

	if (pRow->dsFlags & SG_DIFFSTATUS_FLAGS__MODIFIED)
	{
		// if first letter of file1 is a '>' then we are changing the target of a symlink
		// otherwise we are modifying the contents of the file.

		if (pRow->szFile1[0] == '>')
		{
			SG_ASSERT(  (pRow->szFile2 && pRow->szFile2[0])  );
#if SYM
			VERIFY_ERR_CHECK(  MyFn(replace_symlink)(pCtx,pRow->pPathDir,&pRow->szFile1[1],pRow->szFile2)  );
#else
			SG_ASSERT(  (0  &&  "This platform does not support symlinks.")  );
#endif
		}
		else
		{
			if (pRow->nrContentLines > 0)
			{
				// append n lines of data to <wd>/<dir>/<file_k[1]>

				VERIFY_ERR_CHECK(  MyFn(append_to_file__numbers)(pCtx,pRow->pPathDir,pRow->szFile1,pRow->nrContentLines)  );
			}
			else if (pRow->nrContentLines < 0)
			{
				// prepend -n lines of data to <wd>/<dir>/<file_k[1]>
				// we need to delete it and re-create it becuse create_file__numbers2 does a create-new.

				VERIFY_ERR_CHECK(  MyFn(delete_file)(pCtx,pRow->pPathDir,pRow->szFile1)  );
				VERIFY_ERR_CHECK(  MyFn(create_file__numbers2)(pCtx,pRow->pPathDir,pRow->szFile1,
															   NR_CONTENT_LINES_IN_BASE_ADD,
															   -pRow->nrContentLines)  );
			}
#if defined(DEBUG)
			else
				SG_ASSERT( 0 );
#endif
		}
	}

	if (pRow->dsFlags & SG_DIFFSTATUS_FLAGS__RENAMED)
	{
		if (pRow->szFile1)
		{
			// rename <wd>/<dir>/<file> --> <wd>/<dir>/<file_k[2]>
			// let    <wd>/<dir>/<file>  := <wd>/<dir>/<file_k[2]>

			SG_ASSERT( pRow->szFile2 );
			VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx,pPathWorkingDir,SG_FALSE,&pPendingTree)  );
			VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx,pPendingTree,pRow->pPathFile,pRow->szFile2, SG_FALSE)  );
			SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
			SG_PATHNAME_NULLFREE(pCtx, pRow->pPathFile);
			VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pRow->pPathFile,pRow->pPathDir,pRow->szFile2)  );
		}
		else
		{
			// rename <wd>/<dir> --> <wd>/<dir_k[2]>
			// let    <wd>/<dir>  := <wd>/<dir_k[2]>

			SG_ASSERT( pRow->szDir2 );
			VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx,pPathWorkingDir,SG_FALSE,&pPendingTree)  );
			VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx,pPendingTree,pRow->pPathDir,pRow->szDir2, SG_FALSE)  );
			SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
			SG_PATHNAME_NULLFREE(pCtx, pRow->pPathDir);
			VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pRow->pPathDir,pPathWorkingDir,pRow->szDir2)  );
		}
	}

	if (pRow->dsFlags & SG_DIFFSTATUS_FLAGS__MOVED)
	{
		if (pRow->szFile1)
		{
			// move <wd>/<dir>/<file> --> <wd>/<dir_k[2]>
			// let  <wd>/<dir>         := <wd>/<dir_k[2]>
			// let  <wd>/<dir>/<file>  := <wd>/<dir_k[2]>/<file_k[?]>

			SG_ASSERT( pRow->szDir2 );
			SG_PATHNAME_NULLFREE(pCtx, pRow->pPathDir);
			VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pRow->pPathDir,pPathWorkingDir,pRow->szDir2)  );

			VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx,pPathWorkingDir,SG_FALSE,&pPendingTree)  );
			VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx,pPendingTree,(const SG_pathname **)&(pRow->pPathFile), 1,pRow->pPathDir, SG_FALSE)  );
			SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

			VERIFY_ERR_CHECK(  SG_pathname__get_last(pCtx,pRow->pPathFile,&pStrEntryName)  );
			SG_PATHNAME_NULLFREE(pCtx, pRow->pPathFile);
			VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pRow->pPathFile,pRow->pPathDir,SG_string__sz(pStrEntryName))  );
			SG_STRING_NULLFREE(pCtx, pStrEntryName);
		}
		else
		{
			// move a directory.  we assume that both <dir_k[1]> and <dir_k[2]> are relative pathnames
			// and that the first is the directory to be moved and the second is the path of the parent
			// directory.  for example "a/b/c" and "d/e/f" will put "c" inside "f".

			SG_ASSERT( pRow->szDir2 );
			VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pPathTemp,pPathWorkingDir,pRow->szDir2)  );

			VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx,pPathWorkingDir,SG_FALSE,&pPendingTree)  );
			VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx,pPendingTree,(const SG_pathname **)&(pRow->pPathDir), 1, pPathTemp, SG_FALSE)  );
			SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

			VERIFY_ERR_CHECK(  SG_pathname__get_last(pCtx,pRow->pPathDir,&pStrEntryName)  );
			SG_PATHNAME_NULLFREE(pCtx, pRow->pPathDir);
			VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pPathTemp,SG_string__sz(pStrEntryName))  );
			SG_STRING_NULLFREE(pCtx, pStrEntryName);

			SG_pathname__free(pCtx, pRow->pPathDir);
			pRow->pPathDir = pPathTemp;
			pPathTemp = NULL;
		}
	}

#if AA
	if (pRow->dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)
	{
#if defined(DEBUG)
		if (!pRow->szXAttrParam)
			SG_ASSERT( 0 );
#endif

		if (pRow->szXAttrParam[0] == '2')
		{
			// add "com.sourcegear.test2" XAttr to the set (we don't know whether .test1 is set or not)

			SG_byte * szValue = NULL;

			if (pRow->szXAttrParam[1] == 'l')		// "l" puts the given label in the xattr (this will vary by branch)
				szValue = (SG_byte *)szLabel;
			else if (pRow->szXAttrParam[1] == 'f')	// "f" puts the filename in the xattr (this will NOT vary by branch)
				szValue = (SG_byte *)__FILE__;
#if defined(DEBUG)
			else
				SG_ASSERT( (0) );
#endif

			if (pRow->szFile1)
				VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx,pRow->pPathFile,"com.sourcegear.test2",2,szValue)  );
			else
				VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx,pRow->pPathDir,"com.sourcegear.test2",2,szValue)  );
		}
		else if (pRow->szXAttrParam[0] == '1')
		{
			// set "com.sourcegear.test1" XAttr with new value (it may or may not already be present, so this
			// may add it or replace the value).

			SG_byte * szValue = NULL;

			if (pRow->szXAttrParam[1] == 'l')		// "l" puts the given label in the xattr (this will vary by branch)
				szValue = (SG_byte *)szLabel;
			else if (pRow->szXAttrParam[1] == 'f')	// "f" puts the filename in the xattr (this will NOT vary by branch)
				szValue = (SG_byte *)__FILE__;
#if defined(DEBUG)
			else
				SG_ASSERT( (0) );
#endif

			if (pRow->szFile1)
				VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx,pRow->pPathFile,"com.sourcegear.test1",2,szValue)  );
			else
				VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx,pRow->pPathDir,"com.sourcegear.test1",2,szValue)  );
		}
		else if (pRow->szXAttrParam[0] == 'x')
		{
			// remove all XAttrs (without regard to who set them)

			const SG_pathname * pPath = ((pRow->szFile1) ? pRow->pPathFile : pRow->pPathDir);

			SG_fsobj__xattr__remove(pCtx,pPath,"com.sourcegear.test1");	// ignore error incase this xattr name:value pair wasn't set.
			SG_context__err_reset(pCtx);
			SG_fsobj__xattr__remove(pCtx,pPath,"com.sourcegear.test2");	// ignore error incase this xattr name:value pair wasn't set.
			SG_context__err_reset(pCtx);
		}
	}
#endif

#if AB
	if (pRow->dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)
	{
		if (pRow->szFile1)
		{
			SG_fsobj_stat st;

			VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx,pRow->pPathFile,&st)  );
			if (st.perms & S_IXUSR)
				st.perms &= ~S_IXUSR;
			else
				st.perms |= S_IXUSR;
			VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx,pRow->pPathFile,st.perms)  );
		}
		else
		{
			// TODO chmod a directory.  we currently only define the +x bit on files.

			SG_ASSERT( 0 );
		}
	}
#endif

	// fall thru to common cleanup

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRING_NULLFREE(pCtx, pStrEntryName);
	SG_PATHNAME_NULLFREE(pCtx, pPathTemp);
}

static void MyFn(cleanup_table)( SG_context * pCtx, MyDcl(row) aRow[], SG_uint32 nrElements )
{
	SG_uint32 k;

	for (k=0; k<nrElements; k++)
	{
		SG_PATHNAME_NULLFREE(pCtx, aRow[k].pPathDir);
		SG_PATHNAME_NULLFREE(pCtx, aRow[k].pPathFile);
	}
}

//////////////////////////////////////////////////////////////////

static void MyFn(do_external_listall)(SG_context * pCtx,
									  const SG_pathname * pPathWorkingDir)
{
	// This is a crude test to invoke 'vv resolve --listall'.
	// It doesn't actually verify any of the information, but
	// it does exercise a lot of the code and makes sure that
	// none of the various collisions we can generate cause
	// --listall to fail/crash and lets me manually verify
	// the outputs.
	//
	// TODO Update this test to take the nrFilesAutoMerged
	// TODO and nrUnresolvedIssues and then search the output
	// TODO for them.
	//
	// TODO Update this test to take a list of expected
	// TODO answers and verify they match.

	SG_pathname * pPath_cwd = NULL;
	SG_exec_argvec * pArgVec_status  = NULL;
	SG_exec_argvec * pArgVec_resolve = NULL;
	SG_exit_status exitStatus;
	SG_bool b_cd_successful = SG_FALSE;

	// initial_cwd = `pwd`
	// cd <wd_top>
	// vv resolve --listall
	// cd <inital_cwd>

	VERIFY_ERR_CHECK(  SG_exec_argvec__alloc(pCtx, &pArgVec_status)  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec_status, "status")  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec_status, "--ignore-ignores")  );

	VERIFY_ERR_CHECK(  SG_exec_argvec__alloc(pCtx, &pArgVec_resolve)  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec_resolve, "resolve")  );
	VERIFY_ERR_CHECK(  SG_exec_argvec__append__sz(pCtx, pArgVec_resolve, "--listall")  );

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC(pCtx, &pPath_cwd)  );
	VERIFY_ERR_CHECK(  SG_pathname__set__from_cwd(pCtx, pPath_cwd)  );
	INFOP("do_external_listall",("Initial CWD is [%s]", SG_pathname__sz(pPath_cwd)));
	INFOP("do_external_listall",("Changing to    [%s]", SG_pathname__sz(pPathWorkingDir)));
	
	VERIFY_ERR_CHECK(  SG_fsobj__cd__pathname(pCtx, pPathWorkingDir)  );
	b_cd_successful = SG_TRUE;

	VERIFY_ERR_CHECK(  SG_exec__exec_sync__files(pCtx, "vv", pArgVec_status, NULL, NULL, NULL, &exitStatus)  );
	VERIFY_COND("child_status", (exitStatus == 0));

	VERIFY_ERR_CHECK(  SG_exec__exec_sync__files(pCtx, "vv", pArgVec_resolve, NULL, NULL, NULL, &exitStatus)  );
	VERIFY_COND("child_status", (exitStatus == 0));

fail:
	if (b_cd_successful)
	{
		INFOP("do_external_listall",("Changing to    [%s]", SG_pathname__sz(pPath_cwd)));
		SG_ERR_IGNORE(  SG_fsobj__cd__pathname(pCtx, pPath_cwd)  );
	}

	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec_status);
	SG_EXEC_ARGVEC_NULLFREE(pCtx, pArgVec_resolve);
	SG_PATHNAME_NULLFREE(pCtx, pPath_cwd);
}

//////////////////////////////////////////////////////////////////

static void MyFn(doCase_1)(SG_context * pCtx,
						   const SG_pathname * pPathTopDir,
						   struct MyDcl(row) Table_A[],  SG_uint32 nrRows_Table_A,
						   struct MyDcl(row) Table_B[],  SG_uint32 nrRows_Table_B,
						   struct MyDcl(row) Table_C[],  SG_uint32 nrRows_Table_C,
						   SG_bool bUseSwitchBaseline)
{
	//////////////////////////////////////////////////////////////////
	// create various changesets to produce this graph:
	//
	//     I
	//     |
	//     A
	//    / \.
	//   B   C
	//
	// where I is the initial (empty) changeset when the repo is created.
	//
	// with Baseline I, add a bunch of files and create CSET A so that we
	// have something to work with later.
	//
	// with Baseline A, apply some changes in the WD and create CSET B.
	//
	// with Baseline A, apply some changes in the WD and create CSET C.
	//
	// with Baseline C, try to merge B.  if all goes well, commit it as
	// CSET M.
	//
	//     A
	//    / \.
	//   B   C
	//    \ /
	//     M
	//
	//////////////////////////////////////////////////////////////////

	char bufName_repo[SG_TID_MAX_BUFFER_LENGTH];
	char bufCSet_I[SG_HID_MAX_BUFFER_LENGTH];
	char bufCSet_A[SG_HID_MAX_BUFFER_LENGTH];
	char bufCSet_B[SG_HID_MAX_BUFFER_LENGTH];
	char bufCSet_C[SG_HID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pendingtree * pPendingTree = NULL;
	SG_repo* pRepo = NULL;
	SG_rbtree * prbCSetsToMerge = NULL;
	SG_uint32 kRow;
	SG_mrg * pMrg = NULL;
	const char * szNote = "wd_1";
	SG_bool bIgnorePortabilityWarnings = SG_TRUE;				// TODO allow this to change
	const SG_wd_plan * pPlan;
	const SG_wd_plan_stats * pStats;
	SG_uint32 nrUnresolvedIssues;
	const SG_varray * pvaIssues;

	//////////////////////////////////////////////////////////////////
	// create an unique TID to be the logical name of the repo and serve
	// as the root directory of the WD.

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName_repo, sizeof(bufName_repo), 6)  );

	// create working directory in <topdir>/wd_1/<repo_name>

	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pPathWorkingDir, pPathTopDir, "wd_1")  );
	VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pPathWorkingDir,bufName_repo)  );
	INFO2("test1 working directory [1]", SG_pathname__sz(pPathWorkingDir));
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir_recursive__pathname(pCtx,pPathWorkingDir)  );

	// create a repo using <repo_name> and <topdir>/wd_1/<repo_name>.
	// the initial (implied) commit will be known as CSET_I.

	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx,bufName_repo, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx,pPathWorkingDir,bufCSet_I,sizeof(bufCSet_I))  );

	VERIFY_ERR_CHECK(  SG_repo__open_repo_instance(pCtx,bufName_repo,&pRepo)  );

	//////////////////////////////////////////////////////////////////
	// Using wd_1, add a collection of stuff and create A.

	INFOP("begin_building_A",("begin_building_A [%s]",szNote));
	for (kRow=0; kRow < nrRows_Table_A; kRow++)
	{
		MyDcl(row) * pRowA = & Table_A[kRow];

		VERIFY_ERR_CHECK_DISCARD(  MyFn(process_row)(pCtx,"A",kRow,pRowA,pPathWorkingDir)  );
	}
	INFOP("committing_A",("committing_A"));
	VERIFY_ERR_CHECK(  _ut_pt__addremove_param(pCtx,pPathWorkingDir,bIgnorePortabilityWarnings)  );
	VERIFY_ERR_CHECK(  MyFn(commit_all)(pCtx,pPathWorkingDir,SG_TRUE)  );
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx,pPathWorkingDir,bufCSet_A,sizeof(bufCSet_A))  );
	INFOP("CSET_A",("CSET_A %s",bufCSet_A));
	VERIFY_ERR_CHECK(  MyFn(dump_treenodes_in_changeset)(pCtx,"CSET_A",pRepo,bufCSet_A)  );

	//////////////////////////////////////////////////////////////////
	// Using wd_1, starting from CSET_A, create CSET_B by making some changes.

	INFOP("begin_building_B",("begin_building_B [%s]",szNote));
	for (kRow=0; kRow < nrRows_Table_B; kRow++)
	{
		MyDcl(row) * pRowB = & Table_B[kRow];

		VERIFY_ERR_CHECK_DISCARD(  MyFn(process_row)(pCtx,"B",kRow,pRowB,pPathWorkingDir)  );
	}
	INFOP("committing_B",("committing_B"));
	VERIFY_ERR_CHECK(  _ut_pt__addremove_param(pCtx,pPathWorkingDir,bIgnorePortabilityWarnings)  );
	VERIFY_ERR_CHECK(  MyFn(commit_all)(pCtx,pPathWorkingDir,SG_TRUE)  );
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx,pPathWorkingDir,bufCSet_B,sizeof(bufCSet_B))  );
	INFOP("CSET_B",("CSET_B %s",bufCSet_B));
	VERIFY_ERR_CHECK(  MyFn(dump_treenodes_in_changeset)(pCtx,"CSET_B",pRepo,bufCSet_B)  );

	//////////////////////////////////////////////////////////////////
	// There is currently (09/16/2009) a problem with switching baselines
	// in a WD (changing the contents of the existing WD to match the
	// state it should be in at the other baseline).  So we create a new WD
	// seeded to the state we want and use it for the rest of test.
	//
	// So we allow the test to run with switch-baselines or with 2 wd's.

	if (bUseSwitchBaseline)
	{
		// switch the baseline back to A and create C.
		//
		// we do not create the 2nd copy of the working directory;
		// we just reuse the first working directory by switching the baseline.
		//
		// select A as the baseline.

		INFOP("SwitchingBaseline",("SwitchingBaseline"));
		VERIFY_ERR_CHECK(  MyFn(switch_to_baseline)(pCtx,pPathWorkingDir,bufCSet_A)  );
	}
	else
	{
		SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);

		// create a new WD (wd_2) bound to the same REPO.
		// we have to do this in parts because of the WD API:
		// [a] we create <topdir>/wd_2/.  we ignore errors on
		//     this because this path is the same for all of the
		//     tests; we don't get a unique path until the <repo-name>
		//     gets appended.

		VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx,&pPathWorkingDir, pPathTopDir, "wd_2")  );
		SG_ERR_IGNORE(  SG_fsobj__mkdir_recursive__pathname(pCtx,pPathWorkingDir)  );

		// [b] we then append <repo_name> to our pathname so that we can
		//     begin adding files to the WD.

		VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx,pPathWorkingDir,bufName_repo)  );

		// [c] we let the workingdir code create the <repo_name> directory under it
		//     and populate it with the baseline as of CSET A.

		INFO2("test1 working directory [2]", SG_pathname__sz(pPathWorkingDir));

		VERIFY_ERR_CHECK(  unittests_workingdir__create_and_get(pCtx,bufName_repo,pPathWorkingDir,SG_TRUE,bufCSet_A)  );

		szNote = "wd_2";
	}

	INFOP("begin_building_C",("begin_building_C [%s]",szNote));
	for (kRow=0; kRow < nrRows_Table_C; kRow++)
	{
		MyDcl(row) * pRowC = & Table_C[kRow];

		VERIFY_ERR_CHECK_DISCARD(  MyFn(process_row)(pCtx,"C",kRow,pRowC,pPathWorkingDir)  );
	}
	INFOP("committing_C",("committing_C"));
	VERIFY_ERR_CHECK(  _ut_pt__addremove_param(pCtx,pPathWorkingDir,bIgnorePortabilityWarnings)  );
	VERIFY_ERR_CHECK(  MyFn(commit_all)(pCtx,pPathWorkingDir,SG_TRUE)  );
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx,pPathWorkingDir,bufCSet_C,sizeof(bufCSet_C))  );
	INFOP("CSET_C",("CSET_C %s",bufCSet_C));
	VERIFY_ERR_CHECK(  MyFn(dump_treenodes_in_changeset)(pCtx,"CSET_C",pRepo,bufCSet_C)  );

	//////////////////////////////////////////////////////////////////
	// compute the merge.

	INFOP("beginning_merge",("A[%s] B[%s] C[%s]",
							 bufCSet_A,bufCSet_B,bufCSet_C));

	VERIFY_ERR_CHECK(  SG_RBTREE__ALLOC(pCtx, &prbCSetsToMerge)  );
	VERIFY_ERR_CHECK(  SG_rbtree__add(pCtx, prbCSetsToMerge, bufCSet_B)  );

	VERIFY_ERR_CHECK(  SG_pendingtree__alloc(pCtx,pPathWorkingDir,bIgnorePortabilityWarnings,&pPendingTree)  );

	VERIFY_ERR_CHECK(  SG_MRG__ALLOC(pCtx,pPendingTree, SG_FALSE, &pMrg)  );
	VERIFY_ERR_CHECK(  SG_mrg__compute_merge(pCtx, pMrg, prbCSetsToMerge, &nrUnresolvedIssues)  );

	VERIFY_ERR_CHECK(  SG_mrg__get_wd_plan__ref(pCtx, pMrg, &pPlan)  );
	VERIFY_ERR_CHECK(  SG_wd_plan__get_stats__ref(pCtx, pPlan, &pStats)  );

	SG_ERR_IGNORE(  SG_wd_plan_debug__dump_script(pCtx, pPlan, "Merge Results")  );
	if (nrUnresolvedIssues)
	{
		SG_bool bHaveIssues;

		VERIFY_ERR_CHECK(  SG_mrg__get_wd_issues__ref(pCtx, pMrg, &bHaveIssues, &pvaIssues)  );
		SG_ERR_IGNORE(  SG_varray_debug__dump_varray_of_vhashes_to_console(pCtx, pvaIssues, "Unresolved Merge Issues")  );
	}

	INFOP("Stats",("%d updated, %d deleted, %d added, %d merged, %d unresolved",
				   pStats->nrDirsChanged + pStats->nrFilesChanged,
				   pStats->nrDirsDeleted + pStats->nrFilesDeleted,
				   pStats->nrDirsAdded   + pStats->nrFilesAdded,
				   pStats->nrFilesAutoMerged,
				   nrUnresolvedIssues)  );

	VERIFY_ERR_CHECK(  SG_mrg__apply_merge(pCtx, pMrg)  );

	VERIFY_ERR_CHECK(  SG_pendingtree__save(pCtx, pPendingTree)  );
	SG_PENDINGTREE_NULLFREE(pCtx,pPendingTree);

	//////////////////////////////////////////////////////////////////
	// Quick test to see what 'vv resolve --listall' says.

	VERIFY_ERR_CHECK(  MyFn(do_external_listall)(pCtx, pPathWorkingDir)  );

	//////////////////////////////////////////////////////////////////

	fprintf(stderr,"\n\n##################################################################\n\n");

	//////////////////////////////////////////////////////////////////
	// TODO verify that we have dirty WD and that the dirt is what we expect.
	// TODO note that this dirt will appear as deltas from the baseline rather
	// TODO than CSET A.

	// TODO....

	//////////////////////////////////////////////////////////////////

	// fall thru to common cleanup

fail:
	SG_RBTREE_NULLFREE(pCtx, prbCSetsToMerge);
	SG_PENDINGTREE_NULLFREE(pCtx,pPendingTree);
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_REPO_NULLFREE(pCtx, pRepo);
	MyFn(cleanup_table)(pCtx,Table_A,nrRows_Table_A);
	MyFn(cleanup_table)(pCtx,Table_B,nrRows_Table_B);
	MyFn(cleanup_table)(pCtx,Table_C,nrRows_Table_C);
	SG_MRG_NULLFREE(pCtx, pMrg);
}

//////////////////////////////////////////////////////////////////

static void MyFn(doCase)(SG_context * pCtx,
						 const SG_pathname * pPathTopDir,
						 struct MyDcl(row) Table_A[],  SG_uint32 nrRows_Table_A,
						 struct MyDcl(row) Table_B[],  SG_uint32 nrRows_Table_B,
						 struct MyDcl(row) Table_C[],  SG_uint32 nrRows_Table_C,
						 SG_bool bUseSwitchBaseline)
{
	// run the test as originally designed
	//
	//     A
	//    / \.
	//   B   C (baseline for merge)

	BEGIN_TEST(  MyFn(doCase_1)(pCtx,
								pPathTopDir,
								Table_A, nrRows_Table_A,
								Table_B, nrRows_Table_B,
								Table_C, nrRows_Table_C,
								bUseSwitchBaseline)  );

	// swap B & C and run it again.
	//
	//     A
	//    / \.
	//   C   B (baseline for merge)

	BEGIN_TEST(  MyFn(doCase_1)(pCtx,
								pPathTopDir,
								Table_A, nrRows_Table_A,
								Table_C, nrRows_Table_C,
								Table_B, nrRows_Table_B,
								bUseSwitchBaseline)  );
}

//////////////////////////////////////////////////////////////////

#if DO_000
static void MyFn(test000)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 2 orthogonal adds in 2 orthogonal directories.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_103",	NULL,	NULL,		NULL,			(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_103",	NULL,	"f4.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////
	// verify the result-cset stat summary.  note that for directories
	// we do not count a changed blob-hid as a change (just the meta-data
	// on the directory).
	//
	// the merged tree should have:
	// @/                     U
	// @/dir_100              U
	// @/dir_100/f1.txt       U
	// @/dir_102/             A
	// @/dir_102/fd.txt       A
	// @/dir_103/             A
	// @/dir_103/f4.txt       A

//	SG_mrg_cset_stats csetStats = { 3, 0, 0, 4, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test001)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 2 orthogonal adds in the same directory.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f4.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 0, 0, 2, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test002_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 modify in the same directory.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 2, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test002_1)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 modify in the same directory (reverse branches)
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 2, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test003_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 rename in the same directory.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	"f1_r.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 2, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test003_1)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 rename in the same directory (reverse branches)
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",	"f1_r.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 2, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test004_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 move
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100","dir_101","f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test004_1)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 move (reverse branches)
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101","f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

#if AA
static void MyFn(test005_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 xattr
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1l"),		// put a value on com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test005_1)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 xattr (reverse branches)
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1l"),		// put a value on com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test005_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 xattr -- start with XATTR on original file and take it off
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1l"),		// put a value on com.sourcegear.test1
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"x"),		// remove com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test005_3)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 xattr -- start with XATTR on original file and take it off (reverse branches)
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1l"),		// put a value on com.sourcegear.test1
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"x"),		// remove com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif

#if AB
static void MyFn(test006_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 attrbit
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test006_1)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 attrbit (reverse branches)
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test006_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 attrbit
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test006_3)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 attrbit (reverse branches)
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif

static void MyFn(test007_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 delete
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 0, 1, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test007_1)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 adds and 1 delete (reverse branches)
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 0, 1, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

#if SYM
static void MyFn(test008_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// 1 add some symlinks
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	">sym1.txt",	"f1.txt",	(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"fd.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	">sym2.txt",	"fd.txt",	(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fz.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	">sym3.txt",	"fz.txt",	(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 0, 1, 1, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif//SYM
#endif//DO_000

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

#if DO_100
static void MyFn(test100)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides delete same file
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 0, 1, 2, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test101)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides changed the same file in the same way.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",	"f1_r.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	"f1_r.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 2, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test102)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides changed the same file in the same way.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101","f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100","dir_101","f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 2, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

#if AA
static void MyFn(test103_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides changed the same file in the same way.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1f"),		// put a value on com.sourcegear.test1
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1f"),		// put SAME value on com.sourcegear.test1
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 2, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test103_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides changed the same file in the same way.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1f"),		// put a value on com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"x"),		// remove com.sourcegear.test1
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"x"),		// remove com.sourcegear.test1
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 2, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif

#if AB
static void MyFn(test104_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides changed the same file in the same way.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 2, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test104_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides changed the same file in the same way.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 2, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif

static void MyFn(test105)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides changed the same file in the same way.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 2, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

#if SYM
static void MyFn(test106)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides moved the same symlink file in the same way.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	">sym1.txt",	"f1.txt",	(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101","sym1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100","dir_101","sym1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 2, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test107)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides changed the same symlink file in the same way.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	">sym1.txt",	"f1.txt",	(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"sym1.txt",	"sym1_r.txt",	(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"sym1.txt",	"sym1_r.txt",	(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 2, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test108)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides changed the same symlink file in the same way.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f2.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	">sym.txt",		"f1.txt",	(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	">sym.txt",		"f2.txt",	(SG_DIFFSTATUS_FLAGS__MODIFIED),	0,		NULL),
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	">sym.txt",		"f2.txt",	(SG_DIFFSTATUS_FLAGS__MODIFIED),	0,		NULL),
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 1, 0, 2, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif//SYM
#endif//DO_100

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

#if DO_200
static void MyFn(test200)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// make 2 orthogonal changes to an entry.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",	"f1_r.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 1, 0, 0, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test201)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// make 2 orthogonal changes to an entry.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101","f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 1, 0, 0, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

#if AA
static void MyFn(test202_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// make 2 orthogonal changes to an entry.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1f"),		// put a value on com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 1, 0, 0, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test202_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// make 2 orthogonal changes to an entry.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1f"),		// put a value on com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"x"),		// remove com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 1, 0, 0, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif

#if AB
static void MyFn(test203_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// make 2 orthogonal changes to an entry.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 1, 0, 0, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test203_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// make 2 orthogonal changes to an entry.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 1, 0, 0, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif

static void MyFn(test204_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// make 2 orthogonal changes to an entry.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",	"f1_r.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100","dir_101","f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 1, 0, 0, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test204_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// make 2 orthogonal changes to an entry.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101",	"f1.txt",	"f1_r.txt",		(SG_DIFFSTATUS_FLAGS__MOVED
																 | SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 1, 0, 0, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test204_4)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// make 2 orthogonal changes to an entry.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101","f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__MODIFIED),		+4,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	"f1_r.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 1, 0, 0, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif//DO_200

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

#if DO_300
static void MyFn(test300)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// delete file in one branch and do something else in the other.  should yield conflict.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",	"f1_r.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 0, 0, 0, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test301)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// delete file in one branch and do something else in the other.  should yield conflict.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101","f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 0, 0, 0, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

#if AA
static void MyFn(test302_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// delete file in one branch and do something else in the other.  should yield conflict.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1f"),		// put a value on com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 0, 0, 0, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test302_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// delete file in one branch and do something else in the other.  should yield conflict.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1f"),		// put a value on com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"x"),		// remove com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 0, 0, 0, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif

#if AB
static void MyFn(test303_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// delete file in one branch and do something else in the other.  should yield conflict.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 0, 0, 0, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test303_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// delete file in one branch and do something else in the other.  should yield conflict.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 0, 0, 0, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif

static void MyFn(test304_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// delete file in one branch and do something else in the other.  should yield conflict.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",	"f1_r.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 0, 0, 0, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test304_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// delete file in one branch and do something else in the other.  should yield conflict.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101",	"f1.txt",	"f1_r.txt",		(SG_DIFFSTATUS_FLAGS__MOVED
																 | SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 0, 0, 0, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test304_4)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// delete file in one branch and do something else in the other.  should yield conflict.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101","f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__MODIFIED),		+4,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 0, 0, 0, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

#if SYM
static void MyFn(test305)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// delete symlink file in one branch and do something else in the other.  should yield conflict.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	">sym1.txt",	"f1.txt",	(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101","sym1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"sym1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 0, 0, 0, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif//SYM
#endif//DO_300

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

#if DO_400
static void MyFn(test401)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides changed the same file in the same way with a different value.  this should be a conflict
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",	"f1_rA.txt",	(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	"f1_rB.txt",	(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 0, 0, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test402)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides changed the same file in the same way with a different value.  this should be a conflict
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101","f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100","dir_102","f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 4, 0, 0, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

#if AA
static void MyFn(test403_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// both sides changed the same file in the same way with a different value.  this should be a conflict
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1f"),		// put a value on com.sourcegear.test1
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"2f"),		// put a value on com.sourcegear.test2
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 0, 0, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test403_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1f"),		// put a value on com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1l"),		// put a different value on com.sourcegear.test1
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1l"),		// put yet another value on com.sourcegear.test1
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 0, 0, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test403_4)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1f"),		// put a value on com.sourcegear.test1
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"1l"),		// put a different value on com.sourcegear.test1
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS),		0,		"x"),		// remove com.sourcegear.test1
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 0, 0, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif

#if AB
#if 0 // we currently only have 2 values for attrbits [0,1] for the chmod+x bit.  so we can't set B and C to unique values that are different from A.
static void MyFn(test404_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 0, 0, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test404_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS),		0,		NULL),		// toggle chmod +x/-x
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { 3, 0, 0, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif
#endif

static void MyFn(test405_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),		// create 20 line file
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),		// append 4 lines
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+8,		NULL),		// append 8 lines
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////
	// because both branches wrote to the end of the file, diff3 or whatever tool we use should consider this a hard-conflict.

//	SG_mrg_cset_stats csetStats = { 3, 0, 0, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test405_1)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),		// create 20 line file
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	+4,		NULL),		// append 4 lines
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__MODIFIED),	-4,		NULL),		// prepend 4 lines
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////
	// because both branches wrote to different parts of the file, diff3 should consider this an auto-merge-able file.
	// we mark it a "provisional" conflict because we want the caller to decide what diff3-like tool to use.

//	SG_mrg_cset_stats csetStats = { 3, 0, 0, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

#if SYM
static void MyFn(test406  )(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),		// create 20 line file
		ROW("dir_100",	NULL,	"f2.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),		// create 20 line file
		ROW("dir_100",	NULL,	"f3.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),		// create 20 line file
		ROW("dir_100",	NULL,	">sym.txt",		"f1.txt",	(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	">sym.txt",		"f2.txt",	(SG_DIFFSTATUS_FLAGS__MODIFIED),	0,		NULL),
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	">sym.txt",		"f3.txt",	(SG_DIFFSTATUS_FLAGS__MODIFIED),	0,		NULL),
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////
	// because both branches wrote to different parts of the file, diff3 should consider this an auto-merge-able file.
	// we mark it a "provisional" conflict because we want the caller to decide what diff3-like tool to use.

//	SG_mrg_cset_stats csetStats = { 3, 0, 0, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif//SYM
#endif//DO_400

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

#if DO_500
static void MyFn(test500_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// create an orphan.  create or move a file into a directory on one side
	// and delete that directory on the other side.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101","f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_101",	NULL,	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { ?, ?, ?, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_C, nrRows_Table_C,
				 Table_B, nrRows_Table_B,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test500_1)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// create an orphan.  create or move a file into a directory on one side
	// and delete that directory on the other side.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",				NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",				NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",				NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101/aa",			NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101/aa/bb",		NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101/aa/bb/cc",		NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),

	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",			"dir_101/aa/bb/cc",		"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// mv @/dir_100/f1.txt --> @/dir_101/aa/bb/cc/f1.txt
		ROW("dir_100",			NULL,					"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_101/aa/bb/cc",		NULL,	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),	// rmdir @/dir_101/aa/bb/cc/
		ROW("dir_101/aa/bb",		NULL,	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),	// rmdir @/dir_101/aa/bb/
		ROW("dir_101/aa",			NULL,	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),	// rmdir @/dir_101/aa/
		ROW("dir_101",				NULL,	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),	// rmdir @/dir_101/
		ROW("dir_100",				NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { ?, ?, ?, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_C, nrRows_Table_C,
				 Table_B, nrRows_Table_B,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test501)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// modify child file that is in a directory with a conflict.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101",    "f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// move   @/dir_100/f1.txt --> @/dir_101/f1.txt
		ROW("dir_101","dir_101_b",	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),	// rename @/dir_101/       --> @/dir_101_b/
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_101","dir_101_c",	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),	// rename @/dir_101/       --> @/dir_101_c/
		ROW("dir_100",	NULL,	    "fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

	// f1.txt is not an orphan, we just can't resolve it's place in the tree until the conflict on the parent directory is resolved.

//	SG_mrg_cset_stats csetStats = { ?, ?, ?, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test502)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// create or move a file into a directory on one side
	// and create a conflict on the directory.
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_103",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100","dir_101",    "f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// move   @/dir_100/f1.txt --> @/dir_101/f1.txt
		ROW("dir_101","dir_102",	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// move   @/dir_101/       --> @/dir_102/dir_101
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_101","dir_103",	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// move   @/dir_101/       --> @/dir_103/dir_101
		ROW("dir_100",	NULL,	    "fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

	// f1.txt is not an orphan, we just can't resolve it's place in the tree until the conflict on the parent directory is resolved.

//	SG_mrg_cset_stats csetStats = { ?, ?, ?, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test503_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// create a mess.  move directory dir_102 into dir_103 in one branch
	// and move dir_103 into dir_102 in the other branch.
	// this should cause both directories to be marked as conflicts without
	// problem, but it should cause the SG_mrg_cset_dir stuff fits....
	//
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_103",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_102","dir_103",	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// move   @/dir_102/       --> @/dir_103/
		ROW("dir_100",	NULL,	    "fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_103","dir_102",	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// move   @/dir_103/       --> @/dir_102/
		ROW("dir_100",	NULL,	    "fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

	// f1.txt is not an orphan, we just can't resolve it's place in the tree until the conflict on the parent directory is resolved.

//	SG_mrg_cset_stats csetStats = { ?, ?, ?, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test503_1)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// create a deeper mess.  move directory dir_102 into dir_103 in one branch
	// and move dir_103 into dir_102 in the other branch.
	// this should cause both directories to be marked as conflicts without
	// problem, but it should cause the SG_mrg_cset_dir stuff fits....
	//
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",			NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102/aa",		NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102/aa/bb",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_103",			NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_103/zz",		NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_103/zz/yy",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_102","dir_103/zz/yy",	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// move   @/dir_102/       --> @/dir_103/zz/yy/
		ROW("dir_100",	NULL,	    	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_103","dir_102/aa/bb",	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// move   @/dir_103/       --> @/dir_102/aa/bb
		ROW("dir_100",	NULL,	    	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

	// dir_102 and dir_103 are children of each other...

//	SG_mrg_cset_stats csetStats = { ?, ?, ?, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test503_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// create a deeper mess.  move directory dir_102 into dir_103 in one branch
	// and move dir_103 into dir_102 in the other branch.
	// this should cause both directories to be marked as conflicts without
	// problem, but it should cause the SG_mrg_cset_dir stuff fits....
	//
	// we need to add f[BC] just to cause the cset's HIDs to be different
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",			NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",			NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",			NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",			NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102/aa",		NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102/aa/bb",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_103",			NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_103/zz",		NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_103/zz/yy",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_102/aa/bb",	NULL,				NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),	// rmdir @/dir_102/aa/bb/
		ROW("dir_102",			"dir_103/zz/yy",	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// move   @/dir_102/       --> @/dir_103/zz/yy/
		ROW("dir_100",			NULL,		    	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_103/zz/yy",	NULL,				NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),	// rmdir @/dir_103/zz/yy
		ROW("dir_103/zz",		NULL,				NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__DELETED),		0,		NULL),	// rmdir @/dir_103/zz
		ROW("dir_103",			"dir_102/aa/bb",	NULL,			NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// move   @/dir_103/       --> @/dir_102/aa/bb
		ROW("dir_100",			NULL,		    	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

	// dir_102 and dir_103 are children of each other...

//	SG_mrg_cset_stats csetStats = { ?, ?, ?, 2, 1 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_C, nrRows_Table_C,
				 Table_B, nrRows_Table_B,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif//DO_500

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

#if DO_600
static void MyFn(test600_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// create some real entryname collisions
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"fdup.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fdup.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { ... };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test601_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// create some potential entryname collisions
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"FDUP.TXT",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fdup.txt",	NULL,			(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { ... };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test602_0)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// create a divergent rename and some real entryname collisions.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f2.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		"f_b.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		"f_c.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"f2.txt",		"f1.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { ... };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test602_1)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// create a divergent rename and some real entryname collisions.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f2.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		"f_b.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		"f_c.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"f2.txt",		"f_b.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { ... };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test602_2)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// TODO 2010/06/24 This test currently fails when bUseSwitchBaseline is true (DO_USE_UPDATE)
	// TODO            because sg_pendingtree__update throws not-implemented due to a portability
	// TODO            warning/issue not being handled.  SPAWL-915.

	// create a divergent rename and some real entryname collisions.
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f2.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",		"f_b.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"F1.TXT",		NULL,			(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"f1.txt",		"f_c.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"f2.txt",		"f1.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

//	SG_mrg_cset_stats csetStats = { ... };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif//DO_600

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

#if DO_700
static void MyFn(test700)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// make 2 orthogonal/conflict-free changes that may mess us up as we populate the WD
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_102",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f1.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f2.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	NULL,	"f1.txt",	"swap.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"f2.txt",	"f1.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"swap.txt",	"f2.txt",		(SG_DIFFSTATUS_FLAGS__RENAMED),		0,		NULL),
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

	// there should be no conflicts here,
	// *BUT* when we actually apply the merge to the (clean) WD (assuming that we start with CSET_C
	// as the baseline and use the pendingtree code to modify the WD to reflect the results of the
	// merge), we may run into a transient collision as we modify the WD (but all that is under the
	// hood and has nothing to do with our transient "swap.txt" file (which the merge code won't ever
	// see).

//	SG_mrg_cset_stats csetStats = { 4, 1, 0, 0, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}

static void MyFn(test701)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	// make 2 orthogonal/conflict-free changes that may mess us up as we populate the WD
	//////////////////////////////////////////////////////////////////
	// table used to create CSET_A from CSET_I.

	struct MyDcl(row) Table_A[] = {
		ROW("dir_100",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_swap",	NULL,		NULL,		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_100",	NULL,	"f.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
		ROW("dir_101",	NULL,	"f.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_A = SG_NrElements( Table_A );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_B from CSET_A.

	struct MyDcl(row) Table_B[] = {
		ROW("dir_100",	"dir_swap",	"f.txt",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// mv dir_100/f.txt --> dir_swap/f.txt
		ROW("dir_101",	"dir_100",	"f.txt",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// mv dir_101/f.txt --> dir_100/f.txt
		ROW("dir_swap",	"dir_101",	"f.txt",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED),		0,		NULL),	// mv dir_swap/f.txt --> dir_100/f.txt
		ROW("dir_100",	NULL,	"fB.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_B = SG_NrElements( Table_B );

	//////////////////////////////////////////////////////////////////
	// table used to create CSET_C from CSET_A.

	struct MyDcl(row) Table_C[] = {
		ROW("dir_100",	NULL,	"fC.txt",		NULL,		(SG_DIFFSTATUS_FLAGS__ADDED),		0,		NULL),
	};

	SG_uint32 nrRows_Table_C = SG_NrElements( Table_C );

	//////////////////////////////////////////////////////////////////

	// there should be no conflicts here,
	// *BUT* when we actually apply the merge to the (clean) WD (assuming that we start with CSET_C
	// as the baseline and use the pendingtree code to modify the WD to reflect the results of the
	// merge), we may run into a transient collision as we modify the WD (but all that is under the
	// hood and has nothing to do with our transient "swap" directory (which the merge code will see
	// as unchanged).

//	SG_mrg_cset_stats csetStats = { 4, 1, 0, 0, 0 };

	//////////////////////////////////////////////////////////////////

	MyFn(doCase)(pCtx,
				 pPathTopDir,
				 Table_A, nrRows_Table_A,
				 Table_B, nrRows_Table_B,
				 Table_C, nrRows_Table_C,
//				 &csetStats,
				 bUseSwitchBaseline);
}
#endif//DO_700

//////////////////////////////////////////////////////////////////

static void MyFn(do_main)(SG_context * pCtx, const SG_pathname * pPathTopDir, SG_bool bUseSwitchBaseline)
{
	//////////////////////////////////////////////////////////////////
	// simple orthogonal changes with a clean WD.

#if DO_000
	BEGIN_TEST(  MyFn(test000  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test001  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test002_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test002_1)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test003_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test003_1)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test004_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test004_1)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#if AA
	BEGIN_TEST(  MyFn(test005_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test005_1)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test005_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test005_3)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif
#if AB
	BEGIN_TEST(  MyFn(test006_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test006_1)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test006_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test006_3)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif
	BEGIN_TEST(  MyFn(test007_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test007_1)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#if SYM
	BEGIN_TEST(  MyFn(test008_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif

#else
	INFOP("DO_000",("Skipping test0*"));
#endif//DO_000

	//////////////////////////////////////////////////////////////////
	// identical changes in multiple branches

#if DO_100
	BEGIN_TEST(  MyFn(test100  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test101  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test102  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#if AA
	BEGIN_TEST(  MyFn(test103_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test103_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif
#if AB
	BEGIN_TEST(  MyFn(test104_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test104_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif
	BEGIN_TEST(  MyFn(test105  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#if SYM
	BEGIN_TEST(  MyFn(test106  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test107  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test108  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif

#else
	INFOP("DO_100",("Skipping test1*"));
#endif//DO_100

	//////////////////////////////////////////////////////////////////
	// orthogoonal changes on an entry in different branches (e.g. a rename and a chmod)

#if DO_200
	BEGIN_TEST(  MyFn(test200  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test201  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#if AA
	BEGIN_TEST(  MyFn(test202_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test202_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif
#if AB
	BEGIN_TEST(  MyFn(test203_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test203_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif
	BEGIN_TEST(  MyFn(test204_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test204_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test204_4)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#else
	INFOP("DO_200",("Skipping test2*"));
#endif//DO_200

	//////////////////////////////////////////////////////////////////
	// a delete vs some other change should cause a conflict.

#if DO_300
	BEGIN_TEST(  MyFn(test300  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test301  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#if AA
	BEGIN_TEST(  MyFn(test302_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test302_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif
#if AB
	BEGIN_TEST(  MyFn(test303_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test303_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif
	BEGIN_TEST(  MyFn(test304_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test304_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test304_4)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#if SYM
	BEGIN_TEST(  MyFn(test305  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif

#else
	INFOP("DO_300",("Skipping test3*"));
#endif//DO_300

	//////////////////////////////////////////////////////////////////
	// a divergent change that should cause a conflict.
	// (for example, 2 renames to different names)

#if DO_400
	BEGIN_TEST(  MyFn(test401  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test402  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#if AA
	BEGIN_TEST(  MyFn(test403_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test403_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test403_4)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif
#if AB
//	BEGIN_TEST(  MyFn(test404_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
//	BEGIN_TEST(  MyFn(test404_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif
	BEGIN_TEST(  MyFn(test405_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test405_1)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#if SYM
	BEGIN_TEST(  MyFn(test406  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#endif

#else
	INFOP("DO_400",("Skipping test4*"));
#endif//DO_400

	//////////////////////////////////////////////////////////////////
	// create some orphans and some pathname-cycles

#if DO_500
	BEGIN_TEST(  MyFn(test500_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test500_1)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test501  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test502  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test503_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test503_1)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test503_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#else
	INFOP("DO_500",("Skipping test5*"));
#endif//DO_500

	//////////////////////////////////////////////////////////////////
	// entryname collisions and portability issues

#if DO_600
	BEGIN_TEST(  MyFn(test600_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test601_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test602_0)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test602_1)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test602_2)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#else
	INFOP("DO_600",("Skipping test6*"));
#endif//DO_600

	//////////////////////////////////////////////////////////////////
	// potential problems when changing the contents of the WD (baseline) to reflect the merge result.

#if DO_700
	BEGIN_TEST(  MyFn(test700  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );
	BEGIN_TEST(  MyFn(test701  )(pCtx, pPathTopDir, bUseSwitchBaseline)  );

#else
	INFOP("DO_700",("Skipping test7*"));
#endif//DO_700
}

//////////////////////////////////////////////////////////////////

MyMain()
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;
	SG_bool bUseSwitchBaseline;

	TEMPLATE_MAIN_START;

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufTopDir, sizeof(bufTopDir), 6)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathTopDir,bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir_recursive__pathname(pCtx, pPathTopDir)  );

#if DO_USE_NEW_WD
	bUseSwitchBaseline = SG_FALSE;
	BEGIN_TEST(  MyFn(do_main)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#else
	INFOP("DO_USE_NEW_WD",("Skipping tests using second WD"));
#endif

#if DO_USE_UPDATE
	bUseSwitchBaseline = SG_TRUE;
	BEGIN_TEST(  MyFn(do_main)(pCtx, pPathTopDir, bUseSwitchBaseline)  );
#else
	INFOP("DO_USE_UPDATE",("Skipping tests using UPDATE"));
#endif

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);

	TEMPLATE_MAIN_END;
}

//////////////////////////////////////////////////////////////////

#undef MyMain
#undef MyDcl
#undef MyFn
#undef AA
#undef AB
#undef SYM
