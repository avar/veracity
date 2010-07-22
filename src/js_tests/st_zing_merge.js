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

function st_zing_merge()
{
    this.test_both_add_one_record = function()
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
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var zrec = ztx.new_record("item");
        var recid1 = zrec.recid;
        zrec.val = 17;
        ztx.commit();

        ztx = zs.begin_tx(csid);
        var zrec = ztx.new_record("item");
        var recid2 = zrec.recid;
        zrec.val = 13;
        ztx.commit();

        testlib.ok(zs.get_leaves().length==2, "two leaves");
        zs.merge();
        testlib.ok(zs.get_leaves().length==1, "one leaf");

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid1);
        testlib.ok(17 == zrec.val, "val check");
        zrec = ztx.open_record(recid2);
        testlib.ok(13 == zrec.val, "val check");
        ztx.abort();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_both_add_one_record_explicit_merge = function()
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
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var zrec = ztx.new_record("item");
        var recid1 = zrec.recid;
        zrec.val = 17;
        ztx.commit();

        ztx = zs.begin_tx(csid);
        var zrec = ztx.new_record("item");
        var recid2 = zrec.recid;
        zrec.val = 13;
        ztx.commit();

        var leaves = zs.get_leaves();
        testlib.ok(leaves.length==2, "two leaves");
        zs.merge(leaves[0], leaves[1]);
        testlib.ok(zs.get_leaves().length==1, "one leaf");

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid1);
        testlib.ok(17 == zrec.val, "val check");
        zrec = ztx.open_record(recid2);
        testlib.ok(13 == zrec.val, "val check");
        ztx.abort();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_each_modify_different_field = function()
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
                        "foo" : { "datatype" : "int" },
                        "bar" : { "datatype" : "int" }
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
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.foo = 13;
        zrec.bar = 16;
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 26;
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.bar = 32;
        ztx.commit();

        zs.merge();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(26 == zrec.foo, "26");
        testlib.ok(32 == zrec.bar, "32");
        testlib.ok(4 == zrec.history.length);
        ztx.abort();

        var h = zs.get_history(recid);
        testlib.ok(4 == h.length);

        repo.close();
    }

    this.test_automerge_string_least_recent = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "foo" : 
                        {
                            "datatype" : "string",
                            "merge" : 
                            {
                                "auto" : 
                                [
                                    {
                                        "op" : "least_recent"
                                    }
                                ]
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.foo = "begin";
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = "left";
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = "right";
        ztx.commit();

        zs.merge();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok("left" == zrec.foo, "foo");
        ztx.abort();

        repo.close();
    }

    this.test_automerge_string_most_recent = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "foo" : 
                        {
                            "datatype" : "string",
                            "merge" : 
                            {
                                "auto" : 
                                [
                                    {
                                        "op" : "most_recent"
                                    }
                                ]
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.foo = "begin";
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = "left";
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = "right";
        ztx.commit();

        zs.merge();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok("right" == zrec.foo, "foo");
        ztx.abort();

        repo.close();
    }

    this.test_automerge_int_least_recent = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "foo" : 
                        {
                            "datatype" : "int",
                            "merge" : 
                            {
                                "auto" : 
                                [
                                    {
                                        "op" : "least_recent"
                                    }
                                ]
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.foo = 31;
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 35;
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 44;
        ztx.commit();

        zs.merge();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(35 == zrec.foo, "foo");
        ztx.abort();

        repo.close();
    }

    this.test_automerge_int_most_recent = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "foo" : 
                        {
                            "datatype" : "int",
                            "merge" : 
                            {
                                "auto" : 
                                [
                                    {
                                        "op" : "most_recent"
                                    }
                                ]
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.foo = 31;
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 35;
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 44;
        ztx.commit();

        zs.merge();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(44 == zrec.foo, "foo");
        ztx.abort();

        repo.close();
    }

    this.test_automerge_int_min = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "foo" : 
                        {
                            "datatype" : "int",
                            "merge" : 
                            {
                                "auto" : 
                                [
                                    {
                                        "op" : "min"
                                    }
                                ]
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.foo = 31;
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 35;
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 44;
        ztx.commit();

        zs.merge();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(35 == zrec.foo, "foo");
        ztx.abort();

        repo.close();
    }

    this.test_automerge_int_max = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "foo" : 
                        {
                            "datatype" : "int",
                            "merge" : 
                            {
                                "auto" : 
                                [
                                    {
                                        "op" : "max"
                                    }
                                ]
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.foo = 31;
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 35;
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 44;
        ztx.commit();

        zs.merge();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(44 == zrec.foo, "foo");
        ztx.abort();

        repo.close();
    }

    this.test_automerge_int_sum = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "foo" : 
                        {
                            "datatype" : "int",
                            "merge" : 
                            {
                                "auto" : 
                                [
                                    {
                                        "op" : "sum"
                                    }
                                ]
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.foo = 31;
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 40;
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 60;
        ztx.commit();

        zs.merge();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(100 == zrec.foo, "foo");
        ztx.abort();

        repo.close();
    }

    this.test_automerge_int_average = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "foo" : 
                        {
                            "datatype" : "int",
                            "merge" : 
                            {
                                "auto" : 
                                [
                                    {
                                        "op" : "average"
                                    }
                                ]
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.foo = 31;
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 40;
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = 60;
        ztx.commit();

        zs.merge();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(50 == zrec.foo, "foo");
        ztx.abort();

        repo.close();
    }

    this.test_automerge_string_longest = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "foo" : 
                        {
                            "datatype" : "string",
                            "merge" : 
                            {
                                "auto" : 
                                [
                                    {
                                        "op" : "longest"
                                    }
                                ]
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.foo = "plok";
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = "wilbur";
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = "joe";
        ztx.commit();

        zs.merge();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok("wilbur" == zrec.foo, "foo");
        ztx.abort();

        repo.close();
    }

    this.test_automerge_string_shortest = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "foo" : 
                        {
                            "datatype" : "string",
                            "merge" : 
                            {
                                "auto" : 
                                [
                                    {
                                        "op" : "shortest"
                                    }
                                ]
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.foo = "plok";
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = "wilbur";
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.foo = "joe";
        ztx.commit();

        zs.merge();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok("joe" == zrec.foo, "foo");
        ztx.abort();

        repo.close();
    }

    this.test_automerge_string_unique_no_uniqify = function()
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
                            "datatype" : "string",
                            "constraints" :
                            {
                                "unique" : true
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        ztx.new_record("item", {"val":"hello"});
        ztx.commit();

        ztx = zs.begin_tx(csid);
        ztx.new_record("item", {"val":"hello"});
        ztx.commit();

        testlib.ok(zs.get_leaves().length==2, "two leaves");
        var res = zs.merge();
        testlib.ok(zs.get_leaves().length==2, "two leaves");
        testlib.ok(res.errors.length > 0, "should fail due to unique constraint");
        testlib.ok(res.errors[0].type == "unique");

        repo.close();
    }
    
    this.test_automerge_uniqify_int_add_last_modified = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "label" :
                        {
                            "datatype" : "string",
                        },
                        "val" :
                        {
                            "datatype" : "int",
                            "constraints" :
                            {
                                "unique" : true
                            },
                            "merge" :
                            {
                                "uniqify" : 
                                {
                                    "op" : "add",
                                    "which" : "last_modified",
                                    "addend" : 5
                                }
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid1 = ztx.new_record("item", {"val":31, "label":"one"}).recid;
        var leaf1 = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid2 = ztx.new_record("item", {"val":31, "label" : "two"}).recid;
        var leaf2 = ztx.commit().csid;

        ztx = zs.begin_tx(leaf2);
        ztx.open_record(recid2).label = "TWO";
        ztx.commit();

        ztx = zs.begin_tx(leaf1);
        ztx.open_record(recid1).label = "ONE";
        ztx.commit();

        // recid2 was last created
        // recid1 was last modified

        testlib.ok(zs.get_leaves().length==2, "two leaves");
        var res = zs.merge();
        testlib.ok(zs.get_leaves().length==1, "one leaves");

        zrec = zs.get_record(recid1);
        testlib.ok("ONE" == zrec.label);
        testlib.ok(36 == zrec.val);

        zrec = zs.get_record(recid2);
        testlib.ok("TWO" == zrec.label);
        testlib.ok(31 == zrec.val);

        repo.close();
    }
    
    this.test_automerge_uniqify_int_add_last_created = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "label" :
                        {
                            "datatype" : "string",
                        },
                        "val" :
                        {
                            "datatype" : "int",
                            "constraints" :
                            {
                                "unique" : true
                            },
                            "merge" :
                            {
                                "uniqify" : 
                                {
                                    "op" : "add",
                                    "which" : "last_created",
                                    "addend" : 5
                                }
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid1 = ztx.new_record("item", {"val":31, "label":"one"}).recid;
        var leaf1 = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid2 = ztx.new_record("item", {"val":31, "label" : "two"}).recid;
        var leaf2 = ztx.commit().csid;

        ztx = zs.begin_tx(leaf2);
        ztx.open_record(recid2).label = "TWO";
        ztx.commit();

        ztx = zs.begin_tx(leaf1);
        ztx.open_record(recid1).label = "ONE";
        ztx.commit();

        // recid2 was last created
        // recid1 was last modified

        testlib.ok(zs.get_leaves().length==2, "two leaves");
        var res = zs.merge();
        testlib.ok(zs.get_leaves().length==1, "one leaves");

        zrec = zs.get_record(recid1);
        testlib.ok("ONE" == zrec.label);
        testlib.ok(31 == zrec.val);

        zrec = zs.get_record(recid2);
        testlib.ok("TWO" == zrec.label);
        testlib.ok(36 == zrec.val);

        repo.close();
    }
    
    this.test_automerge_uniqify_string_inc_digits_end_no_digits = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "label" :
                        {
                            "datatype" : "string",
                        },
                        "val" :
                        {
                            "datatype" : "string",
                            "constraints" :
                            {
                                "unique" : true
                            },
                            "merge" :
                            {
                                "uniqify" : 
                                {
                                    "op" : "inc_digits_end",
                                    "which" : "last_created"
                                }
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid1 = ztx.new_record("item", {"val":"EWS", "label":"one"}).recid;
        var leaf1 = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid2 = ztx.new_record("item", {"val":"EWS", "label" : "two"}).recid;
        var leaf2 = ztx.commit().csid;

        // recid2 was last created

        testlib.ok(zs.get_leaves().length==2, "two leaves");
        var res = zs.merge();
        testlib.ok(zs.get_leaves().length==1, "one leaves");

        zrec = zs.get_record(recid1);
        testlib.ok("one" == zrec.label);
        testlib.ok("EWS" == zrec.val);

        zrec = zs.get_record(recid2);
        testlib.ok("two" == zrec.label);
        testlib.ok("EWS_0" == zrec.val);

        repo.close();
    }
    
    this.test_automerge_uniqify_string_inc_digits_end_no_base = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "label" :
                        {
                            "datatype" : "string",
                        },
                        "val" :
                        {
                            "datatype" : "string",
                            "constraints" :
                            {
                                "unique" : true
                            },
                            "merge" :
                            {
                                "uniqify" : 
                                {
                                    "op" : "inc_digits_end",
                                    "which" : "last_created"
                                }
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid1 = ztx.new_record("item", {"val":"00499", "label":"one"}).recid;
        var leaf1 = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid2 = ztx.new_record("item", {"val":"00499", "label" : "two"}).recid;
        var leaf2 = ztx.commit().csid;

        // recid2 was last created

        testlib.ok(zs.get_leaves().length==2, "two leaves");
        var res = zs.merge();
        testlib.ok(zs.get_leaves().length==1, "one leaves");

        zrec = zs.get_record(recid1);
        testlib.ok("one" == zrec.label);
        testlib.ok("00499" == zrec.val);

        zrec = zs.get_record(recid2);
        testlib.ok("two" == zrec.label);
        testlib.ok("00500" == zrec.val);

        repo.close();
    }
    
    this.test_automerge_uniqify_string_inc_digits_end = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "label" :
                        {
                            "datatype" : "string",
                        },
                        "val" :
                        {
                            "datatype" : "string",
                            "constraints" :
                            {
                                "unique" : true
                            },
                            "merge" :
                            {
                                "uniqify" : 
                                {
                                    "op" : "inc_digits_end",
                                    "which" : "last_created"
                                }
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid1 = ztx.new_record("item", {"val":"EWS00499", "label":"one"}).recid;
        var leaf1 = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid2 = ztx.new_record("item", {"val":"EWS00499", "label" : "two"}).recid;
        var leaf2 = ztx.commit().csid;

        // recid2 was last created

        testlib.ok(zs.get_leaves().length==2, "two leaves");
        var res = zs.merge();
        testlib.ok(zs.get_leaves().length==1, "one leaves");

        zrec = zs.get_record(recid1);
        testlib.ok("one" == zrec.label);
        testlib.ok("EWS00499" == zrec.val);

        zrec = zs.get_record(recid2);
        testlib.ok("two" == zrec.label);
        testlib.ok("EWS00500" == zrec.val);

        repo.close();
    }
    
    this.test_automerge_both_delete = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "foo" : 
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
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.foo = 31;
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        ztx.delete_record(recid);
        ztx.commit();

        ztx = zs.begin_tx(csid);
        ztx.delete_record(recid);
        ztx.commit();

        zs.merge();

        var err = "";
        try
        {
            zrec = zs.get_record(recid1);
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(0 < err.length, "get_record should not have succeeded");

        repo.close();
    }

    this.test_uniqify_gen_random_unique = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "val" :
                        {
                            "datatype" : "string",
                            "constraints" :
                            {
                                "unique" : true,
                                "minlength" : 1,
                                "maxlength" : 4,
                                "defaultfunc" : "gen_random_unique",
                                "genchars" : "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            },
                            "merge" :
                            {
                                "uniqify" : 
                                {
                                    "op" : "gen_random_unique",
                                    "which" : "last_created"
                                }
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var csid = ztx.commit().csid;

        var zrec;

        ztx = zs.begin_tx(csid);
        zrec = ztx.new_record("item");
        var recid1 = zrec.recid;
        zrec.val = "CQ";
        var leaf1 = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.new_record("item");
        var recid2 = zrec.recid;
        zrec.val = "CQ";
        var leaf2 = ztx.commit().csid;

        // recid2 was last created

        testlib.ok(zs.get_leaves().length==2, "two leaves");
        var res = zs.merge();
        testlib.ok(zs.get_leaves().length==1, "one leaves");

        zrec = zs.get_record(recid1);
        testlib.ok("CQ" == zrec.val);

        zrec = zs.get_record(recid2);
        testlib.ok("CQ" != zrec.val);

        repo.close();
    }

    this.test_merge_three_leaves = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "a" : 
                        {
                            "datatype" : "string"
                        },
                        "b" : 
                        {
                            "datatype" : "string"
                        },
                        "c" : 
                        {
                            "datatype" : "string"
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
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.a = "alpha";
        zrec.b = "bravo";
        zrec.c = "charlie";
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.a = "uno";
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.b = "dos";
        ztx.commit();

        ztx = zs.begin_tx(csid);
        zrec = ztx.open_record(recid);
        zrec.c = "tres";
        ztx.commit();

        testlib.ok(3 == zs.get_leaves().length);
        zs.merge();
        testlib.ok(1 == zs.get_leaves().length);

        repo.close();
    }

    this.test_automerge_uniqify_int_add_least_impact__history_entries = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "label" :
                        {
                            "datatype" : "string",
                        },
                        "val" :
                        {
                            "datatype" : "int",
                            "constraints" :
                            {
                                "unique" : true
                            },
                            "merge" :
                            {
                                "uniqify" : 
                                {
                                    "op" : "add",
                                    "which" : "least_impact",
                                    "addend" : 5
                                }
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid1 = ztx.new_record("item", {"val":31, "label" : "one"}).recid;
        var leaf1 = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid2 = ztx.new_record("item", {"val":31, "label" : "two"}).recid;
        var leaf2 = ztx.commit().csid;

        ztx = zs.begin_tx(leaf2);
        ztx.open_record(recid2).label = "TWO";
        var leaf2b = ztx.commit().csid;

        testlib.ok(zs.get_leaves().length==2, "two leaves");
        var res = zs.merge();
        testlib.ok(zs.get_leaves().length==1, "one leaves");

        // record TWO has more history entries, so record ONE should
        // be the one that gets changed
  
        zrec = zs.get_record(recid1);
        testlib.ok("one" == zrec.label);
        print(sg.to_json(zrec));
        testlib.ok(36 == zrec.val);

        zrec = zs.get_record(recid2);
        testlib.ok("TWO" == zrec.label);
        print(sg.to_json(zrec));
        testlib.ok(31 == zrec.val);

        repo.close();
    }
    
    this.test_automerge_uniqify_int_add_least_impact__most_recent = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "label" :
                        {
                            "datatype" : "string",
                        },
                        "val" :
                        {
                            "datatype" : "int",
                            "constraints" :
                            {
                                "unique" : true
                            },
                            "merge" :
                            {
                                "uniqify" : 
                                {
                                    "op" : "add",
                                    "which" : "least_impact",
                                    "addend" : 5
                                }
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid1 = ztx.new_record("item", {"val":31, "label" : "one"}).recid;
        var leaf1 = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid2 = ztx.new_record("item", {"val":31, "label" : "two"}).recid;
        var leaf2 = ztx.commit().csid;

        testlib.ok(zs.get_leaves().length==2, "two leaves");
        var res = zs.merge();
        testlib.ok(zs.get_leaves().length==1, "one leaves");

        // record TWO is the most recent, so it should
        // be the one that gets changed
  
        zrec = zs.get_record(recid1);
        testlib.ok("one" == zrec.label);
        print(sg.to_json(zrec));
        testlib.ok(31 == zrec.val);

        zrec = zs.get_record(recid2);
        testlib.ok("two" == zrec.label);
        print(sg.to_json(zrec));
        testlib.ok(36 == zrec.val);

        repo.close();
    }
    
    /* TODO enable this test once the least_impact code can handle it
    this.test_automerge_uniqify_int_add_least_impact__dag_changesets = function()
    {
        var t =
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "merge_type" : "field",
                    "fields" :
                    {
                        "label" :
                        {
                            "datatype" : "string",
                        },
                        "val" :
                        {
                            "datatype" : "int",
                            "constraints" :
                            {
                                "unique" : true
                            },
                            "merge" :
                            {
                                "uniqify" : 
                                {
                                    "op" : "add",
                                    "which" : "least_impact",
                                    "addend" : 5
                                }
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

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var csid = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid1 = ztx.new_record("item", {"val":31, "label" : "one"}).recid;
        var leaf1 = ztx.commit().csid;

        ztx = zs.begin_tx(csid);
        var recid2 = ztx.new_record("item", {"val":31, "label" : "two"}).recid;
        var leaf2 = ztx.commit().csid;

        ztx = zs.begin_tx(leaf2);
        var recid2 = ztx.new_record("item", {"val":42, "label" : "three"}).recid;
        var leaf2b = ztx.commit().csid;

        testlib.ok(zs.get_leaves().length==2, "two leaves");
        var res = zs.merge();
        testlib.ok(zs.get_leaves().length==1, "one leaves");

        // record TWO is on a leaf with more children, so
        // record ONE should be the one that gets changed
  
        zrec = zs.get_record(recid1);
        testlib.ok("one" == zrec.label);
        print(sg.to_json(zrec));
        testlib.ok(36 == zrec.val);

        zrec = zs.get_record(recid2);
        testlib.ok("two" == zrec.label);
        print(sg.to_json(zrec));
        testlib.ok(31 == zrec.val);

        repo.close();
    }
    */
    
    // TODO multiple autos, first one fails
    // TODO int most recent (need sleep?)
    // TODO int least recent (need sleep?)
    // TODO string most recent (need sleep?)
    // TODO string least recent (need sleep?)
    // TODO int allowed last
    // TODO int allowed first
    // TODO string allowed last
    // TODO string allowed first
    // TODO string longest fail because they're equal
    // TODO string shortest fail because they're equal

    // TODO try to merge with just one leaf, fail
    // TODO try to merge with three leaves, fail
    // TODO each leaf modify a different record
    // TODO merge_type="record"
    // TODO conflict on int field with no resolver, fail
}

