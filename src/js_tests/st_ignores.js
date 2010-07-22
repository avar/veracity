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
// These two tests once lived in stSmokeTests.js 
// They were relocated here because non-functional "ignores" were
// causing them to fail.  Failure of these tests risked masking
// other more serious failures that might occur.
//
// When "ignores" is fixed, these can go back to stSmokeTest, or
// they can just stay here.  But when that happens, there are a 
// couple lines in stUpdateTestsWithDirtyWD.js that should be
// un-commented.  The lines are marked with: IGNORES_FAILING
//////////////////////////////////////////////////////////////////

function st_ignores() 
{
	// Helper functions are in update_helpers.js
	// There is a reference list at the top of the file.
	// After loading, you must call initialize_update_helpers(this)
	// before you can use the helpers.
	load("update_helpers.js");			// load the helper functions
	initialize_update_helpers(this);	// initialize helper functions

    this.testSimpleIgnores = function testSimpleIgnores() {
        var o;
        var ignoreSuffix = ".ignorable";
        var base = createFolderOnDisk(getTestName(arguments.callee.toString()));
        addRemoveAndCommit();
        var folder = createFolderOnDisk(pathCombine(base, "folder" + ignoreSuffix));
        var file = createFileOnDisk(pathCombine(folder, "file" + ignoreSuffix), 20);
        
        // folder and folder/file should show up as Adds.  We'll just check for anything...
        testlib.statusDirty();
        // now set an ignores for ignoreSuffix
        o = sg.exec("vv", "localsetting", "add-to", "ignores", ignoreSuffix);
        if (o.exit_status != 0)
        {
            print(sg.to_json(o));
            testlib.ok( (0), '## Unable to add "' + ignoreSuffix + '" to localsetting ignores.' );
            print(sg.exec("vv", "localsettings").stdout);
            return false;
        }
        // and see if it worked
        testlib.statusEmpty();
        dumpTree("Should be ignoring: " + ignoreSuffix );

        // status of controlled files should be unaffected, however
        o = sg.exec("vv", "localsetting", "remove-from", "ignores", ignoreSuffix);
        if (o.exit_status != 0) 
        {
            print(sg.to_json(o));
            testlib.ok( (0), '## Unable to remove "' + ignoreSuffix + '" from localsetting ignores.' );
            return false;
        }
        addRemoveAndCommit();
        o = sg.exec("vv", "localsetting", "add-to", "ignores", ignoreSuffix);
        if (o.exit_status != 0)
        {
            print(sg.to_json(o));
            testlib.ok( (0), '## Unable to add "' + ignoreSuffix + '" to localsetting ignores.' );
            print(sg.exec("vv", "localsettings").stdout);
            return false;
        }
        doRandomEditToFile(file);
        testlib.statusDirty();

        // ok. now clean up
        commit();
        o = sg.exec("vv", "localsetting", "remove-from", "ignores", ignoreSuffix);
        if (o.exit_status != 0) 
        {
            print(sg.to_json(o));
            testlib.ok( (0), '## Unable to remove "' + ignoreSuffix + '" from localsetting ignores.' );
            return false;
        }
        testlib.statusEmpty();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    this.ignores = function(cmd, value) {
        var o;
        if (cmd == "reset") {
            o = sg.exec("vv", "localsetting", "reset", "ignores");
            if (o.exit_status != 0) 
            {
                print(sg.to_json(o));
                testlib.ok( (0), '## Unable to reset localsetting ignores.' );
                return false;
            }
        } else {
            o = sg.exec("vv", "localsetting", cmd, "ignores", value);
            if (o.exit_status != 0)
            {
                print(sg.to_json(o));
                testlib.ok( (0), '## Ignores: unable to ' + cmd + '"' + value + '"' );
                print(sg.exec("vv", "localsettings").stdout);
                return false;
            }
        }
        return true;
    }

    this.testDeeperIgnores = function testDeeperIgnores() {
        // this is a much more thorough test of ignores,
        // verifying that ignores don't apply to version-controlled items
        var xf = ".iFile";
        var xd = ".iDir";
        var tree_status;

        // start with a clean set of ignores
        this.ignores("reset");

        var base = createFolderOnDisk(getTestName(arguments.callee.toString()));
        addRemoveAndCommit();

        // ignores ON
        this.ignores("add-to", xf);
        this.ignores("add-to", xd);

        var mod_1 = createFileOnDisk(pathCombine(base, "mod_1" + xf), 20);
        var mov_1 = createFileOnDisk(pathCombine(base, "mov_1" + xf), 20);
        var ren_1 = createFileOnDisk(pathCombine(base, "ren_1" + xf), 20);
        var del_1 = createFileOnDisk(pathCombine(base, "del_1" + xf), 20);
        var dir_1 = createFolderOnDisk(pathCombine(base, "dir_1" + xd));
        var mod_11 = createFileOnDisk(pathCombine(dir_1, "mod_11" + xf), 20);
        var mov_11 = createFileOnDisk(pathCombine(dir_1, "mov_11" + xf), 20);
        var ren_11 = createFileOnDisk(pathCombine(dir_1, "ren_11" + xf), 20);
        var del_11 = createFileOnDisk(pathCombine(dir_1, "del_11" + xf), 20);
        var dir_A = createFolderOnDisk(pathCombine(base, "dir_A" + xd));
        var mod_A1 = createFileOnDisk(pathCombine(dir_A, "mod_A1" + xf), 20);
        var mov_A1 = createFileOnDisk(pathCombine(dir_A, "mov_A1" + xf), 20);
        var ren_A1 = createFileOnDisk(pathCombine(dir_A, "ren_A1" + xf), 20);
        var del_A1 = createFileOnDisk(pathCombine(dir_A, "del_A1" + xf), 20);
        var dir_Dest = createFolderOnDisk(pathCombine(base, "dir_Dest" + xd));

		// all of the dirt we just created has a suffix that we currently ignore
		// so the status should be empty.
        tree_status = new Array;
        this.confirm(tree_status);

        // ignores OFF
        this.ignores("reset");

		// We cancelled the suffix ignores, so all of the dirt we created
		// should now appear as FOUND items.

        tree_status = new Array;
        tree_status["Found"] = [ "@/" + mod_1 ,
                                 "@/" + mov_1 ,
                                 "@/" + ren_1 ,
                                 "@/" + del_1 ,
                                 "@/" + dir_1 ,
                                 "@/" + mod_11 ,
                                 "@/" + mov_11 ,
                                 "@/" + ren_11 ,
                                 "@/" + del_11 ,
                                 "@/" + dir_A ,
                                 "@/" + mod_A1 ,
                                 "@/" + mov_A1 ,
                                 "@/" + ren_A1 ,
                                 "@/" + del_A1 ,
                                 "@/" + dir_Dest ];
        this.confirm(tree_status);

		// ADDREMOVE will convert the FOUNDs to ADDEDs.
        this.do_addremove_scan();

		// Add the suffixes back to the IGNORES list and
        // confirm that STATUS does not filter out ADDED items.
		// That is, only stuff listed in FOUND should be affected,
		// not stuff listed in ADDED.
        this.ignores("add-to", xf);
        this.ignores("add-to", xd);
        tree_status = new Array;
        tree_status["Added"] = [ "@/" + mod_1 ,
                                 "@/" + mov_1 ,
                                 "@/" + ren_1 ,
                                 "@/" + del_1 ,
                                 "@/" + dir_1 ,
                                 "@/" + mod_11 ,
                                 "@/" + mov_11 ,
                                 "@/" + ren_11 ,
                                 "@/" + del_11 ,
                                 "@/" + dir_A ,
                                 "@/" + mod_A1 ,
                                 "@/" + mov_A1 ,
                                 "@/" + ren_A1 ,
                                 "@/" + del_A1 ,
                                 "@/" + dir_Dest ];
        this.confirm(tree_status);

		// Commit all of the ADDED stuff.  This implicitly tests that COMMIT
		// properly limits ignores.

        commit();

		// Verify that COMMIT got everything that we ADDED.
        tree_status = new Array;
        this.confirm(tree_status);

		// Just to be safe, also verify that there is no other dirt.
        tree_status = new Array;
        this.confirm_when_ignoring_ignores(tree_status);

		////////////////////////////////////////////////////////////////
        // Now, mod, move, rename, remove, add, etc.
		// This is testing that each of these command verbs
		// do not get confused and try to ignore items under
		// version control.

        var mod_1_x = mod_1;
        doRandomEditToFile(mod_1);
        var mov_1_x = pathCombine(dir_Dest, getFileNameFromPath(mov_1));
        this.do_vc_move(mov_1, dir_Dest);
        var ren_1_x = pathCombine(getParentPath(ren_1), "ren_NEW" + xf);
        this.do_vc_rename(ren_1, "ren_NEW" + xf);
        var del_1_x = del_1;
        this.do_vc_remove_file(del_1);
        var add_1_x = createFileOnDisk(pathCombine(base, "add_1" + xf), 20);

        var mod_11_x = mod_11;
        doRandomEditToFile(mod_11);
        var mov_11_x = pathCombine(dir_Dest, getFileNameFromPath(mov_11));
        this.do_vc_move(mov_11, dir_Dest);
        var ren_11_x = pathCombine(getParentPath(ren_11), "ren_NEW" + xf);
        this.do_vc_rename(ren_11, "ren_NEW" + xf);
        var del_11_x = del_11;
        this.do_vc_remove_file(del_11);
        var add_11_x = createFileOnDisk(pathCombine(dir_1, "add_11" + xf), 20);
        var mod_A1_x = mod_A1;
        doRandomEditToFile(mod_A1);
        var mov_A1_x = pathCombine(dir_Dest, getFileNameFromPath(mov_A1));
        print("MOVE: " + mov_A1 + " to " + dir_Dest);
        this.do_vc_move(mov_A1, dir_Dest);
        var ren_A1_x = pathCombine(getParentPath(ren_A1), "ren_NEW" + xf);
        this.do_vc_rename(ren_A1, "ren_NEW" + xf);
        var del_A1_x = del_A1;
        this.do_vc_remove_file(del_A1);
        var add_A1_x = createFileOnDisk(pathCombine(dir_A, "add_A1" + xf), 20);

        var dir_B = pathCombine(getParentPath(dir_A), "dir_B");
        mod_A1_x = pathCombine(dir_B, getFileNameFromPath(mod_A1_x));
        ren_A1_x = pathCombine(dir_B, getFileNameFromPath(ren_A1_x));
        del_A1_x = pathCombine(dir_B, getFileNameFromPath(del_A1_x));
        add_A1_x = pathCombine(dir_B, getFileNameFromPath(add_A1_x));
        this.do_vc_rename(dir_A, "dir_B");

        // all items should be represented except the Found ones
        tree_status = new Array;
        tree_status["Removed"]  = [ "@/" + del_1_x ,
                                    "@/" + del_11_x ,
                                    "@/" + del_A1_x ];
        tree_status["Modified"] = [ "@/" + mod_1_x ,
                                    "@/" + mod_11_x ,
                                    "@/" + mod_A1_x ];
        tree_status["Renamed"]  = [ "@/" + ren_1_x ,
                                    "@/" + ren_11_x ,
                                    "@/" + ren_A1_x ,
                                    "@/" + dir_B ];
        tree_status["Moved"]    = [ "@/" + mov_1_x ,
                                    "@/" + mov_11_x ,
                                    "@/" + mov_A1_x ];
        this.confirm(tree_status);
        commit();

        tree_status = new Array;
        this.confirm(tree_status);

        // turn off the ignores ...
        this.ignores("reset");

        // and the Found items reappear
        tree_status = new Array;
        tree_status["Found"]    = [ "@/" + add_1_x ,
                                    "@/" + add_11_x ,
                                    "@/" + add_A1_x ];
        this.confirm(tree_status);

        addRemoveAndCommit();
    }

}
