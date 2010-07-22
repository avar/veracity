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

/***********************************************************************
    update_lib.js contains the bulk of common functions and vars used in
    the various Update tests.  Here is a summary of its contents.

    Any of these can be overridden after the load();

    this.csets = new Array();                // map[0..n] ==> <hid_k>              a linear list of cset HIDs
    this.names = new Array();                // map[0..n] ==> <symbolic_name_k>    a linear list of symbolic names for each cset
    this.ndxCur = -1;                        // the index of current baseline
    this.csets_in_branch = new Array();
    this.names_in_branch = new Array();
    this.ndxCur_in_branch = -1;
    this.name2ndx = new Array();             // map[<symbolic_name>] ==> 0..n
    this.pairs = new Array();                // map[0..j] ==> (pair_k.a, pair_k.b)     a list of pairs that deserve attention
    this.workdir_root = "";                  // set this in this.setUp()

    this.assert_current_baseline = function(cset) {
    this.assert_clean_WD = function()
    this.do_remove_WD_dirt()
    this.confirm = function(expected)
    this.confirm_all_claimed = function(status)
    this.confirm_entry_exists_in_section = function(status, sectionname, repopath)
    this.confirm_entry_not_in_section = function(status, sectionname, repopath)
    this.confirm_expectations = function(status, expected)
    this.confirm_not_on_disk = function(file_list)
    this.confirm_on_disk = function(file_list)
    this.do_add = function(path)
    this.do_addremove_scan = function()
    this.do_check_entry_status = function(status, sectionname, repopath)
    this.do_chmod = function(path, octal_value)
    this.do_clean_warp_to_head = function()
    this.do_commit = function(label)
    this.do_commit_in_branch = function(label)
    this.do_fsobj_append_file = function(pathname, msg)
    this.do_fsobj_create_file = function(pathname, msg)
    this.do_fsobj_mkdir = function(path)
    this.do_fsobj_remove_dir = function(path)
    this.do_fsobj_remove_file = function(path)
    this.do_revert_all = function()

    this.do_update = function(ndxNew, stateMsg)
    this.do_update_when_clean = function(ndxNew)
    this.do_update_when_clean_by_name = function(name)
    this.do_update_when_dirty = function(ndxNew)
    this.do_update_when_dirty_by_name = function(name)
    this.do_update_in_branch = function(ndxNew, stateMsg)
    this.do_update_in_branch_when_clean = function(ndxNew)
    this.do_update_in_branch_when_dirty = function(ndxNew)

    this.do_vc_move = function(path_old,folder_new)
    this.do_vc_remove_dir = function(path)
    this.do_vc_remove_file = function(path)
    this.do_vc_rename = function(path_old,entryname_new)
    this.get_baseline = function()
    this.list_csets = function(label)
    this.list_csets_in_branch = function(label)
    this.statusEmpty = function(msg)
***********************************************************************/

function initialize_update_helpers(obj) {

    obj.csets = new Array();                // map[0..n] ==> <hid_k>              a linear list of cset HIDs
    obj.names = new Array();                // map[0..n] ==> <symbolic_name_k>    a linear list of symbolic names for each cset
    obj.ndxCur = -1;                        // the index of current baseline

    obj.name2ndx = new Array();             // map[<symbolic_name>] ==> 0..n
    obj.pairs = new Array();                // map[0..j] ==> (pair_k.a, pair_k.b)     a list of pairs that deserve attention
    obj.workdir_root = "";                  // set this in this.setUp()

    obj.csets_in_branch = new Array();
    obj.names_in_branch = new Array();
    obj.ndxCur_in_branch = -1;

    obj.statusEmpty = function(msg)
    {
        // like testlib.statusEmpty() but returns the result of the test.

        var bResult = verifyCleanStatus();
        testlib.ok(bResult, msg);

        return bResult;
    }
    obj.statusEmptyWhenIgnoringIgnores = function(msg)
    {
        // Is the status empty when we DON'T use the installed IGNORES list?
        // like testlib.statusEmptyWhenIgnoringIgnores() but returns the result of the test.

        var bResult = verifyCleanStatusWhenIgnoringIgnores();
        testlib.ok(bResult, msg);

        return bResult;
    }

    obj.get_baseline = function()
    {
        var o=sg.pending_tree.parents();
        return o[0];
    }

    obj.assert_current_baseline = function(cset) {
        // call get_baseline and verify that the new current baseline is 
        // the one we asked for.  Fail if it's not.
        var baseline = obj.get_baseline();
        var matchesCurrent = (cset == baseline);
        testlib.ok( (matchesCurrent), "Current baseline: " + baseline.substr(0,16) + " should match expected: " + cset.substr(0,16));
        return matchesCurrent;
    }

    obj.assert_current_baseline_by_name = function(name)
    {
        var ndx = obj.name2ndx[name];
        if (typeof ndx != undefined)
            return obj.assert_current_baseline(obj.csets[ndx]);

        testlib.ok( (0), "Unknown changeset: [" + name + "]" );
        return false;
    }

    obj.assert_clean_WD = function()
    {
        // assert the initial conditions that each test assumes.
        // if they aren't met, return false and allow the test
        // to just fail and stop.
        //
        // since all tests run on the same WD and have cumulative
        // effects (dirt), once a test fails or generates some dirt
        // in the WD, we're kind of hosed -- and every subsequent
        // test will probably be confused.  so just stop.

        var isClean = verifyCleanStatusWhenIgnoringIgnores();
        if (isClean)
            return true;

        testlib.ok( (0), "RequireInitiallyClean not satisfied." );
        dumpTree("Here's the pending_tree:");
        return false;
    }

    obj.force_clean_WD = function()
    {
        var files = new Array();
        var dirs = new Array();
        var index;
        var status;

	if (verifyCleanStatusWhenIgnoringIgnores())
	    return true;

        sg.pending_tree.revert();

	if (verifyCleanStatusWhenIgnoringIgnores())
	    return true;

	// either REVERT failed or there must be FOUND dirt.

	var status = sg.pending_tree.status(["ignore-ignores"]);
        var statFound = status["Found"];
        if (statFound)
	{
            print("## Cleaning FOUND dirt from WD...");

            for (index in statFound)
            {
		if (statFound[index]["type"] == "File") {
                    files.push(statFound[index]["path"]);
		}
		else if (statFound[index]["type"] == "Directory") {
                    dirs.push(statFound[index]["path"]);
		}
            }

            files.sort();
            for (index=0; index<files.length; index++)
            {
		obj.do_fsobj_remove_file(files[index].substr(2));
            }

            dirs.sort();
            for (idx=dirs.length-1; idx>=0; idx--)
            {
		obj.do_fsobj_remove_dir(dirs[idx].rtrim("/").substr(2));
	    }
        }

	if (verifyCleanStatusWhenIgnoringIgnores())
	    return true;

	// so REVERT must have failed.

        sprawlunit.ok( (0), "## force_clean_WD: Unable to clean the Working Directory...");
        dumpTree("## Here is the pending tree:");

	return false;
    }

    obj.do_clean_warp_to_head = function()
    {
        // at the beginning of a test we need to have a well-defined
        // starting point -- in case they run individual tests rather
        // the whole series in order.
        //
        // assert that we have a clean WD and either at the HEAD or
        // warp to the HEAD (and have a clean WD after the warp).
        //
        // return false if we are dirty or cannot get to the head
        // or are dirty after the update.

        if (!obj.assert_clean_WD())
            return false;

        var ndxHead = obj.csets.length - 1;
        if (obj.ndxCur == ndxHead)
            return true;

        return obj.do_update_when_clean(ndxHead);
    }

    //////////////////////////////////////////////////////////////////

    obj.do_check_entry_status = function(status, sectionname, repopath)
    {
        // We assume that "status" contains the results of a full sg.pending_tree.status()
        // call.  This is a treediff2 and divided into sections by verb.  Each section
        // is an array of entries.

        for (var index in status[sectionname])
        {
            if (status[sectionname][index].path.rtrim("/") == repopath)
            {
                status[sectionname][index].claimed = true;
                return true;
            }
        }

        return false;
    }

    obj.confirm_entry_exists_in_section = function(status, sectionname, repopath)
    {
        testlib.ok( obj.do_check_entry_status(status, sectionname, repopath),
                       "Entry [" + repopath + "] present in section [" + sectionname + "]");
    }

    obj.confirm_expectations = function(status, expected)
    {
        for (var e_section in expected)
        {
            for (var e_index in expected[e_section])
            {
                obj.confirm_entry_exists_in_section(status, e_section, expected[e_section][e_index]);
            }
        }
    }

    obj.confirm_entry_not_in_section = function(status, sectionname, repopath)
    {
        testlib.ok( ! obj.do_check_entry_status(status, sectionname, repopath),
                       "Entry [" + repopath + "] not in section [" + sectionname + "]");
    }

    obj.confirm_all_claimed = function(status)
    {
        // Check that we have accounted for each item in the "status".

        for (var section in status)
        {
            for (var index in status[section])
            {
                if (status[section][index].claimed != true)
                {
                    testlib.ok( (0), "Unclaimed entry [" + status[section][index].path + "] in section [" + section + "]" );
                }
            }
        }
    }

    obj.confirm = function(expected)
    {
	indentLine("=> confirm");
        var status = sg.pending_tree.status();
        obj.confirm_expectations(status, expected);
        obj.confirm_all_claimed(status);
    }

    obj.confirm_when_ignoring_ignores = function(expected)
    {
	indentLine("=> confirm_when_ignoring_ignores");
        var status = sg.pending_tree.status(["ignore-ignores"]);
        obj.confirm_expectations(status, expected);
        obj.confirm_all_claimed(status);
    }

    //////////////////////////////////////////////////////////////////

    obj.confirm_on_disk = function(file_list)
    {
        var path;
        for (var index in file_list)
        {
            path = pathCombine(obj.workdir_root, file_list[index].replace("@/",""));
            testlib.ok(sg.fs.exists(path), path + " should exist on disk");
        }
    }

    obj.confirm_not_on_disk = function(file_list)
    {
        var path;
        for (var index in file_list)
        {
            path = pathCombine(obj.workdir_root, file_list[index].replace("@/",""));
            testlib.ok(!sg.fs.exists(path), path + " should not exist on disk");
        }
    }

    //////////////////////////////////////////////////////////////////

    obj.do_fsobj_mkdir = function(path)
    {
        print("fsobj_mkdir " + path);
        sg.fs.mkdir(path);
    }

    obj.do_fsobj_remove_dir = function(path)
    {
        print("fsobj_rmdir " + path);
        // TODO this should not be recursive.  It should throw if 
        // TODO the directory is not empty.
        sg.fs.rmdir(path);
    }

    obj.do_fsobj_create_file = function(pathname, msg)
    {
        print("create " + pathname);
        if (msg) 
        {
            sg.file.write( pathname, ("This is the contents of file " + pathname + "\n"
                                      + msg + "\n"
                                      + new Date().getTime() + "\n" )  );
        }
        else
        {
            sg.file.write( pathname, ("This is the contents of file " + pathname + "\n"
                                      + new Date().getTime() + "\n" )  );
        }
    }

    obj.do_fsobj_create_file_exactly = function(pathname, content)
    {
	print("create exactly " + pathname);
	sg.file.write( pathname, content );
    }

    obj.do_fsobj_append_file = function(pathname, msg)
    {
        print("append " + pathname);
        if (msg) 
        {
            sg.file.append( pathname, ("This is another line in file " + pathname + " with message: " + msg + "\n") );
        }
        else
        {
            sg.file.append( pathname, ("This is another line in file " + pathname + "\n") );
        }
    }

    obj.do_fsobj_remove_file = function(path)
    {
        print("fsobj_remove " + path);
        sg.fs.remove(path);
    }

    obj.do_vc_remove_dir = function(path)
    {
        print("vc_rmdir " + path);
        // TODO this should not be recursive.  It should throw if 
        // TODO the directory is not empty.
        sg.pending_tree.remove(path);
    }

    obj.do_vc_remove_file = function(path)
    {
        print("vc_remove " + path);
        sg.pending_tree.remove(path);
    }

    obj.do_vc_rename = function(path_old,entryname_new)
    {
        print("vc_rename " + path_old + " " + entryname_new);
        sg.pending_tree.rename(path_old,entryname_new);
    }

    obj.do_vc_move = function(path_old,folder_new)
    {
        print("vc_move " + path_old + " " + folder_new);
        sg.pending_tree.move(path_old,folder_new);
    }

    obj.do_addremove_scan = function()
    {
        print("addremove scan...");
        sg.pending_tree.addremove();
    }

    obj.do_add = function(path)
    {
        print("add " + path);
        sg.pending_tree.add(path);
    }

    obj.do_chmod = function(path, octal_value)
    {
        print("chmod " + octal_value.toString(8) + " " + path);
        sg.fs.chmod(path, octal_value);
    }

    obj.do_commit = function(label)
    {
        print("commit: " + label);
        obj.ndxCur = obj.csets.length;
        obj.ndxCur_in_branch = -1;

        var csetid = sg.pending_tree.commit(["message=" + label]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.

        print("commit: [" + label + "] --> [" + csetid + "]");
        print(" ");

        if (csetid == undefined)
        {
            testlib.ok( (0), "Commit did not do anything." );
            // TODO this should probably stop or maybe we should make this function return true/false
        }

        obj.csets.push( csetid );
        obj.names.push( label );

        obj.name2ndx[label] = obj.ndxCur;
    }

    obj.do_commit_in_branch = function(label)
    {
        print("commit in branch: " + label);
        obj.ndxCur = -1;
        obj.ndxCur_in_branch = obj.csets_in_branch.length;

        var csetid = sg.pending_tree.commit(["message=" + label]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.

        print("commit: [" + label + "] --> [" + csetid + "]");
        print(" ");

        if (csetid == undefined)
        {
            testlib.ok( (0), "Commit in branch did not do anything." );
            // TODO this should probably stop or maybe we should make this function return true/false
        }

        obj.csets_in_branch.push( csetid );
        obj.names_in_branch.push( label );

        obj.name2ndx[label] = obj.ndxCur;
    }

    obj.do_update = function(ndxNew)
    {
        var isClean = verifyCleanStatus();

        if ((obj.ndxCur == -1) && (obj.ndxCur_in_branch == -1))
        {
            testlib.ok( (0), "Where are we [branch/trunk]?" );
            return false;
        }

        if (obj.ndxCur >= 0)
        {
            var msg = "Attempting " + (isClean ? "Clean" : "Dirty") + " Update from trunk[" + obj.names[obj.ndxCur] + "] to trunk[" + obj.names[ndxNew] + "]";
        }
        else /* if (obj.ndxCur_in_branch >= 0) */
        {
            var msg = "Attempting " + (isClean ? "Clean" : "Dirty") + " Update from branch[" + obj.names_in_branch[obj.ndxCur_in_branch] + "] to trunk[" + obj.names[ndxNew] + "]";
        }

        print(msg);

        var log = sg.pending_tree.update([ (obj.verbose ? "verbose" : ""), "rev=" + obj.csets[ndxNew] ] );
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.

        obj.ndxCur = ndxNew;
        obj.ndxCur_in_branch = -1;

        if (log != undefined)
            dumpActionLog(msg, log);

        // Verify that the new current baseline is the one we asked for.
        // Return false if either not empty or not right baseline.
        if (!obj.assert_current_baseline(obj.csets[ndxNew]))
            return false;

        if (isClean) {
            var bIsEmpty = obj.statusEmpty(msg);
            return bIsEmpty;
        }
        else
        {
            return true;
        }
    }

    obj.do_update_when_clean = function(ndxNew)
    {
        return obj.do_update(ndxNew);
    }

    obj.do_update_when_clean_by_name = function(name)
    {
        var ndx = obj.name2ndx[name];
        if (typeof ndx != undefined)
            return obj.do_update_when_clean(ndx);

        testlib.ok( (0), "Unknown changeset: [" + name + "]" );
        return false;
    }

    obj.do_update_when_dirty = function(ndxNew)
    {
        return obj.do_update(ndxNew);
    }

    obj.do_update_when_dirty_by_name = function(name)
    {
        var ndx = obj.name2ndx[name];
        if (typeof ndx != undefined)
            return obj.do_update_when_dirty(ndx);

        testlib.ok( (0), "Unknown changeset: [" + name + "]" );
        return false;
    }

    obj.do_update_in_branch = function(ndxNew)
    {
        var isClean = verifyCleanStatus();

        if ((obj.ndxCur == -1) && (obj.ndxCur_in_branch == -1))
        {
            testlib.ok( (0), "Where are we [branch/trunk]?" );
            return false;
        }

        if (obj.ndxCur >= 0)
        {
            var msg = "Attempting " + (isClean ? "Clean" : "Dirty") + " Update from trunk[" + obj.names[obj.ndxCur] + "] to branch[" + obj.names_in_branch[ndxNew] + "]";
        }
        else /* if (obj.ndxCur_in_branch >= 0) */
        {
            var msg = "Attempting " + (isClean ? "Clean" : "Dirty") + " Update from branch[" + obj.names_in_branch[obj.ndxCur_in_branch] + "] to branch[" + obj.names_in_branch[ndxNew] + "]";
        }

        print(msg);

        var log = sg.pending_tree.update([ (obj.verbose ? "verbose" : ""), "rev=" + obj.csets_in_branch[ndxNew] ] );
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.

        obj.ndxCur = -1;
        obj.ndxCur_in_branch = ndxNew;

        if (log != undefined)
            dumpActionLog(msg, log);

        // Verify that the new current baseline is the one we asked for.
        // Return false if either not empty or not right baseline.
        if (!obj.assert_current_baseline(obj.csets_in_branch[ndxNew]))
            return false;

        if (isClean) {
            var bIsEmpty = obj.statusEmpty(msg);
            return bIsEmpty;
        }
        else
        {
            return true;
        }
    }

    obj.do_update_in_branch_when_clean = function(ndxNew)
    {
        return obj.do_update_in_branch(ndxNew);
    }

    obj.do_update_in_branch_when_dirty = function(ndxNew)
    {
        return obj.do_update_in_branch(ndxNew);
    }

    obj.do_revert_all = function()
    {
        var msg = "Attempting Revert All to [" + obj.names[obj.ndxCur] + "]";

        print(msg);

        var log = sg.pending_tree.revert([ (obj.verbose ? "verbose" : "") ]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.

        if (log != undefined)
            dumpActionLog(msg, log);

        // if there were ADDED items, the status will probably report
        // a bunch of FOUND items now.
        //
        // we DO NOT delete them here.  Let our caller test for FOUND
        // items, then delete them, and then test for clean.
    }

    //////////////////////////////////////////////////////////////////

    obj.list_csets = function(label)
    {
        print("ListCSets: " + label);

        for (var k = 0; k < obj.csets.length; k++)
        {
            print("    CSet[" + k + "] is [" + obj.csets[k] + "] " + obj.names[k]);
        }
        print("");
    }

    obj.list_csets_in_branch = function(label)
    {
        print("ListCSetsInBranch: " + label);

        for (var k = 0; k < obj.csets_in_branch.length; k++)
        {
            print("    CSet[" + k + "] is [" + obj.csets_in_branch[k] + "] " + obj.names_in_branch[k]);
        }
        print("");
    }

    //////////////////////////////////////////////////////////////////
    // Create a bunch of changesets to use for the tests.
    //
    // Place a call to this.create_test_changesets(); in "this.Setup" 
    //   if you want to use this sequence of changes.

    obj.generate_test_changesets = function()
    {
        // we begin with a clean WD in a fresh REPO.

        obj.workdir_root = sg.fs.getcwd();        // save this for later

        //////////////////////////////////////////////////////////////////
        // fetch the HID of the initial commit.

        var h = sg.pending_tree.history();
        obj.csets.push( h[0].changeset_id );
        obj.names.push( "*Initial Commit*" );

        //////////////////////////////////////////////////////////////////
        // Create a linear sequence of cummulative changesets.
        // These will be known as "obj.csets[1..n]".
        // Once we have this linear sequence in the DAG, we should be free to:
        // [1] UPDATE between [0] and [n] and back.
        // [2] UPDATE to [k+1] and [k-1] thru the entire list.
        // [3] WARP UPDATE from any changeset to any other changeset.
        // All without problems.
        //
        // Since [3] is very expensive (O(n^2) with large n) I think I'm going
        // to make it a NIGHTLY or LONGTEST.  We need to test all possible
        // combinations of actions, but not every time someone make a change.
        //
        // So, I'm also going to introduce a list of "Pairs of Interest" that
        // will do the [k] vs [j] for things that I want to make sure get tested
        // and not lost in the full combinatorial mess that is [3].
        //////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////
        // create a set of files and directories that we do not change
        // during the linear sequence.

        obj.do_fsobj_mkdir("dir_0");
        obj.do_fsobj_mkdir("dir_0/dir_00");
        obj.do_fsobj_create_file("file_0");
        obj.do_fsobj_create_file("dir_0/file_00");
        obj.do_fsobj_create_file("dir_0/dir_00/file_000");
        obj.do_fsobj_mkdir("dir_alternate_1");
        obj.do_fsobj_mkdir("dir_alternate_2");
        obj.do_fsobj_mkdir("dir_alternate_2/dir_alternate_22");
        obj.do_addremove_scan();
        obj.do_commit("Simple ADDs 0");

        //////////////////////////////////////////////////////////////////
        // create 3 sets of files and directories in 3 csets.

        obj.do_fsobj_mkdir("dir_1");
        obj.do_fsobj_mkdir("dir_1/dir_11");
        obj.do_fsobj_create_file("file_1");
        obj.do_fsobj_create_file("dir_1/file_11");
        obj.do_fsobj_create_file("dir_1/dir_11/file_111");
        obj.do_addremove_scan();
        obj.do_commit("Simple ADDs 1");


        obj.do_fsobj_mkdir("dir_2");
        obj.do_fsobj_mkdir("dir_2/dir_22");
        obj.do_fsobj_create_file("file_2");
        obj.do_fsobj_create_file("dir_2/file_22");
        obj.do_fsobj_create_file("dir_2/dir_22/file_222");
        obj.do_addremove_scan();
        obj.do_commit("Simple ADDs 2");


        obj.do_fsobj_mkdir("dir_3");
        obj.do_fsobj_mkdir("dir_3/dir_33");
        obj.do_fsobj_create_file("file_3");
        obj.do_fsobj_create_file("dir_3/file_33");
        obj.do_fsobj_create_file("dir_3/dir_33/file_333");
        obj.do_fsobj_mkdir("dir_3_alternate");
        obj.do_fsobj_mkdir("dir_3_alternate_again");
        obj.do_addremove_scan();
        obj.do_commit("Simple ADDs 3");

        //////////////////////////////////////////////////////////////////
        // undo creation of 1 in steps.

        obj.do_vc_remove_file("dir_1/dir_11/file_111");
        obj.do_vc_remove_file("dir_1/file_11");
        obj.do_vc_remove_file("file_1");
        obj.do_commit("Simple DELETEs 1");


        obj.do_vc_remove_dir("dir_1/dir_11");
        obj.do_vc_remove_dir("dir_1");
        obj.do_commit("Simple RMDIRs 1");

        //////////////////////////////////////////////////////////////////
        // begin a sequence of safe file renames on 2.

        var pair = new Array();                // the cummulative effect of these 3 RENAME csets should be
        pair.a = obj.ndxCur;                // a wash (different cset HID but identical actual contents)


        obj.do_vc_rename("file_2",               "file_2_renamed");
        obj.do_vc_rename("dir_2/file_22",        "file_22_renamed");
        obj.do_vc_rename("dir_2/dir_22/file_222","file_222_renamed");
        obj.do_commit("Simple file RENAMEs 2");


        obj.do_vc_rename("file_2_renamed",               "file_2_renamed_again");
        obj.do_vc_rename("dir_2/file_22_renamed",        "file_22_renamed_again");
        obj.do_vc_rename("dir_2/dir_22/file_222_renamed","file_222_renamed_again");
        obj.do_commit("Simple file RENAMEs 2 again");


        obj.do_vc_rename("file_2_renamed_again",               "file_2");
        obj.do_vc_rename("dir_2/file_22_renamed_again",        "file_22");
        obj.do_vc_rename("dir_2/dir_22/file_222_renamed_again","file_222");
        obj.do_commit("Simple file RENAMEs 2 original");


        pair.b = obj.ndxCur;
        obj.pairs.push( pair );

        //////////////////////////////////////////////////////////////////
        // begin a sequence of safe file moves on 3.

        var pair = new Array();
        pair.a = obj.ndxCur;


        obj.do_vc_move("file_3",                "dir_3_alternate");
        obj.do_vc_move("dir_3/file_33",         "dir_3_alternate");
        obj.do_vc_move("dir_3/dir_33/file_333", "dir_3_alternate");
        obj.do_commit("Simple file MOVEs 3");


        obj.do_vc_move("dir_3_alternate/file_3",   "dir_3_alternate_again");
        obj.do_vc_move("dir_3_alternate/file_33",  "dir_3_alternate_again");
        obj.do_vc_move("dir_3_alternate/file_333", "dir_3_alternate_again");
        obj.do_commit("Simple file MOVEs 3 again");


        obj.do_vc_move("dir_3_alternate_again/file_3",   ".");
        obj.do_vc_move("dir_3_alternate_again/file_33",  "dir_3");
        obj.do_vc_move("dir_3_alternate_again/file_333", "dir_3/dir_33");
        obj.do_commit("Simple file MOVEs 3 original");


        obj.do_vc_remove_dir("dir_3_alternate_again");
        obj.do_vc_remove_dir("dir_3_alternate");
        obj.do_commit("Simple RMDIR 3 alternates");


        pair.b = obj.ndxCur;
        obj.pairs.push( pair );

        //////////////////////////////////////////////////////////////////
        // create file and directory set 4 and then do a safe sequence
        // of directory renames.

        obj.do_fsobj_mkdir("dir_4");
        obj.do_fsobj_mkdir("dir_4/dir_44");
        obj.do_fsobj_create_file("file_4");
        obj.do_fsobj_create_file("dir_4/file_44");
        obj.do_fsobj_create_file("dir_4/dir_44/file_444");
        obj.do_addremove_scan();
        obj.do_commit("Simple ADDs 4");


        obj.do_vc_rename("dir_4/dir_44", "dir_44_renamed");
        obj.do_vc_rename("dir_4",        "dir_4_renamed");
        obj.do_commit("Simple directory RENAMEs 4");


        obj.do_vc_rename("dir_4_renamed/dir_44_renamed", "dir_44_renamed_again");
        obj.do_vc_rename("dir_4_renamed",                "dir_4_renamed_again");
        obj.do_commit("Simple directory RENAMEs 4 again");


        obj.do_vc_rename("dir_4_renamed_again/dir_44_renamed_again", "dir_44");
        obj.do_vc_rename("dir_4_renamed_again",                "dir_4");
        obj.do_commit("Simple directory RENAMEs 4 original");

        //////////////////////////////////////////////////////////////////
        // create file and directory set 5 and then do some safe sequence
        // of directory moves.

        obj.do_fsobj_mkdir("dir_5");
        obj.do_fsobj_mkdir("dir_5/dir_55");
        obj.do_fsobj_create_file("file_5");
        obj.do_fsobj_create_file("dir_5/file_55");
        obj.do_fsobj_create_file("dir_5/dir_55/file_555");
        obj.do_fsobj_mkdir("dir_5_alternate");
        obj.do_fsobj_mkdir("dir_5_alternate_again");
        obj.do_addremove_scan();
        obj.do_commit("Simple ADDs 5");


        obj.do_vc_move("dir_5/dir_55", "dir_5_alternate");
        obj.do_vc_move("dir_5",        "dir_5_alternate");
        obj.do_commit("Simple directory MOVEs 5");


        obj.do_vc_move("dir_5_alternate/dir_55", "dir_5_alternate_again");
        obj.do_vc_move("dir_5_alternate/dir_5",  "dir_5_alternate_again");
        obj.do_commit("Simple directory MOVEs 5 again");


        obj.do_vc_move("dir_5_alternate_again/dir_55", "dir_5_alternate_again/dir_5");
        obj.do_vc_move("dir_5_alternate_again/dir_5",  ".");
        obj.do_commit("Simple directory MOVEs 5 original");

        //////////////////////////////////////////////////////////////////
        // create file and directory set 6 and then make some edits.

        obj.do_fsobj_mkdir("dir_6");
        obj.do_fsobj_mkdir("dir_6/dir_66");
        obj.do_fsobj_create_file("file_6");
        obj.do_fsobj_create_file("dir_6/file_66");
        obj.do_fsobj_create_file("dir_6/dir_66/file_666");
        obj.do_addremove_scan();
        obj.do_commit("Simple ADDs 6");


        obj.do_fsobj_append_file("file_6",                "hello1");
        obj.do_fsobj_append_file("dir_6/file_66",         "hello1");
        obj.do_fsobj_append_file("dir_6/dir_66/file_666", "hello1");
        obj.do_commit("Simple file MODIFY 6.1");


        obj.do_fsobj_append_file("file_6",                "hello2");
        obj.do_fsobj_append_file("dir_6/file_66",         "hello2");
        obj.do_fsobj_append_file("dir_6/dir_66/file_666", "hello2");
        obj.do_commit("Simple file MODIFY 6.2");


        obj.do_fsobj_append_file("file_6",                "hello3");
        obj.do_fsobj_append_file("dir_6/file_66",         "hello3");
        obj.do_fsobj_append_file("dir_6/dir_66/file_666", "hello3");
        obj.do_commit("Simple file MODIFY 6.3");

        //////////////////////////////////////////////////////////////////
        // create file and directory set 7 and then do a safe combination
        // of moves and renames.

        obj.do_fsobj_mkdir("dir_7");
        obj.do_fsobj_mkdir("dir_7_alternate");
        obj.do_fsobj_mkdir("dir_7/dir_77");
        obj.do_fsobj_mkdir("dir_7/dir_77_alternate");
        obj.do_fsobj_create_file("dir_7/dir_77/file_777");
        obj.do_addremove_scan();
        obj.do_commit("Simple ADDs 7");


        obj.do_vc_rename("dir_7/dir_77/file_777",         "file_777_renamed");
        obj.do_vc_move(  "dir_7/dir_77/file_777_renamed", "dir_7/dir_77_alternate");
        obj.do_vc_rename("dir_7/dir_77_alternate",        "dir_77_renamed");
        obj.do_vc_move(  "dir_7/dir_77_renamed",          "dir_7_alternate");
        obj.do_commit("Simple file RENAMEs + MOVEs 7");

        //////////////////////////////////////////////////////////////////
        // create file set 8 and then do "swqp" type renames

        obj.do_fsobj_create_file("file_8_1");
        obj.do_fsobj_create_file("file_8_2");
        obj.do_addremove_scan();
        obj.do_commit("Simple ADDs 8");


        obj.do_vc_rename("file_8_1",    "file_8_swap");
        obj.do_vc_rename("file_8_2",    "file_8_1");
        obj.do_vc_rename("file_8_swap", "file_8_2");
        obj.do_commit("Swap file RENAMEs 8");

        //////////////////////////////////////////////////////////////////
        // create directory set 9 and then do "swqp" type renames

        obj.do_fsobj_mkdir("dir_9_1");
        obj.do_fsobj_mkdir("dir_9_2");
        obj.do_addremove_scan();
        obj.do_commit("Simple ADDs 9");


        obj.do_vc_rename("dir_9_1",    "dir_9_swap");
        obj.do_vc_rename("dir_9_2",    "dir_9_1");
        obj.do_vc_rename("dir_9_swap", "dir_9_2");
        obj.do_commit("Swap directory RENAMEs 9");

        //////////////////////////////////////////////////////////////////

        // TODO create something with 6 or more levels deep "@/a/b/c/d/e/f/g/..."
        // TODO and then do MOVES, RENAMES, MOVES+RENAMES on multiple components
        // TODO within the pathname at the same time.
        //
        // TODO do swap stuff to "@/a/b/c/d/e/f/g/..." that will require parking.

        //////////////////////////////////////////////////////////////////

        if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
        {
            // test setting/clearing AttrBits.
            // these only make sense on Linux and Mac.

            obj.do_fsobj_mkdir("dir_attrbits");
            obj.do_fsobj_create_file("dir_attrbits/file_attrbits_1");
            obj.do_addremove_scan();
            obj.do_commit("Start AttrBits Test");
var x = sg.exec("/bin/ls", "-la", "dir_attrbits");
print(x.stdout);

            // chmod +x on an existing file
            obj.do_chmod("dir_attrbits/file_attrbits_1", 0700);
            obj.do_commit("AttrBits Chmod +x");
var x = sg.exec("/bin/ls", "-la", "dir_attrbits");
print(x.stdout);


            // create +x file before we commit it.
            obj.do_fsobj_create_file("dir_attrbits/file_attrbits_2");
            obj.do_chmod("dir_attrbits/file_attrbits_2", 0700);
            obj.do_addremove_scan();
            obj.do_commit("AttrBits Create with +x");
var x = sg.exec("/bin/ls", "-la", "dir_attrbits");
print(x.stdout);


            // turn off x bit.
            obj.do_chmod("dir_attrbits/file_attrbits_1", 0600);
            obj.do_chmod("dir_attrbits/file_attrbits_2", 0600);
            obj.do_commit("AttrBits Off");
var x = sg.exec("/bin/ls", "-la", "dir_attrbits");
print(x.stdout);


        }

        // TODO test AttrBits, XAttrs, Symlinks.
        //////////////////////////////////////////////////////////////////
        obj.list_csets("After setup (from update_helpers.generate_test_changesets())");
                 print("------------------------------------------------------");
    }

}
