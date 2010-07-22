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

//////////////////////////////////////////////////////////////////

#include <sg.h>
#include "unittests.h"
#include "unittests_pendingtree.h"

//////////////////////////////////////////////////////////////////
// we define a little trick here to prefix all global symbols (type,
// structures, functions) with our test name.  this allows all of
// the tests in the suite to be #include'd into one meta-test (without
// name collisions) when we do a GCOV run.

#define MyMain()				TEST_MAIN(u0075_pendingtree_log)
#define MyDcl(name)				u0075_pendingtree_log__##name
#define MyFn(name)				u0075_pendingtree_log__##name

// only bother with xattr and attrbits tests on Mac and Linux.
//
// TODO split this "AA" into 2 flags -- one for XATTRs and one for ATTRBITS.
// TODO and allow the attrbits tests on mac and linux.
// TODO the xattr tests should still be controlled by the build flag.

#ifdef SG_BUILD_FLAG_TEST_XATTR
#	define AA 1
#else
#	define AA 0
#endif


//////////////////////////////////////////////////////////////////

static void MyFn(do_commit)(SG_context * pCtx,
							const SG_pathname * pPathWorkingDir)
{
	SG_pendingtree* pPendingTree = NULL;
    SG_repo* pRepo = NULL;
    SG_audit q;

	// actually do the commit
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
    VERIFY_ERR_CHECK(  SG_pendingtree__get_repo(pCtx, pPendingTree, &pRepo)  );
    VERIFY_ERR_CHECK(  SG_audit__init(pCtx,&q,pRepo,SG_AUDIT__WHEN__NOW, SG_AUDIT__WHO__FROM_SETTINGS)  );
	VERIFY_ERR_CHECK(  unittests_pendingtree__commit(pCtx, pPendingTree, &q, NULL, 0, NULL, NULL, 0, NULL, 0, NULL, 0, NULL)  );

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

//////////////////////////////////////////////////////////////////

struct MyDcl(ctm__data)
{
	SG_int32				sum;
	const char *			szLabel;
};

static SG_treediff2_foreach_callback MyFn(ctm_cb);

static void MyFn(ctm_cb)(SG_context * pCtx,
						 SG_UNUSED_PARAM(SG_treediff2 * pTreeDiff),
						 SG_UNUSED_PARAM(const char * szGidObject),
						 const SG_treediff2_ObjectData * pOD_opaque,
						 void * pVoidCallerData)
{
	struct MyDcl(ctm__data) * pData = (struct MyDcl(ctm__data) *)pVoidCallerData;
	const char * szRepoPath;
	SG_diffstatus_flags dsFlags;

	SG_UNUSED(pTreeDiff);
	SG_UNUSED(szGidObject);

	pData->sum++;

	VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_repo_path(pCtx, pOD_opaque,&szRepoPath)  );
	VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD_opaque,&dsFlags)  );

	VERIFYP_COND(pData->szLabel,(0),("Entry [repo-path %s][dsFlags %x] not accounted for in test.",szRepoPath,dsFlags));

	return;

fail:
	return;
}

/**
 * this is used after we have checked the treediff answers against the expected set of answers in XA or YA.
 * each time an answer is checked, the entry is marked with a 1.  after we have checked them all, we look
 * to see if there are any entries with marker value 0 remaining.
 *
 * Anything that we find implied that the treediff included something that we didn't expect (or that our
 * answer sheet is incomplete).
 */
static void MyFn(check_treediff_markers)(SG_context * pCtx,
										 SG_treediff2 * pTreeDiff, const char * szLabel)
{
	struct MyDcl(ctm__data) data;

	data.sum = 0;
	data.szLabel = szLabel;

	SG_treediff2__foreach_with_marker_value(pCtx, pTreeDiff,0,MyFn(ctm_cb),&data);
}

//////////////////////////////////////////////////////////////////

void MyFn(do_report_action_log)(SG_context * pCtx, SG_pendingtree* pPendingTree)
{
    SG_varray* pva;				// we DO NOT own this
    SG_vhash* pvh;				// we DO NOT own this
    SG_string* pstr = NULL;
    SG_uint32 count;
    SG_uint32 i;

    SG_ERR_CHECK(  SG_pendingtree__get_action_log(pCtx, pPendingTree, &pva)  );
	if (pva)
	{
		SG_ERR_CHECK(  SG_varray__count(pCtx, pva, &count)  );

		SG_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pstr)  );
		for (i=0; i<count; i++)
		{
			SG_ERR_CHECK(  SG_varray__get__vhash(pCtx, pva, i, &pvh)  );
			SG_ERR_CHECK(  SG_string__clear(pCtx, pstr)  );
			SG_ERR_CHECK(  SG_vhash__to_json(pCtx, pvh, pstr)  );
			SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR ,"%s\n", SG_string__sz(pstr))  );
		}
	}

fail:
    SG_STRING_NULLFREE(pCtx, pstr);
}


void MyFn(do_revert_log)(SG_context * pCtx, SG_pathname * pPathWorkingDir, const char * pszLabel)
{
	SG_pendingtree_action_log_enum eActionLog;
	SG_pendingtree * pPendingTree = NULL;

	SG_ERR_IGNORE(  SG_console(pCtx, SG_CS_STDERR ,"%s\n", pszLabel)  );

	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	eActionLog = SG_PT_ACTION__LOG_IT;
	VERIFY_ERR_CHECK(  SG_pendingtree__revert(pCtx, pPendingTree, eActionLog,
											  NULL,
											  0, NULL,
											  SG_TRUE,
											  NULL, 0,
											  NULL, 0)  );
	VERIFY_ERR_CHECK(  MyFn(do_report_action_log)(pCtx, pPendingTree)  );

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

//////////////////////////////////////////////////////////////////

void MyFn(do_verify_y_answer)(SG_context * pCtx, SG_pathname * pPathWorkingDir)
{
	SG_treediff2 * pTreeDiff_check = NULL;
	SG_pendingtree * pPendingTree = NULL;
	SG_uint32 kRow;
	const SG_treediff2_ObjectData * pOD_opaque_check;
	const char * szGidObject_check;
	SG_diffstatus_flags dsFlags_check;
	SG_int64 attrbits_check;
	const char * szHidXAttrs_check;
	const char * szOldName_check;
	const char * szOldParentRepoPath_check;
	SG_bool bFound;

	struct _row_y_answer
	{
		const char *			szRepoPath;
		const char *			szOldName_when_rename;
		const char *			szOldParentRepoPath_when_moved;
		SG_diffstatus_flags		dsFlags;
		SG_bool					bAttrBitsNonZero;
		SG_bool					bXAttrsNonZero;
	};

#define YA(rp,nm,prp,ds,bA,bX)	{ rp, nm, prp, ds, bA, bX }
#define YA_ADD(rp)				{ rp, NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_FALSE }
#define YA_DEL(rp)				{ rp, NULL, NULL, SG_DIFFSTATUS_FLAGS__DELETED, SG_FALSE, SG_FALSE }
#define YA_MOD(rp)				{ rp, NULL, NULL, SG_DIFFSTATUS_FLAGS__MODIFIED, SG_FALSE, SG_FALSE }
#define YA_REN(rp,nm)			{ rp, nm, NULL, SG_DIFFSTATUS_FLAGS__RENAMED, SG_FALSE, SG_FALSE }
#define YA_MOV(rp,prp)			{ rp, NULL, prp, SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE }

#define YA_MOD_REN(rp,nm)		{ rp, nm, NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED, SG_FALSE, SG_FALSE }
#define YA_MOD_MOV(rp,prp)		{ rp, NULL, prp, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE }

	struct _row_y_answer aRowYA[] = {

		// these rows correspond to the effects produced by the aRowY table above.
		// they are not necessarily line-for-line because some lines in aRowY may
		// do more than one thing (such as recursive delete) or many lines may act
		// on one object (such as sequence of moves and renames on one entry).
		//
		// these are for the cset1-vs-pendingtree (before cset2 is created).

		YA_MOD( "@/" ),

		YA_DEL( "@/d0000/f0000" ),
		YA_DEL( "@/d0000/" ),

		YA_ADD( "@/dAdd1/" ),
		YA_ADD( "@/dAdd2/" ),
		YA_ADD( "@/dAdd3/" ),
		YA_ADD( "@/dAdd4/" ),
		YA_ADD( "@/dAdd5/" ),
		YA_ADD( "@/dAdd6/" ),
		YA_ADD( "@/dAdd7/" ),
		YA_ADD( "@/dAdd8/" ),
		YA_ADD( "@/dAdd9/" ),

		YA_ADD( "@/dAdd10/" ),
		YA_ADD( "@/dAdd10/fAdd10" ),
		YA_ADD( "@/dAdd11/" ),
		YA_ADD( "@/dAdd11/fAdd11" ),
		YA_ADD( "@/dAdd12/" ),
		YA_ADD( "@/dAdd12/fAdd12" ),
		YA_ADD( "@/dAdd13/" ),
		YA_ADD( "@/dAdd13/fAdd13" ),
		YA_ADD( "@/dAdd13_M/" ),
		YA_ADD( "@/dAdd14/" ),
		YA_ADD( "@/dAdd14/fAdd14" ),
		YA_ADD( "@/dAdd14_M/" ),
		YA_ADD( "@/dAdd15/" ),
		YA_ADD( "@/dAdd15/fAdd15" ),
		YA_ADD( "@/dAdd15_M/" ),
		YA_ADD( "@/dAdd16/" ),
		YA_ADD( "@/dAdd16/fAdd16" ),
		YA_ADD( "@/dAdd17/" ),
		YA_ADD( "@/dAdd17/fAdd17" ),
		YA_ADD( "@/dAdd18/" ),
		YA_ADD( "@/dAdd18/fAdd18" ),
		YA_ADD( "@/dAdd19/" ),
		YA_ADD( "@/dAdd19/fAdd19" ),

#if AA
		YA_ADD( "@/aAdd20/" ),
		YA(     "@/aAdd20/fAdd20", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),		// an ADD with attrs just shows up as an _ADD, it does not have _CHANGED_ set.
		YA_ADD( "@/aAdd21/" ),
		YA(     "@/aAdd21/fAdd21", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),
		YA_ADD( "@/aAdd22/" ),
		YA(     "@/aAdd22/fAdd22", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),
		YA_ADD( "@/aAdd23/" ),
		YA(     "@/aAdd23/fAdd23", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),
		YA_ADD( "@/aAdd24/" ),
		YA(     "@/aAdd24/fAdd24", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_FALSE, SG_TRUE ),

		YA_ADD( "@/aAdd25/" ),
		YA(     "@/aAdd25/fAdd25", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
		YA_ADD( "@/aAdd26/" ),
		YA(     "@/aAdd26/fAdd26", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
		YA_ADD( "@/aAdd27/" ),
		YA(     "@/aAdd27/fAdd27", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
		YA_ADD( "@/aAdd28/" ),
		YA(     "@/aAdd28/fAdd28", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),
		YA_ADD( "@/aAdd29/" ),
		YA(     "@/aAdd29/fAdd29", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_FALSE ),

		YA_ADD( "@/aAdd30/" ),
		YA(     "@/aAdd30/fAdd30", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
		YA_ADD( "@/aAdd31/" ),
		YA(     "@/aAdd31/fAdd31", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
		YA_ADD( "@/aAdd32/" ),
		YA(     "@/aAdd32/fAdd32", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
		YA_ADD( "@/aAdd33/" ),
		YA(     "@/aAdd33/fAdd33", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
		YA_ADD( "@/aAdd34/" ),
		YA(     "@/aAdd34/fAdd34", NULL, NULL, SG_DIFFSTATUS_FLAGS__ADDED, SG_TRUE, SG_TRUE ),
#endif

		YA_MOD( "@/d2000/" ),
		YA_MOD( "@/d2000/f2000" ),
		YA_MOD( "@/d2001/" ),
		YA_MOD( "@/d2001/f2001" ),
		YA_MOD( "@/d2002/" ),
		YA_MOD( "@/d2002/f2002" ),
		YA_MOD( "@/d2003/" ),
		YA_MOD( "@/d2003/f2003" ),
		YA_MOD( "@/d2004/" ),
		YA_MOD( "@/d2004/f2004" ),
		YA_ADD( "@/d2004_M/" ),
		YA_MOD( "@/d2005/" ),
		YA_MOD( "@/d2005/f2005" ),
		YA_MOD( "@/d2006/" ),
		YA_MOD( "@/d2006/f2006" ),
		YA_MOD( "@/d2007/" ),
		YA_MOD( "@/d2007/f2007" ),
		YA_MOD( "@/d2008/" ),
		YA_MOD( "@/d2008/f2008" ),
		YA_MOD( "@/d2009/" ),
		YA_MOD( "@/d2009/f2009" ),
		YA_MOD( "@/d2010/" ),
		YA_MOD( "@/d2010/f2010" ),
		YA_MOD( "@/d2011/" ),
		YA_MOD( "@/d2011/f2011" ),
		YA_MOD( "@/d2012/" ),
		YA_MOD( "@/d2012/f2012" ),
		YA_MOD( "@/d2013/" ),
		YA_MOD( "@/d2013/f2013" ),
		YA_MOD( "@/d2014/" ),
		YA_MOD( "@/d2014/f2014" ),
		YA_MOD( "@/d2015/" ),
		YA_MOD( "@/d2015/f2015" ),
		YA_MOD( "@/d2016/" ),
		YA_MOD( "@/d2016/f2016" ),
		YA_MOD( "@/d2017/" ),
		YA_MOD( "@/d2017/f2017" ),
		YA_MOD( "@/d2018/" ),
		YA_MOD( "@/d2018/f2018" ),
		YA_MOD( "@/d2019/" ),
		YA_MOD( "@/d2019/f2019" ),
		YA_MOD( "@/d2020/" ),
		YA_MOD( "@/d2020/f2020" ),

		YA_REN( "@/d1000Ren/", "d1000" ),
		YA_REN( "@/d1001Ren/", "d1001" ),
		YA_REN( "@/d1002Ren/", "d1002" ),
		YA_REN( "@/d1003Ren/", "d1003" ),
		YA_REN( "@/d1004Ren/", "d1004" ),

		YA_MOD( "@/d1005/" ),
		YA_REN( "@/d1005/f1005Ren", "f1005" ),
		YA_MOD( "@/d1006/" ),
		YA_REN( "@/d1006/f1006Ren", "f1006" ),
		YA_MOD( "@/d1007/" ),
		YA_REN( "@/d1007/f1007Ren", "f1007" ),
		YA_MOD( "@/d1008/" ),
		YA_REN( "@/d1008/f1008Ren", "f1008" ),
		YA_MOD( "@/d1009/" ),
		YA_REN( "@/d1009/f1009Ren", "f1009" ),

		YA_MOD_REN( "@/d1010Ren/",         "d1010" ),		// dir is modified because stuff within it changed too.
		YA_REN(     "@/d1010Ren/f1010Ren", "f1010" ),
		YA_MOD_REN( "@/d1011Ren/",         "d1011" ),
		YA_REN(     "@/d1011Ren/f1011Ren", "f1011" ),
		YA_MOD_REN( "@/d1012Ren/",         "d1012" ),
		YA_REN(     "@/d1012Ren/f1012Ren", "f1012" ),
		YA_MOD_REN( "@/d1013Ren/",         "d1013" ),
		YA_REN(     "@/d1013Ren/f1013Ren", "f1013" ),
		YA_MOD_REN( "@/d1014Ren/",         "d1014" ),
		YA_REN(     "@/d1014Ren/f1014Ren", "f1014" ),

		YA_MOD_REN( "@/d1015Ren/",         "d1015" ),
		YA_REN(     "@/d1015Ren/f1015Ren", "f1015" ),
		YA_MOD_REN( "@/d1016Ren/",         "d1016" ),
		YA_REN(     "@/d1016Ren/f1016Ren", "f1016" ),
		YA_MOD_REN( "@/d1017Ren/",         "d1017" ),
		YA_REN(     "@/d1017Ren/f1017Ren", "f1017" ),
		YA_MOD_REN( "@/d1018Ren/",         "d1018" ),
		YA_REN(     "@/d1018Ren/f1018Ren", "f1018" ),
		YA_MOD_REN( "@/d1019Ren/",         "d1019" ),
		YA_REN(     "@/d1019Ren/f1019Ren", "f1019" ),
		YA_MOD_REN( "@/d1020Ren/",         "d1020" ),
		YA_REN(     "@/d1020Ren/f1020Ren", "f1020" ),

		YA_MOD(     "@/d1030/" ),
		YA_MOD_REN( "@/d1030/aa_ren/",                  "aa" ),
		YA_MOD_REN( "@/d1030/aa_ren/bb_ren/",           "bb" ),
		YA_REN(     "@/d1030/aa_ren/bb_ren/cc_ren/",    "cc" ),

		YA_MOD(     "@/d1031/" ),
		YA_MOD(     "@/d1031/aa_mov/" ),
		YA_MOD_MOV( "@/d1031/aa_mov/aa/",                  "@/d1031/" ),
		YA_MOD_REN( "@/d1031/aa_mov/aa/bb_ren/",           "bb" ),
		YA_MOD(     "@/d1031/aa_mov/aa/bb_ren/cc_mov/" ),
		YA_MOV(     "@/d1031/aa_mov/aa/bb_ren/cc_mov/cc/", "@/d1031/aa_mov/aa/bb_ren/" ),

		YA_MOD(     "@/d1032/" ),
		YA_MOD(     "@/d1032/aa_mov/" ),
		YA_MOD_MOV( "@/d1032/aa_mov/aa/",                  "@/d1032/" ),
		YA_MOD_REN( "@/d1032/aa_mov/aa/bb_ren/",           "bb" ),
		YA_MOD(     "@/d1032/aa_mov/aa/bb_ren/cc_mov/" ),
		YA_MOV(     "@/d1032/aa_mov/aa/bb_ren/cc_mov/cc/", "@/d1032/aa_mov/aa/bb_ren/" ),

		YA_MOD(     "@/d1033/" ),
		YA_MOD(     "@/d1033/aa_mov/" ),
		YA_MOD_MOV( "@/d1033/aa_mov/aa/",                            "@/d1033/" ),
		YA_MOD_REN( "@/d1033/aa_mov/aa/bb_ren/",                     "bb" ),
		YA_MOD(     "@/d1033/aa_mov/aa/bb_ren/cc_mov/" ),
		YA_MOD_MOV( "@/d1033/aa_mov/aa/bb_ren/cc_mov/cc/",           "@/d1033/aa_mov/aa/bb_ren/" ),
		YA_MOD(     "@/d1033/aa_mov/aa/bb_ren/cc_mov/cc/dd/" ),
		YA_MOD(     "@/d1033/aa_mov/aa/bb_ren/cc_mov/cc/dd/ee/" ),
		YA_MOD(     "@/d1033/aa_mov/aa/bb_ren/cc_mov/cc/dd/ee/f1033" ),

		YA_MOD( "@/d3000/" ),
		YA_MOD( "@/d3000Mov/" ),
		YA_MOV( "@/d3000Mov/f3000", "@/d3000/" ),
		YA_MOD( "@/d3001/" ),
		YA_MOD( "@/d3001Mov/" ),
		YA_MOV( "@/d3001Mov/f3001", "@/d3001/" ),
		YA_MOD( "@/d3002/" ),
		YA_MOD( "@/d3002Mov/" ),
		YA_MOV( "@/d3002Mov/f3002", "@/d3002/" ),
		YA_MOD( "@/d3003/" ),
		YA_MOD( "@/d3003Mov/" ),
		YA_MOV( "@/d3003Mov/f3003", "@/d3003/" ),
		YA_MOD( "@/d3004/" ),
		YA_MOD( "@/d3004Mov/" ),
		YA_MOV( "@/d3004Mov/f3004", "@/d3004/" ),
		YA_MOD( "@/d3005/" ),
		YA_MOD( "@/d3005Mov/" ),
		YA_MOV( "@/d3005Mov/f3005", "@/d3005/" ),
		YA_MOD( "@/d3006/" ),
		YA_MOD( "@/d3006Mov/" ),
		YA_MOV( "@/d3006Mov/f3006", "@/d3006/" ),
		YA_MOD( "@/d3007/" ),
		YA_MOD( "@/d3007Mov/" ),
		YA_MOV( "@/d3007Mov/f3007", "@/d3007/" ),
		YA_MOD( "@/d3008/" ),
		YA_MOD( "@/d3008Mov/" ),
		YA_MOV( "@/d3008Mov/f3008", "@/d3008/" ),
		YA_MOD( "@/d3009/" ),
		YA_MOD( "@/d3009Mov/" ),
		YA_MOV( "@/d3009Mov/f3009", "@/d3009/" ),

#if AA
		YA_MOD( "@/a4000/" ),
		YA(     "@/a4000/f4000", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),	// xattr should now be non-zero
		YA_MOD( "@/a4001/" ),
		YA(     "@/a4001/f4001", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA_MOD( "@/a4002/" ),
		YA(     "@/a4002/f4002", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA_MOD( "@/a4003/" ),
		YA(     "@/a4003/f4003", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA_MOD( "@/a4004/" ),
		YA(     "@/a4004/f4004", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		YA( "@/a4005/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA( "@/a4006/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA( "@/a4007/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA( "@/a4008/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),
		YA( "@/a4009/",      NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		YA_MOD( "@/a4010/" ),
		YA(     "@/a4010/f4010", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),	// attrbits should now be non-zero
		YA_MOD( "@/a4011/" ),
		YA(     "@/a4011/f4011", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),
		YA_MOD( "@/a4012/" ),
		YA(     "@/a4012/f4012", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),
		YA_MOD( "@/a4013/" ),
		YA(     "@/a4013/f4013", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),
		YA_MOD( "@/a4014/" ),
		YA(     "@/a4014/f4014", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS, SG_TRUE, SG_FALSE ),

		YA_MOD( "@/a4015/" ),
		YA(     "@/a4015/f4015", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a4016/" ),
		YA(     "@/a4016/f4016", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a4017/" ),
		YA(     "@/a4017/f4017", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a4018/" ),
		YA(     "@/a4018/f4018", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a4019/" ),
		YA(     "@/a4019/f4019", NULL, NULL, SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),

		YA_MOD( "@/a5000/" ),
		YA(     "@/a5000/f5000Ren", "f5000", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5001/" ),
		YA(     "@/a5001/f5001Ren", "f5001", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5002/" ),
		YA(     "@/a5002/f5002Ren", "f5002", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5003/" ),
		YA(     "@/a5003/f5003Ren", "f5003", NULL, SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,                                       SG_TRUE,  SG_FALSE ),
		YA_MOD( "@/a5004/" ),
		YA(     "@/a5004/f5004Ren", "f5004", NULL, SG_DIFFSTATUS_FLAGS__RENAMED                                         | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE  ),

		YA_MOD( "@/a5005/"    ),
		YA_MOD( "@/a5005Mov/" ),
		YA(     "@/a5005Mov/f5005", NULL, "@/a5005/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5006/"    ),
		YA_MOD( "@/a5006Mov/" ),
		YA(     "@/a5006Mov/f5006", NULL, "@/a5006/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5007/"    ),
		YA_MOD( "@/a5007Mov/" ),
		YA(     "@/a5007Mov/f5007", NULL, "@/a5007/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5008/"    ),
		YA_MOD( "@/a5008Mov/" ),
		YA(     "@/a5008Mov/f5008", NULL, "@/a5008/", SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,                                       SG_TRUE,  SG_FALSE ),
		YA_MOD( "@/a5009/"    ),
		YA_MOD( "@/a5009Mov/" ),
		YA(     "@/a5009Mov/f5009", NULL, "@/a5009/", SG_DIFFSTATUS_FLAGS__MOVED                                         | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE  ),

		YA_MOD( "@/a5010/"    ),
		YA_MOD( "@/a5010Mov/" ),
		YA(     "@/a5010Mov/f5010Ren", "f5010", "@/a5010/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5011/"    ),
		YA_MOD( "@/a5011Mov/" ),
		YA(     "@/a5011Mov/f5011Ren", "f5011", "@/a5011/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5012/"    ),
		YA_MOD( "@/a5012Mov/" ),
		YA(     "@/a5012Mov/f5012Ren", "f5012", "@/a5012/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5013/"    ),
		YA_MOD( "@/a5013Mov/" ),
		YA(     "@/a5013Mov/f5013Ren", "f5013", "@/a5013/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
		YA_MOD( "@/a5014/"    ),
		YA_MOD( "@/a5014Mov/" ),
		YA(     "@/a5014Mov/f5014Ren", "f5014", "@/a5014/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE,  SG_TRUE  ),
#endif

		YA_MOD( "@/d6000/"    ),
		YA_MOD( "@/d6000Mov/" ),
		YA(     "@/d6000Mov/f6000Ren", "f6000", "@/d6000/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d6001/"    ),
		YA_MOD( "@/d6001Mov/" ),
		YA(     "@/d6001Mov/f6001Ren", "f6001", "@/d6001/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d6002/"    ),
		YA_MOD( "@/d6002Mov/" ),
		YA(     "@/d6002Mov/f6002Ren", "f6002", "@/d6002/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d6003/"    ),
		YA_MOD( "@/d6003Mov/" ),
		YA(     "@/d6003Mov/f6003Ren", "f6003", "@/d6003/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d6004/"    ),
		YA_MOD( "@/d6004Mov/" ),
		YA(     "@/d6004Mov/f6004Ren", "f6004", "@/d6004/", SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),

#if AA
		YA_MOD( "@/a7000/" ),
		YA(     "@/a7000/f7000Ren", "f7000", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7001/" ),
		YA(     "@/a7001/f7001Ren", "f7001", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7002/" ),
		YA(     "@/a7002/f7002Ren", "f7002", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7003/" ),
		YA(     "@/a7003/f7003Ren", "f7003", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,                                       SG_TRUE, SG_FALSE ),
		YA_MOD( "@/a7004/" ),
		YA(     "@/a7004/f7004Ren", "f7004", NULL, SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED                                         | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		YA_MOD( "@/a7005/" ),
		YA_MOD( "@/a7005Mov/" ),
		YA(     "@/a7005Mov/f7005", NULL, "@/a7005/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7006/" ),
		YA_MOD( "@/a7006Mov/" ),
		YA(     "@/a7006Mov/f7006", NULL, "@/a7006/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7007/" ),
		YA_MOD( "@/a7007Mov/" ),
		YA(     "@/a7007Mov/f7007", NULL, "@/a7007/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7008/" ),
		YA_MOD( "@/a7008Mov/" ),
		YA(     "@/a7008Mov/f7008", NULL, "@/a7008/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS,                                       SG_TRUE, SG_FALSE ),
		YA_MOD( "@/a7009/" ),
		YA_MOD( "@/a7009Mov/" ),
		YA(     "@/a7009Mov/f7009", NULL, "@/a7009/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__MOVED                                         | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_FALSE, SG_TRUE ),

		YA_MOD( "@/a7010/" ),
		YA_MOD( "@/a7010Mov/" ),
		YA(     "@/a7010Mov/f7010Ren", "f7010", "@/a7010/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7011/" ),
		YA_MOD( "@/a7011Mov/" ),
		YA(     "@/a7011Mov/f7011Ren", "f7011", "@/a7011/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7012/" ),
		YA_MOD( "@/a7012Mov/" ),
		YA(     "@/a7012Mov/f7012Ren", "f7012", "@/a7012/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7013/" ),
		YA_MOD( "@/a7013Mov/" ),
		YA(     "@/a7013Mov/f7013Ren", "f7013", "@/a7013/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
		YA_MOD( "@/a7014/" ),
		YA_MOD( "@/a7014Mov/" ),
		YA(     "@/a7014Mov/f7014Ren", "f7014", "@/a7014/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED | SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS | SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS, SG_TRUE, SG_TRUE ),
#endif

		YA_MOD( "@/d8000/" ),
		YA_MOD( "@/d8000Mov/" ),
		YA(     "@/d8000Mov/f8000Ren", "f8000", "@/d8000/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d8001/" ),
		YA_MOD( "@/d8001Mov/" ),
		YA(     "@/d8001Mov/f8001Ren", "f8001", "@/d8001/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d8002/" ),
		YA_MOD( "@/d8002Mov/" ),
		YA(     "@/d8002Mov/f8002Ren", "f8002", "@/d8002/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d8003/" ),
		YA_MOD( "@/d8003Mov/" ),
		YA(     "@/d8003Mov/f8003Ren", "f8003", "@/d8003/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
		YA_MOD( "@/d8004/" ),
		YA_MOD( "@/d8004Mov/" ),
		YA(     "@/d8004Mov/f8004Ren", "f8004", "@/d8004/", SG_DIFFSTATUS_FLAGS__MODIFIED | SG_DIFFSTATUS_FLAGS__RENAMED | SG_DIFFSTATUS_FLAGS__MOVED, SG_FALSE, SG_FALSE ),
	};

#undef YA
#undef YA_ADD
#undef YA_DEL
#undef YA_REN
#undef YA_MOV
#undef YA_MOD
#undef YA_MOD_REN
#undef YA_MOD_MOV

	SG_uint32 nrRowsYA = SG_NrElements(aRowYA);


	// verify that the pendingtree contains everything that we think it should (and nothing that it shouldn't).
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree,NULL,&pTreeDiff_check)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	for (kRow=0; kRow < nrRowsYA; kRow++)
	{
		struct _row_y_answer * pRowYA = &aRowYA[kRow];

		VERIFY_ERR_CHECK(  SG_treediff2__find_by_repo_path(pCtx, pTreeDiff_check,pRowYA->szRepoPath,&bFound,&szGidObject_check,&pOD_opaque_check)  );
		VERIFYP_COND("Row Y check",(bFound),("RowYA[%d] [repo-path %s] not found in pendingtree as expected.",kRow,pRowYA->szRepoPath));
		if (bFound)
		{
			//INFOP("do_verify_y_answer",("Found RowYA[%d] [repo-path %s]",kRow,pRowYA->szRepoPath));

			VERIFY_ERR_CHECK(  SG_treediff2__set_mark(pCtx, pTreeDiff_check,szGidObject_check,1,NULL)  );
			VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD_opaque_check,&dsFlags_check)  );
			VERIFYP_COND("Row Y check",(dsFlags_check == pRowYA->dsFlags),("RowYA[%d] [repo-path %s] [dsFlags %x] not expected [expected %x].",
																	  kRow,pRowYA->szRepoPath,dsFlags_check,pRowYA->dsFlags));

			VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_attrbits(pCtx, pOD_opaque_check,&attrbits_check)  );
			VERIFYP_COND("Row Y check",((attrbits_check != 0) == pRowYA->bAttrBitsNonZero),
					("RowYA[%d] [repo-path %s] [attrbits %d] expected [%s].",
					 kRow,pRowYA->szRepoPath,attrbits_check,((pRowYA->bAttrBitsNonZero) ? "non zero" : "zero")));

			VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_xattrs(pCtx, pOD_opaque_check,&szHidXAttrs_check)  );
			VERIFYP_COND("Row Y check",((szHidXAttrs_check != NULL) == pRowYA->bXAttrsNonZero),
					("RowYA[%d] [repo-path %s] [xattrs %s] expected [%s].",
					 kRow,pRowYA->szRepoPath,szHidXAttrs_check,((pRowYA->bXAttrsNonZero) ? "non zero" : "zero")));

			if (pRowYA->dsFlags & SG_DIFFSTATUS_FLAGS__RENAMED)
			{
				VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_old_name(pCtx, pOD_opaque_check,&szOldName_check)  );
				VERIFYP_COND("Row Y check",(strcmp(szOldName_check,pRowYA->szOldName_when_rename)==0),
						("RowYA[%d] [repo-path %s] [oldname %s] expected [%s].",
						 kRow,pRowYA->szRepoPath,szOldName_check,pRowYA->szOldName_when_rename));
			}

			if (pRowYA->dsFlags & SG_DIFFSTATUS_FLAGS__MOVED)
			{
				VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_moved_from_repo_path(pCtx, pTreeDiff_check,pOD_opaque_check,&szOldParentRepoPath_check)  );
				VERIFYP_COND("Row Y check",(strcmp(szOldParentRepoPath_check,pRowYA->szOldParentRepoPath_when_moved)==0),
						("RowYA[%d] [repo-path %s] [old parent %s] expected [%s].",
						 kRow,pRowYA->szRepoPath,szOldParentRepoPath_check,pRowYA->szOldParentRepoPath_when_moved));
			}
		}
	}
	VERIFY_ERR_CHECK_DISCARD(  MyFn(check_treediff_markers)(pCtx, pTreeDiff_check,"before_commit_y")  );
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_check);

fail:
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
}

//////////////////////////////////////////////////////////////////

void MyFn(big_revert)(SG_context * pCtx, SG_pathname* pPathTopDir)
{
	// create a changeset [X].
	// create a bunch of dirt [Y].
	//
	// verify the log produced by a "REVERT --test --all".
	// verify the log produced by a "REVERT --verbose --all".

	char bufName[SG_TID_MAX_BUFFER_LENGTH];
	char bufCSet_0[SG_HID_MAX_BUFFER_LENGTH];
	char bufCSet_1[SG_HID_MAX_BUFFER_LENGTH];
	char bufCSet_2[SG_HID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathWorkingDir = NULL;
	SG_pathname* pPathTemp = NULL;
	SG_string * pStrEntryName = NULL;
	SG_repo* pRepo = NULL;
	SG_vhash * pvhManifest_1 = NULL;
	SG_vhash * pvhManifest_2 = NULL;
	SG_vhash * pvhManifest_3 = NULL;
	SG_pendingtree * pPendingTree = NULL;
	SG_uint32 kRow;
	SG_string * pStringStatus01 = NULL;
	SG_string * pStringStatus12 = NULL;
	SG_string * pStringStatus13 = NULL;
	SG_string * pStringStatus1wd = NULL;
	SG_treediff2 * pTreeDiff_join_before_commit = NULL;
	SG_treediff2 * pTreeDiff_join_after_commit = NULL;
	SG_treediff2 * pTreeDiff_check = NULL;
	SG_string * pStr_check = NULL;
	const SG_treediff2_ObjectData * pOD_opaque_check;
	const char * szGidObject_check;
	SG_diffstatus_flags dsFlags_check;
	SG_bool bFound;

	//////////////////////////////////////////////////////////////////
	// aRowX[] -- data used to create <cset1> from the initial commit <cset0>

	struct _row_x
	{
		const char *			szDir;
		const char *			szFile;

		SG_pathname *			pPathDir;
	};

#define X(d,f)	{ d, f, NULL }

	struct _row_x aRowX[] = {
		X(	"d0000",		"f0000"	),	// add <wd>/d0000/f0000

		X(	"d1000",		"f1000"	),
		X(	"d1001",		"f1001"	),
		X(	"d1002",		"f1002"	),
		X(	"d1003",		"f1003"	),
		X(	"d1004",		"f1004"	),
		X(	"d1005",		"f1005"	),
		X(	"d1006",		"f1006"	),
		X(	"d1007",		"f1007"	),
		X(	"d1008",		"f1008"	),
		X(	"d1009",		"f1009"	),
		X(	"d1010",		"f1010"	),
		X(	"d1011",		"f1011"	),
		X(	"d1012",		"f1012"	),
		X(	"d1013",		"f1013"	),
		X(	"d1014",		"f1014"	),
		X(	"d1015",		"f1015"	),
		X(	"d1016",		"f1016"	),
		X(	"d1017",		"f1017"	),
		X(	"d1018",		"f1018"	),
		X(	"d1019",		"f1019"	),
		X(	"d1020",		"f1020"	),

		X(	"d1030",				NULL	),
		X(	"d1030/aa",				NULL	),
		X(	"d1030/aa/bb",			NULL	),
		X(	"d1030/aa/bb/cc",		NULL	),
		X(	"d1030/aa/bb/cc/dd",	NULL	),
		X(	"d1030/aa/bb/cc/dd/ee",	"f1030"	),

		X(	"d1031",				NULL	),
		X(	"d1031/aa",				NULL	),
		X(	"d1031/aa_mov",			NULL	),
		X(	"d1031/aa/bb",			NULL	),
		X(	"d1031/aa/bb/cc",		NULL	),
		X(	"d1031/aa/bb/cc_mov",	NULL	),
		X(	"d1031/aa/bb/cc/dd",	NULL	),
		X(	"d1031/aa/bb/cc/dd/ee",	"f1031"	),

		X(	"d1032",				NULL	),
		X(	"d1032/aa",				NULL	),
		X(	"d1032/aa_mov",			NULL	),
		X(	"d1032/aa/bb",			NULL	),
		X(	"d1032/aa/bb/cc",		NULL	),
		X(	"d1032/aa/bb/cc_mov",	NULL	),
		X(	"d1032/aa/bb/cc/dd",	NULL	),
		X(	"d1032/aa/bb/cc/dd/ee",	"f1032"	),

		X(	"d1033",				NULL	),
		X(	"d1033/aa",				NULL	),
		X(	"d1033/aa_mov",			NULL	),
		X(	"d1033/aa/bb",			NULL	),
		X(	"d1033/aa/bb/cc",		NULL	),
		X(	"d1033/aa/bb/cc_mov",	NULL	),
		X(	"d1033/aa/bb/cc/dd",	NULL	),
		X(	"d1033/aa/bb/cc/dd/ee",	"f1033"	),

		X(	"d2000",		"f2000"	),
		X(	"d2001",		"f2001"	),
		X(	"d2002",		"f2002"	),
		X(	"d2003",		"f2003"	),
		X(	"d2004",		"f2004"	),
		X(	"d2005",		"f2005"	),
		X(	"d2006",		"f2006"	),
		X(	"d2007",		"f2007"	),
		X(	"d2008",		"f2008"	),
		X(	"d2009",		"f2009"	),
		X(	"d2010",		"f2010"	),
		X(	"d2011",		"f2011"	),
		X(	"d2012",		"f2012"	),
		X(	"d2013",		"f2013"	),
		X(	"d2014",		"f2014"	),
		X(	"d2015",		"f2015"	),
		X(	"d2016",		"f2016"	),
		X(	"d2017",		"f2017"	),
		X(	"d2018",		"f2018"	),
		X(	"d2019",		"f2019"	),
		X(	"d2020",		"f2020"	),

		X(	"d3000",		"f3000"	),
		X(	"d3001",		"f3001"	),
		X(	"d3002",		"f3002"	),
		X(	"d3003",		"f3003"	),
		X(	"d3004",		"f3004"	),
		X(	"d3005",		"f3005"	),
		X(	"d3006",		"f3006"	),
		X(	"d3007",		"f3007"	),
		X(	"d3008",		"f3008"	),
		X(	"d3009",		"f3009"	),

		X(	"d3000Mov",		NULL	),
		X(	"d3000Mov2",	NULL	),
		X(	"d3001Mov",		NULL	),
		X(	"d3002Mov",		NULL	),
		X(	"d3003Mov",		NULL	),
		X(	"d3004Mov",		NULL	),
		X(	"d3005Mov",		NULL	),
		X(	"d3006Mov",		NULL	),
		X(	"d3007Mov",		NULL	),
		X(	"d3008Mov",		NULL	),
		X(	"d3009Mov",		NULL	),

#if AA
		X(	"a4000",		"f4000"	),
		X(	"a4001",		"f4001"	),
		X(	"a4002",		"f4002"	),
		X(	"a4003",		"f4003"	),
		X(	"a4004",		"f4004"	),
		X(	"a4005",		"f4005"	),
		X(	"a4006",		"f4006"	),
		X(	"a4007",		"f4007"	),
		X(	"a4008",		"f4008"	),
		X(	"a4009",		"f4009"	),

		X(	"a4010",		"f4010"	),
		X(	"a4011",		"f4011"	),
		X(	"a4012",		"f4012"	),
		X(	"a4013",		"f4013"	),
		X(	"a4014",		"f4014"	),
		X(	"a4015",		"f4015"	),
		X(	"a4016",		"f4016"	),
		X(	"a4016Mov",		NULL	),
		X(	"a4017",		"f4017"	),
		X(	"a4018",		"f4018"	),
		X(	"a4019",		"f4019"	),

		X(	"a5000",		"f5000"	),
		X(	"a5001",		"f5001"	),
		X(	"a5002",		"f5002"	),
		X(	"a5003",		"f5003"	),
		X(	"a5004",		"f5004"	),
		X(	"a5005",		"f5005"	),
		X(	"a5006",		"f5006"	),
		X(	"a5007",		"f5007"	),
		X(	"a5008",		"f5008"	),
		X(	"a5009",		"f5009"	),

		X(	"a5005Mov",		NULL	),
		X(	"a5006Mov",		NULL	),
		X(	"a5007Mov",		NULL	),
		X(	"a5008Mov",		NULL	),
		X(	"a5009Mov",		NULL	),

		X(	"a5010",		"f5010"	),
		X(	"a5011",		"f5011"	),
		X(	"a5012",		"f5012"	),
		X(	"a5013",		"f5013"	),
		X(	"a5014",		"f5014"	),

		X(	"a5010Mov",		NULL	),
		X(	"a5011Mov",		NULL	),
		X(	"a5012Mov",		NULL	),
		X(	"a5013Mov",		NULL	),
		X(	"a5014Mov",		NULL	),
#endif

		X(	"d6000",		"f6000"	),
		X(	"d6001",		"f6001"	),
		X(	"d6002",		"f6002"	),
		X(	"d6003",		"f6003"	),
		X(	"d6004",		"f6004"	),

		X(	"d6000Mov",		NULL	),
		X(	"d6001Mov",		NULL	),
		X(	"d6002Mov",		NULL	),
		X(	"d6003Mov",		NULL	),
		X(	"d6004Mov",		NULL	),

#if AA
		X(	"a7000",		"f7000"	),
		X(	"a7001",		"f7001"	),
		X(	"a7002",		"f7002"	),
		X(	"a7003",		"f7003"	),
		X(	"a7004",		"f7004"	),
		X(	"a7005",		"f7005"	),
		X(	"a7006",		"f7006"	),
		X(	"a7007",		"f7007"	),
		X(	"a7008",		"f7008"	),
		X(	"a7009",		"f7009"	),

		X(	"a7005Mov",		NULL	),
		X(	"a7006Mov",		NULL	),
		X(	"a7007Mov",		NULL	),
		X(	"a7008Mov",		NULL	),
		X(	"a7009Mov",		NULL	),

		X(	"a7010",		"f7010"	),
		X(	"a7011",		"f7011"	),
		X(	"a7012",		"f7012"	),
		X(	"a7013",		"f7013"	),
		X(	"a7014",		"f7014"	),

		X(	"a7010Mov",		NULL	),
		X(	"a7011Mov",		NULL	),
		X(	"a7012Mov",		NULL	),
		X(	"a7013Mov",		NULL	),
		X(	"a7014Mov",		NULL	),
#endif

		X(	"d8000",		"f8000"	),
		X(	"d8001",		"f8001"	),
		X(	"d8002",		"f8002"	),
		X(	"d8003",		"f8003"	),
		X(	"d8004",		"f8004"	),

		X(	"d8000Mov",		NULL	),
		X(	"d8001Mov",		NULL	),
		X(	"d8002Mov",		NULL	),
		X(	"d8003Mov",		NULL	),
		X(	"d8004Mov",		NULL	),

		X(	"d9000",		"f9000"	),
		X(	"d9001",		"f9001"	),
		X(	"d9002",		"f9002"	),
		X(	"d9003",		"f9003"	),
		X(	"d9003Mov",		NULL	),
		X(	"d9004",		"f9004"	),
		X(	"d9004Mov",		NULL	),
		X(	"d9005",		"f9005"	),
		X(	"d9006",		"f9006"	),
		X(	"d9007",		"f9007"	),
		X(	"d9007Mov",		NULL	),
		X(	"d9008",		"f9008"	),
		X(	"d9008Mov",		NULL	),
		X(	"d9009",		"f9009"	),
#if AA
		X(	"a9010",		"f9010"	),
		X(	"a9011",		"f9011"	),
		X(	"a9012",		"f9012"	),
		X(	"a9013",		"f9013"	),
		X(	"a9014",		"f9014"	),
		X(	"a9014Mov",		NULL	),
		X(	"a9015",		"f9015"	),
		X(	"a9015Mov",		NULL	),
#endif

	};

#undef X

	SG_uint32 nrRowsX = SG_NrElements(aRowX);

	//////////////////////////////////////////////////////////////////
	// aRowY[] -- data used to create <cset2> from <cset1>

	struct _row_y
	{
		const char *			szDir1;
		const char *			szDir2;
		const char *			szFile1;
		const char *			szFile2;
		SG_diffstatus_flags		dsFlags;

		SG_pathname *			pPathDir;
		SG_pathname *			pPathFile;
		const char *			szLastDir;
		const char *			szLastFile;
	};

#define Y(d1,d2,f1,f2,ds)	{ d1, d2, f1, f2, ds, NULL, NULL, d1, f1 }

	struct _row_y aRowY[] = {

		// ME_MASK in isolation (this will be [ADM]'s in some of the comments in sg_status.c)
		// we don't bother with Lost/Found records here because we are going to commit these
		// changes and L/F records don't appear in a treediff-type status.

		Y(	"d0000",	NULL,		"f0000",	NULL,		SG_DIFFSTATUS_FLAGS__DELETED	),	// del <wd>/d0000/f0000
		Y(	"d0000",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__DELETED	),	// del <wd>/d0000

		Y(	"dAdd1",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),	// add <wd>/dAdd1
		Y(	"dAdd2",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd3",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd4",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd5",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd6",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd7",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd8",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd9",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),

		Y(	"dAdd10",	NULL,		"fAdd10",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),	// add <wd>/dAdd10/fAdd10 (implicitly adds <wd>/dAdd10 too)
		Y(	"dAdd11",	NULL,		"fAdd11",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd12",	NULL,		"fAdd12",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd13",	NULL,		"fAdd13",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd13_M",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd14",	NULL,		"fAdd14",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd14_M",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd15",	NULL,		"fAdd15",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd15_M",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd16",	NULL,		"fAdd16",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd17",	NULL,		"fAdd17",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd18",	NULL,		"fAdd18",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),
		Y(	"dAdd19",	NULL,		"fAdd19",	NULL,		SG_DIFFSTATUS_FLAGS__ADDED		),

#if AA
		Y(	"aAdd20",	NULL,		"fAdd20",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),	// add <wd>/dAdd20/fAdd20 (implicitly adds <wd>/dAdd20 too) with XATTRS(+test1)
		Y(	"aAdd21",	NULL,		"fAdd21",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"aAdd22",	NULL,		"fAdd22",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"aAdd23",	NULL,		"fAdd23",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"aAdd24",	NULL,		"fAdd24",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),

		Y(	"aAdd25",	NULL,		"fAdd25",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// add <wd>/dAdd25/fAdd25 with chmod(+x)
		Y(	"aAdd26",	NULL,		"fAdd26",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"aAdd27",	NULL,		"fAdd27",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"aAdd28",	NULL,		"fAdd28",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"aAdd29",	NULL,		"fAdd29",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),

		Y(	"aAdd30",	NULL,		"fAdd30",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),	// add <wd>/dAdd30/fAdd30 (implicitly adds <wd>/dAdd30 too) with XATTRS and ATTRBITS
		Y(	"aAdd31",	NULL,		"fAdd31",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"aAdd32",	NULL,		"fAdd32",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"aAdd33",	NULL,		"fAdd33",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"aAdd34",	NULL,		"fAdd34",	NULL,		(SG_DIFFSTATUS_FLAGS__ADDED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),

#endif

		Y(	"d2000",	NULL,		"f2000",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),	// mod <wd>/d2000/f2000
		Y(	"d2001",	NULL,		"f2001",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2002",	NULL,		"f2002",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2003",	NULL,		"f2003",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2004",	NULL,		"f2004",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2004_M",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__ADDED	),
		Y(	"d2005",	NULL,		"f2005",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2006",	NULL,		"f2006",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2007",	NULL,		"f2007",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2008",	NULL,		"f2008",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2009",	NULL,		"f2009",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2010",	NULL,		"f2010",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2011",	NULL,		"f2011",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2012",	NULL,		"f2012",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2013",	NULL,		"f2013",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2014",	NULL,		"f2014",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2015",	NULL,		"f2015",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2016",	NULL,		"f2016",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2017",	NULL,		"f2017",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2018",	NULL,		"f2018",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2019",	NULL,		"f2019",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d2020",	NULL,		"f2020",	NULL,		SG_DIFFSTATUS_FLAGS__MODIFIED	),

		// MODIFIERS_MASK in isolation (these will be know as Z's in some of the comments in sg_status.c)

		Y(	"d1000",	"d1000Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1000 --> <wd>/d1000Ren
		Y(	"d1001",	"d1001Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1002",	"d1002Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1003",	"d1003Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1004",	"d1004Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),

		Y(	"d1005",	NULL,		"f1005",	"f1005Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1005/f1005 --> <wd>/d1005/f1005Ren
		Y(	"d1006",	NULL,		"f1006",	"f1006Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1007",	NULL,		"f1007",	"f1007Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1008",	NULL,		"f1008",	"f1008Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1009",	NULL,		"f1009",	"f1009Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),

		Y(	"d1010",	NULL,		"f1010",	"f1010Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1010/f1010 --> <wd>/d1010/f1010Ren and
		Y(	"d1010",	"d1010Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1010 --> <wd>/d1010Ren YIELDING --> <wd>/d1010Ren/f1010Ren
		Y(	"d1011",	NULL,		"f1011",	"f1011Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1011",	"d1011Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1012",	NULL,		"f1012",	"f1012Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1012",	"d1012Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1013",	NULL,		"f1013",	"f1013Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1013",	"d1013Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1014",	NULL,		"f1014",	"f1014Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1014",	"d1014Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),

		Y(	"d1015",	"d1015Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1015 --> <wd>/d1015Ren and
		Y(	"d1015Ren",	NULL,		"f1015",	"f1015Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1015Ren/f1015 --> <wd>/d1015Ren/f1015Ren
		Y(	"d1016",	"d1016Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1016Ren",	NULL,		"f1016",	"f1016Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1017",	"d1017Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1017Ren",	NULL,		"f1017",	"f1017Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1018",	"d1018Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1018Ren",	NULL,		"f1018",	"f1018Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1019",	"d1019Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1019Ren",	NULL,		"f1019",	"f1019Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1020",	"d1020Ren",	NULL,		NULL,		SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1020Ren",	NULL,		"f1020",	"f1020Ren",	SG_DIFFSTATUS_FLAGS__RENAMED	),

		Y(	"d1030/aa",					"aa_ren",		NULL,	NULL,	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1030/aa --> <wd>/d1030/aa_ren
		Y(	"d1030/aa_ren/bb",			"bb_ren",		NULL,	NULL,	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1030/aa_ren/bb --> <wd>/d1030/aa_ren/bb_ren
		Y(	"d1030/aa_ren/bb_ren/cc",	"cc_ren",		NULL,	NULL,	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1030/aa_ren/bb_ren/cc --> <wd>/d1030/aa_ren/bb_ren/cc_ren

		Y(	"d1031/aa",						"d1031/aa_mov",						NULL,	NULL,	SG_DIFFSTATUS_FLAGS__MOVED		),	// mov <wd>/d1031/aa                   --> <wd>/d1031/aa_mov/aa
		Y(	"d1031/aa_mov/aa/bb",			"bb_ren",							NULL,	NULL,	SG_DIFFSTATUS_FLAGS__RENAMED	),	// ren <wd>/d1031/aa_mov/aa/bb         --> <wd>/d1031/aa_mov/aa/bb_ren
		Y(	"d1031/aa_mov/aa/bb_ren/cc",	"d1031/aa_mov/aa/bb_ren/cc_mov",	NULL,	NULL,	SG_DIFFSTATUS_FLAGS__MOVED		),	// mov <wd>/d1031/aa_mov/aa/bb_ren/cc  --> <wd>/d1031/aa_mov/aa/bb_ren/cc_mov/cc

		Y(	"d1032/aa",						"d1032/aa_mov",							NULL,	NULL,	SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d1032/aa_mov/aa/bb",			"bb_ren",								NULL,	NULL,	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1032/aa_mov/aa/bb_ren/cc",	"d1032/aa_mov/aa/bb_ren/cc_mov",		NULL,	NULL,	SG_DIFFSTATUS_FLAGS__MOVED		),

		Y(	"d1033/aa/bb/cc/dd/ee",			NULL,									"f1033",NULL,	SG_DIFFSTATUS_FLAGS__MODIFIED	),
		Y(	"d1033/aa",						"d1033/aa_mov",							NULL,	NULL,	SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d1033/aa_mov/aa/bb",			"bb_ren",								NULL,	NULL,	SG_DIFFSTATUS_FLAGS__RENAMED	),
		Y(	"d1033/aa_mov/aa/bb_ren/cc",	"d1033/aa_mov/aa/bb_ren/cc_mov",		NULL,	NULL,	SG_DIFFSTATUS_FLAGS__MOVED		),

		Y(	"d3000",	"d3000Mov",	"f3000",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),	// mov <wd>/d3000/f3000 --> <wd>/d3000Mov/f3000
		Y(	"d3001",	"d3001Mov",	"f3001",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3002",	"d3002Mov",	"f3002",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3003",	"d3003Mov",	"f3003",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3004",	"d3004Mov",	"f3004",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3005",	"d3005Mov",	"f3005",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3006",	"d3006Mov",	"f3006",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3007",	"d3007Mov",	"f3007",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3008",	"d3008Mov",	"f3008",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),
		Y(	"d3009",	"d3009Mov",	"f3009",	NULL,		SG_DIFFSTATUS_FLAGS__MOVED		),

		// i'm going to skip directory moves so that i don't have to deal with nested vhashes.

#if AA
		Y(	"a4000",	NULL,		"f4000",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),	// do xattr on <wd>/d4000/f4000
		Y(	"a4001",	NULL,		"f4001",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),
		Y(	"a4002",	NULL,		"f4002",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),
		Y(	"a4003",	NULL,		"f4003",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),
		Y(	"a4004",	NULL,		"f4004",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),

		Y(	"a4005",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),	// do xattr on <wd>/d4005
		Y(	"a4006",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),
		Y(	"a4007",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),
		Y(	"a4008",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),
		Y(	"a4009",	NULL,		NULL,		NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS	),

		Y(	"a4010",	NULL,		"f4010",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS	),	// do attrbits on <wd>/d4010/f4010
		Y(	"a4011",	NULL,		"f4011",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS	),
		Y(	"a4012",	NULL,		"f4012",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS	),
		Y(	"a4013",	NULL,		"f4013",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS	),
		Y(	"a4014",	NULL,		"f4014",	NULL,		SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS	),
		// since we currently only support chmod +x or -x, i'm going to skip testing directory.

		// combination of xattr and attrbits (these will also be Z's)

		Y(	"a4015",	NULL,		"f4015",		NULL,	(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),	// do xattr and attrbits on <wd>/d4015/f4015
		Y(	"a4016",	NULL,		"f4016",		NULL,	(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"a4017",	NULL,		"f4017",		NULL,	(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"a4018",	NULL,		"f4018",		NULL,	(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),
		Y(	"a4019",	NULL,		"f4019",		NULL,	(SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)	),

		// combine xattr and attrbit changes with rename and move

		Y(	"a5000",	NULL,		"f5000",	"f5000Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// change xattr &/| attrbits and rename <wd>/d5000/f5000 --> <wd>/d5000/f5000Ren
		Y(	"a5001",	NULL,		"f5001",	"f5001Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5002",	NULL,		"f5002",	"f5002Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5003",	NULL,		"f5003",	"f5003Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5004",	NULL,		"f5004",	"f5004Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)		),

		Y(	"a5005",	"a5005Mov",	"f5005",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// change xattr &/| attrbits and move <wd>/d5005/f5005 --> <wd>/d5005Mov/f5005
		Y(	"a5006",	"a5006Mov",	"f5006",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5007",	"a5007Mov",	"f5007",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5008",	"a5008Mov",	"f5008",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5009",	"a5009Mov",	"f5009",	NULL,		(SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)		),

		Y(	"a5010",	"a5010Mov",	"f5010",	"f5010Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// change xattr &/| attrbits and rename and move <wd>/d5010/f5010 --> <wd>/d5010Mov/f5010Ren
		Y(	"a5011",	"a5011Mov",	"f5011",	"f5011Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5012",	"a5012Mov",	"f5012",	"f5012Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5013",	"a5013Mov",	"f5013",	"f5013Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a5014",	"a5014Mov",	"f5014",	"f5014Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
#endif

		// combine rename and move (without attrbits or xattrs) for testing on windows

		Y(	"d6000",	"d6000Mov",	"f6000",	"f6000Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),	// rename and move <wd>/d6000/f6000 --> <wd>/d6000Mov/f6000Ren
		Y(	"d6001",	"d6001Mov",	"f6001",	"f6001Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),
		Y(	"d6002",	"d6002Mov",	"f6002",	"f6002Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),
		Y(	"d6003",	"d6003Mov",	"f6003",	"f6003Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),
		Y(	"d6004",	"d6004Mov",	"f6004",	"f6004Ren",	(SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),

		// ME_ and MOD_ MASK combined.  (only M's are really possible because A's would just
		// have the correct initial values and D's won't be present when the join happens later.)

#if AA
		Y(	"a7000",	NULL,		"f7000",	"f7000Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// modify, change xattr &/| attrbits and rename <wd>/d7000/f7000 --> <wd>/d7000/f7000Ren
		Y(	"a7001",	NULL,		"f7001",	"f7001Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7002",	NULL,		"f7002",	"f7002Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7003",	NULL,		"f7003",	"f7003Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7004",	NULL,		"f7004",	"f7004Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)		),

		Y(	"a7005",	"a7005Mov",	"f7005",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// modify, change xattr &/| attrbits and move <wd>/d7005/f7005 --> <wd>/d7005Mov/f7005
		Y(	"a7006",	"a7006Mov",	"f7006",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7007",	"a7007Mov",	"f7007",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7008",	"a7008Mov",	"f7008",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7009",	"a7009Mov",	"f7009",	NULL,		(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)		),

		Y(	"a7010",	"a7010Mov",	"f7010",	"f7010Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),	// modify, change xattr &/| attrbits and rename and move <wd>/d7010/f7010 --> <wd>/d7010Mov/f7010Ren
		Y(	"a7011",	"a7011Mov",	"f7011",	"f7011Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7012",	"a7012Mov",	"f7012",	"f7012Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7013",	"a7013Mov",	"f7013",	"f7013Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
		Y(	"a7014",	"a7014Mov",	"f7014",	"f7014Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED
															 |SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS
															 |SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)	),
#endif

		// combine modify, rename and move (without attrbits or xattrs) for testing on windows

		Y(	"d8000",	"d8000Mov",	"f8000",	"f8000Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),	// rename and move <wd>/d8000/f8000 --> <wd>/d8000Mov/f8000Ren
		Y(	"d8001",	"d8001Mov",	"f8001",	"f8001Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),
		Y(	"d8002",	"d8002Mov",	"f8002",	"f8002Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),
		Y(	"d8003",	"d8003Mov",	"f8003",	"f8003Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),
		Y(	"d8004",	"d8004Mov",	"f8004",	"f8004Ren",	(SG_DIFFSTATUS_FLAGS__MODIFIED
															 |SG_DIFFSTATUS_FLAGS__RENAMED
															 |SG_DIFFSTATUS_FLAGS__MOVED)	),

		// the remaining members in aRowX will be effectively Z's w/o MODIFIERS.  that is,
		// they won't appear in the treediff status at all.
	};

#undef Y

	SG_uint32 nrRowsY = SG_NrElements(aRowY);


	//////////////////////////////////////////////////////////////////

	// create random GID <gid> to be the base of the working dir within the directory given.
	// <wd> := <top>/<gid>
	// mkdir <wd>

	VERIFY_ERR_CHECK(  SG_tid__generate2(pCtx, bufName, sizeof(bufName), 32)  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathWorkingDir, pPathTopDir, bufName)  );
	INFO2("working directory",SG_pathname__sz(pPathWorkingDir));

	//////////////////////////////////////////////////////////////////
	// begin <cset0>
	//
	// create a new repo.  let the pendingtree code set up the initial working directory
	// and do the initial commit.  fetch the baseline and call it <cset0>.

	INFO2("arbitrary cset vs working directory", SG_pathname__sz(pPathWorkingDir));

	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__new_repo(pCtx, bufName, pPathWorkingDir)  );
	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_0,sizeof(bufCSet_0))  );

	SG_ERR_CHECK(  SG_repo__open_repo_instance(pCtx, bufName, &pRepo)  );

	// end of <cset0>
	//////////////////////////////////////////////////////////////////
	// begin <cset1>
	//
	// begin modifying the working directory with the contents in aRowX.
	// this is a simple series of ADDs so that we have some folders/files
	// to play with.

	INFOP("begin_building_x",("begin_building_x"));
	for (kRow=0; kRow < nrRowsX; kRow++)
	{
		struct _row_x * pRowX = &aRowX[kRow];

		// mkdir <wd>/<dir_k>
		// create file <wd>/<dir_k>/<file_k>
		VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowX->pPathDir,pPathWorkingDir,pRowX->szDir)  );
		VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pRowX->pPathDir)  );
		if (pRowX->szFile)
			VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pRowX->pPathDir,pRowX->szFile,20)  );
	}

	// cause scan and update of pendingtree vfile.

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	// verify that the pendingtree contains everything that we think it should (and nothing that it shouldn't).

	VERIFY_ERR_CHECK(  SG_STRING__ALLOC(pCtx, &pStr_check)  );
	VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir, SG_FALSE, &pPendingTree)  );
	VERIFY_ERR_CHECK(  SG_pendingtree__diff(pCtx, pPendingTree, NULL,&pTreeDiff_check)  );
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	for (kRow=0; kRow < nrRowsX; kRow++)
	{
		struct _row_x * pRowX = &aRowX[kRow];

		VERIFY_ERR_CHECK(  SG_string__sprintf(pCtx, pStr_check,"@/%s/",pRowX->szDir)  );
		VERIFY_ERR_CHECK(  SG_treediff2__find_by_repo_path(pCtx, pTreeDiff_check,SG_string__sz(pStr_check),&bFound,&szGidObject_check,&pOD_opaque_check)  );
		VERIFYP_COND("Row X Dir check",(bFound),("RowX[%d] [repo-path %s] not found in pendingtree as expected.",kRow,SG_string__sz(pStr_check)));
		if (bFound)
		{
			VERIFY_ERR_CHECK(  SG_treediff2__set_mark(pCtx, pTreeDiff_check,szGidObject_check,1,NULL)  );
			VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD_opaque_check,&dsFlags_check)  );
			VERIFYP_COND("Row X Dir check",(dsFlags_check == SG_DIFFSTATUS_FLAGS__ADDED),("RowX[%d] [repo-path %s] [dsFlags %x] not expected.",
																					 kRow,SG_string__sz(pStr_check),dsFlags_check));
		}
		if (pRowX->szFile)
		{
			VERIFY_ERR_CHECK(  SG_string__append__sz(pCtx, pStr_check,pRowX->szFile)  );
			VERIFY_ERR_CHECK(  SG_treediff2__find_by_repo_path(pCtx, pTreeDiff_check,SG_string__sz(pStr_check),&bFound,&szGidObject_check,&pOD_opaque_check)  );
			VERIFYP_COND("Row X File check",(bFound),("RowX[%d] [repo-path %s] not found in pendingtree as expected.",kRow,SG_string__sz(pStr_check)));
			if (bFound)
			{
				VERIFY_ERR_CHECK(  SG_treediff2__set_mark(pCtx, pTreeDiff_check,szGidObject_check,1,NULL)  );
				VERIFY_ERR_CHECK(  SG_treediff2__ObjectData__get_dsFlags(pCtx, pOD_opaque_check,&dsFlags_check)  );
				VERIFYP_COND("Row X File check",(dsFlags_check == SG_DIFFSTATUS_FLAGS__ADDED),("RowX[%d] [repo-path %s] [dsFlags %x] not expected.",
																						  kRow,SG_string__sz(pStr_check),dsFlags_check));
			}
		}
		SG_ERR_IGNORE(  SG_string__clear(pCtx, pStr_check)  );
	}
	VERIFY_ERR_CHECK(  SG_treediff2__find_by_repo_path(pCtx, pTreeDiff_check,"@/",&bFound,&szGidObject_check,NULL)  );
	VERIFYP_COND("Row X File check",(bFound),("[repo-path @/] not found in pendingtree as expected."));
	VERIFY_ERR_CHECK(  SG_treediff2__set_mark(pCtx, pTreeDiff_check,szGidObject_check,1,NULL)  );
	VERIFY_ERR_CHECK_DISCARD(  MyFn(check_treediff_markers)(pCtx, pTreeDiff_check,"before_commit_x")  );
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_check);
	SG_STRING_NULLFREE(pCtx, pStr_check);

	// Commit everything we have created in the file system.

	INFOP("commit_x",("commit_x"));
	VERIFY_ERR_CHECK(  MyFn(do_commit)(pCtx, pPathWorkingDir)  );

	// Let this first commit be <cset1>.  This will serve as a starting point for
	// everything else (rather than starting from an empty repo).

	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_1,sizeof(bufCSet_1))  );

	// TODO load a tree-diff <cset0> vs <cset1>.  this should show a series of adds.
	// TODO verify the contents match our expectation from the aRowX table.

	// end of <cset1>
	//////////////////////////////////////////////////////////////////
	// begin <cset2>
	//
	// create <cset2> by building upon <cset1>.  This contains a representative sample
	// of all of the operations and combinations of operations possible.

	INFOP("begin_building_y",("begin_building_y"));
	for (kRow=0; kRow < nrRowsY; kRow++)
	{
		struct _row_y * pRowY = &aRowY[kRow];

		// build <wd>/<dir>         := <wd>/<dir_k[1]>
		// and   <wd>/<dir>/<file>  := <wd>/<dir_k[1]>/<file_k[1]>

		VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowY->pPathDir,pPathWorkingDir,pRowY->szDir1)  );
		if (pRowY->szFile1)
			VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowY->pPathFile,pRowY->pPathDir,pRowY->szFile1)  );

		//INFOP("arbitrary cset vs working directory",("Y kRow[%d]: %s %s",kRow,pRowY->szDir1,((pRowY->szFile1) ? pRowY->szFile1 : "(null)")));

		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__ADDED)
		{
			// mkdir <wd>/<dir>
			// optionally create file <wd>/<dir>/<file_k[1]>

			VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pRowY->pPathDir)  );
			if (pRowY->szFile1)
				VERIFY_ERR_CHECK(  _ut_pt__create_file__numbers(pCtx, pRowY->pPathDir,pRowY->szFile1,20)  );
		}

		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__DELETED)
		{
			if (pRowY->szFile1)
			{
				// delete <wd>/<dir>/<file_k[1]>

				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree,pRowY->pPathDir,1,&pRowY->szFile1, NULL, 0, NULL, 0, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
			}
			else
			{
				// rmdir <wd>/<dir>
				// TODO does this need to be empty?

				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__remove(pCtx, pPendingTree,pPathWorkingDir,1,&pRowY->szDir1, NULL, 0, NULL, 0, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
			}
		}

		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__MODIFIED)
		{
			// append data to <wd>/<dir>/<file_k[1]>

			VERIFY_ERR_CHECK(  _ut_pt__append_to_file__numbers(pCtx, pRowY->pPathDir,pRowY->szFile1,4)  );
		}

		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__RENAMED)
		{
			if (pRowY->szFile1)
			{
				// rename <wd>/<dir>/<file> --> <wd>/<dir>/<file_k[2]>
				// let    <wd>/<dir>/<file>  := <wd>/<dir>/<file_k[2]>

				SG_ASSERT( pRowY->szFile2 );
				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree,pRowY->pPathFile,pRowY->szFile2, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
				SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathFile);
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowY->pPathFile,pRowY->pPathDir,pRowY->szFile2)  );
			}
			else
			{
				// rename <wd>/<dir> --> <wd>/<dir_k[2]>
				// let    <wd>/<dir>  := <wd>/<dir_k[2]>

				SG_ASSERT( pRowY->szDir2 );
				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__rename(pCtx, pPendingTree,pRowY->pPathDir,pRowY->szDir2, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
				SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathDir);
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowY->pPathDir,pPathWorkingDir,pRowY->szDir2)  );
			}
		}

		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__MOVED)
		{
			if (pRowY->szFile1)
			{
				// move <wd>/<dir>/<file> --> <wd>/<dir_k[2]>
				// let  <wd>/<dir>         := <wd>/<dir_k[2]>
				// let  <wd>/<dir>/<file>  := <wd>/<dir_k[2]>/<file_k[?]>

				SG_ASSERT( pRowY->szDir2 );
				SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathDir);
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowY->pPathDir,pPathWorkingDir,pRowY->szDir2)  );

				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree,(const SG_pathname **)&(pRowY->pPathFile), 1,pRowY->pPathDir, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

				VERIFY_ERR_CHECK(  SG_pathname__get_last(pCtx, pRowY->pPathFile,&pStrEntryName)  );
				SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathFile);
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pRowY->pPathFile,pRowY->pPathDir,SG_string__sz(pStrEntryName))  );
				SG_STRING_NULLFREE(pCtx, pStrEntryName);
			}
			else
			{
				// move a directory.  we assume that both <dir_k[1]> and <dir_k[2]> are relative pathnames
				// and that the first is the directory to be moved and the second is the path of the parent
				// directory.  for example "a/b/c" and "d/e/f" will put "c" inside "f".

				SG_ASSERT( pRowY->szDir2 );
				VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__PATHNAME_SZ(pCtx, &pPathTemp,pPathWorkingDir,pRowY->szDir2)  );

				VERIFY_ERR_CHECK(  SG_PENDINGTREE__ALLOC(pCtx, pPathWorkingDir,SG_FALSE,&pPendingTree)  );
				VERIFY_ERR_CHECK(  SG_pendingtree__move(pCtx, pPendingTree,(const SG_pathname **)&(pRowY->pPathDir), 1,pPathTemp, SG_FALSE)  );
				SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);

				VERIFY_ERR_CHECK(  SG_pathname__get_last(pCtx, pRowY->pPathDir,&pStrEntryName)  );
				SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathDir);
				VERIFY_ERR_CHECK(  SG_pathname__append__from_sz(pCtx, pPathTemp,SG_string__sz(pStrEntryName))  );
				SG_STRING_NULLFREE(pCtx, pStrEntryName);

				SG_pathname__free(pCtx, pRowY->pPathDir);
				pRowY->pPathDir = pPathTemp;
				pPathTemp = NULL;
			}
		}

#if AA
		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_XATTRS)
		{
			if (pRowY->szFile1)
				VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pRowY->pPathFile,"com.sourcegear.test1",2,(SG_byte *)"Y")  );
			else
				VERIFY_ERR_CHECK(  SG_fsobj__xattr__set(pCtx, pRowY->pPathDir,"com.sourcegear.test1",2,(SG_byte *)"Y")  );
		}

		if (pRowY->dsFlags & SG_DIFFSTATUS_FLAGS__CHANGED_ATTRBITS)
		{
			if (pRowY->szFile1)
			{
				SG_fsobj_stat st;

				VERIFY_ERR_CHECK(  SG_fsobj__stat__pathname(pCtx, pRowY->pPathFile,&st)  );
				if (st.perms & S_IXUSR)
					st.perms &= ~S_IXUSR;
				else
					st.perms |= S_IXUSR;
				VERIFY_ERR_CHECK(  SG_fsobj__chmod__pathname(pCtx, pRowY->pPathFile,st.perms)  );
			}
			else
			{
				// TODO chmod a directory.  we currently only define the +x bit on files.

				SG_ASSERT( 0 );
			}
		}
#endif
	}

	// cause scan and update of pendingtree vfile.

	VERIFY_ERR_CHECK(  _ut_pt__addremove(pCtx, pPathWorkingDir)  );

	// we now have <cset1> + dirt.
	// verify pendingtree/treediff contains what we think it should and
	// that "REVERT --test" is free of side-effects.
	//
	// TODO verify that the log from "REVERT --test" contains what we
	// TODO think it should.  (i've eye-balled it, but it'd be nice to
	// TODO go thru and verify everything.)

	VERIFY_ERR_CHECK_DISCARD(  MyFn(do_verify_y_answer)(pCtx, pPathWorkingDir)  );
	VERIFY_ERR_CHECK_DISCARD(  MyFn(do_revert_log)(pCtx, pPathWorkingDir, "<cset1> + dirt")  );
	VERIFY_ERR_CHECK_DISCARD(  MyFn(do_verify_y_answer)(pCtx, pPathWorkingDir)  );

	// Commit everything we have created/modified in the file system.

	INFOP("commit_y",("commit_y"));
	VERIFY_ERR_CHECK(  MyFn(do_commit)(pCtx, pPathWorkingDir)  );

	// Let this second commit be <cset2>.  This will become the baseline
	// and be used to do a treediff-status( <cset1> vs <cset2> ) and
	// then to do a pendingtree-status( <cset2> vs <working-directory> )
	// so that we can join them.

	VERIFY_ERR_CHECK(  _ut_pt__get_baseline(pCtx, pPathWorkingDir,bufCSet_2,sizeof(bufCSet_2))  );

	// TODO do a tree-diff of <cset1> vs <cset2>.  this should be a
	// series of lots of things.  and verify the contents match our
	// expectations from the aRowY table.

	// end of <cset2>
	//////////////////////////////////////////////////////////////////

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathWorkingDir);
	SG_PATHNAME_NULLFREE(pCtx, pPathTemp);
	SG_STRING_NULLFREE(pCtx, pStrEntryName);
	SG_VHASH_NULLFREE(pCtx, pvhManifest_1);
	SG_VHASH_NULLFREE(pCtx, pvhManifest_2);
	SG_VHASH_NULLFREE(pCtx, pvhManifest_3);
	SG_PENDINGTREE_NULLFREE(pCtx, pPendingTree);
	SG_STRING_NULLFREE(pCtx, pStringStatus01);
	SG_STRING_NULLFREE(pCtx, pStringStatus12);
	SG_STRING_NULLFREE(pCtx, pStringStatus13);
	SG_STRING_NULLFREE(pCtx, pStringStatus1wd);
	SG_REPO_NULLFREE(pCtx, pRepo);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_join_before_commit);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_join_after_commit);
	SG_TREEDIFF2_NULLFREE(pCtx, pTreeDiff_check);
	SG_STRING_NULLFREE(pCtx, pStr_check);

	for (kRow=0; kRow < nrRowsX; kRow++)
	{
		struct _row_x * pRowX = &aRowX[kRow];
		SG_PATHNAME_NULLFREE(pCtx, pRowX->pPathDir);
	}

	for (kRow=0; kRow < nrRowsY; kRow++)
	{
		struct _row_y * pRowY = &aRowY[kRow];
		SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathDir);
		SG_PATHNAME_NULLFREE(pCtx, pRowY->pPathFile);
	}
}

//////////////////////////////////////////////////////////////////

TEST_MAIN(u0075_pendingtree_log)
{
	char bufTopDir[SG_TID_MAX_BUFFER_LENGTH];
	SG_pathname* pPathTopDir = NULL;

	TEMPLATE_MAIN_START;

	VERIFY_ERR_CHECK(  SG_tid__generate2__suffix(pCtx, bufTopDir, sizeof(bufTopDir), 32, "u0075")  );
	VERIFY_ERR_CHECK(  SG_PATHNAME__ALLOC__SZ(pCtx, &pPathTopDir,bufTopDir)  );
	VERIFY_ERR_CHECK(  SG_fsobj__mkdir__pathname(pCtx, pPathTopDir)  );

	BEGIN_TEST(  MyFn(big_revert)(pCtx, pPathTopDir)  );

	//////////////////////////////////////////////////////////////////

	/* TODO rm -rf the top dir */

fail:
	SG_PATHNAME_NULLFREE(pCtx, pPathTopDir);

	TEMPLATE_MAIN_END;
}

#undef MyMain
#undef MyDcl
#undef MyFn
