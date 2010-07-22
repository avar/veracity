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
// The test here is for bug SPRAWL-544.
//////////////////////////////////////////////////////////////////

function stUpdateTestsComboIssues_544() 
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

	this.do_fsobj_mkdir("dir_a");
	this.do_add(  "dir_a");
	this.do_fsobj_mkdir("dir_a/dir_b");
	this.do_add(  "dir_a/dir_b");
	this.do_commit("Combo 1.0");


	this.do_fsobj_create_file( "dir_a/dir_b/file_1", "This is the trunk version.");
	this.do_add("dir_a/dir_b/file_1");
	this.do_fsobj_create_file( "dir_a/dir_b/file_2", "This is the trunk version.");
	this.do_add("dir_a/dir_b/file_2");
	this.do_commit("Combo Trunk 1.1");

	//////////////////////////////////////////////////////////////////

	this.list_csets("After setUp");

	//////////////////////////////////////////////////////////////////

	this.do_update_when_clean(1);	// go back to "Combo 1.0"


	// add a different file with the same in the same directory
	this.do_fsobj_create_file( "dir_a/dir_b/file_1", "This is the branch version.");
	this.do_add("dir_a/dir_b/file_1");
	this.do_fsobj_create_file( "dir_a/dir_b/file_2", "This is the branch version.");
	this.do_add("dir_a/dir_b/file_2");
	this.do_commit_in_branch("Combo Branch 1.1");

	//////////////////////////////////////////////////////////////////

	this.list_csets_in_branch("After setUp");
    }

    //////////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////////

    this.test_1 = function()
    {
	// we are at the head of the branch.
	// now bounce between the 2 branches

	if (!this.force_clean_WD())
	    return;

	var ndxHead = this.csets.length - 1;
	var ndxHead_in_branch = this.csets_in_branch.length - 1;

	if (!this.do_update_when_clean(ndxHead))
	    return;

	if (!this.do_update_in_branch_when_clean(ndxHead_in_branch))
	    return;

	if (!this.do_update_when_clean(ndxHead))
	    return;
    }

    //////////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////////

    this.tearDown = function() 
    {

    }
}
