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

function stRevertTests() {

    this.testEditFileRevert = function testEditFileRevert() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        addRemoveAndCommit();
        testlib.inRepo(folder);
        testlib.inRepo(file);

        var tmpFile = createTempFile(file);

        insertInFile(file, 20);

        var tmpFile2 = createTempFile(file);

        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);

        var backup = file + "~sg00~";

        // testlib.statusEmpty();
        testlib.ok(compareFiles(tmpFile, file), "Files should be identical after the revert");
        testlib.existsOnDisk(backup);
        testlib.ok(compareFiles(tmpFile2, backup), "Backup file should match edited file");
        sg.fs.remove(tmpFile);
        sg.fs.remove(tmpFile2);
        testlib.statusEmpty();

    }

    this.testDeleteFileRevert = function testDeleteFileRevert() {
        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 29);

        addRemoveAndCommit();

        testlib.inRepo(folder);
        testlib.inRepo(file);

        deleteFileOnDisk(file);
        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);
        testlib.statusEmpty();
        testlib.existsOnDisk(file);
    }

    this.testDeleteFolderRevert = function testDeleteFolderRevert() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base, 3);
        var folder2 = createFolderOnDisk(pathCombine(folder, base + "2"));

        addRemoveAndCommit();

        testlib.inRepo(folder);
        testlib.inRepo(folder2);

        deleteFolderOnDisk(folder2);
        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);
        testlib.statusEmpty();
        testlib.existsOnDisk(folder2);
    }

    this.testMoveRevert = function testRevertMove() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base, 3);
        var folder2 = createFolderOnDisk(pathCombine(folder, base + 4));
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 25);

        addRemoveAndCommit();

        testlib.inRepo(folder);
        testlib.inRepo(folder2);
        testlib.inRepo(file);

        sg.pending_tree.move(file, folder2);
        testlib.existsOnDisk(pathCombine(folder2, "file1.txt"));
        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);
        testlib.statusEmpty();
        testlib.existsOnDisk(file);
        testlib.notOnDisk(pathCombine(folder2, "file1.txt"));


    }

    this.testFileRenameRevert = function testFileRenameRevert() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base, 3);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 25);

        addRemoveAndCommit();

        testlib.inRepo(folder);

        testlib.inRepo(file);

        sg.pending_tree.rename(file, "rename_file1.txt");
        testlib.existsOnDisk(pathCombine(folder, "rename_file1.txt"));
        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);
        testlib.statusEmpty();
        testlib.existsOnDisk(file);
        testlib.notOnDisk(pathCombine(folder, "rename_file1.txt"));

    }

    this.testFolderRenameRevert = function testFolderRenameRevert() {

        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base, 2);

        addRemoveAndCommit();

        testlib.inRepo(folder);

        var newName = pathCombine(getParentPath(folder), "rename_" + base);


        sg.pending_tree.rename(folder, "rename_" + base);
        testlib.existsOnDisk(newName);
        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);
        testlib.statusEmpty();
        testlib.existsOnDisk(folder);
        testlib.notOnDisk(newName);


    }

    this.testMoveBackToOriginal = function testMoveBackToOriginal() {
        /*As per combo test instructions -
           
        sg.pending_tree.move("folder_1/file_1.txt", "folder_2/"); 
        sg.pending_tree.move("folder_2/file_1.txt", "folder_1/"); 
        */
        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(base + "1");
        var folder2 = createFolderOnDisk(base + "2");
        var file = createFileOnDisk(pathCombine(folder1, "file1.txt"));

        addRemoveAndCommit();

        testlib.inRepo(folder1);
        testlib.inRepo(folder2);
        testlib.inRepo(file);

        sg.pending_tree.move(file, folder2);
        sg.pending_tree.move(pathCombine(folder2, "file1.txt"), folder1);
        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);
        testlib.statusEmpty();
        testlib.existsOnDisk(file);
        testlib.notOnDisk(pathCombine(folder2, "file1.txt"));

        insertInFile(file, 3);
        sg.pending_tree.move(file, folder2);
        sg.pending_tree.move(pathCombine(folder2, "file1.txt"), folder1);
        commit();
        testlib.existsOnDisk(file);
        testlib.notOnDisk(pathCombine(folder2, "file1.txt"));

    }

    this.testMoveAndDeleteFileRevert = function testMoveAndDeleteFileRevert() {
        //same as above - delete file with move & revert
        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(pathCombine(base, "folder1"));
        var folder2 = createFolderOnDisk(pathCombine(base, "folder2"));
        var file = createFileOnDisk(pathCombine(folder1, "file.txt"), 10);
        addRemoveAndCommit();
        sg.pending_tree.remove(file);
        testlib.notOnDisk(file);
        sg.pending_tree.move(folder1, folder2);
        testlib.notOnDisk(folder1);
        testlib.existsOnDisk(pathCombine(folder2, "folder1"));
        var revertActionLog = sg.pending_tree.revert(["verbose"], pathCombine(folder2, "folder1/file.txt"));
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);
        testlib.existsOnDisk(pathCombine(folder2, "folder1/file.txt"));
        testlib.notOnDisk(folder1);
        sg.pending_tree.commit(); // flush pending stuff
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
    }

    this.testMoveAndDeleteFolderRevert = function testMoveAndDeleteFolderRevert() {
        //same as above - delete folder with move & revert
        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(pathCombine(base, "folder1"));
        var folder2 = createFolderOnDisk(pathCombine(base, "folder2"));
        var subfolder = createFolderOnDisk(pathCombine(folder1, "subfolder"));
        addRemoveAndCommit();
        sg.pending_tree.remove(subfolder);
        testlib.notOnDisk(subfolder);
        sg.pending_tree.move(folder1, folder2);
        testlib.notOnDisk(folder1);
        testlib.existsOnDisk(pathCombine(folder2, "folder1"));
        var revertActionLog = sg.pending_tree.revert(["verbose"], pathCombine(folder2, "folder1/subfolder"));
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);
        testlib.existsOnDisk(pathCombine(folder2, "folder1/subfolder"));
        testlib.notOnDisk(folder1);
        sg.pending_tree.commit(); // flush pending stuff
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
    }

    this.testEditMoveRevert = function testEditMoveRevert() {
        //edit a file and move the same file then revert.
        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base + "1");
        var folder2 = createFolderOnDisk(base + "2", 3);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 120);

        addRemoveAndCommit();

        testlib.inRepo(folder);
        testlib.inRepo(folder2);
        testlib.inRepo(file);

        var filev1 = createTempFile(file);

        doRandomEditToFile(file);
	testlib.ok((compareFiles(file,filev1) == false), "doRandomEditToFile modified file.");

        var filev2 = createTempFile(file);

        sg.pending_tree.move(file, folder2);
        testlib.notOnDisk(file);
        testlib.existsOnDisk(pathCombine(folder2, "file1.txt"));

        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);

        var backup = file + "~sg00~";

        // testlib.statusEmpty();
        testlib.existsOnDisk(file);
        testlib.notOnDisk(pathCombine(folder2, "file1.txt"));
        testlib.ok(compareFiles(filev1, file), "Files should be identical after the revert");
        testlib.existsOnDisk(backup);
        testlib.ok(compareFiles(filev2, backup), "Backup file should match edited file");
        sg.fs.remove(filev1);
        sg.fs.remove(filev2);
        testlib.statusEmpty();
    }

    this.testEditRenameRevert = function testEditRenameRevert() {
        //edit a file and move the same file then revert.
        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 120);

        addRemoveAndCommit();

        testlib.inRepo(folder);
        testlib.inRepo(file);

        var filev1 = createTempFile(file);

        doRandomEditToFile(file);
	testlib.ok((compareFiles(file,filev1) == false), "doRandomEditToFile modified file.");

        var filev2 = createTempFile(file);

        sg.pending_tree.rename(file, "renamed_file1.txt");

        testlib.existsOnDisk(pathCombine(folder, "renamed_file1.txt"));
        testlib.notOnDisk(file);

        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);

        var backup = file + "~sg00~";

        // testlib.statusEmpty();
        testlib.existsOnDisk(file);
        testlib.notOnDisk(pathCombine(folder, "renamed_file1.txt"));
        testlib.ok(compareFiles(filev1, file), "Files should be identical after the revert");
        testlib.existsOnDisk(backup);
        testlib.ok(compareFiles(filev2, backup), "Backup file should match edited file");
        sg.fs.remove(filev1);
        sg.fs.remove(filev2);
        testlib.statusEmpty();
    }

    this.testRenameFileLikeParentRevert = function testRenameFileLikeParentRevert() {
        // Verify that you can rename a file with the same name as the folder where the file resides.
        // ???? not sure if we want to revert to or from the above condition ... doing both

        // revert from:
        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(pathCombine(base + "_from", "folder1"));
        var fileA = createFileOnDisk(pathCombine(folder1, "fileA.txt"), 3);

        addRemoveAndCommit();

        var fpath = getParentPath(fileA);
        var fname = getFileNameFromPath(fpath);
        var newFileA = pathCombine(fpath, fname);
        sg.pending_tree.rename(fileA, fname);
        testlib.existsOnDisk(newFileA);
        testlib.notOnDisk(fileA);

        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);

        testlib.inRepo(fileA);
        testlib.notInRepo(newFileA);
        testlib.existsOnDisk(fileA);
        testlib.notOnDisk(newFileA);

        // revert to:
        folder1 = createFolderOnDisk(pathCombine(base + "_to", "commonName"));
        fileA = createFileOnDisk(pathCombine(folder1, "commonName"), 3);

        addRemoveAndCommit();

        fpath = getParentPath(fileA);
        fname = "uncommonName";
        newFileA = pathCombine(fpath, fname);
        sg.pending_tree.rename(fileA, fname);
        testlib.existsOnDisk(newFileA);
        testlib.notOnDisk(fileA);

        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);

        testlib.inRepo(fileA);
        testlib.notInRepo(newFileA);
        testlib.existsOnDisk(fileA);
        testlib.notOnDisk(newFileA);
    }

    this.testRenameFolderLikeChildRevert = function testRenameFolderLikeChildRevert() {
        // Verify that you can rename a folder with the same name as a file that exists in that folder. 
        // ???? not sure if we want to revert to or from the above condition ... doing both

        // revert from:
        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(pathCombine(base + "_from", "folder1"));
        var fileA = createFileOnDisk(pathCombine(folder1, "fileA.txt"), 3);

        addRemoveAndCommit();

        var fpath = getParentPath(folder1);
        var fname = getFileNameFromPath(fileA);
        var newFolder1 = pathCombine(fpath, fname);
        sg.pending_tree.rename(folder1, fname);
        testlib.existsOnDisk(newFolder1);
        testlib.notOnDisk(folder1);

        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);

        testlib.inRepo(folder1);
        testlib.notInRepo(newFolder1);
        testlib.existsOnDisk(folder1);
        testlib.notOnDisk(newFolder1);

        // revert to:
        folder1 = createFolderOnDisk(pathCombine(base + "_to", "commonName"));
        fileA = createFileOnDisk(pathCombine(folder1, "commonName"), 3);

        addRemoveAndCommit();

        fpath = getParentPath(folder1);
        fname = "uncommonName";
        newFolder1 = pathCombine(fpath, fname);
        sg.pending_tree.rename(folder1, fname);
        testlib.existsOnDisk(newFolder1);
        testlib.notOnDisk(folder1);

        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);

        testlib.inRepo(folder1);
        testlib.notInRepo(newFolder1);
        testlib.existsOnDisk(folder1);
        testlib.notOnDisk(newFolder1);
    }

    this.testDeleteEditRevert = function testDeleteEditRevert() {
        // delete a file and edit the same file then revert 
        var i = 0;
        var base = getTestName(arguments.callee.toString());
        var folder = createFolderOnDisk(base);
        var file = createFileOnDisk(pathCombine(folder, "file.txt"), 10);
        addRemoveAndCommit();

        var filev1 = createTempFile(file);
        sg.pending_tree.remove(file);
        testlib.notOnDisk(file);

        copyFileOnDisk(filev1, file);
        testlib.existsOnDisk(file);
        doRandomEditToFile(file);
	testlib.ok((compareFiles(file,filev1) == false), "doRandomEditToFile modified file.");
        var filev2 = createTempFile(file);
        reportTreeObjectStatus(file);
        sg.pending_tree.addremove();
        reportTreeObjectStatus(file);
        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);

        var backup = file + "~sg00~";
        testlib.existsOnDisk(file);
        testlib.ok(compareFiles(file, filev1), "Files should be identical after the revert");
        testlib.existsOnDisk(backup);
        testlib.ok(compareFiles(backup, filev2), "Backup file should match edited file");
        sg.fs.remove(filev1);
        sg.fs.remove(filev2);
        sg.fs.remove(backup);
        testlib.statusEmpty();
    }

    this.testStairMoveUpRevert = function testStairMoveUpRevert() {

        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(pathCombine(base, "folder1"));
        var folder2 = createFolderOnDisk(pathCombine(base, "folder2"));
        var folder3 = createFolderOnDisk(pathCombine(base, "folder3"));

        addRemoveAndCommit();

        testlib.inRepo(folder1);
        testlib.inRepo(folder2);
        testlib.inRepo(folder3);

        sg.pending_tree.move(folder3, folder2);
        sg.pending_tree.move(folder2, folder1);
        testlib.existsOnDisk(pathCombine(folder1, "folder2/folder3"));

        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);
        testlib.statusEmpty();
        testlib.notOnDisk(pathCombine(folder1, "folder2"));
        testlib.existsOnDisk(folder1);
        testlib.existsOnDisk(folder2);
        testlib.existsOnDisk(folder3);
    }

    this.testStairMoveDownRevert = function testStairMoveDownRevert() {

        var base = getTestName(arguments.callee.toString());
        var folder1 = createFolderOnDisk(pathCombine(base, "folder1"));
        var folder2 = createFolderOnDisk(pathCombine(folder1, "folder2"));
        var folder3 = createFolderOnDisk(pathCombine(folder2, "folder3"));

        addRemoveAndCommit();

        testlib.inRepo(folder3);

        sg.pending_tree.move(folder2, base);
        sg.pending_tree.move(pathCombine(base, "folder2/folder3"), base);

        testlib.existsOnDisk(folder1);
        testlib.existsOnDisk(pathCombine(base, "folder2"));
        testlib.existsOnDisk(pathCombine(base, "folder3"));
        
        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);
        testlib.statusEmpty();
        testlib.notOnDisk(pathCombine(base, "folder2"));
        testlib.notOnDisk(pathCombine(base, "folder3"));
        testlib.existsOnDisk(folder3);
    }

    this.testDeepStairMoveUpRevert = function testDeepStairMoveUpRevert() {

        var base = getTestName(arguments.callee.toString());
        var depth = 20;
        var rootFolder = [];
        var deepFolder = [];
        var tip = base;
        var i;

        for (i=1; i<=depth; i++) {
            rootFolder[i] = pathCombine(base, "f" + i);
            deepFolder[i] = pathCombine(tip, "f" + i);
            createFolderOnDisk(rootFolder[i]);
            createFileOnDisk(pathCombine(rootFolder[i], "file" + i), i);
            tip = deepFolder[i];
        }

        addRemoveAndCommit();

        for (i=1; i<=depth; i++) {
            testlib.inRepo(pathCombine(rootFolder[i],"file" + i));
        }

        for (i=depth; i>=2; i--) {
            indentLine("*> " + formatFunctionString("sg.pending_tree.move", [rootFolder[i], rootFolder[i-1]]));
            sg.pending_tree.move(rootFolder[i], rootFolder[i-1]);
        }

        for (i=2; i<=depth; i++) {
            testlib.notOnDisk(rootFolder[i]);
        }
        testlib.existsOnDisk(pathCombine(tip, "file" + depth));

        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);

        for (i=1; i<=depth; i++) {
            testlib.existsOnDisk(pathCombine(rootFolder[i],"file" + i));
        }
        testlib.notOnDisk(pathCombine(rootFolder[1], "f2"));
    }

    this.testDeepStairMoveDownRevert = function testDeepStairMoveDownRevert() {

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

        for (i=2; i<=depth; i++) {
            indentLine("*> " + formatFunctionString("sg.pending_tree.move", [pathCombine(rootFolder[i-1], "f" + i), base]));
            sg.pending_tree.move(pathCombine(rootFolder[i-1], "f" + i), base);
        }

        for (i=1; i<=depth; i++) {
            testlib.existsOnDisk(pathCombine(rootFolder[i],"file" + i));
        }

        var revertActionLog = sg.pending_tree.revert(["verbose"]);
	//sleep(1000);	// sleep after a commit so that subsequent operations won't confuse timestamp cache on low resolution filesystems.
	dumpActionLog(base,revertActionLog);

        testlib.existsOnDisk(pathCombine(tip, "file" + depth));
        for (i=2; i<=depth; i++) {
            testlib.notOnDisk(rootFolder[i]);
        }
    }

}
