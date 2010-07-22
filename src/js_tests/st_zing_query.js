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

function num_keys(obj)
{
    var count = 0;
    for (var prop in obj)
    {
        count++;
    }
    return count;
}

function get_keys(obj)
{
    var count = 0;
    var a = [];
    for (var prop in obj)
    {
        a[count++] = prop;
    }
    return a;
}

function err_must_contain(err, s)
{
    testlib.ok(-1 != err.indexOf(s), s);
}

function verify(zs, where, cs, which, count)
{
    var ids = zs.query("item", ["recid"], where, null, 0, 0, cs[which]);
    var msg = "count " + where + " on " + which + " should be " + count;
    testlib.ok(count == ids.length, msg);
}

function st_zing_query()
{
    this.test_query_misc = function()
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
                        "Luke_Skywalker" : 
                        {
                            "datatype" : "int"
                        },
                        "Han_Solo" : 
                        {
                            "datatype" : "int"
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

        var t2 = zs.get_template();
        testlib.ok(sg.to_json(t) == sg.to_json(t2));

        ztx = zs.begin_tx();
        ztx.new_record("item", {"Luke_Skywalker" : 76, "Han_Solo" : 22});
        ztx.new_record("item", {"Luke_Skywalker" : 11, "Han_Solo" : 33});
        ztx.new_record("item", {"Luke_Skywalker" : -5, "Han_Solo" : 44});
        ztx.new_record("item", {"Luke_Skywalker" : 50, "Han_Solo" : -11});
        ztx.new_record("item", {"Luke_Skywalker" : 22, "Han_Solo" : -33});
        ztx.new_record("item", {"Luke_Skywalker" : 0, "Han_Solo" : 55});
        ztx.new_record("item", {"Luke_Skywalker" : 1, "Han_Solo" : 66});
        ztx.new_record("item", {"Luke_Skywalker" : -40, "Han_Solo" : 99});

        ztx.commit();

        var ids = zs.query("item", ["recid"]);
        testlib.ok(ids.length == 8, "8 results");

        recs = zs.query("item", ["Luke_Skywalker", "Han_Solo"], null, "Luke_Skywalker #DESC, Han_Solo #ASC");
        testlib.ok(recs.length == 8, "8 results");
        testlib.ok(recs[0].Han_Solo == 22)

        recs = zs.query("item", ["Luke_Skywalker", "Han_Solo"], "Luke_Skywalker <= 10", "Han_Solo #ASC");
        testlib.ok(recs.length == 4, "4 results");
        testlib.ok(recs[0].Luke_Skywalker == -5)

        repo.close();
    }
    
    this.test_query_sort_allowed = function()
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
                        "foo" : 
                        {
                            "datatype" : "string",
                            "constraints" :
                            {
                                "allowed" :
                                [
                                    "charlie",
                                    "bravo",
                                    "alpha",
                                    "foxtrot",
                                    "echo",
                                    "delta"
                                ]
                            }
                        },
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

        ztx = zs.begin_tx();
        ztx.new_record("item").foo = "foxtrot";
        ztx.new_record("item").foo = "echo";
        ztx.new_record("item").foo = "delta";
        ztx.new_record("item").foo = "charlie";
        ztx.new_record("item").foo = "bravo";
        ztx.new_record("item").foo = "alpha";

        ztx.commit();

        var ids;
        var recs;

        recs = zs.query("item", ["_rechid", "foo"], null, "foo #ASC");
        testlib.ok(recs.length == 6, "6 results");
        testlib.ok(recs[0].foo == "alpha")
        testlib.ok(recs[1].foo == "bravo")
        testlib.ok(recs[2].foo == "charlie")
        testlib.ok(recs[3].foo == "delta")
        testlib.ok(recs[4].foo == "echo")
        testlib.ok(recs[5].foo == "foxtrot")

        t.rectypes.item.fields.foo.constraints.sort_by_allowed = true;

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        ztx.commit();

        recs = zs.query("item", ["_rechid", "foo"], null, "foo #ASC");
        testlib.ok(recs.length == 6, "6 results");
        testlib.ok(recs[0].foo == "charlie")
        testlib.ok(recs[1].foo == "bravo")
        testlib.ok(recs[2].foo == "alpha")
        testlib.ok(recs[3].foo == "foxtrot")
        testlib.ok(recs[4].foo == "echo")
        testlib.ok(recs[5].foo == "delta")

        repo.close();
    }
    
    this.test_query_sort_string = function()
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
                        "foo" : 
                        {
                            "datatype" : "string"
                        },
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

        ztx.new_record("item").foo = "charlie";
        ztx.new_record("item").foo = "delta";
        ztx.new_record("item").foo = "foxtrot";
        ztx.new_record("item").foo = "alpha";
        ztx.new_record("item").foo = "bravo";
        ztx.new_record("item").foo = "echo";

        ztx.commit();

        var ids = zs.query("item", ["recid"]);
        testlib.ok(ids.length == 6, "6 results");

        recs = zs.query("item", ["_rechid", "foo"], null, "foo #DESC");
        testlib.ok(recs.length == 6, "6 results");
        testlib.ok(recs[0].foo == "foxtrot")

        recs = zs.query("item", ["_rechid", "foo"], null, "foo #ASC", 10, 2);
        testlib.ok(recs.length == 4, "4 results");
        testlib.ok(recs[0].foo == "charlie")

        repo.close();
    }
    
    this.test_query_across_states = function()
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
                        "foo" : 
                        {
                            "datatype" : "int"
                        },
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
        // 0

        ztx = zs.begin_tx();
        ztx.new_record("item").foo = 5;
        ztx.new_record("item").foo = 16;
        ztx.new_record("item").foo = 23;
        var cs1 = ztx.commit();
        // + 1 = 1

        ztx = zs.begin_tx();
        ztx.new_record("item").foo = 16;
        ztx.new_record("item").foo = 16;
        ztx.new_record("item").foo = 17;
        ztx.new_record("item").foo = 15;
        var cs2 = ztx.commit();
        // + 2 = 3

        ztx = zs.begin_tx();
        var recid_deleteme = ztx.new_record("item", {"foo" : 16}).recid;
        var cs3 = ztx.commit();
        // + 1 = 4

        ztx = zs.begin_tx();
        ztx.new_record("item").foo = 15;
        ztx.new_record("item").foo = 12;
        var cs4 = ztx.commit();
        // + 0 = 4

        ztx = zs.begin_tx();
        ztx.delete_record(recid_deleteme);
        var cs5 = ztx.commit();
        // - 1 = 3

        ztx = zs.begin_tx();
        ztx.new_record("item").foo = 11;
        ztx.new_record("item").foo = 16;
        ztx.new_record("item").foo = 16;
        var cs6 = ztx.commit();
        // + 2 = 5

        var res = zs.query_across_states("item", "foo == 16");
        testlib.ok(7 == num_keys(res));
        testlib.ok(1 == res[cs1.csid].count);
        testlib.ok(3 == res[cs2.csid].count);
        testlib.ok(4 == res[cs3.csid].count);
        testlib.ok(4 == res[cs4.csid].count);
        testlib.ok(3 == res[cs5.csid].count);
        testlib.ok(5 == res[cs6.csid].count);

        res = zs.query_across_states("item", "foo == 16", cs5.generation);
        testlib.ok(2 == num_keys(res));
        testlib.ok(3 == res[cs5.csid].count);
        testlib.ok(5 == res[cs6.csid].count);

        res = zs.query_across_states("item", "foo == 16", 1 + cs3.generation);
        testlib.ok(3 == num_keys(res));
        testlib.ok(4 == res[cs4.csid].count);
        testlib.ok(3 == res[cs5.csid].count);
        testlib.ok(5 == res[cs6.csid].count);

        res = zs.query_across_states("item", "foo == 16", cs3.generation, cs5.generation);
        testlib.ok(3 == num_keys(res));
        testlib.ok(4 == res[cs3.csid].count);
        testlib.ok(4 == res[cs4.csid].count);
        testlib.ok(3 == res[cs5.csid].count);

        res = zs.query_across_states("item", "foo == 16", 1 + cs3.generation, cs5.generation - 1);
        testlib.ok(1 == num_keys(res));
        testlib.ok(4 == res[cs4.csid].count);

        repo.close();
    }
    
    this.test_query_sort_limit = function()
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
                        "foo" : 
                        {
                            "datatype" : "int"
                        },
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

        ztx.new_record("item").foo = 5;
        ztx.new_record("item").foo = 8;
        ztx.new_record("item").foo = 11;
        ztx.new_record("item").foo = 17;
        ztx.new_record("item").foo = 26;
        ztx.new_record("item").foo = 33;
        ztx.new_record("item").foo = 34;
        ztx.new_record("item").foo = 65;
        ztx.new_record("item").foo = 77;
        ztx.new_record("item").foo = 104;

        ztx.commit();

        var ids = zs.query("item", ["recid"]);
        testlib.ok(ids.length == 10, "10 results");

        recs = zs.query("item", ["_rechid", "foo"], null, "foo #DESC", 10);
        testlib.ok(recs.length == 10, "10 results");
        testlib.ok(recs[0].foo == 104)

        recs = zs.query("item", ["_rechid", "foo"], null, "foo #DESC", 11);
        testlib.ok(recs.length == 10, "10 results");
        testlib.ok(recs[2].foo == 65)

        recs = zs.query("item", ["_rechid", "foo"], null, "foo #DESC", 4);
        testlib.ok(recs.length == 4, "4 results");
        testlib.ok(recs[2].foo == 65)

        repo.close();
    }
    
    this.test_query_sort_skip = function()
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
                        "foo" : 
                        {
                            "datatype" : "int"
                        },
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

        ztx.new_record("item").foo = 5;
        ztx.new_record("item").foo = 8;
        ztx.new_record("item").foo = 11;
        ztx.new_record("item").foo = 17;
        ztx.new_record("item").foo = 26;
        ztx.new_record("item").foo = 33;
        ztx.new_record("item").foo = 34;
        ztx.new_record("item").foo = 65;
        ztx.new_record("item").foo = 77;
        ztx.new_record("item").foo = 104;

        ztx.commit();

        var ids = zs.query("item", ["recid"]);
        testlib.ok(ids.length == 10, "10 results");

        recs = zs.query("item", ["_rechid", "foo"], null, "foo #DESC", 10, 3);
        testlib.ok(recs.length == 7, "7 results");
        testlib.ok(recs[0].foo == 34)

        recs = zs.query("item", ["_rechid", "foo"], null, "foo #ASC", 10, 9);
        testlib.ok(recs.length == 1, "1 results");
        testlib.ok(recs[0].foo == 104)

        recs = zs.query("item", ["_rechid", "foo"], "foo >= 11", "foo #ASC", 10, 3);
        testlib.ok(recs.length == 5, "5 results");
        testlib.ok(recs[0].foo == 33)

        recs = zs.query("item", ["_rechid", "foo"], "foo == 33", "foo #ASC");
        testlib.ok(recs.length == 1, "1 results");
        testlib.ok(recs[0].foo == 33)

        recs = zs.query("item", ["_rechid", "foo"], "(foo < 10) || (foo > 90)", "foo #DESC");
        testlib.ok(recs.length == 3, "3 results");
        testlib.ok(recs[1].foo == 8)

        repo.close();
    }
    
    this.test_queries_or = function()
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
                        "foo" : 
                        {
                            "datatype" : "int"
                        },
                        "bar" : 
                        {
                            "datatype" : "string"
                        },
                        "yum" : 
                        {
                            "datatype" : "int"
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

        var rec = ztx.new_record("item");
        rec.foo = 5;
        rec.bar = "red";
        rec.yum = 72;

        var rec = ztx.new_record("item");
        rec.foo = 7;
        rec.bar = "red";
        rec.yum = 21;

        var rec = ztx.new_record("item");
        rec.foo = 7;
        rec.bar = "red";
        rec.yum = 61;

        var rec = ztx.new_record("item");
        rec.foo = 7;
        rec.bar = "yellow";
        rec.yum = 0;

        var rec = ztx.new_record("item");
        rec.foo = 9;
        rec.bar = "red";
        rec.yum = 15;

        var rec = ztx.new_record("item");
        rec.foo = 11;
        rec.bar = "blue";
        rec.yum = 44;

        var rec = ztx.new_record("item");
        rec.foo = 31;
        rec.bar = "blue";
        rec.yum = 33;

        ztx.commit();

        var ids = zs.query("item", ["recid"]);
        testlib.ok(ids.length == 7, "7 results");

        ids = zs.query("item", ["_rechid"], "(foo == 7) || (yum > 5000)");
        testlib.ok(ids.length == 3, "3 results");

        ids = zs.query("item", ["_rechid"], "(foo == 238746) || (bar == 'yellow')");
        testlib.ok(ids.length == 1, "1 results");

        repo.close();
    }
    
    this.test_queries_1 = function()
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
                        "foo" : 
                        {
                            "datatype" : "int"
                        },
                        "bar" : 
                        {
                            "datatype" : "string"
                        },
                        "yum" : 
                        {
                            "datatype" : "int"
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

        var rec = ztx.new_record("item");
        rec.foo = 5;
        rec.bar = "red";
        rec.yum = 72;

        var rec = ztx.new_record("item");
        rec.foo = 7;
        rec.bar = "red";
        rec.yum = 21;

        var rec = ztx.new_record("item");
        rec.foo = 7;
        rec.bar = "red";
        rec.yum = 61;

        var rec = ztx.new_record("item");
        rec.foo = 7;
        rec.bar = "yellow";
        rec.yum = 0;

        var rec = ztx.new_record("item");
        rec.foo = 9;
        rec.bar = "red";
        rec.yum = 15;

        var rec = ztx.new_record("item");
        rec.foo = 11;
        rec.bar = "blue";
        rec.yum = 44;

        var rec = ztx.new_record("item");
        rec.foo = 31;
        rec.bar = "blue";
        rec.yum = 33;

        ztx.commit();

        var ids = zs.query("item", ["recid"]);
        testlib.ok(ids.length == 7, "7 results");

        recs = zs.query("item", ["bar", "yum"], "foo < 10", "foo,yum");
        print(sg.to_json(recs));
        testlib.ok(recs.length == 5, "5 results");
        testlib.ok(recs[1].bar == "yellow")
        testlib.ok(recs[0].yum == 72)
        testlib.ok(recs[3].yum == 61)

        recs = zs.query("item", ["_rechid", "yum"], "(foo == 7) && (yum > 50)", "yum #DESC");
        testlib.ok(recs.length == 1, "1 results");
        testlib.ok(recs[0].yum == 61)

        repo.close();
    }

    this.test_query_squares = function()
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
                        "n" : 
                        {
                            "datatype" : "int"
                        },
                        "nsquared" : 
                        {
                            "datatype" : "int"
                        },
                        "color":
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
        ztx.commit();

        ztx = zs.begin_tx();
        for (i=1; i<=27; i++)
        {
            var rec = ztx.new_record("item");
            rec.n = i;
            rec.nsquared = i*i;
            if (0 == (i % 4))
            {
                rec.color = "red";
            }
            else
            {
                rec.color = "white";
            }
        }
        ztx.commit();

        var ids = zs.query("item", ["recid"]);
        testlib.ok(ids.length == 27, "27 results");

        ids = zs.query("item", ["_rechid"], "nsquared > 190", "n");
        testlib.ok(ids.length == 14, "14 results");

        recs = zs.query("item", ["_rechid", "n"], "nsquared==225");
        testlib.ok(recs.length == 1, "1 results");
        testlib.ok(recs[0].n == 15)

        ids = zs.query("item", ["_rechid"], "(n>16) && (color=='red')");
        print(ids.length);
        testlib.ok(ids.length == 2, "2 results");

        repo.close();
    }

    this.test_query_bools = function()
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
                        "n" : 
                        {
                            "datatype" : "int"
                        },
                        "b" : 
                        {
                            "datatype" : "bool"
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

        var rec;

        ztx = zs.begin_tx();

        rec = ztx.new_record("item");
        rec.n = 2;
        rec.b = true;

        rec = ztx.new_record("item");
        rec.n = 3;
        rec.b = true;

        rec = ztx.new_record("item");
        rec.n = 4;
        rec.b = false;

        rec = ztx.new_record("item");
        rec.n = 5;
        rec.b = true;

        rec = ztx.new_record("item");
        rec.n = 6;
        rec.b = false;

        rec = ztx.new_record("item");
        rec.n = 7;
        rec.b = true;

        rec = ztx.new_record("item");
        rec.n = 8;
        rec.b = false;

        rec = ztx.new_record("item");
        rec.n = 9;
        rec.b = false;

        rec = ztx.new_record("item");
        rec.n = 10;
        rec.b = false;

        rec = ztx.new_record("item");
        rec.n = 11;
        rec.b = true;

        rec = ztx.new_record("item");
        rec.n = 12;
        rec.b = false;

        rec = ztx.new_record("item");
        rec.n = 13;
        rec.b = true;

        rec = ztx.new_record("item");
        rec.n = 14;
        rec.b = false;

        ztx.commit();

        var ids = zs.query("item", ["recid"]);
        testlib.ok(ids.length == 13, "13 results");

        ids = zs.query("item", ["_rechid"], "b == #T");
        testlib.ok(ids.length == 6, "6 results");

        ids = zs.query("item", ["_rechid"], "b == #TRUE");
        testlib.ok(ids.length == 6, "6 results");

        ids = zs.query("item", ["_rechid"], "b == #F");
        testlib.ok(ids.length == 7, "7 results");

        ids = zs.query("item", ["_rechid"], "b == #FALSE");
        testlib.ok(ids.length == 7, "7 results");

        ids = zs.query("item", ["_rechid"], "b != #F");
        testlib.ok(ids.length == 6, "6 results");

        ids = zs.query("item", ["_rechid"], "b != #T");
        testlib.ok(ids.length == 7, "7 results");

        ids = zs.query("item", ["_rechid"], "(b == #T) && (n < 4)");
        testlib.ok(ids.length == 2, "2 results");

        repo.close();
    }

    this.test_u0018_multiple_states = function()
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
                        "r" : 
                        {
                            "datatype" : "int"
                        },
                        "x" : 
                        {
                            "datatype" : "int"
                        },
                        "y" : 
                        {
                            "datatype" : "int"
                        },
                        "z" : 
                        {
                            "datatype" : "int"
                        },
                        "w" : 
                        {
                            "datatype" : "int"
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

        ztx = zs.begin_tx();

        var rec1 = ztx.new_record("item");
        var recid1 = rec1.recid;
        rec1.r = 1;
        rec1.x = 4;
        rec1.y = 1;
        rec1.z = -4;

        var rec2 = ztx.new_record("item");
        var recid2 = rec2.recid;
        rec2.r = 2;
        rec2.x = 4;
        rec2.y = 0;
        rec2.z = 24;

        var rec3 = ztx.new_record("item");
        var recid3 = rec3.recid;
        rec3.r = 3;
        rec3.x = 4;
        rec3.y = 1;
        rec3.z = 11;

        var cs = [];
        cs[0] = null;
        cs[1] = ztx.commit().csid;

        ztx = zs.begin_tx(cs[1]);

        var rec4 = ztx.new_record("item");
        var recid4 = rec4.recid;
        rec4.r = 4;
        rec4.x = 4;
        rec4.y = 0;
        rec4.z = -4;

        var rec5 = ztx.new_record("item");
        var recid5 = rec5.recid;
        rec5.r = 5;
        rec5.x = 4;
        rec5.y = 1;
        rec5.z = 24;

        var rec6 = ztx.new_record("item");
        var recid6 = rec6.recid;
        rec6.r = 6;
        rec6.x = 4;
        rec6.y = 0;
        rec6.z = 11;

        cs[2] = ztx.commit().csid;

        ztx = zs.begin_tx(cs[1]);

        var rec7 = ztx.new_record("item");
        var recid7 = rec7.recid;
        rec7.r = 7;
        rec7.x = 4;
        rec7.y = 1;
        rec7.z = -4;

        var rec8 = ztx.new_record("item");
        var recid8 = rec8.recid;
        rec8.r = 8;
        rec8.x = 4;
        rec8.y = 0;
        rec8.z = 24;

        cs[3] = ztx.commit().csid;

        ztx = zs.begin_tx(cs[2]);

        var rec9 = ztx.new_record("item");
        var recid9 = rec9.recid;
        rec9.r = 9;
        rec9.x = 4;
        rec9.y = 1;
        rec9.z = -4;

        ztx.delete_record(recid3);

        rec4 = ztx.open_record(recid4);
        rec4.w = 1968;

        cs[4] = ztx.commit().csid;

        //verify(zs, "r==4", cs, 0, 2);
        verify(zs, "r==4", cs, 1, 0);
        verify(zs, "r==4", cs, 2, 1);
        verify(zs, "r==4", cs, 3, 0);
        verify(zs, "r==4", cs, 4, 1);

        //verify(zs, "r==7", cs, 0, 1);
        verify(zs, "r==7", cs, 1, 0);
        verify(zs, "r==7", cs, 2, 0);
        verify(zs, "r==7", cs, 3, 1);
        verify(zs, "r==7", cs, 4, 0);

        //verify(zs, "r==3", cs, 0, 1);
        verify(zs, "r==3", cs, 1, 1);
        verify(zs, "r==3", cs, 2, 1);
        verify(zs, "r==3", cs, 3, 1);
        verify(zs, "r==3", cs, 4, 0);

        //verify(zs, "y==0", cs, 0, 5);
        verify(zs, "y==0", cs, 1, 1);
        verify(zs, "y==0", cs, 2, 3);
        verify(zs, "y==0", cs, 3, 2);
        verify(zs, "y==0", cs, 4, 3);

        //verify(zs, "y==1", cs, 0, 5);
        verify(zs, "y==1", cs, 1, 2);
        verify(zs, "y==1", cs, 2, 3);
        verify(zs, "y==1", cs, 3, 3);
        verify(zs, "y==1", cs, 4, 3);

        repo.close();
    }
    
    this.test_u0033_lots = function()
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
                        "k" : 
                        {
                            "datatype" : "int"
                        },
                        "i" : 
                        {
                            "datatype" : "int"
                        },
                        "n" : 
                        {
                            "datatype" : "int"
                        },
                        "q" : 
                        {
                            "datatype" : "int"
                        },
                        "which" : 
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
        ztx.commit();

        var k;

        for (k=0; k<10; k++)
        {
            ztx = zs.begin_tx();

            var i;

            for (i=0; i<10; i++)
            {
                var n = k * 1000 + i;

                ztx.new_record("item", {"k":k, "i":i, "n":n, "q": n%13, "which": (n%2 ? "odd" : "even")});
                /*
                var recid = rec.recid;

                rec.k = k;
                rec.i = i;
                rec.n = n;
                rec.q = n % 13;
                rec.which = n%2 ? "odd" : "even";
                */
            }

            ztx.commit();
        }

        ids = zs.query("item", ["_rechid"], "(q == 7) && (k == 8)");
        testlib.ok(ids.length == 1, "1 results");

        repo.close();
    }

    this.test_history = function()
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
                            "datatype" : "int"
                        },
                        "name" : 
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
        ztx.commit();
        // 0

        ztx = zs.begin_tx();
        var recid_a = ztx.new_record("item", {"val" : 5, "name" : "a"}).recid;
        var recid_b = ztx.new_record("item", {"val" : 21, "name" : "b"}).recid;
        var cs1 = ztx.commit();

        ztx = zs.begin_tx();
        var recid_c = ztx.new_record("item", {"val" : 11, "name" : "c"}).recid;
        var recid_d = ztx.new_record("item", {"val" : 13, "name" : "d"}).recid;
        r = ztx.open_record(recid_a);
        r.val = 15;
        var cs2 = ztx.commit();

        ztx = zs.begin_tx();
        r = ztx.open_record(recid_a);
        r.val = 6;
        r = ztx.open_record(recid_c);
        r.val = 6;
        var cs3 = ztx.commit();

        ztx = zs.begin_tx();
        r = ztx.open_record(recid_a);
        r.val = 87;
        r = ztx.open_record(recid_b);
        r.val = 543;
        var cs4 = ztx.commit();

        ztx = zs.begin_tx();
        r = ztx.open_record(recid_a);
        r.val = 11;
        r = ztx.open_record(recid_b);
        r.val = 43;
        var cs5 = ztx.commit();

        ztx = zs.begin_tx();
        r = ztx.open_record(recid_a);
        r.val = 876;
        var cs6 = ztx.commit();

        var res = zs.query("item", ["val", "name", "recid", "history"], null, "name #DESC");
        print(sg.to_json(res));
        testlib.ok(res.length == 4);
        testlib.ok(res[0].recid == recid_d);
        testlib.ok(res[1].recid == recid_c);
        testlib.ok(res[2].recid == recid_b);
        testlib.ok(res[3].recid == recid_a);

        testlib.ok(res[0].history.length == 1);
        testlib.ok(res[1].history.length == 2);
        testlib.ok(res[2].history.length == 3);
        testlib.ok(res[3].history.length == 6);

        testlib.ok(res[0].history[0].csid == cs2.csid);

        testlib.ok(res[1].history[0].csid == cs3.csid);
        testlib.ok(res[1].history[1].csid == cs2.csid);

        testlib.ok(res[2].history[0].csid == cs5.csid);
        testlib.ok(res[2].history[1].csid == cs4.csid);
        testlib.ok(res[2].history[2].csid == cs1.csid);

        testlib.ok(res[3].history[0].csid == cs6.csid);
        testlib.ok(res[3].history[1].csid == cs5.csid);
        testlib.ok(res[3].history[2].csid == cs4.csid);
        testlib.ok(res[3].history[3].csid == cs3.csid);
        testlib.ok(res[3].history[4].csid == cs2.csid);
        testlib.ok(res[3].history[5].csid == cs1.csid);

        repo.close();
    }

    this.test_slice = function()
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
                        "foo" : 
                        {
                            "datatype" : "int"
                        },
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
        // 0

        ztx = zs.begin_tx();
        var recid_small = ztx.new_record("item", {"foo" : 5}).recid;
        ztx.new_record("item").foo = 16;
        ztx.new_record("item").foo = 23;
        var cs1 = ztx.commit();
        // + 1 = 1

        ztx = zs.begin_tx();
        ztx.new_record("item").foo = 16;
        ztx.new_record("item").foo = 16;
        ztx.new_record("item").foo = 17;
        ztx.new_record("item").foo = 15;
        var cs2 = ztx.commit();
        // + 2 = 3

        ztx = zs.begin_tx();
        var recid_deleteme = ztx.new_record("item", {"foo" : 16}).recid;
        var cs3 = ztx.commit();
        // + 1 = 4

        ztx = zs.begin_tx();
        ztx.new_record("item").foo = 15;
        ztx.new_record("item").foo = 12;
        var cs4 = ztx.commit();
        // + 0 = 4

        ztx = zs.begin_tx();
        ztx.delete_record(recid_deleteme);
        var cs5 = ztx.commit();
        // - 1 = 3

        ztx = zs.begin_tx();
        ztx.new_record("item").foo = 11;
        ztx.new_record("item").foo = 16;
        ztx.new_record("item").foo = 16;
        var cs6 = ztx.commit();
        // + 2 = 5

        var res = zs.query("item", ["foo", "recid", "history"], "foo < 9");
        testlib.ok(res.length == 1);
        testlib.ok(res[0].recid == recid_small);

        repo.close();
    }

}

