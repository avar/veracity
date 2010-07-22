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

function st_zing_links()
{
    this.test_links_with_actions = function()
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
                },

                "folder" :
                {
                    "fields" :
                    {
                        "sum" :
                        {
                            "datatype" : "int",
                            "calculated" : 
                            { 
                                "depends_on" : "children", 
                                "type" : "builtin", 
                                "function" : "sum", 
                                "field_from" : "val"
                            }
                        },
                        "count" :
                        {
                            "datatype" : "int",
                            "calculated" : 
                            {
                                "depends_on" : "children",
                                "type" : "builtin",
                                "function" : "count",
                                "field_from" : "val"
                            }
                        },
                        "mean" :
                        {
                            "datatype" : "int",
                            "calculated" : 
                            {
                                "depends_on" : "children",
                                "type" : "builtin",
                                "function" : "average",
                                "field_from" : "val"
                            }
                        },
                        "min" :
                        {
                            "datatype" : "int",
                            "calculated" : 
                            {
                                "depends_on" : "children",
                                "type" : "builtin",
                                "function" : "min",
                                "field_from" : "val"
                            }
                        },
                        "max" :
                        {
                            "datatype" : "int",
                            "calculated" : 
                            {
                                "depends_on" : "children",
                                "type" : "builtin",
                                "function" : "max",
                                "field_from" : "val"
                            }
                        }
                    }
                }
            },

            "directed_linktypes" :
            {
                    "parent_link" :
                    {
                        "from" :
                        {
                            "link_rectypes" :
                            [
                                "item"
                            ],
                            "name" : "parent",
                            "singular" : true
                        },

                        "to" :
                        {
                            "link_rectypes" :
                            [
                                "folder"
                            ],
                            "name" : "children",
                            "singular" : false
                        },
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

        var rec_folder = ztx.new_record("folder");
        var recid_folder = rec_folder.recid;

        var rec = ztx.new_record("item");
        rec.val = 17;
        rec.parent = recid_folder;

        rec = ztx.new_record("item");
        rec.val = 13;
        rec.parent = recid_folder;

        rec = ztx.new_record("item");
        rec.val = 33;
        rec.parent = recid_folder;

        testlib.ok(3 == rec_folder.children.length, "count children");

        ztx.commit();

        ztx = zs.begin_tx();
        var rf = ztx.open_record(recid_folder);
        testlib.ok(13 == rf.min, "min");
        testlib.ok(33 == rf.max, "max");
        testlib.ok(21 == rf.mean, "mean");
        testlib.ok(3 == rf.count, "count");
        testlib.ok(63 == rf.sum, "sum");
        ztx.commit();

        repo.close();
    }

    this.test_delete_brand_new_link = function()
    {
        var t = 
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "fields" : { "val" : { "datatype" : "int", } }
                },

                "folder" :
                {
                    "fields" : { "val" : { "datatype" : "int", }, }
                }
            },

            "directed_linktypes" :
            {
                    "parent_link" :
                    {
                        "from" :
                        {
                            "link_rectypes" :
                            [
                                "item",
                                "folder"
                            ],
                            "name" : "parent",
                            "singular" : true
                        },

                        "to" :
                        {
                            "link_rectypes" :
                            [
                                "folder"
                            ],
                            "name" : "children",
                            "singular" : false
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

        var ztx = zs.begin_tx();

        var rec_folder1 = ztx.new_record("folder");
        var recid_folder1 = rec_folder1.recid;

        var rec_3 = ztx.new_record("item");
        rec_3.val = 17;
        var rec_4 = ztx.new_record("item");
        rec_4.val = 64;
        var rec_5 = ztx.new_record("item");
        rec_5.val = 51;

        testlib.ok(0 == rec_folder1.children.length, "folder1 should have no children");
        var recid_3 = rec_3.recid;
        var recid_4 = rec_4.recid;
        var recid_5 = rec_5.recid;

        rec_3.parent = rec_folder1;
        rec_4.parent = recid_folder1;
        rec_5.parent = rec_folder1;

        testlib.ok(3 == rec_folder1.children.length, "folder1 should have 3 children");

        rec_folder1.children.remove(recid_4);

        testlib.ok(2 == rec_folder1.children.length, "folder1 should have 2 children");
        testlib.ok(rec_4.parent == null, "rec_4 should no longer have a parent");

        ztx.commit();

        ztx = zs.begin_tx();
        rec_folder1 = ztx.open_record(recid_folder1);
        testlib.ok(2 == rec_folder1.children.length, "folder1 should have 2 children");
        ztx.abort();

        repo.close();
    }
    
    this.test_links_parents = function()
    {
        var t = 
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "fields" : { "val" : { "datatype" : "int", } }
                },

                "folder" :
                {
                    "fields" : { "val" : { "datatype" : "int", }, }
                }
            },

            "directed_linktypes" :
            {
                    "parent_link" :
                    {
                        "from" :
                        {
                            "link_rectypes" :
                            [
                                "item",
                                "folder"
                            ],
                            "name" : "parent",
                            "singular" : true
                        },

                        "to" :
                        {
                            "link_rectypes" :
                            [
                                "folder"
                            ],
                            "name" : "children",
                            "singular" : false
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

        var ztx = zs.begin_tx();

        var rec_folder1 = ztx.new_record("folder");
        var rec_folder2 = ztx.new_record("folder");
        var recid_folder1 = rec_folder1.recid;
        var recid_folder2 = rec_folder2.recid;

        var rec_3 = ztx.new_record("item");
        rec_3.val = 17;
        var rec_4 = ztx.new_record("item");
        rec_4.val = 64;
        var rec_5 = ztx.new_record("item");
        rec_5.val = 51;

        testlib.ok(0 == rec_folder1.children.length, "folder1 should have no children");
        var recid_3 = rec_3.recid;
        var recid_4 = rec_4.recid;
        var recid_5 = rec_5.recid;

        rec_3.parent = rec_folder1;
        rec_4.parent = recid_folder1;
        rec_5.parent = rec_folder1;

        testlib.ok(3 == rec_folder1.children.length, "folder1 should have 3 children");

        ztx.commit();

        ztx = zs.begin_tx();
        rec_folder1 = ztx.open_record(recid_folder1);
        rec_folder2 = ztx.open_record(recid_folder2);
        rec_folder2.children.add(recid_3);
        testlib.ok(2 == rec_folder1.children.length, "folder1 should have 2 children");
        testlib.ok(1 == rec_folder2.children.length, "folder2 should have 1 children");
        testlib.ok(1 == rec_folder2.children.to_array().length, "folder2.children.to_array should have 1 item");
        testlib.ok(recid_3 == rec_folder2.children.to_array()[0], "folder2's only child should be rec3");
        rec_3 = ztx.open_record(recid_3);
        testlib.ok(rec_3.parent == recid_folder2, "rec3's parent should have changed");
        ztx.commit();

        ztx = zs.begin_tx();
        rec_folder1 = ztx.open_record(recid_folder1);
        rec_folder1.children.remove(recid_5);
        rec_folder2 = ztx.open_record(recid_folder2);
        testlib.ok(1 == rec_folder1.children.length, "folder should have 2 children");
        rec_5 = ztx.open_record(recid_5);
        testlib.ok(rec_5.parent == null, "rec5 currently has no parent");
        rec_folder2.children.add(rec_5);
        testlib.ok(rec_5.parent == recid_folder2, "rec5 has a parent again");
        ztx.commit();

        repo.close();
    }

    this.test_links_parents_slightly_different = function()
    {
        var t = 
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "fields" : { "val" : { "datatype" : "int", } }
                },

                "folder" :
                {
                    "fields" : { "val" : { "datatype" : "int", }, }
                }
            },

            "directed_linktypes" :
            {
                    "parent_link" :
                    {
                        "from" :
                        {
                            "link_rectypes" :
                            [
                                "item",
                                "folder"
                            ],
                            "name" : "parent",
                            "singular" : true
                        },

                        "to" :
                        {
                            "link_rectypes" :
                            [
                                "folder"
                            ],
                            "name" : "children",
                            "singular" : false
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

        var ztx = zs.begin_tx();

        var rec_folder1 = ztx.new_record("folder");
        var rec_folder2 = ztx.new_record("folder");
        var recid_folder1 = rec_folder1.recid;
        var recid_folder2 = rec_folder2.recid;

        var rec_3 = ztx.new_record("item");
        rec_3.val = 17;
        var rec_4 = ztx.new_record("item");
        rec_4.val = 64;
        var rec_5 = ztx.new_record("item");
        rec_5.val = 51;

        testlib.ok(0 == rec_folder1.children.length, "folder1 should have no children");
        var recid_3 = rec_3.recid;
        var recid_4 = rec_4.recid;
        var recid_5 = rec_5.recid;

        rec_3.parent = rec_folder1;
        rec_4.parent = recid_folder1;
        rec_5.parent = rec_folder1;

        testlib.ok(3 == rec_folder1.children.length, "folder1 should have 3 children");

        ztx.commit();

        ztx = zs.begin_tx();
        rec_folder1 = ztx.open_record(recid_folder1);
        rec_folder2 = ztx.open_record(recid_folder2);
        rec_folder2.children.add(recid_3);
        testlib.ok(2 == rec_folder1.children.length, "folder1 should have 2 children");
        testlib.ok(1 == rec_folder2.children.length, "folder2 should have 1 children");
        testlib.ok(1 == rec_folder2.children.to_array().length, "folder2.children.to_array should have 1 item");
        testlib.ok(recid_3 == rec_folder2.children.to_array()[0], "folder2's only child should be rec3");
        rec_3 = ztx.open_record(recid_3);
        testlib.ok(rec_3.parent == recid_folder2, "rec3's parent should have changed");
        ztx.commit();

        ztx = zs.begin_tx();
        rec_folder1 = ztx.open_record(recid_folder1);
        rec_5 = ztx.open_record(recid_5);
        rec_5.parent = null;
        rec_folder2 = ztx.open_record(recid_folder2);
        testlib.ok(1 == rec_folder1.children.length, "folder should have 2 children");
        testlib.ok(rec_5.parent == null, "rec5 currently has no parent");
        rec_folder2.children.add(rec_5);
        testlib.ok(rec_5.parent == recid_folder2, "rec5 has a parent again");
        ztx.commit();

        repo.close();
    }

    this.test_links_overwrite_singular = function()
    {
        var t = 
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "fields" : { "val" : { "datatype" : "int", } }
                },
            },

            "directed_linktypes" :
            {
                    "friend_link" :
                    {
                        "from" :
                        {
                            "link_rectypes" :
                            [
                                "item",
                            ],
                            "name" : "sister",
                            "singular" : true
                        },

                        "to" :
                        {
                            "link_rectypes" :
                            [
                                "item"
                            ],
                            "name" : "brother",
                            "singular" : true
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

        var ztx = zs.begin_tx();

        var rec_mary = ztx.new_record("item");
        var rec_bob = ztx.new_record("item");
        var rec_sally = ztx.new_record("item");
        var rec_joe = ztx.new_record("item");

        var recid_mary = rec_mary.recid;
        var recid_bob = rec_bob.recid;
        var recid_sally = rec_sally.recid;
        var recid_joe = rec_joe.recid;

        rec_bob.sister = recid_mary;

        ztx.commit();

        ztx = zs.begin_tx();
        rec_mary = ztx.open_record(recid_mary);
        rec_mary.brother = recid_joe;
        ztx.commit();

        ztx = zs.begin_tx();
        rec_sally = ztx.open_record(recid_sally);
        rec_sally.brother = recid_joe;
        ztx.commit();

        ztx = zs.begin_tx();
        rec_bob = ztx.open_record(recid_bob);
        rec_bob.sister = recid_sally;
        ztx.commit();

        ztx = zs.begin_tx();
        rec_joe = ztx.open_record(recid_joe);
        rec_joe.sister = recid_mary;
        ztx.commit();

        ztx = zs.begin_tx();
        rec_mary = ztx.open_record(recid_mary);
        rec_sally = ztx.open_record(recid_sally);
        rec_joe = ztx.open_record(recid_joe);
        rec_bob = ztx.open_record(recid_bob);
        testlib.ok(rec_mary.brother == recid_joe, "link check");
        testlib.ok(rec_sally.brother == recid_bob, "link check");
        testlib.ok(rec_bob.sister == recid_sally, "link check");
        testlib.ok(rec_joe.sister == recid_mary, "link check");
        ztx.abort();

        repo.close();
    }
    
    this.test_links_parents_slightly_different_yet = function()
    {
        var t = 
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "fields" : { "val" : { "datatype" : "int", } }
                },

                "folder" :
                {
                    "fields" : { "val" : { "datatype" : "int", }, }
                }
            },

            "directed_linktypes" :
            {
                    "parent_link" :
                    {
                        "from" :
                        {
                            "link_rectypes" :
                            [
                                "item",
                                "folder"
                            ],
                            "name" : "parent",
                            "singular" : true
                        },

                        "to" :
                        {
                            "link_rectypes" :
                            [
                                "folder"
                            ],
                            "name" : "children",
                            "singular" : false
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

        var ztx = zs.begin_tx();

        var rec_folder1 = ztx.new_record("folder");
        var rec_folder2 = ztx.new_record("folder");
        var recid_folder1 = rec_folder1.recid;
        var recid_folder2 = rec_folder2.recid;

        var rec_3 = ztx.new_record("item");
        rec_3.val = 17;
        var rec_4 = ztx.new_record("item");
        rec_4.val = 64;
        var rec_5 = ztx.new_record("item");
        rec_5.val = 51;

        testlib.ok(0 == rec_folder1.children.length, "folder1 should have no children");
        var recid_3 = rec_3.recid;
        var recid_4 = rec_4.recid;
        var recid_5 = rec_5.recid;

        rec_3.parent = rec_folder1;
        rec_4.parent = recid_folder1;
        rec_5.parent = rec_folder1;

        testlib.ok(3 == rec_folder1.children.length, "folder1 should have 3 children");

        ztx.commit();

        ztx = zs.begin_tx();
        rec_folder1 = ztx.open_record(recid_folder1);
        rec_folder2 = ztx.open_record(recid_folder2);
        rec_folder2.children.add(recid_3);
        testlib.ok(2 == rec_folder1.children.length, "folder1 should have 2 children");
        testlib.ok(1 == rec_folder2.children.length, "folder2 should have 1 children");
        testlib.ok(1 == rec_folder2.children.to_array().length, "folder2.children.to_array should have 1 item");
        testlib.ok(recid_3 == rec_folder2.children.to_array()[0], "folder2's only child should be rec3");
        rec_3 = ztx.open_record(recid_3);
        testlib.ok(rec_3.parent == recid_folder2, "rec3's parent should have changed");
        ztx.commit();

        ztx = zs.begin_tx();
        rec_folder1 = ztx.open_record(recid_folder1);
        rec_5 = ztx.open_record(recid_5);
        rec_folder1.children.remove(rec_5);
        rec_folder2 = ztx.open_record(recid_folder2);
        testlib.ok(1 == rec_folder1.children.length, "folder should have 2 children");
        testlib.ok(rec_5.parent == null, "rec5 currently has no parent");
        rec_folder2.children.add(rec_5);
        testlib.ok(rec_5.parent == recid_folder2, "rec5 has a parent again");
        ztx.commit();

        repo.close();
    }

    this.test_delete_record_with_links_to_it = function()
    {
        var t = 
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "fields" : { "val" : { "datatype" : "int", } }
                },

                "folder" :
                {
                    "fields" : { "val" : { "datatype" : "int", }, }
                }
            },

            "directed_linktypes" :
            {
                    "parent_link" :
                    {
                        "from" :
                        {
                            "link_rectypes" :
                            [
                                "item",
                                "folder"
                            ],
                            "name" : "parent",
                            "singular" : true
                        },

                        "to" :
                        {
                            "link_rectypes" :
                            [
                                "folder"
                            ],
                            "name" : "children",
                            "singular" : false
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

        var ztx = zs.begin_tx();

        var rec_folder1 = ztx.new_record("folder");
        var rec_folder2 = ztx.new_record("folder");
        var recid_folder1 = rec_folder1.recid;
        var recid_folder2 = rec_folder2.recid;

        var rec_3 = ztx.new_record("item");
        rec_3.val = 17;
        var rec_4 = ztx.new_record("item");
        rec_4.val = 64;
        var rec_5 = ztx.new_record("item");
        rec_5.val = 51;

        testlib.ok(0 == rec_folder1.children.length, "folder1 should have no children");
        var recid_3 = rec_3.recid;
        var recid_4 = rec_4.recid;
        var recid_5 = rec_5.recid;

        rec_3.parent = rec_folder1;
        rec_4.parent = recid_folder1;
        rec_5.parent = rec_folder1;

        testlib.ok(3 == rec_folder1.children.length, "folder1 should have 3 children");

        ztx.commit();

        ztx = zs.begin_tx();

        var err = "";
        try
        {
            ztx.delete_record(recid_folder1);
        }
        catch (e)
        {
            err = e.toString();
        }
        ztx.abort();

        testlib.ok(err.length>0, "cannot delete a record with links to it");

        repo.close();
    }
    
    this.test_delete_record_with_new_links = function()
    {
        var t = 
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "fields" : { "val" : { "datatype" : "int", } }
                },

                "folder" :
                {
                    "fields" : { "val" : { "datatype" : "int", }, }
                }
            },

            "directed_linktypes" :
            {
                    "parent_link" :
                    {
                        "from" :
                        {
                            "link_rectypes" :
                            [
                                "item",
                                "folder"
                            ],
                            "name" : "parent",
                            "singular" : true
                        },

                        "to" :
                        {
                            "link_rectypes" :
                            [
                                "folder"
                            ],
                            "name" : "children",
                            "singular" : false
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

        var ztx = zs.begin_tx();

        var rec_folder1 = ztx.new_record("folder");
        var rec_folder2 = ztx.new_record("folder");
        var recid_folder1 = rec_folder1.recid;
        var recid_folder2 = rec_folder2.recid;

        var rec_3 = ztx.new_record("item");
        rec_3.val = 17;
        var rec_4 = ztx.new_record("item");
        rec_4.val = 64;
        var rec_5 = ztx.new_record("item");
        rec_5.val = 51;

        testlib.ok(0 == rec_folder1.children.length, "folder1 should have no children");
        var recid_3 = rec_3.recid;
        var recid_4 = rec_4.recid;
        var recid_5 = rec_5.recid;

        rec_3.parent = rec_folder1;
        rec_4.parent = recid_folder1;
        rec_5.parent = rec_folder1;

        testlib.ok(3 == rec_folder1.children.length, "folder1 should have 3 children");

        ztx.delete_record(recid_4);

        testlib.ok(2 == rec_folder1.children.length, "folder1 should have 3 children");

        ztx.commit();

        repo.close();
    }

    this.test_links_singular_merge = function()
    {
        var t = 
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "fields" : { "val" : { "datatype" : "int", } }
                },

                "folder" :
                {
                    "fields" : { "val" : { "datatype" : "int", }, }
                }
            },

            "directed_linktypes" :
            {
                    "parent_link" :
                    {
                        "from" :
                        {
                            "link_rectypes" :
                            [
                                "item"
                            ],
                            "name" : "parent",
                            "singular" : true
                        },

                        "to" :
                        {
                            "link_rectypes" :
                            [
                                "folder"
                            ],
                            "name" : "children",
                            "singular" : false
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

        var rec_folder1 = ztx.new_record("folder");
        var recid_folder1 = rec_folder1.recid;

        var rec_folder2 = ztx.new_record("folder");
        var recid_folder2 = rec_folder2.recid;

        var rec_3 = ztx.new_record("item");
        var recid_3 = rec_3.recid;
        rec_3.val = 17;

        var res = ztx.commit();

        ztx = zs.begin_tx(res.csid);
        rec_3 = ztx.open_record(recid_3);
        rec_3.parent = recid_folder1;
        ztx.commit();

        ztx = zs.begin_tx(res.csid);
        rec_3 = ztx.open_record(recid_3);
        rec_3.parent = recid_folder2;
        ztx.commit();

        testlib.ok(zs.get_leaves().length==2, "two leaves");

        var res = zs.merge();

        testlib.ok(zs.get_leaves().length==2, "two leaves");
        testlib.ok(res.errors.length > 0);

        repo.close();
    }
    
    this.test_links_parents_required_fail_not_dirty = function()
    {
        var t = 
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "fields" : { "val" : { "datatype" : "int", } }
                },

                "folder" :
                {
                    "fields" : { "val" : { "datatype" : "int", }, }
                }
            },

            "directed_linktypes" :
            {
                    "parent_link" :
                    {
                        "from" :
                        {
                            "link_rectypes" :
                            [
                                "item"
                            ],
                            "name" : "parent",
                            "required" : true,
                            "singular" : true
                        },

                        "to" :
                        {
                            "link_rectypes" :
                            [
                                "folder"
                            ],
                            "name" : "children",
                            "singular" : false
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

        var rec_folder1 = ztx.new_record("folder");
        var recid_folder1 = rec_folder1.recid;

        var rec_3 = ztx.new_record("item");
        rec_3.val = 17;
        var rec_4 = ztx.new_record("item");
        rec_4.val = 64;
        var rec_5 = ztx.new_record("item");
        rec_5.val = 51;

        testlib.ok(0 == rec_folder1.children.length, "folder1 should have no children");
        var recid_3 = rec_3.recid;
        var recid_4 = rec_4.recid;
        var recid_5 = rec_5.recid;

        rec_3.parent = rec_folder1;
        rec_4.parent = recid_folder1;
        rec_5.parent = rec_folder1;

        testlib.ok(3 == rec_folder1.children.length, "folder1 should have 3 children");

        ztx.commit();

        ztx = zs.begin_tx();
        rec_4 = ztx.open_record(recid_4);
        rec_4.parent = null;

        var res = ztx.commit();

        testlib.ok(res.errors.length == 1);
        testlib.ok("required_link" == res.errors[0].type);
        testlib.ok("parent_link" == res.errors[0].link_name);

        ztx.abort();

        repo.close();
    }
    
    this.test_links_parents_required_fail = function()
    {
        var t = 
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "fields" : { "val" : { "datatype" : "int", } }
                },

                "folder" :
                {
                    "fields" : { "val" : { "datatype" : "int", }, }
                }
            },

            "directed_linktypes" :
            {
                    "parent_link" :
                    {
                        "from" :
                        {
                            "link_rectypes" :
                            [
                                "item"
                            ],
                            "name" : "parent",
                            "required" : true,
                            "singular" : true
                        },

                        "to" :
                        {
                            "link_rectypes" :
                            [
                                "folder"
                            ],
                            "name" : "children",
                            "singular" : false
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

        var ztx = zs.begin_tx();

        var rec_folder1 = ztx.new_record("folder");
        var recid_folder1 = rec_folder1.recid;

        var rec_3 = ztx.new_record("item");
        rec_3.val = 17;
        var rec_4 = ztx.new_record("item");
        rec_4.val = 64;
        var rec_5 = ztx.new_record("item");
        rec_5.val = 51;

        testlib.ok(0 == rec_folder1.children.length, "folder1 should have no children");
        var recid_3 = rec_3.recid;
        var recid_4 = rec_4.recid;
        var recid_5 = rec_5.recid;

        rec_3.parent = rec_folder1;
        //rec_4.parent = recid_folder1;
        rec_5.parent = rec_folder1;

        testlib.ok(2 == rec_folder1.children.length, "folder1 should have 2 children");

        var res = ztx.commit();

        testlib.ok(res.errors.length == 1);
        testlib.ok("required_link" == res.errors[0].type);
        testlib.ok("parent_link" == res.errors[0].link_name);

        ztx.abort();

        repo.close();
    }
    
    this.test_links_parents_required_ok = function()
    {
        var t = 
        {
            "version" : 1,
            "rectypes" :
            {
                "item" :
                {
                    "fields" : { "val" : { "datatype" : "int", } }
                },

                "folder" :
                {
                    "fields" : { "val" : { "datatype" : "int", }, }
                }
            },

            "directed_linktypes" :
            {
                    "parent_link" :
                    {
                        "from" :
                        {
                            "link_rectypes" :
                            [
                                "item"
                            ],
                            "name" : "parent",
                            "required" : true,
                            "singular" : true
                        },

                        "to" :
                        {
                            "link_rectypes" :
                            [
                                "folder"
                            ],
                            "name" : "children",
                            "singular" : false
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

        var ztx = zs.begin_tx();

        var rec_folder1 = ztx.new_record("folder");
        var recid_folder1 = rec_folder1.recid;

        var rec_3 = ztx.new_record("item");
        rec_3.val = 17;
        var rec_4 = ztx.new_record("item");
        rec_4.val = 64;
        var rec_5 = ztx.new_record("item");
        rec_5.val = 51;

        testlib.ok(0 == rec_folder1.children.length, "folder1 should have no children");
        var recid_3 = rec_3.recid;
        var recid_4 = rec_4.recid;
        var recid_5 = rec_5.recid;

        rec_3.parent = rec_folder1;
        rec_4.parent = recid_folder1;
        rec_5.parent = rec_folder1;

        testlib.ok(3 == rec_folder1.children.length, "folder1 should have 3 children");

        ztx.commit();

        repo.close();
    }

    // TODO lots more tests can/should go here
}

