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

function stRevertTestsFailing() {

    this.testRevertMoveFileToRenamedFile = function testRevertMoveFileToRenamedFile() {
        // rename folder1/sharedname.txt to uniquename.txt
        // move folder2/sharedname.txt to folder1
        // revert
        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(pathCombine(base, "folder1"));
        var folder2 = createFolderOnDisk(pathCombine(base, "folder2"));
        var file1 = createFileOnDisk(pathCombine(folder1, "sharedname.txt"), 9);
        var file2 = createFileOnDisk(pathCombine(folder2, "sharedname.txt"), 20)

        addRemoveAndCommit();

        testlib.inRepo(folder1);
        testlib.inRepo(folder2);
        testlib.inRepo(file1);
        testlib.inRepo(file2);
        sg.pending_tree.rename(file1, "uniquename.txt");
        sg.pending_tree.move(file2, folder1);
        sg.pending_tree.revert();
        testlib.statusEmpty();

        testlib.inRepo(file1);
        testlib.inRepo(file2);
        testlib.existsOnDisk(file1);
	// TODO verify file1 has 9 lines
        testlib.existsOnDisk(file2);
	// TODO verify file2 has 20 lines
    }

    this.testRevertCreateFileWithRenamedFile = function testRevertCreateFileWithRenamedFile() {
        // rename folder1/sharedname.txt to uniquename.txt
        // create new file: folder1/sharedname.txt
        // revert
	//
	// revert should cause the new file to be backed-up to folder1/sharedname.txt~sg00~ so
	// that we can undo the rename.
        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(pathCombine(base, "folder1"));
        var fileA = createFileOnDisk(pathCombine(folder1, "sharedname.txt"), 3);

        addRemoveAndCommit();

        sg.pending_tree.rename(fileA, "uniquename.txt");
        var fileC = pathCombine(folder1, "uniquename.txt");
        testlib.existsOnDisk(fileC);
        testlib.notOnDisk(fileA);

        createFileOnDisk(fileA, 15);
        sg.pending_tree.addremove();

        var backup = fileA + "~sg00~";
        testlib.notOnDisk(backup);

        sg.pending_tree.revert();

        testlib.notOnDisk(fileC);
	testlib.existsOnDisk(fileA);
	// TODO verify that fileA has 3 lines

        testlib.existsOnDisk(backup);
        deleteFileOnDisk(backup);

        testlib.statusEmpty();
    }

    this.testRevertRenameFileAsRenamedFile = function testRevertRenameFileAsRenamedFile() {
        // rename folder1/sharedname.txt to uniquename.txt
        // rename folder1/oldname.txt to sharedname.txt
        // revert
        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(pathCombine(base, "folder1"));
        var fileA = createFileOnDisk(pathCombine(folder1, "sharedname.txt"), 3);
        var fileB = createFileOnDisk(pathCombine(folder1, "oldname.txt"), 8);

        addRemoveAndCommit();

        sg.pending_tree.rename(fileA, "uniquename.txt");
        var fileC = pathCombine(folder1, "uniquename.txt");
        testlib.existsOnDisk(fileC);
        testlib.notOnDisk(fileA);

        sg.pending_tree.rename(fileB, "sharedname.txt");
        var newfileB = pathCombine(folder1, "sharedname.txt");
        testlib.existsOnDisk(newfileB);
        testlib.notOnDisk(fileB);

        sg.pending_tree.revert();

        testlib.notOnDisk(fileC);
        testlib.existsOnDisk(fileB);
        testlib.existsOnDisk(fileA);

        testlib.statusEmpty();
    }

    this.testRevertCreateFolderWithRenamedFolder = function testRevertCreateFolderWithRenamedFolder() {
	// create base/sharedname/a.txt
        // rename base/sharedname/ uniquename/
        // mkdir base/sharedname/
	// create base/sharedname/b.txt
	//
        // revert
	//
	// verify that we have
	// base/sharedname~sg00~/
	// base/sharedname~sg00~/b.txt
	// base/sharedname/
	// base/sharedname/a.txt
        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(pathCombine(base, "sharedname"));
	var fileA = createFileOnDisk(pathCombine(folder1,"a.txt"));

        addRemoveAndCommit();

        sg.pending_tree.rename(folder1, "uniquename");
        var folderUnique = pathCombine(getParentPath(folder1), "uniquename");
	var file_A_Unique = pathCombine(folderUnique,"a.txt");
        testlib.existsOnDisk(folderUnique);
	testlib.existsOnDisk(file_A_Unique);
        testlib.notOnDisk(folder1);

        createFolderOnDisk(folder1);
	var fileB = createFileOnDisk(pathCombine(folder1,"b.txt"));
        sg.pending_tree.addremove();

        var backup = folder1 + "~sg00~";
        testlib.notOnDisk(backup);

        sg.pending_tree.revert();

        testlib.notOnDisk(folderUnique);
	testlib.existsOnDisk(folder1);
	testlib.existsOnDisk(fileA);
	testlib.notOnDisk(file_A_Unique);
        testlib.existsOnDisk(backup);
	testlib.existsOnDisk(pathCombine(backup,"b.txt"));
	testlib.notOnDisk(fileB);

        deleteFolderOnDisk(backup);

        testlib.statusEmpty();
    }

    this.testRevertMoveFileThenDelete = function testRevertMoveFileThenDelete() {
        // Move a file to another folder. Delete the file.  Try to revert.
        // Do this using both deleteOnDisk and pending_tree.remove
        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(pathCombine(base, "folder1"));
        var folder2 = createFolderOnDisk(pathCombine(base, "folder2"));
        var fileA = createFileOnDisk(pathCombine(folder1, "fileA.txt"), 3);

        addRemoveAndCommit();

        sg.pending_tree.move(fileA, folder2);
        testlib.existsOnDisk(pathCombine(folder2, "fileA.txt"));
        testlib.notOnDisk(fileA);
        deleteFileOnDisk(pathCombine(folder2, "fileA.txt"));  // comment this line and revert will succeed

        sg.pending_tree.revert();

        testlib.statusEmpty();
    }

// Update: 01/06/2010 Jeff says: This is not a valid test.  We currently do not
//                               allow DELETE+MOVE.
//
//    this.testRevertMoveFileThenRemove = function testRevertMoveFileThenRemove() {
//        // Move a file to another folder. Delete the file.  Try to revert.
//        // Do this using both deleteOnDisk and pending_tree.remove
//        var base = getTestName(arguments.callee.toString());
//        var folder1 = createFolderOnDisk(pathCombine(base, "folder1"));
//        var folder2 = createFolderOnDisk(pathCombine(base, "folder2"));
//        var fileA = createFileOnDisk(pathCombine(folder1, "fileA.txt"), 3);
//
//        addRemoveAndCommit();
//
//        sg.pending_tree.move(fileA, folder2);
//        testlib.existsOnDisk(pathCombine(folder2, "fileA.txt"));
//        testlib.notOnDisk(fileA);
//        // TODO: pending_tree.remove fails with "object cannot be safely removed."  Not sure why.
//        // TODO: Answer: we currently do not allow DELETES+MOVES or DELETES+RENAMES or DELETES+<anything>
//        sg.pending_tree.remove(pathCombine(folder2, "fileA.txt"));
//        testlib.notOnDisk(pathCombine(folder2, "fileA.txt"));
//
//        sg.pending_tree.revert();
//    }

    this.testRevertComplicatedMoves = function testRevertComplicatedMoves() {
	// mkdir base/parentFolder/moveMe/subFolder/
	// mkdir base/other/
	// commit
	//
	// mv base/parentFolder/moveMe/ base/other/  
	// so we should have:
	//     base/parentFolder/
	//     base/other/moveMe/
	//     base/other/moveMe/subFolder/
	//
	// mv base/parentFolder/ base/other/moveMe/subFolder/
	// so we should have:
	//     base/other/moveMe/
	//     base/other/moveMe/subFolder/
	//     base/other/moveMe/subFolder/parentFolder/
	//
	// revert
        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(pathCombine(base, "parentFolder/moveMe/subFolder"));
        var folder2 = createFolderOnDisk(pathCombine(base, "other"));
        addRemoveAndCommit();

        testlib.inRepo(folder1);
        testlib.inRepo(folder2);

        indentLine('-> sg.pending_tree.move(getParentPath(folder1), folder2);');
        sg.pending_tree.move(getParentPath(folder1), folder2);
        var move2 = pathCombine(base, "other/moveMe/subFolder");
        indentLine('-> sg.pending_tree.move(pathCombine(base, "parentFolder"), move2);');
        sg.pending_tree.move(pathCombine(base, "parentFolder"), move2);
        indentLine('-> sg.pending_tree.revert();');
        sg.pending_tree.revert();
        testlib.statusEmpty();
        testlib.existsOnDisk(folder1);
        testlib.existsOnDisk(folder2);

    }

    this.testRevertFolderDelete = function testRevertFolderDelete() {

        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(base + "1");
        var folder2 = createFolderOnDisk(pathCombine(folder1, base + 2));
        var file = createFileOnDisk(pathCombine(folder2, "file1.txt"));

        addRemoveAndCommit();
        sg.pending_tree.remove(folder2);
        sg.pending_tree.revert();

        testlib.existsOnDisk(folder2);
	testlib.existsOnDisk(file);
        testlib.inRepo(folder2);
    }

    this.testSimpleFileMoveDownRevert = function testSimpleFileMoveDownRevert() {

        var base = getTestName(arguments.callee.toString());
        folder1 = createFolderOnDisk(pathCombine(base, "folder1"));
        nestedFile = createFileOnDisk(pathCombine(folder1, "file1"), 10);
        rootFile = pathCombine(base, "file1");

        addRemoveAndCommit();

        testlib.inRepo(nestedFile);

        indentLine("*> " + formatFunctionString("sg.pending_tree.move", [nestedFile, base]));
        sg.pending_tree.move(nestedFile, base);
        indentLine("*> " + formatFunctionString("sg.pending_tree.remove", [folder1]));
        sg.pending_tree.remove(folder1);

        testlib.existsOnDisk(rootFile);
        testlib.notOnDisk(folder1);

        indentLine("## about to revert ...");
        sg.pending_tree.revert();

        indentLine("!! testing for " + nestedFile);
        testlib.notOnDisk(rootFile);
        testlib.existsOnDisk(nestedFile);
    }

    this.testDeepFileMoveDownRevert = function testDeepFileMoveDownRevert() {
        // this is the same as testSimpleFileMoveDownRevert, but with more depth

        var base = getTestName(arguments.callee.toString());
        var depth = 20;
        var rootFolder = [];
        var deepFolder = [];
        var tip = base;
        var i;

        for (i=1; i<=depth; i++) {
            rootFolder[i] = pathCombine(base, "f" + i);
            deepFolder[i] = pathCombine(tip, "f" + i);
            createFolderOnDisk(deepFolder[i]);
            createFileOnDisk(pathCombine(deepFolder[i], "file" + i), i);
            tip = deepFolder[i];
        }

        addRemoveAndCommit();

        testlib.inRepo(pathCombine(tip, "file" + depth));

        for (i=depth; i>=1; i--) {
            indentLine("*> " + formatFunctionString("sg.pending_tree.move", [pathCombine(deepFolder[i], "file" + i), base]));
            sg.pending_tree.move(pathCombine(deepFolder[i], "file" + i), base);
            indentLine("*> " + formatFunctionString("sg.pending_tree.remove", [deepFolder[i]]));
            sg.pending_tree.remove(deepFolder[i]);
        }

        for (i=1; i<=depth; i++) {
            testlib.existsOnDisk(pathCombine(base,"file" + i));
            testlib.notOnDisk(deepFolder[i]);
        }

        indentLine("## about to revert ...");
        sg.pending_tree.revert();

        for (i=1; i<=depth; i++) {
            indentLine("!! testing for " + deepFolder[i] + "/file" + i);
            testlib.notOnDisk(pathCombine(base,"file" + i));
            testlib.existsOnDisk(pathCombine(deepFolder[i], "file" + i));
        }
    }

}
