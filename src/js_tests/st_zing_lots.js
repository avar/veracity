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

function err_must_contain(err, s)
{
    testlib.ok(-1 != err.indexOf(s), s);
}

function st_zing_records()
{
    this.no_setup = true;

    this.test_lots = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "fields" :
                    {
                        "val" :
                        {
                            "datatype" : "int",
                        }
                    }
                }
            }
        };

        var reponame = sg.gid();
        sg.new_repo(reponame, false);

        var repo = sg.open_repo(reponame);
        var zs = new zingdb(repo, sg.dagnum.TESTING_DB);

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        ztx.commit();

        // TODO change the int below to run more iterations.
        // I leave it set low here so the everyday tests
        // won't take so long.
        for (i=0; i<100; i++)
        {
            var ztx = zs.begin_tx();
            var zrec = ztx.new_record("item");
            zrec.val = i;
            ztx.commit();
        }

        repo.close();
        testlib.ok(true, "No errors");
    }
}

