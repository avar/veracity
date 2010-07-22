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

//Test CLC params

function stCLC() {


    //vv revert [--test] [--nonrecursive -N] [--include=pattern -I] [--exclude=pattern -X] [--include-from=file] [--exclude-from=file] file-or-folder […]
    this.testRevertFileCLC = function testRevertFileCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        addRemoveAndCommit();
        var tmpFile = createTempFile(file);
        insertInFile(file, 20);
        var tmpFile2 = createTempFile(file);

        o = sg.exec(vv, "revert", file);
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        var backup = file + "~sg00~";

        // testlib.statusEmpty();
        testlib.ok(compareFiles(tmpFile, file), "Files should be identical after the revert");
        testlib.existsOnDisk(backup);
        testlib.ok(compareFiles(tmpFile2, backup), "Backup file should match edited file");
        sg.fs.remove(tmpFile);
        sg.fs.remove(tmpFile2);
      
        testlib.statusEmpty();

    }

    this.testRevertMultipleFilesCLC = function testRevertMultipleFilesCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.txt"), 20);
        var file3 = createFileOnDisk(pathCombine(folder, "file3.txt"), 6);


        addRemoveAndCommit();
        var tmpFile1 = createTempFile(file1);
        insertInFile(file1, 20);
        var tmpFile1_2 = createTempFile(file1);

        var tmpFile2 = createTempFile(file2);
        insertInFile(file2, 20);
        var tmpFile2_2 = createTempFile(file2);

        var tmpFile3 = createTempFile(file3);
        insertInFile(file3, 20);
        var tmpFile3_2 = createTempFile(file3);

        o = sg.exec(vv, "revert", file1, file2, file3);
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        var backup1 = file1 + "~sg00~";
        var backup2 = file2 + "~sg00~";
        var backup3 = file3 + "~sg00~";

        // testlib.statusEmpty();
        testlib.ok(compareFiles(tmpFile1, file1), "Files should be identical after the revert");
        testlib.existsOnDisk(backup1);
        testlib.ok(compareFiles(tmpFile1_2, backup1), "Backup file should match edited file");
        testlib.ok(compareFiles(tmpFile2, file2), "Files should be identical after the revert");
        testlib.existsOnDisk(backup2);
        testlib.ok(compareFiles(tmpFile2_2, backup2), "Backup file should match edited file");
        testlib.ok(compareFiles(tmpFile3, file3), "Files should be identical after the revert");
        testlib.existsOnDisk(backup3);
        testlib.ok(compareFiles(tmpFile3_2, backup3), "Backup file should match edited file");
        sg.fs.remove(tmpFile1);
        sg.fs.remove(tmpFile1_2);
        sg.fs.remove(tmpFile2);
        sg.fs.remove(tmpFile2_2);
        sg.fs.remove(tmpFile3);
        sg.fs.remove(tmpFile3_2);        
        testlib.statusEmpty();

    }
    this.testRevertFolderCLC = function testRevertFolderCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder1"), 2);

        addRemoveAndCommit();

        testlib.inRepo(folder);

        var newName = pathCombine(getParentPath(folder), "rename_" + base);

        sg.exec(vv, "rename", folder, "rename_" + base);
        testlib.existsOnDisk(newName);

        o = sg.exec(vv, "revert", base);
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        testlib.statusEmpty();
        testlib.existsOnDisk(folder);
        testlib.notOnDisk(newName);

    }

    this.testRevertMultipleFoldersCLC = function testRevertMultipleFoldersCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder1"), 2);
        var folder2 = createFolderOnDisk(pathCombine(base + "2", "folder2"), 3);


        addRemoveAndCommit();

        testlib.inRepo(folder);
        testlib.inRepo(folder2);

        var newName = pathCombine(getParentPath(folder), "rename_" + base);

        sg.exec(vv, "rename", folder, "rename_" + base);
        sg.pending_tree.remove(folder2);
        testlib.existsOnDisk(newName);
        testlib.notOnDisk(folder2);

        o = sg.exec(vv, "revert", base, base + "2");
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        testlib.statusEmpty();
        testlib.existsOnDisk(folder);
        testlib.existsOnDisk(folder2);
        testlib.notOnDisk(newName);

    }

    this.testRevertAllCLC = function testRevertAllCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder1"), 2);
        var folder2 = createFolderOnDisk(pathCombine(base + "2", "folder2"), 3);


        addRemoveAndCommit();

        testlib.inRepo(folder);
        testlib.inRepo(folder2);

        var newName = pathCombine(getParentPath(folder), "rename_" + base);

        sg.exec(vv, "rename", folder, "rename_" + base);
        sg.pending_tree.remove(folder2);
        testlib.existsOnDisk(newName);
        testlib.notOnDisk(folder2);

        o = sg.exec(vv, "revert", "--all");
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        testlib.statusEmpty();
        testlib.existsOnDisk(folder);
        testlib.existsOnDisk(folder2);
        testlib.notOnDisk(newName);

    }

    this.testRevertErrorCLC = function testRevertErrorCLC() {

        o = sg.exec(vv, "revert");
        testlib.ok(o.stderr.search("Usage") >= 0, "should be error");
    }

    this.testRevertNonrecursiveCLC = function testRevertNonrecursiveCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder"));
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.h"), 20);
        var file3 = createFileOnDisk(pathCombine(folder, "file3.txt"), 6);


        addRemoveAndCommit();
        var tmpFile1 = createTempFile(file1);
        insertInFile(file1, 20);
        var tmpFile1_2 = createTempFile(file1);

        var tmpFile2 = createTempFile(file2);
        insertInFile(file2, 20);
        var tmpFile2_2 = createTempFile(file2);

        var tmpFile3 = createTempFile(file3);
        insertInFile(file3, 20);
        var tmpFile3_2 = createTempFile(file3);

        var newName = pathCombine(getParentPath(folder), "rename_" + base);

        sg.pending_tree.rename(folder, "rename_" + base);
        testlib.existsOnDisk(newName);

        o = sg.exec(vv, "revert", newName, "--nonrecursive");
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        testlib.existsOnDisk(folder);
     
        testlib.ok(compareFiles(tmpFile1_2, file1), "file should be the same");

        testlib.ok(compareFiles(tmpFile2_2, file2), "file should be the same");

        testlib.ok(compareFiles(tmpFile3_2, file3), "file should be the samme");
        sg.fs.remove(tmpFile1);
        sg.fs.remove(tmpFile1_2);
        sg.fs.remove(tmpFile2);
        sg.fs.remove(tmpFile2_2);
        sg.fs.remove(tmpFile3);
        sg.fs.remove(tmpFile3_2);        
        addRemoveAndCommit();
        testlib.statusEmpty();

    }

    /* ***INCLUDE/EXCLUDE***
	  this.testRevertIncludesCLC = function testRevertIncludesCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder"));
        var extra = createFolderOnDisk(pathCombine(base, "folder2"));
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.h"), 20);
        var file3 = createFileOnDisk(pathCombine(folder, "file3.txt"), 6);


        addRemoveAndCommit();
        var tmpFile1 = createTempFile(file1);
        insertInFile(file1, 20);
        var tmpFile1_2 = createTempFile(file1);

        var tmpFile2 = createTempFile(file2);
        insertInFile(file2, 20);
        var tmpFile2_2 = createTempFile(file2);

        var tmpFile3 = createTempFile(file3);
        insertInFile(file3, 20);
        var tmpFile3_2 = createTempFile(file3);
        sg.pending_tree.remove(extra);
        testlib.notOnDisk(extra);

        o = sg.exec(vv, "revert", folder, "--include=*.h", "-I", "*.txt");
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        testlib.notOnDisk(extra);

        var backup1 = file1 + "~sg00~";
        var backup2 = file2 + "~sg00~";
        var backup3 = file3 + "~sg00~";

        // testlib.statusEmpty();
        testlib.ok(compareFiles(tmpFile1, file1), "Files should be identical after the revert");
        testlib.existsOnDisk(backup1);
        testlib.ok(compareFiles(tmpFile1_2, backup1), "Backup file should match edited file");
        testlib.ok(compareFiles(tmpFile2, file2), "Files should be identical after the revert");
        testlib.existsOnDisk(backup2);
        testlib.ok(compareFiles(tmpFile2_2, backup2), "Backup file should match edited file");
        testlib.ok(compareFiles(tmpFile3, file3), "Files should be identical after the revert");
        testlib.existsOnDisk(backup3);
        testlib.ok(compareFiles(tmpFile3_2, backup3), "Backup file should match edited file");
        sg.fs.remove(tmpFile1);
        sg.fs.remove(tmpFile1_2);
        sg.fs.remove(tmpFile2);
        sg.fs.remove(tmpFile2_2);
        sg.fs.remove(tmpFile3);
        sg.fs.remove(tmpFile3_2);
     
        addRemoveAndCommit();
        testlib.statusEmpty();

    }
    
    this.testRevertExcludesCLC = function testRevertExcludesCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder"));
        var extra = createFolderOnDisk(pathCombine(base, "folder2"));
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.h"), 20);
        var file3 = createFileOnDisk(pathCombine(folder, "file3.c"), 6);

            addRemoveAndCommit();
        var tmpFile1 = createTempFile(file1);
        insertInFile(file1, 20);
        var tmpFile1_2 = createTempFile(file1);

            var tmpFile2 = createTempFile(file2);
        insertInFile(file2, 20);
        var tmpFile2_2 = createTempFile(file2);

            var tmpFile3 = createTempFile(file3);
        insertInFile(file3, 20);
        var tmpFile3_2 = createTempFile(file3);
        sg.pending_tree.remove(extra);
        testlib.notOnDisk(extra);

            o = sg.exec(vv, "revert", folder, "--exclude=*.h", "-X", "*.c");
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        testlib.notOnDisk(extra);

        var backup1 = file1 + "~sg00~";

        // testlib.statusEmpty();
        testlib.ok(compareFiles(tmpFile1, file1), "Files should be identical after the revert");
        testlib.existsOnDisk(backup1);
        testlib.ok(compareFiles(tmpFile2_2, file2), "file should match edited file");
        testlib.ok(compareFiles(tmpFile3_2, file3), "file should match edited file");
        sg.fs.remove(tmpFile1);
        sg.fs.remove(tmpFile1_2);
        sg.fs.remove(tmpFile2);
        sg.fs.remove(tmpFile2_2);
        sg.fs.remove(tmpFile3);
        sg.fs.remove(tmpFile3_2);
    
        addRemoveAndCommit();
        testlib.statusEmpty();

    }

    this.testRevertIncludesAndExcludesCLC = function testRevertInlcudesAndExcludesCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder"));
        var extra = createFolderOnDisk(pathCombine(base, "folder2"));
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.txt"), 20);
        var file3 = createFileOnDisk(pathCombine(folder, "file3.txt"), 6);
        var file3 = createFileOnDisk(pathCombine(folder, "file4.c"), 6);


        addRemoveAndCommit();
        var tmpFile1 = createTempFile(file1);
        insertInFile(file1, 20);
        var tmpFile1_2 = createTempFile(file1);

            var tmpFile2 = createTempFile(file2);
        insertInFile(file2, 20);
        var tmpFile2_2 = createTempFile(file2);

            var tmpFile3 = createTempFile(file3);
        insertInFile(file3, 20);
        var tmpFile3_2 = createTempFile(file3);
        sg.pending_tree.remove(extra);
        testlib.notOnDisk(extra);

        o = sg.exec(vv, "revert", folder, "--exclude=**/ /* ***INCLUDE/EXCLUDE*** file3*", "--include=*.txt");
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        testlib.notOnDisk(extra);

        var backup1 = file1 + "~sg00~";
        var backup2 = file2 + "~sg00~";

        // testlib.statusEmpty();
        testlib.ok(compareFiles(tmpFile1, file1), "Files should be identical after the revert");
        testlib.existsOnDisk(backup1);
        testlib.ok(compareFiles(tmpFile2, file2), "Backup file should match edited file");
        testlib.existsOnDisk(backup2);
        testlib.ok(compareFiles(tmpFile3_2, file3), "file should match edited file");
        sg.fs.remove(tmpFile1);
        sg.fs.remove(tmpFile1_2);
        sg.fs.remove(tmpFile2);
        sg.fs.remove(tmpFile2_2);
        sg.fs.remove(tmpFile3);
        sg.fs.remove(tmpFile3_2);
   
        addRemoveAndCommit();
        testlib.statusEmpty();


    }
    


    //this works manually from the command line but not from the test???
    this.testRevertSomeIncludeFromCLC = function testRevertAllIncludeFromCLC() {

         var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder"));
        var extra = createFolderOnDisk(pathCombine(base, "folder2"));
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.h"), 20);
        var file3 = createFileOnDisk(pathCombine(folder, "file3.txt"), 6);
        addRemoveAndCommit();
        var patterns = pathCombine(base, "patterns.txt");
        sg.file.write(patterns, "*.h\r\n*.c\r\n");
        var tmpFile1 = createTempFile(file1);
        insertInFile(file1, 20);
        var tmpFile1_2 = createTempFile(file1);

        var tmpFile2 = createTempFile(file2);
        insertInFile(file2, 20);
        var tmpFile2_2 = createTempFile(file2);

        var tmpFile3 = createTempFile(file3);
        insertInFile(file3, 20);
        var tmpFile3_2 = createTempFile(file3);
        sg.pending_tree.remove(extra);
        testlib.notOnDisk(extra);

        print(pathCombine(repInfo.workingPath, patterns));
        o = sg.exec(vv, "revert", folder, "--include-from=" + pathCombine(repInfo.workingPath, patterns));
        print(o.stderr);
        print(o.stdout);
        testlib.notOnDisk(extra);

        var backup1 = file1 + "~sg00~";
        var backup3 = file3 + "~sg00~";

        // testlib.statusEmpty();
        testlib.ok(compareFiles(tmpFile1, file1), "Files should be identical after the revert");
        testlib.existsOnDisk(backup1);
        testlib.ok(compareFiles(tmpFile1_2, backup1), "Backup file should match edited file");
        testlib.ok(compareFiles(tmpFile2_2, file2), "Backup file should match edited file");
        testlib.ok(compareFiles(tmpFile3, file3), "Files should be identical after the revert");
        testlib.existsOnDisk(backup3);
        testlib.ok(compareFiles(tmpFile3_2, backup3), "Backup file should match edited file");
        sg.fs.remove(tmpFile1);
        sg.fs.remove(tmpFile1_2);
        sg.fs.remove(tmpFile2);
        sg.fs.remove(tmpFile2_2);
        sg.fs.remove(tmpFile3);
        sg.fs.remove(tmpFile3_2);
        

        sg.fs.remove(backup3);
        addRemoveAndCommit();
        testlib.statusEmpty();

    }

    //this works from the command line but not from the tests ???
    this.testRevertAllExcludeFromCLC = function testRevertSomeExcludeFromCLC() {


        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder"));
        var extra = createFolderOnDisk(pathCombine(base, "folder2"));
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.h"), 20);
        var file3 = createFileOnDisk(pathCombine(folder, "file3.c"), 6);


        addRemoveAndCommit();
        sg.file.write(pathCombine(base, "patterns.txt"), "*.h\r\n*.txt\r\n");
        var tmpFile1 = createTempFile(file1);
        insertInFile(file1, 20);
        var tmpFile1_2 = createTempFile(file1);

        var tmpFile2 = createTempFile(file2);
        insertInFile(file2, 20);
        var tmpFile2_2 = createTempFile(file2);

        var tmpFile3 = createTempFile(file3);
        insertInFile(file3, 20);
        var tmpFile3_2 = createTempFile(file3);
        sg.pending_tree.remove(extra);
        testlib.notOnDisk(extra);

        sg.exec(vv, "revert", "--exclude-from=" +  pathCombine(base, "patterns.txt"));


        testlib.notOnDisk(extra);

        var backup1 = file1 + "~sg00~";

        // testlib.statusEmpty();
        testlib.ok(compareFiles(tmpFile1, file1), "Files should be identical after the revert");
        testlib.existsOnDisk(backup1);
        testlib.ok(compareFiles(tmpFile2_2, file2), "file should match edited file");
        testlib.ok(compareFiles(tmpFile3_2, file3), "file should match edited file");
        sg.fs.remove(tmpFile1);
        sg.fs.remove(tmpFile1_2);
        sg.fs.remove(tmpFile2);
        sg.fs.remove(tmpFile2_2);
        sg.fs.remove(tmpFile3);
        sg.fs.remove(tmpFile3_2);
      
        addRemoveAndCommit();
        testlib.statusEmpty();
    }*/

    //vv commit [--user=userid [--password=password]] [--message='changeset description'] [--nonrecursive] [--include=pattern] [--exclude=pattern] [--stamp=stamptext […]] [--test] path […]
    this.testCommitModifiedFileCLC = function testCommitModifiedFileCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        addRemoveAndCommit();
        var tmpFile = createTempFile(file);
        insertInFile(file, 20);
        var tmpFile2 = createTempFile(file);

        o = sg.exec(vv, "commit", "--message=fakemessage", file);
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        testlib.ok(compareFiles(tmpFile2, file), "Files should be identical after the commit");
        sg.fs.remove(tmpFile);
        sg.fs.remove(tmpFile2);
        testlib.statusEmpty();

    }

    this.testCommitModifiedFileWithUserOptionCLC = function testCommitModifiedFileWithUserOptionCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        addRemoveAndCommit();
        var tmpFile = createTempFile(file);
        insertInFile(file, 20);
        var tmpFile2 = createTempFile(file);

        o = sg.exec(vv, "commit", "--message=fakemessage", "--user=myjstestuser", file);
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        s = o.stdout.trim();

        testlib.testResult(s.search("who:  myjstestuser") >= 0, "Default user should have been overriden by myjstestuser");

        testlib.ok(compareFiles(tmpFile2, file), "Files should be identical after the commit");
        sg.fs.remove(tmpFile);
        sg.fs.remove(tmpFile2);
        testlib.statusEmpty();

    }

    this.testCommitModifiedFileWithStampOptionCLC = function testCommitModifiedFileWithStampOptionCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        addRemoveAndCommit();
        var tmpFile = createTempFile(file);
        insertInFile(file, 20);
        var tmpFile2 = createTempFile(file);

        o = sg.exec(vv, "commit", "--message=fakemessage", "--stamp=stampone", "--stamp=stamptwo", file);
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        s = o.stdout.trim();

        testlib.testResult(s.search("stamp:  stampone") >= 0, "Log should show a stamp:  stampone");

        testlib.testResult(s.search("stamp:  stamptwo") >= 0, "Log should show a stamp:  stamptwo");

        testlib.ok(compareFiles(tmpFile2, file), "Files should be identical after the commit");
        sg.fs.remove(tmpFile);
        sg.fs.remove(tmpFile2);
        testlib.statusEmpty();

    }

    // works from the cmdline but not here??
    this.testCommitIncludesCLC = function testCommitIncludesCLC() {
        /*var file1 = createFileOnDisk("file1.txt", 39);
        var file2 = createFileOnDisk("file2.c", 24);
        var file3 = createFileOnDisk("file3.h", 6);

        addRemoveNoCommit();

        confirmTreeObjectStatus("file1.txt", "Added");
        confirmTreeObjectStatus("file2.c", "Added");
        confirmTreeObjectStatus("file3.h", "Added");

        dumpTree("dump");

        o = sg.exec(vv, "commit", "--message=noprompt", "--include=*.h", "-I", "*.c");
        testlib.ok(o.exit_status == 0, "exit status should be 0", o.stderr, o.stdout);

        testlib.inRepo("file2.c");
        testlib.inRepo("file3.h");
        testlib.notInRepo("file1.txt");
        confirmTreeObjectStatus("file1.txt", "Added");*/
    }

	/* ***INCLUDE/EXCLUDE***
    this.testCommitExcludesCLC = function testCommitExcludesCLC() {
        var file1 = createFileOnDisk("file1.txt", 39);
        var file2 = createFileOnDisk("file2.c", 24);
        var file3 = createFileOnDisk("file3.h", 6);

        addRemoveNoCommit();

        o = sg.exec(vv, "commit", "--message=noprompt", "--exclude=*.h", "-X", "*.c");
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        testlib.notInRepo("file2.c");
        testlib.notInRepo("file3.h");
        testlib.inRepo("file1.txt");
        confirmTreeObjectStatus("file2.c", "Added");
        confirmTreeObjectStatus("file3.h", "Added");
    }
    */

    this.testCommitModifiedFileWithMessageOptionSimpleCLC = function testCommitModifiedFileWithMessageOptionSimpleCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        addRemoveAndCommit();
        var tmpFile = createTempFile(file);
        insertInFile(file, 20);
        var tmpFile2 = createTempFile(file);

        o = sg.exec(vv, "commit", "--message=simple message", file);
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        s = o.stdout.trim();

        testlib.testResult(s.search("comment:  simple message") >= 0, "Commit should have the comment: simple message");

        testlib.ok(compareFiles(tmpFile2, file), "Files should be identical after the commit");
        sg.fs.remove(tmpFile);
        sg.fs.remove(tmpFile2);
        testlib.statusEmpty();

    }

    //    this.testCommitModifiedFileWithMessageOptionUnicodeCLC = function testCommitModifiedFileWithMessageOptionUnicodeCLC() {

    //        var base = getTestName(arguments.callee.toString());
    //        var folder = createFolderOnDisk(base);
    //        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
    //        addRemoveAndCommit();
    //        var tmpFile = createTempFile(file);
    //        insertInFile(file, 20);
    //        var tmpFile2 = createTempFile(file);

    //        o = sg.exec(vv, "commit", "--message=abcde\u00A9", file);
    //        testlib.ok(o.exit_status == 0, "exit status should be 0");

    //        s = o.stdout.trim();

    //        testlib.testResult(s.search("Comment : abcde\u00A9") >= 0, "Commit should have the comment: abcde\u00A9", "abcde\u00A9", s);

    //        testlib.ok(compareFiles(tmpFile2, file), "Files should be identical after the commit");
    //        sg.fs.remove(tmpFile);
    //        sg.fs.remove(tmpFile2);
    //        testlib.statusEmpty();

    //    }

    //    this.testCommitModifiedFileWithMessageFromFileCLC = function testCommitModifiedFileWithMessageFromFileCLC() {

    //        var base = getTestName(arguments.callee.toString());
    //        var folder = createFolderOnDisk(base);
    //        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
    //        addRemoveAndCommit();
    //        var tmpFile = createTempFile(file);
    //        insertInFile(file, 20);
    //        var tmpFile2 = createTempFile(file);

    //        o = sg.exec_pipeinput(vv, "commit", "C:/Documents and Settings/Shannon/Local Settings/Temp/testinput.txt");  //, "< pipeFile");
    //        testlib.ok(o.exit_status == 0, "exit status should be 0");

    //        s = o.stdout.trim();

    //        testlib.testResult(s.search("Comment : abcde\u00A9abcde") >= 0, "Commit should have the comment: abcde\u00A9abcde", o.stderr, s);

    //        testlib.ok(compareFiles(tmpFile2, file), "Files should be identical after the commit");
    //        sg.fs.remove(tmpFile);
    //        sg.fs.remove(tmpFile2);
    //        testlib.statusEmpty();
    //    }

    //vv rename [--force] file-or-folder new-name
    this.testRenameFileNewNameExistsCLC = function testRenameFileNewNameExistsCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        addRemoveAndCommit();
        var fileNewName = createFileOnDisk(pathCombine(folder, "file2.txt"), 44);

        var tmpFile = createTempFile(file);
        var tmpFile2 = createTempFile(fileNewName);

        o = sg.exec(vv, "rename", file, "file2.txt");
        testlib.ok(o.exit_status != 0, "exit status should not be 0");

        testlib.ok(compareFiles(tmpFile, file), "Files should have been left as is");
        testlib.ok(compareFiles(tmpFile2, fileNewName), "Files should have been left as is");
        sg.fs.remove(tmpFile);
        sg.fs.remove(tmpFile2);
        sg.fs.remove(fileNewName);
        testlib.statusEmpty(); 

    }

    this.testRenameFileNewNameExistsForceCLC = function testRenameFileNewNameExistsForceCLC() {
        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        addRemoveAndCommit();
        var fileNewName = createFileOnDisk(pathCombine(folder, "file2.txt"), 44);

        var tmpFile = createTempFile(file);
        var tmpFile2 = createTempFile(fileNewName);

        o = sg.exec(vv, "rename", "--force", file, "file2.txt");
        testlib.ok(o.exit_status == 0, "exit status should be 0", o.stdout, o.stderr);

        testlib.notOnDisk(file);
        testlib.ok(compareFiles(tmpFile, fileNewName), "File should have been renamed");
        confirmTreeObjectStatus(fileNewName, "Renamed");
        sg.fs.remove(tmpFile);
        sg.fs.remove(tmpFile2);
        addRemoveAndCommit();
        testlib.statusEmpty();
    }

    this.testRenameAliasFolderNewNameExistsCLC = function testRenameAliasFolderNewNameExistsCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        createFolderWithSubfoldersAndFiles(pathCombine(folder, "folder1"), 2, 2);
        var folderToRename = pathCombine(folder, "folder1");
        addRemoveAndCommit();
        var folderNewName = createFolderOnDisk(pathCombine(folder, "folder2"));


        o = sg.exec(vv, "ren", folderToRename, "folder2");
        testlib.ok(o.exit_status != 0, "exit status should not be 0");

        testlib.log(o.stderr);
        testlib.log(o.stdout);

        testlib.existsOnDisk(folderToRename);
        testlib.existsOnDisk(folderNewName);
        testlib.inRepo(folderToRename);
        testlib.notInRepo(folderNewName);
        addRemoveAndCommit();
        testlib.statusEmpty(); 

    }

    this.testRenameFolderNewNameExistsForceShortCLC = function testRenameFolderNewNameExistsForceShortCLC() {
        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        createFolderWithSubfoldersAndFiles(pathCombine(folder, "folder1"), 2, 2);
        var folderToRename = pathCombine(folder, "folder1");
        addRemoveAndCommit();
        var folderNewName = createFolderOnDisk(pathCombine(folder, "folder2"));


        o = sg.exec(vv, "ren", "-F", folderToRename, "folder2");
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        testlib.notOnDisk(folderToRename);
        testlib.existsOnDisk(folderNewName);
        confirmTreeObjectStatus(folderNewName, "Renamed");
        addRemoveAndCommit();
        testlib.inRepo(folderNewName);
        testlib.statusEmpty();
    }

    this.testRenameFileNewNameExistsAsFolderCLC = function testRenameFileNewNameExistsAsFolderCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        addRemoveAndCommit();
        var folderWithNewName = createFolderOnDisk(pathCombine(folder, "folder1"));

        var tmpFile = createTempFile(file);

        o = sg.exec(vv, "rename", file, "folder1");
        testlib.ok(o.exit_status != 0, "exit status should not be 0");

        testlib.ok(compareFiles(tmpFile, file), "Files should have been left as is");
        testlib.existsOnDisk(folderWithNewName);
        sg.fs.remove(tmpFile);
        addRemoveAndCommit();
        testlib.statusEmpty();

    }

    this.testStatusAllCLC = function testStatusAllCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder"));
        var extra = createFolderOnDisk(pathCombine(base, "folder2"));
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.txt"), 20);
        var file3 = createFileOnDisk(pathCombine(extra, "file3.txt"), 6);

        o = sg.exec(vv, "status");
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        testlib.ok(o.stdout.search("folder/") >= 0);
        testlib.ok(o.stdout.search("folder2/") >= 0);
        testlib.ok(o.stdout.search("file1.txt") >= 0);
        testlib.ok(o.stdout.search("file2.txt") >= 0);
        testlib.ok(o.stdout.search("file3.txt") >= 0);

    }

    this.testStatusDirCLC = function testStatusDirCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder"));
        var extra = createFolderOnDisk(pathCombine(base, "folder2"));
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.txt"), 20);
        var file3 = createFileOnDisk(pathCombine(extra, "file3.txt"), 6);
        print(folder);

        o = sg.exec(vv, "status", folder);
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        print(o.stdout);
        testlib.ok(o.stdout.search("folder/") >= 0);
        testlib.ok(o.stdout.search("folder2/") < 0);
        testlib.ok(o.stdout.search("file1.txt") >= 0);
        testlib.ok(o.stdout.search("file2.txt") >= 0);
        testlib.ok(o.stdout.search("file3.txt") < 0);

    }

    this.testStatusMultipleDirsCLC = function testStatusMultipleDirsCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder"));
        var folder2 = createFolderOnDisk(pathCombine(base, "folder3"));
        var extra = createFolderOnDisk(pathCombine(base, "folder2"));
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var fileex = createFileOnDisk(pathCombine(base, "fileex.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.txt"), 20);
        var file3 = createFileOnDisk(pathCombine(extra, "file3.txt"), 6);
        print(folder);

        o = sg.exec(vv, "status", folder, extra);
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        print(o.stdout);
        testlib.ok(o.stdout.search("folder/") >= 0);
        testlib.ok(o.stdout.search("folder2/") >= 0);
        testlib.ok(o.stdout.search("folder3/") < 0);
        testlib.ok(o.stdout.search("fileex.txt") < 0);
        testlib.ok(o.stdout.search("file1.txt") >= 0);
        testlib.ok(o.stdout.search("file2.txt") >= 0);
        testlib.ok(o.stdout.search("file3.txt") >= 0);
       
        

    }
    this.testStatusFilesCLC = function testStatusFilesCLC() {
        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder"));
        var extra = createFolderOnDisk(pathCombine(base, "folder2"));
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.txt"), 20);
        var file3 = createFileOnDisk(pathCombine(extra, "file3.txt"), 6);
     
        o = sg.exec(vv, "status", file1, file2);
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        print(o.stdout);
       
        testlib.ok(o.stdout.search("folder2/") < 0);
        testlib.ok(o.stdout.search("file1.txt") >= 0);
        testlib.ok(o.stdout.search("file2.txt") >= 0);
        testlib.ok(o.stdout.search("file3.txt") < 0);
    }
    /* ***INCLUDE/EXCLUDE***
    this.testStatusIncludeDirsCLC = function testStatusIncludeDirsCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder"));
        var folder2 = createFolderOnDisk(pathCombine(base, "folder3"));
        var extra = createFolderOnDisk(pathCombine(base, "folder2"));
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var fileex = createFileOnDisk(pathCombine(base, "fileex.h"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.h"), 20);
        var file3 = createFileOnDisk(pathCombine(extra, "file3.h"), 6);
        print(folder);

        o = sg.exec(vv, "status", folder, "--include=*.h");
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        print(o.stdout);
        testlib.ok(o.stdout.search("folder/") >= 0);
        testlib.ok(o.stdout.search("folder2/") < 0);
        testlib.ok(o.stdout.search("folder3/") < 0);
        testlib.ok(o.stdout.search("fileex.h") < 0);
        testlib.ok(o.stdout.search("file1.txt") < 0);
        testlib.ok(o.stdout.search("file2.h") >= 0);
        testlib.ok(o.stdout.search("file3.txt") < 0);
    }
    this.testStatusExcludeDirsCLC = function testStatusExcludeDirsCLC() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(pathCombine(base, "folder"));
        var folder2 = createFolderOnDisk(pathCombine(base, "folder3"));
        var extra = createFolderOnDisk(pathCombine(base, "folder2"));
        var file1 = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var fileex = createFileOnDisk(pathCombine(base, "fileex.h"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.h"), 20);
        var file3 = createFileOnDisk(pathCombine(extra, "file3.h"), 6);
        print(folder);

        o = sg.exec(vv, "status", folder, extra, folder2, "--exclude=*.h");
        testlib.ok(o.exit_status == 0, "exit status should be 0");

        print(o.stdout);
        testlib.ok(o.stdout.search("folder/") >= 0);
        testlib.ok(o.stdout.search("folder2/") >= 0);
        testlib.ok(o.stdout.search("folder3/") >= 0);
        testlib.ok(o.stdout.search("fileex.h") < 0);
        testlib.ok(o.stdout.search("file1.txt") >= 0);
        testlib.ok(o.stdout.search("file2.h") < 0);
        testlib.ok(o.stdout.search("file3.h") < 0);
    }
    */
    this.testStatusNonrecursiveCLC = function testStatusNonrecursiveCLC() {

	// TODO 2010/06/21 All of the tests in this file create the same
	// TODO            set of file and folder names, so the o.stdout.search()
	// TODO            may find files/folders created by others tests rather
	// TODO            than the ones created by this test.  For this reason
	// TODO            I have prefixed all of the ones here with xx_.  We
	// TODO            should probably look at the other tests and do something
	// TODO            similar.

        var base    = getTestName(arguments.callee.toString());
        var folder  = createFolderOnDisk(pathCombine(base, "xx_folder"));
        var folder2 = createFolderOnDisk(pathCombine(base, "xx_folder3"));
        var extra   = createFolderOnDisk(pathCombine(base, "xx_folder2"));
        var file1   = createFileOnDisk(pathCombine(folder, "xx_file1.txt"), 39);
        var fileex  = createFileOnDisk(pathCombine(base,   "xx_fileex.h"), 39);
        var file2   = createFileOnDisk(pathCombine(folder, "xx_file2.h"), 20);
        var file3   = createFileOnDisk(pathCombine(extra,  "xx_file3.h"), 6);

	//////////////////////////////////////////////////////////////////
	// This isn't really a test.  Rather, just a sanity check that
	// the WD is setup as expected.

        o = sg.exec(vv, "status", "--ignore-ignores", base);
        testlib.ok(o.exit_status == 0, "exit status should be 0");
        print("vv status --ignore-ignores " + base + ":\n" + o.stdout);

        testlib.ok(o.stdout.search("xx_folder/")   >= 0);
        testlib.ok(o.stdout.search("xx_folder2/")  >= 0);
        testlib.ok(o.stdout.search("xx_folder3/")  >= 0);
        testlib.ok(o.stdout.search("xx_fileex.h")  >= 0);
        testlib.ok(o.stdout.search("xx_file1.txt") >= 0);
        testlib.ok(o.stdout.search("xx_file2.h")   >= 0);
        testlib.ok(o.stdout.search("xx_file3.h")   >= 0);

	//////////////////////////////////////////////////////////////////
	// -N on a named directory.

        o = sg.exec(vv, "status", base, "-N");
        testlib.ok(o.exit_status == 0, "exit status should be 0");
        print("vv status -N " + base + ":\n" + o.stdout);

	// TODO 2010/06/21 Currently "vv status -N dir" returns only
	// TODO            status for the directory itself -- NOT THE
	// TODO            CONTENTS of the directory.  We are currently
	// TODO            discussing changing this to have depth 1
	// TODO            and/or changing the option to --depth.
	// TODO            So, for now, these search should not find
	// TODO            anything.
	//
	// TODO 2010/06/21 All of these should be in the FOUND section.
	// TODO            Do we need to do more than just verify that
	// TODO            the file/folder name is present in the output?

        testlib.ok(o.stdout.search("xx_folder/")   < 0);
        testlib.ok(o.stdout.search("xx_folder2/")  < 0);
        testlib.ok(o.stdout.search("xx_folder3/")  < 0);
        testlib.ok(o.stdout.search("xx_fileex.h")  < 0);
        testlib.ok(o.stdout.search("xx_file1.txt") < 0);
        testlib.ok(o.stdout.search("xx_file2.h")   < 0);
        testlib.ok(o.stdout.search("xx_file3.h")   < 0);

	//////////////////////////////////////////////////////////////////
	// -N with NO NAMED directory.
	//
	// TODO 2010/06/21 See sprawl-882.  What should -N without a
	// TODO            NAMED directory do?

        o = sg.exec(vv, "status", "-N");
        testlib.ok(o.exit_status == 0, "exit status should be 0");
        print("vv status -N:\n" + o.stdout);

        //testlib.ok(o.stdout.search("xx_folder/")   < 0);
        //testlib.ok(o.stdout.search("xx_folder2/")  < 0);
        //testlib.ok(o.stdout.search("xx_folder3/")  < 0);
        //testlib.ok(o.stdout.search("xx_fileex.h")  < 0);
        //testlib.ok(o.stdout.search("xx_file1.txt") < 0);
        //testlib.ok(o.stdout.search("xx_file2.h")   < 0);
        //testlib.ok(o.stdout.search("xx_file3.h")   < 0);

	//////////////////////////////////////////////////////////////////
	// -N on a file is kinda silly, but we have to allow it.  I guess.

        o = sg.exec(vv, "status", file1, "-N");
        testlib.ok(o.exit_status == 0, "exit status should be 0");
        print("vv status -N " + file1 + ":\n" + o.stdout);

        testlib.ok(o.stdout.search("xx_folder2/")   < 0);
        testlib.ok(o.stdout.search("xx_folder3/")   < 0);
        testlib.ok(o.stdout.search("xx_fileex.h")   < 0);
        testlib.ok(o.stdout.search("xx_file1.txt") >= 0);
        testlib.ok(o.stdout.search("xx_file2.h")    < 0);
        testlib.ok(o.stdout.search("xx_file3.h")    < 0);
       
    }
    this.testMove = function testMove() {
        var base = getTestName(arguments.callee.toString());
	//First, verify that move, and all aliases with no arguments 
	//result in the correct errors.
	returnObj = sg.exec(vv, "move");
        testlib.ok(returnObj.stderr.search("Usage: vv move") >= 0);
	returnObj = sg.exec(vv, "mv");
        testlib.ok(returnObj.stderr.search("Usage: vv move") >= 0);

	}
    this.testForceOption = function testForceOption() {
        var base = getTestName(arguments.callee.toString());
        var origin = createFolderOnDisk(pathCombine(base, "origin"));
        var target = createFolderOnDisk(pathCombine(base, "target"));
        var subFolder = createFolderOnDisk(pathCombine(origin, "subFolder"));
        var subFolderInTarget = createFolderOnDisk(pathCombine(target, "subFolder"));
        var file = createFileOnDisk(pathCombine(origin, "file1.txt"), 39);
        var fileInTarget = createFileOnDisk(pathCombine(target, "file1.txt"), 39);
        addRemoveAndCommit();
        
	returnObj = sg.exec(vv, "move", "-F", subFolder, target);
	testlib.assertSuccessfulExec(returnObj);
	testlib.existsOnDisk(subFolderInTarget);
	testlib.inRepo(subFolderInTarget);
	testlib.notOnDisk(subFolder);
	returnObj = sg.exec(vv, "move", "-F", file, target);
        testlib.ok(returnObj.exit_status == 0);
	testlib.existsOnDisk(fileInTarget);
	testlib.inRepo(fileInTarget);
	testlib.notOnDisk(file);
	returnObj = sg.exec(vv, "commit", "-m", "\"\"");
        testlib.ok(returnObj.exit_status == 0);
	}
    this.testMoveMultiple = function testMoveMultiple() {
        var base = getTestName(arguments.callee.toString());
        var origin = createFolderOnDisk(pathCombine(base, "origin"));
        var target = createFolderOnDisk(pathCombine(base, "target"));
        var file1 = createFileOnDisk(pathCombine(origin, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(origin, "file2.txt"), 39);
        var file3 = createFileOnDisk("file3.txt", 39);
        addRemoveAndCommit();

	returnObj = testlib.execVV("move", file1, file2, file3, target);
	testlib.assertSuccessfulExec(returnObj);
	testlib.existsOnDisk(pathCombine(target, "file1.txt"));
	testlib.existsOnDisk(pathCombine(target, "file2.txt"));
	testlib.existsOnDisk(pathCombine(target, "file3.txt"));
	}

    this.testNoNesting = function testNoNesting() {
	sleep(5000);

	var base = getTestName(arguments.callee.toString());
	var wd = createFolderOnDisk(pathCombine(base, "wd"));

	if (! sg.fs.cd(wd))
	{
	    testlib.testResult(false, "unable to change to " + wd);
	    return;
	}

	var newRepoName = "myshinynewrepo" + (new Date()).getTime();

	returnObj = sg.exec(vv, "init", newRepoName);

	testlib.assertFailedExec(returnObj);

	for ( var r in sg.list_descriptors() )
	    {
		if (r == newRepoName)
		    testlib.testResult(false, newRepoName + " should not be created");
	    }
    };

}

function stCLCStatus() 
{
	this.testStatusOnMissingObject =function()
	{
		//This should inform you that one of the objects couldn't be found.
		returnObj = sg.exec(vv, "status", "itemThatIsn'tThere");
        	//testlib.ok(returnObj.stderr.search("The requested object could not be found") >= 0);
		//testlib.assertFailedExec(returnObj);
        	testlib.ok(returnObj.stdout == "");
        	testlib.ok(returnObj.stderr == "");
		testlib.assertSuccessfulExec(returnObj);
	}
	this.testStatusOnMissingObjectWithPendingChange =function()
	{
		//This should inform you that one of the objects couldn't be found.
		sg.file.write("bob.txt", "nothing");
		returnObj = sg.exec(vv, "status", "itemThatIsn'tThere");
        	testlib.ok(returnObj.stdout == "");
        	testlib.ok(returnObj.stderr == "");
		testlib.assertSuccessfulExec(returnObj);
        	//testlib.ok(returnObj.stderr.search("The requested object could not be found") >= 0);
		//testlib.assertFailedExec(returnObj);
		returnObj = sg.exec(vv, "status", "itemThatIsn'tThere", "bob.txt");
        	testlib.ok(returnObj.stdout.search("@/bob.txt") >= 0);
        	testlib.ok(returnObj.stderr == "");
		testlib.assertSuccessfulExec(returnObj);
        	//testlib.ok(returnObj.stderr.search("The requested object could not be found") >= 0);
		//testlib.assertFailedExec(returnObj);
		sg.fs.remove("bob.txt");
	}
	function verifyThatAtLeastOneLineEndsWith(lines, strSearch)
	{
		for (i in lines)
		{
			if (lines[i].match(strSearch+"$")==strSearch)
				return;
		}
		testlib.equal(1, 0, "Did not find a matching line for: " + strSearch);
	}
    this.testTestOption = function testTestOption() {
        	var base = getTestName(arguments.callee.toString());
		createFolderOnDisk(base);
        	addRemoveAndCommit();
		subfolderName = base + "/subfolder";
		subfileName = subfolderName + "/subfile";
		subsubfolderName = subfolderName + "/sub2";
		subsubfileName = subsubfolderName + "/subfile";
		createFolderOnDisk(subfolderName);
		createFolderOnDisk(subfileName);
		createFolderOnDisk(subsubfolderName);
		createFolderOnDisk(subsubfileName);
		
		statusSoFar = sg.pending_tree.status();
		for (i in statusSoFar)
		{
			testlib.equal(i, "Found", "status should be empty, except for Found files");
		}
		linesOfOutput = testlib.execVV_lines("add", "--test", subfolderName);
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subfolderName + "\",");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subfileName + "\",");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subsubfolderName + "\",");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subsubfileName + "\",");
		statusSoFar = sg.pending_tree.status();
		for (i in statusSoFar)
		{
			testlib.equal(i, "Found", "status should be empty, except for Found files");
		}
		linesOfOutput = testlib.execVV_lines("addremove", "--test", subfolderName);
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subfolderName + "\",");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subfileName + "\",");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subsubfolderName + "\",");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subsubfileName + "\",");
		statusSoFar = sg.pending_tree.status();
		for (i in statusSoFar)
		{
			testlib.equal(i, "Found", "status should be empty, except for Found files");
		}
		linesOfOutput = testlib.execVV_lines("addremove", "--test");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subfolderName + "\",");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subfileName + "\",");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subsubfolderName + "\",");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subsubfileName + "\",");
		statusSoFar = sg.pending_tree.status();
		for (i in statusSoFar)
		{
			testlib.equal(i, "Found", "status should be empty, except for Found files");
		}
	}
    this.testTestOption__Remove = function testTestOption__Remove() {
        	var base = getTestName(arguments.callee.toString());
		createFolderOnDisk(base);
		subfolderName = base + "/subfolder";
		subfileName = subfolderName + "/subfile";
		subsubfolderName = subfolderName + "/sub2";
		subsubfileName = subsubfolderName + "/subfile";
		createFolderOnDisk(subfolderName);
		createFolderOnDisk(subfileName);
		createFolderOnDisk(subsubfolderName);
		createFolderOnDisk(subsubfileName);
        	addRemoveAndCommit();
		
		statusSoFar = sg.pending_tree.status();
		for (i in statusSoFar)
			testlib.equal(1, 0, "status should be empty");
		linesOfOutput = testlib.execVV_lines("remove", "--test", subfolderName);
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subfolderName + "\",");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subfileName + "\",");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subsubfolderName + "\",");
		verifyThatAtLeastOneLineEndsWith(linesOfOutput, subsubfileName + "\",");
		statusSoFar = sg.pending_tree.status();
		for (i in statusSoFar)
			testlib.equal(1, 0, "status should be empty");
	}
}

function stCLCwhoami() 
{
	this.testWhoamiWithNoArguments =function()
	{
		//This should print out your current user.
		returnObj = testlib.execVV("whoami");
        	testlib.ok(returnObj.stdout.length >= 0);
	}
}
