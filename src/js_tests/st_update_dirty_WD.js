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
// Some not-so-basic unit tests for SG_pendingtree__update()
//
// We build a series of changesets and then add some DIRTY to the
// Working Directory and then make a series of UPDATEs and verify
// that:
// [1] the dirty stuff in the WD is preserved (that the dirt is 
//     rolled-forward into the new/target changeset)
// [2] that non-dirty contents of the WD are updated with the new
//     versions in the target changeset.
//
// The dirt that we create in the WD is INDEPENDENT of the changes
// in the sequence of changesets, so there should not be any 
// conflicts/collisions/issues.
//////////////////////////////////////////////////////////////////

var globalThis;  // var to hold "this" pointers

function stUpdateTestsWithDirtyWD() 
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
        this.workdir_root = sg.fs.getcwd();     // save this for later

        // we begin with a clean WD in a fresh REPO.
        // and build a sequence of changesets to use

        // We'll use the sequence from update_helpers.js
        this.generate_test_changesets();
    }


    this.test_clean_linearly = function()
    {
        if (testlib.defined.SG_LONGTESTS != true)
        {
            // since the list of changesets created in setUp is basically the same
            // as in stUpdateTestsWithCleanWD.js, we don't need to repeat
            // this test here (at least for short runs).
            print("Skipping tests because SG_LONGTESTS not set.");
            return;
        }

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
        if (testlib.defined.SG_LONGTESTS != true)
        {
            // since the list of changesets created in setUp is basically the same
            // as in stUpdateTestsWithCleanWD.js, we don't need to repeat
            // this test here (at least for short runs).
            print("Skipping tests because SG_LONGTESTS not set.");
            return;
        }

        // assuming we have a clean WD.
        // we don't care what the starting baseline is.
        // go thru the list of "pairs of interest" and bounce between them.
        // this is a subset of the full combinatorial.

        if (!this.force_clean_WD())
            return false;

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

    //////////////////////////////////////////////////////////////////
    // basic dirty tests.  the dirt created by these should be completely
    // independent of anything done in the sequence of changesets.
    //////////////////////////////////////////////////////////////////

    this.test_basic_dirty_added = function()
    {
        // assuming we have a clean WD.
        // we don't care what the starting baseline is.
        // do clean update to the changeset that we want to begin the test with.
        // ADD some dirt to the WD.
        // do dirty-update to various target changesets and each time confirm 
        // that the dirt got pulled forward and that the rest of the changeset
        // was changed.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return false;

        // create the dirt as a series of ADDED entries.

        this.do_fsobj_mkdir("dirt_dir_a");
        this.do_fsobj_mkdir("dirt_dir_a/dirt_dir_aa");
        this.do_fsobj_mkdir("dirt_dir_a/dirt_dir_aa/dirt_dir_aaa");
        this.do_fsobj_create_file("dirt_file_a");
        this.do_fsobj_create_file("dirt_dir_a/dirt_file_aa");
        this.do_fsobj_create_file("dirt_dir_a/dirt_dir_aa/dirt_file_aaa");
        this.do_addremove_scan();

        var expect_test = new Array;
        expect_test["Added"] = [ "@/dirt_dir_a",
                                 "@/dirt_dir_a/dirt_dir_aa",
                                 "@/dirt_dir_a/dirt_dir_aa/dirt_dir_aaa",
                                 "@/dirt_file_a",
                                 "@/dirt_dir_a/dirt_file_aa",
                                 "@/dirt_dir_a/dirt_dir_aa/dirt_file_aaa" ];
        this.confirm(expect_test);

        // now do various dirty updates and verify that the dirt got carried forward.

        var ndxHead = this.csets.length - 1;
        while (this.ndxCur < ndxHead)
        {
            if (!this.do_update_when_dirty(this.ndxCur + 1))
                return;

            this.confirm(expect_test);
        }

        // revert the dirt.  REVERT should un-ADD the things from the (now) current baseline.
        // it should not remove them from the disk, so afterwards they should appear as FOUND.

        this.do_revert_all();

        var expect_revert = new Array;
        expect_revert["Found"] = [ "@/dirt_dir_a",
                                   "@/dirt_dir_a/dirt_dir_aa",
                                   "@/dirt_dir_a/dirt_dir_aa/dirt_dir_aaa",
                                   "@/dirt_file_a",
                                   "@/dirt_dir_a/dirt_file_aa",
                                   "@/dirt_dir_a/dirt_dir_aa/dirt_file_aaa" ];
        this.confirm(expect_revert);

        // clean up after this test by actually deleting the stuff from the WD
        // so that the next test can start with a clean WD and be completely
        // independently of this test.

        this.do_fsobj_remove_file("dirt_dir_a/dirt_dir_aa/dirt_file_aaa");
        this.do_fsobj_remove_file("dirt_dir_a/dirt_file_aa");
        this.do_fsobj_remove_file("dirt_file_a");
        this.do_fsobj_remove_dir("dirt_dir_a/dirt_dir_aa/dirt_dir_aaa");
        this.do_fsobj_remove_dir("dirt_dir_a/dirt_dir_aa");
        this.do_fsobj_remove_dir("dirt_dir_a");

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }

    this.test_basic_dirty_removed = function()
    {
        // assuming we have a clean WD.
        // we don't care what the starting baseline is.
        // do clean update to the changeset that we want to begin the test with.
        // DELETE some stuff from the WD.
        // do dirty-update to various target changesets and each time confirm 
        // that the dirt got pulled forward and that the rest of the changeset
        // was changed.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return false;

        // create the dirt as a series of DELETED entries.
        // this removes the files both from version control and the WD.

        this.do_vc_remove_file("file_0");
        this.do_vc_remove_file("dir_0/file_00");
        this.do_vc_remove_file("dir_0/dir_00/file_000");
        this.do_vc_remove_dir("dir_0/dir_00");
        this.do_vc_remove_dir("dir_0");

        var expect_test = new Array;
        expect_test["Removed"] = [ "@/file_0",
                                   "@/dir_0/file_00",
                                   "@/dir_0/dir_00/file_000",
                                   "@/dir_0/dir_00",
                                   "@/dir_0" ];
        this.confirm(expect_test);

        // now do various dirty updates and verify that the dirt got carried forward.

        var ndxHead = this.csets.length - 1;
        while (this.ndxCur < ndxHead)
        {
            if (!this.do_update_when_dirty(this.ndxCur + 1))
                return;

            this.confirm(expect_test);
        }

        // revert the dirt.  REVERT should re-ADD the things to the WD, so the
        // status should be empty.

        this.do_revert_all();

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }

    this.test_basic_dirty_renamed = function()
    {
        // assuming we have a clean WD.
        // we don't care what the starting baseline is.
        // do clean update to the changeset that we want to begin the test with.
        // RENAME some stuff from the WD.
        // do dirty-update to various target changesets and each time confirm 
        // that the dirt got pulled forward and that the rest of the changeset
        // was changed.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return false;

        // create the dirt as a series of RENAMES.

        this.do_vc_rename("file_0",                    "renamed_file_0");
        this.do_vc_rename("dir_0/file_00",             "renamed_file_00");
        this.do_vc_rename("dir_0/dir_00/file_000",     "renamed_file_000");
        this.do_vc_rename("dir_0/dir_00",              "renamed_dir_00");
        this.do_vc_rename("dir_0",                     "renamed_dir_0");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/renamed_file_0",
                                   "@/renamed_dir_0/renamed_file_00",
                                   "@/renamed_dir_0/renamed_dir_00/renamed_file_000",
                                   "@/renamed_dir_0/renamed_dir_00",
                                   "@/renamed_dir_0" ];
        this.confirm(expect_test);

        // now do various dirty updates and verify that the dirt got carried forward.

        var ndxHead = this.csets.length - 1;
        while (this.ndxCur < ndxHead)
        {
            if (!this.do_update_when_dirty(this.ndxCur + 1))
                return;

            this.confirm(expect_test);
        }

        // revert the dirt.  REVERT should undo everything in the WD, so the
        // status should be empty.

        this.do_revert_all();

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }

    this.test_basic_dirty_moved = function()
    {
        // assuming we have a clean WD.
        // we don't care what the starting baseline is.
        // do clean update to the changeset that we want to begin the test with.
        // MOVED some stuff from the WD.
        // do dirty-update to various target changesets and each time confirm 
        // that the dirt got pulled forward and that the rest of the changeset
        // was changed.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return false;

        // create the dirt as a series of MOVES.

        this.do_fsobj_mkdir("dir_moved_to_0");
        this.do_addremove_scan();
        this.do_vc_move("file_0",                    "dir_moved_to_0");
        this.do_vc_move("dir_0/file_00",             "dir_moved_to_0");
        this.do_vc_move("dir_0/dir_00/file_000",     "dir_moved_to_0");
        this.do_vc_move("dir_0/dir_00",              "dir_moved_to_0");
        this.do_vc_move("dir_0",                     "dir_moved_to_0");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_moved_to_0/file_0",
                                 "@/dir_moved_to_0/file_00",
                                 "@/dir_moved_to_0/file_000",
                                 "@/dir_moved_to_0/dir_00",
                                 "@/dir_moved_to_0/dir_0" ];
        expect_test["Added"] = [ "@/dir_moved_to_0" ];
        this.confirm(expect_test);

        // now do various dirty updates and verify that the dirt got carried forward.

        var ndxHead = this.csets.length - 1;
        while (this.ndxCur < ndxHead)
        {
            if (!this.do_update_when_dirty(this.ndxCur + 1))
                return;

            this.confirm(expect_test);
        }

        // revert the dirt.  REVERT should undo the MOVES, but not the ADD.

        this.do_revert_all();

        var expect_revert = new Array;
        expect_revert["Found"] = [ "@/dir_moved_to_0" ];
        this.confirm(expect_revert);

        // clean up the ADDs.

        this.do_fsobj_remove_dir("dir_moved_to_0");

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }

    this.test_basic_dirty_file_mod = function()
    {
        // assuming we have a clean WD.
        // we don't care what the starting baseline is.
        // do clean update to the changeset that we want to begin the test with.
        // MODIFY some files.
        // do dirty-update to various target changesets and each time confirm 
        // that the dirt got pulled forward and that the rest of the changeset
        // was changed.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return false;

        // create the dirt as a series of FILE MODIFICATIONS.

        this.do_fsobj_append_file("file_0",                "hello0");
        this.do_fsobj_append_file("dir_0/file_00",         "hello0");
        this.do_fsobj_append_file("dir_0/dir_00/file_000", "hello0");

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/file_0",
                                    "@/dir_0/file_00",
                                    "@/dir_0/dir_00/file_000" ];
        this.confirm(expect_test);

        // now do various dirty updates and verify that the dirt got carried forward.

        var ndxHead = this.csets.length - 1;
        while (this.ndxCur < ndxHead)
        {
            if (!this.do_update_when_dirty(this.ndxCur + 1))
                return;

            this.confirm(expect_test);
        }

        // revert the dirt.  REVERT should undo the edits by doing a backup.

        this.do_revert_all();

        var backup_suffix = "~sg00~";
        var expect_revert = new Array;
        expect_revert["Found"] = [ "@/file_0"                + backup_suffix,
                                   "@/dir_0/file_00"         + backup_suffix,
                                   "@/dir_0/dir_00/file_000" + backup_suffix ];
//        backups are ignored in status, so we'll use the filesystem
//        this.confirm(expect_revert);
        this.confirm_on_disk(expect_revert["Found"]);
        // this status check fails if ignores are not working
        // (it's really a redundant check anyway)
        this.statusEmpty("Status should be empty.");


        // cleanup

        this.do_fsobj_remove_file("dir_0/dir_00/file_000" + backup_suffix);
        this.do_fsobj_remove_file("dir_0/file_00"         + backup_suffix);
        this.do_fsobj_remove_file("file_0"                + backup_suffix);

//        backups are ignored in status, so we'll use the filesystem
//        var expect_nothing = new Array;
//        this.confirm(expect_nothing);
        this.confirm_not_on_disk(expect_revert["Found"]);
        this.statusEmpty("Status should be empty.");
    }

    this.test_basic_dirty_found = function()
    {
        // assuming we have a clean WD.
        // we don't care what the starting baseline is.
        // do clean update to the changeset that we want to begin the test with.
        // create some files, but don't ADD them -- so that they will be FOUND.
        // do dirty-update to various target changesets and each time confirm 
        // that the dirt got pulled forward and that the rest of the changeset
        // was changed.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return false;

        // create the dirt as a series of ADDED entries.

        this.do_fsobj_mkdir("dirt_dir_a");
        this.do_fsobj_mkdir("dirt_dir_a/dirt_dir_aa");
        this.do_fsobj_mkdir("dirt_dir_a/dirt_dir_aa/dirt_dir_aaa");
        this.do_fsobj_create_file("dirt_file_a");
        this.do_fsobj_create_file("dirt_dir_a/dirt_file_aa");
        this.do_fsobj_create_file("dirt_dir_a/dirt_dir_aa/dirt_file_aaa");
        // do not scan, so they they will be FOUND -- this.do_addremove_scan();

        var expect_test = new Array;
        expect_test["Found"] = [ "@/dirt_dir_a",
                                 "@/dirt_dir_a/dirt_dir_aa",
                                 "@/dirt_dir_a/dirt_dir_aa/dirt_dir_aaa",
                                 "@/dirt_file_a",
                                 "@/dirt_dir_a/dirt_file_aa",
                                 "@/dirt_dir_a/dirt_dir_aa/dirt_file_aaa" ];
        this.confirm(expect_test);

        // now do various dirty updates and verify that the dirt got carried forward.

        var ndxHead = this.csets.length - 1;
        while (this.ndxCur < ndxHead)
        {
            if (!this.do_update_when_dirty(this.ndxCur + 1))
                return;

            this.confirm(expect_test);
        }

        // revert the dirt.  REVERT back to baseline; the FOUND items should still be there.
        // it should not remove them from the disk, so afterwards they should appear as FOUND.

        this.do_revert_all();

        var expect_revert = new Array;
        expect_revert["Found"] = [ "@/dirt_dir_a",
                                   "@/dirt_dir_a/dirt_dir_aa",
                                   "@/dirt_dir_a/dirt_dir_aa/dirt_dir_aaa",
                                   "@/dirt_file_a",
                                   "@/dirt_dir_a/dirt_file_aa",
                                   "@/dirt_dir_a/dirt_dir_aa/dirt_file_aaa" ];
        this.confirm(expect_revert);

        // clean up after this test by actually deleting the stuff from the WD
        // so that the next test can start with a clean WD and be completely
        // independently of this test.

        this.do_fsobj_remove_file("dirt_dir_a/dirt_dir_aa/dirt_file_aaa");
        this.do_fsobj_remove_file("dirt_dir_a/dirt_file_aa");
        this.do_fsobj_remove_file("dirt_file_a");
        this.do_fsobj_remove_dir("dirt_dir_a/dirt_dir_aa/dirt_dir_aaa");
        this.do_fsobj_remove_dir("dirt_dir_a/dirt_dir_aa");
        this.do_fsobj_remove_dir("dirt_dir_a");

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }

    this.test_basic_dirty_lost = function()
    {
        // assuming we have a clean WD.
        // we don't care what the starting baseline is.
        // do clean update to the changeset that we want to begin the test with.
        // DELETE some stuff from the WD directly so that they will be LOST.
        // do dirty-update to various target changesets and each time confirm 
        // that the dirt got pulled forward and that the rest of the changeset
        // was changed.
        //
        // 2010/06/30 Changed behavior of UPDATE for LOST items.  We now
        //            restore them to the state in the destination changeset
        //            rather than propagating the LOST status.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return false;

        // now do various dirty updates.

        var ndxHead = this.csets.length - 1;
        while (this.ndxCur < ndxHead)
        {
            // create the dirt as a series of DELETED entries.
            // this removes the files both from version control and the WD.

            this.do_fsobj_remove_file("file_0");
            this.do_fsobj_remove_file("dir_0/file_00");
            this.do_fsobj_remove_file("dir_0/dir_00/file_000");
            this.do_fsobj_remove_dir("dir_0/dir_00");
            this.do_fsobj_remove_dir("dir_0");

            var expect_test = new Array;
            expect_test["Lost"] = [ "@/file_0",
                                    "@/dir_0/file_00",
                                    "@/dir_0/dir_00/file_000",
                                    "@/dir_0/dir_00",
                                    "@/dir_0" ];
            this.confirm(expect_test);

            if (!this.do_update_when_dirty(this.ndxCur + 1))
                return;

            var expect_test = new Array;
            this.confirm(expect_test);
        }

        // revert the dirt.  REVERT should re-ADD the things to the WD, so the
        // status should be empty.

        this.do_revert_all();

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }

    //////////////////////////////////////////////////////////////////
    // no-so-basic dirty tests.  the dirt created here should mesh nicely
    // with the sequence of changesets.
    //////////////////////////////////////////////////////////////////

    this.test_mesh_dirty_file_mod_23 = function()
    {
        // assuming we have a clean WD.
        // pick a known starting baseline.
        // MODIFY some files that are NOT modified during the changeset sequence.
        // do dirty-update to various target changesets and each time confirm
        // that the dirt got pulled forward.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return false;

        // create the dirt as a series of FILE MODIFICATIONS.

        this.do_fsobj_append_file("file_2", "x");        this.do_fsobj_append_file("dir_2/file_22", "x");        this.do_fsobj_append_file("dir_2/dir_22/file_222", "x");
        this.do_fsobj_append_file("file_3", "x");        this.do_fsobj_append_file("dir_3/file_33", "x");        this.do_fsobj_append_file("dir_3/dir_33/file_333", "x");

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/file_2", "@/dir_2/file_22", "@/dir_2/dir_22/file_222",
                                    "@/file_3", "@/dir_3/file_33", "@/dir_3/dir_33/file_333" ];
        this.confirm(expect_test);

        // update to a known point in the middle of the renames of 2.

        if (!this.do_update_when_dirty_by_name("Simple file RENAMEs 2"))
            return false;

        var expect_update_1 = new Array;
        expect_update_1["Modified"] = [ "@/file_2_renamed", "@/dir_2/file_22_renamed", "@/dir_2/dir_22/file_222_renamed",
                                        "@/file_3",         "@/dir_3/file_33",         "@/dir_3/dir_33/file_333"         ];
        this.confirm(expect_update_1);

        // update to a known point after more renames and in the middle of the moves of 3.

        if (!this.do_update_when_dirty_by_name("Simple file MOVEs 3"))
            return false;

        var expect_update_2 = new Array;
        expect_update_2["Modified"] = [ "@/file_2",                 "@/dir_2/file_22",           "@/dir_2/dir_22/file_222",
                                        "@/dir_3_alternate/file_3", "@/dir_3_alternate/file_33", "@/dir_3_alternate/file_333"      ];
        this.confirm(expect_update_2);

        // update to a known point after all of the of renames/moves on the set of files.

        if (!this.do_update_when_dirty_by_name("Simple ADDs 4"))
            return false;

        var expect_update_3 = new Array;
        expect_update_3["Modified"] = [ "@/file_2", "@/dir_2/file_22", "@/dir_2/dir_22/file_222",
                                        "@/file_3", "@/dir_3/file_33", "@/dir_3/dir_33/file_333" ];
        this.confirm(expect_update_3);

        // revert the dirt.  REVERT should undo the edits by doing a backup.

        this.do_revert_all();

        var backup_suffix = "~sg00~";
        var expect_revert = new Array;
        expect_revert["Found"] = [ "@/file_2" + backup_suffix, "@/dir_2/file_22" + backup_suffix, "@/dir_2/dir_22/file_222" + backup_suffix,
                                   "@/file_3" + backup_suffix, "@/dir_3/file_33" + backup_suffix, "@/dir_3/dir_33/file_333" + backup_suffix ];
//        backups are ignored in status, so we'll use the filesystem
//        this.confirm(expect_revert);
        this.confirm_on_disk(expect_revert["Found"]);
        // this status check fails if ignores are not working
        // (it's really a redundant check anyway)
        this.statusEmpty("Status should be empty.");

        // cleanup

        this.do_fsobj_remove_file("file_2" + backup_suffix);        this.do_fsobj_remove_file("dir_2/file_22" + backup_suffix);        this.do_fsobj_remove_file("dir_2/dir_22/file_222" + backup_suffix);
        this.do_fsobj_remove_file("file_3" + backup_suffix);        this.do_fsobj_remove_file("dir_3/file_33" + backup_suffix);        this.do_fsobj_remove_file("dir_3/dir_33/file_333" + backup_suffix);

//        backups are ignored in status, so we'll use the filesystem
//        var expect_nothing = new Array;
//        this.confirm(expect_nothing);
        this.confirm_not_on_disk(expect_revert["Found"]);
        this.statusEmpty("Status should be empty.");
    }

    this.test_mesh_dirty_move_rename = function()
    {
        // assuming we have a clean WD.
        // pick a known starting baseline.
        // MODIFY some files that are NOT modified during the changeset sequence.
        // do dirty-update to various target changesets and each time confirm
        // that the dirt got pulled forward.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return false;

        // MOVE some things (that were RENAMEd in the changeset sequence)
        // and RENAME some things (that were MOVEd in the changeset sequence).

        this.do_fsobj_mkdir("mesh_dirty_move_rename");
        this.do_addremove_scan();
        this.do_vc_move("file_2",                  "mesh_dirty_move_rename");
        this.do_vc_move("dir_2/file_22",           "mesh_dirty_move_rename");
        this.do_vc_move("dir_2/dir_22/file_222",   "mesh_dirty_move_rename");
        this.do_vc_rename("file_3",                "mesh_file_3");
        this.do_vc_rename("dir_3/file_33",         "mesh_file_33");
        this.do_vc_rename("dir_3/dir_33/file_333", "mesh_file_333");

        var expect_test = new Array;
        expect_test["Added"]   = [ "@/mesh_dirty_move_rename" ];
        expect_test["Moved"]   = [ "@/mesh_dirty_move_rename/file_2",
                                   "@/mesh_dirty_move_rename/file_22",
                                   "@/mesh_dirty_move_rename/file_222" ];
        expect_test["Renamed"] = [ "@/mesh_file_3",
                                   "@/dir_3/mesh_file_33",
                                   "@/dir_3/dir_33/mesh_file_333" ];
        this.confirm(expect_test);

        // update to a known point in the middle of the renames of 2.

        if (!this.do_update_when_dirty_by_name("Simple file RENAMEs 2"))
            return false;

        var expect_update_1 = new Array;
        expect_update_1["Added"]   = [ "@/mesh_dirty_move_rename" ];
        expect_update_1["Moved"]   = [ "@/mesh_dirty_move_rename/file_2_renamed",
                                       "@/mesh_dirty_move_rename/file_22_renamed",
                                       "@/mesh_dirty_move_rename/file_222_renamed" ];
        expect_update_1["Renamed"] = [ "@/mesh_file_3",
                                       "@/dir_3/mesh_file_33",
                                       "@/dir_3/dir_33/mesh_file_333" ];
        this.confirm(expect_update_1);

        // update to a known point after more renames and in the middle of the moves of 3.

        if (!this.do_update_when_dirty_by_name("Simple file MOVEs 3"))
            return false;

        var expect_update_2 = new Array;
        expect_update_2["Added"]   = [ "@/mesh_dirty_move_rename" ];
        expect_update_2["Moved"]   = [ "@/mesh_dirty_move_rename/file_2",
                                       "@/mesh_dirty_move_rename/file_22",
                                       "@/mesh_dirty_move_rename/file_222" ];
        expect_update_2["Renamed"] = [ "@/dir_3_alternate/mesh_file_3",
                                       "@/dir_3_alternate/mesh_file_33",
                                       "@/dir_3_alternate/mesh_file_333" ];
        this.confirm(expect_update_2);

        // update to a known point after all of the of renames/moves on the set of files.

        if (!this.do_update_when_dirty_by_name("Simple ADDs 4"))
            return false;

        var expect_update_3 = new Array;
        expect_update_3["Added"]   = [ "@/mesh_dirty_move_rename" ];
        expect_update_3["Moved"]   = [ "@/mesh_dirty_move_rename/file_2",
                                       "@/mesh_dirty_move_rename/file_22",
                                       "@/mesh_dirty_move_rename/file_222" ];
        expect_update_3["Renamed"] = [ "@/mesh_file_3",
                                       "@/dir_3/mesh_file_33",
                                       "@/dir_3/dir_33/mesh_file_333" ];
        this.confirm(expect_update_3);

        // revert the dirt.  REVERT should undo the moves/renames without backups.
        // this will also un-ADD the directory that we created.

        this.do_revert_all();

        var expect_update_4 = new Array;
        expect_update_4["Found"]   = [ "@/mesh_dirty_move_rename" ];
        this.confirm(expect_update_4);

        this.do_fsobj_remove_dir("mesh_dirty_move_rename");

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }

    // TODO test AttrBits, XAttrs, Symlinks.

    //////////////////////////////////////////////////////////////////
    // undone tests.  these tests create dirt in the WD that exactly
    // matches a change made in the sequence of changesets and confirms
    // that we silently allow it.
    //////////////////////////////////////////////////////////////////

    this.test_compat_delete = function()
    {
    /**************************************************
    Fixed: SPRAWL-576
    This test had failed because, after the dirty-update,
    status still shows the Removed items.  The update
    is successful, but baseline/PT seems out of sync.

    Reproducing this from the command line, after the
    dirty-update a commit will succeed, but it's empty
    (diff -r y -r x shows nothing).

    Also, after the dirty-update and a revert --all,
    status will show the formerly deleted items as Found.
    **************************************************/

        // assuming we have a clean WD.
        // warp to a changeset before a DELETE
        // DELETE the same thing in the WD.
        // do dirty-update that will do the same delete and confirm 
        // that the delete got pulled pulled forward (and assimilated).

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return false;

        // duplicate the file removes in the sequence.

        this.do_vc_remove_file("dir_1/dir_11/file_111");
        this.do_vc_remove_file("dir_1/file_11");
        this.do_vc_remove_file("file_1");

        var expect_test = new Array;
        expect_test["Removed"] = [ "@/file_1",
                                   "@/dir_1/file_11",
                                   "@/dir_1/dir_11/file_111" ];
        this.confirm(expect_test);

        // update to a known point where the exact same REMOVES were made.

        if (!this.do_update_when_dirty_by_name("Simple DELETEs 1"))
            return false;

        // in theory, the fact that we did a rename in the WD should be
        // subsumed by the changeset, so all should be clean.

        var expect_nothing = new Array;
        this.confirm(expect_nothing);

        // duplicate the directory removes in the sequence.

        this.do_vc_remove_dir("dir_1/dir_11");
        this.do_vc_remove_dir("dir_1");

        var expect_test = new Array;
        expect_test["Removed"] = [ "@/dir_1/dir_11",
                                   "@/dir_1" ];
        this.confirm(expect_test);

        // update to a known point where the exact same REMOVES were made.

        if (!this.do_update_when_dirty_by_name("Simple RMDIRs 1"))
            return false;

        // in theory, the fact that we did a rename in the WD should be
        // subsumed by the changeset, so all should be clean.

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }

    this.test_compat_rename = function()
    {
        // assuming we have a clean WD.
        // pick a known starting baseline.
        // RENAME some files to the same new names as in the changeset series.
        // do dirty-update to various target changesets and each time confirm
        // that the dirt got pulled forward.
        // we should expect a SIMILAR_RENAME.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return false;

        // duplicate the moves and renames in the sequence.

        this.do_vc_rename("file_2",               "file_2_renamed");
        this.do_vc_rename("dir_2/file_22",        "file_22_renamed");
        this.do_vc_rename("dir_2/dir_22/file_222","file_222_renamed");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/file_2_renamed",
                                   "@/dir_2/file_22_renamed",
                                   "@/dir_2/dir_22/file_222_renamed" ];
        this.confirm(expect_test);

        // update to a known point where the exact same RENAMES were made.

        if (!this.do_update_when_dirty_by_name("Simple file RENAMEs 2"))
            return false;

        // in theory, the fact that we did a rename in the WD should be
        // subsumed by the changeset, so all should be clean.

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }

    this.test_compat_move = function()
    {
        // assuming we have a clean WD.
        // pick a known starting baseline.
        // MOVE some files to the same new names as in the changeset series.
        // do dirty-update to various target changesets and each time confirm
        // that the dirt got pulled forward.
        // we should expect a SIMILAR_MOVE

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple file MOVEs 3 again"))
            return false;

        // duplicate the moves and renames in the sequence.

        this.do_vc_move("dir_3_alternate_again/file_3",   ".");
        this.do_vc_move("dir_3_alternate_again/file_33",  "dir_3");
        this.do_vc_move("dir_3_alternate_again/file_333", "dir_3/dir_33");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/file_3",
                                 "@/dir_3/file_33",
                                 "@/dir_3/dir_33/file_333" ];
        this.confirm(expect_test);

        // update to a known point where the exact same MOVES were made.

        if (!this.do_update_when_dirty_by_name("Simple file MOVEs 3 original"))
            return false;

        // in theory, the fact that we did a MOVE in the WD should be
        // subsumed by the changeset, so all should be clean.

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }

    this.test_compat_rename_move = function()
    {
        // assuming we have a clean WD.
        // pick a known starting baseline.
        // MOVE+RENAME some files to the same new names as in the changeset series.
        // do dirty-update to various target changesets and each time confirm
        // that the dirt got pulled forward.
        // we should expect a SIMILAR_MOVE_WITH_SIMILAR_RENAME.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 7"))
            return false;

        // duplicate the moves and renames in the sequence.

        this.do_vc_rename("dir_7/dir_77/file_777",         "file_777_renamed");
        this.do_vc_move(  "dir_7/dir_77/file_777_renamed", "dir_7/dir_77_alternate");
        this.do_vc_rename("dir_7/dir_77_alternate",        "dir_77_renamed");
        this.do_vc_move(  "dir_7/dir_77_renamed",          "dir_7_alternate");

        var expect_test = new Array;
        expect_test["Moved"]   = [ "@/dir_7_alternate/dir_77_renamed/file_777_renamed",
                                   "@/dir_7_alternate/dir_77_renamed" ];
        expect_test["Renamed"] = [ "@/dir_7_alternate/dir_77_renamed/file_777_renamed",
                                   "@/dir_7_alternate/dir_77_renamed" ];
        this.confirm(expect_test);

        // update to a known point where the exact same RENAMES+MOVES were made.

        if (!this.do_update_when_dirty_by_name("Simple file RENAMEs + MOVEs 7"))
            return false;

        // in theory, the fact that we did a MOVE in the WD should be
        // subsumed by the changeset, so all should be clean.

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }

    // duplicate one of the above test_compat_{rename,move,rename_move}()
    // to cause SIMILAR_MOVE_WITH_RENAME, MOVED_WITH_SIMILAR_RENAME.

    this.test_compat_lost_files = function()
    {

        // assuming we have a clean WD.
        // warp to a changeset before a DELETE
        // LOSE the files in the WD.
        // do dirty-update that will do the same removes and confirm 
        // that the lost items and the removes got blended.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return false;

        // duplicate the file removes in the sequence.

        this.do_fsobj_remove_file("dir_1/dir_11/file_111");
        this.do_fsobj_remove_file("dir_1/file_11");
        this.do_fsobj_remove_file("file_1");

        var expect_test = new Array;
        expect_test["Lost"] = [ "@/file_1",
                                   "@/dir_1/file_11",
                                   "@/dir_1/dir_11/file_111" ];
        this.confirm(expect_test);

        // update to a known point where the exact same REMOVES were made.

        if (!this.do_update_when_dirty_by_name("Simple DELETEs 1"))
            return false;

        // in theory, the fact that we did a rename in the WD should be
        // subsumed by the changeset, so all should be clean.

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }

    this.test_compat_lost_dirs = function()
    {
    /**************************************************
    Fixed: SPRAWL-577 (I think the fix for SPRAWL-575 also got this.)
    This test had failed about 50% of the time.  When it does
    fail, it's at the dirty-update to "Simple RMDIRs 1",
    where we get the following error: (50% of the time)
    vv: Error 62 (sglib): The requested object could not be found: 
    PendingTreeFindByGid 'g775f5cf9f6e44f2298eb5ac8cb93e47070863d0f4aad11df8603000c29a88c8f'
        D:\s\sprawl\src\libraries\ut\sg_pendingtree.c:3071
        d:\s\sprawl\src\libraries\ut\sg_pendingtree__private_update_baseline.h:1745
        d:\s\sprawl\src\libraries\ut\sg_pendingtree__private_update_baseline.h:2619
        d:\s\sprawl\src\libraries\ut\sg_pendingtree__private_update_baseline.h:2850
        d:\s\sprawl\src\libraries\ut\sg_pendingtree__private_update_baseline.h:3023
        d:\s\sprawl\src\libraries\ut\sg_pendingtree__private_update_baseline.h:3477
        d:\s\sprawl\src\libraries\ut\sg_pendingtree__private_update_baseline.h:3528
        D:\s\sprawl\src\cmd\sg__do_cmd_update.c:87
        D:\s\sprawl\src\cmd\sg.c:5008
        D:\s\sprawl\src\cmd\sg.c:5436
        D:\s\sprawl\src\cmd\sg.c:5635

    In the times where the update succeeds, the test is
    successful, in that status is empty after the update.

    This can also be reproduced from the command line.
    **************************************************/

        // assuming we have a clean WD.
        // warp to a changeset before a DELETE
        // LOSE the directories in the WD.
        // do dirty-update that will do the same deletes and confirm 
        // that the lost items and the deletes got blended.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple DELETEs 1"))
            return false;

        // duplicate the directory removes in the sequence.

        this.do_fsobj_remove_dir("dir_1/dir_11");
        this.do_fsobj_remove_dir("dir_1");

        var expect_test = new Array;
        expect_test["Lost"] = [ "@/dir_1/dir_11",
                                   "@/dir_1" ];
        this.confirm(expect_test);

        // update to a known point where the exact same REMOVES were made.

        if (!this.do_update_when_dirty_by_name("Simple RMDIRs 1"))
            return false;

        // in theory, the fact that we did a rename in the WD should be
        // subsumed by the changeset, so all should be clean.

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }



    this.test_compat_file_mod = function()
    {
    /**************************************************
    Fixed: SPRAWL-575
    This test had failed because, after the dirty-update,
    status still shows the files as Modified.  The 
    update is successful, but baseline/PT seems out of sync.

    Reproducing this from the command line, after 
    the dirty-update a commit will succeed, but it's 
    empty (diff -r y -r x shows nothing).

    Also, after the dirty-update and a revert --all, 
    status will still show the files as Modified, 
    but inspections show that they have been reverted 
    to the pre-modified state, like they were at 
    "Simple ADDs 6".  A second revert --all will revert
    them back to the appended state, and status
    will be empty.

    Here's another perspective:  after the dirty-update, 
    a filesystem cat or type shows the files with the 
    append, but vv cat shows them without it.  A parents
    command confirms that we were "updated" to the right place.
    **************************************************/

        // assuming we have a clean WD.
        // warp to to a changeset near a file mod.
        // make the identical mod to the file.
        // do dirty-delete that will do the same mod and confirm
        // that mod got pulled forward and assimilated.

        if (!this.force_clean_WD())
            return false;

        if (!this.do_update_when_clean_by_name("Simple ADDs 6"))
            return false;

        // duplicate the file modifications in the sequence.

        this.do_fsobj_append_file("file_6",                "hello1");
        this.do_fsobj_append_file("dir_6/file_66",         "hello1");
        this.do_fsobj_append_file("dir_6/dir_66/file_666", "hello1");

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/file_6",
                                    "@/dir_6/file_66",
                                    "@/dir_6/dir_66/file_666" ];
        this.confirm(expect_test);

        // update to a known point where the exact same MODIFICATIONS were made.

        if (!this.do_update_when_dirty_by_name("Simple file MODIFY 6.1"))
            return false;

        // in theory, the fact that we did a rename in the WD should be
        // subsumed by the changeset, so all should be clean.

        var expect_nothing = new Array;
        this.confirm(expect_nothing);

        // repeat this test through a couple more steps

        this.do_fsobj_append_file("file_6",                "hello2");
        this.do_fsobj_append_file("dir_6/file_66",         "hello2");
        this.do_fsobj_append_file("dir_6/dir_66/file_666", "hello2");

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/file_6",
                                    "@/dir_6/file_66",
                                    "@/dir_6/dir_66/file_666" ];
        this.confirm(expect_test);

        if (!this.do_update_when_dirty_by_name("Simple file MODIFY 6.2"))
            return false;

        var expect_nothing = new Array;
        this.confirm(expect_nothing);

        this.do_fsobj_append_file("file_6",                "hello3");
        this.do_fsobj_append_file("dir_6/file_66",         "hello3");
        this.do_fsobj_append_file("dir_6/dir_66/file_666", "hello3");

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/file_6",
                                    "@/dir_6/file_66",
                                    "@/dir_6/dir_66/file_666" ];
        this.confirm(expect_test);

        if (!this.do_update_when_dirty_by_name("Simple file MODIFY 6.3"))
            return false;

        var expect_nothing = new Array;
        this.confirm(expect_nothing);
    }


    // note compat_add not possible because each one gets unique gid.

    //////////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////////

    // TODO portability warnings for dirt vs target changeset....

    //////////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////////

    // TODO auto merge file contents -- without conflicts.
    // TODO auto merge file contents -- with conflicts.

    //////////////////////////////////////////////////////////////////
    //    Tests that are expected to fail.
    //////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////
    // create a branch from some arbitrary changeset in the middle of the linear sequence
    // and begin a new sequence from it.  with dirty WD verify that you can't update
    // to a different branch.  Bring over the code from stUpdateTestsWithCleanWD.js.
    //////////////////////////////////////////////////////////////////

    this.test_dirty_branch = function()
    {
        // ## NOTE: We'll be leaving a two headed DAG behind here.
        // ## Consider whether this should be at the end of the tests
        // assuming we have a clean WD.
        // warp to an arbitrary changeset.
        // create another linear sequence in a branch off of it.
        // then create some extra dirt
        // UPDATE should refuse jumping between cousins because of the dirt.

        // this.verbose = true;  // if you want it.  Sticks through the end of the stGroup

        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple ADDs 0"))
            return;

        // begin a branch based upon the initial commit.

        this.do_fsobj_mkdir("dir_X");
        this.do_fsobj_mkdir("dir_X/dir_XX");
        this.do_fsobj_create_file("file_X");
        this.do_fsobj_create_file("dir_X/file_XX");
        this.do_fsobj_create_file("dir_X/dir_XX/file_XXX");
        this.do_addremove_scan();
        this.do_commit_in_branch("Simple ADDs X");

        this.do_fsobj_mkdir("dir_Y");
        this.do_fsobj_mkdir("dir_Y/dir_YY");
        this.do_fsobj_create_file("file_Y");
        this.do_fsobj_create_file("dir_Y/file_YY");
        this.do_fsobj_create_file("dir_Y/dir_YY/file_YYY");
        this.do_addremove_scan();
        this.do_commit_in_branch("Simple ADDs Y");

        this.do_fsobj_mkdir("dir_Z");
        this.do_fsobj_mkdir("dir_Z/dir_ZZ");
        this.do_fsobj_create_file("file_Z");
        this.do_fsobj_create_file("dir_Z/file_ZZ");
        this.do_fsobj_create_file("dir_Z/dir_ZZ/file_ZZZ");
        this.do_addremove_scan();
        // no commit - leave the dirt

        // now bounce between the 2 branches

        var ndxHead = this.csets.length - 1;
        var ndxHead_in_branch = this.csets_in_branch.length - 1;
        
        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty", [ ndxHead ] );
        testlib.verifyFailure(fnstr, "Dirty update (files & dirs) across branches should fail", "Changesets are unrelated");

        // clean up after ourselves
        sg.pending_tree.revert();

        this.do_fsobj_remove_file("dir_Z/dir_ZZ/file_ZZZ");
        this.do_fsobj_remove_file("dir_Z/file_ZZ");
        this.do_fsobj_remove_file("file_Z");
        this.do_fsobj_remove_dir("dir_Z/dir_ZZ");
        this.do_fsobj_remove_dir("dir_Z");

        // try one file for grins
        this.do_fsobj_create_file("file_Z");
        this.do_addremove_scan();

        var ndxHead = this.csets.length - 1;
        var ndxHead_in_branch = this.csets_in_branch.length - 1;
        
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty", [ ndxHead ] );
        testlib.verifyFailure(fnstr, "Dirty update (one file) from branch to trunk should fail", "Changesets are unrelated");


        // cleanup
        sg.pending_tree.revert();

        this.do_fsobj_remove_file("file_Z");
        testlib.statusEmpty();

        // warp back to the main branch
        this.do_update_when_clean(ndxHead);

        // try one-file-dirt from here to the branch
        this.do_fsobj_create_file("file_Z");
        this.do_addremove_scan();

        var ndxHead = this.csets.length - 1;
        var ndxHead_in_branch = this.csets_in_branch.length - 1;
        
        var fnstr = formatFunctionString("globalThis.do_update_in_branch_when_dirty", [ ndxHead_in_branch ] );
        testlib.verifyFailure(fnstr, "Dirty update (one file) from trunk to branch should fail", "Changesets are unrelated");

        // final cleanup
        sg.pending_tree.revert();
        this.do_fsobj_remove_file("file_Z");
        testlib.statusEmpty();
    }


    //////////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////
    //  Non-compatible moves, renames, edits, removes,
    //  Divergent moves and renames
    //////////////////////////////////////////////////////////////////

    // do non-compatible stuff.
    // remove in 1 vs {move,rename,edit,...} in other.
    // {move,rename,edit,...} vs remove.
    // divergent moves.
    // divergent renames.
    // etc.

    //////////////////////////////////////////////////////////////////
    // Remove in 1 vs {move,rename,edit,...} in other.
    // assuming we have a clean WD.
    // warp to a changeset before a series of moves,
    // renames, and edits.  Do dirty removes of the
    // items.  Update should fail when going past
    // the moves, renames, and edits.
    //////////////////////////////////////////////////////////////////

    this.test_dirty_removes_updated_to_renames = function()
    {
        print("/// vc_remove_file ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple RMDIRs 1"))
            return;

        // vc_remove some files that will be renamed
        
        this.do_vc_remove_file("file_2");
        this.do_vc_remove_file("dir_2/file_22");
        this.do_vc_remove_file("dir_2/dir_22/file_222");
        // no commit - leave the dirt

        // now try to update past the rotational renames
        // where the files end up with their original names
        // eg: a->b b->c c->a

        this.do_update_when_dirty_by_name("Simple file RENAMEs 2 original");

        // The update should have succeeded.  Let's check
        // our status ...

        var expect_test = new Array;
        expect_test["Removed"] = [ "@/file_2",
                                   "@/dir_2/file_22",
                                   "@/dir_2/dir_22/file_222" ];
        this.confirm(expect_test);

        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple RMDIRs 1"))
            return;

        // try vc_remove of some files that will be renamed
        
        this.do_vc_remove_file("file_2");
        this.do_vc_remove_file("dir_2/file_22");
        this.do_vc_remove_file("dir_2/dir_22/file_222");
        // no commit - leave the dirt

        // now try to update past the renames

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple file RENAMEs 2 again" ] );
        testlib.verifyFailure(fnstr, "Dirty update (removed file will be renamed) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// vc_remove_dirs ///////////////////////////////////////////////");

        if (!this.do_update_when_clean_by_name("Simple ADDs 4"))
            return;

        // vc_remove some dirs that will be renamed
        
        this.do_vc_remove_file("dir_4/dir_44/file_444");
        this.do_vc_remove_dir("dir_4/dir_44");
        this.do_vc_remove_file("dir_4/file_44");
        this.do_vc_remove_dir("dir_4");
        // no commit - leave the dirt

        // now try to update past the rotational renames
        // where the directories end up with their original names
        // eg: a->b b->c c->a

        this.do_update_when_dirty_by_name("Simple directory RENAMEs 4 original");

        // The update should have succeeded.  Let's check
        // our status ...

        var expect_test = new Array;
        expect_test["Removed"] = [ "@/dir_4",
                                   "@/dir_4/file_44",
                                   "@/dir_4/dir_44",
                                   "@/dir_4/dir_44/file_444" ];
        this.confirm(expect_test);

        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple ADDs 4"))
            return;

        // vc_remove some dirs that will be renamed
        
        this.do_vc_remove_dir("dir_4/dir_44");
        this.do_vc_remove_dir("dir_4");
        // no commit - leave the dirt

        // now try to update past the renames

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple directory RENAMEs 4 again" ] );
        testlib.verifyFailure(fnstr, "Dirty update (removed directory will be renamed) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;
    }


    this.test_file_fsobj_deletes_to_renames = function()
    {
    /**************************************************
    Fixed: SPRAWL-611
    This test had failed because when attempting an update 
    after a non-compatible dirty delete, the error is 
    not what we should expect.  This may or may not 
    indicate a deeper problem.
    **************************************************/

        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple RMDIRs 1"))
            return;

        // fsobj_remove some files that will be renamed
        
        this.do_fsobj_remove_file("file_2");
        this.do_fsobj_remove_file("dir_2/file_22");
        this.do_fsobj_remove_file("dir_2/dir_22/file_222");
        // no commit - leave the dirt

        // now try to update past the rotational renames
        // where the files end up with their original names
        // eg: a->b b->c c->a

        this.do_update_when_dirty_by_name("Simple file RENAMEs 2 original");

        // The update should have succeeded.  Let's check
        // our status ...

        var expect_test = new Array;
        expect_test["Removed"] = [ "@/file_2",
                                   "@/dir_2/file_22",
                                   "@/dir_2/dir_22/file_222" ];
        this.confirm(expect_test);

        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple RMDIRs 1"))
            return;

        // try fsobj_remove of some files that will be renamed
        
        this.do_fsobj_remove_file("file_2");
        this.do_fsobj_remove_file("dir_2/file_22");
        this.do_fsobj_remove_file("dir_2/dir_22/file_222");
        // no commit - leave the dirt

        var expect_test = new Array;
        expect_test["Lost"] = [ "@/file_2",
                                "@/dir_2/file_22",
                                "@/dir_2/dir_22/file_222" ];
        this.confirm(expect_test);

        // now try to update past the renames
        // (since there were changes of some kind to each of the things
        // we lost, the losts will be overridden)

        if (!this.do_update_when_dirty_by_name("Simple file RENAMEs 2 again"))
            return;

        var expect_nothing = new Array;
        this.confirm(expect_nothing);

        // clean up after ourselves
        this.force_clean_WD();
    }

    this.test_dir_fsobj_deletes_to_renames = function()
    {
    /**************************************************
    Fixed: SPRAWL-611
    This test had failed because when attempting an update 
    after a non-compatible dirty delete, the error is 
    not what we should expect.  This may or may not 
    indicate a deeper problem.
    **************************************************/

        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple ADDs 4"))
            return;

        // fsobj_remove some dirs that will be renamed
        
        this.do_fsobj_remove_file("dir_4/dir_44/file_444");
        this.do_fsobj_remove_dir("dir_4/dir_44");
        this.do_fsobj_remove_file("dir_4/file_44");
        this.do_fsobj_remove_dir("dir_4");
        // no commit - leave the dirt

        var expect_test = new Array;
        expect_test["Lost"] = [ "@/dir_4",
                                "@/dir_4/file_44",
                                "@/dir_4/dir_44",
                                "@/dir_4/dir_44/file_444" ];
        this.confirm(expect_test);

        // now try to update past the rotational renames
        // where the directories end up with their original names
        // eg: a->b b->c c->a

        this.do_update_when_dirty_by_name("Simple directory RENAMEs 4 original");

        // The update should have succeeded.  Let's check
        // our status ...

        var expect_test = new Array;
        expect_test["Removed"] = [ "@/dir_4",
                                   "@/dir_4/file_44",
                                   "@/dir_4/dir_44",
                                   "@/dir_4/dir_44/file_444" ];
        this.confirm(expect_test);

        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple ADDs 4"))
            return;

        // fsobj_remove some dirs that will be renamed
        
        this.do_fsobj_remove_file("dir_4/dir_44/file_444");
        this.do_fsobj_remove_dir("dir_4/dir_44");
        this.do_fsobj_remove_file("dir_4/file_44");
        this.do_fsobj_remove_dir("dir_4");
        // no commit - leave the dirt

        var expect_test = new Array;
        expect_test["Lost"] = [ "@/dir_4",
                                "@/dir_4/file_44",
                                "@/dir_4/dir_44",
                                "@/dir_4/dir_44/file_444" ];
        this.confirm(expect_test);

        // now try to update past the renames

        if (!this.do_update_when_dirty_by_name("Simple directory RENAMEs 4 again"))
            return;

        // clean up after ourselves
        this.force_clean_WD();
    }

    this.test_dirty_removes_updated_to_moves = function()
    {
        print("/// vc_remove_file ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // vc_remove some files that will be moved
        
        this.do_vc_remove_file("file_3");
        this.do_vc_remove_file("dir_3/file_33");
        this.do_vc_remove_file("dir_3/dir_33/file_333");
        // no commit - leave the dirt

        // now try to update past the rotational moves
        // where the files end up in their original locations

        this.do_update_when_dirty_by_name("Simple file MOVEs 3 original");

        // The update should have succeeded.  Let's check
        // our status ...

        var expect_test = new Array;
        expect_test["Removed"] = [ "@/file_3",
                                   "@/dir_3/file_33",
                                   "@/dir_3/dir_33/file_333" ];
        this.confirm(expect_test);

        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // try vc_remove of some files that will be moved
        
        this.do_vc_remove_file("file_3");
        this.do_vc_remove_file("dir_3/file_33");
        this.do_vc_remove_file("dir_3/dir_33/file_333");
        // no commit - leave the dirt

        // now try to update past the moves

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple file MOVEs 3 again" ] );
        testlib.verifyFailure(fnstr, "Dirty update (removed file will be moved) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// vc_remove_dirs ///////////////////////////////////////////////");

        if (!this.do_update_when_clean_by_name("Simple ADDs 5"))
            return;

        // vc_remove some dirs that will be moved
        
        this.do_vc_remove_file("dir_5/dir_55/file_555");
        this.do_vc_remove_dir("dir_5/dir_55");
        this.do_vc_remove_file("dir_5/file_55");
        this.do_vc_remove_dir("dir_5");
        // no commit - leave the dirt

        // now try to update past the dir moves

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple directory MOVEs 5" ] );
        testlib.verifyFailure(fnstr, "Dirty update (removed directory will be moved) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;
    }

    this.test_file_fsobj_deletes_to_moves = function()
    {
    /**************************************************
    Fixed: SPRAWL-611
    This test had failed because when attempting an update 
    after a non-compatible dirty delete, the error is 
    not what we should expect.
    **************************************************/
    /// fsobj_remove_files ///////////////////////////////////////////////
        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // fsobj_remove some files that will be moved
        
        this.do_fsobj_remove_file("file_3");
        this.do_fsobj_remove_file("dir_3/file_33");
        this.do_fsobj_remove_file("dir_3/dir_33/file_333");
        // no commit - leave the dirt

        var expect_test = new Array;
        expect_test["Lost"] = [ "@/file_3",
                                "@/dir_3/file_33",
                                "@/dir_3/dir_33/file_333" ];
        this.confirm(expect_test);

        // now try to update past the rotational moves
        // where the files end up in their original locations

        this.do_update_when_dirty_by_name("Simple file MOVEs 3 original");

        // The update should have succeeded.  Let's check
        // our status ...

        var expect_test = new Array;
        this.confirm(expect_test);

        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // try fsobj_remove of some files that will be moved
        
        this.do_fsobj_remove_file("file_3");
        this.do_fsobj_remove_file("dir_3/file_33");
        this.do_fsobj_remove_file("dir_3/dir_33/file_333");
        // no commit - leave the dirt

        var expect_test = new Array;
        expect_test["Lost"] = [ "@/file_3",
                                "@/dir_3/file_33",
                                "@/dir_3/dir_33/file_333" ];
        this.confirm(expect_test);

        // now try to update past the moves

        if (!this.do_update_when_dirty_by_name("Simple file MOVEs 3 again"))
            return;

        var expect_nothing = new Array;
        this.confirm(expect_nothing);

        // clean up after ourselves
        this.force_clean_WD();
    }

    this.test_dir_fsobj_deletes_to_moves = function()
    {
    /**************************************************
    Fixed: SPRAWL-611
    This test had failed because when attempting an update 
    after a non-compatible dirty delete, the error is 
    not what we should expect.
    **************************************************/
    /// fsobj_remove_dirs ///////////////////////////////////////////////
        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple ADDs 5"))
            return;

        // dirty remove some dirs that will be renamed
        
        this.do_fsobj_remove_file("dir_5/dir_55/file_555");
        this.do_fsobj_remove_dir("dir_5/dir_55");
        this.do_fsobj_remove_file("dir_5/file_55");
        this.do_fsobj_remove_dir("dir_5");
        // no commit - leave the dirt

        var expect_test = new Array;
        expect_test["Lost"] = [ "@/dir_5/dir_55/file_555",
                                "@/dir_5/dir_55",
                                "@/dir_5/file_55",
                                "@/dir_5" ];
        this.confirm(expect_test);

        // now try to update past the dir moves

        this.do_update_when_dirty_by_name("Simple directory MOVEs 5");

        var expect_nothing = new Array;
        this.confirm(expect_nothing);

        // clean up after ourselves.
        this.force_clean_WD();

    }

    this.test_dirty_removes_updated_to_edits = function()
    {
        print("/// vc_remove_file ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean_by_name("Simple ADDs 6"))
            return;

        // vc_remove some files that will be edited
        
        this.do_vc_remove_file("file_6");
        this.do_vc_remove_file("dir_6/file_66");
        this.do_vc_remove_file("dir_6/dir_66/file_666");
        // no commit - leave the dirt

        // we shouldn't have to test rotational edits
        // since it would essentially duplicate what 
        // happens in test_compat_file_mod above

        // now try to update past the edits

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple file MODIFY 6.3" ] );
        testlib.verifyFailure(fnstr, "Dirty update (removed file will be edited) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// fsobj_remove_files ///////////////////////////////////////////////");

        // This failing test is stored in failingUpdateTestsWithDirtyWD.js
        // Test name: test_file_fsobj_removes_to_edits
        // Failure: x-bits are incorrectly set in Linux/Mac

    }

    this.test_update_dirty_renames = function()
    {
        print("/// renamed_files_get_removed ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 1"))
            return;

        // rename some files that will be removed
        this.do_vc_rename("file_1",               "file_1_renamed");
        this.do_vc_rename("dir_1/file_11",        "file_11_renamed");
        this.do_vc_rename("dir_1/dir_11/file_111","file_111_renamed");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/file_1_renamed",
                                   "@/dir_1/file_11_renamed",
                                   "@/dir_1/dir_11/file_111_renamed" ];
        this.confirm(expect_test);

        // now try to update past the removes

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple DELETEs 1" ] );
        testlib.verifyFailure(fnstr, "Dirty update (renamed file will be removed) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// renamed_dirs_get_removed ///////////////////////////////////////////////");
        if (!this.assert_clean_WD())
            return;

        // rename some dirs that will be removed
        this.do_vc_rename("dir_1/dir_11",               "dir_11_renamed");
        this.do_vc_rename("dir_1",                      "dir_1_renamed");

        expect_test["Renamed"] = [ "@/dir_1_renamed/dir_11_renamed",
                                   "@/dir_1_renamed" ];
        this.confirm(expect_test);

        // now try to update past the removes

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple RMDIRs 1" ] );
        testlib.verifyFailure(fnstr, "Dirty update (renamed directory will be removed) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// renamed_files_get_moved ///////////////////////////////////////////////");
        if (!this.assert_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // rename some files that will be moved
        this.do_vc_rename("file_3",               "file_3_renamed");
        this.do_vc_rename("dir_3/file_33",        "file_33_renamed");
        this.do_vc_rename("dir_3/dir_33/file_333","file_333_renamed");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/file_3_renamed",
                                   "@/dir_3/file_33_renamed",
                                   "@/dir_3/dir_33/file_333_renamed" ];
        this.confirm(expect_test);

        // now try to update past the moves

        this.do_update_when_dirty_by_name("Simple file MOVEs 3");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/dir_3_alternate/file_3_renamed",
                                   "@/dir_3_alternate/file_33_renamed",
                                   "@/dir_3_alternate/file_333_renamed" ];
        this.confirm(expect_test);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// renamed_dirs_get_moved ///////////////////////////////////////////////");
        if (!this.assert_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 5"))
            return;

        // rename some dirs that will be moved
        this.do_vc_rename("dir_5/dir_55",               "dir_55_renamed");
        this.do_vc_rename("dir_5",                      "dir_5_renamed");

        expect_test["Renamed"] = [ "@/dir_5_renamed/dir_55_renamed",
                                   "@/dir_5_renamed" ];
        this.confirm(expect_test);

        // now try to update past the moves

        this.do_update_when_dirty_by_name("Simple directory MOVEs 5");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/dir_5_alternate/dir_5_renamed",
                                   "@/dir_5_alternate/dir_55_renamed" ];
        this.confirm(expect_test);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// renamed_files_get_moved_to_renamed_dir ///////////////////////////////////////////////");
        if (!this.assert_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // rename some files that will be moved
        this.do_vc_rename("file_3",               "file_3_renamed");
        this.do_vc_rename("dir_3/file_33",        "file_33_renamed");
        this.do_vc_rename("dir_3/dir_33/file_333","file_333_renamed");

        // rename the target of the move
        this.do_vc_rename("dir_3_alternate",      "dir_3_new_alternate");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/file_3_renamed",
                                   "@/dir_3/file_33_renamed",
                                   "@/dir_3/dir_33/file_333_renamed",
                                   "@/dir_3_new_alternate" ];
        this.confirm(expect_test);

        // now try to update past the moves

        this.do_update_when_dirty_by_name("Simple file MOVEs 3");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/dir_3_new_alternate/file_3_renamed",
                                   "@/dir_3_new_alternate/file_33_renamed",
                                   "@/dir_3_new_alternate/file_333_renamed",
                                   "@/dir_3_new_alternate" ];
        this.confirm(expect_test);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// renamed_dirs_get_moved_to_renamed_dir ///////////////////////////////////////////////");
        if (!this.assert_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 5"))
            return;

        // rename some dirs that will be moved
        this.do_vc_rename("dir_5/dir_55",               "dir_55_renamed");
        this.do_vc_rename("dir_5",                      "dir_5_renamed");

        // rename the target of the move
        this.do_vc_rename("dir_5_alternate",            "dir_5_new_alternate");

        expect_test["Renamed"] = [ "@/dir_5_renamed/dir_55_renamed",
                                   "@/dir_5_renamed",
                                   "@/dir_5_new_alternate" ];
        this.confirm(expect_test);

        // now try to update past the moves

        this.do_update_when_dirty_by_name("Simple directory MOVEs 5");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/dir_5_new_alternate/dir_5_renamed",
                                   "@/dir_5_new_alternate/dir_55_renamed",
                                   "@/dir_5_new_alternate" ];
        this.confirm(expect_test);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;
    }


    this.test_renamed_files_get_modified = function()
    {
    /**************************************************
    Fixed: SPRAWL-914
    This one fails because, after the update we should expect
    a status of Renamed for the renamed files.  All the Edits
    should have been applied to the renamed files.
    In fact, we do get Renamed for the renamed files,
    but they also show as Modified, and the old filenames
    come up as Found.
    **************************************************/
    /// renamed_files_get_modified ///////////////////////////////////////////////
        if (!this.assert_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 6"))
            return;

        // rename some files that will be edited
        this.do_vc_rename("file_6",               "file_6_renamed");
        this.do_vc_rename("dir_6/file_66",        "file_66_renamed");
        this.do_vc_rename("dir_6/dir_66/file_666","file_666_renamed");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/file_6_renamed",
                                   "@/dir_6/file_66_renamed",
                                   "@/dir_6/dir_66/file_666_renamed" ];
        this.confirm(expect_test);

        // now try to update past the edits

        this.do_update_when_dirty_by_name("Simple file MODIFY 6.3");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/file_6_renamed",
                                   "@/dir_6/file_66_renamed",
                                   "@/dir_6/dir_66/file_666_renamed" ];
        this.confirm(expect_test);

        //dumpTree("Here's the pending_tree:");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;
    }

    if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
    {
        this.test_renamed_files_get_modifed_2 = function()
        {
            // another variation for SPRAWL-914 where we do a dirty RENAME
            // against a CHMOD in the goal changeset.

            if (!this.assert_clean_WD())
                return;
            if (!this.do_update_when_clean_by_name("AttrBits Chmod +x"))
                return;

            // rename a file that will have chmod

            this.do_vc_rename("dir_attrbits/file_attrbits_1", "file_attrbits_1_renamed");

            var expect_test = new Array;
            expect_test["Renamed"] = [ "@/dir_attrbits/file_attrbits_1_renamed" ];
            this.confirm(expect_test);

            // now try to update past a CHMOD

            this.do_update_when_dirty_by_name("AttrBits Off");

            var expect_test = new Array;
            expect_test["Renamed"] = [ "@/dir_attrbits/file_attrbits_1_renamed" ];
            this.confirm(expect_test);

            // clean up after ourselves
            if (!this.force_clean_WD())
                return;
        }
    }

    this.test_update_dirty_moves = function()
    {
        print("/// moved_files_get_removed ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // move some files that will be removed
        this.do_vc_move("file_1",               "dir_alternate_1");
        this.do_vc_move("dir_1/file_11",        "dir_alternate_1");
        this.do_vc_move("dir_1/dir_11/file_111","dir_alternate_1");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/file_1",
                                 "@/dir_alternate_1/file_11",
                                 "@/dir_alternate_1/file_111" ];
        this.confirm(expect_test);

        // now try to update past the removes

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple DELETEs 1" ] );
        testlib.verifyFailure(fnstr, "Dirty update (moved file will be removed) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// moved_dirs_get_removed ///////////////////////////////////////////////");
        if (!this.assert_clean_WD())
            return;

        // move some dirs that will be removed
        this.do_vc_move("dir_1/dir_11",               "dir_alternate_1");
        this.do_vc_move("dir_1",                      "dir_alternate_1");

        expect_test["Moved"] = [ "@/dir_alternate_1/dir_11",
                                 "@/dir_alternate_1/dir_1" ];
        this.confirm(expect_test);

        // now try to update past the removes

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple RMDIRs 1" ] );
        testlib.verifyFailure(fnstr, "Dirty update (moved directory will be removed) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// moved_files_get_renamed ///////////////////////////////////////////////");
        if (!this.assert_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple RMDIRs 1"))
            return;

        // move some files that will be renamed
        
        this.do_vc_move("file_2",               "dir_alternate_1");
        this.do_vc_move("dir_2/file_22",        "dir_alternate_1");
        this.do_vc_move("dir_2/dir_22/file_222","dir_alternate_1");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/file_2",
                                 "@/dir_alternate_1/file_22",
                                 "@/dir_alternate_1/file_222" ];
        this.confirm(expect_test);

        // now try to update past the rotational renames
        // where the files end up with their original names
        // eg: a->b b->c c->a

        this.do_update_when_dirty_by_name("Simple file RENAMEs 2 original");

        // The update should have succeeded.  Let's check
        // our status ...

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/file_2",
                                 "@/dir_alternate_1/file_22",
                                 "@/dir_alternate_1/file_222" ];
        this.confirm(expect_test);

        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple RMDIRs 1"))
            return;

        // move some files that will be renamed
        
        this.do_vc_move("file_2",               "dir_alternate_1");
        this.do_vc_move("dir_2/file_22",        "dir_alternate_1");
        this.do_vc_move("dir_2/dir_22/file_222","dir_alternate_1");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/file_2",
                                 "@/dir_alternate_1/file_22",
                                 "@/dir_alternate_1/file_222" ];
        this.confirm(expect_test);

        // now try to update into the renames

        this.do_update_when_dirty_by_name("Simple file RENAMEs 2 again");

        // The update should have succeeded.  Let's check
        // our status ...

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/file_2_renamed_again",
                                 "@/dir_alternate_1/file_22_renamed_again",
                                 "@/dir_alternate_1/file_222_renamed_again" ];
        this.confirm(expect_test);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// moved_dirs_get_renamed ///////////////////////////////////////////////");

        if (!this.do_update_when_clean_by_name("Simple ADDs 4"))
            return;

        // move some dirs that will be renamed
        this.do_vc_move("dir_4/dir_44",               "dir_alternate_1");
        this.do_vc_move("dir_4",                      "dir_alternate_1");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/dir_44",
                                 "@/dir_alternate_1/dir_4" ];
        this.confirm(expect_test);

        // now try to update past the rotational renames
        // where the directories end up with their original names
        // eg: a->b b->c c->a

        this.do_update_when_dirty_by_name("Simple directory RENAMEs 4 original");

        // The update should have succeeded.  Let's check
        // our status ...

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/dir_44",
                                 "@/dir_alternate_1/dir_4" ];
        this.confirm(expect_test);

        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 4"))
            return;

        // move some dirs that will be renamed
        this.do_vc_move("dir_4/dir_44",               "dir_alternate_1");
        this.do_vc_move("dir_4",                      "dir_alternate_1");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/dir_44",
                                 "@/dir_alternate_1/dir_4" ];
        this.confirm(expect_test);

        // now try to update into the renames

        this.do_update_when_dirty_by_name("Simple directory RENAMEs 4 again");

        // The update should have succeeded.  Let's check
        // our status ...

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/dir_4_renamed_again",
                                 "@/dir_alternate_1/dir_44_renamed_again" ];
        this.confirm(expect_test);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// moved_files_get_modified ///////////////////////////////////////////////");
        if (!this.assert_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 6"))
            return;

        // move some files that will be edited
        this.do_vc_move("file_6",               "dir_alternate_1");
        this.do_vc_move("dir_6/file_66",        "dir_alternate_1");
        this.do_vc_move("dir_6/dir_66/file_666","dir_alternate_1");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/file_6",
                                 "@/dir_alternate_1/file_66",
                                 "@/dir_alternate_1/file_666" ];
        this.confirm(expect_test);

        // now try to update past the edits

        this.do_update_when_dirty_by_name("Simple file MODIFY 6.3");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/file_6",
                                 "@/dir_alternate_1/file_66",
                                 "@/dir_alternate_1/file_666" ];
        this.confirm(expect_test);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;
    }

    this.test_update_dirty_edits = function()
    {
        print("/// modified_files_get_removed ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        print("/// checkpoint 1");
        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;
        print("/// checkpoint 2");

        // modify some files that will be removed
        this.do_fsobj_append_file("file_1",                "hello1");
        this.do_fsobj_append_file("dir_1/file_11",         "hello1");
        this.do_fsobj_append_file("dir_1/dir_11/file_111", "hello1");

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/file_1",
                                    "@/dir_1/file_11",
                                    "@/dir_1/dir_11/file_111" ];
        this.confirm(expect_test);

        // now try to update past the removes

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple DELETEs 1" ] );
        testlib.verifyFailure(fnstr, "Dirty update (modified file will be removed) should fail", "Consider COMMIT followed by MERGE");

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/file_1",
                                    "@/dir_1/file_11",
                                    "@/dir_1/dir_11/file_111" ];
        this.confirm(expect_test);

        // clean up after ourselves
        print("/// checkpoint 3");
        if (!this.force_clean_WD())
            return;

        //////////////////////////////////////////////////////////////////
        // 2010/06/30 There was a problem in force_clean_WD where it was
        //            not always removing ~sg00~ backup files.  I just 
        //            fixed it, but lets verify that.

        var expect_test = new Array;
        this.confirm_when_ignoring_ignores(expect_test);

        //////////////////////////////////////////////////////////////////

        print("/// checkpoint 4");

        print("/// modified_files_get_renamed ///////////////////////////////////////////////");
        print("/// checkpoint 5");
        if (!this.assert_clean_WD())
            return;
        print("/// checkpoint 6");
        if (!this.do_update_when_clean_by_name("Simple RMDIRs 1"))
            return;
        print("/// checkpoint 7");

        // modify some files that will be renamed
        
        this.do_fsobj_append_file("file_2",                "hello2");
        this.do_fsobj_append_file("dir_2/file_22",         "hello2");
        this.do_fsobj_append_file("dir_2/dir_22/file_222", "hello2");

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/file_2",
                                    "@/dir_2/file_22",
                                    "@/dir_2/dir_22/file_222" ];
        this.confirm(expect_test);

        // now try to update past the rotational renames
        // where the files end up with their original names
        // eg: a->b b->c c->a

        this.do_update_when_dirty_by_name("Simple file RENAMEs 2 original");

        // The update should have succeeded.  Let's check
        // our status ...

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/file_2",
                                    "@/dir_2/file_22",
                                    "@/dir_2/dir_22/file_222" ];
        this.confirm(expect_test);

        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple RMDIRs 1"))
            return;

        // modify some files that will be renamed
        
        this.do_fsobj_append_file("file_2",                "hello2");
        this.do_fsobj_append_file("dir_2/file_22",         "hello2");
        this.do_fsobj_append_file("dir_2/dir_22/file_222", "hello2");

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/file_2",
                                    "@/dir_2/file_22",
                                    "@/dir_2/dir_22/file_222" ];
        this.confirm(expect_test);

        // now try to update into the renames

        this.do_update_when_dirty_by_name("Simple file RENAMEs 2 again");

        // The update should have succeeded.  Let's check
        // our status ...

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/file_2_renamed_again",
                                    "@/dir_2/file_22_renamed_again",
                                    "@/dir_2/dir_22/file_222_renamed_again" ];
        this.confirm(expect_test);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// modified_files_get_moved ///////////////////////////////////////////////");
        if (!this.assert_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // modify some files that will be moved
        this.do_fsobj_append_file("file_3",                "hello3");
        this.do_fsobj_append_file("dir_3/file_33",         "hello3");
        this.do_fsobj_append_file("dir_3/dir_33/file_333", "hello3");

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/file_3",
                                    "@/dir_3/file_33",
                                    "@/dir_3/dir_33/file_333" ];
        this.confirm(expect_test);

        // now try to update past the moves

        this.do_update_when_dirty_by_name("Simple file MOVEs 3");

        var expect_test = new Array;
        expect_test["Modified"] = [ "@/dir_3_alternate/file_3",
                                    "@/dir_3_alternate/file_33",
                                    "@/dir_3_alternate/file_333" ];
        this.confirm(expect_test);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

    }

    this.test_divergent_file_moves = function()
    {
        print("/// divergent_file_move ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // move some files that will be moved
        this.do_vc_move("file_3",               "dir_alternate_1");
        this.do_vc_move("dir_3/file_33",        "dir_alternate_1");
        this.do_vc_move("dir_3/dir_33/file_333","dir_alternate_1");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/file_3",
                                 "@/dir_alternate_1/file_33",
                                 "@/dir_alternate_1/file_333" ];
        this.confirm(expect_test);

        // now try to update past the moves

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple file MOVEs 3" ] );
        testlib.verifyFailure(fnstr, "Dirty update (divergent file move) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// compatible_file_move ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // move some files that will be moved
        this.do_vc_move("file_3",               "dir_3_alternate");
        this.do_vc_move("dir_3/file_33",        "dir_3_alternate");
        this.do_vc_move("dir_3/dir_33/file_333","dir_3_alternate");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_3_alternate/file_3",
                                 "@/dir_3_alternate/file_33",
                                 "@/dir_3_alternate/file_333" ];
        this.confirm(expect_test);

        // now try to update into the same moves

        this.do_update_when_dirty_by_name("Simple file MOVEs 3");

        var expect_nothing = new Array;
        this.confirm(expect_nothing);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// pseudo_compatible_file_move ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // move some files that will be moved
        this.do_vc_move("file_3",               "dir_3_alternate");
        this.do_vc_move("dir_3/file_33",        "dir_3_alternate");
        this.do_vc_move("dir_3/dir_33/file_333","dir_3_alternate");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_3_alternate/file_3",
                                 "@/dir_3_alternate/file_33",
                                 "@/dir_3_alternate/file_333" ];
        this.confirm(expect_test);

        // now try to update past the moves

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple file MOVEs 3 again" ] );
        testlib.verifyFailure(fnstr, "Dirty update (updating past compatible file move) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;
    }

    this.test_divergent_dir_moves = function()
    {
        print("/// divergent_dir_move ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 5"))
            return;

        // move some dirs that will be moved
        this.do_vc_move("dir_5/dir_55",        "dir_alternate_1");
        this.do_vc_move("dir_5",               "dir_alternate_1");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_alternate_1/dir_55",
                                 "@/dir_alternate_1/dir_5" ];
        this.confirm(expect_test);

        // now try to update past the moves

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple directory MOVEs 5" ] );
        testlib.verifyFailure(fnstr, "Dirty update (divergent dir move) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// compatible_dir_move ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 5"))
            return;

        // move some dirs that will be moved
        this.do_vc_move("dir_5/dir_55",        "dir_5_alternate");
        this.do_vc_move("dir_5",               "dir_5_alternate");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_5_alternate/dir_55",
                                 "@/dir_5_alternate/dir_5" ];
        this.confirm(expect_test);

        // now try to update into the same moves

        this.do_update_when_dirty_by_name("Simple directory MOVEs 5");

        var expect_nothing = new Array;
        this.confirm(expect_nothing);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// pseudo_compatible_dir_move ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 5"))
            return;

        // move some dirs that will be moved
        this.do_vc_move("dir_5/dir_55",        "dir_5_alternate");
        this.do_vc_move("dir_5",               "dir_5_alternate");

        var expect_test = new Array;
        expect_test["Moved"] = [ "@/dir_5_alternate/dir_55",
                                 "@/dir_5_alternate/dir_5" ];
        this.confirm(expect_test);


        // now try to update past the moves

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple directory MOVEs 5 again" ] );
        testlib.verifyFailure(fnstr, "Dirty update (updating past compatible dir move) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;
    }

    this.test_divergent_file_renames = function()
    {
        print("/// divergent_file_rename ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // rename some files that will be renamed
        this.do_vc_rename("file_2",               "file_2_diverged");
        this.do_vc_rename("dir_2/file_22",        "file_22_diverged");
        this.do_vc_rename("dir_2/dir_22/file_222","file_222_diverged");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/file_2_diverged",
                                   "@/dir_2/file_22_diverged",
                                   "@/dir_2/dir_22/file_222_diverged" ];
        this.confirm(expect_test);

        // now try to update past the renames

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple file RENAMEs 2" ] );
        testlib.verifyFailure(fnstr, "Dirty update (divergent file rename) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// compatible_file_rename ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // rename some files that will be renamed
        this.do_vc_rename("file_2",               "file_2_renamed");
        this.do_vc_rename("dir_2/file_22",        "file_22_renamed");
        this.do_vc_rename("dir_2/dir_22/file_222","file_222_renamed");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/file_2_renamed",
                                   "@/dir_2/file_22_renamed",
                                   "@/dir_2/dir_22/file_222_renamed" ];
        this.confirm(expect_test);

        // now try to update into the same renames

        this.do_update_when_dirty_by_name("Simple file RENAMEs 2");

        var expect_nothing = new Array;
        this.confirm(expect_nothing);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// pseudo_compatible_file_rename ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 3"))
            return;

        // rename some files that will be renamed
        this.do_vc_rename("file_2",               "file_2_renamed");
        this.do_vc_rename("dir_2/file_22",        "file_22_renamed");
        this.do_vc_rename("dir_2/dir_22/file_222","file_222_renamed");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/file_2_renamed",
                                   "@/dir_2/file_22_renamed",
                                   "@/dir_2/dir_22/file_222_renamed" ];
        this.confirm(expect_test);

        // now try to update past the renames

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple file RENAMEs 2 again" ] );
        testlib.verifyFailure(fnstr, "Dirty update (updating past compatible file rename) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;
    }

    this.test_divergent_dir_renames = function()
    {
        print("/// divergent_dir_rename ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 4"))
            return;

        // rename some dirs that will be renamed
        this.do_vc_rename("dir_4/dir_44",  "dir_44_diverged");
        this.do_vc_rename("dir_4",         "dir_4_diverged");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/dir_4_diverged/dir_44_diverged",
                                   "@/dir_4_diverged" ];
        this.confirm(expect_test);

        // now try to update past the renames

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple directory RENAMEs 4" ] );
        testlib.verifyFailure(fnstr, "Dirty update (divergent dir rename) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// compatible_dir_rename ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 4"))
            return;

        // rename some dirs that will be renamed
        this.do_vc_rename("dir_4/dir_44",  "dir_44_renamed");
        this.do_vc_rename("dir_4",         "dir_4_renamed");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/dir_4_renamed/dir_44_renamed",
                                   "@/dir_4_renamed" ];
        this.confirm(expect_test);

        // now try to update into the same renames

        this.do_update_when_dirty_by_name("Simple directory RENAMEs 4");

        var expect_nothing = new Array;
        this.confirm(expect_nothing);

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;

        print("/// pseudo_compatible_dir_rename ///////////////////////////////////////////////");
        if (!this.force_clean_WD())
            return;
        if (!this.do_update_when_clean_by_name("Simple ADDs 4"))
            return;

        // rename some dirs that will be renamed
        this.do_vc_rename("dir_4/dir_44",  "dir_44_renamed");
        this.do_vc_rename("dir_4",         "dir_4_renamed");

        var expect_test = new Array;
        expect_test["Renamed"] = [ "@/dir_4_renamed/dir_44_renamed",
                                   "@/dir_4_renamed" ];
        this.confirm(expect_test);

        // now try to update past the renames

        globalThis = this;
        var fnstr = formatFunctionString("globalThis.do_update_when_dirty_by_name", [ "Simple directory RENAMEs 4 again" ] );
        testlib.verifyFailure(fnstr, "Dirty update (updating past compatible dir rename) should fail", "Consider COMMIT followed by MERGE");

        // clean up after ourselves
        if (!this.force_clean_WD())
            return;
    }

    this.test_file_fsobj_deletes_to_edits = function()
    {
    /**************************************************
    Fixed: SPRAWL-652
    When attempting a dirty update where a LOST file has
    been EDITed in the target changeset, we silently restore
    the LOST file(s) at the new version, with all edits applied.
    This test fails on Mac & Linux because, although the files
    are restored correctly, the x-bit is set on the restored
    files regardless of its state in the repo.  Status shows
    "ChangedAttrs," and the test fails.

    NOTE: There are a few sections marked with DEBUG
          that can/should be removed when the test goes live.
    **************************************************/
    /// fsobj_remove_files ///////////////////////////////////////////////
        var i;
        var filepath = new Array();
        filepath[0] = "file_6";
        filepath[1] = "dir_6/file_66";
        filepath[2] = "dir_6/dir_66/file_666";

        if (!this.force_clean_WD())
            return;

        if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
        {
            // first, let's fetch the original x-bits of the files
            if (!this.do_update_when_clean_by_name("Simple file MODIFY 6.3"))
                return;
            var prebits = new Array();
            for (i=0; i<filepath.length; i++) {
                prebits[i] = (sg.fs.stat(filepath[i]).permissions & 64);
            }
        }

        // now to the previous changeset
        if (!this.do_update_when_clean_by_name("Simple ADDs 6"))
            return;

        /* ## DEBUG ##
        if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
        {
            // first, let's save the files' pre-delete, pre-update attrbits listings
            var preup = "";
            for (i=0; i<filepath.length; i++) {
                preup += sg.exec("ls", "-la", filepath[i]).stdout;
            }
        }
           ## /DEBUG ## */

        // LOSE some files that will be edited

        for (i=0; i<filepath.length; i++) {
            this.do_fsobj_remove_file(filepath[i]);
        }
        // no commit - leave the dirt

        /* ## DEBUG ##
        dumpTree("## Pending_tree BEFORE the update:");
        if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
        {
            print("## Original attrbits:\n" + preup);
        }
           ## /DEBUG ## */

        // now try to update past the edits

        this.do_update_when_dirty_by_name("Simple file MODIFY 6.3");

        // confirm that we got to our baseline
        this.assert_current_baseline_by_name("Simple file MODIFY 6.3");

        /* ## DEBUG ##
        if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
        {
            // now check the post-update attribits
            var postup = "";
            for (i=0; i<filepath.length; i++) {
                postup += sg.exec("ls", "-la", filepath[i]).stdout;
            }
            print("## Post-update attrbits:\n" + postup);
        }
        dumpTree("Pending_tree AFTER the update:");
           ## /DEBUG ## */

        // Let's check the permissions, just to be sure...

        if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
        {
            var postbits = new Array();
            for (i=0; i<filepath.length; i++) {
                var postbits = (sg.fs.stat(filepath[i]).permissions & 64);
                if (postbits != prebits[i]) {
                    sprawlunit.ok( (0), "Owner x-bit for " + filepath[i] + " has changed from " + prebits[i].toString(8) + " to " + postbits.toString(8));
                }
            }
        }

        // check for clean WD
        this.assert_clean_WD();

        // clean up after ourselves
        this.force_clean_WD();
    }

    if ((sg.platform() == "LINUX") || (sg.platform() == "MAC"))
    {
        this.test_file_fsobj_deletes_to_edits_2 = function()
        {
            // Like the above test, but we use
            // files that have the +x bit set.

            var i;
            var filepath = new Array();
            filepath[0] = "dir_attrbits/file_attrbits_1";

            if (!this.force_clean_WD())
                return;

            // Start with a baseline that has a file that already has +x on it.

            if (!this.do_update_when_clean_by_name("AttrBits Chmod +x"))
                return;

            // LOSE some files that will be edited

            for (i=0; i<filepath.length; i++) {
                this.do_fsobj_remove_file(filepath[i]);
            }
            // no commit - leave the dirt

            // now try to update past the edits
            // the LOST file should be restored and it should NOT have +x.

            this.do_update_when_dirty_by_name("AttrBits Off");

            // confirm that we got to our baseline
            this.assert_current_baseline_by_name("AttrBits Off");

            // check for clean WD
            this.assert_clean_WD();

            // clean up after ourselves
            this.force_clean_WD();
        }

        this.test_file_fsobj_deletes_to_edits_3 = function()
        {
            // Like the above test, but we use
            // files that have the +x bit set.

            var i;
            var filepath = new Array();
            filepath[0] = "dir_attrbits/file_attrbits_1";

            if (!this.force_clean_WD())
                return;

            // Start with a baseline that has a file that does not have +x on it.

            if (!this.do_update_when_clean_by_name("AttrBits Off"))
                return;

            // LOSE some files that will be edited

            for (i=0; i<filepath.length; i++) {
                this.do_fsobj_remove_file(filepath[i]);
            }
            // no commit - leave the dirt

            // now try to update past the edits
            // the LOST file should be restored WITH +x.

            this.do_update_when_dirty_by_name("AttrBits Chmod +x");

            // confirm that we got to our baseline
            this.assert_current_baseline_by_name("AttrBits Chmod +x");

            // check for clean WD
            this.assert_clean_WD();

            // clean up after ourselves
            this.force_clean_WD();
        }
    }

    this.tearDown = function() 
    {

    }
}
