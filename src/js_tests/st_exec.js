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

function stExec() {
    this.setUp = function() {
    }
    
    
    this.testExec = function() {
    	//Verify that a limitless stamp on @ returns an entry for every
    	//commit.
   	testlib.verifyFailure("sg.exec('vvasdfBlah')", "exec with unknown command", "execute command");
   	var execResults = sg.exec(vv, 'b;lkajsdfa');
	testlib.notEqual(execResults.exit_status, 0, "failed call should result in a non-zero exit status, but not throw");
	//This fails differently on Windows than on linux and mac
	//sg.exec_nowait("asdfwer");  //unknown command should not throw in a nowait
	}
    this.testExecNoWait = function() {
	testlib.notOnDisk("asyncOutput");
	testlib.notOnDisk("asyncError");
	sg.exec_nowait(["output=asyncOutput", "error=asyncError"], "echo", "hello");
	testlib.existsOnDisk("asyncOutput");
	testlib.existsOnDisk("asyncError");
	}
}

