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

load("vscript_test_lib.js");

//////////////////////////////////////////////////////////////////
// Some basic unit tests for SG_pendingtree__update()
//
// We build a series of changesets and make a series of UPDATEs
// using a CLEAN Working Directory.  We verify that the contents of
// the entire WD were changed as expected.  At the end of each UPDATE
// command, the contents of the WD should exactly match the contents
// when the changeset was created.
//
// The test here is for bug SPRAWL-549.
//////////////////////////////////////////////////////////////////

function stUpdateTestsComboIssues_549() 
{
    // Helper functions are in update_helpers.js
    // There is a reference list at the top of the file.
    // After loading, you must call initialize_update_helpers(this)
    // before you can use the helpers.
    load("update_helpers.js");          // load the helper functions
    initialize_update_helpers(this);    // initialize helper functions

    // this.verbose = true; // add this wherever you want it.  It'll stick through the end of the stGroup.

    //////////////////////////////////////////////////////////////////

    this.setUp = function()
    {
	// we begin with a clean WD in a fresh REPO.

	//////////////////////////////////////////////////////////////////
	// fetch the HID of the initial commit.

	var h = sg.pending_tree.history();
	this.csets.push( h[0].changeset_id );
	this.names.push( "*Initial Commit*" );
	this.ndxCur = 0;

	//////////////////////////////////////////////////////////////////
	// begin a sequence of events suggested by a generated combo test.

	this.do_fsobj_mkdir("8666ae");
	this.do_add(  "8666ae");
	this.do_fsobj_mkdir("8666ae/56eed4");
	this.do_add(  "8666ae/56eed4");
	this.do_fsobj_mkdir("8666ae/56eed4/a8c9e0");
	this.do_add(  "8666ae/56eed4/a8c9e0");
	this.do_fsobj_mkdir("059b3d");
	this.do_add(  "059b3d");
	this.do_commit("Combo 1.0");


	var pair = new Array();
	pair.a = this.ndxCur;


	this.do_vc_move( "059b3d", "8666ae/56eed4/a8c9e0");
	this.do_commit("Combo 1.1");


	this.do_fsobj_mkdir("8666ae/56eed4/a8c9e0/059b3d/6d0fea");
	this.do_add(  "8666ae/56eed4/a8c9e0/059b3d/6d0fea");
	this.do_commit("Combo 1.2");


	pair.b = this.ndxCur;
	this.pairs.push( pair );

	//////////////////////////////////////////////////////////////////

	this.list_csets("After setUp");
    }

    //////////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////////

    this.xxx_test_clean_all_in_one_step = function()
    {
	// assuming we have a clean WD.
	// start at the HEAD and UPDATE to the initial commit in one step.
	// this should UNDO everything and give us an empty WD.
	// then REDO everything in one step and get us back to the HEAD.

	if (!this.do_clean_warp_to_head())
	    return;

	var ndxHead = this.csets.length - 1;

	if (!this.do_update_when_clean(0))
	    return;

	if (!this.do_update_when_clean(ndxHead))
	    return;
    }

    this.xxx_test_clean_linearly = function()
    {
	// assuming we have a clean WD.
	// start at the HEAD and UPDATE to each of the previous csets 
	// in order until we reach the initial commit.  then UPDATE to
	// each of the descendant csets until we reach the HEAD again.
	// this should be like a series of UNDOs (one step at a time
	// all the way back to the initial commit) and then followed
	// by a series of REDOs back to the HEAD.

	if (!this.do_clean_warp_to_head())
	    return;

	var ndxHead = this.csets.length - 1;

	while (this.ndxCur > 0)
	{
	    if (!this.do_update_when_clean(this.ndxCur - 1))
		return;
	}

	while (this.ndxCur < ndxHead)
	{
	    if (!this.do_update_when_clean(this.ndxCur + 1))
		return;
	}
    }

    this.test_clean_pairs = function()
    {
	// assuming we have a clean WD.
	// we don't care what the starting baseline is.
	// go thru the list of "pairs of interest" and bounce between them.
	// this is a subset of the full combinatorial.

	if (!this.force_clean_WD())
	    return;

	for (var kPair = 0; kPair < this.pairs.length; kPair++)
	{
	    var pair = this.pairs[kPair];
	    print("Pair of Interest[" + kPair + "]: [" + this.names[pair.a] + "] [" + this.names[pair.b] + "]");

	    // warp to "a" from wherever the last test left us.
	    // this should succeed, but it's not the focus of this test.

	    if (!this.do_update_when_clean( pair.a ))
		return;

	    // now warp from "a" --> "b" and then from "b" --> "a"
	    // and make sure everything is ok.  this is why we're here.

	    if (!this.do_update_when_clean( pair.b ))
		return;

	    if (!this.do_update_when_clean( pair.a ))
		return;
	}
    }

    this.test_clean_combinatorially = function()
    {
//	if (testlib.defined.SG_LONGTESTS != true)
//	{
//	    print("Skipping tests because SG_LONGTESTS not set.");
//	    return;
//	}

	// assuming we have a clean WD.
	// do all possible point-to-point combinations.

	if (!this.do_clean_warp_to_head())
	    return;

	var ndxHead = this.csets.length - 1;

	for (var ndxJ = ndxHead; ndxJ > 0; ndxJ--)
	{
	    if (!this.do_update_when_clean(ndxJ))
		return;

	    for (var ndxK = 0; ndxK < ndxJ; ndxK++)
	    {
		if (!this.do_update_when_clean(ndxK))
		    return;

		if (!this.do_update_when_clean(ndxJ))
		    return;
	    }
	}
    }

    //////////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////////

    this.xxx_test_clean_self = function()
    {
	// assuming we have a clean WD.
	// warp to arbitrary changeset.
	// test UPDATE jump to same changeset should do nothing.

	if (!this.do_clean_warp_to_head())
	    return;

	if (!this.do_clean_warp_to_head())
	    return;
    }

    //////////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////////

    this.tearDown = function() 
    {

    }
}
