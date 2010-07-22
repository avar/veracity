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
	runCLCParents("tag", "--rev="+obj.changesets[obj.changesets.length - 1],  "changeset_" + obj.changesets.length);
    	sleepStupidly(1);
    }
	// Removes ending whitespaces
function RTrim( value ) {
	
	var re = /((\s*\S+)*)\s*/;
	return value.replace(re, "$1");
	
}
    function runCLCParents(arg1, arg2, arg3, arg4, arg5, arg6, arg7)
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
    	
    	changesets =execResults.stdout.replace(/\r/g, '').split("Parents of");
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
function getCSID(clcparents)
{
    	return clcparents[0].split("\n")[2].split(":  ")[1];
}

function stParentsOnRoot() {
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
    	//Verify that a limitless parents on @ returns an entry for every
    	//commit.
    	var parentsResults = sg.pending_tree.parents("./");
    	testlib.equal(1, parentsResults.length, "Parents should return an entry.");
    	testlib.equal(parentsResults[0], this.changesets[this.changesets.length - 1], "Parents should have the right parent for a revision to root.");
   	var clcparentsResults = runCLCParents("parents", "./");
   	testlib.equal(1, clcparentsResults.length, "Parents should return an entry.");
    	testlib.equal(getCSID(clcparentsResults), this.changesets[this.changesets.length - 1], "Parents should have the right parent for a revision to root.");
   	var parentsResults = sg.pending_tree.parents();
    	testlib.equal(1, parentsResults.length, "Parents should return an entry.");
    	testlib.equal(parentsResults[0], this.changesets[this.changesets.length - 1], "Parents should have the right parent for a revision to root.");
   	var clcparentsResults = runCLCParents("parents");
   	testlib.equal(1, clcparentsResults.length, "Parents should return an entry.");
    	testlib.equal(getCSID(clcparentsResults), this.changesets[this.changesets.length - 1], "Parents should have the right parent for a revision to root.");

   	parentsResults = sg.pending_tree.parents("test");
    	testlib.equal(1, parentsResults.length, "Parents should return an entry.");

    	testlib.equal(parentsResults[0], this.changesets[1], "Parents should have the right parent for a revision to a subfolder.");
   	clcparentsResults = runCLCParents("parents", "test");
   	testlib.equal(1, clcparentsResults.length, "Parents should return an entry.");
    	testlib.equal(getCSID(clcparentsResults), this.changesets[1], "Parents should have the right parent for a revision to a subfolder.");
    }
    
    
    this.testRev = function() {
    	var parentsResults = sg.pending_tree.parents(["rev=" +this.changesets[this.changesets.length - 3] ])
    	testlib.equal(1, parentsResults.length, "Parents should return an entry.");
    	testlib.equal(parentsResults[0], this.changesets[this.changesets.length - 4], "Parents should have the right parent.");
    	var parentsResults = sg.pending_tree.parents(["rev=" +this.changesets[this.changesets.length - 3] ], "@/test")
    	testlib.equal(1, parentsResults.length, "Parents should return an entry.");
    	testlib.equal(parentsResults[0], this.changesets[1], "Parents should have the right parent for a subfolder revision.");
    	var clcparentsResults = runCLCParents("parents", "--rev=" + this.changesets[this.changesets.length - 3]);
   	testlib.equal(1, clcparentsResults.length, "Parents should return an entry.");
    	testlib.equal(getCSID(clcparentsResults), this.changesets[this.changesets.length - 4], "Parents should have the right parent.");
    	var clcparentsResults = runCLCParents("parents", "@/test", "--rev=" + this.changesets[this.changesets.length - 3]);
   	testlib.equal(1, clcparentsResults.length, "Parents should return an entry.");
    	testlib.equal(getCSID(clcparentsResults), this.changesets[1], "Parents should have the right parent for a subfolder revision.");
    }       

    this.testTag = function() {
    	var parentsResults = sg.pending_tree.parents(["tag=changeset_3"] );
    	testlib.equal(1, parentsResults.length, "Parents should return an entry.");
    	testlib.equal(parentsResults[0], this.changesets[this.changesets.length - 4], "Parents should have the right parent.");
    	var parentsResults = sg.pending_tree.parents(["tag=changeset_3"], "@/test");
    	testlib.equal(1, parentsResults.length, "Parents should return an entry.");
    	testlib.equal(parentsResults[0], this.changesets[1], "Parents should have the right parent for a subfolder revision.");
    	var clcparentsResults = runCLCParents("parents", "--tag=changeset_3");
   	testlib.equal(1, clcparentsResults.length, "Parents should return an entry.");
    	testlib.equal(getCSID(clcparentsResults), this.changesets[this.changesets.length - 4], "Parents should have the right parent.");
    	var clcparentsResults = runCLCParents("parents", "@/test", "--tag=changeset_3");
   	testlib.equal(1, clcparentsResults.length, "Parents should return an entry.");
    	testlib.equal(getCSID(clcparentsResults), this.changesets[1], "Parents should have the right parent for a subfolder revision.");
    } 
    this.testWalkBackParents = function() {
	currentParent = sg.pending_tree.parents()[0];
	while (true)
	{
		parents = sg.pending_tree.parents(["rev=" + currentParent]);
		if (parents == null || parents.length != 1)
			break;
		currentParent = parents[0];
	}
	historyResults = sg.pending_tree.history(["--tag=changeset_2"]);
	rootID = historyResults[historyResults.length - 1].changeset_id;
	testlib.equal(currentParent, rootID, "Should have walked back to the root dag node");
    }
    this.tearDown = function() {
    }
}
