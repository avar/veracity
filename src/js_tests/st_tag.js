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
    	sleepStupidly(1);
    }

function stTagMain() {
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
    
    
    this.testTagMain = function() {
	//Make sure that there are no tags yet
   	var output = testlib.execVV_lines("tags");
	testlib.equal(0, output.length,  "haven't applied any tags yet");
	//Apply one
   	var output = testlib.execVV_lines("tag", "first Tag");
	//Make sure that it's there
   	var output = testlib.execVV_lines("tags");
	testlib.equal(1, output.length,  "applied one tag");
	testlib.stringContains(output[0], "first Tag",  "applied one tag");
	testlib.stringContains(output[0], this.changesets[this.changesets.length - 1],  "applied one tag");
	//apply a new tag to an old revision
   	var output = testlib.execVV_lines("tag", "--rev=" + this.changesets[2], "second Tag");
	//Verify that both tags to applied, and to the correct revisions
   	var output = testlib.execVV_lines("tags");
	testlib.equal(2, output.length,  "applied one tag");
	testlib.stringContains(output[0], "first Tag",  "applied one tag");
	testlib.stringContains(output[0], this.changesets[this.changesets.length - 1],  "applied one tag");
	testlib.stringContains(output[1], "second Tag",  "applied two tags");
	testlib.stringContains(output[1], this.changesets[2],  "applied two tags");
   	//attempt to apply a tag to a different changeset, verify that it fails.
	var execResultObject = testlib.execVV_expectFailure("tag", "--rev=" + this.changesets[2], "first Tag");
   	//manually remove a tag, and apply it to a different changeset
   	var output = testlib.execVV_lines("tag", "--remove", "first Tag");
	var output = testlib.execVV("tag", "--rev=" + this.changesets[2], "first Tag");
   	var output = testlib.execVV_lines("tags");
	testlib.equal(2, output.length,  "applied one tag");
	testlib.stringContains(output[0], "first Tag",  "applied one tag");
	testlib.stringContains(output[0], this.changesets[2],  "applied one tag");
	testlib.stringContains(output[1], "second Tag",  "applied two tags");
	testlib.stringContains(output[1], this.changesets[2],  "applied two tags");
	//Remove all tags, so as not to interfere with other tests
   	var output = testlib.execVV_lines("tag", "--remove", "first Tag", "second Tag");
   	var output = testlib.execVV_lines("tags");
	testlib.equal(0, output.length,  "removed all tags");
	}
   this.testTagForce = function() {
	//Make sure that there are no tags yet
   	testlib.equal(0, testlib.execVV_lines("tags").length,  "haven't applied any tags yet");
   	var output = testlib.execVV_lines("tag", "replaceMe");
   	var output = testlib.execVV_lines("tags");
	testlib.equal(1, output.length,  "applied one tag");
	//Try to reapply the same tag to the current changeset, assert that the CLC doesn't complain
	var outpu = testlib.execVV_lines("tag", "replaceMe");
	//Try to remove a tag that doesn't exist.  Not a failure.
	var outpu = testlib.execVV_lines("tag", "--remove", "not there");
	//Use --force to reapply a tag
   	var output = testlib.execVV_lines("tag", "--rev=" + this.changesets[3], "--force", "replaceMe");
   	var output = testlib.execVV_lines("tags");
	testlib.equal(1, output.length,  "applied one tag");
	testlib.stringContains(output[0], "replaceMe",  "applied one tag");
	testlib.stringContains(output[0], this.changesets[3],  "applied one tag");
   	var output = testlib.execVV_lines("tag", "--remove", "replaceMe");
   	testlib.equal(0, testlib.execVV_lines("tags").length,  "removed all tags");
	}
   this.testTagErrors = function() {
	//Make sure that there are no tags yet
   	testlib.equal(0, testlib.execVV_lines("tags").length,  "haven't applied any tags yet");
   	var output = testlib.execVV_expectFailure("tag", "--rev=blah", "should fail");
   	var output = testlib.execVV_expectFailure("tag", "--tag=blah", "should fail");
   	var output = testlib.execVV("tag", "tryToReapply");
	//Try to remove that tag, but specify the wrong csid, this should not fail.
   	var output = testlib.execVV("tag", "--remove", "--rev=" + this.changesets[2], "tryToReapply");
   	var output = testlib.execVV_expectFailure("tag", "--rev=" + this.changesets[1], "tryToReapply");
   	var output = testlib.execVV_expectFailure("tag", "--rev=" + this.changesets[1], "newTag", "tryToReapply", "another");
   	testlib.equal(1, testlib.execVV_lines("tags").length,  "should not have applied the new tag, if one of the tags failed.");
   	var output = testlib.execVV_lines("tag", "--remove", "tryToReapply");
   	testlib.equal(0, testlib.execVV_lines("tags").length,  "removed all tags");
	}

    this.testTagMainJS = function() {
	//Make sure that there are no tags yet
   	var output = sg.pending_tree.tags();
	testlib.equal(0, output.length,  "haven't applied any tags yet");
	//Apply one
   	var output = sg.pending_tree.tag("first Tag");
	//Make sure that it's there
   	var output = sg.pending_tree.tags();
	testlib.equal(1, output.length,  "applied one tag");
	testlib.equal(output[0].tag, "first Tag",  "applied one tag");
	testlib.equal(output[0].csid, this.changesets[this.changesets.length - 1],  "applied one tag");
	//apply a new tag to an old revision
   	var output = sg.pending_tree.tag(["rev=" + this.changesets[2]], "second Tag");
	//Verify that both tags to applied, and to the correct revisions
   	var output = sg.pending_tree.tags();
	testlib.equal(2, output.length,  "applied one tag");
	testlib.equal(output[0].tag, "first Tag",  "applied one tag");
	testlib.equal(output[0].csid, this.changesets[this.changesets.length - 1],  "applied one tag");
	testlib.equal(output[1].tag, "second Tag",  "applied two tags");
	testlib.equal(output[1].csid, this.changesets[2],  "applied two tags");
   	//attempt to apply a tag to a different changeset, verify that it fails.
	var execResultObject = testlib.verifyFailure("st.pending_tree.tag([\"--rev=" + this.changesets[2] + "\"], \"first Tag\");");
   	//manually remove a tag, and apply it to a different changeset
   	var output = sg.pending_tree.tag(["remove"], "first Tag");
	var output = sg.pending_tree.tag(["rev=" + this.changesets[2]], "first Tag");
   	var output = sg.pending_tree.tags();
	testlib.equal(2, output.length,  "applied one tag");
	testlib.equal(output[0].tag, "first Tag",  "applied one tag");
	testlib.equal(output[0].csid, this.changesets[2],  "applied one tag");
	testlib.equal(output[1].tag, "second Tag",  "applied two tags");
	testlib.equal(output[1].csid, this.changesets[2],  "applied two tags");
	//Remove all tags, so as not to interfere with other tests
   	var output = sg.pending_tree.tag(["remove"], "first Tag", "second Tag");
   	var output = sg.pending_tree.tags();
	testlib.equal(0, output.length,  "removed all tags");
	}
   this.testTagForceJS = function() {
	//Make sure that there are no tags yet
   	testlib.equal(0, sg.pending_tree.tags().length,  "haven't applied any tags yet");
   	var output = sg.pending_tree.tag("replaceMe");
   	var output = sg.pending_tree.tags();
	testlib.equal(1, output.length,  "applied one tag");
	//Try to reapply the same tag to the current changeset, assert that the CLC doesn't complain
	var outpu = sg.pending_tree.tag("replaceMe");
	//Try to remove a tag that doesn't exist.  Not a failure.
	var outpu = sg.pending_tree.tag(["remove"], "not there");
	//Use --force to reapply a tag
   	var output = sg.pending_tree.tag(["rev=" + this.changesets[3], "force"], "replaceMe");
   	var output = sg.pending_tree.tags();
	testlib.equal(1, output.length,  "applied one tag");
	testlib.equal(output[0].tag, "replaceMe",  "applied one tag");
	testlib.equal(output[0].csid, this.changesets[3],  "applied one tag");
   	var output = sg.pending_tree.tag(["remove"], "replaceMe");
   	testlib.equal(0, sg.pending_tree.tags().length,  "removed all tags");
	}
   this.testTagErrorsJS = function() {
	//Make sure that there are no tags yet
   	testlib.equal(0, sg.pending_tree.tags().length,  "haven't applied any tags yet");
   	var output = testlib.verifyFailure("sg.pending_tree.tag([\"rev=blah\"], \"should fail\");");
   	var output = testlib.verifyFailure("sg.pending_tree.tag([\"tag=blah\"], \"should fail\");");
   	var output = sg.pending_tree.tag("tryToReapply");
	//Try to remove that tag, but specify the wrong csid, this should not fail.
   	var output = sg.pending_tree.tag(["remove", "rev=" + this.changesets[2]], "tryToReapply");
   	var output = testlib.verifyFailure("sg.pending_tree.tag([\"rev=" + this.changesets[1] + "\"], \"tryToReapply\");");
   	var output = testlib.verifyFailure("sg.pending_tree.tag([\"rev=" + this.changesets[1] + "\"], \"newTag\", \"tryToReapply\", \"another\");");
   	testlib.equal(1, sg.pending_tree.tags().length,  "should not have applied the new tag, if one of the tags failed.");
   	var output = sg.pending_tree.tag(["remove"], "tryToReapply");
   	testlib.equal(0, sg.pending_tree.tags().length,  "removed all tags");
	}
}

