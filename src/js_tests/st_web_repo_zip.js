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

// USES SERVER PORT 8087

var baseURL = "http://localhost:8087/repos/";
function stWebRepoZip() {

  
    this.unzip = ((sg.platform() == "WINDOWS") ? "unzip.exe" : "unzip");
    this.csid1 = "";
    this.csid2 = "";

    this.setUp = function() {
        // Start the server
        {
            this.serverPid = sg.exec_nowait(
                ["output=../stWebRepoZipStdout.txt","error=../stWebRepoZipStderr.txt"],
                "vv", "serve", "--port=8087");
            sleep(5000); // Give it some time to start up before we start hitting it with requests.
        }


        this.curl = ((sg.platform() == "WINDOWS") ? "curl.exe" : "curl");

        var folder = createFolderOnDisk("stWebRepoZip");
        var folder2 = createFolderOnDisk("stWebRepoZip2");
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.txt"), 9);
        var file3 = createFileOnDisk(pathCombine(folder2, "file3.txt"), 5);
        var file4 = createFileOnDisk(pathCombine(folder2, "file4.txt"), 10);
        var folder3 = createFolderOnDisk(pathCombine(folder, "folder3"));
        var foo = createFileOnDisk(pathCombine(folder3, "foo.txt"), 10);
        var empty = createFolderOnDisk("empty");
        var empty2 = createFolderOnDisk(pathCombine(folder3, "empty2"));
        
        sg.pending_tree.addremove();
        this.csid1 = sg.pending_tree.commit();
        /*stWebRepoZip
        /file1.txt
        /file2.txt
        stWebRepoZip2
        /file3.txt
        /file4.txt        
        */

        var file5 = createFileOnDisk(pathCombine(folder, "file5.txt"), 15);
        insertInFile(file, 20);
        sg.pending_tree.remove(file3);
        sg.pending_tree.move(file2, folder2);
        sg.pending_tree.rename(file4, "file4_rename");
        sg.pending_tree.add(file5);
        this.csid2 = sg.pending_tree.commit();

        /*stWebRepoZip
            /folder3
                /foo
        /file1.txt
        /file5.txt
        stWebRepoZip2
        /file2.txt
        /file4_rename        
        */
    };

    this.checkStatus = function(expected, headerBlock, msg) {
        var s = headerBlock.replace(/[\r\n]+/g, "\n");
        var lines = s.split("\n");
        var got = "";

        var prefix = "";

        if (!!msg)
            prefix = msg + ": ";

        for (var i in lines) {
            var status = lines[i];

            var matches = status.match(/^HTTP\/[0-9\.]+[ \t]+(.+?)[ \t]*$/i);

            if (matches) {
                got = matches[1];

                if (got == expected)
                    return (true);
            }
        }

        var err = prefix + "expected '" + expected + "', got '" + got + "'";

        return (testlib.testResult(false, err));
    };

    this.testZipNoCSID = function() {

        var url = baseURL + encodeURI(repInfo.repoName) + "/zip/";

        var zipName = "testZipNoCSID.zip";
        var o = sg.exec(this.curl, "-D", "headers.txt", "-o", zipName, url);
        if (!testlib.testResult(o.exit_status == 0)) {
            print("exited with non zero status");
            return;
        }
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) {
            print("response not 200");
            return;
        }

        o = sg.exec(this.unzip, zipName, "-d", "testZipNoCSID");
        print(o.stdout);

        sg.fs.cd(pathCombine(repInfo.workingPath, "testZipNoCSID"));
        /*stWebRepoZip
            /folder3
                /empty
                /foo
        /file1.txt
        /file5.txt
        stWebRepoZip2
        /file2.txt
        /file4_rename   
        empty     
        */

        testlib.existsOnDisk("stWebRepoZip/folder3/foo.txt");
        testlib.existsOnDisk("stWebRepoZip/file1.txt");
        testlib.existsOnDisk("stWebRepoZip/file5.txt");
        testlib.existsOnDisk("stWebRepoZip2/file2.txt");
        testlib.existsOnDisk("stWebRepoZip2/file4_rename");
        testlib.existsOnDisk("empty");
        testlib.existsOnDisk("stWebRepoZip/folder3/empty2");

        sg.fs.cd(repInfo.workingPath);
        deleteFolderOnDisk("testZipNoCSID");

    };

    this.testZipCSID1 = function() {

        sg.fs.cd(repInfo.workingPath);

        var url = baseURL + encodeURI(repInfo.repoName) + "/zip/" + this.csid1;

        var zipName = "testZipCSID1.zip";
        var o = sg.exec(this.curl, "-D", "headers.txt", "-o", zipName, url);
        if (!testlib.testResult(o.exit_status == 0)) {
            print("exited with non zero status");
            return;
        }
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) {
            print("response not 200");
            return;
        }


        o = sg.exec(this.unzip, zipName, "-d", "testZipCSID1");
        print(o.stdout);

        sg.fs.cd("testZipCSID1");
        /*stWebRepoZip
             /folder3
                /empty
                /foo.txt
        /file1.txt
        /file2.txt
        stWebRepoZip2
        /file3.txt
        /file4.txt 
        empty       
        */

        testlib.existsOnDisk("stWebRepoZip/folder3/foo.txt");
        testlib.existsOnDisk("stWebRepoZip/file1.txt");
        testlib.existsOnDisk("stWebRepoZip/file2.txt");
        testlib.existsOnDisk("stWebRepoZip2/file3.txt");
        testlib.existsOnDisk("stWebRepoZip2/file4.txt");
        testlib.existsOnDisk("empty");
        testlib.existsOnDisk("stWebRepoZip/folder3/empty2");

        sg.fs.cd(repInfo.workingPath);
        deleteFolderOnDisk("testZipCSID1");

    };

  
    this.tearDown = function() {
        if (sg.platform() == "WINDOWS")
            sg.exec("taskkill", "/PID", this.serverPid.toString(), "/F");
        else
            sg.exec("kill", "-s", "SIGINT", this.serverPid.toString());
        print("(stWeb.tearDown: Giving 'vv serve' some time to shut down before we print its stdout/stderr output.)")
        sleep(30000);
        print();
        print(sg.file.read("../stWebRepoZipStdout.txt"));
        print();
        print(sg.file.read("../stWebRepoZipStderr.txt"));
    };

}
