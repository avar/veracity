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

function sleepStupidly(usec)
    {
	var endtime= new Date().getTime() + (usec * 1000);
        while (new Date().getTime() < endtime)
	    ;
    }
    function commitWrapper(obj)
    {
    	obj.changesets.push(sg.pending_tree.commit());
    	sleepStupidly(1);
	obj.times.push(new Date().getTime());
	runCLCHistory("tag", "--rev="+obj.changesets[obj.changesets.length - 1],  "changeset_" + obj.changesets.length);
    	sleepStupidly(1);
    }
    function runCLCHistory(arg1, arg2, arg3, arg4, arg5, arg6, arg7)
    {
    	var execResults = null;
    	var vv = "vv"
    	if (arg7 != null)
    	{
		print("executing:  " + vv + " " + arg1 + " " + arg2 + " " + arg3 + " " + arg4 + " " + arg5 + " " + arg6 + " " + arg7);
		execResults = sg.exec(vv, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
	}
    	else if (arg6 != null)
    	{
		print("executing:  " + vv + " " + arg1 + " " + arg2 + " " + arg3 + " " + arg4 + " " + arg5 + " " + arg6);
    		execResults = sg.exec(vv, arg1, arg2, arg3, arg4, arg5, arg6);
	}
    	else if (arg5 != null)
    	{
    		print("executing:  " + vv + " " + arg1 + " " + arg2 + " " + arg3 + " " + arg4 + " " + arg5);
    		execResults = sg.exec(vv, arg1, arg2, arg3, arg4, arg5);
	}
    	else if (arg4 != null)
    	{
    		print("executing:  " + vv + " " + arg1 + " " + arg2 + " " + arg3 + " " + arg4);
    		execResults = sg.exec(vv, arg1, arg2, arg3, arg4);
	}
    	else if (arg3 != null)
    	{
    		print("executing:  " + vv + " " + arg1 + " " + arg2 + " " + arg3);
    		execResults = sg.exec(vv, arg1, arg2, arg3);
	}
    	else if (arg2 != null)
    	{
    		print("executing:  " + vv + " " + arg1 + " " + arg2);
    		execResults = sg.exec(vv, arg1, arg2);
	}
    	else if (arg1 != null)
    	{
    		print("executing:  " + vv + " " + arg1);
    		execResults = sg.exec(vv, arg1);
	}
    	else
    	{
    		print("executing:  " + vv);
    		execResults = sg.exec(vv);
	}
    	
    	changesets =execResults.stdout.split("changeset_id:  ");
   	changesets.splice(0, 1);  //Remove the first, empty array element
    	print(dumpObj(execResults));
    	return changesets;
    }

function formatDate(ticks, includeTime)
{
	var dateObj = new Date();
	dateObj.setTime(ticks);
	var returnStr = "";
	returnStr = dateObj.getFullYear() + "-" + (dateObj.getMonth() + 1) + "-" + dateObj.getDate();
	if (includeTime != null)
		returnStr += " " + dateObj.getHours() + ":" + dateObj.getMinutes() + ":" + dateObj.getSeconds();
	return returnStr;
}

function stHistoryOnRoot() {
   this.changesets = new Array();
   this.changesets.push("initial commit");
   this.times = new Array();
   this.times.push(0);
    this.setUp = function() {
	sg.fs.mkdir("test");
	sg.pending_tree.addremove();
	commitWrapper(this);
	sg.fs.mkdir("test2");
	sg.pending_tree.addremove();
	commitWrapper(this);
	sg.fs.mkdir("test3");
	sg.pending_tree.addremove();
	commitWrapper(this);
	sg.fs.mkdir("test2/sub1");
	sg.pending_tree.addremove();
	commitWrapper(this);
    }
    
    
    this.testRoot = function() {
    	//Verify that a limitless history on @ returns an entry for every
    	//commit.
    	var historyResults = sg.pending_tree.history("./");
    	testlib.equal(this.changesets.length, historyResults.length, "History should return an entry for every changeset.");
   	var clchistoryResults = runCLCHistory("history", "./");
   	testlib.equal(this.changesets.length, clchistoryResults.length, "History should return an entry for every changeset.");
   	var historyResults = sg.pending_tree.history();
    	testlib.equal(this.changesets.length, historyResults.length, "History should return an entry for every changeset.");
   	var clchistoryResults = runCLCHistory("history");
   	testlib.equal(this.changesets.length, clchistoryResults.length, "History should return an entry for every changeset.");
    }
    
    this.testFromDate = function() {
        var historyResults = sg.pending_tree.history(["from=" + this.times[1] ], "./");
    	testlib.equal(this.changesets.length - 2, historyResults.length, "History results should not have included the first two commits");
    	var clchistoryResults = runCLCHistory("history", "--from=" + formatDate(this.times[1], true));
   	testlib.equal(this.changesets.length - 2, clchistoryResults.length, "History results should not have included the first two commits");
    }
    
    this.testToDate = function() {
        var historyResults = sg.pending_tree.history(["to=" + this.times[this.times.length - 2] ], "./");
    	testlib.equal(this.changesets.length - 1, historyResults.length, "History results should not have included the last commit");
    	var clchistoryResults = runCLCHistory("history", "--to=" + formatDate(this.times[this.times.length - 2], true));
   	testlib.equal(this.changesets.length - 1, clchistoryResults.length, "History results should not have included the last commit");
    }
    
    this.testToDateAndFromDate = function() {
        var historyResults = sg.pending_tree.history(["to=" + this.times[this.times.length - 3], "from=" + this.times[1] ], "./");
    	testlib.equal(this.changesets.length - 4, historyResults.length, "History results should not have included the last  two commits or the first two commits");
    	var clchistoryResults = runCLCHistory("history", "--to=" + formatDate(this.times[this.times.length - 3], true), "--from=" +  formatDate(this.times[1], true));
   	testlib.equal(this.changesets.length - 4, clchistoryResults.length, "History results should not have included the last  two commits or the first two commits");
    }

    this.testSubFolder = function() {
        var historyResults = sg.pending_tree.history("test2");
    	testlib.equal(2, historyResults.length, "test2 changed in two dag nodes");
	var clchistoryResults = runCLCHistory("history", "test2");
	testlib.equal(2, clchistoryResults.length, "test2 changed in two dag nodes");
    }

    this.testSubFolder = function() {
        var historyResults = sg.pending_tree.history("test");
    	testlib.equal(1, historyResults.length, "test changed in one dag nodes");
	var clchistoryResults = runCLCHistory("history", "test");
	testlib.equal(1, clchistoryResults.length, "test changed in one dag nodes");
    } 
    
    this.testRev = function() {
    	var historyResults = sg.pending_tree.history(["rev=" +this.changesets[this.changesets.length - 3] ])
    	testlib.equal(1, historyResults.length, "rev should return 1 entry");    
    	var clchistoryResults = runCLCHistory("history", "--rev=" + this.changesets[this.changesets.length - 3]);
    	testlib.equal(1, clchistoryResults.length, "rev should return 1 entry");

    	var historyResults = sg.pending_tree.history(["rev=" +this.changesets[this.changesets.length - 2], "rev=" +this.changesets[this.changesets.length - 3] ])
    	testlib.equal(2, historyResults.length, "rev should return 2 entry");    
    	var clchistoryResults = runCLCHistory("history", "--rev=" + this.changesets[this.changesets.length - 3], "--rev=" + this.changesets[this.changesets.length - 2]);
    	testlib.equal(2, clchistoryResults.length, "rev should return 2 entry");
    }       

    this.testPartialRev = function() {
    	var historyResults = sg.pending_tree.history(["rev=" +this.changesets[this.changesets.length - 3].substring(0, 12) ])
    	testlib.equal(1, historyResults.length, "rev should return 1 entry");    
    	var clchistoryResults = runCLCHistory("history", "--rev=" + this.changesets[this.changesets.length - 3].substring(0, 12));
    	testlib.equal(1, clchistoryResults.length, "rev should return 1 entry");

    	var historyResults = sg.pending_tree.history(["rev=" +this.changesets[this.changesets.length - 3].substring(0, 12), "rev=" +this.changesets[this.changesets.length - 2].substring(0, 12) ])
    	testlib.equal(2, historyResults.length, "rev should return 2 entries");    
    	var clchistoryResults = runCLCHistory("history", "--rev=" + this.changesets[this.changesets.length - 2].substring(0, 12), "--rev=" + this.changesets[this.changesets.length - 3].substring(0, 12));
    	testlib.equal(2, clchistoryResults.length, "rev should return 2 entries");
    }       
    
    this.testTag = function() {
    	var historyResults = sg.pending_tree.history(["tag=changeset_" + (this.changesets.length - 2) ])
    	testlib.equal(1, historyResults.length, "rev should return one entry");    
    	var clchistoryResults = runCLCHistory("history", "--tag=changeset_" + (this.changesets.length - 2));
    	testlib.equal(1, clchistoryResults.length, "rev should return one entry");

    	var historyResults = sg.pending_tree.history(["tag=changeset_" + (this.changesets.length - 3), "tag=changeset_" + (this.changesets.length - 2) ])
    	testlib.equal(2, historyResults.length, "rev should return one entry");    
    	var clchistoryResults = runCLCHistory("history", "--tag=changeset_" + (this.changesets.length - 3), "--tag=changeset_" + (this.changesets.length - 2));
    	testlib.equal(2, clchistoryResults.length, "rev should return one entry");
    }       
    this.testMax = function() {
    	var historyResults = sg.pending_tree.history(["max=1"], "./")
    	testlib.equal(1, historyResults.length, "max should limit this to 1 result");    
    	testlib.equal(this.changesets[4], historyResults[0].changeset_id, "Make sure only the expected changesets are returned.")
    	var clchistoryResults = runCLCHistory("history", "./", "--max=1");
	testlib.equal(1, clchistoryResults.length, "max should limit this to 1 result");
    }       
    this.testLeaves = function() {
    	var historyResults = sg.pending_tree.history(["leaves"], "@")
    	testlib.equal(5, historyResults.length, "leaves should act normally when it's just a line");  
    	var clchistoryResults = runCLCHistory("history", "@", "--leaves");
	testlib.equal(5, clchistoryResults.length, "leaves should act normally when it's just a line");  
    }      
    this.tearDown = function() {
    }
}
function stHistoryRenamedObject() {
   this.changesets = new Array();
   this.changesets.push("initial commit");
   this.times = new Array();
	this.times.push(new Date().getTime());
    this.setUp = function() {
    	//Create an item with the correct name, but delete it, so that it's not the right GID
	sg.fs.mkdir("renameToMe");
	sg.pending_tree.addremove();
	commitWrapper(this);
	sg.pending_tree.remove("renameToMe")
	commitWrapper(this);
	sg.fs.mkdir("renameMe");
	sg.pending_tree.addremove();
	commitWrapper(this);
	sg.pending_tree.rename("renameMe", "renameToMe");
	commitWrapper(this);
    }
    
    
    this.testRenamedObject = function() {
    	var historyResults = sg.pending_tree.history("renameToMe");
    	testlib.equal(2, historyResults.length, "There should be only two entries for renameToMe");
    	testlib.equal(this.changesets[3], historyResults[1].changeset_id, "Make sure only the expected changesets are returned.")
    	testlib.equal(this.changesets[4], historyResults[0].changeset_id, "Make sure only the expected changesets are returned.")
    }
    
    this.testRenamedObjectExpectZeroResults = function() {
    	//This is limited to before the object in question existed.
    	var historyResults = sg.pending_tree.history(["to=" + this.times[1] ], "renameToMe");
    	testlib.equal(0, historyResults.length, "There should be zero results");
    }
    
    this.testRenamedObjectExpectOneResult = function() {
    	//This is limited to before the object in question existed.
    	var historyResults = sg.pending_tree.history(["to=" + this.times[3] ], "renameToMe");
    	testlib.equal(1, historyResults.length, "There should be one result");
    	testlib.equal(this.changesets[3], historyResults[0].changeset_id, "Make sure only the expected changesets are returned.")
    	
    	var historyResults = sg.pending_tree.history(["to=" + this.times[4], "from=" + this.times[3] ], "renameToMe");
    	testlib.equal(1, historyResults.length, "There should be one result");
    	testlib.equal(this.changesets[4], historyResults[0].changeset_id, "Make sure only the expected changesets are returned.")
    }
    
}
function stHistoryOnAFile() {
   this.changesets = new Array();
   this.changesets.push("initial commit");
   this.times = new Array();
	this.times.push(new Date().getTime());
    this.setUp = function() {
    	//Create an item with the correct name, but delete it, so that it's not the right GID
	sg.file.write("fileName", "initialContents");
	sg.pending_tree.addremove();
	commitWrapper(this);
	sg.file.append("fileName", "moreContents");
	commitWrapper(this);
	sg.file.write("fileName", "All New Contents");
	commitWrapper(this);
    }
    
    this.testFileHistory = function() {
    	var historyResults = sg.pending_tree.history("./fileName");
    	testlib.equal(3, historyResults.length, "There should be only three entries for fileName");
	testlib.equal(this.changesets[1], historyResults[2].changeset_id, "Make sure only the expected changesets are returned.");
    	testlib.equal(this.changesets[2], historyResults[1].changeset_id, "Make sure only the expected changesets are returned.");
    	testlib.equal(this.changesets[3], historyResults[0].changeset_id, "Make sure only the expected changesets are returned.");
    }
}

/*
function stHistoryUserFilter() {
   this.changesets = new Array();
   this.changesets.push("initial commit");
   this.times = new Array();
   this.times.push(new Date().getTime());
   this.origUserName = "";
    this.setUp = function() {
	this.origUserName = sg.local_settings().userid;
    	//Create an item with the correct name, but delete it, so that it's not the right GID
	sg.local_settings("userid", "user1");
	sg.fs.mkdir("dir1");
	sg.pending_tree.addremove();
	commitWrapper(this);
	sg.local_settings("userid", "user2");
	sg.fs.mkdir("dir2");
	sg.pending_tree.addremove();
	commitWrapper(this);
	sg.local_settings("userid", "user1");
	sg.fs.mkdir("dir3");
	sg.pending_tree.addremove();
	commitWrapper(this);
	sg.local_settings("userid", "user2");
	sg.fs.mkdir("dir4");
	sg.pending_tree.addremove();
	commitWrapper(this);
    }
    this.tearDown = function() {
	   sg.local_settings("userid", this.origUserName);
    }
    
    
    this.testLookForUser1 = function() {
    		historyResults = sg.pending_tree.history(["user=user1"], "./");
        	testlib.equal(2, historyResults.length, "There should be two results");
        	testlib.equal(this.changesets[3], historyResults[0].changeset_id, "Make sure only the expected changesets are returned.")
		testlib.equal(this.changesets[1], historyResults[1].changeset_id, "Make sure only the expected changesets are returned.")
		var clchistoryResults = runCLCHistory("history", "./", "--user=user1");
		testlib.equal(2, clchistoryResults.length, "There should be two results"); 
				
		historyResults = sg.pending_tree.history(["user=user1", "to=" + this.times[1]], "./");
        	testlib.equal(1, historyResults.length, "Restricted by both user and date");
        	}
    
    
    this.testRenamedObjectExpectZeroResults = function() {
    		historyResults = sg.pending_tree.history(["user=luser"], "./");
        	testlib.equal(0, historyResults.length, "Expect zero results");
        	var clchistoryResults = runCLCHistory("history", "./", "--user=luser");
		testlib.equal(0, clchistoryResults.length, "There should be zero results"); 
		
    }
    
    this.testLookForUser2 = function() {
    	historyResults = sg.pending_tree.history(["user=user2", "from=" + this.times[1]], "./");    	
	testlib.equal(2, historyResults.length, "Restricted by both user and date");
        	testlib.equal(this.changesets[4], historyResults[0].changeset_id, "Make sure only the expected changesets are returned.")
		testlib.equal(this.changesets[2], historyResults[1].changeset_id, "Make sure only the expected changesets are returned.")
				
        }
}

function stHistoryStampFilter() {
   this.changesets = new Array();
   this.changesets.push("initial commit");
   this.times = new Array();
   this.times.push(new Date().getTime());
   this.origUserName = "";
    this.setUp = function() {
	this.origUserName = sg.local_settings().userid;
	runCLCHistory("stamp", "--rev=" +sg.pending_tree.parents()[0], "--stamp=user_" +sg.local_settings().userid, "--stamp=odd","--stamp=one" );
    	//Create an item with the correct name, but delete it, so that it's not the right GID
	sg.local_settings("userid", "user1");
	sg.fs.mkdir("dir1");
	sg.pending_tree.addremove();
	commitWrapper(this);
	runCLCHistory("stamp", "--rev=" +sg.pending_tree.parents()[0], "--stamp=user_" +sg.local_settings().userid, "--stamp=even","--stamp=two" );
	sg.local_settings("userid", "user2");
	sg.fs.mkdir("dir2");
	sg.pending_tree.addremove();
	commitWrapper(this);
	runCLCHistory("stamp", "--rev=" +sg.pending_tree.parents()[0], "--stamp=user_" +sg.local_settings().userid, "--stamp=odd","--stamp=three" );
	sg.local_settings("userid", "user1");
	sg.fs.mkdir("dir3");
	sg.pending_tree.addremove();
	commitWrapper(this);
	runCLCHistory("stamp", "--rev=" +sg.pending_tree.parents()[0], "--stamp=user_" +sg.local_settings().userid, "--stamp=even","--stamp=four" );
	sg.local_settings("userid", "user2");
	sg.fs.mkdir("dir4");
	sg.pending_tree.addremove();
	commitWrapper(this);
	runCLCHistory("stamp", "--rev=" +sg.pending_tree.parents()[0], "--stamp=user_" +sg.local_settings().userid, "--stamp=odd","--stamp=five" );
    }
    this.tearDown = function() {
	   sg.local_settings("userid", this.origUserName);
    }
    
    
    this.testLookForUser1 = function() {
		var clchistoryResults = runCLCHistory("history", "./", "--stamp=odd");
		testlib.equal(3, clchistoryResults.length, "There should be three results"); 
		var clchistoryResults = runCLCHistory("history", "./", "--stamp=five");
		testlib.equal(1, clchistoryResults.length, "There should be one result"); 
        	}
    
    this.testRenamedObjectExpectZeroResults = function() {
        	var clchistoryResults = runCLCHistory("history", "./", "--stamp=bogus");
		testlib.equal(0, clchistoryResults.length, "There should be zero results"); 
    }
    
    this.testLookForUser2 = function() {
        	var clchistoryResults = runCLCHistory("history", "./", "--user=user2", "--stamp=odd");
		testlib.equal(2, clchistoryResults.length, "There should be two results"); 
        	var clchistoryResults = runCLCHistory("history", "./", "--user=user2", "--stamp=even");
		testlib.equal(0, clchistoryResults.length, "There should be zero results"); 
				
        }
}

*/

