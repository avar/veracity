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
// TODO consider creating a second WD (or exporting to a temp directory)
// TODO (or making a backup of the WD after each commit)
// TODO and doing some kind of external diff (like gnu-diff's folder diff)
// TODO to compare that the contents of the UPDATEd WD match the other
// TODO copy, so that we're not just relying on the pendingtree/treediff
// TODO to tell us everything is OK (which kinda feels like grading your
// TODO own homework).
//////////////////////////////////////////////////////////////////

function stUpdateTestsWithCleanWD() 
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


    //////////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////////

    this.test_clean_all_in_one_step = function()
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

    this.test_clean_linearly = function()
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
        if (testlib.defined.SG_LONGTESTS != true)
        {
            print("Skipping tests because SG_LONGTESTS not set.");
            return;
        }

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

    this.test_clean_self = function()
    {
        // assuming we have a clean WD.
        // warp to arbitrary changeset.
        // test UPDATE jump to same changeset should do nothing.

        if (!this.do_clean_warp_to_head())
            return;

        if (!this.do_clean_warp_to_head())
            return;
    }

    this.test_clean_branch = function()
    {
        // assuming we have a clean WD.
        // warp to an arbitrary changeset.
        // create another linear sequence in a branch off of it.
        // test UPDATE clean jumping between cousins.

        if (!this.force_clean_WD())
            return;

        if (!this.do_update_when_clean(0))
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
        this.do_commit_in_branch("Simple ADDs Z");

        // now bounce between the 2 branches

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
