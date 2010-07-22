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
    this.test_simple_int = function()
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
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.val = 17;
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(17 == zrec.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_simple_string = function()
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
        zrec.val = "17";
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok("17" == zrec.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_simple_bool = function()
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
                            "datatype" : "bool",
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
        zrec.val = true;
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(zrec.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_int_max = function()
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
                                "max" : 31
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

        var err = "";
        try
        {
            zrec.val = 17;
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(0 == err.length, "value was within range");

        err = "";
        try
        {
            zrec.val = 32;
        }
        catch (e)
        {
            err = e.toString();
        }
        err_must_contain(err, "Constraint");

        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_int_min = function()
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
                                "min" : 31
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

        var err = "";
        try
        {
            zrec.val = 97;
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(0 == err.length, "value was within range");

        err = "";
        try
        {
            zrec.val = 30;
        }
        catch (e)
        {
            err = e.toString();
        }
        err_must_contain(err, "Constraint");

        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_int_allowed = function()
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
                                "allowed" : [2, 4, 8, 16, 32, 64]
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

        var err = "";
        try
        {
            zrec.val = 16;
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(0 == err.length, "value was within range");

        err = "";
        try
        {
            zrec.val = 19;
        }
        catch (e)
        {
            err = e.toString();
        }
        err_must_contain(err, "Constraint");

        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_int_prohibited = function()
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
                                "prohibited" : [2, 4, 8, 16, 32, 64]
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

        var err = "";
        try
        {
            zrec.val = 15;
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(0 == err.length, "value was within range");

        err = "";
        try
        {
            zrec.val = 4;
        }
        catch (e)
        {
            err = e.toString();
        }
        err_must_contain(err, "Constraint");

        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_string_allowed = function()
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
                                "allowed" : ["hello", "world"]
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

        var err = "";
        try
        {
            zrec.val = "hello";
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(0 == err.length, "value was within range");

        err = "";
        try
        {
            zrec.val = "hola";
        }
        catch (e)
        {
            err = e.toString();
        }
        err_must_contain(err, "Constraint");

        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_string_prohibited = function()
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
                                "prohibited" : ["michigan", "indiana", "iowa"]
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

        var err = "";
        try
        {
            zrec.val = "illinois";
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(0 == err.length, "value was within range");

        err = "";
        try
        {
            zrec.val = "michigan";
        }
        catch (e)
        {
            err = e.toString();
        }
        err_must_contain(err, "Constraint");

        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_string_minlength = function()
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
                                "minlength" : 3
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

        var err = "";
        try
        {
            zrec.val = "fiddlesticks";
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(0 == err.length, "value was within range");

        err = "";
        try
        {
            zrec.val = "hi";
        }
        catch (e)
        {
            err = e.toString();
        }
        err_must_contain(err, "Constraint");

        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_string_maxlength = function()
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
                                "maxlength" : 3
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

        var err = "";
        try
        {
            zrec.val = "pop";
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(0 == err.length, "value was within range");

        err = "";
        try
        {
            zrec.val = "welcome";
        }
        catch (e)
        {
            err = e.toString();
        }
        err_must_contain(err, "Constraint");

        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_modify_int = function()
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
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.val = 17;
        var orig = ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        zrec = zs.get_record(recid);
        //ztx = zs.begin_tx();
        //zrec = ztx.open_record(recid);
        testlib.ok(17 == zrec.val, "should match");
        //ztx.commit();

        // open the record, modify the val
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        zrec.val = 68;
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(68 == zrec.val, "should match");
        ztx.commit();

        // just to be cruel, open the old version of the db and see if it's 17
        ztx = zs.begin_tx(orig.csid);
        zrec = ztx.open_record(recid);
        testlib.ok(17 == zrec.val, "should match");
        testlib.ok(zrec.history.length == 2);
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_type_mismatches = function()
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
                        "ival" : { "datatype" : "int" },
                        "bval" : { "datatype" : "bool" },
                        "sval" : { "datatype" : "string" },
                        "dval" :
                        {
                            "datatype" : "dagnode",
                            "constraints" :
                            {
                                "dag" : "TESTING_DB"
                            }
                        },
                        "fval" : { "datatype" : "attachment" }
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

        var err = "";
        try
        {
            zrec.ival = 17;
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(0 == err.length, "value was within range");

        err = "";
        try
        {
            zrec.fval = 32;
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "can't assign an int to an attachment field");

        err = "";
        try
        {
            zrec.dval = 32;
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "can't assign an int to a dagnode field");

        err = "";
        try
        {
            zrec.sval = 32;
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "can't assign an int to a string field");

        err = "";
        try
        {
            zrec.bval = "32";
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "can't assign a string to a bool field");

        err = "";
        try
        {
            zrec.ival = "32";
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "can't assign a string to an int field");

        err = "";
        try
        {
            zrec.ival = {};
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "can't assign an object to an int field");

        err = "";
        try
        {
            zrec.foo = 5;
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "foo does not exist");

        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_delete_record = function()
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
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.val = 17;
        var orig = ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(17 == zrec.val, "should match");
        ztx.commit();

        // now delete the record
        ztx = zs.begin_tx();
        ztx.delete_record(recid);
        ztx.commit();

        // and make sure it's gone
        ztx = zs.begin_tx();
        err = "";
        try
        {
            zrec = ztx.open_record(recid);
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "record is gone");
        ztx.commit();

        ztx = zs.begin_tx(orig.csid);
        zrec = ztx.open_record(recid);
        testlib.ok(17 == zrec.val, "but it's still in the old changeset");
        ztx.abort();


        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_remove_field = function()
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
                            "datatype" : "int",
                        },
                        "bar" :
                        {
                            "datatype" : "int",
                            "constraints" :
                            {
                                "required" : true
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
        zrec.bar = 17;
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(17 == zrec.bar, "should match");
        testlib.ok(31 == zrec.foo, "should match");
        zrec.foo = null;
        ztx.commit();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);

        err = "";
        try
        {
            var x = zrec.foo;
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "foo is gone");

        err = "";
        try
        {
            zrec.bar = null;
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "can't remove bar because it's required");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_required_field = function()
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
                        "bar" :
                        {
                            "datatype" : "int",
                            "constraints" :
                            {
                                "required" : true
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

        var res = ztx.commit();
        testlib.ok(res.errors.length == 1, "bar is required but not present");

        zrec.bar = 22;
        res = ztx.commit();
        testlib.ok(res.errors == null, "no errors now");

        repo.close();
    }

    this.test_string_defaultvalue = function()
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
                                "defaultvalue" : "hola"
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
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok("hola" == zrec.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_int_defaultvalue = function()
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
                                "defaultvalue" : 31
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
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(31 == zrec.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_bool_defaultvalue_true = function()
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
                            "datatype" : "bool",
                            "constraints" :
                            {
                                "defaultvalue" : true
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
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(zrec.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_bool_defaultvalue_false = function()
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
                            "datatype" : "bool",
                            "constraints" :
                            {
                                "defaultvalue" : false
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
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(!zrec.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_try_to_get_pending_deleted_record = function()
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
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.val = 17;
        var orig = ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(17 == zrec.val, "should match");
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        ztx.delete_record(recid);

        err = "";
        try
        {
            zrec = ztx.open_record(recid);
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "record is gone");
        ztx.abort();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(17 == zrec.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_simple_att = function()
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
                        "file" :
                        {
                            "datatype" : "attachment"
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

        var baseName = getTestName(arguments.callee.toString());
        var file1 = baseName + "_attachment.txt";
        createFileOnDisk(file1, 100);

        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.file = file1;
        ztx.commit();

        var ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        var hid = zrec.file;
        ztx.abort();

        repo.close();
        testlib.ok(true, "No errors");

        deleteFileOnDisk(file1);
    }

    this.test_att_with_maxlength = function()
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
                        "file" :
                        {
                            "datatype" : "attachment",
                            "constraints" :
                            {
                                "maxlength" : 50000
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

        var baseName = getTestName(arguments.callee.toString());
        var file1 = baseName + "_attachment.txt";
        createFileOnDisk(file1, 100);

        var zrec = ztx.new_record("item");
        zrec.file = file1;
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");

        deleteFileOnDisk(file1);
    }

    this.test_att_with_maxlength_fail = function()
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
                        "file" :
                        {
                            "datatype" : "attachment",
                            "constraints" :
                            {
                                "maxlength" : 50
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

        var baseName = getTestName(arguments.callee.toString());
        var file1 = baseName + "_attachment.txt";
        createFileOnDisk(file1, 100);

        var zrec = ztx.new_record("item");
        var err = "";
        try
        {
            zrec.file = file1;
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "attachment is too long");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");

        deleteFileOnDisk(file1);
    }

    this.test_delete_brand_new_record = function()
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
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.val = 17;
        ztx.delete_record(recid);
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();

        err = "";
        try
        {
            zrec = ztx.open_record(recid);
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(err.length > 0, "record was never there");
        ztx.abort();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_simple_dagnode = function()
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
                        "cs" :
                        {
                            "datatype" : "dagnode",
                            "constraints" :
                            {
                                "dag" : "TESTING_DB"
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
        var res = ztx.commit();

        ztx = zs.begin_tx();
        var zrec = ztx.new_record("item");
        var recid = zrec.recid;
        zrec.cs = res.csid;
        ztx.commit();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        testlib.ok(res.csid == zrec.cs, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_string_unique_duplicate_without_constraint = function()
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
        var rec_1 = ztx.new_record("item");
        var recid_1 = rec_1.recid;
        rec_1.val = "hey";
        var rec_2 = ztx.new_record("item");
        var recid_2 = rec_2.recid;
        rec_2.val = "hey";
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        rec_1 = ztx.open_record(recid_1);
        testlib.ok("hey" == rec_1.val, "should match");
        rec_2 = ztx.open_record(recid_2);
        testlib.ok("hey" == rec_2.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_string_unique_duplicate_with_explicit_non_constraint = function()
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
                                "unique" : false
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
        var rec_1 = ztx.new_record("item");
        var recid_1 = rec_1.recid;
        rec_1.val = "hey";
        var rec_2 = ztx.new_record("item");
        var recid_2 = rec_2.recid;
        rec_2.val = "hey";
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        rec_1 = ztx.open_record(recid_1);
        testlib.ok("hey" == rec_1.val, "should match");
        rec_2 = ztx.open_record(recid_2);
        testlib.ok("hey" == rec_2.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_string_unique_constraint_but_no_clash = function()
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
        var rec_1 = ztx.new_record("item");
        var recid_1 = rec_1.recid;
        rec_1.val = "hey";
        var rec_2 = ztx.new_record("item");
        var recid_2 = rec_2.recid;
        rec_2.val = "hay";
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        rec_1 = ztx.open_record(recid_1);
        testlib.ok("hey" == rec_1.val, "should match");
        rec_2 = ztx.open_record(recid_2);
        testlib.ok("hay" == rec_2.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_string_unique_constraint_clash = function()
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
        var rec_1 = ztx.new_record("item");
        var recid_1 = rec_1.recid;
        rec_1.val = "hey";
        var rec_2 = ztx.new_record("item");
        var recid_2 = rec_2.recid;
        rec_2.val = "hey";

        var res = ztx.commit();

        testlib.ok(res.errors.length > 0);
        testlib.ok("unique" == res.errors[0].type);
        testlib.ok("val" == res.errors[0].field_name);
        testlib.ok("hey" == res.errors[0].field_value);

        ztx.abort();

        repo.close();
    }

    this.test_int_unique_duplicate_without_constraint = function()
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
        var rec_1 = ztx.new_record("item");
        var recid_1 = rec_1.recid;
        rec_1.val = 17;
        var rec_2 = ztx.new_record("item");
        var recid_2 = rec_2.recid;
        rec_2.val = 17;
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        rec_1 = ztx.open_record(recid_1);
        testlib.ok(17 == rec_1.val, "should match");
        rec_2 = ztx.open_record(recid_2);
        testlib.ok(17 == rec_2.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_int_unique_duplicate_with_explicit_non_constraint = function()
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
                                "unique" : false
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
        var rec_1 = ztx.new_record("item");
        var recid_1 = rec_1.recid;
        rec_1.val = 17;
        var rec_2 = ztx.new_record("item");
        var recid_2 = rec_2.recid;
        rec_2.val = 17;
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        rec_1 = ztx.open_record(recid_1);
        testlib.ok(17 == rec_1.val, "should match");
        rec_2 = ztx.open_record(recid_2);
        testlib.ok(17 == rec_2.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_int_unique_constraint_but_no_clash = function()
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
        var rec_1 = ztx.new_record("item");
        var recid_1 = rec_1.recid;
        rec_1.val = 17;
        var rec_2 = ztx.new_record("item");
        var recid_2 = rec_2.recid;
        rec_2.val = 18;
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        ztx = zs.begin_tx();
        rec_1 = ztx.open_record(recid_1);
        testlib.ok(17 == rec_1.val, "should match");
        rec_2 = ztx.open_record(recid_2);
        testlib.ok(18 == rec_2.val, "should match");
        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_int_unique_constraint_clash = function()
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
        var rec_1 = ztx.new_record("item");
        var recid_1 = rec_1.recid;
        rec_1.val = 17;
        var rec_2 = ztx.new_record("item");
        var recid_2 = rec_2.recid;
        rec_2.val = 17;

        err = "";
        try
        {
            ztx.commit();
        }
        catch (e)
        {
            err = e.toString();
            ztx.abort();
        }

        testlib.ok(err.length > 0, "unique constraint");
        err_must_contain(err, "Constraint");
        err_must_contain(err, "unique");
        err_must_contain(err, "item");
        err_must_contain(err, "val");

        repo.close();
    }

    this.test_int_unique_constraint_clash = function()
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
                            "datatype" : "datetime",
                            "constraints" :
                            {
                                "defaultvalue" : "now"
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
        ztx.commit();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        var d = new Date();
        testlib.ok(new Date().getTime() >= zrec.foo);
        ztx.abort();

        ztx = zs.begin_tx();
        zrec = ztx.open_record(recid);
        zrec.foo = new Date();
        ztx.commit();

        zrec = zs.get_record(recid);
        testlib.ok(new Date().getTime() >= zrec.foo);
        testlib.ok(zrec.history.length == 2);

        repo.close();
    }

    this.test_datetime_max_min = function()
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
                            "datatype" : "datetime",
                            "constraints" :
                            {
                                "max" : new Date(1994,11,1).getTime(),
                                "min" : new Date(1994,10,1).getTime()
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

        var err = "";
        try
        {
            zrec.val = new Date(1994, 10, 15).getTime();
        }
        catch (e)
        {
            err = e.toString();
        }
        testlib.ok(0 == err.length, "value was within range");

        err = "";
        try
        {
            zrec.val = new Date(1993, 10, 15).getTime();
        }
        catch (e)
        {
            err = e.toString();
        }
        err_must_contain(err, "Constraint");

        ztx.commit();

        repo.close();
        testlib.ok(true, "No errors");
    }

    this.test_gen_random_unique = function()
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
        for (i=0; i<200; i++)
        {
            ztx.new_record("item");
        }
        var res = ztx.commit();

        ids = zs.query("item", ["recid"]);
        testlib.ok(ids.length == 200);

        repo.close();

        stats = sg.repo_diag_blobs(reponame);
        print(sg.to_json(stats));
    }

    this.test_gen_userprefix_unique = function()
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
                        "id" :
                        {
                            "datatype" : "string",
                            "constraints" :
                            {
                                "unique" : true,
                                "defaultfunc" : "gen_userprefix_unique"
                            }
                        }
                    }
                }
            }
        };

        var reponame = sg.gid();
        sg.new_repo(reponame, false);

        var repo = sg.open_repo(reponame);

        var zs;
        var ztx;
        var rec;
        var userid;
        var prefix;
        var recid;

        zs = new zingdb(repo, sg.dagnum.USERS);
        ztx = zs.begin_tx();
        var recuser = ztx.new_record("user", {"email" : "eric@sourcegear.com"});
        userid = recuser.recid;
        prefix = recuser.prefix;
        ztx.commit();

        zs = new zingdb(repo, sg.dagnum.TESTING_DB);

        ztx = zs.begin_tx();
        ztx.set_template(t);
        ztx.commit();

        for (i=0; i<5; i++)
        {
            ztx = zs.begin_tx(null, userid);
            ztx.new_record("item");
            ztx.new_record("item");
            ztx.new_record("item");
            ztx.commit();
        }

        ztx = zs.begin_tx(null, userid);
        recid = ztx.new_record("item").recid;
        ztx.commit();

        rec = zs.get_record(recid);
        testlib.ok(rec.id == (prefix + "00016"));
        print(sg.to_json(rec));

        repo.close();
    }

    this.test_string_with_escaped_quote = function()
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
                        "verb" :
                        {
                            "datatype" : "string",
                        },
                        "what" :
                        {
                            "datatype" : "string",
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
        var recid1 = ztx.new_record("item", {"verb" : "don't", "what" : "be cruel"}).recid;
        var recid2 = ztx.new_record("item", {"verb" : "don't", "what" : "be rude"}).recid;
        var recid3 = ztx.new_record("item", {"verb" : "do", "what" : "be nice"}).recid;
        ztx.commit();

        // start a new tx, open the record, and see if the value is correct
        zrec = zs.get_record(recid1);
        testlib.ok("don't" == zrec.verb, "should match");
        testlib.ok("be cruel" == zrec.what, "should match");

        var recs = zs.query("item", ["what", "recid"], "verb == 'do'");
        testlib.ok(1 == recs.length);

        var recs = zs.query("item", ["recid"], "verb == 'don\\'t'", "what #ASC");
        testlib.ok(2 == recs.length);
        testlib.ok(recs[0] == recid1);
        testlib.ok(recs[1] == recid2);

        repo.close();
        testlib.ok(true, "No errors");
    };

    this.testMultiUpdate = function()
    {
        var witTemplate = sg.file.read(pathCombine(srcDir, "../libraries/templates/sg_ztemplate__wit.json"));

        var t;

        eval('t = ' + witTemplate);

        var reponame = sg.gid();
        sg.new_repo(reponame, false);
        var repo = sg.open_repo(reponame);
        var zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);

        var ztx = zs.begin_tx();
        ztx.set_template(t);
        var zrec = ztx.new_record("item");
        this.bugId = zrec.recid; // If you modify the bug, please leave the string "Common" in the description.
        this.bugSimpleId = "123456";
        zrec.title = "Common bug for stWeb tests.";
        zrec.id = this.bugSimpleId;
        ztx.commit();

        repo.close();

        for ( var i = 1; i <= 5; ++i )
        {
            // isolating repo instances as in web calls
            repo = sg.open_repo(reponame);
            zs = new zingdb(repo, sg.dagnum.WORK_ITEMS);

            ztx = zs.begin_tx();
            zrec = ztx.open_record(this.bugId);

            if (testlib.ok(!! zrec, "record found in update " + i))
            {
                zrec.title = "dummy";
                zrec.title = "Common bug for stWeb tests.";
                zrec.logged_time = 0;
                zrec.status = "open";
                zrec.priority = "High";
                ztx.commit();
                repo.close();
            }
            else
            {
                ztx.abort();
                repo.close();
                break;
            }
        }
    };
}

