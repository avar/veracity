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
// Some MERGE tests to try to drive coverage so that we can see
// the output produced by the --verbose option and list the resolved
// and unresolved issues.
//
// These tests replace the test000 section in u0064 since it is
// easier to verify the results in JS (much less typing).
//////////////////////////////////////////////////////////////////

function st_merge_coverage_000()
{
    load("update_helpers.js");          // load the helper functions
    initialize_update_helpers(this);    // initialize helper functions

    //////////////////////////////////////////////////////////////////

    this.setUp = function()
    {
        this.workdir_root = sg.fs.getcwd();     // save this for later

        //////////////////////////////////////////////////////////////////
        // fetch the HID of the initial commit.

        var h = sg.pending_tree.history();
        this.csets.push( h[0].changeset_id );
        this.names.push( "*Initial Commit*" );

        //////////////////////////////////////////////////////////////////
	// create 2 csets:
	//
	//      0
	//     /
	//    1

        this.do_fsobj_mkdir      ("test000_0");
        this.do_fsobj_mkdir      ("test000_0/dir_100");
        this.do_fsobj_create_file("test000_0/dir_100/f1.txt");

        this.do_fsobj_mkdir      ("test001_0");
        this.do_fsobj_mkdir      ("test001_0/dir_100");
        this.do_fsobj_create_file("test001_0/dir_100/f1.txt");

        this.do_fsobj_mkdir      ("test002_0");
        this.do_fsobj_mkdir      ("test002_0/dir_100");
        this.do_fsobj_create_file("test002_0/dir_100/f1.txt");

        this.do_fsobj_mkdir      ("test002_1");
        this.do_fsobj_mkdir      ("test002_1/dir_100");
        this.do_fsobj_create_file("test002_1/dir_100/f1.txt");

        this.do_fsobj_mkdir      ("test003_0");
        this.do_fsobj_mkdir      ("test003_0/dir_100");
        this.do_fsobj_create_file("test003_0/dir_100/f1.txt");

        this.do_fsobj_mkdir      ("test003_1");
        this.do_fsobj_mkdir      ("test003_1/dir_100");
        this.do_fsobj_create_file("test003_1/dir_100/f1.txt");

        this.do_fsobj_mkdir      ("test004_0");
        this.do_fsobj_mkdir      ("test004_0/dir_100");
        this.do_fsobj_create_file("test004_0/dir_100/f1.txt");
        this.do_fsobj_mkdir      ("test004_0/dir_101");

        this.do_fsobj_mkdir      ("test004_1");
        this.do_fsobj_mkdir      ("test004_1/dir_100");
        this.do_fsobj_create_file("test004_1/dir_100/f1.txt");
        this.do_fsobj_mkdir      ("test004_1/dir_101");

	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    // TODO test005_[0123] require xattrs
	}

	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
            this.do_fsobj_mkdir      ("test006_0");
            this.do_fsobj_mkdir      ("test006_0/dir_100");
            this.do_fsobj_create_file("test006_0/dir_100/f1.txt");
            this.do_fsobj_mkdir      ("test006_0/dir_101");

            this.do_fsobj_mkdir      ("test006_1");
            this.do_fsobj_mkdir      ("test006_1/dir_100");
            this.do_fsobj_create_file("test006_1/dir_100/f1.txt");
            this.do_fsobj_mkdir      ("test006_1/dir_101");

            this.do_fsobj_mkdir      ("test006_2");
            this.do_fsobj_mkdir      ("test006_2/dir_100");
            this.do_fsobj_create_file("test006_2/dir_100/f1.txt");
	    this.do_chmod            ("test006_2/dir_100/f1.txt", 0700);
            this.do_fsobj_mkdir      ("test006_2/dir_101");

            this.do_fsobj_mkdir      ("test006_3");
            this.do_fsobj_mkdir      ("test006_3/dir_100");
            this.do_fsobj_create_file("test006_3/dir_100/f1.txt");
	    this.do_chmod            ("test006_3/dir_100/f1.txt", 0700);
            this.do_fsobj_mkdir      ("test006_3/dir_101");
	}

        this.do_fsobj_mkdir      ("test007_0");
        this.do_fsobj_mkdir      ("test007_0/dir_100");
        this.do_fsobj_create_file("test007_0/dir_100/f1.txt");
        this.do_fsobj_mkdir      ("test007_0/dir_101");

        this.do_fsobj_mkdir      ("test007_1");
        this.do_fsobj_mkdir      ("test007_1/dir_100");
        this.do_fsobj_create_file("test007_1/dir_100/f1.txt");
        this.do_fsobj_mkdir      ("test007_1/dir_101");

	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    // TODO test008_0 requires symlinks
	}

        this.do_addremove_scan();
        this.do_commit("CSet 0");

	//////////////////////////////////////////////////////////////////
	// begin '1' which we'll arbitrarily call 'trunk'

        this.do_fsobj_mkdir      ("test000_0/dir_102");
        this.do_fsobj_create_file("test000_0/dir_102/fd.txt");

        this.do_fsobj_create_file("test001_0/dir_100/fd.txt");

        this.do_fsobj_create_file("test002_0/dir_100/fd.txt");

	this.do_fsobj_append_file("test002_1/dir_100/f1.txt");

        this.do_fsobj_create_file("test003_0/dir_100/fd.txt");

	this.do_vc_rename        ("test003_1/dir_100/f1.txt",  "f1_r.txt");

        this.do_fsobj_create_file("test004_0/dir_100/fd.txt");

	this.do_vc_move          ("test004_1/dir_100/f1.txt",  "test004_1/dir_101");

	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    // TODO test005_[0123] require xattrs
	}

	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
            this.do_fsobj_create_file("test006_0/dir_100/fd.txt");

	    this.do_chmod            ("test006_1/dir_100/f1.txt", 0700);

            this.do_fsobj_create_file("test006_2/dir_100/fd.txt");

	    this.do_chmod            ("test006_3/dir_100/f1.txt", 0600);
	}

        this.do_fsobj_create_file("test007_0/dir_100/fd.txt");

	this.do_vc_remove_file   ("test007_1/dir_100/f1.txt");

	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    // TODO test008_0 requires symlinks
	}

        this.do_addremove_scan();
        this.do_commit("CSet 1");

	this.rev_trunk_head = this.csets[ this.csets.length - 1 ];

	//////////////////////////////////////////////////////////////////
	// update back to '0' and start a new branch '2':
	//      0
	//     / \
	//    1   2

	this.do_update_when_clean_by_name("CSet 0");

        this.do_fsobj_mkdir      ("test000_0/dir_103");
        this.do_fsobj_create_file("test000_0/dir_103/f4.txt");

        this.do_fsobj_create_file("test001_0/dir_100/f4.txt");

        this.do_fsobj_append_file("test002_0/dir_100/f1.txt");

        this.do_fsobj_create_file("test002_1/dir_100/fd.txt");

	this.do_vc_rename        ("test003_0/dir_100/f1.txt",  "f1_r.txt");

        this.do_fsobj_create_file("test003_1/dir_100/fd.txt");

	this.do_vc_move          ("test004_0/dir_100/f1.txt",  "test004_0/dir_101");

        this.do_fsobj_create_file("test004_1/dir_100/fd.txt");

	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    // TODO test005_[0123] require xattrs
	}

	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    this.do_chmod            ("test006_0/dir_100/f1.txt", 0700);

            this.do_fsobj_create_file("test006_1/dir_100/fd.txt");

	    this.do_chmod            ("test006_2/dir_100/f1.txt", 0600);

            this.do_fsobj_create_file("test006_3/dir_100/fd.txt");
	}

	this.do_vc_remove_file       ("test007_0/dir_100/f1.txt");

        this.do_fsobj_create_file("test007_1/dir_100/fd.txt");

	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    // TODO test008_0 requires symlinks
	}

        this.do_addremove_scan();
        this.do_commit_in_branch("CSet 2");

	this.rev_branch_head = this.csets_in_branch[ this.csets_in_branch.length - 1 ];

	//////////////////////////////////////////////////////////////////

	var o = sg.exec(vv, "heads");
	print("Heads are:");
	print(o.stdout);
    }

    //////////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////////

    this.do_update_by_rev = function(rev)
    {
	print("vv update --rev=" + rev);
	var o = sg.exec(vv, "update", "--rev="+rev);

	//print("STDOUT:\n" + o.stdout);
	//print("STDERR:\n" + o.stderr);

	testlib.ok( o.exit_status == 0, "Exit status should be 0" );
	this.assert_current_baseline(rev);
    }

    this.do_exec_merge = function(rev, bExpectFail, msgExpected)
    {
	print("vv merge --rev=" + rev);
	var o = sg.exec(vv, "merge", "--rev="+rev, "--verbose");
	
	print("STDOUT:\n" + o.stdout);
	print("STDERR:\n" + o.stderr);

	if (bExpectFail)
	{
	    testlib.ok( o.exit_status == 1, "Exit status should be 1" );
	    testlib.ok( o.stderr.search(msgExpected) >= 0, "Expecting error message [" + msgExpected + "]. Received:\n" + o.stderr);
	}
	else
	{
	    testlib.ok( o.exit_status == 0, "Exit status should be 0" );
	}
    }

    //////////////////////////////////////////////////////////////////
    // NOTE: this.confirm() and this.confirm_when_ignoring_ignores()
    //       compute a STATUS and compare that with the expected results.
    //       A STATUS reflects the changes made to the WD from the
    //       BASELINE.  So the STATUS should show how the FINAL RESULT
    //       changed from the BASELINE.
    //
    //       This feels a little odd because we are accustomed to
    //       think about the total net changes from the ANCESTOR to
    //       the FINAL RESULT.
    //
    //       So, for example, if a file was renamed between the
    //       ancestor and the baseline branch (but not in the other
    //       changeset), it will not show up in the status.  It will
    //       have the new name in the final result, but that is not
    //       a change relative to the baseline.
    //////////////////////////////////////////////////////////////////

    this.test_simple_tb_1 = function()
    {
	this.force_clean_WD();

	// set WD to the trunk.
	// merge (fold) in the branch.
	// make sure basic clean merge works as expected.

	this.do_update_by_rev(this.rev_trunk_head);
	this.do_exec_merge(this.rev_branch_head, 0, "");

        var expect_test = new Array;
	expect_test["Added"] = new Array;
	expect_test["Modified"] = new Array;
	expect_test["Renamed"] = new Array;
	expect_test["Moved"] = new Array;
	expect_test["Removed"] = new Array;
        expect_test["Added"].push(    "@/test000_0/dir_103" );
        expect_test["Added"].push(    "@/test000_0/dir_103/f4.txt" );
        expect_test["Added"].push(    "@/test001_0/dir_100/f4.txt" );
	expect_test["Modified"].push( "@/test002_0/dir_100/f1.txt" );
        expect_test["Added"].push(    "@/test002_1/dir_100/fd.txt" );
	expect_test["Renamed"].push(  "@/test003_0/dir_100/f1_r.txt" );
        expect_test["Added"].push(    "@/test003_1/dir_100/fd.txt" );
	expect_test["Moved"].push(    "@/test004_0/dir_101/f1.txt" );
        expect_test["Added"].push(    "@/test004_1/dir_100/fd.txt" );
	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    // TODO test005_[0123] require xattrs
	}
	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    expect_test["Modified"].push( "@/test006_0/dir_100/f1.txt" );
	    expect_test["Added"].push(    "@/test006_1/dir_100/fd.txt" );
	    expect_test["Modified"].push( "@/test006_2/dir_100/f1.txt" );
	    expect_test["Added"].push(    "@/test006_3/dir_100/fd.txt" );
	}
	expect_test["Removed"].push(  "@/test007_0/dir_100/f1.txt" );
        expect_test["Added"].push(    "@/test007_1/dir_100/fd.txt" );
	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    // TODO test008_0 requires symlinks
	}
        this.confirm_when_ignoring_ignores(expect_test);
    }

    this.test_simple_bt_1 = function()
    {
	this.force_clean_WD();

	// set WD to the branch.
	// merge (fold) in the trunk.
	// make sure basic clean merge works as expected.

	this.do_update_by_rev(this.rev_branch_head);
	this.do_exec_merge(this.rev_trunk_head, 0, "");

        var expect_test = new Array;
	expect_test["Added"] = new Array;
	expect_test["Modified"] = new Array;
	expect_test["Renamed"] = new Array;
	expect_test["Moved"] = new Array;
	expect_test["Removed"] = new Array;
        expect_test["Added"].push(    "@/test000_0/dir_102" );
        expect_test["Added"].push(    "@/test000_0/dir_102/fd.txt" );
        expect_test["Added"].push(    "@/test001_0/dir_100/fd.txt" );
        expect_test["Added"].push(    "@/test002_0/dir_100/fd.txt" );
	expect_test["Modified"].push( "@/test002_1/dir_100/f1.txt" );
        expect_test["Added"].push(    "@/test003_0/dir_100/fd.txt" );
	expect_test["Renamed"].push(  "@/test003_1/dir_100/f1_r.txt" );
        expect_test["Added"].push(    "@/test004_0/dir_100/fd.txt" );
	expect_test["Moved"].push(    "@/test004_1/dir_101/f1.txt" );
	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    // TODO test005_[0123] require xattrs
	}
	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    expect_test["Added"].push(    "@/test006_0/dir_100/fd.txt" );
	    expect_test["Modified"].push( "@/test006_1/dir_100/f1.txt" );
	    expect_test["Added"].push(    "@/test006_2/dir_100/fd.txt" );
	    expect_test["Modified"].push( "@/test006_3/dir_100/f1.txt" );
	}
        expect_test["Added"].push(    "@/test007_0/dir_100/fd.txt" );
	expect_test["Removed"].push(  "@/test007_1/dir_100/f1.txt" );
	if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
	{
	    // TODO test008_0 requires symlinks
	}
        this.confirm_when_ignoring_ignores(expect_test);
    }

}
