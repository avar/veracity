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

// USES SERVER PORT 8086

// for JSON stringifying.  I know sglib has this, but this is the version
// used client-side, so it's available here for consistency
load("json2.js");


function stWeb() {

    this.curl = ((sg.platform() == "WINDOWS") ? "curl.exe" : "curl");

    this.setUp = function() {
        // if this is going to die, let's do it before we start up the server
        // and leak things

        var witTemplate = sg.file.read(pathCombine(srcDir, "../libraries/templates/sg_ztemplate__wit.json"));
        var supTemplate = sg.file.read(pathCombine(srcDir, "../libraries/templates/sg_ztemplate__sup.json"));

        // Set the ssi
       /*{
            sg.exec_nowait("vv", "localsettings", "ssi=c:/s/sprawl/src/cmd");
            sleep(5000); // Give it some time to start up before we start hitting it with requests.
        }*/
        // Start the server
        {
            this.serverPid = sg.exec_nowait(
                ["output=../stWebStdout.txt","error=../stWebStderr.txt"],
                "vv", "serve", "--port=8086");
            sleep(5000); // Give it some time to start up before we start hitting it with requests.
        }

        // Create a changeset for future reference
        {
            sg.file.write("stWeb.txt", "stWeb");
            sg.pending_tree.add("stWeb.txt");
            this.changesetId = sg.pending_tree.commit();

            sg.file.write("stWeb2.txt", "stWeb2");
            sg.pending_tree.add("stWeb2.txt");
            this.changeset2Id = sg.pending_tree.commit();
        }

        // Set up our zing databases.
        {
            var repo = sg.open_repo(repInfo.repoName);

            {

                var t;

                eval('t = ' + witTemplate);

                var zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);
                var ztx = zs.begin_tx();
                ztx.set_template(t);
                var zrec = ztx.new_record("item");
                this.bugId = zrec.recid; // If you modify the bug, please leave the string "Common" in the description.
                this.bugSimpleId = "123456";
                zrec.title = "Common bug for stWeb tests.";
                zrec.id = this.bugSimpleId;
                ztx.commit();

                // and a sprint

                ztx = zs.begin_tx();
                ztx.set_template(t);
                var zrec = ztx.new_record("sprint");
                this.sprintId = zrec.recid; // If you modify the bug, please leave the string "Common" in the description.
                this.sprintName = "dogfood";
                zrec.description = "Common sprint for stWeb tests.";
                zrec.name = this.sprintName;
                ztx.commit();

            }

            {
                var zs = new zingdb(repo, sg.dagnum.WIKI);
                var ztx = zs.begin_tx();
                ztx.set_template({
                    "version": 1,
                    "rectypes": {
                        "page": {
                            "fields": {
                                "title": { "datatype": "string" },
                                "content": { "datatype": "string" }
                            }
                        }
                    }
                });
                var zrec = ztx.new_record("page");
                this.wikiPageId = zrec.recid;
                zrec.title = "Common Wiki Page for stWeb Tests";
                zrec.content = "Initial content.";
                ztx.commit();
            }

            {
                var t;

                eval('t = ' + supTemplate);

                var zs = new zingdb(repo, sg.dagnum.SUP);
                var ztx = zs.begin_tx();
                ztx.set_template(t);

                var zrec = ztx.new_record("item");
                zrec.what = "sample tweet";
                ztx.commit();
            }


            repo.close();
        }

    };


    this._addSearchable = function(title, findable) {
        var repo = sg.open_repo(repInfo.repoName);
        var zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);

	var  ztx = zs.begin_tx();
	var bugid = title.replace(/ +/g, '');

	var zrec = ztx.new_record("item");
	this.searchables[zrec.recid] = findable;
	zrec.title = title;
	zrec.id = bugid;

	ztx.commit();

	repo.close();
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


    this.verify405_postdata = function(path, addJsonHeader, postdata) {
        var url = 'http://localhost:8086' + path;
        var o;

        if (addJsonHeader) {
            o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", "-d", postdata, url);
        }
        else
          o = sg.exec(this.curl, "-D", "headers.txt", "-d", postdata, url);

        if (o.exit_status != 0) {
            testlib.testResult(false, "curl returned nonzero exit status, " + url);
            return;
        }
        var s = sg.file.read("headers.txt");

        return (this.checkStatus("405 Method Not Allowed", s, url));
    };

     this.verify405 = function(path, addJsonHeader) {
        var url = 'http://localhost:8086' + path;
        var o;


        if (addJsonHeader) {
            o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", url);
        }
        else
          o = sg.exec(this.curl, "-D", "headers.txt", url);
        if (o.exit_status != 0) {
            testlib.testResult(false, "curl returned nonzero exit status, " + url);
            return;
        }
        var s = sg.file.read("headers.txt");

        return (this.checkStatus("405 Method Not Allowed", s, url));
    };

    this.verifyNot405 = function(path, addJsonHeader) {
        var url = 'http://localhost:8086' + path;
        var o;
        if (addJsonHeader) {
            o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", url);
        }
        else
          o = sg.exec(this.curl, "-D", "headers.txt", url);
        if (o.exit_status != 0) {
            testlib.testResult(false, "curl returned nonzero exit status, " + url);
            return;
        }
        var s = sg.file.read("headers.txt");
        if (s.search("405 Method Not Allowed") >= 0) {
            testlib.testResult(false, url, "Something other than \"... 405 Method Not Allowed...\"", "\"" + this.firstLineOf(s) + "\"");
            return;
        }
        testlib.testResult(true);
    };

    this.verify404 = function(path, addJsonHeader) {
        var url = 'http://localhost:8086' + path;
        var o;
        if (addJsonHeader) {
            o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", url);
        }
        else
          o = sg.exec(this.curl, "-D", "headers.txt", url);

        if (o.exit_status != 0) {
            testlib.testResult(false, "curl returned nonzero exit status, " + url);
            return;
        }
        var s = sg.file.read("headers.txt");

        return (this.checkStatus("404 Not Found", s, url));
    };

    this.verifyNot404 = function(path, addJsonHeader) {
        var url = 'http://localhost:8086' + path;
        var o;
        if (addJsonHeader) {
            o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", url);
        }
        else
          o = sg.exec(this.curl, "-D", "headers.txt", url);
        if (o.exit_status != 0) {
            testlib.testResult(false, "curl returned nonzero exit status, " + url);
            return;
        }
        var s = sg.file.read("headers.txt");
        if (s.search("404 Not Found") >= 0) {
            testlib.testResult(false, url, "Something other than \"... 404 Not Found...\"", "\"" + this.firstLineOf(s) + "\"");
            return;
        }
        testlib.testResult(true);
    };


    this.firstLineOf = function(st) {
        var t = st.replace(/[\r\n]+/, "\n");
        var lines = t.split("\n");

        if (lines.length == 0)
            return ("");

        return (lines[0]);
    };

    this.test404 = function() {
        var repoName = encodeURI(repInfo.repoName);

        this.verifyNot404("");
        this.verifyNot404("/");
        this.verify404("/nonexistent");
        this.verify404("/nonexistent/");

        this.verifyNot404("/local");
        this.verifyNot404("/local/");
        this.verify404("/local/nonexistent");
        this.verify404("/local/nonexistent/");

        this.verifyNot404("/local/fs");
        this.verifyNot404("/local/fs/");
        this.verify404("/local/fs/nonexistent");
        this.verify404("/local/fs/nonexistent/");

        this.verifyNot404("/local/repo/log");
        this.verifyNot404("/local/repo/log/");

        this.verifyNot404("/local/status");
        this.verifyNot404("/local/status/");
        this.verify404("/local/status/nonexistent");
        this.verify404("/local/status/nonexistent/");

        this.verifyNot404("/repos");
        this.verifyNot404("/repos/");
        this.verify404("/repos/nonexistent");
        this.verify404("/repos/nonexistent/");

        this.verifyNot404("/repos/" + repoName);
        this.verifyNot404("/repos/" + repoName + "/");
        this.verify404("/repos/" + repoName + "/nonexistent");
        this.verify404("/repos/" + repoName + "/nonexistent/");

        this.verify404("/repos/" + repoName + "/changesets/nonexistent");
        this.verify404("/repos/" + repoName + "/changesets/nonexistent/");

        this.verifyNot404("/repos/" + repoName + "/changesets/" + this.changesetId);
        this.verifyNot404("/repos/" + repoName + "/changesets/" + this.changesetId + "/");
        this.verify404("/repos/" + repoName + "/changesets/" + this.changesetId + "/nonexistent");
        this.verify404("/repos/" + repoName + "/changesets/" + this.changesetId + "/nonexistent/");

        this.verifyNot404("/repos/" + repoName + "/changesets/" + this.changesetId + "/comments");
        this.verifyNot404("/repos/" + repoName + "/changesets/" + this.changesetId + "/comments/");
        this.verify404("/repos/" + repoName + "/changesets/" + this.changesetId + "/comments/nonexistent");
        this.verify404("/repos/" + repoName + "/changesets/" + this.changesetId + "/comments/nonexistent/");

        this.verifyNot404("/repos/" + repoName + "/history");
        this.verifyNot404("/local/repo/history/");

        this.verifyNot404("/repos/" + repoName + "/tags");
        this.verifyNot404("/repos/" + repoName + "/tags/");
        this.verifyNot404("/local/repo/tags/");
        this.verifyNot404("/local/repo/tags");

        this.verifyNot404("/repos/" + repoName + "/changesets/" + this.changesetId + "/browsecs");
        this.verifyNot404("/repos/" + repoName + "/changesets/" + this.changesetId + "/browsecs/");
        this.verifyNot404("/repos/" + repoName + "/changesets/" + this.changesetId + "/browsecs/@");
        this.verifyNot404("/repos/" + repoName + "/changesets/" + this.changesetId + "/browsecs/@/");

        this.verifyNot404("/repos/" + repoName + "/changesets/" + this.changesetId + "/browsecs", true);
        this.verifyNot404("/repos/" + repoName + "/changesets/" + this.changesetId + "/browsecs/", true);
        this.verifyNot404("/repos/" + repoName + "/changesets/" + this.changesetId + "/browsecs/@", true);
        this.verifyNot404("/repos/" + repoName + "/changesets/" + this.changesetId + "/browsecs/@/", true);
        this.verify404("/repos/" + repoName + "/changesets/" + this.changesetId + "/browsecs/@/badpath", true);
        this.verify404("/repos/" + repoName + "/changesets/" + this.changesetId + "/browsecs/badgid", true);

        this.verifyNot404("/local/repo/changesets/" + this.changesetId + "/browsecs");
        this.verifyNot404("/local/repo/changesets/" + this.changesetId + "/browsecs/");
        this.verifyNot404("/local/repo/changesets/" + this.changesetId + "/browsecs/@");
        this.verifyNot404("/local/repo/changesets/" + this.changesetId + "/browsecs/@/");

        this.verifyNot404("/repos/" + repoName + "/zip/" + this.changesetId);
        this.verifyNot404("/repos/" + repoName + "/zip/" + this.changesetId + "/");

        this.verifyNot404("/repos/" + repoName + "/zip");
        this.verifyNot404("/repos/" + repoName + "/zip/");
        this.verifyNot404("/local/repo/zip");
        this.verifyNot404("/local/repo/zip/");


        this.verifyNot404("/repos/" + repoName + "/wiki");
        this.verifyNot404("/repos/" + repoName + "/wiki/");
        this.verify404("/repos/" + repoName + "/wiki/nonexistent");
        this.verify404("/repos/" + repoName + "/wiki/nonexistent/");

        this.verifyNot404("/repos/" + repoName + "/wiki/records");
        this.verifyNot404("/repos/" + repoName + "/wiki/records/");
        this.verify404("/repos/" + repoName + "/wiki/records/nonexistent");
        this.verify404("/repos/" + repoName + "/wiki/records/nonexistent/");

        this.verifyNot404("/repos/" + repoName + "/wiki/records/" + this.wikiPageId);
        this.verifyNot404("/repos/" + repoName + "/wiki/records/" + this.wikiPageId + "/");

        this.verifyNot404("/repos/" + repoName + "/wiki/template");
        this.verifyNot404("/repos/" + repoName + "/wiki/template/");
        this.verify404("/repos/" + repoName + "/wiki/template/nonexistent");
        this.verify404("/repos/" + repoName + "/wiki/template/nonexistent/");

        this.verifyNot404("/repos/" + repoName + "/wit");
        this.verifyNot404("/repos/" + repoName + "/wit/");
        this.verify404("/repos/" + repoName + "/wit/nonexistent");
        this.verify404("/repos/" + repoName + "/wit/nonexistent/");

        this.verify404("/repos/" + repoName + "/wit/link");

        this.verifyNot404("/repos/" + repoName + "/wit/records");
        this.verifyNot404("/repos/" + repoName + "/wit/records/");
        this.verify404("/repos/" + repoName + "/wit/records/nonexistent");
        this.verify404("/repos/" + repoName + "/wit/records/nonexistent/");

        this.verifyNot404("/repos/" + repoName + "/wit/records/" + this.bugId);
        this.verifyNot404("/repos/" + repoName + "/wit/records/" + this.bugId + "/");

        this.verifyNot404("/repos/" + repoName + "/wit/template");
        this.verifyNot404("/repos/" + repoName + "/wit/template/");
        this.verify404("/repos/" + repoName + "/wit/template/nonexistent");
        this.verify404("/repos/" + repoName + "/wit/template/nonexistent/");

        this.verifyNot404("/local/repo/");
        this.verifyNot404("/local/repo/");
        this.verify404("/local/repo/nonexistent");
        this.verify404("/local/repo/nonexistent/");
        this.verify404("/local/repo/changesets/nonexistent");
        this.verify404("/local/repo/changesets/nonexistent/");


        this.verifyNot404("/local/repo/wiki");
        this.verifyNot404("/local/repo/wiki/");
        this.verify404("/local/repo/wiki/nonexistent");
        this.verify404("/local/repo/wiki/nonexistent/");

        this.verifyNot404("/local/repo/wiki/records");
        this.verifyNot404("/local/repo/wiki/records/");
        this.verify404("/local/repo/wiki/records/nonexistent");
        this.verify404("/local/repo/wiki/records/nonexistent/");

        this.verifyNot404("/local/repo/wiki/records/" + this.wikiPageId);
        this.verifyNot404("/local/repo/wiki/records/" + this.wikiPageId + "/");

        this.verifyNot404("/local/repo/wiki/template");
        this.verifyNot404("/local/repo/wiki/template/");
        this.verify404("/local/repo/wiki/template/nonexistent");
        this.verify404("/local/repo/wiki/template/nonexistent/");

        this.verifyNot404("/local/repo/wit");
        this.verifyNot404("/local/repo/wit/");
        this.verify404("/local/repo/wit/nonexistent");
        this.verify404("/local/repo/wit/nonexistent/");

        this.verifyNot404("/local/repo/wit/records");
        this.verifyNot404("/local/repo/wit/records/");
        this.verify404("/local/repo/wit/records/nonexistent");
        this.verify404("/local/repo/wit/records/nonexistent/");

        this.verifyNot404("/local/repo/wit/records/" + this.bugId);
        this.verifyNot404("/local/repo/wit/records/" + this.bugId + "/");

        this.verifyNot404("/local/repo/wit/template");
        this.verifyNot404("/local/repo/wit/template/");
        this.verify404("/local/repo/wit/template/nonexistent");
        this.verify404("/local/repo/wit/template/nonexistent/");

        this.verifyNot404("/tests");
        this.verifyNot404("/tests/");
        this.verify404("/tests/nonexistent");
        this.verify404("/tests/nonexistent/");

        this.verifyNot404("/static");
        this.verifyNot404("/static/");
        this.verify404("/static/nonexistent");
        this.verify404("/static/nonexistent/");

        this.verifyNot404("/static/sg.css");
        this.verify404("/static/sg.css/nonexistent");

        this.verifyNot404("/repos/" + repoName + "/wit/records");
        this.verifyNot404("/repos/" + repoName + "/wit/records/new");
        this.verifyNot404("/local/repo/wit/records");
        this.verifyNot404("/local/repo/wit/records/new");

        this.verify404("/repos/" + repoName + "/wiki/records/" + this.wikiPageId + "/nonexistentlink");
        this.verify404("/repos/" + repoName + "/wiki/records/" + this.wikiPageId + "/nonexistentlink/");
        this.verify404("/local/repo/wit/records/" + this.bugId + "/nonexistentlink");
        this.verify404("/local/repo/wit/records/" + this.bugId + "/nonexistentlink/");
    };

    this.test405 = function() {
        var repoName = encodeURI(repInfo.repoName);

       this.verifyNot405("/repos/" + repoName + "/wit/records");
       this.verifyNot405("/repos/" + repoName + "/wit/records/new");

       this.verify405_postdata("/repos/" + repoName + "/changesets/" + this.changesetId + "/browsecs/@", true, "baddata");
       this.verify405_postdata("/repos/" + repoName + "/changesets/" + this.changesetId, true, "baddata");

    };

    this.checkHeader = function(headerName, expected, headerBlock, desc) {
        var s = headerBlock.replace(/[\r\n]+/g, "\n");
        var lines = s.split("\n");
        var headers = {};

        for (var i in lines) {
            var l = lines[i];
            var parts = l.split(": ");

            if (parts.length == 2) {
                headers[parts[0]] = parts[1];
            }
        }

        var got = headers[headerName];
        var msg = "header " + headerName + ": expected '" + expected + "', " +
	    "got '" + got + "'";

	if (!! desc)
	    msg = "(" + desc + ") " + msg;

        return (testlib.testResult(expected == got, msg));
    };

    this.testContentType = function() {
        var o1 = sg.exec(this.curl,
            "-D", "headers1.txt",
            "http://localhost:8086/local/status/");
        if (!testlib.testResult(o1.exit_status == 0)) return;
        var h1 = sg.file.read("headers1.txt");


        if (!this.checkStatus("200 OK", h1, "error status on call 1"))
            return;

        this.checkHeader("Content-Type", "application/xhtml+xml;charset=UTF-8", h1);

        var o2 = sg.exec(this.curl,
            "-D", "headers2.txt",
            "-H", "Accept: application/json",
            "http://localhost:8086/local/status/");
        if (!testlib.testResult(o2.exit_status == 0)) return;
        var h2 = sg.file.read("headers2.txt");
        if (!this.checkStatus("200 OK", h2, "error status on call 2"))
            return;

        this.checkHeader("Content-Type", "application/json;charset=UTF-8", h2);
    };

    this.testContentTypeByBrowser = function() {
	var browsers = {
	    "IE 7" : {
		"useragent" : "Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 6.0)",
		"contenttype" : "text/html"
	    },

	    "IE 8" : {
		"useragent" : "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.0; Trident/4.0)",
		"contenttype" : "text/html"
	    },

	    "IE 8 Compat" : {
		"useragent" : "Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 6.0; Trident/4.0)",
		"contenttype" : "text/html"
	    },

	    "IE 8 Win 7" : {
		"useragent" : "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; Trident/4.0)",
		"contenttype" : "text/html"
	    },

	    "IE 8 64 bit" : {
		"useragent" : "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.0; Win64; x64; Trident/4.0)",
		"contenttype" : "text/html"
	    },

	    "IE 9" : {
		"useragent" : "Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; Trident/5.0)",
		"contenttype" : "application/xhtml+xml"
	    },

	    "Firefox" : {
		"useragent" : "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9.2.3) Gecko/20100401 Firefox/3.6.3",
		"contenttype" : "application/xhtml+xml"
	    },

	    "Chrome" : {
		"useragent" : "Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US) AppleWebKit/533.1 (KHTML, like Gecko) Chrome/5.0.322.2 Safari/533.1",
		"contenttype" : "application/xhtml+xml"
	    },

	    "Safari Win" : {
		"useragent" : "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US) AppleWebKit/531.21.8 (KHTML, like Gecko) Version/4.0.4 Safari/531.21.10",
		"contenttype" : "application/xhtml+xml"
	    },

	    "Safari" : {
		"useragent" : "Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_6_2; en-us) AppleWebKit/531.21.8 (KHTML, like Gecko) Version/4.0.4 Safari/531.21.10",
		"contenttype" : "application/xhtml+xml"
	    },

	    "Opera" : {
		"useragent" : "Opera/9.80 (Windows NT 5.1; U; en) Presto/2.5.22 Version/10.50",
		"contenttype" : "application/xhtml+xml"
	    },
	};

	for ( var desc in browsers )
	{
	    var expected = browsers[desc]["contenttype"];
	    var ua = browsers[desc]["useragent"];

	    var o = sg.exec(this.curl, "-D", "headers.txt",
		"-H", "User-Agent: " + ua,
		"http://localhost:8086/local/");

	    if (! testlib.testResult(o.exit_status == 0, "curl as " + desc))
		continue;

	    var headers = sg.file.read("headers.txt");

	    if (! this.checkStatus("200 OK", sg.file.read("headers.txt"), "curl as " + desc))
		continue;

	    this.checkHeader("Content-Type", expected + ";charset=UTF-8", headers, desc);
	}
    }

    this.testLocalRedirect = function() {
        var o = sg.exec(this.curl,
			"-D", "headers.txt",
			"http://localhost:8086/");

        if (!testlib.testResult(o.exit_status == 0, "failed first curl"))
            return;

        var h = sg.file.read("headers.txt");

        if (this.checkStatus("301 Moved Permanently", h, "status of first curl")) {
            this.checkHeader("Location", "http://localhost:8086/home", h);
        }

        o = sg.exec(this.curl,
			"-D", "headers.txt",
			"http://localhost:8086");

        if (!testlib.testResult(o.exit_status == 0, "failed second curl"))
            return;

        h = sg.file.read("headers.txt");

        if (this.checkStatus("301 Moved Permanently", h, "status of second curl")) {
            this.checkHeader("Location", "http://localhost:8086/home", h);
        }
    };

    this.testRevert = function() {
        var filename = "testRevert.txt";
        var o;

        sg.file.write(filename, "testRevert");


        // Sanity check: make sure the file is "Found" before we pend the add
        {
            o = sg.exec("vv", "status");
            if (!testlib.testResult(o.exit_status == 0, "failed vv")) return;
            var s = o.stdout;
            if (!testlib.testResult(0 <= s.search("Found:") && s.search("Found:") < s.search(filename))) return;
        }

        sg.pending_tree.add("testRevert.txt");

        // Sanity check: make sure the file is "Added" after we pend the add
        {
            o = sg.exec("vv", "status");
            if (!testlib.testResult(o.exit_status == 0, "failed vv 2")) return;
            var s = o.stdout;
            if (!testlib.testResult(
                0 <= s.search("Added:") && s.search("Added:") < s.search(filename)
                && (s.search(filename) < s.search("Found:") || s.search("Found:") < 0)
            )) return;
        }

        var username = "testRevert <test@sourcegear.com>";
        sg.file.write("testRevert.json",
            "{\"revert\":[\"@/" + filename + "\"]}");
        o = sg.exec(this.curl,
            "-H", "From: " + username,
            "-T", "testRevert.json",
            "-D", "headers.txt",
            "http://localhost:8086/local");
        if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        // Make sure the file is "Found" again
        {
            o = sg.exec("vv", "status");
            if (!testlib.testResult(o.exit_status == 0, "failed vv 3")) return;
            var s = o.stdout;
            if (!testlib.testResult(0 <= s.search("Found:") && s.search("Found:") < s.search(filename))) return;
        }
    }

    this.testCommit = function() {
        var filename = "testCommit.txt";
        var o;

        sg.file.write(filename, "testCommit");
        sg.pending_tree.add("testCommit.txt");

        // Sanity check: make sure the file is in the pending changes
        o = sg.exec("vv", "status");
        if (!testlib.testResult(o.exit_status == 0, "failed vv status")) return;
        if (!testlib.testResult(o.stdout.search(filename) > 0, filename + " not found in status")) return;

        var username = "testCommit <test@sourcegear.com>";
        sg.file.write("testCommit.json",
            "{\"commit\":[\"@/" + filename + "\"]}");
        o = sg.exec(this.curl,
            "-H", "From: " + username,
            "-T", "testCommit.json",
            "-D", "headers.txt",
            "http://localhost:8086/local");
        if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;
        var changesetId = o.stdout;

        //todo: this should work on all platforms!
        //// Make sure the file is no longer in the pending changes
        //o = sg.exec("vv", "status");
        //if(!testlib.testResult(o.exit_status==0))return;
        //if(!testlib.testResult(o.stdout.search(filename)<0))return;

        // Make sure the changeset exists in history
        o = sg.exec("vv", "history");
        if (!testlib.testResult(o.exit_status == 0, "failed vv history")) return;
        testlib.testResult(o.stdout.search(changesetId) >= 0, "changesetId not found in history");
    }

    this.testListrepos = function() {
        var o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", "http://localhost:8086/repos/");
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

		/*

		Because the test suite adds garbage descriptors which the web API
		ignores but sg.list_descriptors() doesn't,  these don't work anymore.

        // Make sure all the existing repos are in the fetched list...
        var fetched_list = eval("(" + o.stdout + ")");

        var numRepos = 0;
        for (var x in sg.list_descriptors()) {
            testlib.isNotNull(fetched_list[x], "repository " + x + " should not be null");
            numRepos++;
        }

        var numFetched = 0;

        for (i in fetched_list) {
          numFetched++;
        }

        //... and that no nonexsting repos or dupes are in it.
        testlib.testResult(numFetched == numRepos);
        */
    }

    this.testAddComment = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/changesets/" + this.changesetId + "/comments/";
        var username = "testAddComment <test@sourcegear.com>";
        var comment = "This is a comment from the test testAddComment.";
        var o = sg.exec(this.curl,
            "-H", "From: " + username,
            "-d", comment,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), url)) return;

        // Test that our comment was actually added by finding it in the output of "vv log":
        o = sg.exec("vv", "history");
        if (!testlib.testResult(o.exit_status == 0, "failed vv")) return;
        testlib.testResult(o.stdout.search(comment) >= 0, "comment not found");
        
    }

    this.testAddCommentAnonymously = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/changesets/" + this.changesetId + "/comments/";
        var comment = "This is a comment from the test testAddCommentAnonymously.";
        var o = sg.exec(this.curl,
            "-d", comment,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;

        if (!this.checkStatus("401 Unauthorized", sg.file.read("headers.txt"), url))
            return;

        // Verify that our comment was not added by searching for it in the output of "vv log":
        o = sg.exec("vv", "log");
        if (!testlib.testResult(o.exit_status == 0, "failed vv log")) return;
        testlib.testResult(o.stdout.search(comment) < 0, "failed to find comment");
    }

    this.testFetchWorkItemTemplate = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/template/";
        var o = sg.exec(this.curl,
	    "-H", "Accept: application/json",
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        var template = eval("(" + o.stdout + ")");
        testlib.testResult(template.rectypes.item.fields.title.datatype == "string");
    }

    this.testFetchWorkItem = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/" + this.bugId;
        var o = sg.exec(this.curl,
	    "-H", "Accept: application/json",
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        var bug = eval("(" + o.stdout + ")");
        testlib.testResult(bug.title.search("Common") >= 0);
    }

    this.testAddWorkItem = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/";
        var username = repInfo.userName;
        var ourDesc = "This bug was added by the test testAddWorkItem.";
        var bugJson = "{"
            + "\"rectype\":\"item\","
	    + "\"id\":\"testAddWorkItem\","
            + "\"title\":\"" + ourDesc + "\"}";
        var o = sg.exec(this.curl,
            "-H", "From: " + username,
            "-d", bugJson,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "error posting work item")) return;

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "posting")) return;

        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/";
        var o = sg.exec(this.curl,
            "-H", "Accept: application/json",
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "retrieving records")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "retrieving records")) return;

        var records = false;

        eval('records = ' + o.stdout);

        if (!testlib.testResult((!!records) && (records.length > 0), "no records found"))
            return;

        var found = false;

        for (var i = 0; i < records.length; ++i) {
            if (records[i].title == ourDesc) {
                found = records[i];
                break;
            }
        }

        testlib.testResult(!!found, "no record found with matching description");

        // TODO: ... verify that we can find our username associated with the adding of this bug!!?!
        // paulr - as it stands, with this template & current web service
        // behavior, we don't get the whoami info back
    }

    this.testAddWorkItemAnonymously = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/";
        var bugJson = "{"
            + "\"rectype\":\"item\","
	    + "\"id\":\"testAWIA\","
            + "\"title\":\"This bug was added by the test testAddWorkItemAnnonymously.\"}";
        var o = sg.exec(this.curl,
            "-d", bugJson,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!testlib.testResult(sg.file.read("headers.txt").search("401 Unauthorized") >= 0)) return;
    }

    this.testAddWorkItemWithoutRectype = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/";
        var bugJson = "{"
	    + "\"id\":\"testAWIWR\","
            + "\"title\":\"This bug was added by the test testAddWorkItemWithoutRectype.\"}";
        var o = sg.exec(this.curl,
            "-H", "From: testAddWorkItemWithoutRectype <test@sourcegear.com>",
            "-d", bugJson,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!testlib.testResult(sg.file.read("headers.txt").search("400 Bad Request") >= 0)){
            print(sg.file.read("headers.txt"));
            print(o.stdout);
            return;
        }

        // Response message's body should say something about needing a rectype.
        testlib.testResult(o.stdout.search("rectype") >= 0);
    }

    this.testAddInvalidJsonWorkItem = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/";
        var bugJson = "{"
            + "\"rectype\":\"item\","
	    + "\"id\":\"testAIJWI\","
            + "\"title\":\"This is not \"valid\" JSON.\"}";
        var o = sg.exec(this.curl,
            "-H", "From: testAddInvalidJsonWorkItem <test@sourcegear.com>",
            "-d", bugJson,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!testlib.testResult(sg.file.read("headers.txt").search("400 Bad Request") >= 0)) return;

        // Response message's body should say something about invalid Json.
        testlib.testResult(o.stdout.search("not valid JSON") >= 0);
    }

    this.testUpdateWorkItem = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/" + this.bugId;
        var username = "testUpdateWorkItem <test@sourcegear.com>";
        sg.file.write("tempUpdateWorkItem.json",
            "{\"title\":\"Common stWeb bug updated by testUpdateWorkItem.\"}");
        var o = sg.exec(this.curl,
            "-H", "From: " + username,
            "-T", "tempUpdateWorkItem.json",
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        //todo: Verify that the work item was actually added
        // ... and that we can find our username associated with the updating of this bug!!?!
    }

    this.testUpdateWorkItemAnonymously = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/" + this.bugId;
        sg.file.write("tempUpdateWorkItemAnonymously.json",
            "{\"title\":\"Common stWeb bug updated by testUpdateWorkItemAnonymously!!\"}");
        var o = sg.exec(this.curl,
            "-T", "tempUpdateWorkItemAnonymously.json",
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!testlib.testResult(sg.file.read("headers.txt").search("401 Unauthorized") >= 0)) return;
    }

    this.testUpdateWorkItemInvalidJson = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/" + this.bugId;
        sg.file.write("tempUpdateWorkItemInvalidJson.json",
            "{\"title\":\"Common stWeb bug updated by \"testUpdateWorkItemInvalidJson\"!!\"}");
        var o = sg.exec(this.curl,
            "-H", "From: testUpdateWorkItemInvalidJson <test@sourcegear.com>",
            "-T", "tempUpdateWorkItemInvalidJson.json",
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!testlib.testResult(sg.file.read("headers.txt").search("400 Bad Request") >= 0)) return;

        // Response message's body should say something about invalid Json.
        testlib.testResult(o.stdout.search("not valid JSON") >= 0);
    }

    this.testDeleteWorkItem = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/";
        var username = repInfo.userName;
        var bugJson = "{"
            + "\"rectype\":\"item\","
	    + "\"id\":\"12345678\","
            + "\"title\":\"This bug was added by the test testDeleteWorkItem.\"}";
        var o = sg.exec(this.curl,
            "-H", "From: " + username,
            "-d", bugJson,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        var bugId = o.stdout;
        url = url + bugId;

        // Try to delete the work item anonymously and verify that it fails with a 401 error:
        var o = sg.exec(this.curl,
            "-X", "DELETE",
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!testlib.testResult(sg.file.read("headers.txt").search("401 Unauthorized") >= 0)) return;

        // Now delete it for real and verify 200 OK response:
        var o = sg.exec(this.curl,
            "-X", "DELETE",
            "-H", "From: " + username,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;
    }

    this.assocWIwithCS = function(bugId, csId, testName) {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/" + bugId + "/changesets";

        var tname = "tempLinkWorkItem.json";
        var username = repInfo.userName;

        sg.file.write(tname,
		      '{"commit":"' + csId + '"}');

        var o = sg.exec(this.curl,
            "-H", "From: " + username,
            "-T", tname,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed to link " + bugId + " to " + csId))
            return (false);

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "linking " + bugId + " to " + csId))
            return (false);

        return (true);
    }

    this.testWorkItemChangeSetAssoc = function() {
        if (!this.assocWIwithCS(this.bugId, this.changesetId, "testWorkItemChangeSetAssoc"))
            return;

        var foundIt = false;
        var repo = sg.open_repo(repInfo.repoName);
        var zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);
        var ztx = zs.begin_tx();

        var item = ztx.open_record(this.bugId);

        if (testlib.testResult(!!item, "bug not found")) {
            var a = item.changesets.to_array();

            if (testlib.testResult(a.length > 0, "no changesets found")) {
                for (var i in a) {
                    var lid = a[i];

                    var ln = ztx.open_record(lid);

                    if (ln.commit == this.changesetId) {
                        foundIt = true;
                        break;
                    }
                }

                testlib.ok(foundIt, "no changeset matched " + this.changesetId);
            }
        }

        ztx.abort();
        repo.close();
    }

    this.testWorkItemChangeSetAssocMultiple = function() {
        if ((!this.assocWIwithCS(this.bugId, this.changesetId, "testWorkItemChangeSetAssocMultiple")) ||
	    (!this.assocWIwithCS(this.bugId, this.changeset2Id, "testWorkItemChangeSetAssocMultiple"))
	   )
            return;

        var found1 = false;
        var found2 = false;
        var repo = sg.open_repo(repInfo.repoName);
        var zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);
        var ztx = zs.begin_tx();

        var item = ztx.open_record(this.bugId);

        if (testlib.testResult(!!item, "bug not found")) {
            var a = item.changesets.to_array();

            if (testlib.testResult(a.length > 0, "no changesets found")) {
                for (var i in a) {
                    var lid = a[i];

                    var ln = ztx.open_record(lid);

                    if (ln.commit == this.changesetId) {
                        found1 = true;
                    }
                    else if (ln.commit == this.changeset2Id) {
                        found2 = true;
                    }
                }

                testlib.ok(found1, "no changeset matched 1st " + this.changesetId);
                testlib.ok(found2, "no changeset matched 2nd " + this.changeset2Id);
            }
        }

        ztx.abort();
        repo.close();
    }

    this.testRetrieveAssociations = function() {
        if ((!this.assocWIwithCS(this.bugId, this.changesetId, "testRetrieveAssociations")) ||
	    (!this.assocWIwithCS(this.bugId, this.changeset2Id, "testRetrieveAssociations"))
	   )
            return;

        var url = "http://localhost:8086/repos/" + repInfo.repoName +
	    "/wit/records/" + this.bugId + "/changesets";

        var username = repInfo.userName;

        var o = sg.exec(this.curl,
            "-H", "From: " + username,
            "-D", "headers.txt",
            encodeURI(url));

        if (!testlib.testResult(o.exit_status == 0, "failed calling for links")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "calling for links")) return;


        var s = o.stdout;
        var list = false;

        eval('list = ' + s);

        var found1 = false;
        var found2 = false;

        for (var i in list) {
            var ln = list[i];

            if (ln.commit == this.changesetId) {
                found1 = true;
            }
            else if (ln.commit == this.changeset2Id) {
                found2 = true;
            }
        }

        testlib.ok(found1, "no changeset matched 1st " + this.changesetId);
        testlib.ok(found2, "no changeset matched 2nd " + this.changeset2Id);
    }


    this.testRemoveAssociation = function() {

        var testName = "testRemoveAssociation";


        var repo = sg.open_repo(repInfo.repoName);
        var zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);
        var ztx = zs.begin_tx();

        var item = ztx.open_record(this.bugId);
        var css = item.changesets.to_array();

        for (var i = 0; i < css.length; ++i) {
            var rid = css[i];
            ztx.delete_record(rid);
        }

        ztx.commit();

        if ((!this.assocWIwithCS(this.bugId, this.changesetId, "testWorkItemChangeSetAssocMultiple")) ||
	    (!this.assocWIwithCS(this.bugId, this.changeset2Id, "testWorkItemChangeSetAssocMultiple"))
	   )
            return;

        var linkrecid = false;

        ztx = zs.begin_tx();

        var item = ztx.open_record(this.bugId);

        if (testlib.testResult(!!item, "bug not found")) {
            var a = item.changesets.to_array();

            if (testlib.testResult(a.length > 0, "no changesets found")) {
                for (var i in a) {
                    var lid = a[i];

                    var ln = ztx.open_record(lid);

                    if (ln.commit == this.changesetId) {
                        linkrecid = lid;
                        break;
                    }
                }
            }
        }

        ztx.abort();


        if (!testlib.testResult(!!linkrecid, "no link record found"))
            return;



        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/" + linkrecid;
        var tname = "tempLinkWorkItem.json";
        var username = repInfo.userName;


        var o = sg.exec(this.curl,
			"-H", "From: " + username,
   			"-X", "DELETE",
			"-D", "headers.txt",
			encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed to remove link (" + this.bugId + " to " + this.changesetId + ")"))
            return;

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "linking " + this.bugId + " to " + this.changesetId)) return;


        var found1 = false;
        var found2 = false;


        ztx = zs.begin_tx();
        item = ztx.open_record(this.bugId);


        if (testlib.testResult(!!item, "bug not found")) {
            var a = item.changesets.to_array();


            if (testlib.testResult(a.length > 0, "no changesets found")) {

                for (var i in a) {
                    var lid = a[i];

                    var ln = ztx.open_record(lid);


                    if (ln.commit == this.changesetId) {
                        found1 = true;
                    }
                    else if (ln.commit == this.changeset2Id) {
                        found2 = true;
                    }
                }


                testlib.ok(found2, "no changeset matched 2nd " + this.changeset2Id);
                testlib.ok(!found1, "unexpected changeset matched 1st " + this.changesetId);
            }
        }


        ztx.abort();


        repo.close();

    };


    // Wiki system and Work Items system use common code.
    // The only difference is in a "SG_uint32 dagnum" parameter being passed around.
    // So all the wiki stuff has really been tested already by the Work Items tests.
    // But we'll do a quick sanity check or two.
    this.testWiki = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wiki/records/";
        var pageJson = "{"
            + "\"rectype\":\"page\","
            + "\"title\":\"Page Added By The testWiki Test.\","
            + "\"content\":\"Hooray!\"}";
        var o = sg.exec(this.curl,
            "-H", "From: " + repInfo.userName,
            "-d", pageJson,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0)) return;

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;
    }

    this.testUploadFile = function() {
        createRandomFileOnDisk("src.txt", 3 * 1024 * 1024);
        var o = sg.exec(this.curl,
            "-H", "From: testUploadFile <testuploadfile@sourcegear.com>",
            "--data-binary", "@src.txt",
            "-D", "headers.txt",
            "http://localhost:8086/tests/files/dest.txt");
        if (!testlib.testResult(o.exit_status == 0)) return;

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        testlib.testResult(compareFiles("src.txt", "dest.txt"));
    }


    this.testBrowsefsPassFail = function() {

        var root = "bfstest";

        if (sg.fs.exists(root)) {
            sg.fs.rmdir_recursive(root);
        }

        sg.fs.mkdir(root);
        sg.fs.mkdir(root + "/subexists");

        var existUrl = "http://localhost:8086/local/fs/" + root + "/subexists";
        var nonExistBase = "/local/fs/" + root + "/dne";
        var nonExistUrl = "http://localhost:8086" + nonExistBase;

        var o = sg.exec(this.curl, "-D", "headers.txt", "-H", 'Accept: application/json', existUrl);
        if (o.exit_status != 0) {
            testlib.testResult(false, "curl returned nonzero exit status, " + url);
            return;
        }

        var s = sg.file.read("headers.txt");

        this.checkStatus("200 OK", sg.file.read("headers.txt"), "fail on existing dir: " + existUrl);

        this.verify404(nonExistBase);

        sg.fs.rmdir_recursive(root);
    }

    this.testBrowsefsListFiles = function() {

        var root = "bfstest";

        if (sg.fs.exists(root)) {
            sg.fs.rmdir_recursive(root);
        }

        sg.fs.mkdir(root);
        sg.fs.mkdir(root + "/subwithfiles");
        sg.file.write(root + "/subwithfiles/t1.txt", "one");
        sg.file.write(root + "/subwithfiles/t2.txt", "two");

        var url = "http://localhost:8086/local/fs/" + root + "/subwithfiles";

        var o = sg.exec(this.curl, "-D", "headers.txt", '-H', 'Accept: application/json', url);

        if (o.exit_status != 0) {
            testlib.testResult(false, "curl returned nonzero exit status, " + url);
            return;
        }
        var s = sg.file.read("headers.txt");
        this.checkStatus("200 OK", sg.file.read("headers.txt"), "fail on existing dir: " + url);

        var outstr = o.stdout.trim();

        var flist;

        eval("flist = " + outstr + ";");

        testlib.testResult(!!flist, "file list defined");

        if (!!flist) {
            testlib.equal(3, flist.files.length, "number of files returned");

            flist.files.sort(
                function(a, b) {
                    if (a.name < b.name)
                        return (-1);
                    else if (a.name > b.name)
                        return (1);
                    else
                        return (0);
                }
            );

            testlib.equal("..", flist.files[0].name, "first file name");
            testlib.ok(flist.files[0].isdir, "first file should be dir");
            testlib.equal("t1.txt", flist.files[1].name, "second file name");
            testlib.ok(!flist.files[1].isdir, "second file should not be dir");
            testlib.equal("t2.txt", flist.files[2].name, "third file name");
            testlib.ok(!flist.files[2].isdir, "third file should not be dir");
        }

        sg.fs.rmdir_recursive(root);
    };

    this.testAddWiComment = function() {
        var username = "testAddWiComment <test@example.com>";
        var posturl = "http://localhost:8086/repos/" + repInfo.repoName +
	    "/wit/records/" + this.bugId + "/comments";
        var commentText = "comment from testAddWiComment";

        var commentdata = {
            "text": commentText
        };

        sg.file.write("testAddWiComment.json", sg.to_json(commentdata));

        var o = sg.exec(this.curl,
			"-H", "From: " + username,
			"-T", "testAddWiComment.json",
			"-D", "headers.txt",
			encodeURI(posturl)
		       );

        if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        var repo = sg.open_repo(repInfo.repoName);
        var zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);
        var ztx = zs.begin_tx();
        var item = ztx.open_record(this.bugId);
        var cmts = item.comments.to_array();
        ztx.abort();
        repo.close();

        if (!testlib.testResult(cmts.length == 1, "should be 1 comment"))
            return;

        o = sg.exec(this.curl,
		    "-D", "headers.txt",
		    encodeURI(posturl)
		   );

        if (!testlib.testResult(o.exit_status == 0, "failed curl on get")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "get " + posturl)) return;

        var d = false;
        eval('d = ' + o.stdout);

        if (!testlib.testResult(!!d, "no data returned"))
            return;

        var found = false;

        for (var i in d) {
            if (d[i].text == commentText) {
                found = true;
                break;
            }
        }

        testlib.testResult(found, "comment was not found via web service");
    };


    this.testCommentsInActivity = function() {
        var username = "testCommentsInActivity <test@example.com>";
        var posturl = "http://localhost:8086/repos/" + repInfo.repoName +
	    "/wit/records/" + this.bugId + "/comments";
        var commentText = "testCommentsInActivity";
        var listUrl = "http://localhost:8086/repos/" +
	    repInfo.repoName + "/activity/";

        var commentdata = {
            "text": commentText
        };

        sg.file.write("testCommentsInActivity.json", sg.to_json(commentdata));

        var o = sg.exec(this.curl,
			"-H", "From: " + username,
			"-T", "testCommentsInActivity.json",
                        "-H", "Accept: application/json",
			"-D", "headers.txt",
			encodeURI(posturl)
		       );

        if ((!testlib.assertSuccessfulExec(o)) ||
            (!this.checkStatus("200 OK", sg.file.read("headers.txt"))))
            return;

        var tweets = this.getTweets(listUrl);
        if (!testlib.testResult(!!tweets, "no tweets found"))
            return;

        var found = false;

        for (var i = 0; i < tweets.length; ++i) {
            if (tweets[i].what.search(commentText) >= 0) {
                found = true;
                break;
            }
        }

        testlib.testResult(found, "work item comment in activity stream");
    };


    this.getTweets = function(url, ims) {

        var o = false;

        if (ims) {
            o = sg.exec(this.curl, "-D", "headers.txt",
                        "-H", "Accept: application/json",
			"-H", "If-Modified-Since: " + ims,
			encodeURI(url));
        }
        else
            o = sg.exec(this.curl, "-D", "headers.txt",
                        "-H", "Accept: application/json",
			encodeURI(url));

        if (!testlib.testResult(o.exit_status == 0, "bad status: " + o.exit_status)) return;

        var headers = sg.file.read("headers.txt");

        if (headers.search("304 Not Modified") >= 0) {
            var tweets = [];
            return (tweets);
        }

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), url))
            return;

        var tweets = false;

        eval('tweets = ' + o.stdout + ";");

        return (tweets);
    }

    this.testActivity = function() {
        var repoName = repInfo.repoName;
        var postUrl = "http://localhost:8086/repos/" +
	    repoName + "/activity/records";
        var listUrl = "http://localhost:8086/repos/" +
	    repoName + "/activity/";
        var username = repInfo.userName;

        var nonce = (new Date()).getTime();

        var tweetJSON = "{" +
	    "\"rectype\":\"item\"," +
	    "\"what\":\"activity from testActivity() - " + nonce + "\"" +
	    "}";

        var o = sg.exec(this.curl, "-d", tweetJSON, "-D", "headers.txt",
            "-H", "From: " + username,
	    encodeURI(postUrl));

        if (!testlib.testResult(o.exit_status == 0, "bad status: " + o.exit_status)) return;

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt")))
            return;

        var tweets = this.getTweets(listUrl);

        if (!testlib.testResult(!!tweets, "no tweets found"))
            return;

        var foundNonce = false;

        for (var i = 0; i < tweets.length; ++i) {
            if (tweets[i].what.search(nonce) >= 0) {
                foundNonce = true;
                break;
            }
        }

        testlib.testResult(foundNonce, "nonce not found in activity stream");
    }

    this.pad2 = function(n) {
        if (n < 10)
            return ("0" + n)

        return (n);
    }


    this.rfc850 = function(dt) {
        // Wdy, DD-Mon-YY HH:MM:SS TIMEZONE

        var days = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
        var months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
	    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

        return (days[dt.getUTCDay()] + ", " +
	       this.pad2(dt.getUTCDate()) + " " + months[dt.getUTCMonth()] + " " +
	       dt.getUTCFullYear() + " " +
	       this.pad2(dt.getUTCHours()) + ":" +
	       this.pad2(dt.getUTCMinutes()) + ":" +
	       this.pad2(dt.getUTCSeconds()) + " GMT");
    }

    this.testActivitySince = function() {
        var repoName = repInfo.repoName;
        var listUrl = "http://localhost:8086/repos/" +
	    repoName + "/activity/";
        var username = repInfo.userName;
        var postUrl = "http://localhost:8086/repos/" +
	    repoName + "/activity/records";

        sleep(1000);  // important, since the time-based testing tosses milliseconds
        var before = new Date();
        var nonce = before.getTime();
        sleep(1000);  // important, since the time-based testing tosses milliseconds

        var tweetJSON = "{" +
	    "\"rectype\":\"item\"," +
	    "\"what\":\"activity from testActivitySince() - " + nonce + "\"" +
	    "}";

        var o = sg.exec(this.curl, "-d", tweetJSON, "-D", "headers.txt",
            "-H", "From: " + username,
	    encodeURI(postUrl));

        if (!testlib.assertSuccessfulExec(o))
            return;

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), postUrl))
            return;

        var tweets = this.getTweets(listUrl);

        if (!testlib.testResult(!!tweets, "no tweets first time"))
            return;

        var beforeCount = tweets.length;

        sg.file.write("stWeb-before.txt", "stWeb");
        sg.pending_tree.add("stWeb-before.txt");
        this.changesetId = sg.pending_tree.commit();

        tweets = this.getTweets(listUrl);
        if (!testlib.testResult(!!tweets, "no tweets second time"))
            return;

        var afterCount = tweets.length;

        testlib.equal(beforeCount + 1, afterCount, "tweet count after update");

        sleep(1000);  // important, since the time-based testing tosses milliseconds
        var after = new Date();

        var beforeIMS = this.rfc850(before);
        var afterIMS = this.rfc850(after);

        tweets = this.getTweets(listUrl, afterIMS);

        if (testlib.testResult(!!tweets, "no tweets afterIMS"))
            testlib.equal(0, tweets.length, "no tweets since now");

        tweets = this.getTweets(listUrl, beforeIMS);

        if (testlib.testResult(!!tweets, "no tweets beforeIMS"))
            testlib.equal(2, tweets.length, "tweets since before newest");
    }


    this.testHistoryJSON = function() {

        var repoName = encodeURI(repInfo.repoName);

        commitModFileCLC("testHistoryJSON", "stamp1");

        var o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", "http://localhost:8086/repos/" + repoName + "/history/");
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        //verify JSON results
        var res = eval(o.stdout);
        var cs = res[0];
        var count = res.length;

        //THIS IS WHAT THE HISTORY WEB PAGE EXPECTS FROM THE JSON
        //IF THIS FAILS, THAT MEANS THE HISTORY WEB PAGE IS BROKEN, DON'T JUST CHANGE THE TEST
        //FIX THE WEB PAGE OR TELL MJ
        var csid = cs.changeset_id;
        testlib.ok(csid != null, "changeset_id should not be null");
        testlib.equal(cs.stamps[0], "stamp1", "stamp should be stamp1");
        testlib.isNotNull(cs.parents[0], "parents should not be null");
        testlib.isNotNull(cs.audits[0].who, "audit who should not be null");
        testlib.isNotNull(cs.audits[0].when, "audit when should not be null");
        var uid = cs.audits[0].who;

        var comments = cs.comments;

        testlib.isNotNull(comments[0].text, "comments text should not be null");
        o = sg.exec(vv, "tag", "--rev=" + csid, "tag1");
        o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", "http://localhost:8086/repos/" + repoName + "/history/");
        res = eval(o.stdout);
        cs = res[0];

        testlib.equal(cs.tags[0], "tag1", "tag should be tag1");

        var date = "2020-11-11";

        var url = "http://localhost:8086/repos/" + repoName + "/history?max=2&stamp=stamp1&from=2010-04-20&user="+ uid + "&to=" + date;
        print (url);
        o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", url);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;
        print (o.stdout);
        res = eval(o.stdout);
        testlib.equal(1,  res.length, "length should be 1");
        testlib.equal(repInfo.userName,  res[0].audits[0].email, "username");
        testlib.equal(res[0].stamps[0], "stamp1", "stamp should be stamp1");

        var url = "http://localhost:8086/repos/" + repoName + "/history?max=0";
        o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", url);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        res = eval(o.stdout);
        testlib.equal(count,  res.length, "all items should be returend");

        //test .html page
        o = sg.exec(this.curl, "-D", "headers.txt", "http://localhost:8086/repos/" + repoName + "/history/");
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        var url404 = "/repos/" + repoName + "/history/data";
        this.verify404(url404, true);

    }

    this.testGetChangesetJSON = function() {

        var folder = createFolderOnDisk("testGetChangesetJSON");
        var folder2 = createFolderOnDisk("testGetChangesetJSON_2");
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
        var file2 = createFileOnDisk(pathCombine(folder, "file2.txt"), 9);
        var file3 = createFileOnDisk(pathCombine(folder, "file3.txt"), 5);
        var file4 = createFileOnDisk(pathCombine(folder, "file4.txt"), 10);

        addRemoveAndCommit();
        var file5 = createFileOnDisk(pathCombine(folder, "file5.txt"), 15);

        insertInFile(file, 20);
        sg.pending_tree.remove(file3);
        sg.pending_tree.move(file2, folder2);
        sg.pending_tree.rename(file4, "file4_rename");
        sg.pending_tree.add(file5);
        var csid = sg.pending_tree.commit(["stamp=stamp2", "message=blah"]);
        var o = sg.exec(vv, "tag", "--rev=" + csid, "tag2");

        var repoName = encodeURI(repInfo.repoName);

        var url = "http://localhost:8086/repos/" + repoName + "/changesets/" + csid;

        o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", url);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        //verify JSON results
        var cs = eval('(' + o.stdout + ')');

        //THIS IS WHAT THE CHANGE SET WEB PAGE EXPECTS FROM THE JSON
        //IF THIS FAILS, THAT MEANS THE WEB PAGE IS BROKEN, DON'T JUST CHANGE THE TEST
      
        var d = cs.description;
        testlib.isNotNull(d, "description should not be null");

        testlib.isNotNull(d.changeset_id, "description changeset_id should not be null");
        //testlib.isNotNull(d.parents, "description parents should not be null");
        testlib.isNotNull(d.audits[0].who, "description audits who should not be null");
        testlib.isNotNull(d.audits[0].when, "description audits when should not be null");
        testlib.isNotNull(d.tags, "description tags should not be null");

        //todo check valid stamp equal when stamps work in jsglue
        testlib.isNotNull(d.stamps, "description stamps should not be null");
        //testlib.isNotNull(d.children, "description children should not be null");
        testlib.isNotNull(d.comments, "description comments should not be null");

        testlib.equal(d.comments[0].text, "blah");
        testlib.isNotNull(d.comments[0].history[0].audits[0].who, "description comments who should not be null");
        testlib.isNotNull(d.comments[0].history[0].audits[0].when, "description comments when should not be null");
        
        var add;
        var rename; 
        var deleted; 
        var moved; 
        var mod; 
        
        for (var p in d.parents)
        {
            testlib.isNotNull(cs[p], "diff should not be null");
            print (p);
            add = cs[p].Added[0];
            rename = cs[p].Renamed[0];
            deleted = cs[p].Removed[0];
            moved = cs[p].Moved[0];
            mod = cs[p].Modified[0];
        }

        testlib.isNotNull(mod, "diff modified should not be null");
        testlib.isNotNull(mod.path, "diff modified path should not be null");
        testlib.equal(mod.type, "File", "diff modified type should be file");
        testlib.isNotNull(mod.hid, "diff modified hid should not be null");
        testlib.isNotNull(mod.old_hid, "diff modified old hid should not be null");
        testlib.isNotNull(mod.gid, "diff modified gid should not be null");

        testlib.isNotNull(add, "diff added should not be null");
        testlib.isNotNull(add.path, "diff added path should not be null");
        testlib.equal(add.type, "File", "diff added type should be file");
        testlib.isNotNull(add.hid, "diff added hid should not be null");
        testlib.isNotNull(add.gid, "diff modified gid should not be null");

        testlib.isNotNull(deleted, "diff deleted should not be null");
        testlib.isNotNull(deleted.path, "diff deleted path should not be null");
        testlib.equal(deleted.type, "File", "diff deleted type should be file");
        testlib.isNotNull(deleted.hid, "diff deleted hid should not be null");
        testlib.isNotNull(deleted.gid, "diff deleted gid should not be null");

        testlib.isNotNull(rename, "diff rename should not be null");
        testlib.isNotNull(rename.path, "diff rename path should not be null");
        testlib.equal(rename.type, "File", "diff rename type should be file");
        testlib.isNotNull(rename.hid, "diff rename hid should not be null");
        testlib.equal(rename.oldname, "file4.txt", "diff rename oldname should be file4.txt");
        testlib.isNotNull(rename.gid, "diff rename gid should not be null");

        testlib.isNotNull(moved, "diff moved should not be null");
        testlib.isNotNull(moved.path, "diff moved path should not be null");
        testlib.equal(moved.type, "File", "diff moved type should be file");
        testlib.isNotNull(moved.hid, "diff moved hid should not be null");
        testlib.equal(moved.from, "@/testGetChangesetJSON/", "diff moved from should be " + pathCombine(folder, "file2.txt"));
        testlib.isNotNull(moved.gid, "diff moved gid should not be null");

             //test .html page
        o = sg.exec(this.curl, "-D", "headers.txt", url);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;
    }



    this.testBrowseChangeSetJSON = function testBrowseChangeSetJSON() {

        var folder = createFolderOnDisk("testBrowseChangeSetJSON");
        var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);

        sg.pending_tree.addremove();

        var csid = sg.pending_tree.commit();

        var repoName = encodeURI(repInfo.repoName);

        var csurl = "http://localhost:8086/repos/" + repoName + "/changesets/" + csid;

        //THIS IS WHAT THE BROWSE CHANGE SET WEB PAGE EXPECTS FROM THE JSON
        //IF THIS FAILS, THAT MEANS THE WEB PAGE IS BROKEN, DON'T JUST CHANGE THE TEST
      
        //test folder GID
        var o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", csurl);

        var cs = eval('(' + o.stdout + ')');

        var gid = 0;

        var pcsid;
        for (var p in cs.description.parents)
        {
            pcsid = p;
        
        }
        //we don't know which value is gid is returned first, we want the Directory
        var adds = cs[pcsid].Added;

        for (var i = 0; i < adds.length; i++) {

            if (adds[i].type == "Directory")
                gid = adds[i].gid;
        }

        var bcsurl = "http://localhost:8086/repos/" + repoName + "/changesets/" + csid + "/browsecs/" + gid;

        o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", bcsurl);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        var cs = eval('(' + o.stdout + ')');

        testlib.equal(cs.name, "testBrowseChangeSetJSON", "name should be testBrowseChangeSetJSON");
        testlib.isNotNull(cs.changeset_id, "changeset_id should not be null");
        testlib.equal(cs.path, "@/testBrowseChangeSetJSON", "path should be @/testBrowseChangeSetJSON");
        testlib.equal(cs.type, "Directory", "type should be directory");
        testlib.isNotNull(cs.hid, "hid should not be null");
        testlib.isNotNull(cs.gid, "gid should not be null");

        var ct = cs.contents[0];
        testlib.isNotNull(ct, "contents should not be null");
        testlib.isNotNull(ct.gid, "contents gid should not be null");
        testlib.isNotNull(ct.hid, "contents hid should not be null");
        testlib.equal(ct.name, "file1.txt", "contents name should be file1.txt");
        testlib.equal(ct.type, "File", "type should be File");
        testlib.equal(ct.path, "@/testBrowseChangeSetJSON/file1.txt", "path should be @/testBrowseChangeSetJSON/file1.txt");


        //test folder Path
        bcsurl = "http://localhost:8086/repos/" + repoName + "/changesets/" + csid + "/browsecs/@/testBrowseChangeSetJSON";
        o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", bcsurl);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;


        var cs1 = eval('(' + o.stdout + ')');

        testlib.equal(cs1.name, "testBrowseChangeSetJSON", "name should be testBrowseChangeSetJSON");
        testlib.isNotNull(cs1.changeset_id, "changeset_id should not be null");
        testlib.equal(cs1.path, "@/testBrowseChangeSetJSON", "path should be @/testBrowseChangeSetJSON");
        testlib.equal(cs1.type, "Directory", "type should be directory");
        testlib.isNotNull(cs1.hid, "hid should not be null");
        testlib.isNotNull(cs1.gid, "gid should not be null");

        var ct1 = cs1.contents[0];
        testlib.isNotNull(ct1, "contents should not be null");
        testlib.isNotNull(ct1.gid, "contents gid should not be null");
        testlib.isNotNull(ct1.hid, "contents hid should not be null");
        testlib.equal(ct1.name, "file1.txt", "contents name should be file1.txt");
        testlib.equal(ct1.type, "File", "type should be File");
        testlib.equal(ct1.path, "@/testBrowseChangeSetJSON/file1.txt", "path should be @/testBrowseChangeSetJSON/file1.txt");


        //test file
        bcsurl = "http://localhost:8086/repos/" + repoName + "/changesets/" + csid + "/browsecs/@/testBrowseChangeSetJSON/file1.txt";
        o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", bcsurl);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        var csFile = eval('(' + o.stdout + ')');

        testlib.equal(csFile.name, "file1.txt", "name should be file1.txt");
        testlib.isNotNull(csFile.changeset_id, "changeset_id should not be null");
        testlib.equal(csFile.path, "@/testBrowseChangeSetJSON/file1.txt", "path should be @/testBrowseChangeSetJSON/file1.txt");
        testlib.equal(csFile.type, "File", "type should be File");
        testlib.isNotNull(csFile.hid, "hid should not be null");
        testlib.isNotNull(csFile.gid, "gid should not be null");

        //test .html page path

        var bcsurl = "http://localhost:8086/repos/" + repoName + "/changesets/" + csid + "/browsecs/@/testBrowseChangeSetJSON";
        print("testing " + bcsurl);
        o = sg.exec(this.curl, "-D", "headers.txt", bcsurl);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        //test .html page gid
        bcsurl = "http://localhost:8086/repos/" + repoName + "/changesets/" + csid + "/browsecs/" + encodeURI(gid);
        print("testing " + bcsurl);
        o = sg.exec(this.curl, "-D", "headers.txt", bcsurl);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;
    }


    this.testDiff = function testDiff() {

        var file = createFileOnDisk("testDiff.txt", 10);
        addRemoveAndCommit();
        insertInFile(file, 20);
        var csid = sg.pending_tree.commit();

        var repoName = encodeURI(repInfo.repoName);

        var csurl = "http://localhost:8086/repos/" + repoName + "/changesets/" + csid;

        var o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", csurl);
        var cs = eval('(' + o.stdout + ')');

        var pcsid;
        for (var p in cs.description.parents)
        {
            pcsid = p;        
        }
        var mod = cs[pcsid].Modified[0];


        var diffurl = "/" + repoName + "/diff/" + mod.hid;
        print ("testing " + diffurl);
        this.verify404(diffurl);

        diffurl = "http://localhost:8086/repos/" + repoName + "/diff/" + mod.hid + "/" + mod.old_hid;
        print ("testing " + diffurl);
        o = sg.exec(this.curl, "-D", "headers.txt", diffurl);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        diffurl = "http://localhost:8086/repos/" + repoName + "/diff/" + mod.hid + "/" + mod.old_hid + "/testDiff.txt";
        print ("testing " + diffurl);

        o = sg.exec(this.curl, "-D", "headers.txt", diffurl);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;


        var localFolder = pathCombine("@", file);
        diffurl = "http://localhost:8086/repos/" + repoName + "/diff/" + mod.hid + "/" + localFolder;
        print ("testing " + diffurl);

        o = sg.exec(this.curl, "-D", "headers.txt", diffurl);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

    }


    this.testGetComments = function testGetComments() {

        var file = createFileOnDisk("testGetComments", 10);
        sg.pending_tree.add(file);
        var csid = sg.pending_tree.commit(["message=comment"]);

        var repoName = encodeURI(repInfo.repoName);

        var csurl = "http://localhost:8086/repos/" + repoName + "/changesets/" + csid + "/comments";

        var o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", "-G", csurl);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;
        var cs = eval( o.stdout );

        testlib.equal(cs[0].text, "comment");
        testlib.isNotNull(cs[0].history[0].audits[0].who, "who should not be null");
        testlib.isNotNull(cs[0].history[0].audits[0].when, "when should not be null");
        // 'when' is an int64, NOT a formatted string
        // 'whenint64' does not exist
        //testlib.isNull(cs[0].whenint64, "whenint64 comments should not exist");

    }

  this.testGetAllStamps = function testGetAllStamps() {

        var repoName = encodeURI(repInfo.repoName);

        var o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", "http://localhost:8086/repos/" + repoName + "/stamps/");
        res = eval(o.stdout);

        var cs = res[0];

        testlib.isNotNull(cs.stamp, "stamp should not be null");
  }

  this.testPostStamp = function testPostStamp() {

        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/changesets/" + this.changesetId + "/stamps/";

        var stamp = "testPostStamp";
        var o = sg.exec(this.curl,
            "-H", "From: " + repInfo.userName,
            "-d", stamp,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), url)) return;

        // Test that our stmap was actually added by finding it in the output of "vv log":
        o = sg.exec("vv", "stamps");
        if (!testlib.testResult(o.exit_status == 0, "failed vv")) return;
        testlib.testResult(o.stdout.search(stamp) >= 0, "stamp not found");


  }


  this.testDeleteStamp = function testDeleteStamp() {

        var stamp = "testDeleteStamp";
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/changesets/" + this.changesetId + "/stamps/";


        var o = sg.exec(this.curl,
            "-H", "From: " + repInfo.username,
            "-d", stamp,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), url)) return;

        // Test that our stmap was actually added by finding it in the output of "vv log":
        o = sg.exec("vv", "stamps");
        if (!testlib.testResult(o.exit_status == 0, "failed vv")) return;
        testlib.testResult(o.stdout.search(stamp) >= 0, "stamp should be in list");

        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/changesets/" + this.changesetId + "/stamps/" + stamp;

        o = sg.exec(this.curl,
            "-H", "From: " + repInfo.username,
            "-X", "DELETE",
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), url)) return;

        // Test that our comment was actually added by finding it in the output of "vv log":
        o = sg.exec("vv", "stamps");
        if (!testlib.testResult(o.exit_status == 0, "failed vv")) return;
        print (o.stdout);
        testlib.testResult(o.stdout.search(stamp) < 0, "stamp should be gone");


  }

  this.testPostTag = function testPostTag() {

        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/changesets/" + this.changesetId + "/tags/";

        var tag = "testPostTag";
        var o = sg.exec(this.curl,
            "-H", "From: " + repInfo.userName,
            "-d", tag,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), url)) return;

        // Test that our stmap was actually added by finding it in the output of "vv log":
        o = sg.exec("vv", "tags");
        if (!testlib.testResult(o.exit_status == 0, "failed vv")) return;
        testlib.testResult(o.stdout.search(tag) >= 0, "tag should be in list");


  }

  this.testDeleteTag = function testDeleteTag() {

        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/changesets/" + this.changesetId + "/tags/";

        var tag = "testDeleteTag";
        var o = sg.exec(this.curl,
            "-H", "From: " + repInfo.userName,
            "-d", tag,
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), url)) return;

        // Test that our stmap was actually added by finding it in the output of "vv log":
        o = sg.exec("vv", "tags");
        if (!testlib.testResult(o.exit_status == 0, "failed vv")) return;
        testlib.testResult(o.stdout.search(tag) >= 0, "tag not found");

        url = "http://localhost:8086/repos/" + repInfo.repoName + "/changesets/" + this.changesetId + "/tags/" + tag;

        var o = sg.exec(this.curl,
            "-H", "From: " + repInfo.userName,
            "-X", "DELETE",
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), url)) return;

        // Test that our tag was actually added by finding it in the output of "vv log":
        o = sg.exec("vv", "tags");
        if (!testlib.testResult(o.exit_status == 0, "failed vv")) return;
        testlib.testResult(o.stdout.search(tag) <  0, "tag should be gone");

  }


  this.testHistoryQuery = function testHistoryQuery() {

    //todo

  }

  this.testGetCSTags = function testGetCSTags() {

       var folder = createFolderOnDisk("testGetCSTags");
       var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);

       sg.pending_tree.addremove();

       var csid = sg.pending_tree.commit();

       var repoName = encodeURI(repInfo.repoName);
       var url = "http://localhost:8086/repos/" + repInfo.repoName + "/changesets/" + csid + "/tags/";

       var tag = "testGetCSTags";
       var o = sg.exec(this.curl,
            "-H", "From: " + repInfo.userName,
            "-d", tag,
            "-D", "headers.txt",
            encodeURI(url));
       if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
       if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), url)) return;


       o = sg.exec(this.curl,
            "-H", "From: " + repInfo.userName,
            "-H", "Accept: application/json",
            "-D", "headers.txt",
            encodeURI(url));
       if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
       if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), url)) return;

       var res = eval(o.stdout);

       testlib.equal(1, res.length, "should be one tag");
       var cs = res[0];
       testlib.equal(tag, cs.tag, "tag should be testGetCSTags");

  }

  this.testGetCSStamps = function testGetCSStamps() {
       var folder = createFolderOnDisk("testGetCSStamps");
       var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);

       sg.pending_tree.addremove();
       var csid = sg.pending_tree.commit();

       var repoName = encodeURI(repInfo.repoName);

       var repoName = encodeURI(repInfo.repoName);
       var url = "http://localhost:8086/repos/" + repInfo.repoName + "/changesets/" + csid + "/stamps/";

       var stamp = " testGetCSStamps";
       var o = sg.exec(this.curl,
            "-H", "From: " + repInfo.userName,
            "-d",  stamp,
            "-D", "headers.txt",
            encodeURI(url));
       if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
       if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), url)) return;


       o = sg.exec(this.curl,
            "-H", "From: " + repInfo.userName,
            "-H", "Accept: application/json",
            "-D", "headers.txt",
            encodeURI(url));
       if (!testlib.testResult(o.exit_status == 0, "failed curl")) return;
       if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), url)) return;

       var res = eval(o.stdout);

       testlib.equal(1, res.length, "should be one stamp");
       var cs = res[0];
       testlib.equal( stamp, cs.stamp, "stamp should be testGetCSTags");

  }


  this.testGetBlob = function testGetBlob() {

        var file = createFileOnDisk("testGetBlob.txt", 39);

        sg.pending_tree.addremove();

        var csid = sg.pending_tree.commit();

        var repoName = encodeURI(repInfo.repoName);

        var csurl = "http://localhost:8086/repos/" + repoName + "/changesets/" + csid;

        var o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", csurl);

        var cs = eval('(' + o.stdout + ')');

        var pcsid;
        for (var p in cs.description.parents)
        {
            pcsid = p;
        
        }
        var bloburl =  "http://localhost:8086/repos/" + repoName + "/blobs/" + cs[pcsid].Added[0].hid;

        o = sg.exec(this.curl, "-D", "headers.txt", bloburl);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        bloburl = "/" + repoName + "/blobs/";
        this.verify404(bloburl, false);

    }

    this.testWorkItemRoundTrip = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/";
        var username = repInfo.userName;
        var ourDesc = "This bug was added by the test testWorkItemRoundTrip.";
        var bugJson = "{"
            + "\"rectype\":\"item\","
	    + "\"id\":\"testAddWIRT\","
	    + "\"listorder\":1,"
            + "\"title\":\"" + ourDesc + "\"}";
        var o = sg.exec(this.curl,
            "-H", "From: " + username,
            "-d", bugJson,
            "-D", "headers.txt",
            encodeURI(url));

        if (!testlib.testResult(o.exit_status == 0, "initial post")) return;

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "initial post")) return;

	var wid = o.stdout;

        o = sg.exec(this.curl,
            "-H", "Accept: application/json",
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "retrieval")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "retrieval")) return;

        var records = false;

	print("\nrecid: " + wid + "\n");
	print("\n\n" + o.stdout + "\n\n");

        eval('records = ' + o.stdout);

        if (!testlib.testResult((!!records) && (records.length > 0), "no records found"))
            return;

	var rec = false;

	for ( var i = 0; i < records.length; ++i )
	{
	    if (records[i].recid == wid)
	    {
		rec = records[i];
		break;
	    }
	}

	if (! testlib.testResult(!! rec, "no matching record"))
	    return;

	delete rec.rectype;
	var jUpdate = JSON.stringify(rec);

        var o = sg.exec(this.curl,
            "-H", "From: " + username,
	    "-X", "PUT",
            "-d", jUpdate,
            "-D", "headers.txt",
            encodeURI(url + "/" + wid));

        if (!testlib.testResult(o.exit_status == 0, "update")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "update")) return;

	// todo: once working at all, modify and test an integer value
    }

   this.testGetBlobDownload = function testGetBlobDownload() {

        var file = createFileOnDisk("testGetBlobDownload.txt", 5);

        sg.pending_tree.addremove();

        var csid = sg.pending_tree.commit();

        var repoName = encodeURI(repInfo.repoName);

        var csurl = "http://localhost:8086/repos/" + repoName + "/changesets/" + csid ;

        var o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", csurl);

        var cs = eval('(' + o.stdout + ')');

        var pcsid;
        for (var p in cs.description.parents)
        {
            pcsid = p;
        
        }
        var bloburl = "http://localhost:8086/repos/" + repoName + "/blobs-download/" + cs[pcsid].Added[0].hid + "/testGetBlobDownload.txt";

        o = sg.exec(this.curl, "-D", "headers.txt", bloburl);
        if (!testlib.testResult(o.exit_status == 0)) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"))) return;

        bloburl = "/" + repoName + "/blobs-download/" + cs[pcsid].Added[0].hid;

        this.verify404(bloburl, false);

        bloburl = "/" + repoName + "/blobs-download/";

        this.verify404(bloburl, false);

    };

    this.testGetTags = function testGetTags() {

        var file = createFileOnDisk("testGetTags.txt", 20);

        sg.pending_tree.addremove();

        var csid = sg.pending_tree.commit();

        var repoName = encodeURI(repInfo.repoName);

        var o = sg.exec(vv, "tag", "--rev=" + csid, "testGetTags");
        o = sg.exec(this.curl, "-D", "headers.txt", "-H", "Accept: application/json", "http://localhost:8086/repos/" + repoName + "/tags/");
        res = eval(o.stdout);

        var cs = res[0];
        testlib.isNotNull(cs.tag, "tag should not be null");
        testlib.isNotNull(cs.csid, "csid should not be null");

   }

    this.testAssignToSprint = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/" + this.sprintId + "/items";

        var testname = "testAssignToSprint";
        var username = repInfo.userName;

        var linkdata = {
            "recid": this.bugId
        };
        sg.file.write(testname + ".json", JSON.stringify(linkdata));

        var o = sg.exec(this.curl,
                        "-H", "From: " + username,
                        "-X", "PUT",
                        "-T", testname + ".json",
                        "-D", "headers.txt",
                        encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed to link " + this.bugId + " to " + this.sprintId))
            return (false);

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "linking " + this.bugId + " to " + this.sprintId))
            return (false);

        var repo = sg.open_repo(repInfo.repoName);
        var zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);
        var ztx = zs.begin_tx();

        var sprint = ztx.open_record(this.sprintId);

        if (testlib.testResult(!! sprint.items, "items defined"))
            if (testlib.testResult(sprint.items.length > 0, "non-empty items"))
            {
                var lid = sprint.items.to_array()[0];

                var ln = ztx.open_record(lid);

                testlib.equal(this.bugId, ln.recid, "wrong item ID");
            }


        ztx.abort();

        repo.close();

        url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/" + this.bugId + "/sprint";

        o = sg.exec(this.curl,
            "-H", "Accept: application/json",
             "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "retrieval")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "retrieval")) return;

        var sprintlist = false;
        eval('sprintlist = ' + o.stdout + ';');

        if (testlib.testResult(!! sprintlist, "no sprint list returned") &&
            testlib.testResult(sprintlist.length > 0, "sprint list is empty"))
        {
            var found = false;

            for ( var i in sprintlist )
                {
                    var sprt = sprintlist[i];

                    if (sprt.recid == this.sprintId)
                    {
                        found = true;
                        break;
                    }
                }

            testlib.testResult(found, "target sprint found");
        }
    };

    // see SPRAWL-903
    this.testAssignToSprintTwice = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/" + this.sprintId + "/items";

        var testname = "testAssignToSprint";
        var username = repInfo.userName;

        var linkdata = {
            "recid": this.bugId
        };
        sg.file.write(testname + ".json", JSON.stringify(linkdata));

        var o = sg.exec(this.curl,
                        "-H", "From: " + username,
                        "-X", "PUT",
                        "-T", testname + ".json",
                        "-D", "headers.txt",
                        encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed to link " + this.bugId + " to " + this.sprintId))
            return (false);

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "linking " + this.bugId + " to " + this.sprintId))
            return (false);

        var o = sg.exec(this.curl,
                        "-H", "From: " + username,
                        "-X", "PUT",
                        "-T", testname + ".json",
                        "-D", "headers.txt",
                        encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed to relink " + this.bugId + " to " + this.sprintId))
            return (false);

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "relinking " + this.bugId + " to " + this.sprintId))
            return (false);
    };

    this.testAssignToSprintImmediate = function() {

        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/";
        var username = repInfo.userName;
        var testname = "testAssignToSprintImmediate";

	var rec = {
	    "rectype": "item",
	    "id": "tatsi",
	    "title": testname,
	    "sprint": this.sprintId
	};

	var bugJson = JSON.stringify(rec);

        var o = sg.exec(this.curl,
            "-H", "From: " + username,
            "-d", bugJson,
            "-D", "headers.txt",
            encodeURI(url));

        if (!testlib.testResult(o.exit_status == 0, "post data"))
            return (false);

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "post data"))
            return (false);

	var recid = o.stdout;

        url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/" + recid + "/sprint";

        o = sg.exec(this.curl,
            "-H", "Accept: application/json",
             "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "retrieval")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "retrieval")) return;

        var sprintlist = false;
        eval('sprintlist = ' + o.stdout + ';');

        if (testlib.testResult(!! sprintlist, "no sprint list returned") &&
            testlib.testResult(sprintlist.length > 0, "sprint list is empty"))
        {
            var found = false;

            for ( var i in sprintlist )
                {
                    var sprt = sprintlist[i];

                    if (sprt.recid == this.sprintId)
                    {
                        found = true;
                        break;
                    }
                }

            testlib.testResult(found, "target sprint found");
        }
	
    };


    this.testSprintAsField = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/" + this.sprintId + "/items";

        var testname = "testSprintAsField";
        var username = repInfo.userName;

        var linkdata = {
            "recid": this.bugId
        };
        sg.file.write(testname + ".json", JSON.stringify(linkdata));

        var o = sg.exec(this.curl,
                        "-H", "From: " + username,
                        "-X", "PUT",
                        "-T", testname + ".json",
                        "-D", "headers.txt",
                        encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed to link " + this.bugId + " to " + this.sprintId))
            return (false);

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "linking " + this.bugId + " to " + this.sprintId))
            return (false);

        url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/?rectype=item";

        o = sg.exec(this.curl,
            "-H", "Accept: application/json",
             "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "retrieval")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "retrieval")) return;

        var wilist = false;
        eval('wilist = ' + o.stdout + ';');

        if (testlib.testResult(!! wilist, "no bug list returned") &&
            testlib.testResult(wilist.length > 0, "bug list is empty"))
        {
            var found = false;

            for ( var i in wilist )
                {
                    var bug = wilist[i];

                    if (bug.sprint == this.sprintId)
                    {
                        found = true;
                        break;
                    }
                }

            testlib.testResult(found, "target sprint found");
        }
    };

    this.testGetWitHistory = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/withistory?sprint=" + this.sprintId;

        var testname = "testGetWithHistory";
        var username = repInfo.userName;

        var repo = sg.open_repo(repInfo.repoName);
        var zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);
        var ztx = zs.begin_tx();

        var bug = ztx.open_record(this.bugId);
        bug.sprint = this.sprintId;
        ztx.commit();
        repo.close();

        var o = sg.exec(this.curl,
                        "-H", "From: " + username,
                        "-H", "Accept: application/json",
                        "-D", "headers.txt",
                        encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed to pull wit history"))
            return (false);

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "pulling wit history"))
            return (false);

        var wits = false;
        eval('wits = ' + o.stdout);

        testlib.testResult(!! wits, "no data returned") ||
            testlib.testResult(wits.length > 0);
    };


    this.testListUsers = function() {
        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/users/records";

        var testname = "testListUsers";
        var username = repInfo.userName;

        var o = sg.exec(this.curl,
                        "-H", "From: " + username,
                        "-H", "Accept: application/json",
                        "-D", "headers.txt",
                        encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "failed to pull users"))
            return (false);

        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "listing users"))
            return (false);

        var reclist = false;

        eval('reclist = ' + o.stdout);

        if (testlib.testResult(!! reclist, "user list defined"))
            if (testlib.testResult(reclist.length > 0, "no users seen"))
            {
                var found = false;

                for ( var i = 0; i < reclist.length; ++i )
                {
                    var email = reclist[0].email;
                    print("email: " + email + "\n");

                    if (email == 'debug@sourcegear.com')
                    {
                        found = true;
                        break;
                    }
                }

                testlib.testResult(found, "matching user not found");
            }
    };

    this.testUpdateToDeath = function() {
        var uData = {
            "id": this.bugSimpleId,
            "title": "second work item",
            "logged_time" : 0,
            "status" : "open",
            "priority" : "High"
        };
        var updateUrl = "http://localhost:8086/local/repo/wit/records/" + this.bugId;
	    var uStr = JSON.stringify(uData);
        var username = "debug@sourcegear.com";

        for ( var i = 0; i < 5; ++i )
        {
            var o = sg.exec(this.curl,
                            "-H", "From: " + username,
	                    "-X", "PUT",
                            "-d", uStr,
                            "-D", "headers.txt",
                            encodeURI(updateUrl));

            if (!testlib.testResult(o.exit_status == 0, "curl, update " + i)) return;
            if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "update " + i)) return;
        }
    };

    this.testSearchWorkItems = function() {
	this.searchables = {};
	this._addSearchable("this is here", true);
	this._addSearchable("and this", true);
	this._addSearchable("and this, too", true);
	this._addSearchable( "but not that", false);

        var url = "http://localhost:8086/repos/" + repInfo.repoName + "/wit/records/?q=this";
        var o = sg.exec(this.curl,
            "-H", "Accept: application/json",
            "-D", "headers.txt",
            encodeURI(url));
        if (!testlib.testResult(o.exit_status == 0, "retrieving records")) return;
        if (!this.checkStatus("200 OK", sg.file.read("headers.txt"), "retrieving records")) return;

        var records = false;

        eval('records = ' + o.stdout);

	var found = {};

	for ( var i = 0; i < records.length; ++i )
	{
	    var rec = records[i];
	    
	    found[rec.recid] = true;
	}

	for ( var recid in this.searchables )
	{
	    var shouldFind = this.searchables[recid];

	    if (shouldFind)
		testlib.testResult(!! found[recid], "expected to find rec " + recid);
	    else
		testlib.testResult(! found[recid], "expected not to find rec " + recid);
	}
    };

    this.tearDown = function() {
        if (sg.platform() == "WINDOWS")
            sg.exec("taskkill", "/PID", this.serverPid.toString(), "/F");
        else
            sg.exec("kill", "-s", "SIGINT", this.serverPid.toString());
        print("(stWeb.tearDown: Giving 'vv serve' some time to shut down before we print its stdout/stderr output.)")
        sleep(30000);
        print();
        print(sg.file.read("../stWebStdout.txt"));
        print();
        print(sg.file.read("../stWebStderr.txt"));
    }


}
function commitModFileCLC(name, stamp) {

    var folder = createFolderOnDisk(name);
    var file = createFileOnDisk(pathCombine(folder, "file1.txt"), 39);
    addRemoveAndCommit();
    insertInFile(file, 20);

    o = sg.exec(vv, "commit", "--message=fakemessage", "--stamp=" + stamp);
    testlib.ok(o.exit_status == 0, "exit status should be 0");

}

