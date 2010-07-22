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

function stClcPlusZing() {
    
    this.addItem = function(id, title, description, original_estimate) {
        var repo = sg.open_repo(repInfo.repoName);
        var zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);
        var ztx = zs.begin_tx();
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.id = id;
        zrec.title = title;
        zrec.description = description;
        zrec.original_estimate = original_estimate;
        ztx.commit();
        repo.close();
        return recid;
    }

    //this.setUp = function() {
    //    // Add a work item for future reference
    //    this.itemId = "tcpz1";
    //    addItem(itemId, "Common item for stClcPlusZing tests.", "...", 1);
    //    
    //    // Create a changeset for future reference
    //    {
    //        sg.file.write("stClcPlusZing.txt", "stClcPlusZing");
    //        sg.pending_tree.add("stClcPlusZing.txt");
    //        this.changesetId = sg.pending_tree.commit();
    //    }
    //}
    
    this.testCommitWithItemAssociation = function() {
        var item_recid = this.addItem("tcwia1", "Item for testCommitWithItemAssociation", "...", 0);
        
        sg.file.write("testCommitWithItemAssociation.txt", "testCommitWithItemAssociation");
        sg.exec("vv", "addremove");
        var o = sg.exec("vv",
            "commit",
            "--message=Adding a file from testCommitWithItemAssociation.",
            "--assoc=tcwia1");
        
        if(!testlib.testResult(o.exit_status==0))return;
        
        var repo = sg.open_repo(repInfo.repoName);
        var zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);
        var ztx = zs.begin_tx();
        
        var item = ztx.open_record(item_recid);
        var a = item.changesets.to_array();
        if(!testlib.testResult(a.length==1))return;
        
        var vc_cs = ztx.open_record(a[0]);
        testlib.testResult(vc_cs.commit.length>0); //todo: check for actual match, not just nonempty string
        
        ztx.abort();
        repo.close();
    }
    
    this.testCommitWithMultipleItemAssociations = function() {
        var item1_recid = this.addItem("tcwmia1", "Item #1 for testCommitWithMultipleItemAssociations", "...", 0);
        var item2_recid = this.addItem("tcwmia2", "Item #2 for testCommitWithMultipleItemAssociations", "...", 0);
        
        sg.file.write("testCommitWithMultipleItemAssociations.txt", "testCommitWithMultipleItemAssociations");
        sg.exec("vv", "addremove");
        var o = sg.exec("vv",
            "commit",
            "--message=Adding a file from testCommitWithMultipleItemAssociations.",
            "--assoc=tcwmia1",
            "--assoc=tcwmia2");
        
        if(!testlib.testResult(o.exit_status==0))return;
        
        var repo = sg.open_repo(repInfo.repoName);
        var zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);
        var ztx = zs.begin_tx();
        
        // Verify first item.
        {
            var item = ztx.open_record(item1_recid);
            var a = item.changesets.to_array();
            if(!testlib.testResult(a.length==1))return;
            
            var vc_cs = ztx.open_record(a[0]);
            testlib.testResult(vc_cs.commit.length>0); //todo: check for actual match, not just nonempty string
        }
        
        // Verify second item.
        {
            var item = ztx.open_record(item2_recid);
            var a = item.changesets.to_array();
            if(!testlib.testResult(a.length==1))return;
            
            var vc_cs = ztx.open_record(a[0]);
            testlib.testResult(vc_cs.commit.length>0); //todo: check for actual match, not just nonempty string
        }
        
        ztx.abort();
        repo.close();
    }
    
    //this.testCommitWithBogusItemAssociation = function() {
    //    var history_before = sg.exec("vv", "history").stdout;
    //    
    //    sg.file.write("testCommitWithBogusItemAssociation.txt", "testCommitWithBogusItemAssociation");
    //    sg.exec("vv", "addremove");
    //    var o = sg.exec("vv",
    //        "commit",
    //        "--message=Adding a file from testCommitWithBogusItemAssociation.",
    //        "--assoc=bogus");
    //    
    //    testlib.testResult(o.exit_status!=0);
    //    
    //    var history_after = sg.exec("vv", "history").stdout;
    //    testlib.testResult(history_before == history_after);
    //}
}

