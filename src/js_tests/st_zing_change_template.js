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

function st_zing_change_template()
{
    this.test_simple_change = function()
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
                            "constraints" :
                            {
                                "max" : 10
                            }
                        }
                    }
                }
            }
        };

        var reponame = sg.gid();
        sg.new_repo(reponame, false);

        var repo = sg.open_repo(reponame);
        var zs = new zingdb(repo, sg.dagnum.TESTING_DB);

        var ztx;
        var res;

        ztx = zs.begin_tx();
        ztx.set_template(t);
        res = ztx.commit();
        testlib.ok(res.errors == null);

        ztx = zs.begin_tx();
        ztx.new_record("item", {"val" : 3});
        ztx.new_record("item", {"val" : 5});
        ztx.new_record("item", {"val" : 7});
        ztx.new_record("item", {"val" : 9});
        res = ztx.commit();
        testlib.ok(res.errors == null);

        t.rectypes.item.fields.val.constraints.max = 8;
        ztx = zs.begin_tx();
        ztx.set_template(t);
        res = ztx.commit();
        testlib.ok(1 == res.errors.length);
        testlib.ok("max" == res.errors[0].type);
        testlib.ok("val" == res.errors[0].field_name);
        ztx.abort();

        t.rectypes.item.fields.val.constraints.max = 4;
        ztx = zs.begin_tx();
        ztx.set_template(t);
        res = ztx.commit();
        testlib.ok(3 == res.errors.length);
        testlib.ok("max" == res.errors[0].type);
        testlib.ok("max" == res.errors[1].type);
        testlib.ok("max" == res.errors[2].type);
        ztx.abort();

        repo.close();
    }

}

